// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * datapath/datapath_api.c and datapath_soc.c.
 *
 * Datapath manager: port and sub-interface registration, wiring a netdev to a
 * CBM datapath port and a GSWIP port.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/bug.h>

#include <net/cqm_cbm_api.h>
#include "datapath.h"
#include "datapath_api.h"
#include "../switch-api/gsw_flow_core.h"

/*
 * Forward declaration for the dispatch helper defined in
 * ../switch-api/gsw_flow_core.c. The signature mirrors gsw_flow_core.c:122
 * verbatim.
 */
extern struct core_ops *gsw_get_swcore_ops(u32 devid);

/* File-scope globals declared in datapath.h. */
int dp_inst_num;
struct inst_property dp_port_prop[DP_MAX_INST];
struct pmac_port_info dp_port_info[DP_MAX_INST][MAX_DP_PORTS];
struct pmac_port_info2 dp_port_info2[DP_MAX_INST][MAX_DP_PORTS];
struct cqm_port_info dp_deq_port_tbl[DP_MAX_INST][24];
struct dma_chan_info dp_dma_chan_tbl[DP_MAX_INST][4][8][16];

DEFINE_MUTEX(dp_lock);
struct device *dp_dev;
int dp_init_ok;

struct dma_rx_desc_1 dma_rx_desc_mask1 = { .all = 0 };
struct dma_rx_desc_3 dma_rx_desc_mask3 = { .all = 0 };
struct dma_tx_desc_0 dma_tx_desc_mask0 = { .all = 0 };
struct dma_tx_desc_1 dma_tx_desc_mask1 = { .all = 0 };

/*
 * dp_bp_dev_tbl: pmapper bookkeeping array used by dp_register_subif_private
 * (AVM datapath_api.c:114).
 */
struct bp_pmapper_dev dp_bp_dev_tbl[DP_MAX_INST][DP_MAX_BP_NUM];

#ifndef DP_FAILURE
#define DP_FAILURE (-1)
#endif
#ifndef DP_SUCCESS
#define DP_SUCCESS 0
#endif

/*
 * Local fallbacks for DP_F_* flag bits the AVM public API enum defines in
 * include/net/datapath_api.h. Same approach as datapath_misc.c: byte values
 * must match AVM to keep the compiled .o ABI-compatible with future
 * cross-TU callers. #ifndef guards let a future subtask import the AVM
 * enum first without redefinition diagnostics.
 */
#ifndef DP_F_DEREGISTER
#define DP_F_DEREGISTER			0x00000001
#endif
#ifndef DP_F_FAST_ETH_LAN
#define DP_F_FAST_ETH_LAN		0x00000002
#endif
#ifndef DP_F_FAST_DSL
#define DP_F_FAST_DSL			0x00000010
#endif
#ifndef DP_F_DIRECT
#define DP_F_DIRECT			0x00000020
#endif
#ifndef DP_F_SUBIF_LOGICAL
#define DP_F_SUBIF_LOGICAL		0x00000100
#endif
#ifndef DP_F_ALLOC_EXPLICIT_SUBIFID
#define DP_F_ALLOC_EXPLICIT_SUBIFID	0x00000400
#endif

#ifndef CBM_PORT_F_DISABLE
#define CBM_PORT_F_DISABLE		0x8 /* verbatim */
#endif

#define _DMA_CONTROLLER(nr)	(((nr) >> 24) & 0xFF)
#define _DMA_PORT(nr)		(((nr) >> 16) & 0xFF)
#define _DMA_CHANNEL(nr)	((nr) & 0xFFFF)

/*
 * Minimal type definitions for the public-API structs that AVM keeps in
 * include/net/datapath_api.h.
 *
 * IMPORTANT: each TU that needs these uses its own local definition because
 * datapath_inst.h only forward-declares the tag. datapath_misc.c carries an
 * orthogonal local definition of struct dp_subif_data (deq_port_idx only).
 * Cross-TU passing is by-pointer so the layouts never need to match.
 */
struct dp_subif {
	s32 port_id;
	int inst;
	int subif_num;
	s32 subif;
};

struct dp_port_data {
	int flag_ops;
	u32 resv_num_port;
	u32 start_port_no;
	int num_resv_q;
	int num_resv_sched;
	int deq_port_base;
	int enq_num;
	int deq_num;
};

struct dp_subif_data {
	s8 deq_port_idx;
};

struct dp_pmac_cfg {
	int dummy;
};


/*
 * Defining the privates after their callers requires these forward
 * declarations to resolve compile-time references inside dp_alloc_port_ext /
 * dp_register_subif_ext.
 */
static int dp_alloc_port_private(int inst, struct module *owner,
				 struct net_device *dev, u32 dev_port,
				 s32 port_id, dp_pmac_cfg_t *pmac_cfg,
				 struct dp_port_data *data, u32 flags);
static int dp_register_subif_private(int inst, struct module *owner,
				     struct net_device *dev,
				     char *subif_name, dp_subif_t *subif_id,
				     struct dp_subif_data *data, u32 flags);
static int dp_deregister_subif_private(int inst, struct module *owner,
				       struct net_device *dev,
				       char *subif_name, dp_subif_t *subif_id,
				       struct dp_subif_data *data, u32 flags);

int register_dp_cap(u32 flag)
{
	return register_dp_cap_gswip30(flag);
}

/* request_dp - stripped. */
int request_dp(u32 flag)
{
	(void)flag;

	dp_port_prop[0].ops[0] = gsw_get_swcore_ops(0);
	dp_port_prop[0].ops[1] = gsw_get_swcore_ops(1);
	dp_port_prop[0].cbm_inst = 0;
	dp_port_prop[0].qos_inst = 0;

	if (dp_port_prop[0].info.dp_platform_set &&
	    dp_port_prop[0].info.dp_platform_set(0, DP_PLATFORM_INIT) < 0) {
		dev_err(dp_dev,
			"dp-gswip30: request_dp: dp_platform_set failed\n");
		return -1;
	}
	dp_port_prop[0].valid = 1;
	return 0;
}

/*
 * dp_init_module - probe-time entry point B.
 *
 * If gsw_get_swcore_ops(0) returns NULL (somehow the refcount gate fired
 * before gsw_devs[0] was populated), the function returns -ENODEV with
 * dev_err(NULL, ...) - dev_err tolerates NULL @dev safely in v6.18.
 */
int dp_init_module(void)
{
	struct core_ops *gsw_ops;
	ethsw_api_dev_t *pethdev;
	struct platform_device *pdev;
	int ret;

	if (dp_init_ok)
		return 0;

	ret = dp_inst_init();
	if (ret)
		return ret;

	gsw_ops = gsw_get_swcore_ops(0);
	if (!gsw_ops) {
		dev_err(NULL,
			"dp-gswip30: dp_init_module: GSW HAL not available\n");
		return -ENODEV;
	}

	pethdev = container_of(gsw_ops, ethsw_api_dev_t, ops);
	pdev = (struct platform_device *)pethdev->pdev;
	dp_dev = &pdev->dev;

	register_dp_cap(0);
	if (request_dp(0)) {
		dev_err(dp_dev,
			"dp-gswip30: dp_init_module: request_dp failed\n");
		return -1;
	}

	dp_init_ok = 1;
	return 0;
}

/*
 * dp_alloc_port - AVM datapath_api.c:788-797 wrapper around
 * dp_alloc_port_ext with inst=0 (SOC-side).
 */
int dp_alloc_port(struct module *owner, struct net_device *dev,
		  u32 dev_port, u32 port_id,
		  dp_pmac_cfg_t *pmac_cfg, u32 flags)
{
	return dp_alloc_port_ext(0, owner, dev, dev_port, (int)port_id,
				 pmac_cfg, NULL, flags);
}

int dp_alloc_port_ext(int inst, struct module *owner,
		      struct net_device *dev,
		      u32 dev_port, int port_id,
		      dp_pmac_cfg_t *pmac_cfg,
		      struct dp_port_data *data, u32 flags)
{
	int res;
	struct dp_port_data tmp_data = { 0 };

	if (!dp_init_ok) {
		dev_err(dp_dev,
			"dp_alloc_port_ext: dp_init_module not yet run, refusing\n");
		return DP_FAILURE;
	}
	if (!dp_port_prop[0].valid) {
		dev_err(dp_dev, "No Valid datapath instance yet?\n");
		return DP_FAILURE;
	}
	if (!data)
		data = &tmp_data;
	DP_LIB_LOCK(&dp_lock);
	res = dp_alloc_port_private(inst, owner, dev, dev_port,
				    (s32)port_id, pmac_cfg, data, flags);
	DP_LIB_UNLOCK(&dp_lock);
	return res;
}

static int dp_alloc_port_private(int inst,
				  struct module *owner,
				  struct net_device *dev,
				  u32 dev_port, s32 port_id,
				  dp_pmac_cfg_t *pmac_cfg,
				  struct dp_port_data *data,
				  u32 flags)
{
	int i;
	struct cbm_dp_alloc_data cbm_data = { 0 };

	(void)pmac_cfg; /* dp_pmac_set call excised - see file header */

	if (!owner) {
		dev_err(dp_dev, "Allocate port failed for owner NULL\n");
		return DP_FAILURE;
	}

	if (port_id >= MAX_DP_PORTS || port_id < 0) {
		DP_DEBUG_ASSERT((port_id >= MAX_DP_PORTS),
				"port_id(%d) >= MAX_DP_PORTS(%d)", port_id,
				MAX_DP_PORTS);
		DP_DEBUG_ASSERT((port_id < 0), "port_id(%d) < 0", port_id);
		return DP_FAILURE;
	}

	cbm_data.dp_inst = inst;
	cbm_data.cbm_inst = dp_port_prop[inst].cbm_inst;

	if (flags & DP_F_DEREGISTER) { /*De-register */
		if (dp_port_info[inst][port_id].status != PORT_ALLOCATED) {
			dev_err(dp_dev,
				"No Deallocate for module %s w/o deregistered\n",
				owner->name);
			return DP_FAILURE;
		}
		cbm_data.deq_port = dp_port_info[inst][port_id].deq_port_base;
		cbm_data.dma_chan = dp_port_info[inst][port_id].dma_chan;
		cbm_dp_port_dealloc(owner, dev_port, port_id,
				    &cbm_data, flags);
		dp_inst_insert_mod(owner, port_id, inst, 0);
		DP_DEBUG(DP_DBG_FLAG_REG, "de-alloc port %d\n", port_id);
		DP_CB(inst, port_platform_set)
		(inst, port_id, data, flags);
		memset(&dp_port_info[inst][port_id], 0,
		       sizeof(dp_port_info[inst][port_id]));
		return DP_SUCCESS;
	}
	if (port_id) { /*with specified port_id */
		if (dp_port_info[inst][port_id].status != PORT_FREE) {
			dev_err(dp_dev,
				"module %s(dev_port %d) fail: port %d used by %s %d\n",
				owner->name, dev_port, port_id,
				dp_port_info[inst][port_id].owner ?
					dp_port_info[inst][port_id].owner->name :
					"NULL",
				dp_port_info[inst][port_id].dev_port);
			return DP_FAILURE;
		}
	}
	if (cbm_dp_port_alloc(owner, dev, dev_port, port_id,
			      &cbm_data, flags)) {
		dev_err(dp_dev,
			"cbm_dp_port_alloc fail for %s/dev_port %d: %d\n",
			owner->name, dev_port, port_id);
		return DP_FAILURE;
	} else if (!(cbm_data.flags & CBM_PORT_DP_SET) &&
		   !(cbm_data.flags & CBM_PORT_DQ_SET)) {
		dev_err(dp_dev,
			"cbm_dp_port_alloc NO DP_SET/DQ_SET(%x):%s/dev_port %d\n",
			cbm_data.flags, owner->name, dev_port);
		return DP_FAILURE;
	}
	port_id = cbm_data.dp_port;
	memset(&dp_port_info[inst][port_id], 0,
	       sizeof(dp_port_info[inst][port_id]));
	/*save info from caller */
	dp_port_info[inst][port_id].owner = owner;
	dp_port_info[inst][port_id].dev = dev;
	dp_port_info[inst][port_id].dev_port = dev_port;
	dp_port_info[inst][port_id].alloc_flags = flags;
	dp_port_info[inst][port_id].status = PORT_ALLOCATED;
	/*save info from cbm_dp_port_alloc*/
	dp_port_info[inst][port_id].flag_other = cbm_data.flags;
	dp_port_info[inst][port_id].port_id = cbm_data.dp_port;
	dp_port_info[inst][port_id].deq_port_base = cbm_data.deq_port;
	dp_port_info[inst][port_id].deq_port_num = cbm_data.deq_port_num;
	dp_port_info[inst][port_id].num_dma_chan = cbm_data.num_dma_chan;
	/*save info to port data*/
	data->deq_port_base = dp_port_info[inst][port_id].deq_port_base;
	data->deq_num = dp_port_info[inst][port_id].deq_port_num;
	DP_DEBUG(DP_DBG_FLAG_REG,
		 "cbm alloc dp_port:%d deq:%d deq_num:%d\n",
		 cbm_data.dp_port, cbm_data.deq_port, cbm_data.deq_port_num);
	if (cbm_data.flags & CBM_PORT_DMA_CHAN_SET)
		dp_port_info[inst][port_id].dma_chan = cbm_data.dma_chan;
	if (cbm_data.flags & CBM_PORT_PKT_CRDT_SET)
		dp_port_info[inst][port_id].tx_pkt_credit =
			cbm_data.tx_pkt_credit;
	if (cbm_data.flags & CBM_PORT_BYTE_CRDT_SET)
		dp_port_info[inst][port_id].tx_b_credit = cbm_data.tx_b_credit;
	if (cbm_data.flags & CBM_PORT_RING_ADDR_SET)
		dp_port_info[inst][port_id].tx_ring_addr =
			cbm_data.tx_ring_addr;
	if (cbm_data.flags & CBM_PORT_RING_SIZE_SET)
		dp_port_info[inst][port_id].tx_ring_size =
			cbm_data.tx_ring_size;
	if (cbm_data.flags & CBM_PORT_RING_OFFSET_SET)
		dp_port_info[inst][port_id].tx_ring_offset =
			cbm_data.tx_ring_offset;

	DP_DEBUG(DP_DBG_FLAG_DBG, "cid=%d pid=%d nid=%d\n",
		 _DMA_CONTROLLER(cbm_data.dma_chan),
		 _DMA_PORT(cbm_data.dma_chan),
		 _DMA_CHANNEL(cbm_data.dma_chan));

	if ((cbm_data.num_dma_chan) && (cbm_data.num_dma_chan >
					cbm_data.deq_port_num)) {
		dev_err(dp_dev,
			"ERROR: deq_port_num=%d not equal to num_dma_chan=%d\n",
			cbm_data.deq_port_num, cbm_data.num_dma_chan);
		return DP_FAILURE;
	}

	if (dp_port_prop[inst].info.port_platform_set(inst, port_id,
						      data, flags)) {
		dev_err(dp_dev,
			"Failed port_platform_set for port_id=%d(%s)\n",
			port_id, owner ? owner->name : "");
		cbm_dp_port_dealloc(owner, dev_port, port_id,
				    &cbm_data,
				    flags | DP_F_DEREGISTER);
		memset(&dp_port_info[inst][port_id], 0,
		       sizeof(dp_port_info[inst][port_id]));
		return DP_FAILURE;
	}
	/*only 1st dp instance support real CPU path traffic */
	if (!inst && dp_port_prop[inst].info.init_dma_pmac_template)
		dp_port_prop[inst].info.init_dma_pmac_template(port_id, flags);
	for (i = 0; i < MAX_SUBIFS; i++)
		INIT_LIST_HEAD(&dp_port_info[inst][port_id].subif_info[i].logic_dev);
	dp_inst_insert_mod(owner, port_id, inst, 0);

	DP_DEBUG(DP_DBG_FLAG_REG,
		 "Port %d allocation succeed for module %s with dev_port %d\n",
		 port_id, owner->name, dev_port);
	return port_id;
}

/*
 * dp_register_subif - AVM datapath_api.c:1053-1085 thin wrapper around
 * dp_register_subif_ext. Resolves inst via dp_get_inst_via_module before
 * dispatch.
 */
int dp_register_subif(struct module *owner, struct net_device *dev,
		      char *subif_name, dp_subif_t *subif_id, u32 flags)
{
	int inst;
	struct dp_subif_data data = { 0 };

	if ((!subif_id) || (!subif_id->port_id) || (!owner) ||
	    (subif_id->port_id >= MAX_DP_PORTS) ||
	    (subif_id->port_id <= 0)) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "register subif fail port_id or owner invalid\n");
		return DP_FAILURE;
	}
	inst = dp_get_inst_via_module(owner, (u32)subif_id->port_id, 0);
	if (inst < 0) {
		dev_err(dp_dev, "wrong inst for owner=%s with ep=%d\n",
			owner->name, subif_id->port_id);
		return DP_FAILURE;
	}
	return dp_register_subif_ext(inst, owner, dev, subif_name,
				     subif_id, &data, flags);
}

/* dp_register_subif_ext - AVM datapath_api.c:963-1051. */
int dp_register_subif_ext(int inst, struct module *owner,
			  struct net_device *dev,
			  char *subif_name, dp_subif_t *subif_id,
			  /*device related info*/
			  struct dp_subif_data *data, u32 flags)
{
	int res = DP_FAILURE;
	int port_id;
	struct pmac_port_info *port_info;
	struct dp_subif_data tmp_data = { 0 };

	if (!dp_init_ok) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "dp_register_subif fail for datapath not init yet\n");
		return DP_FAILURE;
	}
	DP_DEBUG(DP_DBG_FLAG_REG,
		 "%s:owner=%s dev=%s subif_name=%s port_id=%d subif=%d\n",
		 (flags & DP_F_DEREGISTER) ?
			 "unregister subif:" :
			 "register subif",
		 owner ? owner->name : "NULL",
		 dev ? dev->name : "NULL",
		 subif_name,
		 subif_id ? subif_id->port_id : -1,
		 subif_id ? subif_id->subif : -1);

	if ((!subif_id) || (!subif_id->port_id) || (!owner) ||
	    (subif_id->port_id >= MAX_DP_PORTS) ||
	    (subif_id->port_id <= 0) ||
	    ((inst < 0) || (inst >= DP_MAX_INST))) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "register subif failed port_id or owner invalid\n");
		return DP_FAILURE;
	}
	port_id = subif_id->port_id;
	port_info = &dp_port_info[inst][port_id];

	if (((!dev) && !(port_info->alloc_flags & DP_F_FAST_DSL)) ||
	    !subif_name) {
		DP_DEBUG(DP_DBG_FLAG_REG, "Wrong dev=%p, subif_name=%p\n",
			 dev, subif_name);
		return DP_FAILURE;
	}
	if (!data)
		data = &tmp_data;
	DP_LIB_LOCK(&dp_lock);
	if (port_info->owner != owner) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister subif fail:Not matching owner\n");
		DP_LIB_UNLOCK(&dp_lock);
		return res;
	}

	if (flags & DP_F_DEREGISTER) {
		res = dp_deregister_subif_private(inst, owner, dev,
						  subif_name,
						  subif_id, data, flags);
	} else {
		res = dp_register_subif_private(inst, owner, dev,
						subif_name,
						subif_id, data, flags);
	}
	DP_LIB_UNLOCK(&dp_lock);
	return res;
}

static int dp_register_subif_private(int inst, struct module *owner,
				     struct net_device *dev,
				     char *subif_name, dp_subif_t *subif_id,
				     /*device related info*/
				     struct dp_subif_data *data, u32 flags)
{
	int res = DP_FAILURE;

	int i, port_id, start, end;
	struct pmac_port_info *port_info;
	struct cbm_dp_en_data cbm_data = { 0 };
	struct subif_platform_data platfrm_data = { 0 };

	port_id = subif_id->port_id;
	port_info = &dp_port_info[inst][port_id];
	subif_id->inst = inst;
	subif_id->subif_num = 1;
	platfrm_data.subif_data = (void *)data;
	platfrm_data.dev = dev;
	/*
	 * TODO(phase-3-dp-register-dev): port AVM dp_register_dev /
	 * dp_register_dev_ext from datapath_api.c:866-961 in a future
	 * subtask. Until then, dp_register_subif accepts PORT_ALLOCATED
	 * status as an entry condition (loosened from AVM line 506:
	 * status < PORT_DEV_REGISTERED).
	 */
	if (port_info->status < PORT_ALLOCATED) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "register subif failed:%s is not a registered dev!\n",
			 subif_name);
		return res;
	}

	if (subif_id->subif < 0) { /*dynamic mode */
		if (flags & DP_F_SUBIF_LOGICAL) {
			if (!(DP_CB(inst, supported_logic_dev)(inst, dev,
							       subif_name))) {
				DP_DEBUG(DP_DBG_FLAG_REG,
					 "reg subif fail:%s not support dev\n",
					 subif_name);
				return res;
			}
			if (!(flags & DP_F_ALLOC_EXPLICIT_SUBIFID)) {
				/*Share same subif with its base device */
				res = dp_local_add_logic_dev(inst, port_id, dev,
							     subif_id, flags);
				return res;
			}
		}
		start = 0;
		end = port_info->ctp_max;
	} else {
		/*caller provided subif. Try to get its vap value as start */
		start = GET_VAP(subif_id->subif, port_info->vap_offset,
				port_info->vap_mask);
		end = start + 1;
	}

	/*allocate a free subif */
	for (i = start; i < end; i++) {
		u32 cqm_deq_port;
		u32 dma_chan;
		u32 cid, pid, nid;
		struct dma_chan_info *dp_dma_chan_tbl_info = NULL;

		if (port_info->subif_info[i].flags) /*used already & not free*/
			continue;

		/*now find a free subif or valid subif
		 *need to do configuration HW
		 */
		if (port_info->status) {
			if (dp_port_prop[inst].info.subif_platform_set(inst,
								       port_id,
								       i,
								       &platfrm_data,
								       flags)) {
				dev_err(dp_dev, "subif_platform_set fail\n");
				goto EXIT;
			} else {
				DP_DEBUG(DP_DBG_FLAG_REG,
					 "subif_platform_set succeed\n");
			}
		} else {
			dev_err(dp_dev, "port info status fail for 0\n");
			return res;
		}

		cqm_deq_port = port_info->subif_info[i].cqm_deq_port;
		dma_chan = dp_deq_port_tbl[inst][cqm_deq_port].dma_chan;
		cid = _DMA_CONTROLLER(dma_chan);
		pid = _DMA_PORT(dma_chan);
		nid = _DMA_CHANNEL(dma_chan);
		/* cid, pid and nid bounds match the dp_dma_chan_tbl shape
		 * declared in datapath.h ([4][8][16]).
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

		DP_DEBUG(DP_DBG_FLAG_REG, "cid=%d pid=%d nid=%d\n",
			 cid, pid, nid);
		dp_dma_chan_tbl_info = &dp_dma_chan_tbl[inst][cid][pid][nid];
		(void)dp_dma_chan_tbl_info;

		port_info->subif_info[i].flags = 1;
		port_info->subif_info[i].netif = dev;
		port_info->port_id = port_id;

		if (subif_id->subif < 0) /*dynamic:shift bits as HW defined*/
			port_info->subif_info[i].subif =
				SET_VAP(i, port_info->vap_offset,
					port_info->vap_mask);
		else /*provided by caller since it is alerady shifted properly*/
			port_info->subif_info[i].subif = subif_id->subif;
		strncpy(port_info->subif_info[i].device_name,
			subif_name,
			sizeof(port_info->subif_info[i].device_name) - 1);
		port_info->subif_info[i].flags = PORT_SUBIF_REGISTERED;
		port_info->subif_info[i].subif_flag = flags;
		port_info->status = PORT_SUBIF_REGISTERED;
		subif_id->port_id = port_id;
		subif_id->subif = port_info->subif_info[i].subif;
		port_info->num_subif++;
		if ((port_info->num_subif == 1) ||
		    (platfrm_data.act & TRIGGER_CQE_DP_ENABLE)) {
			cbm_data.dp_inst = inst;
			cbm_data.num_dma_chan = port_info->num_dma_chan;
			cbm_data.cbm_inst = dp_port_prop[inst].cbm_inst;
			cbm_data.deq_port = port_info->deq_port_base +
					    (data ? data->deq_port_idx : 0);
			if ((cbm_data.deq_port == 0) ||
			    (cbm_data.deq_port >= DP_MAX_CQM_DEQ)) {
				dev_err(dp_dev, "Wrong deq_port: %d\n",
					cbm_data.deq_port);
				return res;
			}
			/* PPA Directpath/LitePath don't have DMA CH */
			if ((atomic_read(&dp_dma_chan_tbl[inst][cid][pid][nid].ref_cnt) == 1) &&
			    !(port_info->alloc_flags & DP_F_DIRECT) &&
			    (cbm_data.num_dma_chan))
				cbm_data.dma_chnl_init = 1; /*to enable DMA*/
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "cbm_dp_enable: dp_port=%d deq_port=%d dma_chnl_init=%d ref=%d\n",
				 port_id, cbm_data.deq_port,
				 cbm_data.dma_chnl_init,
				 atomic_read(&dp_dma_chan_tbl[inst][cid][pid][nid].ref_cnt));
			if (cbm_dp_enable(owner, port_id, &cbm_data, 0,
					  port_info->alloc_flags)) {
				DP_DEBUG(DP_DBG_FLAG_REG,
					 "cbm_dp_enable fail\n");
				return res;
			}
			DP_DEBUG(DP_DBG_FLAG_REG, "cbm_dp_enable ok\n");
		} else {
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "No need cbm_dp_enable:dp_port=%d subix=%d\n",
				 port_id, i);
		}
		break;
	}

	if (i < end) {
		res = DP_SUCCESS;
		if (dp_bp_dev_tbl[inst][port_info->subif_info[i].bp].ref_cnt > 1)
			return res;
		dp_inst_add_dev(dev, subif_name,
				subif_id->inst, subif_id->port_id,
				port_info->subif_info[i].bp,
				subif_id->subif, flags);
	} else {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "register subif failed for no matched vap\n");
	}
EXIT:
	return res;
}

/*
 * AVM datapath_api.c:664-781.
 *
 * AVM dependencies stripped or substituted: - del_logic_dev: AVM logic-dev
 * path (AVM:708) calls del_logic_dev to unwind add_logic_dev's bookkeeping.
 * Documented inline. dp_node_children_free: AVM:742 walks PPv4-managed queue
 * children associated with the deq_port.
 */
static int dp_deregister_subif_private(int inst, struct module *owner,
				       struct net_device *dev,
				       char *subif_name, dp_subif_t *subif_id,
				       struct dp_subif_data *data, u32 flags)
{
	int res = DP_FAILURE;
	int i, port_id, cqm_port, bp;
	u8 find = 0;
	struct pmac_port_info *port_info;
	struct cbm_dp_en_data cbm_data = { 0 };
	struct subif_platform_data platfrm_data = { 0 };

	port_id = subif_id->port_id;
	port_info = &dp_port_info[inst][port_id];
	platfrm_data.subif_data = data;
	platfrm_data.dev = dev;

	DP_DEBUG(DP_DBG_FLAG_REG,
		 "Try to unregister subif=%s with dp_port=%d subif=%d\n",
		 subif_name, subif_id->port_id, subif_id->subif);

	/* (1) AVM:686 — only deregister fully-registered subifs. */
	if (port_info->status != PORT_SUBIF_REGISTERED) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister failed:%s not registered subif!\n",
			 subif_name);
		return res;
	}

	/* (2) AVM:693 — find the matching subif slot. */
	for (i = 0; i < port_info->ctp_max; i++) {
		if (port_info->subif_info[i].subif ==
		    (u32)subif_id->subif) {
			find = 1;
			break;
		}
	}
	if (!find)
		return res;

	DP_DEBUG(DP_DBG_FLAG_REG,
		 "Found matched subif: port_id=%d subif=%x vap=%d\n",
		 subif_id->port_id, subif_id->subif, i);

	/* (3) AVM:706 — device-match guard. */
	if (port_info->subif_info[i].netif != dev) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister fail: dev mismatch (subif %s, expected dev %p, got %p)\n",
			 subif_name, port_info->subif_info[i].netif, dev);
		return res;
	}
	if (!list_empty(&port_info->subif_info[i].logic_dev)) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "Unregister fail: logic_dev of %s not empty yet!\n",
			 subif_name);
		return res;
	}

	cqm_port = port_info->subif_info[i].cqm_deq_port;
	bp = port_info->subif_info[i].bp;

	memset(&port_info->subif_info[i].mib, 0,
	       sizeof(port_info->subif_info[i].mib));
	port_info->subif_info[i].flags = 0;
	port_info->subif_info[i].netif = NULL;
	port_info->num_subif--;

	/* (5) AVM:725-730 — subif_platform_set unprograms CTP / BridgePort
	 * / PCE_QUEUE_MAP. Continue on failure (matches AVM's commented-out
	 * `return res;` at AVM:729 — AVM intentionally falls through so the
	 * remaining teardown still runs).
	 */
	if (dp_port_prop[inst].info.subif_platform_set(inst, port_id, i,
						       &platfrm_data, flags)) {
		dev_err(dp_dev, "subif_platform_set fail on deregister\n");
		/* fall through per AVM:729 commented-out return */
	}

	/*
	 * (6) Symmetric inverse of dp_register_subif_private's PORT_ALLOCATED
	 * -> PORT_SUBIF_REGISTERED transition. Dropping back to
	 * PORT_ALLOCATED here closes the state-machine loop so
	 * dp_dealloc_port_ext's DEREGISTER branch (which requires status ==
	 * PORT_ALLOCATED at dp_alloc_port_private's guard) passes.
	 */
	if (!port_info->num_subif)
		port_info->status = PORT_ALLOCATED;

	if (!dp_deq_port_tbl[inst][cqm_port].ref_cnt) {
		cbm_data.dp_inst = inst;
		cbm_data.cbm_inst = dp_port_prop[inst].cbm_inst;
		cbm_data.deq_port = cqm_port;
		/* PPA Directpath/LitePath don't have DMA CH */
		if (!(port_info->alloc_flags & DP_F_DIRECT))
			cbm_data.dma_chnl_init = 1; /* to disable DMA */
		if (cbm_dp_enable(owner, port_id, &cbm_data,
				  CBM_PORT_F_DISABLE,
				  port_info->alloc_flags)) {
			DP_DEBUG(DP_DBG_FLAG_REG,
				 "cbm_dp_disable fail:port=%d subix=%d dma_chnl_init=%d\n",
				 port_id, i, cbm_data.dma_chnl_init);
			return res;
		}
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "cbm_dp_disable ok:port=%d subix=%d cqm_port=%d\n",
			 port_id, i, cqm_port);
	}

	/* (8) AVM:768-774 — remove the device from the per-instance table
	 * when the bp ref drops to zero.
	 */
	if (!dp_bp_dev_tbl[inst][bp].dev) {
		DP_DEBUG(DP_DBG_FLAG_REG,
			 "dp_inst_del_dev for %s inst=%d bp=%d\n",
			 dev ? dev->name : "(null)", inst, bp);
		dp_inst_del_dev(dev, subif_name, inst, port_id,
				bp, subif_id->subif);
	}

	DP_DEBUG(DP_DBG_FLAG_REG, "  dp_port=%d subif=%d cqm_port=%d\n",
		 subif_id->port_id, subif_id->subif, cqm_port);
	res = DP_SUCCESS;
	return res;
}

/*
 * AVM does NOT carry a separate dp_dealloc_port_ext function — AVM uses
 * dp_alloc_port_ext with the DP_F_DEREGISTER flag bit to drive the dealloc
 * path inside dp_alloc_port_private (which already carries the `if (flags &
 * DP_F_DEREGISTER)` branch).
 *
 * (b) Per-call-site safety: passing DP_F_DEREGISTER through every
 * teardown-path call site invites a typo bug (a missing DP_F_DEREGISTER turns
 * dealloc into alloc). Wrapping the flag-set inside the function eliminates
 * that footgun class.
 *
 * Returns DP_SUCCESS / DP_FAILURE per the same convention as
 * dp_alloc_port_ext — DP_F_DEREGISTER's branch inside
 * dp_alloc_port_private returns DP_SUCCESS on the happy path
 * (datapath_api.c body documented above) and DP_FAILURE on any
 * intermediate error.
 *
 * The function exists at file scope (NOT static) because intel_xrx500.c
 * in the sibling intel-xrx500-gswip composite calls it from ndo_stop.
 * Exported in the DATAPATH_INTERNAL namespace alongside dp_alloc_port_ext.
 */
int dp_dealloc_port_ext(int inst, struct module *owner,
			struct net_device *dev,
			u32 dev_port, int port_id,
			dp_pmac_cfg_t *pmac_cfg,
			struct dp_port_data *data, u32 flags)
{
	/*
	 * Route through dp_alloc_port_ext with DP_F_DEREGISTER OR'd into
	 * flags. dp_alloc_port_ext acquires DP_LIB_LOCK(&dp_lock), validates
	 * dp_init_ok, then dispatches into dp_alloc_port_private whose
	 * `if (flags & DP_F_DEREGISTER)` branch frees the per-port state.
	 *
	 *   (1) Guard: requires `dp_port_info[inst][port_id].status ==
	 *       PORT_ALLOCATED`. dp_deregister_subif_private's step (6) is
	 *       responsible for dropping status back to PORT_ALLOCATED so
	 *       this guard passes; without that transition this function
	 *       returns DP_FAILURE on every call.
	 *   (2) cbm_dp_port_dealloc(owner, dev_port, port_id,
	 *       &cbm_data, flags) — release CBM-side port resources.
	 *   (3) dp_inst_insert_mod(owner, port_id, inst, 0) — clear the
	 *       per-port instance mapping.
	 *   (4) DP_CB(inst, port_platform_set)(inst, port_id, data, flags)
	 *       with DP_F_DEREGISTER set — unprogram per-port HW state.
	 *   (5) memset(&dp_port_info[inst][port_id], 0, ...) — zero the
	 *       per-port bookkeeping (status -> PORT_FREE, owner -> NULL,
	 *       dev -> NULL, etc.) so a future dp_alloc_port_ext can rebind
	 *       the port.
	 *
	 * COUPLING CONTRACT: any future widening of dp_alloc_port_private's
	 * non-DEREGISTER branch that affects shared state (the
	 * dp_dma_chan_tbl entries, dp_deq_port_tbl ref_cnts, etc.) MUST be
	 * matched by a corresponding undo in the DEREGISTER branch or
	 * dp_dealloc_port_ext will silently leak state across ndo_stop ->
	 * ndo_open cycles. The current AVM-faithful body covers the five
	 * steps above and is correct.
	 *
	 * Several parameters of dp_dealloc_port_ext are no-ops in the
	 * DEREGISTER path: @pmac_cfg is not consulted (line-excised
	 * dp_pmac_set), @dev_port is only used inside cbm_dp_port_dealloc's
	 * tracing, @dev is unused.
	 */
	return dp_alloc_port_ext(inst, owner, dev, dev_port, port_id,
				 pmac_cfg, data, flags | DP_F_DEREGISTER);
}

/*
 * The private wrappers (dp_alloc_port_private, dp_register_subif_private),
 * the probe-time entry (dp_init_module), and the cap/instance helpers
 * (register_dp_cap, request_dp) are intra-module callers only and are NOT
 * exported.
 *
 * The deregister-side private body (dp_deregister_subif_private) is NOT
 * exported: it is reached only via dp_register_subif_ext's DP_F_DEREGISTER
 * dispatch (which IS exported).
 */
EXPORT_SYMBOL_NS_GPL(dp_alloc_port, "DATAPATH_INTERNAL");
EXPORT_SYMBOL_NS_GPL(dp_alloc_port_ext, "DATAPATH_INTERNAL");
EXPORT_SYMBOL_NS_GPL(dp_dealloc_port_ext, "DATAPATH_INTERNAL");
EXPORT_SYMBOL_NS_GPL(dp_register_subif, "DATAPATH_INTERNAL");
EXPORT_SYMBOL_NS_GPL(dp_register_subif_ext, "DATAPATH_INTERNAL");
