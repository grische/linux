// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright (c) 2020 Intel Corporation
 *
 *****************************************************************************/
#include <net/pkt_cls.h>
#include <linux/list.h>
#include <net/flow_dissector.h>
#include <net/tc_act/tc_mirred.h>
#include <net/tc_act/tc_gact.h>
#include <net/pon_qos_tc.h>
#include "pon_qos_tc_qos.h"

struct flower_cls_map {
	__be16 proto;
	u32 pref;
	u32 classid;
	bool ingress;
	struct flow_dissector_key_vlan key;
	struct flow_dissector_key_vlan mask;
	struct net_device *dev;
	struct net_device *indev;
	struct list_head list;
};

static LIST_HEAD(tc_class_list);

static struct flower_cls_map *pon_qos_tc_flower_get_cls(struct net_device *dev,
							u32 pref)
{
	struct flower_cls_map *p, *n;

	list_for_each_entry_safe (p, n, &tc_class_list, list) {
		if (p->dev == dev && p->pref == pref)
			return p;
	}

	return NULL;
}

static int pon_qos_tc_flower_action_valid(struct tcf_exts *exts)
{
	const struct tc_action *a;
	LIST_HEAD(actions);
	int act_nr = 0;
	int act_ok_nr = 0;

	if (tc_no_actions(exts))
		return -EINVAL;

	tcf_exts_to_list(exts, &actions);
	list_for_each_entry (a, &actions, list) {
		if (a->ops && a->ops->type == TCA_ACT_GACT &&
		    to_gact(a)->tcf_action == TC_ACT_OK)
			act_ok_nr++;
		act_nr++;
	}

	if (act_nr != 1 && act_ok_nr != 1)
		return -EINVAL;

	return 0;
}

static int pon_qos_parse_tc_flower(struct tc_cls_flower_offload *f,
				   struct flower_cls_map **flt)
{
	int ret = 0;

	*flt = kzalloc(sizeof(**flt), GFP_KERNEL);
	if (!*flt)
		return -ENOMEM;

	if (f->dissector->used_keys &
	    ~(BIT(FLOW_DISSECTOR_KEY_CONTROL) |
	      BIT(FLOW_DISSECTOR_KEY_BASIC) |
	      BIT(FLOW_DISSECTOR_KEY_ETH_ADDRS) |
	      BIT(FLOW_DISSECTOR_KEY_VLAN) |
	      BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS) |
	      BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS) |
	      BIT(FLOW_DISSECTOR_KEY_IP) |
	      BIT(FLOW_DISSECTOR_KEY_PORTS))) {
		pr_debug("%s: Unsupported key used: 0x%x\n", __func__,
			 f->dissector->used_keys);
		return -EINVAL;
	}
	pr_debug("%s: Supported key used: 0x%x\n", __func__,
		 f->dissector->used_keys);

	memset(*flt, 0, sizeof(**flt));

	(*flt)->pref = f->common.prio >> 16;
	(*flt)->proto = f->common.protocol;
	(*flt)->classid = f->classid;
	if (TC_H_MIN(f->common.classid) == TC_H_MIN(TC_H_MIN_EGRESS))
		(*flt)->ingress = false;
	else
		(*flt)->ingress = true;

	/* Classification/Matching arguments parsing */
	if (dissector_uses_key(f->dissector, FLOW_DISSECTOR_KEY_VLAN)) {
		struct flow_dissector_key_vlan *key =
			skb_flow_dissector_target(f->dissector,
						  FLOW_DISSECTOR_KEY_VLAN,
						  f->key);
		struct flow_dissector_key_vlan *mask =
			skb_flow_dissector_target(f->dissector,
						  FLOW_DISSECTOR_KEY_VLAN,
						  f->mask);
		pr_debug("%s: match vid: %#x/%#x pcp: %#x\n", __func__,
			 key->vlan_id,
			 key->vlan_priority,
			 mask->vlan_id);
		(*flt)->key = *key;
		(*flt)->mask = *mask;
	}

	return ret;
}

int pon_qos_tc_flower_assign_tc(struct net_device *dev,
				struct tc_cls_flower_offload *f)
{
	struct flower_cls_map *map = NULL;
	struct net_device *indev = NULL;
	struct pon_qos_qmap_tc qmap = { 0 };
	int ifi = (int)*(int *)f->key;
	int ret = 0;

	if (ifi) {
		indev = dev_get_by_index(dev_net(dev), ifi);
		if (!indev)
			return -1;
	}

	ret = pon_qos_tc_flower_action_valid(f->exts);
	if (ret < 0)
		return -EINVAL;

	ret = pon_qos_parse_tc_flower(f, &map);
	if (ret < 0)
		return -EINVAL;

	map->dev = dev;
	map->indev = indev;
	qmap.indev = map->indev;
	qmap.tc = map->key.vlan_priority;
	qmap.handle = map->classid;
	ret = pon_qos_update_qmap(dev, &qmap, 1);
	if (ret < 0) {
		netdev_err(dev, "%s: queue map fail\n", __func__);
		goto err;
	}

	list_add(&map->list, &tc_class_list);
	return 0;
err:
	kfree(map);
	return -EINVAL;
}
EXPORT_SYMBOL(pon_qos_tc_flower_assign_tc);

int pon_qos_tc_flower_unassign_tc(struct net_device *dev,
				  struct tc_cls_flower_offload *f)
{
	struct flower_cls_map *map = NULL;
	struct pon_qos_qmap_tc qmap = { 0 };
	int ret = 0;

	map = pon_qos_tc_flower_get_cls(dev, f->common.prio >> 16);
	if (!map)
		return -EOPNOTSUPP;

	qmap.indev = map->indev;
	qmap.tc = map->key.vlan_priority;
	qmap.handle = map->classid;
	ret = pon_qos_update_qmap(dev, &qmap, 0);
	if (ret < 0) {
		netdev_err(dev, "%s: queue unmap fail\n", __func__);
		ret = -EINVAL;
	}

	list_del(&map->list);
	if (map->indev)
		dev_put(map->indev);
	kfree(map);

	return 0;
}
EXPORT_SYMBOL(pon_qos_tc_flower_unassign_tc);
