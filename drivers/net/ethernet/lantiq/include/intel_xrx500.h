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
#include <linux/u64_stats_sync.h>

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
 */
#define INTEL_XRX500_NUM_PORTS  4

/**
 * struct intel_xrx500_port_stats - per-port traffic counters.
 *
 * @rx_packets: frames received and delivered to the stack.
 *
 * @rx_bytes:   bytes received (post-CRC strip).
 *
 * @tx_packets: frames successfully posted to the egress DMA.
 *
 * @tx_bytes:   bytes posted to egress.
 *
 * @rx_dropped: receive-path drops (allocation failure, queue full, ...).
 *
 * @tx_dropped: transmit-path drops.
 *
 * @syncp:      seqcount-based torn-read protection for the u64_stats_t
 *              fields above.
 *
 * The two drop counters use atomic_long_t because they (a) are incremented
 * from drop fastpaths that have no obligation to advance under the same
 * seqcount sequence as the packet/byte pair, and (b) atomic_long_t avoids
 * forcing the caller to acquire u64_stats_update_begin in the drop-only path.
 * On 64-bit architectures (BITS_PER_LONG == 64) the u64_stats_sync discipline
 * collapses to a no-op; on 32-bit architectures u64_stats_init must be called
 * before any update.
 */
struct intel_xrx500_port_stats {
	u64_stats_t		rx_packets;
	u64_stats_t		rx_bytes;
	u64_stats_t		tx_packets;
	u64_stats_t		tx_bytes;
	atomic_long_t		rx_dropped;
	atomic_long_t		tx_dropped;
	struct u64_stats_sync	syncp;
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
 * @port_idx:         per-port index 0..3 - matches the DT child node
 *                    reg = <N> property. Used as the array index back
 *                    into priv->ports[] from phylink callbacks.
 *
 * Identical to the GSWIP silicon port-number for these four LAN ports -
 * directly indexes into MAC_CTRL_0[port] et seq.
 *
 * Used when wiring the TMU port-enable sequence.
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
 * Initialized to DP_F_FAST_ETH_LAN at probe time (the four FRITZ!Box 7560 LAN ports
 * are all fast-Ethernet LAN class). The same value (OR'd with
 * DP_F_DEREGISTER) is passed on the ndo_stop teardown path so the
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
 * @ports: per-port state pointers; NULL slots are unallocated.
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
	struct intel_xrx500_port	*ports[INTEL_XRX500_NUM_PORTS];
	void				*gswl_dev;
};

#endif /* _LANTIQ_INTEL_XRX500_H_ */
