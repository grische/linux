/*
 * Copyright (C) Intel Corporation
 * Author: Robert Dabrowski <robertx.dabrowski@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "datapath.h"

/* Ethtool statistics strings */
static const char dp_ethtool_gstrings[][ETH_GSTRING_LEN] = {
#define _DP_ETHTOOL(x, y) #x,
	DP_ETHTOOL_STAT_REG_DECLARE
#undef _DP_ETHTOOL
};

/* ethtool statistics support */
int dp_net_dev_get_ss_stat_strings_count_3X(struct net_device *dev)
{
	return ARRAY_SIZE(dp_ethtool_gstrings);
}

void dp_net_dev_get_ss_stat_strings_3X(struct net_device *dev, u8 *data)
{
	memcpy(data, dp_ethtool_gstrings, sizeof(dp_ethtool_gstrings));
}
