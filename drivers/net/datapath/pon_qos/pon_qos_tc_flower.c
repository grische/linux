// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright (c) 2020 Intel Corporation
 *
 *****************************************************************************/
#include <net/pkt_cls.h>
#include "pon_qos_tc_qmap.h"

static int pon_qos_tc_flower_replace(struct net_device *dev,
				     struct tc_cls_flower_offload *f)
{
	if (f->classid)
		return pon_qos_tc_flower_assign_tc(dev, f);

	return -EOPNOTSUPP;
}

static int pon_qos_tc_flower_destroy(struct net_device *dev,
				     struct tc_cls_flower_offload *f)
{
	return pon_qos_tc_flower_unassign_tc(dev, f);
}

int pon_qos_tc_flower_offload(struct net_device *dev,
			      u32 handle,
			      struct tc_cls_flower_offload *f)
{
	int ret = 0;

	ASSERT_RTNL();
	netdev_dbg(dev, "%s:start %d\n", __func__, ret);
	if (TC_H_MAJ(f->common.classid) != TC_H_MAJ(TC_H_CLSACT))
		return -EOPNOTSUPP;

	switch (f->command) {
	case TC_CLSFLOWER_REPLACE: {
		netdev_dbg(dev, "%s:replace:\n", __func__);
		ret = pon_qos_tc_flower_replace(dev, f);
		break;
	}
	case TC_CLSFLOWER_DESTROY: {
		netdev_dbg(dev, "%s:destroy:\n", __func__);
		ret = pon_qos_tc_flower_destroy(dev, f);
		break;
	}
	case TC_CLSFLOWER_STATS: {
		netdev_dbg(dev, "%s:stats:\n", __func__);
		return -EOPNOTSUPP;
	}
	default:
		return -EOPNOTSUPP;
	}

	netdev_dbg(dev, "%s:exit %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(pon_qos_tc_flower_offload);
