/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * include/net/datapath_inst.h.
 *
 * Per-instance datapath state.
 */

#ifndef DATAPATH_INST_H
#define DATAPATH_INST_H

#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>

/*
 * Any caller that actually dereferences struct core_ops fields must include
 * the gsw_dev header itself.
 */
struct core_ops;
struct mac_ops;

#define DP_MAX_GSW_HANDLE 2  /* max GSW instance per SOC */
#define DP_MAX_MAC_HANDLE 11 /* max MAC instance per SOC */

#ifndef DP_MAX_INST
#define DP_MAX_INST 1
#endif

/* Forward decls of types referenced only by pointer below. */
struct logic_dev;
struct subif_platform_data;
struct pmac_port_info;
struct pmac_port_info2;
struct gsw_itf;
struct dp_port_data;
struct dp_dev_data;
struct dp_subif_data;
struct dp_pmapper;
struct dp_tc_vlan;
struct dp_tc_vlan_info;
struct pmac_tx_hdr;
struct pmac_rx_hdr;
struct dma_tx_desc_0;
struct dma_tx_desc_1;
struct dma_tx_desc_2;
struct dma_tx_desc_3;
struct dma_rx_desc_0;
struct dma_rx_desc_1;
struct dma_rx_desc_2;
struct dma_rx_desc_3;
struct dp_subif;
typedef struct dp_subif dp_subif_t;
struct dp_pmac_cfg;
typedef struct dp_pmac_cfg dp_pmac_cfg_t;

/* enum for DP HW capability type */
enum DP_HW_CAP_TYPE {
	GSWIP30_TYPE = 0,
	GSWIP31_TYPE
};

/* enum for DP HW version */
enum DP_HW_CAP_VER {
	GSWIP30_VER = 0,
	GSWIP31_VER
};

#ifndef DP_MAX_NAME
#define DP_MAX_NAME 20
#endif

struct dp_umt_cap_mode {
	u32 enable : 1;
	u32 rx_accumulate : 1;
	u32 rx_incremental : 1;
	u32 tx_accumulate : 1;
	u32 tx_incremental : 1;
};

struct dp_umt_cap {
	struct dp_umt_cap_mode umt_hw_auto;
	struct dp_umt_cap_mode umt_hw_user;
	struct dp_umt_cap_mode umt_sw;
};

/* struct dp_cap: dp capability per instance (AVM datapath_api.h:1149-1178). */
struct dp_cap {
	int inst;
	u32 tx_hw_chksum : 1;
	u32 rx_hw_chksum : 1;
	u32 hw_tso : 1;
	u32 hw_gso : 1;
	u32 hw_ptp : 1;
	char qos_eng_name[DP_MAX_NAME];
	char pkt_eng_name[DP_MAX_NAME];
	int max_num_queues;
	int max_num_scheds;
	int max_num_deq_ports;
	int max_num_qos_ports;
	int max_num_dp_ports;
	int max_num_subif_per_port;
	int max_num_subif;
	int max_num_bridge_port;
	struct dp_umt_cap umt;
};

struct dp_inst_info {
	int inst;
	enum DP_HW_CAP_TYPE type;
	enum DP_HW_CAP_VER ver;
	struct core_ops *ops[DP_MAX_GSW_HANDLE];
	struct mac_ops *mac_ops[DP_MAX_MAC_HANDLE];
	int cbm_inst;
	int qos_inst;
};

/* Per-HW-capability dispatch table. */
struct inst_info {
	enum DP_HW_CAP_TYPE type;
	enum DP_HW_CAP_VER ver;
	int max_ports;
	int max_port_subifs;
	struct dp_cap cap; /* value-embedded */
	int (*dp_platform_set)(int inst, u32 flag);
	int (*port_platform_set)(int inst, u8 ep, struct dp_port_data *data,
				 uint32_t flags);
	int (*dev_platform_set)(int inst, u8 ep, struct dp_dev_data *data,
				uint32_t flags);
	int (*subif_platform_set_unexplicit)(int inst, int port_id,
					     struct logic_dev *dev,
					     u32 flag);
	int (*port_platform_set_unexplicit)(int inst, int port_id,
					    struct logic_dev *dev,
					    u32 flag);
	int (*subif_platform_set)(int inst, int portid, int subif_ix,
				  struct subif_platform_data *data,
				  u32 flags);
	int (*proc_print_ctp_bp_info)(struct seq_file *s, int inst,
				      struct pmac_port_info *port,
				      int subif_index, u32 flag);
	void (*init_dma_pmac_template)(int portid, uint32_t flags);
	int (*not_valid_rx_ep)(int ep);
	void (*set_pmac_subif)(struct pmac_tx_hdr *pmac, int32_t subif);
	void (*update_port_vap)(int inst, u32 *ep, int *vap,
				struct sk_buff *skb,
				struct pmac_rx_hdr *pmac, char *decryp);
	int (*check_csum_cap)(void);
	void (*get_dma_pmac_templ)(int index, struct pmac_tx_hdr *pmac,
				   struct dma_tx_desc_0 *desc_0,
				   struct dma_tx_desc_1 *desc_1,
				   struct pmac_port_info2 *dp_info);
	int (*get_itf_start_end)(struct gsw_itf *itf_info, u16 *start,
				 u16 *end);
	void (*dump_rx_dma_desc)(struct dma_rx_desc_0 *desc_0,
				 struct dma_rx_desc_1 *desc_1,
				 struct dma_rx_desc_2 *desc_2,
				 struct dma_rx_desc_3 *desc_3);
	void (*dump_tx_dma_desc)(struct dma_tx_desc_0 *desc_0,
				 struct dma_tx_desc_1 *desc_1,
				 struct dma_tx_desc_2 *desc_2,
				 struct dma_tx_desc_3 *desc_3);
	void (*dump_rx_pmac)(struct pmac_rx_hdr *pmac);
	void (*dump_tx_pmac)(struct pmac_tx_hdr *pmac);
	int (*supported_logic_dev)(int inst, struct net_device *dev,
				   char *subif_name);
	int (*dp_pmac_set)(int inst, u32 port, dp_pmac_cfg_t *pmac_cfg);
	int (*dp_set_gsw_parser)(u8 flag, u8 cpu, u8 mpe1, u8 mpe2, u8 mpe3);
	int (*dp_get_gsw_parser)(u8 *cpu, u8 *mpe1, u8 *mpe2, u8 *mpe3);
	int (*dp_qos_platform_set)(int cmd_id, void *cfg, int flag);
	int (*dp_get_port_vap_mib)(dp_subif_t *subif_id, void *priv,
				   void *stats,
				   u32 flags);
	int (*dp_clear_netif_mib)(dp_subif_t *subif, void *priv, u32 flag);
	int (*dp_set_gsw_pmapper)(int inst, int bport, int lport,
				  struct dp_pmapper *mapper, u32 flag);
	int (*dp_get_gsw_pmapper)(int inst, int bport, int lport,
				  struct dp_pmapper *mapper, u32 flag);
	int (*dp_tc_vlan_set)(struct core_ops *ops, struct dp_tc_vlan *vlan,
			      struct dp_tc_vlan_info *info,
			      int flag);
};

struct dp_inst {
	enum DP_HW_CAP_TYPE type;
	enum DP_HW_CAP_VER ver;
	struct inst_info info;
};

struct dp_hw_cap {
	u8 valid;
	struct inst_info info;
};

struct inst_property {
	u8 valid;
	struct inst_info info;
	/* driver should know which HW to configure, esp for PCIe case */
	struct core_ops *ops[DP_MAX_GSW_HANDLE];
	struct mac_ops *mac_ops[DP_MAX_MAC_HANDLE];
	int cbm_inst;
	int qos_inst;
	void *priv_hal; /* private data per HAL */
};

int register_dp_cap_gswip30(int flag);
int register_dp_hw_cap(struct dp_hw_cap *info, u32 flag);

/* request a new DP instance based on its HW type/version */
int dp_request_inst(struct dp_inst_info *info, u32 flag);

#endif /* DATAPATH_INST_H */
