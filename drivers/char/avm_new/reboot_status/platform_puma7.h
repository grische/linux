/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (c) 2017-2019 AVM GmbH <fritzbox_info@avm.de> */

#ifndef AVM_NEW_REBOOT_STATUS_PLATFORM_PUMA7_H
#define AVM_NEW_REBOOT_STATUS_PLATFORM_PUMA7_H

#include <linux/mm.h>
#include <avm/tffs/tffs.h>

#define MAILBOX_STRING_YEAR "2016"

static bool should_report_reset(enum _avm_reset_status status)
{
	switch (status) {
	case RS_POWERON:
	case RS_FIRMWAREUPDATE:
	case RS_TEMP_REBOOT:
	case RS_DOCSIS_LOCAL:
	case RS_DOCSIS_OPERATOR:
	case RS_BOXCHANGE:
	case RS_OPERATOR:
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
