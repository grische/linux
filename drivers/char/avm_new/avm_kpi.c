// SPDX-License-Identifier: GPL-2.0+

#include "avm_kpi.h"

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/sort.h>
#include <linux/version.h>
#include <linux/module.h>

#define ILLEGAL_CHARACTERS "\r\n"
#define NAME_PREFIX_LEN 30u
#define NAME_SUFFIX_LEN 12u
#define ATTRIBUTE_NAME_LEN (NAME_PREFIX_LEN + NAME_SUFFIX_LEN)
#define MAX_REDUCERS (_KPI_REDUCER_BOTTOM_)
#define ATTRIBUTE_DATA_SIZE (sizeof(u64))
typedef u8 kpi_attribute_data[ATTRIBUTE_DATA_SIZE];

static struct kset *section_set;

enum kpi_node_type {
	KPI_NODE_TYPE_SECTION,
	KPI_NODE_TYPE_DICTIONARY,
};

enum kpi_attribute_data_type {
	KPI_ATTRIBUTE_DATA_STRING,
	KPI_ATTRIBUTE_DATA_U32,
	KPI_ATTRIBUTE_DATA_U64,
	KPI_ATTRIBUTE_DATA_S32,
	KPI_ATTRIBUTE_DATA_S64,
};

typedef int (*kpi_reducer_fn)(const kpi_attribute_data *data, size_t data_num,
			      enum kpi_attribute_data_type type, char *out,
			      size_t out_len);

struct kpi_attribute;

typedef ssize_t (*kpi_show_fn)(struct kpi_node *node,
			       struct kpi_attribute *attr, char *buffer);

struct kpi_reducer_info {
	char suffix[NAME_SUFFIX_LEN];
	kpi_reducer_fn fn;
};

struct kpi_node {
	struct kobject kobj;
	enum kpi_node_type type;
};
#define to_kpi_node(x) container_of(x, struct kpi_node, kobj)

struct kpi_attribute {
	struct attribute attr;
	char name[ATTRIBUTE_NAME_LEN];
	struct kpi_node *node;
	kpi_show_fn show;
};
#define to_kpi_attr(x) container_of(x, struct kpi_attribute, attr)

struct kpi_attribute_absolute {
	struct kpi_attribute attr;
	enum kpi_attribute_data_type data_type;
	void *collector;
	void *ctx;
	size_t data_len;
	size_t data_num;
	u8 data[];
};
#define to_attr_absolute(x) container_of(x, struct kpi_attribute_absolute, attr)

struct kpi_attribute_reducer {
	struct kpi_attribute attr;
	struct kpi_sampler *sampler;
	kpi_reducer_fn fn;
};
#define to_attr_reducer(x) container_of(x, struct kpi_attribute_reducer, attr)

struct kpi_sampler {
	struct delayed_work work;
	enum kpi_attribute_data_type data_type;
	void *collector;
	void *ctx;
	struct mutex collect_lock;
	struct kpi_sliding_window window;
	size_t next_sample;
	size_t num_valid_samples;
	size_t num_reducers;
	struct kpi_attribute_reducer reducers[MAX_REDUCERS];
	kpi_attribute_data samples[];
};
#define to_sampler(x) container_of(x, struct kpi_sampler, work)

static ssize_t kpi_attr_show(struct kobject *kobj, struct attribute *attr,
			     char *buf)
{
	struct kpi_attribute *attribute = to_kpi_attr(attr);
	struct kpi_node *node = to_kpi_node(kobj);

	return attribute->show(node, attribute, buf);
}

static const struct sysfs_ops kpi_sysfs_ops = {
	.show = kpi_attr_show,
};

static void kpi_node_release(struct kobject *kobj)
{
	kfree(to_kpi_node(kobj));
}

static struct kobj_type kpi_node_ktype = {
	.sysfs_ops = &kpi_sysfs_ops,
	.release = kpi_node_release,
};

static size_t data_type_size(enum kpi_attribute_data_type data_type)
{
	switch (data_type) {
	case KPI_ATTRIBUTE_DATA_U32:
		return sizeof(u32);
	case KPI_ATTRIBUTE_DATA_S32:
		return sizeof(s32);
	case KPI_ATTRIBUTE_DATA_U64:
		return sizeof(u64);
	case KPI_ATTRIBUTE_DATA_S64:
		return sizeof(s64);
	case KPI_ATTRIBUTE_DATA_STRING:
		return 0;
	}

	return 0;
}

static void collect(enum kpi_attribute_data_type data_type, u8 *to, size_t len,
		    size_t num, void *collector, void *ctx)
{
	switch (data_type) {
	case KPI_ATTRIBUTE_DATA_U32:
		if (num == 1)
			*((u32 *)to) = ((kpi_collector_u32)collector)(ctx);
		else
			((kpi_collector_u32_arr)collector)(ctx, (u32 *)to, num);
		break;
	case KPI_ATTRIBUTE_DATA_S32:
		if (num == 1)
			*((s32 *)to) = ((kpi_collector_s32)collector)(ctx);
		else
			((kpi_collector_s32_arr)collector)(ctx, (s32 *)to, num);
		break;
	case KPI_ATTRIBUTE_DATA_U64:
		if (num == 1)
			*((u64 *)to) = ((kpi_collector_u64)collector)(ctx);
		else
			((kpi_collector_u64_arr)collector)(ctx, (u64 *)to, num);
		break;
	case KPI_ATTRIBUTE_DATA_S64:
		if (num == 1)
			*((s64 *)to) = ((kpi_collector_s64)collector)(ctx);
		else
			((kpi_collector_s64_arr)collector)(ctx, (s64 *)to, num);
		break;
	case KPI_ATTRIBUTE_DATA_STRING:
		((kpi_collector_string)collector)(ctx, (char *)to, len);
		break;
	}
}

static int format_value(enum kpi_attribute_data_type data_type, const u8 *from,
			size_t from_len, char *to, size_t to_len)
{
	switch (data_type) {
	case KPI_ATTRIBUTE_DATA_U32:
		return scnprintf(to, to_len, "%llu\n",
				 (unsigned long long)*((u32 *)from));
	case KPI_ATTRIBUTE_DATA_S32:
		return scnprintf(to, to_len, "%lld\n",
				 (long long)*((s32 *)from));
	case KPI_ATTRIBUTE_DATA_U64:
		return scnprintf(to, to_len, "%llu\n",
				 (unsigned long long)*((u64 *)from));
	case KPI_ATTRIBUTE_DATA_S64:
		return scnprintf(to, to_len, "%lld\n",
				 (long long)*((s64 *)from));
	case KPI_ATTRIBUTE_DATA_STRING:
		if (strpbrk((char *)from, ILLEGAL_CHARACTERS))
			return -EINVAL;
		return scnprintf(to, min(to_len, from_len), "\"%s\"\n",
				 (char *)from);
	}

	return -EIO;
}

static bool is_signed(enum kpi_attribute_data_type data_type)
{
	switch (data_type) {
	case KPI_ATTRIBUTE_DATA_U32:
	case KPI_ATTRIBUTE_DATA_U64:
		return false;
	case KPI_ATTRIBUTE_DATA_S32:
	case KPI_ATTRIBUTE_DATA_S64:
		return true;
	case KPI_ATTRIBUTE_DATA_STRING:
		pr_err("%s: Invalid conversion.\n", __func__);
		break;
	}

	return false;
}

static u64 to_unsigned_num(enum kpi_attribute_data_type data_type,
			   const u8 *from)
{
	switch (data_type) {
	case KPI_ATTRIBUTE_DATA_U32:
		return (u64)(*((u32 *)from));
	case KPI_ATTRIBUTE_DATA_U64:
		return (u64)(*((u64 *)from));
	case KPI_ATTRIBUTE_DATA_S32:
	case KPI_ATTRIBUTE_DATA_S64:
	case KPI_ATTRIBUTE_DATA_STRING:
		pr_err("%s: Invalid conversion.\n", __func__);
		break;
	}

	return 0u;
}

static s64 to_signed_num(enum kpi_attribute_data_type data_type, const u8 *from)
{
	switch (data_type) {
	case KPI_ATTRIBUTE_DATA_U32:
		return (s64)(*((u32 *)from));
	case KPI_ATTRIBUTE_DATA_S32:
		return (s64)(*((s32 *)from));
	case KPI_ATTRIBUTE_DATA_U64:
		return (s64)(*((u64 *)from));
	case KPI_ATTRIBUTE_DATA_S64:
		return (s64)(*((s64 *)from));
	case KPI_ATTRIBUTE_DATA_STRING:
		pr_err("%s: Invalid conversion.\n", __func__);
		break;
	}

	return 0;
}

static int cmp_s64(const void *lhs, const void *rhs)
{
	s64 l = *(const s64 *)(lhs);
	s64 r = *(const s64 *)(rhs);

	if (l < r)
		return -1;
	if (l > r)
		return 1;
	return 0;
}

static int cmp_u64(const void *lhs, const void *rhs)
{
	u64 l = *(const u64 *)(lhs);
	u64 r = *(const u64 *)(rhs);

	if (l < r)
		return -1;
	if (l > r)
		return 1;
	return 0;
}

static int cmp_s32(const void *lhs, const void *rhs)
{
	s32 l = *(const s32 *)(lhs);
	s32 r = *(const s32 *)(rhs);

	if (l < r)
		return -1;
	if (l > r)
		return 1;
	return 0;
}

static int cmp_u32(const void *lhs, const void *rhs)
{
	u32 l = *(const u32 *)(lhs);
	u32 r = *(const u32 *)(rhs);

	if (l < r)
		return -1;
	if (l > r)
		return 1;
	return 0;
}

static int calc_median(const kpi_attribute_data *data, size_t data_num,
		       enum kpi_attribute_data_type type, void *out)
{
	int (*cmp_fn)(const void *lhr, const void *rhs) = 0;
	kpi_attribute_data *copy;

#if KERNEL_VERSION(3, 3, 0) <= LINUX_VERSION_CODE
	copy = kmalloc_array(data_num, sizeof(kpi_attribute_data), GFP_KERNEL);
#else
	copy = kmalloc(data_num * sizeof(kpi_attribute_data), GFP_KERNEL);
#endif
	if (!copy)
		return -ENOMEM;

	memcpy(copy, data, sizeof(kpi_attribute_data) * data_num);

	switch (type) {
	case KPI_ATTRIBUTE_DATA_S32:
		cmp_fn = cmp_s32;
		break;
	case KPI_ATTRIBUTE_DATA_U32:
		cmp_fn = cmp_u32;
		break;
	case KPI_ATTRIBUTE_DATA_S64:
		cmp_fn = cmp_s64;
		break;
	case KPI_ATTRIBUTE_DATA_U64:
		cmp_fn = cmp_u64;
		break;
	case KPI_ATTRIBUTE_DATA_STRING:
		kfree(copy);
		return -EINVAL;
	}

	sort(copy, data_num, sizeof(kpi_attribute_data), cmp_fn, NULL);
	memcpy(out, copy[(data_num + 1) / 2], data_type_size(type));
	kfree(copy);
	return 0;
}

static int reducer_none(const kpi_attribute_data *data, size_t data_num,
			enum kpi_attribute_data_type type, char *out,
			size_t out_len)
{
	int cur = 0;
	size_t i;

	for (i = 0; i < data_num && (int)out_len - cur > 1; ++i) {
		cur += format_value(type, data[i], 0, out + cur,
				    (int)out_len - cur);
	}

	/* too much data for output buffer */
	if (i != data_num)
		return -ENOMEM;

	return cur;
}

static int reducer_min(const kpi_attribute_data *data, size_t data_num,
		       enum kpi_attribute_data_type type, char *out,
		       size_t out_len)
{
	size_t i;

	if (is_signed(type)) {
		s64 min = LLONG_MAX;

		for (i = 0; i < data_num; ++i)
			min = min(min, to_signed_num(type, data[i]));

		return format_value(KPI_ATTRIBUTE_DATA_S64, (u8 *)&min, 0, out,
				    out_len);
	} else {
		u64 min = ULLONG_MAX;

		for (i = 0; i < data_num; ++i)
			min = min(min, to_unsigned_num(type, data[i]));

		return format_value(KPI_ATTRIBUTE_DATA_U64, (u8 *)&min, 0, out,
				    out_len);
	}
}

static int reducer_max(const kpi_attribute_data *data, size_t data_num,
		       enum kpi_attribute_data_type type, char *out,
		       size_t out_len)
{
	size_t i;

	if (is_signed(type)) {
		s64 max = LLONG_MIN;

		for (i = 0; i < data_num; ++i)
			max = max(max, to_signed_num(type, data[i]));

		return format_value(KPI_ATTRIBUTE_DATA_S64, (u8 *)&max, 0, out,
				    out_len);
	} else {
		u64 max = 0;

		for (i = 0; i < data_num; ++i)
			max = max(max, to_unsigned_num(type, data[i]));

		return format_value(KPI_ATTRIBUTE_DATA_U64, (u8 *)&max, 0, out,
				    out_len);
	}
}

static int reducer_sum(const kpi_attribute_data *data, size_t data_num,
		       enum kpi_attribute_data_type type, char *out,
		       size_t out_len)
{
	if (is_signed(type)) {
		s64 sum = 0;
		size_t i;

		for (i = 0; i < data_num; ++i)
			sum += to_signed_num(type, data[i]);

		return format_value(KPI_ATTRIBUTE_DATA_S64, (u8 *)&sum, 0, out,
				    out_len);

	} else {
		u64 sum = 0;
		size_t i;

		for (i = 0; i < data_num; ++i)
			sum += to_unsigned_num(type, data[i]);

		return format_value(KPI_ATTRIBUTE_DATA_U64, (u8 *)&sum, 0, out,
				    out_len);
	}
}

static int reducer_avg(const kpi_attribute_data *data, size_t data_num,
		       enum kpi_attribute_data_type type, char *out,
		       size_t out_len)
{
	if (is_signed(type)) {
		bool is_negative;
		s64 sum = 0;
		u64 abs_sum;
		size_t i;

		for (i = 0; i < data_num; ++i)
			sum += to_signed_num(type, data[i]);

		is_negative = sum < 0;
		abs_sum = abs(sum);
		do_div(abs_sum, data_num);
		sum = abs_sum;
		if (is_negative)
			sum = sum * -1;

		return format_value(KPI_ATTRIBUTE_DATA_S64, (u8 *)&sum, 0, out,
				    out_len);

	} else {
		u64 sum = 0;
		size_t i;

		for (i = 0; i < data_num; ++i)
			sum += to_unsigned_num(type, data[i]);

		do_div(sum, data_num);
		return format_value(KPI_ATTRIBUTE_DATA_U64, (u8 *)&sum, 0, out,
				    out_len);
	}
}

static int reducer_median(const kpi_attribute_data *data, size_t data_num,
			  enum kpi_attribute_data_type type, char *out,
			  size_t out_len)
{
	kpi_attribute_data median;
	int err;

	err = calc_median(data, data_num, type, &median);
	if (err)
		return err;

	return format_value(type, median, 0, out, out_len);
}

static int reducer_variance(const kpi_attribute_data *data, size_t data_num,
			    enum kpi_attribute_data_type type, char *out,
			    size_t out_len)
{
	kpi_attribute_data median;
	u64 variance = 0;
	size_t i;
	int err;

	err = calc_median(data, data_num, type, &median);
	if (err)
		return err;

	for (i = 0; i < data_num; ++i) {
		s64 cur = to_signed_num(type, data[i]);
		s64 addend = cur - to_signed_num(type, median);

		variance += addend * addend;
	}
	do_div(variance, data_num);

	return format_value(KPI_ATTRIBUTE_DATA_U64, (u8 *)&variance, 0, out,
			    out_len);
}

static struct kpi_reducer_info reducer_map[MAX_REDUCERS] = {
	{ .suffix = "raw", .fn = reducer_none }, /* REDUCER_NONE */
	{ .suffix = "min", .fn = reducer_min }, /* REDUCER_MIN */
	{ .suffix = "max", .fn = reducer_max }, /* REDUCER_MAX */
	{ .suffix = "sum", .fn = reducer_sum }, /* REDUCER_SUM */
	{ .suffix = "avg", .fn = reducer_avg }, /* REDUCER_AVG */
	{ .suffix = "median", .fn = reducer_median }, /* REDUCER_MEDIAN */
	{ .suffix = "variance", .fn = reducer_variance }, /* REDUCER_VARIANCE */
};

static ssize_t absolute_show(struct kpi_node *node,
			     struct kpi_attribute *attr, char *buffer)
{
	struct kpi_attribute_absolute *abs = to_attr_absolute(attr);
	int cur = 0;
	size_t i;

	collect(abs->data_type, abs->data, abs->data_len, abs->data_num,
		abs->collector, abs->ctx);

	for (i = 0; i < abs->data_num && PAGE_SIZE - cur > 1; ++i) {
		cur += format_value(abs->data_type,
				    abs->data + i * abs->data_len,
				    abs->data_len, buffer + cur,
				    PAGE_SIZE - cur);
	}

	return cur;
}

static ssize_t sampler_show(struct kpi_node *node,
			    struct kpi_attribute *attr, char *buffer)
{
	struct kpi_attribute_reducer *reduc = to_attr_reducer(attr);
	ssize_t retval = 0;

	mutex_lock(&reduc->sampler->collect_lock);
	if (reduc->sampler->num_valid_samples <
	    reduc->sampler->window.num_samples)
		goto unlock;

	retval = reduc->fn(reduc->sampler->samples,
			   reduc->sampler->num_valid_samples,
			   reduc->sampler->data_type, buffer, PAGE_SIZE);

unlock:
	mutex_unlock(&reduc->sampler->collect_lock);
	return retval;
}

static unsigned int window_to_jiffies(const struct kpi_sliding_window *window)
{
	return msecs_to_jiffies(window->size_seconds * 1000 /
				window->num_samples);
}

static bool is_array_attr_length_valid(size_t num_elements)
{
	/**
	 * This is necessary for two reasons:
	 * 1. We decide based on this number which callback to invoke.
	 * 2. Allowing would make no difference as userspace cannot distinguish
	 *    between a scalar and a 1-element array.
	 */
	if (num_elements < 2) {
		pr_err("Array attributes must have at least two elements");
		return false;
	}
	return true;
}

static void collect_samples_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct kpi_sampler *sampler = to_sampler(dwork);

	mutex_lock(&sampler->collect_lock);

	collect(sampler->data_type, sampler->samples[sampler->next_sample],
		data_type_size(sampler->data_type), 1, sampler->collector,
		sampler->ctx);

	sampler->next_sample =
		(sampler->next_sample + 1) % sampler->window.num_samples;

	sampler->num_valid_samples = min(sampler->num_valid_samples + 1,
					 sampler->window.num_samples);

	mutex_unlock(&sampler->collect_lock);

	schedule_delayed_work(dwork, window_to_jiffies(&sampler->window));
}

void destroy_kpi_node(struct kpi_node *node)
{
	kobject_put(&node->kobj);
}
EXPORT_SYMBOL(destroy_kpi_node);

static int init_attribute(struct kpi_attribute *attr, const char *name,
			  struct kpi_node *parent, kpi_show_fn show_fn)
{
	strlcpy(attr->name, name, sizeof(attr->name));

	attr->node = parent;
	attr->show = show_fn;
	attr->attr.name = attr->name;
	attr->attr.mode = 0440;

	return sysfs_create_file(&parent->kobj, &attr->attr);
}

static struct kpi_attribute_absolute *
add_absolute(const char *name, struct kpi_node *parent, void *collector,
	     void *ctx, enum kpi_attribute_data_type data_type,
	     size_t data_size, size_t data_num)
{
	size_t size = data_type_size(data_type);
	struct kpi_attribute_absolute *abs;
	int err;

	if (!data_num) {
		pr_err("Attributes must have at least one element\n");
		return NULL;
	}

	if (data_type == KPI_ATTRIBUTE_DATA_STRING)
		size = data_size;

	if (!size) {
		pr_err("Attributes of size zero are invalid\n");
		return NULL;
	}

	abs = kzalloc(data_num * size + sizeof(struct kpi_attribute_absolute),
		      GFP_KERNEL);

	if (!abs)
		return NULL;

	abs->data_len = size;
	abs->data_num = data_num;
	abs->data_type = data_type;
	abs->collector = collector;
	abs->ctx = ctx;

	err = init_attribute(&abs->attr, name, parent, absolute_show);
	if (err) {
		kfree(abs);
		return NULL;
	}

	return abs;
}

static struct kpi_sampler *
add_sampler(const char *name, struct kpi_node *parent, void *collector,
	    void *ctx, enum kpi_attribute_data_type data_type,
	    struct kpi_sliding_window window, const enum kpi_reducer *reducers)
{
	struct kpi_sampler *sampler;
	size_t dyn_size;
	unsigned int i;
	int err;

	if (data_type == KPI_ATTRIBUTE_DATA_STRING) {
		pr_err("Strings cannot be sampled. Use a numeric type.\n");
		return NULL;
	}

	if (window.num_samples < 2 || !window.size_seconds) {
		pr_err("Invalid sliding window specified\n");
		return NULL;
	}

	dyn_size = window.num_samples * sizeof(kpi_attribute_data);
	sampler = kzalloc(dyn_size + sizeof(struct kpi_sampler), GFP_KERNEL);

	if (!sampler)
		return NULL;

	sampler->data_type = data_type;
	sampler->collector = collector;
	sampler->ctx = ctx;
	sampler->window = window;
	mutex_init(&sampler->collect_lock);
	INIT_DELAYED_WORK(&sampler->work, collect_samples_worker);
	schedule_delayed_work(&sampler->work, 0);

	for (i = 0; reducers[i] != _KPI_REDUCER_BOTTOM_; ++i) {
		struct kpi_attribute_reducer *to = &sampler->reducers[i];
		struct kpi_reducer_info *from = &reducer_map[reducers[i]];
		char reducer_name[ATTRIBUTE_NAME_LEN];

		to->sampler = sampler;
		to->fn = from->fn;

		scnprintf(reducer_name, sizeof(reducer_name), "%s_%s", name,
			  from->suffix);

		err = init_attribute(&to->attr, reducer_name, parent,
				     sampler_show);
		if (err) {
			pr_err("Failed to create reducer attribute: %s\n",
			       reducer_name);
		}
	}
	sampler->num_reducers = i;

	return sampler;
}

struct kpi_node *create_kpi_node(const char *name, struct kobject *parent)
{
	enum kpi_node_type type;
	struct kpi_node *node;
	int retval;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return NULL;

	if (!parent) {
		node->kobj.kset = section_set;
		type = KPI_NODE_TYPE_SECTION;
	} else
		type = KPI_NODE_TYPE_DICTIONARY;

	node->type = type;
	retval = kobject_init_and_add(&node->kobj, &kpi_node_ktype, parent,
				      "%s", name);
	if (retval) {
		kobject_put(&node->kobj);
		return NULL;
	}
	kobject_uevent(&node->kobj, KOBJ_ADD);

	return node;
}
EXPORT_SYMBOL(create_kpi_node);

static int __init kpi_init(void)
{
	struct kpi_node *system, *usb, *hwpa;

	section_set = kset_create_and_add("kpi", NULL, kernel_kobj);
	if (!section_set)
		return -ENOMEM;

	system = create_kpi_node("system", NULL);
	if (!system)
		goto system_error;

	usb = create_kpi_node("usb", NULL);
	if (!usb)
		goto usb_error;

	hwpa = create_kpi_node("hwpa", NULL);
	if (!hwpa)
		goto hwpa_error;

	return 0;

hwpa_error:
	destroy_kpi_node(usb);
usb_error:
	destroy_kpi_node(system);
system_error:
	kset_unregister(section_set);
	return -EINVAL;
}
postcore_initcall(kpi_init);

struct kpi_node *kpi_get_section(const char *name)
{
	struct kobject *kobj;

	kobj = kset_find_obj(section_set, name);
	if (!kobj)
		return NULL;

	return to_kpi_node(kobj);
}
EXPORT_SYMBOL(kpi_get_section);

struct kpi_node *kpi_create_dict(const char *name, struct kpi_node *parent)
{
	if (!parent)
		return NULL;

	return create_kpi_node(name, &parent->kobj);
}
EXPORT_SYMBOL(kpi_create_dict);

void kpi_delete_dictionary(struct kpi_node *node)
{
	if (node->type != KPI_NODE_TYPE_DICTIONARY)
		return;

	destroy_kpi_node(node);
}
EXPORT_SYMBOL(kpi_delete_dictionary);

void kpi_delete_attribute(struct kpi_attribute_absolute *attr)
{
	sysfs_remove_file(&attr->attr.node->kobj, &attr->attr.attr);
	kfree(attr);
}
EXPORT_SYMBOL(kpi_delete_attribute);

void kpi_delete_sampler(struct kpi_sampler *sampler)
{
	unsigned int i;

	cancel_delayed_work_sync(&sampler->work);

	for (i = 0; i < sampler->num_reducers; ++i) {
		struct kpi_attribute_reducer *reduc = &sampler->reducers[i];

		sysfs_remove_file(&reduc->attr.node->kobj, &reduc->attr.attr);
	}

	kfree(sampler);
}
EXPORT_SYMBOL(kpi_delete_sampler);

struct kpi_attribute_absolute *kpi_add_attr_u32(const char *name,
						struct kpi_node *parent,
						kpi_collector_u32 collector,
						void *ctx)
{
	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_U32, 0, 1);
}
EXPORT_SYMBOL(kpi_add_attr_u32);

struct kpi_attribute_absolute *kpi_add_attr_u64(const char *name,
						struct kpi_node *parent,
						kpi_collector_u64 collector,
						void *ctx)
{
	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_U64, 0, 1);
}
EXPORT_SYMBOL(kpi_add_attr_u64);

struct kpi_attribute_absolute *kpi_add_attr_s32(const char *name,
						struct kpi_node *parent,
						kpi_collector_s32 collector,
						void *ctx)
{
	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_S32, 0, 1);
}
EXPORT_SYMBOL(kpi_add_attr_s32);

struct kpi_attribute_absolute *kpi_add_attr_s64(const char *name,
						struct kpi_node *parent,
						kpi_collector_s64 collector,
						void *ctx)

{
	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_S64, 0, 1);
}
EXPORT_SYMBOL(kpi_add_attr_s64);

struct kpi_attribute_absolute *
kpi_add_attr_string(const char *name, struct kpi_node *parent,
		    kpi_collector_string collector, void *ctx, size_t size)
{
	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_STRING, size, 1);
}
EXPORT_SYMBOL(kpi_add_attr_string);

struct kpi_sampler *
kpi_add_sampler_u32(const char *name, struct kpi_node *parent,
		    kpi_collector_u32 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers)
{
	return add_sampler(name, parent, collector, ctx, KPI_ATTRIBUTE_DATA_U32,
			   sliding_window, reducers);
}
EXPORT_SYMBOL(kpi_add_sampler_u32);

struct kpi_sampler *
kpi_add_sampler_u64(const char *name, struct kpi_node *parent,
		    kpi_collector_u64 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers)
{
	return add_sampler(name, parent, collector, ctx, KPI_ATTRIBUTE_DATA_U64,
			   sliding_window, reducers);
}
EXPORT_SYMBOL(kpi_add_sampler_u64);

struct kpi_sampler *
kpi_add_sampler_s32(const char *name, struct kpi_node *parent,
		    kpi_collector_s32 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers)
{
	return add_sampler(name, parent, collector, ctx, KPI_ATTRIBUTE_DATA_S32,
			   sliding_window, reducers);
}
EXPORT_SYMBOL(kpi_add_sampler_s32);

struct kpi_sampler *
kpi_add_sampler_s64(const char *name, struct kpi_node *parent,
		    kpi_collector_s64 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers)
{
	return add_sampler(name, parent, collector, ctx, KPI_ATTRIBUTE_DATA_S64,
			   sliding_window, reducers);
}
EXPORT_SYMBOL(kpi_add_sampler_s64);

struct kpi_attribute_absolute *
kpi_add_attr_u32_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_u32_arr collector, size_t num_elements,
		     void *ctx)
{
	if (!is_array_attr_length_valid(num_elements))
		return NULL;

	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_U32, 0, num_elements);
}
EXPORT_SYMBOL(kpi_add_attr_u32_arr);

struct kpi_attribute_absolute *
kpi_add_attr_u64_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_u64_arr collector, size_t num_elements,
		     void *ctx)
{
	if (!is_array_attr_length_valid(num_elements))
		return NULL;

	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_U64, 0, num_elements);
}
EXPORT_SYMBOL(kpi_add_attr_u64_arr);

struct kpi_attribute_absolute *
kpi_add_attr_s32_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_s32_arr collector, size_t num_elements,
		     void *ctx)
{
	if (!is_array_attr_length_valid(num_elements))
		return NULL;

	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_S32, 0, num_elements);
}
EXPORT_SYMBOL(kpi_add_attr_s32_arr);

struct kpi_attribute_absolute *
kpi_add_attr_s64_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_s64_arr collector, size_t num_elements,
		     void *ctx)
{
	if (!is_array_attr_length_valid(num_elements))
		return NULL;

	return add_absolute(name, parent, collector, ctx,
			    KPI_ATTRIBUTE_DATA_S64, 0, num_elements);
}
EXPORT_SYMBOL(kpi_add_attr_s64_arr);
