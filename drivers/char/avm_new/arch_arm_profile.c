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
#include <asm/uaccess.h>
#include <asm/mach_avm.h>
#include <linux/threads.h>
#include <linux/seq_file.h>

#include "avm_profile.h"
#include "arch_profile.h"

#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/cpufreq.h>
#include <asm/thread_info.h>
#include "asm/stacktrace.h"
#include "asm/traps.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#ifdef CONFIG_AVM_FIQ_PUMA7
#include <asm-arm/arch-avalanche/puma7/puma7.h>
#endif

#if defined(PROFILING_PERFORMANCE_COUNTER_SUPPORT)

#ifdef CONFIG_AVM_FASTIRQ_TZ
#ifdef CONFIG_AVM_FASTIRQ_OPTEE
#else
#ifdef CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON
#include <asm/avm_enh/avm_tz.h>
#else
#include <mach/avm_tz.h>
#endif
#endif
#endif

static void arm_profiling_special_enable(enum _simple_profile_enable_mode on,
					 unsigned int enable_perfcnt);

/*--- #define DBG_PERF(args...) pr_info(args) ---*/
#define DBG_PERF(args...) no_printk(args)

#define GET_ARM_PERFORMANCE_EVENT_NAME(x)                                      \
	((x) >= ARRAY_SIZE(performance_counter_options) ?                      \
		 NULL :                                                        \
		 performance_counter_options[x].name)
#define GET_ARM_PERFORMANCE_EVENT_NORMFACTOR(x)                                \
	((x) >= ARRAY_SIZE(performance_counter_options) ?                      \
		 NORMED_MAX :                                                  \
		 performance_counter_options[x].norm_factor)

/**
 */
struct _perfctl_param_read {
	unsigned int counting_reg_mask;
	char *str_buf; /* optional: write out in str_buf */
	int str_len;
};

/**
 */
struct _perfctl_param_write {
	/* Wenn dieser Wert gesetzt, wird auch die jeweilige Statisik-Struktur
	 * geupdatet/ per perf_preset_ctl Werte restauriert werden:
	 */
	struct _fiq_profiling *fiq_profile;

	unsigned int counting_reg_mask;
	enum {
		perf_set_ctl = 0x1,    /* setze komplettes Ctrl-Register */
		perf_reset_cnt = 0x2,  /* setze Count-Register zurueck */
		perf_preset_ctl = 0x4, /* Ctrl-Register aus simple_perfstat
					*  (nur wenn fiq_profile gesetzt)
					*/
	} set_val;
	unsigned int preset_ctl;
};
static void setup_all_perfcounter(struct _perfctl_param_write *perfctl_param,
				  unsigned int cpu);

#if defined(PROFILING_IN_FIQ)
/**
 * Simple Messung mit "festen" Performance-Countern
 */
struct _simple_perfstat {
	unsigned int perf_ctrl
		[PROFILING_MAX_PERF_REGISTER]; /*--- Performance-Counter-Mode ---*/
	unsigned long long sum_perf_count[PROFILING_MAX_PERF_REGISTER];
};

/**
 */
struct _fiq_profiling {
	int id;
	int cpu;
	unsigned long long tsum;
	unsigned int last_time;
	unsigned int
		perf_readmask; /*--- beim Profiling die ersten 2 Performance-Register nicht mittracen, da vom Profiler verwendet  ---*/
	struct _simple_perfstat simple_perfstat;
	struct _roundrobin_perf_ctrlstat *act_roundrobin_perfstat;
	struct _roundrobin_perf_ctrlstat roundrobin_perfstat[48];
};

static struct _fiq_profiling gFiqProfiler[NR_CPUS];

/**
 * Initial fuer Round-Robin Mode vorbereiten
 * roundrobin_perfstat_table: zu initialisierende Table
 * perf_ctrl_ref:  Roundrobin-Ctrl-Presets
 * Die Liste wird als Ringliste initialisiert
 */
static void __init init_roundrobin_perfctrlstat(
	struct _roundrobin_perf_ctrlstat roundrobin_perfstat_table[],
	const struct _roundrobin_perf_ctrlstat perf_ctrl_ref[],
	unsigned int entries)
{
	unsigned int reg, i;

	for (i = 0; i < entries; i++) {
		struct _roundrobin_perf_ctrlstat *table =
			&roundrobin_perfstat_table[i];

		table->prefix = perf_ctrl_ref[i].prefix;
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			unsigned int event;

			table->sum_perf_count[reg] = 0;
			table->sum_perf_time[reg] = 0;
			event = perf_ctrl_ref[i].perf_ctrl[reg];

			if (GET_ARM_PERFORMANCE_EVENT_NAME(event) == NULL) {
				pr_err("[simple-profiling]Warning: Performance-Counter ctrl%u Option %u do not exist\n",
				       reg, event);
				continue;
			}
			table->perf_ctrl[reg] = event;
			/*--- pr_err("ctrl%x Option %2u (%25s) : %08x\n", reg, event, GET_ARM_PERFORMANCE_EVENT_NAME(event), table->perf_ctrl[reg]); ---*/
		}
		table->next =
			&roundrobin_perfstat_table
				[(i + 1) % entries]; /*--- als Ringliste ---*/
	}
}

/**
 */
static char *get_percent(char *txt, int size, unsigned long cnt,
			 unsigned long norm)
{
	unsigned long long val_main, val_remainder;

	if (norm == 0) {
		return "   ?   ";
	}
	val_main = (unsigned long long)cnt * 100;

	do_div(val_main, norm);
	val_remainder = (unsigned long long)cnt * (100 * 100);

	do_div(val_remainder, norm);
	snprintf(txt, size, "%3lu.%02lu %%", (unsigned long)val_main,
		 (unsigned long)val_remainder % 100);
	return txt;
}

/**
 */
#define SHIFT_FACTOR 8 /*--- auf 2^x-tel Sekunde genau ---*/
static inline unsigned long norm_per_sec(unsigned long long count,
					 unsigned long long cycle)
{
	unsigned long long sec = cycle;

	do_div(sec, ((gCycle_per_usec * 1000 * 1000) >> SHIFT_FACTOR));
	if (sec == 0) {
		return 0;
	}
	count <<= SHIFT_FACTOR;
	do_div(count, sec);
	return (unsigned long)count;
}

/**
 * Cleanup der Ringliste
 */
static void
clean_roundrobin_perfstat(struct _roundrobin_perf_ctrlstat *roundrobin_perfstat)
{
	struct _roundrobin_perf_ctrlstat *table = roundrobin_perfstat;
	unsigned int reg;

	while (table) {
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			table->sum_perf_count[reg] = 0;
			table->sum_perf_time[reg] = 0;
		}
		table = table->next;
		if (table == roundrobin_perfstat) {
			break;
		}
	}
}

/**
 * Summiert die letzte Messung des aktuellen Performance-Counters und setzt auf neues Performance-Register
 * return: Zeiger auf naechsten Eintrag
 * aus FIQ-Kontext aufrufen
 */
static struct _roundrobin_perf_ctrlstat *
update_roundrobin_perfstat(struct _roundrobin_perf_ctrlstat *entry,
			   unsigned int perf_cnt[], unsigned int perf_readmask,
			   unsigned int meassure_time)
{
	struct _roundrobin_perf_ctrlstat *next = entry->next;
	unsigned int reg;

	for (reg = 0; reg < PROFILING_MAX_PERF_REGISTER; reg++) {
		entry->sum_perf_count[reg] += perf_cnt[reg];
		entry->sum_perf_time[reg] +=
			(unsigned long long)(meassure_time);

		if (((1 << reg) & (perf_readmask))) {
			if (next) {
				write_p15_performance_event_type_with_cnt_reset(
					reg, next->perf_ctrl[reg]);
			}
		}
	}
	return next;
}

/**
 * Simple-Perfomance-Countertable initialisieren
 * nur aus Kontext der entprechenden CPU aufrufen
 */
static void __init
init_simple_perfstat_per_cpu(struct _simple_perfstat *psimple_perfstat)
{
	unsigned int reg;
	int this_cpu __maybe_unused = get_cpu();

	for (reg = 0; reg < ARRAY_SIZE(psimple_perfstat->sum_perf_count);
	     reg++) {
		psimple_perfstat->sum_perf_count[reg] = 0;
		if (((1 << reg) & (PROFILING_PERF_REGISTERMASK))) {
			psimple_perfstat->perf_ctrl[reg] =
				read_p15_performance_event_type(reg);
			DBG_PERF("%s: cpu=%u reg[%u] = %x\n", __func__,
				 this_cpu, reg,
				 psimple_perfstat->perf_ctrl[reg]);
		}
	}
	put_cpu();
	psimple_perfstat->perf_ctrl[PROFILING_CYCLE_REGISTER] =
		PM_EVENT_CPU_CYCLES; /*--- not really an performance-register but needed ---*/
}

/**
 * Simple Messung mit "festen" Performance-Countern
 * nur aus Kontext der entprechenden CPU aufrufen
 */
static void
clean_simple_perfstat_per_cpu(struct _simple_perfstat *psimple_perfstat)
{
	unsigned int reg;
	int this_cpu __maybe_unused = get_cpu();

	for (reg = 0; reg < ARRAY_SIZE(psimple_perfstat->sum_perf_count);
	     reg++) {
		psimple_perfstat->sum_perf_count[reg] = 0;
		if (((1 << reg) & (PROFILING_PERF_REGISTERMASK))) {
			write_p15_performance_event_type_with_cnt_reset(
				reg, psimple_perfstat->perf_ctrl[reg]);
			DBG_PERF("%s: cpu=%u reg[%u] = %x\n", __func__,
				 this_cpu, reg,
				 psimple_perfstat->perf_ctrl[reg]);
		}
	}
	put_cpu();
}

/**
 * Simple Messung mit "festen" Performance-Countern
 * aus FIQ-Kontext aufrufen
 */
static void update_simple_perfstat(struct _simple_perfstat *simple_perfstat,
				   unsigned int perf_cnt[])
{
	struct _simple_perfstat *psimple_perfstat = simple_perfstat;
	unsigned int reg;

	for (reg = 0; reg < ARRAY_SIZE(psimple_perfstat->sum_perf_count);
	     reg++) {
		psimple_perfstat->sum_perf_count[reg] += perf_cnt[reg];
	}
}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/

/**
 * liefert Anzahl der Performance-Counter
 */
static unsigned int arm_get_performance_counter_nr(void)
{
#if ((defined CONFIG_MACH_PUMA6 ) || (defined CONFIG_MACH_PUMA7))
	uint32_t p_cnts = 2;
#else
	uint32_t p_cnts = (read_p15_performance_monitor_control() >> 11) & 0x1F;
#endif
	return p_cnts;
}

/**
 * Lese perf_ctrl und perf_cnt
 * Welche Register mit Option param->counting_reg_mask
 * Ausgabe wahlweise per printk oder per param->str_buf
 */
static void
read_perfcounter_per_cpu(struct _perfctl_param_read *param)
{
#if defined(PROFILING_IN_FIQ)
	unsigned int reg_cnt;
	unsigned int max_perf;
	char *str_buf = param->str_buf;
	int str_len = param->str_len;

	max_perf = arm_get_performance_counter_nr();

	for (reg_cnt = 0; reg_cnt < max_perf; reg_cnt++) {
		unsigned int cnt, ctl;

		if (((1 << reg_cnt) & param->counting_reg_mask) == 0) {
			continue;
		}
		cnt = read_p15_performance_counter(reg_cnt);
		ctl = read_p15_performance_event_type(reg_cnt);
		if (str_buf == NULL) {
			pr_info("perf%u: cnt=0x%08x ctl=0x%08x \"%s\"\n",
				reg_cnt, cnt, ctl,
				GET_ARM_PERFORMANCE_EVENT_NAME(ctl));
		} else {
			if (str_len > 0) {
				int len = snprintf(
					str_buf, str_len,
					"perf%u: cnt=0x%08x ctl=0x%08x \"%s\"\n",
					reg_cnt, cnt, ctl,
					GET_ARM_PERFORMANCE_EVENT_NAME(ctl));

				len = min(len, str_len);
				str_buf += len;
				str_len -= len;
			}
		}
	}
	if (str_buf) {
		param->str_len = str_buf - param->str_buf;
	} else {
		param->str_len = 0;
	}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
}

/**
 * Setzt perf_ctrl und/oder perf_cnt (Option param->set_val)
 * Welche Register mit Option param->counting_reg_mask
 *
 * Wenn auch perf-Statistik initialisiert werden soll: param->fiq_profile gesetzt
 *                                                     in simple_perfstat wird das event vermerkt
 */
static void
setup_perfcounter_per_cpu(struct _perfctl_param_write *param)
{
#if defined(PROFILING_IN_FIQ)
	struct _fiq_profiling *fiq_profile;
	unsigned int *perf_ctrl;
	unsigned long cnt;
	unsigned int reg;
	size_t nbits;

	fiq_profile = param->fiq_profile;
	cnt = param->counting_reg_mask & PROFILING_PERF_REGISTERMASK;
	perf_ctrl = fiq_profile->simple_perfstat.perf_ctrl;

	nbits = BITS_PER_BYTE * sizeof(param->counting_reg_mask);
	for_each_set_bit(reg, &cnt, nbits) {
		if (param->set_val & perf_preset_ctl && fiq_profile) {
			write_p15_performance_event_type(reg, perf_ctrl[reg]);
			DBG_PERF(
				"%s: %x -> reg[%u]\n", __func__,
				read_p15_performance_event_type(reg),
				reg);
		} else if (param->set_val & perf_set_ctl) {
			write_p15_performance_event_type(
				reg, param->preset_ctl);
			if (fiq_profile) {
				perf_ctrl[reg] = param->preset_ctl;
				DBG_PERF("%s: reg[%u] = %x\n",
					 __func__, reg, param->preset_ctl);
			}
		}
		if (param->set_val & perf_reset_cnt)
			write_p15_performance_counter(reg, 0);
	}
	if ((param->set_val & perf_reset_cnt) && fiq_profile) {
		clean_roundrobin_perfstat(fiq_profile->act_roundrobin_perfstat);
		clean_simple_perfstat_per_cpu(&fiq_profile->simple_perfstat);
	}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
}

/**
 */
static void print_performance_options(void)
{
	unsigned int max_perf, i;

	max_perf = arm_get_performance_counter_nr();
	if (max_perf == 0) {
		return;
	}
	for (i = 0; i < ARRAY_SIZE(performance_counter_options); i++) {
		if (GET_ARM_PERFORMANCE_EVENT_NAME(i) == NULL) {
			continue;
		}
		pr_info("\tctrl:[%3d]: %s\n", i,
			GET_ARM_PERFORMANCE_EVENT_NAME(i));
	}
}

/**
 */
static int print_all_performance_options_cb(unsigned int param1, unsigned int param2)
{
	print_performance_options();
	return 1;
}

/**
 */
static int print_config_cb(unsigned int cpu, unsigned int param2)
{
	struct _perfctl_param_read perfctl_param;

	perfctl_param.counting_reg_mask =
		(1 << arm_get_performance_counter_nr()) - 1;
	perfctl_param.str_buf = NULL;
	perfctl_param.str_len = 0;
#if defined(PROFILING_IN_FIQ)
	if (cpu >= num_possible_cpus()) {
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			smp_call_function_single(
				cpu, (smp_call_func_t)read_perfcounter_per_cpu,
				&perfctl_param, true);
		}
	} else {
		smp_call_function_single(
			cpu, (smp_call_func_t)read_perfcounter_per_cpu,
			&perfctl_param, true);
	}
#else /*--- #if defined(PROFILING_IN_FIQ) ---*/
	read_perfcounter_per_cpu(&perfctl_param);
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_FIQ) ---*/

	return 1;
}

/**
 * ret: len of str
 */
static int arm_get_performance_counter_mode(char *str, int str_len,
					    unsigned int nr)
{
	int len = 0;
	struct _perfctl_param_read perfctl_param;
	unsigned int cpu __maybe_unused;

	perfctl_param.counting_reg_mask = 0x1 << nr;

#if defined(PROFILING_IN_FIQ)
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		int mlen = snprintf(str, str_len, "CPU%u:\n", cpu);

		mlen = min(str_len, mlen);
		str_len -= mlen, str += mlen, len += mlen;

		perfctl_param.str_len = str_len;
		perfctl_param.str_buf = str;
		smp_call_function_single(
			gFiqProfiler[cpu].cpu,
			(smp_call_func_t)read_perfcounter_per_cpu,
			&perfctl_param, true);
		/*--- pr_info("%s: smp-retlen: %u\n", __func__, perfctl_param.str_len); ---*/
		str_len -= perfctl_param.str_len, str += perfctl_param.str_len,
			len += perfctl_param.str_len;
	}
#else /*--- #if defined(PROFILING_IN_FIQ) ---*/
	perfctl_param.str_len = str_len;
	perfctl_param.str_buf = str;
	read_perfcounter_per_cpu(&perfctl_param);
	len = perfctl_param.str_len;
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_FIQ) ---*/
	/*--- pr_info("%s: len: %u\n", __func__, len); ---*/
	return len;
}

/**
 */
static int check_setting_param_cb(unsigned int perf_reg, unsigned int option)
{
	unsigned int perf_max_registers = arm_get_performance_counter_nr();

	if (perf_reg >= perf_max_registers) {
		pr_err("error: invalid param1 on set %u <option> (max %u)\n",
		       perf_reg, perf_max_registers);
		return 1;
	}
	if (option >= ARRAY_SIZE(performance_counter_options) ||
	    (GET_ARM_PERFORMANCE_EVENT_NAME(option) == NULL)) {
		pr_err("error: invalid param2 on set %u <%u>\n", perf_reg,
		       option);
		print_performance_options();
		return 1;
	}
	return 0;
}

/**
 */
static const struct _perf_parse_helper {
	const char *name;
	const char *help;
	enum {
		no_param,
		one_param,
		two_param
	} type;
	int (*func)(unsigned int param1, unsigned int param2);
	enum {
		CPU_IDX = 0,
		SET_IDX,
		TRACE_ROUNDROBIN_IDX,
		TRACE_SIMPLE_IDX,
		START_IDX,
		STOP_IDX,
		MAX_IDX
	} field_idx;
} perf_parse_helper[] = {
	{ .name = "options",
	  .func = print_all_performance_options_cb,
	  .help = "print all performance-options",
	  .type = no_param,
	  .field_idx = MAX_IDX },
	{ .name = "readconfig",
	  .func = print_config_cb,
	  .help = "<cpu>",
	  .type = one_param,
	  .field_idx = MAX_IDX },
#if defined(PROFILING_IN_FIQ)
	{ .name = "stop",
	  .func = NULL,
	  .help = "stop",
	  .type = no_param,
	  .field_idx = STOP_IDX },
	{ .name = "start",
	  .func = NULL,
	  .help = "start (all CPUs)",
	  .type = no_param,
	  .field_idx = START_IDX },
	{ .name = "further params for start:",
	  .func = NULL,
	  .help = "(optional)",
	  .type = no_param,
	  .field_idx = MAX_IDX },
	{ .name = "simple",
	  .func = NULL,
	  .help = "simple instead roundrobin",
	  .type = no_param,
	  .field_idx = TRACE_SIMPLE_IDX },
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
	{ .name = "set",
	  .func = check_setting_param_cb,
	  .help = "<perf-reg> <perf-option> ...",
	  .type = two_param,
	  .field_idx = SET_IDX },
	{ .name = "further params for set/start:",
	  .func = NULL,
	  .help = "(optional)",
	  .type = no_param,
	  .field_idx = MAX_IDX },
#if defined(PROFILING_IN_FIQ) && (NR_CPUS > 1)
	{ .name = "cpu",
	  .func = NULL,
	  .help = "<cpunr>",
	  .type = one_param,
	  .field_idx = CPU_IDX },
#endif /*--- #if defined(PROFILING_IN_FIQ) && (NR_CPUS > 1) ---*/
	{ .name = NULL },
};

#if defined(PROFILING_IN_FIQ)
/**
 * den RoundRobin Performance-Count-Mode aktivieren/deaktivieren
 */
static void set_performance_trace_mode(struct _fiq_profiling *profile_entry,
				       int roundrobin_trace)
{
	if (roundrobin_trace == 0) {
		struct _perfctl_param_write perfctl_param;

		profile_entry->act_roundrobin_perfstat = NULL;

		memset(&perfctl_param, 0x0, sizeof(perfctl_param));
		DBG_PERF("%s -> switch to simple\n", __func__);
		/*--- alle regs restaurieren (preset_ctl aus simple_perfstat) ---*/
		profile_entry->perf_readmask = PROFILING_PERF_REGISTERMASK;
		perfctl_param.set_val = perf_preset_ctl | perf_reset_cnt;
		setup_all_perfcounter(&perfctl_param, num_possible_cpus());
	} else if (roundrobin_trace != -1) {
		profile_entry->act_roundrobin_perfstat =
			profile_entry->roundrobin_perfstat;
	}
	clean_roundrobin_perfstat(profile_entry->act_roundrobin_perfstat);
}

/**
 */
static char *build_cpuinfo_string(char *txt, size_t size,
				  char *prefix, unsigned int val,
				  unsigned int max)
{
	if (size)
		txt[0] = 0;
	if (val >= max) {
		snprintf(txt, size, "all %ss", prefix);
	} else {
		snprintf(txt, size, "%s%u", prefix, val);
	}
	return txt;
}

#define build_cpu_string(txt, val)                                             \
	build_cpuinfo_string(txt, sizeof(txt), "cpu", val, num_possible_cpus())

/**
 */
static void print_performance_trace_mode_by_cpu(unsigned int cpu,
						int processor_trace)
{
	char txt[32];

	pr_err("[simple-profiling]%s:performance-mode: %s\n",
	       build_cpu_string(txt, cpu),
	       processor_trace ? "round-robin" : "simple");
}

/**
 */
static void set_performance_trace_mode_by_cpu(unsigned int cpu,
					      int processor_trace)
{
	if (cpu >= num_possible_cpus()) {
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			set_performance_trace_mode(&gFiqProfiler[cpu],
						   processor_trace);
		}
		return;
	}
	set_performance_trace_mode(&gFiqProfiler[cpu], processor_trace);
}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
#define print_out(seq, args...)                                                \
	{                                                                      \
		if (seq)                                                       \
			seq_printf(seq, args);                                 \
		else                                                           \
			pr_err(args);                                          \
	}
/**
 */
void arm_performance_counter_help(struct seq_file *seq)
{
	const struct _perf_parse_helper *pph = &perf_parse_helper[0];
	unsigned int perf_count;

	perf_count = arm_get_performance_counter_nr();
	print_out(
		seq,
		"\nuse 'cat /proc/avm/profile/perform' to read performance-summary of last profiling\n");
	print_out(
		seq,
		"\nPerformance-Counter: %d\nperform <params>\nparameter(s) for perform:\n",
		perf_count);

	while (pph->name) {
		print_out(seq, "%-10s - %s\n", pph->name, pph->help);
		pph++;
	}
}

#define SKIP_SPACE(a)                                                          \
	{                                                                      \
		while (*(a) && ((*(a) == ' ') || (*(a) == '\t')))              \
			(a)++;                                                 \
	}
#define SKIP_NON_SPACE(a)                                                      \
	{                                                                      \
		while (*(a) && ((*(a) != ' ') && (*(a) != '\t')))              \
			(a)++;                                                 \
	}
/**
 */
static void setup_all_perfcounter(struct _perfctl_param_write *perfctl_param,
				  unsigned int cpu)
{
#if defined(PROFILING_IN_FIQ)
	if (cpu >= num_possible_cpus()) {
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			perfctl_param->fiq_profile = &gFiqProfiler[cpu];
			smp_call_function_single(
				cpu, (smp_call_func_t)setup_perfcounter_per_cpu,
				perfctl_param, true);
		}
	} else {
		perfctl_param->fiq_profile = &gFiqProfiler[cpu];
		smp_call_function_single(
			perfctl_param->fiq_profile->cpu,
			(smp_call_func_t)setup_perfcounter_per_cpu,
			perfctl_param, true);
	}
#else /*--- #if defined(PROFILING_IN_FIQ) ---*/
	setup_perfcounter_per_cpu(perfctl_param);
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_FIQ) ---*/
}

/**
 * echo ... >/proc/avm/profile/perform
 */
static void arm_performance_counter_action(char *p)
{
	struct _perfctl_param_write perfctl_param;
	unsigned int perf_max_registers;
	const struct _perf_parse_helper *pph = &perf_parse_helper[0];
	unsigned int field[MAX_IDX][2];
	int ret = 0;

	memset(field, 0xFF, sizeof(field));
	memset(&perfctl_param, 0x0, sizeof(perfctl_param));
	perf_max_registers = arm_get_performance_counter_nr();
	if (perf_max_registers == 0) {
		return;
	}
	while (pph->name) {
		char *p1;
		unsigned int tmp[2];
		unsigned int idx = 0;

		tmp[0] = tmp[1] = (unsigned int)-1;

		p1 = strstr(p, pph->name);
		if (p1) {
			p1 += strlen(pph->name);
			SKIP_SPACE(p1);
			switch (pph->type) {
			case two_param:
				if (*p1 >= '0' && *p1 <= '9')
					sscanf(p1, "%u", &tmp[idx++]);
				SKIP_NON_SPACE(p1);
				SKIP_SPACE(p1);
				/* FALLTHRU */
			case one_param:
				if (*p1 >= '0' && *p1 <= '9')
					sscanf(p1, "%u", &tmp[idx]);
				/* FALLTHRU */
			case no_param:
				if (pph->func) {
					ret = pph->func(tmp[0], tmp[1]);
				} else if (pph->type == no_param) {
					tmp[0] = 1; /*--- als bool-Wert ---*/
				}
				if (pph->field_idx < MAX_IDX) {
					field[pph->field_idx][0] = tmp[0];
					field[pph->field_idx][1] = tmp[1];
				}
				break;
			}
			if (ret) {
				return;
			}
		}
		pph++;
	}
	if (field[STOP_IDX][0] == 1) {
		arm_profiling_special_enable(sp_enable_off, 0);
		return;
	}
	if (field[START_IDX][0] == 1) {
		if (field[TRACE_ROUNDROBIN_IDX][0] == (unsigned int)-1)
			field[TRACE_ROUNDROBIN_IDX][0] =
				1; /*--- CPU Performance (Round-Robin) ---*/
	}
	if (field[TRACE_SIMPLE_IDX][0] == 1) {
		field[TRACE_ROUNDROBIN_IDX][0] = 0;
	}
#if defined(PROFILING_IN_FIQ)
	print_performance_trace_mode_by_cpu(field[CPU_IDX][0],
					    field[TRACE_ROUNDROBIN_IDX][0]);
	set_performance_trace_mode_by_cpu(field[CPU_IDX][0],
					  field[TRACE_ROUNDROBIN_IDX][0]);
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/

	if ((field[SET_IDX][0] < perf_max_registers) &&
	    (field[SET_IDX][1] < ARRAY_SIZE(performance_counter_options))) {
		unsigned int perf_reg_number = field[SET_IDX][0];
		unsigned int perf_reg_index = field[SET_IDX][1];

		perfctl_param.counting_reg_mask = 1 << perf_reg_number;
		perfctl_param.preset_ctl = (perf_reg_index);
		perfctl_param.set_val = perf_reset_cnt | perf_set_ctl;
		setup_all_perfcounter(&perfctl_param, field[CPU_IDX][0]);
	}
#if defined(PROFILING_IN_FIQ)
	if (field[START_IDX][0] == 1) {
		arm_profiling_special_enable(sp_enable_perform, 0);
	}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
}

#if defined(PROFILING_IN_FIQ)
#define MIN_OFFSET_USEC 500U

/**
 * Range: 500 us - 1524 us
 */
static unsigned long pseudo_random_value(void)
{
	static unsigned short pn_16 = 0xead1;
	unsigned int val = avm_get_cycles();

	pn_16 = (pn_16 >> 1) ^ ((-(pn_16 & 1)) & 0xb400);

	val ^= (val >> PROFILING_TRIGGER_SHIFT) ^ pn_16;
	val &= PROFILING_TRIGGER_MASK;
	return val + MIN_OFFSET_USEC;
}

/**
 * FIQ-Kontext
 */
static irqreturn_t profiling_fiq_handler(int irq, void *handle)
{
	unsigned int act_time, reg;
	unsigned int diff_time;
	unsigned int new_event;
	struct _fiq_profiling *profile_entry = (struct _fiq_profiling *)handle;
	unsigned int perf_cnt[PROFILING_MAX_PERF_REGISTER];

	perf_cnt[PROFILING_CYCLE_REGISTER] = read_p15_cycle_counter();
	act_time = avm_get_cycles();
	diff_time = act_time - profile_entry->last_time;

	profile_entry->tsum += (unsigned long long)(diff_time);
	profile_entry->last_time = act_time;

	for (reg = 0; reg < PROFILING_MAX_PERF_REGISTER; reg++) {
		if ((1 << reg) & profile_entry->perf_readmask) {
			perf_cnt[reg] =
				read_p15_performance_counter_with_reset(reg);
		}
	}
	if (profile_entry->act_roundrobin_perfstat) {
		profile_entry->act_roundrobin_perfstat =
			update_roundrobin_perfstat(
				profile_entry->act_roundrobin_perfstat,
				perf_cnt, profile_entry->perf_readmask,
				diff_time);
	} else {
		update_simple_perfstat(&profile_entry->simple_perfstat,
				       perf_cnt);
	}
	if ((simple_profiling.mask &
	     (1 << avm_profile_data_type_code_address_info))) {
		unsigned long sp;
		struct thread_info *pthread;
		struct task_struct *curr_tsk = NULL;
		struct pt_regs regs;

		copy_banked_regs(&regs, get_fiq_regs());
		if (processor_mode(&regs) == SVC_MODE) {
			sp = regs.ARM_sp;
		} else {
			/*--- der current-thread-sp existiert nur im SVC-Mode ---*/
			sp = get_svc_sp();
		}
		if (sp) {
			pthread = thread_info_by_sp(sp);
			if (pthread) {
				curr_tsk = pthread->task;
			}
		}
		/*--- pr_err("%s:cpsr=0x%lx pc=0x%lx lr=0x%lx sp=0x%lx (%lx / %lx)\n", curr_tsk ? curr_tsk->comm : "?",regs.ARM_cpsr, regs.ARM_pc, regs.ARM_lr, regs.ARM_sp, sp, get_fiq_regs()->ARM_sp); ---*/
		__avm_simple_profiling_code_from_other_context(
			regs.ARM_pc, regs.ARM_lr, curr_tsk, profile_entry->cpu,
			profile_entry->cpu, 0, arch_profile_perfcnt1(),
			arch_profile_perfcnt2(), regs.ARM_sp);
		new_event = pseudo_random_value();
		/*--- new_event = 5000 * 1000;  ---*/ /*--- us ---*/
	} else {
		/*--- ohne Codetrace wird nur Performance-Counter-Statistik getraced - relaxtes triggern ---*/
		new_event = 100 * 1000; /*--- us ---*/
	}
	arch_set_next_trigger(new_event, profile_entry->id);
	write_p15_cycle_counter(0);
	return IRQ_HANDLED;
}

/**
 * entsprechender CPU-Kontext
 */
static void __init init_perfcounter_per_cpu(struct _fiq_profiling *profile_entry)
{
	/*--- fuer Simple-Performance: ---*/
	init_simple_perfstat_per_cpu(&profile_entry->simple_perfstat);
	clean_simple_perfstat_per_cpu(&profile_entry->simple_perfstat);

	/*--- fuer Roundrobin-Performance: ---*/
	init_roundrobin_perfctrlstat(
		profile_entry->roundrobin_perfstat, roundrobin_performance,
		min(ARRAY_SIZE(profile_entry->roundrobin_perfstat),
		    array_size_roundrobin_performance()));
	clean_roundrobin_perfstat(profile_entry->roundrobin_perfstat);
}

/**
 * initialisiere per-cpu profile_entry fuer FIQ und Statistik
 */
static void __init arm_profiling_special_init(void)
{
	unsigned int cpu;

	for (cpu = 0; cpu < ARRAY_SIZE(gFiqProfiler); cpu++) {
		struct _fiq_profiling *profile_entry = &gFiqProfiler[cpu];

		profile_entry->cpu = cpu;
		profile_entry->id = -1;
		smp_call_function_single(
			cpu, (smp_call_func_t)init_perfcounter_per_cpu,
			profile_entry, true);
	}
}

/**
 */
static char *human_time(unsigned long msec, char *buf)
{
	char *start = buf;
	unsigned long sec = (msec % (60 * 1000)) / 1000;

	if (msec >= (60 * 1000)) {
		unsigned long minutes = msec / (60 * 1000);
		int len = sprintf(buf, "%lu min ", minutes);

		buf += len;
	}
	msec = msec % 1000;
	sprintf(buf, "%3lu.%02lu s", sec, msec / 10);
	return start;
}

/**
 * die Simple Statistik (ohne RoundRobin-Mode)
 */
static void simple_performance_statistic(struct _fiq_profiling *profile_entry,
					 struct seq_file *seq)
{
	unsigned long perf_per_sec;
	unsigned int reg;
	struct _simple_perfstat *psimple_perfstat =
		&profile_entry->simple_perfstat;

	if (profile_entry->act_roundrobin_perfstat) {
		return;
	}
	perf_per_sec = norm_per_sec(
		psimple_perfstat->sum_perf_count[PROFILING_CYCLE_REGISTER],
		profile_entry->tsum);
	seq_printf(
		seq, "[%3u]%-52s%30lu.%03lu (Mio Cnts/s)\n",
		psimple_perfstat->perf_ctrl[PROFILING_CYCLE_REGISTER],
		GET_ARM_PERFORMANCE_EVENT_NAME(
			psimple_perfstat->perf_ctrl[PROFILING_CYCLE_REGISTER]),
		(unsigned long)perf_per_sec / 1000 / 1000,
		(perf_per_sec / 1000) % 1000);
	for (reg = 0; reg < PROFILING_MAX_PERF_REGISTER; reg++) {
		if (((profile_entry->perf_readmask) & (1 << reg)) == 0) {
			continue;
		}
		if (psimple_perfstat->sum_perf_count[reg]) {
			perf_per_sec = norm_per_sec(
				psimple_perfstat->sum_perf_count[reg],
				profile_entry->tsum);
			seq_printf(seq, "[%3u]%-52s%30lu.%03lu (Mio Cnts/s)\n",
				   psimple_perfstat->perf_ctrl[reg],
				   GET_ARM_PERFORMANCE_EVENT_NAME(
					   psimple_perfstat->perf_ctrl[reg]),
				   (unsigned long)perf_per_sec / 1000 / 1000,
				   (perf_per_sec / 1000) % 1000);
		}
	}
}

/**
 */
static unsigned long long
sum_roundrobin_perfcount_by_event(unsigned long performance_event,
				  struct _roundrobin_perf_ctrlstat *table,
				  unsigned long long *time)
{
	unsigned long long perf_cnt = 0;
	unsigned long long perf_time = 0;
	struct _roundrobin_perf_ctrlstat *start = table;

	while (table) {
		unsigned int reg;

		for (reg = 0; reg < PROFILING_MAX_PERF_REGISTER; reg++) {
			if (table->perf_ctrl[reg] == performance_event) {
				perf_cnt += table->sum_perf_count[reg];
				perf_time += table->sum_perf_time[reg];
				break; /*--- gleich konfigurierte Events machen keinen Sinn ---*/
			}
		}
		table = table->next;
		if (table == start) {
			break;
		}
	}
	if (time)
		*time = perf_time;
	return perf_cnt;
}

/**
 * RoundRobin-Statistik per Core
 * format:  0 csv
 * format:  1 alle Counts
 *
 * csv: Csv-Format:
 * cpu;0;0;ctrl-nmb;count,msec
 */
static void roundrobin_performance_statistic_processor(
	struct _fiq_profiling *profile_entry, struct seq_file *seq,
	unsigned int format, unsigned int cpu)
{
	char txt1[128], txt2[128];
	struct _roundrobin_perf_ctrlstat *table =
		profile_entry->roundrobin_perfstat;
	unsigned long long msec;
	unsigned long inst_per_sec, cnt_per_sec, perf_per_sec;
	unsigned long long cycle_cnt, time, perf_cnt;
	unsigned long long instr_cnt;
	unsigned int perf_event;
	enum _norm_factor norm_factor;

	cycle_cnt = sum_roundrobin_perfcount_by_event(PM_EVENT_CPU_CYCLES,
						      table, &time);
	msec = time;
	do_div(msec, (gCycle_per_usec * 1000));
	if (msec == 0) {
		return;
	}
	cnt_per_sec = norm_per_sec(cycle_cnt, time);

#if ((defined CONFIG_MACH_PUMA6 ) || (defined CONFIG_MACH_PUMA7))
	instr_cnt = sum_roundrobin_perfcount_by_event(PM_EVENT_INST_EXEC, table,
						      &time);
#else
	instr_cnt = sum_roundrobin_perfcount_by_event(PM_EVENT_INST_RETIRED,
						      table, &time);
#endif
	inst_per_sec = norm_per_sec(instr_cnt, time);

	for (norm_factor = 0; norm_factor < NORMED_MAX; norm_factor++) {
		unsigned int norm_value = 0;

		if (format) {
			switch (norm_factor) {
			case NORMED_BY_FREQUENCY:
#if ((defined CONFIG_MACH_PUMA6 ) || (defined CONFIG_MACH_PUMA7))
				norm_value = /*cpufreq_quick_get(cpu) */
					PAL_sysClkcGetFreq(
						PAL_SYS_CLKC_ARM); /* Hz */
#else
				norm_value =
					cpufreq_quick_get(cpu) * 1000; /* Hz */
#endif
				seq_printf(
					seq,
					"CPU%u: -------------------------- %58s  Percent (normed on Freq %d MHz)\n",
					profile_entry->cpu, "Mio Cnts/s",
					norm_value / 1000 / 1000);
				break;
			case NORMED_BY_CYCLE:
				norm_value = cnt_per_sec;
				seq_printf(
					seq,
					"CPU%u: -------------------------- %58s  Percent (normed on cycle)\n",
					profile_entry->cpu, "Mio Cnts/s");
				break;
			case NORMED_BY_INSTRUCTION:
				seq_printf(
					seq,
					"CPU%u: -------------------------- %58s  Percent (normed on instruction)\n",
					profile_entry->cpu, "Mio Cnts/s");
				norm_value = inst_per_sec;
				break;
			default:
				break;
			}
		}
		for (perf_event = 0; perf_event < PM_EVENT_LAST; perf_event++) {
			if (GET_ARM_PERFORMANCE_EVENT_NAME(perf_event) ==
			    NULL) {
				continue;
			}
			if (GET_ARM_PERFORMANCE_EVENT_NORMFACTOR(perf_event) !=
			    norm_factor) {
				continue;
			}
			perf_cnt = sum_roundrobin_perfcount_by_event(
				perf_event, table, &time);
			if (perf_cnt == 0) {
				continue;
			}
			msec = time;
			do_div(msec, (gCycle_per_usec * 1000));
			if (msec == 0) {
				continue;
			}
			perf_per_sec = norm_per_sec(perf_cnt, time);
			if (format == 0) {
				seq_printf(seq, "%u;%u;%u;%u;%llu;%lu;%s\n",
					   profile_entry->cpu, 0, 0, perf_event,
					   perf_cnt, (unsigned long)msec,
					   GET_ARM_PERFORMANCE_EVENT_NAME(
						   perf_event));
			} else {
				seq_printf(
					seq,
					"[%3u]%-52s%30lu.%03lu %s (%llu - %s)\n",
					perf_event,
					GET_ARM_PERFORMANCE_EVENT_NAME(
						perf_event),
					perf_per_sec / 1000 / 1000,
					(perf_per_sec / 1000) % 1000,
					get_percent(txt2, sizeof(txt2),
						    perf_per_sec, norm_value),
					perf_cnt,
					human_time((unsigned long)msec, txt1));
			}
		}
	}
}

/**
 */
static void arm_profiling_performance_statistic(int cpu, struct seq_file *seq,
						unsigned int format)
{
	char txt[128];
	unsigned long long tsum_msec;
	struct _fiq_profiling *profile_entry;

	if (cpu >= (int)ARRAY_SIZE(gFiqProfiler)) {
		seq_puts(seq, "\n");
		return;
	}
	profile_entry = &gFiqProfiler[cpu];

	tsum_msec = profile_entry->tsum;
	do_div(tsum_msec, (gCycle_per_usec * 1000));

	if (tsum_msec == 0) {
		seq_puts(seq, "\n");
		return;
	}
	seq_printf(seq, " (Measure-Time %s):\n",
		   human_time((unsigned long)tsum_msec, txt));
	if (format) {
		simple_performance_statistic(profile_entry, seq);
	}
	/*--- die Round-Robin-Statistik: ---*/
	if (profile_entry->act_roundrobin_perfstat) {
		/*--- Statistik eines Cores ---*/
		roundrobin_performance_statistic_processor(profile_entry, seq,
							   format, cpu);
	}
	if ((profile_entry->perf_readmask & 0x3) == 0) {
		seq_printf(
			seq,
			"Attention: Profiler reserve perf_reg[0]=%x(%s) and perf_reg[1]=%x (%s)\n",
			profile_entry->simple_perfstat.perf_ctrl[0],
			GET_ARM_PERFORMANCE_EVENT_NAME(
				profile_entry->simple_perfstat.perf_ctrl[0]),
			profile_entry->simple_perfstat.perf_ctrl[1],
			GET_ARM_PERFORMANCE_EVENT_NAME(
				profile_entry->simple_perfstat.perf_ctrl[1]));
	}
}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/

/**
 */
static void arm_profiling_perfcnt_per_cpu(enum _simple_profile_enable_mode on)
{
	arch_profile_perfcnt_on(on);
}

/**
 */
static void
arm_profiling_special_enable(enum _simple_profile_enable_mode on,
			     unsigned int enable_perfcnt)
{
	unsigned int new_event __maybe_unused;
	int cpu;

	/*--- auch ohne fast-irq-support performance-counter anschalten (fuer profiling): ---*/
	for (cpu = 0; cpu < (int)num_possible_cpus(); cpu++) {
		smp_call_function_single(cpu,
					 (smp_call_func_t)arm_profiling_perfcnt_per_cpu,
					 (void *)on, true);
	}
#ifdef CONFIG_AVM_FASTIRQ_TZ
#ifdef CONFIG_AVM_FASTIRQ_OPTEE
#else
	{
		uint32_t version, modified;

		if (avm_get_tz_version(&version, &modified)) {
			pr_err("[%s] No AVM TZ found, profiling not possible\n",
			       __func__);
			return;
		}
	}
#endif
#endif
#if defined(PROFILING_IN_FIQ)
	/*--- ohne Codetrace wird nur Performance-Counter-Statistik getraced - relaxtes triggern ---*/
	new_event = 100 * 1000; /*--- usec ---*/
	for_each_online_cpu(cpu) {
		struct _fiq_profiling *profile_entry = &gFiqProfiler[cpu];

		if (on) {
			profile_entry->tsum = 0;
			profile_entry->perf_readmask =
				PROFILING_PERF_REGISTERMASK;
			profile_entry->last_time = avm_get_cycles();
			if (on != sp_enable_perform) {
				struct _perfctl_param_write perfctl_param;

				memset(&perfctl_param, 0x0,
				       sizeof(perfctl_param));
				profile_entry->perf_readmask &=
					~0x3; /*--- die ersten 2 Register fuer Profiler reserviert ---*/
				/*--- reg0 und reg1 restaurieren (preset_ctl in simple_perfstat): ---*/
				perfctl_param.counting_reg_mask = 0x3;
				perfctl_param.set_val =
					perf_preset_ctl | perf_reset_cnt;
				setup_all_perfcounter(&perfctl_param,
						      num_possible_cpus());
			}
			if (profile_entry->id < 0) {
				profile_entry->id = arch_setup_timer_firq(
					cpu, profiling_fiq_handler,
					profile_entry);
			}
			if (profile_entry->id >= 0) {
				if ((simple_profiling.mask &
				     (1
				      << avm_profile_data_type_code_address_info))) {
					new_event = pseudo_random_value();
				}
				/*--- pr_info("%s: id=%x\n", __func__, profile_entry->id); ---*/
				arch_set_next_trigger(new_event,
						      profile_entry->id);
			}
		} else if ((profile_entry->id >= 0)) {
			arch_free_timer_firq(profile_entry->id, profile_entry);
			profile_entry->id = -1;
		}
	}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
}

/**
 */
static int __init arch_arm_profiler_init(void)
{
	unsigned int perf_count;

#if defined(PROFILING_IN_FIQ)
	/*--- arch_init_arm_cpu_config(); ---*/
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
	arch_profile_ctrl.cpu_profile = arm_cpu_config;
	arch_profile_ctrl.performance_counter_action   = arm_performance_counter_action;
	arch_profile_ctrl.performance_counter_help     = arm_performance_counter_help;
	arch_profile_ctrl.get_performance_counter_nr   = arm_get_performance_counter_nr;
	arch_profile_ctrl.get_performance_counter_mode = arm_get_performance_counter_mode;
#if defined(PROFILING_IN_FIQ)
	arch_profile_ctrl.profiling_special_enable        = arm_profiling_special_enable;
	arch_profile_ctrl.profiling_performance_statistic = arm_profiling_performance_statistic;
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
	perf_count = arm_get_performance_counter_nr();
	pr_err("[simple-profiling]:%d performance counters implemented\n",
	       perf_count);
	if (perf_count) {
		/*--- Preset der Performance-Counter --- */
		INITIAL_EVENT_CNT_SETUP();
	}
#if defined(PROFILING_IN_FIQ)
	arm_profiling_special_init();
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
	return 0;
}

device_initcall(arch_arm_profiler_init);

#endif /*--- #if defined(PROFILING_PERFORMANCE_COUNTER_SUPPORT) ---*/
