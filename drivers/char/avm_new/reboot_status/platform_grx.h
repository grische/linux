/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_PLATFORM_GRX_H
#define AVM_NEW_REBOOT_STATUS_PLATFORM_GRX_H

#include <asm/reboot.h>
#include <asm/bootinfo.h>
#include <asm/mach_avm.h>

#define MAILBOX_STRING_YEAR "2015"

static int arch_flush_mailbox(char *mbox, size_t len)
{
	dma_cache_wback((unsigned long)mbox, len);
	return 0;
}

/**
 */
static char *arch_get_mailbox(void)
{
	static char *mailbox;
	u32 addr = 0;
	struct device_node *rsvdmem_node, *rebootstr_node;

	if (mailbox) {
		return mailbox;
	}

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
		mailbox = (void *)KSEG1ADDR(addr);

	if (!mailbox) {
		mailbox = (char *)AVM_REBOOT_STRING_LOCATION;
		pr_err("Didn't found avm_reboot_string, setting mailbox to fallback 0x%08x\n",
		       (uint32_t)mailbox);
	}

	return mailbox;
}

/**
 */
static int arch_grx_die_notifier(struct notifier_block *self,
				 unsigned long cmd,
				 void *ptr)
{
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;
	static int die_counter;
	int this_cpu;
	cpumask_t cpu_mask;

	oops_enter();

	this_cpu = get_cpu();
	cpumask_setall(&cpu_mask);
	cpumask_clear_cpu(this_cpu, &cpu_mask);
	put_cpu();

	console_verbose();
	bust_spinlocks(1);

	pr_info("%s[#%d]:\n", args->str, ++die_counter);
	avm_stack_check(current);
	show_registers(regs);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	avm_stop_all_other_cpus();
	arch_trigger_all_cpu_backtrace(&cpu_mask);

	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);

	/* oops_exit triggers kmsg_dump() -> last chance to get infos
	 * for panic-log */
	oops_exit();
	panic("Fatal exception %s",
	      in_interrupt() ? "in interrupt" : "");
	return NOTIFY_OK;
}

#define arch_die_notifier arch_grx_die_notifier

/**
 */
static int
arch_grx_panic_notifier(struct notifier_block *notifier_block,
			  unsigned long event,
			  void *cause_string)
{
	avm_stop_all_other_cpus();
	return NOTIFY_OK;
}
#define arch_panic_notifier arch_grx_panic_notifier
#define arch_panic_notifier_priority INT_MAX

#endif
