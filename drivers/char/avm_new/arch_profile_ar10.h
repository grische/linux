/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_ar10_h__
#define __arch_profile_ar10_h__

#include <asm/time.h>
#include <asm/yield_context.h>
#include <ifx_gptu.h>

#define PROFILING_IN_YIELD
#define PROFILING_CORES 1
#define PROFILING_MAX_COUNT_TCS 3
#define PROFILING_CPU_HAS_TC

#define PROFILING_MAX_PERF_REGISTER 2 /*--- 2 x Perf-counter ---*/

#if defined(ARCH_MIPS_PROFILE_C)
static const struct _cpucore_profile mips_cpu_config[1] = {
	{ .cpu_nr_offset = 0,
	  .vpe_nr = 2,
	  .linux_os_mask = (0x1 << 0) | (0x1 << 1),
	  .next_core = NULL },
};

static unsigned int next_cycle;

/**
 */
static inline void arch_init_mips_cpu_config(void)
{
	cpu_nr_to_tc_and_core[0].core = 0;
	cpu_nr_to_tc_and_core[0].tc = 0;
	cpu_nr_to_tc_and_core[1].core = 0;
	cpu_nr_to_tc_and_core[1].tc = 1;
}

/**
 */
static inline void arch_set_next_trigger(unsigned int next_cnt)
{
	next_cycle = avm_get_cycles() + next_cnt;
}

/**
 */
int arch_trigger_valid(void)
{
	return ((next_cycle - avm_get_cycles()) >= (1U << 31));
}

/**
 */
static inline void
arch_next_trigger_for_cpu(int cpu, unsigned int next_cnt)
{
}

#define YIELD_PROFILE_IPI_ID(core)  0			    /*--- dummy ---*/
#define YIELD_SIGNAL_BY_ID(id)      YIELD_SIGNAL_COP0_COMPARE
#define YIELD_CPU_BY_ID(id)         0			    /*--- dummy ---*/
#define YIELD_TC_BY_ID(id)          2

#define PROFILING_USEC_TO_TIMER(usec) ((usec)*250)

#define PROFILING_TRIGGER_SHIFT 18
#define PROFILING_TRIGGER_MASK                                                 \
	((1 << PROFILING_TRIGGER_SHIFT) -                                      \
	 1) /*--- Range: (1 << 18) / 250  = 1048 us ---*/

/**
 */
static inline int arch_yield_map_setup(unsigned int cpu,
				       unsigned int irq,
				       unsigned int mode,
				       unsigned int pin)
{
	return 0;
}

/**
 * dummy
 */
static inline int arch_uart_init(unsigned int core, unsigned int baud)
{
	return 1;
}

/**
 */
static inline void arch_uart_send_byte(unsigned int core, unsigned char value)
{
}

/**
 */
static inline void arch_uart_send_word(unsigned int core, unsigned int val32)
{
}

/**
 */
static inline int
request_yield_handler_on(int cpu, int tc, int signal,
			 int (*yield_handler)(int signal, void *ref), void *ref)
{
	return request_yield_handler(signal, yield_handler, ref);
}

/**
 */
static inline int free_yield_handler_on(int cpu, int tc, int signal, void *ref)
{
	return free_yield_handler(signal, ref);
}

/**
 */
void enable_yield_handler_on(int cpu, int tc, int signal)
{
	enable_yield_handler(signal);
}

/**
 */
void disable_yield_handler_on(int cpu, int tc, int signal)
{
	disable_yield_handler(signal);
}
#endif /*--- #if defined(ARCH_MIPS_PROFILE_C) ---*/

#endif /*--- #ifndef __arch_profile_ar10_h__ ---*/
