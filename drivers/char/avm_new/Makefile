# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de>
#
#   vim: noexpandtab

obj-$(CONFIG_AVM_SAMMEL) += avm.o


avm-objs  = avm_sammel.o
avm-objs += avm_factory_reset.o
avm-objs += avm_helpers.o
avm-objs += avm_irq_on.o
avm-objs += avm_oom_status.o
avm-objs += avm_page_statistic.o
avm-objs += avm_reboot_status.o
avm-objs += avm_stack_usage.o
avm-objs += simple_mempool.o
avm-objs += simple_proc.o
avm-objs += prom_config_procfs.o
avm-objs += prom_config.o
avm-objs += $(avm-objs-y)


### AVM Simple Profiling
ifeq ($(CONFIG_AVM_SIMPLE_PROFILING),y)
avm-objs-y += avm_proc_profile.o
avm-objs-y += simple_profile.o
avm-objs-y += avm_profile_stat.o

IS_BRCM := $(if $(CONFIG_BCM_CHIP_NUMBER),y)

avm-objs-$(CONFIG_MIPS) += arch_mips_profile.o
avm-objs-$(CONFIG_ARM) += arch_arm_profile.o
avm-objs-$(CONFIG_MACH_PUMA6) += arch_profile_pumaX.o
avm-objs-$(CONFIG_MACH_PUMA7) += arch_profile_pumaX.o
avm-objs-$(IS_BRCM) += arch_profile_brcma.o
avm-objs-$(CONFIG_ARCH_IPQ40XX) += arch_profile_cortexa9.o
avm-objs-$(CONFIG_X86) += arch_x86_profile.o
avm-objs-$(CONFIG_MIPS) += arch_mips_backtrace.o
endif # ifeq ($(CONFIG_AVM_SIMPLE_PROFILING),y)


### AVM Event
ifeq ($(CONFIG_AVM_EVENT),y)
avm-objs-y += avm_event_main.o
avm-objs-y += avm_event_file.o
avm-objs-y += avm_event_alloc.o

avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA6) += avm_event_node_pumaX.o
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA6) += remotecpu_irq.o
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA6) += avm_event_remote.o

avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA7) += avm_event_node_pumaX.o
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA7) += remotecpu_irq.o
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA7) += avm_event_remote.o
endif


### AVM Event 2.0
obj-$(CONFIG_AVM_EVENT_20) += avm_dist_event/


avm-objs-$(IS_BRCM) += avm_dma_device.o

### AVM Watchdog
ifeq ($(CONFIG_AVM_WATCHDOG),y)
avm-objs-y += ar7wdt_main.o
avm-objs-y += ar7wdt_file.o

avm-objs-$(CONFIG_AVM_WATCHDOG_SHIM) += ar7wdt_hw_shim.o

avm-objs-$(CONFIG_MACH_PUMA6) += ar7wdt_hw_puma.o checkpoint_proc.o

# CONFIG_ARCH_AVALANCHE=y => Puma 7/ARM
avm-objs-$(CONFIG_ARCH_AVALANCHE) += ar7wdt_hw_puma.o checkpoint_proc.o

avm-objs-$(CONFIG_AVM_WATCHDOG_GRX500) += grx_wdt.o

avm-objs-$(CONFIG_AVM_WATCHDOG_IFXMIPS) += ifxmips_wdt.o

avm-objs-$(CONFIG_ATH79) += ath_wdt.o
# On Wasp, Honeybee and Scorpion access to invalid addresses may freeze the cpu
# This makes using the nmi watchdog very unrelaiable, so we set this to reset.
ccflags-$(CONFIG_SOC_AR934X) += -DATH_NO_NMI
ccflags-$(CONFIG_SOC_QCA953X) += -DATH_NO_NMI
ccflags-$(CONFIG_SOC_QCA955X) += -DATH_NO_NMI


avm-objs-$(CONFIG_X86) += checkpoint_proc.o
ifneq ($(CONFIG_MACH_PUMA7),y)
avm-objs-$(CONFIG_X86) += ar7wdt_hw_wdtremote.o checkpoint_proc.o
endif

avm-objs-$(IS_BRCM) += brcm_wdt.o
avm-objs-$(CONFIG_ARCH_IPQ40XX) += qcom_wdt.o
endif


### AVM Debug
avm-objs-$(CONFIG_AVM_DEBUG) += avm_debug.o avm_logger.o
avm-objs-$(CONFIG_AVM_DEBUG_CONSOLE) += avm_console.o


### AVM Net Event
avm-objs-$(CONFIG_AVM_NET_EVENT) += avm_net_event.o


### special compile flag handling
ccflags-y	+= -Wall -Wextra -Wno-unused-parameter
ccflags-$(CONFIG_AVM_EVENT_20) += -I$(objtree)/$(obj)/avm_dist_event/

### AVM KPI
# Test module wird nur gebaut und kann zum testen auf die box geladen werden
avm-objs-$(CONFIG_AVM_KPI) += avm_kpi.o
obj-$(if $(CONFIG_AVM_KPI),m) += avm_kpi_test.o

ifeq ($(CONFIG_AVM_KPI),y)
avm-objs-$(CONFIG_ARCH_IPQ40XX) += arch_dakota_kpi.o
endif

### AVM Core Debug
avm_core_debug-objs = avm_proc_call.o
obj-$(if $(CONFIG_AVM_SAMMEL),m) += avm_core_debug.o

obj-y += enh/
