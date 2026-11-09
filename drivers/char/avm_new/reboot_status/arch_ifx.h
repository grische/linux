/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_avm_reboot_status_ifx_h__
#define __arch_avm_reboot_status_ifx_h__

extern unsigned int avm_nmi_taken;

#define UPDATE_REBOOT_STATUS_TEXT                                              \
	"(c) AVM 2013, Reboot Status is: Firmware-Update"                      \
	"(c) AVM 2013, Reboot Status is: Firmware-Update"                      \
	"(c) AVM 2013, Reboot Status is: Firmware-Update"
#define SOFTWATCHDOG_REBOOT_STATUS_TEXT                                        \
	"(c) AVM 2013, Reboot Status is: Software-Watchdog"                    \
	"(c) AVM 2013, Reboot Status is: Software-Watchdog"                    \
	"(c) AVM 2013, Reboot Status is: Software-Watchdog"

#define NMI_REBOOT_STATUS_TEXT                                                 \
	"(c) AVM 2013, Reboot Status is: Software-NMI-Watchdog"                \
	"(c) AVM 2013, Reboot Status is: Software-NMI-Watchdog"                \
	"(c) AVM 2013, Reboot Status is: Software-NMI-Watchdog"
#define POWERON_REBOOT_STATUS_TEXT                                             \
	"(c) AVM 2013, Reboot Status is: Power-On-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Power-On-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Power-On-Reboot"
#define TEMP_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2013, Reboot Status is: Temperature-Reboot"                   \
	"(c) AVM 2013, Reboot Status is: Temperature-Reboot"                   \
	"(c) AVM 2013, Reboot Status is: Temperature-Reboot"

#define SOFT_REBOOT_STATUS_TEXT_PANIC                                          \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(PANIC)"
#define SOFT_REBOOT_STATUS_TEXT_OOM                                            \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(OOM)"
#define SOFT_REBOOT_STATUS_TEXT_OOPS                                           \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(OOPS)"
#define SOFT_REBOOT_STATUS_TEXT_BE                                             \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(BE)"

#define SOFT_REBOOT_STATUS_TEXT_UPDATE                                         \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(REBOOT-FOR-UPDATE)"
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_LOCAL                                   \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(DOCSIS_LOCAL)"
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_OPERATOR                                \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(DOCSIS_OPERATOR)"
#define SOFT_REBOOT_STATUS_TEXT_BOXCHANGE                                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(BOXCHANGE)"
#define SOFT_REBOOT_STATUS_TEXT_OPERATOR                                       \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot\0(OPERATOR)"
/*--- Achtung! Untermenge von obigen Eintraegen: ---*/
#define SOFT_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2013, Reboot Status is: Software-Reboot"

#include <asm/mips_mt.h>
#include <asm/reboot.h>

static int arch_flush_mailbox(char *mbox, size_t len)
{
	dma_cache_wback((unsigned long)mbox, len);
	return 0;
}

/**
 */
static char *arch_get_mailbox(void)
{
	char *mailbox = (char *)(0xA1000000 - 512);

	return mailbox;
}

/**
 */
static int arch_ifx_die_notifier(struct notifier_block *self,
				 unsigned long cmd,
				 void *ptr)
{
	int this_cpu;
	cpumask_t cpu_mask;

	if (cmd == DIE_OOPS) {
		struct die_args *args = (struct die_args *)ptr;
		struct pt_regs *regs = args->regs;
		static int die_counter;

		oops_enter();

		this_cpu = get_cpu();
		cpumask_setall(&cpu_mask);
		cpumask_clear_cpu(this_cpu, &cpu_mask);
		put_cpu();

		console_verbose();
		bust_spinlocks(1);
#ifdef CONFIG_MIPS_MT_SMTC
		mips_mt_regdump(0);
#endif /* CONFIG_MIPS_MT_SMTC */

		pr_info("%s[#%d]:\n", args->str, ++die_counter);
		avm_stack_check(current);
		show_registers(regs);
		add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
		arch_trigger_all_cpu_backtrace(&cpu_mask);
		avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);

		/* oops_exit triggers kmsg_dump() -> last chance to get infos
		 * for panic-log */
		oops_exit();
		panic("Fatal exception %s",
		      in_interrupt() ? "in interrupt" : "");
	}
	return NOTIFY_OK;
}

#define arch_die_notifier arch_ifx_die_notifier
#endif /*--- #ifndef __arch_avm_reboot_status_ifx_h__ ---*/
