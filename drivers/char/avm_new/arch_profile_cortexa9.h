/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_cortexa9_h__
#define __arch_profile_cortexa9_h__

#include <clocksource/qcom-qtimer.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 4, 0))
#include <asm/avm_enh/avm_fiq_groups.h>
#include <asm/avm_enh/avm_fiq.h>
#include <asm/avm_enh/avm_fiq_os.h>
#include <asm/avm_enh/avm_gic_fiq.h>

#include <linux/qcom_scm.h>
#else
#include <mach/avm_fiq.h>
#include <mach/avm_gic.h>
#include <mach/avm_gic_smc.h>
#include <mach/avm_gic_fiq.h>
#include <mach/avm_fiq_os.h>

#include <soc/qcom/scm.h>
#endif

#include <asm/performance.h>

#include <asm/mach_avm.h>

#if defined(CONFIG_AVM_FASTIRQ)
#define PROFILING_IN_FIQ
#endif

#define PROFILING_USEC_TO_TIMER(usec)                                          \
	(avm_get_profiling_clk() ? ((usec)*avm_get_profiling_clk()) :          \
				   ((usec)*48))

#define PROFILING_MAX_PERF_REGISTER                                            \
	(4 + 1) /* we treat the cycle counter like a performance counter */
#define PROFILING_PERF_REGISTERMASK                                            \
	((1 << 4) - 1) /* alle "echten" Performance-Register */
#define PROFILING_CYCLE_REGISTER (PROFILING_MAX_PERF_REGISTER - 1)

#define PROFILING_TRIGGER_SHIFT 10
#define PROFILING_TRIGGER_MASK                                                 \
	((1 << PROFILING_TRIGGER_SHIFT) - 1) /*--- Range: 1024 us ---*/

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

extern const struct _perfcount_options
	performance_counter_options[PM_EVENT_LAST];

#define INITIAL_EVENT_CNT_SETUP()                                              \
	do {                                                                   \
		arm_performance_counter_action(                                \
			"set 0 19"); /*--- PM_EVENT_MEM_ACCESS ---*/           \
		arm_performance_counter_action(                                \
			"set 1 20"); /*--- PM_EVENT_L1I_CACHE ---*/            \
		arm_performance_counter_action(                                \
			"set 2 201"); /*--- PM_EVENT_DATA_WRITE_STALL ---*/    \
		arm_performance_counter_action(                                \
			"set 3 8"); /*--- PM_EVENT_INST_RETIRED ---*/          \
	} while (0)

/**
 */
static const struct _cpucore_profile arm_cpu_config[4] = {
	{ .cpu_nr_offset = 0, .vpe_nr = 1, .next_core = &arm_cpu_config[1] },
	{ .cpu_nr_offset = 1, .vpe_nr = 1, .next_core = &arm_cpu_config[2] },
	{ .cpu_nr_offset = 2, .vpe_nr = 1, .next_core = &arm_cpu_config[3] },
	{ .cpu_nr_offset = 3, .vpe_nr = 1, .next_core = NULL },
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
 */
// clang-format off
static struct _qcom_timer_firq {
	const qtimer_ipq40xx qtim;
	const int fiq;
	const char *fiq_name;
	 irqreturn_t (*firqfunc)(int irq, void *handle);
	void *handle;
	int enable_id;
} qcom_timer_firq[NR_CPUS] = {
	[0] = { .qtim = qtim3_v1, .fiq = 43, .fiq_name = "qtim3", .firqfunc = NULL, .handle = NULL, .enable_id = -1},
	[1] = { .qtim = qtim4_v1, .fiq = 44, .fiq_name = "qtim4", .firqfunc = NULL, .handle = NULL, .enable_id = -1},
	[2] = { .qtim = qtim5_v1, .fiq = 45, .fiq_name = "qtim5", .firqfunc = NULL, .handle = NULL, .enable_id = -1},
	[3] = { .qtim = qtim6_v1, .fiq = 46, .fiq_name = "qtim6", .firqfunc = NULL, .handle = NULL, .enable_id = -1},
};
// clang-format on

/**
 */
static inline int
arch_setup_timer_firq(int cpu, irqreturn_t (*lfirqfunc)(int irq, void *handle),
		      void *handle)
{
	struct _qcom_timer_firq *pfirq = &qcom_timer_firq[cpu];

	/*--- pr_info("%s: %u %pS %p\n", __func__, cpu, lfirqfunc, handle); ---*/
	if (pfirq->enable_id < 0) {
		pfirq->firqfunc = lfirqfunc;
		pfirq->handle = handle;
#if defined(CONFIG_AVM_FASTIRQ)
		if (!avm_request_fiq_on(cpu, pfirq->fiq, pfirq->firqfunc,
					FIQ_EDGE | FIQ_HWIRQ, pfirq->fiq_name,
					pfirq->handle)) {
			avm_gic_fiq_setup(pfirq->fiq, 1 << cpu,
					  FIQ_PRIO_PROFILING, (FIQ_EDGE << 1),
					  0);
			/*--- pr_info("%s: %u %pS %p success\n", __func__, cpu, lfirqfunc, handle); ---*/
			pfirq->enable_id = cpu;
			qtimer_ipq40xx_enable(pfirq->qtim, 1, 0);
		}
#endif
	}
	return pfirq->enable_id;
}

/**
 */
static inline void arch_free_timer_firq(int id, void *handle)
{
	struct _qcom_timer_firq *pfirq = &qcom_timer_firq[id];
	int cpu;

	if ((id < 0) || (id >= num_possible_cpus()) ||
	    (pfirq->enable_id != id)) {
		pr_err("%s: false id=%u %p\n", __func__, id, handle);
		return;
	}
	cpu = pfirq->enable_id;
	pfirq->enable_id = -1;
#if defined(CONFIG_AVM_FASTIRQ)
	qtimer_ipq40xx_enable(pfirq->qtim, 0, 0);
	avm_free_fiq_on(cpu, pfirq->fiq, handle);
	/*--- pr_info("%s: %u %p done\n", __func__, id, handle); ---*/
#endif
}

/**
 * timer in usec
 */
static inline void arch_set_next_trigger(unsigned int next_us, int id)
{
	struct _qcom_timer_firq *pfirq = &qcom_timer_firq[id];

	if ((id < 0) || (id >= num_possible_cpus()) ||
	    (pfirq->enable_id != id)) {
		/*--- pr_err("%s: false id=%u\n", __func__, id); ---*/
		return;
	}
#if defined(CONFIG_AVM_FASTIRQ)
	qtimer_ipq40xx_set_next_event(pfirq->qtim,
				      PROFILING_USEC_TO_TIMER(next_us));
#endif
}
#endif /*--- #ifndef __arch_profile_cortexa_h__ ---*/
