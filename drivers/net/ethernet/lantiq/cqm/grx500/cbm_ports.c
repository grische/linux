// SPDX-License-Identifier: GPL-2.0-only
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.c, cbm_config.c and
 * cbm_config.h.
 *
 * CBM port tables: the per-datapath-port dequeue, TMU queue and
 * descriptor-count rows transcribed from the vendor's xrx500_cbm_config[],
 * and the lookups over them.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/printk.h>
#include <linux/string.h>

#include "cbm.h"
#include "../../datapath/lantiq_cbm_api.h"
#include "../../tmu/drv_tmu_ll.h"

/*
 * Registry storage — AVM cbm.c:99-101. spin_lock_init runs once from
 * cbm_configure_dqm_cpu_ports() (AVM does it in probe at cbm.c:5505, right
 * before configure_ports). All list ops are under spin_lock_irqsave.
 */
static LIST_HEAD(pmac_mapping_list);
/*!< spin lock for cbm port mapping list*/
static spinlock_t cbm_port_mapping;

/* AVM cbm.c:61 — CBM_PORT_MAX = 64 (AVM cbm.h:210). */
static struct cbm_dqm_port_info dqm_port_info[CBM_PORT_MAX] = { { 0 } };

/*
 * AVM cbm.h:128-140 — EQM/DQM port-config discriminator. Full value set kept
 * verbatim for documentation even though only DQM_CPU_TYPE / NONE_TYPE are
 * handled by the minimal configure_ports below.
 */
enum EQM_DQM_PORT_TYPE {
	DQM_CPU_TYPE = 0,
	DQM_SCPU_TYPE = 5,
	DQM_DMA_TYPE = 6,
	DQM_LDMA_TYPE = 23,
	DQM_WAVE_TYPE = 24,
	EQM_CPU_TYPE = 100,
	EQM_DMA_TYPE = 200,
	EQM_TOE_TYPE = 9,
	EQM_DL_TYPE = 12,
	EQM_VRX318_TYPE = 15,
	NONE_TYPE = 0xffff
};

/* AVM cbm_config.h:14-19 verbatim. */
struct dqm_cpu_port {
	u32 tmu_port;
	u32 tmu_queue;
	u32 cpu_port_type;
	u32 num_desc;
};

/*
 * AVM cbm_config.h:50-64, union REDUCED to the one ported member — the
 * dqm_dma/dqm_ldma/dqm_scpu/eqm_* port structs arrive with their
 * conf_*_port functions (not in B1 scope).
 */
struct cqm_config {
	u32 type;
	union {
		struct dqm_cpu_port dqm_cpu;
	} data;
};

/*
 * The three DQM-CPU rows — AVM cbm_config.c:6-29 verbatim (values AND row
 * order: port2/q35/DP_F_DEQ_CPU, port1/q34/DP_F_DEQ_DL, port3/q36/
 * DP_F_DEQ_MPE, each num_desc=2; the commented-out port0/q33/DEQ_CPU1 row is
 * dropped with CONFIG_CBM_LS_ENABLE off). NONE_TYPE terminator per the AVM
 * configure_ports walk contract.
 */
static const struct cqm_config xrx500_cbm_config[] = {
	{ .type = DQM_CPU_TYPE,
	  .data.dqm_cpu.tmu_port = 2,
	  .data.dqm_cpu.tmu_queue = 35,
	  .data.dqm_cpu.cpu_port_type = DP_F_DEQ_CPU,
	  .data.dqm_cpu.num_desc = 2 },
	{
		.type = DQM_CPU_TYPE,
		.data.dqm_cpu.tmu_port = 1,
		.data.dqm_cpu.tmu_queue = 34,
		.data.dqm_cpu.cpu_port_type = DP_F_DEQ_DL,
		.data.dqm_cpu.num_desc = 2 },
	{ .type = DQM_CPU_TYPE,
	  .data.dqm_cpu.tmu_port = 3,
	  .data.dqm_cpu.tmu_queue = 36,
	  .data.dqm_cpu.cpu_port_type = DP_F_DEQ_MPE,
	  .data.dqm_cpu.num_desc = 2 },
	{ .type = NONE_TYPE },
};

/* AVM cbm.c:412-446 verbatim (adaptation (b) logging only). */
static struct cbm_pmac_port_map *cbm_add_to_list(
	struct cbm_pmac_port_map *val)
{
	struct cbm_pmac_port_map *ptr = NULL;
	int i = 0;
	unsigned long flags;

	pr_debug("cbm: \n new linked list\n");
	ptr = kmalloc(
		sizeof(struct cbm_pmac_port_map), GFP_ATOMIC);
	if (!ptr) {
		pr_err("cbm: \n Node creation failed\n");
		return NULL;
	}
	pr_debug("cbm: \n %s : 1\n", __func__);
	ptr->pmac = val->pmac;
	ptr->egp_port_map = val->egp_port_map;
	for (i = 0; i < val->qid_num; i++)
		ptr->qids[i] = val->qids[i];
	ptr->qid_num = val->qid_num;
	ptr->egp_type = val->egp_type;
	ptr->owner = 0;
	ptr->dev = 0;
	ptr->dev_port = 0;
	ptr->flags = P_ALLOCATED;
	pr_debug("cbm: \n %s : 2\n", __func__);
	spin_lock_irqsave(&cbm_port_mapping, flags);
	/* Init the list within the struct. */
	INIT_LIST_HEAD(&ptr->list);
	/* Add this struct to the tail of the list. */
	list_add_tail(&ptr->list, &pmac_mapping_list);
	spin_unlock_irqrestore(&cbm_port_mapping, flags);
	return ptr;
}

/* AVM cbm.c:448-488 verbatim. */
struct cbm_pmac_port_map *is_cbm_allocated(
	s32 cbm, u32 flags)
{
	struct cbm_pmac_port_map *ptr = NULL;
	unsigned long lock_flags;
	int num_deq_ports, i;
	u32 port_map, index;

	if ((flags != DP_F_MPE_ACCEL) &&
	    (flags != DP_F_DIRECTPATH_RX) &&
	    (flags != DP_F_CHECKSUM))
		flags = DP_F_DONTCARE;
	spin_lock_irqsave(&cbm_port_mapping, lock_flags);
	if (flags == DP_F_DONTCARE) {
		list_for_each_entry (ptr, &pmac_mapping_list, list) {
			num_deq_ports = hweight_long(ptr->egp_port_map);
			port_map = ptr->egp_port_map;
			for (i = 0; i < num_deq_ports; i++) {
				index = get_is_bit_set(port_map);
				if ((index == cbm) &&
				    (ptr->egp_type != DP_F_MPE_ACCEL) &&
				    (ptr->egp_type != DP_F_DIRECTPATH_RX)) {
					spin_unlock_irqrestore(&cbm_port_mapping,
							       lock_flags);
					return ptr;
				}
				port_map &= ~(1 << index);
			}
		}
	} else {
		list_for_each_entry (ptr, &pmac_mapping_list, list) {
			if (ptr->egp_type == flags) {
				spin_unlock_irqrestore(&cbm_port_mapping,
						       lock_flags);
				return ptr;
			}
		}
	}
	spin_unlock_irqrestore(&cbm_port_mapping, lock_flags);
	return NULL;
}

struct cbm_pmac_port_map *is_dp_allocated(
	s32 pmac, u32 flags)
{
	struct cbm_pmac_port_map *ptr = NULL;
	unsigned long lock_flags;

	if ((!(flags & DP_F_MPE_ACCEL)) &&
	    (!(flags & DP_F_DIRECTPATH_RX)) &&
	    (!(flags & DP_F_CHECKSUM)))
		flags = DP_F_DONTCARE;
	pr_debug("cbm: %s: flags 0x%x\r\n", __func__, flags);
	spin_lock_irqsave(&cbm_port_mapping, lock_flags);
	if (flags & DP_F_DONTCARE) {
		list_for_each_entry (ptr, &pmac_mapping_list, list) {
			pr_debug("cbm: 11:pmac %d type %d  input %d \
			input %d\r\n",
				 ptr->pmac, ptr->egp_type, pmac, flags);
			if ((ptr->pmac == pmac) &&
			    (!(ptr->egp_type & DP_F_MPE_ACCEL)) &&
			    (!(ptr->egp_type & DP_F_DIRECTPATH_RX)) &&
			    (IS_ENABLED(CONFIG_LTQ_DATAPATH_ACA_CSUM_WORKAROUND) || (!(ptr->egp_type & DP_F_CHECKSUM)))) {
				spin_unlock_irqrestore(&cbm_port_mapping,
						       lock_flags);
				return ptr;
			}
		}
	} else {
		list_for_each_entry (ptr, &pmac_mapping_list, list) {
			pr_debug("cbm: 22:pmac %d type %d \r\n", ptr->pmac,
				 ptr->egp_type);
			if (ptr->egp_type == flags) {
				spin_unlock_irqrestore(&cbm_port_mapping,
						       lock_flags);
				return ptr;
			}
		}
	}
	spin_unlock_irqrestore(&cbm_port_mapping, lock_flags);
	return NULL;
}

/*
 * AVM cbm.c:531-565 verbatim. __maybe_unused: static in AVM too; no caller
 * until the dp_port_dealloc registry teardown is ported.
 */
static int __maybe_unused cbm_delete_from_list(
	s32 pmac, u32 flags)
{
	struct cbm_pmac_port_map *ptr = NULL;
	struct cbm_pmac_port_map *next = NULL;
	int found = 0;
	unsigned long lock_flags;

	if ((flags != DP_F_MPE_ACCEL) &&
	    (flags != DP_F_DIRECTPATH_RX) &&
	    (flags != DP_F_CHECKSUM))
		flags = DP_F_DONTCARE;
	pr_debug("cbm: %s: flags 0x%x\r\n", __func__, flags);

	spin_lock_irqsave(&cbm_port_mapping, lock_flags);

	list_for_each_entry_safe (ptr, next, &pmac_mapping_list, list) {
		if (ptr->pmac == pmac) {
			if (flags == DP_F_DONTCARE) {
				found = 1;
				break;
			} else if (ptr->egp_type & flags) {
				found = 1;
				break;
			}
		}
	}
	if (found) {
		list_del(&ptr->list);
		kfree(ptr);
		spin_unlock_irqrestore(&cbm_port_mapping, lock_flags);
		return 1;
	}
	spin_unlock_irqrestore(&cbm_port_mapping, lock_flags);
	return 0;
}

/*
 * Static here (non-static in AVM's single cbm.c TU); this TU is the only
 * consumer.
 */
static struct cbm_egp_map epg_lookup_table[] = {
	{ 0, 0, 0 },
	{ 0, CBM_PMAC_DYNAMIC, DP_F_DIRECT },
	{ 1, 0, DP_F_MPE_ACCEL },
	{ 1, CBM_PMAC_DYNAMIC, DP_F_DIRECTLINK },
	{ CBM_PORT_NOT_APPL, CBM_PMAC_DYNAMIC, DP_F_FAST_WLAN | DP_F_FAST_DSL },
	{ 5, CBM_PMAC_NOT_APPL, DP_F_LRO },
	{ 6, 1, DP_F_FAST_ETH_LAN },
	{ 7, 2, DP_F_FAST_ETH_LAN },
	{ 8, 3, DP_F_FAST_ETH_LAN },
	{ 9, 4, DP_F_FAST_ETH_LAN },
	{ 10, 5, DP_F_FAST_ETH_LAN },
	{ 11, 6, DP_F_FAST_ETH_LAN },
	{ 12, 1, DP_F_FAST_ETH_LAN },
	{ 13, 2, DP_F_FAST_ETH_LAN },
	{ 14, 3, DP_F_FAST_ETH_LAN },
	{ 15, 4, DP_F_FAST_ETH_LAN },
	{ 16, 5, DP_F_FAST_ETH_LAN },
	{ 17, 6, DP_F_FAST_ETH_LAN },
	{ 18, CBM_PMAC_NOT_APPL, DP_F_FAST_DSL_DOWNSTREAM },
	{ 19, 15, DP_F_FAST_ETH_WAN },
	{ 20, CBM_PMAC_NOT_APPL, DP_F_CHECKSUM },
	{ 21, CBM_PMAC_NOT_APPL, DP_F_DIRECTPATH_RX },
	{ 23, CBM_PMAC_DYNAMIC, DP_F_FAST_DSL },
	{ CBM_PORT_NOT_APPL, CBM_PMAC_DYNAMIC, DP_F_FAST_WLAN },
	{ CBM_PORT_NOT_APPL, CBM_PMAC_DYNAMIC, DP_F_FAST_WLAN },
	{ CBM_PORT_NOT_APPL, CBM_PMAC_DYNAMIC, DP_F_FAST_WLAN },
	{ CBM_PORT_NOT_APPL, CBM_PMAC_DYNAMIC, DP_F_PORT_TUNNEL_DECAP },
};

/* AVM cbm.c:602-624 verbatim. */
static u32 get_matching_EP(
	u32 cbm_port,
	u32 flags,
	u32 *pmac)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(epg_lookup_table); i++) {
		pr_debug("cbm: %s: %d %d %d\r\n", __func__,
			 epg_lookup_table[i].epg,
			 epg_lookup_table[i].port_type,
			 epg_lookup_table[i].pmac);
		pr_debug("cbm: %s: %d %d \r\n", __func__, cbm_port, flags);
		if ((epg_lookup_table[i].epg == cbm_port) &&
		    (epg_lookup_table[i].port_type == flags)) {
			/*flags = epg_lookup_table[i].port_type;*/
			*pmac = epg_lookup_table[i].pmac;
			return 1;
		}
	}
	return 0;
}

/* AVM cbm.c:625-644 verbatim. */
static u32 assign_port_from_DT(
	u32 flags,
	u32 cbm_port)
{
	int i, result = CBM_NOTFOUND;

	for (i = 0; i < ARRAY_SIZE(epg_lookup_table); i++) {
		if (epg_lookup_table[i].port_type == flags) {
			if ((flags == DP_F_FAST_WLAN) &&
			    (epg_lookup_table[i].epg != CBM_PORT_NOT_APPL))
				continue;
			epg_lookup_table[i].epg = cbm_port;
			result = CBM_SUCCESS;
			break;
		}
	}
	return result;
}

/*
 * AVM cbm.c:645-660 verbatim. __maybe_unused: static in AVM too; its AVM
 * callers (cbm_dp_port_dealloc / cbm_cpu_pkt_tx PMAC resolution) are not in
 * B1 scope yet.
 */
static u32 __maybe_unused get_matching_flag(
	u32 *flags,
	u32 cbm_port)
{
	int i, result = CBM_NOTFOUND;

	for (i = 0; i < ARRAY_SIZE(epg_lookup_table); i++) {
		if (epg_lookup_table[i].epg == cbm_port) {
			*flags = epg_lookup_table[i].port_type;
			result = CBM_SUCCESS;
			break;
		}
	}
	return result;
}

/*
 * AVM cbm.c:698-725 verbatim. __maybe_unused: its only AVM caller is the
 * dp_port_resources_get static-pmac fallback, which B1 gates off (spec
 * risk 3) — kept ready for the verbatim restore once conf_dqm_dma_port
 * populates the dqm_port_info DMA rows.
 */
static u32 __maybe_unused get_matching_pmac_noflags(
	u32 *cbm_port,
	int pmac,
	u32 *flags,
	u32 *num_ports)
{
	int i, j = 0, result = CBM_NOTFOUND;

	for (i = 0; i < CBM_MAX_PHY_PORT_PER_EP;
	     i++) {
		cbm_port[i] = CBM_PORT_INVALID;
	}
	for (i = 0; i < ARRAY_SIZE(epg_lookup_table); i++) {
		if (j < CBM_MAX_PHY_PORT_PER_EP) {
			if (
				(epg_lookup_table[i].pmac == pmac) &&
				(epg_lookup_table[i].port_type != DP_F_MPE_ACCEL) &&
				(epg_lookup_table[i].port_type != DP_F_DIRECTPATH_RX)) {
				cbm_port[j] = epg_lookup_table[i].epg;
				*flags = epg_lookup_table[i].port_type;
				j++;
				*num_ports = j;
				result = CBM_SUCCESS;
			}
		}
	}
	return result;
}

/*
 * Non-static (static in AVM) so cbm_dp.c can wrap+EXPORT it; prototype in
 * cbm.h.
 */
s32 dp_port_resources_get(
	u32 *dp_port,
	u32 *num_tmu_ports,
	cbm_tmu_res_t **res_pp,
	u32 flags)
{
	int i = 0;
	u32 port_map;
	cbm_tmu_res_t *res;
	struct cbm_pmac_port_map *local_entry = NULL;

	pr_debug("cbm: %s: flags 0x%x dp %d\r\n", __func__, flags,
		 dp_port ? *dp_port : 0);
	if (dp_port) {
		pr_debug("cbm: %s: dp %d\r\n", __func__, *dp_port);
		local_entry = is_dp_allocated(*dp_port, flags);
	} else {
		local_entry = is_dp_allocated(0, flags);
	}
	if (local_entry) {
		*num_tmu_ports = hweight_long(local_entry->egp_port_map);
		if ((*num_tmu_ports > 16) || (*num_tmu_ports == 0))
			return -1;
		res = kmalloc(sizeof(cbm_tmu_res_t) * (*num_tmu_ports), GFP_ATOMIC);
		if (res) {
			*res_pp = res;
			port_map = local_entry->egp_port_map;
			pr_debug("cbm: port_map %d\r\n", port_map);
			for (i = 0; i < *num_tmu_ports; i++) {
				res[i].tmu_port = get_is_bit_set(port_map);
				res[i].cbm_deq_port = get_is_bit_set(port_map);
				pr_debug("cbm: %d tmu_port\r\n", res[i].tmu_port);
				/*clear the flag for the current bitpos*/
				port_map &= ~(1 << res[i].tmu_port);
				res[i].tmu_q = local_entry->qids[i];
				pr_debug("cbm: %d tmu_q\r\n", res[i].tmu_q);
				res[i].tmu_sched = res[i].tmu_q - SBID_START;
			}
		} else {
			pr_err("cbm: %s error in allocating memory", __func__);
		}
	} else if ((!local_entry) &&
		   ((flags & DP_F_MPE_ACCEL) ||
		    (flags & DP_F_DIRECTPATH_RX) ||
		    (flags & DP_F_CHECKSUM))) {
		return -1;
	} else if (dp_port && ((*dp_port <= 6) || (*dp_port == 15))) {
		pr_warn_ratelimited("cbm: resources_get static-LAN fallback not ported (dqm_port_info DMA rows empty)\n");
		return -1;
	} else {
		/*pr_err("cbm: %s: unallocated pmac port\r\n", __func__);*/
		return -1;
	}
	return 0;
}

/* AVM cbm.c:5031-5052 verbatim — SW high-watermark bookkeeping only (the
 * dynamic port allocator reads it back at AVM cbm.c:914); no register
 * writes.
 */
static void reserved_ports_highest(cbm_tmu_res_t *tmu_res, int flag_set)
{
	static cbm_tmu_res_t high_tmu_res = { 0 };

	if (flag_set) {
		if (high_tmu_res.tmu_port < tmu_res->tmu_port)
			high_tmu_res.tmu_port = tmu_res->tmu_port;
		if (high_tmu_res.cbm_deq_port < tmu_res->cbm_deq_port)
			high_tmu_res.cbm_deq_port = tmu_res->cbm_deq_port;

		if ((high_tmu_res.tmu_q < tmu_res->tmu_q) && (tmu_res->tmu_q != 255))
			high_tmu_res.tmu_q = tmu_res->tmu_q;
		if ((high_tmu_res.tmu_sched < tmu_res->tmu_sched) && (tmu_res->tmu_q != 255))
			high_tmu_res.tmu_sched = tmu_res->tmu_sched;
	} else {
		tmu_res->cbm_deq_port = high_tmu_res.cbm_deq_port;
		tmu_res->tmu_port = high_tmu_res.tmu_port;
		tmu_res->tmu_sched = high_tmu_res.tmu_sched;
		tmu_res->tmu_q = high_tmu_res.tmu_q;
	}
}

/* AVM cbm.c:5069-5077 verbatim. */
#define RESERVE_PORTS(tmp_res, tmu_port, tmu_queue)         \
	do {                                                \
		tmp_res.cbm_deq_port = tmu_port;            \
		tmp_res.tmu_port = tmu_port;                \
		tmp_res.tmu_sched = (tmu_queue)-SBID_START; \
		tmp_res.tmu_q = tmu_queue;                  \
		reserved_ports_highest(&tmp_res, 1);        \
	} while (0)

/* AVM cbm.c:5129-5229. */
static int conf_dqm_cpu_port(const struct dqm_cpu_port *cpu_ptr)
{
	u32 tmu_port, flags = 0;
	cbm_tmu_res_t tmp_res;
	struct cbm_pmac_port_map local_entry = { 0 };
	u32 ep, res;

	tmu_port = cpu_ptr->tmu_port;
	RESERVE_PORTS(tmp_res, tmu_port, cpu_ptr->tmu_queue);
	memset(&local_entry, 0, sizeof(local_entry));
	dqm_port_info[tmu_port].def_qid = cpu_ptr->tmu_queue;
	dqm_port_info[tmu_port].def_schd = cpu_ptr->tmu_queue - SBID_START;
	dqm_port_info[tmu_port].deq_info.num_desc = cpu_ptr->num_desc;
	dqm_port_info[tmu_port].deq_info.port_no = tmu_port;
	dqm_port_info[tmu_port].deq_info.dma_tx_chan = 255;
	dqm_port_info[tmu_port].num_free_entries = (tmu_port > 3) ? 32 : 1;
	/*config cbm/dma port*/
	init_cbm_dqm_cpu_port(tmu_port);
	/*config the tmu queue and scheduler*/
	tmu_create_flat_egress_path(1, tmu_port,
				    cpu_ptr->tmu_queue - SBID_START,
				    cpu_ptr->tmu_queue, 1);
	if (tmu_port >= 5) {
		flags = DP_F_FAST_WLAN;
		assign_port_from_DT(DP_F_FAST_WLAN, tmu_port);
		goto ASSIGN_FLAGS;
	} else if (tmu_port == 4) {
		flags = DP_F_FAST_WLAN | DP_F_FAST_DSL;
		assign_port_from_DT(DP_F_FAST_WLAN | DP_F_FAST_DSL, tmu_port);
		goto ASSIGN_FLAGS;
	}

	if (cpu_ptr->cpu_port_type == DP_F_DEQ_CPU) {
		flags = 0;
		assign_port_from_DT(0, tmu_port);
		assign_port_from_DT(DP_F_DIRECT, tmu_port);
	} else if (cpu_ptr->cpu_port_type == DP_F_DEQ_CPU1) {
		flags = 0;
	} else if (cpu_ptr->cpu_port_type == DP_F_DEQ_MPE) {
		flags = DP_F_MPE_ACCEL;
		assign_port_from_DT(DP_F_MPE_ACCEL, tmu_port);
	} else if (cpu_ptr->cpu_port_type == DP_F_DEQ_DL) {
		flags = DP_F_DIRECTLINK;
		assign_port_from_DT(DP_F_DIRECTLINK, tmu_port);
	}
/*if(tmu_port != 0) {*/
/*TMU/CBM port  with pmac 0 is fixed for cpu port*/
	if (cpu_ptr->cpu_port_type == DP_F_DEQ_CPU) {
		local_entry.pmac = 0;
		local_entry.egp_type = 0;
		cbm_program_cpu_qidt(cpu_ptr->tmu_queue);
	} else {
		res = get_matching_EP(tmu_port, flags, &ep);
		if (res) {
			local_entry.egp_type = flags;
			local_entry.pmac = ep;
			if (ep == CBM_PMAC_DYNAMIC) {
				pr_debug("cbm: ep is dyanmic %d\r\n", tmu_port);
				/*break;*/
				goto ASSIGN_FLAGS;
			}
		} else {
			pr_err("cbm: mapping missing for phys port %d\r\n", tmu_port);
			return CBM_FAILURE;
		}
	}
	local_entry.egp_port_map |= BIT(tmu_port);
	local_entry.qids[local_entry.qid_num] = dqm_port_info[tmu_port].def_qid;
	local_entry.qid_num++;
	local_entry.owner = 0;
	local_entry.dev = 0;
	local_entry.dev_port = 0;
	local_entry.flags = P_ALLOCATED;
	cbm_add_to_list(&local_entry);
ASSIGN_FLAGS:
	dqm_port_info[tmu_port].egp_type = flags;
	cpu_ptr = NULL;
	return CBM_SUCCESS;
}

/*
 * AVM cbm.c:5365-5405, MINIMAL: only DQM_CPU_TYPE + NONE_TYPE are ported.
 * Any other row type is a hard CBM_FAILURE with a pr_err — the matching
 * conf_*_port functions are not ported yet, and silently skipping a row
 * would desync the walk from the AVM config table.
 */
static int configure_ports(const struct cqm_config *port_config)
{
	int result;

	while (port_config->type != NONE_TYPE) {
		switch (port_config->type) {
		case DQM_CPU_TYPE:
			result = conf_dqm_cpu_port(&port_config->data.dqm_cpu);
			break;
		default:
			pr_err("cbm: configure_ports: unported port type %u\n",
			       port_config->type);
			result = CBM_FAILURE;
			break;
		}
		if (result)
			return CBM_FAILURE;
		port_config++;
	}
	return CBM_SUCCESS;
}

int cbm_configure_dqm_cpu_ports(void)
{
	static bool registry_lock_ready;

	if (!registry_lock_ready) {
		spin_lock_init(&cbm_port_mapping);
		registry_lock_ready = true;
	}
	return configure_ports(&xrx500_cbm_config[0]);
}
