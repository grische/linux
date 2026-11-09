/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef AVM_NEW_REBOOT_STATUS_CONFIG_DEVICE_TREE_H
#define AVM_NEW_REBOOT_STATUS_CONFIG_DEVICE_TREE_H

#include <linux/of.h>
#include <linux/of_reserved_mem.h>

/* required to be implemented by the architecture specific header */
static void *arch_map_mailbox(phys_addr_t start, size_t size);
static int arch_flush_mailbox(char *mbox, size_t len);

static void __iomem *mbox_virt;

static struct {
	size_t size;
	phys_addr_t base;
} mbox_phys;

static int __init mbox_map(void)
{
	if (!mbox_phys.size || !mbox_phys.base) {
		pr_err("reboot_string: found no reboot string location in the device tree\n");
		return -ENODEV;
	}

	mbox_virt = arch_map_mailbox(mbox_phys.base, mbox_phys.size);
	if (!mbox_virt) {
		pr_err("reboot_string: failed to map %zu@%pa\n", mbox_phys.size,
		       &mbox_phys.base);
		return -ENODEV;
	}

	pr_info("reboot_string: mapped %zu@%pa\n", mbox_phys.size,
		&mbox_phys.base);
	return 0;
}
core_initcall(mbox_map);

static int __init mbox_setup(struct reserved_mem *rmem)
{
	mbox_phys.size = rmem->size;
	mbox_phys.base = rmem->base;

	return 0;
}
RESERVEDMEM_OF_DECLARE(mbox, "avm,reboot_string", mbox_setup);

static char *arch_get_mailbox(void)
{
	return mbox_virt;
}

#endif
