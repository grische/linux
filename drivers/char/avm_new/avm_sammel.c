// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
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
#include <linux/proc_fs.h>
#include <linux/threads.h> // für NR_CPUS

#include <linux/cdev.h>

#include <avm/watchdog/watchdog.h>
#include <avm/event/event.h>
#include "event/internal.h"

#if defined(CONFIG_MIPS)
#include <asm/prom.h>
#else
#include <asm/setup.h>
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#include "avm_sammel.h"
#include <avm/event/event.h>
#include "avm_debug.h"

#define MODULE_NAME "avm"
MODULE_DESCRIPTION("AR7 Watchdog Timer + AVM Central Event distribution");
MODULE_LICENSE("GPL");

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern void avm_checkpoint_init(void);
int __init avm_sammel_init(void)
{
	int ret __maybe_unused;
	/*--------------------------------------------------------------------------------------*\
	\*--------------------------------------------------------------------------------------*/
	printk("[avm] configured: "
#ifdef CONFIG_AVM_WATCHDOG
	       "watchdog "
#endif
#ifdef CONFIG_AVM_WATCHDOG_MODULE
	       "watchdog (module) "
#endif
#ifdef CONFIG_AVM_EVENT
	       "event "
#endif
#ifdef CONFIG_AVM_DEBUG
	       "debug "
#endif
#ifdef CONFIG_AVM_EVENT_MODULE
	       "event (module) "
#endif
	);
	printk("\n");
	/*--------------------------------------------------------------------------------------*\
	\*--------------------------------------------------------------------------------------*/

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
	ret = avm_event_init();
	if (ret) {
		printk("[avm]: avm_event_init: failed\n");
		return ret;
	}
	avm_factory_reset_init();
#endif /*--- #if (CONFIG_AVM_EVENT == 1) || (CONFIG_AVM_EVENT_MODULE == 1) ---*/
#if defined(CONFIG_AVM_DEBUG) || defined(CONFIG_AVM_DEBUG_MODULE)
	ret = avm_debug_init();
	if (ret) {
		printk("[avm]: avm_event_init: failed\n");
		return ret;
	}
#endif /*--- #if (CONFIG_AVM_DEBUG == 1) || (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
#if defined(CONFIG_AVM_EVENTNODE_PUMA7)
	avm_checkpoint_init();
#endif
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
module_init(avm_sammel_init);

#if defined(CONFIG_ARCH_IPQ40XX) || \
    defined(CONFIG_ARCH_IPQ5018) || \
    defined(CONFIG_ARCH_IPQ8074)
// Dakota: Funktioniert nicht - cycle counter werden irgendwo wieder ausgeschaltet?
/*--------------------------------------------------------------------------------*\
 * actually used for get_cycles()
\*--------------------------------------------------------------------------------*/
static void initialize_perfcounter(void *dummy)
{
#ifndef CONFIG_ARM64
	unsigned int val;

	/*---  initialize c9, Performance Monitor Control Register ---*/
	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(val)); /* PMNC-read */

	val |= 1; /* Enable all counters */
	val |= (1 << 3); /* counts every 64th processor clock cycle */
	/* count is enabled in regions where non-invasive debug is prohibited */
	val &= ~(1 << 5);

	isb();
	asm volatile("mcr p15, 0, %0, c9, c12, 0"
		     :
		     : "r"(val)); /* PMNC-write */

	/* Count Enable Set Register */
	/*--- asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(val)); ---*/
	val = 1 << 31; /* Enable cycle counter */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(val)); /* write */
#endif
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int __init avm_cycle_count_enable(void)
{
	on_each_cpu(initialize_perfcounter, NULL, 0);
	return 0;
}
arch_initcall(avm_cycle_count_enable);
#endif

/*--------------------------------------------------------------------------------*\
 * neccessary: early install the '/proc/avm' directory
\*--------------------------------------------------------------------------------*/
struct proc_dir_entry *proc_avm __attribute__((weak));

static int __init early_avm_proc_init(void)
{
	if (!proc_avm)
		proc_avm = proc_mkdir("avm", NULL);
	return 0;
}
fs_initcall(early_avm_proc_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(AVM_SAMMEL_MODULE)
int avm_sammel_deinit(void)
{
#if (CONFIG_AVM_EVENT_MODULE == 1)
	avm_event_cleanup();
#endif /*--- #if (CONFIG_AVM_EVENT_MODULE == 1) ---*/
#if (CONFIG_AVM_EVENT_MODULE == 1)
	avm_event_cleanup();
#endif /*--- #if (CONFIG_AVM_EVENT_MODULE == 1) ---*/
#if (CONFIG_AVM_DEBUG_MODULE == 1)
	avm_debug_cleanup();
#endif /*--- #if (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
module_exit(avm_sammel_deinit);
#endif /*--- #if defined(AVM_SAMMEL_MODULE) ---*/
