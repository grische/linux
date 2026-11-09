# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2015 AVM GmbH <fritzbox_info@avm.de>

# Each configuration option enables a list of files.

obj-$(CONFIG_TFFS)	+= init.o   \
                       fops.o   \
                       core.o   \
                       proc.o   \
                       stat.o

obj-$(CONFIG_TFFS_DEV_MTDNAND)	+= nand_stat.o

# FIXME: Temporary hack to keep nand.c clean from no oob stuff.
ifeq ($(CONFIG_TFFS_NAND_V31),y)
	obj-$(CONFIG_TFFS_DEV_MTDNAND)      += nand_noob.o
else
	obj-$(CONFIG_TFFS_DEV_MTDNAND)      += nand.o
endif
obj-$(CONFIG_TFFS_DEV_LEGACY)       += legacy.o
obj-$(CONFIG_TFFS_DEV_CLIENT)       += remote_client.o
obj-$(CONFIG_TFFS_DEV_SERVER)       += remote_srv.o
obj-$(CONFIG_TFFS_DEV_CACHE)        += cache.o
obj-$(CONFIG_TFFS_DEV_EFI)          += efi.o
obj-$(CONFIG_TFFS_EFI_SYNC)         += efi_sync.o
obj-$(CONFIG_TFFS_ENV)              += env.o
obj-$(CONFIG_TFFS_PANIC_LOG)        += panic.o
