// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020 Intel Corporation.
 */

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/init.h> /* Needed for the macros */
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/string.h>

#include <net/datapath_api.h>
#include "../dpm/datapath.h"
#include "dp_mib_counters.h"
#include "../dpm/gswip31/datapath_misc.h"

/* Counters update interval = 3[min] -> 180'000[ms] */
#define CNT_UPDATE_TIME 180000

struct mib_counters_u64 {
#define _DP_ETHTOOL(x, y) u64 y;
	DP_ETHTOOL_STAT_REG_DECLARE
#undef _DP_ETHTOOL
};

/** Data set needed for maintain mib counters overflow for one net device */
struct mib_counters {
	/** devices list handle */
	struct list_head dev_list;
	/** lock for update the set */
	rwlock_t cnt_lock;
	/** net device handle */
	struct net_device *dev;
	/** inst */
	s32 inst;
	/** device id */
	s32 port_id;
	/** sub interface id group*/
	s32 sub_if_id_grp;
	/** last read counters */
	GSW_RMON_Port_cnt_t last_read_rmon;
	/** stored 64 bits wide counters */
	struct mib_counters_u64 wide_cntrs;
};

/* Timer for update counters for all registered net devices */
static struct timer_list cnt_upd_timer;

/* List of registered net devices */
static struct list_head registered_devices_list;

/* Devices list lock */
static rwlock_t dev_list_lock;

/* Notify callback registration ID (pointer) - used for deregister CB */
static void *g_notify_cb_id;

/* GSWIP "shortcut" mode flag - based on device tree */
static u8 shortcut_mode;

/* Wraparound counter handle */
static u64 wraparound(u64 curr, u64 last, u32 size)
{
#define WRAPAROUND_MAX_32 0xFFFFFFFF
	/* for 8 bytes(64bit mib),no need to do wraparound */
	if ((size > 4) || (curr >= last))
		return curr - last;
	return ((u64)WRAPAROUND_MAX_32) + curr - last;
}

/* Update counters for one device */
static void update_dev_counters(struct mib_counters *cnt)
{
	GSW_RMON_Port_cnt_t *tmp_counters;
	struct core_ops *ops;
	/* net device is a bridge port on UNI side */
	u8 dev_is_uni = 0;
	s32 ret = 0;
	u8 i = 0;

	tmp_counters = devm_kzalloc(&g_dp_dev->dev,
				    (2 * sizeof(GSW_RMON_Port_cnt_t)),
				    GFP_ATOMIC);
	if (!tmp_counters) {
		netdev_err(cnt->dev, "failed to get memory for counters");
		return;
	}

	ops = dp_port_prop[cnt->inst].ops[0];

	DP_DEBUG(DP_DBG_FLAG_MIB,
		 "MIB CNT - update counters for dev: %s, port_id: %d, sub_if_id_grp: %d\n",
		 cnt->dev->name, cnt->port_id, cnt->sub_if_id_grp);

	/* read current counters' values */
	for (i = 0; i < 2; i++) {
		tmp_counters[i].ePortType = GSW_CTP_PORT;
		tmp_counters[i].nPortId = cnt->port_id;
		tmp_counters[i].nSubIfIdGroup = cnt->sub_if_id_grp;
		tmp_counters[i].bPceBypass = i;

		/* read CTP counters */
		ret = ops->gsw_rmon_ops.RMON_Port_Get(ops, &tmp_counters[i]);
		if (ret != DP_SUCCESS) {
			netdev_err(cnt->dev,
				   "RMON_Port_Get returned %d\n", ret);
			devm_kfree(&g_dp_dev->dev, tmp_counters);
			return;
		}
	}

	/* Overwrite CTP Tx counters by CTP Tx Pce-Bypass counters.
	 * CTP Tx counters are placed before QoS modules, and they
	 * record packets dropped by QoS modules too.
	 * CTP Tx PCE-Bypass counters are placed after QoS modules,
	 * and they record the packets actually transmitted.
	 *
	 * Do not overwrite CTP Tx counters for an UNI interface
	 * in "shortcut" mode
	 */

	/* TODO: find out better way to check given dev is UNI bridge port */
	dev_is_uni = cnt->dev->name[0] == 'e';

	if (!(dev_is_uni && shortcut_mode)) {
#define _DP_ETHTOOL_TX(r) tmp_counters[0].r = tmp_counters[1].r;
		DP_ETHTOOL_STAT_REG_TX_DECLARE
#undef _DP_ETHTOOL_TX
	}
	/* update 64 bits wide counters based on last read values */
	write_lock(&cnt->cnt_lock);
#define _DP_ETHTOOL(x, y) cnt->wide_cntrs.y += wraparound(tmp_counters[0].y, cnt->last_read_rmon.y, sizeof(cnt->last_read_rmon.y));
	DP_ETHTOOL_STAT_REG_DECLARE
#undef _DP_ETHTOOL
	write_unlock(&cnt->cnt_lock);

	/* Update last read counters set */
	cnt->last_read_rmon = tmp_counters[0];

	devm_kfree(&g_dp_dev->dev, tmp_counters);
}

/* update counters for all registered devices */
static void update_all_devs_counters(void)
{
	struct mib_counters *mib_cnt;
	struct list_head *p;

	read_lock(&dev_list_lock);
	list_for_each (p, &registered_devices_list) {
		mib_cnt = list_entry(p, struct mib_counters, dev_list);
		update_dev_counters(mib_cnt);
	}
	read_unlock(&dev_list_lock);
}

/* Adds new net device to counter update list */
s32 add_dev_to_list(struct net_device *dev, s32 port_id, s32 sub_if, s32 inst)
{
	struct mib_counters *dev_cnt;
	struct pmac_port_info *port_info;

	dev_cnt = devm_kzalloc(&g_dp_dev->dev,
			       sizeof(struct mib_counters),
			       GFP_ATOMIC);
	if (!dev_cnt)
		return -ENOMEM;

	port_info = get_dp_port_info(inst, port_id);

	dev_cnt->inst = inst;
	dev_cnt->dev = dev;
	dev_cnt->port_id = port_id;
	dev_cnt->sub_if_id_grp = GET_VAP(sub_if, port_info->vap_offset,
					 port_info->vap_mask);
	rwlock_init(&dev_cnt->cnt_lock);

	INIT_LIST_HEAD(&dev_cnt->dev_list);

	write_lock(&dev_list_lock);
	list_add_tail(&dev_cnt->dev_list, &registered_devices_list);
	write_unlock(&dev_list_lock);

	DP_DEBUG(DP_DBG_FLAG_MIB,
		 "MIB CNT - added device: %s port_id = %d, sub_if_id_grp = %d\n",
		 dev->name, port_id, dev_cnt->sub_if_id_grp);

	return 0;
}

/* Looks for matched list item and remove it */
s32 rm_dev_from_list(struct net_device *dev, s32 port_id, s32 sub_if, s32 inst)
{
	struct pmac_port_info *port_info;
	struct mib_counters *mib_cnt;
	struct list_head *p, *q;
	s32 sub_if_id_grp;

	if ((!dev) || (port_id < 0) || (port_id > 12) || (sub_if < 0) || (sub_if > 512)) {
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - item to delete: name = %s, port_id = %d, sub_if = %d, inst = %d  - POSSIBLY WRONG DATA !\n",
			 dev->name, port_id, sub_if, inst);
		return -EINVAL;
	}

	port_info = get_dp_port_info(inst, port_id);

	DP_DEBUG(DP_DBG_FLAG_MIB,
		 "MIB CNT - looking for item to delete: name = %s, port_id = %d, sub_if = %d, inst = %d\n",
		 dev->name, port_id, sub_if, inst);

	sub_if_id_grp = GET_VAP(sub_if, port_info->vap_offset,
				port_info->vap_mask);

	write_lock(&dev_list_lock);
	list_for_each_safe (p, q, &registered_devices_list) {
		mib_cnt = list_entry(p, struct mib_counters, dev_list);

		if ((mib_cnt->port_id == port_id) &&
		    (mib_cnt->sub_if_id_grp == sub_if_id_grp) &&
		    (mib_cnt->inst == inst) &&
		    (!strcmp(mib_cnt->dev->name, dev->name))) {
			DP_DEBUG(DP_DBG_FLAG_MIB,
				 "MIB CNT - list item: name = %s, port_id = %d, sub_if_id_grp = %d, inst = %d - removed",
				 mib_cnt->dev->name,
				 mib_cnt->port_id,
				 mib_cnt->sub_if_id_grp,
				 mib_cnt->inst);
			list_del(p);
			devm_kfree(&g_dp_dev->dev, mib_cnt);
		} else {
			DP_DEBUG(DP_DBG_FLAG_MIB,
				 "MIB CNT - list item: name = %s, port_id = %d, sub_if_id_grp = %d, inst = %d - doesn't match",
				 mib_cnt->dev->name,
				 mib_cnt->port_id,
				 mib_cnt->sub_if_id_grp,
				 mib_cnt->inst);
		}
	}
	write_unlock(&dev_list_lock);

	return 0;
}

/* Function needed for ethtool to find on list appropriate wide counters set */
struct mib_counters *find_registered_dev(s32 port_id, s32 sub_if_id_grp,
					 s32 inst, struct net_device *dev)
{
	struct mib_counters *mib_cnt;
	struct list_head *p;

	list_for_each (p, &registered_devices_list) {
		mib_cnt = list_entry(p, struct mib_counters, dev_list);
		if ((mib_cnt->port_id == port_id) &&
		    (mib_cnt->sub_if_id_grp == sub_if_id_grp) &&
		    (mib_cnt->inst == inst) &&
		    (!strcmp(mib_cnt->dev->name, dev->name))) {
			return mib_cnt;
		}
	}

	return NULL;
}

/* Callback function to register new net device (subif) reported by DP API */
s32 subif_register_cb(struct dp_event_info *info)
{
	DP_DEBUG(DP_DBG_FLAG_MIB,
		 "MIB CNT - EVENT - SUBIF REGISTER CB called\n - event info type = %d\n",
		 info->type);

	switch (info->type) {
	case DP_EVENT_REGISTER_SUBIF:
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - event type: DP_EVENT_REGISTER_SUBIF\n");
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - info->reg_subif_info.dpid = %d\n",
			 info->reg_subif_info.dpid);
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - info->reg_subif_info.subif = %d\n",
			 info->reg_subif_info.subif);
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - info->reg_subif_info.dev->name = %s\n",
			 info->reg_subif_info.dev->name);

		/* add new net device to registered devices list */
		add_dev_to_list(info->reg_subif_info.dev,
				info->reg_subif_info.dpid,
				info->reg_subif_info.subif,
				info->inst);
		break;
	case DP_EVENT_DE_REGISTER_SUBIF:

		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - event type: DP_EVENT_DE_REGISTER_SUBIF\n");
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - info->de_reg_subif_info.dpid = %d\n",
			 info->de_reg_subif_info.dpid);
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - info->de_reg_subif_info.subif = %d\n",
			 info->de_reg_subif_info.subif);
		DP_DEBUG(DP_DBG_FLAG_MIB,
			 "MIB CNT - info->de_reg_subif_info.dev->name = %s\n",
			 info->de_reg_subif_info.dev->name);

		/* remove net device from registered devices list */
		rm_dev_from_list(info->de_reg_subif_info.dev,
				 info->de_reg_subif_info.dpid,
				 info->de_reg_subif_info.subif,
				 info->inst);
		break;
	default:
		return 0;
	}

	return 0;
}

/************************* ETHTOOL SUPPORT ************************/

void mib_cnt_get_ethtool_stats_31(struct net_device *dev,
				  struct ethtool_stats *stats, u64 *data)
{
	struct pmac_port_info *port_info;
	struct mib_counters *mib_cnt;
	struct mib_counters_u64 wide_cnt_mod;
	struct dp_reinsert_count dp_reins_count;
	dp_subif_t *ps_subif;
	u32 sub_if_id_grp;
	u64 *data_dst;
	u32 flags = 0;
	int ret;

	ps_subif = devm_kzalloc(&g_dp_dev->dev, (sizeof(dp_subif_t)),
				GFP_ATOMIC);
	if (!ps_subif)
		return;

	/* Get port_id and sub_if information from net device */
	ret = dp_get_netif_subifid(dev, NULL, NULL, NULL, ps_subif, flags);
	if (ret != DP_SUCCESS) {
		netdev_err(dev, "dp_get_netif_subifid returned %d\n", ret);
		devm_kfree(&g_dp_dev->dev, ps_subif);
		return;
	}

	port_info = get_dp_port_info(ps_subif->inst, ps_subif->port_id);

	sub_if_id_grp = GET_VAP(ps_subif->subif,
				port_info->vap_offset,
				port_info->vap_mask);

	/* Find required registered device on list */
	read_lock(&dev_list_lock);
	mib_cnt = find_registered_dev(ps_subif->port_id, sub_if_id_grp,
				      ps_subif->inst, dev);
	if (!mib_cnt) {
		devm_kfree(&g_dp_dev->dev, ps_subif);
		read_unlock(&dev_list_lock);
		return;
	}

	devm_kfree(&g_dp_dev->dev, ps_subif);

	/* Update counters for the requested net device */
	update_dev_counters(mib_cnt);

	/* Copy wide counters to update by reinserted packets */
	wide_cnt_mod = mib_cnt->wide_cntrs;

	read_unlock(&dev_list_lock);

	/* Get reinserted packets counters */
	ret = dp_get_reinsert_cnt_31(mib_cnt->inst,
				     mib_cnt->port_id,
				     mib_cnt->sub_if_id_grp,
				     0,
				     &dp_reins_count);
	if (ret != DP_SUCCESS) {
		netdev_err(mib_cnt->dev, "dp_get_reinsert_cnt_31 returned %d\n", ret);
		return;
	}

	/* Take away reinserted packets */
	wide_cnt_mod.nRx64BytePkts -= dp_reins_count.dp_64BytePkts;
	wide_cnt_mod.nRx127BytePkts -= dp_reins_count.dp_127BytePkts;
	wide_cnt_mod.nRx255BytePkts -= dp_reins_count.dp_255BytePkts;
	wide_cnt_mod.nRx511BytePkts -= dp_reins_count.dp_511BytePkts;
	wide_cnt_mod.nRx1023BytePkts -= dp_reins_count.dp_1023BytePkts;
	wide_cnt_mod.nRxMaxBytePkts -= dp_reins_count.dp_MaxBytePkts;
	wide_cnt_mod.nRxOversizeGoodPkts -= dp_reins_count.dp_OversizeGoodPkts;

	/* Copy counters values to ethtool buffer */
	data_dst = data;

#define _DP_ETHTOOL(x, y) *data_dst++ = wide_cnt_mod.y;
	DP_ETHTOOL_STAT_REG_DECLARE
#undef _DP_ETHTOOL
}

/********************* TIMER **************************************/

void cnt_upd_timer_handler(unsigned long data)
{
	/* Restarting the timer...*/
	mod_timer(&cnt_upd_timer, jiffies + msecs_to_jiffies(CNT_UPDATE_TIME));

	update_all_devs_counters();
}

/*******************************************************************/

static int __init mib_counters_mod_init(void)
{
	struct dp_event event_info = { 0 };
	struct device_node *np;
	u32 gsw_mode = 0;
	int ret;

	/* Initialize empty list of registered net devices */
	INIT_LIST_HEAD(&registered_devices_list);

	rwlock_init(&dev_list_lock);

	/* Starting the timer.*/
	setup_timer(&cnt_upd_timer, cnt_upd_timer_handler, 0);
	mod_timer(&cnt_upd_timer, jiffies + msecs_to_jiffies(CNT_UPDATE_TIME));

	/* Registering of net dev register callback */
	event_info.inst = 0;
	event_info.owner = DP_EVENT_OWNER_MIB;
	event_info.type = DP_EVENT_REGISTER_SUBIF | DP_EVENT_DE_REGISTER_SUBIF;
	event_info.id = 0;
	event_info.dp_event_cb = subif_register_cb;

	ret = dp_register_event_cb(&event_info, 0);
	if (ret != DP_SUCCESS) {
		pr_err("Can't register DP_EVENT_REGISTERT_SUBIF callback\n");
		return ret;
	}

	/* Save callback ID for deregistration purpose */
	g_notify_cb_id = event_info.id;

	/* Get info from device tree about "shortcut" mode */
	np = of_find_node_by_name(NULL, "gsw_core");
	of_property_read_u32(np, "gsw_mode", &gsw_mode);
	shortcut_mode = ((gsw_mode & BIT(0)) == GSW_SHORTCUT_MODE);

	return 0;
}

static void __exit mib_counters_mod_exit(void)
{
	struct dp_event event_info = { 0 };
	int ret;
	struct mib_counters *mib_cnt;
	struct list_head *p;

	/* Deregistering the callback function */
	event_info.inst = 0;
	event_info.owner = DP_EVENT_OWNER_MIB;
	event_info.type = DP_EVENT_REGISTER_SUBIF | DP_EVENT_DE_REGISTER_SUBIF;
	event_info.id = g_notify_cb_id;
	event_info.dp_event_cb = subif_register_cb;

	ret = dp_register_event_cb(&event_info, DP_F_DEREGISTER);
	if (ret != DP_SUCCESS) {
		pr_err("Can't deregister SUBIF notify callback\n");
		return;
	}

	/* STOP and remove timer */
	del_timer(&cnt_upd_timer);

	/* Free all list entries for registered net devices */
	write_lock(&dev_list_lock);
	list_for_each (p, &registered_devices_list) {
		mib_cnt = list_entry(p, struct mib_counters, dev_list);
		list_del(p);
		devm_kfree(&g_dp_dev->dev, mib_cnt);
	}
	write_unlock(&dev_list_lock);
}

module_init(mib_counters_mod_init);
module_exit(mib_counters_mod_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MIB counters driver for overflow handling");
