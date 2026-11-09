ccflags-y	+= -Werror

ifeq ($(CONFIG_AVM_FW_INFO),y)
obj-y += fw_info.o
else
obj-y += no_fw_info.o
endif

obj-y += semi_seq.o

obj-$(CONFIG_AVM_PROM_ENVIRONMENT) += fw_info_common.o

obj-$(CONFIG_AVM_MODULE_MEM) += module_mem.o
