/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_avm_reboot_status_scrpn_h__
#define __arch_avm_reboot_status_scrpn_h__

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

#include <asm/reboot.h>
#include <ar71xx_regs.h>

#if defined(CONFIG_SOC_QCA953x)
#define ATH_SRAM_PRINTKLEN (AR934X_SRAM_SIZE_HBEE - 0x500)
#define ATH_REBOOT_LOGBUFFER                                                   \
	(AR934X_SRAM_BASE_UNCACHED + AR934X_SRAM_SIZE_HBEE - ATH_SRAM_PRINTKLEN)
#elif defined(AR934X_SRAM_SIZE)
/*--- use internal sram, but not at the begin, because here maybe data from mni-workarround (aprox first 0x500 bytes) ---*/
#define ATH_SRAM_PRINTKLEN (AR934X_SRAM_SIZE - 0x500)
#define ATH_REBOOT_LOGBUFFER                                                   \
	(AR934X_SRAM_BASE_UNCACHED + AR934X_SRAM_SIZE - ATH_SRAM_PRINTKLEN)
#else /*--- #if defined(CONFIG_SOC_AR934x) ---*/
#error ATH_REBOOT_LOGBUFFER not defined
#undef ATH_SRAM_PRINTKLEN
#endif /*--- #else ---*/ /*--- #if defined(ATH_SRAM_SIZE) ---*/

#if defined(ATH_SRAM_BASE_UNCACHED)
#define ATH_DUMPREGISTER_MEMORY ATH_REBOOT_LOGBUFFER
#endif /*--- #if defined(ATH_SRAM_BASE_UNCACHED) ---*/

static int arch_flush_mailbox(char *mbox, size_t len)
{
	dma_cache_wback((unsigned long)mbox, len);
	return 0;
}

/**
 */
static char *arch_get_mailbox(void)
{
	return (char *)ATH_REBOOT_LOGBUFFER;
}

extern void show_registers(struct pt_regs *regs);

/**
 */
static int arch_scrpn_die_notifier(struct notifier_block *self,
				   unsigned long cmd,
				   void *ptr)
{
	if (cmd == DIE_OOPS) {
		struct die_args *args = (struct die_args *)ptr;
		struct pt_regs *regs = args->regs;
		static int die_counter;

		oops_enter();

		console_verbose();
		bust_spinlocks(1);

		pr_info("%s[#%d]:\n", args->str, ++die_counter);
		avm_stack_check(current);
		show_registers(regs);
		add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
		trigger_all_cpu_backtrace();

		avm_oom_show_memstat(
			0x1); /*--- oops_exit triggers kmsg_dump() -> last chance to get infos for panic-log ---*/
		oops_exit();
		panic("Fatal exception %s",
		      in_interrupt() ? "in interrupt" : "");
	}
	return NOTIFY_OK;
}

#define arch_die_notifier arch_scrpn_die_notifier
#endif /*--- #ifndef __arch_avm_reboot_status_scrpn_h__ ---*/
