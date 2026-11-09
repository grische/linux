/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_vr9_h__
#define __arch_profile_vr9_h__

#include <asm/time.h>
#include <asm/yield_context.h>
#include <ifx_gptu.h>

#ifndef YIELD_TC_SCHED_PRIORITY_MAX
#define YIELD_TC_SCHED_PRIORITY_MAX 3
#endif
#ifndef YIELD_TC_SCHED_PRIORITY_MED
#define YIELD_TC_SCHED_PRIORITY_MED 2
#endif
#ifndef YIELD_TC_SCHED_PRIORITY_LOW
#define YIELD_TC_SCHED_PRIORITY_LOW 1
#endif
#ifndef YIELD_TC_SCHED_GROUP_MASK
#define YIELD_TC_SCHED_GROUP_MASK 0x3
#endif

#define PROFILING_IN_YIELD
#define PROFILING_CORES 1
#define PROFILING_MAX_COUNT_TCS 4
#define PROFILING_CPU_HAS_TC

#define PROFILING_MAX_PERF_REGISTER 2 /*--- 2 x Perf-counter ---*/

#if defined(ARCH_MIPS_PROFILE_C)
static const struct _cpucore_profile mips_cpu_config[1] = {
	{ .cpu_nr_offset = 0,
	  .vpe_nr = 2,
	  .linux_os_mask = (0x1 << 0) | (0x1 << 1),
	  .next_core = NULL },
};

static int yield_timer_no = -1;

/**
 */
static inline void arch_init_mips_cpu_config(void)
{
	cpu_nr_to_tc_and_core[0].core = 0;
	cpu_nr_to_tc_and_core[1].core = 0;
	cpu_nr_to_tc_and_core[0].tc = 0;
	cpu_nr_to_tc_and_core[1].tc = 1;
}

/**
 */
static inline void arch_set_next_trigger(unsigned int next_cnt)
{
	ifx_gptu_timer_yield_ack(yield_timer_no, next_cnt);
}

/**
 * nur dummy
 */
int arch_trigger_valid(void)
{
	return 1;
}

/**
 */
static inline void arch_next_trigger_for_cpu(int cpu, unsigned int next_cnt)
{
	arch_set_next_trigger(next_cnt);
}

#define YIELD_PROFILE_IPI_ID(core)  0			    /*--- dummy ---*/
#define YIELD_SIGNAL_BY_ID(id)      YIELD_SIGNAL_TIMER1
#define YIELD_CPU_BY_ID(id)         0			    /*--- dummy ---*/
#define YIELD_TC_BY_ID(id)                                                     \
	((LANTIQ_YIELD_MASK_TC1 & LANTIQ_YIELD_MASK_PROFILING) ? 2 : 3)

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
	if ((mode == 2) && (yield_timer_no < 0)) {
		/*--- printk(KERN_ERR "%s: setup timer\n", __func__); ---*/
		yield_timer_no = ifx_gptu_timer_set(
			0, 1000, 1, 0, TIMER_FLAG_YIELD_MODE, 0, 0);

		switch (yield_timer_no) {
		case TIMER1A:
			YIELDEN_GPCT_1A(1);
			break;
		case TIMER1B:
			YIELDEN_GPCT_1B(1);
			break;
		case TIMER2A:
			YIELDEN_GPCT_2A(1);
			break;
		case TIMER2B:
			YIELDEN_GPCT_2B(1);
			break;
		case TIMER3A:
			YIELDEN_GPCT_3A(1);
			break;
		case TIMER3B:
			YIELDEN_GPCT_3B(1);
			break;
		default:
			pr_err("%s: could not reserve valid HW Timer: %d\n",
			       __func__, yield_timer_no);
			return -EBUSY;
		}
		ifx_gptu_timer_start(yield_timer_no, 0);
		/*--- printk(KERN_ERR "%s: request_yield_handler successful timer_no= %d\n", __func__, yield_timer_no); ---*/
	} else if (yield_timer_no >= 0) {
		switch (yield_timer_no) {
		case TIMER1A:
			YIELDEN_GPCT_1A(0);
			break;
		case TIMER1B:
			YIELDEN_GPCT_1B(0);
			break;
		case TIMER2A:
			YIELDEN_GPCT_2A(0);
			break;
		case TIMER2B:
			YIELDEN_GPCT_2B(0);
			break;
		case TIMER3A:
			YIELDEN_GPCT_3A(0);
			break;
		case TIMER3B:
			YIELDEN_GPCT_3B(0);
			break;
		}
		ifx_gptu_timer_stop(yield_timer_no);
		ifx_gptu_timer_free(yield_timer_no);
		yield_timer_no = -1;
	}
	return 0;
}

/**
 * dummy
 */
static inline unsigned int arch_get_profile_irq(void)
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

static inline void arch_uart_send_byte(unsigned int core, unsigned char value)
{
}

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

#endif /*--- #ifndef __arch_profile_vr9_h__ ---*/
