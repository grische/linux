/*
 * pp_bm.c
 * Description: Packet Processor Buffer Manager Driver
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2017-2020 Intel Corporation
 */
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/bitfield.h>

#include "pp_common.h"
#include "pp_qos_utils.h"
#include "pp_regs.h"
#include "pp_buffer_mgr.h"
#include "bm.h"
#include "pp_buffer_mgr_internal.h"

static u64 qos_uc_base_addr;

#define QOS_UC_MCDM0_OFF (0x10000)
#define MCDMA_SRC_OFFSET (0)
#define MCDMA_DST_OFFSET (0x4)
#define MCDMA_CONTROL_OFFSET (0x8)
#define MCDMA_ACTIVE_MASK (BIT(30))

static void __bmgr_work_handler(struct work_struct *w);

static struct bmgr_driver_db *db;
static struct workqueue_struct *bmgr_wq;

/**
 * @brief This structure is used to pass data to the work queue
 * @work work queue structure
 * @policy_reset_id policy to reset
 */
struct policy_reset_action {
	struct delayed_work work;
	u16 policy_reset_id;
	void (*cb)(s32 ret);
};

/**
 * @brief This structure is used to pause/resume a group
 * @pause if set group is paused else it is resumed
 * @validate_resources if set pool resource validation will
 *        occur
 */
struct group_pause_config {
	bool pause;
	bool validate_resources;
};

/**
 * @brief This function locks the DB in a recursion-save manner
 * @return void
 */
static inline void __bmgr_db_lock(void)
{
	spin_lock_bh(&db->db_lock);
}

/**
 * @brief This function unlocks the DB in a recursion-save manner
 * @return void
 */
static inline void __bmgr_db_unlock(void)
{
	spin_unlock_bh(&db->db_lock);
}

static inline bool __bmgr_is_ready(void)
{
	if (likely(db && db->ready))
		return true;

	pr_err("PP buffer manager isn't ready!\n");
	return false;
}

/**
 * @brief Check wheather pool parameters are valid
 * @param pool_params: Pool param from user
 * @return 0 on success, other error code on failure
 */
static s32
__bmgr_is_pool_params_valid(const struct pp_bmgr_pool_params *const pool_params)
{
	/* Validity check */
	if (unlikely(!pool_params)) {
		pr_err("pool_params is NULL\n");
		return -EINVAL;
	}

	if (unlikely(pool_params->size_of_buffer < BM_MIN_POOL_BUFFER_SIZE)) {
		pr_err("size_of_buffer %d should be larger than %d\n",
		       pool_params->size_of_buffer,
		       BM_MIN_POOL_BUFFER_SIZE);
		return -EINVAL;
	}

	if (unlikely(pool_params->size_of_buffer % BM_MIN_POOL_BUFFER_SIZE)) {
		pr_err("size_of_buffer %d must be aligned to %d bytes\n",
		       pool_params->size_of_buffer,
		       BM_MIN_POOL_BUFFER_SIZE);
		return -EINVAL;
	}

	if (unlikely(pool_params->base_addr_low & 0x3F)) {
		pr_err("base_addr_low %d must be aligned to %d bytes\n",
		       pool_params->base_addr_low,
		       BM_MIN_POOL_BUFFER_SIZE);
		return -EINVAL;
	}

	if (!IS_ALIGNED(pool_params->num_buffers, 64)) {
		pr_err("Number of buffers must be aligned to 64\n");
		return -EINVAL;
	}

	/* Num_buffers can be up to 2^24 */
	if (unlikely(pool_params->num_buffers >= 0x1000000)) {
		pr_err("Number of buffers can be up to 0x1000000\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Check wheather policy parameters are valid
 * @param policy_params: Policy param from user
 * @return 0 on success, other error code on failure
 */
static s32
__bmgr_is_policy_params_valid(const struct pp_bmgr_policy_params
				      *const policy_params)
{
	if (unlikely(!policy_params)) {
		pr_err("policy_params is NULL\n");
		return -EINVAL;
	}

	if (unlikely(policy_params->num_pools_in_policy >
		     db->cfg.max_pools_in_policy)) {
		pr_err("num_pools_in_policy %d should be up to %d\n",
		       policy_params->num_pools_in_policy,
		       db->cfg.max_pools_in_policy);
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Allocate group
 * @note This function should be called under db lock
 * @param group_id allocated groop id
  @return 0 on success, other error code on failure
 */
static s32 __bmgr_allocate_group(u8 *group_id)
{
	u8 idx;

	BM_FOR_EACH_ISOLATED_GROUP(db, idx)
	{
		if (!db->groups[idx].is_busy) {
			*group_id = idx;
			db->groups[idx].is_busy = 1;
			break;
		}
	}

	if (idx == db->cfg.max_groups)
		return -ENOMEM;

	return 0;
}

/**
 * @brief Enable pool pop
 * @param pool_id: Pool ID
 * @param enable: True to enable, false to disable
 * @return 0 on success, other error code on failure
 */
static s32 __bmgr_pool_pop_enable(u8 pool_id, bool enable)
{
	/* Pool Pop Enable */
	if (db->cfg.pool_pop_hw_en)
		return bm_pool_pop_enable(pool_id, enable);

	return 0;
}

/**
 * @brief Pause/Unpause buffer allocation in pools associated
 *        with a group
 * @note This function should be called under db lock
 * @param group_id groop id to pause
 * @param cfg pause config
 * @return 0 on success, other error code on failure
 */
static s32 __group_pause(u8 group_id, struct group_pause_config *cfg)
{
	struct bmgr_group_db_entry *group;
	struct bmgr_pool_db_entry *pool;
	struct pp_bmgr_pool_stats pool_stats;
	bool pool_pop_enable;
	s32 ret = 0;

	if (cfg->pause)
		pool_pop_enable = false;
	else
		pool_pop_enable = true;

	/* Group entry in db */
	group = &db->groups[group_id];

	/* Disable pop for all pools in group */
	list_for_each_entry (pool, &group->pools_head, group_link) {
		if (!pool->is_busy) {
			pr_err("Pool %d in Group %d is not active\n",
			       pool->pool_id, group_id);
			continue;
		}

		ret = __bmgr_pool_pop_enable(pool->pool_id, pool_pop_enable);
		if (unlikely(ret)) {
			pr_err("bmgr_pool_pop_enable %d failed\n",
			       pool->pool_id);
			return ret;
		}

		/* Verify pool gained back its resources */
		if (!pool_pop_enable && cfg->validate_resources) {
			ret = pp_bmgr_pool_stats_get(&pool_stats,
						     pool->pool_id);
			if (unlikely(ret)) {
				pr_err("Pool %d get stats error\n",
				       pool->pool_id);
				return ret;
			} else if (pool_stats.pool_allocated_ctr != 0) {
				pr_err("Pool %d resources are still in use\n",
				       pool->pool_id);
				return -EBUSY;
			}
		}
	}

	return ret;
}

/**
 * @brief Add pool to group
 * @note This function should be called under db lock
 * @param group_id groop id to add pool to
 * @param pool_id pool id to add
 * @return 0 on success, other error code on failure
 */
static s32 __add_pool_to_group(u8 group_id, u8 pool_id)
{
	struct bmgr_group_db_entry *grp;
	struct bmgr_pool_db_entry *pool;

	if (!db->groups[group_id].is_busy)
		return -EINVAL;

	grp = &db->groups[group_id];
	pool = &db->pools[pool_id];

	list_add_tail(&pool->group_link, &grp->pools_head);
	grp->available_buffers += pool->pool_params.num_buffers;

	return bm_group_available_set(group_id, grp->available_buffers);
}

/**
 * @brief Add pools to policy
 * @note This function should be called under db lock and assume
 *       group is not active
 * @param policy_id policy id to add pool to
 * @param group_id groop id to add pool to
 * @param policy_params pools in policy info
 * @return 0 on success, other error code on failure
 */
static s32
__add_pools_to_policy(u16 policy_id,
		      u8 group_id,
		      const struct pp_bmgr_policy_params *const policy_params)
{
	struct bmgr_group_db_entry *group;
	u32 max = 0;
	s32 ret = 0;
	u8 pool_id;
	u8 idx = 0;

	/* Group entry in db */
	group = &db->groups[group_id];

	/* Traverse pools in policy */
	for (idx = 0; idx < policy_params->num_pools_in_policy; idx++) {
		pool_id = policy_params->pools_in_policy[idx].pool_id;

		/* Update pool/policy reference count */
		db->pools[pool_id].policy_ref_cnt++;

		/* Update group if it's the first time this pool is in use */
		if (db->pools[pool_id].policy_ref_cnt == 1) {
			ret = __add_pool_to_group(group_id, pool_id);
			if (unlikely(ret)) {
				pr_err("__add_pool(%d)_to_group(%d) failed\n",
				       pool_id, group_id);
				goto add_pools_to_policy_done;
			}
		}

		max = policy_params->pools_in_policy[idx].max_allowed;
		ret = bm_policy_pool_mapping_set(policy_id, pool_id,
						 idx, max, true);
		if (unlikely(ret)) {
			pr_err("bm_policy(%d)_pool(%d)_mapping_set Failed\n",
			       policy_id, pool_id);
			goto add_pools_to_policy_done;
		}
	}

	/* Set the group's reserved buffers */
	group->reserved_buffers += policy_params->min_guaranteed;

	ret = bm_group_reserved_set(group_id,
				    group->reserved_buffers);
	if (unlikely(ret)) {
		pr_err("bm_group(%d)_reserved_set failed\n", group_id);
		goto add_pools_to_policy_done;
	}

add_pools_to_policy_done:
	return ret;
}

/**
 * @brief Remove a pool from a group
 * @note This function should be called under db lock
 * @param group_id groop id to remove pool from
 * @param pool_id pool id to remove from group
 * @return 0 on success, other error code on failure
 */
static s32 __remove_pool_from_group(u8 group_id, u8 pool_id)
{
	struct bmgr_group_db_entry *grp;
	struct bmgr_pool_db_entry *pool;

	if (!db->groups[group_id].is_busy)
		return -EINVAL;

	grp = &db->groups[group_id];
	pool = &db->pools[pool_id];

	list_del(&pool->group_link);
	grp->available_buffers -= pool->pool_params.num_buffers;

	/* Group's rsrvd buffers will be updated when configuring the policy */
	return bm_group_available_set(group_id, grp->available_buffers);
}

/**
 * @brief Remove pools from policy
 * @note This function should be called under db lock and assume
 *       group is not active
 * @param policy_id policy id to remove pools from
 * @param group_id groop id to remove pools from
 * @param policy_params pools in policy info
 * @return 0 on success, other error code on failure
 */
static s32
__remove_pools_from_policy(u16 policy_id,
			   u8 group_id,
			   struct pp_bmgr_policy_params *policy_params)
{
	struct bmgr_group_db_entry *group;
	s32 ret = 0;
	u8 pool_id;
	u8 idx = 0;

	/* Traverse pools in policy */
	for (idx = 0; idx < policy_params->num_pools_in_policy; idx++) {
		pool_id = policy_params->pools_in_policy[idx].pool_id;
		db->pools[pool_id].policy_ref_cnt--;

		/* Update group only if pool is not used by any policy */
		if (db->pools[pool_id].policy_ref_cnt == 0) {
			ret = __remove_pool_from_group(group_id, pool_id);
			if (unlikely(ret)) {
				pr_err("__remove_pool(%d)_from_group(%d) failed\n",
				       pool_id, group_id);
				goto remove_pools_from_policy_done;
			}
		}

		ret = bm_policy_pool_mapping_set(policy_id,
						 PP_BM_INVALID_POOL_ID,
						 idx, 0, true);
		if (unlikely(ret)) {
			pr_err("bm_policy(%d)_pool(%d)_mapping_set Failed\n",
			       policy_id, PP_BM_INVALID_POOL_ID);
			goto remove_pools_from_policy_done;
		}
	}

	/* Group entry in db */
	group = &db->groups[group_id];

	/* Reduce the group's reserved buffers */
	if (group->reserved_buffers < policy_params->min_guaranteed) {
		/* Sanity check in order not to go below 0 */
		pr_err("group %d rsrvd buffs (%d) < policy %d min guaranteed\n",
		       group_id,
		       group->reserved_buffers,
		       policy_params->min_guaranteed);
		group->reserved_buffers = 0;
	} else {
		group->reserved_buffers -= policy_params->min_guaranteed;
	}

	ret = bm_group_reserved_set(group_id,
				    group->reserved_buffers);
	if (unlikely(ret)) {
		pr_err("bm_group(%d)_reserved_set(%d) failed\n",
		       group_id, group->reserved_buffers);
		goto remove_pools_from_policy_done;
	}

remove_pools_from_policy_done:
	return ret;
}

/**
 * @brief get pool pop status
 * @param pool_id: Pool ID
 * @param enable: out arg, true if enabled, false if disabled
 * @return 0 on success, other error code on failure
 */
static s32 bmgr_pool_pop_status_get(u8 pool_id, bool *enable)
{
	*enable = true;

	/* Pool Pop Enable */
	if (db->cfg.pool_pop_hw_en)
		return bm_pool_pop_status_get(pool_id, enable);

	return 0;
}

/**
 * @brief Enable pool
 * @param pool_id: Pool ID
 * @param enable: True to enable, false to disable
 * @return 0 on success, other error code on failure
 */
static s32 __bmgr_pool_enable(u8 pool_id, bool enable)
{
	s32 ret = 0;

	ret = bm_pool_enable(pool_id, enable);
	if (unlikely(ret)) {
		pr_err("bm_pool_enable %d failed\n", pool_id);
		return -EINVAL;
	}

	ret = __bmgr_pool_pop_enable(pool_id, enable);
	if (unlikely(ret))
		pr_err("__bmgr_pool_pop_enable %d failed\n", pool_id);

	return ret;
}

static s32
__bmgr_pool_configure(const struct pp_bmgr_pool_params *const pool_params,
		      u8 *pool_id, bool lock)
{
	struct bmgr_pool_db_entry *pool;
	struct bmgr_pool_cfg cfg;
	u32 *temp_pointers_table_ptr = NULL;
	u32 idx = 0;
	s32 ret = 0;
	u64 user_array_ptr;
	bool pool_enable;

	/* Validity check */
	ret = __bmgr_is_pool_params_valid(pool_params);
	if (unlikely(ret))
		return ret;

	if (lock)
		__bmgr_db_lock();

	/* Find next available slot in pools db */
	if (*pool_id == PP_BM_INVALID_POOL_ID) {
		BM_FOR_EACH_POOL(db, idx)
		{
			if (db->pools[idx].is_busy == 0) {
				*pool_id = idx;
				break;
			}
		}

		/* If not found (Can be done also using num_pools) */
		if (idx == db->cfg.max_pools) {
			pr_err("bmgr_pool_configure: pools DB is full!\n");
			ret = -EIO;
			goto unlock;
		}
	}

	/* Verify pool is valid */
	if (*pool_id >= db->cfg.max_pools) {
		pr_err("Pool %d id not valid\n", *pool_id);
		ret = -EINVAL;
		goto unlock;
	}

	/* Verify pool is not active */
	if (db->pools[*pool_id].is_busy == 1) {
		pr_err("Pool %d id active\n", *pool_id);
		ret = -EINVAL;
		goto unlock;
	}

	pr_debug("Configuring pool %d\n", *pool_id);
	pool = &db->pools[*pool_id];

	/* Mark pool as busy before sleepable memory allocations */
	pool->is_busy = 1;

	if (lock)
		__bmgr_db_unlock();

	/* Allocate pool_param->pool_num_of_buff * POINTER_SIZE bytes array */
	pool->internal_ptrs_sz = PAGE_ALIGN(sizeof(unsigned int) *
					    pool_params->num_buffers);
	pool->internal_ptrs_virt = pp_dma_alloc(pool->internal_ptrs_sz,
						GFP_ATOMIC,
						&pool->internal_ptrs_phys);

	if (!pool->internal_ptrs_virt) {
		pr_err("Could not allocate %u bytes for pool %u pointers\n",
		       (u32)pool->internal_ptrs_sz, *pool_id);
		/* Mark pool as not used since pool allocation failed */
		pool->is_busy = 0;
		ret = -ENOMEM;
		goto pool_conf_done;
	}

	if (lock)
		__bmgr_db_lock();

	temp_pointers_table_ptr = (unsigned int *)pool->internal_ptrs_virt;

	user_array_ptr = (pool_params->base_addr_low) |
			 ((u64)pool_params->base_addr_high << 32);

	for (idx = 0; idx < pool_params->num_buffers; idx++) {
		*temp_pointers_table_ptr = user_array_ptr >> 6;
		temp_pointers_table_ptr++;
		user_array_ptr += pool_params->size_of_buffer;
	}
	pp_cache_writeback(pool->internal_ptrs_virt, pool->internal_ptrs_sz);

	cfg.num_buffers = pool_params->num_buffers;
	cfg.ptr_table_addr_low = (u32)pool->internal_ptrs_phys;
	cfg.ptr_table_addr_high = (u32)((u64)pool->internal_ptrs_phys >> 32);
	cfg.min_guaranteed_enable =
		pool_params->flags & POOL_ENABLE_FOR_MIN_GRNT_POLICY_CALC;

	ret = bm_pool_configure(*pool_id, &cfg);
	if (unlikely(ret)) {
		pr_err("bm_pool_configure %d failed\n", *pool_id);
		goto free_memory;
	}

	pool_enable = true;
	ret = __bmgr_pool_enable(*pool_id, pool_enable);
	if (unlikely(ret)) {
		pr_err("__bmgr_pool_enable %d failed\n", *pool_id);
		ret = -EINVAL;
		goto free_memory;
	}

	/* Update pool's DB */
	memcpy(&pool->pool_params, pool_params,
	       sizeof(struct pp_bmgr_pool_params));
	db->num_pools++;

	if (bm_ctrl_poll()) {
		pr_err("bm_ctrl_poll failed\n");
		ret = -EBUSY;
	}

unlock:
	if (lock)
		__bmgr_db_unlock();

pool_conf_done:
	pr_debug("done (%d)\n", ret);
	return ret;

free_memory:
	/* free pointers_table */
	pp_dma_free(pool->internal_ptrs_sz,
		    (void *)pool->internal_ptrs_virt,
		    &pool->internal_ptrs_phys);
	pool->is_busy = 0;
	__bmgr_db_unlock();
	return ret;
}

s32 pp_bmgr_pool_configure(const struct pp_bmgr_pool_params *const pool_params,
			   u8 *pool_id)
{
	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	return __bmgr_pool_configure(pool_params, pool_id, true);
}
EXPORT_SYMBOL(pp_bmgr_pool_configure);

s32 pp_bmgr_pool_pop_disable(u8 pool_id)
{
	bool pool_pop_enable;
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	/* Verify pool if is valid */
	if (pool_id >= db->cfg.max_pools) {
		pr_err("Pool %d id not valid\n", pool_id);
		return -EINVAL;
	}

	__bmgr_db_lock();

	/* Verify pool is active */
	if (db->pools[pool_id].is_busy == 0) {
		pr_err("Pool %d id not active\n", pool_id);
		ret = -EINVAL;
		goto unlock;
	}

	/* Verify pool is not used by any policy */
	if (unlikely(db->pools[pool_id].policy_ref_cnt)) {
		pr_err("Pool %d is in use by some policies\n", pool_id);
		ret = -EBUSY;
		goto unlock;
	}

	/* Disable pool pop */
	pool_pop_enable = false;
	ret = __bmgr_pool_pop_enable(pool_id, pool_pop_enable);
	if (unlikely(ret)) {
		pr_err("bmgr_pool_pop_enable %d failed\n", pool_id);
		goto unlock;
	}

	if (bm_ctrl_poll()) {
		pr_err("bm_ctrl_poll failed\n");
		ret = -EBUSY;
	}

unlock:
	__bmgr_db_unlock();

	return ret;
}
EXPORT_SYMBOL(pp_bmgr_pool_pop_disable);

static s32 __bmgr_pool_remove(u8 pool_id, bool lock)
{
	struct pp_bmgr_pool_stats pool_stats;
	struct bmgr_pool_db_entry *pool;
	struct bmgr_pool_cfg cfg;
	bool pool_enable;
	bool pool_pop_enabled;
	s32 ret = 0;

	pr_debug("Removing pool %d\n", pool_id);

	/* Verify pool if is valid */
	if (unlikely(pool_id >= db->cfg.max_pools)) {
		pr_err("Pool id %d not valid\n", pool_id);
		return -EINVAL;
	}
	if (lock)
		__bmgr_db_lock();

	/* Verify pool is active */
	if (unlikely(db->pools[pool_id].is_busy == 0)) {
		pr_err("Pool %d id not active\n", pool_id);
		ret = -EINVAL;
		goto unlock;
	}

	/* Verify pool pop is disabled */
	ret = bmgr_pool_pop_status_get(pool_id, &pool_pop_enabled);
	if (unlikely(ret)) {
		pr_err("bmgr_pool_pop_status_get %d failed\n", pool_id);
		goto unlock;
	}

	if (pool_pop_enabled) {
		pr_err("Pool %d pop is not disabled\n", pool_id);
		ret = -EBUSY;
		goto unlock;
	}

	/* Verify pool gained back its resources */
	ret = pp_bmgr_pool_stats_get(&pool_stats, pool_id);
	if (unlikely(ret)) {
		pr_err("Pool %d get stats error\n", pool_id);
		ret = -EINVAL;
		goto unlock;
	} else if (pool_stats.pool_allocated_ctr != 0) {
		pr_err("Pool %d resources are still in use\n", pool_id);
		ret = -EBUSY;
		goto unlock;
	}

	/* Disable pool */
	pool_enable = false;
	ret = __bmgr_pool_enable(pool_id, pool_enable);
	if (unlikely(ret)) {
		pr_err("__bmgr_pool_enable %d failed\n", pool_id);
		goto unlock;
	}

	pool = &db->pools[pool_id];

	/* free pointers_table */
	pp_dma_free(pool->internal_ptrs_sz,
		    (void *)pool->internal_ptrs_virt,
		    &pool->internal_ptrs_phys);

	/* Reset pool config */
	cfg.min_guaranteed_enable = false;
	cfg.num_buffers = 0;
	cfg.ptr_table_addr_high = 0;
	cfg.ptr_table_addr_low = 0;
	ret = bm_pool_configure(pool_id, &cfg);
	if (unlikely(ret)) {
		pr_err("bm_pool_configure Failed\n");
		goto unlock;
	}

	/* Update pool's DB */
	memset(&db->pools[pool_id], 0,
	       sizeof(struct bmgr_pool_db_entry));
	db->pools[pool_id].pool_id = pool_id;

	db->num_pools--;

	if (bm_ctrl_poll()) {
		pr_err("bm_ctrl_poll failed\n");
		ret = -EBUSY;
	}

unlock:
	if (lock)
		__bmgr_db_unlock();

	pr_debug("done (%d)\n", ret);
	return ret;
}

s32 pp_bmgr_pool_remove(u8 pool_id)
{
	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	return __bmgr_pool_remove(pool_id, true);
}
EXPORT_SYMBOL(pp_bmgr_pool_remove);

static s32
__bmgr_policy_configure(const struct pp_bmgr_policy_params *const plcy_params,
			u16 *policy_id, bool lock)
{
	struct bmgr_policy_cfg cfg;
	struct group_pause_config pause_cfg;
	u8 isolated_pools_in_policy = 0;
	u8 group_id = 0;
	s32 ret = 0;
	u16 idx;
	u8 pool_id;

	/* Validity check */
	ret = __bmgr_is_policy_params_valid(plcy_params);
	if (unlikely(ret))
		return ret;

	if (lock)
		__bmgr_db_lock();

	/* Find next available slot in policy db */
	if (*policy_id == PP_BM_INVALID_POLICY_ID) {
		BM_FOR_EACH_POLICY(db, idx)
		{
			if (db->policies[idx].is_busy == 0) {
				*policy_id = idx;
				break;
			}
		}

		/* If not found (Can be done also using num_policies) */
		if (idx == db->cfg.max_policies) {
			pr_err("No free policy!\n");
			ret = -EIO;
			goto unlock;
		}
	}

	/* Verify policy id is valid */
	if (unlikely(*policy_id >= db->cfg.max_policies)) {
		pr_err("Invalid policy id %d\n", *policy_id);
		ret = -EINVAL;
		goto unlock;
	}

	/* Verify policy is not active in db */
	if (unlikely(db->policies[*policy_id].is_busy == 1)) {
		pr_err("policy %d is active\n", *policy_id);
		ret = -EINVAL;
		goto unlock;
	}

	pr_debug("Configuring policy %d\n", *policy_id);

	/* Verify all pools in policy are active */
	for (idx = 0; idx < plcy_params->num_pools_in_policy; idx++) {
		pool_id = plcy_params->pools_in_policy[idx].pool_id;

		/* Verify pool in policy is valid */
		if (unlikely(pool_id >= db->cfg.max_pools)) {
			pr_err("pool_id %d out of range %d\n",
			       pool_id,
			       db->cfg.max_pools);
			ret = -EINVAL;
			goto unlock;
		}

		/* Verify pool in policy is active */
		if (unlikely(!db->pools[pool_id].is_busy)) {
			pr_err("pool_id %d is not allocated\n",
			       pool_id);
			ret = -EINVAL;
			goto unlock;
		}

		/* Verify isolated pool is not used by any policy */
		if (db->pools[pool_id].pool_params.flags &
		    POOL_ISOLATED) {
			if (db->pools[pool_id].policy_ref_cnt) {
				pr_err("Isolated Pool %d has a policy\n",
				       pool_id);
				ret = -EINVAL;
				goto unlock;
			}

			isolated_pools_in_policy++;
		}
	}

	/* If there is an isolated pool, all pools should be isolated */
	if (isolated_pools_in_policy) {
		if (isolated_pools_in_policy !=
		    plcy_params->num_pools_in_policy) {
			pr_err("Policy %d has mix of isolated pools\n",
			       *policy_id);
			ret = -EINVAL;
			goto unlock;
		}

		ret = __bmgr_allocate_group(&group_id);
		if (unlikely(ret)) {
			pr_err("__bmgr_allocate_group failed\n");
			goto unlock;
		}

		db->policies[*policy_id].is_isolated = true;
	} else {
		group_id = BM_DEFAULT_GROUP_ID;
	}

	/* Pause group before policy config */
	pause_cfg.pause = true;
	pause_cfg.validate_resources = true;
	ret = __group_pause(group_id, &pause_cfg);
	if (unlikely(ret)) {
		pr_err("Group %d pause failed\n", group_id);
		goto group_resume;
	}

	/* Add policy pools and update group */
	ret = __add_pools_to_policy(*policy_id, group_id, plcy_params);
	if (unlikely(ret)) {
		pr_err("Policy %d pool set\n", *policy_id);
		goto group_resume;
	}

	cfg.group_id = group_id;
	cfg.max_allowed = plcy_params->max_allowed;
	cfg.min_guaranteed = plcy_params->min_guaranteed;
	ret = bm_policy_configure(*policy_id, &cfg);
	if (unlikely(ret)) {
		pr_err("bm_policy(%d)_configure failed\n", *policy_id);
		goto group_resume;
	}

	/* Update Policy DB */
	db->num_policies++;
	db->policies[*policy_id].is_busy = 1;
	db->policies[*policy_id].group_id = group_id;
	memcpy(&db->policies[*policy_id].policy_params,
	       plcy_params,
	       sizeof(struct pp_bmgr_policy_params));

	if (bm_ctrl_poll()) {
		pr_err("bm_ctrl_poll failed\n");
		ret = -EBUSY;
	}

group_resume:
	/* Resume group */
	pause_cfg.pause = false;
	pause_cfg.validate_resources = false;
	ret = __group_pause(group_id, &pause_cfg);
	if (unlikely(ret))
		pr_err("Group %d resume failed\n", group_id);

unlock:
	if (lock)
		__bmgr_db_unlock();

	pr_debug("done (%d)\n", ret);
	return ret;
}

s32 pp_bmgr_policy_configure(const struct pp_bmgr_policy_params *const plcy_params,
			     u16 *policy_id)
{
	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	return __bmgr_policy_configure(plcy_params, policy_id, true);
}
EXPORT_SYMBOL(pp_bmgr_policy_configure);

static s32 __bmgr_policy_remove(u16 policy_id, bool lock)
{
	struct bmgr_group_db_entry *group;
	struct pp_bmgr_policy_params *plcy_params;
	struct group_pause_config pause_cfg;
	struct bmgr_policy_cfg cfg;
	s32 ret = 0;
	u8 group_id;

	pr_debug("Removing policy %d\n", policy_id);

	/* Verify policy id is valid */
	if (unlikely(policy_id >= db->cfg.max_policies)) {
		pr_err("Invalid policy id %d\n", policy_id);
		ret = -EINVAL;
		return ret;
	}

	if (lock)
		__bmgr_db_lock();

	/* Verify policy is active in db */
	if (unlikely(db->policies[policy_id].is_busy == 0)) {
		pr_err("policy %d is not active\n", policy_id);
		ret = -EINVAL;
		goto unlock;
	}

	group_id = db->policies[policy_id].group_id;

	/* Pause group before policy config */
	pause_cfg.pause = true;
	pause_cfg.validate_resources = true;
	ret = __group_pause(group_id, &pause_cfg);
	if (unlikely(ret)) {
		pr_err("Group %d pause failed\n", group_id);
		goto group_resume;
	}

	/* Fetch policy params from db */
	plcy_params = &db->policies[policy_id].policy_params;

	/* Add policy pools and update group */
	ret = __remove_pools_from_policy(policy_id, group_id, plcy_params);
	if (unlikely(ret)) {
		pr_err("Policy %d pool remove\n", policy_id);
		goto group_resume;
	}

	/* Reset policy */
	cfg.group_id = 0;
	cfg.max_allowed = 0;
	cfg.min_guaranteed = 0;
	ret = bm_policy_configure(policy_id, &cfg);
	if (unlikely(ret)) {
		pr_err("bm_policy_configure %d failed\n", policy_id);
		goto group_resume;
	}

	/* Handle Isolated Policy */
	if (db->policies[policy_id].is_isolated) {
		/* Group entry in db */
		group = &db->groups[group_id];
		group->is_busy = false;

		if (group->available_buffers ||
		    group->reserved_buffers ||
		    !list_empty(&group->pools_head)) {
			pr_err("Isolated group %d is not empty\n", group_id);
			group->available_buffers = 0;
			group->reserved_buffers = 0;
		}
	}

	/* Update Policy DB */
	db->num_policies--;
	memset(&db->policies[policy_id],
	       0, sizeof(struct bmgr_policy_db_entry));

	if (bm_ctrl_poll()) {
		pr_err("bm_ctrl_poll failed\n");
		ret = -EBUSY;
	}

group_resume:
	/* Resume group */
	pause_cfg.pause = false;
	pause_cfg.validate_resources = false;
	ret = __group_pause(group_id, &pause_cfg);
	if (unlikely(ret)) {
		pr_err("Group %d resume failed\n", group_id);
		goto group_resume;
	}
unlock:
	if (lock)
		__bmgr_db_unlock();

	pr_debug("done (%d)\n", ret);
	return ret;
}

s32 pp_bmgr_policy_remove(u16 policy_id)
{
	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	return __bmgr_policy_remove(policy_id, true);
}
EXPORT_SYMBOL(pp_bmgr_policy_remove);

static void __bmgr_work_handler(struct work_struct *w)
{
	struct group_pause_config pause_cfg;
	struct policy_reset_action *current_action;
	struct delayed_work *dwork;
	struct pp_bmgr_policy_params policy_params;
	struct pp_bmgr_pool_params pool_params[PP_BM_MAX_POOLS_PER_PLCY];
	u16 policy_reset_id;
	u8 pools_in_policy;
	u8 pool_idx;
	u8 pool_id;
	u8 group_id;
	s32 ret;

	/* Extract delayed work structure */
	dwork = container_of(w,
			     struct delayed_work,
			     work);

	/* Extract action structure */
	current_action = container_of(dwork,
				      struct policy_reset_action,
				      work);

	policy_reset_id = current_action->policy_reset_id;
	pr_debug("Work queue active - Policy %d\n", policy_reset_id);
	__bmgr_db_lock();

	/* Get policy */
	pr_debug("Copy policy %d from db\n", policy_reset_id);
	memcpy(&policy_params,
	       &db->policies[policy_reset_id].policy_params,
	       sizeof(struct pp_bmgr_policy_params));
	pools_in_policy = policy_params.num_pools_in_policy;

	/* Get pools */
	for (pool_idx = 0; pool_idx < pools_in_policy; pool_idx++) {
		pool_id = policy_params.pools_in_policy[pool_idx].pool_id;
		pr_debug("Copy pool %d from db\n", pool_id);

		memcpy(&pool_params[pool_idx],
		       &db->pools[pool_id].pool_params,
		       sizeof(struct pp_bmgr_pool_params));
	}

	/* Remove policy */
	ret = __bmgr_policy_remove(policy_reset_id, false);
	if (unlikely(ret)) {
		pr_err("__bmgr_policy_remove %d failed\n", policy_reset_id);
		goto unlock;
	}

	/* Re-Configure pools */
	for (pool_idx = 0; pool_idx < pools_in_policy; pool_idx++) {
		pool_id = policy_params.pools_in_policy[pool_idx].pool_id;

		ret = __bmgr_pool_remove(pool_id, false);
		if (unlikely(ret)) {
			pr_err("__bmgr_pool_remove %d failed\n", pool_id);
			goto unlock;
		}

		ret = __bmgr_pool_configure(&pool_params[pool_idx],
					    &pool_id, false);
		if (unlikely(ret)) {
			pr_err("__bmgr_pool_configure %d failed\n", pool_id);
			goto unlock;
		}

		ret = __bmgr_pool_pop_enable(pool_id, false);
		if (unlikely(ret)) {
			pr_err("__bmgr_pool_pop_enable %d failed\n", pool_id);
			goto unlock;
		}
	}

	/* Add policy */
	ret = __bmgr_policy_configure(&policy_params, &policy_reset_id, false);
	if (unlikely(ret)) {
		pr_err("__bmgr_policy_configure %d failed\n", policy_reset_id);
		goto unlock;
	}

	group_id = db->policies[policy_reset_id].group_id;

	/* Resume group */
	pause_cfg.pause = false;
	pause_cfg.validate_resources = false;
	if (unlikely(__group_pause(group_id, &pause_cfg))) {
		pr_err("Group %d resume failed\n", group_id);
		goto unlock;
	}

unlock:
	__bmgr_db_unlock();
	if (current_action->cb)
		(*current_action->cb)(ret);

	kfree(current_action);
	pr_debug("done (%d)\n", ret);
}

s32 pp_bmgr_policy_reset(u16 policy_id, void (*cb)(s32 ret))
{
	s32 ret = 0;
	u8 group_id;
	struct group_pause_config pause_cfg;
	struct policy_reset_action *action;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	/* Verify policy id is valid */
	if (unlikely(policy_id >= db->cfg.max_policies)) {
		pr_err("Invalid policy id %d\n", policy_id);
		return -EINVAL;
	}

	pr_debug("Resetting policy %d\n", policy_id);

	__bmgr_db_lock();

	/* Verify policy is active in db */
	if (unlikely(db->policies[policy_id].is_busy == 0)) {
		pr_err("policy %d is not active\n", policy_id);
		ret = -EINVAL;
		goto unlock;
	}

	/* Verify policy is isolated */
	if (unlikely(db->policies[policy_id].is_isolated == 0)) {
		pr_err("policy %d is not isolated\n", policy_id);
		ret = -EINVAL;
		goto unlock;
	}

	group_id = db->policies[policy_id].group_id;

	/* Pause group before policy reset */
	pause_cfg.pause = true;
	pause_cfg.validate_resources = false;
	ret = __group_pause(group_id, &pause_cfg);
	if (unlikely(ret)) {
		pr_err("Group %d pause failed\n", group_id);
		goto group_resume;
	}

	action = kzalloc(sizeof(*action), GFP_ATOMIC);
	if (!action) {
		ret = -ENOMEM;
		goto group_resume;
	}

	__bmgr_db_unlock();

	/* Trigger delayed work queue */
	INIT_DELAYED_WORK(&action->work, __bmgr_work_handler);
	action->policy_reset_id = policy_id;
	if (cb)
		action->cb = cb;

	queue_delayed_work(bmgr_wq,
			   &action->work,
			   msecs_to_jiffies(BM_POLICY_RESET_DELAY_MS));

	pr_debug("done (%d)\n", ret);
	return ret;

group_resume:
	/* Pause group before policy reset */
	pause_cfg.pause = false;
	pause_cfg.validate_resources = false;
	ret = __group_pause(group_id, &pause_cfg);
	if (unlikely(ret))
		pr_err("Group %d resume failed\n", group_id);

unlock:
	__bmgr_db_unlock();
	pr_debug("done (%d)\n", ret);
	return ret;
}
EXPORT_SYMBOL(pp_bmgr_policy_reset);

/**
 * @brief Get buffer manager configuration
 * @param regs returned config structure
 * @return 0 on success, other error code on failure
 */
s32 bmgr_get_cfg_regs(struct bmgr_cfg_regs *const regs)
{
	if (unlikely(!regs)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	bm_get_cfg_regs(regs);

	return 0;
}

s32 pp_bmgr_pool_conf_get(u8 pool_id,
			  struct pp_bmgr_pool_params *const pool_params)
{
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!pool_params)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	/* Validity check */
	if (unlikely(pool_id >= db->cfg.max_pools)) {
		pr_err("pool_id %d out of range %d\n",
		       pool_id,
		       db->cfg.max_pools);
		return -EINVAL;
	}

	__bmgr_db_lock();

	/* Active check */
	if (unlikely(!db->pools[pool_id].is_busy)) {
		pr_debug("pool_id %d is not active\n", pool_id);
		ret = -EINVAL;
		goto unlock;
	}

	memcpy(pool_params,
	       &db->pools[pool_id].pool_params,
	       sizeof(struct pp_bmgr_pool_params));

unlock:
	__bmgr_db_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_bmgr_pool_conf_get);

s32 pp_bmgr_pool_stats_get(struct pp_bmgr_pool_stats *const stats, u8 pool_id)
{
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!stats)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	/* Validity check */
	if (unlikely(pool_id >= db->cfg.max_pools)) {
		pr_err("pool_id %d out of range %d\n",
		       pool_id,
		       db->cfg.max_pools);
		return -EINVAL;
	}

	/* Active check */
	if (unlikely(!db->pools[pool_id].is_busy)) {
		pr_err("pool_id %d is not active\n", pool_id);
		return -EINVAL;
	}

	ret = bm_pool_stats_get(stats, pool_id);
	if (unlikely(ret))
		pr_err("bm_pool_stats_get %d failed\n", pool_id);

	return ret;
}
EXPORT_SYMBOL(pp_bmgr_pool_stats_get);

s32 pp_bmgr_policy_conf_get(u16 policy_id,
			    struct pp_bmgr_policy_params *const policy_params)
{
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!policy_params)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	/* Validity check */
	if (unlikely(policy_id >= db->cfg.max_policies)) {
		pr_err("policy_id %d out of range %d\n",
		       policy_id,
		       db->cfg.max_policies);
		return -EINVAL;
	}

	__bmgr_db_lock();

	/* Active check */
	if (unlikely(!db->policies[policy_id].is_busy)) {
		pr_debug("policy_id %d is not active\n", policy_id);
		ret = -EINVAL;
		goto unlock;
	}

	memcpy(policy_params,
	       &db->policies[policy_id].policy_params,
	       sizeof(struct pp_bmgr_policy_params));

unlock:
	__bmgr_db_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_bmgr_policy_conf_get);

s32 pp_bmgr_policy_stats_get(struct pp_bmgr_policy_stats *const stats,
			     u16 policy_id)
{
	u32 num_pools_in_policy;
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!stats)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	/* Validity check */
	if (unlikely(policy_id >= db->cfg.max_policies)) {
		pr_err("policy_id %d out of range %d\n", policy_id,
		       db->cfg.max_policies);
		return -EINVAL;
	}

	/* Active check */
	if (unlikely(!db->policies[policy_id].is_busy)) {
		pr_err("policy_id %d is not active\n", policy_id);
		return -EINVAL;
	}

	ret = bm_policy_stats_get(stats, policy_id);
	if (unlikely(ret))
		pr_err("bm_policy_stats_get %d failed\n", policy_id);

	num_pools_in_policy =
		db->policies[policy_id].policy_params.num_pools_in_policy;

	stats->policy_pools[0] = (stats->policy_pools_mapping & 0xFF);
	stats->policy_pools[1] =
		(num_pools_in_policy >= 2) ?
			(stats->policy_pools_mapping >> 8) & 0xFF :
			-1;
	stats->policy_pools[2] =
		(num_pools_in_policy >= 3) ?
			(stats->policy_pools_mapping >> 16) & 0xFF :
			-1;
	stats->policy_pools[3] =
		(num_pools_in_policy >= 4) ?
			(stats->policy_pools_mapping >> 24) & 0xFF :
			-1;

	return ret;
}
EXPORT_SYMBOL(pp_bmgr_policy_stats_get);

s32 bmgr_group_stats_get(struct bmgr_group_stats *const stats, u8 group_id)
{
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!stats)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	ret = bm_group_stats_get(group_id, stats);
	if (unlikely(ret))
		pr_err("bm_group_stats_get %d failed\n", group_id);

	return ret;
}

s32 pp_bmgr_stats_get(struct bmgr_stats *stats)
{
	if (unlikely(ptr_is_null(stats)))
		return -EINVAL;

	stats->active_pools = db->num_pools;
	stats->active_policies = db->num_policies;

	return 0;
}

s32 bmgr_config_set(const struct pp_bmgr_init_param *const cfg)
{
	s32 ret = 0;
	u32 policy_id;
	u8 group_id;
	u8 pool_id;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!cfg)) {
		pr_err("Null cfg\n");
		return -EINVAL;
	}

	pr_debug("Ext-config: pools(%d), groups(%d), policies(%d)\n",
		 cfg->max_pools, cfg->max_groups, cfg->max_policies);

	memcpy(&db->cfg, cfg, sizeof(struct pp_bmgr_init_param));

	if (unlikely(db->cfg.max_pools > PP_BM_MAX_POOLS)) {
		pr_err("Max pools %d exceeds maximum of %d\n",
		       db->cfg.max_pools,
		       PP_BM_MAX_POOLS);
		ret = -EINVAL;
		goto reset_cfg_db;
	}

	if (unlikely(db->cfg.max_groups > BM_MAX_GROUPS)) {
		pr_err("Max groups %d exceeds maximum of %d\n",
		       db->cfg.max_groups,
		       BM_MAX_GROUPS);
		ret = -EINVAL;
		goto reset_cfg_db;
	}

	if (unlikely(db->cfg.max_policies > PP_BM_MAX_POLICIES)) {
		pr_err("Max groups %d exceeds maximum of %d\n",
		       db->cfg.max_policies,
		       PP_BM_MAX_POLICIES);
		ret = -EINVAL;
		goto reset_cfg_db;
	}

	__bmgr_db_lock();

	/* Init RAM */
	BM_FOR_EACH_POLICY(db, policy_id)
	{
		ret = bm_policy_default_set(policy_id);
		if (unlikely(ret)) {
			pr_err("bm_policy_default_set %d failed\n", policy_id);
			goto unlock;
		}
	}

	BM_FOR_EACH_GROUP(db, group_id)
	INIT_LIST_HEAD(&db->groups[group_id].pools_head);

	/* Set default group 0 active */
	db->groups[BM_DEFAULT_GROUP_ID].is_busy = 1;

	BM_FOR_EACH_POOL(db, pool_id)
	db->pools[pool_id].pool_id = pool_id;

unlock:
	__bmgr_db_unlock();

	return ret;

reset_cfg_db:
	memset(&db->cfg, 0, sizeof(struct pp_bmgr_init_param));
	return ret;
}

s32 pp_bmgr_config_get(struct pp_bmgr_init_param *const cfg)
{
	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (unlikely(!cfg)) {
		pr_err("NULL argument\n");
		return -EINVAL;
	}

	/* Store configuration in db */
	cfg->max_pools = db->cfg.max_pools;
	cfg->max_groups = db->cfg.max_groups;
	cfg->max_policies = db->cfg.max_policies;
	cfg->pool_pop_hw_en = db->cfg.pool_pop_hw_en;

	return 0;
}
EXPORT_SYMBOL(pp_bmgr_config_get);

bool pp_bmgr_is_policy_active(u16 policy)
{
	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	/* Verify policy id is valid */
	if (unlikely(policy >= db->cfg.max_policies)) {
		pr_err("Invalid policy id %d\n", policy);
		return false;
	}

	/* Check if policy is active in db */
	if (db->policies[policy].is_busy == 0)
		return false;
	else
		return true;
}
EXPORT_SYMBOL(pp_bmgr_is_policy_active);

s32 pp_bmgr_set_total_active_pools(u32 num_active_pools)
{
	s32 ret = 0;

	if (unlikely(!__bmgr_is_ready()))
		return -EPERM;

	if (num_active_pools > db->cfg.max_pools) {
		pr_err("Number active pools %d is not valid\n",
		       num_active_pools);
		return -EINVAL;
	}

	__bmgr_db_lock();
	ret = bm_set_total_active_pools(num_active_pools);
	if (unlikely(ret))
		goto unlock;

unlock:
	__bmgr_db_unlock();

	return ret;
}
EXPORT_SYMBOL(pp_bmgr_set_total_active_pools);

s32 pp_bmgr_init(const struct pp_bmgr_init_param *init_param)
{
	if (!init_param->valid)
		return -EINVAL;

	db = kzalloc(sizeof(*db), GFP_KERNEL);
	if (unlikely(!db))
		return -ENOMEM;

	spin_lock_init(&db->db_lock);

	if (unlikely(bm_dbg_init(db, init_param->dbgfs))) {
		pr_err("Failed to init debug stuff\n");
		return -ENOMEM;
	}

	bmgr_wq = alloc_workqueue("pp-buffer-mgr", 0, 1);
	qos_uc_base_addr = init_param->qos_uc_base;
	db->ready = true;

	if (unlikely(bmgr_config_set(init_param))) {
		pr_err("Failed to set bmgr config\n");
		db->ready = false;
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(pp_bmgr_init);

void pp_bmgr_exit(void)
{
	pr_debug("start\n");
	if (unlikely(!db))
		return;

	db->ready = false;
	if (bmgr_wq)
		destroy_workqueue(bmgr_wq);

	kfree(db);
	db = NULL;
	bm_dbg_clean();

	pr_debug("done\n");
}
EXPORT_SYMBOL(pp_bmgr_exit);
