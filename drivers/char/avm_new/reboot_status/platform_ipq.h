/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_PLATFORM_IPQ_H
#define AVM_NEW_REBOOT_STATUS_PLATFORM_IPQ_H

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#include <linux/avm_hw_config.h>
#else
#include <linux/sched/task_stack.h>
#endif
#include <asm/cacheflush.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#ifdef CONFIG_AVM_GENERIC_RTE
#include <avm/rte/bthelper.h>
#else
#ifdef CONFIG_AVM_FASTIRQ
#include <asm/avm_enh/avm_enh.h>
#endif
#endif

#define MAILBOX_STRING_YEAR "2015"
#define AVM_REBOOT_STRING_LOCATION  0xa0001000
#define AVM_REBOOT_STRING_SIZE      512

static int arch_flush_mailbox(char *mbox, size_t len)
{
	dmac_flush_range(mbox, mbox + len);
	return 0;
}

/**
 */
static char *arch_get_mailbox(void)
{
	struct reserved_mem *rmem;
	struct device_node *np __maybe_unused;
	static char *mailbox;

	if (mailbox)
		return mailbox;

	//try to find the reserved entry for 'avm_reboot_string'
	np = of_find_node_by_name(NULL, "avm_reboot_string");
	rmem = of_reserved_mem_lookup(np);
	if (rmem) {
		mailbox = (char *)phys_to_virt(rmem->base);
		/*--- mailbox = rmem->base; ---*/
		/*--- pr_info("Found avm_reboot_string, setting mailbox to 0x%08x\n",(uint32_t) mailbox); ---*/
		return mailbox;
	}
	mailbox = (char *)AVM_REBOOT_STRING_LOCATION;
	pr_err("No reserved memory for avm_reboot_string found. Setting mailbox to fallback 0x%px\n",
	       mailbox);
	return mailbox;
}

#if defined(CONFIG_AVM_FASTIRQ)
static int fastirq_dump_once;

/**
 */
static atomic_t crashed_cpu = ATOMIC_INIT(-1);

#define SET_CRASHED_CPU(cpu) atomic_set(&crashed_cpu, cpu)
#if defined(CONFIG_ARCH_IPQ5018)
#define GET_CRASHED_CPU()						\
	({								\
		int cpu;						\
		((cpu = atomic_read(&crashed_cpu)) != -1) ? cpu : 0;	\
	})
#else
#define GET_CRASHED_CPU() atomic_read(&crashed_cpu)
#endif

static int arch_ipq_die_notifier(struct notifier_block *self,
				   unsigned long cmd, void *ptr)
{
	int this_cpu;
	cpumask_t cpu_mask;
	struct thread_info *pthread = NULL;
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;
	struct task_struct *tsk = NULL;

	this_cpu = atomic_read(&crashed_cpu);
	this_cpu = (this_cpu != -1) ? this_cpu : 0;
	cpumask_clear(&cpu_mask);
	cpumask_set_cpu(this_cpu, &cpu_mask);

	if (this_cpu != raw_smp_processor_id())
		pthread = thread_info_by_sp(regs->ARM_sp);
	else
		pthread = current_thread_info_depend_on_context(regs);

	if (virt_addr_valid(pthread))
		tsk = pthread->task;

#if defined(CONFIG_ARCH_IPQ40XX)
	console_verbose();
#endif

	avm_stack_check(tsk);
	print_modules();

	pr_emerg("Process %.*s (pid: %d, stack limit = 0x%p)\n",
		 TASK_COMM_LEN, (tsk ? tsk->comm : NULL),
		 (tsk ? task_pid_nr(tsk) : -1), end_of_stack(tsk));

	if (!avm_trigger_all_cpu_backtrace(
		    regs, &cpu_mask)) { /*--- this cpu ---*/
		pr_err("\nFIQ backtrace trigger not ready, falling back ...\n");

		return NOTIFY_OK;
	}

	print_code_range(NULL, "Code:", instruction_pointer(regs),
			 thumb_mode(regs));

	fastirq_dump_once = 1;
	pr_emerg("\nBacktrace of all other CPU's:");
	cpumask_setall(&cpu_mask);
	cpumask_clear_cpu(this_cpu, &cpu_mask);
	(void)avm_trigger_all_cpu_backtrace(
		regs, &cpu_mask); /*--- all other cpus ---*/
	pr_emerg("\nBacktrace of all other CPU's done\n\n");
	avm_fiq_dump_stat();

	return NOTIFY_STOP;
}

#define arch_die_notifier arch_ipq_die_notifier
#define arch_panic_notifier_priority 0
/**
 */
static int
arch_ipq_panic_notifier(struct notifier_block *notifier_block,
			   unsigned long event,
			   void *cause_string)
{
	if (fastirq_dump_once == 0) {
		avm_fiq_dump_stat();
	}
	return NOTIFY_OK;
}

#define arch_panic_notifier arch_ipq_panic_notifier

#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

#endif
