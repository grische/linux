/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_ARCH_ARM64_H
#define AVM_NEW_REBOOT_STATUS_ARCH_ARM64_H

#include <linux/io.h>

#include <asm/cacheflush.h>

static void *arch_map_mailbox(phys_addr_t start, size_t size)
{
	return memremap(start, size, MEMREMAP_WB);
}

static int arch_flush_mailbox(char *mbox, size_t len)
{
	/* This will be renamed in 5.14 / fade9c2c6ee2 ("arm64: Rename arm64-internal cache
	 * maintenance functions")
	 */
	__flush_dcache_area(mbox, len);
	return 0;
}

#endif
