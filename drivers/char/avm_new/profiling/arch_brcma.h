/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_brcma_h__
#define __arch_profile_brcma_h__

#include <bcm_intr.h>
#include <bcm_ext_timer.h>
#include <asm/performance.h>
#include <linux/cpumask.h>

/* INTERRUPT_ID_TIMER not defined anymore in 4.19 sdk */
#ifndef INTERRUPT_ID_TIMER
extern int timer_base_irq;
#define INTERRUPT_ID_TIMER timer_base_irq
#endif

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

#if defined(CONFIG_AVM_FASTIRQ)
#define PROFILING_IN_FIQ
#define PROFILING_MAX_PERF_REGISTER                                            \
	(6 + 1) /* we treat the cycle counter like a performance counter */
#define PROFILING_PERF_REGISTERMASK     ((1 << 6) - 1)
#define PROFILING_CYCLE_REGISTER        (PROFILING_MAX_PERF_REGISTER - 1)

#define PROFILING_TRIGGER_SHIFT 9
#define PROFILING_TRIGGER_MASK                                                 \
	((1 << PROFILING_TRIGGER_SHIFT) - 1) /*--- Range: 512 us ---*/

#if defined(CONFIG_AVM_GENERIC_RTE)
#include <avm/rte/rte.h>
#else
#include <asm/avm_enh/avm_fiq_groups.h>
#include <asm/avm_enh/avm_fiq_os.h>
#include <asm/avm_enh/avm_gic_fiq.h>
#include <asm/avm_enh/avm_gic.h>
#endif

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
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

extern const struct _perfcount_options
	performance_counter_options[PM_EVENT_LAST];

#define PROFILE_TIMER_ID_BY_ID(id)            ((id) & 0xF)
#define PROFILE_CPU_ID_BY_ID(id)              (((id) >> 4) & 0xF)
#define PROFILE_IRQ_ID_BY_ID(id)              (((id) >> 8))
#define PROFILE_BUILD_ID(cpu, timer, irq)                                      \
	((timer) | ((cpu) << 4) | ((irq) << 8))

#define INITIAL_EVENT_CNT_SETUP()                                               \
	do {                                                                    \
		arm_performance_counter_action(                                 \
			"set 0 1"); /*--- PM_EVENT_L1I_CACHE_REFILL --- */      \
		arm_performance_counter_action(                                 \
			"set 1 3"); /*--- PM_EVENT_L1D_CACHE_REFILL --- */      \
		arm_performance_counter_action(                                 \
			"set 2 129"); /*--- PM_EVENT_EXT_MEM_WRITE_STALL --- */ \
		arm_performance_counter_action(                                 \
			"set 3 104"); /*--- PM_EVENT_INST_RETIRED --- */        \
	} while (0)

/**
 */
static const struct _cpucore_profile arm_cpu_config[NR_CPUS] = {
	{ .cpu_nr_offset = 0, .vpe_nr = 1, .next_core = &arm_cpu_config[1] },
#if NR_CPUS == 2
	{ .cpu_nr_offset = 1, .vpe_nr = 1, .next_core = NULL },
#elif NR_CPUS == 3
	{ .cpu_nr_offset = 1, .vpe_nr = 1, .next_core = &arm_cpu_config[2] },
	{ .cpu_nr_offset = 2, .vpe_nr = 1, .next_core = NULL },
#elif NR_CPUS == 4
	{ .cpu_nr_offset = 1, .vpe_nr = 1, .next_core = &arm_cpu_config[2] },
	{ .cpu_nr_offset = 2, .vpe_nr = 1, .next_core = &arm_cpu_config[3] },
	{ .cpu_nr_offset = 3, .vpe_nr = 1, .next_core = NULL },
#else
#error missing configuration
#endif
};

#define PROFILING_PERFORMANCE_COUNTER_SUPPORT
/**
 */
static inline void arch_profile_perfcnt_on(unsigned int on)
{
	union __performance_monitor_control C;

	if (on) {
		C.Register = read_p15_performance_monitor_control();
		C.Bits.CycleCounterDivider = 0;
		C.Bits.EnableBit = 1;
		write_p15_performance_monitor_control(C.Register);
		write_p15_performance_count_enable(0x8000000F);
		p15_reset_performance_counter(0);
		p15_reset_performance_counter(1);
		p15_reset_performance_counter(2);
		p15_reset_performance_counter(3);
		p15_reset_performance_counter(4);
		p15_reset_performance_counter(5);
		write_p15_cycle_counter(0);
	} else {
		C.Register = read_p15_performance_monitor_control();
		C.Bits.EnableBit = 0;
		write_p15_performance_monitor_control(C.Register);
		write_p15_performance_count_enable(0);
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

/**
 * benoetigt, da sonst irqs nicht angeschaltet werden
 */
static void dummy_timer_handler(unsigned long param)
{
	pr_info("%s\n", __func__);
}

/**
 */
#if defined(CONFIG_AVM_FASTIRQ)
static int last_timer_id = -1;
static int shared_timer;

/**
 */
static inline int
arch_setup_timer_firq(int cpu, irqreturn_t (*lfirqfunc)(int irq, void *handle),
		      void *handle)
{
	int irqid;
	char txt[64];
	int timerid = ext_timer_alloc_only(-1, dummy_timer_handler, 0);

	if (timerid < 0) {
		if (last_timer_id < 0) {
			pr_err("no timer for cpu#%d\n", cpu);
			return timerid;
		}
		shared_timer = 1;
		timerid = last_timer_id;
	}
	last_timer_id = timerid;
	ext_timer_stop(timerid);
	ext_timer_set_mode(timerid, EXT_TIMER_MODE_ONESHOT);

	irqid = INTERRUPT_ID_TIMER + timerid;
	snprintf(txt, sizeof(txt), "Profiling");
	if (avm_request_fiq_on(cpumask_of(cpu), irqid, lfirqfunc, 0, txt, handle) >= 0) {
		avm_gic_fiq_setup(irqid, 0, cpumask_of(cpu), FIQ_PRIO_PROFILING, 1, 0);
		if (!shared_timer) {
			pr_err("%s: timerid=%u irqid=%u cpu%u\n", __func__,
			       timerid, irqid, cpu);
		} else {
			pr_err("%s: timerid=%u irqid=%u additional cpu%u\n",
			       __func__, timerid, irqid, cpu);
		}
		return PROFILE_BUILD_ID(cpu, timerid, irqid);
	}
	return -1;
}

/**
 */
static inline void arch_free_timer_firq(int id, void *handle)
{
	if (id < 0) {
		return;
	}
	avm_free_fiq_on(PROFILE_CPU_ID_BY_ID(id), PROFILE_IRQ_ID_BY_ID(id),
			0, handle);
	ext_timer_free(PROFILE_TIMER_ID_BY_ID(id));
	last_timer_id = -1;
	shared_timer = 0;
}
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

/**
 * timer in usec
 */
static inline void arch_set_next_trigger(unsigned int next_us, int id)
{
	int cpuid = PROFILE_CPU_ID_BY_ID(id);
	int timerid = PROFILE_TIMER_ID_BY_ID(id);
	int irqid = PROFILE_IRQ_ID_BY_ID(id);
	int c = 0;

	if (id < 0) {
		return;
	}
	/*--- pr_info("%s: id=%x -> timerid=%u\n", __func__, id, timerid); ---*/
	ext_timer_stop(timerid);

#if defined(CONFIG_AVM_FASTIRQ)
	/* Target Routing Round Robin zur nächsten CPU stellen */
	set_ICDIPTR(irqid, (1 << (((c = cpuid + 1) >= (int)num_possible_cpus()) ? 0 : c)), IS_NOT_ATOMIC);
#endif

	ext_timer_set_period(timerid, next_us);
	ext_timer_start(timerid);
}

#endif /*--- #ifndef __arch_profile_brcma_h__ ---*/
