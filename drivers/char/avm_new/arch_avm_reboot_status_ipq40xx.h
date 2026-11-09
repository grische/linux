/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_avm_reboot_status_ipq40xx_h__
#define __arch_avm_reboot_status_ipq40xx_h__

#include <linux/of_reserved_mem.h>
#include <linux/avm_hw_config.h>
#include <asm/mach_avm.h>
#include <asm/avm_enh/avm_enh.h>
#include <asm/cacheflush.h>

#define UPDATE_REBOOT_STATUS_TEXT                                              \
	"(c) AVM 2015, Reboot Status is: Firmware-Update"                      \
	"(c) AVM 2015, Reboot Status is: Firmware-Update"                      \
	"(c) AVM 2015, Reboot Status is: Firmware-Update"
#define NMI_REBOOT_STATUS_TEXT                                                 \
	"(c) AVM 2015, Reboot Status is: NMI-Watchdog"                         \
	"(c) AVM 2015, Reboot Status is: NMI-Watchdog"                         \
	"(c) AVM 2015, Reboot Status is: NMI-Watchdog"
#define SOFTWATCHDOG_REBOOT_STATUS_TEXT                                        \
	"(c) AVM 2015, Reboot Status is: Software-Watchdog"                    \
	"(c) AVM 2015, Reboot Status is: Software-Watchdog"                    \
	"(c) AVM 2015, Reboot Status is: Software-Watchdog"
#define POWERON_REBOOT_STATUS_TEXT                                             \
	"(c) AVM 2015, Reboot Status is: Power-On-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Power-On-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Power-On-Reboot"
#define TEMP_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2015, Reboot Status is: Temperature-Reboot"                   \
	"(c) AVM 2015, Reboot Status is: Temperature-Reboot"                   \
	"(c) AVM 2015, Reboot Status is: Temperature-Reboot"
#define SOFT_REBOOT_STATUS_TEXT_PANIC                                          \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(PANIC)"
#define SOFT_REBOOT_STATUS_TEXT_OOM                                            \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(OOM)"
#define SOFT_REBOOT_STATUS_TEXT_OOPS                                           \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(OOPS)"
#define SOFT_REBOOT_STATUS_TEXT_UPDATE                                         \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(RESET-FOR-UPDATE)"
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_LOCAL                                   \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(DOCSIS_LOCAL)"
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_OPERATOR                                \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(DOCSIS_OPERATOR)"
/*--- Achtung! Untermenge von obigen Eintraegen: ---*/
#define SOFT_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"

#if defined(CONFIG_OF_AVM_DT)
#include <linux/of_fdt.h>
#endif /* defined(CONFIG_OF_AVM_DT) */

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
	static char *mailbox;

	if (mailbox)
		return mailbox;

#if defined(CONFIG_OF_AVM_DT)
	//try to find the reserved entry for 'avm_reboot_string'
	rmem = fdt_get_reserved_mem_resource("avm_reboot_string");
	if (rmem) {
		mailbox = (char *)phys_to_virt(rmem->base);
		/*--- mailbox = rmem->base; ---*/
		/*--- pr_info("Found avm_reboot_string, setting mailbox to 0x%08x\n",(uint32_t) mailbox); ---*/
		return mailbox;
	}
#endif /* defined(CONFIG_OF_AVM_DT) */
	mailbox = (char *)AVM_REBOOT_STRING_LOCATION;
	pr_err("No reserved memory for avm_reboot_string found. Setting mailbox to fallback 0x%08x\n",
	       (uint32_t)mailbox);
	return mailbox;
}

#if defined(CONFIG_AVM_FASTIRQ)
static int fastirq_dump_once;

/**
 */
static atomic_t crashed_cpu = ATOMIC_INIT(-1);

#define SET_CRASHED_CPU(cpu) atomic_set(&crashed_cpu, cpu)
#define GET_CRASHED_CPU() atomic_read(&crashed_cpu)
static int arch_dakota_die_notifier(struct notifier_block *self,
				    unsigned long cmd, void *ptr)
{
	int this_cpu;
	cpumask_t cpu_mask;
	struct thread_info *pthread = NULL;
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;

	if (cmd == DIE_OOPS) {
		struct task_struct *tsk = NULL;

		pthread = current_thread_info_depend_on_context(regs);
		if (virt_addr_valid(pthread)) {
			tsk = pthread->task;
		}

		this_cpu = atomic_read(&crashed_cpu);
		this_cpu = (this_cpu != -1) ? this_cpu : 0;
		cpumask_clear(&cpu_mask);
		cpumask_set_cpu(this_cpu, &cpu_mask);

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
	return NOTIFY_OK;
}

#define arch_die_notifier arch_dakota_die_notifier
/**
 */
static int
arch_dakota_panic_notifier(struct notifier_block *notifier_block,
			   unsigned long event,
			   void *cause_string)
{
	if (fastirq_dump_once == 0) {
		avm_fiq_dump_stat();
	}
	return NOTIFY_OK;
}

#define arch_panic_notifier arch_dakota_panic_notifier

#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

#endif /*--- #ifndef __arch_avm_reboot_status_ipq40xx_h__ ---*/
