/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef AVM_KPI_H
#define AVM_KPI_H

#include <linux/kernel.h>

/**
 * DOC: Using key performance indicators from kernel space.
 *
 * The avm_kpi uses the sysfs to create kpi values from kernel space in a
 * generic way. It consists of a kernel space part (this API) and a user space
 * daemon (kpid).
 *
 * The kernel API maps kpi values (dictionaries, attributes, samplers) to a
 * unified sysfs hierarchy at /sys/kernel/kpi. The userspace kpid understands
 * this hierarchy and maps it to the required json output format without
 * further knowledge about the specific contents.
 *
 * Currently, each section (top level sysfs directory) can only send
 * 4K bytes of json data. Violating this limit results in kpid (user space)
 * skipping the whole section. The kernel cannot enforce this limit as it
 * cannot know the size of the user space generated json. However, the kernel
 * limits per file output to PAGE_SIZE.
 */

struct kpi_attribute_absolute;
struct kpi_sampler;
struct kpi_node;

/**
 * enum kpi_reducer - List of available reducers.
 * @KPI_REDUCER_NONE:     Do not reduce. Output the raw samples. This produces
 *		          a lot of data and should therefore only be used with a
			  a low number of samples because of the per section
			  and per file size limitations.
 * @KPI_REDUCER_MIN:      Output the minimum value of the samples.
 * @KPI_REDUCER_MAX:      Output the maximum value of the samples.
 * @KPI_REDUCER_SUM:      Output the sum of the samples.
 * @KPI_REDUCER_AVG:      Output the average of the samples.
 * @KPI_REDUCER_MEDIAN:   Output the median of the samples.
 * @KPI_REDUCER_VARIANCE: Output the variance from the median.
 *
 * All calculations are done with 64bit types but overflows will happen if
 * values are too large when using sum, average oder variance.
 */
enum kpi_reducer {
	KPI_REDUCER_NONE,
	KPI_REDUCER_MIN,
	KPI_REDUCER_MAX,
	KPI_REDUCER_SUM,
	KPI_REDUCER_AVG,
	KPI_REDUCER_MEDIAN,
	KPI_REDUCER_VARIANCE,
	_KPI_REDUCER_BOTTOM_,
};

/**
 * struct kpi_sliding_window - Sliding window configuration for samplers.
 * @size_seconds: The length of the sliding window in seconds. In other words:
 *		  The oldest sample is size_seconds in the past.
 * @num_samples:  Number of samples that are taken during one sliding window
		  iteration. That means a sample is taken every
		  (size_seconds / num_samples) seconds.
 */
struct kpi_sliding_window {
	unsigned int size_seconds;
	unsigned int num_samples;
};

/**
 * @kpi_collector_u32 - A callback that is used by scalar attributes.
 * @ctx: User specified value as passed when creating the attribute.
 *
 * The task of the callback is to return the current value of the attribute.
 *
 * Return: Current value of the attribute.
 */
typedef u32 (*kpi_collector_u32)(void *ctx);
typedef u64 (*kpi_collector_u64)(void *ctx);
typedef s32 (*kpi_collector_s32)(void *ctx);
typedef s64 (*kpi_collector_s64)(void *ctx);
typedef void (*kpi_collector_string)(void *ctx, char *buf, size_t buffer_len);

/**
 * @kpi_collector_u32_arr - A callback that is used by array atttributes.
 * @ctx: User specified value as passed when creating the attribute.
 * @dest: Target array for the current attribute value.
 * @num: Number of elements (NOT bytes) in the target array.
 *
 * The task of the callback is to copy the array elements to dest.
 * The callback MUST copy each and array element each time it is called.
 */
typedef void (*kpi_collector_u32_arr)(void *ctx, u32 *dest, size_t num);
typedef void (*kpi_collector_u64_arr)(void *ctx, u64 *dest, size_t num);
typedef void (*kpi_collector_s32_arr)(void *ctx, s32 *dest, size_t num);
typedef void (*kpi_collector_s64_arr)(void *ctx, s64 *dest, size_t num);

/**
 * @kpi_get_section() - Get a reference to an existing section.
 * @name: Name of the section that should be retrieved.
 *
 * Sections are the top level directories in /sys/kernel/kpi and cannot
 * be created dynamically. They must be known to the ctrlmgr and therefore
 * are created statically.
 *
 * Context: Atomic allowed but no (soft)irq context. Holds spinlock.
 * Return: A reference to the section or NULL on error.
 */
struct kpi_node *kpi_get_section(const char *name);

/**
 * @kpi_create_dict() - Create a new dictionary.
 * @name:   The name of the new dictionary. Copied by this function.
 * @parent: Either a section or another dictionary.
 *
 * A dictionary maps to a directory in sysfs and is a namespace to add
 * more dictionaries, attributes or samplers to.
 *
 * Context: Process context. May sleep.
 * Return: Reference to the new dictionary or NULL on error.
 */
struct kpi_node *kpi_create_dict(const char *name, struct kpi_node *parent);

/**
 * @kpi_delete_dictionary() - Remove a dictionary.
 * @dict: Reference of the dictionary to remove.
 *
 * After calling this function the dictionary is invalidated and must
 * not be used. Please note that this is not a recursive removal. As long
 * as there are other elements below this dictionary it is not removed. It is
 * removed when the last reference is dropped.
 *
 * Context: Any context.
 */
void kpi_delete_dictionary(struct kpi_node *dict);

/**
 * @kpi_delete_attribute() - Remove an attribute.
 * @dict: Reference of the attribute to remove.
 *
 * After calling this function the attribute is invalidated and must
 * not be used.
 *
 * Context: Any context.
 */
void kpi_delete_attribute(struct kpi_attribute_absolute *attribute);

/**
 * @kpi_delete_sampler() - Remove a sampler.
 * @dict: Reference of the sampler to remove.
 *
 * After calling this function the attribute is invalidated and must
 * not be used.
 *
 * Context: Any context.
 */
void kpi_delete_sampler(struct kpi_sampler *sampler);

/**
 * @kpi_add_attr_u32() - Create a new attribute.
 * @name:      The name of the new attribute. Copied by this function.
 * @parent:    Either a section or a dictionary.
 * @collector: A function that supplies the attribute value.
 * @ctx:       Value that is passed as-is to the collector.
 *
 * An attribute is represented as a single file in the directory of the
 * parent. The collector callback is called whenever a read happens to this
 * file. This happens in syscall context where sleeping is tolerated. Apart
 * from converting the value to text it is returned as-is to userspace.
 *
 * Context: Process context. May sleep.
 * Return: Reference to the new attribute or NULL on error.
 */
struct kpi_attribute_absolute *kpi_add_attr_u32(const char *name,
						struct kpi_node *parent,
						kpi_collector_u32 collector,
						void *ctx);

struct kpi_attribute_absolute *kpi_add_attr_u64(const char *name,
						struct kpi_node *parent,
						kpi_collector_u64 collector,
						void *ctx);

struct kpi_attribute_absolute *kpi_add_attr_s32(const char *name,
						struct kpi_node *parent,
						kpi_collector_s32 collector,
						void *ctx);

struct kpi_attribute_absolute *kpi_add_attr_s64(const char *name,
						struct kpi_node *parent,
						kpi_collector_s64 collector,
						void *ctx);

/**
 * @kpi_add_attr_u32_arr() - Create a new array attribute.
 * @name:      The name of the new attribute. Copied by this function.
 * @parent:    Either a section or a dictionary.
 * @collector: A function that supplies the attribute value.
 * @num_elements: The number of u32 values the array consists of.
 * @ctx:       Value that is passed as-is to the collector.
 *
 * An attribute is represented as a single file in the directory of the
 * parent. The collector callback is called whenever a read happens to this
 * file. This happens in syscall context where sleeping is tolerated. The
 * individual values of the array are separated by newlines inside the sysfs
 * file.
 *
 * Context: Process context. May sleep.
 * Return: Reference to the new attribute or NULL on error.
 */
struct kpi_attribute_absolute *
kpi_add_attr_u32_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_u32_arr collector, size_t num_elements,
		     void *ctx);

struct kpi_attribute_absolute *
kpi_add_attr_u64_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_u64_arr collector, size_t num_elements,
		     void *ctx);

struct kpi_attribute_absolute *
kpi_add_attr_s32_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_s32_arr collector, size_t num_elements,
		     void *ctx);

struct kpi_attribute_absolute *
kpi_add_attr_s64_arr(const char *name, struct kpi_node *parent,
		     kpi_collector_s64_arr collector, size_t num_elements,
		     void *ctx);

struct kpi_attribute_absolute *
kpi_add_attr_string(const char *name, struct kpi_node *parent,
		    kpi_collector_string collector, void *ctx, size_t size);

/**
 * @kpi_add_sampler_u32() - Create a new sampler.
 * @name:           The name of the new sampler. Copied by this function.
 * @parent:         Either a section or a dictionary.
 * @collector:      A function that supplies a sample.
 * @ctx:	    Value that is passed as-is to the collector.
 * @sliding_window: Sliding window configuration. num_samples must be >= 2.
 * @reducers:       List of reducers terminated with _KPI_REDUCER_BOTTOM_.
 *
 * An sampler is represented as one or multiple files in the directory of the
 * parent. For every supplied reducer a file is created. The collector function
 * is called at a sampling rate defined by the sliding window. This call happens
 * in a context where sleeping is allowed. The reducers job is to create
 * a meaningful value from all the collected samples. This reduced value is
 * what is read from the sysfs file. This file will be empty as long as the
 * sliding window is not full.
 *
 * Context: Process context. May sleep.
 * Return: Reference to the new sampler or NULL on error.
 */
struct kpi_sampler *
kpi_add_sampler_u32(const char *name, struct kpi_node *parent,
		    kpi_collector_u32 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers);

struct kpi_sampler *
kpi_add_sampler_u64(const char *name, struct kpi_node *parent,
		    kpi_collector_u64 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers);

struct kpi_sampler *
kpi_add_sampler_s32(const char *name, struct kpi_node *parent,
		    kpi_collector_s32 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers);

struct kpi_sampler *
kpi_add_sampler_s64(const char *name, struct kpi_node *parent,
		    kpi_collector_s64 collector, void *ctx,
		    struct kpi_sliding_window sliding_window,
		    const enum kpi_reducer *reducers);

#endif /* AVM_KPI_H */
