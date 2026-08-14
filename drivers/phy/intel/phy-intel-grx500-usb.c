// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016-2017 Intel Corporation.
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * USB PHY driver for the Intel GRX500 (xRX500) SoC family: the chip-top AXI
 * endianness word, the lane polarity overrides and the clock, reset and VBUS
 * ordering the controller needs.
 *
 * The register offsets, bit positions and settling delays are reproduced from
 * the vendor BSP (drivers/phy/phy-grx500-usb.c and
 * drivers/usb/dwc3/dwc3-grx500.c); there is no hardware reference manual for
 * this SoC.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>

/*
 * Per-controller AXI configuration word in the chip-top syscon. It selects
 * the byte order the controller presents to the AXI fabric, which is why this
 * PHY driver exists at all: nothing else owns that register.
 */
#define USB_CFG_DEV_ENDIAN		BIT(9)
#define USB_CFG_HOST_ENDIAN		BIT(10)

/*
 * PHY PDI registers, relative to the PHY's own window. These two overrides
 * are the only registers in it this driver ever touches; the vendor driver
 * has no calibration, trim, impedance or squelch programming either.
 */
#define USB_PHY_TX_OVRD			0x4000
#define  USB_PHY_TX_INVERT		BIT(3)
#define USB_PHY_RX_OVRD			0x4014
#define  USB_PHY_RX_INVERT		BIT(1)

/*
 * Three settling delays, all of them the vendor's, all of them
 * unconditional and undocumented there: one after the endian write, one
 * between enabling the gate clock and releasing the PHY from reset, and one
 * between that and switching VBUS on. Nothing in the AVM 4.9 sources says
 * what is being waited for, and there is no hardware reference manual for
 * this SoC to check against, so they are reproduced as-is rather than
 * guessed at. This is the only kernel that is known to bring the block up.
 */
#define USB_PHY_ENDIAN_SETTLE_MS	100
#define USB_PHY_CLK_SETTLE_MS		100
#define USB_PHY_RESET_SETTLE_MS		100

struct intel_usbphy {
	struct device *dev;
	void __iomem *base;
	struct regmap *chiptop;
	unsigned int cfg_offset;
	struct clk *clk;
	struct reset_control *rst;
	struct regulator *vbus;
	struct phy *phy;
	bool invert_tx;
	bool invert_rx;
};

static void intel_usbphy_set_bit(struct intel_usbphy *priv, unsigned int reg,
				 u32 bit)
{
	u32 val;

	val = readl(priv->base + reg);
	val |= bit;
	writel(val, priv->base + reg);
}

/*
 * Host port big endian, device port little. Idempotent, and called from
 * two places on purpose -- see the comment at the probe-side call.
 */
static int intel_usbphy_set_endian(struct intel_usbphy *priv)
{
	int ret;

	ret = regmap_update_bits(priv->chiptop, priv->cfg_offset,
				 USB_CFG_HOST_ENDIAN | USB_CFG_DEV_ENDIAN,
				 USB_CFG_HOST_ENDIAN);
	if (ret)
		dev_err(priv->dev, "failed to select the AXI byte order\n");

	return ret;
}

static int intel_usbphy_init(struct phy *phy)
{
	struct intel_usbphy *priv = phy_get_drvdata(phy);
	int ret;

	ret = intel_usbphy_set_endian(priv);
	if (ret)
		return ret;

	msleep(USB_PHY_ENDIAN_SETTLE_MS);

	/*
	 * Lane polarity overrides, for boards that route the differential
	 * pair inverted. Absent properties leave the hardware default.
	 */
	if (priv->invert_tx)
		intel_usbphy_set_bit(priv, USB_PHY_TX_OVRD, USB_PHY_TX_INVERT);

	if (priv->invert_rx)
		intel_usbphy_set_bit(priv, USB_PHY_RX_OVRD, USB_PHY_RX_INVERT);

	return 0;
}

static int intel_usbphy_power_on(struct phy *phy)
{
	struct intel_usbphy *priv = phy_get_drvdata(phy);
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(priv->dev, "failed to enable the PHY gate clock\n");
		return ret;
	}

	msleep(USB_PHY_CLK_SETTLE_MS);

	ret = reset_control_deassert(priv->rst);
	if (ret) {
		dev_err(priv->dev, "failed to release the PHY reset\n");
		goto err_disable_clk;
	}

	msleep(USB_PHY_RESET_SETTLE_MS);

	if (priv->vbus) {
		ret = regulator_enable(priv->vbus);
		if (ret) {
			dev_err(priv->dev, "failed to switch VBUS on\n");
			goto err_assert_reset;
		}
	}

	return 0;

err_assert_reset:
	reset_control_assert(priv->rst);
err_disable_clk:
	clk_disable_unprepare(priv->clk);

	return ret;
}

static int intel_usbphy_power_off(struct phy *phy)
{
	struct intel_usbphy *priv = phy_get_drvdata(phy);

	if (priv->vbus)
		regulator_disable(priv->vbus);

	reset_control_assert(priv->rst);
	clk_disable_unprepare(priv->clk);

	return 0;
}

static const struct phy_ops intel_usbphy_ops = {
	.init = intel_usbphy_init,
	.power_on = intel_usbphy_power_on,
	.power_off = intel_usbphy_power_off,
	.owner = THIS_MODULE,
};

static int intel_usbphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct phy_provider *phy_provider;
	struct intel_usbphy *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return dev_err_probe(dev, PTR_ERR(priv->base),
				     "failed to map the PHY registers\n");

	priv->chiptop = syscon_regmap_lookup_by_phandle(np, "intel,syscon");
	if (IS_ERR(priv->chiptop))
		return dev_err_probe(dev, PTR_ERR(priv->chiptop),
				     "failed to get the CHIPTOP syscon\n");

	ret = device_property_read_u32(dev, "intel,syscon-offset",
				       &priv->cfg_offset);
	if (ret)
		return dev_err_probe(dev, ret, "missing intel,syscon-offset\n");

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk))
		return dev_err_probe(dev, PTR_ERR(priv->clk),
				     "failed to get the PHY gate clock\n");

	priv->rst = devm_reset_control_get_exclusive(dev, "phy");
	if (IS_ERR(priv->rst))
		return dev_err_probe(dev, PTR_ERR(priv->rst),
				     "failed to get the PHY reset\n");

	/*
	 * VBUS is optional: port power is not software-controlled on every
	 * board.
	 */
	priv->vbus = devm_regulator_get_optional(dev, "vbus");
	if (IS_ERR(priv->vbus)) {
		if (PTR_ERR(priv->vbus) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(priv->vbus),
					     "failed to get the VBUS supply\n");

		dev_dbg(dev, "no VBUS supply, assuming port power is fixed\n");
		priv->vbus = NULL;
	}

	priv->invert_tx = device_property_present(dev, "intel,invert-tx-polarity");
	priv->invert_rx = device_property_present(dev, "intel,invert-rx-polarity");


	priv->phy = devm_phy_create(dev, np, &intel_usbphy_ops);
	if (IS_ERR(priv->phy))
		return dev_err_probe(dev, PTR_ERR(priv->phy),
				     "failed to create the PHY\n");

	phy_set_drvdata(priv->phy, priv);

	/*
	 * The byte order must be set before the controller issues its first
	 * AXI transfer, which is earlier than .init runs, so it is programmed
	 * here too.
	 */
	ret = intel_usbphy_set_endian(priv);
	if (ret)
		return ret;

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return dev_err_probe(dev, PTR_ERR(phy_provider),
				     "failed to register the PHY provider\n");

	dev_dbg(dev, "USB PHY probed, AXI config at CHIPTOP + 0x%x\n",
		priv->cfg_offset);

	return 0;
}

static const struct of_device_id intel_usbphy_match[] = {
	{ .compatible = "intel,grx500-usb-phy" },
	{}
};
MODULE_DEVICE_TABLE(of, intel_usbphy_match);

static struct platform_driver intel_usbphy_driver = {
	.probe = intel_usbphy_probe,
	.driver = {
		.name = "intel-grx500-usb-phy",
		.of_match_table = intel_usbphy_match,
	},
};
module_platform_driver(intel_usbphy_driver);

MODULE_DESCRIPTION("Intel GRX500 USB PHY driver");
MODULE_LICENSE("GPL");
