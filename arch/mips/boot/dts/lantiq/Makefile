dtb-$(CONFIG_DT_EASY50712)	+= easy50712.dtb
dtb-$(CONFIG_DT_ANYWAN)	+= easy350_anywan.dtb
dtb-$(CONFIG_DT_EASY350550_BOOTCORE)	+= easy350550_bootcore.dtb
obj-$(CONFIG_BUILTIN_DTB)	    += $(addsuffix .o, $(dtb-y))

subdir-$(CONFIG_GRX5_AVM_FRITZ_BOX)	+= avm
subdir-$(CONFIG_PRX_AVM_FRITZ_BOX)	+= avm
obj-$(CONFIG_BUILTIN_DTB)		+= $(addsuffix /, $(subdir-y))
