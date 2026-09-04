// SPDX-License-Identifier: GPL-2.0
/*
 * Lantiq / Intel GSWIP 3.0 switch driver for the xRX500 SoC family
 *
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * The switch core is a superset of the GSWIP-2.x core the common function
 * library drives, at the same register offsets, so the parser, VLAN, address
 * table and per-port code apply unchanged. What this file adds are the parts
 * of the register window the earlier models do not have: the switch macro's
 * top block, which holds the global control and the MDIO master, the packet
 * MAC that terminates the CPU side of the switch, and the register content
 * that is specific to this generation.
 *
 * Register meanings, sequences and table layouts were re-derived from the
 * vendor's GPL switch API sources for this SoC family.
 */

#include "lantiq_gswip.h"
#include "lantiq_pce_xrx500.h"

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/gfp.h>
#include <linux/mii.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <net/dsa.h>

/* Switch macro top block. It holds the global control register and the MDIO
 * master, which GSWIP-2.x keeps in a region of its own.
 */
#define GSWIP_XRX500_TOP			0xF00
#define GSWIP_XRX500_GCTRL			(GSWIP_XRX500_TOP + 0x000)
#define  GSWIP_XRX500_GCTRL_SE			BIT(15)
#define GSWIP_XRX500_MDCTRL			(GSWIP_XRX500_TOP + 0x004)
#define GSWIP_XRX500_MDREAD			(GSWIP_XRX500_TOP + 0x005)
#define GSWIP_XRX500_MDWRITE			(GSWIP_XRX500_TOP + 0x006)
#define GSWIP_XRX500_MDCCFG0			(GSWIP_XRX500_TOP + 0x007)
#define GSWIP_XRX500_MDCCFG1			(GSWIP_XRX500_TOP + 0x008)
#define  GSWIP_XRX500_MDCCFG1_MCEN		BIT(8)
#define  GSWIP_XRX500_MDCCFG1_FREQ		GENMASK(7, 0)
/* The MDIO master needs its clock divider and its own enable. */
#define  GSWIP_XRX500_MDCCFG1_MASK		(GSWIP_XRX500_MDCCFG1_MCEN | \
						 GSWIP_XRX500_MDCCFG1_FREQ)
#define  GSWIP_XRX500_MDCCFG1_VAL		(GSWIP_XRX500_MDCCFG1_MCEN | 0x09)
/* Per-port PHY address and link force register. Numbered from port 1
 * upwards; port 0 has none because it has no MAC.
 */
#define GSWIP_XRX500_PHY_ADDRp(p)		(GSWIP_XRX500_TOP + 0x044 + \
						 0x4 * ((p) - 1))

/* The MDIO master raises its busy bit a short time after the control
 * register is written rather than taking it from the written value.
 */
#define GSWIP_XRX500_MDIO_SETTLE_US		10

/* The integrated gigabit PHYs fetch their firmware from DRAM. Each one has a
 * pair of registers in the top block holding the low and the high half of
 * that address, and the pair is programmed while the PHY is held in reset.
 */
#define GSWIP_XRX500_GPHY_MBADR_OFFSET		1
/* The firmware has to start on a 16 kB boundary. */
#define GSWIP_XRX500_GPHY_FW_ALIGN		(16 * 1024)
/* The address pair is sampled when the core leaves reset, so the reset is
 * held while the pair is written and the write is given time to land before
 * the core is released.
 */
#define GSWIP_XRX500_GPHY_RESET_HOLD_US		50
#define GSWIP_XRX500_GPHY_RESET_SETTLE_US	100
/* The cores give no sign of having finished booting, so the wait is blind. */
#define GSWIP_XRX500_GPHY_FW_BOOT_MS		100

/* These cores answer their identification registers before their firmware
 * has finished booting, and in that window a read of the status register
 * completes and returns zero. Wait for a plausible answer before the bus is
 * registered, so nothing samples a half-alive core.
 */
#define GSWIP_XRX500_PHY_READY_TIMEOUT_MS	2000
#define GSWIP_XRX500_PHY_READY_POLL_MS		20

/* Port count of each switch macro. The first has seven ports and carries the
 * internal PHYs; the second numbers its external port 15.
 */
#define GSWIP_XRX500_MAX_PORTS			7
#define GSWIP_XRX500_R_MAX_PORTS		16
#define GSWIP_XRX500_R_EXT_PORT			15

/* Packet MAC. It sits inside the switch's register window rather than being a
 * device of its own, and the reset the common function library issues in
 * setup() returns it to its defaults, so this model programs it from the
 * per-model setup callback that runs afterwards.
 */
#define GSWIP_XRX500_PMAC_CTRL_0		0xD03
#define  GSWIP_XRX500_PMAC_CTRL_0_PADEN		BIT(8)
#define GSWIP_XRX500_PMAC_CTRL_2		0xD05
#define  GSWIP_XRX500_PMAC_CTRL_2_LCHKS		GENMASK(1, 0)
#define  GSWIP_XRX500_PMAC_CTRL_2_MLEN		BIT(3)
#define GSWIP_XRX500_PMAC_CTRL_4		0xD07
#define  GSWIP_XRX500_PMAC_CTRL_4_FLAGEN	BIT(0)
#define GSWIP_XRX500_PMAC_TBL_VAL(x)		(0xD44 - (x))
#define GSWIP_XRX500_PMAC_TBL_ADDR		0xD45
#define GSWIP_XRX500_PMAC_TBL_CTRL		0xD46
#define  GSWIP_XRX500_PMAC_TBL_CTRL_BAS		BIT(15)
#define  GSWIP_XRX500_PMAC_TBL_CTRL_WRITE	BIT(5)
#define  GSWIP_XRX500_PMAC_TBL_CTRL_TABLE	GENMASK(2, 0)
#define GSWIP_XRX500_PMAC_TABLE_INGRESS		1
#define GSWIP_XRX500_PMAC_TABLE_EGRESS		2

/* Ingress table entry: the header the packet MAC assumes on a frame arriving
 * from the CPU, and which of its fields to take from that header rather than
 * from the descriptor.
 */
#define GSWIP_XRX500_PMAC_IG_PMAC_PRESENT	BIT(0)
#define GSWIP_XRX500_PMAC_IG_SPID_DEFAULT	BIT(1)
#define GSWIP_XRX500_PMAC_IG_SUBID_DEFAULT	BIT(2)
#define GSWIP_XRX500_PMAC_IG_CLASS_ENA		BIT(3)
#define GSWIP_XRX500_PMAC_IG_PMAP_ENA		BIT(4)
#define GSWIP_XRX500_PMAC_IG_CLASS_DEFAULT	BIT(5)
#define GSWIP_XRX500_PMAC_IG_PMAP_DEFAULT	BIT(6)
#define GSWIP_XRX500_PMAC_IG_ERR_DISCARD	BIT(7)

/* Egress table entry: what the packet MAC puts on a frame leaving towards
 * the CPU, and which receive channel it leaves on.
 */
#define GSWIP_XRX500_PMAC_EG_PMAC_ENA		BIT(0)
#define GSWIP_XRX500_PMAC_EG_TRAFFIC_CLASSES	16
#define GSWIP_XRX500_PMAC_EG_FLOW_IDS		4

/* Transmit channels the ingress table describes, and destination ports the
 * egress table describes on each macro.
 */
#define GSWIP_XRX500_PMAC_CHANNELS		16
#define GSWIP_XRX500_PMAC_EG_PORTS_CPU		1
#define GSWIP_XRX500_PMAC_EG_PORTS_ALL		16

/* The WRED mode selector is two bits wide on this generation, where the
 * common function library knows it as one.
 */
#define GSWIP_XRX500_BM_QUEUE_GCTRL_GL_MOD	GENMASK(11, 10)

/* Identity the integrated gigabit PHYs answer with, and the mask the driver
 * matches it under.
 */
#define GSWIP_XRX500_GPHY_ID			0xd565a408
#define GSWIP_XRX500_GPHY_ID_MASK		0xfffffff8

/* Register window of one switch macro, in 32-bit words. */
#define GSWIP_XRX500_MAX_REGISTER		0xFFF

/**
 * struct gswip_xrx500_model - what differs between the two switch macros
 * @hw_info: the part of it the common function library reads
 * @pmac_eg_ports: number of destination ports whose egress entries are
 *	programmed. The first is the CPU; the macro that relays frames on
 *	behalf of the accelerator paths has more.
 * @pmac_pad: pad short frames on egress
 * @pmac_long_untagged: allow the longer untagged frame the second macro
 *	carries, because a frame crossing it has a tag added
 */
struct gswip_xrx500_model {
	struct gswip_hw_info hw_info;
	unsigned int pmac_eg_ports;
	bool pmac_pad;
	bool pmac_long_untagged;
};

/**
 * struct gswip_xrx500_gphy - one integrated PHY's firmware state
 * @reset: reset line holding the PHY core
 * @lbadr: word offset of the register pair holding the firmware address
 */
struct gswip_xrx500_gphy {
	struct reset_control *reset;
	u32 lbadr;
};

/**
 * struct gswip_xrx500 - per-instance state of this model
 * @priv: state shared with the common function library
 * @num_gphy: number of integrated PHYs described in the device tree
 * @gphy: per-PHY firmware state
 * @fw_page: page allocation backing the firmware image
 * @fw_order: allocation order of @fw_page
 * @fw_dma: address the PHY cores fetch the image from
 * @fw_len: length of the mapped image
 */
struct gswip_xrx500 {
	struct gswip_priv priv;
	unsigned int num_gphy;
	struct gswip_xrx500_gphy *gphy;
	unsigned long fw_page;
	unsigned int fw_order;
	dma_addr_t fw_dma;
	size_t fw_len;
};

static const struct gswip_xrx500_model *
gswip_xrx500_model(struct gswip_priv *priv)
{
	return container_of(priv->hw_info, struct gswip_xrx500_model, hw_info);
}

static const struct gswip_mdio_layout gswip_xrx500_mdio_layout = {
	.glob		= GSWIP_XRX500_GCTRL,
	.ctrl		= GSWIP_XRX500_MDCTRL,
	.read		= GSWIP_XRX500_MDREAD,
	.write		= GSWIP_XRX500_MDWRITE,
	.mdc_cfg0	= GSWIP_XRX500_MDCCFG0,
	.mdc_cfg1	= GSWIP_XRX500_MDCCFG1,
	.mdc_cfg1_mask	= GSWIP_XRX500_MDCCFG1_MASK,
	.mdc_cfg1_val	= GSWIP_XRX500_MDCCFG1_VAL,
	.settle_us	= GSWIP_XRX500_MDIO_SETTLE_US,
	.phy		= {
		[0] = -1,
		[1] = GSWIP_XRX500_PHY_ADDRp(1),
		[2] = GSWIP_XRX500_PHY_ADDRp(2),
		[3] = GSWIP_XRX500_PHY_ADDRp(3),
		[4] = GSWIP_XRX500_PHY_ADDRp(4),
		[5] = GSWIP_XRX500_PHY_ADDRp(5),
		[6] = GSWIP_XRX500_PHY_ADDRp(6),
		[7 ... GSWIP_XRX500_R_EXT_PORT - 1] = -1,
		/* The external port of the second macro reaches the register
		 * of port 1, not the one its own number would give.
		 */
		[GSWIP_XRX500_R_EXT_PORT] = GSWIP_XRX500_PHY_ADDRp(1),
	},
};

/* The MAC blocks are numbered from port 1 upwards, so a port's block sits one
 * stride below where the port number alone would put it, and port 0 has no
 * block at all. The external port of the second macro shares the first block
 * with port 1.
 */
static const s16 gswip_xrx500_mac_ctrl[GSWIP_MAX_PORTS] = {
	[0] = -1,
	[1] = GSWIP_MAC_CTRL_BASEp(0),
	[2] = GSWIP_MAC_CTRL_BASEp(1),
	[3] = GSWIP_MAC_CTRL_BASEp(2),
	[4] = GSWIP_MAC_CTRL_BASEp(3),
	[5] = GSWIP_MAC_CTRL_BASEp(4),
	[6] = GSWIP_MAC_CTRL_BASEp(5),
	[7 ... GSWIP_XRX500_R_EXT_PORT - 1] = -1,
	[GSWIP_XRX500_R_EXT_PORT] = GSWIP_MAC_CTRL_BASEp(0),
};

/* RMON banks follow the port numbers up to the eighth port and are offset by
 * eight above it.
 */
static const s16 gswip_xrx500_rmon_table[GSWIP_MAX_PORTS] = {
	0, 1, 2, 3, 4, 5, 6,
	[7 ... GSWIP_XRX500_R_EXT_PORT - 1] = -1,
	[GSWIP_XRX500_R_EXT_PORT] = GSWIP_XRX500_R_EXT_PORT + 8,
};

/* MDIO master, driven directly. The common function library registers the
 * bus from inside setup(), which runs after the PHY firmware has to be up.
 */
static int gswip_xrx500_mdio_busy(struct gswip_priv *priv)
{
	u32 ctrl;

	return regmap_read_poll_timeout(priv->gswip, GSWIP_XRX500_MDCTRL, ctrl,
					!(ctrl & GSWIP_MDIO_CTRL_BUSY),
					40, 4000);
}

static int gswip_xrx500_mdio_read(struct gswip_priv *priv, int addr, int reg)
{
	u32 val;
	int err;

	err = gswip_xrx500_mdio_busy(priv);
	if (err)
		return err;

	regmap_write(priv->gswip, GSWIP_XRX500_MDCTRL,
		     GSWIP_MDIO_CTRL_BUSY | GSWIP_MDIO_CTRL_RD |
		     ((addr & GSWIP_MDIO_CTRL_PHYAD_MASK) <<
		      GSWIP_MDIO_CTRL_PHYAD_SHIFT) |
		     (reg & GSWIP_MDIO_CTRL_REGAD_MASK));
	udelay(GSWIP_XRX500_MDIO_SETTLE_US);

	err = gswip_xrx500_mdio_busy(priv);
	if (err)
		return err;

	err = regmap_read(priv->gswip, GSWIP_XRX500_MDREAD, &val);
	if (err)
		return err;

	return val & 0xffff;
}

/* Waits for every PHY the device tree describes to answer its status register
 * with something other than an all-zeroes or all-ones word. Fails open: a
 * core that never answers is left to the PHY layer, which reports it as a
 * link that never comes up rather than as a switch that failed to probe.
 */
static void gswip_xrx500_wait_phys(struct gswip_priv *priv)
{
	struct device_node *mdio_np, *phy_np;
	unsigned int waited = 0;

	mdio_np = of_get_child_by_name(priv->dev->of_node, "mdio");
	if (!mdio_np)
		return;

	for_each_available_child_of_node(mdio_np, phy_np) {
		u32 addr;
		int bmsr;

		if (of_property_read_u32(phy_np, "reg", &addr))
			continue;

		for (;;) {
			bmsr = gswip_xrx500_mdio_read(priv, addr, MII_BMSR);
			if (bmsr > 0 && bmsr != 0xffff)
				break;

			if (waited >= GSWIP_XRX500_PHY_READY_TIMEOUT_MS) {
				dev_warn(priv->dev,
					 "phy %u did not answer within the boot wait\n",
					 addr);
				break;
			}

			msleep(GSWIP_XRX500_PHY_READY_POLL_MS);
			waited += GSWIP_XRX500_PHY_READY_POLL_MS;
		}
	}

	of_node_put(mdio_np);
}

static void gswip_xrx500_gphy_free(struct gswip_xrx500 *chip)
{
	if (!chip->fw_page)
		return;

	dma_unmap_single(chip->priv.dev, chip->fw_dma, chip->fw_len,
			 DMA_TO_DEVICE);
	free_pages(chip->fw_page, chip->fw_order);
	chip->fw_page = 0;
}

static void gswip_xrx500_gphy_release(struct gswip_xrx500 *chip)
{
	unsigned int i;

	for (i = 0; i < chip->num_gphy; i++)
		reset_control_put(chip->gphy[i].reset);

	gswip_xrx500_gphy_free(chip);
}

/* Stages the firmware image where the PHY cores can fetch it. Their fetch
 * master does not take part in the cache coherence this platform otherwise
 * has, so the image goes into ordinary memory and is mapped for the device,
 * which writes it back. A coherent allocation would leave the cores reading
 * whatever DRAM held before.
 */
static int gswip_xrx500_gphy_stage(struct gswip_xrx500 *chip,
				   struct device_node *fw_np)
{
	struct device *dev = chip->priv.dev;
	const struct firmware *fw;
	const char *fw_name;
	size_t size;
	void *fw_addr;
	int err;

	err = of_property_read_string(fw_np, "firmware-name", &fw_name);
	if (err)
		return dev_err_probe(dev, err,
				     "no firmware named for the integrated PHYs\n");

	/* Read the image directly rather than through the usermode helper: a
	 * built-in build probes before any filesystem is mounted, and the
	 * helper turns that into a minute of waiting followed by no ethernet
	 * at all. This driver is therefore built as a module and loaded once
	 * the root filesystem is up.
	 */
	err = request_firmware_direct(&fw, fw_name, dev);
	if (err)
		return dev_err_probe(dev, err, "cannot read %s\n", fw_name);

	size = fw->size + GSWIP_XRX500_GPHY_FW_ALIGN;
	chip->fw_order = get_order(size);
	chip->fw_page = __get_free_pages(GFP_KERNEL, chip->fw_order);
	if (!chip->fw_page) {
		release_firmware(fw);
		return -ENOMEM;
	}

	fw_addr = PTR_ALIGN((void *)chip->fw_page, GSWIP_XRX500_GPHY_FW_ALIGN);
	memcpy(fw_addr, fw->data, fw->size);
	chip->fw_len = fw->size;
	release_firmware(fw);

	chip->fw_dma = dma_map_single(dev, fw_addr, chip->fw_len,
				      DMA_TO_DEVICE);
	if (dma_mapping_error(dev, chip->fw_dma)) {
		free_pages(chip->fw_page, chip->fw_order);
		chip->fw_page = 0;
		return -ENOMEM;
	}

	return 0;
}

/* The reset controller writes the request before it confirms it, so a failed
 * confirmation is not a reason to stop: a line that really did not move
 * shows up as a PHY that never answers, which the wait below reports by
 * name.
 *
 * The confirmation is only as good as the status bit the device tree names.
 * The status register is a remap of the request register rather than a
 * mirror of it, so a node that repeats its request bit in the second cell
 * watches an unrelated bit and can only ever time out.
 */
static void gswip_xrx500_gphy_boot(struct gswip_xrx500 *chip,
				   struct gswip_xrx500_gphy *gphy)
{
	struct gswip_priv *priv = &chip->priv;
	int err;

	err = reset_control_assert(gphy->reset);
	if (err)
		dev_dbg(priv->dev, "reset assert reported %d\n", err);

	udelay(GSWIP_XRX500_GPHY_RESET_HOLD_US);

	/* Programmed while the core is held in reset, so it boots from here
	 * the moment the reset is released.
	 */
	regmap_write(priv->gswip, gphy->lbadr, chip->fw_dma & 0xffff);
	regmap_write(priv->gswip, gphy->lbadr + GSWIP_XRX500_GPHY_MBADR_OFFSET,
		     (chip->fw_dma >> 16) & 0xffff);

	err = reset_control_deassert(gphy->reset);
	if (err)
		dev_dbg(priv->dev, "reset deassert reported %d\n", err);

	udelay(GSWIP_XRX500_GPHY_RESET_SETTLE_US);
}

/* Every core is reset and pointed at the image the device tree names rather
 * than left alone: the two images this family ships answer the
 * identification registers alike, so nothing the driver can read tells a
 * core already staged with the right image from one that is not.
 */
static int gswip_xrx500_gphy_load(struct gswip_xrx500 *chip)
{
	struct device_node *fw_np, *gphy_np;
	struct device *dev = chip->priv.dev;
	unsigned int i = 0;
	int err;

	fw_np = of_get_child_by_name(dev->of_node, "gphy-fw");
	if (!fw_np)
		return 0;

	if (!of_device_is_available(fw_np)) {
		of_node_put(fw_np);
		return 0;
	}

	chip->num_gphy = of_get_available_child_count(fw_np);
	if (!chip->num_gphy) {
		of_node_put(fw_np);
		return dev_err_probe(dev, -ENOENT,
				     "no integrated PHY described\n");
	}

	/* Probe can be deferred and run again, so allocate once. */
	if (!chip->gphy)
		chip->gphy = devm_kcalloc(dev, chip->num_gphy,
					  sizeof(*chip->gphy), GFP_KERNEL);
	if (!chip->gphy) {
		of_node_put(fw_np);
		return -ENOMEM;
	}

	err = gswip_xrx500_gphy_stage(chip, fw_np);
	if (err) {
		of_node_put(fw_np);
		return err;
	}

	for_each_available_child_of_node(fw_np, gphy_np) {
		struct gswip_xrx500_gphy *gphy = &chip->gphy[i++];

		err = of_property_read_u32(gphy_np, "reg", &gphy->lbadr);
		if (err)
			goto out_put;

		gphy->reset = of_reset_control_array_get_exclusive(gphy_np);
		if (IS_ERR(gphy->reset)) {
			err = PTR_ERR(gphy->reset);
			gphy->reset = NULL;
			goto out_put;
		}

		gswip_xrx500_gphy_boot(chip, gphy);
	}

	of_node_put(fw_np);

	msleep(GSWIP_XRX500_GPHY_FW_BOOT_MS);

	return 0;

out_put:
	of_node_put(gphy_np);
	of_node_put(fw_np);
	gswip_xrx500_gphy_release(chip);

	return dev_err_probe(dev, err, "cannot start the integrated PHYs\n");
}

/* Loads the parser microcode with the ordering this generation needs: the
 * macro's global enable is set before the microcode is marked invalid, and
 * the per-port fetch and store engines are held off across the load.
 *
 * The common function library repeats the load from setup(), after the reset
 * it issues there. This copy exists because the integrated PHYs have to be
 * started with a loaded parser, and they in turn have to answer before the
 * MDIO bus is registered, which setup() does between the two.
 */
static int gswip_xrx500_load_microcode(struct gswip_priv *priv)
{
	unsigned int port;
	int err, i;

	for (port = 0; port < priv->hw_info->max_ports; port++) {
		regmap_clear_bits(priv->gswip, GSWIP_FDMA_PCTRLp(port),
				  GSWIP_FDMA_PCTRL_EN);
		regmap_clear_bits(priv->gswip, GSWIP_SDMA_PCTRLp(port),
				  GSWIP_SDMA_PCTRL_EN);
	}

	regmap_set_bits(priv->gswip, GSWIP_XRX500_GCTRL, GSWIP_XRX500_GCTRL_SE);
	regmap_clear_bits(priv->gswip, GSWIP_PCE_GCTRL_0,
			  GSWIP_PCE_GCTRL_0_MC_VALID);

	regmap_write_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
			  GSWIP_PCE_TBL_CTRL_ADDR_MASK |
			  GSWIP_PCE_TBL_CTRL_OPMOD_MASK,
			  GSWIP_PCE_TBL_CTRL_OPMOD_ADWR);

	for (i = 0; i < priv->hw_info->pce_microcode_size; i++) {
		u32 ctrl;

		regmap_write(priv->gswip, GSWIP_PCE_TBL_ADDR, i);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(0),
			     (*priv->hw_info->pce_microcode)[i].val_0);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(1),
			     (*priv->hw_info->pce_microcode)[i].val_1);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(2),
			     (*priv->hw_info->pce_microcode)[i].val_2);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(3),
			     (*priv->hw_info->pce_microcode)[i].val_3);
		regmap_set_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
				GSWIP_PCE_TBL_CTRL_BAS);

		err = regmap_read_poll_timeout(priv->gswip, GSWIP_PCE_TBL_CTRL,
					       ctrl,
					       !(ctrl & GSWIP_PCE_TBL_CTRL_BAS),
					       20, 50000);
		if (err)
			return err;
	}

	regmap_set_bits(priv->gswip, GSWIP_PCE_GCTRL_0,
			GSWIP_PCE_GCTRL_0_MC_VALID);

	return 0;
}

/* Writes one packet MAC table entry. Neither the table nor the control
 * registers report anything about a write, so a read of the entry through
 * the same window is the only way to see what landed.
 */
static int gswip_xrx500_pmac_write(struct gswip_priv *priv, u8 table,
				   u16 addr, const u16 val[5])
{
	unsigned int i;
	int err;
	u32 ctrl;

	err = regmap_read_poll_timeout(priv->gswip,
				       GSWIP_XRX500_PMAC_TBL_CTRL, ctrl,
				       !(ctrl & GSWIP_XRX500_PMAC_TBL_CTRL_BAS),
				       20, 50000);
	if (err)
		return err;

	regmap_write(priv->gswip, GSWIP_XRX500_PMAC_TBL_ADDR, addr);
	for (i = 0; i < 5; i++)
		regmap_write(priv->gswip, GSWIP_XRX500_PMAC_TBL_VAL(i),
			     val[i]);

	regmap_write(priv->gswip, GSWIP_XRX500_PMAC_TBL_CTRL,
		     GSWIP_XRX500_PMAC_TBL_CTRL_BAS |
		     GSWIP_XRX500_PMAC_TBL_CTRL_WRITE |
		     FIELD_PREP(GSWIP_XRX500_PMAC_TBL_CTRL_TABLE, table));

	return regmap_read_poll_timeout(priv->gswip,
					GSWIP_XRX500_PMAC_TBL_CTRL, ctrl,
					!(ctrl & GSWIP_XRX500_PMAC_TBL_CTRL_BAS),
					20, 50000);
}

/* One ingress entry per transmit channel. Every field comes from the default
 * header this entry holds, the source port, the sub-interface id, the traffic
 * class and the destination port map alike, so a malformed frame cannot claim
 * to have arrived from a front port, and a frame the packet MAC finds broken
 * is dropped rather than handed to the switch.
 */
static int gswip_xrx500_pmac_ingress(struct gswip_priv *priv)
{
	unsigned int chan;
	int err;

	for (chan = 0; chan < GSWIP_XRX500_PMAC_CHANNELS; chan++) {
		u16 val[5] = {};

		val[1] = ((((chan & 0x8) >> 3) * 2 + 1) << 8) | 0x90;
		val[3] = BIT(chan & 0x7);
		val[4] = GSWIP_XRX500_PMAC_IG_PMAC_PRESENT |
			 GSWIP_XRX500_PMAC_IG_SPID_DEFAULT |
			 GSWIP_XRX500_PMAC_IG_SUBID_DEFAULT |
			 GSWIP_XRX500_PMAC_IG_CLASS_ENA |
			 GSWIP_XRX500_PMAC_IG_PMAP_ENA |
			 GSWIP_XRX500_PMAC_IG_CLASS_DEFAULT |
			 GSWIP_XRX500_PMAC_IG_PMAP_DEFAULT |
			 GSWIP_XRX500_PMAC_IG_ERR_DISCARD;

		err = gswip_xrx500_pmac_write(priv,
					      GSWIP_XRX500_PMAC_TABLE_INGRESS,
					      chan, val);
		if (err)
			return err;
	}

	return 0;
}

/* One egress entry per destination port, traffic class and flow identifier.
 * Every frame leaving towards the CPU carries the header the tagging driver
 * reads its source port from, and leaves on the one receive channel this
 * design uses. The entry is addressed by traffic class, which is what the
 * cleared flag-select in PMAC_CTRL_4 selects.
 */
static int gswip_xrx500_pmac_egress(struct gswip_priv *priv)
{
	const struct gswip_xrx500_model *model = gswip_xrx500_model(priv);
	unsigned int port, tc, flow;
	int err;

	for (port = 0; port < model->pmac_eg_ports; port++) {
		for (tc = 0; tc < GSWIP_XRX500_PMAC_EG_TRAFFIC_CLASSES; tc++) {
			for (flow = 0; flow < GSWIP_XRX500_PMAC_EG_FLOW_IDS;
			     flow++) {
				u16 val[5] = {};
				u16 addr = port | tc << 4 | flow << 8;

				val[2] = GSWIP_XRX500_PMAC_EG_PMAC_ENA;

				err = gswip_xrx500_pmac_write(priv,
							      GSWIP_XRX500_PMAC_TABLE_EGRESS,
							      addr, val);
				if (err)
					return err;
			}
		}
	}

	return 0;
}

static int gswip_xrx500_setup(struct dsa_switch *ds)
{
	struct gswip_priv *priv = ds->priv;
	const struct gswip_xrx500_model *model = gswip_xrx500_model(priv);
	int err;

	/* The queue manager's drop-policy selector is two bits wide here, and
	 * this generation runs it at zero. Written after the common function
	 * library has set the one-bit form it knows.
	 */
	regmap_clear_bits(priv->gswip, GSWIP_BM_QUEUE_GCTRL,
			  GSWIP_XRX500_BM_QUEUE_GCTRL_GL_MOD);

	/* Source address spoofing detection on the CPU port is deliberately
	 * left off. It drops a frame whose source address the switch has
	 * learned on another port, which is what the software bridge sends
	 * whenever it forwards for the switch.
	 */

	/* Address the egress table by traffic class rather than by the
	 * processing flags of the accelerator paths, which this design does
	 * not build. This has to be settled before the table is written,
	 * because it decides what the entry addresses mean.
	 */
	regmap_clear_bits(priv->gswip, GSWIP_XRX500_PMAC_CTRL_4,
			  GSWIP_XRX500_PMAC_CTRL_4_FLAGEN);

	/* The short-frame length check drops a CPU frame before the switch
	 * sees it, and has to end up off. The vendor switch library turns it
	 * on and the vendor ethernet driver turns it back off; only the
	 * second half is wanted here.
	 */
	regmap_clear_bits(priv->gswip, GSWIP_XRX500_PMAC_CTRL_2,
			  GSWIP_XRX500_PMAC_CTRL_2_LCHKS);

	regmap_write_bits(priv->gswip, GSWIP_XRX500_PMAC_CTRL_0,
			  GSWIP_XRX500_PMAC_CTRL_0_PADEN,
			  model->pmac_pad ?
			  GSWIP_XRX500_PMAC_CTRL_0_PADEN : 0);
	regmap_write_bits(priv->gswip, GSWIP_XRX500_PMAC_CTRL_2,
			  GSWIP_XRX500_PMAC_CTRL_2_MLEN,
			  model->pmac_long_untagged ?
			  GSWIP_XRX500_PMAC_CTRL_2_MLEN : 0);

	err = gswip_xrx500_pmac_ingress(priv);
	if (err)
		return err;

	return gswip_xrx500_pmac_egress(priv);
}

static void gswip_xrx500_phylink_get_caps(struct dsa_switch *ds, int port,
					  struct phylink_config *config)
{
	/* Port 0 terminates on the packet MAC and the ports above the first
	 * on the integrated PHYs. Both are fixed internal interfaces. The one
	 * external interface this macro has is port 1, and no board describes
	 * it, so it advertises nothing and a port node for it is rejected
	 * rather than quietly not working.
	 */
	if (port == 0 || port >= 2)
		__set_bit(PHY_INTERFACE_MODE_INTERNAL,
			  config->supported_interfaces);

	config->mac_capabilities = MAC_ASYM_PAUSE | MAC_SYM_PAUSE |
		MAC_10 | MAC_100 | MAC_1000;
}

static void gswip_xrx500_r_phylink_get_caps(struct dsa_switch *ds, int port,
					    struct phylink_config *config)
{
	if (port == 0 || port == GSWIP_XRX500_R_EXT_PORT)
		__set_bit(PHY_INTERFACE_MODE_INTERNAL,
			  config->supported_interfaces);

	config->mac_capabilities = MAC_ASYM_PAUSE | MAC_SYM_PAUSE |
		MAC_10 | MAC_100 | MAC_1000;
}

/* Every one of these PHYs faces a switch port, so it should carry the
 * master clock of a gigabit link rather than take it from the far end. The
 * preference survives an autonegotiation restart, which only rewrites the
 * advertisement bits of the same register, so setting it once as the PHY
 * comes up is enough.
 */
static int gswip_xrx500_gphy_fixup(struct phy_device *phydev)
{
	return phy_set_bits(phydev, MII_CTRL1000, CTL1000_PREFER_MASTER);
}

/* The block presents its registers in the byte order of the CPU it sits on,
 * and this platform does not mangle memory-mapped accesses, so the plain
 * accessors reach them unchanged. Asking for the native format instead would
 * add a byte swap on a big-endian build, because the format regmap calls
 * native is the one that pairs with a platform whose accessors swap.
 */
static const struct regmap_config gswip_xrx500_regmap_config = {
	.name = "switch",
	.reg_bits = 32,
	.val_bits = 32,
	.reg_shift = REGMAP_UPSHIFT(2),
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
	.max_register = GSWIP_XRX500_MAX_REGISTER,
};

static int gswip_xrx500_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gswip_xrx500 *chip;
	struct gswip_priv *priv;
	void __iomem *base;
	u32 version, dummy;
	int err;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	priv = &chip->priv;
	priv->dev = dev;

	priv->hw_info = of_device_get_match_data(dev);
	if (!priv->hw_info)
		return -EINVAL;

	/* The PHY cores fetch their firmware outside the coherence domain the
	 * platform sets up for its bus masters. On a platform that is
	 * coherent by default the switch node has to say so, or the write-back
	 * of the staged image is skipped and the cores boot from whatever
	 * DRAM held before, which shows up as PHYs that answer their
	 * identification registers and never link.
	 */
	if (of_dma_is_coherent(dev->of_node))
		return dev_err_probe(dev, -EINVAL,
				     "the switch node needs dma-noncoherent\n");

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	/* One window covers the switch core, the top block and the packet MAC,
	 * so one map serves every register this driver reaches. The per-model
	 * tables carry the offsets, so the common function library finds the
	 * MDIO master through the same map.
	 */
	priv->gswip = devm_regmap_init_mmio(dev, base,
					    &gswip_xrx500_regmap_config);
	if (IS_ERR(priv->gswip))
		return PTR_ERR(priv->gswip);

	priv->mdio = priv->gswip;
	priv->mii = priv->gswip;

	err = regmap_read(priv->gswip, GSWIP_VERSION, &version);
	if (err)
		return err;

	if (version != GSWIP_VERSION_3_0)
		return dev_err_probe(dev, -ENODEV,
				     "unexpected GSWIP version: 0x%x\n",
				     version);

	/* The parser microcode is loaded before the PHY firmware, so the cores are
	 * released into a switch whose parser is already live.
	 */
	err = gswip_xrx500_load_microcode(priv);
	if (err)
		return dev_err_probe(dev, err,
				     "cannot load the parser microcode\n");

	/* The auto-polling master shares the control and data registers with
	 * software transactions and corrupts them, so turn it off before the
	 * first read below. It stays off: link state reaches the MAC through
	 * phylink instead.
	 */
	regmap_write(priv->gswip, GSWIP_XRX500_MDCCFG0, 0);
	regmap_write_bits(priv->gswip, GSWIP_XRX500_MDCCFG1,
			  GSWIP_XRX500_MDCCFG1_MASK,
			  GSWIP_XRX500_MDCCFG1_VAL);
	regmap_read(priv->gswip, GSWIP_XRX500_MDCTRL, &dummy);

	err = gswip_xrx500_gphy_load(chip);
	if (err)
		return err;

	gswip_xrx500_wait_phys(priv);

	err = gswip_probe_common(priv, version);
	if (err)
		goto out_gphy;

	platform_set_drvdata(pdev, chip);

	return 0;

out_gphy:
	gswip_xrx500_gphy_release(chip);

	return err;
}

static void gswip_xrx500_remove(struct platform_device *pdev)
{
	struct gswip_xrx500 *chip = platform_get_drvdata(pdev);

	if (!chip)
		return;

	dsa_unregister_switch(chip->priv.ds);
	gswip_xrx500_gphy_release(chip);
}

static void gswip_xrx500_shutdown(struct platform_device *pdev)
{
	struct gswip_xrx500 *chip = platform_get_drvdata(pdev);

	if (!chip)
		return;

	dsa_switch_shutdown(chip->priv.ds);

	platform_set_drvdata(pdev, NULL);
}

static const struct gswip_xrx500_model gswip_xrx500 = {
	.pmac_eg_ports = GSWIP_XRX500_PMAC_EG_PORTS_CPU,
	.hw_info = {
		.max_ports = GSWIP_XRX500_MAX_PORTS,
		.allowed_cpu_ports = BIT(0),
		/* No port has an xMII mode or delay register: the
		 * internal PHY ports attach over a fixed interface and
		 * the external one is not wired.
		 */
		.mii_cfg = { [0 ... GSWIP_MAX_PORTS - 1] = -1 },
		.mii_pcdu = { [0 ... GSWIP_MAX_PORTS - 1] = -1 },
		.mdio_layout = &gswip_xrx500_mdio_layout,
		.mac_ctrl = gswip_xrx500_mac_ctrl,
		.rmon_table = gswip_xrx500_rmon_table,
		.pce_microcode = &gswip_xrx500_pce_microcode,
		.pce_microcode_size = ARRAY_SIZE(gswip_xrx500_pce_microcode),
		.setup = gswip_xrx500_setup,
		.phylink_get_caps = gswip_xrx500_phylink_get_caps,
		.tag_protocol = DSA_TAG_PROTO_GSWIP3,
	},
};

static const struct gswip_xrx500_model gswip_xrx500_r = {
	/* A frame crossing this macro has a tag added, so the untagged limit
	 * is raised and short frames are padded.
	 */
	.pmac_eg_ports = GSWIP_XRX500_PMAC_EG_PORTS_ALL,
	.pmac_pad = true,
	.pmac_long_untagged = true,
	.hw_info = {
		.max_ports = GSWIP_XRX500_R_MAX_PORTS,
		.allowed_cpu_ports = BIT(0),
		.mii_cfg = { [0 ... GSWIP_MAX_PORTS - 1] = -1 },
		.mii_pcdu = { [0 ... GSWIP_MAX_PORTS - 1] = -1 },
		.mdio_layout = &gswip_xrx500_mdio_layout,
		.mac_ctrl = gswip_xrx500_mac_ctrl,
		.rmon_table = gswip_xrx500_rmon_table,
		.pce_microcode = &gswip_xrx500_pce_microcode,
		.pce_microcode_size = ARRAY_SIZE(gswip_xrx500_pce_microcode),
		.setup = gswip_xrx500_setup,
		.phylink_get_caps = gswip_xrx500_r_phylink_get_caps,
		.tag_protocol = DSA_TAG_PROTO_GSWIP3,
	},
};

static const struct of_device_id gswip_xrx500_of_match[] = {
	{ .compatible = "lantiq,xrx500-gswip", .data = &gswip_xrx500.hw_info },
	{ .compatible = "lantiq,xrx500-gswip-r",
	  .data = &gswip_xrx500_r.hw_info },
	{},
};
MODULE_DEVICE_TABLE(of, gswip_xrx500_of_match);

static struct platform_driver gswip_xrx500_driver = {
	.probe = gswip_xrx500_probe,
	.remove = gswip_xrx500_remove,
	.shutdown = gswip_xrx500_shutdown,
	.driver = {
		.name = "gswip-xrx500",
		.of_match_table = gswip_xrx500_of_match,
	},
};

static int __init gswip_xrx500_init(void)
{
	int err;

	err = phy_register_fixup_for_uid(GSWIP_XRX500_GPHY_ID,
					 GSWIP_XRX500_GPHY_ID_MASK,
					 gswip_xrx500_gphy_fixup);
	if (err)
		return err;

	err = platform_driver_register(&gswip_xrx500_driver);
	if (err)
		phy_unregister_fixup_for_uid(GSWIP_XRX500_GPHY_ID,
					     GSWIP_XRX500_GPHY_ID_MASK);

	return err;
}
module_init(gswip_xrx500_init);

static void __exit gswip_xrx500_exit(void)
{
	platform_driver_unregister(&gswip_xrx500_driver);
	phy_unregister_fixup_for_uid(GSWIP_XRX500_GPHY_ID,
				     GSWIP_XRX500_GPHY_ID_MASK);
}
module_exit(gswip_xrx500_exit);

MODULE_AUTHOR("Grische <github@grische.xyz>");
MODULE_DESCRIPTION("Lantiq / Intel GSWIP 3.0 switch driver");
/*
 * The blob is named by the device tree rather than by the driver, so this is
 * a declaration of the in-tree name and not the string the driver asks for.
 * It is what tells an image builder the module has a firmware dependency.
 */
MODULE_FIRMWARE("lantiq/xrx500-phy-fw.bin");
MODULE_LICENSE("GPL");
