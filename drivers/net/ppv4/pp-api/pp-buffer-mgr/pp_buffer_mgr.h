/*
 * pp_buffer_mgr.h
 * Description: Packet Processor Buffer Manager Driver
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018 Intel Corporation
 */
#ifndef _PP_BUFFER_MGR_H_
#define _PP_BUFFER_MGR_H_

#include <linux/types.h>
#include <linux/pp_api.h>
#include <linux/pp_buffer_mgr_api.h>
#include <linux/debugfs.h> /* struct dentry */

/**
 * @brief PP_BM_MAX_POOLS
 *       Max supported pools. Real max defined in the DTS
 */
#define PP_BM_MAX_POOLS (16)

/**
 * @brief PP_BM_MAX_POLICIES
 *       Max supported policies. Real max defined in the DTS
 */
#define PP_BM_MAX_POLICIES (128)
#define PP_BM_POLICY_INVALID PP_BM_MAX_POLICIES
#define PP_BM_IS_POLICY_VALID(p) \
	(0 <= (p) && (p) < PP_BM_POLICY_INVALID)

#define PP_BM_MAX_BURST_IN_POP (32)

/**
 * @struct pp_bmgr_cfg
 * @brief Buffer manager initial configuration
 */
struct pp_bmgr_init_param {
	bool valid; /*! params valid */
	u64 qos_uc_base; /*! base address of qos uc for mcdma operations */
	u32 max_pools; /*! Max number of pools */
	u32 max_groups; /*! Max number of groups */
	u32 max_policies; /*! Max number of policies */
	u32 max_pools_in_policy; /*! Max number of pools in policy */
	bool pool_pop_hw_en; /*! true if feature is enabled in hw */
	struct dentry *dbgfs;
};

/**
 * @brief Buffer Manager stats
 */
struct bmgr_stats {
	u32 active_pools; /*! number of active pools */
	u32 active_policies; /*! number of active policies */
};

/**
 * @brief Module init function
 * @param cfg Buffer manager configuration
 * @return s32 0 on success, non-zero value otherwise
 */
s32 pp_bmgr_init(const struct pp_bmgr_init_param *cfg);

/**
 * @brief Module exit function, clean all resources
 * @return void
 */
void pp_bmgr_exit(void);

/**
 * @brief Get buffer manager statistics
 * @param stats
 * @return s32 0 on success, error code otherwise
 */
s32 pp_bmgr_stats_get(struct bmgr_stats *stats);

/**
 * @brief Set bmgr configuration
 * @param cfg Buffer manager configuration
 * @return 0 on success, other error code on failure
 */
s32 bmgr_config_set(const struct pp_bmgr_init_param *const cfg);

/**
 * @brief Get bmgr configuration
 * @param cfg Buffer manager configuration
 * @return 0 on success, other error code on failure
 */
s32 pp_bmgr_config_get(struct pp_bmgr_init_param *const cfg);

#endif /* _PP_BUFFER_MGR_H_ */
