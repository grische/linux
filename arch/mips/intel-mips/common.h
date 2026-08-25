/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Shared declarations for the Intel MIPS interAptiv platform code.
 *
 * Copyright (C) 2016 Intel Corporation.
 */
#ifndef __INTEL_MIPS_COMMON_H
#define __INTEL_MIPS_COMMON_H

#ifdef CONFIG_INTEL_MIPS_VMB_SMP
int register_vmb_smp_ops(void);
#else
static inline int register_vmb_smp_ops(void)
{
	return -ENODEV;
}
#endif

#endif /* __INTEL_MIPS_COMMON_H */
