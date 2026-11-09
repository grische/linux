/*
 * pp_bm_tests.c
 * Description: Packet Processor unit tests
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018-2020 Intel Corporation
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "../pput.h"
#include "bm.h"
#include "pp_buffer_mgr.h"

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "[PP_BM_TEST] %s:%d: " fmt, __func__, __LINE__
#endif

#ifdef CONFIG_PPV4_LGM
#define BM_TEST_POOLS (16)
#define BM_TEST_POLICIES (128)
#define BM_TEST_GROUPS (16)
#else
#define BM_TEST_POOLS (4)
#define BM_TEST_POLICIES (8)
#define BM_TEST_GROUPS (2)
#endif

#define BM_TEST_MAX_ALLOWED_PER_POOL (16)

#define BM_POOL_BUFFERS (64)
#define BM_POOL_BUFFER_SIZE (2048)
#define BM_ALLOC_SZ (BM_POOL_BUFFERS * BM_POOL_BUFFER_SIZE)

static void *bm_virt_ddr[BM_TEST_POOLS];
static dma_addr_t bm_phys_ddr[BM_TEST_POOLS];

bool bm_restore_cfg;
struct pp_bmgr_init_param cfg;

struct pool_config {
	bool is_active;
	struct pp_bmgr_pool_params params;
};

struct policy_config {
	bool is_active;
	struct pp_bmgr_policy_params params;
};

struct pool_config *pool_cfg;
struct policy_config *policy_cfg;

static int pp_bm_pre_test(void *data);
static int pp_bm_configuration_test(void *data);
#ifdef PP_BM_ALLOCATION_TEST
static int pp_bm_allocation_test(void *data);
#endif
static int pp_bm_post_test(void *data);

static struct pp_test_t pp_bm_tests[] = {
	{
		.component = PP_BM_TESTS,
		.test_name = "pp_bm_pre_test",
		.level_bitmap = PP_UT_LEVEL_FULL |
				PP_UT_LEVEL_MUST_PASS |
				PP_UT_LEVEL_MUST_RUN,
		.test_data = NULL,
		.fn = pp_bm_pre_test,
	},
	{
		.component = PP_BM_TESTS,
		.test_name = "pp_bm_configuration_test",
		.level_bitmap = PP_UT_LEVEL_FULL |
				PP_UT_LEVEL_MUST_PASS,
		.test_data = NULL,
		.fn = pp_bm_configuration_test,
	},
#ifdef PP_BM_ALLOCATION_TEST
	{
		.component = PP_BM_TESTS,
		.test_name = "pp_bm_allocation_test",
		.level_bitmap = PP_UT_LEVEL_FULL,
		.test_data = NULL,
		.fn = pp_bm_allocation_test,
	},
#endif
	{
		.component = PP_BM_TESTS,
		.test_name = "pp_bm_post_test",
		.level_bitmap = PP_UT_LEVEL_FULL |
				PP_UT_LEVEL_MUST_PASS |
				PP_UT_LEVEL_MUST_RUN,
		.test_data = NULL,
		.fn = pp_bm_post_test,
	},
};

#ifdef PP_BM_ALLOCATION_TEST
static bool __validate_allocation(u8 pool_id, u16 policy_id, u32 num_buffers)
{
	struct pp_bmgr_pool_stats pool_stats;
	struct pp_bmgr_policy_stats policy_stats;

	/* Get Pool Stats */
	if (unlikely(pp_bmgr_pool_stats_get(&pool_stats, pool_id))) {
		pr_info("Getting pool %d stats failed\n", pool_id);
		goto validate_allocation_fail;
	}

	/* Get Policy Stats */
	if (unlikely(pp_bmgr_policy_stats_get(&policy_stats, policy_id))) {
		pr_info("Getting policy %d stats failed\n", policy_id);
		goto validate_allocation_fail;
	}

	/* Verify Pool Allocated Statistics */
	if (unlikely(pool_stats.pool_allocated_ctr != num_buffers)) {
		pr_info("Pool Allocated is %d instead of %d\n",
			pool_stats.pool_allocated_ctr, num_buffers);
		goto validate_allocation_fail;
	}

	/* Verify Policy Allocated Statistics */
	if (unlikely(policy_stats.policy_alloc_buff != num_buffers)) {
		pr_info("Policy Allocated is %d instead of %d\n",
			policy_stats.policy_alloc_buff, num_buffers);
		goto validate_allocation_fail;
	}

	return true;

validate_allocation_fail:
	return false;
}

static s32 __bm_validate_buffer(struct pp_bmgr_buff_info *buff_info,
				u8 pool_id)
{
	/* Verify pool is valid */
	if (unlikely(buff_info->pool_id[0] != pool_id)) {
		pr_info("Pool id is %d instead of %d\n",
			buff_info->pool_id[0], pool_id);
		return -1;
	}

	/* Make sure buffer is in address range */
	if ((buff_info->addr_low[0] <
	     DDR_BM_BUFF_POOL_BASE(pool_id)) ||
	    (buff_info->addr_low[0] >
	     DDR_BM_BUFF_POOL_BASE(pool_id + 1))) {
		pr_info("Buff 0x%x is not in pool %d range\n",
			buff_info->addr_low[0], pool_id);
		return -1;
	}

	return 0;
}
#endif

static s32 __bm_test_configure(void)
{
	cfg.max_pools = BM_TEST_POOLS;
	cfg.max_groups = BM_TEST_GROUPS;
	cfg.max_policies = BM_TEST_POLICIES;
	cfg.max_pools_in_policy = 2;
	if (IS_ENABLED(CONFIG_PPV4_LGM))
		cfg.pool_pop_hw_en = true;
	else
		cfg.pool_pop_hw_en = false;

	return bmgr_config_set(&cfg);
}

#ifdef PP_BM_ALLOCATION_TEST
static int pp_bm_allocation_test(void *data)
{
	struct pp_bmgr_buff_info *buff_info;
	struct pp_bmgr_policy_params policy_params;
	u32 idx;
	u16 policy_id;
	u8 pool_id;

	buff_info = kcalloc(BM_TEST_MAX_ALLOWED_PER_POOL + 1,
			    sizeof(struct pp_bmgr_buff_info),
			    GFP_KERNEL);

	/* Pop all buffers from policy */
	for (policy_id = 0; policy_id < cfg.max_policies; policy_id++) {
		if (unlikely(pp_bmgr_policy_conf_get(policy_id,
						     &policy_params))) {
			pr_info("Config get for policy %d failed\n",
				policy_id);
			goto pp_bm_allocation_test_fail;
		}

		pool_id = policy_params.pools_in_policy[0].pool_id;

		/* Pop buffers */
		for (idx = 0; idx < BM_TEST_MAX_ALLOWED_PER_POOL; idx++) {
			buff_info[idx].num_allocs = 1;
			buff_info[idx].policy_id = policy_id;
			if (unlikely(bmgr_pop_buffer(&buff_info[idx]))) {
				pr_info("Pop from policy %d failed\n",
					policy_id);
				goto pp_bm_allocation_test_fail;
			}

			if (unlikely(__bm_validate_buffer(&buff_info[idx],
							  pool_id))) {
				pr_info("Pop from policy %d failed\n",
					policy_id);
				goto pp_bm_allocation_test_fail;
			}
		}

		/* Try popping extra buffer */
		buff_info[idx].num_allocs = 1;
		buff_info[idx].policy_id = policy_id;
		if (unlikely(bmgr_pop_buffer(&buff_info[idx]))) {
			pr_info("Pop from policy %d failed\n", policy_id);
			goto pp_bm_allocation_test_fail;
		}

		/* Pop should fail */
		if (unlikely(!__bm_validate_buffer(&buff_info[idx],
						   pool_id))) {
			pr_info("Error: Extra pop from policy %d succeeded\n",
				policy_id);
			goto pp_bm_allocation_test_fail;
		}

		if (unlikely(!__validate_allocation(pool_id, policy_id,
						    BM_TEST_MAX_ALLOWED_PER_POOL))) {
			pr_info("Allocation validation failed\n");
			goto pp_bm_allocation_test_fail;
		}

		/* Push buffers back */
		for (idx = 0; idx < BM_TEST_MAX_ALLOWED_PER_POOL; idx++) {
			if (unlikely(bmgr_push_buffer(&buff_info[idx]))) {
				pr_info("Pop from policy %d failed\n",
					policy_id);
				goto pp_bm_allocation_test_fail;
			}
		}

		if (unlikely(!__validate_allocation(pool_id, policy_id, 0))) {
			pr_info("Allocation validation failed\n");
			goto pp_bm_allocation_test_fail;
		}
	}

	kfree(buff_info);
	return PP_UT_PASS;

pp_bm_allocation_test_fail:
	kfree(buff_info);
	return PP_UT_FAIL;
}
#endif

static int pp_bm_configuration_test(void *data)
{
	u8 pool_id;
	u16 policy_id;
	struct pp_bmgr_policy_params policy_params;
	struct pp_bmgr_pool_params pool_params;

	/* Create Pools */
	for (pool_id = 0; pool_id < cfg.max_pools; pool_id++) {
		pool_params.flags = POOL_ENABLE_FOR_MIN_GRNT_POLICY_CALC;
		bm_virt_ddr[pool_id] = pp_dma_alloc(BM_ALLOC_SZ,
						    GFP_KERNEL,
						    &bm_phys_ddr[pool_id]);
		if (!bm_virt_ddr[pool_id]) {
			pr_err("Could not allocate %u bytes for buffer manager\n",
			       BM_ALLOC_SZ);
			return -ENOMEM;
		}

		pool_params.num_buffers = BM_POOL_BUFFERS;
		pool_params.base_addr_low = (u32)(bm_phys_ddr[pool_id]);
		pool_params.base_addr_high =
			(u32)((u64)bm_phys_ddr[pool_id] << 32);
		pool_params.size_of_buffer = BM_POOL_BUFFER_SIZE;
		if (pp_bmgr_pool_configure(&pool_params, &pool_id)) {
			pr_info("Configuring pool %d failed\n", pool_id);
			goto pp_bm_configuration_test_fail;
		}
	}

	/* Try creating extra pool */
	if (unlikely(!pp_bmgr_pool_configure(&pool_params, &pool_id))) {
		pr_info("Error: Configuring extra pool %d succeeded\n",
			pool_id);
		goto pp_bm_configuration_test_fail;
	}

	/* Create Policies, leave last 2 pools not associated */
	for (policy_id = 0; policy_id < cfg.max_policies; policy_id++) {
		policy_params.max_allowed = 32;
		policy_params.min_guaranteed = 8;
		policy_params.num_pools_in_policy = 1;
		policy_params.pools_in_policy[0].pool_id =
			policy_id % (cfg.max_pools - 2);
		policy_params.pools_in_policy[0].max_allowed =
			BM_TEST_MAX_ALLOWED_PER_POOL;
		if (unlikely(pp_bmgr_policy_configure(&policy_params,
						      &policy_id))) {
			pr_info("Configuring policy %d failed\n", policy_id);
			goto pp_bm_configuration_test_fail;
		}
	}

	/* Try creating extra policy */
	if (unlikely(!pp_bmgr_policy_configure(&policy_params, &policy_id))) {
		pr_info("Error: Configuring extra policy %d succeeded\n",
			policy_id);
		goto pp_bm_configuration_test_fail;
	}

	/* Re-configure last pool as isolated */
	pool_id = cfg.max_pools - 1;
	pool_params.flags =
		POOL_ENABLE_FOR_MIN_GRNT_POLICY_CALC | POOL_ISOLATED;

	pool_params.num_buffers = BM_POOL_BUFFERS;
	pool_params.base_addr_low = (u32)(bm_phys_ddr[pool_id]);
	pool_params.base_addr_high =
		(u32)(((u64)bm_phys_ddr[pool_id]) << 32);
	pool_params.size_of_buffer = BM_POOL_BUFFER_SIZE;

	if (unlikely(pp_bmgr_pool_pop_disable(pool_id))) {
		pr_info("Error: Pop disable for pool %d failed\n", pool_id);
		goto pp_bm_configuration_test_fail;
	}
	if (unlikely(pp_bmgr_pool_remove(pool_id))) {
		pr_info("Error: pool %d remove failed\n", pool_id);
		goto pp_bm_configuration_test_fail;
	}
	if (pp_bmgr_pool_configure(&pool_params, &pool_id)) {
		pr_info("Configuring pool %d failed\n", pool_id);
		goto pp_bm_configuration_test_fail;
	}

	/* Try Reset non-isolated policy */
	if (unlikely(!pp_bmgr_policy_reset(cfg.max_policies - 1, NULL))) {
		pr_info("Error: non isolated policy %d reset succeeded\n",
			policy_id);
		goto pp_bm_configuration_test_fail;
	}

	/* Remove last 2 policies */
	if (unlikely(pp_bmgr_policy_remove(cfg.max_policies - 2))) {
		pr_info("policy %d remove failed\n", cfg.max_policies - 2);
		goto pp_bm_configuration_test_fail;
	}

	policy_id = cfg.max_policies - 1;
	if (unlikely(pp_bmgr_policy_remove(policy_id))) {
		pr_info("policy %d remove failed\n", policy_id);
		goto pp_bm_configuration_test_fail;
	}

	policy_params.max_allowed = 32;
	policy_params.min_guaranteed = 8;

	/* Try Re-configure last policy as mixed isolated */
	policy_params.num_pools_in_policy = 2;
	policy_params.pools_in_policy[0].pool_id = cfg.max_pools - 1;
	policy_params.pools_in_policy[0].max_allowed =
		BM_TEST_MAX_ALLOWED_PER_POOL;
	policy_params.pools_in_policy[1].pool_id = cfg.max_pools - 2;
	policy_params.pools_in_policy[1].max_allowed =
		BM_TEST_MAX_ALLOWED_PER_POOL;
	if (unlikely(!pp_bmgr_policy_configure(&policy_params, &policy_id))) {
		pr_info("Error: Configuring policy %d as mixed isolated succeeded",
			policy_id);
		goto pp_bm_configuration_test_fail;
	}

	/* Re-configure last policy as isolated */
	policy_params.num_pools_in_policy = 1;
	policy_params.pools_in_policy[0].pool_id = cfg.max_pools - 1;
	policy_params.pools_in_policy[0].max_allowed =
		BM_TEST_MAX_ALLOWED_PER_POOL;
	if (unlikely(pp_bmgr_policy_configure(&policy_params, &policy_id))) {
		pr_info("Error: Configuring policy %d failed\n",
			policy_id);
		goto pp_bm_configuration_test_fail;
	}

	/* Try configure 2nd last policy with an isolated pool already used */
	policy_id = cfg.max_policies - 2;
	if (unlikely(!pp_bmgr_policy_configure(&policy_params, &policy_id))) {
		pr_info("Error: Configuring policy %d with a used isolated pool succeeded\n",
			policy_id);
		goto pp_bm_configuration_test_fail;
	}

	/* configure 2nd last policy properly */
	policy_id = cfg.max_policies - 2;
	policy_params.num_pools_in_policy = 1;
	policy_params.pools_in_policy[0].pool_id = cfg.max_pools - 2;
	policy_params.pools_in_policy[0].max_allowed =
		BM_TEST_MAX_ALLOWED_PER_POOL;
	if (unlikely(pp_bmgr_policy_configure(&policy_params, &policy_id))) {
		pr_info("Configuring policy %d wfailed\n",
			policy_id);
		goto pp_bm_configuration_test_fail;
	}

	return PP_UT_PASS;

pp_bm_configuration_test_fail:
	return PP_UT_FAIL;
}

static int pp_bm_post_test(void *data)
{
	u8 pool_id;
	u16 policy_id;
	struct pp_bmgr_policy_params *policy_params;

	/* Delete Test Policies */
	for (policy_id = 0; policy_id < cfg.max_policies; policy_id++) {
		if (unlikely(pp_bmgr_policy_remove(policy_id)))
			pr_info("Removing policy %d failed\n", policy_id);
	}

	/* Delete Test Pools */
	for (pool_id = 0; pool_id < cfg.max_pools; pool_id++) {
		if (unlikely(pp_bmgr_pool_pop_disable(pool_id)))
			pr_info("Disabling pool %d failed\n", pool_id);
		if (unlikely(pp_bmgr_pool_remove(pool_id)))
			pr_info("Removing pool %d failed\n", pool_id);
		/* Release pool memory */
		pp_dma_free(BM_ALLOC_SZ,
			    bm_virt_ddr[pool_id],
			    bm_phys_ddr[pool_id]);
	}

	/* Reset bm config if no need to restore previous config */
	if (!bm_restore_cfg) {
		/* Reset test config */
		memset(&cfg, 0, sizeof(struct pp_bmgr_init_param));
		if (bmgr_config_set(&cfg)) {
			pr_info("bmgr_config_set failed\n");
			return PP_UT_FAIL;
		}

		return PP_UT_PASS;
	}

	/* Restore previous config */
	if (unlikely(bmgr_config_set(&cfg))) {
		pr_info("bmgr_config_set failed\n");
		goto pp_bm_post_test_config_restore_fail;
	}

	/* Restore previous pool config */
	for (pool_id = 0; pool_id < cfg.max_pools; pool_id++) {
		if (pool_cfg[pool_id].is_active) {
			if (pp_bmgr_pool_configure(&pool_cfg[pool_id].params,
						   &pool_id)) {
				pr_info("Configuring pool %d failed\n",
					pool_id);
				goto pp_bm_post_test_config_restore_fail;
			}
		}
	}

	/* Restore policy config */
	for (policy_id = 0; policy_id < cfg.max_policies; policy_id++) {
		if (policy_cfg[policy_id].is_active) {
			policy_params = &policy_cfg[policy_id].params;
			if (pp_bmgr_policy_configure(policy_params,
						     &policy_id)) {
				pr_info("Removing policy %d failed\n",
					policy_id);
				goto pp_bm_post_test_config_restore_fail;
			}
		}
	}

	kfree(pool_cfg);
	kfree(policy_cfg);
	return PP_UT_PASS;

pp_bm_post_test_config_restore_fail:
	kfree(pool_cfg);
	kfree(policy_cfg);
	return PP_UT_FAIL;
}

static int pp_bm_pre_test(void *data)
{
	u32 idx;

	if (unlikely(pp_bmgr_config_get(&cfg))) {
		pr_info("pp_bmgr_config_get fail\n");
		goto pp_bm_pre_test_done;
	}

	/* If max pools is not 0, need to restore bm configurations */
	bm_restore_cfg = cfg.max_pools ? true : false;

	/* If no existing configuration go and load test config */
	if (!bm_restore_cfg)
		goto pp_bm_pre_test_configure;

	/* Allocate pool cfg array in order to store pool config */
	pool_cfg = kcalloc(cfg.max_pools,
			   sizeof(struct pool_config),
			   GFP_KERNEL);
	if (unlikely(!pool_cfg))
		goto pp_bm_pre_test_config_store_fail;

	/* Allocate policy cfg array in order to store policy config */
	policy_cfg = kcalloc(cfg.max_policies,
			     sizeof(struct policy_config),
			     GFP_KERNEL);
	if (unlikely(!policy_cfg))
		goto pp_bm_pre_test_config_store_fail;

	/* Get existing policy config */
	for (idx = 0; idx < cfg.max_policies; idx++) {
		if (!pp_bmgr_policy_conf_get(idx, &policy_cfg[idx].params)) {
			policy_cfg[idx].is_active = true;
			if (pp_bmgr_policy_remove(idx)) {
				pr_info("Removing policy %d failed\n", idx);
				goto pp_bm_pre_test_config_store_fail;
			}
		}
	}

	/* Get existing pool config */
	for (idx = 0; idx < cfg.max_pools; idx++) {
		if (!pp_bmgr_pool_conf_get(idx, &pool_cfg[idx].params)) {
			pool_cfg[idx].is_active = true;
			if (unlikely(pp_bmgr_pool_pop_disable(idx))) {
				pr_info("Pool %d disable failed\n", idx);
				goto pp_bm_pre_test_config_store_fail;
			}
			if (unlikely(pp_bmgr_pool_remove(idx))) {
				pr_info("Removing pool %d failed\n", idx);
				goto pp_bm_pre_test_config_store_fail;
			}
		}
	}

pp_bm_pre_test_configure:
	/* Configure bm for test */
	if (unlikely(__bm_test_configure())) {
		pr_info("__bm_test_configure fail\n");
		return PP_UT_FAIL;
	}

pp_bm_pre_test_done:
	return PP_UT_PASS;

pp_bm_pre_test_config_store_fail:
	kfree(pool_cfg);
	kfree(policy_cfg);
	return PP_UT_FAIL;
}

/**
 * Register all pp_bm tests to pput
 */
void pp_bm_tests_init(void)
{
	int test_idx;

	/* Add Tests */
	for (test_idx = 0; test_idx < ARRAY_SIZE(pp_bm_tests); test_idx++)
		pp_register_test(&pp_bm_tests[test_idx]);
}
