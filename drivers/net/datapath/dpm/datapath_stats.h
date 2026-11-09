/*
 * Copyright (C) Intel Corporation
 * Author: Robert Dabrowski <robertx.dabrowski@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef DATAPATH_STATS_H
#define DATAPATH_STATS_H

/*!
 *@brief dp_net_dev_get_ss_stat_strings_count_3X - function used by callback
 *dp_net_dev_get_ss_stat_strings_count to retrieve number of counters
 *@param[in] dev: net device
 *@return number of counters
 */
int dp_net_dev_get_ss_stat_strings_count_3X(struct net_device *dev);

/*!
 *@brief dp_net_dev_get_ss_stat_strings_3X - function used by gswip callback
 *dp_net_dev_get_ss_stat_strings to retrieve set of counters' names.
 *NOTE: The set of counters names is common for gswip31 and gdwip32
 *@param[in] dev: net device
 *@param[out] data: buffer pointer where the function will copy counters' names
 */
void dp_net_dev_get_ss_stat_strings_3X(struct net_device *dev, u8 *data);

#endif
