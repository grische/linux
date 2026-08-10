/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * CBM datapath-port allocation interface exported to the datapath and netdev
 * drivers.
 */

#ifndef _NET_CQM_CBM_API_H
#define _NET_CQM_CBM_API_H

#include <linux/types.h>
#include <linux/module.h>

struct cbm_dp_en_data;
struct net_device;
struct cbm_dp_alloc_data;

/*
 * cbm_dp_enable — port-level enable/disable hook called from
 * dp_register_subif_private when the first subif on a dp_port comes
 * online. AVM signature (cqm/grx500/cbm.c:3629-3635) preserved verbatim:
 * 5 arguments, owner / port_id / data / flags / alloc_flags.
 */
int cbm_dp_enable(struct module *owner, u32 port_id,
		  struct cbm_dp_en_data *cbm_data,
		  u32 flags, u32 alloc_flags);

/*
 * cbm_dp_port_alloc / cbm_dp_port_dealloc — CBM dp_port allocate/free hooks
 * called from dp_alloc_port_private (datapath_api.c) when a LAN port is
 * registered / deregistered.
 */
int cbm_dp_port_alloc(struct module *owner, struct net_device *dev,
		      u32 dev_port, s32 port_id,
		      struct cbm_dp_alloc_data *data, u32 flags);
int cbm_dp_port_dealloc(struct module *owner, u32 dev_port, s32 port_id,
			struct cbm_dp_alloc_data *data, u32 flags);

/*
 * cbm_counter_mode_set — toggle CBM enqueue/dequeue counter between
 * packet-count and byte-count modes. AVM signature
 * (cqm/grx500/cbm.c:4843): two ints (idx, mode).
 */
int cbm_counter_mode_set(int idx, int mode);

/*
 * get_lookup_qid_via_index — CBM QIDT lookup. Returns the 8-bit queue id
 * stored at the given lookup-index entry.
 */
u8 get_lookup_qid_via_index(u32 lookup_idx);

void *cbm_buf_alloc(u32 size, u32 *pool_phys, u32 flags);
int   cbm_buf_free(void *buf, u32 size);

int dma_port_enable(u32 idx, int dqm_flag);
int init_cbm_eqm_dma_port(int idx, u32 flags); /* flags = std/jumbo buffer type */
int setup_eqm_dma_desc(int pid, int desc_count, u32 flags, u32 buf_offset);

/*
 * cbm_dequeue - return a buffer to CBM EQM CPU port @pid via the AVM
 * cbm.c:2936-2962 wait-cycle pattern.
 */
int cbm_dequeue(int pid, u32 buf_ptr, u32 flags);

/*
 * cbm_cpu_enqueue_hw - fire-and-forget CPU TX enqueue to the CBM EQM CPU
 * ingress port @pid.
 *
 * @pid:          EQM CPU ingress port id (0..3).
 *
 * @dw0:          descriptor DWORD0 (dest_sub_if_id etc).
 *
 * @dw1:          descriptor DWORD1 (ep / color etc).
 *
 * @data_phys:    physical/bus address of the buffer (incl pmac hdr).
 *
 * @dw3:          descriptor DWORD3 (data_len | sop | eop, own=0) —
 *                written LAST, commits the enqueue.
 */
int cbm_cpu_enqueue_hw(int pid, u32 dw0, u32 dw1, u32 data_phys, u32 dw3);

#endif /* _NET_CQM_CBM_API_H */
