// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2025 AVM GmbH <fritzbox_info@avm.de> */

#define pr_fmt(fmt) "[arht:watchdog] " fmt

#include <asm-generic/irq_regs.h>
#include <asm/mach_avm.h>
#include <asm/system_misc.h>
#include <asm/tc3162/ecnt_timer.h>
#include <asm/tc3162/tc3162.h>
#include <avm_sammel.h>
#include <avm/enh/reboot_status.h>
#include <avm/rte/rte.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/nmi.h>
#include <linux/sched/debug.h>
#include <linux/types.h>
#include <avm/watchdog/watchdog.h>

#include "ar7wdt_private.h"

/**
 * WDT_BITE_TIME - The maximum bite time on a 7581 is 28 seconds. This is
 * determined by the factor "SYS_HCLK * 500 * TIMERTICKS_1S", which is
 * multiplied with the bite time. The time must respect the 32-Bit
 * limitation of the register.
 */
#define WDT_BITE_TIME (25)

/**
 * WDT_BARK_TIME - The WDT_DEFAULT_TIME defines the rate at which the
 * round robin method is used to kick the watchdog. Right now the bark
 * time of the watchdog is thrice of that rate. The bark time should stay
 * between the rate of round robin kicks and the bite time to leave
 * enough time for crash handling.
 */
#define WDT_BARK_TIME (WDT_DEFAULT_TIME * 3)

/**
 * wd_lock - Lock for watchdog resource accesses.
 */
static spinlock_t wd_lock;

/**
 * tc3162wdog_kick - Replacement for arch/arm64/mach-econet/tcwdog.c.
 * The method is called in various places, when a driver might mask
 * interrupts (including the timer interrupt) for a longer duration.
 * We do not want to take down the system in such a case, which is why
 * this method kicks the watchdog when it is the current CPU's turn.
 */
void tc3162wdog_kick(void)
{
	AVM_WATCHDOG_local_trigger();
}
EXPORT_SYMBOL(tc3162wdog_kick);

/**
 * wdog_kick_api - Replacement for arch/arm64/mach-econet/tcwdog.c.
 * See tc3162wdog_kick for information on why this method is defined here.
 */
void wdog_kick_api(int source)
{
	AVM_WATCHDOG_local_trigger();
}
EXPORT_SYMBOL(wdog_kick_api);

/**
 * arht_wdog_start - Start the watchdog. The watchdog must be regularly
 * kicked after this method was run.
 */
static void arht_wdog_start(void)
{
	unsigned long flags;

	__raw_rte_spin_lock_irqsave(&wd_lock, flags);

	set_wdogTimer_threshold(WDT_BARK_TIME * TIMERTICKS_1S * SYS_HCLK * 500);
	timerSet(3, WDT_BITE_TIME * TIMERTICKS_1S,
		 ENABLE, TIMER_HALTDISABLE);
	timer_WatchDogConfigure(ENABLE, ENABLE);

	__raw_rte_spin_unlock_irqrestore(&wd_lock, flags);
}

/**
 * arht_wdog_stop - Stop the watchdog.
 */
static void arht_wdog_stop(void)
{
	unsigned long flags;

	__raw_rte_spin_lock_irqsave(&wd_lock, flags);

	timer_WatchDogConfigure(DISABLE, DISABLE);

	__raw_rte_spin_unlock_irqrestore(&wd_lock, flags);
}

/**
 * arht_wdog_timeout - Called on watchdog timeout either via the watchdog's irq
 * handler or via an SDEI event that is hooked up to the watchog's irq.
 * Note, that the latter case happens in an NMI context.
 * @pregs: register state of the interrupted context
 */
static void arht_wdog_timeout(struct pt_regs *pregs)
{
	unsigned int cpu = get_cpu();

	/* Make sure the following prints do not get filtered */
	console_verbose();
	bust_spinlocks(1);

	/* Ensure that the hardware watchdog does not kill us */
	wdog_kick();

	/* Backtrace *other* CPUs (if possible) and flush printk_safe.
	 * Newer kernels no longer need such flushing.
	 */
	trigger_allbutcpu_cpu_backtrace(cpu);
	printk_safe_flush();

	avm_set_reset_status(RS_NMIWATCHDOG);
	die("Hardware-Watchdog", pregs, 0);

	while (1)
		;
}

void ar7wdt_hw_trigger(void)
{
	wdog_kick();
}

void ar7wdt_hw_deinit(void)
{
	arht_wdog_stop();
}

void ar7wdt_hw_init(void)
{
	__raw_rte_spin_lock_init(&wd_lock);
	arht_wdog_start();
	ecnt_timer_register_callback(arht_wdog_timeout, NULL);
}
