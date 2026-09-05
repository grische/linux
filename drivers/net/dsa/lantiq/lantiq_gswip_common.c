// SPDX-License-Identifier: GPL-2.0
/*
 * Lantiq / Intel / MaxLinear GSWIP common function library
 *
 * Copyright (C) 2025 Daniel Golle <daniel@makrotopia.org>
 * Copyright (C) 2023 - 2024 MaxLinear Inc.
 * Copyright (C) 2022 Snap One, LLC.  All rights reserved.
 * Copyright (C) 2017 - 2019 Hauke Mehrtens <hauke@hauke-m.de>
 * Copyright (C) 2012 John Crispin <john@phrozen.org>
 * Copyright (C) 2010 Lantiq Deutschland
 *
 * The VLAN and bridge model the GSWIP hardware uses does not directly
 * matches the model DSA uses.
 *
 * The hardware has 64 possible table entries for bridges with one VLAN
 * ID, one flow id and a list of ports for each bridge. All entries which
 * match the same flow ID are combined in the mac learning table, they
 * act as one global bridge.
 * The hardware does not support VLAN filter on the port, but on the
 * bridge, this driver converts the DSA model to the hardware.
 *
 * The CPU gets all the exception frames which do not match any forwarding
 * rule and the CPU port is also added to all bridges. This makes it possible
 * to handle all the special cases easily in software.
 * At the initialization the driver allocates one bridge table entry for
 * each switch port which is used when the port is used without an
 * explicit bridge. This prevents the frames from being forwarded
 * between all LAN ports by default.
 */

#include "lantiq_gswip.h"

#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/phylink.h>
#include <linux/regmap.h>
#include <net/dsa.h>

struct gswip_pce_table_entry {
	u16 index;      // PCE_TBL_ADDR.ADDR = pData->table_index
	u16 table;      // PCE_TBL_CTRL.ADDR = pData->table
	u16 key[8];
	u16 val[5];
	u16 mask;
	u8 gmap;
	bool type;
	bool valid;
	bool key_mode;
};

struct gswip_rmon_cnt_desc {
	unsigned int size;
	unsigned int offset;
	const char *name;
};

#define MIB_DESC(_size, _offset, _name) {.size = _size, .offset = _offset, .name = _name}

/*
 * Counters inside one port's bank of the buffer manager's counter RAM. A
 * counter occupies one entry, except for the four byte counts, which occupy
 * the named entry and the one above it.
 */
enum gswip_rmon_counter {
	GSWIP_RMON_TX_64B_PKTS = 0x00,
	GSWIP_RMON_TX_127B_PKTS = 0x01,
	GSWIP_RMON_TX_255B_PKTS = 0x02,
	GSWIP_RMON_TX_511B_PKTS = 0x03,
	GSWIP_RMON_TX_1023B_PKTS = 0x04,
	GSWIP_RMON_TX_MAXB_PKTS = 0x05,
	GSWIP_RMON_TX_UNICAST_PKTS = 0x06,
	GSWIP_RMON_TX_MULTICAST_PKTS = 0x07,
	GSWIP_RMON_TX_SINGLE_COLL = 0x08,
	GSWIP_RMON_TX_MULT_COLL = 0x09,
	GSWIP_RMON_TX_LATE_COLL = 0x0A,
	GSWIP_RMON_TX_EXCESS_COLL = 0x0B,
	GSWIP_RMON_TX_GOOD_PKTS = 0x0C,
	GSWIP_RMON_TX_PAUSE = 0x0D,
	GSWIP_RMON_TX_GOOD_BYTES = 0x0E,
	GSWIP_RMON_TX_DROPPED_PKTS = 0x10,
	GSWIP_RMON_TX_ACM_DROPPED_PKTS = 0x11,
	GSWIP_RMON_RX_64B_PKTS = 0x12,
	GSWIP_RMON_RX_127B_PKTS = 0x13,
	GSWIP_RMON_RX_255B_PKTS = 0x14,
	GSWIP_RMON_RX_511B_PKTS = 0x15,
	GSWIP_RMON_RX_1023B_PKTS = 0x16,
	GSWIP_RMON_RX_MAXB_PKTS = 0x17,
	GSWIP_RMON_RX_DROPPED_PKTS = 0x18,
	GSWIP_RMON_RX_FILTERED_PKTS = 0x19,
	GSWIP_RMON_RX_ALIGN_ERROR_PKTS = 0x1A,
	GSWIP_RMON_RX_OVERSIZE_GOOD_PKTS = 0x1B,
	GSWIP_RMON_RX_OVERSIZE_ERROR_PKTS = 0x1C,
	GSWIP_RMON_RX_UNDERSIZE_GOOD_PKTS = 0x1D,
	GSWIP_RMON_RX_UNDERSIZE_ERROR_PKTS = 0x1E,
	GSWIP_RMON_RX_GOOD_PKTS = 0x1F,
	GSWIP_RMON_RX_GOOD_PAUSE_PKTS = 0x20,
	GSWIP_RMON_RX_FCS_ERROR_PKTS = 0x21,
	GSWIP_RMON_RX_MULTICAST_PKTS = 0x22,
	GSWIP_RMON_RX_UNICAST_PKTS = 0x23,
	GSWIP_RMON_RX_GOOD_BYTES = 0x24,
	GSWIP_RMON_RX_BAD_BYTES = 0x26,
};

static const struct gswip_rmon_cnt_desc gswip_rmon_cnt[] = {
	/** Receive Packet Count (only packets that are accepted and not discarded). */
	MIB_DESC(1, GSWIP_RMON_RX_GOOD_PKTS, "RxGoodPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_UNICAST_PKTS, "RxUnicastPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_MULTICAST_PKTS, "RxMulticastPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_FCS_ERROR_PKTS, "RxFCSErrorPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_UNDERSIZE_GOOD_PKTS, "RxUnderSizeGoodPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_UNDERSIZE_ERROR_PKTS, "RxUnderSizeErrorPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_OVERSIZE_GOOD_PKTS, "RxOversizeGoodPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_OVERSIZE_ERROR_PKTS, "RxOversizeErrorPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_GOOD_PAUSE_PKTS, "RxGoodPausePkts"),
	MIB_DESC(1, GSWIP_RMON_RX_ALIGN_ERROR_PKTS, "RxAlignErrorPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_64B_PKTS, "Rx64BytePkts"),
	MIB_DESC(1, GSWIP_RMON_RX_127B_PKTS, "Rx127BytePkts"),
	MIB_DESC(1, GSWIP_RMON_RX_255B_PKTS, "Rx255BytePkts"),
	MIB_DESC(1, GSWIP_RMON_RX_511B_PKTS, "Rx511BytePkts"),
	MIB_DESC(1, GSWIP_RMON_RX_1023B_PKTS, "Rx1023BytePkts"),
	/** Receive Size 1024-1522 (or more, if configured) Packet Count. */
	MIB_DESC(1, GSWIP_RMON_RX_MAXB_PKTS, "RxMaxBytePkts"),
	MIB_DESC(1, GSWIP_RMON_RX_DROPPED_PKTS, "RxDroppedPkts"),
	MIB_DESC(1, GSWIP_RMON_RX_FILTERED_PKTS, "RxFilteredPkts"),
	MIB_DESC(2, GSWIP_RMON_RX_GOOD_BYTES, "RxGoodBytes"),
	MIB_DESC(2, GSWIP_RMON_RX_BAD_BYTES, "RxBadBytes"),
	MIB_DESC(1, GSWIP_RMON_TX_ACM_DROPPED_PKTS, "TxAcmDroppedPkts"),
	MIB_DESC(1, GSWIP_RMON_TX_GOOD_PKTS, "TxGoodPkts"),
	MIB_DESC(1, GSWIP_RMON_TX_UNICAST_PKTS, "TxUnicastPkts"),
	MIB_DESC(1, GSWIP_RMON_TX_MULTICAST_PKTS, "TxMulticastPkts"),
	MIB_DESC(1, GSWIP_RMON_TX_64B_PKTS, "Tx64BytePkts"),
	MIB_DESC(1, GSWIP_RMON_TX_127B_PKTS, "Tx127BytePkts"),
	MIB_DESC(1, GSWIP_RMON_TX_255B_PKTS, "Tx255BytePkts"),
	MIB_DESC(1, GSWIP_RMON_TX_511B_PKTS, "Tx511BytePkts"),
	MIB_DESC(1, GSWIP_RMON_TX_1023B_PKTS, "Tx1023BytePkts"),
	/** Transmit Size 1024-1522 (or more, if configured) Packet Count. */
	MIB_DESC(1, GSWIP_RMON_TX_MAXB_PKTS, "TxMaxBytePkts"),
	MIB_DESC(1, GSWIP_RMON_TX_SINGLE_COLL, "TxSingleCollCount"),
	MIB_DESC(1, GSWIP_RMON_TX_MULT_COLL, "TxMultCollCount"),
	MIB_DESC(1, GSWIP_RMON_TX_LATE_COLL, "TxLateCollCount"),
	MIB_DESC(1, GSWIP_RMON_TX_EXCESS_COLL, "TxExcessCollCount"),
	MIB_DESC(1, GSWIP_RMON_TX_PAUSE, "TxPauseCount"),
	MIB_DESC(1, GSWIP_RMON_TX_DROPPED_PKTS, "TxDroppedPkts"),
	MIB_DESC(2, GSWIP_RMON_TX_GOOD_BYTES, "TxGoodBytes"),
};

static u32 gswip_switch_r_timeout(struct gswip_priv *priv, u32 offset,
				  u32 cleared)
{
	u32 val;

	return regmap_read_poll_timeout(priv->gswip, offset, val,
					!(val & cleared), 20, 50000);
}

static void gswip_mii_mask_cfg(struct gswip_priv *priv, u32 mask, u32 set,
			       int port)
{
	/* MII_CFG register only exists for MII ports */
	if (priv->hw_info->mii_cfg[port] == -1)
		return;

	regmap_write_bits(priv->mii, priv->hw_info->mii_cfg[port], mask,
			  set);
}

static const struct gswip_mdio_layout gswip_mdio_layout_2x = {
	.glob		= GSWIP_MDIO_GLOB,
	.ctrl		= GSWIP_MDIO_CTRL,
	.read		= GSWIP_MDIO_READ,
	.write		= GSWIP_MDIO_WRITE,
	.mdc_cfg0	= GSWIP_MDIO_MDC_CFG0,
	.mdc_cfg1	= GSWIP_MDIO_MDC_CFG1,
	.mdc_cfg1_mask	= 0xff,
	.mdc_cfg1_val	= 0x09,
	.phy		= {
		GSWIP_MDIO_PHYp(0), GSWIP_MDIO_PHYp(1), GSWIP_MDIO_PHYp(2),
		GSWIP_MDIO_PHYp(3), GSWIP_MDIO_PHYp(4), GSWIP_MDIO_PHYp(5),
		GSWIP_MDIO_PHYp(6),
		[GSWIP_2X_MAX_PORTS ... GSWIP_MAX_PORTS - 1] = -1,
	},
};

static void gswip_mdio_mask_phy(struct gswip_priv *priv, int port, u32 mask,
				u32 set)
{
	/* PHY register only exists for ports the MDIO master can poll */
	if (priv->mdio_layout->phy[port] == -1)
		return;

	regmap_write_bits(priv->mdio, priv->mdio_layout->phy[port], mask, set);
}

static const s16 gswip_mac_ctrl_2x[GSWIP_MAX_PORTS] = {
	GSWIP_MAC_CTRL_BASEp(0), GSWIP_MAC_CTRL_BASEp(1),
	GSWIP_MAC_CTRL_BASEp(2), GSWIP_MAC_CTRL_BASEp(3),
	GSWIP_MAC_CTRL_BASEp(4), GSWIP_MAC_CTRL_BASEp(5),
	GSWIP_MAC_CTRL_BASEp(6),
	[GSWIP_2X_MAX_PORTS ... GSWIP_MAX_PORTS - 1] = -1,
};

/* Returns the address of one register of a port's MAC block, or a negative
 * value if the port has no MAC block.
 */
static int gswip_mac_ctrl_reg(struct gswip_priv *priv, int port, u16 reg)
{
	if (priv->mac_ctrl[port] == -1)
		return -1;

	return priv->mac_ctrl[port] + reg;
}

static const s16 gswip_rmon_table_2x[GSWIP_MAX_PORTS] = {
	0, 1, 2, 3, 4, 5, 6,
	[GSWIP_2X_MAX_PORTS ... GSWIP_MAX_PORTS - 1] = -1,
};

/* Some models raise the busy bit themselves a short time after the control
 * register is written, rather than taking it from the written value. Polling
 * for the bit to clear before that happens returns the result of the previous
 * transaction, so a model that behaves that way asks for a settle delay.
 */
static int gswip_mdio_poll(struct gswip_priv *priv)
{
	u32 ctrl;

	return regmap_read_poll_timeout(priv->mdio, priv->mdio_layout->ctrl,
					ctrl, !(ctrl & GSWIP_MDIO_CTRL_BUSY),
					40, 4000);
}

static int gswip_mdio_wr(struct mii_bus *bus, int addr, int reg, u16 val)
{
	struct gswip_priv *priv = bus->priv;
	int err;

	err = gswip_mdio_poll(priv);
	if (err) {
		dev_err(&bus->dev, "waiting for MDIO bus busy timed out\n");
		return err;
	}

	regmap_write(priv->mdio, priv->mdio_layout->write, val);
	regmap_write(priv->mdio, priv->mdio_layout->ctrl,
		     GSWIP_MDIO_CTRL_BUSY | GSWIP_MDIO_CTRL_WR |
		     ((addr & GSWIP_MDIO_CTRL_PHYAD_MASK) << GSWIP_MDIO_CTRL_PHYAD_SHIFT) |
		     (reg & GSWIP_MDIO_CTRL_REGAD_MASK));
	udelay(priv->mdio_layout->settle_us);

	return 0;
}

static int gswip_mdio_rd(struct mii_bus *bus, int addr, int reg)
{
	struct gswip_priv *priv = bus->priv;
	u32 val;
	int err;

	err = gswip_mdio_poll(priv);
	if (err) {
		dev_err(&bus->dev, "waiting for MDIO bus busy timed out\n");
		return err;
	}

	regmap_write(priv->mdio, priv->mdio_layout->ctrl,
		     GSWIP_MDIO_CTRL_BUSY | GSWIP_MDIO_CTRL_RD |
		     ((addr & GSWIP_MDIO_CTRL_PHYAD_MASK) << GSWIP_MDIO_CTRL_PHYAD_SHIFT) |
		     (reg & GSWIP_MDIO_CTRL_REGAD_MASK));
	udelay(priv->mdio_layout->settle_us);

	err = gswip_mdio_poll(priv);
	if (err) {
		dev_err(&bus->dev, "waiting for MDIO bus busy timed out\n");
		return err;
	}

	err = regmap_read(priv->mdio, priv->mdio_layout->read, &val);
	if (err)
		return err;

	return val;
}

static int gswip_mdio(struct gswip_priv *priv)
{
	struct device_node *mdio_np, *switch_np = priv->dev->of_node;
	struct device *dev = priv->dev;
	struct mii_bus *bus;
	int err = 0;

	mdio_np = of_get_compatible_child(switch_np, "lantiq,xrx200-mdio");
	if (!mdio_np)
		mdio_np = of_get_child_by_name(switch_np, "mdio");

	if (!of_device_is_available(mdio_np))
		goto out_put_node;

	bus = devm_mdiobus_alloc(dev);
	if (!bus) {
		err = -ENOMEM;
		goto out_put_node;
	}

	bus->priv = priv;
	bus->read = gswip_mdio_rd;
	bus->write = gswip_mdio_wr;
	bus->name = "lantiq,xrx200-mdio";
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s-mii", dev_name(priv->dev));
	bus->parent = priv->dev;

	err = devm_of_mdiobus_register(dev, bus, mdio_np);

out_put_node:
	of_node_put(mdio_np);

	return err;
}

static int gswip_pce_table_entry_read(struct gswip_priv *priv,
				      struct gswip_pce_table_entry *tbl)
{
	int i;
	int err;
	u32 crtl;
	u32 tmp;
	u16 addr_mode = tbl->key_mode ? GSWIP_PCE_TBL_CTRL_OPMOD_KSRD :
					GSWIP_PCE_TBL_CTRL_OPMOD_ADRD;

	mutex_lock(&priv->pce_table_lock);

	err = gswip_switch_r_timeout(priv, GSWIP_PCE_TBL_CTRL,
				     GSWIP_PCE_TBL_CTRL_BAS);
	if (err)
		goto out_unlock;

	regmap_write(priv->gswip, GSWIP_PCE_TBL_ADDR, tbl->index);
	regmap_write_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
			  GSWIP_PCE_TBL_CTRL_ADDR_MASK |
			  GSWIP_PCE_TBL_CTRL_OPMOD_MASK |
			  GSWIP_PCE_TBL_CTRL_BAS,
			  tbl->table | addr_mode | GSWIP_PCE_TBL_CTRL_BAS);

	err = gswip_switch_r_timeout(priv, GSWIP_PCE_TBL_CTRL,
				     GSWIP_PCE_TBL_CTRL_BAS);
	if (err)
		goto out_unlock;

	for (i = 0; i < ARRAY_SIZE(tbl->key); i++) {
		err = regmap_read(priv->gswip, GSWIP_PCE_TBL_KEY(i), &tmp);
		if (err)
			goto out_unlock;
		tbl->key[i] = tmp;
	}
	for (i = 0; i < ARRAY_SIZE(tbl->val); i++) {
		err = regmap_read(priv->gswip, GSWIP_PCE_TBL_VAL(i), &tmp);
		if (err)
			goto out_unlock;
		tbl->val[i] = tmp;
	}

	err = regmap_read(priv->gswip, GSWIP_PCE_TBL_MASK, &tmp);
	if (err)
		goto out_unlock;

	tbl->mask = tmp;
	err = regmap_read(priv->gswip, GSWIP_PCE_TBL_CTRL, &crtl);
	if (err)
		goto out_unlock;

	tbl->type = !!(crtl & GSWIP_PCE_TBL_CTRL_TYPE);
	tbl->valid = !!(crtl & GSWIP_PCE_TBL_CTRL_VLD);
	tbl->gmap = (crtl & GSWIP_PCE_TBL_CTRL_GMAP_MASK) >> 7;

out_unlock:
	mutex_unlock(&priv->pce_table_lock);

	return err;
}

static int gswip_pce_table_entry_write(struct gswip_priv *priv,
				       struct gswip_pce_table_entry *tbl)
{
	int i;
	int err;
	u32 crtl;
	u16 addr_mode = tbl->key_mode ? GSWIP_PCE_TBL_CTRL_OPMOD_KSWR :
					GSWIP_PCE_TBL_CTRL_OPMOD_ADWR;

	mutex_lock(&priv->pce_table_lock);

	err = gswip_switch_r_timeout(priv, GSWIP_PCE_TBL_CTRL,
				     GSWIP_PCE_TBL_CTRL_BAS);
	if (err) {
		mutex_unlock(&priv->pce_table_lock);
		return err;
	}

	regmap_write(priv->gswip, GSWIP_PCE_TBL_ADDR, tbl->index);
	regmap_write_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
			  GSWIP_PCE_TBL_CTRL_ADDR_MASK |
			  GSWIP_PCE_TBL_CTRL_OPMOD_MASK,
			  tbl->table | addr_mode);

	for (i = 0; i < ARRAY_SIZE(tbl->key); i++)
		regmap_write(priv->gswip, GSWIP_PCE_TBL_KEY(i), tbl->key[i]);

	for (i = 0; i < ARRAY_SIZE(tbl->val); i++)
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(i), tbl->val[i]);

	regmap_write_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
			  GSWIP_PCE_TBL_CTRL_ADDR_MASK |
			  GSWIP_PCE_TBL_CTRL_OPMOD_MASK,
			  tbl->table | addr_mode);

	regmap_write(priv->gswip, GSWIP_PCE_TBL_MASK, tbl->mask);

	regmap_read(priv->gswip, GSWIP_PCE_TBL_CTRL, &crtl);
	crtl &= ~(GSWIP_PCE_TBL_CTRL_TYPE | GSWIP_PCE_TBL_CTRL_VLD |
		  GSWIP_PCE_TBL_CTRL_GMAP_MASK);
	if (tbl->type)
		crtl |= GSWIP_PCE_TBL_CTRL_TYPE;
	if (tbl->valid)
		crtl |= GSWIP_PCE_TBL_CTRL_VLD;
	crtl |= (tbl->gmap << 7) & GSWIP_PCE_TBL_CTRL_GMAP_MASK;
	crtl |= GSWIP_PCE_TBL_CTRL_BAS;
	regmap_write(priv->gswip, GSWIP_PCE_TBL_CTRL, crtl);

	err = gswip_switch_r_timeout(priv, GSWIP_PCE_TBL_CTRL,
				     GSWIP_PCE_TBL_CTRL_BAS);

	mutex_unlock(&priv->pce_table_lock);

	return err;
}

/* Add the LAN port into a bridge with the CPU port by
 * default. This prevents automatic forwarding of
 * packages between the LAN ports when no explicit
 * bridge is configured.
 */
static int gswip_add_single_port_br(struct gswip_priv *priv, int port, bool add)
{
	struct gswip_pce_table_entry vlan_active = {0,};
	struct gswip_pce_table_entry vlan_mapping = {0,};
	int err;

	vlan_active.index = port + 1;
	vlan_active.table = GSWIP_TABLE_ACTIVE_VLAN;
	vlan_active.key[0] = GSWIP_VLAN_UNAWARE_PVID;
	vlan_active.val[0] = port + 1 /* fid */;
	vlan_active.valid = add;
	err = gswip_pce_table_entry_write(priv, &vlan_active);
	if (err) {
		dev_err(priv->dev, "failed to write active VLAN: %d\n", err);
		return err;
	}

	if (!add)
		return 0;

	vlan_mapping.index = port + 1;
	vlan_mapping.table = GSWIP_TABLE_VLAN_MAPPING;
	vlan_mapping.val[0] = GSWIP_VLAN_UNAWARE_PVID;
	vlan_mapping.val[1] = BIT(port) | dsa_cpu_ports(priv->ds);
	vlan_mapping.val[2] = 0;
	err = gswip_pce_table_entry_write(priv, &vlan_mapping);
	if (err) {
		dev_err(priv->dev, "failed to write VLAN mapping: %d\n", err);
		return err;
	}

	return 0;
}

static int gswip_port_set_learning(struct gswip_priv *priv, int port,
				   bool enable)
{
	if (!GSWIP_VERSION_GE(priv, GSWIP_VERSION_2_2))
		return -EOPNOTSUPP;

	/* learning disable bit */
	return regmap_update_bits(priv->gswip, GSWIP_PCE_PCTRL_3p(port),
				  GSWIP_PCE_PCTRL_3_LNDIS,
				  enable ? 0 : GSWIP_PCE_PCTRL_3_LNDIS);
}

/* The forwarding domain a port belongs to follows its bridge membership, and
 * the DSA core has already updated that by the time a join or a leave reaches
 * this driver. Deriving the domain here rather than taking it from the caller
 * gives port setup, a join, a leave and any later path that re-initialises the
 * switch one entry point that always writes the state the port should have.
 */
static u16 gswip_port_fid(struct gswip_priv *priv, int port)
{
	struct dsa_bridge *bridge = dsa_to_port(priv->ds, port)->bridge;

	if (bridge)
		return bridge->num;

	return GSWIP_FID_STANDALONE_BASE + port;
}

static int gswip_port_apply_fid(struct gswip_priv *priv, int port)
{
	if (!priv->hw_info->port_set_fid)
		return 0;

	return priv->hw_info->port_set_fid(priv->ds, port,
					   gswip_port_fid(priv, port));
}

static int gswip_port_pre_bridge_flags(struct dsa_switch *ds, int port,
				       struct switchdev_brport_flags flags,
				       struct netlink_ext_ack *extack)
{
	struct gswip_priv *priv = ds->priv;
	unsigned long supported = 0;

	if (GSWIP_VERSION_GE(priv, GSWIP_VERSION_2_2))
		supported |= BR_LEARNING;

	if (flags.mask & ~supported)
		return -EINVAL;

	return 0;
}

static int gswip_port_bridge_flags(struct dsa_switch *ds, int port,
				   struct switchdev_brport_flags flags,
				   struct netlink_ext_ack *extack)
{
	struct gswip_priv *priv = ds->priv;

	if (flags.mask & BR_LEARNING)
		return gswip_port_set_learning(priv, port,
					       !!(flags.val & BR_LEARNING));

	return 0;
}

static int gswip_port_setup(struct dsa_switch *ds, int port)
{
	struct gswip_priv *priv = ds->priv;
	int err;

	if (priv->hw_info->port_setup) {
		err = priv->hw_info->port_setup(ds, port);
		if (err)
			return err;
	}

	if (!dsa_is_cpu_port(ds, port)) {
		err = gswip_add_single_port_br(priv, port, true);
		if (err)
			return err;

		/* A port the device tree does not describe gets no domain of
		 * its own here, which leaves it in domain 0 where nothing is
		 * ever learned, so its frames reach the processor rather than
		 * the fabric.
		 */
		err = gswip_port_apply_fid(priv, port);
		if (err)
			return err;
	}

	return 0;
}

static int gswip_port_enable(struct dsa_switch *ds, int port,
			     struct phy_device *phydev)
{
	struct gswip_priv *priv = ds->priv;

	if (!dsa_is_cpu_port(ds, port)) {
		u32 mdio_phy = 0;

		if (phydev)
			mdio_phy = phydev->mdio.addr & GSWIP_MDIO_PHY_ADDR_MASK;

		gswip_mdio_mask_phy(priv, port, GSWIP_MDIO_PHY_ADDR_MASK,
				    mdio_phy);
	}

	/* RMON Counter Enable for port */
	regmap_write(priv->gswip, GSWIP_BM_PCFGp(port), GSWIP_BM_PCFG_CNTEN);

	/* enable port fetch/store dma & VLAN Modification */
	regmap_set_bits(priv->gswip, GSWIP_FDMA_PCTRLp(port),
			GSWIP_FDMA_PCTRL_EN | GSWIP_FDMA_PCTRL_VLANMOD_BOTH);
	regmap_set_bits(priv->gswip, GSWIP_SDMA_PCTRLp(port),
			GSWIP_SDMA_PCTRL_EN);

	return 0;
}

static void gswip_port_disable(struct dsa_switch *ds, int port)
{
	struct gswip_priv *priv = ds->priv;

	regmap_clear_bits(priv->gswip, GSWIP_FDMA_PCTRLp(port),
			  GSWIP_FDMA_PCTRL_EN);
	regmap_clear_bits(priv->gswip, GSWIP_SDMA_PCTRLp(port),
			  GSWIP_SDMA_PCTRL_EN);
}

static int gswip_pce_load_microcode(struct gswip_priv *priv)
{
	int i;
	int err;

	regmap_write_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
			  GSWIP_PCE_TBL_CTRL_ADDR_MASK |
			  GSWIP_PCE_TBL_CTRL_OPMOD_MASK |
			  GSWIP_PCE_TBL_CTRL_OPMOD_ADWR,
			  GSWIP_PCE_TBL_CTRL_OPMOD_ADWR);
	regmap_write(priv->gswip, GSWIP_PCE_TBL_MASK, 0);

	for (i = 0; i < priv->hw_info->pce_microcode_size; i++) {
		regmap_write(priv->gswip, GSWIP_PCE_TBL_ADDR, i);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(0),
			     (*priv->hw_info->pce_microcode)[i].val_0);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(1),
			     (*priv->hw_info->pce_microcode)[i].val_1);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(2),
			     (*priv->hw_info->pce_microcode)[i].val_2);
		regmap_write(priv->gswip, GSWIP_PCE_TBL_VAL(3),
			     (*priv->hw_info->pce_microcode)[i].val_3);

		/* start the table access: */
		regmap_set_bits(priv->gswip, GSWIP_PCE_TBL_CTRL,
				GSWIP_PCE_TBL_CTRL_BAS);
		err = gswip_switch_r_timeout(priv, GSWIP_PCE_TBL_CTRL,
					     GSWIP_PCE_TBL_CTRL_BAS);
		if (err)
			return err;
	}

	/* tell the switch that the microcode is loaded */
	regmap_set_bits(priv->gswip, GSWIP_PCE_GCTRL_0,
			GSWIP_PCE_GCTRL_0_MC_VALID);

	return 0;
}

static void gswip_port_commit_pvid(struct gswip_priv *priv, int port)
{
	struct dsa_port *dp = dsa_to_port(priv->ds, port);
	struct net_device *br = dsa_port_bridge_dev_get(dp);
	u32 vinr;
	int idx;

	if (!dsa_port_is_user(dp))
		return;

	if (br) {
		u16 pvid = GSWIP_VLAN_UNAWARE_PVID;

		if (br_vlan_enabled(br))
			br_vlan_get_pvid(br, &pvid);

		/* VLAN-aware bridge ports with no PVID will use Active VLAN
		 * index 0. The expectation is that this drops all untagged and
		 * VID-0 tagged ingress traffic.
		 */
		idx = 0;
		for (int i = priv->hw_info->max_ports;
		     i < ARRAY_SIZE(priv->vlans); i++) {
			if (priv->vlans[i].bridge == br &&
			    priv->vlans[i].vid == pvid) {
				idx = i;
				break;
			}
		}
	} else {
		/* The Active VLAN table index as configured by
		 * gswip_add_single_port_br()
		 */
		idx = port + 1;
	}

	vinr = idx ? GSWIP_PCE_VCTRL_VINR_ALL : GSWIP_PCE_VCTRL_VINR_TAGGED;
	regmap_write_bits(priv->gswip, GSWIP_PCE_VCTRL(port),
			  GSWIP_PCE_VCTRL_VINR,
			  FIELD_PREP(GSWIP_PCE_VCTRL_VINR, vinr));

	/* Note that in GSWIP 2.2 VLAN mode the VID needs to be programmed
	 * directly instead of referencing the index in the Active VLAN Tablet.
	 * However, without the VLANMD bit (9) in PCE_GCTRL_1 (0x457) even
	 * GSWIP 2.2 and newer hardware maintain the GSWIP 2.1 behavior.
	 */
	regmap_write(priv->gswip, GSWIP_PCE_DEFPVID(port), idx);
}

/* A model that confines the destination lookup to a forwarding domain gets one
 * domain per bridge, not one per VLAN: the identifier the VLAN tables carry
 * takes no part in that lookup. So a VLAN-aware bridge is fenced off from every
 * other bridge and from every standalone port, but two of its own VLANs share a
 * domain and a unicast between them is forwarded in hardware where the bridge
 * would not have forwarded it.
 *
 * Refusing the attribute would not close that: the core turns the refusal into
 * a no-op when a port joins an already-filtering bridge, and the port would
 * then keep port-based VLAN mode while the bridge tagged its frames, which
 * breaks more than it fences. The tagging behaviour below is therefore kept and
 * the limit is documented instead; separating VLANs needs a domain per VLAN.
 */
static int gswip_port_vlan_filtering(struct dsa_switch *ds, int port,
				     bool vlan_filtering,
				     struct netlink_ext_ack *extack)
{
	struct gswip_priv *priv = ds->priv;

	if (vlan_filtering) {
		/* Use tag based VLAN */
		regmap_write_bits(priv->gswip, GSWIP_PCE_VCTRL(port),
				  GSWIP_PCE_VCTRL_VSR |
				  GSWIP_PCE_VCTRL_UVR |
				  GSWIP_PCE_VCTRL_VIMR |
				  GSWIP_PCE_VCTRL_VEMR |
				  GSWIP_PCE_VCTRL_VID0,
				  GSWIP_PCE_VCTRL_UVR |
				  GSWIP_PCE_VCTRL_VIMR |
				  GSWIP_PCE_VCTRL_VEMR |
				  GSWIP_PCE_VCTRL_VID0);
		regmap_clear_bits(priv->gswip, GSWIP_PCE_PCTRL_0p(port),
				  GSWIP_PCE_PCTRL_0_TVM);
	} else {
		/* Use port based VLAN */
		regmap_write_bits(priv->gswip, GSWIP_PCE_VCTRL(port),
				  GSWIP_PCE_VCTRL_UVR |
				  GSWIP_PCE_VCTRL_VIMR |
				  GSWIP_PCE_VCTRL_VEMR |
				  GSWIP_PCE_VCTRL_VID0 |
				  GSWIP_PCE_VCTRL_VSR,
				  GSWIP_PCE_VCTRL_VSR);
		regmap_set_bits(priv->gswip, GSWIP_PCE_PCTRL_0p(port),
				GSWIP_PCE_PCTRL_0_TVM);
	}

	gswip_port_commit_pvid(priv, port);

	return 0;
}

static void gswip_mii_delay_setup(struct gswip_priv *priv, struct dsa_port *dp,
				  phy_interface_t interface)
{
	u32 tx_delay = GSWIP_MII_PCDU_TXDLY_DEFAULT;
	u32 rx_delay = GSWIP_MII_PCDU_RXDLY_DEFAULT;
	struct device_node *port_dn = dp->dn;

	/* As MII_PCDU registers only exist for MII ports, silently return
	 * unless the port is an MII port
	 */
	if (priv->hw_info->mii_pcdu[dp->index] == -1)
		return;

	/* legacy code to set default delays according to the interface mode */
	switch (interface) {
	case PHY_INTERFACE_MODE_RGMII_ID:
		tx_delay = 0;
		rx_delay = 0;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		rx_delay = 0;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		tx_delay = 0;
		break;
	default:
		break;
	}

	/* allow settings delays using device tree properties */
	of_property_read_u32(port_dn, "rx-internal-delay-ps", &rx_delay);
	of_property_read_u32(port_dn, "tx-internal-delay-ps", &tx_delay);

	regmap_write_bits(priv->mii, priv->hw_info->mii_pcdu[dp->index],
			  GSWIP_MII_PCDU_TXDLY_MASK |
			  GSWIP_MII_PCDU_RXDLY_MASK,
			  GSWIP_MII_PCDU_TXDLY(tx_delay) |
			  GSWIP_MII_PCDU_RXDLY(rx_delay));
}

static int gswip_setup(struct dsa_switch *ds)
{
	unsigned int cpu_ports = dsa_cpu_ports(ds);
	struct gswip_priv *priv = ds->priv;
	struct dsa_port *cpu_dp;
	int err, i;

	regmap_write(priv->gswip, GSWIP_SWRES, GSWIP_SWRES_R0);
	usleep_range(5000, 10000);
	regmap_write(priv->gswip, GSWIP_SWRES, 0);

	/* disable port fetch/store dma on all ports */
	for (i = 0; i < priv->hw_info->max_ports; i++) {
		gswip_port_disable(ds, i);
		gswip_port_vlan_filtering(ds, i, false, NULL);
	}

	/* enable Switch */
	regmap_set_bits(priv->mdio, priv->mdio_layout->glob,
			GSWIP_MDIO_GLOB_ENABLE);

	err = gswip_pce_load_microcode(priv);
	if (err) {
		dev_err(priv->dev, "writing PCE microcode failed, %i\n", err);
		return err;
	}

	/* Default unknown Broadcast/Multicast/Unicast port maps */
	regmap_write(priv->gswip, GSWIP_PCE_PMAP1, cpu_ports);
	regmap_write(priv->gswip, GSWIP_PCE_PMAP2, cpu_ports);
	regmap_write(priv->gswip, GSWIP_PCE_PMAP3, cpu_ports);

	/* Deactivate MDIO PHY auto polling. Some PHYs as the AR8030 have an
	 * interoperability problem with this auto polling mechanism because
	 * their status registers think that the link is in a different state
	 * than it actually is. For the AR8030 it has the BMSR_ESTATEN bit set
	 * as well as ESTATUS_1000_TFULL and ESTATUS_1000_XFULL. This makes the
	 * auto polling state machine consider the link being negotiated with
	 * 1Gbit/s. Since the PHY itself is a Fast Ethernet RMII PHY this leads
	 * to the switch port being completely dead (RX and TX are both not
	 * working).
	 * Also with various other PHY / port combinations (PHY11G GPHY, PHY22F
	 * GPHY, external RGMII PEF7071/7072) any traffic would stop. Sometimes
	 * it would work fine for a few minutes to hours and then stop, on
	 * other device it would no traffic could be sent or received at all.
	 * Testing shows that when PHY auto polling is disabled these problems
	 * go away.
	 */
	regmap_write(priv->mdio, priv->mdio_layout->mdc_cfg0, 0x0);

	/* Configure the MDIO clock. On GSWIP-2.x the clock divider is the only
	 * field the driver owns in this register; later models also carry the
	 * MDIO master enable here, and it has to be set before the bus is
	 * registered and the first transaction is issued.
	 */
	regmap_write_bits(priv->mdio, priv->mdio_layout->mdc_cfg1,
			  priv->mdio_layout->mdc_cfg1_mask,
			  priv->mdio_layout->mdc_cfg1_val);

	/* bring up the mdio bus */
	err = gswip_mdio(priv);
	if (err) {
		dev_err(priv->dev, "mdio bus setup failed\n");
		return err;
	}

	/* Disable the xMII interface and clear it's isolation bit */
	for (i = 0; i < priv->hw_info->max_ports; i++)
		gswip_mii_mask_cfg(priv,
				   GSWIP_MII_CFG_EN | GSWIP_MII_CFG_ISOLATE,
				   0, i);

	dsa_switch_for_each_cpu_port(cpu_dp, ds) {
		/* enable special tag insertion on cpu port */
		regmap_set_bits(priv->gswip, GSWIP_FDMA_PCTRLp(cpu_dp->index),
				GSWIP_FDMA_PCTRL_STEN);

		/* accept special tag in ingress direction */
		regmap_set_bits(priv->gswip,
				GSWIP_PCE_PCTRL_0p(cpu_dp->index),
				GSWIP_PCE_PCTRL_0_INGRESS);
	}

	regmap_set_bits(priv->gswip, GSWIP_BM_QUEUE_GCTRL,
			GSWIP_BM_QUEUE_GCTRL_GL_MOD);

	/* VLAN aware Switching */
	regmap_set_bits(priv->gswip, GSWIP_PCE_GCTRL_0,
			GSWIP_PCE_GCTRL_0_VLAN);

	/* Flush MAC Table */
	regmap_set_bits(priv->gswip, GSWIP_PCE_GCTRL_0,
			GSWIP_PCE_GCTRL_0_MTFL);

	err = gswip_switch_r_timeout(priv, GSWIP_PCE_GCTRL_0,
				     GSWIP_PCE_GCTRL_0_MTFL);
	if (err) {
		dev_err(priv->dev, "MAC flushing didn't finish\n");
		return err;
	}

	ds->mtu_enforcement_ingress = true;

	if (priv->hw_info->setup) {
		err = priv->hw_info->setup(ds);
		if (err)
			return err;
	}

	return 0;
}

static void gswip_teardown(struct dsa_switch *ds)
{
	struct gswip_priv *priv = ds->priv;

	regmap_clear_bits(priv->mdio, priv->mdio_layout->glob,
			  GSWIP_MDIO_GLOB_ENABLE);
}

static enum dsa_tag_protocol gswip_get_tag_protocol(struct dsa_switch *ds,
						    int port,
						    enum dsa_tag_protocol mp)
{
	struct gswip_priv *priv = ds->priv;

	return priv->hw_info->tag_protocol;
}

static int gswip_vlan_active_create(struct gswip_priv *priv,
				    struct net_device *bridge,
				    int fid, u16 vid)
{
	struct gswip_pce_table_entry vlan_active = {0,};
	unsigned int max_ports = priv->hw_info->max_ports;
	int idx = -1;
	int err;
	int i;

	/* Look for a free slot */
	for (i = max_ports; i < ARRAY_SIZE(priv->vlans); i++) {
		if (!priv->vlans[i].bridge) {
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return -ENOSPC;

	if (fid == -1)
		fid = idx;

	vlan_active.index = idx;
	vlan_active.table = GSWIP_TABLE_ACTIVE_VLAN;
	vlan_active.key[0] = vid;
	vlan_active.val[0] = fid;
	vlan_active.valid = true;

	err = gswip_pce_table_entry_write(priv, &vlan_active);
	if (err) {
		dev_err(priv->dev, "failed to write active VLAN: %d\n",	err);
		return err;
	}

	priv->vlans[idx].bridge = bridge;
	priv->vlans[idx].vid = vid;
	priv->vlans[idx].fid = fid;

	return idx;
}

static int gswip_vlan_active_remove(struct gswip_priv *priv, int idx)
{
	struct gswip_pce_table_entry vlan_active = {0,};
	int err;

	vlan_active.index = idx;
	vlan_active.table = GSWIP_TABLE_ACTIVE_VLAN;
	vlan_active.valid = false;
	err = gswip_pce_table_entry_write(priv, &vlan_active);
	if (err)
		dev_err(priv->dev, "failed to delete active VLAN: %d\n", err);
	priv->vlans[idx].bridge = NULL;

	return err;
}

static int gswip_vlan_add(struct gswip_priv *priv, struct net_device *bridge,
			  int port, u16 vid, bool untagged, bool pvid,
			  bool vlan_aware)
{
	struct gswip_pce_table_entry vlan_mapping = {0,};
	unsigned int max_ports = priv->hw_info->max_ports;
	unsigned int cpu_ports = dsa_cpu_ports(priv->ds);
	bool active_vlan_created = false;
	int fid = -1, idx = -1;
	int i, err;

	/* Check if there is already a page for this bridge */
	for (i = max_ports; i < ARRAY_SIZE(priv->vlans); i++) {
		if (priv->vlans[i].bridge == bridge) {
			if (vlan_aware) {
				if (fid != -1 && fid != priv->vlans[i].fid)
					dev_err(priv->dev, "one bridge with multiple flow ids\n");
				fid = priv->vlans[i].fid;
			}
			if (priv->vlans[i].vid == vid) {
				idx = i;
				break;
			}
		}
	}

	/* If this bridge is not programmed yet, add a Active VLAN table
	 * entry in a free slot and prepare the VLAN mapping table entry.
	 */
	if (idx == -1) {
		idx = gswip_vlan_active_create(priv, bridge, fid, vid);
		if (idx < 0)
			return idx;
		active_vlan_created = true;

		vlan_mapping.index = idx;
		vlan_mapping.table = GSWIP_TABLE_VLAN_MAPPING;
	} else {
		/* Read the existing VLAN mapping entry from the switch */
		vlan_mapping.index = idx;
		vlan_mapping.table = GSWIP_TABLE_VLAN_MAPPING;
		err = gswip_pce_table_entry_read(priv, &vlan_mapping);
		if (err) {
			dev_err(priv->dev, "failed to read VLAN mapping: %d\n",
				err);
			return err;
		}
	}

	/* VLAN ID byte, maps to the VLAN ID of vlan active table */
	vlan_mapping.val[0] = vid;
	/* Update the VLAN mapping entry and write it to the switch */
	vlan_mapping.val[1] |= cpu_ports;
	vlan_mapping.val[1] |= BIT(port);
	if (vlan_aware)
		vlan_mapping.val[2] |= cpu_ports;
	if (untagged)
		vlan_mapping.val[2] &= ~BIT(port);
	else
		vlan_mapping.val[2] |= BIT(port);
	err = gswip_pce_table_entry_write(priv, &vlan_mapping);
	if (err) {
		dev_err(priv->dev, "failed to write VLAN mapping: %d\n", err);
		/* In case an Active VLAN was creaetd delete it again */
		if (active_vlan_created)
			gswip_vlan_active_remove(priv, idx);
		return err;
	}

	gswip_port_commit_pvid(priv, port);

	return 0;
}

static int gswip_vlan_remove(struct gswip_priv *priv,
			     struct net_device *bridge, int port,
			     u16 vid)
{
	struct gswip_pce_table_entry vlan_mapping = {0,};
	unsigned int max_ports = priv->hw_info->max_ports;
	int idx = -1;
	int i;
	int err;

	/* Check if there is already a page for this bridge */
	for (i = max_ports; i < ARRAY_SIZE(priv->vlans); i++) {
		if (priv->vlans[i].bridge == bridge &&
		    priv->vlans[i].vid == vid) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		dev_err(priv->dev, "Port %d cannot find VID %u of bridge %s\n",
			port, vid, bridge ? bridge->name : "(null)");
		return -ENOENT;
	}

	vlan_mapping.index = idx;
	vlan_mapping.table = GSWIP_TABLE_VLAN_MAPPING;
	err = gswip_pce_table_entry_read(priv, &vlan_mapping);
	if (err) {
		dev_err(priv->dev, "failed to read VLAN mapping: %d\n",	err);
		return err;
	}

	vlan_mapping.val[1] &= ~BIT(port);
	vlan_mapping.val[2] &= ~BIT(port);
	err = gswip_pce_table_entry_write(priv, &vlan_mapping);
	if (err) {
		dev_err(priv->dev, "failed to write VLAN mapping: %d\n", err);
		return err;
	}

	/* In case all ports are removed from the bridge, remove the VLAN */
	if (!(vlan_mapping.val[1] & ~dsa_cpu_ports(priv->ds))) {
		err = gswip_vlan_active_remove(priv, idx);
		if (err) {
			dev_err(priv->dev, "failed to write active VLAN: %d\n",
				err);
			return err;
		}
	}

	gswip_port_commit_pvid(priv, port);

	return 0;
}

/* Defined below, and called from both sides of a bridge membership change to
 * drop the addresses the port learned in the domain it is leaving.
 */
static void gswip_port_fast_age(struct dsa_switch *ds, int port);

static int gswip_port_bridge_join(struct dsa_switch *ds, int port,
				  struct dsa_bridge bridge,
				  bool *tx_fwd_offload,
				  struct netlink_ext_ack *extack)
{
	struct net_device *br = bridge.dev;
	struct gswip_priv *priv = ds->priv;
	int err;

	/* Set up the VLAN for VLAN-unaware bridging for this port, and remove
	 * it from the "single-port bridge" through which it was operating as
	 * standalone.
	 */
	err = gswip_vlan_add(priv, br, port, GSWIP_VLAN_UNAWARE_PVID,
			     true, true, false);
	if (err)
		return err;

	err = gswip_add_single_port_br(priv, port, false);
	if (err)
		goto err_vlan;

	/* Move the port into the bridge's forwarding domain. Addresses it
	 * learned in the domain it just left name a domain it is no longer
	 * part of, so they are dropped and the port learns again.
	 */
	err = gswip_port_apply_fid(priv, port);
	if (err)
		goto err_single;

	gswip_port_fast_age(ds, port);

	return 0;

err_single:
	gswip_add_single_port_br(priv, port, true);
err_vlan:
	gswip_vlan_remove(priv, br, port, GSWIP_VLAN_UNAWARE_PVID);

	return err;
}

static void gswip_port_bridge_leave(struct dsa_switch *ds, int port,
				    struct dsa_bridge bridge)
{
	struct net_device *br = bridge.dev;
	struct gswip_priv *priv = ds->priv;
	int err;

	/* Add the port back to the "single-port bridge", and remove it from
	 * the VLAN-unaware PVID created for this bridge.
	 */
	gswip_add_single_port_br(priv, port, true);
	gswip_vlan_remove(priv, br, port, GSWIP_VLAN_UNAWARE_PVID);

	/* Back to a domain of the port's own, and rid of the addresses it
	 * learned in the bridge's. There is no way to refuse a leave, so a
	 * failure here leaves the port in the domain of a bridge it is no
	 * longer in and has to be said out loud.
	 */
	err = gswip_port_apply_fid(priv, port);
	if (err)
		dev_err(priv->dev,
			"port %d failed to leave its forwarding domain: %d\n",
			port, err);

	gswip_port_fast_age(ds, port);
}

static int gswip_port_vlan_prepare(struct dsa_switch *ds, int port,
				   const struct switchdev_obj_port_vlan *vlan,
				   struct netlink_ext_ack *extack)
{
	struct net_device *bridge = dsa_port_bridge_dev_get(dsa_to_port(ds, port));
	struct gswip_priv *priv = ds->priv;
	unsigned int max_ports = priv->hw_info->max_ports;
	int pos = max_ports;
	int i, idx = -1;

	/* We only support VLAN filtering on bridges */
	if (!dsa_is_cpu_port(ds, port) && !bridge)
		return -EOPNOTSUPP;

	/* Check if there is already a page for this VLAN */
	for (i = max_ports; i < ARRAY_SIZE(priv->vlans); i++) {
		if (priv->vlans[i].bridge == bridge &&
		    priv->vlans[i].vid == vlan->vid) {
			idx = i;
			break;
		}
	}

	/* If this VLAN is not programmed yet, we have to reserve
	 * one entry in the VLAN table. Make sure we start at the
	 * next position round.
	 */
	if (idx == -1) {
		/* Look for a free slot */
		for (; pos < ARRAY_SIZE(priv->vlans); pos++) {
			if (!priv->vlans[pos].bridge) {
				idx = pos;
				pos++;
				break;
			}
		}

		if (idx == -1) {
			NL_SET_ERR_MSG_MOD(extack, "No slot in VLAN table");
			return -ENOSPC;
		}
	}

	return 0;
}

static int gswip_port_vlan_add(struct dsa_switch *ds, int port,
			       const struct switchdev_obj_port_vlan *vlan,
			       struct netlink_ext_ack *extack)
{
	struct net_device *bridge = dsa_port_bridge_dev_get(dsa_to_port(ds, port));
	struct gswip_priv *priv = ds->priv;
	bool untagged = vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED;
	bool pvid = vlan->flags & BRIDGE_VLAN_INFO_PVID;
	int err;

	if (vlan->vid == GSWIP_VLAN_UNAWARE_PVID)
		return 0;

	err = gswip_port_vlan_prepare(ds, port, vlan, extack);
	if (err)
		return err;

	/* We have to receive all packets on the CPU port and should not
	 * do any VLAN filtering here. This is also called with bridge
	 * NULL and then we do not know for which bridge to configure
	 * this.
	 */
	if (dsa_is_cpu_port(ds, port))
		return 0;

	return gswip_vlan_add(priv, bridge, port, vlan->vid, untagged, pvid,
			      true);
}

static int gswip_port_vlan_del(struct dsa_switch *ds, int port,
			       const struct switchdev_obj_port_vlan *vlan)
{
	struct net_device *bridge = dsa_port_bridge_dev_get(dsa_to_port(ds, port));
	struct gswip_priv *priv = ds->priv;

	if (vlan->vid == GSWIP_VLAN_UNAWARE_PVID)
		return 0;

	/* We have to receive all packets on the CPU port and should not
	 * do any VLAN filtering here. This is also called with bridge
	 * NULL and then we do not know for which bridge to configure
	 * this.
	 */
	if (dsa_is_cpu_port(ds, port))
		return 0;

	return gswip_vlan_remove(priv, bridge, port, vlan->vid);
}

static void gswip_port_fast_age(struct dsa_switch *ds, int port)
{
	struct gswip_priv *priv = ds->priv;
	struct gswip_pce_table_entry mac_bridge = {0,};
	int i;
	int err;

	for (i = 0; i < 2048; i++) {
		mac_bridge.table = GSWIP_TABLE_MAC_BRIDGE;
		mac_bridge.index = i;

		err = gswip_pce_table_entry_read(priv, &mac_bridge);
		if (err) {
			dev_err(priv->dev, "failed to read mac bridge: %d\n",
				err);
			return;
		}

		if (!mac_bridge.valid)
			continue;

		if (mac_bridge.val[1] & GSWIP_TABLE_MAC_BRIDGE_VAL1_STATIC)
			continue;

		if (port != FIELD_GET(GSWIP_TABLE_MAC_BRIDGE_VAL0_PORT,
				      mac_bridge.val[0]))
			continue;

		mac_bridge.valid = false;
		err = gswip_pce_table_entry_write(priv, &mac_bridge);
		if (err) {
			dev_err(priv->dev, "failed to write mac bridge: %d\n",
				err);
			return;
		}
	}
}

static void gswip_port_stp_state_set(struct dsa_switch *ds, int port, u8 state)
{
	struct gswip_priv *priv = ds->priv;
	u32 stp_state;

	switch (state) {
	case BR_STATE_DISABLED:
		regmap_clear_bits(priv->gswip, GSWIP_SDMA_PCTRLp(port),
				  GSWIP_SDMA_PCTRL_EN);
		return;
	case BR_STATE_BLOCKING:
	case BR_STATE_LISTENING:
		stp_state = GSWIP_PCE_PCTRL_0_PSTATE_LISTEN;
		break;
	case BR_STATE_LEARNING:
		stp_state = GSWIP_PCE_PCTRL_0_PSTATE_LEARNING;
		break;
	case BR_STATE_FORWARDING:
		stp_state = GSWIP_PCE_PCTRL_0_PSTATE_FORWARDING;
		break;
	default:
		dev_err(priv->dev, "invalid STP state: %d\n", state);
		return;
	}

	regmap_set_bits(priv->gswip, GSWIP_SDMA_PCTRLp(port),
			GSWIP_SDMA_PCTRL_EN);
	regmap_write_bits(priv->gswip, GSWIP_PCE_PCTRL_0p(port),
			  GSWIP_PCE_PCTRL_0_PSTATE_MASK,
			  stp_state);
}

static int gswip_port_fdb(struct dsa_switch *ds, int port,
			  struct net_device *bridge, const unsigned char *addr,
			  u16 vid, bool add)
{
	struct gswip_priv *priv = ds->priv;
	struct gswip_pce_table_entry mac_bridge = {0,};
	unsigned int max_ports = priv->hw_info->max_ports;
	int fid = -1;
	int i;
	int err;

	/* Where the model confines the destination lookup to a forwarding
	 * domain, the entry has to be planted in the domain that lookup runs
	 * in - the port's own - rather than in the one the VLAN tables carry,
	 * or it is never found. The port is in @bridge here, so the two ways
	 * of naming the domain agree.
	 */
	if (priv->hw_info->port_set_fid) {
		fid = gswip_port_fid(priv, port);
	} else {
		for (i = max_ports; i < ARRAY_SIZE(priv->vlans); i++) {
			if (priv->vlans[i].bridge == bridge) {
				fid = priv->vlans[i].fid;
				break;
			}
		}
	}

	if (fid == -1) {
		dev_err(priv->dev, "no FID found for bridge %s\n",
			bridge->name);
		return -EINVAL;
	}

	mac_bridge.table = GSWIP_TABLE_MAC_BRIDGE;
	mac_bridge.key_mode = true;
	mac_bridge.key[0] = addr[5] | (addr[4] << 8);
	mac_bridge.key[1] = addr[3] | (addr[2] << 8);
	mac_bridge.key[2] = addr[1] | (addr[0] << 8);
	mac_bridge.key[3] = FIELD_PREP(GSWIP_TABLE_MAC_BRIDGE_KEY3_FID, fid);
	mac_bridge.val[0] = add ? BIT(port) : 0; /* port map */
	if (GSWIP_VERSION_GE(priv, GSWIP_VERSION_2_2_ETC))
		mac_bridge.val[1] = add ? (GSWIP_TABLE_MAC_BRIDGE_VAL1_STATIC |
					   GSWIP_TABLE_MAC_BRIDGE_VAL1_VALID) : 0;
	else
		mac_bridge.val[1] = GSWIP_TABLE_MAC_BRIDGE_VAL1_STATIC;

	mac_bridge.valid = add;

	err = gswip_pce_table_entry_write(priv, &mac_bridge);
	if (err)
		dev_err(priv->dev, "failed to write mac bridge: %d\n", err);

	return err;
}

static int gswip_port_fdb_add(struct dsa_switch *ds, int port,
			      const unsigned char *addr, u16 vid,
			      struct dsa_db db)
{
	if (db.type != DSA_DB_BRIDGE)
		return -EOPNOTSUPP;

	return gswip_port_fdb(ds, port, db.bridge.dev, addr, vid, true);
}

static int gswip_port_fdb_del(struct dsa_switch *ds, int port,
			      const unsigned char *addr, u16 vid,
			      struct dsa_db db)
{
	if (db.type != DSA_DB_BRIDGE)
		return -EOPNOTSUPP;

	return gswip_port_fdb(ds, port, db.bridge.dev, addr, vid, false);
}

static int gswip_port_fdb_dump(struct dsa_switch *ds, int port,
			       dsa_fdb_dump_cb_t *cb, void *data)
{
	struct gswip_priv *priv = ds->priv;
	struct gswip_pce_table_entry mac_bridge = {0,};
	unsigned char addr[ETH_ALEN];
	int i;
	int err;

	for (i = 0; i < 2048; i++) {
		mac_bridge.table = GSWIP_TABLE_MAC_BRIDGE;
		mac_bridge.index = i;

		err = gswip_pce_table_entry_read(priv, &mac_bridge);
		if (err) {
			dev_err(priv->dev,
				"failed to read mac bridge entry %d: %d\n",
				i, err);
			return err;
		}

		if (!mac_bridge.valid)
			continue;

		addr[5] = mac_bridge.key[0] & 0xff;
		addr[4] = (mac_bridge.key[0] >> 8) & 0xff;
		addr[3] = mac_bridge.key[1] & 0xff;
		addr[2] = (mac_bridge.key[1] >> 8) & 0xff;
		addr[1] = mac_bridge.key[2] & 0xff;
		addr[0] = (mac_bridge.key[2] >> 8) & 0xff;
		if (mac_bridge.val[1] & GSWIP_TABLE_MAC_BRIDGE_VAL1_STATIC) {
			if (mac_bridge.val[0] & BIT(port)) {
				err = cb(addr, 0, true, data);
				if (err)
					return err;
			}
		} else {
			if (port == FIELD_GET(GSWIP_TABLE_MAC_BRIDGE_VAL0_PORT,
					      mac_bridge.val[0])) {
				err = cb(addr, 0, false, data);
				if (err)
					return err;
			}
		}
	}
	return 0;
}

static int gswip_port_max_mtu(struct dsa_switch *ds, int port)
{
	/* Includes 8 bytes for special header. */
	return GSWIP_MAX_PACKET_LENGTH - VLAN_ETH_HLEN - ETH_FCS_LEN;
}

static int gswip_port_change_mtu(struct dsa_switch *ds, int port, int new_mtu)
{
	struct gswip_priv *priv = ds->priv;
	int reg;

	/* CPU port always has maximum mtu of user ports, so use it to set
	 * switch frame size, including 8 byte special header.
	 */
	if (dsa_is_cpu_port(ds, port)) {
		new_mtu += 8;
		regmap_write(priv->gswip, GSWIP_MAC_FLEN,
			     VLAN_ETH_HLEN + new_mtu + ETH_FCS_LEN);
	}

	reg = gswip_mac_ctrl_reg(priv, port, GSWIP_MAC_CTRL_2);
	if (reg < 0)
		return 0;

	/* Enable MLEN for ports with non-standard MTUs, including the special
	 * header on the CPU port added above.
	 */
	if (new_mtu != ETH_DATA_LEN)
		regmap_set_bits(priv->gswip, reg, GSWIP_MAC_CTRL_2_MLEN);
	else
		regmap_clear_bits(priv->gswip, reg, GSWIP_MAC_CTRL_2_MLEN);

	return 0;
}

static void gswip_phylink_get_caps(struct dsa_switch *ds, int port,
				   struct phylink_config *config)
{
	struct gswip_priv *priv = ds->priv;

	priv->hw_info->phylink_get_caps(ds, port, config);
}

static void gswip_port_set_link(struct gswip_priv *priv, int port, bool link)
{
	u32 mdio_phy;

	if (link)
		mdio_phy = GSWIP_MDIO_PHY_LINK_UP;
	else
		mdio_phy = GSWIP_MDIO_PHY_LINK_DOWN;

	gswip_mdio_mask_phy(priv, port, GSWIP_MDIO_PHY_LINK_MASK, mdio_phy);
}

static void gswip_port_set_speed(struct gswip_priv *priv, int port, int speed,
				 phy_interface_t interface)
{
	u32 mdio_phy = 0, mii_cfg = 0, mac_ctrl_0 = 0;
	int reg;

	switch (speed) {
	case SPEED_10:
		mdio_phy = GSWIP_MDIO_PHY_SPEED_M10;

		if (interface == PHY_INTERFACE_MODE_RMII)
			mii_cfg = GSWIP_MII_CFG_RATE_M50;
		else
			mii_cfg = GSWIP_MII_CFG_RATE_M2P5;

		mac_ctrl_0 = GSWIP_MAC_CTRL_0_GMII_MII;
		break;

	case SPEED_100:
		mdio_phy = GSWIP_MDIO_PHY_SPEED_M100;

		if (interface == PHY_INTERFACE_MODE_RMII)
			mii_cfg = GSWIP_MII_CFG_RATE_M50;
		else
			mii_cfg = GSWIP_MII_CFG_RATE_M25;

		mac_ctrl_0 = GSWIP_MAC_CTRL_0_GMII_MII;
		break;

	case SPEED_1000:
		mdio_phy = GSWIP_MDIO_PHY_SPEED_G1;

		mii_cfg = GSWIP_MII_CFG_RATE_M125;

		mac_ctrl_0 = GSWIP_MAC_CTRL_0_GMII_RGMII;
		break;
	}

	gswip_mdio_mask_phy(priv, port, GSWIP_MDIO_PHY_SPEED_MASK, mdio_phy);
	gswip_mii_mask_cfg(priv, GSWIP_MII_CFG_RATE_MASK, mii_cfg, port);

	reg = gswip_mac_ctrl_reg(priv, port, GSWIP_MAC_CTRL_0);
	if (reg >= 0)
		regmap_write_bits(priv->gswip, reg, GSWIP_MAC_CTRL_0_GMII_MASK,
				  mac_ctrl_0);
}

static void gswip_port_set_duplex(struct gswip_priv *priv, int port, int duplex)
{
	u32 mac_ctrl_0, mdio_phy;
	int reg;

	if (duplex == DUPLEX_FULL) {
		mac_ctrl_0 = GSWIP_MAC_CTRL_0_FDUP_EN;
		mdio_phy = GSWIP_MDIO_PHY_FDUP_EN;
	} else {
		mac_ctrl_0 = GSWIP_MAC_CTRL_0_FDUP_DIS;
		mdio_phy = GSWIP_MDIO_PHY_FDUP_DIS;
	}

	reg = gswip_mac_ctrl_reg(priv, port, GSWIP_MAC_CTRL_0);
	if (reg >= 0)
		regmap_write_bits(priv->gswip, reg, GSWIP_MAC_CTRL_0_FDUP_MASK,
				  mac_ctrl_0);
	gswip_mdio_mask_phy(priv, port, GSWIP_MDIO_PHY_FDUP_MASK, mdio_phy);
}

static void gswip_port_set_pause(struct gswip_priv *priv, int port,
				 bool tx_pause, bool rx_pause)
{
	u32 mac_ctrl_0, mdio_phy;
	int reg;

	if (tx_pause && rx_pause) {
		mac_ctrl_0 = GSWIP_MAC_CTRL_0_FCON_RXTX;
		mdio_phy = GSWIP_MDIO_PHY_FCONTX_EN |
			   GSWIP_MDIO_PHY_FCONRX_EN;
	} else if (tx_pause) {
		mac_ctrl_0 = GSWIP_MAC_CTRL_0_FCON_TX;
		mdio_phy = GSWIP_MDIO_PHY_FCONTX_EN |
			   GSWIP_MDIO_PHY_FCONRX_DIS;
	} else if (rx_pause) {
		mac_ctrl_0 = GSWIP_MAC_CTRL_0_FCON_RX;
		mdio_phy = GSWIP_MDIO_PHY_FCONTX_DIS |
			   GSWIP_MDIO_PHY_FCONRX_EN;
	} else {
		mac_ctrl_0 = GSWIP_MAC_CTRL_0_FCON_NONE;
		mdio_phy = GSWIP_MDIO_PHY_FCONTX_DIS |
			   GSWIP_MDIO_PHY_FCONRX_DIS;
	}

	reg = gswip_mac_ctrl_reg(priv, port, GSWIP_MAC_CTRL_0);
	if (reg >= 0)
		regmap_write_bits(priv->gswip, reg, GSWIP_MAC_CTRL_0_FCON_MASK,
				  mac_ctrl_0);
	gswip_mdio_mask_phy(priv, port,
			    GSWIP_MDIO_PHY_FCONTX_MASK |
			    GSWIP_MDIO_PHY_FCONRX_MASK, mdio_phy);
}

static void gswip_phylink_mac_config(struct phylink_config *config,
				     unsigned int mode,
				     const struct phylink_link_state *state)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct gswip_priv *priv = dp->ds->priv;
	int port = dp->index;
	u32 miicfg = 0;

	miicfg |= GSWIP_MII_CFG_LDCLKDIS;

	switch (state->interface) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
		return;
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_INTERNAL:
		miicfg |= GSWIP_MII_CFG_MODE_MIIM;
		break;
	case PHY_INTERFACE_MODE_REVMII:
		miicfg |= GSWIP_MII_CFG_MODE_MIIP;
		break;
	case PHY_INTERFACE_MODE_RMII:
		miicfg |= GSWIP_MII_CFG_MODE_RMIIM;
		if (of_property_read_bool(dp->dn, "maxlinear,rmii-refclk-out"))
			miicfg |= GSWIP_MII_CFG_RMII_CLK;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		miicfg |= GSWIP_MII_CFG_MODE_RGMII;
		break;
	case PHY_INTERFACE_MODE_GMII:
		miicfg |= GSWIP_MII_CFG_MODE_GMII;
		break;
	default:
		dev_err(dp->ds->dev,
			"Unsupported interface: %d\n", state->interface);
		return;
	}

	gswip_mii_mask_cfg(priv,
			   GSWIP_MII_CFG_MODE_MASK | GSWIP_MII_CFG_RMII_CLK |
			   GSWIP_MII_CFG_RGMII_IBS | GSWIP_MII_CFG_LDCLKDIS,
			   miicfg, port);

	gswip_mii_delay_setup(priv, dp, state->interface);
}

static void gswip_phylink_mac_link_down(struct phylink_config *config,
					unsigned int mode,
					phy_interface_t interface)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct gswip_priv *priv = dp->ds->priv;
	int port = dp->index;

	gswip_mii_mask_cfg(priv, GSWIP_MII_CFG_EN, 0, port);

	if (!dsa_port_is_cpu(dp))
		gswip_port_set_link(priv, port, false);
}

static void gswip_phylink_mac_link_up(struct phylink_config *config,
				      struct phy_device *phydev,
				      unsigned int mode,
				      phy_interface_t interface,
				      int speed, int duplex,
				      bool tx_pause, bool rx_pause)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct gswip_priv *priv = dp->ds->priv;
	int port = dp->index;

	if (!dsa_port_is_cpu(dp) || interface != PHY_INTERFACE_MODE_INTERNAL) {
		gswip_port_set_link(priv, port, true);
		gswip_port_set_speed(priv, port, speed, interface);
		gswip_port_set_duplex(priv, port, duplex);
		gswip_port_set_pause(priv, port, tx_pause, rx_pause);
	}

	gswip_mii_mask_cfg(priv, GSWIP_MII_CFG_EN, GSWIP_MII_CFG_EN, port);
}

static void gswip_get_strings(struct dsa_switch *ds, int port, u32 stringset,
			      uint8_t *data)
{
	int i;

	if (stringset != ETH_SS_STATS)
		return;

	for (i = 0; i < ARRAY_SIZE(gswip_rmon_cnt); i++)
		ethtool_puts(&data, gswip_rmon_cnt[i].name);
}

static u32 gswip_bcm_ram_entry_read(struct gswip_priv *priv, u32 table,
				    u32 index)
{
	u32 result, val;
	int err;

	regmap_write(priv->gswip, GSWIP_BM_RAM_ADDR, index);
	regmap_write_bits(priv->gswip, GSWIP_BM_RAM_CTRL,
			  GSWIP_BM_RAM_CTRL_ADDR_MASK | GSWIP_BM_RAM_CTRL_OPMOD |
			  GSWIP_BM_RAM_CTRL_BAS,
			  table | GSWIP_BM_RAM_CTRL_BAS);

	err = gswip_switch_r_timeout(priv, GSWIP_BM_RAM_CTRL,
				     GSWIP_BM_RAM_CTRL_BAS);
	if (err) {
		dev_err(priv->dev, "timeout while reading table: %u, index: %u\n",
			table, index);
		return 0;
	}

	regmap_read(priv->gswip, GSWIP_BM_RAM_VAL(0), &result);
	regmap_read(priv->gswip, GSWIP_BM_RAM_VAL(1), &val);
	result |= val << 16;

	return result;
}

static void gswip_get_ethtool_stats(struct dsa_switch *ds, int port,
				    uint64_t *data)
{
	struct gswip_priv *priv = ds->priv;
	const struct gswip_rmon_cnt_desc *rmon_cnt;
	s16 table = priv->rmon_table[port];
	int i;
	u64 high;

	if (table < 0)
		return;

	for (i = 0; i < ARRAY_SIZE(gswip_rmon_cnt); i++) {
		rmon_cnt = &gswip_rmon_cnt[i];

		data[i] = gswip_bcm_ram_entry_read(priv, table,
						   rmon_cnt->offset);
		if (rmon_cnt->size == 2) {
			high = gswip_bcm_ram_entry_read(priv, table,
							rmon_cnt->offset + 1);
			data[i] |= high << 32;
		}
	}
}

static int gswip_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	if (sset != ETH_SS_STATS)
		return 0;

	return ARRAY_SIZE(gswip_rmon_cnt);
}

static u64 gswip_rmon_read(struct gswip_priv *priv, s16 table,
			   enum gswip_rmon_counter counter)
{
	return gswip_bcm_ram_entry_read(priv, table, counter);
}

/*
 * A 64-bit count occupies two entries and each entry is read by a separate
 * transaction, so a count that carries between the two reads is reported
 * wrongly by four gigabytes either way. Read the low half first, which is the
 * order the driver-defined statistics use, so that the two ways of reading the
 * same counter cannot disagree about which way they err.
 */
static u64 gswip_rmon_read64(struct gswip_priv *priv, s16 table,
			     enum gswip_rmon_counter counter)
{
	u64 low = gswip_bcm_ram_entry_read(priv, table, counter);

	return low | (u64)gswip_bcm_ram_entry_read(priv, table, counter + 1) << 32;
}

/*
 * The standardised groups below leave a field the counter bank has no counter
 * for at the value the caller marked it unset with, so that it is reported as
 * absent rather than as zero. The bank has no broadcast counters of its own:
 * turning one on means relabelling a counter this driver already reports under
 * its standard name, so those fields stay absent.
 */
static void gswip_get_eth_mac_stats(struct dsa_switch *ds, int port,
				    struct ethtool_eth_mac_stats *mac_stats)
{
	struct gswip_priv *priv = ds->priv;
	s16 table = priv->rmon_table[port];

	if (table < 0)
		return;

#define RD(counter)	gswip_rmon_read(priv, table, GSWIP_RMON_##counter)
#define RD64(counter)	gswip_rmon_read64(priv, table, GSWIP_RMON_##counter)
	mac_stats->FramesTransmittedOK = RD(TX_GOOD_PKTS);
	mac_stats->SingleCollisionFrames = RD(TX_SINGLE_COLL);
	mac_stats->MultipleCollisionFrames = RD(TX_MULT_COLL);
	mac_stats->FramesReceivedOK = RD(RX_GOOD_PKTS);
	mac_stats->FrameCheckSequenceErrors = RD(RX_FCS_ERROR_PKTS);
	mac_stats->AlignmentErrors = RD(RX_ALIGN_ERROR_PKTS);
	mac_stats->OctetsTransmittedOK = RD64(TX_GOOD_BYTES);
	mac_stats->LateCollisions = RD(TX_LATE_COLL);
	mac_stats->FramesAbortedDueToXSColls = RD(TX_EXCESS_COLL);
	mac_stats->OctetsReceivedOK = RD64(RX_GOOD_BYTES);
	mac_stats->MulticastFramesXmittedOK = RD(TX_MULTICAST_PKTS);
	mac_stats->MulticastFramesReceivedOK = RD(RX_MULTICAST_PKTS);
	/*
	 * A frame past the maximum length is counted in one of two counters
	 * depending on whether its check sequence is also wrong, and this
	 * field covers both.
	 */
	mac_stats->FrameTooLongErrors = RD(RX_OVERSIZE_GOOD_PKTS) +
					RD(RX_OVERSIZE_ERROR_PKTS);
#undef RD64
#undef RD
}

static void gswip_get_eth_ctrl_stats(struct dsa_switch *ds, int port,
				     struct ethtool_eth_ctrl_stats *ctrl_stats)
{
	struct gswip_priv *priv = ds->priv;
	s16 table = priv->rmon_table[port];

	if (table < 0)
		return;

	ctrl_stats->MACControlFramesTransmitted =
		gswip_rmon_read(priv, table, GSWIP_RMON_TX_PAUSE);
	ctrl_stats->MACControlFramesReceived =
		gswip_rmon_read(priv, table, GSWIP_RMON_RX_GOOD_PAUSE_PKTS);
}

static void gswip_get_pause_stats(struct dsa_switch *ds, int port,
				  struct ethtool_pause_stats *pause_stats)
{
	struct gswip_priv *priv = ds->priv;
	s16 table = priv->rmon_table[port];

	if (table < 0)
		return;

	pause_stats->tx_pause_frames =
		gswip_rmon_read(priv, table, GSWIP_RMON_TX_PAUSE);
	pause_stats->rx_pause_frames =
		gswip_rmon_read(priv, table, GSWIP_RMON_RX_GOOD_PAUSE_PKTS);
}

/*
 * The top bucket ends where the longest frame the switch accepts does. The
 * counter behind it is documented as counting anything at or above 1024 bytes,
 * so a frame longer than the bucket says is counted here rather than nowhere.
 */
static const struct ethtool_rmon_hist_range gswip_rmon_ranges[] = {
	{    0,   64 },
	{   65,  127 },
	{  128,  255 },
	{  256,  511 },
	{  512, 1023 },
	{ 1024, VLAN_ETH_FRAME_LEN + ETH_FCS_LEN },
	{}
};

static void gswip_get_rmon_stats(struct dsa_switch *ds, int port,
				 struct ethtool_rmon_stats *rmon_stats,
				 const struct ethtool_rmon_hist_range **ranges)
{
	struct gswip_priv *priv = ds->priv;
	s16 table = priv->rmon_table[port];

	*ranges = gswip_rmon_ranges;

	if (table < 0)
		return;

#define RD(counter)	gswip_rmon_read(priv, table, GSWIP_RMON_##counter)
	/*
	 * A short or long frame is counted in one of two counters depending on
	 * whether its check sequence is also wrong, which is exactly the split
	 * this group draws between a runt and a fragment, and between an
	 * oversized frame and a jabber.
	 */
	rmon_stats->undersize_pkts = RD(RX_UNDERSIZE_GOOD_PKTS);
	rmon_stats->fragments = RD(RX_UNDERSIZE_ERROR_PKTS);
	rmon_stats->oversize_pkts = RD(RX_OVERSIZE_GOOD_PKTS);
	rmon_stats->jabbers = RD(RX_OVERSIZE_ERROR_PKTS);

	rmon_stats->hist[0] = RD(RX_64B_PKTS);
	rmon_stats->hist[1] = RD(RX_127B_PKTS);
	rmon_stats->hist[2] = RD(RX_255B_PKTS);
	rmon_stats->hist[3] = RD(RX_511B_PKTS);
	rmon_stats->hist[4] = RD(RX_1023B_PKTS);
	rmon_stats->hist[5] = RD(RX_MAXB_PKTS);

	rmon_stats->hist_tx[0] = RD(TX_64B_PKTS);
	rmon_stats->hist_tx[1] = RD(TX_127B_PKTS);
	rmon_stats->hist_tx[2] = RD(TX_255B_PKTS);
	rmon_stats->hist_tx[3] = RD(TX_511B_PKTS);
	rmon_stats->hist_tx[4] = RD(TX_1023B_PKTS);
	rmon_stats->hist_tx[5] = RD(TX_MAXB_PKTS);
#undef RD
}

static int gswip_set_mac_eee(struct dsa_switch *ds, int port,
			     struct ethtool_keee *e)
{
	if (e->tx_lpi_timer > 0x7f)
		return -EINVAL;

	return 0;
}

static void gswip_phylink_mac_disable_tx_lpi(struct phylink_config *config)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct gswip_priv *priv = dp->ds->priv;
	int reg;

	reg = gswip_mac_ctrl_reg(priv, dp->index, GSWIP_MAC_CTRL_4);
	if (reg < 0)
		return;

	regmap_clear_bits(priv->gswip, reg, GSWIP_MAC_CTRL_4_LPIEN);
}

static int gswip_phylink_mac_enable_tx_lpi(struct phylink_config *config,
					   u32 timer, bool tx_clock_stop)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct gswip_priv *priv = dp->ds->priv;
	int reg;

	reg = gswip_mac_ctrl_reg(priv, dp->index, GSWIP_MAC_CTRL_4);
	if (reg < 0)
		return -EOPNOTSUPP;

	return regmap_update_bits(priv->gswip, reg,
				  GSWIP_MAC_CTRL_4_LPIEN |
				  GSWIP_MAC_CTRL_4_GWAIT_MASK |
				  GSWIP_MAC_CTRL_4_WAIT_MASK,
				  GSWIP_MAC_CTRL_4_LPIEN |
				  GSWIP_MAC_CTRL_4_GWAIT(timer) |
				  GSWIP_MAC_CTRL_4_WAIT(timer));
}

static bool gswip_support_eee(struct dsa_switch *ds, int port)
{
	struct gswip_priv *priv = ds->priv;

	if (GSWIP_VERSION_GE(priv, GSWIP_VERSION_2_2))
		return true;

	return false;
}

static struct phylink_pcs *gswip_phylink_mac_select_pcs(struct phylink_config *config,
							phy_interface_t interface)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct gswip_priv *priv = dp->ds->priv;

	if (priv->hw_info->mac_select_pcs)
		return priv->hw_info->mac_select_pcs(config, interface);

	return NULL;
}

static const struct phylink_mac_ops gswip_phylink_mac_ops = {
	.mac_config		= gswip_phylink_mac_config,
	.mac_link_down		= gswip_phylink_mac_link_down,
	.mac_link_up		= gswip_phylink_mac_link_up,
	.mac_disable_tx_lpi	= gswip_phylink_mac_disable_tx_lpi,
	.mac_enable_tx_lpi	= gswip_phylink_mac_enable_tx_lpi,
	.mac_select_pcs		= gswip_phylink_mac_select_pcs,
};

static const struct dsa_switch_ops gswip_switch_ops = {
	.get_tag_protocol	= gswip_get_tag_protocol,
	.setup			= gswip_setup,
	.teardown		= gswip_teardown,
	.port_setup		= gswip_port_setup,
	.port_enable		= gswip_port_enable,
	.port_disable		= gswip_port_disable,
	.port_pre_bridge_flags	= gswip_port_pre_bridge_flags,
	.port_bridge_flags	= gswip_port_bridge_flags,
	.port_bridge_join	= gswip_port_bridge_join,
	.port_bridge_leave	= gswip_port_bridge_leave,
	.port_fast_age		= gswip_port_fast_age,
	.port_vlan_filtering	= gswip_port_vlan_filtering,
	.port_vlan_add		= gswip_port_vlan_add,
	.port_vlan_del		= gswip_port_vlan_del,
	.port_stp_state_set	= gswip_port_stp_state_set,
	.port_fdb_add		= gswip_port_fdb_add,
	.port_fdb_del		= gswip_port_fdb_del,
	.port_fdb_dump		= gswip_port_fdb_dump,
	.port_change_mtu	= gswip_port_change_mtu,
	.port_max_mtu		= gswip_port_max_mtu,
	.phylink_get_caps	= gswip_phylink_get_caps,
	.get_strings		= gswip_get_strings,
	.get_ethtool_stats	= gswip_get_ethtool_stats,
	.get_sset_count		= gswip_get_sset_count,
	.get_eth_mac_stats	= gswip_get_eth_mac_stats,
	.get_eth_ctrl_stats	= gswip_get_eth_ctrl_stats,
	.get_pause_stats	= gswip_get_pause_stats,
	.get_rmon_stats		= gswip_get_rmon_stats,
	.set_mac_eee		= gswip_set_mac_eee,
	.support_eee		= gswip_support_eee,
	.port_hsr_join		= dsa_port_simple_hsr_join,
	.port_hsr_leave		= dsa_port_simple_hsr_leave,
};

static int gswip_validate_cpu_port(struct dsa_switch *ds)
{
	struct gswip_priv *priv = ds->priv;
	struct dsa_port *cpu_dp;
	int cpu_port = -1;

	dsa_switch_for_each_cpu_port(cpu_dp, ds) {
		if (cpu_port != -1)
			return dev_err_probe(ds->dev, -EINVAL,
					     "only a single CPU port is supported\n");

		cpu_port = cpu_dp->index;
	}

	if (cpu_port == -1)
		return dev_err_probe(ds->dev, -EINVAL, "no CPU port defined\n");

	if (BIT(cpu_port) & ~priv->hw_info->allowed_cpu_ports)
		return dev_err_probe(ds->dev, -EINVAL,
				     "unsupported CPU port defined\n");

	return 0;
}

int gswip_probe_common(struct gswip_priv *priv, u32 version)
{
	int err;

	mutex_init(&priv->pce_table_lock);

	if (priv->hw_info->mdio_layout)
		priv->mdio_layout = priv->hw_info->mdio_layout;
	else
		priv->mdio_layout = &gswip_mdio_layout_2x;

	if (priv->hw_info->mac_ctrl)
		priv->mac_ctrl = priv->hw_info->mac_ctrl;
	else
		priv->mac_ctrl = gswip_mac_ctrl_2x;

	if (priv->hw_info->rmon_table)
		priv->rmon_table = priv->hw_info->rmon_table;
	else
		priv->rmon_table = gswip_rmon_table_2x;

	priv->ds = devm_kzalloc(priv->dev, sizeof(*priv->ds), GFP_KERNEL);
	if (!priv->ds)
		return -ENOMEM;

	priv->ds->dev = priv->dev;
	priv->ds->num_ports = priv->hw_info->max_ports;
	priv->ds->ops = &gswip_switch_ops;
	priv->ds->phylink_mac_ops = &gswip_phylink_mac_ops;
	priv->ds->priv = priv;

	/* One forwarding domain per bridge needs one number per bridge, which
	 * the core only hands out once it knows how many the switch can hold.
	 * The bound is the port count: every domain is anchored on a port, so
	 * there can be no more bridges than there are ports to put in them.
	 */
	if (priv->hw_info->port_set_fid)
		priv->ds->max_num_bridges = priv->hw_info->max_ports;

	/* The hardware has the 'major/minor' version bytes in the wrong order
	 * preventing numerical comparisons. Construct a 16-bit unsigned integer
	 * having the REV field as most significant byte and the MOD field as
	 * least significant byte. This is effectively swapping the two bytes of
	 * the version variable, but other than using swab16 it doesn't affect
	 * the source variable.
	 */
	priv->version = GSWIP_VERSION_REV(version) << 8 |
			GSWIP_VERSION_MOD(version);

	err = dsa_register_switch(priv->ds);
	if (err)
		return dev_err_probe(priv->dev, err, "dsa switch registration failed\n");

	err = gswip_validate_cpu_port(priv->ds);
	if (err)
		goto unregister_switch;

	dev_info(priv->dev, "probed GSWIP version %lx mod %lx\n",
		 GSWIP_VERSION_REV(version), GSWIP_VERSION_MOD(version));

	return 0;

unregister_switch:
	dsa_unregister_switch(priv->ds);

	return err;
}
EXPORT_SYMBOL_GPL(gswip_probe_common);

MODULE_AUTHOR("Hauke Mehrtens <hauke@hauke-m.de>");
MODULE_AUTHOR("Daniel Golle <daniel@makrotopia.org>");
MODULE_DESCRIPTION("Lantiq / Intel / MaxLinear GSWIP common functions");
MODULE_LICENSE("GPL");
