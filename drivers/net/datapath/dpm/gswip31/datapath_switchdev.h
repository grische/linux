// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#ifndef DATAPATH_SWITCHDEV_H_
#define DATAPATH_SWITCHDEV_H_

int dp_swdev_alloc_bridge_id(int inst);
int dp_swdev_bridge_port_cfg_set(struct br_info *br_item,
				 int inst, int bport, u32 flags);
int dp_swdev_bridge_port_cfg_reset(struct br_info *br_item,
				   int inst, int bport, u16 hairpin);
int dp_swdev_bridge_cfg_set(int inst, u16 fid);
int dp_swdev_free_brcfg(int inst, u16 fid);
int dp_gswip_ext_vlan(int inst, int vap, int ep);
int dp_swdev_bridge_port_flags_set(int inst, int bport, unsigned long flags);
int dp_swdev_bridge_port_flags_get(int inst, int bport, unsigned long *flags);
int dp_swdev_port_learning_limit_set(int inst, int bport, int learning_limit);
#endif
