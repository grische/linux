/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_h__
#define __arch_profile_h__

#if defined(CONFIG_SOC_GRX500)
/*--- GRX ---*/
#include "arch_grx.h"

#elif defined(CONFIG_ARCH_IPQ40XX)
/*--- IPQ40XX ---*/
#include "arch_cortexa9.h"
#elif defined(CONFIG_MACH_BCM963138) || defined(CONFIG_BCM963178) || defined(CONFIG_BCM96764) || defined(CONFIG_BCM96766)
/*--- Broadcom ---*/
#include "arch_brcma.h"
#elif defined(CONFIG_X86)
#include "arch_x86.h"
#elif defined(CONFIG_MACH_PUMA7)
#include "arch_pumaX.h"
#elif defined(CONFIG_MIPS)
#if defined(ARCH_MIPS_PROFILE_C)
/*--- AR9/Atheros ---*/
static const struct _cpucore_profile mips_cpu_config[1] = {
	{ .cpu_nr_offset = 0,
	  .vpe_nr = 1,
	  .linux_os_mask = 0x1,
	  .next_core = NULL },
};
#endif /*--- #if defined(ARCH_MIPS_PROFILE_C) ---*/
#endif /*--- #if defined(CONFIG_MIPS) ---*/

#if !defined(THREAD_MASK) /*--- THREAD_MASK gibt es nur bei MIPS ---*/
#define THREAD_MASK (THREAD_SIZE - 1)
#endif

#if defined(CONFIG_MIPS)
#include <asm/mipsregs.h>
#define PROFILING_BACKTRACE
extern void __avm_simple_profiling_backtrace_from_other_context(unsigned long pc, unsigned long lr, unsigned int sp,
								struct task_struct *curr_tsk,
								int cpu_id, int core, int tc_id, int no_backtrace);
/**
 */
extern void arch_mips_backtrace(unsigned int sp, unsigned int pc,
				unsigned int lr, unsigned int *bt, int size_bt);

/**
 */
static inline void arch_backtrace(unsigned int sp, unsigned int pc,
				  unsigned int lr, unsigned int *bt,
				  int size_bt)
{
#if defined(CONFIG_KALLSYMS)
	arch_mips_backtrace(sp, pc, lr, bt, size_bt);
#endif /*--- #if defined(CONFIG_KALLSYMS) ---*/
}

#define PROFILING_PERFORMANCE_COUNTER_SUPPORT
/**
 */
static inline unsigned int arch_profile_perfcnt1(void)
{
	return read_c0_perfcntr0();
}

/**
 */
static inline unsigned int arch_profile_perfcnt2(void)
{
	return read_c0_perfcntr1();
}

/**
 */
static inline unsigned int arch_profile_sp(void)
{
	unsigned int sp;

	__asm__ __volatile__(".set push\n\t"
			     ".set noat\n\t"
			     "sw $29, %0\n\t"
			     ".set pop\n\t"
			     : "=m"(sp)
			     : /* no inputs */
			     : "memory");

	return sp;
}

#if defined(ARCH_MIPS_PROFILE_C)

/**
 */
int arch_is_linux_cpu(unsigned int core, unsigned int tc)
{
	if (core >= ARRAY_SIZE(mips_cpu_config)) {
		return 0;
	}
	return mips_cpu_config[core].linux_os_mask & (1 << tc) ? 1 : 0;
}
#else /*--- #if defined(ARCH_MIPS_PROFILE_C) ---*/
extern int arch_is_linux_cpu(unsigned int core, unsigned int tc);
#endif /*--- #else ---*/ /*--- #if defined(ARCH_MIPS_PROFILE_C) ---*/

#endif /*--- #if defined(CONFIG_MIPS) ---*/

#if defined(CONFIG_ARM) | defined(CONFIG_X86)
/**
 */
static inline unsigned int arch_profile_sp(void)
{
	register unsigned long sp asm("sp");

	return sp;
}
#endif /*--- #if defined(CONFIG_ARM) | defined(CONFIG_X86) ---*/

#if !defined(PROFILING_PERFORMANCE_COUNTER_SUPPORT)
/**
 */
static inline void arch_profile_perfcnt_on(unsigned int on)
{
	pr_err("%s: no support for performance-counter\n", __func__);
}

/**
 */
static inline unsigned int arch_profile_perfcnt1(void)
{
	return 0;
}

/**
 */
static inline unsigned int arch_profile_perfcnt2(void)
{
	return 0;
}

/**
 */
static inline int arch_is_linux_cpu(unsigned int core, unsigned int tc)
{
	return 1;
}
#endif /*--- #if !defined(PROFILING_PERFORMANCE_COUNTER_SUPPORT) ---*/

#if defined(CONFIG_MIPS)
#define KSTK_RA(tsk) (task_pt_regs(tsk)->regs[31])
#elif defined(CONFIG_ARM)
#define KSTK_RA(tsk) (task_pt_regs(tsk)->ARM_lr)
#elif defined(CONFIG_X86)
#include <asm/stacktrace.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
#define get_frame_pointer(...) stack_frame(__VA_ARGS__)
#endif
static inline unsigned long get_ra_from_bp(struct task_struct *tsk,
					   unsigned long bp)
{
	struct stack_frame *frame;
	unsigned long stack;

	if (tsk) {
		frame = (struct stack_frame *)get_frame_pointer(tsk, NULL);
		if (frame) {
			return (unsigned long)frame->return_address;
		}
	} else {
		void *p, *t;
		struct thread_info *context;

		context = (struct thread_info *)((unsigned long)&stack &
						 (~(THREAD_SIZE - 1)));
		frame = (struct stack_frame *)bp;

		p = frame;
		t = context;
		if ((p > t) &&
		    (p < (t + THREAD_SIZE - sizeof(struct stack_frame)))) {
			/*--- valid kernel frame-pointer ---*/
			return (unsigned long)frame->return_address;
		}
	}
	return 0;
}

#define KSTK_RA(tsk) get_ra_from_bp(tsk, task_pt_regs(tsk)->bp)
#else
#define KSTK_RA(tsk) NULL
#endif

#ifndef PROFILING_BACKTRACE

static inline void arch_backtrace(unsigned int sp,
				  unsigned int pc,
				  unsigned int lr,
				  unsigned int *bt,
				  int size_bt)
{
}
#endif

#endif /*--- #ifndef __arch_profile_h__ ---*/
