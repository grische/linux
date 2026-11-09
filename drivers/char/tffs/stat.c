// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <avm/tffs/tffs.h>
#include <avm/compat/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>

#ifdef CONFIG_PROC_FS
struct _tffs_stat {
	unsigned int id;
	unsigned long access;
	unsigned long firstaccess_jiffies;
	unsigned long lastaccess_jiffies;
	unsigned long long size;
	unsigned long cached_acc;
	unsigned long cached_size;
	struct _tffs_stat *next;
};
static struct _tffs_stat *tffsstat_readanchor;
static struct _tffs_stat *tffsstat_writeanchor;
/**
 * access_inc: add one to access and sort in list
 */
static struct _tffs_stat *tffsstat_process(unsigned int id, struct _tffs_stat **anchor,
					   unsigned int access_inc, unsigned int cached)
{
	struct _tffs_stat *act, *tmpact = NULL, *lastact = NULL;
	unsigned long access = 0;

	act = *anchor;
	while (act) {
		if (act->access != access) {
			access = act->access;
			tmpact = lastact;
			/*--- pr_info("mark last access=%lu lastact=%p\n", act->access, tmpact); ---*/
		}
		if (act->id == id) {
			if (access_inc) {
				if (cached)
					act->cached_acc++;
				else
					act->access++;
				if (act->access > access) {
					/*--- pr_info("resort id=%u access=%lu\n", act->id, act->access); ---*/
					/*--- resort ---*/
					if (lastact)
						lastact->next = act->next;
					if (tmpact) {
						act->next    = tmpact->next;
						tmpact->next = act;
						/*--- pr_info("set id=%u(%lu) behind id=%u(%lu)\n", act->id, act->access, tmpact->id, tmpact->access); ---*/
					} else if (*anchor != act) {
						/*--- pr_info("set id=%u(%lu) as anchor\n", act->id, act->access); ---*/
						act->next = *anchor;
						*anchor	  = act;
					}
				}
			}
			return act;
		}
		lastact = act;
		act	= act->next;
	}
	if (access_inc == 0) {
		return act;
	}
	act = kzalloc(sizeof(struct _tffs_stat), GFP_ATOMIC);
	if (act == NULL) {
		return act;
	}
	act->id	    = id;
	act->access = 1;
	if (lastact == NULL) {
		*anchor = act;
	} else {
		lastact->next = act;
	}
	/*--- pr_info("%p: new entry id=%u access=%lu lastact=%p\n", act, act->id, act->access, lastact); ---*/
	return act;
}
/**
 * collect statistic value
 * id:  TFFS-ID
 * len: written/read len
 * mode: 0: read 1: write
 */
void tffs_write_statistic(unsigned int id, unsigned int len, unsigned int mode, unsigned int cached)
{
	struct _tffs_stat *ptffsstat_per_id;

	ptffsstat_per_id = tffsstat_process(
		id, mode == 0 ? &tffsstat_readanchor : &tffsstat_writeanchor, 1, cached);
	if (ptffsstat_per_id == NULL) {
		return;
	}
	if (cached) {
		ptffsstat_per_id->cached_size += len;
	} else {
		ptffsstat_per_id->size += len;
	}

	if ((ptffsstat_per_id->access == 1 || ptffsstat_per_id->cached_acc == 1) &&
	    ptffsstat_per_id->access != ptffsstat_per_id->cached_acc) {
		ptffsstat_per_id->firstaccess_jiffies = jiffies;
	}
	ptffsstat_per_id->lastaccess_jiffies = jiffies;
}
/**
 */
static inline void fill_sum(struct _tffs_stat *psum, struct _tffs_stat *pact)
{
	psum->access += pact->access;
	psum->cached_acc += pact->cached_acc;
	psum->size += pact->size;
	psum->cached_size += pact->cached_size;
	if ((psum->firstaccess_jiffies == 0) ||
	    (psum->firstaccess_jiffies > pact->firstaccess_jiffies)) {
		psum->firstaccess_jiffies = pact->firstaccess_jiffies;
	}
	if ((psum->lastaccess_jiffies == 0) ||
	    (psum->lastaccess_jiffies < pact->lastaccess_jiffies)) {
		psum->lastaccess_jiffies = pact->lastaccess_jiffies;
	}
}
/**
 * timediff in jiffies
 */
static char *human_time(char *buf, long timediff)
{
	if (timediff >= 0) {
		unsigned long sec, min, hour;
		unsigned long time = timediff / HZ;

		sec = time % 60;
		time /= 60;
		min = time % 60;
		time /= 60;
		hour = time;
		sprintf(buf, "%lu:%02lu:%02lu", hour, min, sec);
	} else {
		strcpy(buf, "never");
	}
	return buf;
}
/**
 */
static char *human_bytes(char *buf, unsigned long long uncached, unsigned long long cached)
{
	static const char *const tab[] = { "  B", "kiB", "MiB", "GiB", "TiB" };
	unsigned long long bytes;
	unsigned int i = 0, rest = 0, rest_cached = 0;

	bytes = max(uncached, cached);
	while ((bytes > (1 << 10)) && i < (ARRAY_SIZE(tab) - 1)) {
		rest	    = ((unsigned int)uncached & ((1 << 10) - 1));
		rest_cached = ((unsigned int)cached & ((1 << 10) - 1));
		uncached >>= 10;
		cached >>= 10;
		bytes >>= 10;
		i++;
	}

	sprintf(buf, "%03u.%03u/%3u.%03u %s", (unsigned int)uncached, (rest * 1000) >> 10,
		(unsigned int)cached, (rest_cached * 1000) >> 10, tab[i]);

	return buf;
}
/**
 * id == 0: Summe
 */
static inline int print_entry(char *buf, int buflen, const char *prefix, struct _tffs_stat *pact,
			      int sum)
{
	unsigned long timediff;
	int len, start_buflen = buflen;
	unsigned long long bytes_per_sec, bytes_per_sec_cached;
	char txtbuf[3][64];

	if (buflen <= 0) {
		return 0;
	}
	timediff = (jiffies - pact->firstaccess_jiffies) / HZ;
	if (timediff) {
		bytes_per_sec = pact->size;
		do_div(bytes_per_sec, timediff);
		bytes_per_sec_cached = pact->cached_size;
		do_div(bytes_per_sec_cached, timediff);
	} else {
		bytes_per_sec = bytes_per_sec_cached = 0;
	}
	if (!sum) {
		if (pact->id == FLASH_FS_ID_SKIP) {
			len = snprintf(buf, buflen, "Managing ");
		} else {
			len = snprintf(buf, buflen, "ID %4u  ", pact->id);
		}
		if (len > 0) {
			buflen -= len, buf += len;
		}
	}
	if (buflen) {
		len = snprintf(buf, buflen,
			       "%s access %8lu/%-8lu - %s (%s/s) last access before %s\n", prefix,
			       pact->access, pact->cached_acc,
			       human_bytes(txtbuf[0], pact->size, pact->cached_size),
			       human_bytes(txtbuf[1], bytes_per_sec, bytes_per_sec_cached),
			       human_time(txtbuf[2], jiffies - pact->lastaccess_jiffies));
		if (len > 0) {
			buflen -= len;
		}
	}
	return start_buflen - buflen;
}
/**
 */
static void tffs_statistic(char *buf, unsigned int buflen, unsigned int reset)
{
	struct _tffs_stat *act;
	struct _tffs_stat readsum, writesum;
	unsigned int len;

	memset(&readsum, 0, sizeof(readsum));
	memset(&writesum, 0, sizeof(writesum));

	act = tffsstat_readanchor;
	while (act) {
		len = print_entry(buf, buflen, "read ", act, 0);
		buflen -= len, buf += len;
		fill_sum(&readsum, act);
		if (reset) {
			act->size = act->cached_size = act->access = act->cached_acc =
				act->firstaccess_jiffies = act->lastaccess_jiffies = 0;
		}
		act = act->next;
	}
	len = snprintf(buf, buflen,
		       "-----------------------------------------------------------------------\n");
	if (len > 0) {
		buflen -= len, buf += len;
	}
	len = print_entry(buf, buflen, " summary read ", &readsum, 1);
	buflen -= len, buf += len;
	len = snprintf(buf, buflen, "\n");
	if (len > 0) {
		buflen -= len, buf += len;
	}
	act = tffsstat_writeanchor;
	while (act) {
		len = print_entry(buf, buflen, "write", act, 0);
		buflen -= len, buf += len;
		fill_sum(&writesum, act);
		if (reset) {
			act->size = act->cached_size = act->access = act->cached_acc =
				act->firstaccess_jiffies = act->lastaccess_jiffies = 0;
		}
		act = act->next;
	}
	len = snprintf(buf, buflen,
		       "-----------------------------------------------------------------------\n");
	if (len > 0) {
		buflen -= len, buf += len;
	}
	len = print_entry(buf, buflen, " summary write", &writesum, 1);
	buflen -= len, buf += len;
	len = snprintf(buf, buflen, "\n");
}

/**
 */
static int tffsstat_show(struct seq_file *seq, void *data)
{
	no_printk(KERN_DEBUG "%s %p %p\n", __func__, seq->private, data);
	if (seq->private) {
		seq_printf(seq, "%s", (char *)seq->private);
	}
	return 0;
}
#define TFFSTSTAT_BUF_SIZE (64 << 10)
/**
 */
static int tffsstat_open(struct inode *inode, struct file *file)
{
	unsigned int bufsize = TFFSTSTAT_BUF_SIZE;
	char *buf;

	buf = kmalloc(bufsize, GFP_KERNEL);
	if (buf == NULL) {
		return -ENOMEM;
	}
	buf[0] = 0;
	tffs_statistic(buf, bufsize, 0);
	return single_open(file, tffsstat_show, buf);
}

/**
 */
static int tffsstat_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;

	kfree(seq->private);
	seq->private = NULL;
	return single_release(inode, file);
}

/**
 */
static const struct proc_ops tffsstat_ops = {
	.proc_open	= tffsstat_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= tffsstat_release,
};

/**
 */
static int tffs_procstat_init(void)
{
	proc_create("tffs_stat", 0444, NULL, &tffsstat_ops);
	return 0;
}
late_initcall(tffs_procstat_init);

#endif /*--- #ifdef CONFIG_PROC_FS ---*/
