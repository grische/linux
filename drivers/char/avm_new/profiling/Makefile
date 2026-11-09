obj-$(CONFIG_AVM_SIMPLE_PROFILING) += profiling.o

profiling-y += proc.o
profiling-y += simple_profile.o
profiling-y += stat.o

profiling-$(CONFIG_MIPS)        += arch_mips.o
profiling-$(CONFIG_ARM)         += arch_arm.o
profiling-$(CONFIG_MACH_PUMA6)  += arch_pumaX.o
profiling-$(CONFIG_MACH_PUMA7)  += arch_pumaX.o
profiling-$(IS_BRCM)            += arch_brcma.o
profiling-$(CONFIG_ARCH_IPQ40XX) += arch_cortexa9.o
profiling-$(CONFIG_X86)         += arch_x86.o
profiling-$(CONFIG_MIPS)        += arch_mips_backtrace.o

