// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#ifndef DATAPATH_MIB_H
#define DATAPATH_MIB_H

int dp_reset_mib(u32 flag);
int set_gsw_itf(u8 ep, u8 ena, int start);
int reset_gsw_itf(u8 ep);
int dp_get_port_vap_mib_31(dp_subif_t *subif, void *priv,
			   struct rtnl_link_stats64 *net_mib, u32 flag);

#endif
