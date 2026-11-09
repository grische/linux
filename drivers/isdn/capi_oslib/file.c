// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/device.h>
#pragma GCC pop_options

#include "debug.h"
#include <avm/capi_oslib/oslib.h>
#include <avm/capi_oslib/new.h>
#include <avm/compat/misc.h>
#include "appl.h"
#include "host.h"
#include "capi_oslib.h"
#include "local_capi.h"
#include "capi_pipe.h"

#include <linux/cdev.h>
#include "zugriff.h"


/*--- MODULE_DESCRIPTION("AVM Central Event distribution"); ---*/
/*--- MODULE_LICENSE("\n(C) Copyright 2004, AVM\n"); ---*/

static struct _capi_oslib capi_oslib;

static int capi_oslib_open(struct inode *inode __attribute__((unused)), struct file *filp);
static int capi_oslib_close(struct inode *inode __attribute__((unused)), struct file *filp);
static int capi_oslib_fasync(int fd, struct file *filp, int mode);
static ssize_t capi_oslib_write(struct file *filp, const char __user *write_buffer, size_t write_length, loff_t *write_pos);
static ssize_t capi_oslib_read(struct file *filp, char __user *read_buffer, size_t max_read_length, loff_t *read_pos);
void capi_oslib_cleanup(void);
long capi_oslib_ioctl(struct file *file, unsigned int cmd, unsigned long args);
static unsigned int capi_oslib_poll(struct file *filp, poll_table *wait);

static int capi_oslib_capi_register(struct _capi_oslib_open_data *open_data,
				    unsigned char *Buffer, unsigned int BufferSize,
				    unsigned int MessageBufferSize, unsigned int MaxNCCIs,
				    unsigned int WindowSize, unsigned int B3BlockSize);
static int capi_oslib_capi_release(struct _capi_oslib_open_data *open_data);

struct semaphore capi_oslib_sema;
unsigned long long capi_oslib_source_mask;
DEFINE_SPINLOCK(capi_oslib_lock);

static const struct file_operations capi_oslib_fops = {
	.owner =   THIS_MODULE,
	.open =    capi_oslib_open,
	.release = capi_oslib_close,
	.read =    capi_oslib_read,
	.write =   capi_oslib_write,
	.unlocked_ioctl =   capi_oslib_ioctl,
	.fasync =  capi_oslib_fasync,
	.poll = capi_oslib_poll,
};
#define CAPIOSLIB_UDEV
/**
 */
int __init capi_oslib_file_init(void)
{
	int reason;

	DEB_INFO("%s: register_chrdev_region()\n", __func__);
	capi_oslib.device = MKDEV(68, 0); /* CAPI !!! */
	reason = register_chrdev_region(capi_oslib.device, 1, "capi_oslib");
	if (reason) {
		DEB_ERR("%s: register_chrdev_region failed: reason %d!\n", __func__, reason);
		return -ERESTARTSYS;
	}

	capi_oslib.cdev = cdev_alloc();
	if (!capi_oslib.cdev) {
		unregister_chrdev_region(capi_oslib.device, 1);
		DEB_ERR("%s: cdev_alloc failed!\n", __func__);
		return -ERESTARTSYS;
	}

	capi_oslib.cdev->owner = capi_oslib_fops.owner;
	capi_oslib.cdev->ops = &capi_oslib_fops;
	kobject_set_name(&(capi_oslib.cdev->kobj), "capi_oslib");
	spin_lock_init(&capi_oslib_lock);

	/**
	 */
	sema_init(&capi_oslib_sema, 1);

	/*  TODO: weitere initialisierungen */

	if (cdev_add(capi_oslib.cdev, capi_oslib.device, 1)) {
		kobject_put(&capi_oslib.cdev->kobj);
		unregister_chrdev_region(capi_oslib.device, 1);
		DEB_ERR("%s: cdev_add failed!\n", __func__);
		return -ERESTARTSYS;
	}
#if defined(CAPIOSLIB_UDEV)
	/*--- Geraetedatei anlegen: ---*/
	capi_oslib.osclass = class_create(THIS_MODULE, "capi_oslib");
	device_create(capi_oslib.osclass, NULL, capi_oslib.device, NULL, "capi_oslib");
#endif/*--- #if defined(CAPIOSLIB_UDEV) ---*/
	return 0;
}

/**
 */
#if defined(CONFIG_CAPI_OSLIB_MODULE)
void capi_oslib_file_cleanup(void)
{
	DEB_INFO("[%s]: unregister_chrdev(%u)\n", "capi_oslib", capi_oslib.major);
#if defined(CONFIG_AVM_PUSH_BUTTON)
	capi_oslib_push_button_deinit();
#endif /*--- #if defined(CONFIG_AVM_PUSH_BUTTON) ---*/
#if defined(CAPIOSLIB_UDEV)
	device_destroy(capi_oslib.osclass, capi_oslib.device);
	class_destroy(capi_oslib.osclass);
#endif/*--- #if defined(CAPIOSLIB_UDEV) ---*/
	cdev_del(capi_oslib.cdev); /* Delete char device */

	/*  TODO: weitere de-initialisierungen */

	unregister_chrdev_region(capi_oslib.device, 1);
}
#endif /*--- #if defined(CONFIG_CAPI_OSLIB_MODULE) ---*/

/**
 */
void capi_oslib_file_activate(void)
{
	HOST_INIT(SOURCE_DEV_CAPI, 25 /* max APPLs */, 100 /* max NCCIs */, 0 /* CAPI_INDEX */);
	HOST_INIT(SOURCE_PTR_CAPI, 25 /* max APPLs */, 100 /* max NCCIs */, 0 /* CAPI_INDEX */);
	capi_oslib.activated = 1;
	DEB_INFO("%s: activated\n", __func__);
}

/**
 */
static int capi_oslib_fasync(int fd, struct file *filp, int mode)
{
	struct _capi_oslib_open_data *open_data = (struct _capi_oslib_open_data *)filp->private_data;

	DEB_INFO("%s:\n", __func__);
	return fasync_helper(fd, filp, mode, &(open_data->fasync));
}

/**
 */
static unsigned int capi_oslib_poll(struct file *filp, poll_table *wait)
{
	struct _capi_oslib_open_data *open_data = (struct _capi_oslib_open_data *)filp->private_data;
	unsigned int status = 0;

	if (open_data->ApplId == 0) /* nicht (mehr) registriet */
		return 0;

	/*--- DEB_INFO("[poll] wait:\n"); ---*/
	poll_wait(filp, &(open_data->wait_queue), wait);
#if defined(CAPIOSLIB_CHECK_LATENCY)
	/*--- capi_check_latency(open_data->ApplId & 0x3F, (char *)__func__, 0); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/

	/* TODO: pruefen ob message anliegt */
	if (open_data->ApplId) {
		status |= POLLOUT | POLLWRNORM;
		if (LOCAL_CAPI_GET_MESSAGE(open_data->mode, open_data->ApplId, NULL, CAPI_NO_SUSPEND))
			status |= POLLIN | POLLRDNORM;
	}
	/*--- if(status) ---*/
#if 0
	if (status)
		DEB_INFO("%s:%s%s (%s)\n", __func__,
				status & POLLIN ? " POLLIN" : "",
				status & POLLOUT ? " POLLOUT" : "",
				current->comm);
#endif

	/*--- { ---*/
		/*--- static int xxx = 0; ---*/
		/*--- if((xxx++ & 0xFF) == 0) ---*/
			/*--- printk("[poll] %s\n", current->comm); ---*/
	/*--- } ---*/

	return status;
}

/**
 */
static int capi_oslib_open(struct inode *inode __attribute__((unused)), struct file *filp)
{
	struct _capi_oslib_open_data *open_data;

	DEB_INFO("%s\n", __func__);

	if (filp->f_flags & O_APPEND) {
		DEB_ERR("%s: open O_APPEND not supported\n", __func__);
		return -EFAULT;
	}

	if (capi_oslib.activated == 0) {
		DEB_ERR("%s:not jet activated\n", __func__);
		return -EFAULT;
	}

	if (down_interruptible(&capi_oslib_sema)) {
		DEB_ERR("%s:down_interruptible() failed\n", __func__);
		return -ERESTARTSYS;
	}

	open_data = kmalloc(sizeof(struct _capi_oslib_open_data), GFP_KERNEL);
	if (!open_data) {
		DEB_ERR("%s: open malloc failed\n", __func__);
		up(&capi_oslib_sema);
		return -EFAULT;
	}
	memset(open_data, 0, sizeof(*open_data));

	init_waitqueue_head(&open_data->wait_queue);
	open_data->pf_owner = &(filp->f_owner);
	filp->private_data = (void *)open_data;

	up(&capi_oslib_sema);
	DEB_INFO("%s: open success flags=0x%x\n", __func__, filp->f_flags);
	return 0;
}

/**
 */
static int capi_oslib_close(struct inode *inode __attribute__((unused)), struct file *filp)
{

	DEB_INFO("%s\n", __func__);

	down(&capi_oslib_sema);
	/*--- if(down_interruptible(&capi_oslib_sema)) { ---*/
		/*--- DEB_ERR("%s down_interruptible() failed\n", "capi_oslib"); ---*/
		/*--- return -ERESTARTSYS; ---*/
	/*--- } ---*/
	/*--- achtung auf ind wartende "gefreien" und warten bis alle fertig ---*/

	if (filp->private_data) {
		struct _capi_oslib_open_data *open_data = (struct _capi_oslib_open_data *)filp->private_data;

		/* TODO: auswerten des close falls */
		if (open_data->ApplId)
			capi_oslib_capi_release(open_data);

		capi_oslib_fasync(-1, filp, 0);  /*--- remove this file from asynchonously notified filp ---*/
		kfree(filp->private_data);
		filp->private_data = NULL;
	}

	up(&capi_oslib_sema);
	return 0;
}

#if defined(CAPIOSLIB_CHECK_LATENCY)
/**
 */
struct _generic_stat {
	signed long cnt;
	signed long avg;
	signed long min;
	signed long max;
};

/**
 */
void init_generic_stat(struct _generic_stat *pgstat)
{
	pgstat->min = LONG_MAX;
	pgstat->max = LONG_MIN;
	pgstat->cnt = 0;
	pgstat->avg = 0;
}

/**
 */
void generic_stat(struct _generic_stat *pgstat, signed long val)
{
	if (pgstat->cnt == 0) {
		init_generic_stat(pgstat);
	}
	if (val > pgstat->max)
		pgstat->max = val;
	if (val < pgstat->min)
		pgstat->min = val;
	pgstat->avg += val;
	pgstat->cnt++;
}

/**
 * reset: Statistik ruecksetzen
 * mode: 0 in msec
 *       1 nur Wert
 *       2 in usec
 */
void display_generic_stat(char *prefix, unsigned int preval,
			  struct _generic_stat *pgstat, unsigned int mode,
			  unsigned int reset)
{
	struct _generic_stat gstat;
	signed long cnt;
	unsigned long flags;

	spin_lock_irqsave(&capi_oslib_lock, flags);
	cnt = pgstat->cnt;
	if (cnt == 0) {
		spin_unlock_irqrestore(&capi_oslib_lock, flags);
		return;
	}
	memcpy(&gstat, pgstat, sizeof(gstat));
	if (reset) {
		pgstat->cnt = 0;
	}
	spin_unlock_irqrestore(&capi_oslib_lock, flags);
	pr_info("[%x]%s[%ld] min=%ld max=%ld avg=%ld %s\n",
	       preval, prefix, cnt, gstat.min,
	       gstat.max, gstat.avg / cnt, mode == 0 ? "msec" : mode == 2 ? "usec" : "");
}

#define CAPI_MAGIC_TIMESTAMP_START      0x434D5453UL
#define CAPI_MAGIC_TIMESTAMP_END      (~0x434D5453UL)

#define CAPI_MAGIC_TIMESTAMP_START_HOST      0x4D435354UL
#define CAPI_MAGIC_TIMESTAMP_END_HOST      (~0x4D435354UL)
#define MAX_STAT_INSTANCE                   64
struct _capimagic_timestamp {
	unsigned int tsmstart;
	struct timeval tv;
	unsigned int bytenmb;
	unsigned int handle;
	unsigned int tsmend;
} __attribute__((packed));

static unsigned int bytenmbcounter[MAX_STAT_INSTANCE];

/**
 */
void capi_check_latency(unsigned int handle, char *name, unsigned int start)
{
	static unsigned long setflag[MAX_STAT_INSTANCE];
	static struct timeval tvstart[MAX_STAT_INSTANCE];
	static int last_jiffies[MAX_STAT_INSTANCE];
	static struct _generic_stat latency[MAX_STAT_INSTANCE];

	if (handle >= MAX_STAT_INSTANCE) {
		return;
	}
	if (last_jiffies[handle] == 0) {
		init_generic_stat(&latency[handle]);
		last_jiffies[handle] = jiffies;
	}
	if (test_and_set_bit(1, &setflag[handle]) == 0) {
		if ((jiffies - last_jiffies[handle]) > 10 * HZ) {
			last_jiffies[handle] = jiffies;
			display_generic_stat(name ? name : "latency", handle, &latency[handle], 2, 1);
		}
	}
	clear_bit(1, &setflag[handle]);
	if (start) {
		unsigned long flags;

		spin_lock_irqsave(&capi_oslib_lock, flags);
		if (test_and_set_bit(0, &setflag[handle])) {
			/*--- bereits ein start gemerkt ---*/
			spin_unlock_irqrestore(&capi_oslib_lock, flags);
			return;
		}
		do_gettimeofday(&tvstart[handle]);
		spin_unlock_irqrestore(&capi_oslib_lock, flags);
	} else {
		struct timeval tvact;
		long long te, ta;
		unsigned long tdiff, flags;

		spin_lock_irqsave(&capi_oslib_lock, flags);
		ta    = ((long long)tvstart[handle].tv_sec  * (long long)(1000U * 1000U)) + (long long)tvstart[handle].tv_usec;
		if (test_and_clear_bit(0, &setflag[handle]) == 0) {
			/*--- start fehlte ---*/
			spin_unlock_irqrestore(&capi_oslib_lock, flags);
			return;
		}
		do_gettimeofday(&tvact);
		spin_unlock_irqrestore(&capi_oslib_lock, flags);
		te    = ((long long)tvact.tv_sec * (long long)(1000U * 1000U)) + (long long)tvact.tv_usec;
		tdiff = (unsigned long) ((te - ta)); /*--- in usec ---*/

		generic_stat(&latency[handle], tdiff);
	}
}
EXPORT_SYMBOL(capi_check_latency);
#define TS_FRAGMENT_OFFSET  max(64U, sizeof(struct _capimagic_timestamp))
/**
 */
void capi_generate_timestamp(unsigned int handle, unsigned char *data, unsigned int datalen)
{
	struct timeval tv;
#if 1
	static int last_jiffies[MAX_STAT_INSTANCE];
	static struct _generic_stat datalen_stat[MAX_STAT_INSTANCE];

	if (handle >= MAX_STAT_INSTANCE) {
		return;
	}
	if (last_jiffies[handle] == 0) {
		init_generic_stat(&datalen_stat[handle]);
		last_jiffies[handle] = jiffies;
	}
	if ((jiffies - last_jiffies[handle]) > 10 * HZ) {
		last_jiffies[handle] = jiffies;
		/*--- printk("%s[%d]: len=%d %*B\n", __func__, handle, datalen, datalen, data); ---*/
		display_generic_stat("capi-datalen", handle, &datalen_stat[handle], 1, 1);
	}
	generic_stat(&datalen_stat[handle], datalen);
#endif
	do_gettimeofday(&tv);

	while (datalen >= sizeof(struct _capimagic_timestamp)) {
		struct _capimagic_timestamp *pcmts = (struct _capimagic_timestamp *)data;

		copy_dword_to_le_unaligned(&(pcmts->tsmstart), CAPI_MAGIC_TIMESTAMP_START);
		copy_dword_to_le_unaligned(&(pcmts->tv.tv_sec), tv.tv_sec);
		copy_dword_to_le_unaligned(&(pcmts->tv.tv_usec), tv.tv_usec);
		copy_dword_to_le_unaligned(&(pcmts->bytenmb), bytenmbcounter[handle]);
		copy_dword_to_le_unaligned(&(pcmts->handle),  handle);
		copy_dword_to_le_unaligned(&(pcmts->tsmend), CAPI_MAGIC_TIMESTAMP_END);
		if (datalen < TS_FRAGMENT_OFFSET) {
			bytenmbcounter[handle] += datalen;
			break;
		}
		datalen -= TS_FRAGMENT_OFFSET, data += TS_FRAGMENT_OFFSET;
		bytenmbcounter[handle] += TS_FRAGMENT_OFFSET;
	}
}
EXPORT_SYMBOL(capi_generate_timestamp);
/**
 */
void capi_parse_timestamp(unsigned int handle, char *name, unsigned char *data, unsigned int datalen)
{
	static int last_jiffies[MAX_STAT_INSTANCE];
	static struct _generic_stat timestamp_stat[MAX_STAT_INSTANCE];
	static struct _generic_stat bytenmbstat[MAX_STAT_INSTANCE];
	unsigned int bytenmb, blkhandle;

	if (handle >= MAX_STAT_INSTANCE) {
		return;
	}
	if (last_jiffies[handle] == 0) {
		init_generic_stat(&timestamp_stat[handle]);
		init_generic_stat(&bytenmbstat[handle]);
		last_jiffies[handle] = jiffies;
	}
	if ((jiffies - last_jiffies[handle]) > 10 * HZ) {
		last_jiffies[handle] = jiffies;
		display_generic_stat(name ? name : "capi-latency", handle, &timestamp_stat[handle], 2, 1);
		display_generic_stat("byte-latency", handle,  &bytenmbstat[handle], 1, 1);
		/*--- printk("%s:%d: %s: len=%d %*B\n", __func__, handle, name, datalen, datalen, data); ---*/
	}
	while (datalen >= sizeof(struct _capimagic_timestamp)) {
		struct timeval tvts;
		unsigned int parsed;
		struct _capimagic_timestamp *pcmts = (struct _capimagic_timestamp *)data;

		if ((extract_le_unaligned_dword(&(pcmts->tsmstart)) == CAPI_MAGIC_TIMESTAMP_START) &&
		   (extract_le_unaligned_dword(&(pcmts->tsmend))   == CAPI_MAGIC_TIMESTAMP_END)) {
			tvts.tv_sec  = extract_le_unaligned_dword(&(pcmts->tv.tv_sec));
			tvts.tv_usec = extract_le_unaligned_dword(&(pcmts->tv.tv_usec));
			bytenmb      = extract_le_unaligned_dword(&(pcmts->bytenmb));
			blkhandle    = extract_le_unaligned_dword(&(pcmts->handle));
			parsed = 1;
		} else if ((extract_le_unaligned_dword(&(pcmts->tsmstart)) == CAPI_MAGIC_TIMESTAMP_START_HOST) &&
				  (extract_le_unaligned_dword(&(pcmts->tsmend))   == CAPI_MAGIC_TIMESTAMP_END_HOST)) {
			char buf[4];
			/*--- die Daten liegen schon in 16 Bit-Hostformat vor: -> 16 Bit-Werte wieder in LE ---*/
			unsigned char *pdata = data;

			pdata = (unsigned char *)&(pcmts->tv.tv_sec);
			buf[0] = pdata[1], buf[1] = pdata[0], buf[2] = pdata[3], buf[3] = pdata[2];
			tvts.tv_sec  = extract_le_unaligned_dword(&buf);
			pdata = (unsigned char *)&(pcmts->tv.tv_usec);
			buf[0] = pdata[1], buf[1] = pdata[0], buf[2] = pdata[3], buf[3] = pdata[2];
			tvts.tv_usec = extract_le_unaligned_dword(&buf);
			pdata = (unsigned char *)&(pcmts->bytenmb);
			buf[0] = pdata[1], buf[1] = pdata[0], buf[2] = pdata[3], buf[3] = pdata[2];
			bytenmb = extract_le_unaligned_dword(&buf);
			pdata = (unsigned char *)&(pcmts->handle);
			buf[0] = pdata[1], buf[1] = pdata[0], buf[2] = pdata[3], buf[3] = pdata[2];
			blkhandle = extract_le_unaligned_dword(&buf);
			parsed = 1;
		} else {
			datalen--, data++;
			parsed = 0;
		}
		if (parsed) {
			struct timeval tvact;
			long long te, ta;
			unsigned long tdiff;

			do_gettimeofday(&tvact);
			ta    = ((long long)tvts.tv_sec  * (long long)(1000U * 1000U)) + (long long)tvts.tv_usec;
			te    = ((long long)tvact.tv_sec * (long long)(1000U * 1000U)) + (long long)tvact.tv_usec;
			tdiff = (unsigned long) ((te - ta)); /*--- in msec ---*/
			generic_stat(&timestamp_stat[handle], tdiff);
			/*--- memset(data, 0, sizeof(struct _capimagic_timestamp)); ---*/
			datalen -= sizeof(struct _capimagic_timestamp);
			data    += sizeof(struct _capimagic_timestamp);
			if (blkhandle < MAX_STAT_INSTANCE) {
				generic_stat(&bytenmbstat[handle], bytenmbcounter[blkhandle] - bytenmb);
			}
			return;
		}
	}
}
EXPORT_SYMBOL(capi_parse_timestamp);
/**
 * Timestamp durch Capicodec schleusen
 */
void capi_capicodec_timestamp(unsigned char *datain, unsigned char *dataout,
			      unsigned int datalenin, unsigned int datalenout)
{
	memset(dataout, 0, datalenout);
	while ((datalenin >= sizeof(struct _capimagic_timestamp)) &&
		  (datalenout >= sizeof(struct _capimagic_timestamp))) {
		struct _capimagic_timestamp *pcmts = (struct _capimagic_timestamp *)datain;

		if (((extract_le_unaligned_dword(&(pcmts->tsmstart)) == CAPI_MAGIC_TIMESTAMP_START) &&
			(extract_le_unaligned_dword(&(pcmts->tsmend))   == CAPI_MAGIC_TIMESTAMP_END)) ||
			((extract_le_unaligned_dword(&(pcmts->tsmstart)) == CAPI_MAGIC_TIMESTAMP_START_HOST) &&
			(extract_le_unaligned_dword(&(pcmts->tsmend))   == CAPI_MAGIC_TIMESTAMP_END_HOST))) {

			memcpy(dataout, datain, sizeof(struct _capimagic_timestamp));
			dataout     += sizeof(struct _capimagic_timestamp);
			datain      += sizeof(struct _capimagic_timestamp);
			datalenin   -= sizeof(struct _capimagic_timestamp);
			datalenout  -= sizeof(struct _capimagic_timestamp);
		} else {
			datalenin--, datain++;
		}
	}
}
EXPORT_SYMBOL(capi_capicodec_timestamp);
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
#define CAPIMSG_SIZE (sizeof(struct __packed _capi_message))
/**
 * CapiTrace
 */
static ssize_t capi_oslib_write(struct file *filp, const char __user *write_buffer, size_t write_length,
				loff_t *write_pos __attribute__((unused)))
{
	unsigned int status;
	unsigned char *data;
	/*--- unsigned int data_length; ---*/
	struct _capi_oslib_open_data *open_data = (struct _capi_oslib_open_data *)filp->private_data;
	struct __packed _capi_message_header *header;
	unsigned int rest_len;

	if (open_data->ApplId == 0) {
		DEB_ERR("[write]%s not registered\n", current->comm);
		open_data->last_error = ERR_IllegalApplId;
		return -EIO;
	}
	/*--- DEB_INFO("[write]%s write_length = %u *write_pos = 0x%LX\n", , current->comm, write_length, *write_pos); ---*/
	/*--- DEB_ERR("[write]%s start write_length=%u\n", current->comm, write_length); ---*/
	data          = open_data->put_message_buffer;
	write_length -= copy_from_user(data, write_buffer, min(write_length, CAPIMSG_SIZE));
	rest_len      = (write_length >  CAPIMSG_SIZE) ? write_length - CAPIMSG_SIZE : 0;
	header = (struct __packed _capi_message_header *)data;

	/*--- DEB_ERR("[write]%s msg readed write_length=%u rest_len=%u header-length=%u\n", current->comm, write_length, rest_len, copy_word_from_le_aligned((unsigned char *)&header->Length)); ---*/
	if (write_length < sizeof(struct __packed _capi_message_header)) {
		DEB_ERR("[write]%s write_lengh < %zu\n", current->comm, sizeof(struct __packed _capi_message_header));
		open_data->last_error = ERR_IllegalMessage;
		return -EIO;
	}
	if (sizeof(*header) != 8) {
		DEB_ERR("[write]%s capi header should be %d is %zd\n", current->comm, 8, sizeof(*header));
	}
	if (down_interruptible(&capi_oslib_sema)) {
		DEB_ERR("[write]%s down_interruptible() failed\n", current->comm);
		open_data->last_error = ERR_OS_Resource;
		return -ERESTARTSYS;
	}

	/**
	 * AUSWERTEN der WRITE DATA  CAPI Header ist gelesen
	 */
	copy_word_to_le_unaligned((unsigned char *)&(header->ApplId), open_data->ApplId);

	/*--- DEB_INFO("[write]%s %s\n", current->comm, CAPI_MESSAGE_NAME(header->Command, header->SubCommand)); ---*/

	if (open_data->mode == SOURCE_PTR_CAPI)
		capi_oslib_map_addr(open_data, (void *)data);
	if (open_data->mode == SOURCE_DEV_CAPI) {
		struct __packed _capi_message *C = (struct __packed _capi_message *)data;
		if (CA_IS_DATA_B3_REQ(data)) {
			unsigned int b3datalen = copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen);
			unsigned char *b3data = LOCAL_CAPI_NEW_DATA_B3_REQ_BUFFER(open_data->mode, open_data->ApplId, copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.NCCI));

			if (b3data == NULL) {
				DEB_ERR("[write]%s  data b3 buffer overflow\n", current->comm);
				open_data->last_error = ERR_QueueFull;
				up(&capi_oslib_sema);
				return -EIO;
			}
			if (b3datalen > open_data->AllocB3BlockSize) {
				DEB_ERR("[write]%s data b3 buffer too small\n", current->comm);
				open_data->last_error = ERR_MessageLost;
				up(&capi_oslib_sema);
				return -EIO;
			}
			if (copy_from_user(b3data, write_buffer + copy_word_from_le_aligned((unsigned char *)&header->Length), b3datalen)) {
				DEB_ERR("[write]%s copy_from_user failed\n", current->comm);
				open_data->last_error = ERR_MessageLost;
				up(&capi_oslib_sema);
				return -EIO;
			}
#if defined(CAPIOSLIB_CHECK_LATENCY)
			/*--- capi_generate_timestamp(0x20 + (open_data->ApplId & 0x1F), b3data, b3datalen); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
			copy_data_b3_pointer_to_le_aligned(C->capi_message_part.data_b3_req, b3data);
			rest_len = 0;
			/*--- DEB_INFO("[b3_req] len=%u h=0x%x\n", b3datalen, C->capi_message_part.data_b3_req.Handle); ---*/
		/*--- } else if(CA_IS_DATA_B3_RESP(data)) { ---*/
			/*--- DEB_INFO("[b3_resp] h=0x%x\n", copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_resp.Handle)); ---*/
		}
	}
	if (rest_len) {
		/*--- DEB_ERR("[write]%s restlen exist %u CAPIMSG_SIZE=%u\n", current->comm, rest_len, CAPIMSG_SIZE); ---*/
		if (rest_len < sizeof(open_data->put_message_buffer) - CAPIMSG_SIZE) {
			if (copy_from_user(data + CAPIMSG_SIZE, write_buffer + CAPIMSG_SIZE, rest_len)) {
				DEB_ERR("[write]%s copy_from_user failed\n", current->comm);
				open_data->last_error = ERR_MessageLost;
				up(&capi_oslib_sema);
				return -EIO;
			}
		} else {
			DEB_ERR("[write]%s buffer to small %u\n", current->comm, rest_len);
			open_data->last_error = ERR_MessageLost;
			up(&capi_oslib_sema);
			return -EIO;
		}
	}
	status = LOCAL_CAPI_PUT_MESSAGE(open_data->mode, open_data->ApplId, (unsigned char *)data);

	up(&capi_oslib_sema);

	if (status) {
		DEB_ERR("[write]%s CAPI_PUT_MESSAGE failed error 0x%x\n", current->comm, status);
		open_data->last_error = status;
		return -EIO;
	}
	return write_length;
}

/**
 */
static ssize_t capi_oslib_read(struct file *filp, char __user *read_buffer, size_t max_read_length, loff_t *read_pos)
{
	int status;
	unsigned int copy_length = 0;
	unsigned int rx_buffer_length = 0;
	struct _capi_oslib_open_data *open_data = (struct _capi_oslib_open_data *)filp->private_data;
	struct __packed _capi_message *M;
	unsigned char *rx_buffer = open_data->get_message_buffer;

	/*--- DEB_INFO("capi_oslib_read: (%s)\n", current->comm); ---*/

	if (open_data->ApplId == 0) {
		DEB_ERR("[read]%s not registered\n", current->comm);
		return capi_oslib_dump_open_data(filp, rx_buffer, max_read_length, read_pos);
		/*--- return -EFAULT; ---*/
	}

capi_oslib_read_retry:
	if (down_interruptible(&capi_oslib_sema)) {
		DEB_ERR("[read]%s down_interruptible() failed\n", current->comm);
		return -ERESTARTSYS;
	}
	status = LOCAL_CAPI_GET_MESSAGE(open_data->mode, open_data->ApplId, (unsigned char **)&rx_buffer, CAPI_NO_SUSPEND);
	switch (status) {
	case ERR_QueueEmpty:
		rx_buffer_length = 0;
		break;
	case 0:
		M = (struct __packed _capi_message *)(rx_buffer);
		rx_buffer_length = copy_word_from_le_unaligned((unsigned char *)&(M->capi_message_header.Length));
		/*--- printk(KERN_ERR "[read] rx_buffer 0x%x 0x%x rx_buffer_length = 0x%x\n", rx_buffer[0], rx_buffer[1], rx_buffer_length); ---*/
		if (open_data->mode == SOURCE_PTR_CAPI)
			capi_oslib_map_addr(open_data, rx_buffer);
		if (open_data->mode == SOURCE_DEV_CAPI) {
			struct __packed _capi_message *C = (struct __packed _capi_message *)rx_buffer;

			if (CA_IS_DATA_B3_IND(rx_buffer)) {
				unsigned char *data = copy_data_b3_pointer_from_le_aligned(C->capi_message_part.data_b3_ind);
				unsigned int datalen = copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen);

#if defined(CAPIOSLIB_CHECK_LATENCY)
				/*--- capi_parse_timestamp(0x20 + (open_data->ApplId & 0x1F), (char *)__func__, data, datalen); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
				/*--- DEB_INFO("[b3_ind] data=0x%x len=%u h=0x%x\n", data, datalen, copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.Handle)); ---*/
				if (rx_buffer_length + datalen <= sizeof(open_data->get_message_buffer)) {
					/*--- DEB_ERR("[read]%s rx_buffer_length=%u datalen=%u max_read_length=%u\n", current->comm, rx_buffer_length, datalen, max_read_length); ---*/
					memcpy(rx_buffer + rx_buffer_length, data, datalen);
					rx_buffer_length += datalen;
				} else {
					DEB_ERR("[read]%s buffer too short: rx_buffer_length=%u datalen=%u max_read_length=%zu\n", current->comm, rx_buffer_length, datalen, max_read_length);
					open_data->last_error = ERR_MessageToSmall;
					up(&capi_oslib_sema);
					return -EIO;
				}
			} else if (CA_IS_DATA_B3_CONF(rx_buffer)) {
				/*--- DEB_INFO("[b3_conf] h=0x%x\n", copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_conf.Handle)); ---*/
			} else {
				DEB_INFO("[read] %s(ApplID=%d MsgNr=%d)\n", CAPI_MESSAGE_NAME(M->capi_message_header.Command, M->capi_message_header.SubCommand), open_data->ApplId, extract_le_aligned_word(&M->capi_message_header.MessageNr));
			}
		}
		break;
	default:
		open_data->last_error = status;
		up(&capi_oslib_sema);
		return -EIO;
	}
	/**
	 * sind überhaupt Daten vorhanden
	 */
	if (rx_buffer_length) {
		/*--- DEB_INFO("[read] rx_buffer_length = %u *read_pos = %Lu\n", rx_buffer_length, *read_pos); ---*/
		copy_length = rx_buffer_length;
		if (copy_length > max_read_length) {
			DEB_ERR("[read]%s read buffer too small\n", current->comm);
			copy_length = max_read_length;
		}
	/**
	 * sind wir blockierend, nein
	 */
	} else if (filp->f_flags & O_NONBLOCK) {
		up(&capi_oslib_sema);
		/*--- DEB_INFO("[read] non block, empty\n"); ---*/
		return -EAGAIN;
	/**
	 * sind wir blockierend, ja
	 */
	} else {
		up(&capi_oslib_sema);
		DEB_INFO("[read]%s sleep on\n", current->comm);
		if (wait_event_interruptible(open_data->wait_queue,
		    open_data->read_pipe->WritePos == open_data->read_pipe->ReadPos)) {
			DEB_INFO("[read]%s handle released\n", current->comm);
			return -ERESTARTSYS;
		}
#if defined(CAPIOSLIB_CHECK_LATENCY)
		/*--- capi_check_latency(open_data->ApplId & 0x3F, (char *)__func__, 0); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
		DEB_INFO("[read]%s wake up\n", current->comm);
		goto capi_oslib_read_retry;
	}

	if (copy_to_user(read_buffer, rx_buffer, copy_length)) {
		up(&capi_oslib_sema);
		DEB_ERR("[read]%s copy_to_user failed\n", current->comm);
		return -EFAULT;
	}
	*read_pos = (loff_t)0;
	up(&capi_oslib_sema);
	return copy_length;
}

/**
 */
static int capi_oslib_capi_register(struct _capi_oslib_open_data *open_data, unsigned char *Buffer,
				    unsigned int BufferSize, unsigned int MessageBufferSize,
				    unsigned int MaxNCCIs, unsigned int WindowSize, unsigned int B3BlockSize)
{
	unsigned int status;

	if (open_data->ApplId) {
		DEB_ERR("[register]%s already registered (ApplId=%u) !\n", current->comm, open_data->ApplId);
		return ERR_ResourceError;
	}
reregister:
	status = LOCAL_CAPI_REGISTER(open_data->mode, MessageBufferSize, MaxNCCIs, WindowSize, B3BlockSize, &(open_data->ApplId));
	if ((status == ERR_NoError) && (open_data->mode == SOURCE_PTR_CAPI)) {
		if (capi_oslib_register_user_space_blocks(open_data, Buffer, BufferSize, MaxNCCIs, WindowSize, B3BlockSize)) {
			DEB_WARN("[register] change from SOURCE_PTR_CAPI to SOURCE_DEV_CAPI mode\n");
			LOCAL_CAPI_RELEASE(open_data->mode, open_data->ApplId);
			open_data->mode = SOURCE_DEV_CAPI;
			goto reregister;
		}
	}
	open_data->read_pipe = LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE(open_data->mode, open_data->ApplId, &(open_data->wait_queue), NULL);

	open_data->B3BlockSize        = B3BlockSize;
	open_data->B3WindowSize       = WindowSize;
	open_data->MaxNCCIs           = MaxNCCIs;
	open_data->MessageBufferSize  = MessageBufferSize;
	open_data->AllocB3BlockSize   = max_t(unsigned int, B3BlockSize, B3_DATA_ALLOC_SIZE);

	capi_oslib_register_open_data(open_data);

	DEB_INFO("[register] ApplId=%u status=0x%x (%s)\n", open_data->ApplId, status, current->comm);
	return status;
}

/**
 */
static int capi_oslib_capi_release(struct _capi_oslib_open_data *open_data)
{
	unsigned int status;

	capi_oslib_release_open_data(open_data);

	DEB_INFO("[close] ApplId=%d\n", open_data->ApplId);
	status = LOCAL_CAPI_RELEASE(open_data->mode, open_data->ApplId);
	open_data->mode = SOURCE_UNKNOWN;
	return status;
}

/**
 */
long capi_oslib_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct _capi_oslib_open_data *open_data = (struct _capi_oslib_open_data *)filp->private_data;
	int status = 0, ret = 0;

	capi_ioctl_struct capi_ioctl_struct;

	/*--- unsigned int dir  = _IOC_DIR(cmd); ---*/
	unsigned int type = _IOC_TYPE(cmd);
	unsigned int nr   = _IOC_NR(cmd);
	unsigned int size = _IOC_SIZE(cmd);

	/*--- DEB_ERR("[ioctl] cmd: 0x%04x\n", cmd); ---*/
	/*--- DEB_ERR("[ioctl] args: %d\n", args); ---*/
	/*--- DEB_ERR("[ioctl] type: 0x%x\n", type); ---*/
	/*--- DEB_ERR("[ioctl] nr: 0x%x\n", nr); ---*/
	/*--- DEB_ERR("[ioctl] size: %d\n", size); ---*/

	if (type != 'C') {
		DEB_ERR("[ioctl] type not 'C', was type %d, nr %d, size %d from %s\n", type, nr, size, current->comm);
		return -EFAULT;
	}

	if (size > sizeof(capi_ioctl_struct)) {
		DEB_ERR("[ioctl] size invalid, was %d from %s\n", size, current->comm);
		return -EFAULT;
	}

	switch (nr) {
	case 0x01: /*--- #define CAPI_REGISTER	_IOW('C',0x01,struct capi_register_params) ---*/
		{
			capi_register_params *p_capi_register_params = &capi_ioctl_struct.rparams;
			struct _extended_register {
				unsigned char *b3_buffer;
				unsigned int b3_buffer_len;
			} *extended_register = (struct _extended_register *)(&p_capi_register_params[1]);
			unsigned int b3_buffer_len;
			unsigned char *b3_buffer;
			short *p_Error = &capi_ioctl_struct.errcode;

			if (args) {
				if (copy_from_user(&capi_ioctl_struct, (void *)args, size)) {
					DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__,
						current->comm);
					return -EFAULT;
				}
			} else {
				DEB_ERR("[ioctl] args null for ioctl nr %d, size %d from %s\n",
					nr, size, current->comm);
				return -EFAULT;
			}
			DEB_INFO("[register] size=%u sizeof(capi_register_params)=%zu sizeof(struct _extended_register)=%zu\n",
				 size, sizeof(capi_register_params), sizeof(struct _extended_register));

			if (size < sizeof(capi_register_params) + sizeof(struct _extended_register)) {
				b3_buffer       = NULL;
				b3_buffer_len   = 0;
				open_data->mode = SOURCE_DEV_CAPI;
				DEB_WARN("no user space b3_buffer\n");
			} else {
				b3_buffer       = extended_register->b3_buffer;
				b3_buffer_len   = extended_register->b3_buffer_len;
				open_data->mode = SOURCE_PTR_CAPI;
				DEB_INFO("user space b3_buffer = 0x%p len=%u\n", b3_buffer, b3_buffer_len);
				if (b3_buffer_len == 0) {
					open_data->last_error = ERR_OS_Resource;
					DEB_ERR("[capi_register] failed status=%d no user space buffer length\n", status);
					return -EIO;
				} else {
					int Len;

					Len = capi_oslib_get_data_b3_ind_buffer_size(p_capi_register_params->level3cnt,
										     p_capi_register_params->datablklen,
										     p_capi_register_params->datablkcnt);
					if (Len > (int)b3_buffer_len) {
						DEB_ERR("[capi_register] failed user space buffer too small (should be %u is %u)\n",
							Len, b3_buffer_len);
						open_data->last_error = ERR_OS_Resource;
						return -EIO;
					}
				}
			}

			status = capi_oslib_capi_register(open_data,
							  b3_buffer, b3_buffer_len,
							  1024 + (1024 * p_capi_register_params->level3cnt) /* MessageBufferSize */,
							  p_capi_register_params->level3cnt  /* MaxNCCIs */,
							  p_capi_register_params->datablkcnt /* WindowSize */,
							  p_capi_register_params->datablklen /* B3BlockSize */);

			if (status) {
				open_data->last_error = status;
				DEB_ERR("capi_register failed status=%d\n", status);
				return -EIO;
			}
			*p_Error = status;
			size = sizeof(*p_Error);
			ret = open_data->ApplId;
		}
		break;
	case 0x06: /*--- #define CAPI_GET_MANUFACTURER	_IOWR('C',0x06,int) ---*//* broken: wanted size 64 (CAPI_MANUFACTURER_LEN) */
		{
			char *p_Manufacturer = (char *)&capi_ioctl_struct.manufacturer;

			if (args) {
				if (copy_from_user(&capi_ioctl_struct, (void *)args, size)) {
					DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__, current->comm);
					return -EFAULT;
				}
			} else {
				DEB_ERR("[ioctl] args null for ioctl nr %d, size %d from %s\n", nr, size, current->comm);
				return -EFAULT;
			}
			CAPI_GET_MANUFACTURER(p_Manufacturer);
			size = strlen(p_Manufacturer);
		}
		break;
	case 0x07: /*--- #define CAPI_GET_VERSION	_IOWR('C',0x07,struct capi_version) ---*/
		{
			capi_version *p_capi_version = &capi_ioctl_struct.version;

			if (args) {
				if (copy_from_user(&capi_ioctl_struct, (void *)args, size)) {
					DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__, current->comm);
					return -EFAULT;
				}
			} else {
				DEB_ERR("[ioctl] args null for ioctl nr %d, size %d from %s\n", nr, size, current->comm);
				return -EFAULT;
			}
			status = CAPI_GET_VERSION(&p_capi_version->majorversion,
						  &p_capi_version->minorversion,
						  &p_capi_version->majormanuversion,
						  &p_capi_version->minormanuversion);
			if (status) {
				open_data->last_error = status;
				DEB_ERR("capi_get_version failed status=%d\n", status);
				return -EIO;
			}
			size = sizeof(capi_version);
		}
		break;
	case 0x08: /*--- #define CAPI_GET_SERIAL _IOWR('C',0x08,int) ---*/ /* broken: wanted size 8 (CAPI_SERIAL_LEN) */
		{
			char *p_Serial = (char *)&capi_ioctl_struct.serial;

			if (args) {
				if (copy_from_user(&capi_ioctl_struct, (void *)args, size)) {
					DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__, current->comm);
					return -EFAULT;
				}
			} else {
				DEB_ERR("[ioctl] args null for ioctl nr %d, size %d from %s\n", nr, size, current->comm);
				return -EFAULT;
			}
			/*--- DEB_ERR("[getserial] %4B\n", p_Serial); ---*/
			CAPI_GET_SERIAL_NUMBER(capi_ioctl_struct.contr, p_Serial);
			size = strlen(p_Serial);
			/*--- DEB_ERR("[getserial] %*B\n", size, p_Serial); ---*/
		}
		break;
	case 0x09: /*--- #define CAPI_GET_PROFILE	_IOWR('C',0x09,struct capi_profile) ---*/
		{
			capi_profile *p_capi_profile = &capi_ioctl_struct.profile;
			unsigned char Buffer[64];

			if (args) {
				if (copy_from_user(&capi_ioctl_struct, (void *)args, size)) {
					DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__, current->comm);
					return -EFAULT;
				}
			} else {
				DEB_ERR("[ioctl] args null for ioctl nr %d, size %d from %s\n", nr, size, current->comm);
				return -EFAULT;
			}
			/*--- DEB_ERR("[getprofile]Cntrl=%d '%*B' %p %p\n", capi_ioctl_struct.contr, size, &capi_ioctl_struct, &capi_ioctl_struct, &capi_ioctl_struct.contr); ---*/
			size = 0;
			status = CAPI_GET_PROFILE(Buffer, capi_ioctl_struct.contr);
			if (status == 0) {
				if (capi_ioctl_struct.contr == 0)
					size = sizeof(unsigned int);
				else
					size = sizeof(capi_profile);
				memcpy(p_capi_profile, Buffer, size);
			} else {
				open_data->last_error = status;
				DEB_ERR("capi_get_profile failed status=%d\n", status);
				return -EIO;
			}
			/*--- DEB_ERR("[getprofile] '%*B'\n", size, p_capi_profile); ---*/
		}
		break;
	case 0x20: /*--- #define CAPI_MANUFACTURER_CMD	_IOWR('C',0x20, struct capi_manufacturer_cmd) ---*/
		{
			/*--- capi_manufacturer_cmd *p_capi_manufacturer_cmd = &capi_ioctl_struct.cmd; ---*/
			size = 0;
			open_data->last_error = 0;
			DEB_ERR("capi_get_manufacturer_cmd failed status=%d\n", status);
			return -EIO;
		}
		break;
	case 0x21: /*--- #define CAPI_GET_ERRCODE	_IOR('C',0x21, __u16) ---*/
		{
			short *p_Error = &capi_ioctl_struct.errcode;

			if (args) {
				if (copy_from_user(&capi_ioctl_struct, (void *)args, size)) {
					DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__, current->comm);
					return -EFAULT;
				}
			} else {
				DEB_ERR("[ioctl] args null for ioctl nr %d, size %d from %s\n", nr, size, current->comm);
				return -EFAULT;
			}
			/* der Fehlerwert bleibt in Hostorder */
			*p_Error = open_data->last_error;
			open_data->last_error = 0;
			DEB_INFO("collect last error=%d\n", open_data->last_error);
			size = sizeof(*p_Error);
		}
		break;
	case 0x22: /*--- #define CAPI_INSTALLED		_IOR('C',0x22, __u16) ---*/
		{
			size = 0;
			/*--- DEB_ERR("capi_installed\n"); ---*/
#if 0
			if (capi_ioctl_struct.contr == 0) {
				unsigned int controller;

				status = ERR_IllegalController;
				for (controller = 1 ; controller <= capi_oslib_stack->controllers ; controller++) {
					if (CAPI_INSTALLED(capi_ioctl_struct.contr) == ERR_NoError) {
						status = 0;
						break;
					}
				}
			} else {
				status = CAPI_INSTALLED(capi_ioctl_struct.contr);
			}
#else
			status = CAPI_INSTALLED(0);
#endif

			if (status != ERR_NoError) {
				open_data->last_error = status;
				DEB_ERR("capi_installed failed status=0x%x\n", status);
				return -EIO;
			}
		}
		break;
	case 0x23: /*--- #define CAPI_GET_FLAGS		_IOR('C',0x23, unsigned) ---*/
	case 0x24: /*--- #define CAPI_SET_FLAGS		_IOR('C',0x24, unsigned) ---*/
	case 0x25: /*--- #define CAPI_CLR_FLAGS		_IOR('C',0x25, unsigned) ---*/
	case 0x26: /*--- #define CAPI_NCCI_OPENCOUNT	_IOR('C',0x26, unsigned) ---*/
	case 0x27: /*--- #define CAPI_NCCI_GETUNIT	_IOR('C',0x27, unsigned) ---*/
	default:
		size = 0;
		DEB_ERR("[ioctl] nr 0x%x not supported\n", nr);
		return -EFAULT;
	}
	if (size) {
		if (copy_to_user((void *)args, &capi_ioctl_struct, size)) {
			DEB_ERR("[ioctl] %s:%d failed from %s\n", __func__, __LINE__, current->comm);
			ret =  -EFAULT;
		}
	}

	return ret;
}
