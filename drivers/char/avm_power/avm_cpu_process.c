#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/jiffies.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/cputime.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/signal.h>
#endif

#if defined(CONFIG_AVM_POWER)
#include <avm/power/power.h>
#endif/*--- #if defined(CONFIG_AVM_POWER) ---*/
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
#include <avm/profile/profile.h>
#endif/*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
#include <asm/mach_avm.h>
#if defined(CONFIG_AVM_ENHANCED)
#include <asm/avm_enh/avm_enh.h>
#endif/*--- #if defined(CONFIG_AVM_ENHANCED) ---*/

#include <avm/sammel/simple_proc.h>

DEFINE_PER_CPU(struct _avm_pagefault, avm_pagefault);

#if defined(CONFIG_AVM_SOFTIRQ_STATISTIC)
DEFINE_PER_CPU(struct _softirq_stat, avm_softirqstat);

static char *softirq_runtime(char *txt, int txt_len, int cpu);
#else
static inline char *softirq_runtime(char *txt, int txt_len __maybe_unused, int cpu __maybe_unused)
{
	txt[0] = 0;
	return txt;
}
#endif

#if KERNEL_VERSION(2, 6, 32) > LINUX_VERSION_CODE
/**
 * get_avenrun - get the load average array
 * @loads:	pointer to dest load array
 * @offset:	offset to add
 * @shift:	shift count to shift the result left
 *
 * These values are estimates at best, so no need for locking.
 */
void get_avenrun(unsigned long *loads, unsigned long offset, int shift)
{
	loads[0] = (avenrun[0] + offset) << shift;
	loads[1] = (avenrun[1] + offset) << shift;
	loads[2] = (avenrun[2] + offset) << shift;
}
#endif/*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32) ---*/

#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#define cputime_t u64
#define jiffies_to_cputime(j) jiffies_to_nsecs(j)
#define div_cputime(a, b) div64_u64(a, b)
#else
#define div_cputime(a, b) ((a) / (b))
#endif

#if KERNEL_VERSION(3, 8, 0) > LINUX_VERSION_CODE
#define task_cputime_adjusted(a, b, c) task_times(a, b, c)
#define thread_group_cputime_adjusted(a, b, c) thread_group_times(a, b, c)
#endif

#if KERNEL_VERSION(3, 5, 0) > LINUX_VERSION_CODE
#define sprint_symbol_no_offset(symname, addr)   lookup_symbol_name(addr, symname)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#define avm_compat_totalram_pages() totalram_pages()
#else
#define avm_compat_totalram_pages() (totalram_pages)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#define avm_compat_free_pages() global_zone_page_state(NR_FREE_PAGES)
#else
#define avm_compat_free_pages() global_page_state(NR_FREE_PAGES)
#endif

#if defined(CONFIG_X86)
#define CPU_COUNT_SHIFT     4
#else/*--- #if defined(CONFIG_X86) ---*/
#define CPU_COUNT_SHIFT     0
#endif/*--- #else ---*//*--- #if defined(CONFIG_X86) ---*/

#define snprintf_add(ptxt, txtlen, args...)				\
	{								\
		int local_add_len;					\
		local_add_len = snprintf(ptxt, txtlen, args);		\
		if (local_add_len > 0) {				\
			int tail = min_t(int, txtlen, local_add_len);	\
			(ptxt) += tail, (txtlen) -= tail;		\
		}							\
	}
/**
 */
struct _cpu_idlecount {
	unsigned long last_cycle;
	atomic_t cpu_idle_done;
	unsigned long wait_count;
	unsigned long sum_count;
	unsigned int actrun;
	unsigned long long systemload_run_count;
	unsigned long long systemload_sum_count;
	unsigned long systemload_check_jiffies;
	unsigned long systemload_check_disp_count;

};
static DEFINE_PER_CPU(struct _cpu_idlecount, cpu_idlecount);

struct _cpu_pgallocfailed_stat {
	unsigned long last_pgalloc_normal;
	unsigned long last_pg_fault_cnt;
};
static DEFINE_PER_CPU(struct _cpu_pgallocfailed_stat, cpu_pgallocfailed);

/**
 * die Normierung erfolgt entsprechend des Zeitfensters
 */
#if defined(__get_last_unaligned_info)
static const char *norm_ai(unsigned long *ai, unsigned int sec)
{
	if (sec >= 60 * 60) {
		*ai *= 60 * 60;
		return "h";
	}
	if (sec >= 60) {
		*ai *= 60;
		return "min";
	}
	return "s";
}
#endif
/**
 */
static char *print_ai_info(char *txt, int txtlen __maybe_unused,
			   unsigned long sec __maybe_unused, int user __maybe_unused)
{
#if defined(__get_last_unaligned_info)
	static unsigned long last_ai_user;
	static unsigned long last_ai_sys;
	const char *comm, *post_fix;
	unsigned long ai_diff;
	unsigned long ai_count;
	unsigned long last_pc;
	unsigned long *last_ai_count = user ? &last_ai_user : &last_ai_sys;

	comm = get_last_unaligned_info(&ai_count,  &last_pc, user);
	ai_diff = ai_count - *last_ai_count;
	if (ai_diff == 0) {
		txt[0] = 0;
		return txt;
	}
	*last_ai_count = ai_count;
	post_fix = norm_ai(&ai_diff, sec);
	if (sec == 0)
		sec = 1;
	if (user) {
		snprintf(txt, txtlen, " ai_user:%lu.%02lu/%s 0x%08lx %s",
					ai_diff / sec, ((ai_diff * 100) / sec) % 100, post_fix,
					last_pc, comm);
	} else {
		snprintf(txt, txtlen, " ai_sys:%lu.%02lu/%s %pS %s",
					ai_diff / sec, ((ai_diff * 100) / sec) % 100, post_fix,
					(void *)last_pc, comm);
	}
#else
	txt[0] = 0;
#endif/*--- #if defined(__get_last_unaligned_info) ---*/
	return txt;
}
#define COUNT_DIFF(act, last)   ((act) - (last))
/**
 * liefert ob diese CPU im IDLE-Mode
 */
unsigned int avm_cpu_idle_state(int vpe)
{
	struct _cpu_idlecount *pcpuidle = per_cpu_ptr(&cpu_idlecount, vpe);

	return !atomic_read(&pcpuidle->cpu_idle_done);
}

/**
 * hook for idle
 */
void avm_cpu_wait_start(void)
{
	unsigned long count, tmp, flags;
	unsigned int cpu = get_cpu();
	struct _cpu_idlecount *pcpuidle = per_cpu_ptr(&cpu_idlecount, cpu);

	put_cpu();
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
	avm_simple_profiling_enter_idle((unsigned int)avm_cpu_wait_start);
#endif/*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING)  ---*/
	local_irq_save(flags);
	count = avm_get_cycles();
	tmp = COUNT_DIFF(count, pcpuidle->last_cycle) >> CPU_COUNT_SHIFT;
	pcpuidle->last_cycle	        = count;
	pcpuidle->sum_count	       += tmp;
	pcpuidle->systemload_run_count += tmp;
	pcpuidle->systemload_sum_count += tmp;
	atomic_set(&pcpuidle->cpu_idle_done, 0);	/* in idle-state */
	local_irq_restore(flags);
}

static struct _hist_process_times {
	cputime_t last_runtime;
	unsigned int pid;
	unsigned int mark;
} hist_process_times[200];

static cputime_t last_process_cputime;

/**
 */
static struct _hist_process_times *get_histprocess_by_pid(unsigned int pid, unsigned int *cache_idx)
{
	unsigned int i;

	if (*cache_idx >= ARRAY_SIZE(hist_process_times)) {
		*cache_idx = 0;
	}
	for (i = *cache_idx; i < ARRAY_SIZE(hist_process_times); i++) {
		if (hist_process_times[i].pid == pid) {
			*cache_idx = i + 1;
			hist_process_times[i].mark = 2;
			return &hist_process_times[i];
		}
	}
	for (i = 0; i < *cache_idx; i++) {
		if (hist_process_times[i].pid == pid) {
			*cache_idx = i + 1;
			hist_process_times[i].mark = 2;
			return &hist_process_times[i];
		}
	}
	/*--- Create ---*/
	for (i = 0; i < ARRAY_SIZE(hist_process_times); i++) {
		if (hist_process_times[i].mark == 0) {
			hist_process_times[i].mark = 2;
			hist_process_times[i].last_runtime = 0;
			hist_process_times[i].pid  = pid;
			return &hist_process_times[i];
		}
	}
	/*--- printk("<error pid %d>", pid); ---*/
	return NULL;
}

/**
 */
static void put_histprocess(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(hist_process_times); i++) {
		struct _hist_process_times *pht = &hist_process_times[i];

		if (pht->mark) {
			pht->mark--; /*--- um geloeschte Eintraege frei zu bekommen  ---*/
		}
	}
}

struct _accumulate_output {
	cputime_t sum_runtime;
	cputime_t curr_runtime;
	cputime_t max_runtime;
	struct task_struct *max_runtask;
	unsigned int processes;
};

/**
 * fill ao with task-values since last measure-point
 * ret:  measure-time of cpu
 */
static cputime_t accumulate_runtime(struct _accumulate_output *ao)
{
	cputime_t process_measure_cputime;
	cputime_t act_runtime, diff_runtime;
	cputime_t sum_runtime = 0, max_runtime = 0;
	struct task_struct *g, *max_runtask = current;
	unsigned int cache_idx = 0;
	unsigned int processes = 0;

	memset(ao, 0, sizeof(*ao));

	process_measure_cputime = jiffies_to_cputime(jiffies) - last_process_cputime;
	if (process_measure_cputime == 0) {
		return 0;
	}
	rcu_read_lock();
	for_each_process(g) {
		cputime_t ut, st;
		struct _hist_process_times *pht = get_histprocess_by_pid(g->pid, &cache_idx);

		processes++;
		if (!pht) {
			continue;
		}
		thread_group_cputime_adjusted(g, &ut, &st);
		act_runtime   = ut + st;
		diff_runtime  = act_runtime - pht->last_runtime;
		sum_runtime  += diff_runtime;

		if (diff_runtime > max_runtime) {
			max_runtime = diff_runtime;
			max_runtask = g;
		}
		if (same_thread_group(current, g)) {
			ao->curr_runtime = diff_runtime;
		}
		pht->last_runtime = act_runtime;
	}
	rcu_read_unlock();

	put_histprocess();
	ao->max_runtask = max_runtask;
	ao->max_runtime = max_runtime;
	ao->sum_runtime = sum_runtime | 0x1;
	ao->processes   = processes;
	last_process_cputime = jiffies_to_cputime(jiffies);
	/*--- pr_err("%s: processes: %u cputime %lu maxrun: %s: maxruntime=%lu currruntime=%lu sumruntime=%lu\n", __func__, ao->processes, process_measure_cputime, ao->max_runtask->comm, ao->max_runtime, ao->curr_runtime, ao->sum_runtime); ---*/
	return process_measure_cputime;
}

/**
 */
static char *process_runtime(char *txt, int txt_len)
{
	struct _accumulate_output ao;
	unsigned int max_percent, sum_percent;
	cputime_t measure_cputime;

	txt[0] = 0;
	measure_cputime = accumulate_runtime(&ao) + 1;
	if (measure_cputime <= 1) {
		return txt;
	}
	measure_cputime *= num_possible_cpus(); /* normed for all cpus */
	sum_percent  = div_cputime(ao.sum_runtime * 100, measure_cputime);
	if (sum_percent == 0) {
		return txt;
	}
	max_percent  = div_cputime(ao.max_runtime * 100, measure_cputime);

	snprintf(txt, txt_len, " task runtime:%u%% max:%s %u%% ",
		 sum_percent, ao.max_runtask->comm, max_percent);
	return txt;
}

/**
 */
static char *page_allocfault_statistic(char *txt, int txt_len, int cpu, unsigned long j_diff)
{
#ifdef CONFIG_VM_EVENT_COUNTERS
	char *p = txt;
	unsigned long vmevents[NR_VM_EVENT_ITEMS];
	unsigned long pg_alloc_per_sec, pg_fault_per_sec;
	struct _cpu_pgallocfailed_stat *pg = per_cpu_ptr(&cpu_pgallocfailed, cpu);
#endif
	txt[0] = 0;
#ifdef CONFIG_VM_EVENT_COUNTERS
	__all_vm_events(vmevents);

	pg_alloc_per_sec = ((vmevents[PGALLOC_NORMAL] - pg->last_pgalloc_normal) * HZ) / j_diff;
	pg_fault_per_sec = ((vmevents[PGFAULT] - pg->last_pg_fault_cnt) * HZ) / j_diff;
	pg->last_pg_fault_cnt    = vmevents[PGFAULT];
	pg->last_pgalloc_normal = vmevents[PGALLOC_NORMAL];

	if (pg_alloc_per_sec) {
		snprintf_add(p, txt_len, " alloc=%lu/s", pg_alloc_per_sec);
	}
	if (pg_fault_per_sec) {
		snprintf_add(p, txt_len, " fault=%lu/s", pg_fault_per_sec);
	}
#endif
	return txt;
}

/* These are available from linux/sched/loadavg.h on kernels >= 4.20 */
#ifndef LOAD_FRAC
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1 - 1)) * 100)
#endif
/**
 * j_diff:    measure time
 * run_percent: load since last measure
 */
static void display_systemload(int cpu, unsigned long j_diff, unsigned long run_percent)
{
	char txt[2][64];
	char txt_softirq[128];
	char txt_process[128];
	char txt_pgallocfault[64];
	struct _avm_pagefault *pgf = per_cpu_ptr(&avm_pagefault, cpu);
	unsigned long avnrun[3];
	unsigned long pg_fault_max;

	if (j_diff == 0) {
		return;
	}
	get_avenrun(avnrun, FIXED_1 / 200, 0);
	pg_fault_max = pgf->faultlink_max;
	pgf->faultlink_max = 0;

	pr_warn("[%x]system-load %3lu%% loadavg %lu.%lu %lu.%lu %lu.%lu -%s%s"\
		" pgstat: sum=%lu free=%lu slab=%lu%s%s%s (sleep %lu)\n",
		cpu,
		run_percent,
		LOAD_INT(avnrun[0]), LOAD_FRAC(avnrun[0]),
		LOAD_INT(avnrun[1]), LOAD_FRAC(avnrun[1]),
		LOAD_INT(avnrun[2]), LOAD_FRAC(avnrun[2]),
		process_runtime(txt_process, sizeof(txt_process)),
		softirq_runtime(txt_softirq, sizeof(txt_softirq), cpu),
		avm_compat_totalram_pages(),
		avm_compat_free_pages(),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
		global_node_page_state_pages(NR_SLAB_RECLAIMABLE_B) +
		global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B),
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
		global_node_page_state(NR_SLAB_RECLAIMABLE) +
		global_node_page_state(NR_SLAB_UNRECLAIMABLE),
#else
		global_page_state(NR_SLAB_RECLAIMABLE) +
		global_page_state(NR_SLAB_UNRECLAIMABLE),
#endif
		page_allocfault_statistic(txt_pgallocfault, sizeof(txt_pgallocfault), cpu, j_diff),
		print_ai_info(txt[0], sizeof(txt[0]), j_diff / HZ, 0),
		print_ai_info(txt[1], sizeof(txt[1]), j_diff / HZ, 1),
		pg_fault_max
		);
}

/**
 */
static void signalize_powerdevice_loadrate(void)
{
	unsigned int cpu, cpu_run = 0;

	for_each_online_cpu(cpu) {
		struct _cpu_idlecount *pcpuidle = per_cpu_ptr(&cpu_idlecount, cpu);

		cpu_run |= pcpuidle->actrun << (cpu * 8);
	}
#ifdef CONFIG_AVM_POWERMETER
	PowerManagmentRessourceInfo(powerdevice_loadrate, cpu_run);
#endif
}

/**
 */
static void caluclate_per_cpu_loadrate(struct _cpu_idlecount *pcpuidle)
{
	if (unlikely(pcpuidle->sum_count > (1 << 29))) {
		int p_idle = ((pcpuidle->wait_count >> 8) * 100) / (pcpuidle->sum_count >> 8);

		pcpuidle->actrun = max(0, 100 - p_idle);
		pcpuidle->sum_count  >>= 8;
		pcpuidle->wait_count >>= 8;
		signalize_powerdevice_loadrate();
	}
}

#define RUN_WARN_LOAD_SCALESHIFT  2
/**
 * hook for irq and idle-function (wait-irq-off after idle-instruction)
 * for statistic function
 * idle stopps
 * call at the end of idle in wait_irq_off-function or direct in the irq-function
 */
int avm_cpu_wait_end(void)
{
	unsigned long j_diff;
	unsigned long count, tmp;
	unsigned int cpu =  get_cpu();
	struct _cpu_idlecount *pcpuidle = per_cpu_ptr(&cpu_idlecount, cpu);

	put_cpu();

	caluclate_per_cpu_loadrate(pcpuidle);

	if (unlikely(atomic_inc_return(&pcpuidle->cpu_idle_done) == 1)) {
		/* first call after idle: summarize idles */
		count = avm_get_cycles();
		tmp = COUNT_DIFF(count, pcpuidle->last_cycle) >> CPU_COUNT_SHIFT;
		pcpuidle->last_cycle   = count;
		pcpuidle->wait_count  += tmp;
		pcpuidle->sum_count   += tmp;
		pcpuidle->systemload_sum_count += tmp;
		return 0;
	}

	j_diff = COUNT_DIFF(jiffies, pcpuidle->systemload_check_jiffies);
	if (unlikely(j_diff >= HZ)) {
		/*--- nur alle Sekunde checken ---*/
		pcpuidle->systemload_check_jiffies = jiffies;

		count = avm_get_cycles();
		tmp = COUNT_DIFF(count, pcpuidle->last_cycle) >> CPU_COUNT_SHIFT;
		pcpuidle->last_cycle = count;
		pcpuidle->sum_count   += tmp;
		pcpuidle->systemload_run_count += tmp;
		pcpuidle->systemload_sum_count += tmp;

		if (++pcpuidle->systemload_check_disp_count >= 10) {
			unsigned long long run_count, sum_count;

			run_count = pcpuidle->systemload_run_count;
			sum_count = pcpuidle->systemload_sum_count;
			/*--- maximal alle 10 Sekunden darstellen, wenn Systemlast > 25 % ---*/
			if (run_count >= (sum_count >> RUN_WARN_LOAD_SCALESHIFT)) {
				unsigned long long percent;

				do_div(sum_count, 100);
				percent = run_count;
				do_div(percent, sum_count | 0x1);

				display_systemload(cpu, j_diff * pcpuidle->systemload_check_disp_count, (unsigned long)percent);
			}
			/*--- Resete systemload-Messwerte alle 10 Sekunden ---*/
			pcpuidle->systemload_sum_count        = 0;
			pcpuidle->systemload_run_count	      = 0;
			pcpuidle->systemload_check_disp_count = 0;
		}
	}
	return 1;
}

/**
 * depreceated
 */
void avm_cpu_wait_info(void)
{
}

#if defined(CONFIG_AVM_SOFTIRQ_STATISTIC)
static unsigned int reset_softirqstatistic_after_read = 1;
#define CLK_TO_USEC(a) ((a) / (avm_get_cyclefreq() / 1000000))
#define PRIV_SCALE		100
#define PERCENT_TO_PRIV_SCALE(a)  ((a) * PRIV_SCALE)
#define PRIV_SCALE_TO_PERCENT(a)  ((a) / PRIV_SCALE)
#define PRIV_SCALE_MOD_PERCENT(a) ((a) % PRIV_SCALE)

/**
 */
static const char *avm_softirq_name(int vec_nr)
{
	return vec_nr == HI_SOFTIRQ		? "HI" :
	       vec_nr == TIMER_SOFTIRQ		? "TIMER" :
	       vec_nr == NET_TX_SOFTIRQ		? "NET_TX" :
	       vec_nr == NET_RX_SOFTIRQ		? "NET_RX" :
	       vec_nr == BLOCK_SOFTIRQ		? "BLOCK" :
#if KERNEL_VERSION(4, 5, 0) <= LINUX_VERSION_CODE
	       vec_nr == IRQ_POLL_SOFTIRQ	? "IRQ_POLL" :
#else
	       vec_nr == BLOCK_IOPOLL_SOFTIRQ   ? "BLOCK_IOPOLL_SOFTIRQ" :
#endif
	       vec_nr == TASKLET_SOFTIRQ	? "TASKLET" :
	       vec_nr == SCHED_SOFTIRQ		? "SCHED" :
	       vec_nr == HRTIMER_SOFTIRQ	? "HRTIMER" :
	       vec_nr == RCU_SOFTIRQ		? "RCU" : "?";
}

/**
 * ret percent * PRIV_SCALE
 */
static int get_softirq_percent(struct _softirq_stat *pss, unsigned long *_measuretime_msec)
{
	unsigned long measuretime_msec;
	unsigned long long sum_runtime;
	unsigned long ts_start;
	unsigned long freq_qot;

	ts_start = pss->measure_ts_start_jiffies;
	sum_runtime = READ_ONCE(pss->sum_softirq.sum_runtime);

	if (sum_runtime == 0) {
		return 0;
	}
	freq_qot = avm_get_cyclefreq() / (1000 * PERCENT_TO_PRIV_SCALE(100)); /* normed for 100 * percent */
	measuretime_msec = jiffies_to_msecs(jiffies - ts_start) | 0x1; /* not zero */

	if (unlikely(freq_qot == 0)) { /* avm_get_cyclefreq() might not be initialized yet */
		return 0;
	}

	if (_measuretime_msec) {
		*_measuretime_msec = measuretime_msec;
	}
	do_div(sum_runtime, freq_qot);
	do_div(sum_runtime, measuretime_msec);

	return (int)sum_runtime > PERCENT_TO_PRIV_SCALE(100) ? PERCENT_TO_PRIV_SCALE(100) : sum_runtime;
}

/**
 * ret: count
 */
static int get_per_call_stat(struct _runtime_stat *pst, unsigned long *max_usec,
			     unsigned long *avg_usec)
{
	unsigned long long avg;
	unsigned long count, max;

	avg	= pst->sum_runtime;
	max	= pst->max_runtime;
	count	= READ_ONCE(pst->count);

	if (count == 0)
		return count;

	do_div(avg, count);
	if (max_usec)
		*max_usec = CLK_TO_USEC(max);
	if (avg_usec)
		*avg_usec = CLK_TO_USEC((unsigned long)avg);
	return count;
}

/**
 */
static int check_softirqstatistic_reset_req_timeout(struct _softirq_stat *pss)
{
	return pss->reset_req_on_jiffies &&
	       ((jiffies | 0x1) - pss->reset_req_on_jiffies) > (5 * HZ);
}

/**
 */
static void raise_softirq_on_cpu(void *param __attribute__((unused)))
{
	raise_softirq(TIMER_SOFTIRQ);
}

/**
 * trigger softirq to reset counts synchronous in softirq-context
 */
static void req_softirqstatistic_reset_async(struct _softirq_stat *pss, int cpu)
{
	if (pss->reset_req_on_jiffies == 0) {
		int this_cpu;

		pss->reset_req_on_jiffies = jiffies | 0x1; /*should be non-zero */
		this_cpu = get_cpu();
		if (this_cpu != cpu)
			smp_call_function_single(cpu, raise_softirq_on_cpu, NULL, 0);
		else
			raise_softirq(TIMER_SOFTIRQ);

		put_cpu();
	}
}

/**
 * ret: vector with max runtime
 */
static int softirq_vector_max_sum_runtime(struct _softirq_stat *pss)
{
	unsigned long long max_sum_runtime = 0;
	int vec_nr, max_vec_nr = 0;

	for (vec_nr = 0; vec_nr < NR_SOFTIRQS; vec_nr++) {
		struct _runtime_stat *pts = &pss->softirq[vec_nr];

		if (max_sum_runtime < pts->sum_runtime) {
			max_sum_runtime = pts->sum_runtime;
			max_vec_nr      = vec_nr;
		}
	}
	return max_vec_nr;
}

/**
 * ret: vector in pending-state, -1 if no pending
 *
 */
static int softirq_vector_pending(struct _softirq_stat *pss)
{
	int vec_nr;

	for (vec_nr = 0; vec_nr < NR_SOFTIRQS; vec_nr++) {
		struct _runtime_stat *pts = &pss->softirq[vec_nr];

		if (pts->pending)
			return vec_nr;
	}
	return -1;
}

/**
 * ret: _runtime_stat-pointer with function with maxruntime - else zero
 * all_sum_runtime: sum of all function-runtime
 */
static struct _runtime_stat *softirq_func_max_sum_runtime(struct _softirq_stat *pss,
							  int vec_nr,
							  unsigned long long *all_sum_runtime)
{
	unsigned long long max_sum_runtime = 0;
	struct _runtime_stat *pfuncmax = NULL;
	unsigned int i;

	*all_sum_runtime = 0;
	for (i = 0; i < ARRAY_SIZE(pss->func); i++) {
		struct _runtime_stat *pts = &pss->func[i];

		if (pts->caller == NULL)
			break;
		if ((int)pts->vec_nr != vec_nr)
			continue;
		if (max_sum_runtime < pts->sum_runtime) {
			max_sum_runtime = pts->sum_runtime;
			pfuncmax	= pts;
		}
		*all_sum_runtime += pts->sum_runtime;
	}
	return pfuncmax;
}

/**
 * ret: _runtime_stat-pointer if pending - else zero
 */
static struct _runtime_stat *softirq_func_pending(struct _softirq_stat *pss)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pss->func); i++) {
		struct _runtime_stat *pts = &pss->func[i];

		if (pts->caller == NULL) {
			break;
		}
		if (pts->pending)
			return pts;

	}
	return NULL;
}

/**
 */
static void _softirq_runtime(char *txt, int txt_len, struct _softirq_stat *pss)
{
	char fn_name[KSYM_SYMBOL_LEN];
	unsigned long long max_sum_runtime = 0;
	unsigned long long all_sum_runtime;
	unsigned long long runtime_percent;
	int hanging = 0, vec_nr = -1;
	struct _runtime_stat *pfuncmax = NULL;
	unsigned int percent_scale;

	txt[0] = 0;
	if (check_softirqstatistic_reset_req_timeout(pss)) {
		/*--- 2 possible reasons: hanging or locked softirq ---*/
		vec_nr = softirq_vector_pending(pss);
		if (vec_nr >= 0)
			/* reason is hanging */
			hanging = 1;
	}
	all_sum_runtime = pss->sum_softirq.sum_runtime;
	if (hanging) {
		/* correct values if hanging softirq (vec_nr) found */
		percent_scale   = PERCENT_TO_PRIV_SCALE(100);
		max_sum_runtime = all_sum_runtime;
	} else {
		percent_scale = get_softirq_percent(pss, NULL);
		if (percent_scale < PERCENT_TO_PRIV_SCALE(1))
			/* smaller 1 % -> ignore */
			return;
		vec_nr	        = softirq_vector_max_sum_runtime(pss);
		max_sum_runtime = pss->softirq[vec_nr].sum_runtime;
	}
	if ((vec_nr == TASKLET_SOFTIRQ) || (vec_nr == HI_SOFTIRQ)) {
		/* show more details (function-caller) - only if all caller recorded */
		if (pss->func_array_full == 0) {
			pfuncmax = softirq_func_max_sum_runtime(pss, vec_nr, &all_sum_runtime);
			if (pfuncmax)
				max_sum_runtime = pfuncmax->sum_runtime;
		}
		if (hanging) {
			struct _runtime_stat *pfuncpending = softirq_func_pending(pss);

			if (pfuncpending) {
				/* correct values if hanging function found */
				pfuncmax        = pfuncpending;
				max_sum_runtime = all_sum_runtime;
			}
		}
	}
	runtime_percent = max_sum_runtime * 100;
	do_div(runtime_percent, all_sum_runtime | 0x1);
	if (runtime_percent > 100)
		runtime_percent = 100;

	if (pfuncmax) {
		sprint_symbol_no_offset(fn_name, (unsigned long)pfuncmax->caller);
		snprintf(txt, txt_len, " softirqs:%u.%02u%%%s (%s %lu%%)",
			 PRIV_SCALE_TO_PERCENT(percent_scale), PRIV_SCALE_MOD_PERCENT(percent_scale),
			 hanging ? " (hanging)" : "",
			 fn_name,
			 (unsigned long)runtime_percent);
	} else {
		snprintf(txt, txt_len, " softirqs:%u.%02u%%%s (%s %lu%%)",
			 PRIV_SCALE_TO_PERCENT(percent_scale), PRIV_SCALE_MOD_PERCENT(percent_scale),
			 hanging ? " (hanging)" : "",
			 avm_softirq_name(vec_nr),
			 (unsigned long)runtime_percent);
	}
}

/**
 * called in idle-context or irq-context - only on the same cpu
 */
static char *softirq_runtime(char *txt, int txt_len, int cpu)
{
	struct _softirq_stat *pss = per_cpu_ptr(&avm_softirqstat, cpu);
	unsigned int seq;

	do {
		seq = read_seqcount_begin(&pss->seq);
		_softirq_runtime(txt, txt_len, pss);
	} while (read_seqcount_retry(&pss->seq, seq));

	if (reset_softirqstatistic_after_read)
		/* trigger reset */
		req_softirqstatistic_reset_async(pss, cpu);

	return txt;
}
#define PENDING_POSTFIX " hanging! - values may be incorrect"
/**
 */
static void softirq_proc_statistic(struct seq_file *seq, void *priv __maybe_unused)
{
	unsigned long long all_func_sum_runtime;
	char *ptxt = NULL;
	char fn_name[KSYM_SYMBOL_LEN];
	int cpu, vec_nr, i;

	for_each_online_cpu(cpu) {
		struct _softirq_stat local_pss;
		struct _softirq_stat *pss_cached = &local_pss;
		struct _softirq_stat *pss = per_cpu_ptr(&avm_softirqstat, cpu);
		unsigned int hanging = 0;
		unsigned long avg_usec;
		unsigned long max_usec;
		unsigned long measure_msec;
		unsigned long long runtime_percent;
		unsigned long count;
		int percent_scale, init = 1;
		unsigned int seqcnt;

		do {
			/* cache a snapshot */
			seqcnt = read_seqcount_begin(&pss->seq);
			memcpy(pss_cached, pss, sizeof(*pss));
		} while (read_seqcount_retry(&pss->seq, seqcnt));

		count = get_per_call_stat(&pss_cached->sum_softirq, &max_usec, &avg_usec);
		if (count == 0) {
			continue;
		}
		percent_scale = get_softirq_percent(pss_cached, &measure_msec);
		if (check_softirqstatistic_reset_req_timeout(pss_cached)) {
			/*--- 2 possible reasons: hanging or locked softirq ---*/
			if (softirq_vector_pending(pss_cached) >= 0) {
				/* reason is hanging */
				percent_scale = PERCENT_TO_PRIV_SCALE(100);
				hanging       = 1;
			}
		}
		seq_printf(seq, "[cpu%u][%6lu]Softirq max %9lu avg %6lu us %3u.%02u%% %s(measure-time=%lu.%03lu s)\n",
			   cpu, count, max_usec, avg_usec,
			   PRIV_SCALE_TO_PERCENT(percent_scale), PRIV_SCALE_MOD_PERCENT(percent_scale),
			   hanging ? PENDING_POSTFIX : "",
			   measure_msec / 1000, measure_msec % 1000);

		for (vec_nr = 0; vec_nr < NR_SOFTIRQS; vec_nr++) {
			struct _runtime_stat *pts = &pss_cached->softirq[vec_nr];

			count = get_per_call_stat(pts, &max_usec, &avg_usec);
			if (count == 0) {
				continue;
			}
			if (hanging && pts->pending) {
				ptxt = PENDING_POSTFIX;
				runtime_percent = 100;
			} else {
				ptxt = "";
				runtime_percent = pts->sum_runtime * 100;
				do_div(runtime_percent, pss_cached->sum_softirq.sum_runtime | 0x1); /* not zero */
				if (runtime_percent > 100)
					/* unlocked access -> can happen:
					 * because accumulate pss_cached->sum_softirq.sum_runtime after pts->sum_runtime
					 */
					runtime_percent = 100;
			}
			seq_printf(seq, "\t[%6lu]%-8s:max %6lu avg %6lu us (%3lu%%)%s\n",
				   count,
				   avm_softirq_name(vec_nr),
				   max_usec,
				   avg_usec,
				   (unsigned long)runtime_percent,
				   ptxt);

			softirq_func_max_sum_runtime(pss_cached, vec_nr, &all_func_sum_runtime);
			for (i = 0; i < ARRAY_SIZE(pss_cached->func); i++) {
				struct _runtime_stat *pfunc = &pss_cached->func[i];

				if (pfunc->caller == NULL) {
					continue;
				}
				if ((int)pfunc->vec_nr != vec_nr) {
					continue;
				}
				count = get_per_call_stat(pfunc, &max_usec, &avg_usec);
				if (count == 0) {
					continue;
				}
				if (hanging && pfunc->pending) {
					ptxt = PENDING_POSTFIX;
					runtime_percent = 100;
				} else {
					ptxt = "";
					runtime_percent = pfunc->sum_runtime * 100;
					do_div(runtime_percent, all_func_sum_runtime | 0x1);
				}
				sprint_symbol_no_offset(fn_name, (unsigned long)pfunc->caller);
				seq_printf(seq, "\t\t[%6lu] max %6lu avg %6lu us (%3lu%%) %s%s\n",
					   count,
					   max_usec,
					   avg_usec,
					   (unsigned long)runtime_percent,
					   fn_name,
					   ptxt);
				if (pss_cached->func_array_full && init) {
					init = 0;
					seq_puts(seq, "\t\t...(not all tasklet-functions traced)\n");
				}
			}
		}
		if (reset_softirqstatistic_after_read)
			req_softirqstatistic_reset_async(pss, cpu);
	}
}

/**
 */
static int softirq_proc_statistic_resetflag(char *string, void *priv __maybe_unused)
{
	sscanf(string, "%u", &reset_softirqstatistic_after_read);
	return 0;
}

#define PROC_AVMTASTKLETDIR "avm/tasklet"
/**
 */
int __init avm_softirq_statistic_init(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct _softirq_stat *pss = per_cpu_ptr(&avm_softirqstat, cpu);

		seqcount_init(&pss->seq);
		req_softirqstatistic_reset_async(pss, cpu);
	}
	proc_mkdir(PROC_AVMTASTKLETDIR, NULL);
	add_simple_proc_file("avm/tasklet/stat",
			     softirq_proc_statistic_resetflag,
			     softirq_proc_statistic, NULL);
	/*--- pr_err("%s\n", __func__); ---*/
	return 0;
}
late_initcall(avm_softirq_statistic_init);
#endif/*--- #if defined(CONFIG_AVM_SOFTIRQ_STATISTIC) ---*/
