/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * Per-port and per-device state for the xRX500 network driver.
 */

#ifndef _LANTIQ_INTEL_XRX500_H_
#define _LANTIQ_INTEL_XRX500_H_

#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/netdevice.h>
#include <linux/phylink.h>
#include <linux/spinlock.h>
#include <linux/types.h>

/*
 * Forward declarations.
 *
 * The .c TU that actually dereferences port->tx_ring (intel_xrx500.c)
 * includes dma/hdma.h directly.
 */
struct dma_tx_desc;

/*
 *   eth0 = physical LAN1 (silkscreen) = dp_port_id 5
 *   eth1 = physical LAN2              = dp_port_id 4
 *   eth2 = physical LAN3              = dp_port_id 3
 *   eth3 = physical LAN4              = dp_port_id 2
 *
 * Note: the orientation is INVERSE the "natural" silicon counting order (eth0
 * -> dp_port_id 2 .. eth3 -> dp_port_id 5 would be natural).
 *
 * Boards with a WAN port add one more netdev on dp_port_id 15 (GSWIP-R,
 * PMAC-R). The PMAC<->CPU link is internal and unbacked, so it never gets a
 * netdev of its own on any board.
 *
 * How many ports a board actually has is a device-tree fact, read from the
 * child count of the ethernet node - see priv->num_ports. The constant below
 * is only the array bound: four LAN plus one WAN is every port this silicon
 * exposes to a netdev.
 */
#define INTEL_XRX500_MAX_PORTS  5

/*
 * The WAN port's datapath id. Not a choice: DP_F_FAST_ETH_WAN drives the
 * vendor allocator over a single-entry range whose start and end are both
 * PMAC_ETH_WAN_ID, so the WAN class can only ever land on dp port 15. It sits
 * on GSWIP-R / PMAC-R, which is why it is nowhere near the LAN block.
 */
#define INTEL_XRX500_DP_PORT_WAN  15

/**
 * struct intel_xrx500_port_stats - per-port drop counters.
 * @rx_dropped: receive-path drops (allocation failure, queue full, ...).
 * @tx_dropped: transmit-path drops.
 *
 * The packet and byte counters are NOT here. They live in the netdev core's
 * per-CPU &struct pcpu_sw_netstats, requested by setting
 * dev->pcpu_stat_type = %NETDEV_PCPU_STAT_TSTATS before register_netdev();
 * the writers use dev_sw_netstats_rx_add() / dev_sw_netstats_tx_add() and
 * ndo_get_stats64 sums them with dev_fetch_sw_netstats(). That is what puts
 * exactly one writer on each seqcount -- the CPU that owns it -- which a
 * single per-port u64_stats_sync could not do once the receive bottom half
 * and ndo_start_xmit ran on different CPUs.
 *
 * The two drop counters stay atomic_long_t: they are bumped from drop
 * fastpaths with no obligation to advance in step with a packet/byte pair,
 * and an atomic needs no writer-side seqcount at all, so it is immune to the
 * same hazard. ndo_get_stats64 reads them outside the aggregation.
 *
 * Zero-initialised by alloc_etherdev_mq(), which backs the per-port struct
 * via netdev_priv() -- ATOMIC_LONG_INIT(0) is a zero bit pattern.
 */
struct intel_xrx500_port_stats {
	atomic_long_t		rx_dropped;
	atomic_long_t		tx_dropped;
};

/**
 * struct intel_xrx500_port - per-LAN-port state object.
 *
 * @netdev:           the per-port net_device allocated via
 *                    alloc_etherdev_mq at probe time.
 *
 * @phylink:          phylink instance returned by phylink_create.
 *                    NULL between alloc_etherdev_mq and phylink_create.
 *
 * @port_idx:         per-port index 0..4 - matches the DT child node
 *                    reg = <N> property. Used as the array index back
 *                    into priv->ports[] from phylink callbacks.
 *
 * Identical to the GSWIP silicon port-number for the four LAN ports -
 * directly indexes into MAC_CTRL_0[port] et seq.
 *
 * @cbm_deq_port: the CBM dequeue-port number for this port: eth0=10, eth1=9,
 * eth2=8, eth3=7, WAN=19. Resolved from the ported vendor tables by
 * cbm_dp_deq_port_get(), not computed.
 *
 * @tmu_egress_port: the TMU egress-port number for this port. Used when
 * wiring the TMU port-enable sequence.
 *
 * @phy_node:         the DT node carrying the phy-handle (= one of
 *                    &gphy_lan1..&gphy_lan4); used by
 *                    phylink_of_phy_connect at probe time. Held by an
 *                    of_node_get reference released in
 *                    intel_xrx500_remove.
 *
 * @port_node:        the per-port DT node (port@N) whose fwnode is
 *                    handed to phylink_create. Held by an of_node_get
 *                    reference released in intel_xrx500_remove.
 *
 * @parent:           back-pointer to the owning
 *                    struct intel_xrx500_priv (for dev_err routing).
 *
 * Set at probe from the port's DT class: DP_F_FAST_ETH_LAN, or
 * DP_F_FAST_ETH_WAN where the node carries lantiq,wan. The same value (OR'd
 * with DP_F_DEREGISTER) is passed on the ndo_stop teardown path so the
 * alloc/dealloc pair use bit-for-bit identical base flags.
 *
 * @dp_subif_id: the @subif handle returned by dp_register_subif on success
 * (the VAP/CTP identifier inside the dp_port). Undefined when
 * @state_subif_registered is false.
 *
 * The struct sits at netdev_priv(netdev) for that port's netdev, so any ndo_*
 * callback can recover it via netdev_priv(dev).
 */
struct intel_xrx500_port {
	struct net_device		*netdev;
	struct phylink			*phylink;
	struct phylink_config		phylink_config;
	u32				port_idx;
	u32				dp_port_id;
	u32				cbm_deq_port;
	u32				tmu_egress_port;
	struct device_node		*phy_node;
	struct device_node		*port_node;
	struct intel_xrx500_priv	*parent;
	struct intel_xrx500_port_stats	stats;
	u32				dp_alloc_flags;
	s32				dp_subif_id;
	bool				state_open;
	bool				state_subif_registered;
	spinlock_t			tx_lock;
	u32				tx_chan;
	struct dma_tx_desc		*tx_ring;
	dma_addr_t			tx_ring_phys;
	u32				tx_head;
	bool				state_tx_initialized;
};

/**
 * struct intel_xrx500_priv - per-platform_device top-level state.
 *
 * @pdev:    the bound platform_device.
 *
 * @dev:     convenience back-pointer for dev_err / dev_info.
 *
 * @ports:   per-port state pointers, indexed by the DT "reg" of the port
 *           child node; NULL slots are unallocated.
 *
 * @num_ports: how many port child nodes this board's ethernet node has, read
 *           from DT at probe. Four on a LAN-only board, five where a WAN port
 *           is described. Probe requires exactly this many netdevs to come
 *           up - a partial bring-up is a DT/driver disagreement, not a
 *           degraded mode.
 *
 * @gswl_dev: cached ethsw_api_dev_t pointer for GSW-L (devid=0), recovered
 * via gsw_get_swcore_ops(0) + container_of at probe time. Carries gswl_base /
 * gsw_base for direct gsw_w32 register writes from mac_link_up /
 * mac_link_down. NULL until probe completes - probe returns -EPROBE_DEFER if
 * gsw_get_swcore_ops(0) returns NULL at probe time.
 *
 * Allocated once via devm_kzalloc at probe time, freed automatically
 * on probe failure or driver unbind.
 */
struct intel_xrx500_priv {
	struct platform_device		*pdev;
	struct device			*dev;
	struct intel_xrx500_port	*ports[INTEL_XRX500_MAX_PORTS];
	u32				num_ports;
	void				*gswl_dev;
};

void intel_xrx500_rx_account(struct net_device *dev, unsigned int len);
void intel_xrx500_rx_drop_account(struct net_device *dev);

#endif /* _LANTIQ_INTEL_XRX500_H_ */
