/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_pumaX_h__
#define __arch_profile_pumaX_h__

#include <asm/performance.h>
#include <linux/cpumask.h>

enum _norm_factor {
	NORMED_BY_FREQUENCY = 0,
	NORMED_BY_CYCLE,
	NORMED_BY_INSTRUCTION,
	NORMED_MAX
};

struct _perfcount_options {
	char *name;
	enum _norm_factor norm_factor;
};

#if defined(CONFIG_AVM_FIQ_PUMA7)
#define PROFILING_IN_FIQ
#define PROFILING_MAX_PERF_REGISTER                                            \
	(2 + 1) /* we treat the cycle counter like a performance counter */
#define PROFILING_PERF_REGISTERMASK ((1 << 2) - 1)
#define PROFILING_CYCLE_REGISTER    (PROFILING_MAX_PERF_REGISTER - 1)

#define PROFILING_TRIGGER_SHIFT     10
#define PROFILING_TRIGGER_MASK                                                 \
	((1 << PROFILING_TRIGGER_SHIFT) - 1) /*--- Range: 1024 us ---*/

#include <mach/avm_fiq.h>
#include <mach/avm_fiq_os.h>
#include <mach/avm_gic_fiq.h>
#include <mach/avm_gic.h>

/**
 * Liste mit Round-Robin-Performance-Counter pro CPU
 */
struct _roundrobin_perf_ctrlstat {
	unsigned int perf_ctrl[PROFILING_MAX_PERF_REGISTER];
	unsigned long long sum_perf_time[PROFILING_MAX_PERF_REGISTER];
	unsigned long long sum_perf_count[PROFILING_MAX_PERF_REGISTER];
	const char *prefix;
	struct _roundrobin_perf_ctrlstat *next;
};

extern const struct _roundrobin_perf_ctrlstat roundrobin_performance[];
unsigned int array_size_roundrobin_performance(void);
#endif /* CONFIG_AVM_FIQ_PUMA7 */

extern const struct _perfcount_options
	performance_counter_options[PM_EVENT_LAST];

#define PROFILE_TIMER_ID_BY_ID(id)            ((id) & 0xF)
#define PROFILE_CPU_ID_BY_ID(id)              (((id) >> 4) & 0xF)
#define PROFILE_IRQ_ID_BY_ID(id)              (((id) >> 8))
#define PROFILE_BUILD_ID(cpu, timer, irq)                                      \
	((timer) | ((cpu) << 4) | ((irq) << 8))

#define INITIAL_EVENT_CNT_SETUP()                                              \
	do {                                                                   \
		arm_performance_counter_action(                                \
			"set 0 16"); /*--- PM_EVENT_EXT_DATA_ACCESS ---*/      \
		arm_performance_counter_action(                                \
			"set 1 7"); /*--- PM_EVENT_INST_EXEC ---*/             \
	} while (0)

static const struct _cpucore_profile arm_cpu_config[1] = {
	{ cpu_nr_offset: 0, vpe_nr: 1, next_core: NULL },
};

#define PROFILING_PERFORMANCE_COUNTER_SUPPORT
/**
 */
static inline void arch_profile_perfcnt_on(unsigned int on)
{
	union __performance_monitor_control C;

	if (on) {
		write_secure_debug_enable_register(0, 1);
		C.Register = read_p15_performance_monitor_control();
		C.Bits.CycleCounterDivider =
			0; /*--- / 64 * 32 = takt / 2 ->  wie get_cycle() auf dem MIPS ---*/
		C.Bits.EvtCount0 =
			PM_EVENT_EXT_DATA_ACCESS; /*--- Data  cache miss ---*/
		C.Bits.EvtCount1 =
			PM_EVENT_INST_EXEC; /*--- Instruction cache miss ---*/
		C.Bits.EnableCounters = 1;
		write_p15_performance_monitor_control(C.Register);
	} else {
		C.Register = read_p15_performance_monitor_control();
		C.Bits.EnableCounters = 0;
		write_p15_performance_monitor_control(C.Register);
	}
}

/**
 */
static inline unsigned int arch_profile_perfcnt1(void)
{
	return read_p15_performance_counter(0);
}

/**
 */
static inline unsigned int arch_profile_perfcnt2(void)
{
	return read_p15_performance_counter(1);
}

/**
 */
static inline int arch_is_linux_cpu(unsigned int core, unsigned int tc)
{
	return 1;
}

#if defined(PROFILING_IN_FIQ)
/**
 */
static inline int
arch_setup_timer_firq(int cpu, irqreturn_t (*lfirqfunc)(int irq, void *handle),
		      void *handle)
{
	int irqid;
	char txt[64];

	int timer_clk;
	int MaxTimerPeriod_usec = (int)((1.0 / (float)(HZ)) * 100000.0);

	int timerid = 1;

	if (timerid < 0) {
		return timerid;
	}

	timer_clk = PAL_sysClkcGetFreq(PAL_SYS_CLKC_TIMER1);
	PAL_sysResetCtrl(AVALANCHE_TIMER1_RESET, OUT_OF_RESET);

	PAL_sysTimer16SetParams(AVALANCHE_TIMER1_BASE, timer_clk,
				TIMER16_CNTRL_ONESHOT, MaxTimerPeriod_usec,
				NULL);

	irqid = AVALANCHE_TIMER_0_INT + timerid;
	snprintf(txt, sizeof(txt), "profile/%u", cpu);
	if (avm_request_fiq_on(cpumask_of(cpu), irqid, lfirqfunc, 0, txt, handle) >= 0) {
		return PROFILE_BUILD_ID(cpu, timerid, irqid);
	}
	pr_info("%s: timerid=%u irqid=%u cpu%u\n", __func__, timerid, irqid,
		cpu);
	return -1;
}

/**
 */
static inline void arch_free_timer_firq(int id, void *handle)
{
	if (id < 0) {
		return;
	}
	PAL_sysTimer16Ctrl(AVALANCHE_TIMER1_BASE, TIMER16_CTRL_STOP);
	avm_free_fiq_on(PROFILE_CPU_ID_BY_ID(id), PROFILE_IRQ_ID_BY_ID(id),
			handle);
}

/**
 * timer in usec
 */
static inline void arch_set_next_trigger(unsigned int next_us, int id)
{
	int timer_clk;

	if (id < 0) {
		return;
	}
	/*--- printk(KERN_INFO"%s: id=%x -> timerid=%u\n", __func__, id, timerid); ---*/
	timer_clk = PAL_sysClkcGetFreq(PAL_SYS_CLKC_TIMER1);
	PAL_sysTimer16Ctrl(AVALANCHE_TIMER1_BASE, TIMER16_CTRL_STOP);
	PAL_sysTimer16SetParams(AVALANCHE_TIMER1_BASE, timer_clk,
				TIMER16_CNTRL_ONESHOT, next_us,
				NULL);
	PAL_sysTimer16Ctrl(AVALANCHE_TIMER1_BASE, TIMER16_CTRL_START);
}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/

#endif /*--- #ifndef __arch_profile_pumaX_h__ ---*/
