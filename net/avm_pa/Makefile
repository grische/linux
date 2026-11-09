#
# Makefile for the AVM Packet Accelerator
#

# KBUILD_CFLAGS	+= -ggdb -fno-function-sections

obj-y += avm_pa.o
obj-y += avm_pa_frag.o
obj-y += avm_pa_netdev.o
obj-y += avm_pa_selector.o
obj-y += avm_pa_sessiongroups.o
obj-$(CONFIG_AVM_PA_SCH_LLQ)       += sch_llq.o
obj-$(CONFIG_AVM_PA_SCH_TACK)      += sch_tack.o
obj-$(CONFIG_AVM_PA_GENERIC_CT)    += generic_ct/
obj-$(CONFIG_AVM_PA_SCH_CAKE)         += sch_cake/

# print out last tag upon boot
AVM_PA_VERSION=$(shell git -C $(srctree)/$(src) describe --tags || echo unknown)
ccflags-y += -DAVM_PA_VERSION=\"$(AVM_PA_VERSION)\"
