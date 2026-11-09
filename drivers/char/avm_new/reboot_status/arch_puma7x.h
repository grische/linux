/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2017-2019 AVM GmbH <fritzbox_info@avm.de> */

#ifndef __arch_avm_reboot_status_puma7x_h__
#define __arch_avm_reboot_status_puma7x_h__

#include <linux/string.h>
#include <linux/env.h>
#include <asm/avm_enh/avm_enh.h>

#define _repeat3concat(a, b) a a a b

#define UPDATE_REBOOT_STATUS_TEXT                                              \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Firmware-Update", "")
#define NMI_REBOOT_STATUS_TEXT                                                 \
	_repeat3concat("(c) AVM 2017, Reboot Status is: NMI-Watchdog", "")
#define SOFTWATCHDOG_REBOOT_STATUS_TEXT                                        \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Watchdog", "")
#define POWERON_REBOOT_STATUS_TEXT                                             \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Power-On-Reboot", "")
#define TEMP_REBOOT_STATUS_TEXT                                                \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Temperature-Reboot", "")
#define SOFT_REBOOT_STATUS_TEXT_PANIC                                          \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(PANIC)")
#define SOFT_REBOOT_STATUS_TEXT_OOM                                            \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(OOM)")
#define SOFT_REBOOT_STATUS_TEXT_OOPS                                           \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(OOPS)")
#define SOFT_REBOOT_STATUS_TEXT_BE                                             \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(BE)")
#define SOFT_REBOOT_STATUS_TEXT_UPDATE                                         \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(RESET-FOR-UPDATE)")
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_LOCAL                                   \
        _repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
                       "\0(DOCSIS_LOCAL)")
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_OPERATOR                                \
        _repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
                       "\0(DOCSIS_OPERATOR)")
#define SOFT_REBOOT_STATUS_TEXT_BOXCHANGE                                      \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(BOXCHANGE)")
#define SOFT_REBOOT_STATUS_TEXT_OPERATOR                                       \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot",      \
		       "\0(OPERATOR)")

/*--- Achtung! Untermenge von obigen Einträgen: ---*/
#define SOFT_REBOOT_STATUS_TEXT                                                \
	_repeat3concat("(c) AVM 2017, Reboot Status is: Software-Reboot", "")


#define EFIVAR_REBOOTSTATUS "reboot_status"


static char *arch_get_mailbox(void)
{
	static u8 mb[MAILBOX_SIZE];

	return (char *)mb;
}

#define arch_read_mailbox puma7_read_mailbox
static int puma7_read_mailbox(char *mbox, size_t size)
{
	char *static_mbox;
	int mblen;

	if (size > MAILBOX_SIZE)
		size = MAILBOX_SIZE;

	mblen = avm_efivar_get_one(EFIVAR_REBOOTSTATUS, mbox, size);
	if (mblen > size)
		return -ERANGE;
	if (mblen < 0)
		return mblen;

	static_mbox = arch_get_mailbox();
	memcpy(static_mbox, mbox, mblen);

	return 0;
}

static int arch_flush_mailbox(char *mbox, size_t len)
{
	int ret;

	ret = avm_efivar_set_one_atomic(EFIVAR_REBOOTSTATUS,
					(uint8_t *)mbox,
					len);
	if (ret != 0)
		pr_err("Error setting value for %s: %d\n",
		       EFIVAR_REBOOTSTATUS, ret);

	return ret;
}

static int arch_x86_die_notifier(struct notifier_block *self,
				 unsigned long cmd,
				 void *ptr)
{
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;

	if (cmd != DIE_OOPS)
		return NOTIFY_DONE;

	oops_enter();

	console_verbose();
	bust_spinlocks(1);

	show_regs(regs);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);

	/* oops_exit triggers kmsg_dump() -> last chance to get infos
	 * for panic-log */
	oops_exit();
	panic("Fatal exception %s", in_interrupt() ? "in interrupt" : "");

	return NOTIFY_OK;
}
#define arch_die_notifier arch_x86_die_notifier
#endif

/* vim: set ts=8 sw=8 noet cino=>8\:0l1(0: */
