/**
 *
 *   Copyright (C) 2007 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/io.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <avm/tffs/tffs.h>
#include <linux/zlib.h>
#include <linux/vmalloc.h>
#include <linux/rtc.h>
#include <linux/env.h>
#include <linux/kmsg_dump.h>
#include <linux/random.h>
#if defined(CONFIG_PROC_FS)
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uuid.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include "tffs_local.h"
#if __has_include(<avm/watchdog/watchdog.h>)
#include <avm/watchdog/watchdog.h>
#else
#include <linux/ar7wdt.h>
#endif
#include <asm/mach_avm.h>
#include <avm/enh/fw_info.h>

#ifdef TFFS_ARM_PUMA7
#  define CRASHPANIC_LOG_PER_REMOTE_EVENT_SOURCE
#endif

#if defined(CONFIG_MACH_AR934x)
extern int ath_restore_log_from_ram(char *txtbuf, unsigned int txtlen);
extern void ath_inval_log_from_ram(void);
#endif/*--- #if defined(CONFIG_MACH_AR934x) ---*/

static struct tffs_fops_handle *panic_handle;
unsigned int tffs_spi_mode;

unsigned int tffs_panic_log_suppress;
EXPORT_SYMBOL(tffs_panic_log_suppress);

/* minimal panic-log: only for atheros without correct nmi because freezer occur */
static char panic_dummy_log[512];
static int tffs_read_dummy_log(char *buf, int buf_len, unsigned int id);

static DEFINE_SEMAPHORE(log_sema);

/**
 * Schreiben im Panic-Kontext
 */
void tffs_panic_log_open(void)
{
	if (panic_handle == NULL) {
		panic_handle = tffs_open_panic();
	}
}
EXPORT_SYMBOL(tffs_panic_log_open);

/**
 * Schreiben im Panic-Kontext
 */
void tffs_panic_log_write(const char *buffer, unsigned int len)
{
	static loff_t off __attribute__ ((unused));

	if (panic_handle != NULL) {
		tffs_write_kern(panic_handle, buffer, len, &off);
	}
}
EXPORT_SYMBOL(tffs_panic_log_write);

/**
 * Schreiben im Panic-Kontext
 */
void tffs_panic_log_close(void)
{
	if (panic_handle != NULL) {
		tffs_release_kern(panic_handle);
		panic_handle = NULL;
	}
}
EXPORT_SYMBOL(tffs_panic_log_close);

/**
 */
static unsigned int get_act_time(void)
{
	struct timeval now;

	do_gettimeofday(&now);
	return (unsigned int)(now.tv_sec);
}

/**
 */
static char *time_to_ascii(unsigned long local_time, char *buf, int len, char *prefix)
{
	static const char * const month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char * const day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	struct rtc_time tm;

	rtc_time_to_tm(local_time, &tm);
	snprintf(buf, len, "%s %s %02d %02d:%02d:%02d %02d UTC %s",
		 day[tm.tm_wday], month[tm.tm_mon],
		 tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900, prefix);
	return buf;
}

/**
 * maxsize of buf: 14 + 10 + 2 + 2 + 2  = 30
 */
static char *human_readable_time(char *buf, unsigned long buf_size, unsigned long time)
{
	unsigned long seconds, minutes, hours, days;

	seconds = time % 60;
	time /= 60;
	minutes = time % 60;
	time /= 60;
	hours = time % 24;
	time /= 24;
	days = time;
	snprintf(buf, buf_size, "%lu d %lu h %lu min %lu s", days, hours, minutes, seconds);
	return buf;
}

/**
 */
void tffs_panic_log_register_spi(void)
{
	tffs_spi_mode = 1;
}

/**
 */
static unsigned int calc_sum_and_correct(unsigned char *buf, int buf_len, int correct)
{
	unsigned int i, sum = 0;

	for (i = 0; i < buf_len; i++) {
		if (correct && (buf[i] == 0)) {
			buf[i] = ' ';	/*--- correct buffer (support null-teminated string) ---*/
		}

		sum += buf[i];
	}

	return sum ^ (buf_len << 16);
}

#define MAX_LOG_IDS      4
/**
 */
static int map_log_id_to_index(unsigned int id)
{
	switch (id) {
	case FLASH_FS_ID_PANIC_LOG:
		return 0x0;
	case FLASH_FS_ID_PANIC2_LOG:
		return 0x1;
	case FLASH_FS_ID_CRASH_LOG:
		return 0x2;
	case FLASH_FS_ID_CRASH2_LOG:
		return 0x3;
	}
	return MAX_LOG_IDS;
}

static unsigned int log_read_multiple[MAX_LOG_IDS];

/**
 */
static ssize_t tffs_panic_log_read(unsigned char *buf, int buf_len, unsigned int id)
{
	struct tffs_fops_handle *handle;
	loff_t offp = 0;
	ssize_t read;

	handle = tffs_open_kern(id, 0);
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s tffs_open_kern failed\n", __func__);
		return -EINVAL;
	}

	read = tffs_read_kern(handle, buf, buf_len, &offp);
	/*--- pr_info("%s tffs_read ret=%d\n", __func__, ret); ---*/
	tffs_release_kern(handle);

	return read;
}

/**
 */
static int paniclog_show(struct seq_file *seq, void *data __attribute__ ((unused)))
{
	/*--- pr_info("%s %p %p\n", __func__, seq->private, data); ---*/
	if (seq->private) {
		seq_printf(seq, "%s", (char *)seq->private);
	}
	return 0;
}

/**
 */
struct _time_checksum {
	unsigned int cs;
	unsigned int ts;	/* timestamp on read */
#define READ_BY_CR          0x1
#define READ_BY_SD          0x2
#define FIRST_READ_BY_SD    0x4	/* first read by SD */
	unsigned int readmask;
};

#define SKIP_UNTIL_CHAR(p, sign) { while (*p && (*p != sign)) p++; }
/**
 */
static inline unsigned int COUNT_UNTIL_CHAR(const char *p, unsigned char sign, unsigned char count_sign)
{
	unsigned int count = 0;

	while (*p && (*p != sign)) {
		if (*p == count_sign) {
			count++;
		}
		p++;
	}
	return count;
}

/**
 * return: gefuelltes checksum
 * [id]cs,ts,readmask,[id2]cs,ts,readmask
 */
void parse_crash_env(const char *crashp, struct _time_checksum checksum[])
{
	memset(checksum, 0, MAX_LOG_IDS * sizeof(struct _time_checksum));
	while (*crashp) {
		unsigned int id, val, val1, val2;

		if (*crashp == '[') {
			sscanf(++crashp, "%x]%x,%x,%x", &id, &val, &val1, &val2);
			if (id < MAX_LOG_IDS) {
				unsigned int count = COUNT_UNTIL_CHAR(crashp, '[', ',');

				checksum[id].cs = val;
				checksum[id].ts = val1;
				if (count >= 1) {
					checksum[id].ts = val1;
				}
				if (count >= 2) {
					checksum[id].readmask = val2;
				}
				/*--- pr_info("%s: %d(count=%d): %x %x %x\n", __func__, id, count, checksum[id].cs, checksum[id].ts,checksum[id].readmask); ---*/
			}
			SKIP_UNTIL_CHAR(crashp, '[');
		} else {
			return;
		}
	}
}

/**
 */
static void set_crash_env(char *crashp, int len, struct _time_checksum checksum[])
{
	unsigned int written, i;

	for (i = 0; i < MAX_LOG_IDS; i++) {
		if (len > 0) {
			written = snprintf(crashp, len, "[%x]%x,%x,%x", i, checksum[i].cs,
					   checksum[i].ts, checksum[i].readmask);
			len -= written, crashp += written;
		}
	}
}

#define snprintf_add(ptxt, txtlen, args...)				\
	{								\
		int local_add_len;					\
		local_add_len = snprintf(ptxt, txtlen, args);		\
		if (local_add_len > 0) {				\
			int tail = min_t(int, txtlen, local_add_len);	\
			(ptxt) += tail, (txtlen) -= tail;		\
		}							\
	}
/**
 * report_type: READ_BY_SD      Support-Data
 *              READ_BY_CR      CrashReport
 */
static int __log_proc_open(struct inode *inode __attribute__ ((unused)), struct file *file,
			   unsigned int report_type, int id)
{
	char crashenv[128];
	char log[386], *plog;
	int log_len;
	char timebuf[128];
	const char *timestart = "";
	/* space for kernel log + a bit extra for panic log header and timestamp footer */
	int len = (0x1 << CONFIG_LOG_BUF_SHIFT) + 1024;
	int buf_len = len - sizeof(timebuf) - sizeof(log); /*--- ReserveTimestamp ---*/
	struct _time_checksum saved_checksum[MAX_LOG_IDS];
	unsigned int new_read_mask = 0;
	unsigned int map_idx = map_log_id_to_index(id);
	char *buf = NULL;
	struct tffs_core_handle *handle;
	unsigned int crashreport = (report_type == READ_BY_CR) ? 1 : 0;
	int result;

	if (down_interruptible(&log_sema) < 0) {
		/* exit bei signal */
		return 0;
	}
	/*--- pr_err("%s: id=%d map_idx=%x %s\n", __func__, id, map_idx, crashreport ? "cr" : "sd"); ---*/
	if (map_idx >= MAX_LOG_IDS) {
		/*--- pr_err("%s: error: id=%d too big\n", __func__, map_idx); ---*/
		up(&log_sema);
		return -ENODATA;
	}
	/*--- pr_err("%s: id=%d map_idx=%x %s f:_flags=%x\n", __func__, id, map_idx, crashreport ? "cr" : "sd", file->f_flags); ---*/
	if (file->f_flags & O_WRONLY) {
		unsigned int mapvalue;

		mapvalue = report_type;
		mapvalue |= map_idx << 8;
		file->private_data = (void *)mapvalue;
		up(&log_sema);
		return 0;
	}

	if ((id == FLASH_FS_ID_CRASH_LOG) ||
	    (id == FLASH_FS_ID_CRASH2_LOG)) {
		if (AVM_WATCHDOG_Crashlog_read_forbidden()) {
			up(&log_sema);
			return -ENODATA;
		}
	}
	memset(saved_checksum, 0, sizeof(saved_checksum));
	buf = vmalloc(len);
	if (buf == NULL) {
		result = -ENOMEM;
		goto err_out;
	}
	len = sizeof(crashenv);
	crashenv[0] = 0;
	handle = TFFS3_Open(FLASH_FS_CRASH, tffs3_mode_read);
	if (IS_ERR_OR_NULL(handle)) {
		result = -ENODEV;
		goto err_out;
	}

	log[0] = 0;
	plog = log;
	log_len = sizeof(log);
	result = TFFS3_Read(handle, crashenv, &len);
	TFFS3_Close(handle);
	if (result == 0) {
		parse_crash_env(crashenv, &saved_checksum[0]);
		snprintf_add(plog, log_len, "crashreport: read crash-variable '%s'\n", crashenv);
	} else {
		snprintf_add(plog, log_len, "crashreport: FLASH_FS_CRASH: TFFS3_Read() -> result=%d\n", result);
	}
	if (result == -EBUSY) {
		goto err_out;
	}
	buf[0] = 0;
	/*--- nmi-wa-check: ---*/
	len = tffs_read_dummy_log(buf, buf_len, id);
	if (len <= 0) {
		len = tffs_panic_log_read(buf, buf_len, id);
	}
	if (len > 0) {
		unsigned int new_sum = calc_sum_and_correct(buf, len, 1);

		/*--- pr_info("%s: sum=%x len=%d\n", __func__, new_sum, len); ---*/
#if 1
		snprintf_add(plog, log_len, "crashreport: old parsed checksum[%u].cs = %08x new_sum=%08x (new_len=%u)\n",
			     map_idx, saved_checksum[map_idx].cs, new_sum, len);
#endif
		if (new_sum != saved_checksum[map_idx].cs) {
			/*--- new panic-log: set struct ---*/
			new_read_mask = crashreport ? READ_BY_CR : (READ_BY_SD | FIRST_READ_BY_SD);
			saved_checksum[map_idx].readmask = 0;
			saved_checksum[map_idx].ts = get_act_time();
			saved_checksum[map_idx].cs = new_sum;
		} else {
			new_read_mask = crashreport ? READ_BY_CR : READ_BY_SD;
		}
	} else {
		len = 0;
	}
#if defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA953x)
	if (id == FLASH_FS_ID_PANIC_LOG) {
		unsigned int sram_len = ath_restore_log_from_ram(buf, buf_len);

		if (sram_len) {
			unsigned int new_sram_sum;

			map_idx = 1;
			/*--- ... and prefer sram-log---*/
			len = sram_len;
			new_sram_sum = calc_sum_and_correct(buf, len, 1);
			/*--- pr_info("%s: prefer sram-trace len=%d", __func__, sram_len); ---*/
			if (new_sram_sum != saved_checksum[map_idx].cs) {
				struct timeval now;

				/*--- new sram-panic-log: clear counter ---*/
				new_read_mask = crashreport ? READ_BY_CR : (READ_BY_SD | FIRST_READ_BY_SD);
				saved_checksum[map_idx].readmask = 0;
				saved_checksum[map_idx].ts = get_act_time();
				saved_checksum[map_idx].cs = new_sram_sum;
			} else {
				new_read_mask = crashreport ? READ_BY_CR : READ_BY_SD;
			}
		}
	}
#endif/*--- #if defined(CONFIG_MACH_AR934x) ---*/

	if ((len == 0)
	    || (((saved_checksum[map_idx].readmask & new_read_mask) == new_read_mask)
		&& !((log_read_multiple[map_idx] & new_read_mask) == new_read_mask))) {
		/*--- check if sent ---*/
		result = -ENODATA;
		goto err_out;
	}

	saved_checksum[map_idx].readmask |= new_read_mask;
	if (saved_checksum[map_idx].ts) {
		timestart = time_to_ascii(saved_checksum[map_idx].ts, timebuf, sizeof(timebuf),
					  (saved_checksum[map_idx].readmask & FIRST_READ_BY_SD) ? "by support data" : "by crash report");
	}
	set_crash_env(crashenv, sizeof(crashenv), saved_checksum);
/*---   pr_info("%s: '%s'\n", __func__, crashenv); ---*/

	handle = TFFS3_Open(FLASH_FS_CRASH, tffs3_mode_write);
	if (IS_ERR_OR_NULL(handle)) {
		result = -ENODEV;
		goto err_out;
	}
	result = TFFS3_Write(handle, crashenv, strlen(crashenv) + 1, 1);
	TFFS3_Close(handle);
	if (result < 0) {
		snprintf_add(plog, log_len, "crashreport: FLASH_FS_CRASH: TFFS3_Write() -> result=%d\n", result);
	} else {
		snprintf_add(plog, log_len, "crashreport: crash-variable set to '%s'\n", crashenv);
	}
	sprintf(&buf[len], "-----\n%s(first) sent on: %s\n", log, timestart);
	result = single_open(file, paniclog_show, buf);
	up(&log_sema);
	return result;
err_out:
	if (buf != NULL) {
		vfree(buf);
	}
	up(&log_sema);
	return result;

}

#define __generic_proc_log_open(name,  type, id)  static int name(struct inode *inode, struct file *file) {  return __log_proc_open(inode, file, type, id); }
  /*--- Crashreports ---*/
__generic_proc_log_open(paniclog_open_cr,  READ_BY_CR, FLASH_FS_ID_PANIC_LOG);
__generic_proc_log_open(crashlog_open_cr,  READ_BY_CR, FLASH_FS_ID_CRASH_LOG);
  /*---  Supportdata ---*/
__generic_proc_log_open(paniclog_open_sd,  READ_BY_SD, FLASH_FS_ID_PANIC_LOG);
__generic_proc_log_open(crashlog_open_sd,  READ_BY_SD, FLASH_FS_ID_CRASH_LOG);

  /*--- Crashreports ---*/
__generic_proc_log_open(panic2log_open_cr, READ_BY_CR, FLASH_FS_ID_PANIC2_LOG);
__generic_proc_log_open(crash2log_open_cr, READ_BY_CR, FLASH_FS_ID_CRASH2_LOG);
  /*---  Supportdata ---*/
__generic_proc_log_open(panic2log_open_sd, READ_BY_SD, FLASH_FS_ID_PANIC2_LOG);
__generic_proc_log_open(crash2log_open_sd, READ_BY_SD, FLASH_FS_ID_CRASH2_LOG);

/**
 */
static ssize_t log_proc_write(struct file *file, const char *buffer, size_t count, loff_t *offset __maybe_unused)
{
	unsigned char procfs_buffer[64];
	size_t procfs_buffer_size;
	unsigned int id, report;

	/*--- von wo kommen wir: ---*/
	id	   = ((unsigned int) file->private_data) >> 8;
	report = ((unsigned int)file->private_data)  & 0xFF;
	if (id >= MAX_LOG_IDS) {
		return count;
	}
	if (count >= sizeof(procfs_buffer)) {
		procfs_buffer_size = sizeof(procfs_buffer) - 1;
	} else {
		procfs_buffer_size = count;
	}
	/* write data to the buffer */
	if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size))
		return -EFAULT;

	procfs_buffer[procfs_buffer_size] = 0;
	if (strstr(procfs_buffer, "readmultiple")) {
		log_read_multiple[id] |= report;
	} else if (strstr(procfs_buffer, "readonce")) {
		log_read_multiple[id] &= ~report;
#if defined(avm_rebootcounter_string)
	} else if (strstr(procfs_buffer, "clear_rebootcounter")) {
		avm_rebootcounter_reset();
#endif/*--- #if defined(avm_rebootcounter_string) ---*/
	} else {
		pr_info("unknown option: use readmultiple, readonce"
#if defined(avm_rebootcounter_string)
		       ", clear_rebootcounter"
#endif/*--- #if defined(avm_rebootcounter_string) ---*/
		       "\n");
	}
	/*--- pr_info("%s: '%s' %p log_read_multiple[%d]=%x (report=%x)\n", __func__, procfs_buffer, file->private_data, id, log_read_multiple[id], report); ---*/
	return count;
}

/**
 */
static int log_proc_release(struct inode *inode, struct file *file)
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

#if defined(CRASHPANIC_LOG_PER_REMOTE_EVENT_SOURCE)
#  define _FLASH_FS_ID_CRASH_LOG FLASH_FS_ID_CRASH2_LOG
#else
#  define _FLASH_FS_ID_CRASH_LOG FLASH_FS_ID_CRASH_LOG
#endif

#define CRASHLOG_BUFSIZE        (1 << CONFIG_LOG_BUF_SHIFT)
struct _crashlog_write_priv {
	char crashenv[128];
	unsigned int do_write_crashenv;
	unsigned int id;
	unsigned int buf_size;
	unsigned int index;
	char *buf;
};

/**
 */
static int crashlog_open_write(struct inode *inode __attribute__ ((unused)), struct file *file)
{
	struct _crashlog_write_priv *pclog_w;
	struct tffs_core_handle *handle = NULL;
	struct _time_checksum saved_checksum[MAX_LOG_IDS];
	int result = 0, len;
	unsigned int map_idx;

	if (!(file->f_flags & O_WRONLY)) {
		return -EPERM;
	}
	if (down_interruptible(&log_sema) < 0) {
		/* exit bei signal */
		return 0;
	}
	pclog_w = vmalloc(sizeof(struct _crashlog_write_priv) + CRASHLOG_BUFSIZE);
	if (pclog_w == NULL) {
		result = -ENOMEM;
		goto err_out;
	}
	pclog_w->buf = (char *)(pclog_w + 1);
	pclog_w->index = 0;
	pclog_w->buf_size = CRASHLOG_BUFSIZE;
	pclog_w->id = _FLASH_FS_ID_CRASH_LOG;
	pclog_w->do_write_crashenv = 0;

	map_idx = map_log_id_to_index(pclog_w->id);
	if (map_idx > MAX_LOG_IDS) {
		result = -EINVAL;
		goto err_out;
	}
	memset(saved_checksum, 0, sizeof(saved_checksum));
	len = sizeof(pclog_w->crashenv);
	pclog_w->crashenv[0] = 0;
	handle = TFFS3_Open(FLASH_FS_CRASH, tffs3_mode_read);
	if (IS_ERR_OR_NULL(handle)) {
		result = -ENODEV;
		goto err_out;
	}
	result = TFFS3_Read(handle, pclog_w->crashenv, &len);
	TFFS3_Close(handle);
	if (result == 0) {
		parse_crash_env(pclog_w->crashenv, &saved_checksum[0]);
	}
	/*--- pr_info("%s: crash crashenv='%s' map_idx=%u readmask=%x\n", __func__, pclog_w->crashenv, map_idx, saved_checksum[map_idx].readmask); ---*/
	if ((saved_checksum[map_idx].readmask & READ_BY_CR) == 0) {
		/*--- pr_info("%s: readmask=%x - append\n", __func__, saved_checksum[map_idx].readmask); ---*/
		len = tffs_panic_log_read(pclog_w->buf, pclog_w->buf_size, pclog_w->id);
		if (len < 0) {
			len = 0;
		}
	} else {
		saved_checksum[map_idx].ts = 0;
		saved_checksum[map_idx].cs = 0;
		saved_checksum[map_idx].readmask = 0;
		set_crash_env(pclog_w->crashenv, sizeof(pclog_w->crashenv), saved_checksum);
		/*--- pr_info("%s: new crashenv '%s'\n", __func__, crashenv); ---*/
		/* write crash-variable when written finished: crashlog_close_write() */
		pclog_w->do_write_crashenv = 1;
		len = 0;
	}
	pclog_w->index = len;
	file->private_data = pclog_w;
	AVM_WATCHDOG_Crashlog_notify();
	/*--- pr_info("%s: done - index=%u\n", __func__, len); ---*/
	up(&log_sema);
	return 0;
err_out:
	if (pclog_w) {
		vfree(pclog_w);
	}
	up(&log_sema);
	return result;
}

/**
 * ret: newlen
 * skippe an Zeilengrenzen
 */
static unsigned int nice_skip(char *buf, unsigned int *skip, unsigned int len)
{
	unsigned int new_len;

	while ((len > *skip) && (buf[*skip] != '\n'))
		*skip += 1;
	if ((len > *skip) && (buf[*skip] == '\n'))
		*skip += 1;
	new_len = len - *skip;
	memmove(buf, &buf[*skip], new_len);
	return new_len;
}

/**
 */
static ssize_t crashlog_write(struct file *file, const char *buffer, size_t count, loff_t *offset __maybe_unused)
{
	const char *src;
	struct _crashlog_write_priv *pclog_w = (struct _crashlog_write_priv *)file->private_data;
	unsigned int free_place, skip;

	if (count > pclog_w->buf_size) {
		skip = count - pclog_w->buf_size;
		count = pclog_w->buf_size;
		src = &buffer[skip];
	} else {
		src = buffer;
	}
	free_place = (pclog_w->buf_size - pclog_w->index);
	/*--- pr_info("%s: index=%u -> free_place=%u count=%u\n", __func__, pclog_w->index, free_place, count); ---*/
	if (count > free_place) {
		skip = count - free_place;
		pclog_w->index = nice_skip(pclog_w->buf, &skip, pclog_w->index);
		/*--- pr_info("%s: skipped: %u index=%u\n", __func__, skip, pclog_w->index); ---*/
	}
	if (copy_from_user(&pclog_w->buf[pclog_w->index], src, count)) {
		return -EFAULT;
	}
	pclog_w->index += count;
	return count;
}

/**
 */
static int crashlog_close_write(struct inode *inode, struct file *file)
{
	int result = 0;
	size_t written;
	loff_t off __attribute__ ((unused));
	struct _crashlog_write_priv *pclog_w = (struct _crashlog_write_priv *)file->private_data;
	struct tffs_fops_handle *handle;

	/*--- pr_info("%s: idx=%u\n", __func__, pclog_w->index); ---*/
	handle = tffs_open_kern(pclog_w->id, 1);
	if (IS_ERR_OR_NULL(handle)) {
		/*--- pr_info("[%s] tffs_open_kern() failed\n", __func__); ---*/
		vfree(pclog_w);
		return (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
	}
	written = tffs_write_kern(handle, pclog_w->buf, pclog_w->index, &off);
	if (written != pclog_w->index) {
		pr_err("[%s] tffs_release_kern() failed %d\n", __func__, written);
	}
	tffs_release_kern(handle);

	if ((written == pclog_w->index) && pclog_w->do_write_crashenv) {
		void *cr_handle;

		cr_handle = TFFS3_Open(FLASH_FS_CRASH, tffs3_mode_write);
		if (!IS_ERR_OR_NULL(cr_handle)) {
			result = TFFS3_Write(cr_handle, pclog_w->crashenv, strlen(pclog_w->crashenv) + 1, 1);
			TFFS3_Close(cr_handle);
			if (result < 0) {
				pr_err("%s: FLASH_FS_CRASH: TFFS3_Write() -> result=%d\n",
				       __func__, result);
			} else {
				pr_info("%s: crash-variable set to '%s'\n",
					__func__, pclog_w->crashenv);
			}
		} else
			pr_err("[%s] can't write crash-variable result=%d '%s'\n",
			        __func__, (int)cr_handle, pclog_w->crashenv);
	}
	vfree(pclog_w);
	return 0;
}

/**
 */
static struct _logproc {
	const char *dir;
	const char *name;
	const unsigned int mode;
	const struct file_operations fops;
	struct proc_dir_entry *dir_entry;
} logproc[] = {
	{ .dir =  NULL,         .name = "avm/crash.log", .mode = 0222, .fops = { .open = crashlog_open_write, .write = crashlog_write, .read = NULL,	 .llseek = NULL,      .release = crashlog_close_write} },
	{ .dir =  "avm/log_cr", .name = "panic",	 .mode = 0666, .fops = { .open = paniclog_open_cr,    .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_cr", .name = "crash",	 .mode = 0666, .fops = { .open = crashlog_open_cr,    .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_cr", .name = "panic2",	 .mode = 0666, .fops = { .open = panic2log_open_cr,   .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_cr", .name = "crash2",	 .mode = 0666, .fops = { .open = crash2log_open_cr,   .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_sd", .name = "panic",	 .mode = 0666, .fops = { .open = paniclog_open_sd,    .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_sd", .name = "crash",	 .mode = 0666, .fops = { .open = crashlog_open_sd,    .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_sd", .name = "panic2",	 .mode = 0666, .fops = { .open = panic2log_open_sd,   .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
	{ .dir =  "avm/log_sd", .name = "crash2",	 .mode = 0666, .fops = { .open = crash2log_open_sd,   .write = log_proc_write, .read = seq_read, .llseek = seq_lseek, .release = log_proc_release} },
};

/**
 */
static int __init prepare_log_proc(void)
{
	struct proc_dir_entry *dir_entry = NULL;
	const char *lastdir = "";
	unsigned int i;

	/*--- pr_info("%s\n", __func__); ---*/
	for (i = 0; i < ARRAY_SIZE(logproc); i++) {
		if (logproc[i].dir && strcmp(lastdir, logproc[i].dir)) {
			dir_entry = proc_mkdir(logproc[i].dir, NULL);
			lastdir = logproc[i].dir;
		}
#if !defined(CRASHPANIC_LOG_PER_REMOTE_EVENT_SOURCE)
		if (logproc[i].dir == NULL) {
			if (!proc_create(logproc[i].name, logproc[i].mode, NULL, &logproc[i].fops)) {
				pr_err("%s can't proc_create(%s)\n", __func__, logproc[i].name);
			}
		} else if (dir_entry) {
			logproc[i].dir_entry = dir_entry;
			if (!proc_create(logproc[i].name, logproc[i].mode, dir_entry, &logproc[i].fops)) {
				pr_err("%s can't proc_create(%s)\n", __func__, logproc[i].name);
			}
		}
#endif/*--- #if !defined(CRASHPANIC_LOG_PER_REMOTE_EVENT_SOURCE) ---*/
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

/**
 * minimal panic-log: only for atheros without correct nmi because freezer occur
 */
static int tffs_read_dummy_log(char *buf, int buf_len, unsigned int id)
{
	int len;

	/*--- support only for panic-log ---*/
	if (id != FLASH_FS_ID_PANIC_LOG) {
		return 0;
	}
	len = strlen(panic_dummy_log);
	if (len == 0) {
		return 0;
	}
	len = min(len, buf_len);
	memcpy(buf, panic_dummy_log, len);
	return len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
#define HEADER_FORMAT_A	"UPTIME: %lu\n"

#define HEADER_FORMAT_B	"(%s - panic on %s)\n"

#define HEADER_FORMAT_C "%s %s\n"\
			"HW: %s.%s\n"\
			"FW: %s\n"\
			"Bootloader: %s\n"\
			"PANIC LOG VERSION 2.1\n"

#define HEADER_FORMAT       HEADER_FORMAT_A""HEADER_FORMAT_B""HEADER_FORMAT_C
#define HEADER_FORMAT_NMI   HEADER_FORMAT_A""HEADER_FORMAT_C


/**
 */
static void get_firmware_info(char const **fw_version, char **hw_version,
				     char **hw_subversion, char **bootloaderVersion)
{
	*fw_version = avm_fw_version();
	if (*fw_version == NULL) {
		*fw_version = prom_getenv("firmware_info");
	}
	if (*fw_version == NULL) {
		*fw_version = "";
	}
	*hw_version = prom_getenv("HWRevision");
	if (*hw_version == NULL) {
		*hw_version = "?";
	}
	*hw_subversion = prom_getenv("HWSubRevision");
	if (*hw_subversion ==  NULL) {
		*hw_subversion = "0";
	}
	*bootloaderVersion = prom_getenv("bootloaderVersion");
	if (*bootloaderVersion == NULL) {
		*bootloaderVersion = "0";
	}
}

/**
 */
static void tffs_panic_log_printkbuf(struct kmsg_dumper *dumper)
{
	if (tffs_panic_log_suppress == 0) {
		size_t towrite;
		struct timespec uptime;
		char htime[32], huptime[128];
		char reboot_txt[128], *reboot_post_txt;
		char time_stamp_buf[sizeof(HEADER_FORMAT) + 10 + sizeof(huptime) + sizeof(htime) + sizeof(reboot_txt) + 4 + 2 + 30 + 8];
		static char panic_log_buf[1 << CONFIG_LOG_BUF_SHIFT];
		unsigned long time_stamp_buf_len;
		const char *fw_version;
		char *hw_version;
		char *hw_subversion;
		char *bootloaderVersion;

		tffs_panic_log_suppress = 1;
		mb();
		tffs_panic_log_open();
		get_monotonic_boottime(&uptime);
		reboot_post_txt = "";
		reboot_txt[0] = 0;
#if defined(avm_rebootcounter_string)
		avm_rebootcounter_string(reboot_txt, sizeof(reboot_txt));
#endif	/*--- #else ---*/	       /*--- #if defined(avm_rebootcounter_string) ---*/
		if (reboot_txt[0]) {
			reboot_post_txt = "(since last regular reboot/power-cut)";
		}
		get_firmware_info(&fw_version, &hw_version, &hw_subversion, &bootloaderVersion);

		time_stamp_buf_len =
		    snprintf(time_stamp_buf, sizeof(time_stamp_buf), HEADER_FORMAT, (unsigned long)uptime.tv_sec,
			     human_readable_time(huptime, sizeof(huptime), (unsigned long)uptime.tv_sec),
			     time_to_ascii((unsigned long)get_act_time(), htime, sizeof(htime), ""),
			     reboot_txt, reboot_post_txt, hw_version, hw_subversion,
			     fw_version, bootloaderVersion);
		tffs_panic_log_write(time_stamp_buf, time_stamp_buf_len);
		AVM_WATCHDOG_emergency_retrigger();
		if (kmsg_dump_get_buffer(dumper, 0, panic_log_buf, sizeof(panic_log_buf), &towrite) == false) {
			towrite = 0;
		}
		AVM_WATCHDOG_emergency_retrigger();
		if (towrite > 0) {
			tffs_panic_log_write(panic_log_buf, towrite);
		}
		tffs_panic_log_close();

		/*--- Entwickler wollen alles auch noch auf der Konsole sehen ---*/
		pr_err("\n\n------------------- Last part of Panic-log Content: -------------------\n\n");
		{
		unsigned int log_symbols = 2  /* Sekunden */ * 115000 / 10;
		unsigned int offset = 0;

		if (towrite > log_symbols) {
			offset  = towrite - log_symbols;
			towrite = log_symbols;
		} else {
			offset = 0;
		}
		print_to_console(&panic_log_buf[offset], towrite);
		}
	}
}

/**
 */
static __attribute__((unused)) void panic_log(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason)
{
	static bool panicking;
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;

/*--- printk_linux("%s(%d)\n", __func__, reason); ---*/
	switch (reason) {
	case KMSG_DUMP_RESTART:
	case KMSG_DUMP_HALT:
	case KMSG_DUMP_POWEROFF:
		/* These are almost always orderly shutdowns. */
		return;
	case KMSG_DUMP_OOPS:
#ifdef CONFIG_AVM_FASTIRQ
#ifdef CONFIG_ARCH_QCOM
		return; // We want to split OOPS and PANIC
#endif
#endif
	case KMSG_DUMP_PANIC:
		panicking = true;
		break;
	case KMSG_DUMP_EMERG:
		if (panicking)
			/* Panic report already captured. */
			return;
		break;
	default:
		pr_err("%s: ignoring unrecognized KMSG_DUMP_* reason %d\n", __func__, (int) reason);
		return;
	}
	if (!spin_trylock_irqsave(&lock, flags)) {
		return;
	}
	tffs_panic_log_printkbuf(dumper);
	spin_unlock_irqrestore(&lock, flags);
}
/**
 * minimal panic-log: only for atheros without correct nmi because freezer occur
 */
void TFFS3_panic_dummy_log(const char *txt)
{
	char reboot_txt[128], *reboot_post_txt;
	const char *fw_version;
	char *hw_version;
	char *hw_subversion;
	char *bootloaderVersion;
	unsigned char uuid[16];

	reboot_post_txt = "";
	reboot_txt[0] = 0;
#if defined(avm_rebootcounter_string)
	avm_rebootcounter_string(reboot_txt, sizeof(reboot_txt));
#endif	/*--- #else ---*/	       /*--- #if defined(avm_rebootcounter_string) ---*/
	if (reboot_txt[0]) {
		reboot_post_txt = "(since last regular reboot/power-cut)";
	}
	get_firmware_info(&fw_version, &hw_version, &hw_subversion, &bootloaderVersion);

	generate_random_uuid(uuid);
	snprintf(panic_dummy_log, sizeof(panic_dummy_log), HEADER_FORMAT_NMI "%s\n%16ph\n", 0UL, reboot_txt,
		 reboot_post_txt, hw_version, hw_subversion,
		 fw_version, bootloaderVersion, txt, uuid);
}
#else
/**
 */
static void tffs_panic_log_printkbuf(struct kmsg_dumper *dumper, const char *s1, unsigned long l1,
				     const char *s2, unsigned long l2)
{
	if (tffs_panic_log_suppress == 0) {
		struct timespec uptime;
		char htime[32], huptime[128];
		char time_stamp_buf[sizeof(" UPTIME:_\n") + sizeof(htime) + sizeof(huptime) + 10 + 128 + sizeof("\n( - panic on )\nPANIC LOG VERSION 2.0\n")];
		unsigned long time_stamp_buf_len;

		tffs_panic_log_suppress = 1;
		mb();
		tffs_panic_log_open();

		get_monotonic_boottime(&uptime);
		time_stamp_buf_len =
		    snprintf(time_stamp_buf, sizeof(time_stamp_buf),
			     "UPTIME: %lu\n(%s - panic on %s)\nPANIC LOG VERSION 2.0\n",
			     (unsigned long)uptime.tv_sec,
			     human_readable_time(huptime, sizeof(huptime), (unsigned long)uptime.tv_sec),
			     time_to_ascii((unsigned long)get_act_time(), htime, sizeof(htime), "")
		    );

		AVM_WATCHDOG_emergency_retrigger();

		tffs_panic_log_write(time_stamp_buf, time_stamp_buf_len);

		AVM_WATCHDOG_emergency_retrigger();

		if (l1 > 0) {
			tffs_panic_log_write(s1, l1);
		}
		AVM_WATCHDOG_emergency_retrigger();

		if (l2 > 0) {
			tffs_panic_log_write(s2, l2);
		}

		tffs_panic_log_close();

		/*--- Entwickler wollen alles auch noch auf der Konsole sehen ---*/
		pr_err("\n\n------------------- Last part of Panic-log Content: -------------------\n\n");
		{
			unsigned int log_symbols = 2 /* Sekunden */  * 115000 / 10;

			if (l2 > log_symbols) {
				s2 = &s2[l2 - log_symbols];
				l2 = log_symbols;
				print_to_console(s2, l2);
			} else {
				if (l2 > 0) {
					log_symbols -= l2;
				}
				if (l1 > 0) {
					if (l1 > log_symbols) {
						s1 = &s1[l1 - log_symbols];
						l1 = log_symbols;
						print_to_console(s1, l1);
					}
				}
				if (l2 > 0) {
					print_to_console(s2, l2);
				}
			}
		}
	}
}

/**
 */
static void panic_log(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason,
					  const char *s1, unsigned long l1,
					  const char *s2, unsigned long l2)
{
	static bool panicking;
	static DEFINE_SPINLOCK(lock);
	unsigned long flags;

/*--- printk_linux("%s(%d)\n", __func__, reason); ---*/
	switch (reason) {
	case KMSG_DUMP_RESTART:
	case KMSG_DUMP_HALT:
	case KMSG_DUMP_POWEROFF:
	/* These are almost always orderly shutdowns. */
		return;
	case KMSG_DUMP_OOPS:
#ifdef CONFIG_AVM_FASTIRQ
#ifdef CONFIG_ARCH_QCOM
		return; // We want to split OOPS and PANIC
#endif
#endif
	case KMSG_DUMP_PANIC:
		panicking = true;
		break;
	case KMSG_DUMP_EMERG:
	if (panicking)
		/* Panic report already captured. */
		return;
	break;
	default:
		pr_err("%s: ignoring unrecognized KMSG_DUMP_* reason %d\n", __func__, (int) reason);
	return;
	}
	if (!spin_trylock_irqsave(&lock, flags)) {
		return;
	}
	tffs_panic_log_printkbuf(dumper, s1, l1, s2, l2);
	spin_unlock_irqrestore(&lock, flags);
}
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)

#if !defined(CONFIG_PSTORE_RAM)
/**
 */
static struct kmsg_dumper panic_log_dumper = {
	.dump = panic_log
};

/**
 */
static int __init install_panic_log_dumper(void)
{
	/*--- pr_info("%s\n", __func__); ---*/
	return kmsg_dump_register(&panic_log_dumper);
}

late_initcall(install_panic_log_dumper);
#endif
