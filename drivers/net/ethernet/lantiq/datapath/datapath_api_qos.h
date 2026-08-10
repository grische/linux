/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * include/net/datapath_api_qos.h.
 *
 * Datapath QoS API types.
 */

#ifndef DP_QOS_API_H
#define DP_QOS_API_H

#include <linux/types.h>

/* @brief enum DP_API_STATUS - shared between the QoS API and the main API */
enum DP_API_STATUS_QOS {
	DP_QOS_FAILURE = -1,
	DP_QOS_SUCCESS = 0,
};

/* Common QoS-link-node identifier carried through subif registration. */
enum dp_node_type {
	DP_NODE_UNKNOWN = 0,
	DP_NODE_QUEUE,
	DP_NODE_SCH,
	DP_NODE_PORT
};

union dp_node_id {
	int q_id;
	int sch_id;
	int cqm_deq_port;
};

#endif /* DP_QOS_API_H */
