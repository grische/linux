// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/device.h>
#pragma GCC pop_options
#include <avm/watchdog/watchdog.h>
#include "avm_sammel.h"
#include "ar7wdt.h"

typedef unsigned int UINT32;
typedef int INT32;
#include <asm/mach_avm.h>
#if defined(CONFIG_MIPS)
#include <asm/prom.h>
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#define DEVICE_NAME "watchdog"

/*--- #define AVM_WATCHDOG_DEBUG ---*/

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(args...) pr_info(args)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(args...) no_printk(args)
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

extern int ar7wdt_no_reboot;

/**
 */
// clang-format off
static const struct _ar7wdt_cmd {
	char *cmd;
	int cmd_len;
	int (*funktion)(int handle, char *name, int len);
} ar7wdt_cmd[] = {
	{ .cmd = "start",      .cmd_len = sizeof("start") - 1,      .funktion = AVM_WATCHDOG_announce },
	{ .cmd = "register",   .cmd_len = sizeof("register") - 1,   .funktion = AVM_WATCHDOG_register },
	{ .cmd = "release",    .cmd_len = sizeof("release") - 1,    .funktion = AVM_WATCHDOG_release },
	{ .cmd = "timeout",    .cmd_len = sizeof("timeout") - 1,    .funktion = AVM_WATCHDOG_set_timeout },
	{ .cmd = "time",       .cmd_len = sizeof("time") - 1,       .funktion = AVM_WATCHDOG_set_timeout },
	{ .cmd = "trigger",    .cmd_len = sizeof("trigger") - 1,    .funktion = AVM_WATCHDOG_trigger },
	{ .cmd = "disable",    .cmd_len = sizeof("disable") - 1,    .funktion = AVM_WATCHDOG_disable },
	{ .cmd = "init-start", .cmd_len = sizeof("init-start") - 1, .funktion = AVM_WATCHDOG_init_start },
	{ .cmd = "init-done",  .cmd_len = sizeof("init-done") - 1,  .funktion = AVM_WATCHDOG_init_done },
	{ .cmd = NULL,         .cmd_len = 0,                        .funktion = NULL }
};
// clang-format on

/**
 */
static int ar7wdt_open(struct inode *, struct file *);
static int ar7wdt_release(struct inode *, struct file *);
static ssize_t ar7wdt_read(struct file *, char *, size_t, loff_t *);
static ssize_t ar7wdt_write(struct file *, const char *, size_t, loff_t *);
void ar7wdt_cleanup(void);
static int ar7wdt_fasync(int fd, struct file *filp, int mode);
static unsigned int ar7wdt_poll(struct file *filp, poll_table *wait);

/**
 */
const struct file_operations ar7wdt_fops = {
	.owner =   THIS_MODULE,
	.open =    ar7wdt_open,
	.release = ar7wdt_release,
	.read =    ar7wdt_read,
	.write =   ar7wdt_write,
	.fasync =  ar7wdt_fasync,
	.poll =    ar7wdt_poll,
};

/**
 */
static struct _avm_wdt {
	struct char_device_struct *device_region;
	dev_t                      device;
	struct cdev               *cdev;
	struct class              *osclass;
} avmwdt;

/**
 */
int ar7wdt_disable_watchdog(char *str)
{
	switch (*str) {
	case '0':
		ar7wdt_no_reboot = 0;
		return 0;
	case '1':
		ar7wdt_no_reboot = 1;
		return 0;
	case '2':
		ar7wdt_no_reboot = 2;
		return 0;
	}
	return 1;
}

__setup("ar7wdt_no_reboot=", ar7wdt_disable_watchdog);

/**
 */
int __init ar7wdt_init(void)
{
	int reason;

	pr_info("AVM_WATCHDOG: Watchdog Driver for AR7 Hardware (Version %s)\n",
		"1.0");
	if (ar7wdt_no_reboot) {
		ar7wdt_hw_secure_wdt_disable();
		if (ar7wdt_no_reboot == 2) {
			pr_err("hw+sw watchdog disabled\n");
			return 0;
		}
		pr_err("hw watchdog disabled\n");
	}
	reason = alloc_chrdev_region(&avmwdt.device, 0, 1, DEVICE_NAME);
	if (reason) {
		pr_err("[avmwdt] register_chrdev_region failed: reason %d!\n",
		       reason);
		return -ERESTARTSYS;
	}

	avmwdt.cdev = cdev_alloc();
	if (!avmwdt.cdev) {
		unregister_chrdev_region(avmwdt.device, 1);
		pr_err("[avmwdt]%s: cdev_alloc failed!\n", __func__);
		return -ERESTARTSYS;
	}

	avmwdt.cdev->owner = ar7wdt_fops.owner;
	avmwdt.cdev->ops = &ar7wdt_fops;
	kobject_set_name(&(avmwdt.cdev->kobj), DEVICE_NAME);

	if (cdev_add(avmwdt.cdev, avmwdt.device, 1)) {
		kobject_put(&avmwdt.cdev->kobj);
		unregister_chrdev_region(avmwdt.device, 1);
		pr_err("[avmwdt]%s: cdev_add failed!\n", __func__);
		return -ERESTARTSYS;
	}
	/*--- Geraetedatei anlegen: ---*/
	avmwdt.osclass = class_create(THIS_MODULE, DEVICE_NAME);
	device_create(avmwdt.osclass, NULL, 1, NULL, "%s%d", DEVICE_NAME, 0);

	AVM_WATCHDOG_init();
	return 0;
}

#if defined(CONFIG_AVM_WATCHDOG_MODULE)
/**
 */
void ar7wdt_cleanup(void)
{
	if (ar7wdt_no_reboot == 2) {
		pr_err("watchdog was disabled\n");
		return;
	}
	AVM_WATCHDOG_deinit();
	if (avmwdt.cdev) {
		device_destroy(avmwdt.osclass, 1);
		class_destroy(avmwdt.osclass);
		cdev_del(avmwdt.cdev); /* Delete char device */
		unregister_chrdev_region(avmwdt.device, 1);
	}
	ar7wdt_hw_deinit();
}
#endif /*--- #if defined(CONFIG_AVM_WATCHDOG_MODULE) ---*/

/**
 */
static int ar7wdt_fasync(int fd, struct file *filp, int mode)
{
	struct fasync_struct **fasync;

	DBG("%s: fd=%u\n", __func__, fd);
	fasync = AVM_WATCHDOG_get_fasync_ptr((int)filp->private_data);
	if (fasync)
		return fasync_helper(fd, filp, mode, fasync);
	return -EPERM;
}

/**
 */
static unsigned int ar7wdt_poll(struct file *filp, poll_table *wait)
{
	wait_queue_head_t *wait_queue;
	int poll;

	wait_queue = AVM_WATCHDOG_get_wait_queue((int)filp->private_data);
	if (wait_queue == NULL)
		return 0;

	poll_wait(filp, wait_queue, wait);

	poll = AVM_WATCHDOG_poll((int)filp->private_data);
	if (poll > 0) {
		/*--- DBG("%s: data avail\n", __func__); ---*/
		return POLLIN | POLLRDNORM;
	}
	/*--- DBG("%s: no data\n", __func__); ---*/
	return 0;
}

/**
 */
static int ar7wdt_open(struct inode *inode, struct file *filp)
{
	DBG("%s: always success\n", __func__);
	filp->private_data = 0;
	return 0;
}

/**
 */
static int ar7wdt_release(struct inode *inode, struct file *filp)
{
	DBG("%s: always success\n", __func__);
	if (filp->private_data) {
		AVM_WATCHDOG_ungraceful_release((int)filp->private_data);
		/*--- AVM_WATCHDOG_reboot((int)filp->private_data); ---*/
		return 0;
	}
	return 0;
}

/**
 */
static ssize_t ar7wdt_read(struct file *filp, char *read_buffer,
			   size_t max_read_length, loff_t *offp)
{
	char Buffer[64];
	int ret, len = 0;

	if ((int)filp->private_data == 0)
		return -EINVAL;

	for (;;) {
		DBG("%s:\n", __func__);

		ret = AVM_WATCHDOG_read((int)filp->private_data, Buffer,
					sizeof(Buffer));
		if (ret < 0) {
			break;
		}
		len = strlen(Buffer);
		if (len) {
			break;
		}
		if (filp->f_flags & O_NONBLOCK) {
			DBG("%s empty\n", __func__);
			return -EAGAIN;
		}
		if (AVM_WATCHDOG_wait_event_interruptible(
			    (int)filp->private_data) < 0) {
			DBG("%s: inval wait or got signal\n", __func__);
			break;
		}
	}
	len = min((int)max_read_length, len);

	if (len) {
		if (copy_to_user(read_buffer, Buffer, len)) {
			DBG("%s: copy_to_user failed len=%u\n", __func__, len);
			return (unsigned int)-EFAULT;
		}
	}
	DBG("%s: '%s' len=%u\n", __func__, Buffer, len);
	*offp += len;
	return len;
}

/**
 */
static ssize_t ar7wdt_write(struct file *filp, const char *write_buffer,
			    size_t write_length, loff_t *offp)
{
	char Buffer[128];
	unsigned int copy_count;
	const struct _ar7wdt_cmd *C = &ar7wdt_cmd[0];

	copy_count = min(write_length, sizeof(Buffer) - 1);
	if (copy_from_user(Buffer, write_buffer, copy_count)) {
		return (unsigned int)-EFAULT;
	}
	Buffer[copy_count] = '\0';

	DBG("%s: hdl=%d '%s'\n", __func__, (int)filp->private_data, Buffer);
	while (C->cmd) {
		if (write_length >= (size_t)C->cmd_len &&
		    !strncmp(C->cmd, Buffer, C->cmd_len) && C->funktion) {
			DBG("%s: call funktion for '%s'\n", __func__, C->cmd);
			filp->private_data =
				(void *)C->funktion((int)filp->private_data,
						    Buffer + C->cmd_len,
						    write_length - C->cmd_len);
			if ((int)filp->private_data < 0) {
				/*--- io error ---*/
				DBG("%s: error %d\n", __func__,
				    (int)filp->private_data);
				return (int)filp->private_data;
			}
			DBG("%s: success\n", __func__);
			break;
		}
		C++;
	}
	if (C->cmd == NULL) {
		/*--- io error ---*/
		DBG("%s: no support funktion\n", __func__);
		return -EBADRQC; /*--- invalid request code ---*/
	}
	*offp += write_length;
	return write_length;
}
