ccflags-y += -I$(src)/..

obj-$(CONFIG_AVM_WATCHDOG) += avmwdt.o

avmwdt-y += main.o
avmwdt-y += file.o

avmwdt-$(CONFIG_AVM_WATCHDOG_SHIM)      += hw_shim.o

avmwdt-$(CONFIG_AVM_WATCHDOG_GRX500)    += hw_grx.o
avmwdt-$(CONFIG_AVM_WATCHDOG_IFXMIPS)   += hw_ifxmips.o
avmwdt-$(CONFIG_ATH79)                  += hw_ath.o
avmwdt-$(IS_BRCM)                       += hw_brcm.o
avmwdt-$(CONFIG_ARCH_IPQ40XX)           += hw_qcom.o
avmwdt-$(CONFIG_ARCH_IPQ5018)           += hw_qcom.o

avmwdt-$(CONFIG_ARCH_AVALANCHE)         += hw_puma.o
avmwdt-$(CONFIG_X86_PUMA6)              += hw_remote.o
