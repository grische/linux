/*
 * pp_bm_internal.h
 * Description: Packet Processor Buffer Manager Driver Internal
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018-2019 Intel Corporation
 */
#ifndef _BM_DRV_INTERNAL_H_
#define _BM_DRV_INTERNAL_H_

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/pp_api.h>
#include "pp_buffer_mgr.h"
#include "bm.h"

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "[PP_BM]:%s:%d: " fmt, __func__, __LINE__
#endif

/**
 * @brief Shortcut to iterate through BM pools
 * @param p loop iterator
 */
#define BM_FOR_EACH_POOL(_db, p) \
	for (p = 0; p < (_db)->cfg.max_pools; p++)

/**
 * @brief Shortcut to iterate through BM policies
 * @param p loop iterator
 */
#define BM_FOR_EACH_POLICY(_db, p) \
	for (p = 0; p < (_db)->cfg.max_policies; p++)

/**
 * @brief Shortcut to iterate through BM groups
 * @param g loop iterator
 */
#define BM_FOR_EACH_GROUP(_db, g) \
	for (g = 0; g < (_db)->cfg.max_groups; g++)

/**
 * @brief Shortcut to iterate through BM isolated groups
 * @param g loop iterator
 */
#define BM_FOR_EACH_ISOLATED_GROUP(_db, g) \
	for (g = 1; g < (_db)->cfg.max_groups; g++)

/**
 * @brief BM_MIN_POOL_BUFFER_SIZE Minimum buffer size in
 *      configured pool
 */
#define BM_MIN_POOL_BUFFER_SIZE (64)

/**
 * @brief BM_DEFAULT_GROUP_ID Default group id
 */
#define BM_DEFAULT_GROUP_ID (0)

/**
 * @brief BM_POLICY_RESET_DELAY_MS
 */
#define BM_POLICY_RESET_DELAY_MS (10)

/**
 * @brief This structure is used for buffer manager database
 * @available_buffers available buffers in group
 * @reserved_buffers reserved buffers in this group
 * @pools Pools in policy
 */
struct bmgr_group_db_entry {
	u32 available_buffers;
	u32 reserved_buffers;
	struct list_head pools_head;
	u8 is_busy;
};

/**
 * @brief This structure holds the pool database
 * @pool_params Pool params
 * @num_allocated_buffers Num allocated buffers
 * @num_deallocated_buffers Num deallocated buffers
 * @is_busy Is entry in used
 * @internal_pointers_tables Ptrs table to be used in HW
 */
struct bmgr_pool_db_entry {
	struct pp_bmgr_pool_params pool_params;
	struct list_head group_link;
	u32 num_allocated_buffers;
	u32 num_deallocated_buffers;
	u32 policy_ref_cnt;
	u8 pool_id;
	u8 is_busy;
	void *internal_ptrs_virt;
	dma_addr_t internal_ptrs_phys;
	size_t internal_ptrs_sz;
};

/**
 * @brief This structure holds the policy database
 * @policy_params Policy params
 * @num_allocated_buffers Num allocated buffers
 * @num_deallocated_buffers Num deallocated buffers
 * @is_busy Is entry in used
 */
struct bmgr_policy_db_entry {
	struct pp_bmgr_policy_params policy_params;
	bool is_isolated;
	u32 num_allocated_buffers;
	u32 num_deallocated_buffers;
	u8 group_id;
	u8 is_busy;
};

/**
 * @brief This structure holds the buffer manager database
 * @cfg Initial configuration
 * @pools Pools information
 * @policies Policies information
 * @group Group information
 * @ready specify if the buffer manager ready to be called
 * @db_lock
 * @num_pools Number of active pools
 * @num_policies Number of active policies
 */
struct bmgr_driver_db {
	struct pp_bmgr_init_param cfg;
	struct bmgr_pool_db_entry pools[PP_BM_MAX_POOLS];
	struct bmgr_policy_db_entry policies[PP_BM_MAX_POLICIES];
	struct bmgr_group_db_entry groups[BM_MAX_GROUPS];
	bool ready;
	spinlock_t db_lock;
	u32 num_pools;
	u32 num_policies;
};

/**
 * @brief Get buffer manager config registers
 * @param config struct
 * @return 0 on success, other error code on failure
 */
s32 bmgr_get_cfg_regs(struct bmgr_cfg_regs *const regs);

/**
 * @brief Get buffer manager group statistics
 * @param group stats statistics struct
 * @param group_id group id
 * @return 0 on success, other error code on failure
 */
s32 bmgr_group_stats_get(struct bmgr_group_stats *const stats, u8 group_id);

#ifdef CONFIG_DEBUG_FS
/**
 * @brief bm debug init
 * @param db db structure
 * @return s32 0 for success, non-zero otherwise
 */
s32 bm_dbg_init(struct bmgr_driver_db *db, struct dentry *parent);

/**
 * @brief bm debug cleanup
 * @param void
 * @return s32 0 for success, non-zero otherwise
 */
void bm_dbg_clean(void);
#else
static inline s32 bm_dbg_init(struct bmgr_driver_db *db, struct dentry *parent)
{
	return 0;
}

static inline void bm_dbg_clean(void)
{
}
#endif

#endif /* _BM_DRV_INTERNAL_H_ */
