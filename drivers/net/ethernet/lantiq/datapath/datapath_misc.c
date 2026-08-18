// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * datapath/gswip30/datapath_misc.c.
 *
 * GSWIP-3.0 datapath platform hooks: PMAC templates, DMA descriptor masks and
 * the per-port and per-sub-interface hardware setup.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linux/build_bug.h>
#include <linux/atomic.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/device.h>
#include <linux/printk.h>

#include <net/cqm_cbm_api.h>
#include "datapath.h"
#include "datapath_misc.h"
#include "../switch-api/gsw_flow_core.h"
#include "../switch-api/gsw_tbl_rw.h"

/*
 * Minimal local types and macros not available in datapath.h.
 *
 * Only `deq_port_idx` is dereferenced from datapath_misc.c (subif_hw_set,
 * subif_hw_reset).
 *
 * Define them locally with the same bit layout as AVM (controller<<24 |
 * port<<16 | channel) so subif_hw_set's dma_chan decoding matches AVM
 * byte-for-byte.
 *
 * (3) DP_FAILURE: AVM macro returning -1 on subif_hw_set / subif_hw_reset
 *     failure paths. Local define keeps the AVM source readable.
 */
struct dp_subif_data {
	s8 deq_port_idx;
};

#define _DMA_CONTROLLER(nr) (((nr) >> 24) & 0xFF)
#define _DMA_PORT(nr)       (((nr) >> 16) & 0xFF)
#define _DMA_CHANNEL(nr)    ((nr) & 0xFFFF)

#ifndef DP_FAILURE
#define DP_FAILURE (-1)
#endif

/*
 * Local fallbacks for the DP_F_* flag bits the AVM source-of-truth defines in
 * include/net/datapath_api.h:120-130 as `enum { DP_F_DEREGISTER = BIT(0),
 * DP_F_FAST_ETH_LAN = BIT(1), ... DP_F_LOOPBACK = BIT(6), DP_F_DIRECTLINK =
 * BIT(7) }`.
 *
 *   DP_F_DEREGISTER   = BIT(0) = 0x01 — subif_platform_set dispatch flag.
 *   DP_F_FAST_ETH_LAN = BIT(1) = 0x02 — init_dma_pmac_template LAN branch.
 *   DP_F_FAST_ETH_WAN = BIT(2) = 0x04 — init_dma_pmac_template WAN branch.
 *   DP_F_LOOPBACK     = BIT(6) = 0x40 — update_port_vap tunnel-loop branch.
 *
 * #ifndef guards let any later subtask import the AVM enum first and have
 * the local defines compile out cleanly per C11 6.10.1 / 6.10.3 semantics.
 */
#ifndef DP_F_DEREGISTER
#define DP_F_DEREGISTER 0x00000001
#endif
#ifndef DP_F_FAST_ETH_LAN
#define DP_F_FAST_ETH_LAN 0x00000002
#endif
#ifndef DP_F_FAST_ETH_WAN
#define DP_F_FAST_ETH_WAN 0x00000004
#endif
#ifndef DP_F_LOOPBACK
#define DP_F_LOOPBACK 0x00000040
#endif

/*
 * init_dma_desc_mask — AVM:32-58 verbatim. Populates the four DMA-descriptor
 * mask globals via the union/bitfield `.field.X` accessors.
 */
static void init_dma_desc_mask(void)
{
	BUILD_BUG_ON(sizeof(struct dma_rx_desc_1) != 4);
	BUILD_BUG_ON(sizeof(struct dma_rx_desc_3) != 4);
	BUILD_BUG_ON(sizeof(struct dma_rx_desc_0) != 4);

	/* mask 0: to remove the bit, 1 -- keep the bit */
	dma_rx_desc_mask1.all = 0xFFFFFFFF;
	dma_rx_desc_mask3.all = 0xFFFFFFFF;
	dma_rx_desc_mask3.field.own = 0;
	dma_rx_desc_mask3.field.c = 0;
	dma_rx_desc_mask3.field.sop = 0;
	dma_rx_desc_mask3.field.eop = 0;
	dma_rx_desc_mask3.field.dic = 0;
	dma_rx_desc_mask3.field.byte_offset = 0;
	dma_rx_desc_mask1.field.dec = 0;
	dma_rx_desc_mask1.field.enc = 0;
	dma_rx_desc_mask1.field.mpe2 = 0;
	dma_rx_desc_mask1.field.mpe1 = 0;
	/* mask to keep some value via 1
	 * set by top application all others set to 0
	 */
	dma_tx_desc_mask0.all = 0;
	dma_tx_desc_mask1.all = 0;
	dma_tx_desc_mask0.field.flow_id = 0xFF;
	dma_tx_desc_mask0.field.dest_sub_if_id = 0x7FFF;
	dma_tx_desc_mask1.field.mpe1 = 0x1;
	dma_tx_desc_mask1.field.color = 0x3;
	dma_tx_desc_mask1.field.ep = 0xF;
}

/* init_dma_pmac_template - AVM:60-223 with the branch surgery applied. */
static void init_dma_pmac_template(int portid, u32 flags)
{
	int i;
	struct pmac_port_info2 *dp_info = &dp_port_info2[0][portid];

	/* Note:
	 * final tx_dma0 = (tx_dma0 & dma0_mask_template) | dma0_template
	 * final tx_dma1 = (tx_dma1 & dma1_mask_template) | dma1_template
	 * final tx_pmac = pmac_template
	 */
	memset(dp_info->pmac_template, 0, sizeof(dp_info->pmac_template));
	memset(dp_info->dma0_template, 0, sizeof(dp_info->dma0_template));
	memset(dp_info->dma1_template, 0, sizeof(dp_info->dma1_template));
	for (i = 0; i < MAX_TEMPLATE; i++) {
		dp_info->dma0_mask_template[i].all = 0xFFFFFFFF;
		dp_info->dma1_mask_template[i].all = 0xFFFFFFFF;
	}

	if (flags & DP_F_FAST_ETH_LAN) { /* always with pmac */
		/* always with pmac */
		for (i = 0; i < MAX_TEMPLATE; i++) {
			dp_info->pmac_template[i].port_map_en = 1;
			dp_info->pmac_template[i].sppid = PMAC_CPU_ID;
			dp_info->pmac_template[i].redirect = 0;
			dp_info->pmac_template[i].class_en = 1;
			SET_PMAC_PORTMAP(&dp_info->pmac_template[i], portid);
		}
		/* for checksum for pmac_template[1] */
		dp_info->pmac_template[TEMPL_CHECKSUM].tcp_chksum = 1;
	} else if (flags & DP_F_FAST_ETH_WAN) { /* always with pmac */
		/*
		 * AVM:91-102. Identical to the LAN branch above but for
		 * redirect = 1 — that single bit is the whole downstream
		 * difference between the two Ethernet classes, and it is what
		 * makes the WAN port's counters land in the GSW-R redirect RMON
		 * block (datapath_mib.c reads WAN stats from there).
		 */
		for (i = 0; i < MAX_TEMPLATE; i++) {
			dp_info->pmac_template[i].port_map_en = 1;
			dp_info->pmac_template[i].sppid = PMAC_CPU_ID;
			dp_info->pmac_template[i].redirect = 1;
			dp_info->pmac_template[i].class_en = 1;
			SET_PMAC_PORTMAP(&dp_info->pmac_template[i], portid);
		}
		/* for checksum for pmac_template[1] */
		dp_info->pmac_template[TEMPL_CHECKSUM].tcp_chksum = 1;
	} else { /* DP_F_DIRECT fallback - AVM lines 197-222 */
		/* normal dirctpath without checksum support */
		dp_info->pmac_template[TEMPL_NORMAL].port_map_en = 0;
		dp_info->pmac_template[TEMPL_NORMAL].sppid = portid;
		dp_info->pmac_template[TEMPL_NORMAL].redirect = 0;
		dp_info->pmac_template[TEMPL_NORMAL].port_map = 0xff;
		dp_info->pmac_template[TEMPL_NORMAL].port_map2 = 0xff;
		dp_info->pmac_template[TEMPL_NORMAL].class_en = 1;
		dp_info->dma1_template[TEMPL_NORMAL].field.enc = 1;
		dp_info->dma1_template[TEMPL_NORMAL].field.dec = 1;
		dp_info->dma1_template[TEMPL_NORMAL].field.mpe2 = 0;
		dp_info->dma1_mask_template[TEMPL_NORMAL].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_NORMAL].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_NORMAL].field.mpe2 = 0;

		/* dirctpath with checksum support */
		dp_info->pmac_template[TEMPL_CHECKSUM].port_map_en = 1;
		dp_info->pmac_template[TEMPL_CHECKSUM].sppid = PMAC_CPU_ID;
		dp_info->pmac_template[TEMPL_CHECKSUM].redirect = 1;
		dp_info->pmac_template[TEMPL_CHECKSUM].tcp_chksum = 1;
		dp_info->pmac_template[TEMPL_CHECKSUM].class_en = 1;
		SET_PMAC_PORTMAP(&dp_info->pmac_template[TEMPL_CHECKSUM],
				 portid);
		dp_info->dma1_template[TEMPL_CHECKSUM].field.enc = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.dec = 1;
		dp_info->dma1_template[TEMPL_CHECKSUM].field.mpe2 = 1;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.enc = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.dec = 0;
		dp_info->dma1_mask_template[TEMPL_CHECKSUM].field.mpe2 = 0;
	}
}

static void dump_rx_dma_desc(struct dma_rx_desc_0 *desc_0,
			     struct dma_rx_desc_1 *desc_1,
			     struct dma_rx_desc_2 *desc_2,
			     struct dma_rx_desc_3 *desc_3)
{
	if (!desc_0 || !desc_1 || !desc_2 || !desc_3) {
		dev_err(dp_dev, "rx desc_0/1/2/3 NULL\n");
		return;
	}
	dev_info(dp_dev,
		 " DMA Descripotr:D0=0x%08x D1=0x%08x D2=0x%08x D3=0x%08x\n",
		 *(u32 *)desc_0, *(u32 *)desc_1,
		 *(u32 *)desc_2, *(u32 *)desc_3);
	dev_info(dp_dev,
		 "  DW0:%s=%d tunl_id=%d flow_id=%d eth_type=%d subif=0x%04x\n",
		 "resv0", desc_0->field.resv0,
		 desc_0->field.tunnel_id,
		 desc_0->field.flow_id, desc_0->field.eth_type,
		 desc_0->field.dest_sub_if_id);
	dev_info(dp_dev,
		 "  DW1:sess=%d tcp_err=%d nat=%d dec=%d enc=%d mpe2/1=%d/%d\n",
		 desc_1->field.session_id, desc_1->field.tcp_err,
		 desc_1->field.nat, desc_1->field.dec, desc_1->field.enc,
		 desc_1->field.mpe2, desc_1->field.mpe1);
	dev_info(dp_dev,
		 "      color=%02d ep=%02d resv1=%d classid=%02d\n",
		 desc_1->field.color, desc_1->field.ep, desc_1->field.resv1,
		 desc_1->field.classid);
	dev_info(dp_dev, "  DW2:data_ptr=0x%08x\n", desc_2->field.data_ptr);
	dev_info(dp_dev,
		 "  DW3:own=%d c=%d sop=%d eop=%d dic=%d pdu_type=%d\n",
		 desc_3->field.own, desc_3->field.c, desc_3->field.sop,
		 desc_3->field.eop, desc_3->field.dic, desc_3->field.pdu_type);
	dev_info(dp_dev,
		 "      offset=%d atm_q=%d mpoa_pt=%d mpoa_mode=%d len=%d\n",
		 desc_3->field.byte_offset, desc_3->field.qid,
		 desc_3->field.mpoa_pt, desc_3->field.mpoa_mode,
		 desc_3->field.data_len);
}

/* dump_tx_dma_desc — AVM:262-301 ported verbatim. */
static void dump_tx_dma_desc(struct dma_tx_desc_0 *desc_0,
			     struct dma_tx_desc_1 *desc_1,
			     struct dma_tx_desc_2 *desc_2,
			     struct dma_tx_desc_3 *desc_3)
{
	int lookup;

	if (!desc_0 || !desc_1 || !desc_2 || !desc_3) {
		dev_err(dp_dev, "tx desc_0/1/2/3 NULL\n");
		return;
	}
	dev_info(dp_dev,
		 " DMA Descripotr:D0=0x%08x D1=0x%08x D2=0x%08x D3=0x%08x\n",
		 *(u32 *)desc_0, *(u32 *)desc_1,
		 *(u32 *)desc_2, *(u32 *)desc_3);
	dev_info(dp_dev,
		 "  DW0:%s=%d tunl_id=%d flow_id=%d eth_type=%d subif=0x%04x\n",
		 "resv0", desc_0->field.resv0,
		 desc_0->field.tunnel_id,
		 desc_0->field.flow_id, desc_0->field.eth_type,
		 desc_0->field.dest_sub_if_id);
	dev_info(dp_dev,
		 "  DW1:sess=%d tcp_err=%d nat=%d dec=%d enc=%d mpe2/1=%d/%d\n",
		 desc_1->field.session_id, desc_1->field.tcp_err,
		 desc_1->field.nat, desc_1->field.dec, desc_1->field.enc,
		 desc_1->field.mpe2, desc_1->field.mpe1);
	dev_info(dp_dev,
		 "  color=%02d ep=%02d resv1=%d classid=%02d\n",
		 desc_1->field.color, desc_1->field.ep, desc_1->field.resv1,
		 desc_1->field.classid);
	dev_info(dp_dev, "  DW2:data_ptr=0x%08x\n", desc_2->field.data_ptr);
	dev_info(dp_dev,
		 "  DW3:own=%d c=%d sop=%d eop=%d dic=%d pdu_type=%d\n",
		 desc_3->field.own, desc_3->field.c, desc_3->field.sop,
		 desc_3->field.eop, desc_3->field.dic, desc_3->field.pdu_type);
	dev_info(dp_dev,
		 "  offset=%d atm_qid=%d mpoa_pt=%d mpoa_mode=%d len=%d\n",
		 desc_3->field.byte_offset, desc_3->field.qid,
		 desc_3->field.mpoa_pt, desc_3->field.mpoa_mode,
		 desc_3->field.data_len);
	lookup =
		((desc_0->field.flow_id >> 6) << 12) |
		((desc_1->field.dec) << 11) |
		((desc_1->field.enc) << 10) |
		((desc_1->field.mpe2) << 9) |
		((desc_1->field.mpe1) << 8) |
		((desc_1->field.ep) << 4) |
		((desc_1->field.classid) << 0);
	dev_info(dp_dev, "  lookup index=0x%x qid=%d\n", lookup,
		 get_lookup_qid_via_index(lookup));
}

/*
 * dump_rx_pmac — AVM:303-330 ported verbatim with PR_* -> dev_*.
 */
static void dump_rx_pmac(struct pmac_rx_hdr *pmac)
{
	int i, l;
	unsigned char *p = (char *)pmac;
	unsigned char buf[100];

	if (!pmac) {
		dev_err(dp_dev, " pmac NULL ??\n");
		return;
	}

	l = sprintf(buf, "PMAC at 0x%p: ", p);
	for (i = 0; i < 8; i++)
		l += sprintf(buf + l, "0x%02x ", p[i]);
	l += sprintf(buf + l, "\n");
	dev_info(dp_dev, "%s", buf);

	/* byte 0 */
	dev_info(dp_dev, "  byte 0:res=%d ver_done=%d ip_offset=%d\n",
		 pmac->res1, pmac->ver_done, pmac->ip_offset);
	/* byte 1 */
	dev_info(dp_dev, "  byte 1:tcp_h_offset=%d tcp_type=%d\n",
		 pmac->tcp_h_offset, pmac->tcp_type);
	/* byte 2 */
	dev_info(dp_dev, "  byte 2:ppid=%d class=%d\n",
		 pmac->sppid, pmac->class);
	/* byte 3 */
	dev_info(dp_dev, "  byte 3:res=%d pkt_type=%d\n",
		 pmac->res2, pmac->pkt_type);
	/* byte 4 */
	dev_info(dp_dev,
		 "  byte 4:res=%d redirect=%d res2=%d src_sub_inf_id=%d\n",
		 pmac->res3, pmac->redirect, pmac->res4,
		 pmac->src_sub_inf_id);
	/* byte 5 */
	dev_info(dp_dev, "  byte 5:src_sub_inf_id2=%d\n",
		 pmac->src_sub_inf_id2);
	/* byte 6 */
	dev_info(dp_dev, "  byte 6:port_map=%d\n", pmac->port_map);
	/* byte 7 */
	dev_info(dp_dev, "  byte 7:port_map2=%d\n", pmac->port_map2);
}

/*
 * dump_tx_pmac — AVM:332-359 ported verbatim with PR_* -> dev_*.
 */
static void dump_tx_pmac(struct pmac_tx_hdr *pmac)
{
	int i, l;
	unsigned char *p = (char *)pmac;
	unsigned char buf[100];

	if (!pmac) {
		dev_err(dp_dev, "dump_tx_pmac pmac NULL ??\n");
		return;
	}

	l = sprintf(buf, "PMAC at 0x%p: ", p);
	for (i = 0; i < 8; i++)
		l += sprintf(buf + l, "0x%02x ", p[i]);
	sprintf(buf + l, "\n");
	dev_info(dp_dev, "%s", buf);
	/* byte 0 */
	dev_info(dp_dev, "  byte 0:tcp_chksum=%d res=%d ip_offset=%d\n",
		 pmac->tcp_chksum, pmac->res1, pmac->ip_offset);
	/* byte 1 */
	dev_info(dp_dev, "  byte 1:tcp_h_offset=%d tcp_type=%d\n",
		 pmac->tcp_h_offset, pmac->tcp_type);
	/* byte 2 */
	dev_info(dp_dev, "  byte 2:ppid=%d res=%d\n",
		 pmac->sppid, pmac->res);
	/* byte 3 */
	dev_info(dp_dev,
		 "  byte 3:%s=%d %s=%d/%d time_dis=%d class_en=%d pkt_type=%d\n",
		 "map_en", pmac->port_map_en,
		 "res", pmac->res2, pmac->res3,
		 pmac->time_dis, pmac->class_en,
		 pmac->pkt_type);
	/* byte 4 */
	dev_info(dp_dev,
		 "  byte 4:fcs_ins_dis=%d redirect=%d time_stmp=%d subif=%d\n",
		 pmac->fcs_ins_dis, pmac->redirect, pmac->time_stmp,
		 pmac->src_sub_inf_id);
	/* byte 5 */
	dev_info(dp_dev, "  byte 5:src_sub_inf_id2=%d\n",
		 pmac->src_sub_inf_id2);
	/* byte 6 */
	dev_info(dp_dev, "  byte 6:port_map=%d\n", pmac->port_map);
	/* byte 7 */
	dev_info(dp_dev, "  byte 7:port_map2=%d\n", pmac->port_map2);
}

/*
 * mib_init — AVM:361-371 ported as a no-op shell per the surgery. The
 * cbm_counter_mode_set calls resolve to the stub in datapath_cbm_stubs.h.
 */
static void mib_init(u32 flag)
{
	dev_dbg(dp_dev,
		"mib reset skipped: no MIB module (flag=0x%x)\n",
		flag);
	cbm_counter_mode_set(0, 1); /* enqueue to byte */
	cbm_counter_mode_set(1, 1); /* dequeue to byte */
}

void dp_sys_mib_reset_30(u32 flag)
{
	dev_dbg(dp_dev,
		"dp_sys_mib_reset_30 skipped: no MIB module (flag=0x%x)\n",
		flag);
}

/*
 * dp_get_gsw_parser_30 call dropped (task constraint #5). dp_coc_cpufreq_init
 * / CONFIG_LTQ_DATAPATH_CPUFREQ block excised. mib_init call retained (it is
 * a no-op shell after the surgery). init_dma_desc_mask retained on inst==0.
 * The dispatch-table NULL check is retained.
 */
static int dp_platform_set(int inst, u32 flag)
{
	if (!inst) /* only inst zero needs DMA descriptor masks */
		init_dma_desc_mask();

	if (!dp_port_prop[inst].ops[0] ||
	    !dp_port_prop[inst].ops[1]) {
		dev_err(dp_dev, "Why gswip handle zero?\n");
		return -1;
	}
	if (!inst)
		mib_init(0);

	dev_info(dp_dev,
		 "dp-gswip30: dp_platform_set inst=%d, DMA desc masks initialised, MIB-init skipped (no CONFIG_LTQ_DATAPATH_MIB)\n",
		 inst);
	return 0;
}

/*
 * dev_platform_set — AVM:407-411 verbatim. Returns 0 (no per-dev HW
 * setup).
 */
static int dev_platform_set(int inst, u8 ep, struct dp_dev_data *data,
			    u32 flags)
{
	return 0;
}

static int gswip30_program_queue_map(int inst, int portid);

/*
 * DP_DEBUG calls preserved (rewritten to dev_dbg via the macro redefinition
 * in datapath.h).
 */
static int port_platform_set(int inst, u8 ep, struct dp_port_data *data,
			     u32 flags)
{
	int idx, i;
	struct pmac_port_info *port_info = &dp_port_info[inst][ep];
	u32 dma_chan = 0;

	dp_port_info[inst][ep].ctp_max = MAX_SUBIF_PER_PORT;
	dp_port_info[inst][ep].vap_offset = 8;
	dp_port_info[inst][ep].vap_mask = 0xF;
	idx = port_info->deq_port_base;
	for (i = 0; i < port_info->deq_port_num; i++) {
		dp_deq_port_tbl[inst][i + idx].dp_port = ep;

		/* For G.INT num_dma_chan 8 or 16, for other 1 */
		if (port_info->num_dma_chan > 1)
			dp_deq_port_tbl[inst][i + idx].dma_chan = dma_chan++;
		else
			dp_deq_port_tbl[inst][i + idx].dma_chan = dma_chan;
		DP_DEBUG(DP_DBG_FLAG_DBG, "deq_port_tbl[%d][%d].dma_chan=%x\n",
			 inst, (i + idx), dma_chan);
	}

	/*
	 * The vendor datapath performs no PCE QUEUE_MAP writes on the LAN
	 * path; table 0x11 is left to switch init. The removed call zeroed
	 * {ep << 4 | tc} -> queue for every registered port at each ifup,
	 * clobbering whatever switch init established.
	 */
	return 0;
}

/*
 * supported_logic_dev — AVM:386-390 verbatim. Returns is_vlan_dev result.
 */
static int supported_logic_dev(int inst, struct net_device *dev,
			       char *subif_name)
{
	return is_vlan_dev(dev);
}

/*
 * Bounds checks against DP_DMAMAX / DP_MAX_DMA_PORT / DP_MAX_DMA_CHAN are
 * preserved.
 */
static noinline int subif_hw_set(int inst, int portid, int subif_ix,
				 struct subif_platform_data *data, u32 flags)
{
	int deq_port_idx = 0, cqe_deq;
	struct pmac_port_info *port_info;
	int cid, pid, nid;
	u32 dma_chan;

	if (!data || !data->subif_data) {
		dev_err(dp_dev, "data NULL or subif_data NULL\n");
		return -1;
	}
	port_info = &dp_port_info[inst][portid];
	if (data->subif_data)
		deq_port_idx = data->subif_data->deq_port_idx;
	if (port_info->deq_port_num < deq_port_idx + 1) {
		dev_err(dp_dev, "Wrong deq_port_idx(%d), should < %d\n",
			deq_port_idx, port_info->deq_port_num);
		return -1;
	}
	cqe_deq = port_info->deq_port_base + deq_port_idx;
	port_info->subif_info[subif_ix].cqm_deq_port = cqe_deq;

	dma_chan = dp_deq_port_tbl[inst][cqe_deq].dma_chan;
	cid = _DMA_CONTROLLER(dma_chan);
	pid = _DMA_PORT(dma_chan);
	nid = _DMA_CHANNEL(dma_chan);
	/* cid, pid and nid should not greater than DP_DMAMAX,
	 * DP_MAX_DMA_PORT and DP_MAX_DMA_CHAN respectively.
	 */
	if ((cid >= DP_DMAMAX) || (pid >= DP_MAX_DMA_PORT) ||
	    nid >= DP_MAX_DMA_CHAN) {
		dev_err(dp_dev, "ERROR: cid=%d pid=%d nid=%d\n",
			cid, pid, nid);
		dev_err(dp_dev,
			"DMAMAX=%d MAX_DMA_PORT=%d MAX_DMA_CHAN=%d\n",
			DP_DMAMAX, DP_MAX_DMA_PORT, DP_MAX_DMA_CHAN);
		return DP_FAILURE;
	}
	dp_deq_port_tbl[inst][cqe_deq].ref_cnt++;
	atomic_inc(&dp_dma_chan_tbl[inst][cid][pid][nid].ref_cnt);
	DP_DEBUG(DP_DBG_FLAG_REG, "cbm[%d].ref_cnt=%d DMATXCH_Ref.cnt=%d\n",
		 cqe_deq,
		 dp_deq_port_tbl[inst][cqe_deq].ref_cnt,
		 atomic_read(&dp_dma_chan_tbl[inst][cid][pid][nid].ref_cnt));
	return 0;
}

/*
 * subif_hw_reset - AVM:512-548 verbatim with PR_ERR -> dev_err and DP_DEBUG
 * -> dev_dbg rewrite.
 */
static noinline int subif_hw_reset(int inst, int portid, int subif_ix,
				   struct subif_platform_data *data, u32 flags)
{
	int deq_port_idx = 0, cqe_deq;
	struct pmac_port_info *port_info;
	u32 cid, pid, nid;
	u32 dma_chan;

	if (!data || !data->subif_data) {
		dev_err(dp_dev, "data NULL or subif_data NULL\n");
		return -1;
	}
	port_info = &dp_port_info[inst][portid];
	if (data->subif_data)
		deq_port_idx = data->subif_data->deq_port_idx;
	if (port_info->deq_port_num < deq_port_idx + 1) {
		dev_err(dp_dev, "Wrong deq_port_idx(%d), should < %d\n",
			deq_port_idx, port_info->deq_port_num);
		return -1;
	}
	cqe_deq = port_info->deq_port_base + deq_port_idx;
	if (!dp_deq_port_tbl[inst][cqe_deq].ref_cnt) {
		dev_err(dp_dev, "Wrong cbm[%d].ref_cnt=%d\n",
			cqe_deq,
			dp_deq_port_tbl[inst][cqe_deq].ref_cnt);
		return -1;
	}
	dma_chan = dp_deq_port_tbl[inst][cqe_deq].dma_chan;
	cid = _DMA_CONTROLLER(dma_chan);
	pid = _DMA_PORT(dma_chan);
	nid = _DMA_CHANNEL(dma_chan);
	/* cid, pid and nid should not greater than DP_DMAMAX,
	 * DP_MAX_DMA_PORT and DP_MAX_DMA_CHAN respectively.
	 */
	if ((cid >= DP_DMAMAX) || (pid >= DP_MAX_DMA_PORT) ||
	    nid >= DP_MAX_DMA_CHAN) {
		dev_err(dp_dev, "ERROR: cid=%d pid=%d nid=%d\n",
			cid, pid, nid);
		dev_err(dp_dev,
			"DMAMAX=%d MAX_DMA_PORT=%d MAX_DMA_CHAN=%d\n",
			DP_DMAMAX, DP_MAX_DMA_PORT, DP_MAX_DMA_CHAN);
		return DP_FAILURE;
	}
	DP_DEBUG(DP_DBG_FLAG_DBG, "cid=%d pid=%d nid=%d\n", cid, pid, nid);
	dp_deq_port_tbl[inst][cqe_deq].ref_cnt--;
	atomic_dec(&dp_dma_chan_tbl[inst][cid][pid][nid].ref_cnt);
	DP_DEBUG(DP_DBG_FLAG_REG, "cbm[%d].ref_cnt=%d DMATXCH_Ref_cnt=%d\n",
		 cqe_deq,
		 dp_deq_port_tbl[inst][cqe_deq].ref_cnt,
		 atomic_read(&dp_dma_chan_tbl[inst][cid][pid][nid].ref_cnt));
	return 0;
}

/*
 * subif_platform_set — AVM:550-557 verbatim. Dispatches to subif_hw_reset
 * on DP_F_DEREGISTER, otherwise subif_hw_set.
 */
static int subif_platform_set(int inst, int portid, int subif_ix,
			      struct subif_platform_data *data, u32 flags)
{
	if (flags & DP_F_DEREGISTER)
		return subif_hw_reset(inst, portid, subif_ix, data, flags);
	return subif_hw_set(inst, portid, subif_ix, data, flags);
}

/*
 * subif_platform_set_unexplicit — AVM:559-562 verbatim. Returns 0.
 */
static int subif_platform_set_unexplicit(int inst, int port_id,
					 struct logic_dev *dev,
					 u32 flag)
{
	return 0;
}

/*
 * not_valid_rx_ep — AVM:564-566 verbatim.
 */
static int not_valid_rx_ep(int ep)
{
	return (((ep >= 1) && (ep <= 6)) || (ep >= 15));
}

/*
 * set_pmac_subif — AVM:568-573 verbatim.
 */
static void set_pmac_subif(struct pmac_tx_hdr *pmac, int32_t subif)
{
	pmac->src_sub_inf_id2 = subif & 0xff;
	pmac->src_sub_inf_id = (subif >> 8) & 0x1f;
}

/*
 * update_port_vap — AVM:575-590 verbatim. DP_F_LOOPBACK token resolves to
 * the file-scope local fallback (= 0x40 = AVM BIT(6)).
 */
static void update_port_vap(int inst, u32 *ep, int *vap,
			    struct sk_buff *skb,
			    struct pmac_rx_hdr *pmac, char *decryp)
{
	*ep = pmac->sppid; /* get the port_id from pmac's sppid */
	if (dp_port_info[inst][*ep].alloc_flags & DP_F_LOOPBACK) {
		*ep = GET_VAP((u32)pmac->src_sub_inf_id2 +
				      (u32)(pmac->src_sub_inf_id << 8),
			      PORT_INFO(inst, *ep, vap_offset),
			      PORT_INFO(inst, *ep, vap_mask));
		*vap = 0;
		*decryp = 1;
	} else {
		*vap = GET_VAP((u32)pmac->src_sub_inf_id2 +
				       (u32)(pmac->src_sub_inf_id << 8),
			       PORT_INFO(inst, *ep, vap_offset),
			       PORT_INFO(inst, *ep, vap_mask));
	}
}

/*
 * get_dma_pmac_templ — AVM:592-602 verbatim. Applies the per-CTP DMA1
 * mask-and-OR template plus optional pmac header memcpy.
 */
static void get_dma_pmac_templ(int index, struct pmac_tx_hdr *pmac,
			       struct dma_tx_desc_0 *desc_0,
			       struct dma_tx_desc_1 *desc_1,
			       struct pmac_port_info2 *dp_info)
{
	if (likely(pmac))
		memcpy(pmac, &dp_info->pmac_template[index], sizeof(*pmac));
	desc_1->all = (desc_1->all & dp_info->dma1_mask_template[index].all) |
		      dp_info->dma1_template[index].all;
}

/*
 * check_csum_cap — AVM:604-606 verbatim. Returns 1.
 */
static int check_csum_cap(void)
{
	return 1;
}

/*
 * get_itf_start_end — AVM:608-616 verbatim. Optional out-pointer fill of
 * the CTP-interface start/end range.
 */
static int get_itf_start_end(struct gsw_itf *itf_info, u16 *start, u16 *end)
{
	if (!itf_info)
		return -1;
	if (start)
		*start = itf_info->start;
	if (end)
		*end = itf_info->end;

	return 0;
}

/*
 * Partial PCE programming is recoverable via dev_warn + continue; aborting on
 * first failure could leave silicon in a worse state.
 */
static int __maybe_unused gswip30_program_queue_map(int inst, int portid)
{
	ethsw_api_dev_t *pethdev;
	void *cdev;
	int tc;

	BUILD_BUG_ON(PCE_QUEUE_MAP_INDEX > 30);

	pethdev = container_of(dp_port_prop[inst].ops[0],
			       ethsw_api_dev_t, ops);
	cdev = pethdev;

	for (tc = 0; tc < 16; tc++) {
		/*
		 * pctbl_prog_t carries fixed-size key[34]/mask[4]/val[31]
		 * arrays. The matching {num_key, num_mask, num_val} triplet
		 * lives in the gsw_pce_tbl_info_30[] table-shape descriptor
		 * inside switch-api/gsw_flow_pce.c — the helper reads
		 * gswdev->pce_tbl_info[table].num_val to decide how many
		 * val[] entries to push. For PCE_QUEUE_MAP_INDEX the shape
		 * descriptor sets num_val=1, so a zero-initialised val[0]=0
		 * (default queue) is exactly what is programmed.
		 */
		pctbl_prog_t ptbl = {
			.table = PCE_QUEUE_MAP_INDEX,
			.pcindex = (u16)((portid << 4) | tc),
		};
		int ret;

		ret = gsw_pce_table_write(cdev, &ptbl);
		if (ret != 0) {
			dev_warn(dp_dev,
				 "gswip30_program_queue_map: gsw_pce_table_write tc=%d failed=%d\n",
				 tc, ret);
			continue;
		}
	}
	return 0;
}

/*
 * The cap struct is zero-initialised by memset(&cap, 0, sizeof(cap)) at
 * function entry, so unassigned fields default to NULL.
 */
int register_dp_cap_gswip30(int flag)
{
	struct dp_hw_cap cap;

	memset(&cap, 0, sizeof(cap));
	cap.info.type = GSWIP30_TYPE;
	cap.info.ver = GSWIP30_VER;
	cap.info.dp_platform_set = dp_platform_set;
	cap.info.port_platform_set = port_platform_set;
	cap.info.dev_platform_set = dev_platform_set;
	cap.info.subif_platform_set_unexplicit = subif_platform_set_unexplicit;
	cap.info.init_dma_pmac_template = init_dma_pmac_template;
	cap.info.subif_platform_set = subif_platform_set;
	cap.info.not_valid_rx_ep = not_valid_rx_ep;
	cap.info.set_pmac_subif = set_pmac_subif;
	cap.info.update_port_vap = update_port_vap;
	cap.info.check_csum_cap = check_csum_cap;
	cap.info.get_dma_pmac_templ = get_dma_pmac_templ;
	cap.info.get_itf_start_end = get_itf_start_end;
	cap.info.dump_rx_dma_desc = dump_rx_dma_desc;
	cap.info.dump_tx_dma_desc = dump_tx_dma_desc;
	cap.info.dump_rx_pmac = dump_rx_pmac;
	cap.info.dump_tx_pmac = dump_tx_pmac;
	cap.info.supported_logic_dev = supported_logic_dev;
	cap.info.cap.tx_hw_chksum = 1;
	cap.info.cap.rx_hw_chksum = 1;
	cap.info.cap.hw_tso = 1;
	cap.info.cap.hw_gso = 1;
	strncpy(cap.info.cap.qos_eng_name, "TMU",
		sizeof(cap.info.cap.qos_eng_name));
	strncpy(cap.info.cap.pkt_eng_name, "PAE/MPE",
		sizeof(cap.info.cap.pkt_eng_name));
	cap.info.cap.max_num_queues = 128;
	cap.info.cap.max_num_scheds = 128;
	cap.info.cap.max_num_deq_ports = 24;
	cap.info.cap.max_num_qos_ports = 24;
	cap.info.cap.max_num_dp_ports = PMAC_MAX_NUM;
	cap.info.cap.max_num_subif_per_port = 16;
	cap.info.cap.max_num_subif = 256;
	cap.info.cap.max_num_bridge_port = 0;

	if (register_dp_hw_cap(&cap, flag)) {
		dev_err(dp_dev, "Why register_dp_hw_cap fail\n");
		return -1;
	}

	dev_info(dp_dev,
		 "dp-gswip30: hw cap registered, type=%d ver=%d max_dp_ports=%u max_subif=%u\n",
		 cap.info.type, cap.info.ver,
		 cap.info.cap.max_num_dp_ports, cap.info.cap.max_num_subif);
	return 0;
}
