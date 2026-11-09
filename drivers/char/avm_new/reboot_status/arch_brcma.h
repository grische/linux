/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_avm_reboot_status_brcma_h__
#define __arch_avm_reboot_status_brcma_h__

#include <linux/of_reserved_mem.h>
#include <linux/avm_hw_config.h>
#include <asm/mach_avm.h>
#include <asm/avm_enh/avm_enh.h>
#include <asm/cacheflush.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task_stack.h>
#endif

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
#define SOFT_REBOOT_STATUS_TEXT_BE                                             \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(BE)"
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
#define SOFT_REBOOT_STATUS_TEXT_BOXCHANGE                                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(BOXCHANGE)"
#define SOFT_REBOOT_STATUS_TEXT_OPERATOR                                       \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(OPERATOR)"
/*--- Achtung! Untermenge von obigen Eintraegen: ---*/
#define SOFT_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2015, Reboot Status is: Software-Reboot"

#if defined(CONFIG_OF_AVM_DT)
#include <linux/of.h>
#endif /* defined(CONFIG_OF_AVM_DT) */

extern void *avm_boot_string_virt_addr;

/**
 */
static char *arch_get_mailbox(void)
{
	static char *mailbox;

	if (mailbox)
		return mailbox;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	{
	u32 addr = 0;
	struct device_node *rsvdmem_node, *rebootstr_node;

	rsvdmem_node = of_find_node_by_path("/reserved-memory");
	if (rsvdmem_node) {
		rebootstr_node = of_get_compatible_child(rsvdmem_node, "avm,reboot_string");
		if (rebootstr_node) {
			of_property_read_u32(rebootstr_node, "reg", &addr);
			of_node_put(rebootstr_node);
		} else {
			of_property_read_u32(rsvdmem_node, "avm_reboot_string", &addr);
		}
		of_node_put(rsvdmem_node);
	}
	if (addr)
		mailbox = (char *)phys_to_virt(addr);
	}
#else

	if (!mailbox) {
		mailbox = (char *)avm_boot_string_virt_addr;
		pr_err("Didn't found avm_reboot_string, setting mailbox to fallback 0x%08x\n",
		       (uint32_t)mailbox);
	}
#endif

	return mailbox;
}

#if defined(CONFIG_AVM_FASTIRQ)
static int fastirq_dump_once;

/**
 */
static int arch_brcma_die_notifier(struct notifier_block *self,
				   unsigned long cmd, void *ptr)
{
	int this_cpu;
	cpumask_t cpu_mask;
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;

	if (cmd == DIE_OOPS) {
		struct task_struct *tsk = current;

#if defined(CONFIG_AVM_FASTIRQ)
		tsk = current_thread_info_depend_on_context(regs)->task;
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/
		this_cpu = get_cpu();
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
	return NOTIFY_OK;
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

static int arch_flush_mailbox(char *mbox, size_t len)
{
	dmac_flush_range(mbox, mbox + len);
	return 0;
}

#endif /*--- #ifndef __arch_avm_reboot_status_brcma_h__ ---*/
