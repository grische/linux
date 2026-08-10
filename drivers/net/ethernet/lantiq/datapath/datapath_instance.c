// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * datapath/datapath_instance.c.
 *
 * Datapath instance bookkeeping: one instance per GSWIP device, resolved at
 * registration time.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/if.h>

#include "datapath.h"
#include "datapath_api.h"

/* Field layout matches the plan's enumerated set verbatim. */
struct dp_inst_storage_entry {
	struct net_device *dev;
	char subif_name[IFNAMSIZ];
	int inst;
	int ep;
	int bp;
	int ctp;
	u32 flag;
	bool valid;
};

/*
 * dp_inst_storage[DP_MAX_INST][16][16]: file-scope static bookkeeping mesh.
 */
static struct dp_inst_storage_entry
	dp_inst_storage[DP_MAX_INST][MAX_DP_PORTS][MAX_DP_PORTS];

int register_dp_hw_cap(struct dp_hw_cap *info, u32 flag)
{
	(void)flag;
	if (!info)
		return -1;
	mutex_lock(&dp_lock);
	/* Single-instance case: copy cap info into dp_port_prop[0]. */
	dp_port_prop[0].info = info->info;
	mutex_unlock(&dp_lock);
	return 0;
}

int dp_request_inst(struct dp_inst_info *info, u32 flag)
{
	(void)info;
	(void)flag;
	/*
	 * Real per-instance state is set up by request_dp() in datapath_api.c
	 * (ops[0]/ops[1] population + dp_platform_set call).
	 */
	mutex_lock(&dp_lock);
	if (dp_inst_num == 0)
		dp_inst_num = 1;
	mutex_unlock(&dp_lock);
	return 0;
}

int dp_inst_init(void)
{
	/*
	 * AVM datapath_instance.c dp_inst_init walks the dp_dev_list /
	 * dp_dev_list_free hash heads. The minimal stub clears the single
	 * dp_port_prop[0] slot - the corresponding zero of dp_port_info /
	 * dp_port_info2 is implicit because they are file-scope BSS
	 * allocations.
	 */
	mutex_lock(&dp_lock);
	memset(&dp_port_prop[0], 0, sizeof(dp_port_prop[0]));
	memset(dp_inst_storage, 0, sizeof(dp_inst_storage));
	mutex_unlock(&dp_lock);
	return 0;
}

int dp_inst_add_dev(struct net_device *dev, char *subif_name, int inst,
		    int port_id, int bp, int subif, u32 flag)
{
	int i;

	/*
	 * No additional locking here; lockdep_assert_held documents the
	 * requirement.
	 */
	lockdep_assert_held(&dp_lock);

	if (inst < 0 || inst >= DP_MAX_INST)
		return -1;
	if (port_id < 0 || port_id >= MAX_DP_PORTS)
		return -1;
	if (subif < 0 || subif >= MAX_DP_PORTS)
		return -1;

	for (i = 0; i < MAX_DP_PORTS; i++) {
		struct dp_inst_storage_entry *e =
			&dp_inst_storage[inst][port_id][i];

		if (e->valid)
			continue;

		e->dev = dev;
		if (subif_name) {
			strncpy(e->subif_name, subif_name, IFNAMSIZ - 1);
			e->subif_name[IFNAMSIZ - 1] = '\0';
		} else {
			e->subif_name[0] = '\0';
		}
		e->inst = inst;
		e->ep = port_id;
		e->bp = bp;
		e->ctp = subif;
		e->flag = flag;
		e->valid = true;
		return 0;
	}
	return -1;
}

int dp_inst_del_dev(struct net_device *dev, char *subif_name, int inst,
		    int port_id, int bp, int subif)
{
	int i;

	(void)bp;
	(void)subif;

	lockdep_assert_held(&dp_lock);

	if (inst < 0 || inst >= DP_MAX_INST)
		return -1;
	if (port_id < 0 || port_id >= MAX_DP_PORTS)
		return -1;

	for (i = 0; i < MAX_DP_PORTS; i++) {
		struct dp_inst_storage_entry *e =
			&dp_inst_storage[inst][port_id][i];

		if (!e->valid)
			continue;
		if (e->dev != dev)
			continue;
		if (subif_name &&
		    strncmp(e->subif_name, subif_name, IFNAMSIZ) != 0)
			continue;
		e->valid = false;
		return 0;
	}
	return -1;
}

int dp_get_inst_via_module(struct module *owner, u32 port_id, u32 dev_port)
{
	int ret;

	(void)dev_port;

	if (port_id >= MAX_DP_PORTS)
		return -1;

	mutex_lock(&dp_lock);
	if (dp_port_info[0][port_id].status >= PORT_ALLOCATED &&
	    dp_port_info[0][port_id].owner == owner)
		ret = 0;
	else
		ret = -1;
	mutex_unlock(&dp_lock);
	return ret;
}

int dp_inst_insert_mod(struct module *owner, u16 ep, u32 inst, u32 flag)
{
	(void)owner;
	(void)ep;
	(void)inst;
	(void)flag;
	/* No locking here. */
	return 0;
}
