/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2017-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/mm.h>

#ifndef __arch_avm_reboot_status_puma7_h__
#define __arch_avm_reboot_status_puma7_h__
#if __has_include(<avm/tffs/tffs.h>)
#include <avm/tffs/tffs.h>
#else
#include <linux/tffs.h>
#endif

#define _repeat3concat(a, b) a a a b

#define UPDATE_REBOOT_STATUS_TEXT                                              \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Firmware-Update", "")
#define NMI_REBOOT_STATUS_TEXT                                                 \
	_repeat3concat("(c) AVM 2016, Reboot Status is: NMI-Watchdog", "")
#define SOFTWATCHDOG_REBOOT_STATUS_TEXT                                        \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Software-Watchdog", "")
#define POWERON_REBOOT_STATUS_TEXT                                             \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Power-On-Reboot", "")
#define TEMP_REBOOT_STATUS_TEXT                                                \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Temperature-Reboot", "")
#define SOFT_REBOOT_STATUS_TEXT_PANIC                                          \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot",      \
		       "\0(PANIC)")
#define SOFT_REBOOT_STATUS_TEXT_OOM                                            \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot",      \
		       "\0(OOM)")
#define SOFT_REBOOT_STATUS_TEXT_OOPS                                           \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot",      \
		       "\0(OOPS)")
#define SOFT_REBOOT_STATUS_TEXT_UPDATE                                         \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot",      \
		       "\0(RESET-FOR-UPDATE)")
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_LOCAL                                   \
        _repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot",      \
                       "\0(DOCSIS_LOCAL)")
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_OPERATOR                                \
        _repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot",      \
                       "\0(DOCSIS_OPERATOR)")

/*--- Achtung! Untermenge von obigen Einträgen: ---*/
#define SOFT_REBOOT_STATUS_TEXT                                                \
	_repeat3concat("(c) AVM 2016, Reboot Status is: Software-Reboot", "")

static bool should_report_reset(enum _avm_reset_status status)
{
	switch (status) {
	case RS_POWERON:
	case RS_FIRMWAREUPDATE:
	case RS_TEMP_REBOOT:
	case RS_REBOOT_FOR_UPDATE:
	case RS_DOCSIS_LOCAL:
	case RS_DOCSIS_OPERATOR:
		pr_debug("Not reporting next reset status: %d\n", status);
		return false;
	default:
		return !tffs_panic_log_suppress;
	}
}

#define arch_reboot_handler puma7_arch_reboot_handler
static int puma7_arch_reboot_handler(struct notifier_block *nb, unsigned long event,
			       void *data)
{
	static int is_recursion;

	if (!is_recursion++ && should_report_reset(avm_next_reset_status())) {
		pr_notice("Reboot pending, dumping context of current (pid %d):\n",
			  task_pid_nr(current));
		show_parent_tasks_cmdlines(current);
		pr_emerg("calling panic() instead of \"silent\" reboot\n");
		panic("panic prevents \"silent\" reboot");
	}

	return NOTIFY_OK;
}

static inline char *arch_get_mailbox(void)
{
	return NULL;
}

static int arch_flush_mailbox(char *mbox, size_t len)
{
	return 0;
}

#endif

/* vim: set ts=8 sw=8 noet cino=>8\:0l1(0: */
