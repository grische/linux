/* SPDX-License-Identifier: GPL-2.0+ */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/seq_file.h>
#pragma GCC pop_options

#define SP_MAX_INPUT_BUF 8192
#define SP_MAX_PATH_LEN 4096
#define sp_debug_print(                                                        \
	format,                                                                \
	arg...) // printk(KERN_ERR "[%s/%d] " format, __func__, __LINE__, ##arg)

/*
 * add_simple_proc_file creates a simple procfile:
 *    path relative to '/proc/': e.g. '{subdir/}my_proc_file'
 *    kernel_input: function, which takes two arguments:
 *      - char *buffer: contains data, which was received via proc file, copied from userspace
 *      - void *private: pointer to private data, which was setup here via add_simple_proc_file
 *    kernel_output: function, which takes two arguments, creates data, which is sent to userspace via proc_file
 *      - struct seq_file *seq: you should pass this to seq_printf in your output kernel_output function
 *      - void *private: pointer to private data, which was setup here via add_simple_proc_file
 *    kernel_output might be called several times, though userspace performs only one read
 *    you can improve this behavior by setting simple_proc_file_setup_expected_output_size
 */
int add_simple_proc_file(const char *path,
			 int (*kernel_input)(char *send, void *private),
			 void (*kernel_output)(struct seq_file *pseq,
					       void *private),
			 void *priv_data);

/*
 * removes simple procfile, which was created by add_simple_proc_file():
 */
void remove_simple_proc_file(const char *path);

/*
 * in case your kernel_output function writes more than PAGESIZE,
 * your kernel_output function will be called several times. On every call, output
 * buffer size will be doubled, till everything fits in
 * If you want to improve this behavior you can setup output buffers size for the first
 * call of your output function
 */
void simple_proc_file_setup_expected_output_size(const char *path,
						 size_t expected_output_size);

/*
 * this struct is used in add_simple_proc_file_array and remove_simple_proc_file_array
 */
struct simple_proc_file {
	const char *path;
	int (*kernel_input)(char *send, void *prvate);
	void (*kernel_output)(struct seq_file *pseq, void *private);
	void *priv_data;
	int enabled;
};

/*
 * in case you want to (un-)register a NULL terminated struct simple_proc_file array[]
 * you can use these functions for batch processing
 */
int add_simple_proc_file_array(struct simple_proc_file *array);
void remove_simple_proc_file_array(struct simple_proc_file *array);
