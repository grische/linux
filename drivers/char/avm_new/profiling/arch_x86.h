/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_x86_h__
#define __arch_profile_x86_h__

#include <asm/time.h>
#include <asm/mach_avm.h>
#include <linux/avm_hw_config.h>

#define PROFILING_IN_NMI
#define PROFILING_PERIOD_USEC   10UL
#define PROFILING_PERIOD_BASE   (100UL)         // 100 us minimum
#define PROFILING_PERIOD_MASK   ((1 << 10) - 1) // 1023 us max random
#define PROFILING_USEC_TO_TIMER(usec) ((usec) * PROFILING_PERIOD_USEC)
#endif /*--- #ifndef __arch_profile_x86_h__ ---*/
