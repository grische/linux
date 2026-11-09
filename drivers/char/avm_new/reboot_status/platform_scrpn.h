/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_PLATFORM_SCRPN_H
#define AVM_NEW_REBOOT_STATUS_PLATFORM_SCRPN_H

#include <asm/reboot.h>
#include <ar71xx_regs.h>

#define MAILBOX_STRING_YEAR "2015"

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

	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);

	/* oops_exit triggers kmsg_dump() -> last chance to get infos
	 * for panic-log */
	oops_exit();
	panic("Fatal exception %s",
	      in_interrupt() ? "in interrupt" : "");
}

#define arch_die_notifier arch_scrpn_die_notifier
#endif
