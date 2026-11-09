// SPDX-License-Identifier: GPL-2.0+

#define pr_fmt(fmt) "avm_kpi_test: " fmt

#include "avm_kpi.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>

#define MAX_TEST_ATTR 32u
#define MAX_TEST_SAMPLER 16u
#define TEST_ARRAY_LEN 51u

#ifndef U8_MAX
#define U8_MAX  ((u8)~0U)
#define S8_MAX  ((s8)(U8_MAX >> 1))
#define S8_MIN  ((s8)(-S8_MAX - 1))
#define U16_MAX ((u16)~0U)
#define S16_MAX ((s16)(U16_MAX >> 1))
#define S16_MIN ((s16)(-S16_MAX - 1))
#define U32_MAX ((u32)~0U)
#define S32_MAX ((s32)(U32_MAX >> 1))
#define S32_MIN ((s32)(-S32_MAX - 1))
#define U64_MAX ((u64)~0ULL)
#define S64_MAX ((s64)(U64_MAX >> 1))
#define S64_MIN ((s64)(-S64_MAX - 1))
#endif

static struct kpi_node *test_node;
static struct kpi_attribute_absolute *attributes[MAX_TEST_ATTR];
static struct kpi_sampler *samplers[MAX_TEST_SAMPLER];
static u32 u32_ascending[TEST_ARRAY_LEN];
static u64 u64_ascending[TEST_ARRAY_LEN];
static s32 s32_ascending[TEST_ARRAY_LEN];
static s64 s64_ascending[TEST_ARRAY_LEN];

static void u32_ascending_collector(void *ctx, u32 *dest, size_t num)
{
	const size_t len = sizeof(*dest) * num;

	WARN_ON(len != sizeof(u32_ascending));
	memcpy(dest, u32_ascending, len);
}

static void u64_ascending_collector(void *ctx, u64 *dest, size_t num)
{
	const size_t len = sizeof(*dest) * num;

	WARN_ON(len != sizeof(u64_ascending));
	memcpy(dest, u64_ascending, len);
}

static void s32_ascending_collector(void *ctx, s32 *dest, size_t num)
{
	const size_t len = sizeof(*dest) * num;

	WARN_ON(len != sizeof(s32_ascending));
	memcpy(dest, s32_ascending, len);
}

static void s64_ascending_collector(void *ctx, s64 *dest, size_t num)
{
	const size_t len = sizeof(*dest) * num;

	WARN_ON(len != sizeof(s64_ascending));
	memcpy(dest, s64_ascending, len);
}

static u32 positive_sequence(void *ctx)
{
	static u32 i;

	return i++;
}

static s32 negative_sequence(void *ctx)
{
	static s32 i;

	return 0 - i++;
}

static s32 random_32(void *ctx)
{
	s32 num;

	get_random_bytes(&num, sizeof(num));
	return num;
}

static s64 random_64(void *ctx)
{
	s64 num;

	get_random_bytes(&num, sizeof(num));
	return num;
}

static void str_collect(void *ctx, char *buf, size_t buffer_len)
{
	strcpy(buf, "Hello, World!");
}

static void __init init_arrays(void)
{
	unsigned int i;
	u32 u32_start = 0;
	u64 u64_start = U32_MAX;
	s32 s32_start = S32_MIN;
	s64 s64_start = S64_MIN;

	for (i = 0; i < TEST_ARRAY_LEN; ++i) {
		u32_ascending[i] = u32_start++;
		u64_ascending[i] = u64_start++;
		s32_ascending[i] = s32_start++;
		s64_ascending[i] = s64_start++;
	}
}

static int __init avm_kpi_test_init(void)
{
	const enum kpi_reducer reducers[] = {
		KPI_REDUCER_NONE,     KPI_REDUCER_MIN,      KPI_REDUCER_MAX,
		KPI_REDUCER_SUM,      KPI_REDUCER_AVG,      KPI_REDUCER_MEDIAN,
		KPI_REDUCER_VARIANCE, _KPI_REDUCER_BOTTOM_,
	};
	const struct kpi_sliding_window window = {
		.size_seconds = 60,
		.num_samples = 12,
	};
	struct kpi_node *system_node;
	unsigned int a = 0, s = 0;

	init_arrays();

	system_node = kpi_get_section("system");
	if (!system_node)
		return -EPERM;

	test_node = create_kpi_node("test", NULL);
	if (!test_node)
		return -EPERM;

	attributes[a++] = kpi_add_attr_u32("sessions", test_node,
					   positive_sequence, NULL);

	attributes[a++] = kpi_add_attr_string("version", test_node, str_collect,
					      NULL, 100u);

	attributes[a++] = kpi_add_attr_string("test", system_node, str_collect,
					      NULL, 100u);

	attributes[a++] = kpi_add_attr_u32_arr("u32_array", test_node,
					       u32_ascending_collector,
					       TEST_ARRAY_LEN, NULL);

	attributes[a++] = kpi_add_attr_u64_arr("u64_array", test_node,
					       u64_ascending_collector,
					       TEST_ARRAY_LEN, NULL);
	attributes[a++] = kpi_add_attr_s32_arr("s32_array", test_node,
					       s32_ascending_collector,
					       TEST_ARRAY_LEN, NULL);

	attributes[a++] = kpi_add_attr_s64_arr("s64_array", test_node,
					       s64_ascending_collector,
					       TEST_ARRAY_LEN, NULL);
	samplers[s++] =
		kpi_add_sampler_u32("psequence", test_node, positive_sequence,
				    NULL, window, reducers);

	samplers[s++] =
		kpi_add_sampler_s32("nsequence", test_node, negative_sequence,
				    NULL, window, reducers);

	samplers[s++] = kpi_add_sampler_s32("rand32", test_node, random_32,
					    NULL, window, reducers);

	samplers[s++] = kpi_add_sampler_s64("rand64", test_node, random_64,
					    NULL, window, reducers);

	WARN_ON(a > MAX_TEST_ATTR);
	WARN_ON(s > MAX_TEST_SAMPLER);

	return 0;
}
module_init(avm_kpi_test_init);

static void __exit avm_kpi_test_exit(void)
{
	unsigned int i;

	for (i = 0; attributes[i]; ++i)
		kpi_delete_attribute(attributes[i]);

	for (i = 0; samplers[i]; ++i)
		kpi_delete_sampler(samplers[i]);

	destroy_kpi_node(test_node);
}
module_exit(avm_kpi_test_exit);
