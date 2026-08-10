// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * MDIO bus on a GSWIP-3.0 instance. The bus is a sub-aperture of the switch
 * register window, so the region is claimed by the parent and mapped without
 * claiming it again, and the block is accessed natively on this big-endian
 * SoC.
 */

#include <linux/bits.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "gsw30_reg_top.h"
#include "../include/xrx500_phy_fw.h"

#define LTQ_GSWIP_MDIO_SUBAP_DWORD_BASE \
	(GSWT_MDCTRL_MBUSY_OFFSET + GSW30_TOP_OFFSET)
#define LTQ_GSWIP_MDIO_SUBAP_BYTE_BASE    (LTQ_GSWIP_MDIO_SUBAP_DWORD_BASE * 4)

/*
 * Byte offsets WITHIN the sub-aperture for the three registers the
 * bus callbacks touch. Sub-aperture base == MDCTRL, so:
 *   MDCTRL == 0x000, MDREAD == 0x004, MDWRITE == 0x008.
 */
#define LTQ_GSWIP_MDIO_CTRL_OFF \
	(((GSWT_MDCTRL_MBUSY_OFFSET  + GSW30_TOP_OFFSET) * 4) - LTQ_GSWIP_MDIO_SUBAP_BYTE_BASE)
#define LTQ_GSWIP_MDIO_READ_OFF \
	(((GSWT_MDREAD_RDATA_OFFSET  + GSW30_TOP_OFFSET) * 4) - LTQ_GSWIP_MDIO_SUBAP_BYTE_BASE)
#define LTQ_GSWIP_MDIO_WRITE_OFF \
	(((GSWT_MDWRITE_WDATA_OFFSET + GSW30_TOP_OFFSET) * 4) - LTQ_GSWIP_MDIO_SUBAP_BYTE_BASE)

/* MBUSY single-bit at MDCTRL[12]. */
#define LTQ_GSWIP_MDIO_CTRL_MBUSY  BIT(GSWT_MDCTRL_MBUSY_SHIFT)

/*
 * The mainline DSA driver (drivers/net/dsa/lantiq/lantiq_gswip.c:634-637)
 * programs these at probe time so the MDIO bus has a known good clock rate
 * before any transactions issue. Without explicit programming, the post-reset
 * default may leave MCEN clear or FREQ at an unsupported value, which causes
 * MDIO transactions to silently never complete (MBUSY stays stuck) and the
 * 10ms readl_poll_timeout cap then surfaces -ETIMEDOUT.
 *
 * AVM's mdio_init_gswip3_0() (switch-api/gsw_flow_core.c:2733-2752)
 * programs MDCCFG_1 on GSWIP-3.0: FREQ=0x9 (2.5 MHz), MCEN=1, RES=1
 * (MDIO hardware reset). The GSWIP-3.0 register byte offsets are:
 *
 *   MDCCFG_0 = (0x007 + 0xF00) * 4 = 0x3C1C  -> aperture offset 0x00C
 *   MDCCFG_1 = (0x008 + 0xF00) * 4 = 0x3C20  -> aperture offset 0x010
 *
 * MDCCFG_1 RES (bit 15) MDIO HW reset, MCEN (bit 8) master-clock enable, FREQ
 * (bits 7:0) = 0x9 -> 2.5 MHz MDC.
 */
#define LTQ_GSWIP_MDIO_MDC_CFG_0_OFF \
	(((GSWT_MDCCFG_0_PEN_ALL_OFFSET + GSW30_TOP_OFFSET) * 4) - LTQ_GSWIP_MDIO_SUBAP_BYTE_BASE)
#define LTQ_GSWIP_MDIO_MDC_CFG_1_OFF \
	(((GSWT_MDCCFG_1_FREQ_OFFSET    + GSW30_TOP_OFFSET) * 4) - LTQ_GSWIP_MDIO_SUBAP_BYTE_BASE)

/* MDCCFG_1 field values (AVM mdio_init_gswip3_0). */
#define LTQ_GSWIP_MDIO_MDC_CFG_1_RES   BIT(GSWT_MDCCFG_1_RES_SHIFT)   /* bit 15 */
#define LTQ_GSWIP_MDIO_MDC_CFG_1_MCEN  BIT(GSWT_MDCCFG_1_MCEN_SHIFT)  /* bit 8 */
#define LTQ_GSWIP_MDIO_MDC_CFG_1_FREQ_DEFAULT  0x09                   /* 2.5 MHz */
#define LTQ_GSWIP_MDIO_MDC_CFG_1_INIT  \
	(LTQ_GSWIP_MDIO_MDC_CFG_1_RES | LTQ_GSWIP_MDIO_MDC_CFG_1_MCEN | \
	 LTQ_GSWIP_MDIO_MDC_CFG_1_FREQ_DEFAULT)

/*
 * OP[1:0] = MDIO_CTRL[11:10] encodes the bus operation.
 *
 *   OP = 0b10 -> READ   (datasheet review focus #7; do not swap)
 *   OP = 0b01 -> WRITE
 *
 * AVM packs the control word as ((OP << 10) | (PHYAD << 5) | REGAD)
 * with OP=0x2 for read, 0x1 for write (gsw_flow_core.c:12016,12071).
 * Use GSWT_MDCTRL_OP_SHIFT (=10) as the shift base so a future
 * re-numbering of the field stays single-sourced.
 */
#define LTQ_GSWIP_MDIO_OP_READ   (2u << GSWT_MDCTRL_OP_SHIFT)
#define LTQ_GSWIP_MDIO_OP_WRITE  (1u << GSWT_MDCTRL_OP_SHIFT)

/* 5-bit PHYAD and REGAD field masks (each is 5 bits wide). */
#define LTQ_GSWIP_MDIO_PHYAD_MASK  GENMASK(GSWT_MDCTRL_PHYAD_SIZE - 1, 0)
#define LTQ_GSWIP_MDIO_REGAD_MASK  GENMASK(GSWT_MDCTRL_REGAD_SIZE - 1, 0)

/*
 * The MDIO control word convention is calibrated at probe: PHYID1 is read
 * with each candidate encoding and the one that answers is kept. MBUSY lives
 * in the control register on this instance.
 */
static bool ltq_gswip_mdio_ctrl_mbusy; /* false = AVM-exact (MBUSY=0) */

static inline u32 ltq_gswip_mdio_ctrl_word(u32 op, int addr, int reg)
{
	return (ltq_gswip_mdio_ctrl_mbusy ? LTQ_GSWIP_MDIO_CTRL_MBUSY : 0)
	     | op
	     | (((u32)addr & LTQ_GSWIP_MDIO_PHYAD_MASK) << GSWT_MDCTRL_PHYAD_SHIFT)
	     | (((u32)reg  & LTQ_GSWIP_MDIO_REGAD_MASK) << GSWT_MDCTRL_REGAD_SHIFT);
}

#define LTQ_GSWIP_MDIO_POLL_INTERVAL_US  1
#define LTQ_GSWIP_MDIO_POLL_TIMEOUT_US   10000

/**
 * struct ltq_gswip_mdio_priv - per-bus state.
 * @base:  ioremap'd sub-aperture covering MDIO_CTRL / MDIO_READ /
 *         MDIO_WRITE (DT reg = <0x1c000020 0x20>).
 * @dev:   probing device, retained for dev_err / dev_info.
 *
 * Allocated inline at the tail of devm_mdiobus_alloc_size's mii_bus
 * via bus->priv — devm-managed, freed automatically on probe failure
 * or device unbind.
 */
struct ltq_gswip_mdio_priv {
	void __iomem *base;
	struct device *dev;
	struct mii_bus *bus;	/* for mdio_lock serialization */
	struct delayed_work gsw_reinit_work;
};

int gsw_hw_reinit_gswl(void);

static int ltq_gswip_mdio_poll_mbusy(struct ltq_gswip_mdio_priv *priv);

/* Raw C22 read on priv->base (no mii_bus) — probe-time CTRL calibration. */
static int ltq_gswip_mdio_dbg_rd(struct ltq_gswip_mdio_priv *priv,
				 int addr, int reg)
{
	u32 cmd;

	if (ltq_gswip_mdio_poll_mbusy(priv))
		return -1;
	cmd = ltq_gswip_mdio_ctrl_word(LTQ_GSWIP_MDIO_OP_READ, addr, reg);
	__raw_writel(cmd, priv->base + LTQ_GSWIP_MDIO_CTRL_OFF);
	if (ltq_gswip_mdio_poll_mbusy(priv))
		return -1;
	return __raw_readl(priv->base + LTQ_GSWIP_MDIO_READ_OFF) & 0xFFFF;
}

/*
 * The vendor PHY driver sets MII_CTRL1000 bit 10 (prefer multi-port device)
 * before every autonegotiation (lantiq.c, vr9_gphy_config_aneg); genphy does
 * not, and the resulting 1000BASE-T slave role makes this PHY recover its
 * clock from the link partner. Registered as a phylib fixup.
 */
#define LTQ_GPHY11G_PHY_ID	0xd565a409
#define LTQ_GPHY11G_PHY_MASK	0xfffffff8
#define LTQ_CTL1000_MULTIPORT	BIT(10)

static int ltq_gswip_gphy_mpd_fixup(struct phy_device *phydev)
{
	int ret;

	ret = genphy_soft_reset(phydev);
	if (ret)
		return ret;

	ret = phy_write_mmd(phydev, 0x1f, 0x707, 0x11);
	if (ret)
		return ret;

	ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV, 0);
	if (ret)
		return ret;
	phy_disable_eee(phydev);

	return phy_set_bits(phydev, MII_CTRL1000, LTQ_CTL1000_MULTIPORT);
}

static void ltq_gswip_mdio_calibrate_ctrl(struct ltq_gswip_mdio_priv *priv)
{
	int id;

	ltq_gswip_mdio_ctrl_mbusy = false;
	id = ltq_gswip_mdio_dbg_rd(priv, 5, 2 /* PHYID1 */);
	if ((id & 0xFFFF) == 0xD565) {
		dev_info(priv->dev,
			 "mdio: CTRL calibration: MBUSY=0 (AVM-exact) verified, PHYID1=0x%04x\n",
			 id & 0xFFFF);
		return;
	}

	ltq_gswip_mdio_ctrl_mbusy = true;
	id = ltq_gswip_mdio_dbg_rd(priv, 5, 2);
	dev_warn(priv->dev,
		 "mdio: CTRL calibration: MBUSY=0 FAILED, fallback MBUSY=1 active (PHYID1=0x%04x, want 0xd565)\n",
		 id & 0xFFFF);
}

/*
 * One-shot GSW-L switch-core re-init, matching the vendor's GSW_HW_Init
 * sequence. It runs from a work function because it must not run from the
 * MDIO bus callbacks it re-initialises.
 */
#define LTQ_GSWIP_GSW_REINIT_DELAY_MS	(3000 + 14 * 2000)

static void ltq_gswip_gsw_reinit_work(struct work_struct *w)
{
	struct ltq_gswip_mdio_priv *priv =
		container_of(to_delayed_work(w), struct ltq_gswip_mdio_priv,
			     gsw_reinit_work);
	u32 c1;
	int ret;

	if (priv->bus)
		mutex_lock(&priv->bus->mdio_lock);
	ret = gsw_hw_reinit_gswl();
	/* MDIO master re-init: FREQ, then MCEN, then RES (self-clearing). */
	c1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	c1 &= ~GENMASK(GSWT_MDCCFG_1_FREQ_SHIFT +
		       GSWT_MDCCFG_1_FREQ_SIZE - 1,
		       GSWT_MDCCFG_1_FREQ_SHIFT);
	c1 |= LTQ_GSWIP_MDIO_MDC_CFG_1_FREQ_DEFAULT;
	__raw_writel(c1, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	c1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	__raw_writel(c1 | LTQ_GSWIP_MDIO_MDC_CFG_1_MCEN,
		     priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	c1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	__raw_writel(c1 | LTQ_GSWIP_MDIO_MDC_CFG_1_RES,
		     priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	/* Re-enable the HW polling unit for the four GE GPHY ports. */
	__raw_writel(0x003Cu, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_0_OFF);
	if (priv->bus)
		mutex_unlock(&priv->bus->mdio_lock);
	dev_info(priv->dev,
		 "GSW-L re-init under live link done (rc=%d)\n", ret);
}

/**
 * ltq_gswip_mdio_poll_mbusy() - wait for MDIO_CTRL.MBUSY to clear.
 *
 * @priv: per-bus state.
 *
 * Polls the MBUSY bit (MDIO_CTRL[12]) until it reads 0 or the 10 ms hard cap
 * fires. The hard cap matters because phylink and the phy_state machine may
 * end up calling into this from softirq context during link-state changes — a
 * wedged bus must NOT block the kernel indefinitely.
 *
 * Return: 0 when MBUSY cleared in time, -ETIMEDOUT on cap expiry.
 */
static int ltq_gswip_mdio_poll_mbusy(struct ltq_gswip_mdio_priv *priv)
{
	u32 ctrl;

	return read_poll_timeout(__raw_readl, ctrl,
				 !(ctrl & LTQ_GSWIP_MDIO_CTRL_MBUSY),
				 LTQ_GSWIP_MDIO_POLL_INTERVAL_US,
				 LTQ_GSWIP_MDIO_POLL_TIMEOUT_US,
				 false,
				 priv->base + LTQ_GSWIP_MDIO_CTRL_OFF);
}

/**
 * ltq_gswip_mdio_read() - mii_bus read callback (C22).
 *
 * @bus:  registered mii_bus.
 *
 * @addr: PHY address (5-bit, 0..31).
 *
 * @reg:  register address (5-bit, 0..31).
 *
 * Return: PHY register value (0..0xFFFF) on success, -ETIMEDOUT on
 * a wedged bus.
 */
static int ltq_gswip_mdio_read(struct mii_bus *bus, int addr, int reg)
{
	struct ltq_gswip_mdio_priv *priv = bus->priv;
	u32 cmd;
	int ret;

	/* Drain any in-flight prior operation first. */
	ret = ltq_gswip_mdio_poll_mbusy(priv);
	if (ret) {
		dev_err(priv->dev,
			"mdio_read: pre-issue MBUSY poll timed out (phy=%d reg=%d)\n",
			addr, reg);
		return -ETIMEDOUT;
	}

	cmd = ltq_gswip_mdio_ctrl_word(LTQ_GSWIP_MDIO_OP_READ, addr, reg);
	__raw_writel(cmd, priv->base + LTQ_GSWIP_MDIO_CTRL_OFF);

	/* Wait for the read transaction to complete (MBUSY -> 0). */
	ret = ltq_gswip_mdio_poll_mbusy(priv);
	if (ret) {
		dev_err(priv->dev,
			"mdio_read: post-issue MBUSY poll timed out (phy=%d reg=%d)\n",
			addr, reg);
		return -ETIMEDOUT;
	}

	return __raw_readl(priv->base + LTQ_GSWIP_MDIO_READ_OFF) & 0xFFFF;
}

/**
 * ltq_gswip_mdio_write() - mii_bus write callback (C22).
 *
 * @bus:  registered mii_bus.
 *
 * @addr: PHY address (5-bit, 0..31).
 *
 * @reg:  register address (5-bit, 0..31).
 *
 * @val:  16-bit value to write.
 *
 * Return: 0 on success, -ETIMEDOUT on a wedged bus.
 */
static int ltq_gswip_mdio_write(struct mii_bus *bus, int addr, int reg,
				u16 val)
{
	struct ltq_gswip_mdio_priv *priv = bus->priv;
	u32 cmd;
	int ret;

	ret = ltq_gswip_mdio_poll_mbusy(priv);
	if (ret) {
		dev_err(priv->dev,
			"mdio_write: pre-issue MBUSY poll timed out (phy=%d reg=%d)\n",
			addr, reg);
		return -ETIMEDOUT;
	}

	/*
	 * WDATA must be staged BEFORE MDIO_CTRL is written
	 * (gsw_flow_core.c:12069). Use __raw_writel (native byte order) to
	 * match ltq_w32 (lantiq.h:17) on big-endian MIPS.
	 */
	__raw_writel((u32)val, priv->base + LTQ_GSWIP_MDIO_WRITE_OFF);

	cmd = ltq_gswip_mdio_ctrl_word(LTQ_GSWIP_MDIO_OP_WRITE, addr, reg);
	__raw_writel(cmd, priv->base + LTQ_GSWIP_MDIO_CTRL_OFF);

	/* Wait for the write transaction to retire. */
	ret = ltq_gswip_mdio_poll_mbusy(priv);
	if (ret) {
		dev_err(priv->dev,
			"mdio_write: post-issue MBUSY poll timed out (phy=%d reg=%d val=0x%04x)\n",
			addr, reg, val);
		return -ETIMEDOUT;
	}

	return 0;
}

/**
 * ltq_gswip_mdio_log_scanned_phys() - dump the discovered PHY-address list.
 *
 * @pdev: probing platform_device, used for dev_info routing.
 *
 * @bus:  registered mii_bus whose mdio_map[] is now populated.
 */
static void ltq_gswip_mdio_log_scanned_phys(struct platform_device *pdev,
					    struct mii_bus *bus)
{
	char list[80] = "";
	int n = 0;
	int i;

	for (i = 0; i < PHY_MAX_ADDR; i++) {
		struct phy_device *phydev;
		size_t off;

		phydev = mdiobus_get_phy(bus, i);
		if (!phydev)
			continue;

		off = strlen(list);
		scnprintf(list + off, sizeof(list) - off,
			  "%s0x%02x", n ? "," : "", i);
		n++;
	}

	dev_info(&pdev->dev,
		 "lantiq-gswip-mdio: bus registered, scanned %d PHY(s) at addresses [%s]\n",
		 n, list);
}

static int ltq_gswip_mdio_probe(struct platform_device *pdev)
{
	struct ltq_gswip_mdio_priv *priv;
	struct mii_bus *bus;
	struct resource *res;
	u32 mdc_cfg_1;
	int ret;

	if (!xrx500_gphy_fw_is_loaded()) {
		dev_dbg(&pdev->dev,
			"GPHY firmware not yet loaded; deferring MDIO probe\n");
		return -EPROBE_DEFER;
	}

	bus = devm_mdiobus_alloc_size(&pdev->dev, sizeof(*priv));
	if (!bus)
		return -ENOMEM;

	priv = bus->priv;
	priv->dev = &pdev->dev;
	priv->bus = bus;	/* LINKDBG mdio_lock serialization */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no MDIO sub-aperture resource in DT\n");
		return -ENODEV;
	}
	priv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!priv->base) {
		dev_err(&pdev->dev,
			"devm_ioremap(MDIO sub-aperture %pR) failed\n", res);
		return -ENOMEM;
	}

	__raw_writel(0u, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_0_OFF);

	/* 1) FREQ = 0x09 (2.5 MHz MDC), preserving the rest of the reg. */
	mdc_cfg_1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	mdc_cfg_1 &= ~GENMASK(GSWT_MDCCFG_1_FREQ_SHIFT + GSWT_MDCCFG_1_FREQ_SIZE - 1,
			      GSWT_MDCCFG_1_FREQ_SHIFT);
	mdc_cfg_1 |= LTQ_GSWIP_MDIO_MDC_CFG_1_FREQ_DEFAULT;
	__raw_writel(mdc_cfg_1, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);

	/* 2) MCEN = 1 (enable management clock) — separate write. */
	mdc_cfg_1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	mdc_cfg_1 |= LTQ_GSWIP_MDIO_MDC_CFG_1_MCEN;
	__raw_writel(mdc_cfg_1, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);

	/* 3) RES = 1 (MDIO HW reset) LAST — on its own so it does not
	 *    clobber the just-latched MCEN bit. RES self-clears. */
	mdc_cfg_1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);
	mdc_cfg_1 |= LTQ_GSWIP_MDIO_MDC_CFG_1_RES;
	__raw_writel(mdc_cfg_1, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);

	mdc_cfg_1 = __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_1_OFF);

	ltq_gswip_mdio_calibrate_ctrl(priv);

	/*
	 * Enable the GSWIP-3.0 hardware MDIO poller (MDC_CFG_0.PEN). It is
	 * the only link-to-MAC coupling the vendor uses — its phylib
	 * adjust_link is an empty stub — and it is enabled at switch-core
	 * init (gsw_flow_core.c legacy init and GSW_HW_Init). The PHY address
	 * register is left at its reset default.
	 */
	__raw_writel(0x003Cu, priv->base + LTQ_GSWIP_MDIO_MDC_CFG_0_OFF);

	dev_info(&pdev->dev, "MDC_CFG_0=0x%08x MDC_CFG_1=0x%08x\n",
		 __raw_readl(priv->base + LTQ_GSWIP_MDIO_MDC_CFG_0_OFF),
		 mdc_cfg_1);

	ret = phy_register_fixup_for_uid(LTQ_GPHY11G_PHY_ID,
					 LTQ_GPHY11G_PHY_MASK,
					 ltq_gswip_gphy_mpd_fixup);
	if (ret)
		dev_warn(&pdev->dev,
			 "MPD fixup registration failed: %d\n", ret);

	bus->name = "lantiq-gswip-mdio";
	bus->read = ltq_gswip_mdio_read;
	bus->write = ltq_gswip_mdio_write;
	bus->parent = &pdev->dev;
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s", dev_name(&pdev->dev));

	platform_set_drvdata(pdev, bus);

	ret = devm_of_mdiobus_register(&pdev->dev, bus, pdev->dev.of_node);
	if (ret) {
		dev_err(&pdev->dev,
			"of_mdiobus_register failed: %d\n", ret);
		return ret;
	}

	ltq_gswip_mdio_log_scanned_phys(pdev, bus);

	/*
	 * One-shot GSW-L switch-core re-init, matching the vendor's
	 * GSW_HW_Init sequence. It runs from a work function because it must
	 * not run from the MDIO bus callbacks it re-initialises.
	 */
	INIT_DELAYED_WORK(&priv->gsw_reinit_work, ltq_gswip_gsw_reinit_work);
	schedule_delayed_work(&priv->gsw_reinit_work,
			      msecs_to_jiffies(LTQ_GSWIP_GSW_REINIT_DELAY_MS));
	return 0;
}

static const struct of_device_id ltq_gswip_mdio_match[] = {
	{ .compatible = "lantiq,gswip-mdio" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ltq_gswip_mdio_match);

struct platform_driver ltq_gswip_mdio_driver = {
	.probe = ltq_gswip_mdio_probe,
	.driver = {
		.name = "lantiq-gswip-mdio",
		.of_match_table = ltq_gswip_mdio_match,
	},
};
