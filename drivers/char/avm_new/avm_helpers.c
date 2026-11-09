// SPDX-License-Identifier: GPL-2.0+

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <avm/sammel/helpers.h>

int avm_read_from_file(const char *filename, char *str, int maxlen)
{
	struct file *fp;
	mm_segment_t oldfs;
	int bytesRead;

	if (WARN_ON(!filename || !str || maxlen == 0))
		return -EINVAL;

	fp = filp_open(filename, O_RDONLY, 00);
	if (IS_ERR(fp) || (fp->f_op == 0)) {
		str[0] = 0;
		return -EIO;
	}
	/* Lesezugriff auf File(system) erlaubt? */
	if (fp->f_op->read == NULL) {
		str[0] = 0;
		filp_close(fp, NULL);
		return -EPERM;
	}
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0; /* Von Anfang an lesen */
	bytesRead = fp->f_op->read(fp, str, maxlen - 1, &fp->f_pos);

	if (bytesRead < 0)
		bytesRead = 0;

	str[bytesRead] = 0;
	set_fs(oldfs);
	filp_close(fp, NULL); /* Close the file */
	return bytesRead;
}
EXPORT_SYMBOL(avm_read_from_file);

int avm_write_to_file(const char *filename, const char *str, int len)
{
	struct file *fp;
	mm_segment_t oldfs;

	if (WARN_ON(!filename || !str || len == 0))
		return -EINVAL;

	pr_debug("%s: write %s to %s\n", __func__, str, filename);
	fp = filp_open(filename, O_WRONLY, 00);

	if (IS_ERR(fp) || (fp->f_op == 0))
		return -EIO;

	/* Schreibzugriff auf File(system) erlaubt? */
	if (fp->f_op->write == NULL) {
		pr_err("[avm_power]speedstep failed: write %s\n", filename);
		filp_close(fp, NULL);
		return -EPERM;
	}
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	fp->f_pos = 0; /* Von Anfang an schreiben*/
	fp->f_op->write(fp, str, len, &fp->f_pos);
	set_fs(oldfs);
	filp_close(fp, NULL); /* Close the file */
	return 0;
}
EXPORT_SYMBOL(avm_write_to_file);

/*
 * Sometimes avm modules need symbols that are not exported by the kernel.
 *
 * To avoid the need to patch every kernel there is, we just export them
 * here. Use the GPL variant unless the exported symbol is known to be non-GPL.
 */
EXPORT_SYMBOL_GPL(func_ptr_is_kernel_text);

ssize_t time_to_ascii(time64_t local_time, char *buf, int len)
{
	static const char * const month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char * const day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	struct rtc_time tm;

	rtc_time_to_tm(local_time, &tm);
	return scnprintf(buf, len, "%s %s %02d %02d:%02d:%02d %02d UTC",
		 day[tm.tm_wday], month[tm.tm_mon],
		 tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);
}

/*
 * ktime_get_seconds() reads the monotonic clock without taking locks.
 * Emulate it on old kernels.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
time64_t ktime_get_seconds(void)
{
	unsigned long long nsecs;
	unsigned long flags;

	/*
	 * We use sched_clock() as a lockless fallback here.  Now, it depends
	 * on the kernel/arch if sched_clock() is lockless.  The default
	 * (weak) implementation is, but arch code can override it.
	 *
	 * However, at AVM we only have two platforms with kernels < v3.19,
	 * VR9 and Puma6.  VR9 and armp6 don't touch sched_clock().  Atomp6
	 * overrides it with native_sched_clock(), which is also lockless.
	 */
	local_irq_save(flags);
	nsecs = sched_clock();
	local_irq_restore(flags);

	return div_u64(nsecs, NSEC_PER_SEC);
}
EXPORT_SYMBOL_GPL(ktime_get_seconds);
#endif
