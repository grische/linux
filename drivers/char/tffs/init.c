// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <avm/tffs/tffs.h>
#include <linux/zlib.h>
#include <linux/version.h>
#include <linux/env.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include <linux/seq_file.h>

#include <avm/compat/misc.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include "local.h"
#include "nand_stat.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
//MODULE_PARM_DESC (tffs_mtd, "MTD device used for tiny flash file system with double buffering");
MODULE_DESCRIPTION("TFFS 'tiny flash file system with double buffering' driver version 2.2");
MODULE_LICENSE("GPL");
#define TFFS3_MODULE_VERSION "3.0"
MODULE_VERSION(TFFS3_MODULE_VERSION);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void tffs_cleanup(void);

struct tffs_cdev tffs;

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int tffs_thread(void *data)
{
	struct tffs_cdev *my_tffs;
	int result;
	struct tffs_core_handle *handle;

	my_tffs = (struct tffs_cdev *)data;

	my_tffs->thread_state = tffs3_thread_state_init;

	handle = TFFS3_Open(0, tffs3_mode_write);
	BUG_ON(IS_ERR_OR_NULL(handle));

	result = 0;
	do {
		pr_debug("[%s] waiting for events\n", __func__);
		my_tffs->thread_state = tffs3_thread_state_idle;
		if (wait_event_interruptible(my_tffs->event_wq,
					     test_and_clear_bit(TFFS_EVENT_BIT_TRIGGER,
								&my_tffs->pending_events))) {
			pr_err("[%s] interrupted while waiting for events, exiting\n", __func__);
			result = -EINTR;
			break;
		}

		my_tffs->thread_state = tffs3_thread_state_process;
		my_tffs->request_count++;

		/*--- auswerten des Events ---*/
		pr_debug("[%s] processing events 0x%08lx\n", __func__, my_tffs->pending_events);

		if (test_and_clear_bit(TFFS_EVENT_BIT_PANIC, &my_tffs->pending_events)) {
			pr_emerg("[%s] panic triggered\n", __func__);
			panic("Async panic triggered.\n");
		}

		if (kthread_should_stop() ||
		    test_and_clear_bit(TFFS_EVENT_BIT_CLEANUP, &my_tffs->pending_events)) {
			pr_debug("[%s] Cleanup\n", __func__);
			(void)TFFS3_Cleanup(handle);
		}

	} while (!kthread_should_stop());

	TFFS3_Close(handle);

	pr_info("[%s] event thread dead\n", __func__);

	my_tffs->thread_state = tffs3_thread_state_down;
	my_tffs->kthread      = NULL;

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void tffs_send_event(unsigned long event)
{
	unsigned int i;

	pr_debug("[%s] Called with event 0x%08lx\n", __func__, event);

	event &= ~TFFS_EVENT_TRIGGER;
	if (!event)
		return;

	for_each_set_bit(i, &event, BITS_PER_LONG)
		set_bit(i, &tffs.pending_events);
	set_bit(TFFS_EVENT_BIT_TRIGGER, &tffs.pending_events);

	// call wake.._sync so caller won't be scheduled out immediately
	// and gets a chance to release the tffs core mutex
	wake_up_interruptible_sync(&tffs.event_wq);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void tffs_init_cleanup(void)
{
#if defined(CONFIG_PROC_FS)
	tffs_proc_remove(&tffs);
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/

	device_destroy(tffs.osclass, tffs.device);
	device_destroy(tffs.osclass, MKDEV(MAJOR(tffs.device), FLASH_FS_ID_PANIC_LOG));
	device_destroy(tffs.osclass, MKDEV(MAJOR(tffs.device), FLASH_FS_ID_USER_LOG));
	class_destroy(tffs.osclass);

	if (tffs.cdev_ticfg) {
		kobject_put(&tffs.cdev_ticfg->kobj);
		cdev_del(tffs.cdev_ticfg);
	}

	if (tffs.cdev) {
		kobject_put(&tffs.cdev->kobj);
		cdev_del(tffs.cdev);
	}
	unregister_chrdev_region(tffs.device, 1);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/

static int __tffs_init tffs_do_init(void)
{
	unsigned int dummy __maybe_unused;
	int reason;

	memset(&tffs, 0x0, sizeof(tffs));

	tffs.device = MKDEV(0, 0);
	/*--- reason = register_chrdev_region(tffs.device, 1, "tffs"); ---*/
	reason = alloc_chrdev_region(&tffs.device, 0, 1, "tffs");
	if (reason < 0) {
		pr_err("[%s] register_chrdev_region failed: reason %d!\n", __func__, reason);
		return reason;
	}

	tffs.cdev_ticfg = 0;
	tffs.cdev	= cdev_alloc();
	if (!tffs.cdev) {
		pr_debug("[tffs] cdev_alloc failed!\n");
		tffs_cleanup();
		return -ENODEV;
	}

	tffs.cdev->owner = tffs_fops.owner;
	tffs.cdev->ops	 = &tffs_fops;
	kobject_set_name(&(tffs.cdev->kobj), "tffs0");

	if (cdev_add(tffs.cdev, tffs.device, 256)) {
		pr_debug("[tffs] cdev_add failed!\n");
		tffs_cleanup();
		tffs_init_cleanup();
		return -ENODEV;
	}

	init_waitqueue_head(&tffs.event_wq);

	if (TFFS3_Init()) {
		tffs_cleanup();
		tffs_init_cleanup();
		pr_debug("[tffs] TFFS_Init failed!\n");
		return -ENODEV;
	}

	pr_debug("[tffs] Character device init successful\n");

	tffs.thread_state = tffs3_thread_state_off;
	tffs.kthread	  = kthread_run(tffs_thread, (void *)&tffs, "tffsd");
	BUG_ON((tffs.kthread == NULL) || IS_ERR((void *)tffs.kthread));
	pr_info("TFFS: tiny flash file system driver. GPL (c) AVM Berlin (Version %s)\n",
		TFFS3_MODULE_VERSION);

#ifdef CONFIG_PROC_FS
	tffs_proc_init(&tffs);
#endif

	avm_urlader_env_init();

#if defined(CONFIG_TFFS_EFI_SYNC)
	/* Check for TFFS marker set by bootloader. If this is found, the *\
	\* eMMC TFFS is handled there directly, so no sync is needed.     */
	reason = avm_efivar_get_one("AVM_EVA_TFFS", (uint8_t *)&dummy, sizeof(dummy));
	if (reason < 0) {
		pr_info("[%s] Eva TFFS marker not found, syncing EFI to TFFS.\n", __func__);
		avm_prom_tffs_sync();
	}
#endif

	tffs.osclass = class_create(THIS_MODULE, "tffs");
	device_create(tffs.osclass, NULL, tffs.device, NULL, "tffs");
	device_create(tffs.osclass, NULL, MKDEV(MAJOR(tffs.device), FLASH_FS_ID_PANIC_LOG), NULL, "tffs_panic");
	device_create(tffs.osclass, NULL, MKDEV(MAJOR(tffs.device), FLASH_FS_ID_USER_LOG), NULL, "tffs_userlog");

	if (tffs_erase_stat_init())
		pr_err("[%s] Failed to init NAND erase_cnt stats\n", __func__);

	return 0;
}

#ifdef CONFIG_TFFS_AUTO_INIT
late_initcall(tffs_do_init);

int tffs_init(void)
{
	return 0;
}
#else
int tffs_init(void)
{
	return tffs_do_init();
}
#endif

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void tffs_cleanup(void)
{
}

module_exit(tffs_cleanup);
