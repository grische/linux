/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_ARCH_ARM_H
#define AVM_NEW_REBOOT_STATUS_ARCH_ARM_H

#include <linux/io.h>

#include <asm/cacheflush.h>

static void *arch_map_mailbox(phys_addr_t start, size_t size)
{
	return memremap(start, size, MEMREMAP_WB);
}

static int arch_flush_mailbox(char *mbox, size_t len)
{
	dmac_flush_range(mbox, mbox + len);
	return 0;
}

#endif
