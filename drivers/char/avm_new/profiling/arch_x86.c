// SPDX-License-Identifier: GPL-2.0+

#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>
#include <linux/kallsyms.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <linux/perf_event.h>
#include <asm/uaccess.h>
#include <asm/perf_event.h>
#include <asm/perf_event_p4.h>
#include <asm/nmi.h>
#include <asm/hardirq.h>
#include <asm/ptrace.h>

#include "avm_profile.h"
#include "arch_profile.h"

static const struct _cpucore_profile x86_cpu_config[2] = {
	{ .cpu_nr_offset = 0, .vpe_nr = 1, .next_core = &x86_cpu_config[1] },
	{ .cpu_nr_offset = 1, .vpe_nr = 1, .next_core = NULL },
};

static DEFINE_PER_CPU(struct perf_event *, pmu_ev);
static DEFINE_PER_CPU(struct perf_event_attr, pmu_attr);

static uint64_t state0 = 1;
static uint64_t state1 = 2;

unsigned int xor_shift_128(void)
{
	uint64_t s1 = state0;
	uint64_t s0 = state1;

	state0 = s0;
	s1 ^= s1 << 23;
	s1 ^= s1 >> 17;
	s1 ^= s0;
	s1 ^= s0 >> 26;
	state1 = s1;

	return (unsigned int)(state0 + state1);
}

/**
 * liefert Anzahl der Performance-Counter
 */
static unsigned int x86_get_performance_counter_nr(void)
{
	return 0;
}

/**
 */
int x86_get_performance_counter_mode(char *str, int str_len, unsigned int nr)
{
	if (str_len)
		str[0] = 0;
	return 0;
}

/**
 */
static void x86_performance_counter_action(char *p)
{
}

/**
 */
void x86_performance_counter_help(struct seq_file *seq)
{
}

/**
 */
static void
x86_profiling_performance_statistic(int core,
				    struct seq_file *seq,
				    unsigned int format)
{
}

/**
 */
static void
x86_profiling_special_enable(enum _simple_profile_enable_mode on,
			     unsigned int perfcnt_for_profile)
{
	int cpu;
	struct perf_event *event;

	if ((on == sp_enable_on) || (on == sp_enable_wrap)) {
		for_each_online_cpu(cpu) {
			event = per_cpu(pmu_ev, cpu);
			if (event == NULL) {
				continue;
			}

			event->pending_disable = 0;
			smp_mb();
			perf_event_enable(event);
		}
	} else {
		for_each_online_cpu(cpu) {
			event = per_cpu(pmu_ev, cpu);
			if (event == NULL) {
				continue;
			}

			event->pending_disable = 1;
			smp_mb();
			/* may be called from NMI context. Avoid
			 * calling perf_event_disable here, as it will
			 * make cross-CPU function calls. Will be disabled
			 * next time when overflow callback is triggered
			 */
		}
	}
}

/* Callback function for perf event subsystem */
static void pmu_overflow_callback(struct perf_event *event,
				  struct perf_sample_data *data,
				  struct pt_regs *regs)
{
	uint64_t next_trigger;
	unsigned long ra;
	unsigned int cpu;

	/* execute pending disable */
	if (event->pending_disable == 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 77)
		irq_work_queue(&event->pending_irq);
#else
		irq_work_queue(&event->pending);
#endif
		return;
	}

	/* prevent throttling */
	event->hw.interrupts = 0;

	cpu = task_cpu(current);
	ra = get_ra_from_bp(NULL, regs->bp);
	__avm_simple_profiling_code_from_other_context(
		regs->ip, ra, current, cpu, cpu, 0, arch_profile_perfcnt1(),
		arch_profile_perfcnt2(), regs->sp);

	next_trigger = PROFILING_USEC_TO_TIMER(PROFILING_PERIOD_BASE);
	next_trigger += PROFILING_USEC_TO_TIMER(xor_shift_128() &
						PROFILING_PERIOD_MASK);
	event->hw.sample_period = next_trigger;
	event->attr.sample_period = next_trigger;
}

/**
 */
static int nmi_profiler_init(int cpu)
{
	struct perf_event_attr *hw_attr;
	struct perf_event *event;
	int result;

	event = per_cpu(pmu_ev, cpu);
	result = 0;

	/* is it already setup and enabled? */
	if (event && event->state > PERF_EVENT_STATE_OFF) {
		goto err_out;
	}

	/* it is setup but not enabled */
	if (event != NULL) {
		goto err_out;
	}

	hw_attr = &per_cpu(pmu_attr, cpu);
	hw_attr->type = PERF_TYPE_HARDWARE;
	hw_attr->config = PERF_COUNT_HW_BUS_CYCLES;
	hw_attr->size = sizeof(struct perf_event_attr);
	hw_attr->pinned = 1;
	hw_attr->disabled = 1;

	/* Try to register using hardware perf events */
	hw_attr->sample_period = (u64)PROFILING_USEC_TO_TIMER(1000UL);
	event = perf_event_create_kernel_counter(hw_attr, cpu, NULL,
						 pmu_overflow_callback,
						 NULL
	);
	if (!IS_ERR(event)) {
		pr_info("NMI profiling enabled, takes one hw-pmu counter.\n");
		per_cpu(pmu_ev, cpu) = event;
	} else {
		result = PTR_ERR(event);

		switch (result) {
		case -EOPNOTSUPP:
			pr_info("NMI profiling disabled (cpu%i): not supported (no LAPIC?)\n",
				cpu);
			break;
		case -ENOENT:
			pr_warn("NMI profiling disabled (cpu%i): hardware events not enabled\n",
				cpu);
			break;
		default:
			pr_err("NMI profiling disabled (cpu%i): unable to create perf event: %d\n",
			       cpu, result);
			break;
		}
	}

err_out:
	return result;
}

static void __maybe_unused nmi_profiler_deinit(int cpu)
{
	struct perf_event *event = per_cpu(pmu_ev, cpu);

	if (event) {
		perf_event_disable(event);
		per_cpu(pmu_ev, cpu) = NULL;

		/* should be in cleanup, but blocks oprofile */
		perf_event_release_kernel(event);
	}
}

/**
 */
static int __init arch_x86_profiler_init(void)
{
	int cpu;

	cpu_nr_to_tc_and_core[0].core = 0;
	cpu_nr_to_tc_and_core[0].tc = 0;
	cpu_nr_to_tc_and_core[1].core = 1;
	cpu_nr_to_tc_and_core[1].tc = 0;

	arch_profile_ctrl.cpu_profile = x86_cpu_config;
	arch_profile_ctrl.performance_counter_action =
		x86_performance_counter_action;
	arch_profile_ctrl.performance_counter_help =
		x86_performance_counter_help;
	arch_profile_ctrl.get_performance_counter_nr =
		x86_get_performance_counter_nr;
	arch_profile_ctrl.get_performance_counter_mode =
		x86_get_performance_counter_mode;
	arch_profile_ctrl.profiling_special_enable =
		x86_profiling_special_enable;
	arch_profile_ctrl.profiling_performance_statistic =
		x86_profiling_performance_statistic;

	for_each_online_cpu(cpu) {
		if (nmi_profiler_init(cpu)) {
			pr_err("[%s] performance monitor failed for cpu %d\n",
			       __func__, cpu);
		}
	}

	return 0;
}

device_initcall(arch_x86_profiler_init);
