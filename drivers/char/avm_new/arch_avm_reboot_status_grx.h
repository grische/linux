/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_avm_reboot_status_grx_h__
#define __arch_avm_reboot_status_grx_h__

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
	"(c) AVM 2015, Reboot Status is: Software-Reboot\0(REBOOT-FOR-UPDATE)"
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

#include <asm/reboot.h>
#include <asm/bootinfo.h>

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

		pr_info("%s[#%d]:\n", args->str, ++die_counter);
		avm_stack_check(current);
		show_registers(regs);
		add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
		arch_trigger_all_cpu_backtrace(&cpu_mask);

		avm_oom_show_memstat(
			0x1); /*--- oops_exit triggers kmsg_dump() -> last chance to get infos for panic-log ---*/
		oops_exit();
		panic("Fatal exception %s",
		      in_interrupt() ? "in interrupt" : "");
	}
	return NOTIFY_OK;
}

#define arch_die_notifier arch_grx_die_notifier
#endif /*--- #ifndef __arch_avm_reboot_status_grx_h__ ---*/
