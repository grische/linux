// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <asm/prom.h>
#include <linux/timer.h>
#include <avm/watchdog/watchdog.h>
#include "avm_sammel.h"
#include <linux/hardirq.h>
#include <asm/mach_avm.h>
#include <asm/traps.h>

#ifdef CONFIG_MACH_AR7240
#include <ar7240.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 46)
#include <atheros.h>
#else
#include <avm_atheros.h>
#endif

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define AVM_WATCHDOG_DEBUG ---*/

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG_ERR(...) printk(KERN_ERR __VA_ARGS__)
#define DBG_INFO(...) printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG_ERR(...) no_printk(KERN_ERR __VA_ARGS__)
#define DBG_INFO(...) no_printk(KERN_INFO __VA_ARGS__)
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

#if defined(CONFIG_MACH_AR7240)
extern uint32_t ar7240_ahb_freq;

static uint32_t get_ath_wdt_freq(void)
{
	return ar7240_ahb_freq;
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 46)
extern uint32_t ath_ahb_freq;

static uint32_t get_ath_wdt_freq(void)
{
	return ath_ahb_freq;
}
#else
static uint32_t get_ath_wdt_freq(void)
{
	return ath79_get_clock(avm_clock_id_wdt);
}
#endif
static atomic_t wdt_active;

static int nmi_notify_first(struct notifier_block *self,
			    unsigned long dummy, void *param);
static int nmi_notify_last(struct notifier_block *self, unsigned long dummy,
			   void *param);
static atomic_t nmi_trigger_once;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct notifier_block nmi_nb[2] = {
	{ .notifier_call = nmi_notify_first, .priority = INT_MAX },
	{ .notifier_call = nmi_notify_last,  .priority = 0 },
};
#define TIME_OUT_SECS 10

#if defined(ATH_NO_NMI)
/*--- wa wegen freezer ---*/
static unsigned int ath_watchdog_mode = ATH_WD_ACT_RESET;
#else
/*--- default ---*/
static unsigned int ath_watchdog_mode = ATH_WD_ACT_NMI;
#endif
/**--------------------------------------------------------------------------------**\
\**--------------------------------------------------------------------------------**/
int ath_nmi_mode(char *str)
{
	switch (*str) {
	case '0':
		ath_watchdog_mode = ATH_WD_ACT_RESET;
		return 0;
	case '1':
		ath_watchdog_mode = ATH_WD_ACT_NMI;
		return 0;
	}
	return 1;
}
__setup("nmi=", ath_nmi_mode);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int ath_get_nmi_mode(void)
{
	return ath_watchdog_mode;
}
EXPORT_SYMBOL(ath_get_nmi_mode);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void)
{
	unsigned int i;
	DBG_ERR("[ath:watchdog] start ...\n");
	atomic_set(&nmi_trigger_once, 0);
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
	if (nmi_workaround_func.cb_ath_workaround_watchdog) {
		ath_workaround_watchdog(TIME_OUT_SECS);
	} else
#endif /*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
	{
		ath_reg_wr(ATH_WATCHDOG_TMR,
			   (TIME_OUT_SECS * get_ath_wdt_freq()));
		wmb();
		ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ath_watchdog_mode);
		wmb();
		if (ath_watchdog_mode == ATH_WD_ACT_NMI) {
			pr_err("[ath:watchdog] activate nmi-mode\n");
		}
	}
	DBG_ERR("[ath:watchdog] action: %u (hardware reset) - ticks: %u (= %u seconds * %u Hz)\n",
		ath_reg_rd(ATH_WATCHDOG_TMR_CONTROL),
		TIME_OUT_SECS * get_ath_wdt_freq(), TIME_OUT_SECS,
		get_ath_wdt_freq());
	for (i = 0; i < ARRAY_SIZE(nmi_nb); i++) {
		register_nmi_notifier(&nmi_nb[i]);
	}
	atomic_set(&wdt_active, 1);
	return;
}
/**--------------------------------------------------------------------------------**\
\**--------------------------------------------------------------------------------**/
int ar7wdt_hw_is_wdt_running(void)
{
	return atomic_read(&wdt_active);
}
/**--------------------------------------------------------------------------------**\
 * dummy
\**--------------------------------------------------------------------------------**/
void ar7wdt_hw_secure_wdt_disable(void)
{
}
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void)
{
	DBG_ERR("[ath:watchdog] stop ...\n");
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
	if (nmi_workaround_func.cb_ath_workaround_watchdog) {
		ath_workaround_watchdog(0);
	} else
#endif /*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
	{
		ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_NONE);
		wmb();
	}
	atomic_set(&wdt_active, 0);
	return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_reboot(void)
{
	DBG_ERR("ar7wdt_hw_reboot!!\n");
	panic("ar7wdt_hw_reboot: watchdog expired\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_trigger(void)
{
	if (!ar7wdt_hw_is_wdt_running()) {
		return;
	}
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
	if (nmi_workaround_func.cb_ath_workaround_watchdog) {
		ath_workaround_watchdog(TIME_OUT_SECS);
	} else
#endif /*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
	{
#ifdef AVM_WATCHDOG_DEBUG
		unsigned int ticks_left = ath_reg_rd(ATH_WATCHDOG_TMR);
		DBG_ERR("[ath:watchdog] %x %x triggered %u.%u secs before reset\n",
			ath_reg_rd(ATH_WATCHDOG_TMR_CONTROL),
			ath_reg_rd(ATH_WATCHDOG_TMR),
			ticks_left / get_ath_wdt_freq(),
			(((ticks_left % get_ath_wdt_freq()) * 10U) /
			 get_ath_wdt_freq()) %
				10);
#endif
		ath_reg_wr(ATH_WATCHDOG_TMR,
			   (TIME_OUT_SECS * get_ath_wdt_freq()));
		wmb();
	}
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);
extern void set_reboot_status_to_NMI(void);

/**
Atheros: Exception 0xbfc00380 wird auch auf nmi_exception_handler gelegt
	High-Prio!
 */
static int nmi_notify_first(struct notifier_block *self,
			    unsigned long dummy, void *param)
{
	struct pt_regs *regs = (struct pt_regs *)param;
	u32 status;

	if (regs)
		regs->cp0_epc =
			read_c0_errorepc(); /*--- damit backtrace vernuenftig funktioniert ---*/
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
	ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_NONE);
	wmb();
	memset(&nmi_workaround_func, 0, sizeof(struct _nmi_workaround_func));
#elif defined(CONFIG_ATH79)
	ath_reg_wr(ATH_WATCHDOG_TMR, (10 /* s */ * get_ath_wdt_freq()));
	wmb();
	ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_RESET);
	wmb();
	atomic_set(&wdt_active, 0);
#endif /*--- #if defined(CONFIG_ATH79) ---*/
	status = read_c0_status();
	status &= ~(1 << 0); /* disable all interrupts */
	status &= ~(1 << 19); /* reset NMI status */
	status &= ~(1 << 22); /* bootstrap bit BEV zurücksetzen */
	/*--------------------------------------------------------------------------------*\
	 * mbahr:
	   Doku MIPS32 4KE Processor Cores Software User's Manual:
		Operation:
		// If StatusEXL is 1, all exceptions go through the general exception vector !!!
		// and neither EPC nor CauseBD nor SRSCtl are modified
			if StatusEXL = 1 then
				vectorOffset ← 16#180
			else
				if InstructionInBranchDelaySlot then
				EPC ← restartPC // PC of branch/jump
				CauseBD ← 1
			else
				EPC ← restartPC //PC of instruction
				CauseBD ← 0
			endif
			....
			-> NMI setzt EXL!!!!!!
	\*--------------------------------------------------------------------------------*/

	/*
	 * Superwichtig! EXL ruecksetzen - somit funktionieren nachfolgend
	 * auch TLB-Exceptions (Zugriff auf virtuellen Speicher)
	 */
	status &= ~(1 << 1);

	write_c0_status(status);
	set_reboot_status_to_NMI();
	return NOTIFY_OK;
}
/**
	Lowest-Prio -> trigger die !
 */
static int nmi_notify_last(struct notifier_block *self,
			   unsigned long dummy, void *param)
{
	struct pt_regs *regs = (struct pt_regs *)param;
	char str[100];

	bust_spinlocks(1);
	console_verbose();
	printk_avm_console_bend(0); /* force serial-output */

	snprintf(str, sizeof(str), "CPU%d NMI taken (err)epc=%pF",
		 smp_processor_id(),
		 regs ? (void *)regs->cp0_epc : (void *)read_c0_errorepc());
	die(str, regs);
	return NOTIFY_STOP;
}
