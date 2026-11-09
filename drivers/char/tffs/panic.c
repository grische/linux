// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2007 AVM GmbH <fritzbox_info@avm.de> */
#define pr_fmt(fmt) "tffs/panic: " fmt
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/rtc.h>
#include <linux/env.h>
#include <linux/kmsg_dump.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/circ_buf.h>
#include <linux/pstore.h>
#include <linux/string_helpers.h>
#include <linux/crc32.h>
#include <avm/watchdog/watchdog.h>
#include <avm/enh/fw_info.h>
#include <avm/enh/reboot_status.h>
#include <avm/compat/proc_fs.h>
#include <avm/sammel/helpers.h>
#include <avm/compat/misc.h>
#include "local.h"

#ifdef DEBUG
#  undef pr_devel
#  define pr_devel pr_info
#endif

#ifdef USERLOG_DEBUG
#  define userlog_debug_enabled 1
#  define userlog_debug(fmt, ...) \
	printk(KERN_INFO "%s: " pr_fmt(fmt) , __func__, ##__VA_ARGS__)
#else
#  define userlog_debug_enabled 0
#  define userlog_debug(fmt, ...) \
	no_printk(KERN_INFO "%s: " pr_fmt(fmt) , __func__, ##__VA_ARGS__)
#endif

#define HAVE_STRING_ESCAPEMEM \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
#if 1
#  define USERLOG_BUFSIZE (1U << CONFIG_LOG_BUF_SHIFT)
#  define userlog_do_show_buf 0
#else /* userlog buffer debugging: */
#  if !HAVE_STRING_ESCAPEMEM
#    error userlog buffer debugging requires newer kernel
#  endif
   /* Use a very short buffer for the userlog, this allows
	* easy wraparound testing and it can be completely printk'ed */
#  define USERLOG_BUFSIZE 32 /* Must be a power of two */
#  define userlog_do_show_buf 1
#endif

/* circ_buf.buf has always one char unused: */
#define USERLOG_BUFCAPACITY (USERLOG_BUFSIZE - 1)

#if IS_ENABLED(CONFIG_TFFS_USERLOG_TMPBUF_PSTORE) && !defined(pstore_copy)
#error PSTORE buffer requires a kernel providing pstore_copy.
#endif

#define PANICLOG_MAX_SIZE (1 << CONFIG_LOG_BUF_SHIFT)
#define CRASHLOG_BUFSIZE  (1 << CONFIG_LOG_BUF_SHIFT)

/**
 * JZ-113261:
 * When pstore is used, more than PANICLOG_MAX_SIZE bytes can be written, because
 * the printk-LOG_BUF contains binary-information for each line (timestamp,
 * cpu-index) which will be expanded by using from kmsg_dump_get_buffer()
 * So we should use spacious temporary buffer ...
 */
static size_t log_max_size(int tffs_id)
{
	switch (tffs_id) {
	case FLASH_FS_ID_PANIC_LOG:
	case FLASH_FS_ID_PANIC2_LOG:
		return PANICLOG_MAX_SIZE * 2;
	case FLASH_FS_ID_USER_LOG:
	case FLASH_FS_ID_USER2_LOG:
		return USERLOG_BUFSIZE;
	case FLASH_FS_ID_CRASH_LOG:
	case FLASH_FS_ID_CRASH2_LOG:
		return CRASHLOG_BUFSIZE;
	}

	BUG();
}

static struct tffs_fops_handle *panic_handle;

unsigned int tffs_panic_log_suppress;
EXPORT_SYMBOL(tffs_panic_log_suppress);

#if defined(CONFIG_TFFS_REMOTE_CRASHLOG_WRITER) || \
    defined(CONFIG_TFFS_REMOTE_CRASHLOG_READER)
#define __exported_for_remote_crashlog /* extern */
#else
#define __exported_for_remote_crashlog static
#endif

__exported_for_remote_crashlog DEFINE_SEMAPHORE_1(tffs_log_sema);


/**
 * panic-log provided by platform code. May be written before tffs is up,
 * therefore stored as a separate buffer.
 */
static char platform_panic_log[512];

static int tffs_read_platform_paniclog(char *buf, size_t bufsiz, unsigned int id)
{
	size_t len;

	if (!IS_ENABLED(CONFIG_AVM_PLATFORM_RESET_STATUS))
		return 0;

	if (id != FLASH_FS_ID_PANIC_LOG || !bufsiz)
		return 0;

	len = strlcpy(buf, platform_panic_log, bufsiz);

	return min_t(int, len, bufsiz - 1);
}

/**
 * Schreiben im Panic-Kontext
 */
static void tffs_panic_log_open(void)
{
	if (panic_handle == NULL) {
		panic_handle = tffs_open_panic();
	}
}

/**
 * Schreiben im Panic-Kontext
 */
static void tffs_panic_log_write(const char *buffer, unsigned int len)
{
	static loff_t off;

	if (panic_handle != NULL) {
		tffs_write_kern(panic_handle, buffer, len, &off);
	}
}

/**
 * Schreiben im Panic-Kontext
 */
static void tffs_panic_log_close(void)
{
	if (panic_handle != NULL) {
		tffs_release_kern(panic_handle);
		panic_handle = NULL;
	}
}

static void replace_nulchars(unsigned char *buf, size_t buflen)
{
	unsigned char *nul = buf;

	while ((nul = memchr(nul, '\0', buflen - (nul - buf))))
		*nul++ = ' ';
}

/**
 */
static unsigned int calc_sum(const unsigned char *buf, int buf_len)
{
	unsigned int i, sum = 0;

	for (i = 0; i < buf_len; i++)
		sum += buf[i];

	return sum ^ (buf_len << 16);
}

static u32 calc_crc32(const unsigned char *buf, size_t buflen)
{
	return crc32(~0, buf, buflen);
}

/**
 */
struct _time_checksum {
#define CRASH_CHKSUM_OLD    1
#define CRASH_CHKSUM_CRC32  2
	unsigned int fields;	/* bitfield tracking the checksums present */
	unsigned int cs;	/* old checksum, obsolete custom algo */
	u32          crc32;	/* checksum replacing .cs */
	time64_t     ts;	/* timestamp on read */
#define READ_BY_CR          0x1
#define READ_BY_SD          0x2
#define FIRST_READ_BY_SD    0x4	/* first read by SD */
	unsigned int readmask;
};

static bool checksum_valid(const struct _time_checksum *chksum)
{
	return !!chksum->ts;
}

static bool crc32_present(const struct _time_checksum *chksum)
{
	return chksum->fields & CRASH_CHKSUM_CRC32;
}

static void crc32_enable(struct _time_checksum *chksum)
{
	chksum->fields |= CRASH_CHKSUM_CRC32;
}

static bool checksum_matches(const struct _time_checksum *chksum, char *buf, size_t len)
{
	if (crc32_present(chksum))
		return calc_crc32(buf, len) == chksum->crc32;

	/*
	 * Yikes! No crc32. Sadly, computing the old checksum requires
	 * modifying the buffer.
	 */
	replace_nulchars(buf, len);

	return calc_sum(buf, len) == chksum->cs;
}

#define MAX_LOG_IDS 6

/* Do we have unreported userspace crashes from a previous boot? */
static bool old_unreported_crash[MAX_LOG_IDS];

/**
 */
static int array_index(unsigned int tffs_id)
{
	switch (tffs_id) {
	case FLASH_FS_ID_PANIC_LOG:
		return 0x0;
	case FLASH_FS_ID_PANIC2_LOG:
		return 0x1;
	case FLASH_FS_ID_CRASH_LOG:
		return 0x2;
	case FLASH_FS_ID_CRASH2_LOG:
		return 0x3;
	case FLASH_FS_ID_USER_LOG:
		return 0x4;
	case FLASH_FS_ID_USER2_LOG:
		return 0x5;
	default:
		BUG();
	}
}

/**
 * Allow reading log multiple times only for "log_sd" in private/internal builds
 */
static inline bool log_read_multiple_allowed(int report_type)
{
	bool ret = false;

	if (report_type == READ_BY_SD) {
		int buildtype = avm_fw_buildtype();

		if (buildtype == avm_fw_buildtype_Private || buildtype == avm_fw_buildtype_Intern)
			ret = true;
	}
	return ret;
}

/**
 */
static ssize_t tffs_simple_kern_read(unsigned char *buf, int buf_len, unsigned int id)
{
	struct tffs_fops_handle *handle;
	loff_t offp = 0;
	ssize_t read;

	handle = tffs_open_kern(id, 0);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	read = tffs_read_kern(handle, buf, buf_len, &offp);
	tffs_release_kern(handle);

	return read;
}

static ssize_t tffs_simple_kern_write(void *buf, size_t len, unsigned int id)
{
	struct tffs_fops_handle *handle;
	ssize_t ret;
	loff_t off;

	handle = tffs_open_kern(id, 1);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	if (len > 0)
		ret = tffs_write_kern(handle, buf, len, &off);
	else
		ret = 0;

	tffs_release_kern(handle);

	return ret;
}

/**
 */
static int paniclog_show(struct seq_file *seq, void *data)
{
	/*--- pr_info("%s %p %p\n", __func__, seq->private, data); ---*/
	if (seq->private) {
		seq_printf(seq, "%s", (char *)seq->private);
	}
	return 0;
}

/**
 * return: gefuelltes checksum
 * [id]cs,ts,readmask,<crc32>[id2]cs,ts,readmask,<crc32>
 * crc32 may not be present if the variable was set by old F!OS firmware.
 * (new crc32-feature since 2.61 - MOVE21/PSQ19P2NL4)
 */
static void parse_crash_env_old(const char *crashp, struct _time_checksum checksum[])
{
	memset(checksum, 0, MAX_LOG_IDS * sizeof(struct _time_checksum));

	while ((crashp = strchr(crashp, '['))) {
		struct _time_checksum _chksum = {0};
		unsigned int id;
		int crash_n_fields;

		_chksum.fields = CRASH_CHKSUM_OLD;
		crash_n_fields = sscanf(crashp, "[%x]%x,%llx,%x,%x",
				&id, &_chksum.cs, &_chksum.ts,
				&_chksum.readmask, &_chksum.crc32);
		if (crash_n_fields >= 3 && id < MAX_LOG_IDS) {
			if (crash_n_fields == 5)
				crc32_enable(&_chksum);
			checksum[id] = _chksum;
		}
		++crashp;
	}
}

/**
 * return: gefuelltes checksum
 * crc32_id1,ts_id1,readmask_id1:crc32_id2,ts_id2,readmask_id2: ...
 *
 * Mit Fallback zum alten Format: siehe parse_crash_env_old()
 */
static void parse_crash_env(const char *crashp, struct _time_checksum checksum[])
{
	unsigned int id;

	if (*crashp == '[')
		return parse_crash_env_old(crashp, checksum);

	memset(checksum, 0, MAX_LOG_IDS * sizeof(struct _time_checksum));
	for (id = 0; id < MAX_LOG_IDS; id++) {
		struct _time_checksum _chksum = { 0 };
		int crash_n_fields;

		crc32_enable(&_chksum);
		crash_n_fields = sscanf(crashp, "%x,%llx,%x",
					&_chksum.crc32, &_chksum.ts,
					&_chksum.readmask);
		if (crash_n_fields != 3)
			break;

		checksum[id] = _chksum;

		crashp = strchr(crashp, ':');
		if (crashp == NULL)
			break;
		crashp++;
	}
}

/**
 * Kompatibilität:
 * Falls es noch Elemente gibt, in denen noch kein crc32 gesetzt.
 * Hier muessen wir leider noch das ganz alte checksum-Format weiter unterstützen.
 */
static bool can_migrate_old_chksums(const struct _time_checksum checksum[])
{
	unsigned int i;

	for (i = 0; i < MAX_LOG_IDS; i++) {
		if (checksum_valid(&checksum[i]) && !crc32_present(&checksum[i]))
			return false;
	}
	return true;
}

/**
 * Kompatibilität:
 * return: gefuelltes checksum
 * [id]cs,ts,readmask,<crc32>[id2]cs,ts,readmask,<crc32>
 * crc32 may not be present if the variable was set by old F!OS firmware.
 */
static void set_crash_env_old(char *crashp, int len,
				  const struct _time_checksum checksum[])
{
	unsigned int written, i;

	for (i = 0; i < MAX_LOG_IDS && len > 0; i++) {
		if (crc32_present(&checksum[i]))
			written = scnprintf(crashp, len, "[%x]%x,%llx,%x,%x", i,
					   checksum[i].cs,
					   checksum[i].ts,
					   checksum[i].readmask,
					   checksum[i].crc32);
		else
			written = scnprintf(crashp, len, "[%x]%x,%llx,%x", i,
					   checksum[i].cs,
					   checksum[i].ts,
					   checksum[i].readmask);
		len -= written, crashp += written;
	}
}

/**
 * Format:
 * crc32_id1,ts_id1,readmask_id1:crc32_id2,ts_id2,readmask_id2: ...
 *
 * Falls noch nicht komplett alle Elemente auf crc32 umgestellt
 * noch das alte Format schreiben!
 */
static void set_crash_env(char *crashp, int len, const struct _time_checksum checksum[])
{
	unsigned int written, i;

	if (!can_migrate_old_chksums(checksum))
		return set_crash_env_old(crashp, len, checksum);

	for (i = 0; i < MAX_LOG_IDS && len > 0; i++) {
		written = scnprintf(crashp, len, "%s%x,%llx,%x",
					i == 0 ? "" : ":",
					checksum[i].crc32,
					   checksum[i].ts,
					   checksum[i].readmask);
		len -= written, crashp += written;
	}
}

#define CRASHENV_MAX_LEN 128

static int read_crash_var(struct _time_checksum chksum[])
{
	char crashenv[CRASHENV_MAX_LEN];
	int ret;

	ret = tffs_read_entry(FLASH_FS_CRASH, crashenv, sizeof(crashenv));
	if (ret < 0) {
		if (ret != -ENOENT)
			pr_err("%ps: Error reading crashenv: %d\n", (void *)_RET_IP_, ret);
		return ret;
	}

	pr_devel("%ps: Read crashenv='%s'\n", (void *)_RET_IP_, crashenv);

	parse_crash_env(crashenv, chksum);

	return 0;
}

static bool tffs_userlog_read_forbidden;
/*
 * We don't allow a userspace crash to hijack a userlog with
 * panic-related content that has not yet been reported.
 */
static bool tffs_userlog_reserved_for_paniclog;

static bool tffs_log_read_forbidden(int tffs_id, bool crashreport)
{
	if (userlog_debug_enabled && crashreport && tffs_id == _FLASH_FS_ID_USER_LOG) {
		/*
		 * HAX: Block ctlmgr from messing with
		 * /proc/avm/log_cr/user, which interferes
		 * with manual testing
		 */
		char comm[TASK_COMM_LEN];

		if (!strcmp(get_task_comm(comm, current), "ctlmgr"))
			return true;
	}

	switch (tffs_id) {
	case FLASH_FS_ID_USER_LOG:
	case FLASH_FS_ID_USER2_LOG:
		if (tffs_userlog_read_forbidden)
			return true;
		if (tffs_userlog_reserved_for_paniclog)
			return false;
		fallthrough;
	case FLASH_FS_ID_CRASH_LOG:
	case FLASH_FS_ID_CRASH2_LOG:
		/* Support data can always read log data */
		if (!crashreport)
			return false;
		/*
		 * Forbidding locks are only in effect if all existing log
		 * entries have been read.
		 */
		if (AVM_WATCHDOG_Crashlog_read_forbidden() ||
		    avm_remote_crashlog_is_read_forbidden())
			return !old_unreported_crash[array_index(tffs_id)];
	}

	return false;
}

static ssize_t tffs_read_log(void *buf, size_t bufsiz, int tffs_id)
{
	ssize_t buflen;

	buflen = tffs_read_platform_paniclog(buf, bufsiz, tffs_id);
	if (buflen > 0)
		return buflen;

	return tffs_simple_kern_read(buf, bufsiz, tffs_id);
}

#define LOG_FOOTER_SIZE 512
#define LOG_FOOTER_SEP	"-----\n"

/**
 * report_type: READ_BY_SD      Support-Data
 *              READ_BY_CR      CrashReport
 */
static int __log_proc_open(struct inode *inode, struct file *file, unsigned int report_type, int id)
{
	char crashenv[CRASHENV_MAX_LEN];
	struct _time_checksum saved_checksum[MAX_LOG_IDS] = { 0 };
	bool do_update_crashenv, do_err_nodata;
	u32 new_crc32, new_cs;
	struct _time_checksum *chksum;
	bool crashreport, new_report;
	char *footer;
	size_t footsiz, footlen;
	char *buf = NULL;
	int buflen, bufsiz;
	unsigned int idx;
	int err, result;

	idx = array_index(id);
	footsiz = LOG_FOOTER_SIZE;
	footer	= kmalloc(footsiz, GFP_KERNEL);
	if (!footer)
		return -ENOMEM;
	footlen = 0;

	crashreport = report_type == READ_BY_CR;

	result = down_interruptible(&tffs_log_sema);
	if (result) {
		kfree(footer);
		return -ERESTARTSYS;
	}
	pr_devel("%s: id=%d idx=%x %s\n", __func__, id, idx, crashreport ? "cr" : "sd");

	if (tffs_log_read_forbidden(id, crashreport)) {
		result = -ENODATA;
		goto err_out;
	}

	err = tffs_read_entry(FLASH_FS_CRASH, crashenv, sizeof(crashenv));
	result = err < 0 ? err : 0;
	switch (result) {
	case 0:
		parse_crash_env(crashenv, &saved_checksum[0]);
		footlen += scnprintf(&footer[footlen], footsiz - footlen,
				"crashreport: read crash-variable '%s'\n", crashenv);
		break;
	case -EBUSY:
		goto err_out;
	case -ENOENT:
		footlen += scnprintf(&footer[footlen], footsiz - footlen,
				"crashreport: FLASH_FS_CRASH: variable not set\n");
		break;
	default:
		footlen += scnprintf(&footer[footlen], footsiz - footlen,
				"crashreport: FLASH_FS_CRASH: TFFS3_Read() -> err=%d\n", result);
	}

	bufsiz = log_max_size(id) + AVM_REBOOT_INFO_SIZE + sizeof(LOG_FOOTER_SEP) + footsiz;
	buf    = vmalloc(bufsiz);
	if (buf == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	bufsiz -= sizeof(LOG_FOOTER_SEP) + footsiz;
	buflen = tffs_read_log(buf, bufsiz, id);
	if (buflen == 0) {
		result = -ENODATA;
		goto read_done;
	}
	if (buflen < 0) {
		result = buflen;
		goto err_out;
	}

	chksum = &saved_checksum[idx];

	new_crc32 = calc_crc32(buf, buflen);

	replace_nulchars(buf, buflen);

	if (crc32_present(chksum)) {
		new_report = new_crc32 != chksum->crc32;

		/* Maintain old .cs in case of FW downgrades */
		if (new_report)
			new_cs = calc_sum(buf, buflen);
		else
			new_cs = chksum->cs;
	} else {
		new_cs	   = calc_sum(buf, buflen);
		new_report = !checksum_valid(chksum) || new_cs != chksum->cs;
	}

	footlen += scnprintf(
		&footer[footlen], footsiz - footlen,
		"crashreport: old parsed checksum[%u]: .cs=%08x .crc32=%08x; new: .cs=%08x .crc32=%08x (new_len=%u)\n",
		idx, chksum->cs, chksum->crc32, new_cs, new_crc32, buflen);

	pr_devel(
		"%s: id=%d: old parsed checksum[%u]: .cs=%08x .crc32=%08x .readmask=%u; new: .cs=%08x .crc32=%08x (new_len=%u)\n",
		__func__, id, idx, chksum->cs, chksum->crc32, chksum->readmask, new_cs,
		new_crc32, buflen);

	if (new_report) {
		do_err_nodata	   = false;
		do_update_crashenv = true;

		chksum->readmask = crashreport ? READ_BY_CR : (READ_BY_SD | FIRST_READ_BY_SD);
		chksum->ts	 = ktime_get_real_seconds();
		chksum->cs	 = new_cs;
	} else {
		do_err_nodata = (chksum->readmask & report_type) &&
				(log_read_multiple_allowed(report_type)) == false;

		do_update_crashenv = !crc32_present(chksum) ||
				     (chksum->readmask & report_type) == 0;

		chksum->readmask |= report_type;
	}

	if (do_update_crashenv) {
		chksum->crc32 = new_crc32;
		chksum->cs    = new_cs;
		crc32_enable(chksum);

		set_crash_env(crashenv, sizeof(crashenv), saved_checksum);

		err = tffs_write_entry(FLASH_FS_CRASH, crashenv, strlen(crashenv) + 1);
		if (err)
			footlen += scnprintf(
				&footer[footlen], footsiz - footlen,
				"crashreport: FLASH_FS_CRASH: TFFS3_Write() -> err=%d\n", err);
		else
			footlen += scnprintf(&footer[footlen], footsiz - footlen,
					     "crashreport: crash-variable set to '%s'\n", crashenv);
	} else {
		footlen += scnprintf(&footer[footlen], footsiz - footlen,
				     "crashreport: crash-variable left as '%s'\n", crashenv);
	}

	if (do_err_nodata) {
		result = -ENODATA;
		goto err_out;
	}

	footlen += scnprintf(&footer[footlen], footsiz - footlen, "(first) sent on: ");
	if (chksum->ts) {
		ssize_t tlen;

		tlen = time_to_ascii(chksum->ts, &footer[footlen], footsiz - footlen);
		if (tlen > 0) {
			footlen += tlen;
			strlcpy(&footer[footlen],
				(chksum->readmask & FIRST_READ_BY_SD) ? " by support data" :
									      " by crash report",
				footsiz - footlen);
		}
	}

	sprintf(&buf[buflen], "%s%s\n", LOG_FOOTER_SEP, footer);

	result = single_open(file, paniclog_show, buf);

read_done:
	/* Log per Crashreport ausgelesen - ab jetzt gilt für (folgende) crashende WD-Applikationen wieder die Auslesesperre: */
	if (crashreport)
		old_unreported_crash[idx] = false;
	if (id == FLASH_FS_ID_USER_LOG && crashreport)
		tffs_userlog_reserved_for_paniclog = false;
err_out:
	kfree(footer);
	if (result)
		vfree(buf);
	up(&tffs_log_sema);

	return result;
}

#define __generic_proc_log_open(name,  type, id)	\
static int __maybe_unused name(struct inode *inode, struct file *file)	\
{							\
	return __log_proc_open(inode, file, type, id);	\
}
  /*--- Crashreports ---*/
__generic_proc_log_open(paniclog_open_cr,  READ_BY_CR, FLASH_FS_ID_PANIC_LOG);
__generic_proc_log_open(crashlog_open_cr,  READ_BY_CR, FLASH_FS_ID_CRASH_LOG);
__generic_proc_log_open(userlog_open_cr,   READ_BY_CR, FLASH_FS_ID_USER_LOG);
  /*---  Supportdata ---*/
__generic_proc_log_open(paniclog_open_sd,  READ_BY_SD, FLASH_FS_ID_PANIC_LOG);
__generic_proc_log_open(crashlog_open_sd,  READ_BY_SD, FLASH_FS_ID_CRASH_LOG);
__generic_proc_log_open(userlog_open_sd,   READ_BY_SD, FLASH_FS_ID_USER_LOG);

  /*--- Crashreports ---*/
__generic_proc_log_open(panic2log_open_cr, READ_BY_CR, FLASH_FS_ID_PANIC2_LOG);
__generic_proc_log_open(crash2log_open_cr, READ_BY_CR, FLASH_FS_ID_CRASH2_LOG);
__generic_proc_log_open(user2log_open_cr,  READ_BY_CR, FLASH_FS_ID_USER2_LOG);
  /*---  Supportdata ---*/
__generic_proc_log_open(panic2log_open_sd, READ_BY_SD, FLASH_FS_ID_PANIC2_LOG);
__generic_proc_log_open(crash2log_open_sd, READ_BY_SD, FLASH_FS_ID_CRASH2_LOG);
__generic_proc_log_open(user2log_open_sd,  READ_BY_SD, FLASH_FS_ID_USER2_LOG);

/**
 */
static int __maybe_unused log_proc_release(struct inode *inode, struct file *file)
{
	if (!(file->f_flags & O_WRONLY)) {
		struct seq_file *seq = file->private_data;

		vfree(seq->private);
		seq->private = NULL;
		/*--- pr_info("%s: free after read\n", __func__); ---*/
		return single_release(inode, file); /*---  gibt txtbuf frei ----*/
	}
	/*--- pr_info("%s no free\n", __func__); ---*/
	return 0;
}

static size_t __userlog_buf_space(int head, int tail)
{
	return CIRC_SPACE(head, tail, USERLOG_BUFSIZE);
}

static size_t __userlog_buf_space_to_end(int head, int tail)
{
	return CIRC_SPACE_TO_END(head, tail, USERLOG_BUFSIZE);
}

static size_t __userlog_buf_cnt(int head, int tail)
{
	return CIRC_CNT(head, tail, USERLOG_BUFSIZE);
}

static size_t __userlog_buf_cnt_to_end(int head, int tail)
{
	return CIRC_CNT_TO_END(head, tail, USERLOG_BUFSIZE);
}

static void __userlog_buf_forward(int *cursor, size_t offset)
{
	/* Build-test buf size to be power of two */
	BUILD_BUG_ON(USERLOG_BUFSIZE & (USERLOG_BUFSIZE - 1));

	*cursor = (*cursor + offset) & (USERLOG_BUFSIZE - 1);
}

static size_t userlog_buf_space(struct circ_buf *cbuf)
{
	return __userlog_buf_space(cbuf->head, cbuf->tail);
}

static size_t userlog_buf_space_to_end(struct circ_buf *cbuf)
{
	return __userlog_buf_space_to_end(cbuf->head, cbuf->tail);
}

static size_t userlog_buf_cnt(struct circ_buf *cbuf)
{
	return __userlog_buf_cnt(cbuf->head, cbuf->tail);
}

/*
 * The user log data is stored in a ring buffer in memory, and saved to tffs
 * only on request, that is, if either a crashlog is written by userspace, or
 * the AVM watchdog triggers a reboot.  Since user log data is expected to be
 * large, the save operation discards data that precedes the last bootup.
 */
struct userlog_priv {
	struct circ_buf cbuf;
	char cbufdata[USERLOG_BUFSIZE];
	struct mutex lock;
	atomic_t file_busy;
};
static struct userlog_priv userlog_priv = {
	.file_busy = ATOMIC_INIT(0),
	.lock	   = __MUTEX_INITIALIZER(userlog_priv.lock),
};

static void userlog_clear_buffer(struct userlog_priv *priv)
{
	userlog_debug("clearing buffer\n");
	priv->cbuf.head = priv->cbuf.tail = 0;
}

static int __maybe_unused userlog_open_write(struct inode *ino, struct file *file)
{
	struct userlog_priv *priv = &userlog_priv;
	struct circ_buf *cbuf;
	int ret;

	if (!(file->f_flags & O_WRONLY))
		return -EPERM;

	if (atomic_xchg(&priv->file_busy, 1))
		return -EBUSY;

	ret = mutex_lock_killable(&priv->lock);
	if (ret)
		return ret;

	if (userlog_debug_enabled) {
		char fname[64];

		snprintf(fname, sizeof(fname), "%pD", file);
		userlog_debug("procfs %s open, %s\n", fname,
			      (file->f_flags & O_TRUNC) ? "truncating" : "appending");
	}

	cbuf	  = &priv->cbuf;
	cbuf->buf = priv->cbufdata;
	if (file->f_flags & O_TRUNC)
		userlog_clear_buffer(priv);

	mutex_unlock(&priv->lock);

	file->private_data = priv;

	return nonseekable_open(ino, file);
}

static void userlog_debug_show_buf(struct userlog_priv *priv)
{
	/* Make sure we don't consume too much stack */
	BUILD_BUG_ON(userlog_do_show_buf && USERLOG_BUFSIZE > 64);

#if HAVE_STRING_ESCAPEMEM
	if (userlog_debug_enabled && userlog_do_show_buf) {
		char escbuf[USERLOG_BUFSIZE * 2];
		size_t esclen;

		esclen = string_escape_mem(priv->cbuf.buf, USERLOG_BUFSIZE, escbuf, sizeof(escbuf),
					   ESCAPE_SPACE | ESCAPE_NP, NULL);
		esclen = min(sizeof(escbuf), esclen);
		userlog_debug("current buffer: \"%.*s\"\n", (int)esclen, escbuf);
	}
#endif
}

static void userlog_vacuum_buffer(struct userlog_priv *priv, size_t size_needed)
{
	struct circ_buf *cbuf = &priv->cbuf;
	char *cbufdata	      = priv->cbufdata, *nl;
	int min_pretail, pretail;
	size_t room, newroom, c;

	room = userlog_buf_space(cbuf);
	if (room >= size_needed)
		return;

	userlog_debug("%zu/%u bytes available, vacuuming buffer\n", room, USERLOG_BUFCAPACITY);
	userlog_debug_show_buf(priv);

	/* Make room by moving the tail, preferably after a newline. */

	newroom = size_needed - room;
	BUG_ON(newroom > userlog_buf_cnt(cbuf));

	/* Start searching for a newline at the preceding character */
	min_pretail = cbuf->tail;
	__userlog_buf_forward(&min_pretail, newroom - 1);
	pretail = min_pretail;
	while ((c = __userlog_buf_cnt_to_end(cbuf->head, pretail))) {
		nl = memchr(&cbufdata[pretail], '\n', c);
		if (nl) {
			pretail += nl - &cbufdata[pretail];
			newroom += __userlog_buf_cnt(pretail, min_pretail);
			break;
		}
		__userlog_buf_forward(&pretail, c);
	}

	__userlog_buf_forward(&cbuf->tail, newroom);

	userlog_debug("vacuuming discarded %zu bytes from buffer, head=%d tail=%d\n", newroom,
		      cbuf->head, cbuf->tail);
}

static ssize_t __maybe_unused userlog_write(struct file *file, const char __user *buffer,
					    size_t count, loff_t *offset)
{
	struct userlog_priv *priv = file->private_data;
	struct circ_buf *cbuf	  = &priv->cbuf;
	char *cbufdata		  = priv->cbufdata;
	size_t room, consumed, copied;
	int ret;

	if (count == 0)
		return 0;

	ret = mutex_lock_killable(&priv->lock);
	if (ret)
		return ret;

	userlog_debug("enter, head=%d tail=%d write=%zu\n", cbuf->head, cbuf->tail, count);

	if (count > USERLOG_BUFCAPACITY) {
		consumed = count - USERLOG_BUFCAPACITY;
		count	 = USERLOG_BUFCAPACITY;
		userlog_debug("discarding %zu excessive input bytes\n", consumed);
		userlog_clear_buffer(priv);
	} else {
		consumed = 0;
		userlog_vacuum_buffer(priv, count);
	}

	copied = 0;
	while (copied < count) {
		size_t c;

		/* Our buffer now has enough space for the remaining new data. */
		room = userlog_buf_space_to_end(cbuf);
		BUG_ON(room == 0);

		c   = min(room, count - copied);
		ret = copy_from_user(&cbufdata[cbuf->head], &buffer[consumed], c);
		if (ret) {
			ret = -EFAULT;
			goto err_unlock;
		}

		__userlog_buf_forward(&cbuf->head, c);
		consumed += c;
		copied += c;
	}

	*offset += consumed;
err_unlock:
	userlog_debug("leave, new head=%d tail=%d ret=%d\n", cbuf->head, cbuf->tail, ret);
	mutex_unlock(&priv->lock);

	return ret ?: consumed;
}

static int __maybe_unused userlog_release(struct inode *inode, struct file *file)
{
	struct userlog_priv *priv = file->private_data;

	atomic_set(&priv->file_busy, 0);

	return 0;
}

static int __userlog_save(struct tffs_fops_handle *tffs_userlog)
{
	struct userlog_priv *priv = &userlog_priv;
	struct circ_buf *cbuf	  = &priv->cbuf;
	int tail, ret;
	size_t count;
	loff_t off;

	mutex_lock(&priv->lock);
	ret = off = 0;
	tail	  = cbuf->tail;

	userlog_debug("head=%d, tail=%d cnt=%zu\n", cbuf->head, cbuf->tail, userlog_buf_cnt(cbuf));

	while ((count = __userlog_buf_cnt_to_end(cbuf->head, tail))) {
		ssize_t written;

		written = tffs_write_kern(tffs_userlog, &cbuf->buf[tail], count, &off);
		if (written < 0) {
			ret = written;
			break;
		}

		__userlog_buf_forward(&tail, written);
	}
	mutex_unlock(&priv->lock);

	userlog_debug("New head=%d tail=%d, result: %d\n", cbuf->head, cbuf->tail, ret);

	return ret;
}

/*
 * Save the userlog buffer in memory to tffs.  This wears down flash memory,
 * so we only do this if there are known user space problems.
 *
 * It would be nice to do this on kernel panics as well, but tffs only has
 * support for writing a single tffs "file", and that is reserved for the
 * kmsg log.  But, if user space is the source of problems, the kernel should
 * be unaffected, so we can write to tffs normally.
 */
static int userlog_save(void)
{
	int tffs_userlog_id = _FLASH_FS_ID_USER_LOG;
	struct tffs_fops_handle *tffs_userlog;
	void *buf = NULL;
	int ret, ret2;
	ssize_t size;
	loff_t off;

	if (!IS_ENABLED(CONFIG_TFFS_USER_LOG))
		return 0;

	pr_devel("Saving the userlog\n");

#if IS_ENABLED(CONFIG_TFFS_USERLOG_TMPBUF_PSTORE)
	size = pstore_copy(PSTORE_TYPE_PMSG, 0, &buf);
	if (size < 0)
		return size;
#endif

	tffs_userlog = tffs_open_kern(tffs_userlog_id, true /*writing*/);
	if (IS_ERR(tffs_userlog)) {
		ret = PTR_ERR(tffs_userlog);
		goto out;
	}

	if (IS_ENABLED(CONFIG_TFFS_USERLOG_TMPBUF_PSTORE)) {
		ssize_t written;

		if (size)
			written = tffs_write_kern(tffs_userlog, buf, size, &off);
		else
			written = 0;
		ret = written < 0 ? written : 0;
		kfree(buf);
	} else {
		ret = __userlog_save(tffs_userlog);
	}

	ret2 = tffs_release_kern(tffs_userlog);
	if (ret == 0)
		ret = ret2;
out:
	if (ret)
		pr_err("[TFFS] Error saving user log: %d\n", ret);

	return ret;
}

static int avm_watchdog_reboot_notifier(struct notifier_block *nb, unsigned long unused,
					void *hanging_task)
{
	if (hanging_task) {
		down(&tffs_log_sema);

		userlog_save();
		tffs_userlog_read_forbidden = true;

		up(&tffs_log_sema);
	}
	return NOTIFY_OK;
}

static struct notifier_block avm_watchdog_reboot_nb = {
	.notifier_call = avm_watchdog_reboot_notifier,
	.priority      = 255,
};

/**
 * Es handelt sich um einen neuen Logeintrag wenn:
 *
 * aus Crashvariable ermittelt:
 * (a) Timestamp == 0 (generell noch kein Aufruf des log_sd/.. /log_cr/..)
 * oder
 * (b) kein Aufruf von log_cr/.. (nur Crashreport ist hier relevant)
 * oder
 * zusätzlich per Logbuffer ermittelt:
 * (c) die Checksumme des Logs stimmt nicht mit Checksumme in der Crashvariable überein -> neuer Eintrag
 */
static bool have_new_logentry(int tffs_id)
{
	struct _time_checksum chksum[MAX_LOG_IDS] = { 0 };
	unsigned char *buf;
	bool have_new_log;
	size_t bufsiz;
	int idx;
	ssize_t len;

	idx = array_index(tffs_id);

	read_crash_var(chksum);
	pr_devel("%s: tffs_id=%u crc32=0x%x ts= %llu readmask=%x\n", __func__, tffs_id,
		 chksum[idx].crc32, chksum[idx].ts, chksum[idx].readmask);
	/* niemals ausgelesen (kein read-timestamp gesetzt): */
	if (!checksum_valid(&chksum[idx]))
		return true;
	/* niemals per log_cr ausgelesen: */
	if ((chksum[idx].readmask & READ_BY_CR) == 0)
		return true;

	bufsiz = log_max_size(tffs_id);
	buf    = vmalloc(bufsiz);
	if (!buf)
		return false;

	len = tffs_read_log(buf, bufsiz, tffs_id);

	if (len < 0) {
		pr_err("Error reading log data from tffs id %d: %zd\n", tffs_id, len);
		vfree(buf);
		return false;
	}

	have_new_log = !checksum_matches(&chksum[idx], buf, len);
	vfree(buf);
	pr_devel("TFFS log entry %d: %snew log entry\n", tffs_id, have_new_log ? "" : "no ");

	return have_new_log;
}

static bool tffs_is_empty(int tffs_id)
{
	char buf[8];

	return tffs_simple_kern_read(buf, sizeof(buf), tffs_id) == 0;
}

static int tffs_purge_userlog(void)
{
	int tffs_id = FLASH_FS_ID_USER_LOG;

	if (tffs_is_empty(tffs_id))
		return 0;

	pr_info("[%s] Purging userlog\n", __func__);

	return tffs_simple_kern_write(NULL, 0, tffs_id);
}

/**
 * Szenario: Usercrash einer an den Watchdog angemeldeten
 *  Applikation in der Bootphase:
 *-Versenden des Crash-Logs wird nun unterbunden um den Crash "kombiniert"
 * inklusive dem folgenden Wd-panic-Log zu verschicken.
 *-Nach dem erneuten Hochfahren passiert aber wieder ein Crash -> sperrt
 * also wieder das Auslesen. Somit wird im besten Fall (Internet-Up)
 * nur der panic-Log ohne Crashlog verschickt.
 * Um dies zu verhindern wird nun initial, also bevor überhaupt
 * eine Applikation crashen kann, geschaut, ob der letze Crash-Log
 * schon ausgelesen(versendet) wurde. Falls nicht, so wird hier
 * die Auslesesperre ausgeschaltet.
 * Wenn der Crashlog per Crashreport endlich ausgelesen wurde, kann
 * die Auslesesperre für evtl. nachfolgend crashende WD-Applikationen
 * wieder wirksam werden.
 */
static void crashlog_handle_bootup_after_panic(void)
{
	bool new_crash[2];

	if (IS_ENABLED(CONFIG_TFFS_REMOTE_CRASHLOG_READER))
		return;

	new_crash[0] = have_new_logentry(FLASH_FS_ID_CRASH_LOG);
	new_crash[1] = have_new_logentry(FLASH_FS_ID_CRASH2_LOG);

	if (new_crash[0] || new_crash[1])
		pr_devel("TFFS: perhaps outstanding%s%s\n",
			 new_crash[0] ? " crash" : "", new_crash[1] ? " crash2" : "");

	old_unreported_crash[array_index(FLASH_FS_ID_CRASH_LOG)]  = new_crash[0];
	old_unreported_crash[array_index(FLASH_FS_ID_CRASH2_LOG)] = new_crash[1];
}

/*
 * Check if tffs may have a user log for a preceding kernel panic during
 * "early" bootup, before userspace starts executing.
 *
 * Note that if PSTORE_PMSG is used for this and it is populated, it's only
 * later that userspace wil save it to tffs.  So at this stage, panic-related
 * content in the userlog must have been written by the dying kernel.
 */
static bool userlog_may_have_panic_content_early(void)
{
	return !IS_ENABLED(CONFIG_TFFS_USERLOG_TMPBUF_PSTORE) &&
	       avm_reset_status() == RS_SOFTWATCHDOG;
}

static void userlog_handle_bootup_after_panic(void)
{
	bool new_entry;

	if (!IS_ENABLED(CONFIG_TFFS_USER_LOG))
		return;
	if (!rebooting_after_panic())
		return;

	pr_info("Reboot after panic detected\n");

	down(&tffs_log_sema);
	new_entry = userlog_may_have_panic_content_early() &&
		    have_new_logentry(FLASH_FS_ID_USER_LOG);
	if (!new_entry)
		tffs_purge_userlog();
	tffs_userlog_reserved_for_paniclog = true;
	old_unreported_crash[array_index(FLASH_FS_ID_USER_LOG)] = new_entry;
	up(&tffs_log_sema);
}

static int tffs_ready_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	enum tffs3_module_state state = (enum tffs3_module_state)action;

	pr_devel("%s: tffs3_module_state=%d\n", __func__, state);
	if (state == tffs3_module_running) {
		userlog_handle_bootup_after_panic();
		crashlog_handle_bootup_after_panic();
	}
	return NOTIFY_OK;
}

static struct notifier_block tffs_notify_ready = {
	.notifier_call = tffs_ready_notifier,
};

struct _crashlog_write_priv {
	size_t index;
	char buf[0];
};

/**
 */
static int crashlog_open_write(struct inode *inode, struct file *file)
{
	struct _crashlog_write_priv *pclog_w;
	int result;

	if (!(file->f_flags & O_WRONLY))
		return -EPERM;

	result = down_interruptible(&tffs_log_sema);
	if (result)
		return -ERESTARTSYS;

	pclog_w = vmalloc(sizeof(struct _crashlog_write_priv) + CRASHLOG_BUFSIZE);
	if (pclog_w == NULL) {
		result = -ENOMEM;
		goto out;
	}
	pclog_w->index = 0;

	file->private_data = pclog_w;
out:
	up(&tffs_log_sema);
	return result ?: nonseekable_open(inode, file);
}

/**
 * skippe an Zeilengrenzen
 */
static void nice_skip(char *buf, size_t skip, size_t *end)
{
	char *nlchar;

	nlchar = memchr(&buf[skip], '\n', *end - skip);
	if (nlchar) {
		skip = nlchar - buf + 1;
		*end -= skip;
		memmove(buf, &buf[skip], *end);
	} else {
		*end = 0;
	}
}

/**
 */
static ssize_t crashlog_write(struct file *file, const char *buffer, size_t count, loff_t *offset)
{
	struct _crashlog_write_priv *pclog_w = file->private_data;
	unsigned int free_space, skip;

	if (count > CRASHLOG_BUFSIZE) {
		skip  = count - CRASHLOG_BUFSIZE;
		count = CRASHLOG_BUFSIZE;
		buffer += skip;
	}

	free_space = CRASHLOG_BUFSIZE - pclog_w->index;
	pr_devel("%s: index=%zu -> free_space=%u count=%zu\n", __func__, pclog_w->index, free_space,
		 count);

	if (count > free_space) {
		skip = count - free_space;
		nice_skip(pclog_w->buf, skip, &pclog_w->index);
	}

	if (copy_from_user(&pclog_w->buf[pclog_w->index], buffer, count))
		return -EFAULT;

	pclog_w->index += count;

	return count;
}

static size_t format_crashlog_header(char *str, size_t size);

/**
 */
static int crashlog_close_write(struct inode *inode, struct file *file)
{
	struct _crashlog_write_priv *pclog_w		= file->private_data;
	struct _time_checksum saved_chksum[MAX_LOG_IDS] = { 0 };
	struct tffs_fops_handle *handle;
	struct _time_checksum *chksum;
	int id = _FLASH_FS_ID_CRASH_LOG;
	unsigned int idx;
	ssize_t len, written;
	bool appending;
	char *oldlog;
	int result;
	loff_t off;

	pr_devel("%s: idx=%zu\n", __func__, pclog_w->index);

	down(&tffs_log_sema);

	result = avm_remote_crashlog_sem_down();
	if (result)
		goto no_remote_lock;

	appending = false;

	oldlog = vmalloc(CRASHLOG_BUFSIZE);
	if (!oldlog)
		goto overwrite;

	len = tffs_simple_kern_read(oldlog, CRASHLOG_BUFSIZE, id);
	if (len <= 0)
		goto overwrite;

	read_crash_var(saved_chksum);
	idx = array_index(_FLASH_FS_ID_CRASH_LOG);
	chksum	= &saved_chksum[idx];

	/*
	 * Note that we cannot safely modify the crashenv variable here,
	 * because on systems where !system_is_primary_FritzOS, the
	 * variable is modified by the remote system, and there is no locking
	 * that coordinates accesses by the remote and the local system.
	 */
	appending = !checksum_valid(chksum) || (chksum->readmask & READ_BY_CR) == 0 ||
		    !checksum_matches(chksum, oldlog, len);
overwrite:
	handle = tffs_open_kern(id, 1 /*write*/);
	if (IS_ERR(handle)) {
		result = PTR_ERR(handle);
		goto out;
	}

	result = -EIO;
	if (appending) {
		int discard;

		discard = len + pclog_w->index - CRASHLOG_BUFSIZE;
		if (discard > 0)
			len -= discard;
		else
			discard = 0;

		written = tffs_write_kern(handle, &oldlog[discard], len, &off);
		if (written != len) {
			tffs_release_kern(handle);
			goto out;
		}
	} else {
		char header[AVM_FIRMWARE_INFO_SIZE];

		len	= format_crashlog_header(header, sizeof(header));
		written = tffs_write_kern(handle, header, len, &off);
		if (written != len) {
			tffs_release_kern(handle);
			goto out;
		}
	}

	len	= pclog_w->index;
	written = tffs_write_kern(handle, pclog_w->buf, len, &off);
	tffs_release_kern(handle);
	if (written != len)
		goto out;

	result = 0;

	AVM_WATCHDOG_Crashlog_notify();

	/* Supplement the crash log with user log data */
	if (!tffs_userlog_reserved_for_paniclog)
		userlog_save();
	else
		pr_devel("Not saving userlog entry as this would clobber panic-related log data\n");

out:
	vfree(oldlog);
	vfree(pclog_w);

	avm_remote_crashlog_sem_up();

no_remote_lock:
	up(&tffs_log_sema);

	if (result)
		pr_err("%s: Error saving crashlog data: %d\n", __func__, result);

	return result;
}

#define __LOG_FOPS(_open, _write, _read, _llseek, _release) {	\
	.proc_open = _open,					\
	.proc_write = _write,					\
	.proc_read  = _read,					\
	.proc_lseek = _llseek,					\
	.proc_release = _release,				\
}

#define LOG_FOPS_RO(_open) \
	__LOG_FOPS(_open, NULL, seq_read, seq_lseek, log_proc_release)

/* The open function must call nonseekable_open, otherwise the NULL
 * llseek causes crashes on Linux >= 5.13, < 6.0.
 */
#define LOG_FOPS_WO(_open, _write, _release) \
	__LOG_FOPS(_open, _write, NULL, NULL, _release)

/**
 */
static struct _logproc {
	const char *dir;
	const char *name;
	const unsigned int mode;
	const struct proc_ops ops;
	struct proc_dir_entry *dir_entry;
} logproc[] = {
#if !IS_ENABLED(CONFIG_TFFS_USERLOG_TMPBUF_PSTORE) && IS_ENABLED(CONFIG_TFFS_USER_LOG)
	{ .dir =  NULL,         .name = "avm/user.log",  .mode = 0222, .ops = LOG_FOPS_WO(userlog_open_write, userlog_write, userlog_release), },
#endif
	{ .dir =  NULL,         .name = "avm/crash.log", .mode = 0222, .ops = LOG_FOPS_WO(crashlog_open_write, crashlog_write, crashlog_close_write), },
#if !IS_ENABLED(CONFIG_TFFS_REMOTE_CRASHLOG_READER)
	{ .dir =  "avm/log_cr", .name = "panic",	 .mode = 0444, .ops = LOG_FOPS_RO(paniclog_open_cr), },
	{ .dir =  "avm/log_cr", .name = "crash",	 .mode = 0444, .ops = LOG_FOPS_RO(crashlog_open_cr), },
	{ .dir =  "avm/log_cr", .name = "user",          .mode = 0444, .ops = LOG_FOPS_RO(userlog_open_cr), },
	{ .dir =  "avm/log_cr", .name = "user2",         .mode = 0444, .ops = LOG_FOPS_RO(user2log_open_cr), },
	{ .dir =  "avm/log_cr", .name = "panic2",	 .mode = 0444, .ops = LOG_FOPS_RO(panic2log_open_cr), },
	{ .dir =  "avm/log_cr", .name = "crash2",	 .mode = 0444, .ops = LOG_FOPS_RO(crash2log_open_cr), },
	{ .dir =  "avm/log_sd", .name = "panic",	 .mode = 0444, .ops = LOG_FOPS_RO(paniclog_open_sd), },
	{ .dir =  "avm/log_sd", .name = "crash",	 .mode = 0444, .ops = LOG_FOPS_RO(crashlog_open_sd), },
	{ .dir =  "avm/log_sd", .name = "panic2",	 .mode = 0444, .ops = LOG_FOPS_RO(panic2log_open_sd), },
	{ .dir =  "avm/log_sd", .name = "crash2",	 .mode = 0444, .ops = LOG_FOPS_RO(crash2log_open_sd), },
	{ .dir =  "avm/log_sd", .name = "user",	         .mode = 0444, .ops = LOG_FOPS_RO(userlog_open_sd), },
	{ .dir =  "avm/log_sd", .name = "user2",         .mode = 0444, .ops = LOG_FOPS_RO(user2log_open_sd), },
#endif
};

static int __init tffs_panic_init_early(void)
{
	return blocking_notifier_chain_register(&tffs_state_notifier, &tffs_notify_ready);
}
device_initcall(tffs_panic_init_early);

/**
 */
static int __init prepare_log_proc(void)
{
	struct proc_dir_entry *dir_entry = NULL;
	const char *lastdir		 = "";
	unsigned int i;

	/*--- pr_info("%s\n", __func__); ---*/

	/* FIXME: No support for user2 yet! */
	if (!IS_ENABLED(CONFIG_TFFS_USERLOG_TMPBUF_PSTORE))
		AVM_WATCHDOG_register_reboot_notifier(&avm_watchdog_reboot_nb);

	for (i = 0; i < ARRAY_SIZE(logproc); i++) {
		if (logproc[i].dir && strcmp(lastdir, logproc[i].dir)) {
			dir_entry = proc_mkdir(logproc[i].dir, NULL);
			lastdir	  = logproc[i].dir;
		}

		if (logproc[i].dir == NULL) {
			if (!proc_create(logproc[i].name, logproc[i].mode, NULL,
					 &logproc[i].ops)) {
				pr_err("%s can't proc_create(%s)\n", __func__, logproc[i].name);
			}
		} else if (dir_entry) {
			logproc[i].dir_entry = dir_entry;
			if (!proc_create(logproc[i].name, logproc[i].mode, dir_entry,
					 &logproc[i].ops)) {
				pr_err("%s can't proc_create(%s)\n", __func__, logproc[i].name);
			}
		}
	}
	return 0;
}
late_initcall(prepare_log_proc);

/**
 */
static void print_to_console(const char *str, int len)
{
	char txt[512 + 1];

	while (len) {
		int i, tail = min(len, (int)(sizeof(txt) - 1));

		/*--- Zeilenweise kopieren - sonst unschoen formatiert ---*/
		for (i = 0; i < tail; i++) {
			if (str[i] == '\n') {
				tail = i + 1;
				break;
			}
		}
		memcpy(txt, str, tail);
		str += tail, len -= tail;
		txt[tail] = 0;
		pr_cont("%s", txt);
		AVM_WATCHDOG_emergency_retrigger();
	}
}

#define CRASH_LOG_VERSION AVM_FWINFO_VERSION(2, 1)

static size_t format_paniclog_header(char *str, size_t size, bool add_times)
{
	struct avm_reboot_info info = { 0 };
	char *s;

	if (!size)
		return 0;

	if (add_times)
		avm_get_firmware_uptime(&info.tm);

	avm_get_firmware_info(&info.fw, CRASH_LOG_VERSION);

	s = str;
	s += avm_snprint_reboot_info(&info, str, size);
	s += scnprintf(s, size, "\n");

	return s - str;
}

static size_t format_crashlog_header(char *str, size_t size)
{
	struct avm_firmware_info fwinfo;
	char *s, *end;

	avm_get_firmware_info(&fwinfo, CRASH_LOG_VERSION);

	s   = str;
	end = str + size;

	s += scnprintf(s, end - s, "Uptime: %llu\n", ktime_get_seconds());
	s += avm_snprint_firmware_info(&fwinfo, s, end - s);
	s += scnprintf(s, end - s, "\n");

	return s - str;
}

/*--- Entwickler wollen alles auch noch auf der Konsole sehen ---*/
static void tffs_panic_log_print_to_console(const char *logbuf, size_t buflen)
{
	unsigned int log_symbols = 2 /* Sekunden */ * 115000 / 10;
	unsigned int offset	 = 0;

	pr_alert("\n\n------------------- Last part of Panic-log Content: -------------------\n\n");
	if (buflen > log_symbols) {
		offset = buflen - log_symbols;
		buflen = log_symbols;
	}

	print_to_console(&logbuf[offset], buflen);
}

static void tffs_panic_log_printkbuf(const char *buf, size_t buflen)
{
	char header[AVM_REBOOT_INFO_SIZE];
	size_t headlen;

	headlen = format_paniclog_header(header, sizeof(header), true);

	tffs_panic_log_open();
	tffs_panic_log_write(header, headlen);
	if (buflen > 0)
		tffs_panic_log_write(buf, buflen);
	tffs_panic_log_close();

	tffs_panic_log_print_to_console(buf, buflen);
}

static bool kmsg_dump_reason_is_panic(enum kmsg_dump_reason reason)
{
	switch (reason) {
/* 5.8 and above collapse these three cases into one */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	case KMSG_DUMP_RESTART:
	case KMSG_DUMP_HALT:
	case KMSG_DUMP_POWEROFF:
#else
	case KMSG_DUMP_SHUTDOWN:
#endif
		/* These are almost always orderly shutdowns. */
		return false;
	case KMSG_DUMP_OOPS:
#if defined(CONFIG_AVM_FASTIRQ) && defined(CONFIG_ARCH_QCOM)
		return false; // We want to split OOPS and PANIC
#endif
	case KMSG_DUMP_EMERG:
	case KMSG_DUMP_PANIC:
		return true;
	default:
		pr_err("%s: ignoring unrecognized KMSG_DUMP_* reason %d\n", __func__, (int)reason);
		return false;
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
static void kmsg_dump_get_buffer_compat(struct kmsg_dumper *dumper, char *buf,
					size_t buf_size, size_t *len_out)
{
	struct kmsg_dump_iter iter;

	kmsg_dump_rewind(&iter);
	kmsg_dump_get_buffer(&iter, false, buf, buf_size, len_out);
}
#else
static void kmsg_dump_get_buffer_compat(struct kmsg_dumper *dumper, char *buf,
					size_t buf_size, size_t *len_out)
{
	kmsg_dump_get_buffer(dumper, false, buf, buf_size, len_out);
}
#endif

static __maybe_unused void panic_log(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason)
{
	static char buf[PANICLOG_MAX_SIZE];
	static bool panicking;
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;
	size_t len = 0;

	if (panicking) /* Panic report already captured. */
		return;

	panicking = kmsg_dump_reason_is_panic(reason);
	if (!panicking)
		return;

	if (!spin_trylock_irqsave(&lock, flags))
		return;

	if (!tffs_panic_log_suppress) {
		tffs_panic_log_suppress = 1;
		AVM_WATCHDOG_emergency_retrigger();

		kmsg_dump_get_buffer_compat(dumper, buf, sizeof(buf), &len);
		tffs_panic_log_printkbuf(buf, len);
	}

	spin_unlock_irqrestore(&lock, flags);
}

#ifdef CONFIG_AVM_PLATFORM_RESET_STATUS
void tffs_write_pltfm_paniclog(const char *message)
{
	unsigned char uuid[16];
	size_t len;

	len = format_paniclog_header(platform_panic_log, sizeof(platform_panic_log), false);

	/* Use uuid to distinguish new log from previous, so that the crashreporter sees it */
	generate_random_uuid(uuid);
	snprintf(&platform_panic_log[len], sizeof(platform_panic_log) - len,
		 "%s\n%15ph\n", message, uuid);
}
#endif

#if !defined(CONFIG_PSTORE_RAM)
/**
 */
static struct kmsg_dumper panic_log_dumper = { .dump = panic_log };

/**
 */
static int __init install_panic_log_dumper(void)
{
	/*--- pr_info("%s\n", __func__); ---*/
	return kmsg_dump_register(&panic_log_dumper);
}

late_initcall(install_panic_log_dumper);
#endif
