// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2022 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/sched.h>
#include <avm/compat/misc.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
static void format_callback(struct erase_info *instr)
{
	struct mtd_erase_waiter *waiter = (void *)instr->priv;

	switch (instr->state) {
	case MTD_ERASE_PENDING:
	case MTD_ERASING:
	case MTD_ERASE_SUSPEND:
		break;
	case MTD_ERASE_FAILED:
	case MTD_ERASE_DONE:
		wake_up(&waiter->wait_q);
		break;
	}
}

void mtd_prepare_erase_compat(struct erase_info *instr,
			      struct mtd_info *mtd,
			      struct mtd_erase_waiter *waiter)
{
	init_waitqueue_head(&waiter->wait_q);

	instr->mtd      = mtd;
	instr->callback = format_callback;
	instr->next	= NULL;
	instr->priv	= (u_long)waiter;
}

int mtd_erase_compat(struct mtd_info *mtd,
		     struct erase_info *instr,
		     struct mtd_erase_waiter *waiter)
{
	int ret;

	pr_debug("Erasing %llu bytes at offset 0x%llx\n", instr->len, instr->addr);
	ret = mtd_erase(mtd, instr);
	if (ret)
		return -EIO;

	wait_event(waiter->wait_q, instr->state == MTD_ERASE_DONE ||
				   instr->state == MTD_ERASE_FAILED);

	return instr->state == MTD_ERASE_FAILED ? -EIO : 0;
}
#endif
