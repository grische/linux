/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * datapath/datapath.h.
 *
 * Intra-module state and helpers for the datapath manager.
 */

#ifndef DATAPATH_H
#define DATAPATH_H

#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/bug.h>

#include "lantiq_cbm_api.h"
#include "datapath_api_qos.h"
#include "datapath_inst.h"
#include "datapath_api_gswip30.h"

/*
 * Forward declarations for public-API types that AVM keeps in
 * include/net/datapath_api.h.
 */
struct dp_subif;
typedef struct dp_subif dp_subif_t;

#ifndef DP_MAX_INST
#define DP_MAX_INST 1
#endif
#define MAX_SUBIFS 256
#define MAX_DP_PORTS 16
#define PMAC_SIZE 8
#define PMAC_CPU_ID 0
#define DP_MAX_BP_NUM 128
#define DP_MAX_QUEUE_NUM 256
#define DP_MAX_SCHED_NUM 2048    /* max scheduler node count */
#define DP_MAX_CQM_DEQ 128       /* CQM dequeue port */

/* fallbacks for the AVM klogging.h LOGF_* macros now that the include is
 * dropped (AVM gated each of these behind ifdef LOGF_KLOG_*). */
#define PR_ERR printk
#define PR_INFO printk
#define PR_INFO_ONCE printk_once
#define PR_RATELIMITED printk_ratelimited

#define DP_PLATFORM_INIT 1
#define DP_PLATFORM_DE_INIT 2

#define UP_STATS(atomic) atomic_add(1, &(atomic))

#define STATS_GET(atomic) atomic_read(&(atomic))
#define STATS_SET(atomic, val) atomic_set(&(atomic), val)
#define DP_CB(i, x) dp_port_prop[i].info.x

#define PORT(inst, ep) (&dp_port_info[inst][ep])
#define PORT_INFO(inst, ep, x) (dp_port_info[inst][ep].x)
#define PORT_SUBIF(inst, ep, ix, x) (dp_port_info[inst][ep].subif_info[ix].x)
#define PORT_VAP_MIB(i, ep, vap, x) (dp_port_info[i][ep].subif_info[vap].mib.x)
#define PORT_VAP(i, ep, vap, x) (dp_port_info[i][ep].subif_info[vap].x)

#define dp_set_val(reg, val, mask, offset)               \
	do {                                             \
		(reg) &= ~(mask);                        \
		(reg) |= (((val) << (offset)) & (mask)); \
	} while (0)

#define dp_get_val(val, mask, offset) (((val) & (mask)) >> (offset))

#define DP_DEBUG_ASSERT(expr, fmt, arg...)                  \
	do {                                                \
		if (expr)                                   \
			dev_err(dp_dev, fmt, ##arg);        \
	} while (0)

#define DP_DEBUG(flags, fmt, arg...) dev_dbg(dp_dev, fmt, ##arg)

/* AVM define IFNAMSIZ inline; v6.18 has linux/if.h providing it. Define
 * unconditionally for compatibility with the AVM struct layouts. */
#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#define DP_MAX_HW_CAP 4

/*
 * DP_LIB_LOCK / DP_LIB_UNLOCK: AVM picked spin_lock_bh for SOC_GRX500 and
 * mutex_lock otherwise; the SOC_GRX500 family is in the strip list so the
 * mutex form is pinned unconditionally.
 */
#define DP_LIB_LOCK mutex_lock
#define DP_LIB_UNLOCK mutex_unlock

#define PARSER_FLAG_SIZE 40
#define PARSER_OFFSET_SIZE 8
#define DP_PMAC_OPS(gsw, cmd) (dp_gsw_cb)(gsw)->gsw_pmac_ops.cmd

#define PKT_PASER_FLAG_OFFSET 0
#define PKT_PASER_OFFSET_OFFSET (PARSER_FLAG_SIZE)
#define PKT_PMAC_OFFSET ((PARSER_FLAG_SIZE) + (PARSER_OFFSET_SIZE))
#define PKT_DATA_OFFSET ((PKT_PMAC_OFFSET) + (PMAC_SIZE))

#define CHECK_BIT(var, pos) (((var) & (1 << (pos))) ? 1 : 0)

#define PASAR_OFFSETS_NUM 40 /* 40 bytes offset */
#define PASAR_FLAGS_NUM 8    /* 8 bytes */

#define GET_VAP(subif, bit_shift, mask) (((subif) >> (bit_shift)) & (mask))
#define SET_VAP(vap, bit_shift, mask) ((((u32)vap) & (mask)) << (bit_shift))

/* maximum DMA port per controller */
#define DP_MAX_DMA_PORT 4
/* maximum dma channels per port */
#define DP_MAX_DMA_CHAN 64
/* maximum dma controller */
#define DP_DMAMAX 7

enum dp_xmit_errors {
	DP_XMIT_ERR_DEFAULT = 0,
	DP_XMIT_ERR_NOT_INIT,
	DP_XMIT_ERR_IN_IRQ,
	DP_XMIT_ERR_NULL_SUBIF,
	DP_XMIT_ERR_PORT_TOO_BIG,
	DP_XMIT_ERR_NULL_SKB,
	DP_XMIT_ERR_NULL_IF,
	DP_XMIT_ERR_REALLOC_SKB,
	DP_XMIT_ERR_EP_ZERO,
	DP_XMIT_ERR_GSO_NOHEADROOM,
	DP_XMIT_ERR_CSM_NO_SUPPORT,
	DP_XMIT_PTP_ERR,
};

enum PARSER_FLAGS {
	PASER_FLAGS_NO = 0,
	PASER_FLAGS_END,
	PASER_FLAGS_CAPWAP,
	PASER_FLAGS_GRE,
	PASER_FLAGS_LEN,
	PASER_FLAGS_GREK,
	PASER_FLAGS_NN1,
	PASER_FLAGS_NN2,
	PASER_FLAGS_ITAG,
	PASER_FLAGS_1VLAN,
	PASER_FLAGS_2VLAN,
	PASER_FLAGS_3VLAN,
	PASER_FLAGS_4VLAN,
	PASER_FLAGS_SNAP,
	PASER_FLAGS_PPPOES,
	PASER_FLAGS_1IPV4,
	PASER_FLAGS_1IPV6,
	PASER_FLAGS_2IPV4,
	PASER_FLAGS_2IPV6,
	PASER_FLAGS_ROUTEXP,
	PASER_FLAGS_TCP,
	PASER_FLAGS_1UDP,
	PASER_FLAGS_IGMP,
	PASER_FLAGS_IPV4OPT,
	PASER_FLAGS_IPV6EXT,
	PASER_FLAGS_TCPACK,
	PASER_FLAGS_IPFRAG,
	PASER_FLAGS_EAPOL,
	PASER_FLAGS_2IPV6EXT,
	PASER_FLAGS_2UDP,
	PASER_FLAGS_L2TPNEXP,
	PASER_FLAGS_LROEXP,
	PASER_FLAGS_L2TP,
	PASER_FLAGS_GRE_VLAN1,
	PASER_FLAGS_GRE_VLAN2,
	PASER_FLAGS_GRE_PPPOE,
	PASER_FLAGS_BYTE4_BIT4,
	PASER_FLAGS_BYTE4_BIT5,
	PASER_FLAGS_BYTE4_BIT6,
	PASER_FLAGS_BYTE4_BIT7,
	PASER_FLAGS_BYTE5_BIT0,
	PASER_FLAGS_BYTE5_BIT1,
	PASER_FLAGS_BYTE5_BIT2,
	PASER_FLAGS_BYTE5_BIT3,
	PASER_FLAGS_BYTE5_BIT4,
	PASER_FLAGS_BYTE5_BIT5,
	PASER_FLAGS_BYTE5_BIT6,
	PASER_FLAGS_BYTE5_BIT7,
	PASER_FLAGS_BYTE6_BIT0,
	PASER_FLAGS_BYTE6_BIT1,
	PASER_FLAGS_BYTE6_BIT2,
	PASER_FLAGS_BYTE6_BIT3,
	PASER_FLAGS_BYTE6_BIT4,
	PASER_FLAGS_BYTE6_BIT5,
	PASER_FLAGS_BYTE6_BIT6,
	PASER_FLAGS_BYTE6_BIT7,
	PASER_FLAGS_BYTE7_BIT0,
	PASER_FLAGS_BYTE7_BIT1,
	PASER_FLAGS_BYTE7_BIT2,
	PASER_FLAGS_BYTE7_BIT3,
	PASER_FLAGS_BYTE7_BIT4,
	PASER_FLAGS_BYTE7_BIT5,
	PASER_FLAGS_BYTE7_BIT6,
	PASER_FLAGS_BYTE7_BIT7,
	/* Must be put at the end of the enum */
	PASER_FLAGS_MAX
};

/* PMAC port flag */
enum PORT_FLAG {
	PORT_FREE = 0,         /* port is free */
	PORT_ALLOCATED,        /* port is allocated to others, possibly not
				* registered, eg LRO/CAPWA */
	PORT_DEV_REGISTERED,   /* dev registered already */
	PORT_SUBIF_REGISTERED, /* subif registered already */
	PORT_FLAG_NO_VALID     /* not valid flag */
};

#define DP_DBG_ENUM_OR_STRING(name, value, short_name) \
	{                                              \
		name = value                           \
	}

enum PMAC_TCP_TYPE {
	TCP_OVER_IPV4 = 0,
	UDP_OVER_IPV4,
	TCP_OVER_IPV6,
	UDP_OVER_IPV6,
	TCP_OVER_IPV6_IPV4,
	UDP_OVER_IPV6_IPV4,
	TCP_OVER_IPV4_IPV6,
	UDP_OVER_IPV4_IPV6
};

enum DP_DBG_FLAG {
	DP_DBG_FLAG_DBG = BIT(0),
	DP_DBG_FLAG_DUMP_RX_DATA = BIT(1),
	DP_DBG_FLAG_DUMP_RX_DESCRIPTOR = BIT(2),
	DP_DBG_FLAG_DUMP_RX_PASER = BIT(3),
	DP_DBG_FLAG_DUMP_RX_PMAC = BIT(4),
	DP_DBG_FLAG_DUMP_RX = (BIT(1) | BIT(2) | BIT(3) | BIT(4)),
	DP_DBG_FLAG_DUMP_TX_DATA = BIT(5),
	DP_DBG_FLAG_DUMP_TX_DESCRIPTOR = BIT(6),
	DP_DBG_FLAG_DUMP_TX_PMAC = BIT(7),
	DP_DBG_FLAG_DUMP_TX_SUM = BIT(8),
	DP_DBG_FLAG_DUMP_TX = (BIT(5) | BIT(6) | BIT(7) | BIT(8)),
	DP_DBG_FLAG_COC = BIT(9),
	DP_DBG_FLAG_MIB = BIT(10),
	DP_DBG_FLAG_MIB_ALGO = BIT(11),
	DP_DBG_FLAG_CBM_BUF = BIT(12),
	DP_DBG_FLAG_PAE = BIT(13),
	DP_DBG_FLAG_INST = BIT(14),
	DP_DBG_FLAG_SWDEV = BIT(15),
	DP_DBG_FLAG_NOTIFY = BIT(16),
	DP_DBG_FLAG_LOGIC = BIT(17),
	DP_DBG_FLAG_GSWIP_API = BIT(18),
	DP_DBG_FLAG_QOS = BIT(19),
	DP_DBG_FLAG_QOS_DETAIL = BIT(20),
	DP_DBG_FLAG_LOOKUP = BIT(21),
	DP_DBG_FLAG_REG = BIT(22),
	DP_DBG_FLAG_MAX = BIT(31)
};

enum {
	NODE_LINK_ADD = 0,
	NODE_LINK_GET,
	NODE_LINK_EN_GET,
	NODE_LINK_EN_SET,
	NODE_UNLINK,
	LINK_ADD,
	LINK_GET,
	LINK_PRIO_SET,
	LINK_PRIO_GET,
	QUEUE_CFG_SET,
	QUEUE_CFG_GET,
	SHAPER_SET,
	SHAPER_GET,
	NODE_ALLOC,
	NODE_FREE,
	NODE_CHILDREN_FREE,
	DEQ_PORT_RES_GET,
	COUNTER_MODE_SET,
	COUNTER_MODE_GET,
	QUEUE_MAP_GET,
	QUEUE_MAP_SET,
	NODE_CHILDREN_GET,
	QOS_LEVEL_GET,
};

/* Driver-mib network counters (AVM include/net/datapath_api.h dp_drv_mib_t). */
typedef struct dp_drv_mib {
	u64 rx_drop_pkts;
	u64 rx_error_pkts;
	u64 tx_drop_pkts;
	u64 tx_error_pkts;
	u64 tx_pkts;
	u64 tx_bytes;
} dp_drv_mib_t;

struct dev_mib {
	atomic_t rx_fn_rxif_pkt;
	atomic_t rx_fn_txif_pkt;
	atomic_t rx_fn_dropped;
	atomic_t tx_cbm_pkt;
	atomic_t tx_clone_pkt;
	atomic_t tx_hdr_room_pkt;
	atomic_t tx_tso_pkt;
	atomic_t tx_pkt_dropped;
};

struct logic_dev {
	struct list_head list;
	struct net_device *dev;
	u16 bp;
	u16 ep;
	u16 ctp;
	u32 subif_flag;
};

/*
 * Sub-interface (CTP) detail. Note dp_subif_t is forward-declared and used
 * by pointer only - struct dp_subif_info uses its own fields, not dp_subif_t.
 */
struct dp_subif_info {
	s32 flags;
	u32 subif : 15;
	struct net_device *netif;
	char device_name[IFNAMSIZ];
	struct dev_mib mib;
	struct net_device *ctp_dev;
	u16 bp;
	u16 fid;
	struct list_head logic_dev;
	struct net_device_ops *old_dev_ops;
	struct net_device_ops new_dev_ops;
	s16 qid;
	s16 sched_id;
	s16 q_node;
	s16 qos_deq_port;
	s16 cqm_deq_port;
	s16 cqm_port_idx;
	u32 subif_flag;
};

struct vlan_info {
	u16 out_proto;
	u16 out_vid;
	u16 in_proto;
	u16 in_vid;
	int cnt;
};

enum DP_TEMP_DMA_PMAC {
	TEMPL_NORMAL = 0,
	TEMPL_CHECKSUM,
	TEMPL_OTHERS,
	MAX_TEMPLATE
};

enum DP_PRIV_F {
	DP_PRIV_PER_CTP_QUEUE = BIT(0), /* manage queue per CTP/subif */
};

/* Datapath registration callback bundle. */
typedef int32_t (*dp_rx_fn_t)(struct net_device *rxif, struct net_device *txif,
			      struct sk_buff *skb, int32_t len);
typedef int32_t (*dp_stop_tx_fn_t)(struct net_device *dev);
typedef int32_t (*dp_restart_tx_fn_t)(struct net_device *dev);
typedef int32_t (*dp_reset_mib_fn_t)(dp_subif_t *subif, int32_t flag);
typedef int32_t (*dp_get_mib_fn_t)(dp_subif_t *subif,
				   dp_drv_mib_t *mib, int32_t flag);
typedef int32_t (*dp_get_netif_subifid_fn_t)(struct net_device *netif,
					     struct sk_buff *skb,
					     void *subif_data,
					     uint8_t *dst_mac,
					     dp_subif_t *subif,
					     uint32_t flags);

struct dp_cb {
	dp_rx_fn_t rx_fn;
	dp_stop_tx_fn_t stop_fn;
	dp_restart_tx_fn_t restart_fn;
	dp_get_netif_subifid_fn_t get_subifid_fn;
	dp_reset_mib_fn_t reset_mib_fn;
	dp_get_mib_fn_t get_mib_fn;
	irqreturn_t (*dma_rx_irq)(int irq, void *dev_instance);
	int (*aca_fw_stop)(void *cfg, int flags);
};

struct pmac_port_info {
	enum PORT_FLAG status;
	int alloc_flags;
	struct dp_cb cb;
	struct module *owner;
	struct net_device *dev;
	u32 dev_port;
	u32 num_subif;
	s32 port_id;
	struct dp_subif_info subif_info[MAX_SUBIFS];
	atomic_t tx_err_drop;
	atomic_t rx_err_drop;
	struct gsw_itf *itf_info;
	int ctp_max;
	u32 vap_offset;
	u32 vap_mask;
	u8 cqe_lu_mode;

	u32 flag_other;
	u32 deq_port_base;
	u32 deq_port_num;
	u32 dma_chan;
	u32 tx_pkt_credit;
	u32 tx_b_credit;
	u32 tx_ring_addr;
	u32 tx_ring_size;
	u32 tx_ring_offset;
	u32 num_dma_chan;
};

struct pmac_port_info2 {
	struct pmac_tx_hdr pmac_template[MAX_TEMPLATE];
	struct dma_tx_desc_0 dma0_template[MAX_TEMPLATE];
	struct dma_tx_desc_0 dma0_mask_template[MAX_TEMPLATE];
	struct dma_tx_desc_1 dma1_template[MAX_TEMPLATE];
	struct dma_tx_desc_1 dma1_mask_template[MAX_TEMPLATE];
};

#define DP_PMAP_PCP_NUM 8
#define DP_PMAP_DSCP_NUM 64

struct bp_pmapper_dev {
	int flag;
	struct net_device *dev;
	int pcp[DP_PMAP_PCP_NUM];
	int dscp[DP_PMAP_DSCP_NUM];
	int def_ctp;
	int mode;
	int ref_cnt;
};

struct q_info {
	int flag;
	int need_free;
	int q_node_id;
	int ref_cnt;
	int cqm_dequeue_port;
};

/*
 * Note: AVM datapath.h named this struct sched_info. v6.18's <linux/sched.h>
 * already defines struct sched_info with different semantics (task-scheduler
 * accounting), so this port renames the datapath variant to dp_sched_info to
 * avoid a redefinition collision.
 */
struct dp_sched_info {
	int flag;
	int ref_cnt;
	int cqm_dequeue_port;
};

struct dma_chan_info {
	atomic_t ref_cnt;
};

struct cqm_port_info {
	int f_first_qid : 1;
	u32 ref_cnt;
	u32 tx_pkt_credit;
	u32 tx_ring_addr;
	u32 tx_ring_size;
	int qos_port;
	int first_qid;
	int q_node;
	int dp_port;
	u32 dma_chan;
};

struct parser_info {
	u8 v;
	s8 size;
};

struct subif_platform_data {
	struct net_device *dev;
	struct dp_subif_data *subif_data;
#define TRIGGER_CQE_DP_ENABLE 1
	int act;
};

/*
 * Global state declared by datapath_api.c. The .c definitions must match the
 * dimensions used here.
 */
extern int dp_inst_num;
extern struct inst_property dp_port_prop[DP_MAX_INST];
extern struct pmac_port_info dp_port_info[DP_MAX_INST][MAX_DP_PORTS];
extern struct pmac_port_info2 dp_port_info2[DP_MAX_INST][MAX_DP_PORTS];
extern struct cqm_port_info dp_deq_port_tbl[DP_MAX_INST][24];
extern struct dma_chan_info dp_dma_chan_tbl[DP_MAX_INST][4][8][16];
extern struct device *dp_dev;
extern struct mutex dp_lock;
extern int dp_init_ok;

extern struct dma_rx_desc_1 dma_rx_desc_mask1;
extern struct dma_rx_desc_3 dma_rx_desc_mask3;
extern struct dma_tx_desc_0 dma_tx_desc_mask0;
extern struct dma_tx_desc_1 dma_tx_desc_mask1;

int dp_init_module(void);
void dp_cleanup_module(void);
int dp_probe(struct platform_device *pdev);
int register_dp_cap(u32 flag);
int request_dp(u32 flag);
int dp_inst_init(void);
int register_dp_cap_gswip30(int flag);
int register_dp_hw_cap(struct dp_hw_cap *info, u32 flag);

int dp_inst_add_dev(struct net_device *dev, char *subif_name, int inst,
		    int port_id, int bp, int subif, u32 flag);
int dp_inst_del_dev(struct net_device *dev, char *subif_name, int inst,
		    int port_id, int bp, int subif);
int dp_get_inst_via_module(struct module *owner, u32 port_id, u32 dev_port);
int dp_request_inst(struct dp_inst_info *info, u32 flag);
int dp_inst_insert_mod(struct module *owner, u16 ep, u32 inst, u32 flag);

static inline int dp_local_add_logic_dev(int inst, int port_id,
					 struct net_device *dev,
					 dp_subif_t *subif_id, u32 flag)
{
	(void)inst;
	(void)port_id;
	(void)dev;
	(void)subif_id;
	(void)flag;
	WARN_ONCE(1,
		  "add_logic_dev: logical device support is not implemented\n");
	return -1;
}

extern uint8_t get_lookup_qid_via_index(uint32_t index);

#endif /* DATAPATH_H */
