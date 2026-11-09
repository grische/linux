// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright (c) 2020 Intel Corporation
 *
 *****************************************************************************/
#include <net/pkt_cls.h>
#include <net/pon_qos_tc.h>
#include "pon_qos_tc_main.h"
#include "pon_qos_tc_qos.h"

#define DRV_NAME "mod_pon_qos"

int (*pon_qos_setup_tc_fn)(struct net_device *dev,
			   u32 handle,
			   __be16 protocol,
			   struct tc_to_netdev *tc,
			   int port_id,
			   int deq_idx);
EXPORT_SYMBOL(pon_qos_setup_tc_fn);

int pon_qos_setup_tc(struct net_device *dev,
		     u32 handle,
		     __be16 protocol,
		     struct tc_to_netdev *tc,
		     int port_id,
		     int deq_idx)
{
	int ret = 0;

	ASSERT_RTNL();
	switch (tc->type) {
	case TC_SETUP_CLSFLOWER:
		ret = pon_qos_tc_flower_offload(dev,
						handle,
						tc->cls_flower);
		break;
	case TC_SETUP_MQPRIO:
		return pon_qos_tc_mqprio_offload(dev, handle, tc,
						 port_id, deq_idx);
		break;
	case TC_SETUP_QDISC_PRIO:
		return pon_qos_tc_prio_offload(dev, handle, tc,
					       port_id, deq_idx);
		break;
	case TC_SETUP_DRR:
		return pon_qos_tc_drr_offload(dev, handle, tc,
					      port_id, deq_idx);
		break;
	default:
		netdev_err(dev, "offload type:%d not supported\n", tc->type);
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(pon_qos_setup_tc);

static int __init pon_qos_init(void)
{
	pon_qos_setup_tc_fn = pon_qos_setup_tc;
	return 0;
}

static void __exit pon_qos_destroy(void)
{
	rtnl_lock();
	pon_qos_ports_cleanup();
	rtnl_unlock();
}

module_init(pon_qos_init);
module_exit(pon_qos_destroy);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK(DRV_NAME);
