#!/bin/bash
# /*------------------------------------------------------------------------------------------*\
# *   
# *   Copyright (C) 2015 AVM GmbH <fritzbox_info@avm.de>
# *
# *   This program is free software; you can redistribute it and/or modify
# *   it under the terms of the GNU General Public License as published by
# *   the Free Software Foundation; either version 2 of the License, or
# *   (at your option) any later version.
# *
# *   This program is distributed in the hope that it will be useful,
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *   GNU General Public License for more details.
# *
# *   You should have received a copy of the GNU General Public License
# *   along with this program; if not, write to the Free Software
# *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
# \*------------------------------------------------------------------------------------------*/

# Each configuration option enables a list of files.

obj-$(CONFIG_TFFS)	+= tffs_init.o   \
                       tffs_fops.o   \
                       tffs_core.o   \
                       tffs_intern.o \
                       tffs_proc.o   \
                       tffs_stat.o 

# FIXME: Temporary hack to keep tffs_nand.c clean from no oob stuff.
ifeq ($(CONFIG_TFFS_NAND_V31),y)
	obj-$(CONFIG_TFFS_DEV_MTDNAND)      += tffs_nand_noob.o
else
	obj-$(CONFIG_TFFS_DEV_MTDNAND)      += tffs_nand.o
endif
obj-$(CONFIG_TFFS_DEV_LEGACY)       += tffs_legacy.o
obj-$(CONFIG_TFFS_DEV_REMOTE)       += tffs_remote.o tffs_remote_srv.o
obj-$(CONFIG_TFFS_DEV_CACHE)        += tffs_cache.o
obj-$(CONFIG_TFFS_DEV_EFI)          += tffs_efi.o
obj-$(CONFIG_TFFS_EFI_SYNC)         += tffs_efi_sync.o
obj-$(CONFIG_TFFS_PANIC_LOG)        += tffs_panic.o

extra-y         += supportdata

quiet_cmd_install = INSTALL $< -> $@
      cmd_install = ( test -d $(dir $@) || mkdir -p $(dir $@) ) && cp -p $< $@

quiet_cmd_set_executable = CHMOD+x $@
      cmd_set_executable = chmod +x $@
      
$(INSTALL_MOD_PATH)/bin/supportdata.tffs: $(src)/supportdata.tffs
	$(call if_changed,install)
	$(call if_changed,set_executable)

PHONY += $(obj)/supportdata
$(obj)/supportdata: $(INSTALL_MOD_PATH)/bin/supportdata.tffs
      
