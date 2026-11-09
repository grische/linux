// SPDX-License-Identifier: GPL-2.0+

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#pragma GCC pop_options

#include <avm/sammel/simple_proc.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static void *PDE_DATA(const struct inode *inode)
{
	return PROC_I(inode)->pde->data;
}

/**
 */
#ifndef CONFIG_MACH_BCM963138
static int single_open_size(struct file *file,
			    int (*show)(struct seq_file *, void *), void *data,
			    size_t size)
{
	char *buf = kmalloc(size, GFP_KERNEL);
	int ret;

	if (!buf)
		return -ENOMEM;
	ret = single_open(file, show, data);
	if (ret) {
		kfree(buf);
		return ret;
	}
	((struct seq_file *)file->private_data)->buf = buf;
	((struct seq_file *)file->private_data)->size = size;
	return 0;
}
#endif
#endif

LIST_HEAD(simple_proc_file_list);

struct simple_proc_file_internal {
	struct list_head list;
	struct file_operations fops;
	char *path;
	int (*kernel_input)(char *send, void *private);
	void (*kernel_output)(struct seq_file *pseq, void *private);
	void *priv_data;
	size_t expected_output_buffer_size;
};

static int simple_proc_file_show(struct seq_file *m, void *offset)
{
	struct simple_proc_file_internal *spf = m->private;

	if (spf->kernel_output) {
		sp_debug_print("offset: %#p\n", offset);
		spf->kernel_output(m, spf->priv_data);
	}
	return 0;
}

static int simple_proc_file_open(struct inode *inode, struct file *file)
{
	unsigned int res;
	struct simple_proc_file_internal *spf = PDE_DATA(inode);

	sp_debug_print(
		"call open with simple_proc_file_entry %p: %s, priv=%p\n", spf,
		spf->path, spf->priv_data);

	if (spf->expected_output_buffer_size) {
		res = single_open_size(file, simple_proc_file_show, spf,
				       spf->expected_output_buffer_size);
	} else {
		res = single_open(file, simple_proc_file_show, spf);
	}
	return res;
}

/**
 */
static ssize_t simple_proc_file_write(struct file *file, const char *buffer,
				      size_t count, loff_t *offset)
{
	unsigned char *procfs_buffer;
	ssize_t res;
	size_t procfs_buffer_size;
	struct simple_proc_file_internal *spf;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
	spf = PDE_DATA(file->f_path.dentry->d_inode);
#else
	spf = PDE_DATA(file->f_inode);
#endif

	if (spf->kernel_input == NULL) {
		return -EFAULT;
	}
	/*
	 * get buffer size:
	 * we need one extra byte in our buffer: the terminating 0-byte
	 * count variable contains user data len, this data is not 0-terminated
	 */
	if (count >= SP_MAX_INPUT_BUF) {
		procfs_buffer_size = SP_MAX_INPUT_BUF;
	} else {
		procfs_buffer_size = count + 1;
	}
	/* alloc buffer */
	if (procfs_buffer_size > PAGE_SIZE) {
		procfs_buffer = vmalloc(procfs_buffer_size);
	} else {
		procfs_buffer = kmalloc(procfs_buffer_size, GFP_KERNEL);
	}
	if (!procfs_buffer) {
		return -ENOMEM;
	}
	/* write data to the buffer */
	if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size - 1)) {
		res = -EFAULT;
		goto fini;
	}
	procfs_buffer[procfs_buffer_size - 1] = 0;
	res = spf->kernel_input(procfs_buffer, spf->priv_data);
	if (res >= 0) {
		res = procfs_buffer_size - 1;
	}
fini:
	if (is_vmalloc_addr(procfs_buffer)) {
		vfree(procfs_buffer);
	} else {
		kfree(procfs_buffer);
	}
	return res;
}

/**
 */
int add_simple_proc_file(const char *path, int (*kernel_input)(char *, void *),
			 void (*kernel_output)(struct seq_file *, void *),
			 void *priv_data)
{
	struct simple_proc_file_internal *simple_proc_file_entry = NULL;
	unsigned int path_len;

	/* path check */
	path_len = strlen(path) + 1;
	if (path_len > SP_MAX_PATH_LEN) {
		return -EINVAL;
	}

	/* alloc private data structure */
	simple_proc_file_entry =
		kzalloc(sizeof(*simple_proc_file_entry) + path_len, GFP_KERNEL);
	if (!simple_proc_file_entry) {
		return -ENOMEM;
	}
	simple_proc_file_entry->path = (char *)(simple_proc_file_entry + 1);

	/* setup fops */
	simple_proc_file_entry->fops.open = simple_proc_file_open;
	simple_proc_file_entry->fops.read = seq_read;
	simple_proc_file_entry->fops.llseek = seq_lseek;
	simple_proc_file_entry->fops.release = single_release;
	if (kernel_input) {
		simple_proc_file_entry->fops.write = simple_proc_file_write;
	}

	/*setup more */
	simple_proc_file_entry->priv_data = priv_data;
	simple_proc_file_entry->kernel_input = kernel_input;
	simple_proc_file_entry->kernel_output = kernel_output;
	/* copy path */
	memcpy(simple_proc_file_entry->path, path, path_len);

	/* create proc file */
	if (!proc_create_data(path, 0, NULL, &simple_proc_file_entry->fops,
			      simple_proc_file_entry)) {
		kfree(simple_proc_file_entry);
		pr_err("%s: proc_create_data failed\n", __func__);
		return -EFAULT;
	}
	list_add(&simple_proc_file_entry->list, &simple_proc_file_list);
	sp_debug_print("register simple_proc_file_entry %p\n",
		       simple_proc_file_entry);
	return 0;
}
EXPORT_SYMBOL(add_simple_proc_file);

/**
 */
void remove_simple_proc_file(const char *path)
{
	struct simple_proc_file_internal *spf;

	list_for_each_entry(spf, &simple_proc_file_list, list) {
		if (strncmp(spf->path, path, SP_MAX_PATH_LEN) == 0) {
			list_del(&spf->list);
			// kfree(spf) is done in remove_proc_entry
			remove_proc_entry(path, NULL);
			sp_debug_print("remove entry %s\n", path);
			return;
		}
	}
}
EXPORT_SYMBOL(remove_simple_proc_file);

/**
 */
int add_simple_proc_file_array(struct simple_proc_file *array)
{
	int i;
	int res = 0;

	for (i = 0; array[i].path != NULL; i++) {
		if (array[i].enabled) {
			int tres = add_simple_proc_file(array[i].path,
							array[i].kernel_input,
							array[i].kernel_output,
							array[i].priv_data);

			if (tres)
				res = tres;
		}
	}
	return res;
}
EXPORT_SYMBOL(add_simple_proc_file_array);

/**
 */
void remove_simple_proc_file_array(struct simple_proc_file *array)
{
	int i;

	for (i = 0; array[i].path != NULL; i++) {
		if (array[i].enabled)
			remove_simple_proc_file(array[i].path);
	}
}
EXPORT_SYMBOL(remove_simple_proc_file_array);

/**
 */
void simple_proc_file_setup_expected_output_size(const char *path,
						 size_t expected_output_size)
{
	struct simple_proc_file_internal *spf;

	list_for_each_entry (spf, &simple_proc_file_list, list) {
		if (strncmp(spf->path, path, SP_MAX_PATH_LEN) == 0) {
			spf->expected_output_buffer_size = expected_output_size;
		}
	}
}
EXPORT_SYMBOL(simple_proc_file_setup_expected_output_size);

MODULE_LICENSE("GPL");
