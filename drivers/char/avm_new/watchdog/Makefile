obj-$(CONFIG_AVM_WATCHDOG) += avmwdt.o

avmwdt-y += main.o
avmwdt-y += file.o

avmwdt-$(CONFIG_AVM_WATCHDOG_SHIM)      += hw_shim.o

avmwdt-$(CONFIG_AVM_WATCHDOG_GRX500)    += hw_grx.o
avmwdt-$(CONFIG_ATH79)                  += hw_ath.o
ifeq ($(CONFIG_AVM_GENERIC_RTE),y)
avmwdt-$(CONFIG_AVM_ARCH_BCM)  		+= hw_brcm.o hw_fiq_generic.o
else
avmwdt-$(CONFIG_AVM_ARCH_BCM)  		+= hw_brcm_legacy.o
endif
avmwdt-$(CONFIG_ARCH_QCOM)              += hw_qcom.o
avmwdt-$(CONFIG_RTK_RLX)                += hw_rtk.o
avmwdt-$(CONFIG_ARCH_ECONET)            += hw_arht.o

avmwdt-$(CONFIG_ARCH_AVALANCHE)         += hw_puma.o
