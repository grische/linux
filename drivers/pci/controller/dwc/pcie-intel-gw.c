// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Intel Gateway SoCs
 *
 * Copyright (c) 2019 Intel Corporation.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/pci_regs.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "../../pci.h"
#include "pcie-designware.h"

#define PORT_AFR_N_FTS_GEN12_DFT	(SZ_128 - 1)
#define PORT_AFR_N_FTS_GEN3		180
#define PORT_AFR_N_FTS_GEN4		196

/* PCIe Application logic Registers */
#define PCIE_APP_CCR			0x10
#define PCIE_APP_CCR_LTSSM_ENABLE	BIT(0)

#define PCIE_APP_MSG_CR			0x30
#define PCIE_APP_MSG_XMT_PM_TURNOFF	BIT(0)

#define PCIE_APP_PMC			0x44
#define PCIE_APP_PMC_IN_L2		BIT(20)

#define PCIE_APP_IRNEN			0xF4
#define PCIE_APP_IRNCR			0xF8
#define PCIE_APP_IRN_AER_REPORT		BIT(0)
#define PCIE_APP_IRN_PME		BIT(2)
#define PCIE_APP_IRN_RX_VDM_MSG		BIT(4)
#define PCIE_APP_IRN_PM_TO_ACK		BIT(9)
#define PCIE_APP_IRN_LINK_AUTO_BW_STAT	BIT(11)
#define PCIE_APP_IRN_BW_MGT		BIT(12)
#define PCIE_APP_IRN_INTA		BIT(13)
#define PCIE_APP_IRN_INTB		BIT(14)
#define PCIE_APP_IRN_INTC		BIT(15)
#define PCIE_APP_IRN_INTD		BIT(16)
#define PCIE_APP_IRN_MSG_LTR		BIT(18)
#define PCIE_APP_IRN_SYS_ERR_RC		BIT(29)
#define PCIE_APP_INTX_OFST		12

#define PCIE_APP_IRN_INTX \
	(PCIE_APP_IRN_INTA | PCIE_APP_IRN_INTB | \
	PCIE_APP_IRN_INTC | PCIE_APP_IRN_INTD)

#define PCIE_APP_IRN_INT \
	(PCIE_APP_IRN_AER_REPORT | PCIE_APP_IRN_PME | \
	PCIE_APP_IRN_RX_VDM_MSG | PCIE_APP_IRN_SYS_ERR_RC | \
	PCIE_APP_IRN_PM_TO_ACK | PCIE_APP_IRN_MSG_LTR | \
	PCIE_APP_IRN_BW_MGT | PCIE_APP_IRN_LINK_AUTO_BW_STAT | \
	PCIE_APP_IRN_INTX)

/*
 * Byte-swap control for the three xRX500 root complexes, in the chiptop
 * syscon rather than in the controller itself. One bit per direction per
 * RC; which bits belong to which RC is described by "intel,inbound-shift"
 * and "intel,outbound-shift" in the devicetree.
 */
#define PCIE_CHIPTOP_ENDIAN		0x4c

/* xRX500 decodes its 8 MiB config window with only three bits of bus number */
#define PCIE_XRX500_CFG_BUS(x)		(((x) & 0x7) << 20)
#define PCIE_XRX500_CFG_DEV(x)		(((x) & 0x1f) << 15)
#define PCIE_XRX500_CFG_FUNC(x)		(((x) & 0x7) << 12)
#define PCIE_XRX500_CFG_REG(x)		((x) & 0xffc)

#define RESET_INTERVAL_MS		100

/**
 * struct intel_pcie_soc - Per-SoC quirks and hooks
 * @dw_pcie_ops: DesignWare core accessor overrides for this SoC
 * @host_ops: DesignWare host bridge hooks for this SoC
 * @irn_mask: Application logic interrupts to unmask once the link is up
 * @core_rst_optional: SoC has no core reset line of its own
 * @native_ecam: Glue decodes the config window itself, keep generic ECAM away
 * @needs_endian_syscon: Byte swapping lives outside the controller, "intel,syscon"
 *			 is not optional here
 */
struct intel_pcie_soc {
	const struct dw_pcie_ops	*dw_pcie_ops;
	const struct dw_pcie_host_ops	*host_ops;
	u32				irn_mask;
	bool				core_rst_optional;
	bool				native_ecam;
	bool				needs_endian_syscon;
};

struct intel_pcie {
	struct dw_pcie		pci;
	const struct intel_pcie_soc *soc;
	void __iomem		*app_base;
	struct gpio_desc	*reset_gpio;
	u32			rst_intrvl;
	struct clk		*core_clk;
	struct reset_control	*core_rst;
	struct phy		*phy;
	struct regmap		*syscon;
	u32			inbound_shift;
	u32			outbound_shift;
	u32			inbound_swap;
	u32			outbound_swap;
};

static void pcie_update_bits(void __iomem *base, u32 ofs, u32 mask, u32 val)
{
	u32 old;

	old = readl(base + ofs);
	val = (old & ~mask) | (val & mask);

	if (val != old)
		writel(val, base + ofs);
}

static inline void pcie_app_wr(struct intel_pcie *pcie, u32 ofs, u32 val)
{
	writel(val, pcie->app_base + ofs);
}

static void pcie_app_wr_mask(struct intel_pcie *pcie, u32 ofs,
			     u32 mask, u32 val)
{
	pcie_update_bits(pcie->app_base, ofs, mask, val);
}

static inline u32 pcie_rc_cfg_rd(struct intel_pcie *pcie, u32 ofs)
{
	return dw_pcie_readl_dbi(&pcie->pci, ofs);
}

static inline void pcie_rc_cfg_wr(struct intel_pcie *pcie, u32 ofs, u32 val)
{
	dw_pcie_writel_dbi(&pcie->pci, ofs, val);
}

static void pcie_rc_cfg_wr_mask(struct intel_pcie *pcie, u32 ofs,
				u32 mask, u32 val)
{
	pcie_update_bits(pcie->pci.dbi_base, ofs, mask, val);
}

/*
 * The xRX500 answers DBI and configuration reads with a numerically correct
 * value in the wrong byte order, so every access through this window has to
 * be swapped in software. The quirk is per-SoC, not per-board.
 */
static u32 intel_pcie_xrx500_read_dbi(struct dw_pcie *pci, void __iomem *base,
				      u32 reg, size_t size)
{
	u32 val = readl(base + (reg & ~0x3));

	if (size == 4)
		return val;

	val >>= (reg & 0x3) * BITS_PER_BYTE;

	return val & (BIT(size * BITS_PER_BYTE) - 1);
}

static void intel_pcie_xrx500_write_dbi(struct dw_pcie *pci, void __iomem *base,
					u32 reg, size_t size, u32 val)
{
	void __iomem *addr = base + (reg & ~0x3);
	u32 mask, shift;

	if (size == 4) {
		writel(val, addr);
		return;
	}

	shift = (reg & 0x3) * BITS_PER_BYTE;
	mask = (BIT(size * BITS_PER_BYTE) - 1) << shift;

	writel((readl(addr) & ~mask) | ((val << shift) & mask), addr);
}

static const struct dw_pcie_ops intel_pcie_xrx500_ops = {
	.read_dbi	= intel_pcie_xrx500_read_dbi,
	.write_dbi	= intel_pcie_xrx500_write_dbi,
};

/*
 * The root bus does not go through .read_dbi/.write_dbi -- dw_pcie_ops
 * installs pci_generic_config_read()/write(), which reach dbi_base with a
 * bare readb()/readw() and land in the same trap. The 32-bit variants read
 * and write whole dwords and do the lane extraction themselves, which is
 * exactly what this controller needs.
 */
static int intel_pcie_xrx500_rd_own_conf(struct pci_bus *bus, unsigned int devfn,
					 int where, int size, u32 *val)
{
	/* RC BAR0 and BAR1 are not usable on this controller. */
	if ((where & ~0x3) == PCI_BASE_ADDRESS_0 ||
	    (where & ~0x3) == PCI_BASE_ADDRESS_1) {
		*val = 0;
		return PCIBIOS_SUCCESSFUL;
	}

	return pci_generic_config_read32(bus, devfn, where, size, val);
}

static struct pci_ops intel_pcie_xrx500_own_ops = {
	.map_bus	= dw_pcie_own_conf_map_bus,
	.read		= intel_pcie_xrx500_rd_own_conf,
	.write		= pci_generic_config_write32,
};

static void __iomem *intel_pcie_xrx500_child_map_bus(struct pci_bus *bus,
						     unsigned int devfn,
						     int where)
{
	struct dw_pcie_rp *pp = bus->sysdata;

	/* A point-to-point link has exactly one device on it. */
	if (pci_is_root_bus(bus->parent) && PCI_SLOT(devfn) > 0)
		return NULL;

	if (!dw_pcie_link_up(to_dw_pcie_from_pp(pp)))
		return NULL;

	return pp->va_cfg0_base + (PCIE_XRX500_CFG_BUS(bus->number) |
				   PCIE_XRX500_CFG_DEV(PCI_SLOT(devfn)) |
				   PCIE_XRX500_CFG_FUNC(PCI_FUNC(devfn)) |
				   PCIE_XRX500_CFG_REG(where));
}

/*
 * This controller decodes the "config" window itself; there is no address
 * translation unit in the path, which is why the vendor devicetree says
 * "intel,iatu = <0>". Providing .child_ops at all is what tells
 * dw_pcie_setup_rc() so: it programs the iATU only when child_ops is still
 * the DesignWare default. The eight bus numbers the window decodes are also
 * why the devicetree bus-range must stop at 7 -- bus 8 would alias onto DBI.
 */
static struct pci_ops intel_pcie_xrx500_child_ops = {
	.map_bus	= intel_pcie_xrx500_child_map_bus,
	.read		= pci_generic_config_read32,
	.write		= pci_generic_config_write32,
};

static void intel_pcie_endian_setup(struct intel_pcie *pcie)
{
	struct device *dev = pcie->pci.dev;
	u32 mask, val = 0;

	if (!pcie->syscon)
		return;

	mask = BIT(pcie->inbound_shift) | BIT(pcie->outbound_shift);

	/*
	 * The chiptop swappers exist for a big-endian CPU; the vendor BSP
	 * compiles the "set" side out on little-endian and clears both bits
	 * unconditionally there. Keep that, but as a plain condition so both
	 * arms stay compile-checked.
	 */
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN)) {
		if (pcie->inbound_swap)
			val |= BIT(pcie->inbound_shift);
		if (pcie->outbound_swap)
			val |= BIT(pcie->outbound_shift);
	}

	regmap_update_bits(pcie->syscon, PCIE_CHIPTOP_ENDIAN, mask, val);

	if (!regmap_read(pcie->syscon, PCIE_CHIPTOP_ENDIAN, &val))
		dev_info(dev, "chiptop endian %#04x: %#010x (in bit %u swap %u, out bit %u swap %u)\n",
			 PCIE_CHIPTOP_ENDIAN, val, pcie->inbound_shift,
			 pcie->inbound_swap, pcie->outbound_shift,
			 pcie->outbound_swap);
}

static void intel_pcie_ltssm_enable(struct intel_pcie *pcie)
{
	pcie_app_wr_mask(pcie, PCIE_APP_CCR, PCIE_APP_CCR_LTSSM_ENABLE,
			 PCIE_APP_CCR_LTSSM_ENABLE);
}

static void intel_pcie_ltssm_disable(struct intel_pcie *pcie)
{
	pcie_app_wr_mask(pcie, PCIE_APP_CCR, PCIE_APP_CCR_LTSSM_ENABLE, 0);
}

static void intel_pcie_link_setup(struct intel_pcie *pcie)
{
	u32 val;
	u8 offset = dw_pcie_find_capability(&pcie->pci, PCI_CAP_ID_EXP);

	val = pcie_rc_cfg_rd(pcie, offset + PCI_EXP_LNKCTL);

	val &= ~(PCI_EXP_LNKCTL_LD | PCI_EXP_LNKCTL_ASPMC);
	pcie_rc_cfg_wr(pcie, offset + PCI_EXP_LNKCTL, val);
}

static void intel_pcie_init_n_fts(struct dw_pcie *pci)
{
	switch (pci->max_link_speed) {
	case 3:
		pci->n_fts[1] = PORT_AFR_N_FTS_GEN3;
		break;
	case 4:
		pci->n_fts[1] = PORT_AFR_N_FTS_GEN4;
		break;
	default:
		pci->n_fts[1] = PORT_AFR_N_FTS_GEN12_DFT;
		break;
	}
	pci->n_fts[0] = PORT_AFR_N_FTS_GEN12_DFT;
}

static int intel_pcie_ep_rst_init(struct intel_pcie *pcie)
{
	struct device *dev = pcie->pci.dev;
	int ret;

	pcie->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(pcie->reset_gpio)) {
		ret = PTR_ERR(pcie->reset_gpio);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to request PCIe GPIO: %d\n", ret);
		return ret;
	}

	/* Make initial reset last for 100us */
	usleep_range(100, 200);

	return 0;
}

static void intel_pcie_core_rst_assert(struct intel_pcie *pcie)
{
	reset_control_assert(pcie->core_rst);
}

static void intel_pcie_core_rst_deassert(struct intel_pcie *pcie)
{
	/*
	 * One micro-second delay to make sure the reset pulse
	 * wide enough so that core reset is clean.
	 */
	udelay(1);
	reset_control_deassert(pcie->core_rst);

	/*
	 * Some SoC core reset also reset PHY, more delay needed
	 * to make sure the reset process is done.
	 */
	usleep_range(1000, 2000);
}

static void intel_pcie_device_rst_assert(struct intel_pcie *pcie)
{
	gpiod_set_value_cansleep(pcie->reset_gpio, 1);
}

static void intel_pcie_device_rst_deassert(struct intel_pcie *pcie)
{
	msleep(pcie->rst_intrvl);
	gpiod_set_value_cansleep(pcie->reset_gpio, 0);
}

static void intel_pcie_core_irq_disable(struct intel_pcie *pcie)
{
	pcie_app_wr(pcie, PCIE_APP_IRNEN, 0);
	pcie_app_wr(pcie, PCIE_APP_IRNCR, pcie->soc->irn_mask);
}

static int intel_pcie_get_resources(struct platform_device *pdev)
{
	struct intel_pcie *pcie = platform_get_drvdata(pdev);
	struct dw_pcie *pci = &pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	pcie->core_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(pcie->core_clk)) {
		ret = PTR_ERR(pcie->core_clk);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to get clks: %d\n", ret);
		return ret;
	}

	if (pcie->soc->core_rst_optional)
		pcie->core_rst = devm_reset_control_get_optional_exclusive(dev,
									  NULL);
	else
		pcie->core_rst = devm_reset_control_get(dev, NULL);
	if (IS_ERR(pcie->core_rst)) {
		ret = PTR_ERR(pcie->core_rst);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to get resets: %d\n", ret);
		return ret;
	}

	ret = device_property_read_u32(dev, "reset-assert-ms",
				       &pcie->rst_intrvl);
	if (ret)
		pcie->rst_intrvl = RESET_INTERVAL_MS;

	pcie->app_base = devm_platform_ioremap_resource_byname(pdev, "app");
	if (IS_ERR(pcie->app_base))
		return PTR_ERR(pcie->app_base);

	pcie->phy = devm_phy_get(dev, "pcie");
	if (IS_ERR(pcie->phy)) {
		ret = PTR_ERR(pcie->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Couldn't get pcie-phy: %d\n", ret);
		return ret;
	}

	pcie->syscon = syscon_regmap_lookup_by_phandle_optional(dev->of_node,
								"intel,syscon");
	if (IS_ERR(pcie->syscon))
		return dev_err_probe(dev, PTR_ERR(pcie->syscon),
				     "Failed to look up \"intel,syscon\"\n");

	/*
	 * syscon_regmap_lookup_by_phandle_optional() returns NULL both when
	 * the property is absent and when the lookup fails, so the two cases
	 * have to be told apart by checking for the property first.
	 */
	if (pcie->soc->needs_endian_syscon && !pcie->syscon)
		return dev_err_probe(dev, -EINVAL,
				     "\"intel,syscon\" is required on this SoC\n");

	if (pcie->syscon) {
		/*
		 * Which bits of the chiptop register belong to this root
		 * complex is not derivable from anything else the node
		 * carries, and getting it wrong means silently swapping some
		 * other RC's traffic, so refuse to guess. The vendor BSP
		 * defaults both to zero and writes bit 0 twice when the
		 * properties are missing.
		 */
		ret = device_property_read_u32(dev, "intel,inbound-shift",
					       &pcie->inbound_shift);
		if (!ret)
			ret = device_property_read_u32(dev, "intel,outbound-shift",
						       &pcie->outbound_shift);
		if (ret)
			return dev_err_probe(dev, ret,
					     "\"intel,syscon\" needs intel,inbound-shift and intel,outbound-shift\n");

		if (pcie->inbound_shift > 31 || pcie->outbound_shift > 31)
			return dev_err_probe(dev, -ERANGE,
					     "Endian bit %u/%u is outside the chiptop register\n",
					     pcie->inbound_shift,
					     pcie->outbound_shift);

		device_property_read_u32(dev, "intel,inbound-swap",
					 &pcie->inbound_swap);
		device_property_read_u32(dev, "intel,outbound-swap",
					 &pcie->outbound_swap);
	}

	return 0;
}

static int intel_pcie_wait_l2(struct intel_pcie *pcie)
{
	u32 value;
	int ret;
	struct dw_pcie *pci = &pcie->pci;

	if (pci->max_link_speed < 3)
		return 0;

	/* Send PME_TURN_OFF message */
	pcie_app_wr_mask(pcie, PCIE_APP_MSG_CR, PCIE_APP_MSG_XMT_PM_TURNOFF,
			 PCIE_APP_MSG_XMT_PM_TURNOFF);

	/* Read PMC status and wait for falling into L2 link state */
	ret = readl_poll_timeout(pcie->app_base + PCIE_APP_PMC, value,
				 value & PCIE_APP_PMC_IN_L2, 20,
				 jiffies_to_usecs(5 * HZ));
	if (ret)
		dev_err(pcie->pci.dev, "PCIe link enter L2 timeout!\n");

	return ret;
}

static void intel_pcie_turn_off(struct intel_pcie *pcie)
{
	if (dw_pcie_link_up(&pcie->pci))
		intel_pcie_wait_l2(pcie);

	/* Put endpoint device in reset state */
	intel_pcie_device_rst_assert(pcie);
	pcie_rc_cfg_wr_mask(pcie, PCI_COMMAND, PCI_COMMAND_MEMORY, 0);
}

static int intel_pcie_host_setup(struct intel_pcie *pcie)
{
	int ret;
	struct dw_pcie *pci = &pcie->pci;

	/*
	 * Configure the chiptop byte swappers before anything else touches the
	 * controller: they sit in the path the DBI and configuration windows
	 * ride on, and the syscon block they live in is reachable while the
	 * core is still in reset and its clock still gated. The vendor BSP
	 * does this once in probe and never again, so its setting does not
	 * survive a suspend/resume cycle; doing it here covers ->init and
	 * intel_pcie_resume_noirq() alike.
	 */
	intel_pcie_endian_setup(pcie);

	intel_pcie_core_rst_assert(pcie);
	intel_pcie_device_rst_assert(pcie);

	ret = phy_init(pcie->phy);
	if (ret)
		return ret;

	intel_pcie_core_rst_deassert(pcie);

	ret = clk_prepare_enable(pcie->core_clk);
	if (ret) {
		dev_err(pcie->pci.dev, "Core clock enable failed: %d\n", ret);
		goto clk_err;
	}

	pci->atu_base = pci->dbi_base + 0xC0000;

	intel_pcie_ltssm_disable(pcie);
	intel_pcie_link_setup(pcie);
	intel_pcie_init_n_fts(pci);

	ret = dw_pcie_setup_rc(&pci->pp);
	if (ret)
		goto app_init_err;

	dw_pcie_upconfig_setup(pci);

	intel_pcie_device_rst_deassert(pcie);
	intel_pcie_ltssm_enable(pcie);

	ret = dw_pcie_wait_for_link(pci);
	if (ret)
		goto app_init_err;

	/*
	 * Clear any pending status before unmasking, as the vendor driver
	 * does per pin.
	 */
	pcie_app_wr(pcie, PCIE_APP_IRNCR, pcie->soc->irn_mask);

	/* Enable integrated interrupts */
	pcie_app_wr_mask(pcie, PCIE_APP_IRNEN, pcie->soc->irn_mask,
			 pcie->soc->irn_mask);

	return 0;

app_init_err:
	clk_disable_unprepare(pcie->core_clk);
clk_err:
	intel_pcie_core_rst_assert(pcie);
	phy_exit(pcie->phy);

	return ret;
}

static void __intel_pcie_remove(struct intel_pcie *pcie)
{
	intel_pcie_core_irq_disable(pcie);
	intel_pcie_turn_off(pcie);
	clk_disable_unprepare(pcie->core_clk);
	intel_pcie_core_rst_assert(pcie);
	phy_exit(pcie->phy);
}

static void intel_pcie_remove(struct platform_device *pdev)
{
	struct intel_pcie *pcie = platform_get_drvdata(pdev);
	struct dw_pcie_rp *pp = &pcie->pci.pp;

	dw_pcie_host_deinit(pp);
	__intel_pcie_remove(pcie);
}

static int intel_pcie_suspend_noirq(struct device *dev)
{
	struct intel_pcie *pcie = dev_get_drvdata(dev);
	int ret;

	intel_pcie_core_irq_disable(pcie);
	ret = intel_pcie_wait_l2(pcie);
	if (ret)
		return ret;

	phy_exit(pcie->phy);
	clk_disable_unprepare(pcie->core_clk);
	return ret;
}

static int intel_pcie_resume_noirq(struct device *dev)
{
	struct intel_pcie *pcie = dev_get_drvdata(dev);

	return intel_pcie_host_setup(pcie);
}

static int intel_pcie_rc_init(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct intel_pcie *pcie = dev_get_drvdata(pci->dev);

	return intel_pcie_host_setup(pcie);
}

static const struct dw_pcie_ops intel_pcie_ops = {
};

static const struct dw_pcie_host_ops intel_pcie_dw_ops = {
	.init = intel_pcie_rc_init,
};

/*
 * INTx only: INTA-D reach the GIC over their own lines, and there is no MSI
 * controller behind this root complex.
 */
static int intel_pcie_xrx500_msi_init(struct dw_pcie_rp *pp)
{
	return 0;
}

static int intel_pcie_xrx500_rc_init(struct dw_pcie_rp *pp)
{
	pp->bridge->ops = &intel_pcie_xrx500_own_ops;
	pp->bridge->child_ops = &intel_pcie_xrx500_child_ops;

	return intel_pcie_rc_init(pp);
}

static const struct dw_pcie_host_ops intel_pcie_xrx500_dw_ops = {
	.init = intel_pcie_xrx500_rc_init,
	.msi_init = intel_pcie_xrx500_msi_init,
};

static const struct intel_pcie_soc lgm_pcie_soc = {
	.dw_pcie_ops		= &intel_pcie_ops,
	.host_ops		= &intel_pcie_dw_ops,
	.irn_mask		= PCIE_APP_IRN_INT,
};

static const struct intel_pcie_soc xrx500_pcie_soc = {
	.dw_pcie_ops		= &intel_pcie_xrx500_ops,
	.host_ops		= &intel_pcie_xrx500_dw_ops,
	/* Unmask the INTx sources and nothing else. */
	.irn_mask		= PCIE_APP_IRN_INTX,
	/*
	 * No core reset of its own: the controller leaves reset with the block
	 * it sits in, and the vendor devicetree describes no "resets" here.
	 */
	.core_rst_optional	= true,
	/*
	 * The controller decodes the "config" window itself; it is not
	 * ECAM-shaped, so the bus number is masked to three bits and
	 * bus-range stops at 7.
	 */
	.native_ecam		= true,
	/*
	 * Byte swapping for this SoC is in the chiptop syscon rather than in
	 * the controller, so an absent regmap is not a missing nicety, it is a
	 * datapath left in an unknown endianness. See intel_pcie_get_resources().
	 */
	.needs_endian_syscon	= true,
};

static int intel_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct intel_pcie *pcie;
	struct dw_pcie_rp *pp;
	struct dw_pcie *pci;
	int ret;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->soc = device_get_match_data(dev);
	if (!pcie->soc)
		return -ENODEV;

	platform_set_drvdata(pdev, pcie);
	pci = &pcie->pci;
	pci->dev = dev;
	pci->use_parent_dt_ranges = true;
	pp = &pci->pp;

	ret = intel_pcie_get_resources(pdev);
	if (ret)
		return ret;

	ret = intel_pcie_ep_rst_init(pcie);
	if (ret)
		return ret;

	pci->ops = pcie->soc->dw_pcie_ops;
	pp->ops = pcie->soc->host_ops;
	pp->native_ecam = pcie->soc->native_ecam;

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "Cannot initialize host\n");
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops intel_pcie_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(intel_pcie_suspend_noirq,
				  intel_pcie_resume_noirq)
};

static const struct of_device_id of_intel_pcie_match[] = {
	{ .compatible = "intel,lgm-pcie", .data = &lgm_pcie_soc },
	{ .compatible = "intel,xrx500-pcie", .data = &xrx500_pcie_soc },
	{}
};

static struct platform_driver intel_pcie_driver = {
	.probe = intel_pcie_probe,
	.remove = intel_pcie_remove,
	.driver = {
		.name = "intel-gw-pcie",
		.of_match_table = of_intel_pcie_match,
		.pm = &intel_pcie_pm_ops,
	},
};
builtin_platform_driver(intel_pcie_driver);
