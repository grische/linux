// SPDX-License-Identifier: GPL-2.0+
#define pr_fmt(fmt) "[avm_prom_config] " fmt

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/compiler.h>
#pragma GCC pop_options

#include <avm/enh/prom_config.h>
#include "prom_config_internal.h"

#ifdef DEBUG
#  define prom_config_debug 1
#else
#  define prom_config_debug 0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static inline void *PDE_DATA(struct inode *inode)
{
	return PDE(inode)->data;
}
#endif

static struct proc_dir_entry *calibprocdir;
static struct dentry *debugfs_dir __maybe_unused;

#define PROC_CALIBDIR "avm/calib"

const char *const avm_prom_config_type_name[] = {
	[WLAN]			= "wlan",
	[DECT]			= "dect",
	[WLAN2]			= "wlan2",
	[ZERTIFIKATE]		= "zertifikate",
	[DOCSIS]		= "docsis",
	[DSL]			= "dsl",
	[PROLIFIC]		= "prolific",
	[WLAN_ZIP]		= "wlan_zip",
	[WLAN2_ZIP]		= "wlan2_zip",
	[ZERTIFIKATE2]		= "zertifikate2",
	[ZERTIFIKATE3]		= "zertifikate3",
	[ZERTIFIKATE4]		= "zertifikate4",
	[FINAL]			= "final",
	[PRODCERT1]		= "prodcert1",
	[PRODCERT2]		= "prodcert2",
	[PRODCERT3]		= "prodcert3",
	[PRODCERT4]		= "prodcert4",
	[PRODENV]		= "prodenv",
	[WLAN3_ZIP]		= "wlan3_zip",
	[WLAN3]			= "wlan3",
	[AVMZERTIFIKATE]	= "avmzertifikate",
};

static void compiletime_checks(void) __always_unused;
static void compiletime_checks(void)
{
	BUILD_BUG_ON(ARRAY_SIZE(avm_prom_config_type_name) !=
			AVM_PROM_NUM_TYPES);
}

struct private_data {
	void *buffer;
	size_t size;
};

static int __avm_prom_config_open(enum avm_prom_config_type type,
		struct inode *inode, struct file *file)
{
	struct private_data *data;
	ssize_t ret;

	WARN_ON(file->private_data);

	data = kmalloc(sizeof(struct private_data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	ret = avm_prom_get_config_alloc(type, &data->buffer);
	if (ret < 0) {
		kfree(data);
		return ret;
	}

	data->size = ret;

	file->private_data = data;

	return 0;
}

static int avm_prom_config_open_procfs(struct inode *inode, struct file *file)
{
	enum avm_prom_config_type type =
		(enum avm_prom_config_type)PDE_DATA(inode);

	return __avm_prom_config_open(type, inode, file);
}

static int avm_prom_config_open_debugfs(struct inode *inode,
					struct file *file) __maybe_unused;
static int avm_prom_config_open_debugfs(struct inode *inode, struct file *file)
{
	enum avm_prom_config_type type =
		(enum avm_prom_config_type)inode->i_private;

	return __avm_prom_config_open(type, inode, file);
}

static ssize_t avm_prom_config_read(struct file *file, char __user *buf,
				    size_t len, loff_t *ofs)
{
	struct private_data *data = file->private_data;

	return simple_read_from_buffer(buf, len, ofs, data->buffer, data->size);
}

static int avm_prom_config_release(struct inode *inode, struct file *file)
{
	struct private_data *data = file->private_data;

	vfree(data->buffer);
	kfree(data);

	return 0;
}

static const struct file_operations proc_fops = {
	.open = avm_prom_config_open_procfs,
	.read = avm_prom_config_read,
	.release = avm_prom_config_release,
};

static const struct file_operations debugfs_fops __maybe_unused = {
	.open = avm_prom_config_open_debugfs,
	.read = avm_prom_config_read,
	.release = avm_prom_config_release,
};

int avm_prom_create_config_proc(enum avm_prom_config_type type)
{
	if (!calibprocdir)
		calibprocdir = proc_mkdir(PROC_CALIBDIR, NULL);

	if (!calibprocdir) {
		pr_err("%s: can't create %s\n", __func__, PROC_CALIBDIR);
		return -ENOMEM;
	}

	proc_create_data(avm_prom_config_type_name[type], 0, calibprocdir,
			&proc_fops, (void *)type);

	return 0;
}

int avm_prom_create_config_debugfs(enum avm_prom_config_type type)
{
	if (!prom_config_debug)
		return 0;

	if (!debugfs_dir) {
		struct dentry *d;

		d = debugfs_create_dir("avm-prom", NULL);
		if (IS_ERR_OR_NULL(d))
			return 0;

		debugfs_dir = d;
	}

	debugfs_create_file(avm_prom_config_type_name[type], 0444,
			debugfs_dir, (void *)type, &debugfs_fops);

	return 0;
}
