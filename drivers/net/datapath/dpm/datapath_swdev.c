// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#include <linux/types.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/ethtool.h>
#include <net/rtnetlink.h>
#include <generated/utsrelease.h>
#include <net/datapath_api.h>
#include "datapath_swdev.h"
#include "datapath.h"
#include "datapath_instance.h"
#include "datapath_ioctl.h"

static void dp_swdev_insert_bridge_id_entry(struct br_info *);
static void dp_swdev_remove_bridge_id_entry(struct br_info *);
static int dp_swdev_add_bport_to_list(struct br_info *br_item,
				      int bport);
static int dp_swdev_del_bport_from_list(struct br_info *br_item,
					int bport, u16 *hairpin_port);

struct hlist_head
	g_bridge_id_entry_hash_table[DP_MAX_INST][BR_ID_ENTRY_HASH_TABLE_SIZE];

DP_DEFINE_LOCK(dp_swdev_lock);
static inline void swdev_lock(void)
{
	DP_LIB_LOCK(&dp_swdev_lock);
}

static inline void swdev_unlock(void)
{
	DP_LIB_UNLOCK(&dp_swdev_lock);
}

u16 dp_swdev_cal_hash(unsigned char *name)
{
	unsigned long hash = init_name_hash(0);

	if (name) {
		while (*name)
			hash = partial_name_hash(*name++, hash);
	}
	return hash_long(hash, BRIDGE_ID_ENTRY_HASH_LENGTH);
}

/* dp_kfree - used as destructor for
 * switchdev_trans_item_enqueue() API
 */
void dp_kfree(void const *data)
{
	devm_kfree(&g_dp_dev->dev, (void *)data);
}

struct hlist_head *get_dp_g_bridge_id_entry_hash_table_info(int instance,
							    int index)
{
	return &g_bridge_id_entry_hash_table[instance][index];
}
EXPORT_SYMBOL(get_dp_g_bridge_id_entry_hash_table_info);

int dp_get_fid_by_brname(struct net_device *dev, int *inst)
{
	struct br_info *br_info;

	if (unlikely(dp_late_init() <= 0)) {
		pr_err("%s failed: datapath not initialized yet\n", __func__);
		return DP_FAILURE;
	}

	if (!dev) {
		pr_err("%s failed: dev can not be NULL\n", __func__);
		return DP_FAILURE;
	}

	br_info = dp_swdev_bridge_entry_lookup(dev->name);
	if (!br_info)
		return -1;

	*inst = br_info->inst;
	return br_info->fid;
}
EXPORT_SYMBOL(dp_get_fid_by_brname);

int dp_swdev_chk_bport_in_br(struct net_device *bp_dev, int bport, int inst)
{
	struct net_device *br_dev;
	struct bridge_member_port *temp_list = NULL;
	struct br_info *br_info;
	int found = 0;

	br_dev = netdev_master_upper_dev_get(bp_dev);
	if (!br_dev)
		return -1;
	br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
	if (!br_info)
		return -1;
	list_for_each_entry (temp_list, &br_info->bp_list, list) {
		if (temp_list->portid == bport) {
			found = 1;
			DP_DEBUG(DP_DBG_FLAG_SWDEV, "bport(%s) in bridge\n",
				 bp_dev->name ? bp_dev->name : "NULL");
			return 0;
		}
	}
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "bport(%s) not in bridge\n",
		 bp_dev->name ? bp_dev->name : "NULL");
	return -1;
}

struct br_info *dp_swdev_bridge_entry_lookup(char *br_name)
{
	u16 idx;
	struct br_info *br_item = NULL;
	struct hlist_head *tmp;
	int i = 0;

	idx = dp_swdev_cal_hash(br_name);
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "hash index:%d\n", idx);
	for (i = 0; i < DP_MAX_INST; i++) {
		tmp = (&g_bridge_id_entry_hash_table[i][idx]);
		hlist_for_each_entry (br_item, tmp, br_hlist) {
			if (br_item) {
				if (strcmp(br_name,
					   br_item->br_device_name) == 0) {
					DP_DEBUG(DP_DBG_FLAG_SWDEV,
						 "hash entry found(%s)\n",
						 br_name);
					return br_item;
				}
			} else {
				break;
			}
		}
	}
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "No hash entry found(%s)\n",
		 br_name);
	return NULL;
}

static void dp_swdev_remove_bridge_id_entry(struct br_info *br_item)
{
	/*reset switch bridge configurations*/
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "hash del\n");
	hlist_del(&br_item->br_hlist);
	devm_kfree(&g_dp_dev->dev, br_item);
}

static void dp_swdev_insert_bridge_id_entry(struct br_info
						    *br_item)
{
	u16 idx;
	struct hlist_head *tmp;

	idx = dp_swdev_cal_hash(br_item->br_device_name);
	tmp = (&g_bridge_id_entry_hash_table[br_item->inst][idx]);
	hlist_add_head(&br_item->br_hlist, tmp);
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "hash added idx:%d bridge(%s)\n",
		 idx, br_item->br_device_name);
}

int dp_swdev_bridge_id_entry_free(int instance)
{
	u32 index;
	struct hlist_node *tmp;
	struct br_info *br_item;

	for (index = 0; index < BR_ID_ENTRY_HASH_TABLE_SIZE;
	     index++) {
		hlist_for_each_entry_safe (br_item, tmp,
					   &g_bridge_id_entry_hash_table[instance][index],
					   br_hlist) {
			dp_swdev_remove_bridge_id_entry(br_item);
		}
	}
	return 0;
}

int dp_swdev_bridge_id_entry_init(void)
{
	int i, j;

	for (i = 0; i < DP_MAX_INST; i++)
		for (j = 0; j < BR_ID_ENTRY_HASH_TABLE_SIZE; j++)
			INIT_HLIST_HEAD(&g_bridge_id_entry_hash_table[i][j]);

	return 0; /*<success>*/
}

static int dp_swdev_del_bport_from_list(struct br_info *br_item,
					int bport, u16 *hairpin_bport)
{
	int found = 0;
	struct bridge_member_port *temp_list = NULL;

	list_for_each_entry (temp_list, &br_item->bp_list, list) {
		if (temp_list->portid == bport) {
			found = 1;
			*hairpin_bport = temp_list->hairpin;
			break;
		}
	}
	if (found) {
		list_del(&temp_list->list);
		devm_kfree(&g_dp_dev->dev, temp_list);
		return 1;
	}
	return 0;
}

static int dp_swdev_add_bport_to_list(struct br_info *br_item,
				      int bport)
{
	int found = 0;
	struct bridge_member_port *bport_list = NULL;
	struct bridge_member_port *temp_list = NULL;

	list_for_each_entry (temp_list, &br_item->bp_list, list) {
		if (temp_list->portid == bport) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		bport_list = (struct bridge_member_port *)
			devm_kzalloc(&g_dp_dev->dev,
				     sizeof(struct bridge_member_port),
				     GFP_ATOMIC);
		if (!bport_list) {
			pr_err("\n Node creation failed\n");
			return -1;
		}
		bport_list->dev_reg_flag = br_item->flag;
		bport_list->portid = bport;
		bport_list->no_loop_flag = br_item->no_loop_flag;
		bport_list->hairpin = br_item->hairpin;
		bport_list->dev_priv_flag = br_item->bp_priv_flag;
		bport_list->dp_port = br_item->dp_port;
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "bport:%d reg_flag:%d priv_flag:0x%x\n",
			 bport_list->portid, bport_list->dev_reg_flag,
			 bport_list->dev_priv_flag);
		list_add(&bport_list->list, &br_item->bp_list);
	}
	return 0;
}

static int dp_swdev_clr_gswip_cfg(struct bridge_id_entry_item *br_item,
				  u8 *addr)
{
	struct br_info *br_info;
	struct inst_info *i_info;
	u16 hairpin = 0;
	int ret;

	if (br_item->flags == BRIDGE_NO_ACTION) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "bport not added so no action required\n");
		return 0;
	}
	br_info = dp_swdev_bridge_entry_lookup(br_item->br_device_name);
	if (!br_info)
		return 0;
	if (!dp_swdev_del_bport_from_list(br_info, br_item->portid, &hairpin))
		return 0;
	i_info = &dp_port_prop[br_item->inst].info;
	ret = i_info->swdev_bridge_port_cfg_reset(br_info, br_item->inst,
						  br_item->portid, hairpin);
	if (ret == DEL_BRENTRY) {
		i_info->swdev_free_brcfg(br_item->inst, br_item->fid);
		dp_swdev_remove_bridge_id_entry(br_info);
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "last bport(%d) to remove from bridge(%s)\n",
			 br_item->portid, br_item->br_device_name);
	}
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "rem bport(%d)\n", br_item->portid);
	return 0;
}

static int dp_swdev_cfg_vlan(struct bridge_id_entry_item *br_item,
			     struct net_device *dev)
{
	struct dp_dev *dp_dev;
	u32 idx, inst;
	int vap;
	struct inst_info *i_info;

	if (br_item->flags & LOGIC_DEV_REGISTER) {
		idx = dp_dev_hash(dev, NULL);
		dp_dev = dp_dev_lookup(&dp_dev_list[idx], dev, NULL, 0);
		if (!dp_dev) {
			pr_err("\n dp_dev NULL\n");
			/* Cannot return -1 from here as this fn is
			 * called by swdev commit phase
			 */
			return 0;
		}
		inst = br_item->inst;
		i_info = get_dp_prop_info(inst);
		vap = GET_VAP(dp_dev->ctp,
			      get_dp_port_info(inst, dp_dev->ep)->vap_offset,
			      get_dp_port_info(inst, dp_dev->ep)->vap_mask);
		DP_DEBUG(DP_DBG_FLAG_SWDEV, "vap=%d ep=%d bp=%d\n",
			 vap, dp_dev->ep, dp_dev->bp);
		i_info->dp_cfg_vlan(br_item->inst, vap, dp_dev->ep);
	}
	return 0;
}

static int dp_swdev_filter_vlan(struct net_device *dev,
				const struct switchdev_obj *obj,
				struct switchdev_trans *trans,
				struct net_device *br_dev)
{
	struct br_info *br_info;
	struct bridge_id_entry_item *br_item;
	dp_subif_t subif = { 0 };

	if (switchdev_trans_ph_prepare(trans)) {
		/*Get current BPORT ID,instance from DP*/
		if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
			pr_err("%s dp_get_netif_subifid failed for %s\n",
			       __func__, dev->name);
			return -EOPNOTSUPP;
		}
		br_item = devm_kzalloc(&g_dp_dev->dev, sizeof(*br_item),
				       GFP_ATOMIC);
		if (!br_item)
			//TODO need to check dequeue if no memory
			return -ENOMEM;
		br_item->inst = subif.inst;
		/* current bridge member port*/
		br_item->portid = subif.bport;
		swdev_lock();
		br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
		if (br_info) {
			strcpy(br_item->br_device_name,
			       br_info->br_device_name);
			br_item->fid = br_info->fid;
		} else {
			swdev_unlock();
			devm_kfree(&g_dp_dev->dev, br_item);
			return -EOPNOTSUPP;
		}
		switchdev_trans_item_enqueue(trans, br_item,
					     dp_kfree, &br_item->tritem);
		swdev_unlock();
		return 0;
	}
	swdev_lock();
	br_item = switchdev_trans_item_dequeue(trans);
	if (br_item)
		dp_swdev_cfg_vlan(br_item, dev);
	swdev_unlock();
	return 0;
}

static int dp_swdev_cfg_gswip(struct bridge_id_entry_item *br_item,
			      struct net_device *dev)
{
	struct br_info *br_info;
	struct inst_info *i_info;
	int ret;

	i_info = get_dp_prop_info(br_item->inst);
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "britem flags:%x\n", br_item->flags);

	if (br_item->flags & ADD_BRENTRY) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV, "Add br entry %s\n",
			 br_item->br_device_name);

		ret = i_info->swdev_bridge_cfg_set(br_item->inst, br_item->fid);
		if (ret == 0) {
			br_info = devm_kzalloc(&g_dp_dev->dev,
					       sizeof(*br_info), GFP_ATOMIC);
			if (!br_info)
				/*TODO need to check return value
				 *for switchdev commit
				 */
				return 0;
			br_info->fid = br_item->fid;
			br_info->inst = br_item->inst;
			br_info->no_loop_flag = br_item->no_loop_flag;
			br_info->hairpin = br_item->hairpin;
			br_info->bp_priv_flag = br_item->priv_flag;
			br_info->dp_port = br_item->dp_port;

			/* Logic dev flag added to verify if SWDEV registered
			 * the logical i.e. VLAN device.Helpful during
			 * br/bport delete
			 */
			if (br_item->flags & LOGIC_DEV_REGISTER)
				br_info->flag = LOGIC_DEV_REGISTER;

			strcpy(br_info->br_device_name,
			       br_item->br_device_name);
			INIT_LIST_HEAD(&br_info->bp_list);
			dp_swdev_insert_bridge_id_entry(br_info);
			dp_swdev_add_bport_to_list(br_info, br_item->portid);

			i_info->swdev_bridge_port_cfg_set(br_info,
							  br_item->inst,
							  br_item->portid,
							  dev->priv_flags);
			br_item->flags &= ~ADD_BRENTRY;
			DP_DEBUG(DP_DBG_FLAG_SWDEV,
				 "added bport(%d),bridge(%s)\n",
				 br_item->portid,
				 br_info->br_device_name);
			return 0;
		}
	} else {
		br_info = dp_swdev_bridge_entry_lookup(br_item->br_device_name);
		if (!br_info)
			return 0;
		br_info->flag = 0;
		br_info->no_loop_flag = br_item->no_loop_flag;
		br_info->hairpin = br_item->hairpin;
		br_info->bp_priv_flag = br_item->priv_flag;
		br_info->dp_port = br_item->dp_port;

		if (br_item->flags & LOGIC_DEV_REGISTER)
			br_info->flag = LOGIC_DEV_REGISTER;
		dp_swdev_add_bport_to_list(br_info, br_item->portid);
		i_info->swdev_bridge_port_cfg_set(br_info, br_item->inst,
						  br_item->portid,
						  dev->priv_flags);
		DP_DEBUG(DP_DBG_FLAG_SWDEV, "added bport(%d)\n",
			 br_item->portid);
		return 0;
	}
	DP_DEBUG(DP_DBG_FLAG_SWDEV, "No configuration,Pls check!!\n");
	return 0;
}

static int dp_swdev_add_if(struct net_device *dev,
			   const struct switchdev_attr *attr,
			   struct switchdev_trans *trans,
			   struct net_device *br_dev)
{
	struct br_info *br_info;
	struct pmac_port_info *p_info;
	struct inst_info *i_info;
	struct bridge_id_entry_item *br_item;
	int br_id = 0;
	struct net_device *base;
	dp_subif_t subif = { 0 };
	u32 flag = 0, vap;
	int port, inst;
	s32 res;
	struct dp_subif_info *sif;

	/*Get current BPORT ID,instance from DP*/
	res = dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0);
	if (res) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "get subifid failed for %s, chk if vlan dev\n",
			 dev->name);
		/*Check bridge port exists otherwise register
		 *device with datapath  i.e. only in case of new
		 *VLAN interface
		 */
		/*Check if dev is a VLAN device */
		if (!is_vlan_dev(dev))
			return -EOPNOTSUPP;
		base = get_base_dev(dev, -1);
		DP_DEBUG(DP_DBG_FLAG_SWDEV, "base dev name:%s\n",
			 base ? base->name : "NULL");
		if (!base)
			base = dev;
		res = dp_get_netif_subifid(base, NULL, NULL, NULL,
					   &subif, 0);
		if (res) {
			pr_err("dp_get_netif_subifid fail:%s\n",
			       base->name);
			return -EOPNOTSUPP;
		}
		port = subif.port_id;
		inst = subif.inst;
		subif.subif = -1;
		if (dp_register_subif(
			    get_dp_port_info(inst, port)->owner,
			    dev, dev->name, &subif,
			    DP_F_SUBIF_LOGICAL)) {
			pr_err("dp_register_subif fail: %s\n",
			       dev->name);
			return -EOPNOTSUPP;
		}
		flag = LOGIC_DEV_REGISTER;
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "registered subif,bp=%d port=%d\n",
			 subif.bport, subif.port_id);
	}
	p_info = get_dp_port_info(subif.inst, subif.port_id);
	i_info = &dp_port_prop[subif.inst].info;
	vap = GET_VAP(subif.subif, p_info->vap_offset,
		      p_info->vap_mask);
	sif = get_dp_port_subif(p_info, vap);
	/* SWITCHDEV_TRANS_PREPARE phase */
	if (switchdev_trans_ph_prepare(trans)) {
		br_item = devm_kzalloc(&g_dp_dev->dev, sizeof(*br_item),
				       GFP_ATOMIC);
		if (!br_item)
			/*TODO need to check dequeue if no memory*/
			return -ENOMEM;
		br_item->inst = subif.inst;
		/* current bridge member port*/
		br_item->portid = subif.bport;
		br_item->dp_port = subif.port_id;

		/* Alloc Flag is needed by HAL layer to see the interface is
		 * LAN or GPON or EPON
		 */
		br_item->priv_flag = dev->priv_flags;

		swdev_lock();
		br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
		br_item->no_loop_flag = p_info->loop_dis;
		br_item->hairpin = p_info->bp_hairpin_cap;
		if (br_info) {
			strcpy(br_item->br_device_name,
			       br_info->br_device_name);
			br_item->fid = br_info->fid;
			br_item->flags = flag;
		} else {
			br_item->flags = ADD_BRENTRY | flag;
			if (sif->swdev_en == 1) {
				br_id = i_info->swdev_alloc_bridge_id(
					br_item->inst);
				if (br_id) {
					/* Store bridge information
					 * to add in the table.
					 * This info is used during
					 * switchdev commit phase
					 */
					strcpy(br_item->br_device_name,
					       br_dev->name);
					br_item->fid = br_id;
				} else {
					pr_err("Switch config failed\r\n");
					devm_kfree(&g_dp_dev->dev, br_item);
					swdev_unlock();
					return -EOPNOTSUPP;
				}
			} else {
				strcpy(br_item->br_device_name, br_dev->name);
				br_item->fid = br_id;
			}
		}
		switchdev_trans_item_enqueue(trans, br_item,
					     dp_kfree, &br_item->tritem);
		swdev_unlock();
		return 0;
	}
	/*configure switch in commit phase and it cannot return failure*/
	swdev_lock();
	br_item = switchdev_trans_item_dequeue(trans);
	if (br_item) {
		if (sif->swdev_en == 1)
			dp_swdev_cfg_gswip(br_item, dev);
		if (br_item->flags & LOGIC_DEV_REGISTER)
			/*do only for vlan flag*/
			dp_swdev_cfg_vlan(br_item, dev);
	}
	swdev_unlock();
	return 0;
}

static int dp_swdev_del_if(struct net_device *dev,
			   const struct switchdev_attr *attr,
			   struct switchdev_trans *trans,
			   struct net_device *br_dev)
{
	struct br_info *br_info;
	struct pmac_port_info *p_info;
	struct bridge_id_entry_item *br_item;
	struct net_device *base;
	struct bridge_member_port *temp_list = NULL;
	dp_subif_t subif = { 0 };
	int vap = 0;
	int port, inst;
	u8 *addr = (u8 *)dev->dev_addr;
	struct dp_subif_info *sif;

	/*Get current BR_PORT ID from DP*/
	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "dp_get_netif_subifid failed for %s\n",
			 dev->name);
		/*	if (!is_vlan_dev(dev))*/
		return -EINVAL;
	}
	p_info = get_dp_port_info(subif.inst, subif.port_id);
	vap = GET_VAP(subif.subif, p_info->vap_offset,
		      p_info->vap_mask);
	sif = get_dp_port_subif(p_info, vap);
	/* SWITCHDEV_TRANS_PREPARE phase */
	if (switchdev_trans_ph_prepare(trans)) {
		br_item = devm_kzalloc(&g_dp_dev->dev,
				       sizeof(*br_item), GFP_ATOMIC);
		if (!br_item)
			/*TODO need to check dequeue if no memory*/
			return -ENOMEM;
		swdev_lock();
		br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
		if (br_info) {
			br_item->fid = br_info->fid;
			br_item->inst = subif.inst;
			/* current bridge member port*/
			br_item->portid = subif.bport;
			br_item->dp_port = subif.port_id;
			br_item->flags = BRIDGE_NO_ACTION;
			strcpy(br_item->br_device_name,
			       br_info->br_device_name);

			list_for_each_entry (temp_list, &br_info->bp_list,
					     list) {
				if (temp_list->portid == br_item->portid) {
					br_item->flags =
						temp_list->dev_reg_flag;
					break;
				}
			}
		} else {
			DP_DEBUG(DP_DBG_FLAG_SWDEV,
				 "No bridge entry or bport to delete\r\n");
			br_item->flags = BRIDGE_NO_ACTION;
		}
		/*TODO check return value & enqueue*/
		switchdev_trans_item_enqueue(trans, br_item, dp_kfree,
					     &br_item->tritem);
		swdev_unlock();
		return 0;
	}
	/*Check bridge port exists otherwise register device with datapath
	 *i.e. only in case of new VLAN interface
	 */
	/*configure switch in commit phase and it cannot return failure*/
	swdev_lock();
	br_item = switchdev_trans_item_dequeue(trans);
	if (br_item) {
		if (sif->swdev_en == 1)
			dp_swdev_clr_gswip_cfg(br_item, addr);
		/* De-Register Logical Dev i.e. VLAN DEV
		 * if it is registered
		 */
		if (br_item->flags & LOGIC_DEV_REGISTER) {
			base = get_base_dev(dev, -1);
			DP_DEBUG(DP_DBG_FLAG_SWDEV,
				 "base dev name:%s\n",
				 base ? base->name : "NULL");
			if (!base)
				base = dev;
			/*the previous sequence was running into a deadlock in
			 * taking the swdev_lock
			 */
			dp_swdev_cfg_vlan(br_item, dev);
			swdev_unlock();
			if (dp_get_netif_subifid(base, NULL, NULL,
						 NULL, &subif, 0)) {
				pr_err("dp_get_netif_subifid fail:%s\n",
				       base->name);
				/*Cannot Return -EOPNOTSUPP
				 * in swdev commit stage
				 */
				return 0;
			}
			port = subif.port_id;
			inst = subif.inst;
			if (dp_register_subif(
				    get_dp_port_info(inst, port)->owner,
				    dev, dev->name, &subif,
				    DP_F_DEREGISTER)) {
				pr_err("dp_register_subif fail: %s\n",
				       dev->name);
				/* Cannot Return -EOPNOTSUPP
				 * in swdev commit stage
				 */
				return 0;
			}
			swdev_lock();
		}
	}
	swdev_unlock();
	return 0;
}

int dp_del_br_if(struct net_device *dev, struct net_device *br_dev,
		 int inst, int bport)
{
	struct br_info *br_info;
	struct bridge_id_entry_item *br_item;
	struct bridge_member_port *temp_list = NULL;
	u8 *addr = (u8 *)dev->dev_addr;

	br_item = devm_kzalloc(&g_dp_dev->dev, sizeof(*br_item), GFP_ATOMIC);
	if (!br_item)
		return -1;
	swdev_lock();
	br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
	if (br_info) {
		br_item->fid = br_info->fid;
		br_item->inst = inst;
		br_item->portid = bport;
		strcpy(br_item->br_device_name, br_info->br_device_name);

		list_for_each_entry (temp_list, &br_info->bp_list, list) {
			if (temp_list->portid == br_item->portid) {
				br_item->flags =
					temp_list->dev_reg_flag;
				break;
			}
		}
	} else {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "No bridge entry or bport to delete\r\n");
		br_item->flags = BRIDGE_NO_ACTION;
	}

	if (br_item)
		dp_swdev_clr_gswip_cfg(br_item, addr);
	swdev_unlock();
	devm_kfree(&g_dp_dev->dev, br_item);
	return 0;
}

static int dp_swdev_port_attr_bridge_flags_set(struct net_device *dev,
					       unsigned long flags,
					       struct switchdev_trans *trans)
{
	dp_subif_t subif = { 0 };
	struct inst_info *i_info;
	int err = 0;

	if (switchdev_trans_ph_prepare(trans))
		return 0;

	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "dp_get_netif_subifid failed for %s\n",
			 dev->name);
		return -EINVAL;
	}

	i_info = &dp_port_prop[subif.inst].info;
	if (!i_info->swdev_bridge_port_flags_set)
		return -EOPNOTSUPP;

	err = i_info->swdev_bridge_port_flags_set(subif.inst,
						  subif.bport,
						  flags);
	return err;
}

static int dp_swdev_port_attr_bridge_flags_get(struct net_device *dev,
					       unsigned long *flags)
{
	dp_subif_t subif = { 0 };
	struct inst_info *i_info;
	int err = 0;

	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "dp_get_netif_subifid failed for %s\n",
			 dev->name);
		return -EINVAL;
	}

	i_info = &dp_port_prop[subif.inst].info;
	if (!i_info->swdev_bridge_port_flags_get)
		return -EOPNOTSUPP;

	err = i_info->swdev_bridge_port_flags_get(subif.inst,
						  subif.bport,
						  flags);
	return err;
}

static int dp_swdev_port_attr_learning_limit_set(struct net_device *dev,
						 int learning_limit,
						 struct switchdev_trans *trans)
{
	dp_subif_t subif = { 0 };
	struct inst_info *i_info;
	int err = 0;

	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "dp_get_netif_subifid failed for %s\n",
			 dev->name);
		return -EINVAL;
	}

	i_info = &dp_port_prop[subif.inst].info;
	if (!i_info->swdev_port_learning_limit_set)
		return -EOPNOTSUPP;

	/* The maximum limited supported by the GSWIP 3.1 is 254 entries */
	if (learning_limit > i_info->cap.max_num_learn_limit_port) {
		netdev_err(dev, "mac learning limit (%i) too high, max: %i\n",
			   learning_limit,
			   i_info->cap.max_num_learn_limit_port);
		return -EINVAL;
	}

	/* If we are in preperation do not execute the command */
	if (switchdev_trans_ph_prepare(trans))
		return 0;

	err = i_info->swdev_port_learning_limit_set(subif.inst,
						    subif.bport,
						    learning_limit);
	return err;
}

static int dp_swdev_port_attr_set(struct net_device *dev,
				  const struct switchdev_attr *attr,
				  struct switchdev_trans *trans)
{
	int err = -EOPNOTSUPP;
	struct net_device *br_dev;
	/* switchdev attr orig dev -> bridge port dev pointer
	 * then get the bridge dev from switchdev attr's orig dev
	 */
	br_dev = netdev_master_upper_dev_get(attr->orig_dev);
	if (!br_dev)
		return -EOPNOTSUPP;
	switch (attr->id) {
	case SWITCHDEV_ATTR_ID_PORT_STP_STATE:
		/* STP STATE forwading or ifconfig UP - add bridge*/
		if (attr->u.stp_state == BR_STATE_FORWARDING) {
			err = dp_swdev_add_if(attr->orig_dev,
					      attr, trans, br_dev);
		}
		/* STP STATE disabled or ifconfig DOWN - del bridge*/
		else if (attr->u.stp_state == BR_STATE_DISABLED) {
			err = dp_swdev_del_if(attr->orig_dev,
					      attr, trans, br_dev);
			if (err != 0)
				err = -EOPNOTSUPP;
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case SWITCHDEV_ATTR_ID_PORT_BRIDGE_FLAGS:
		err = dp_swdev_port_attr_bridge_flags_set(dev,
							  attr->u.brport_flags,
							  trans);
		break;
	case SWITCHDEV_ATTR_ID_BRIDGE_AGEING_TIME:
		/* err = dp_swdev_port_attr_bridge_ageing_time_set(dp_swdev_port,
	 * attr->u.ageing_time,
	 * trans);
	 */
		break;
	case SWITCHDEV_ATTR_ID_BRIDGE_VLAN_FILTERING:
		/* err = dp_swdev_port_attr_bridge_br_vlan_set(dev,
		 * attr->orig_dev,trans);
		 */
	case SWITCHDEV_ATTR_ID_PORT_LEARNING_LIMIT:
		err =
			dp_swdev_port_attr_learning_limit_set(dev,
							      attr->u.learning_limit,
							      trans);
	default:
		err = -EOPNOTSUPP;
		break;
	}
	return err;
}

static int dp_swdev_port_attr_get(struct net_device *dev,
				  struct switchdev_attr *attr)
{
	struct net_device *br_dev;
	struct br_info *br_info;
	dp_subif_t subif = { 0 };
	int err = 0;
	/*For this api default err return value "-EOPNOTSUPP"
	 * cannot be set as this blocks bridgeport offload_fwd_mark
	 * setting at linux bridge level("nbp_switchdev_mark_set")
	 */
	br_dev = netdev_master_upper_dev_get(attr->orig_dev);
	if (!br_dev)
		return 0;

	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "%s dp_get_netif_subifid failed for %s\n",
			 __func__, dev->name);
		return 0;
	}

	switch (attr->id) {
	case SWITCHDEV_ATTR_ID_PORT_PARENT_ID:
		br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
		if (!br_info)
			return 0;
		if (br_info->fid < 0)
			return -EOPNOTSUPP;
		attr->u.ppid.id_len = sizeof(br_info->fid);
		memcpy(&attr->u.ppid.id, &br_info->fid, attr->u.ppid.id_len);
		DP_DEBUG(DP_DBG_FLAG_SWDEV,
			 "SWITCHDEV_ATTR_ID_PORT_PARENT_ID:%s fid=%d\n",
			 attr->orig_dev ? attr->orig_dev->name : "NULL",
			 br_info->fid);
		//err = 0;
		break;
	case SWITCHDEV_ATTR_ID_PORT_BRIDGE_FLAGS:
		err =
			dp_swdev_port_attr_bridge_flags_get(dev,
							    &attr->u.brport_flags);
		break;
	default:
		return 0;
	}
	return err;
}

static int dp_swdev_port_obj_add(struct net_device *dev,
				 const struct switchdev_obj *obj,
				 struct switchdev_trans *trans)
{
	int err = -EOPNOTSUPP;
	struct net_device *br_dev;

	DP_DEBUG(DP_DBG_FLAG_SWDEV, "%s id:%d flags:%d dev name:%s\r\n",
		 __func__, obj->id, obj->flags, dev->name);
	br_dev = netdev_master_upper_dev_get(obj->orig_dev);
	if (!br_dev)
		return err;
	switch (obj->id) {
	case SWITCHDEV_OBJ_ID_PORT_VLAN:
		err = dp_swdev_filter_vlan(obj->orig_dev, obj, trans, br_dev);
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}
	return err;
}

static int dp_swdev_port_obj_del(struct net_device *dev,
				 const struct switchdev_obj *obj)
{
	int err = -EOPNOTSUPP;
	return err; //TODO

	DP_DEBUG(DP_DBG_FLAG_SWDEV, "%s\r\n", __func__);
	switch (obj->id) {
	case SWITCHDEV_OBJ_ID_PORT_VLAN:
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}
	return err;
}

int dp_notif_br_alloc(struct net_device *br_dev)
{
	int br_id = 0;
	struct br_info *br_info;
	struct inst_info *dp_info = get_dp_prop_info(0);

	br_id = dp_info->swdev_alloc_bridge_id(0);
	if (br_id) {
		if (dp_info->swdev_bridge_cfg_set(0, br_id) == 0) {
			br_info = devm_kzalloc(&g_dp_dev->dev, sizeof(*br_info),
					       GFP_ATOMIC);
			if (!br_info)
				return -1;
			br_info->fid = br_id;
			br_info->inst = 0;
			br_info->flag = 0;
			strcpy(br_info->br_device_name, br_dev->name);
			INIT_LIST_HEAD(&br_info->bp_list);
			dp_swdev_insert_bridge_id_entry(br_info);
		} else {
			pr_err("Switch configuration failed\r\n");
			return -1;
		}
	} else {
		pr_err("Switch bridge alloc failed\r\n");
		return -1;
	}
	return br_id;
}

/* This function registers the created port in datapath to switchdev */
int dp_register_switchdev_ops(struct dp_dev *dp_dev,
			      struct net_device *dev)
{
	int err = DP_SUCCESS;
	struct inst_info *inst_info = get_dp_prop_info(dp_dev->inst);

	if (!dev) {
		pr_err("cannot support switchdev if dev is NULL\n");
		return -1;
	}
	if (inst_info->swdev_flag == 1) {
		if (!dev->netdev_ops) {
			pr_err("netdev_ops not defined\n");
			return -1;
		}
		/* switchdev ops register */
		err = dp_ops_set((void **)&dev->switchdev_ops,
				 offsetof(const struct switchdev_ops,
					  switchdev_port_attr_get),
				 sizeof(*dev->switchdev_ops),
				 (void **)&dp_dev->old_swdev_ops,
				 &dp_dev->new_swdev_ops,
				 &dp_swdev_port_attr_get);
		if (err)
			return DP_FAILURE;
		err = dp_ops_set((void **)&dev->switchdev_ops,
				 offsetof(const struct switchdev_ops,
					  switchdev_port_attr_set),
				 sizeof(*dev->switchdev_ops),
				 (void **)&dp_dev->old_swdev_ops,
				 &dp_dev->new_swdev_ops,
				 &dp_swdev_port_attr_set);
		if (err)
			return DP_FAILURE;
		err = dp_ops_set((void **)&dev->switchdev_ops,
				 offsetof(const struct switchdev_ops,
					  switchdev_port_obj_add),
				 sizeof(*dev->switchdev_ops),
				 (void **)&dp_dev->old_swdev_ops,
				 &dp_dev->new_swdev_ops,
				 &dp_swdev_port_obj_add);
		if (err)
			return DP_FAILURE;
		err = dp_ops_set((void **)&dev->switchdev_ops,
				 offsetof(const struct switchdev_ops,
					  switchdev_port_obj_del),
				 sizeof(*dev->switchdev_ops),
				 (void **)&dp_dev->old_swdev_ops,
				 &dp_dev->new_swdev_ops,
				 &dp_swdev_port_obj_del);
		if (err)
			return DP_FAILURE;
	}
	return 0;
}

void dp_switchdev_exit(void)
{
	int i;

	for (i = 0; i < DP_MAX_INST; i++)
		dp_swdev_bridge_id_entry_free(i);
}

int dp_switchdev_init(void)
{
	dp_swdev_bridge_id_entry_init();
	return 0;
}
