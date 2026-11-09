// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/sizes.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/semaphore.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include <linux/zlib.h>
#include <avm/enh/reboot_status.h>
#include <avm/tffs/tffs.h>
#include <avm/compat/proc_fs.h>
#include <linux/env.h>
#include "local.h"

#ifdef CONFIG_SYSCTL

#if !defined(CONFIG_TFFS_ENV)
#error "no CONFIG_TFFS_ENV defined"
#endif /*--- #if !defined(CONFIG_TFFS_ENV) ---*/

#define DEV_ADAM2     6
#define DEV_ADAM2_ENV 1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
/* Evaluates to zero, because memcpy always succeeds */
#define sysctl_copy_from_user(dst, src, len) (memcpy(dst, src, len), 0)
#define sysctl_copy_to_user(dst, src, len)   (memcpy(dst, src, len), 0)
#else
#define sysctl_copy_from_user(dst, src, len) copy_from_user(dst, src, len)
#define sysctl_copy_to_user(dst, src, len)   copy_to_user(dst, src, len)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
#define register_sysctl_sz(path, ctl, size) register_sysctl(path, ctl)
#endif

/* Legacy environment file format is "%s\t%s\n" per entry. */
#define URLADER_ENV_BUF_ENTRY_SIZE \
	(URLADER_ENV_NAME_MAX + URLADER_ENV_VALUE_MAX + 1)
static struct legacy_env_cache {
	struct mutex lock;
	bool valid;
} legacy_env_cache;

static int __legacy_getenv_begin(void)
{
	return mutex_lock_killable(&legacy_env_cache.lock);
}

static void __legacy_getenv_end(void)
{
	mutex_unlock(&legacy_env_cache.lock);
}

int avm_urlader_setenv_begin(void)
{
	return mutex_lock_killable(&legacy_env_cache.lock);
}

void avm_urlader_setenv_end(void)
{
	legacy_env_cache.valid = false;
	mutex_unlock(&legacy_env_cache.lock);
}

static int avm_urlader_sysctl_environ(struct ctl_table *ctl, int write,
				      void *buffer, size_t *lenp, loff_t *ppos)
{
	int tffs_id = (unsigned long)ctl->extra1;
	char data[URLADER_ENV_VALUE_MAX];
	int ret;

	if (write) {
		size_t len = *lenp;

		if (*ppos)
			return -EBADFD;
		/*
		 * Final data including NUL must not exceed URLADER_ENV_VALUE_MAX.
		 * But we strip a trailing newline, so we have to boundary-check
		 * in two steps, one before copying the user string, and one
		 * after stripping a newline.
		 */
		if (len > sizeof(data))
			return -EFBIG;

		if (sysctl_copy_from_user(data, buffer, len))
			return -EFAULT;

		*ppos = len;

		if (len && data[len-1] == '\n')
			--len;

		/* Check boundary again after newline stripping */
		if (len == sizeof(data))
			return -EFBIG;

		/* We interpret an empty value as an unset request */
		if (!len) {
			ret = avm_urlader_unsetenv_by_id(tffs_id);
		} else {
			data[len] = '\0';
			ret = avm_urlader_setenv_by_id(tffs_id, data);
		}

		return ret;
	} else {
		int len, ret;
		char *d;

		ret = avm_urlader_getenv_by_id(tffs_id, data, sizeof(data));
		if (ret)
			return ret;

		len = strlen(data);
		if (*ppos >= len) {
			*lenp = 0;
			goto out;
		}
		d = &data[*ppos];
		len -= *ppos;
		if (len > *lenp)
			len = *lenp;
		if (len < *lenp)
			d[len++] = '\n';

		if (sysctl_copy_to_user(buffer, d, len)) {
			ret = -EFAULT;
			goto out;
		}
		*lenp = len;
		*ppos += len;
out:
		return ret;
	}
}

static int avm_urlader_getenv_proc(struct ctl_table *ctl, int write,
				   void *buffer, size_t *lenp, loff_t *ppos)
{
	if (write)
		return -EPERM;
	return avm_urlader_sysctl_environ(ctl, false, buffer, lenp, ppos);
}

/**
 */
#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_GEN3) || \
	defined(CONFIG_MACH_PUMA7)
static int avm_urlader_get_config(struct ctl_table *ctl, int write, void *buffer, size_t *lenp,
				  loff_t *ppos)
{
	char tmp_buf[URLADER_ENV_VALUE_MAX];
	int ret;

	if (!*lenp || (*ppos && !write)) {
		*lenp = 0;
		return 0;
	}

	if (write) {
		size_t len = *lenp;

		if (len >= URLADER_ENV_VALUE_MAX)
			return -EOVERFLOW;
		if (sysctl_copy_from_user(tmp_buf, buffer, len))
			return -EFAULT;
		tmp_buf[len] = '\0';

		ret = copy_wlan_dect_config2user(tmp_buf, len + 1);
		if (ret)
			return ret;

		*ppos = len;
		*lenp = len; /*--- lenp darf nicht kleiner sein als bei Uebergabe, sonst werden endlos aufgerufen ---*/

		return 0;
	} else {
		char *mybuffer = kmalloc(*lenp, GFP_KERNEL);

		if (mybuffer) {
			if (!test_wlan_dect_config(mybuffer, lenp)) {
				if (sysctl_copy_to_user(buffer, mybuffer, *lenp)) {
					kfree(mybuffer);
					return -EFAULT;
				}
				*ppos = *lenp;
				kfree(mybuffer);
				return 0;
			}
			kfree(mybuffer);
		} else {
			pr_err("[%s] kmalloc failed\n", __func__);
		}
	}
	return -EFAULT;
}
#endif /*--- #if defined(CONFIG_ARCH_PUMA5)  || defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_GEN3)---*/

static int __legacy_env_cache_update(struct ctl_table *ctl)
{
	const struct _TFFS_Name_Table *nametbl = TFFS_Name_Table;
	struct legacy_env_cache *cache = &legacy_env_cache;
	size_t bufsiz;
	char *buffer;
	int i, result = 0;
	size_t total = 0;

	if (cache->valid)
		return 0;

	buffer = ctl->data;
	bufsiz = ctl->maxlen;

	/* Scan through the flash, looking for each possible variable index */
	for (i = 0; i < ARRAY_SIZE(TFFS_Name_Table); i++) {
		char val[URLADER_ENV_VALUE_MAX];
		const char *var;
		int id;

		id  = nametbl[i].id;
		var = nametbl[i].Name;

		if (!id)
			break;

		result = avm_urlader_getenv_by_id(id, val, sizeof(val));
		if (result) {
			if (result == -ENOENT) {
				result = 0;
				continue;
			}
			if (result == -EINTR)
				result = -ERESTARTSYS;
			goto err_out;
		}

		if (total + strlen(var) + strlen(val) + sizeof("\t\n") >= bufsiz) {
			size_t biggersiz;
			char *biggerbuf;

			biggersiz = bufsiz ? 2*bufsiz : SZ_1K;
			biggerbuf = krealloc(buffer, biggersiz, GFP_KERNEL);
			if (!biggerbuf) {
				pr_err("Out of memory: Truncating legacy sysctl urlader environment\n");
				break;
			}
			buffer = biggerbuf;
			bufsiz = biggersiz;
		}

		total += sprintf(&buffer[total], "%s\t%s\n", var, val);
	}
err_out:
	ctl->data = buffer;
	ctl->maxlen = bufsiz;
	cache->valid = !result;
	return result;
}

static bool avm_urlader_env_hidden_id(unsigned int id)
{
	return id == FLASH_FS_TABLE_VERSION;
}

static int avm_urlader_count_environ(void)
{
	int i, cnt = 0;

	for (i = 0; i < ARRAY_SIZE(TFFS_Name_Table); ++i) {
		const char *name = TFFS_Name_Table[i].Name;
		unsigned int id  = TFFS_Name_Table[i].id;

		if (!id)
			break;

		if (*name && !avm_urlader_env_hidden_id(id))
			cnt++;
	}
	return cnt;
}

static struct ctl_table_header *register_urlader_environ_sysctl(void)
{
	struct ctl_table *ctl;
	int tblsiz, i, j;

	tblsiz = avm_urlader_count_environ();
	ctl = kcalloc(tblsiz + 1, sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return ERR_PTR(-ENOMEM);

	for (i = 0, j = 0; i < ARRAY_SIZE(TFFS_Name_Table); ++i) {
		const char *name = TFFS_Name_Table[i].Name;
		unsigned long id  = TFFS_Name_Table[i].id;

		if (!id)
			break;
		if (!*name || avm_urlader_env_hidden_id(id))
			continue;

		BUG_ON(j == tblsiz);
		ctl[j++] = (struct ctl_table) {
			.procname     = name,
			.mode         = 0664,
			.proc_handler = avm_urlader_sysctl_environ,
			.extra1       = (void *)id,
		};
	}

	return register_sysctl_sz("urlader/environ", ctl, j);
}

/**
 */
static int avm_urlader_setenv_sysctl(struct ctl_table *ctl, int write, void *buffer, size_t *lenp,
				     loff_t *ppos)
{
	char tmp_buf[URLADER_ENV_BUF_ENTRY_SIZE];
	char *ptr;
	int result = 0;

	if (!write) {
		result = __legacy_getenv_begin();
		if (result)
			return result;

		result = __legacy_env_cache_update(ctl);
		if (!result)
			result = proc_dostring(ctl, write, buffer, lenp, ppos);
		__legacy_getenv_end();

		return result;
	}

	if (ctl->extra1 != (void *)DEV_ADAM2_ENV) {
		result = -ENODEV;
		goto err_out;
	}

	if (*lenp >= ARRAY_SIZE(tmp_buf)) {
		result = -EINVAL;
		goto err_out;
	}

	if (sysctl_copy_from_user(tmp_buf, buffer, *lenp)) {
		result = -EFAULT;
		goto err_out;
	}

	tmp_buf[*lenp] = '\0';
	if (tmp_buf[*lenp - 1] == '\n') {
		tmp_buf[*lenp - 1] = '\0';
	}

	*ppos = *lenp;

	no_printk(KERN_ERR "{%s}\n", __func__);
	/*--- dump_stack(); ---*/

	ptr = strpbrk(tmp_buf, " \t");
	if (!ptr) {
		/* We have no way to distinguish between unsetting a
		 * variable and setting a non-value variable.
		 *
		 * Consequently, we will treat any variable
		 * without a value as an unset request.
		 */
		avm_urlader_unsetenv(tmp_buf);
	} else {
		/* Set the variable-value pair in flash */
		*ptr++ = '\0';

		result = avm_urlader_setenv(tmp_buf, ptr);
	}
err_out:
	return result;
}

/**
 */
static int avm_urlader_reset_status_proc(struct ctl_table *ctl, int write, void *buffer,
					 size_t *lenp, loff_t *ppos)
{
	char tmp_buf[URLADER_ENV_VALUE_MAX] __maybe_unused;
	int result;

	result = 0;
	if (!*lenp || (*ppos && !write)) {
		*lenp = 0;
		goto err_out;
	}

	if (write) {
		if (*lenp >= URLADER_ENV_VALUE_MAX) {
			result = -EFAULT;
			goto err_out;
		}

		if (sysctl_copy_from_user(tmp_buf, buffer, *lenp)) {
			result = -EFAULT;
			goto err_out;
		}

		tmp_buf[*lenp] = '\0';
		if (tmp_buf[*lenp - 1] == '\n') {
			tmp_buf[*lenp - 1] = '\0';
		}
		*ppos = *lenp;

		result = avm_sysctl_set_reset_status(tmp_buf);
	} else {
		char reason[32];

		*ppos = *lenp = scnprintf(reason, sizeof(reason), "%s\n",
					  avm_sysctl_reset_status());

		if (sysctl_copy_to_user(buffer, reason, *lenp)) {
			result = -EFAULT;
		}
	}
err_out:
	return result;
}

struct ctl_table urlader_sysctl_tbl[] = {
	// Start of deprecated files, users should switch to environ/*
	{
	  .extra1	= (void *)DEV_ADAM2_ENV,
	  .procname	= "environment",
	  .mode		= 0644,
	  .proc_handler = &avm_urlader_setenv_sysctl },
	{
	  .extra1	= (void *)FLASH_FS_FIRMWARE_VERSION,
	  .procname	= "firmware_version",
	  .data		= NULL,
	  .maxlen	= URLADER_ENV_VALUE_MAX,
	  .mode		= 0444,
	  .proc_handler = &avm_urlader_getenv_proc },
	{
	  .extra1	= (void *)FLASH_FS_ANNEX,
	  .procname	= "annex",
	  .data		= NULL,
	  .maxlen	= URLADER_ENV_VALUE_MAX,
	  .mode		= 0444,
	  .proc_handler = &avm_urlader_getenv_proc },
	// End of deprecated files
	{
	  .procname	= "reboot_status",
	  .data		= NULL,
	  .maxlen	= URLADER_ENV_VALUE_MAX,
	  .mode		= 0644,
	  .proc_handler = &avm_urlader_reset_status_proc },
#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_GEN3) || \
	defined(CONFIG_MACH_PUMA7)
	{
	  .procname	= "config",
	  .data		= NULL,
	  .maxlen	= 0,
	  .mode		= 0644,
	  .proc_handler = &avm_urlader_get_config },
#endif
	{ /* sentinel */ }
};

/* /proc/sys/urlader */
static struct ctl_table_header *urlader_sysctl_header;
/* /proc/sys/urlader/environ */
static struct ctl_table_header *environ_sysctl_header;

int avm_urlader_sysctl_init(void)
{
	static int initialized;

	if (initialized)
		return 0;

	mutex_init(&legacy_env_cache.lock);

	urlader_sysctl_header = register_sysctl("urlader", urlader_sysctl_tbl);
	if (!urlader_sysctl_header)
		pr_err("Error creating urlader sysctl table\n");

	environ_sysctl_header = register_urlader_environ_sysctl();
	if (!environ_sysctl_header)
		pr_err("Error creating urlader/environ sysctl table\n");

	return 0;
}

#if defined(MODULE)
void avm_urlader_env_exit(void)
{
	unregister_sysctl_table(environ_sysctl_header);
	unregister_sysctl_table(urlader_sysctl_header);
}
#endif /*--- #if defined(MODULE) ---*/

#endif /* CONFIG_SYSCTL */

/**
 */
#ifdef CONFIG_PROC_FS
static struct tffs_cdev *tffsptr;
static struct proc_dir_entry *tffs_proc;
unsigned int tffs_info_value;

/**
 */
static ssize_t tffsproc_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	char page[80];
	char *p;
	int request = -1;
	unsigned int id;
	struct tffs_core_handle *handle;
	int result;

	handle = NULL;
	result = 0;

	if (IS_ERR_OR_NULL(tffsptr)) {
		result = -ENODEV;
		goto err_out;
	}

	if (count > 0 && count <= sizeof(page)) {
		if (copy_from_user(&page[0], buffer, count))
			result = -EFAULT;
		page[count - 1] = '\0';
	} else {
		result = -EINVAL;
	}

	if (result != 0) {
		goto err_out;
	}

	p = skip_spaces(&page[0]);
	if (strstarts(p, "info")) {
		request = _TFFS_INFO;
	} else if (strstarts(p, "index")) {
		request = _TFFS_REINDEX;
	} else if (strstarts(p, "cleanup")) {
		request = _TFFS_CLEANUP;
	} else if (strstarts(p, "clear_id")) {
		request = _TFFS_CLEAR_ID;
	} else if (strstarts(p, "werkseinstellung")) {
		request = _TFFS_WERKSEINSTELLUNG;
	} else {
		pr_err("/proc/tffs: request not recognized\n");
		result = -EINVAL;
		goto err_out;
	}

	switch (request) {
	case _TFFS_INFO:
	case _TFFS_REINDEX:
	case _TFFS_CLEANUP:
	case _TFFS_WERKSEINSTELLUNG:
		id = 0;
		break;
	case _TFFS_CLEAR_ID:
		p += strlen("clear_id");
		p      = skip_spaces(p);
		result = kstrtouint(p, 0, &id);
		if (result != 0 || id == 0 || id > FLASH_FS_ID_LAST) {
			pr_err("/proc/tffs: clear id request: invalid id %s\n", p);
			result = -EINVAL;
		}
		break;
	default:
		pr_err("/proc/tffs: command not recognized\n");
		result = -EINVAL;
	}

	if (result != 0) {
		goto err_out;
	}

	handle = TFFS3_Open(id, tffs3_mode_write);
	if (IS_ERR_OR_NULL(handle)) {
		result = (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
		goto err_out;
	}

	switch (request) {
	case _TFFS_INFO:
		result = TFFS3_Info(handle, &tffs_info_value);
		pr_info("/proc/tffs: info request: %s\n", result ? "failed" : "success");
		break;
	case _TFFS_REINDEX:
		result = TFFS3_Create_Index();
		pr_info("/proc/tffs: index request: %s\n", result ? "failed" : "success");
		break;
	case _TFFS_CLEANUP:
		result = TFFS3_Cleanup(handle);
		pr_info("/proc/tffs: cleanup request: %s\n", result ? "failed" : "success");
		break;
	case _TFFS_WERKSEINSTELLUNG:
		result = TFFS3_Werkseinstellungen(handle);
		pr_info("/proc/tffs: werkseinstellungen request: %s\n",
			result ? "failed" : "success");
		break;
	case _TFFS_CLEAR_ID:
		result = TFFS3_Clear(handle);
		pr_info("/proc/tffs: clear id 0x%x request: %s\n", id,
			result ? "failed" : "success");
		break;
	}

err_out:
	if (!IS_ERR_OR_NULL(handle)) {
		TFFS3_Close(handle);
	}
	return result == 0 ? count : result;
}

/**
 */
static int tffsproc_show(struct seq_file *seq, void *data)
{
	seq_puts(seq, "TFFS\n");

#if defined(CONFIG_TFFS2) || defined(CONFIG_TFFS_DEV_LEGACY)
	seq_printf(seq, "mount=mtd%u\n", tffs_mtd[0]);
	seq_printf(seq, "mount=mtd%u\n", tffs_mtd[1]);
#endif

	seq_printf(seq, "request=%u\n", tffsptr->request_count);

	if (tffs_info_value) {
		seq_printf(seq, "fill=%u\n", tffs_info_value);
	}

	if (tffsptr->pending_events) {
		seq_puts(seq, "event pending\n");
	}

#if defined(CONFIG_TFFS_EXCLUSIVE)
	seq_puts(seq, "mode: read/write: exclusive\n");
#else /*--- #if defined(CONFIG_TFFS_EXCLUSIVE) ---*/
	seq_puts(seq, "mode: read/write: shared\n");
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_EXCLUSIVE) ---*/

	seq_printf(seq, "thread state=%s\n",
		   tffsptr->thread_state == tffs3_thread_state_off     ? "off" :
		   tffsptr->thread_state == tffs3_thread_state_init    ? "init" :
		   tffsptr->thread_state == tffs3_thread_state_idle    ? "idle" :
		   tffsptr->thread_state == tffs3_thread_state_process ? "process" :
		   tffsptr->thread_state == tffs3_thread_state_down    ? "down" :
									       "unknown");

	return 0;
}

/**
 */
static int tffsproc_open(struct inode *inode, struct file *file)
{
	int result;

	if (IS_ERR_OR_NULL(tffsptr)) {
		result = -ENODEV;
	} else {
		result = single_open(file, tffsproc_show, NULL);
	}
	return result;
}

/**
 */
static const struct proc_ops tffsproc_ops = {
	.proc_open	= tffsproc_open,
	.proc_read	= seq_read,
	.proc_write	= tffsproc_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

void tffs_proc_init(struct tffs_cdev *tffs)
{
	tffsptr = tffs;

	if (!IS_ERR_OR_NULL(tffsptr)) {
		tffs_proc = proc_create("tffs", 0, 0, &tffsproc_ops);
	}
}

void tffs_proc_remove(struct tffs_cdev *tffs)
{
	if (!IS_ERR_OR_NULL(tffs_proc)) {
		remove_proc_entry("tffs", 0);
	}
}

#endif /*--- #ifdef CONFIG_PROC_FS ---*/
