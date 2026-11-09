/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * Copyright (c) 2020 Intel Corporation
 *
 *****************************************************************************/
#ifndef _PON_QOS_TC_QMAP_
#define _PON_QOS_TC_QMAP_

int pon_qos_tc_flower_assign_tc(struct net_device *dev,
				struct tc_cls_flower_offload *f);

int pon_qos_tc_flower_unassign_tc(struct net_device *dev,
				  struct tc_cls_flower_offload *f);

#endif
