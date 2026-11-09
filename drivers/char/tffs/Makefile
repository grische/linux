# SPDX-License-Identifier: GPL-2.0+
# Copyright (C) 2015 AVM GmbH <fritzbox_info@avm.de>

# Each configuration option enables a list of files.

obj-$(CONFIG_TFFS)	+= init.o   \
                       fops.o   \
                       core.o   \
                       intern.o \
                       proc.o   \
                       stat.o

# FIXME: Temporary hack to keep nand.c clean from no oob stuff.
ifeq ($(CONFIG_TFFS_NAND_V31),y)
	obj-$(CONFIG_TFFS_DEV_MTDNAND)      += nand_noob.o
else
	obj-$(CONFIG_TFFS_DEV_MTDNAND)      += nand.o
endif
obj-$(CONFIG_TFFS_DEV_LEGACY)       += legacy.o
obj-$(CONFIG_TFFS_DEV_REMOTE)       += remote_client.o remote_srv.o
obj-$(CONFIG_TFFS_DEV_CACHE)        += cache.o
obj-$(CONFIG_TFFS_DEV_EFI)          += efi.o
obj-$(CONFIG_TFFS_EFI_SYNC)         += efi_sync.o
obj-$(CONFIG_TFFS_PANIC_LOG)        += panic.o

extra-y         += supportdata

quiet_cmd_install = INSTALL $< -> $@
      cmd_install = ( test -d $(dir $@) || mkdir -p $(dir $@) ) && cp -p $< $@

quiet_cmd_set_executable = CHMOD+x $@
      cmd_set_executable = chmod +x $@

$(INSTALL_MOD_PATH)/bin/supportdata.tffs: $(src)/supportdata.tffs FORCE
	$(call if_changed,install)
	$(call if_changed,set_executable)

PHONY += $(obj)/supportdata
$(obj)/supportdata: $(INSTALL_MOD_PATH)/bin/supportdata.tffs

