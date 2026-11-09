/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_PLATFORM_BRCMA_H
#define AVM_NEW_REBOOT_STATUS_PLATFORM_BRCMA_H

#include "config_device_tree.h"
#include "arch_arm.h"

#include <linux/of_reserved_mem.h>
#include <asm/mach_avm.h>
#if defined(CONFIG_AVM_GENERIC_RTE)
#include <avm/rte/bthelper.h>
#else
#include <asm/avm_enh/avm_enh.h>
#endif
#include <asm/cacheflush.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task_stack.h>
#endif
#if defined(CONFIG_OF_AVM_DT)
#include <linux/of.h>
#endif /* defined(CONFIG_OF_AVM_DT) */

#define MAILBOX_STRING_YEAR "2015"

extern void *avm_boot_string_virt_addr;

#if defined(CONFIG_AVM_FASTIRQ)
static int fastirq_dump_once;

static atomic_t crashed_cpu = ATOMIC_INIT(-1);

#define SET_CRASHED_CPU(cpu) atomic_set(&crashed_cpu, cpu)
#define GET_CRASHED_CPU() atomic_read(&crashed_cpu)

/**
 */
static int arch_brcma_die_notifier(struct notifier_block *self,
				   unsigned long cmd, void *ptr)
{
	int this_cpu;
	cpumask_t cpu_mask;
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;
	struct task_struct *tsk = current;

#if defined(CONFIG_AVM_FASTIRQ)
	tsk = current_thread_info_depend_on_context(regs)->task;
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/
	this_cpu = atomic_read(&crashed_cpu);
	this_cpu = (this_cpu != -1) ? this_cpu : 0;
	cpumask_clear(&cpu_mask);
	cpumask_set_cpu(this_cpu, &cpu_mask);
	put_cpu();

	avm_stack_check(tsk);
	print_modules();

	pr_emerg("Process %.*s (pid: %d, stack limit = 0x%p)\n",
		 TASK_COMM_LEN, tsk->comm, task_pid_nr(tsk),
		 end_of_stack(tsk));
	avm_trigger_all_cpu_backtrace(regs,
				      &cpu_mask); /*--- this cpu ---*/
	if (regs)
		print_code_range(NULL, "Code:", instruction_pointer(regs),
				 thumb_mode(regs));

	fastirq_dump_once = 1;
	avm_fiq_dump_stat();
	pr_err("\nBacktrace of all other CPU's:");
	cpumask_setall(&cpu_mask);
	cpumask_clear_cpu(this_cpu, &cpu_mask);
	avm_trigger_all_cpu_backtrace(
		regs, &cpu_mask); /*--- all other cpus ---*/
	pr_err("\nBacktrace of all other CPU's done\n\n");
	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);

	return NOTIFY_STOP;
}

#define arch_die_notifier arch_brcma_die_notifier

/**
 */
static int
arch_brcma_panic_notifier(struct notifier_block *notifier_block,
			  unsigned long event,
			  void *cause_string)
{
	if (fastirq_dump_once == 0) {
		avm_fiq_dump_stat();
	}
	return NOTIFY_OK;
}

#define arch_panic_notifier arch_brcma_panic_notifier
#define arch_panic_notifier_priority 0
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

#endif
