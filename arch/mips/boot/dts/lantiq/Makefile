# SPDX-License-Identifier: GPL-2.0
dtb-$(CONFIG_DT_EASY50712)	+= easy50712.dtb
dtb-$(CONFIG_DT_ANYWAN)	+= easy350_anywan.dtb
dtb-$(CONFIG_DT_EASY350550_BOOTCORE)	+= easy350550_bootcore.dtb
dtb-$(CONFIG_DT_FALCONMXHAPS_BOOTCORE)	+= falconmx_haps_bootcore.dtb
dtb-$(CONFIG_DT_FALCONMX_SFU_BOOTCORE)	+= falconmx_sfu_bootcore.dtb

obj-$(CONFIG_BUILTIN_DTB)	+= $(addsuffix .o, $(dtb-y))
