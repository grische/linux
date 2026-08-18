// SPDX-License-Identifier: GPL-2.0-only
/*
 * Intel GRX500 (xRX500) PCIe slim PHY driver
 *
 * Copyright (C) 2018 Intel Corporation.
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/string_choices.h>
#include <linux/time64.h>

/*
 * PCIe 2.0 PDI PHY register definition. The block spans 0xc004..0xc014;
 * only CFG4 is programmed here, the rest keep their reset values.
 */
#define PCIE_PHY_CFG0			0xc004
#define PCIE_PHY_CFG1			0xc008
#define PCIE_PHY_CFG2			0xc00c
#define PCIE_PHY_CFG3			0xc010
#define PCIE_PHY_CFG4			0xc014
#define  PCIE_PHY_PIPE_PD		BIT(14)
#define  PCIE_PHY_PIPE_PD_O		BIT(15)

/* xRX500 LCPLL SSC, inside the CGU */
#define PCIE_LCPLL_CFG0			0x0094
#define  PLL_RESET			BIT(0)
#define  LCPLL_CFG0_LOCKED		BIT(1)
#define PCIE_LCPLL_CFG1			0x0098
#define  PLL_SSC			BIT(24)
#define  PLL_CLK_OV			BIT(20)
#define PCIE_LCPLL_SSC_CTRL		0x009c
#define PCIE_LCPLL_SSC_SCALE		0x00a0
#define PCIE_LCPLL_COEF(n)		(0x00a4 + (n) * 4)
#define PCIE_LCPLL_COEF_NUM		8

#define PCIE_LCPLL_LOCK_SLEEP_US	100
#define PCIE_LCPLL_LOCK_TIMEOUT_US	(100 * USEC_PER_MSEC)

/*
 * This instance's reference-clock gate bit in the chip-top interface mux
 * word, reached through intel,syscon at the byte offset that property's
 * argument gives (0x120). Unlike the USB PHY's per-controller word the
 * offset is the same for all three instances, and which instance is meant
 * is a bit position inside it, which is what intel,phy-id supplies.
 */
#define PCIE_RCLK(id)			BIT((id) + 22)

#define PCIE_PHY_ID_MAX			2

/*
 * The LCPLL and its spread-spectrum generator sit in the CGU and are shared
 * by every PHY instance, so the programming sequence must run exactly once.
 * The refcount and the sequence itself are serialised against concurrent and
 * deferred probes of the sibling instances.
 */
static DEFINE_MUTEX(intel_pciephy_ssc_lock);
static int intel_pciephy_ssc_refcount;

struct intel_pciephy {
	struct device *dev;
	void __iomem *phy_base;
	struct regmap *cgu;
	struct regmap *chiptop;
	unsigned int ifmux_off;
	struct phy *phy;
	struct reset_control *rst;
	u32 id;
	bool ssc_en;
	bool ssc_on;
};

static void intel_pciephy_dbg_dump(struct intel_pciephy *priv)
{
	struct device *dev = priv->dev;
	u32 val;
	int i;

	regmap_read(priv->cgu, PCIE_LCPLL_CFG0, &val);
	dev_dbg(dev, "LCPLL CFG0: 0x%08x\n", val);
	regmap_read(priv->cgu, PCIE_LCPLL_CFG1, &val);
	dev_dbg(dev, "LCPLL CFG1: 0x%08x\n", val);
	regmap_read(priv->cgu, PCIE_LCPLL_SSC_CTRL, &val);
	dev_dbg(dev, "SSC CTRL: 0x%08x\n", val);
	regmap_read(priv->cgu, PCIE_LCPLL_SSC_SCALE, &val);
	dev_dbg(dev, "SSC SCALE: 0x%08x\n", val);

	for (i = 0; i < PCIE_LCPLL_COEF_NUM; i++) {
		regmap_read(priv->cgu, PCIE_LCPLL_COEF(i), &val);
		dev_dbg(dev, "LCPLL COEF[%d]: 0x%08x\n", i, val);
	}

	regmap_read(priv->chiptop, priv->ifmux_off, &val);
	dev_dbg(dev, "PCIE PHY IFMUX[22-24]: 0x%08x\n", val);
}

static void intel_pciephy_power(struct intel_pciephy *priv, bool on)
{
	u32 val;

	val = readl(priv->phy_base + PCIE_PHY_CFG4);
	if (on)
		val &= ~PCIE_PHY_PIPE_PD_O;
	else
		val |= PCIE_PHY_PIPE_PD_O;
	writel(val, priv->phy_base + PCIE_PHY_CFG4);

	mdelay(1);
}

static void intel_pciephy_lcpll_wait_lock(struct intel_pciephy *priv)
{
	u32 val;
	int ret;

	ret = regmap_read_poll_timeout(priv->cgu, PCIE_LCPLL_CFG0, val,
				       val & LCPLL_CFG0_LOCKED,
				       PCIE_LCPLL_LOCK_SLEEP_US,
				       PCIE_LCPLL_LOCK_TIMEOUT_US);
	if (ret)
		dev_err(priv->dev, "LCPLL not locked yet\n");
}

static void intel_pciephy_ssc_enable(struct intel_pciephy *priv)
{
	int i;

	if (!priv->ssc_en)
		return;

	guard(mutex)(&intel_pciephy_ssc_lock);

	if (priv->ssc_on)
		return;

	priv->ssc_on = true;
	if (intel_pciephy_ssc_refcount++ > 0) {
		dev_dbg(priv->dev, "SSC already enabled, count: %d\n",
			intel_pciephy_ssc_refcount);
		return;
	}

	/* Enable SSC and LCPLL */
	regmap_write(priv->cgu, PCIE_LCPLL_CFG1, 0x10003004);
	regmap_write(priv->cgu, PCIE_LCPLL_CFG1, 0x10103004);

	/* Need bit 0 to go from 0 to 1 */
	regmap_write(priv->cgu, PCIE_LCPLL_CFG0, 0x00000190);
	regmap_write(priv->cgu, PCIE_LCPLL_CFG0, 0x00000191);

	intel_pciephy_lcpll_wait_lock(priv);

	/* Fixed coefficient parameters */
	for (i = 0; i < 4; i++)
		regmap_write(priv->cgu, PCIE_LCPLL_COEF(i), 0x0000ff60);
	for (i = 4; i < PCIE_LCPLL_COEF_NUM; i++)
		regmap_write(priv->cgu, PCIE_LCPLL_COEF(i), 0x000000a0);

	/* Program DIV and len parameters */
	regmap_write(priv->cgu, PCIE_LCPLL_SSC_CTRL, 0x0000ff1c);
	regmap_write(priv->cgu, PCIE_LCPLL_SSC_CTRL, 0x0000ff1f);
	regmap_write(priv->cgu, PCIE_LCPLL_SSC_CTRL, 0x0000ff1c);
	regmap_write(priv->cgu, PCIE_LCPLL_SSC_CTRL, 0x0000ff1d);
	mdelay(1);

	dev_dbg(priv->dev, "PCIe LCPLL SSC mode enabled\n");
}

static void intel_pciephy_ssc_disable(struct intel_pciephy *priv)
{
	if (!priv->ssc_en)
		return;

	guard(mutex)(&intel_pciephy_ssc_lock);

	if (!priv->ssc_on)
		return;

	priv->ssc_on = false;
	if (--intel_pciephy_ssc_refcount > 0) {
		dev_dbg(priv->dev, "SSC still in use, count: %d\n",
			intel_pciephy_ssc_refcount);
		return;
	}

	/* Disable SSC CTRL */
	regmap_write(priv->cgu, PCIE_LCPLL_SSC_CTRL, 0x0);
	mdelay(1);

	/* Disable SSC and LCPLL */
	regmap_update_bits(priv->cgu, PCIE_LCPLL_CFG1,
			   PLL_SSC | PLL_CLK_OV, PLL_SSC);

	/* Reset the PLL */
	regmap_update_bits(priv->cgu, PCIE_LCPLL_CFG0, PLL_RESET, 0);
	regmap_update_bits(priv->cgu, PCIE_LCPLL_CFG0, PLL_RESET, PLL_RESET);

	intel_pciephy_lcpll_wait_lock(priv);
}

static void intel_pciephy_reset(struct intel_pciephy *priv)
{
	reset_control_assert(priv->rst);
	reset_control_deassert(priv->rst);
	udelay(1);
}

static void intel_pciephy_ref_clk(struct intel_pciephy *priv, bool on)
{
	/* 0 is enable, 1 is disable */
	regmap_update_bits(priv->chiptop, priv->ifmux_off, PCIE_RCLK(priv->id),
			   on ? 0 : PCIE_RCLK(priv->id));
}

static int intel_pciephy_init(struct phy *phy)
{
	struct intel_pciephy *priv = phy_get_drvdata(phy);

	intel_pciephy_power(priv, false);
	intel_pciephy_ssc_enable(priv);
	intel_pciephy_power(priv, true);

	intel_pciephy_reset(priv);

	intel_pciephy_ref_clk(priv, true);
	intel_pciephy_dbg_dump(priv);

	return 0;
}

static int intel_pciephy_exit(struct phy *phy)
{
	struct intel_pciephy *priv = phy_get_drvdata(phy);

	/* Powering the PDI down halts the whole system, leave it alone */
	intel_pciephy_ssc_disable(priv);
	intel_pciephy_ref_clk(priv, false);
	intel_pciephy_dbg_dump(priv);

	return 0;
}

static const struct phy_ops intel_pciephy_ops = {
	.init = intel_pciephy_init,
	.exit = intel_pciephy_exit,
	.owner = THIS_MODULE,
};

static int intel_pciephy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct phy_provider *phy_provider;
	struct intel_pciephy *priv;
	u32 val;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	ret = device_property_read_u32(dev, "intel,phy-id", &priv->id);
	if (ret)
		return dev_err_probe(dev, ret, "missing intel,phy-id\n");
	if (priv->id > PCIE_PHY_ID_MAX)
		return dev_err_probe(dev, -EINVAL, "invalid intel,phy-id %u\n",
				     priv->id);

	priv->phy_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->phy_base))
		return dev_err_probe(dev, PTR_ERR(priv->phy_base),
				     "failed to map the PHY registers\n");

	priv->cgu = syscon_regmap_lookup_by_phandle(np, "intel,cgu-syscon");
	if (IS_ERR(priv->cgu))
		return dev_err_probe(dev, PTR_ERR(priv->cgu),
				     "failed to get the CGU syscon\n");

	priv->chiptop = syscon_regmap_lookup_by_phandle_args(np, "intel,syscon",
							     1,
							     &priv->ifmux_off);
	if (IS_ERR(priv->chiptop))
		return dev_err_probe(dev, PTR_ERR(priv->chiptop),
				     "failed to get the CHIPTOP syscon\n");

	priv->rst = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(priv->rst))
		return dev_err_probe(dev, PTR_ERR(priv->rst),
				     "failed to get the PHY reset\n");

	if (!device_property_read_u32(dev, "intel,ssc-enable", &val))
		priv->ssc_en = !!val;
	else
		priv->ssc_en = device_property_present(dev, "intel,ssc-enable");

	priv->phy = devm_phy_create(dev, np, &intel_pciephy_ops);
	if (IS_ERR(priv->phy))
		return dev_err_probe(dev, PTR_ERR(priv->phy),
				     "failed to create the PHY\n");

	phy_set_drvdata(priv->phy, priv);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return dev_err_probe(dev, PTR_ERR(phy_provider),
				     "failed to register the PHY provider\n");

	/* Leave the PHY in a known state before any consumer shows up */
	intel_pciephy_reset(priv);

	dev_dbg(dev, "PCIe slim PHY[%u] probed, SSC %s\n", priv->id,
		str_enabled_disabled(priv->ssc_en));

	return 0;
}

static const struct of_device_id intel_pciephy_match[] = {
	{ .compatible = "intel,grx500-pciephy" },
	{}
};
MODULE_DEVICE_TABLE(of, intel_pciephy_match);

static struct platform_driver intel_pciephy_driver = {
	.probe = intel_pciephy_probe,
	.driver = {
		.name = "intel-pcie-slim-phy",
		.of_match_table = intel_pciephy_match,
	},
};
module_platform_driver(intel_pciephy_driver);

MODULE_DESCRIPTION("Intel GRX500 PCIe slim PHY driver");
MODULE_LICENSE("GPL");
