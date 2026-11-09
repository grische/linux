/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (c) 2017-2019 AVM GmbH <fritzbox_info@avm.de> */

#ifndef AVM_NEW_REBOOT_STATUS_PLATFORM_PUMA7X_H
#define AVM_NEW_REBOOT_STATUS_PLATFORM_PUMA7X_H

#include <linux/string.h>
#include <linux/env.h>
#include <asm/avm_enh/avm_enh.h>
#include <linux/hw_mailbox.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/debug.h>
#endif

#define MAILBOX_STRING_YEAR "2017"
#define EFIVAR_REBOOTSTATUS "reboot_status"

static __init int puma7_kill_reboot_status_efivar(void)
{
	int ret;

	ret = avm_efivar_delete(EFIVAR_REBOOTSTATUS);
	/*
	 * Unfortunately, for some reason the EFI rejects deleting a
	 * non-existing variable with EFI_INVALID_PARAMETER,
	 * instead of EFI_NOT_FOUND as it should.  So, with the EFI values
	 * being translated to errno, once the variable has been removed we
	 * see EINVAL here instead of ENOENT.  So we accept this as a valid
	 * result.
	 */
	if (ret == -ENOENT || ret == -EINVAL)
		return 0;
	if (ret)
		pr_err("Error removing obsolete avm efivar `%s': %d\n",
		       EFIVAR_REBOOTSTATUS, ret);
	return ret;
}
late_initcall(puma7_kill_reboot_status_efivar);

static char *arch_get_mailbox(void)
{
	static u8 mbox[MAILBOX_SIZE];

	return (char *)mbox;
}

#define arch_read_mailbox puma7_read_mailbox
static int puma7_read_mailbox(char *mbox, size_t size)
{
	const volatile void __iomem *hw_mbox_sram;
	size_t sram_size;

	hw_mbox_sram = avm_hw_mbox_private_sram(&sram_size);
	if (!hw_mbox_sram)
		return -ENODEV;
	BUG_ON(size > sram_size);
	memcpy_fromio(mbox, hw_mbox_sram, size);
	return 0;
}

static int arch_flush_mailbox(char *mbox, size_t len)
{
	volatile void __iomem *hw_mbox_sram;
	size_t sram_size;

	hw_mbox_sram = avm_hw_mbox_private_sram(&sram_size);
	if (!hw_mbox_sram)
		return -ENODEV;
	BUG_ON(len > sram_size);
	memcpy_toio(hw_mbox_sram, mbox, len);
	return 0;
}

static int arch_x86_die_notifier(struct notifier_block *self,
				 unsigned long cmd,
				 void *ptr)
{
	struct die_args *args = (struct die_args *)ptr;
	struct pt_regs *regs = args->regs;

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
