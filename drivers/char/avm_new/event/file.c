// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

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
#include <linux/device.h>

#include <avm/compat/misc.h>
#include <avm/event/event.h>
#include "internal.h"
#include "remote.h"
#include "struct/endian.h"
#include "struct/avm_event_gen_types.h"

#include <linux/cdev.h>

/**
 */
static int avm_event_open(struct inode *, struct file *);
static int avm_event_close(struct inode *, struct file *);
static int avm_event_fasync(int, struct file *, int);
static ssize_t avm_event_write(struct file *, const char *, size_t, loff_t *);
static ssize_t avm_event_read(struct file *, char *, size_t, loff_t *);
void avm_event_cleanup(void);
static unsigned int avm_event_poll(struct file *filp, poll_table *wait);

/*--- #define DEBUG_AVM_EVENT_FILE ---*/

#define DBG_ERR(args...) pr_err("[avm_event_file]" args)
#if defined(DEBUG_AVM_EVENT_FILE)
#define DBG_TRC(args...) pr_info("[avm_event_file]" args)
/**
 */
static void dump_data(const char *prefix, const unsigned char *data,
		      unsigned int len)
{
	pr_err("%s: data len=%d:", prefix, len);
	while (len--) {
		pr_cont("%02x,", *data++);
	}
	pr_cont("\n");
}
#else /*--- #if defined(DEBUG_AVM_EVENT_FILE) ---*/
#define DBG_TRC(args...) no_printk(args)
#define dump_data(a, b, c)
#endif
struct _avm_event avm_event;
struct semaphore avm_event_sema;

DEFINE_SPINLOCK(avm_event_lock);

/**
 */
struct file_operations avm_event_fops = {
	.owner   = THIS_MODULE,
	.open    = avm_event_open,
	.release = avm_event_close,
	.read    = avm_event_read,
	.write   = avm_event_write,
	/*--- .ioctl = avm_event_ioctl, ---*/
	.fasync  = avm_event_fasync,
	.poll    =  avm_event_poll,
};

/**
 */
int __init avm_event_init(void)
{
	int reason;

	DBG_TRC("%s: register_chrdev_region()\n", __func__);
	reason = alloc_chrdev_region(&avm_event.device, 0, 1, "avm_event");
	if (reason) {
		DBG_ERR("%s: register_chrdev_region failed: reason %d!\n",
			__func__, reason);
		return -ERESTARTSYS;
	}

	avm_event.cdev = cdev_alloc();
	if (!avm_event.cdev) {
		unregister_chrdev_region(avm_event.device, 1);
		DBG_ERR("%s: cdev_alloc failed!\n", __func__);
		return -ERESTARTSYS;
	}

	avm_event.cdev->owner = avm_event_fops.owner;
	avm_event.cdev->ops = &avm_event_fops;
	kobject_set_name(&(avm_event.cdev->kobj), "avm_event");
	spin_lock_init(&avm_event_lock);
	sema_init(&avm_event_sema, 1);

	avm_event_init2(512 /* max_items */, 512 /* max_datas */);

	if (cdev_add(avm_event.cdev, avm_event.device, 1)) {
		kobject_put(&avm_event.cdev->kobj);
		unregister_chrdev_region(avm_event.device, 1);
		DBG_ERR("%s: cdev_add failed!\n", __func__);
		return -ERESTARTSYS;
	}
	/*--- Geraetedatei anlegen: ---*/
	avm_event.osclass = class_create(THIS_MODULE, "avm_event");
	device_create(avm_event.osclass, NULL, avm_event.device, NULL, "avm_event");

	avm_event_proc_init();
	return 0;
}

/*--- module_init(avm_event_init); ---*/

/**
 */
#if defined(AVM_EVENT_MODULE)
void avm_event_cleanup(void)
{
	DBG_TRC("%s: unregister_chrdev(%u)\n", __func__, avm_event.major);
#if defined(CONFIG_AVM_PUSH_BUTTON)
	avm_event_push_button_deinit();
#endif /*--- #if defined(CONFIG_AVM_PUSH_BUTTON) ---*/
	avm_event_deinit2();
	device_destroy(avm_event.osclass, avm_event.device);
	class_destroy(avm_event.osclass);
	cdev_del(avm_event.cdev); /* Delete char device */
	avm_event_deinit2();
	unregister_chrdev_region(avm_event.device, 1);
}

/*--- module_exit(avm_event_cleanup); ---*/
#endif /*--- #if defined(AVM_EVENT_MODULE) ---*/

/**
 */
static int avm_event_fasync(int fd, struct file *filp, int mode)
{
	struct _avm_event_open_data *open_data =
		(struct _avm_event_open_data *)filp->private_data;

	DBG_TRC("%s\n", __func__);
	return fasync_helper(fd, filp, mode, &(open_data->fasync));
}

/**
 */
static unsigned int avm_event_poll(struct file *filp, poll_table *wait)
{
	struct _avm_event_open_data *open_data =
		(struct _avm_event_open_data *)filp->private_data;

	poll_wait(filp, &(open_data->wait_queue), wait);

	if (open_data->item) {
		DBG_TRC("%s: POLLIN (%s)\n", __func__, current->comm);
		return POLLIN | POLLRDNORM;
	}
	DBG_TRC("%s: (%s)\n", __func__, current->comm);

	return 0;
}

/**
 */
static int avm_event_open(struct inode *inode, struct file *filp)
{
	struct _avm_event_open_data *open_data;

	DBG_TRC("%s:\n", __func__);
	if (filp->f_flags & O_APPEND) {
		DBG_ERR("%s: open O_APPEND not supported\n", __func__);
		return -EFAULT;
	}
	if (down_interruptible(&avm_event_sema)) {
		DBG_ERR("%s: down_interruptible() failed\n", __func__);
		return -ERESTARTSYS;
	}
	open_data = kmalloc(sizeof(struct _avm_event_open_data), GFP_KERNEL);
	if (!open_data) {
		DBG_ERR("%s: open malloc failed\n", __func__);
		up(&avm_event_sema);
		return -EFAULT;
	}
	memset(open_data, 0, sizeof(*open_data));

	init_waitqueue_head(&(open_data->wait_queue));
	open_data->pf_owner = &(filp->f_owner);
	filp->private_data = (void *)open_data;

	up(&avm_event_sema);
	DBG_TRC("%s: open success flags=0x%x\n", __func__, filp->f_flags);
	return 0;
}

/**
 */
static int avm_event_close(struct inode *inode, struct file *filp)
{
	DBG_TRC("%s:\n", __func__);

	if (down_interruptible(&avm_event_sema)) {
		DBG_ERR("%s down_interruptible() failed\n", __func__);
		return -ERESTARTSYS;
	}
	/*--- achtung auf ind wartende "gefreien" und warten bis alle fertig ---*/

	if (filp->private_data) {
		struct _avm_event_open_data *open_data =
			(struct _avm_event_open_data *)filp->private_data;
		struct _avm_event_cmd_param_release avm_event_cmd_param_release;

		if (open_data->registered) {
			if (open_data->event_source_handle)
				avm_event_source_release(
					open_data->event_source_handle);
			open_data->event_source_handle = NULL;

			strcpy(avm_event_cmd_param_release.Name,
			       open_data->Name);
			(void)avm_event_release(open_data,
						&avm_event_cmd_param_release);
		}
		avm_event_fasync(
			-1, filp,
			0); /*--- remove this file from asynchonously notified filp ---*/
		kfree(filp->private_data);
		filp->private_data = NULL;
	}
	up(&avm_event_sema);
	return 0;
}

/**
 *
 * "Trigger" (call) notifier for event sources from user space.
 *
 * Runs only if .event_mask_registered & avm_event_id_user_source_notify is
 * set.
 */
void avm_event_source_user_mode_notify(void *Context, enum _avm_event_id id)
{
	struct _avm_event_open_data *O = (struct _avm_event_open_data *)Context;
	struct _avm_event_data *D;

	DBG_TRC("[%s]\n", __func__);

	if (check_id_mask_with_id(&O->event_mask_registered,
				  avm_event_id_user_source_notify)) {
		struct _avm_event_user_mode_source_notify *N;

		N = kmalloc(sizeof(struct _avm_event_user_mode_source_notify),
			    GFP_KERNEL);
		if (N == NULL) {
			DBG_ERR("[%s]: out of memory\n", __func__);
			return;
		}

		D = (struct _avm_event_data *)avm_event_alloc_data();
		if (D == NULL) {
			kfree(N);
			DBG_ERR("[%s]: out of memory\n", __func__);
			return;
		}
		N->header.id = avm_event_id_user_source_notify;
		N->id = id;
		atomic_set(&D->link_count, 0);
		D->data = N;
		D->data_length =
			sizeof(struct _avm_event_user_mode_source_notify);
		avm_event_source_trigger_one(O, D);
	}
}

/**
 */
static ssize_t avm_event_write(struct file *filp, const char *write_buffer,
			       size_t write_length,
			       loff_t *write_pos)
{
	unsigned int status;
	unsigned char *data;
	unsigned int data_length;
	struct _avm_event_cmd Buffer;
	struct _avm_event_open_data *open_data =
		(struct _avm_event_open_data *)filp->private_data;

	DBG_TRC("%s: write_length = %zu *write_pos = 0x%llx\n", __func__,
		write_length, *write_pos);

	if (write_length < sizeof(Buffer)) {
		DBG_ERR("%s: write_lengh < %zu\n", __func__, sizeof(Buffer));
		return -EINVAL;
	}
	if (copy_from_user(&Buffer, write_buffer, sizeof(Buffer))) {
		DBG_ERR("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}
	dump_data(__func__, (const char *)&Buffer, sizeof(Buffer));
	if ((Buffer.cmd != avm_event_cmd_register) &&
	    (open_data->registered == 0)) {
		DBG_ERR("%s: not registered\n", __func__);
		return -EFAULT;
	}
	if (down_interruptible(&avm_event_sema)) {
		DBG_ERR("%s down_interruptible() failed\n", __func__);
		return -ERESTARTSYS;
	}
	switch (Buffer.cmd) {
	case avm_event_cmd_register:
		DBG_TRC("%s: avm_event_cmd_register\n", __func__);
		status = avm_event_register(
			open_data, &Buffer.param.avm_event_cmd_param_register);
		if (status == 0)
			open_data->registered = 1;
		break;

	case avm_event_cmd_release:
		DBG_TRC("%s: avm_event_cmd_release\n", __func__);
		status = avm_event_release(
			open_data, &Buffer.param.avm_event_cmd_param_release);
		open_data->registered = 0;
		break;

	case avm_event_cmd_trigger:
		DBG_TRC("%s: avm_event_cmd_release\n", __func__);
		status = avm_event_trigger(
			open_data, &Buffer.param.avm_event_cmd_param_trigger);
		break;

	case avm_event_cmd_source_register:
		DBG_TRC("%s: avm_event_cmd_source_register(name='%s' mask[0]=%llx\n",
			__func__,
			Buffer.param.avm_event_cmd_param_source_register.Name,
			Buffer.param.avm_event_cmd_param_source_register.mask
				.mask[0]);
		if (open_data->event_source_handle) {
			status = -EACCES;
			break;
		}
		open_data->event_source_handle = avm_event_source_register(
			Buffer.param.avm_event_cmd_param_source_register.Name,
			&Buffer.param.avm_event_cmd_param_source_register.mask,
			avm_event_source_user_mode_notify, open_data);
		if (open_data->event_source_handle) {
			status = 0;
		} else {
			status = -EACCES;
		}
		break;

	case avm_event_cmd_source_release:
		DBG_TRC("%s: avm_event_cmd_source_release\n", __func__);
		status = 0;
		if (open_data->event_source_handle == NULL) {
			break;
		}
		avm_event_source_release(open_data->event_source_handle);
		open_data->event_source_handle = NULL;
		break;

	case avm_event_cmd_source_trigger:
		DBG_TRC("%s: avm_event_cmd_source_trigger id=%u data_length=%u\n",
			__func__,
			Buffer.param.avm_event_cmd_param_source_trigger.id,
			Buffer.param.avm_event_cmd_param_source_trigger
				.data_length);
		if (open_data->event_source_handle == NULL) {
			status = -EACCES;
			break;
		}
		data_length = Buffer.param.avm_event_cmd_param_source_trigger
				      .data_length;
		data = kmalloc(data_length, GFP_KERNEL);
		if (data == NULL) {
			status = -ENOMEM;
			break;
		}
		if (copy_from_user(data,
				   write_buffer + sizeof(struct _avm_event_cmd),
				   data_length)) {
			kfree(data);
			DBG_ERR("%s: copy_from_user failed\n", __func__);
			return -EFAULT;
		}
		avm_event_source_trigger(
			open_data->event_source_handle,
			Buffer.param.avm_event_cmd_param_source_trigger.id,
			data_length, data);
		status = 0;
		break;

	default:
	case avm_event_cmd_undef:
		DBG_ERR("%s: avm_event_cmd_undef\n", __func__);
		status = -EINVAL;
	}
	up(&avm_event_sema);
	if (status)
		return status;
	return write_length;
}

/**
 */
static ssize_t avm_event_read(struct file *filp, char *read_buffer,
			      size_t max_read_length, loff_t *read_pos)
{
	unsigned int event_pos;
	unsigned int copy_length = 0;
	unsigned int rx_buffer_length = 0;
	unsigned char *rx_buffer;
	unsigned int commit = 0;
	struct _avm_event_open_data *open_data =
		(struct _avm_event_open_data *)filp->private_data;

	DBG_TRC("%s\n", __func__);

avm_event_read_retry:
	if (down_interruptible(&avm_event_sema)) {
		DBG_ERR("%s: down_interruptible() failed\n", __func__);
		return -ERESTARTSYS;
	}
	avm_event_get(open_data, &rx_buffer, &rx_buffer_length, &event_pos);
	/**
	 * sind ueberhaupt Daten vorhanden
	 */
	if (rx_buffer_length) {
		DBG_TRC("%s: '%s' rx_buffer_length = %u *read_pos = %llu (%s)\n",
			__func__, open_data->Name, rx_buffer_length, *read_pos,
			current->comm);
		copy_length = rx_buffer_length - *read_pos;
		if (copy_length <= max_read_length) {
			commit = 1;
		} else {
			copy_length = max_read_length;
		}
	/**
	 * sind wir blockierend, nein
	 */
	} else if (filp->f_flags & O_NONBLOCK) {
		up(&avm_event_sema);
		DBG_TRC("%s: non block, empty\n", __func__);
		return -EAGAIN;
	/**
	 * sind wir blockierend, ja
	 */
	} else {
		up(&avm_event_sema);
		DBG_TRC("%s: sleep on\n", __func__);
		if (wait_event_interruptible(open_data->wait_queue,
					     open_data->item)) {
			DBG_TRC("%s: handle released\n", __func__);
			return -ERESTARTSYS;
		}
		DBG_TRC("%s: wake up\n", __func__);
		goto avm_event_read_retry;
	}

	if (copy_to_user(read_buffer, rx_buffer + *read_pos, copy_length)) {
		up(&avm_event_sema);
		DBG_ERR("%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}
	*read_pos += (loff_t)copy_length;
	if (commit) {
		*read_pos = (loff_t)0;
		avm_event_commit(open_data, event_pos);
	}
	up(&avm_event_sema);
	return copy_length;
}
