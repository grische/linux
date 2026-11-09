// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#include <linux/module.h>
#include <linux/types.h> /* size_t */
#include <linux/inetdevice.h>
#include <net/datapath_api.h>
#include <net/datapath_proc_api.h>
#include "datapath.h"
#include "datapath_instance.h"
#include "datapath_swdev_api.h"
#include "datapath_swdev.h"

static int dp_event(struct notifier_block *this,
		    unsigned long event, void *ptr);
static struct notifier_block dp_dev_notifier = {
	dp_event, /*handler*/
	NULL,
	0
};

int register_notifier(u32 flag)
{
	return register_netdevice_notifier(&dp_dev_notifier);
}

int unregister_notifier(u32 flag)
{
	return unregister_netdevice_notifier(&dp_dev_notifier);
}

int dp_event(struct notifier_block *this, unsigned long event, void *ptr)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_SWITCHDEV)
	struct net_device *dev;
	u8 *addr;
	int i;
	struct net_device *br_dev;
	struct dp_dev *dp_dev;
	struct br_info *br_info;
	int fid, inst, vap = 0;
	u32 idx;
	struct pmac_port_info *port;
	dp_subif_t subif = { 0 };
	struct inst_property *prop;
	struct dp_subif_info *sif;
	struct inst_info *dp_info;

	dev = netdev_notifier_info_to_dev(ptr);
	if (!dev)
		return 0;
	addr = (u8 *)dev->dev_addr;
	if (!addr || dev->addr_len != ETH_ALEN) /*only support ethernet */
		return 0;
	if (is_zero_ether_addr(addr)) {
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "Skip zero mac address for %s\n", dev->name);
		return 0;
	}
	idx = dp_dev_hash(dev, NULL);
	dp_dev = dp_dev_lookup(&dp_dev_list[idx], dev, NULL, 0);
	if (!dp_dev) {
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "datapath dev(%s) not exists!!,No mac config\n",
			 dev ? dev->name : "NULL");
		return 0;
	}
	inst = dp_dev->inst;
	port = get_dp_port_info(inst, dp_dev->ep);
	if (dp_get_netif_subifid(dev, NULL, NULL, NULL, &subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_DBG,
			 "get subifid fail(%s), No Mac config\n",
			 dev ? dev->name : "NULL");
		return DP_FAILURE;
	}
	vap = GET_VAP(subif.subif, port->vap_offset,
		      port->vap_mask);
	prop = &dp_port_prop[inst];
	sif = get_dp_port_subif(port, vap);
	dp_info = get_dp_prop_info(inst);

	switch (event) {
	case NETDEV_GOING_DOWN:
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "%s%d %s%d %s%s %s%02x%02x%02x%02x%02x%02x\n",
			 "Rem MAC with BP:",
			 sif->bp, "FID:", dp_dev->fid,
			 "dev:", dev ? dev->name : "NULL",
			 "MAC:", addr[0], addr[1], addr[2],
			 addr[3], addr[4], addr[5]);
		for (i = 0; i < prop->info.cap.max_num_subif_per_port; i++) {
			if (get_dp_port_subif(port, i)->netif == dev) {
				vap = i;
				sif = get_dp_port_subif(port, vap);
				DP_DEBUG(DP_DBG_FLAG_NOTIFY, "vap:%d\n", vap);
				sif->fid = 0;
			}
		}
		dp_info->dp_mac_reset(0, dp_dev->fid, dp_dev->inst, addr);
		break;
	case NETDEV_CHANGEUPPER:
		dp_info->dp_mac_reset(0, dp_dev->fid, dp_dev->inst, addr);
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "%s%d %s%d %s%s %s%02x%02x%02x%02x%02x%02x\n",
			 "Rem MAC with BP:",
			 sif->bp, "FID:", dp_dev->fid,
			 "dev:", dev ? dev->name : "NULL",
			 "MAC:", addr[0], addr[1], addr[2],
			 addr[3], addr[4], addr[5]);
		if (!netif_is_bridge_port(dev)) {
			DP_DEBUG(DP_DBG_FLAG_NOTIFY,
				 "chg upper:dev not a Bport\n");
		}
		br_dev = netdev_master_upper_dev_get(dev);
		if (!br_dev) {
			DP_DEBUG(DP_DBG_FLAG_NOTIFY,
				 "chg upper:without br name\n");
			/* Set FID to Zero when br not attached to
			 * bport
			 */
			dp_dev->fid = 0;
			sif->fid = dp_dev->fid;
			goto dev_status;
		}
		/* Get respective FID when bport attached to bridge
		 */
		DP_DEBUG(DP_DBG_FLAG_NOTIFY, "Bridge name:%s\n",
			 br_dev ? br_dev->name : "NULL");
		br_info = dp_swdev_bridge_entry_lookup(br_dev->name);
		if (sif->swdev_en == 1) {
			if (br_info) {
				dp_dev->fid = br_info->fid;
			} else {
				fid = dp_notif_br_alloc(br_dev);
				if (fid > 0) {
					dp_dev->fid = fid;
				} else {
					pr_err("FID alloc failed in %s\r\n",
					       __func__);
					return 0;
				}
			}
		} else {
			dp_dev->fid = 0;
		}
		for (i = 0; i < prop->info.cap.max_num_subif_per_port; i++) {
			if (get_dp_port_subif(port, i)->netif == dev) {
				vap = i;
				sif = get_dp_port_subif(port, vap);
				DP_DEBUG(DP_DBG_FLAG_NOTIFY, "vap:%d\n", vap);
				sif->fid = dp_dev->fid;
			}
		}
	dev_status:
		if (dev->flags & IFF_UP) {
			DP_DEBUG(DP_DBG_FLAG_NOTIFY,
				 "%s%s%d%s%d %s%s %s%02x%02x%02x%02x%02x%02x\n",
				 "link UP,",
				 "ADD MAC with BP:",
				 sif->bp, " FID:", dp_dev->fid,
				 "dev:", dev ? dev->name : "NULL",
				 "MAC:", addr[0], addr[1], addr[2],
				 addr[3], addr[4], addr[5]);
			/* Note: For Linux network device's mac address,
			 *       its bridge port should be CPU port, not its
			 *       mapped bridge port in GSWIP
			 */
			dp_info->dp_mac_set(0, dp_dev->fid, dp_dev->inst, addr);
		} else {
			DP_DEBUG(DP_DBG_FLAG_NOTIFY, "DEV:%s %s %s\n",
				 dev ? dev->name : "NULL",
				 "link down", "No MAC configuration");
		}
		break;
	case NETDEV_UP:
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "%s%d %s%d %s%s %s%02x%02x%02x%02x%02x%02x\n",
			 "ADD MAC with BP:",
			 sif->bp, "FID:", dp_dev->fid,
			 "dev:", dev ? dev->name : "NULL",
			 "MAC:", addr[0], addr[1], addr[2],
			 addr[3], addr[4], addr[5]);
		dp_info->dp_mac_set(0, dp_dev->fid, dp_dev->inst, addr);
		break;
	case NETDEV_UNREGISTER:
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "DevUnreg %s:%02x:%02x:%02x:%02x:%02x:%02x\n",
			 dev->name,
			 addr[0], addr[1], addr[2],
			 addr[3], addr[4], addr[5]);
		break;
	case NETDEV_CHANGEADDR:
		DP_DEBUG(DP_DBG_FLAG_NOTIFY,
			 "DevChg %s:%02x:%02x:%02x:%02x:%02x:%02x\n",
			 dev->name,
			 addr[0], addr[1], addr[2],
			 addr[3], addr[4], addr[5]);
		break;
	default:
		break;
	}
#endif
	return 0;
}

ATOMIC_NOTIFIER_HEAD(dp_evt_notif_list);
static int dp_notifier(struct notifier_block *self, unsigned long action,
		       void *data)
{
	struct dp_evt_notif_data *notif_data = data;
	struct dp_evt_notif_info *evt_notif = NULL;
	struct dp_event_info info;
	struct list_head *pos;

	if (!data) {
		pr_err("invalid notifier data\n");
		return -EINVAL;
	}

	rcu_read_lock();
	list_for_each (pos, &evt_notif_list[notif_data->inst]) {
		evt_notif = list_entry(pos, struct dp_evt_notif_info, list);
		if (evt_notif->nb == self)
			break;
	}

	if (!evt_notif || !evt_notif->evt_info.dp_event_cb)
		goto end;

	switch (evt_notif->evt_info.type & action) {
	case DP_EVENT_INIT:
		info.init_info.dev = notif_data->dev;
		info.init_info.owner = notif_data->mod;
		info.init_info.dev_port = notif_data->dev_port;
		break;
	case DP_EVENT_ALLOC_PORT:
		info.alloc_info.dev = notif_data->dev;
		info.alloc_info.owner = notif_data->mod;
		info.alloc_info.dev_port = notif_data->dev_port;
		break;
	case DP_EVENT_DE_ALLOC_PORT:
		info.de_alloc_info.dev = notif_data->dev;
		info.de_alloc_info.owner = notif_data->mod;
		info.de_alloc_info.dev_port = notif_data->dev_port;
		break;
	case DP_EVENT_REGISTER_DEV:
		info.reg_dev_info.dev = notif_data->dev;
		info.reg_dev_info.dpid = notif_data->dpid;
		info.reg_dev_info.owner = notif_data->mod;
		break;
	case DP_EVENT_DE_REGISTER_DEV:
		info.dereg_dev_info.dev = notif_data->dev;
		info.dereg_dev_info.dpid = notif_data->dpid;
		info.dereg_dev_info.owner = notif_data->mod;
		break;
	case DP_EVENT_REGISTER_SUBIF:
		info.reg_subif_info.dev = notif_data->dev;
		info.reg_subif_info.dpid = notif_data->dpid;
		info.reg_subif_info.subif = notif_data->subif;
		break;
	case DP_EVENT_DE_REGISTER_SUBIF:
		info.de_reg_subif_info.dev = notif_data->dev;
		info.de_reg_subif_info.dpid = notif_data->dpid;
		info.de_reg_subif_info.subif = notif_data->subif;
		break;
	case DP_EVENT_OWNER_SWITCH:
		info.owner_info.dpid = notif_data->dpid;
		info.owner_info.new_owner = notif_data->type;
		break;
	default:
		goto end;
	}

	info.inst = notif_data->inst;
	info.type = action;
	info.data = evt_notif->evt_info.data;
	evt_notif->evt_info.dp_event_cb(&info);
end:
	rcu_read_unlock();

	return NOTIFY_DONE;
}

int register_dp_event_notifier(struct dp_event *info)
{
	struct dp_evt_notif_info *evt_notif;
	struct notifier_block *nb = NULL;

	if (!info->id) {
		evt_notif = devm_kzalloc(&g_dp_dev->dev, sizeof(*evt_notif),
					 GFP_ATOMIC);
		if (!evt_notif)
			return DP_FAILURE;

		nb = devm_kzalloc(&g_dp_dev->dev, sizeof(*nb), GFP_ATOMIC);
		if (!nb)
			return DP_FAILURE;

		nb->notifier_call = dp_notifier;
	} else {
		list_for_each_entry (evt_notif, &evt_notif_list[info->inst],
				     list) {
			if (evt_notif->nb == info->id)
				break;
		}

		if (!evt_notif) {
			pr_err("invalid handle\n");
			return DP_FAILURE;
		}
	}

	evt_notif->evt_info.f_owner_only = info->f_owner_only;
	evt_notif->evt_info.dp_event_cb = info->dp_event_cb;
	evt_notif->evt_info.type = info->type;
	evt_notif->evt_info.data = info->data;

	if (nb) {
		evt_notif->nb = nb;
		info->id = nb;

		list_add(&evt_notif->list, &evt_notif_list[info->inst]);

		atomic_notifier_chain_register(&dp_evt_notif_list, nb);
	}

	return 0;
}

int unregister_dp_event_notifier(struct dp_event *info)
{
	struct dp_evt_notif_info *evt_notif;

	list_for_each_entry (evt_notif, &evt_notif_list[info->inst], list) {
		if (evt_notif->nb == info->id) {
			list_del_rcu(&evt_notif->list);
			synchronize_rcu();
			break;
		}
	}

	if (!evt_notif->nb) {
		pr_err("invalid handle\n");
		return DP_FAILURE;
	}

	atomic_notifier_chain_unregister(&dp_evt_notif_list, evt_notif->nb);
	devm_kfree(&g_dp_dev->dev, evt_notif->nb);
	devm_kfree(&g_dp_dev->dev, evt_notif);

	return 0;
}
