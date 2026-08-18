// SPDX-License-Identifier: GPL-2.0-only
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.c.
 *
 * CBM datapath-port allocation: the queue-index table (QIDT) mapping and the
 * allocate/enable/disable/free surface the datapath API calls.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/errno.h>

#include "cbm.h"
#include "../../datapath/lantiq_cbm_api.h"
#include "../../dma/lantiq_dmax.h"

#include "../../tmu/drv_tmu_ll.h"

#ifndef CBM_PORT_F_DISABLE
#define CBM_PORT_F_DISABLE 0x8	/* verbatim */
#endif

/*
 * Programs one QIDT entry at the (clsid=0, ep=port_id,
 * mpe1=mpe2=enc=dec=flowidh=flowidl=0) index.
 *
 * QIDT index formula (AVM cbm.c:1458 verbatim, with the non-CPU-subif
 * bits zeroed out):
 *   qidt = ((port_id << 4) & 0xF0)
 *
 * Each QIDT word holds four 8-bit queue-id slots; the (qidt % 4) sub-byte
 * selects which slot is updated.
 *
 * @port_id: CBM port (caller-supplied; 0..15 fits the 0xF0 mask).
 *
 * dp_port_id -> { tmu_egress_port (EPN), tmu_queue (QID) } now comes from
 * cbm_dp_egress_res_get() (cbm_ports.c), which reads the ported vendor
 * tables. SBID_START=16 (AVM cbm.h:207) -> base_sbid = tmu_queue - 16.
 *
 *   dp  2 -> EPN  7, QID 17, SBID  1   (eth3 / LAN4)
 *   dp  3 -> EPN  8, QID 18, SBID  2   (eth2 / LAN3)
 *   dp  4 -> EPN  9, QID 19, SBID  3   (eth1 / LAN2)
 *   dp  5 -> EPN 10, QID 20, SBID  4   (eth0 / LAN1)
 *   dp 15 -> EPN 19, QID 28, SBID 12   (wan, GRX550 boards only)
 *
 * The LAN rows read like dp+5 / dp+15 and used to be computed that way. They
 * are not a formula: dp 15 resolves to 19 and 28, where the arithmetic would
 * have said 20 and 30 — and 20 is the checksum dequeue port, not an unused
 * number. See the block comment on cbm_dp_egress_res_get().
 */

static void cbm_qidt_set(int port_id, struct cbm_dp_en_data *cbm_data,
			 u8 qid_val)
{
	u32 qidt;
	u32 qidt_idx;
	u32 qidt_offset;
	u32 offset_factor;
	u32 value_mask;
	u32 value;

	(void)cbm_data;

	if (!g_cbm_qidt_base) {
		pr_err("cbm: cbm_qidt_set: g_cbm_qidt_base not mapped\n");
		return;
	}

	/*
	 * AVM cbm.c:1458 qidt formula (clsid=ep=mpe1=mpe2=enc=dec=
	 * flowidh=flowidl all zero except ep):
	 *   qidt = ((port_id << 4) & 0xF0)
	 */
	qidt = ((u32)port_id << 4) & 0xF0U;
	qidt_idx = qidt >> 2;
	qidt_offset = qidt % 4;
	offset_factor = qidt_offset << 3;
	value_mask = 255U << offset_factor;

	/*
	 * AVM cbm.c:1475-1476: read-modify-write the 32-bit QIDT word with
	 * the new 8-bit qid_val in the (qidt_offset)-th byte slot.
	 */
	value = __raw_readl(g_cbm_qidt_base + qidt_idx * 4);
	value = (value & ~value_mask) | (((u32)qid_val & 0xFFU) << offset_factor);
	__raw_writel(value, g_cbm_qidt_base + qidt_idx * 4);
}

/*
 * CBM_PORT_F_DISABLE ...)" call had no effect on the QIDT mapping — leaving
 * stale entries across ndo_stop / ndo_open cycles.
 *
 * Writes 0 into the SAME slot cbm_qidt_set programs, using the identical
 * QIDT-index formula `((port_id << 4) & 0xF0)` so the per-byte slot selection
 * matches bit-for-bit.
 *
 * Returns silently with a pr_err if the base is not mapped — matches
 * cbm_qidt_set's defensive contract.
 */
static void cbm_qidt_clear(int port_id)
{
	u32 qidt;
	u32 qidt_idx;
	u32 qidt_offset;
	u32 offset_factor;
	u32 value_mask;
	u32 value;
	const u8 qid_val = 0;	/* clear == write 0 to the slot */

	if (!g_cbm_qidt_base) {
		pr_err("cbm: cbm_qidt_clear: g_cbm_qidt_base not mapped\n");
		return;
	}

	/*
	 * Same QIDT-index formula as cbm_qidt_set (AVM cbm.c:1458) — index
	 * MUST match bit-for-bit or the inverse leaves a stale entry in a
	 * different slot.
	 */
	qidt = ((u32)port_id << 4) & 0xF0U;
	qidt_idx = qidt >> 2;
	qidt_offset = qidt % 4;
	offset_factor = qidt_offset << 3;
	value_mask = 255U << offset_factor;

	value = __raw_readl(g_cbm_qidt_base + qidt_idx * 4);
	value = (value & ~value_mask) | (((u32)qid_val & 0xFFU) << offset_factor);
	__raw_writel(value, g_cbm_qidt_base + qidt_idx * 4);
}

/*
 * AVM source: cqm/grx500/cbm.c:3629-3707 (dp_enable).
 *
 * @port_id: dp_port id to enable.
 *
 * @flags: caller-supplied.
 */
int cbm_dp_enable(struct module *owner, u32 port_id,
		  struct cbm_dp_en_data *cbm_data,
		  u32 flags, u32 alloc_flags)
{
	(void)owner;
	(void)alloc_flags;

	if (!cbm_data) {
		pr_err("cbm: cbm_dp_enable: cbm_data NULL port_id=%u\n",
		       port_id);
		return -EINVAL;
	}

	if (flags & CBM_PORT_F_DISABLE) {
		cbm_qidt_clear((int)port_id);
		return 0;
	}

	if (port_id >= 2 && port_id <= 5) {
		struct cbm_dp_egress_res res;
		u32 base_sbid;

		if (cbm_dp_egress_res_get(port_id, &res)) {
			pr_err("cbm: cbm_dp_enable: no egress resources for dp port %u\n",
			       port_id);
			return -EINVAL;
		}
		base_sbid = res.tmu_queue - SBID_START;

		if (!g_cbm_egress_preconfig[port_id]) {
			init_cbm_dqm_dma_port((int)res.deq_port);
			tmu_create_flat_egress_path(1, (u16)res.deq_port,
						    (u16)base_sbid,
						    (u16)res.tmu_queue, 1);
		}

		cbm_qidt_set((int)port_id, cbm_data, (u8)res.tmu_queue);
		return 0;
	}

	/* Program the QIDT entry for the CPU subif lookup. */
	cbm_qidt_set((int)port_id, cbm_data, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(cbm_dp_enable);

/*
 * AVM read-only reference for the deq_port_num=1 / minimum-port field set:
 * cqm/grx500/cbm.c:3162-3363 in AVM's 4.9 GPL release.
 *
 * @owner:    requesting kernel module (unused at minimum port; the AVM
 *            is_dp_allocated bookkeeping is not ported).
 *
 * @dev:      net_device for the port (unused at minimum port; only the
 *            datapath caller tracks it).
 *
 * @dev_port: caller device-port index (unused at minimum port).
 *
 * @port_id: CBM dp_port id.
 *
 * @data:     out — the cbm_dp_alloc_data block the caller reads back at
 *            datapath_api.c:480-507. Must be non-NULL.
 *
 * @flags:    caller-supplied DP_F_* mask (unused at minimum port).
 *
 * Returns 0 on success, -EINVAL if @data is NULL or @port_id is outside
 * {2,3,4,5}.
 */
int cbm_dp_port_alloc(struct module *owner, struct net_device *dev,
		      u32 dev_port, s32 port_id,
		      struct cbm_dp_alloc_data *data, u32 flags)
{
	struct cbm_dp_egress_res res;
	u32 dma_chan;

	(void)owner;
	(void)dev;
	(void)dev_port;
	(void)flags;

	if (!data) {
		pr_err("cbm: cbm_dp_port_alloc: data NULL port_id=%d\n",
		       port_id);
		return -EINVAL;
	}

	if (port_id < 2 || port_id > 5) {
		pr_err("cbm: cbm_dp_port_alloc: unsupported port_id=%d (minimum port serves 2..5 only)\n",
		       port_id);
		return -EINVAL;
	}

	if (cbm_dp_egress_res_get((u32)port_id, &res)) {
		pr_err("cbm: cbm_dp_port_alloc: no egress resources for port_id=%d\n",
		       port_id);
		return -EINVAL;
	}

	/*
	 *   dp  5 (eth0/LAN1) -> deq 10 -> DMA2TX ch 5  -> PMAC p5 -> GPHY5
	 *   dp  4 (eth1/LAN2) -> deq  9 -> DMA2TX ch 4  -> PMAC p4 -> GPHY4
	 *   dp  3 (eth2/LAN3) -> deq  8 -> DMA2TX ch 3  -> PMAC p3 -> GPHY3
	 *   dp  2 (eth3/LAN4) -> deq  7 -> DMA2TX ch 2  -> PMAC p2 -> GPHY2
	 *   dp 15 (wan)       -> deq 19 -> DMA1TX ch 15 -> PMAC-R p15
	 *
	 * WAN is on a DIFFERENT CONTROLLER, so the identity does not survive:
	 * the controller has to be resolved, not assumed. cbm_config.c
	 * numbers its controllers 1 = DMA1TX / 2 = DMA2TX, which is not the
	 * cid ordering of enum dma_controller, hence the map here.
	 */
	switch (res.dma_ctrl) {
	case 1:
		dma_chan = _DMA_C(DMA1TX, DMA1TX_PORT, res.dma_chan);
		break;
	case 2:
		dma_chan = _DMA_C(DMA2TX, DMA2TX_PORT, res.dma_chan);
		break;
	default:
		pr_err("cbm: cbm_dp_port_alloc: port_id=%d unknown dma_ctrl %u\n",
		       port_id, res.dma_ctrl);
		return -EINVAL;
	}

	data->dp_port = (u32)port_id;
	data->deq_port = res.deq_port;

	data->deq_port_num = 1;
	data->num_dma_chan = 1;
	data->dma_chan = dma_chan;

	data->flags |= CBM_PORT_DP_SET | CBM_PORT_DQ_SET | CBM_PORT_DMA_CHAN_SET;

	return 0;
}
EXPORT_SYMBOL_GPL(cbm_dp_port_alloc);

/*
 * @owner:    requesting kernel module (unused).
 *
 * @dev_port: caller device-port index (unused).
 *
 * @port_id:  CBM dp_port id (unused — nothing is freed per id).
 *
 * @data:     caller's cbm_dp_alloc_data block (unused — read-only at the
 *            caller's dealloc path, datapath_api.c:434-437).
 *
 * @flags:    caller-supplied DP_F_* mask (unused).
 *
 * Returns 0 (CBM_OK).
 */
int cbm_dp_port_dealloc(struct module *owner, u32 dev_port, s32 port_id,
			struct cbm_dp_alloc_data *data, u32 flags)
{
	(void)owner;
	(void)dev_port;
	(void)port_id;
	(void)data;
	(void)flags;

	return 0;
}
EXPORT_SYMBOL_GPL(cbm_dp_port_dealloc);

/*
 * cbm_dp_port_resources_get - resolve a dp_port's TMU/CBM dequeue resources
 * from the pmac-port registry (cbm_ports.c).
 *
 * B1 semantics (spec S1): for the CPU endpoint (*dp_port == 0, or dp_port ==
 * NULL) the registry entry installed by the conf_dqm_cpu_port walk resolves
 * {tmu_port=2, cbm_deq_port=2, tmu_q=35, tmu_sched=19}. The AVM static-pmac
 * LAN/WAN fallback (dp_port <= 6 || == 15) is GATED OFF inside the
 * implementation (returns -1 with a rate-limited warn) because the
 * dqm_port_info DQM-DMA rows are not populated yet — spec B1 risk 3. On
 * success the caller must kfree(*res_pp).
 */
s32 cbm_dp_port_resources_get(u32 *dp_port, u32 *num_tmu_ports,
			      cbm_tmu_res_t **res_pp, u32 flags)
{
	return dp_port_resources_get(dp_port, num_tmu_ports, res_pp, flags);
}
EXPORT_SYMBOL_GPL(cbm_dp_port_resources_get);
