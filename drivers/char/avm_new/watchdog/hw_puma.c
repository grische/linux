// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <avm/watchdog/watchdog.h>
#include <linux/cpumask.h>

#if defined(CONFIG_MACH_PUMA7) //PUMA7_ARM
#include <asm-arm/arch-avalanche/generic/pal.h>
#include <asm-arm/arch-avalanche/generic/pal_sysTimer16.h>
#include <mach/avm_fiq.h>
#else
#include <mach/hw_timer.h>
#include <arch-avalanche/generic/pal.h>
#endif
#include "avm_sammel.h"
#include <avm/enh/reboot_status.h>
#include "ar7wdt_private.h"

// #define AVM_WATCHDOG_DEBUG
#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(args...) pr_info(args)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(args...) no_printk(args)
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

/**
 */
int ar7wdt_hw_is_wdt_running(void)
{
	volatile PAL_SYS_WDTIMER_STRUCT_T *p_wdtimer =
		(volatile PAL_SYS_WDTIMER_STRUCT_T *)
			AVALANCHE_WATCHDOG_TIMER_BASE;

	return p_wdtimer->disable != 0;
}

/**
 * dummy
 */
void ar7wdt_hw_secure_wdt_disable(void)
{
}

#if defined(CONFIG_MACH_PUMA7)
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void die_with_mode_switch(const char *str, struct pt_regs *regs, int err)
{
#if defined(CONFIG_AVM_FIQ_PUMA7)
	static char *priv_str;
	static struct pt_regs *priv_regs;
	static int priv_err;

	priv_str = (char *)str;
	priv_regs = regs;
	priv_err = err;

	dsb();

	barrier();

	/* Mode-Umschaltung, um Memory-Classifier Auflösung zu ermöglichen. Im FIQ würden wir dabei abstürzen ... */
	AVM_CHANGE_MODE(SVC_MODE);

	barrier();

	dsb();
#else
#define priv_str str
#define priv_regs regs
#define priv_err err
#endif
	console_verbose();
	printk_avm_console_bend(0);
	pr_emerg("continuing in SVC\n");
	die(priv_str, priv_regs, priv_err);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static atomic_t wd_sync;
static atomic_t wd_count;

static irqreturn_t avm_wdt_bark (int irq, void *ref)
{
	struct pt_regs *orig_pregs;
	struct pt_regs pregs;

	int wd_counter = atomic_read(&wd_count);
	int wd_kicked  = atomic_read(&wd_sync);
	if ((wd_kicked == 0) && (wd_counter >= 20)) {
		// BARK HANDLING!

		char *str;

		// stop timer
		PAL_sysTimer16Ctrl(AVALANCHE_TIMER1_BASE, TIMER16_CTRL_STOP);

		avm_set_reset_status(RS_NMIWATCHDOG);
		str = "HardwareWatchDog - NMI taken";

		orig_pregs = get_fiq_regs();
		copy_banked_regs_full(&pregs, orig_pregs);
		set_fiq_regs(&pregs);

		die_with_mode_switch(str, &pregs, 0);

		// the point no return
		while(1) { }

	} else {
		if(wd_kicked == 0) {
			atomic_inc(&wd_count);
		} else {
			atomic_set(&wd_count, 0);
			atomic_set(&wd_sync, 0);
		}
	}

	return IRQ_HANDLED;
}

static int prepare_wdt_bark(void)
{
	int irqid;
	int timer_clk;
	int MaxTimerPeriod_usec = (int)((1.0 / (float)(HZ)) * 100000.0);

	timer_clk = PAL_sysClkcGetFreq(PAL_SYS_CLKC_TIMER2);
	PAL_sysResetCtrl(AVALANCHE_TIMER2_RESET, OUT_OF_RESET);
	PAL_sysTimer16SetParams(AVALANCHE_TIMER2_BASE, timer_clk,
				TIMER16_CNTRL_AUTOLOAD, MaxTimerPeriod_usec,
				NULL);

	irqid = AVALANCHE_TIMER_2_INT;
	return avm_request_fiq_on(cpumask_of(0), irqid, avm_wdt_bark, 0, "WDTimer bark/0", (void *)0xDEADDEAD);
	{
		printk(KERN_ERR "[%s] Watchdog timer bark setup failed, aborting!\n", __func__);
		return 0;
	}

	return -1;
}

static void kick_wdt_bark(void)
{
	int timer_clk;

	timer_clk = PAL_sysClkcGetFreq(PAL_SYS_CLKC_TIMER2);
	PAL_sysTimer16Ctrl(AVALANCHE_TIMER2_BASE, TIMER16_CTRL_STOP);
	PAL_sysTimer16SetParams(AVALANCHE_TIMER2_BASE, timer_clk,
				TIMER16_CNTRL_AUTOLOAD, 1000000,
				NULL);
	PAL_sysTimer16Ctrl(AVALANCHE_TIMER2_BASE, TIMER16_CTRL_START);
}
#endif

/**
 */
void ar7wdt_hw_init(void)
{
	int result;

#if defined(CONFIG_MACH_PUMA7)
	if(prepare_wdt_bark())
	{
		printk(KERN_ERR "[%s] Watchdog timer bark setup failed, aborting!\n", __func__);
		goto wdt_set_err;
	}
#endif

	// get the watchdog module out of reset and initialize watchdog timer
	PAL_sysResetCtrl(AVALANCHE_WDT_RESET, OUT_OF_RESET);
	PAL_sysWdtimerInit(AVALANCHE_WATCHDOG_TIMER_BASE,
			   PAL_sysClkcGetFreq(PAL_SYS_CLKC_WDT));

	// disable watchdog timer
	result = PAL_sysWdtimerCtrl(AVALANCHE_WDT_DISABLE_VALUE);
	if (result) {
		pr_err("[%s] Disabling watchdog timer failed, aborting!\n",
		       __func__);
		goto wdt_set_err;
	}
	// set default timeout value in ms
	result = PAL_sysWdtimerSetPeriod(AVALANCHE_WDT_MARGIN_MAX_VAL * 1000);
	if (result) {
		pr_err("[%s] setting watchdog timer to %d seconds failed, aborting!\n",
		       __func__, AVALANCHE_WDT_MARGIN_MAX_VAL);
		goto wdt_set_err;
	}
	// re-enable watchdog
	result = PAL_sysWdtimerCtrl(AVALANCHE_WDT_ENABLE_VALUE);
	if (result) {
		pr_err("[%s] Re-enabling watchdog timer failed, aborting!\n",
		       __func__);
		goto wdt_set_err;
	}
	// start watchdog by kicking it
	result = PAL_sysWdtimerKick();
	if (result) {
		pr_err("[%s] Kicking the watchdog timer failed!\n", __func__);
	}

#if defined(CONFIG_MACH_PUMA7)
	// Initialize WD sync/count
	atomic_set(&wd_count, 0);
	atomic_set(&wd_sync, 0);

	// kick WDT bark
	kick_wdt_bark();
#endif

wdt_set_err:
	return;
}

/**
 */
void ar7wdt_hw_deinit(void)
{
	int result;

	DBG("stopping WDT\n");

	result = PAL_sysWdtimerCtrl(AVALANCHE_WDT_DISABLE_VALUE);
	if (result) {
		pr_err("[%s] Disabling watchdog failed!\n", __func__);
	}
}

/**
 */
void ar7wdt_hw_reboot(void)
{
	DBG("%s!!\n", __func__);
	panic("%s: watchdog expired\n", __func__);
}

/**
 */
void ar7wdt_hw_trigger(void)
{
	int result;

	if (!ar7wdt_hw_is_wdt_running()) {
		return;
	}
#if defined(CONFIG_MACH_PUMA7)
	DBG("%s, bark count = %d\n", __func__, atomic_read(&wd_count));
#else
	DBG("%s !!\n", __func__);
#endif
	result = PAL_sysWdtimerKick();
	if (result) {
		pr_err("[%s] Kicking the watchdog failed!\n", __func__);
	}

#if defined(CONFIG_MACH_PUMA7)
	// WDTimer bark sync
	atomic_inc(&wd_sync);
#endif
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);
