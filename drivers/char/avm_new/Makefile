# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de>
#
#   vim: noexpandtab

ccflags-y += -I$(src)

obj-$(CONFIG_AVM_SAMMEL) += avm.o

# Note, ordering in Makefiles implies linking order,
# which implies ordering of initcalls.

avm-objs  = avm_sammel.o
avm-objs += avm_factory_reset.o
avm-objs += avm_helpers.o
avm-objs += avm_irq_on.o
avm-objs += avm_oom_status.o
avm-objs += avm_page_statistic.o
obj-$(CONFIG_AVM_SAMMEL) += reboot_status/
avm-objs += avm_stack_usage.o
avm-$(CONFIG_AVM_RTE) += simple_mempool.o
avm-objs += simple_proc.o
avm-objs += prom_config_procfs.o
avm-objs += prom_config.o
avm-objs += $(avm-objs-y)

export IS_BRCM := $(if $(CONFIG_BCM_CHIP_NUMBER),y)

### AVM Simple Profiling
obj-$(CONFIG_AVM_SIMPLE_PROFILING) += profiling/

### AVM Event
obj-$(CONFIG_AVM_EVENT) += event/

# FIXME: remotecpu_irq.o should probably depend on something else
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA6) += remotecpu_irq.o
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA7) += remotecpu_irq.o

avm-objs-$(IS_BRCM) += avm_dma_device.o

# Must follow the event code in linking order, since it depends on it.
obj-$(CONFIG_AVM_WATCHDOG) += watchdog/

avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA7) += checkpoint_proc.o
avm-objs-$(CONFIG_AVM_EVENTNODE_PUMA6) += checkpoint_proc.o


### AVM Debug
avm-objs-$(CONFIG_AVM_DEBUG) += avm_debug.o avm_logger.o
avm-objs-$(CONFIG_AVM_DEBUG_CONSOLE) += avm_console.o


### AVM Net Event
avm-objs-$(CONFIG_AVM_NET_EVENT) += avm_net_event.o

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

### AVM Companion CPU Panic via GPIO
avm-objs-$(CONFIG_AVM_COMPANION_PANIC) += avm_companion_panic.o

### AVM Devicetree Urlader Environment
avm-objs-$(CONFIG_AVM_DT_URLADER_ENV) += dt_urlader_env.o

obj-y += enh/
