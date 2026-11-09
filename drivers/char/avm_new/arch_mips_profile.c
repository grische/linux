// SPDX-License-Identifier: GPL-2.0+

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#pragma GCC pop_options

#include <asm/mach_avm.h>
#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>

#if defined(CONFIG_AVM_POWER)
#if __has_include(<avm/power/power.h>)
#include <avm/power/power.h>
#else
#include <linux/avm_power.h>
#endif
#endif

#include "avm_profile.h"
#define ARCH_MIPS_PROFILE_C
#include "arch_profile.h"

#if defined(PROFILING_CPU_HAS_TC)

#define write_tc_c0_perfcntr0(val) mttc0(25, 1, val)
#define write_tc_c0_perfcntr1(val) mttc0(25, 3, val)

#define write_tc_c0_perfctrl0(val) mttc0(25, 0, val)
#define write_tc_c0_perfctrl1(val) mttc0(25, 2, val)

#define read_tc_c0_perfcntr0() mftc0(25, 1)
#define read_tc_c0_perfcntr1() mftc0(25, 3)

#define read_tc_c0_perfctrl0() mftc0(25, 0)
#define read_tc_c0_perfctrl1() mftc0(25, 2)

static DEFINE_SPINLOCK(perfcnt_lock);
static DEFINE_SPINLOCK(trace_lock);
static void mips_profiling_special_enable(enum _simple_profile_enable_mode on,
					  unsigned int enable_perfcnt);
extern unsigned long kernelsp[NR_CPUS];
#else /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/

#define PROFILING_MAX_COUNT_TCS 1
#define PROFILING_MAX_PERF_REGISTER 2

#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/

#define MIPS_PERFORMANCE_HAS_MORE                   (1 << 31)
#define MIPS_PERFORMANCE_IRQ_ENABLE                 (1 << 4)
#define MIPS_PERFORMANCE_USER_MODE_ENABLE           (1 << 3)
#define MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE     (1 << 2)
#define MIPS_PERFORMANCE_KERNEL_MODE_ENABLE         (1 << 1)
#define MIPS_PERFORMANCE_EXCEPTION_ENABLE           (1 << 0)
#define MIPS_PERFORMANCE_MODE_MASK                                             \
	(MIPS_PERFORMANCE_USER_MODE_ENABLE |                                   \
	 MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE |                             \
	 MIPS_PERFORMANCE_KERNEL_MODE_ENABLE |                                 \
	 MIPS_PERFORMANCE_EXCEPTION_ENABLE)
#define MIPS_PERFORMANCE_VPE_SPECIFIC_ENABLE        (0x01 << 20)
#define MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE         (0x02 << 20)
#define MIPS_PERFORMANCE_VPEID(x)                   (((x) & 0x0F) << 16)
#define MIPS_PERFORMANCE_TCID(x)                    (((x) & 0xFF) << 22)
#define MIPS_PERFORMANCE_EVENT(x)                   (((x) & 0x7F) << 5)
#define GET_MIPS_PERFORMANCE_EVENT(x)               (((x) >>  5) & 0x7F)
#define GET_MIPS_PERFORMANCE_TCID(x)                (((x) >> 22) & 0xFF)
#define GET_MIPS_PERFORMANCE_VPEID(x)               (((x) >> 16) & 0x0F)

/*--- #define DBG_PERF(args...) pr_info(args) ---*/
#define DBG_PERF(args...) no_printk(args)

#define performance_counter_options(reg, ctrl)                                 \
	_performance_counter_options[(reg)&0x1][(ctrl)&0x7F]
#if defined(CONFIG_CPU_MIPS_74K)
static const char *_performance_counter_options[2][128] = {
	[0] = { [0] = "Cycles",
		[1] = "Instructions completed",
		[2] = "JR r31 (return) instructions",
		[3] = "Cycles where no instn is fetched or after wait",
		[4] = "ITLB accesses",
		[6] = "Instruction cache accesses",
		[7] = "Cycles without Instn fetch due to I-cache miss",
		[8] = "Cycles waiting for direct Instn fetch",
		[9] = "Replays in IFU due to full Instn buffer",
		[13] = "Cycles with no Instn to ALU due to full buffer",
		[14] = "Cycles with no Instn to ALU due to no free ALU CB",
		[15] = "Cycles without Instn added to ALU due to no free FIFOs",
		[16] = "Cycles with no ALU-pipe issue: no Instn avail.",
		[17] = "Cycles with no ALU-pipe issue: no operands ready",
		[18] = "Cycles with no ALU-pipe issue: ressource busy",
		[19] = "ALU pipe-bubbles issued",
		[20] = "Cycles with no Instn issued",
		[21] = "Out-of-order ALU issue",
		[22] = "Graduated JAR/JALR.HB",
		[23] = "Cacheable loads",
		[24] = "D-Cache writebacks",
		[26] = "D-side JTLB accesses",
		[28] = "L2 cache writebacks",
		[29] = "L2 cache misses",
		[30] = "Pipe stalls due to full FSB",
		[31] = "Pipe stalls due to full LDQ",
		[32] = "Pipe stalls due to full WBB",
		[35] = "Redirects following optimistic instn issue which failed",
		[36] = "JR (not r31) instructions",
		[37] = "Branch-likely instns graduated",
		[38] = "L2 I-miss stall cycles",
		[39] = "Branches graduated",
		[40] = "Integer instns graduated",
		[41] = "Loads graduated",
		[42] = "j/ja1 graduated",
		[43] = "Co-ops graduated",
		[44] = "DSP instructions graduated",
		[45] = "DSP branch instructions graduated",
		[46] = "Uncached loads graduated",
		[49] = "EJTAG Instruction Triggerpoints",
		[50] = "CP1 branches mispredicted",
		[51] = "sc instructions graduated",
		[52] = "prefetch instns graduated top of LSGB",
		[53] = "Cycles where no instns graduated",
		[54] = "Cycles where one instn graduated",
		[55] = "GFifo blocked cycles",
		[56] = "Cycles where 0 instns graduated",
		[58] = "Exceptions taken",
		[59] = "Impl. specific CorExtend event",
		[62] = "Impl. specific ISPRAM event",
		[63] = "L2 single bit errors corrected" },
	[1] = { [0] = "Cycles",
		[1] = "Instructions completed",
		[2] = "JR r31 mispredictions",
		[3] = "JR r31 not predicted",
		[4] = "ITLB misses",
		[5] = "JTLB instruction access fails",
		[6] = "Instruction cache misses",
		[7] = "L2 I-miss cycles",
		[8] = "PDTrace back stalls",
		[9] = "Fetch slots killed in IFU",
		[13] = "AGEN issue pool full",
		[14] = "run out of AGEN CBs",
		[15] = "IOIQ FIFO full",
		[16] = "No instns avail. for AGEN-pipe issue",
		[17] = "No operands avail. for AGEN-pipe issue",
		[18] = "No AGEN-pipe issue, waiting for data",
		[20] = "Cycles with two instns issued",
		[21] = "Out-of-order AGEN issue",
		[22] = "D-cache line refill (not LD/ST misses)",
		[23] = "All D-cache accesses",
		[24] = "D-Cache misses",
		[25] = "D-side JTBL translt. fails",
		[26] = "Bogus D-ache misses",
		[28] = "L2 cache accesses",
		[29] = "L2 cache misses",
		[30] = "FSB >1/2 full",
		[31] = "LDQ >1/2 full",
		[32] = "WBB >1/2 full",
		[35] = "Copro. load instns.",
		[36] = "jr $31 graduated after mispredict",
		[37] = "CP1/CP2 conditional branch instns. graduated",
		[38] = "Mispredicted branch-like ins. graduated",
		[39] = "Mispredicted branches graduated",
		[40] = "FPU instructions graduated",
		[41] = "Stores graduated",
		[42] = "MIPS16 instn. graduated",
		[43] = "integer multiply/divide graduated",
		[44] = "ALU-DSP graduated, result saturated",
		[45] = "MDU-DSP graduated, result saturated",
		[46] = "Uncached stores graduated",
		[49] = "EJTAG data triggers",
		[51] = "sc instrns. failed",
		[52] = "prefetch instns. cache hits",
		[53] = "load misses graduated",
		[54] = "Two instns. graduated",
		[55] = "Floating point stores graduated",
		[56] = "Cycles where 0 instns. graduated",
		[58] = "Replays initiated from graduation",
		[59] = "Impl. specific system event",
		[61] = "Reserved for CP2 event",
		[62] = "Impl. specific DSPRAM block event" }
};
#else /*--- #if defined(CONFIG_CPU_MIPS_74K) ---*/
static const char *_performance_counter_options[2][128] = {
	[0] = {   [0] = "Cycles (P)",
		  [1] = "Instructions completed",
		  [2] = "branch instructions completed",
		  [3] = "JR r31 (return) instructions",
		  [4] = "JR (not r31) instructions",
		  [5] = "ITLB accesses",
		  [6] = "DTLB accesses",
		  [7] = "JTLB instruction accesses",
		  [8] = "JTLB data accesses",
		  [9] = "Instruction Cache accesses",
		  [10] = "Data cache load/stores",
		  [11] = "Data cache load/store misses",
		  [13] = "Store misses",
		  [14] = "integer instructions completed",
		  [15] = "loads completed",
		  [16] = "J/JAL completed",
		  [17] = "no-ops completed",
		  [18] = "Main pipeline stalls (P)",
		  [19] = "SC instructions completed",
		  [20] = "Prefetch instructions to cached addresses",
		  [21] = "L2 cache writebacks (P)",
		  [22] = "L2 cache misses (P)",
		  [23] = "Exceptions taken",
		  [24] = "cache fixup",
		  [25] = "IFU stall cycles (P)",
		  [26] = "DSP Instructions Completed",
		  [28] = "Impl. specific PM event",
		  [29] = "Impl. specific ISPRAM event",
		  [30] = "Impl. specific CorExtend event",
		  [31] = "Impl. specific customer yield manager event",
		  [32] = "ITC loads",
		  [33] = "Uncached loads",
		  [34] = "fork instructions completed",
		  [35] = "CP2 register-to-register Instns Completed",
		  [36] = "Intervention stall main pipe (P)",
		  [37] = "I$ Miss Stall cycles",
		  [38] = "SYNC stall cycles",
		  [39] = "D$ miss cycles (P)",
		  [40] = "Uncached stall cycles",
		  [41] = "MDU stall cycles",
		  [42] = "CP2 stall cycles",
		  [43] = "ISPRAM Stall Cycles",
		  [44] = "CACHE Instn stall cycles (P)",
		  [45] = "Load to Use stalls",
		  [46] = "Read-CP0-value interlock stalls",
		  [47] = "Relax bubbles (V)",
		  [48] = "IFU FB full refetches",
		  [49] = "EJTAG Instruction Triggerpoints",
		  [50] = "FSB < 1/4 full (P)",
		  [51] = "FSB > 1/2 full (P)",
		  [52] = "LDQ < 1/4 full (P)",
		  [53] = "LDQ > 1/2 full (P)",
		  [54] = "WBB < 1/4 full (P)",
		  [55] = "WBB > 1/2 full (P)",
		  [56] = "Intervention Hits (P)",
		  [57] = "All Invalidates (P)",
		  [58] = "Evictions (P)",
		  [59] = "ST_Inval (P)",
		  [60] = "ST_Store_S",
		  [61] = "Request Latency to Self Intervention (P)",
		  [62] = "Request Latency to Read Response (P)",
		  [64] = "System Specific event 0 (P)",
		  [65] = "System Specific event 2 (P)",
		  [66] = "System Specific event 4 (P)",
		  [67] = "System Specific event 6 (P)" },
	  [1] = { [0] = "Cycles (P)",
		  [1] = "Instructions completed",
		  [2] = "Branch mispredictions",
		  [3] = "JR r31 mispredictions",
		  [4] = "JR r31 not predicted",
		  [5] = "ITLB misses",
		  [6] = "DTLB misses",
		  [7] = "JTLB instruction misses",
		  [8] = "JTLB data misses",
		  [9] = "Instruction cache misses",
		  [10] = "Data cache writebacks",
		  [11] = "Data cache load/store misses",
		  [13] = "Load misses",
		  [14] = "FPU instructions completed",
		  [15] = "stores completed",
		  [16] = "MIPS16 instructions completed",
		  [17] = "integer multiply/divide completed",
		  [18] = "replay traps (other than uTLB)",
		  [19] = "SC instructions failed",
		  [20] = "Prefetch instructions completed with cache hit",
		  [21] = "L2 cache accesses (P)",
		  [22] = "L2 cache single bit errors corrected (P)",
		  [23] = "Cycles spent in Single Threaded Mode",
		  [24] = "Refetches: refetched and reissued by IFU",
		  [25] = "ALU stalls (P)",
		  [26] = "ALU-DSP Saturations Done",
		  [27] = "MDU-DSP Saturations Done",
		  [28] = "Impl. specific Cp2 event",
		  [29] = "Impl. specific DSPRAM event",
		  [31] = "Custom ITC event",
		  [32] = "ITC Stores",
		  [33] = "Uncached Stores",
		  [34] = "yield instructions completed",
		  [35] = "CP2 To/From Instns completed",
		  [36] = "Intevention response stalled on miss (P)",
		  [37] = "D$ miss stall cycles",
		  [38] = "FSB stall cycles",
		  [39] = "L2 miss cycles (P)",
		  [40] = "ITC stall cycles",
		  [41] = "FPU stall cycles",
		  [42] = "CorExtend stall cycles",
		  [43] = "DSPRAM stall cycles",
		  [44] = "Long stall cycle",
		  [45] = "ALU to AGEN stalls",
		  [46] = "Branch mispredict stalls (P)",
		  [47] = "Number of corrected ECC errors in the L1 Data Cache or DSPRAM (V)",
		  [48] = "FB entry allocated (P)",
		  [49] = "EJTAG Data Triggerpoints",
		  [50] = "FSB 1/4-1/2 full (P)",
		  [51] = "FSB full pipeline stalls (P)",
		  [52] = "LDQ 1/4-1/2 full (P)",
		  [53] = "LDQ full pipeline stalls (P)",
		  [54] = "WBB 1/4-1/2 full (P)",
		  [55] = "WBB full pipeline stalls (P)",
		  [56] = "All Intervention (P)",
		  [57] = "Invalidate Hits (P)",
		  [58] = "Writebacks (P)",
		  [59] = "ST_Exclusive (P)",
		  [60] = "ST_Downgrade (P)",
		  [61] = "Request Count for SI Latency (P)",
		  [62] = "Request Count for Resp Latency (P)",
		  [64] = "System Specific event 1 (P)",
		  [65] = "System Specific event 3 (P)",
		  [66] = "System Specific event 5 (P)",
		  [67] = "System Specific event 7 (P)" }
};
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_CPU_MIPS_74K) ---*/

/**
 * Simple Messung mit "festen" Performance-Countern
 */
struct _simple_performance_count_tc_stat {
	unsigned int perf_ctrl
		[PROFILING_MAX_PERF_REGISTER]; /*--- Performance-Counter-Mode ---*/
	unsigned int last_perf_count[PROFILING_MAX_PERF_REGISTER];
	unsigned long long sum_perf_count[PROFILING_MAX_PERF_REGISTER];
	unsigned long long sum_tc_sched;
	unsigned int last_tc_sched;
};
struct _yield_profiling;

/**
 * Liste mit Round-Robin-Performance-Counter pro TC
 */
struct _roundrobin_perf_ctrl_tc_stat {
	unsigned int perf_ctrl[PROFILING_MAX_PERF_REGISTER];
	unsigned long long sum_perf_time[PROFILING_MAX_PERF_REGISTER];
	unsigned long long sum_perf_count[PROFILING_MAX_PERF_REGISTER];
	void (*proceed)(struct seq_file *seq,
			struct _yield_profiling *profile_entry,
			struct _roundrobin_perf_ctrl_tc_stat *pstat,
			unsigned long inst_per_sec);
	const char *prefix;
	const char *prefix2;
	struct _roundrobin_perf_ctrl_tc_stat *next;
};

/**
 */
struct _yield_profiling {
	int yield_handle;
	int yield_signal;
	int yield_profile_id;
	unsigned int irq_nr;
	enum {
		YIELD_NOT_STOP = (1 << 0),
		YIELD_TRACE_ON = (1 << 1),
	} yield_mode;
	unsigned int linux_os_cpu;
	unsigned int core;
	unsigned int uart_trace; /*--- UART-Trace ueber GPIO's ---*/
	unsigned int uart_trace_cnt;
	unsigned int ntc;
	unsigned int profiling_tc;
	unsigned int long long tsum;
	unsigned int last_time;
	unsigned int tc_to_cpu_nr[PROFILING_MAX_COUNT_TCS];
	struct _simple_performance_count_tc_stat
		simple_tcstat[PROFILING_MAX_COUNT_TCS];
	struct _roundrobin_perf_ctrl_tc_stat
		*act_roundrobin_tcstat[PROFILING_MAX_COUNT_TCS];
	struct _roundrobin_perf_ctrl_tc_stat
		roundrobin_tc_stat[PROFILING_MAX_COUNT_TCS][32];
};

#if defined(PROFILING_IN_YIELD)
static struct _yield_profiling gYieldProfiler[PROFILING_CORES];

/*
 * Per-TC-Table initial fuer Round-Robin Mode vorbereiten
 * mit dem Flag wird gesagt, ob dieser Thread nur Performance-Counter pro Prozessor
 * nehmen soll (der Monitor-TC ist dafuer vorgesehen)
 * tc_stat_table: zu initialisierende Table
 * Die Liste wird als Ringliste initialisiert
 */
static void
init_tc_stat(struct _roundrobin_perf_ctrl_tc_stat tc_stat_table[],
	     const struct _roundrobin_perf_ctrl_tc_stat tc_ctrl_ref[],
	     unsigned int entries, unsigned int tc, unsigned int per_processor)
{
	unsigned int reg, i;

	for (i = 0; i < entries; i++) {
		struct _roundrobin_perf_ctrl_tc_stat *table = &tc_stat_table[i];

		table->proceed = tc_ctrl_ref[i].proceed;
		table->prefix = tc_ctrl_ref[i].prefix;
		table->prefix2 = tc_ctrl_ref[i].prefix2;
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			unsigned int event;

			table->sum_perf_count[reg] = 0;
			table->sum_perf_time[reg] = 0;
			event = GET_MIPS_PERFORMANCE_EVENT(
				tc_ctrl_ref[i].perf_ctrl[reg]);

			if (performance_counter_options(reg, event) == NULL) {
				pr_err("[simple-profiling]Warning: Performance-Counter ctrl%u Option %u do not exist\n",
				       reg, event);
				continue;
			}
			if (per_processor) {
				if (strstr(performance_counter_options(reg, event), "(P)") == NULL) {
					pr_err("[simple-profiling]Warning: Performance-Counter ctrl%u Option %u (%s)"
					       "doesn't support per-Processor-Tracing\n",
					       reg, event,
					       performance_counter_options(
						       reg, event));
				}
			} else {
				if (strstr(performance_counter_options(reg, event), "(P)") ||
				    strstr(performance_counter_options(reg, event), "(V)")) {
					pr_err("[simple-profiling]Warning: Performance-Counter ctrl%u Option %u (%s)"
					       "doesn't support per-TC-Tracing\n",
					       reg, event,
					       performance_counter_options(
						       reg, event));
				}
			}
			table->perf_ctrl[reg] =
				MIPS_PERFORMANCE_EVENT(event) |
				MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE |
				MIPS_PERFORMANCE_TCID(tc) |
				MIPS_PERFORMANCE_USER_MODE_ENABLE |
				MIPS_PERFORMANCE_KERNEL_MODE_ENABLE |
				MIPS_PERFORMANCE_EXCEPTION_ENABLE |
				MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE;
			/*--- pr_err("TC%x ctrl%x Option %2u (%25s) : %08x\n", tc, reg, event, performance_counter_options(reg, event), table->perf_ctrl[reg]); ---*/
		}
		table->next = &tc_stat_table[(i + 1) %
					     entries]; /*--- als Ringliste ---*/
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
static char *get_relation(char *txt, int size, unsigned long cnt,
			  unsigned long norm)
{
	unsigned long long val_main, val_remainder;

	if (norm == 0) {
		return "   ?   %";
	}
	val_main = (long long)cnt;
	do_div(val_main, norm);
	val_remainder = (long long)cnt * 1000;
	do_div(val_remainder, norm);
	snprintf(txt, size, "%3lu.%03lu", (unsigned long)val_main,
		 (unsigned long)val_remainder % 1000);
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
 * Achtung! fixe Definition Entry0, Reg1 des PROFILING_MAX_COUNT_TCS muss Cycle-Counter sein !
 */
static unsigned long norm_cycle_sec(struct _yield_profiling *profile_entry)
{
	struct _roundrobin_perf_ctrl_tc_stat *pstat =
		&profile_entry
			 ->roundrobin_tc_stat[profile_entry->profiling_tc][0];

	if (GET_MIPS_PERFORMANCE_EVENT(pstat->perf_ctrl[1]) != 0) {
		pr_err("[simple-profiling]%s: error: cycle-counter not on position 0 - reg1\n",
		       __func__);
		return 0;
	}
	return norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);
}

/**
 */
static unsigned long long
all_instruction_completed(struct _yield_profiling *profile_entry)
{
	unsigned int tc;
	unsigned long long all_instruction = 0;

	for (tc = 0; tc < profile_entry->ntc; tc++) {
		struct _simple_performance_count_tc_stat *psimple_tcstat =
			&profile_entry->simple_tcstat[tc];

		all_instruction += psimple_tcstat->sum_tc_sched;
	}
	return all_instruction;
}
#if 0

/**
 * eine Option ueber alle TC's aufsummieren
 */
static unsigned long long sum_tc_option(struct _yield_profiling *profile_entry, unsigned int reg,
					unsigned int option, unsigned long long *_tsum)
{
	struct _roundrobin_perf_ctrl_tc_stat *table, *roundrobin_tc_stat;
	unsigned long long sum = 0, tsum = 0;
	unsigned int tc;

	if (reg >= ARRAY_SIZE(table->perf_ctrl)) {
		*_tsum = 0;
		return 0;
	}
	for (tc = 0; tc < PROFILING_MAX_COUNT_TCS; tc++) {
		table   = profile_entry->roundrobin_tc_stat[tc];
		roundrobin_tc_stat = table;
		while (table) {
			if (table->perf_ctrl[reg] == option) {
				sum  += table->sum_perf_count[reg];
				tsum += table->sum_perf_time[reg];
			}
			table = table->next;
			if (table == roundrobin_tc_stat) {
				break;
			}
		}
	}
	*_tsum = tsum;
	return sum;
}
#endif

/**
 */
static void instruction_per_cycle(struct seq_file *seq,
				  struct _yield_profiling *profile_entry,
				  struct _roundrobin_perf_ctrl_tc_stat *pstat,
				  unsigned long inst_per_sec)
{
	char txt[3][128];
	unsigned long stall_norm =
		norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long cycle_norm = norm_cycle_sec(profile_entry);
	unsigned long cpu_freq = avm_get_clock(avm_clock_id_cpu);
	unsigned long instruction_norm = norm_per_sec(
		all_instruction_completed(profile_entry), profile_entry->tsum);

	if (cycle_norm == 0) {
		return;
	}
	seq_printf(
		seq,
		"%s %lu MHz: Run: %s Instructions/Cycle %s Stalls/Instruction: %s\n",
		pstat->prefix ? pstat->prefix : "", cpu_freq / (1000 * 1000),
		get_percent(txt[0], sizeof(txt[0]), cycle_norm, cpu_freq),
		get_relation(txt[1], sizeof(txt[1]), instruction_norm,
			     cycle_norm),
		get_relation(txt[2], sizeof(txt[2]), stall_norm,
			     instruction_norm));
}
#if 0

/**
 */
static void relation(struct seq_file *seq, struct _yield_profiling *profile_entry,
			struct _roundrobin_perf_ctrl_tc_stat *pstat,  unsigned long inst_per_sec)
{
	char txt[128];
	unsigned long p1_norm = norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long p2_norm = norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);

	if (p1_norm == 0) {
		return;
	}
	seq_printf(seq, "%-33s: 1.00 : %s\n", pstat->prefix, get_relation(txt, sizeof(txt), p2_norm, p1_norm));
}
#endif

/**
 */
static void
sum_instruction_relation(struct seq_file *seq,
			 struct _yield_profiling *profile_entry,
			 struct _roundrobin_perf_ctrl_tc_stat *pstat,
			 unsigned long inst_per_sec)
{
	char txt[128];
	unsigned long p1_norm =
		norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long p2_norm =
		norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);

	seq_printf(seq, "%-33s: %s (normed on instr)\n", pstat->prefix,
		   get_percent(txt, sizeof(txt), p1_norm + p2_norm,
			       inst_per_sec));
}

/**
 */
static void param_instruction_relation(
	struct seq_file *seq,
	struct _yield_profiling *profile_entry,
	struct _roundrobin_perf_ctrl_tc_stat *pstat,
	unsigned long inst_per_sec)
{
	char txt[128];

	if (pstat->prefix) {
		unsigned long p1_norm = norm_per_sec(pstat->sum_perf_count[0],
						     pstat->sum_perf_time[0]);

		seq_printf(seq, "%-33s: %s (normed on instr)\n", pstat->prefix,
			   get_percent(txt, sizeof(txt), p1_norm,
				       inst_per_sec));
	}
	if (pstat->prefix2) {
		unsigned long p2_norm = norm_per_sec(pstat->sum_perf_count[1],
						     pstat->sum_perf_time[1]);

		seq_printf(seq, "%-33s: %s (normed on instr)\n", pstat->prefix2,
			   get_percent(txt, sizeof(txt), p2_norm,
				       inst_per_sec));
	}
}

/**
 */
static void
param_cycle_relation(struct seq_file *seq,
		     struct _yield_profiling *profile_entry,
		     struct _roundrobin_perf_ctrl_tc_stat *pstat,
		     unsigned long inst_per_sec)
{
	unsigned long cycle_norm = norm_cycle_sec(profile_entry);
	char txt[128];

	if (pstat->prefix) {
		unsigned long p1_norm = norm_per_sec(pstat->sum_perf_count[0],
						     pstat->sum_perf_time[0]);

		/*--- seq_printf(seq, "debug:%s: p1_norm=%lu cycle_per_sec: %lu\n", pstat->prefix, p1_norm, cycle_norm); ---*/
		seq_printf(seq, "%-33s: %s (normed on cycle)\n", pstat->prefix,
			   get_percent(txt, sizeof(txt), p1_norm, cycle_norm));
	}
	if (pstat->prefix2) {
		unsigned long p2_norm = norm_per_sec(pstat->sum_perf_count[1],
						     pstat->sum_perf_time[1]);

		/*--- seq_printf(seq, "debug:%s: p1_norm=%lu cycle_per_sec: %lu\n", pstat->prefix, p2_norm, cycle_norm); ---*/
		seq_printf(seq, "%-33s: %s (normed on cycle)\n", pstat->prefix2,
			   get_percent(txt, sizeof(txt), p2_norm, cycle_norm));
	}
}

/**
 * Cnt im Verhaeltnis zu den Instructions
 */
static void relation_instruction_and_pair(
	struct seq_file *seq,
	struct _yield_profiling *profile_entry,
	struct _roundrobin_perf_ctrl_tc_stat *pstat, unsigned long inst_per_sec)
{
	char txt[128], txt2[128];
	unsigned long p1_norm =
		norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long p2_norm =
		norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);

	/*--- seq_printf(seq, "debug:%s: p1_norm=%lu p2_norm=%lu inst_per_sec: %lu\n", pstat->prefix, p1_norm, p2_norm, inst_per_sec); ---*/
	seq_printf(seq,
		   "%-33s: %s (normed on instr) %s: %s (normed on option)\n",
		   pstat->prefix,
		   get_percent(txt, sizeof(txt), p1_norm, inst_per_sec),
		   pstat->prefix2 ? pstat->prefix2 : "misses ",
		   get_percent(txt2, sizeof(txt2), p2_norm, p1_norm));
}

/**
 * Cnt im Verhaeltnis zu den Instructions
 */
static void relation_instruction_and_pair_swap(
	struct seq_file *seq,
	struct _yield_profiling *profile_entry,
	struct _roundrobin_perf_ctrl_tc_stat *pstat, unsigned long inst_per_sec)
{
	char txt[128], txt2[128];
	unsigned long p2_norm =
		norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long p1_norm =
		norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);

	seq_printf(seq,
		   "%-33s: %s (normed on instr) %s: %s (normed on option)\n",
		   pstat->prefix,
		   get_percent(txt, sizeof(txt), p1_norm, inst_per_sec),
		   pstat->prefix2 ? pstat->prefix2 : "misses ",
		   get_percent(txt2, sizeof(txt2), p2_norm, p1_norm));
}
#if 0
/**
 * Cnt im Verhaeltnis zu den Cycles
 */
static void relation_cycle_and_pair(struct seq_file *seq, struct _yield_profiling *profile_entry,
							struct _roundrobin_perf_ctrl_tc_stat *pstat,
							unsigned long inst_per_sec)
{
	char txt[128], txt2[128];
	unsigned long cycle_norm = norm_cycle_sec(profile_entry);
	unsigned long p1_norm = norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long p2_norm = norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);

	seq_printf(seq, "%-33s: %s (normed on cycle) %s: %s (normed on option)\n", pstat->prefix,
							get_percent(txt, sizeof(txt), p1_norm, cycle_norm),
							pstat->prefix2 ? pstat->prefix2 : "misses ",
							get_percent(txt2, sizeof(txt2), p2_norm, p1_norm)
			 );
}

/**
 * Cnt im Verhaeltnis zu den Cycles
 */
static void relation_cycle_and_pair_swap(struct seq_file *seq, struct _yield_profiling *profile_entry,
							struct _roundrobin_perf_ctrl_tc_stat *pstat,
							unsigned long inst_per_sec)
{
	char txt[128], txt2[128];
	unsigned long cycle_norm = norm_cycle_sec(profile_entry);
	unsigned long p2_norm = norm_per_sec(pstat->sum_perf_count[0], pstat->sum_perf_time[0]);
	unsigned long p1_norm = norm_per_sec(pstat->sum_perf_count[1], pstat->sum_perf_time[1]);

	seq_printf(seq, "%-33s: %s (normed on cycle) %s: %s (normed on option)\n", pstat->prefix,
							get_percent(txt, sizeof(txt), p1_norm, cycle_norm),
							pstat->prefix2 ? pstat->prefix2 : "misses ",
							get_percent(txt2, sizeof(txt2), p2_norm, p1_norm)
			 );
}
#endif

/**
 */
// clang-format off
static const struct _roundrobin_perf_ctrl_tc_stat performance_per_processor[] =  {
	{ .prefix = "Summary:",                                   .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(18), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(0), .proceed = instruction_per_cycle }, /*--- Stall Cycles, Cycles ---*/
#if defined(CONFIG_SOC_GRX500)
	{ .prefix = "L2 Cache Access",                             .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(22), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(21), .proceed = relation_instruction_and_pair_swap },  /*--- L2 Cache Misses , L2 Cache accesses ---*/
	{ .prefix = "L2 Cache WB",                                 .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(21), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(22), .proceed = param_instruction_relation },  /*--- L2 Cache WB, L2 Cache Single Bit Error Corrected ---*/
#endif/*--- #if defined(CONFIG_SOC_GRX500) ---*/
	{ .prefix = "Fetch-Unit-Stalls", .prefix2 = "ALU-Stalls", .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(25), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(25), .proceed = param_cycle_relation }, /*--- IFU stall cycles , ALU stall cycles---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(39), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(39), .proceed = NULL }, /*--- D$ miss cycles , L2 miss cycles---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(44), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(46), .proceed = NULL }, /*--- CACHE Instn stall cycles , Branch misspredict stall cycles ---*/
	/*--- { .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(50), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(50), .proceed = NULL },  ---*//*--- FSB < 1/4 , 1/4 < FSB > 1/2  ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(51), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(51), .proceed = NULL }, /*--- FSB > 1/2 , 1/4 < FSB full pipline stall cycles ---*/
	/*--- { .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(52), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(52), .proceed = NULL },  ---*//*--- LDQ < 1/4 , 1/4 < LDQ > 1/2  ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(53), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(53), .proceed = NULL }, /*--- LDQ > 1/2 , 1/4 < LDQ full pipline stall cycles ---*/
	/*--- { .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(54), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(54), .proceed = NULL },  ---*//*--- WBB < 1/4 , 1/4 < WBB > 1/2  ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(55), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(55), .proceed = NULL }, /*--- WBB > 1/2 , 1/4 < WBB full pipline stall cycles ---*/
	/*--- { .prefix = "", .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(56), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(56), .proceed = NULL },  ---*//*--- Intervention Hits, All Interventions ---*/
	/*--- { .prefix = "", .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(57), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(57), .proceed = NULL },  ---*//*--- All Invalidates, Invalidate Hits ---*/
	/*--- { .prefix = "", .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(58), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(58), .proceed = NULL },  ---*//*--- Evictions , Writebacks ---*/
};
// clang-format on
#if defined(CONFIG_SOC_GRX500)
#define MIP16_INSTRUCTION_STRING NULL
#else /*--- #if defined(CONFIG_SOC_GRX500) ---*/
#define MIP16_INSTRUCTION_STRING "MIPS16-Instruction"
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_SOC_GRX500) ---*/

/**
 */
// clang-format off
static const struct _roundrobin_perf_ctrl_tc_stat performance_per_tc[] =  {
	{ .prefix = "Branch Instruction",                            .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(2),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(2),  .proceed = relation_instruction_and_pair },       /*--- branch instructions, branch mispredictions ---*/
	{ .prefix = "Jump Return",                                   .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(3),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(3),  .proceed = relation_instruction_and_pair },       /*--- JR r31 (return) instructions, JR r31 mispredictions ---*/
	{ .prefix = "J/JAL",   .prefix2 = MIP16_INSTRUCTION_STRING,  .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(16), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(16), .proceed = param_instruction_relation },           /*--- J/JAL completed, MIP16 instructions completed ---*/
	/*--- { .prefix = "JR not ra",                                     .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(4),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(4),  .proceed = relation_instruction_and_pair},        ---*//*--- JR not r31  instructions, JR r31 not predicted ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(5),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(5),  .proceed = NULL },       /*--- ITLB accesses, ITLB misses ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(6),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(6),  .proceed = NULL },       /*--- DTLB accesses, DTLB misses ---*/
	{ .prefix = "Instruction-TLB accesses",                   .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(7),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(7),  .proceed = relation_instruction_and_pair },       /*--- JTLB instructions accesses, JTLB instructions misses ---*/
	{ .prefix = "Data-TLB accesses",                          .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(8),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(8),  .proceed = relation_instruction_and_pair },       /*--- JTLB data accesses, JTLB data misses ---*/
	{ .prefix = "Instruction cache accesses",                 .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(9),  .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(9),  .proceed = relation_instruction_and_pair },       /*--- Instn Cache accesses, Instn cache misses ---*/
	{ .prefix = "Data cache access",                          .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(10), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(11), .proceed = relation_instruction_and_pair },       /*--- Data Cache accesses, Data cache misses ---*/
	{ .prefix = "Store access",                               .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(13), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(15), .proceed = relation_instruction_and_pair_swap },  /*--- Store misses, Store completed ---*/
	{ .prefix = "Load access",                                .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(15), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(13), .proceed = relation_instruction_and_pair},        /*--- loads completed, load miss ---*/
	{ .prefix = "DSP Inst + Mul/Div-Instruction",             .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(26), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(17), .proceed = sum_instruction_relation }, /*--- integer instruction completed, integer/divide completed ---*/
	/*--- { .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(20), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(20), .proceed = NULL },  ---*//*--- Prefetch instruction completed, Prefetch instruction completed with cache hit ---*/
	{ .prefix = "Exceptions",  .prefix2 = "Data cache WB",    .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(23), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(10), .proceed = param_instruction_relation }, /*--- Exception taken, Data cache WB ---*/
	{ .prefix = "Uncached Load/Stores",                       .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(33), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(33), .proceed = sum_instruction_relation }, /*---  Uncached Load, Uncached Stored ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(37), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(37), .proceed = NULL }, /*--- I$ Miss stall cycles, D$ Miss stall cycles ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(38), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(38), .proceed = NULL }, /*--- SYNC stall cycles, FSB conflict stall cycles ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(40), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(40), .proceed = NULL }, /*--- Uncached stall cycles, ITC stall cycles ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(41), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(42), .proceed = NULL }, /*--- MDU stall cycles, CoreExtend stall cycles ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(43), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(43), .proceed = NULL }, /*--- ISPRAM stall cycles, DSPRAM stall cycles ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(45), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(44), .proceed = NULL }, /*--- Load to Use stall, Long stall cycle ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(48), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(45), .proceed = NULL }, /*--- IFU FB full refetches, ALU to AGEN stalls cycle ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(19), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(19), .proceed = NULL }, /*--- SC, failed SC ---*/
	{ .prefix = "",                                           .perf_ctrl[0] = MIPS_PERFORMANCE_EVENT(24), .perf_ctrl[1] = MIPS_PERFORMANCE_EVENT(23), .proceed = NULL },  /*--- Cache fixup, Single Thread Mode ---*/
};
// clang-format on

/**
 */
static void init_all_tc_stat(struct _yield_profiling *profile_entry)
{
	unsigned int tc;

	for (tc = 0; tc < PROFILING_MAX_COUNT_TCS; tc++) {
		if (tc == profile_entry->profiling_tc) {
			init_tc_stat(profile_entry->roundrobin_tc_stat[tc],
				     performance_per_processor,
				     min(ARRAY_SIZE(profile_entry->roundrobin_tc_stat[tc]), ARRAY_SIZE(performance_per_processor)),
				     tc, 1);
		} else {
			init_tc_stat(profile_entry->roundrobin_tc_stat[tc],
				     performance_per_tc,
				     min(ARRAY_SIZE(profile_entry->roundrobin_tc_stat[tc]), ARRAY_SIZE(performance_per_tc)),
				     tc, 0);
		}
	}
}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
#if defined(PROFILING_IN_YIELD)

/**
 * Cleanup der Ringliste
 */
static void
clean_tc_stat(struct _roundrobin_perf_ctrl_tc_stat *roundrobin_tc_stat)
{
	struct _roundrobin_perf_ctrl_tc_stat *table = roundrobin_tc_stat;
	unsigned int reg;

	while (table) {
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			table->sum_perf_count[reg] = 0;
			table->sum_perf_time[reg] = 0;
		}
		table = table->next;
		if (table == roundrobin_tc_stat) {
			break;
		}
	}
}

/**
 * setze neue Ctl's-Modes
 */
static void
set_tc_perf_mod(struct _roundrobin_perf_ctrl_tc_stat *roundrobin_tc_stat,
		unsigned int mode)
{
	struct _roundrobin_perf_ctrl_tc_stat *table = roundrobin_tc_stat;
	unsigned int reg;

	while (table) {
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			table->perf_ctrl[reg] &= ~MIPS_PERFORMANCE_MODE_MASK;
			table->perf_ctrl[reg] |= (mode & MIPS_PERFORMANCE_MODE_MASK);
		}
		table = table->next;
		if (table == roundrobin_tc_stat) {
			break;
		}
	}
}
#else /*--- #if defined(PROFILING_IN_YIELD) ---*/
#define clean_tc_stat(a)
#define set_tc_perf_mod(a, b)
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_YIELD) ---*/

#if defined(PROFILING_IN_YIELD)
/*
 * Summiert die letzte Messung des aktuellen Performance-Counters und setzt auf neuen
 * Counter
 * return: Zeiger auf naechsten Eintrag
 */
static struct _roundrobin_perf_ctrl_tc_stat *
set_tc_stat(struct _roundrobin_perf_ctrl_tc_stat *entry,
	    unsigned int perf_cnt[], unsigned int last_perf_count[],
	    unsigned int meassure_time, int *ret)
{
	struct _roundrobin_perf_ctrl_tc_stat *next = entry->next;
	unsigned int reg;

	for (reg = 0; reg < ARRAY_SIZE(entry->sum_perf_count); reg++) {
		unsigned int diff = (perf_cnt[reg] - last_perf_count[reg]);

		if (diff < (1U << 31)) {
			entry->sum_perf_count[reg] += (unsigned long long)(diff);
			entry->sum_perf_time[reg]  += (unsigned long long)(meassure_time);
		} else {
			*ret = -ERANGE;
		}
		if (next) {
			switch (reg) {
			case 0:
				write_tc_c0_perfctrl0(next->perf_ctrl[0]);
				break;
			case 1:
				write_tc_c0_perfctrl1(next->perf_ctrl[1]);
				break;
			default:
				*ret = -EINVAL;
			}
		}
	}
	return next;
}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/

#if defined(PROFILING_IN_YIELD)

/**
 * Simple Messung mit "festen" Performance-Countern
 */
static void init_simple_performance_count_tc_stat(
	struct _simple_performance_count_tc_stat *ptcstat, unsigned int ctrl,
	unsigned int cnt, unsigned int reg)
{
	ptcstat->sum_perf_count[reg] = 0;
	ptcstat->last_perf_count[reg] = cnt;
	ptcstat->perf_ctrl[reg] = ctrl;

	ptcstat->sum_tc_sched = 0;
	ptcstat->last_tc_sched = read_tc_c0_tcschefback();
}
#else /*--- #if defined(PROFILING_IN_YIELD) ---*/
#define init_simple_performance_count_tc_stat(a, ctrl, cnt, reg)
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_YIELD) ---*/
#if defined(PROFILING_IN_YIELD)

/**
 * Simple Messung mit "festen" Performance-Countern
 */
static void set_simple_performance_count_tc_perfcnt(
	int tc, struct _simple_performance_count_tc_stat tcstat[],
	unsigned int perf_cnt[], int *err)
{
	struct _simple_performance_count_tc_stat *ptcstat = &tcstat[tc];
	unsigned int i;
	unsigned int diff;

	for (i = 0; i < ARRAY_SIZE(ptcstat->sum_perf_count); i++) {
		diff = perf_cnt[i] - ptcstat->last_perf_count[i];
		if (diff < (1U << 31)) {
			ptcstat->sum_perf_count[i] +=
				(unsigned long long)(diff);
		} else {
			DBG_PERF("[simple-profiling]%s: tc=%u perf%u ovr %u > %u: ignore\n",
				 __func__, tc, i, ptcstat->last_perf_count[i],
				 perf_cnt[i]);
			if (*err == 0)
				*err = -ERANGE;
		}
		ptcstat->last_perf_count[i] = perf_cnt[i];
	}
}

/**
 */
static void set_simple_performance_count_tc_sched(
	int tc, struct _simple_performance_count_tc_stat tcstat[],
	unsigned int tc_sched, int *err, int reset)
{
	struct _simple_performance_count_tc_stat *ptcstat = &tcstat[tc];
	unsigned int diff;

	diff = tc_sched - ptcstat->last_tc_sched;
	if (diff < (1U << 31)) {
		ptcstat->sum_tc_sched += diff;
	} else {
		DBG_PERF("[simple-profiling]%s: tc=%u tcsched ovr %u > %u: ignore\n",
			__func__, tc, ptcstat->last_tc_sched, tc_sched);
		if (*err == 0)
			*err = -ERANGE;
	}
	ptcstat->last_tc_sched = reset ? 1 : tc_sched;
}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/

/**
 */
static char *print_performance_counter_mode(char *str, int str_len,
					    unsigned int mode)
{
	char txt[32];

	if (mode & MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE) {
		sprintf(txt, "TC-ID=%u", GET_MIPS_PERFORMANCE_TCID(mode));
	} else if (mode & MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE) {
		sprintf(txt, "VPE-ID=%u", GET_MIPS_PERFORMANCE_VPEID(mode));
	} else {
		txt[0] = 0;
	}
	snprintf(str, str_len, "%s%s%s%s%s",
		 (MIPS_PERFORMANCE_USER_MODE_ENABLE & mode) ? "User " : "",
		 (MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE & mode) ?
			 "Supervisor " :
			 "",
		 (MIPS_PERFORMANCE_KERNEL_MODE_ENABLE & mode) ? "Kernel " : "",
		 (MIPS_PERFORMANCE_EXCEPTION_ENABLE & mode) ? "Exception " : "",
		 txt);
	return str;
}
/*
 * liefert Anzahl der Performance-Counter
 * bei InterAptiv/34K inlusive tcschedFeedBack
 */
static unsigned int mips_get_performance_counter_nr(void)
{
	unsigned int perf_max_registers = 0;
	unsigned int val;

	val = read_c0_config1();
	/*--- pr_err("%s:  config1: %08x\n", __func__, val); ---*/
	if ((val & (1 << 4)) == 0) {
		return perf_max_registers;
	}
	for (;;) {
		perf_max_registers++;
		switch (perf_max_registers) {
		case 1:
			val = read_c0_perfctrl0();
			break;
		case 2:
			val = read_c0_perfctrl1();
			break;
		case 3:
			val = read_c0_perfctrl2();
			break;
		case 4:
			val = read_c0_perfctrl3();
			break;
		default:
			val = 0;
		}
		/*--- pr_err("%s:  perfctl%u: %08x\n", __func__, perf_max_registers - 1, val); ---*/
		if (!(val & MIPS_PERFORMANCE_HAS_MORE)) {
			break;
		}
	}
#if defined(PROFILING_CPU_HAS_TC)
	perf_max_registers += 1; /*--- tcschedFeedBack hinzu ---*/
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	return perf_max_registers;
}

/**
 */
static unsigned int supported_tcs(void)
{
#if defined(PROFILING_CPU_HAS_TC)
	unsigned int mvpconf0;

	mvpconf0 = read_c0_mvpconf0();
	return ((mvpconf0 & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
#else /*--- #if defined(CONFIG_CPU_MIPS_34K) ---*/
	return 1;
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_CPU_MIPS_34K) ---*/
}

#define vpflags flags[0]
#define sys_flag flags[1]
#define old_tc flags[2]
#define haltval flags[3]
/*
 * Achtung ! Innerhalb von set_tcmode() ... restore_tcmode() keine Funktionen, die
 * spin_lock etc. verwenden aufrufen (.z.B. printk - Deadlock-Gefahr)
 */
static inline unsigned int set_tcmode(unsigned int tc, unsigned long flags[])
{
#if defined(PROFILING_CPU_HAS_TC)
	spin_lock_irqsave(&perfcnt_lock, sys_flag);
	vpflags = dvpe();
	old_tc = read_c0_vpecontrol() & VPECONTROL_TARGTC;
	settc(tc);
	if (!(read_tc_c0_tcstatus() & TCSTATUS_A)) {
		settc(old_tc);
		evpe(vpflags);
		spin_unlock_irqrestore(&perfcnt_lock, sys_flag);
		return 1;
	}
	if (read_tc_c0_tcbind() == (unsigned int)read_c0_tcbind()) {
		/* Are we dumping ourself?  */
		haltval = 0; /* Then we're not halted, and mustn't be */
	} else {
		haltval = read_tc_c0_tchalt();
		write_tc_c0_tchalt(1);
	}
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	return 0;
}

/**
 */
static inline void restore_tcmode(unsigned long flags[])
{
#if defined(PROFILING_CPU_HAS_TC)
	if (!haltval) {
		write_tc_c0_tchalt(0);
	}
	settc(old_tc);
	evpe(vpflags);
	spin_unlock_irqrestore(&perfcnt_lock, sys_flag);
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
}
#undef vpflags
#undef sys_flag
#undef old_tc
#undef haltval

/**
 */
static unsigned int read_c0_perfctl(unsigned int ctl_reg, unsigned int tc,
				    unsigned int *valid)
{
	unsigned long flags[4] = { 0, 0, 0, 0 };
	unsigned int res = 0;

	if (tc < PROFILING_MAX_COUNT_TCS) {
		if (set_tcmode(tc, flags)) {
			*valid = 0;
			return res;
		}
	}
	*valid = 1;
	switch (ctl_reg) {
#if defined(PROFILING_CPU_HAS_TC)
	case 0:
		res = read_tc_c0_perfctrl0();
		break;
	case 1:
		res = read_tc_c0_perfctrl1();
		break;
	case 2:
		res = MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE |
		      MIPS_PERFORMANCE_TCID(tc) | MIPS_PERFORMANCE_EVENT(1);
		break; /*--- dummy: TCSchedFeedBack liefert Instruction completed per TC ---*/
#else /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	case 0:
		res = read_c0_perfctrl0();
		break;
	case 1:
		res = read_c0_perfctrl1();
		break;
	case 2:
		res = read_c0_perfctrl2();
		break;
	case 3:
		res = read_c0_perfctrl3();
		break;
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	default:
		*valid = 0;
	}
	if (tc < PROFILING_MAX_COUNT_TCS) {
		restore_tcmode(flags);
	}
	/*--- pr_info("%s: tc=%u reg=%x val=%#08x\n", __func__, tc, ctl_reg, res); ---*/
	return res;
}

/**
 */
static void write_c0_perfctl(unsigned int ctl_reg, unsigned int tc,
			     unsigned int val)
{
	unsigned long flags[4];
	unsigned int dummy __maybe_unused;

	if (set_tcmode(tc, flags)) {
		return;
	}
	/*--- if(val & MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE) { ---*/
	/*---    val &= ~MIPS_PERFORMANCE_TCID(0xFF); ---*/
	/*---    val |= MIPS_PERFORMANCE_TCID(tc); ---*/
	/*--- } ---*/
	switch (ctl_reg) {
#if defined(PROFILING_CPU_HAS_TC)
	case 0:
		write_tc_c0_perfctrl0(val);
		break;
	case 1:
		write_tc_c0_perfctrl1(val);
		break;
#else /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	case 0:
		write_c0_perfctrl0(val);
		break;
	case 1:
		write_c0_perfctrl1(val);
		break;
	case 2:
		write_c0_perfctrl2(val);
		break;
	case 3:
		write_c0_perfctrl3(val);
		break;
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	}
	restore_tcmode(flags);
	DBG_PERF("%s: tc=%u ctl_reg: %x 0x%08x -> val=0x%08x epc=%pS\n",
		 __func__, tc, ctl_reg,
		 read_c0_perfctl(ctl_reg, PROFILING_MAX_COUNT_TCS, &dummy), val,
		 (void *)read_tc_c0_tcrestart());
}

/**
 */
static void write_c0_perfcnt(unsigned int count_reg, unsigned int tc,
			     unsigned int val)
{
	unsigned long flags[4];

	if (set_tcmode(tc, flags)) {
		return;
	}
	switch (count_reg) {
#if defined(PROFILING_CPU_HAS_TC)
	case 0:
		write_tc_c0_perfcntr0(val);
		break;
	case 1:
		write_tc_c0_perfcntr1(val);
		break;
	case 2:
		write_tc_c0_tcschefback(val);
		break; /*--- wird als perfcnt2 missbraucht ---*/
#else /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	case 0:
		write_c0_perfcntr0(val);
		break;
	case 1:
		write_c0_perfcntr1(val);
		break;
	case 2:
		write_c0_perfcntr2(val);
		break;
	case 3:
		write_c0_perfcntr3(val);
		break;
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	}
	restore_tcmode(flags);
	DBG_PERF("%s: tc=%u count_reg: %x val=0x%x\n", __func__, tc, count_reg,
		 val);
}

/**
 */
static unsigned int read_c0_perfcnt(unsigned int count_reg, unsigned int tc)
{
	unsigned long flags[4] = { 0, 0, 0, 0 };
	unsigned int res = 0;

	if (tc < PROFILING_MAX_COUNT_TCS) {
		if (set_tcmode(tc, flags)) {
			return res;
		}
	}
	switch (count_reg) {
#if defined(PROFILING_CPU_HAS_TC)
	case 0:
		res = read_tc_c0_perfcntr0();
		break;
	case 1:
		res = read_tc_c0_perfcntr1();
		break;
	case 2:
		res = read_tc_c0_tcschefback();
		break; /*--- wird als perfcnt2 missbraucht ---*/
#else /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	case 0:
		res = read_c0_perfcntr0();
		break;
	case 1:
		res = read_c0_perfcntr1();
		break;
	case 2:
		res = read_c0_perfcntr2();
		break;
	case 3:
		res = read_c0_perfcntr3();
		break;
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	}
	if (tc < PROFILING_MAX_COUNT_TCS) {
		restore_tcmode(flags);
	}
	DBG_PERF("%s: tc=%u count_reg: %x val=0x%x\n", __func__, tc, count_reg,
		 res);
	return res;
}

struct _perfctl_param_read {
	unsigned int counting_tc_mask;
	unsigned int counting_reg_mask;
	char *str_buf; /* optional: write out in str_buf */
	int str_len;
};

struct _perfctl_param_write {
	/* wenn gesetzt, wird auch die jeweilige Statisik-Struktur
	 * performance_count_tc_stat[] geupdatet:
	 */
	struct _yield_profiling *yield_profile;
	unsigned int counting_tc_mask;
	unsigned int counting_reg_mask;
	enum {
		perf_set_ctl = 0x1, /* setze komplettes Ctrl-Register */
		perf_set_cnt = 0x2, /* setze Count-Register */
		/* folgende funktionieren nur, wenn perf_set_ctl == 0:  */
		perf_set_mode =
			0x4, /* modifiziere nur Ctrl-Register (Mode Enable) */
	} set_val;
	unsigned int preset_cnt;
	unsigned int preset_ctl;
};
/**
 * Lese perf_ctrl und perf_cnt
 * Welche Register mit Option param->counting_reg_mask
 * welche TC's mit Option param->counting_tc_mask
 * Ausgabe wahlweise per printk oder per param->str_buf
 */
static void read_perfcounter_per_core(struct _perfctl_param_read *param)
{
	char txt[128];
	unsigned int tc, reg_cnt;
	unsigned int ntc = supported_tcs();
	unsigned int max_perf, valid;
	char *str_buf = param->str_buf;
	int str_len = param->str_len;

	max_perf = mips_get_performance_counter_nr();

	for (tc = 0; tc < ntc; tc++) {
		if (((1 << tc) & param->counting_tc_mask) == 0) {
			continue;
		}
		for (reg_cnt = 0; reg_cnt < max_perf; reg_cnt++) {
			unsigned int cnt, ctl;

			if (((1 << reg_cnt) & param->counting_reg_mask) == 0) {
				continue;
			}
			cnt = read_c0_perfcnt(reg_cnt, tc);
			ctl = read_c0_perfctl(reg_cnt, tc, &valid);
			if (!valid) {
				continue;
			}
			if (str_buf == NULL) {
				pr_info("[TC=%u] perf%u: cnt=0x%08x ctl=0x%08x \"%s\" (%s)\n",
					tc, reg_cnt, cnt, ctl,
					performance_counter_options(
						reg_cnt,
						GET_MIPS_PERFORMANCE_EVENT(ctl)),
					print_performance_counter_mode(
						txt, sizeof(txt), ctl));
			} else {
				if (str_len > 0) {
					int len = snprintf(
						str_buf, str_len,
						"[TC=%u] perf%u: cnt=0x%08x ctl=0x%08x \"%s\" (%s)\n",
						tc, reg_cnt, cnt, ctl,
						performance_counter_options(
							reg_cnt,
							GET_MIPS_PERFORMANCE_EVENT(
								ctl)),
						print_performance_counter_mode(
							txt, sizeof(txt), ctl));
					len = min(len, str_len);
					str_buf += len;
					str_len -= len;
				}
			}
		}
	}
	if (str_buf) {
		param->str_len = str_buf - param->str_buf;
	} else {
		param->str_len = 0;
	}
}
/*
 * Setzt perf_ctrl und/oder perf_cnt (Option param->set_val)
 * Welche Register mit Option param->counting_reg_mask
 * Welche TC's mit Option param->counting_tc_mask
 * Wenn auch tc-perf-Statistik initialisiert werden soll: param->yield_profile gesetzt
 */
static void setup_perfcounter_per_core(struct _perfctl_param_write *param)
{
	struct _simple_performance_count_tc_stat *simple_tcstat = NULL;
	struct _roundrobin_perf_ctrl_tc_stat **act_rr_tcstat = NULL;
	struct _yield_profiling *yield_profile;
	unsigned int valid __maybe_unused;
	unsigned int tc, ntc, reg, ctl;
	unsigned long cnt, tc_mask;
	size_t nbits;

	yield_profile = param->yield_profile;
	ctl = param->preset_ctl;
	cnt = param->counting_reg_mask;
	tc_mask = param->counting_tc_mask;
	ntc = supported_tcs();

	if (yield_profile) {
		act_rr_tcstat = yield_profile->act_roundrobin_tcstat;
		simple_tcstat = yield_profile->simple_tcstat;
	}

	nbits = BITS_PER_BYTE * sizeof(param->counting_reg_mask);
	for_each_set_bit(reg, &cnt, nbits) {
		if (param->set_val & (perf_set_ctl | perf_set_mode)) {
			for_each_set_bit(tc, &tc_mask, ntc) {
				if ((param->set_val & perf_set_ctl) == 0) {
					/*--- nur den Mode setzen: ---*/
					ctl = read_c0_perfctl(reg, tc, &valid);
					ctl &= ~MIPS_PERFORMANCE_MODE_MASK;
					ctl |= (param->preset_ctl &
						MIPS_PERFORMANCE_MODE_MASK);
					if (act_rr_tcstat) {
						set_tc_perf_mod(
							act_rr_tcstat[tc],
							param->preset_ctl);
					}
				}
				write_c0_perfctl(reg, tc, ctl);
			}
		}
		if (param->set_val & perf_set_cnt) {
			for_each_set_bit(tc, &tc_mask, ntc) {
				write_c0_perfcnt(reg, tc, param->preset_cnt);
				if (act_rr_tcstat) {
					ctl = read_c0_perfctl(reg, tc, &valid);
					init_simple_performance_count_tc_stat(
						&simple_tcstat[tc],
						ctl,
						param->preset_cnt,
						reg);
					clean_tc_stat(act_rr_tcstat[tc]);
#if 0
					pr_info("%s: core=%u tc=%u ctrl[%x] %x\n",
							__func__,
							yield_profile->core,
							tc, reg,
							yield_profile->roundrobin_tc_stat[tc].perf_ctrl[reg]);
#endif
				}
			}
		}
	}
}

/**
 */
static void print_performance_options(unsigned int perf_reg)
{
	char txt[32];
	unsigned int max_perf, i;

	max_perf = mips_get_performance_counter_nr();
	if (max_perf == 0) {
		return;
	}
	for (i = 0; i < ARRAY_SIZE(_performance_counter_options[0]); i++) {
		if (performance_counter_options(perf_reg, i) == NULL) {
			continue;
		}
		if (max_perf == 4) {
			snprintf(txt, sizeof(txt), "%u/%u", perf_reg & 1,
				 (perf_reg & 1) + 2);
		} else {
			snprintf(txt, sizeof(txt), "%u", perf_reg);
		}
		pr_info("\tctrl%s:[%2d]: %s\n", txt, i,
			performance_counter_options(perf_reg, i));
	}
}

/**
 */
static int print_all_performance_options_cb(unsigned int param1,
					    unsigned int param2)
{
	print_performance_options(0);
	print_performance_options(1);
	return 1;
}

/**
 */
static int print_config_cb(unsigned int core, unsigned int param2)
{
	struct _perfctl_param_read perfctl_param;

	perfctl_param.counting_tc_mask = (1 << PROFILING_MAX_COUNT_TCS) - 1;
	perfctl_param.counting_reg_mask =
		(1 << mips_get_performance_counter_nr()) - 1;
	perfctl_param.str_buf = NULL;
	perfctl_param.str_len = 0;
#if defined(PROFILING_IN_YIELD)
	if (core >= PROFILING_CORES) {
		for (core = 0; core < PROFILING_CORES; core++) {
			smp_call_function_single(
				gYieldProfiler[core].linux_os_cpu,
				(smp_call_func_t)read_perfcounter_per_core,
				&perfctl_param, true);
		}
	} else {
		smp_call_function_single(
			gYieldProfiler[core].linux_os_cpu,
			(smp_call_func_t)read_perfcounter_per_core,
			&perfctl_param, true);
	}
#else /*--- #if defined(PROFILING_IN_YIELD) ---*/
	read_perfcounter_per_core(&perfctl_param);
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_YIELD) ---*/

	return 1;
}

/**
 * ret: len of str
 */
int mips_get_performance_counter_mode(char *str, int str_len, unsigned int nr)
{
	int len = 0;
	struct _perfctl_param_read perfctl_param;
	unsigned int core __maybe_unused;

	/*--- pr_err("%s: init-len: %u\n", __func__, str_len); ---*/
	perfctl_param.counting_tc_mask = (1 << PROFILING_MAX_COUNT_TCS) - 1;
	perfctl_param.counting_reg_mask = 0x1 << nr;

#if defined(PROFILING_IN_YIELD)
	for (core = 0; core < PROFILING_CORES; core++) {
		int mlen = snprintf(str, str_len, "CORE%u:\n", core);

		mlen = min(str_len, mlen);
		str_len -= mlen, str += mlen, len += mlen;

		perfctl_param.str_len = str_len;
		perfctl_param.str_buf = str;
		smp_call_function_single(
			gYieldProfiler[core].linux_os_cpu,
			(smp_call_func_t)read_perfcounter_per_core,
			&perfctl_param, true);
		/*--- pr_err("%s: smp-retlen: %u\n", __func__, perfctl_param.str_len); ---*/
		str_len -= perfctl_param.str_len, str += perfctl_param.str_len,
			len += perfctl_param.str_len;
	}
#else /*--- #if defined(PROFILING_IN_YIELD) ---*/
	perfctl_param.str_len = str_len;
	perfctl_param.str_buf = str;
	read_perfcounter_per_core(&perfctl_param);
	len = perfctl_param.str_len;
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_YIELD) ---*/
	/*--- pr_err("%s: len: %u\n", __func__, len); ---*/
	return len;
}

/**
 */
static int check_setting_param_cb(unsigned int perf_reg, unsigned int option)
{
	unsigned int perf_max_registers = mips_get_performance_counter_nr();

	if (perf_reg >= perf_max_registers) {
		pr_err("error: invalid param1 on set %u <option> (max %u)\n",
		       perf_reg, perf_max_registers);
		return 1;
	}
	if ((option >= ARRAY_SIZE(_performance_counter_options[0])) ||
	    (performance_counter_options(perf_reg, option) == NULL)) {
		pr_err("error: invalid param2 on set %u <%u>\n", perf_reg,
		       option);
		print_performance_options(perf_reg & 0x1);
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
		CORE_IDX = 0,
		SET_IDX,
		TC_IDX,
		TRACE_ALL_TC_IDX,
		TRACE_ALL_PROCESSOR_IDX,
		START_IDX,
		STOP_IDX,
		USER_ONLY_IDX,
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
	  .help = "<core>",
	  .type = one_param,
	  .field_idx = MAX_IDX },
	{ .name = "mode",
	  .func = NULL,
	  .help = "<mode> 0: all 1: only user 2: only kernel+exec+sv",
	  .type = one_param,
	  .field_idx = USER_ONLY_IDX },
#if defined(PROFILING_CPU_HAS_TC)
	{ .name = "stop",
	  .func = NULL,
	  .help = "stop",
	  .type = no_param,
	  .field_idx = STOP_IDX },
	{ .name = "start",
	  .func = NULL,
	  .help = "start (all TC,all Core)",
	  .type = no_param,
	  .field_idx = START_IDX },
	{ .name = "further params for start:",
	  .func = NULL,
	  .help = "(optional)",
	  .type = no_param,
	  .field_idx = MAX_IDX },
	{ .name = "perform_tc",
	  .func = NULL,
	  .help = "<enable> trace all/no tc",
	  .type = one_param,
	  .field_idx = TRACE_ALL_TC_IDX },
	{ .name = "perform_cpu",
	  .func = NULL,
	  .help = "<enable> trace all/no processor",
	  .type = one_param,
	  .field_idx = TRACE_ALL_PROCESSOR_IDX },
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	{ .name = "set",
	  .func = check_setting_param_cb,
	  .help = "<perf-reg> <perf-option> ...",
	  .type = two_param,
	  .field_idx = SET_IDX },
	{ .name = "further params for set:",
	  .func = NULL,
	  .help = "(optional)",
	  .type = no_param,
	  .field_idx = MAX_IDX },
#if defined(PROFILING_IN_YIELD) && (PROFILING_CORES > 1)
	{ .name = "core",
	  .func = NULL,
	  .help = "<corenr>",
	  .type = one_param,
	  .field_idx = CORE_IDX },
#endif /*--- #if defined(PROFILING_IN_YIELD) && (PROFILING_CORES > 1) ---*/
#if defined(PROFILING_CPU_HAS_TC)
	{ .name = "per_tc",
	  .func = NULL,
	  .help = "<tc_id> perfcounter only for tc-id",
	  .type = one_param,
	  .field_idx = TC_IDX },
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	{ .name = NULL },
};

#if defined(PROFILING_CPU_HAS_TC)

/**
 * den RoundRobin Performance-Count-Mode aktivieren/deaktivieren
 */
static void set_performance_trace_mode(struct _yield_profiling *yield_profile,
				       int tc_trace, int processore_trace)
{
	unsigned int tc;

	for (tc = 0; tc < PROFILING_MAX_COUNT_TCS; tc++) {
		if (tc == yield_profile->profiling_tc) {
			if (processore_trace == 0) {
				yield_profile->act_roundrobin_tcstat[tc] = NULL;
			} else if (processore_trace != -1) {
				yield_profile->act_roundrobin_tcstat[tc] = yield_profile->roundrobin_tc_stat[tc];
			}
			clean_tc_stat(yield_profile->act_roundrobin_tcstat[tc]);
			continue;
		}
		if (tc_trace == 0) {
			yield_profile->act_roundrobin_tcstat[tc] = NULL;
		} else if (tc_trace != -1) {
			yield_profile->act_roundrobin_tcstat[tc] = yield_profile->roundrobin_tc_stat[tc];
		}
		clean_tc_stat(yield_profile->act_roundrobin_tcstat[tc]);
	}
}

/**
 */
static char *build_tc_coreinfo_string(char *txt, size_t size, char *prefix,
				      unsigned int val, unsigned int max)
{
	if (size)
		txt[0] = 0;
#if defined(PROFILING_CPU_HAS_TC)
	if (val >= max) {
		snprintf(txt, size, "all %ss", prefix);
	} else {
		snprintf(txt, size, "%s: %u", prefix, val);
	}
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	return txt;
}
#define build_tc_string(txt, val)                                              \
	build_tc_coreinfo_string(txt, sizeof(txt), "tc", val,                  \
				 PROFILING_MAX_COUNT_TCS)
#define build_core_string(txt, val)                                            \
	build_tc_coreinfo_string(txt, sizeof(txt), "core", val, PROFILING_CORES)
/**
 */
static void print_performance_trace_mode_by_core(unsigned int core,
						 int tc_trace,
						 int processor_trace)
{
	char txt[32], tctxt[32], processortxt[32];

	/*--- pr_err("%s: core=%u %d %d\n", __func__, core, tc_trace, processor_trace); ---*/
	if (tc_trace >= 0) {
		sprintf(tctxt, "tc's %s", tc_trace == 0 ? "off" : "on");
	} else {
		tctxt[0] = 0;
	}
	if (processor_trace >= 0) {
		sprintf(processortxt, "cpus %s",
			processor_trace == 0 ? "off" : "on");
	} else {
		processortxt[0] = 0;
	}
	if (processortxt[0] || tctxt[0]) {
		pr_err("[simple-profiling]%s:performance-mode (round-robin): %s %s\n",
		       build_core_string(txt, core), tctxt, processortxt);
	}
}

/**
 */
static void set_performance_trace_mode_by_core(unsigned int core, int tc_trace,
					       int processor_trace)
{
	if (core >= PROFILING_CORES) {
		for (core = 0; core < PROFILING_CORES; core++) {
			set_performance_trace_mode(&gYieldProfiler[core],
						   tc_trace, processor_trace);
		}
		return;
	}
	set_performance_trace_mode(&gYieldProfiler[core], tc_trace,
				   processor_trace);
}
#else /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
static inline char *build_tc_string(char *txt, size_t size)
{
	txt[0] = 0;
	return txt;
}
static inline char *build_core_string(char *txt, size_t size)
{
	txt[0] = 0;
	return txt;
}
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
#define print_out(seq, args...)                                                \
	{                                                                      \
		if (seq)                                                       \
			seq_printf(seq, args);                                 \
		else                                                           \
			pr_err(args);                                          \
	}
/**
 */
void mips_performance_counter_help(struct seq_file *seq)
{
	const struct _perf_parse_helper *pph = &perf_parse_helper[0];
	unsigned int perf_count;

	perf_count = mips_get_performance_counter_nr();
	print_out(seq,
		  "\nuse 'cat /proc/avm/profile/perform' to read performance-summary of last profiling\n");
	print_out(seq,
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
				  unsigned int cores)
{
#if defined(PROFILING_IN_YIELD)
	if (cores >= PROFILING_CORES) {
		unsigned int core;

		for (core = 0; core < PROFILING_CORES; core++) {
			perfctl_param->yield_profile = &gYieldProfiler[core];
			if (perfctl_param->preset_ctl ==
			    MIPS_PERFORMANCE_USER_MODE_ENABLE) {
				/*--- nicht den Monitor-TC: im Usermode zaehlt er keine Prozessor-Werte  ---*/
				perfctl_param->counting_tc_mask &= ~(1 << gYieldProfiler[core].profiling_tc);
			}
			smp_call_function_single(perfctl_param->yield_profile->linux_os_cpu,
						 (smp_call_func_t)setup_perfcounter_per_core,
						 perfctl_param, true);
		}
	} else {
		perfctl_param->yield_profile = &gYieldProfiler[cores];
		if (perfctl_param->preset_ctl == MIPS_PERFORMANCE_USER_MODE_ENABLE) {
			/*--- nicht den Monitor-TC: im Usermode zaehlt er keine Prozessor-Werte  ---*/
			perfctl_param->counting_tc_mask &= ~(1 << gYieldProfiler[cores].profiling_tc);
		}
		smp_call_function_single(perfctl_param->yield_profile->linux_os_cpu,
					 (smp_call_func_t)setup_perfcounter_per_core,
					 perfctl_param, true);
	}
#else /*--- #if defined(PROFILING_IN_YIELD) ---*/
	setup_perfcounter_per_core(perfctl_param);
#endif /*--- #else ---*/ /*--- #if defined(PROFILING_IN_YIELD) ---*/
}

/**
 * echo ... >/proc/avm/profile/perform
 */
static void mips_performance_counter_action(char *p)
{
	char tctxt[64];
	char coretxt[64];
	struct _perfctl_param_write perfctl_param;
	char txt[128];
	unsigned int perf_reg_flags, perf_max_registers, mode;
	const struct _perf_parse_helper *pph = &perf_parse_helper[0];
	unsigned int field[MAX_IDX][2];
	int ret = 0;

	memset(field, 0xFF, sizeof(field));
	memset(&perfctl_param, 0x0, sizeof(perfctl_param));
	/*
	 *  NEW_34K: the core has two performance counters, replicated per-TC
	 *  OLD_34K: had four performance counters, not replicated
	 */
	perf_max_registers = mips_get_performance_counter_nr();
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
#if defined(PROFILING_CPU_HAS_TC)
	if (field[STOP_IDX][0] == 1) {
		mips_profiling_special_enable(sp_enable_off, 0);
		return;
	}
	if (field[START_IDX][0] == 1) {
		if (field[TRACE_ALL_TC_IDX][0] == (unsigned int)-1)
			field[TRACE_ALL_TC_IDX][0] =
				1; /*--- TC Performance (Round-Robin)---*/
		if (field[TRACE_ALL_PROCESSOR_IDX][0] == (unsigned int)-1)
			field[TRACE_ALL_PROCESSOR_IDX][0] =
				1; /*--- CPU Performance (Round-Robin) ---*/
	}
	print_performance_trace_mode_by_core(field[CORE_IDX][0],
					     field[TRACE_ALL_TC_IDX][0],
					     field[TRACE_ALL_PROCESSOR_IDX][0]);
	set_performance_trace_mode_by_core(field[CORE_IDX][0],
					   field[TRACE_ALL_TC_IDX][0],
					   field[TRACE_ALL_PROCESSOR_IDX][0]);
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	perf_reg_flags = MIPS_PERFORMANCE_USER_MODE_ENABLE |
			 MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE |
			 MIPS_PERFORMANCE_KERNEL_MODE_ENABLE |
			 MIPS_PERFORMANCE_EXCEPTION_ENABLE | 0;

	perfctl_param.counting_tc_mask = (1 << PROFILING_MAX_COUNT_TCS) - 1;
#if defined(PROFILING_CPU_HAS_TC)
	if (field[TC_IDX][0] < PROFILING_MAX_COUNT_TCS) {
		perf_reg_flags |= MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE |
				  MIPS_PERFORMANCE_TCID(field[TC_IDX][0]);
		perfctl_param.counting_tc_mask = 1 << field[TC_IDX][0];
	}
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	mode = field[USER_ONLY_IDX][0];
	if (mode <= 2) {
		perfctl_param.counting_reg_mask =
			(1 << mips_get_performance_counter_nr()) - 1;

		switch (mode) {
		default:
		case 0: /*--- all ---*/
			perfctl_param.preset_ctl =
				MIPS_PERFORMANCE_USER_MODE_ENABLE |
				MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE |
				MIPS_PERFORMANCE_KERNEL_MODE_ENABLE |
				MIPS_PERFORMANCE_EXCEPTION_ENABLE;
			break;
		case 1: /*--- user-mode ---*/
			perfctl_param.preset_ctl =
				MIPS_PERFORMANCE_USER_MODE_ENABLE;
			break;
		case 2: /*--- kernel/sv/exec-mode ---*/
			perfctl_param.preset_ctl =
				MIPS_PERFORMANCE_SUPERVISOR_MODE_ENABLE |
				MIPS_PERFORMANCE_KERNEL_MODE_ENABLE |
				MIPS_PERFORMANCE_EXCEPTION_ENABLE;
			break;
		}

		pr_info("[simple-profiling]Modify Performance Counter-Mode to %s for %s %s\n",
			print_performance_counter_mode(txt, sizeof(txt), perfctl_param.preset_ctl),
						       build_core_string(coretxt, field[CORE_IDX][0]),
						       build_tc_string(tctxt, field[TC_IDX][0]));
		perfctl_param.set_val = perf_set_mode;
		setup_all_perfcounter(&perfctl_param, field[CORE_IDX][0]);
		return;
	}
	if ((field[SET_IDX][0] < perf_max_registers) &&
	    (field[SET_IDX][1] < ARRAY_SIZE(_performance_counter_options[0]))) {
		unsigned int perf_reg_number = field[SET_IDX][0];
		unsigned int perf_reg_index = field[SET_IDX][1];

		pr_info("[simple-profiling]Enable Performance Counter %d for %s (%s) %s %s\n",
			perf_reg_number,
			performance_counter_options(perf_reg_number,
						    perf_reg_index),
			print_performance_counter_mode(txt, sizeof(txt),
						       perf_reg_flags),
			build_core_string(coretxt, field[CORE_IDX][0]),
			build_tc_string(tctxt, field[TC_IDX][0]));

		perfctl_param.counting_reg_mask = 1 << perf_reg_number;
		perfctl_param.preset_cnt = 0;
		perfctl_param.preset_ctl =
			(perf_reg_index << 5) | perf_reg_flags;
		perfctl_param.set_val = perf_set_cnt | perf_set_ctl;
		setup_all_perfcounter(&perfctl_param, field[CORE_IDX][0]);
	}
#if defined(PROFILING_IN_YIELD)
	if (field[START_IDX][0] == 1) {
		mips_profiling_special_enable(sp_enable_on, 0);
	}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
}
#if defined(PROFILING_IN_YIELD)
#define MIN_OFFSET_USEC 100U
/*--- #define MIN_OFFSET_USEC             (100U * 10 * 100) ---*/

/**
 * usec: 0: default
 */
static cycle_t pseudo_random_value(unsigned int usec)
{
	static unsigned short pn_16 = 0xead1;
	unsigned int val = avm_get_cycles();

	pn_16 = (pn_16 >> 1) ^ ((-(pn_16 & 1)) & 0xb400);

	val ^= (val >> PROFILING_TRIGGER_SHIFT) ^ pn_16;
	val &= PROFILING_TRIGGER_MASK;
	if (unlikely(usec)) {
		val %= (PROFILING_USEC_TO_TIMER(usec) / 2);
		return val + PROFILING_USEC_TO_TIMER(usec) / 2;
	}
	return val + PROFILING_USEC_TO_TIMER(MIN_OFFSET_USEC);
}

/**
 */
static void trace_to_uart(unsigned int core, unsigned int pc, unsigned int lr,
			  int cpu_id, int tc_id)
{
	/*--- if(core == 0) return; ---*/
	yield_spin_lock(&trace_lock);
#if 1
	arch_uart_send_byte(core, 0x7E);
	arch_uart_send_byte(core, (cpu_id << 4) | (tc_id));
	arch_uart_send_word(core, pc);
	arch_uart_send_word(core, lr);
#else
	arch_uart_send_byte(core, 'T');
	arch_uart_send_byte(core, 'E');
	arch_uart_send_byte(core, 'S');
	arch_uart_send_byte(core, 'A');
#endif
	yield_spin_unlock(&trace_lock);
}

/**
 * fuer avm_get_load nur tc_sched abfragen
 */
static void get_tcscheds(struct _yield_profiling *profile_entry)
{
	unsigned int tcstatus, tc_sched, reset_tcsched, tc;
	unsigned long vpflags;
	int haltval, old_tc, actual;
	int err = 0;

	for (tc = 0; tc < profile_entry->ntc; tc++) {
		/**
		 * START-CRITICAL START-CRITICAL START-CRITICAL START-CRITICAL START-CRITICAL
		 * Achtung kritischer Abschnitt - hier keine printks (ergo spinlocks  etc.) verwenden
		 * -> Deadlock-Gefahr!
		 */
		vpflags = dvpe();
		old_tc = read_c0_vpecontrol() & VPECONTROL_TARGTC;
		settc(tc);
		tcstatus = read_tc_c0_tcstatus();
		if (!(tcstatus & TCSTATUS_A)) {
			/* not dumping inactive threads */
			settc(old_tc);
			evpe(vpflags);
			continue;
		}
		if (unlikely(read_tc_c0_tcbind() ==
			     (unsigned int)read_c0_tcbind())) {
			haltval = 0;
			actual = 1;
		} else {
			haltval = read_tc_c0_tchalt();
			write_tc_c0_tchalt(1);
			actual = 0;
		}
		tc_sched = read_tc_c0_tcschefback();

		if (unlikely(tc_sched > (1U << 31))) {
			/*
			 * nur selten ruecksetzen, sonst ist der Summierungsfehler zumindest bei niedriger TC-Aktivitaet
			 *(verglichen mit wenn man perf_cnt0 auf instructions-completed setzt) zu gross
			 * Zusaetzlich Workarround: wenn man tcschefback auf 0 setzt, sind merkwuerdigerweise
			 * auch von allen anderen TC-Kontexten tcschefback = 0
			 * bei tcschefback != 0 z.B. 1  ist alles ok ... (???)
			 */
			reset_tcsched = 1;
			write_tc_c0_tcschefback(1); /*--- muss (fast) genullt werden, da kein rollover: workarround  */
		} else {
			reset_tcsched = 0;
		}
		if (!haltval) {
			write_tc_c0_tchalt(0);
		}
		settc(old_tc);
		evpe(vpflags);
		/**
		 * END-CRITICAL END-CRITICAL END-CRITICAL END-CRITICAL END-CRITICAL
		 */
		set_simple_performance_count_tc_sched(tc, profile_entry->simple_tcstat, tc_sched, &err,
						      reset_tcsched);
	}
}

/**
 * Yield-Kontext
 */
static int profiling_yield_handler(int signal, void *ref)
{
	unsigned int act_time, tc_prio;
	unsigned int diff_time;
	unsigned int new_event;
	struct _yield_profiling *profile_entry = (struct _yield_profiling *)ref;
	unsigned int status = 0, tc, cpu = -1, pc = 0, gp = 0, lr = 0, tcstatus,
		     mode, perf_cnt[3], reset_tcsched, sp = 0;

	act_time = avm_get_cycles();
	if (arch_trigger_valid() == 0) {
		return YIELD_HANDLED;
	}
	diff_time = act_time - profile_entry->last_time;
	profile_entry->tsum += (unsigned long long)(diff_time);
	profile_entry->last_time = act_time;

	if ((profile_entry->yield_mode & (YIELD_NOT_STOP | YIELD_TRACE_ON)) == YIELD_NOT_STOP) {
		/*--- nur die load-Werte berechnen ---*/
		get_tcscheds(profile_entry);
		new_event = PROFILING_USEC_TO_TIMER(100 * 1000);
		arch_set_next_trigger(new_event);
		return YIELD_HANDLED;
	}
	for (tc = 0; tc < profile_entry->ntc; tc++) {
		unsigned long vpflags;
		int haltval, old_tc, actual;
		int err = 0;

		/**
		 * START-CRITICAL START-CRITICAL START-CRITICAL START-CRITICAL START-CRITICAL
		 * Achtung kritischer Abschnitt - hier keine printks (ergo spinlocks  etc.) verwenden
		 * -> Deadlock-Gefahr!
		 */
		vpflags = dvpe();
		old_tc = read_c0_vpecontrol() & VPECONTROL_TARGTC;
		settc(tc);
		tcstatus = read_tc_c0_tcstatus();
		if (!(tcstatus & TCSTATUS_A)) {
			/* not dumping inactive threads */
			settc(old_tc);
			evpe(vpflags);
			continue;
		}
		if (unlikely(read_tc_c0_tcbind() ==
			     (unsigned int)read_c0_tcbind())) {
			haltval = 0;
			actual = 1;
		} else {
			haltval = read_tc_c0_tchalt();
			write_tc_c0_tchalt(1);
			actual = 0;

			/*--- eigentlich interessiert uns nur folgendes ... ---*/
			status = read_tc_c0_tcstatus();
			/*--- Pointer auf thread_info-Struktur fuer current-task-Ermittlung (falls Linux-OS) ---*/
			gp = read_tc_gpr_gp();
			if (((status >> TCSTATUS_TKSU_SHIFT) & 0x3) == 2) {
				/*--- wir sind im USER-Mode: Spezialbehandlung fuer gp-Ermittlung (da virtuelle Adressen) ---*/
				cpu = profile_entry->tc_to_cpu_nr[tc];
				if (cpu < num_possible_cpus()) {
					gp = kernelsp[cpu] & ~THREAD_MASK; /*--- kseg0-gp ermitteln ---*/
				}
			}
			pc = read_tc_c0_tcrestart(); /*--- aktueller pc ---*/
			lr = read_tc_gpr_ra(); /*--- Pointer auf thread_info-Struktur fuer current-task-Ermittlung (falls Linux-OS) ---*/
			sp = read_tc_gpr_sp(); /*--- aktueller sp fuer backtrace ---*/
		}
		perf_cnt[0] = read_tc_c0_perfcntr0();
		perf_cnt[1] = read_tc_c0_perfcntr1();
		perf_cnt[2] = read_tc_c0_tcschefback();

		if (profile_entry->act_roundrobin_tcstat[tc]) {
			profile_entry->act_roundrobin_tcstat[tc] = set_tc_stat(profile_entry->act_roundrobin_tcstat[tc],
									       perf_cnt,
									       profile_entry->simple_tcstat[tc].last_perf_count,
									       diff_time, &err);
		}
		if (unlikely(perf_cnt[2] > (1U << 31))) {
			/**
			 *	nur selten ruecksetzen, sonst ist der Summierungsfehler zumindest bei niedriger TC-Aktivitaet
				(verglichen mit wenn man perf_cnt0 auf instructions-completed setzt ) zu gross
			 *	Zusaetzlich Workarround: wenn man tcschefback auf 0 setzt, sind merkwuerdigerweise
			 *	auch von allen anderen TC-Kontexten tcschefback = 0
			 *	bei tcschefback != 0 z.B. 1  ist alles ok ... (???)
			 */
			reset_tcsched = 1;
			write_tc_c0_tcschefback(
				1); /*--- muss (fast) genullt werden, da kein rollover: workarround  */
		} else {
			reset_tcsched = 0;
		}
		/**
		 * backtrace
		 */
		if ((actual == 0) && virt_addr_valid(gp) &&
		    ((gp & THREAD_MASK) == 0)) { /*--- valid gp-address ? ---*/
			/*--- Linux-OS-Thread ---*/
			struct thread_info *pthread_info = (struct thread_info *)gp;

			/**
			 * Hier sind noch alle tc's gesperrt aber backtrace kann ziemlich lange dauern (bis zu 1 ms) !
			 * vpe wieder anschalten und tc's mit maximaler tc_prio nur wie Code tracen (kein backtrace)
			 * (tdmframeyield muss durchkommen) !
			 */
			tc_prio = read_tc_c0_tcschedule();
			evpe(vpflags);
			__avm_simple_profiling_backtrace_from_other_context(pc, lr, sp,
									    pthread_info->task,
									    pthread_info->cpu,
									    profile_entry->core,
									    tc,
									    ((tc_prio & YIELD_TC_SCHED_GROUP_MASK) == YIELD_TC_SCHED_PRIORITY_MAX) ?  1 : 0);
			vpflags = dvpe();
		}
		if (!haltval) {
			write_tc_c0_tchalt(0);
		}
		settc(old_tc);
		evpe(vpflags);
		/**
		 * END-CRITICAL END-CRITICAL END-CRITICAL END-CRITICAL END-CRITICAL
		 */
		set_simple_performance_count_tc_perfcnt(tc,
							profile_entry->simple_tcstat,
							perf_cnt, &err);
		set_simple_performance_count_tc_sched(tc,
						      profile_entry->simple_tcstat,
						      perf_cnt[2], &err,
						      reset_tcsched);
		if (unlikely(err)) {
			if (err == -ERANGE) {
				pr_err("[simple-profiling]set_tc_stat: tc%u ovr -> ignore\n",
				       tc);
			} else if (err == -EINVAL) {
				pr_err("[simple-profiling]set_tc_stat: tc%u invalid register\n",
				       tc);
			}
		}
		/*--- pr_err("%s: tc=%u perfs: %u(%llu) %u(%llu) %u(%llu)\n", __func__, tc, perf_cnt[0], profile_entry->perf_tcstat[tc].sum_perf_count[0], perf_cnt[1], profile_entry->perf_tcstat[tc].sum_perf_count[1], perf_cnt[2], profile_entry->perf_tcstat[tc].sum_tc_sched); ---*/

		if (actual) {
			/*--- fuer aktuellen Analyse-TC kein Profiling---*/
			continue;
		}
		mode = (tcstatus >> 23) & 0x3;
		/*--- pr_err("%s: tc=%u %08x: %08x %08x (%s)\n", __func__, tc, pc, content, tcstatus, mode == 1 ? "WAIT" : mode == 2 ? "YIELD" : mode == 3 ? "WAIT WHILE LOAD/STORE" : "RUN"); ---*/
		if ((mode == 1) || (mode == 2)) {
			/*--- do not trace thread if in yield (mode==1) or wait (mode==2) ---*/
			continue;
		}
		if (profile_entry->act_roundrobin_tcstat[tc]) {
			/*--- RoundRobin-Statistik an: Per-Counter invalidieren, da fuer Code-Profiling nonsens ---*/
			perf_cnt[0] = perf_cnt[1] = 0;
		}
		if (arch_is_linux_cpu(profile_entry->core,
				      tc) && /*--- really linux os ?  ---*/
		    (virt_addr_valid(gp) &&
		     ((gp & THREAD_MASK) == 0))) { /*--- valid gp-address ? ---*/
			/*--- Linux-OS-Thread ---*/
			struct thread_info *pthread_info = (struct thread_info *)gp;

			if (profile_entry->uart_trace) {
				trace_to_uart(profile_entry->core, pc, lr,
					      pthread_info->cpu, tc);
				profile_entry->uart_trace_cnt++;
			} else {
				__avm_simple_profiling_code_from_other_context(pc, lr,
									       pthread_info->task,
									       pthread_info->cpu,
									       profile_entry->core, tc,
									       perf_cnt[0], perf_cnt[1],
									       sp);
			}
		} else {
			/*--- wir uebergeben die Linux-CPU-ID: somit wird der CODE an diesen Core gebunden (VPE) - es kann kein current ermittelt werden ---*/
			if (profile_entry->uart_trace) {
				trace_to_uart(profile_entry->core, pc, lr,
					      profile_entry->linux_os_cpu, tc);
				profile_entry->uart_trace_cnt++;
			} else {
				__avm_simple_profiling_code_from_other_context(pc, lr, NULL,
									       profile_entry->linux_os_cpu,
									       profile_entry->core, tc,
									       perf_cnt[0], perf_cnt[1],
									       sp);
			}
		}
	}
	if ((simple_profiling.mask &
	     ((1 << avm_profile_data_type_code_address_info) |
	      (1 << avm_profile_data_type_backtrace)))) {
		new_event = pseudo_random_value(
			profile_entry->uart_trace ? 100 : 0);
	} else {
		/*--- ohne Codetrace wird nur Performance-Counter-Statistik getraced - relaxtes triggern ---*/
		new_event = PROFILING_USEC_TO_TIMER(100 * 1000);
	}
	arch_set_next_trigger(new_event);
	return YIELD_HANDLED;
}

/**
 * initialisiere per-core profile_entry fuer Yield und Statistik
 */
static void mips_profiling_special_init(void)
{
	unsigned int core, linux_os_cpu, cpu;

	for (core = 0; core < ARRAY_SIZE(gYieldProfiler); core++) {
		struct _yield_profiling *profile_entry = &gYieldProfiler[core];

		profile_entry->core = core;
		profile_entry->irq_nr = arch_get_profile_irq();
		profile_entry->ntc = supported_tcs();
		profile_entry->yield_handle = -1;
		profile_entry->yield_profile_id = YIELD_PROFILE_IPI_ID(core);
		profile_entry->yield_signal = YIELD_SIGNAL_BY_ID(profile_entry->yield_profile_id);

		profile_entry->profiling_tc = YIELD_TC_BY_ID(profile_entry->yield_profile_id);
		linux_os_cpu = ffs(mips_cpu_config[core].linux_os_mask); /*--- die erste Linux-CPU dieses Cores ermitteln ---*/
		if (linux_os_cpu) {
			profile_entry->linux_os_cpu = linux_os_cpu - 1 + mips_cpu_config[core].cpu_nr_offset;
		} else {
			profile_entry->linux_os_cpu = num_possible_cpus();
		}
		memset(profile_entry->tc_to_cpu_nr, 0xFF,
		       sizeof(profile_entry->tc_to_cpu_nr));
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			/*--- pr_err("%s:cpu=%u: core=%u vs %u - tc=%u\n", __func__, cpu, core, cpu_nr_to_tc_and_core[cpu].core, cpu_nr_to_tc_and_core[cpu].tc); ---*/
			if (cpu_nr_to_tc_and_core[cpu].core != core) {
				continue;
			}
			if (cpu_nr_to_tc_and_core[cpu].tc >= ARRAY_SIZE(profile_entry->tc_to_cpu_nr)) {
				continue;
			}
			profile_entry->tc_to_cpu_nr[cpu_nr_to_tc_and_core[cpu].tc] = cpu;
			/*--- pr_err("%s: set: core=%u tc=%u cpu=%d\n", __func__, core, cpu_nr_to_tc_and_core[cpu].tc, cpu); ---*/
		}
		init_all_tc_stat(profile_entry);
		/*--- pr_err("%s: core=%u linux-os=%u\n", __func__, core, profile_entry->linux_os_cpu); ---*/
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
static void simple_performance_statistic(struct _yield_profiling *profile_entry,
					 struct seq_file *seq)
{
	char txt[128];
	unsigned int tc, reg, head, display;
	unsigned int last_ctrl[PROFILING_MAX_PERF_REGISTER];

	memset(last_ctrl, 0xFF, sizeof(last_ctrl));

	for (tc = 0; tc < profile_entry->ntc; tc++) {
		struct _simple_performance_count_tc_stat *psimple_tcstat = &profile_entry->simple_tcstat[tc];

		if (profile_entry->act_roundrobin_tcstat[tc]) {
			continue;
		}
		head = display = 0;
		for (reg = 0; reg < PROFILING_MAX_PERF_REGISTER; reg++) {
			if (psimple_tcstat->sum_perf_count[reg]) {
				display++;
			}
			if (GET_MIPS_PERFORMANCE_EVENT(psimple_tcstat->perf_ctrl[reg]) != last_ctrl[reg]) {
				last_ctrl[reg] = GET_MIPS_PERFORMANCE_EVENT(psimple_tcstat->perf_ctrl[reg]);
				head++;
			}
		}
		if (display == 0) {
			continue;
		}
		snprintf(txt, sizeof(txt), "TC%u:", tc);
		for (reg = 0; head && reg < PROFILING_MAX_PERF_REGISTER;
		     reg++) {
			seq_printf(seq, "    %30s%s",
				   performance_counter_options(reg,
							       GET_MIPS_PERFORMANCE_EVENT(psimple_tcstat->perf_ctrl[reg])),
				   reg == (PROFILING_MAX_PERF_REGISTER - 1) ?  "\n" : " ");
		}
		for (reg = 0; reg < PROFILING_MAX_PERF_REGISTER; reg++) {
			unsigned long inst_per_sec = norm_per_sec(psimple_tcstat->sum_perf_count[reg],
								  profile_entry->tsum);

			seq_printf(seq, "%s%30lu.%03lu", txt,
				   (unsigned long)inst_per_sec / 1000 / 1000,
				   (inst_per_sec / 1000) % 1000);
			txt[0] = 0;
		}
		seq_puts(seq, " (Mio counts/s)\n");
	}
	seq_puts(seq, "\n");
}
/**
 * RoundRobin-Statistik per TC
 * format:  0 csv
 * format:  1 alle Counts
 * format:  2 interpretiert
 * csv: Csv-Format:
 * core;tc;ctrl-reg;ctrl-nmb;count,msec
 */
static void
roundrobin_performance_statistic_per_tc(struct _yield_profiling *profile_entry,
					int tc, struct seq_file *seq,
					unsigned int format)
{
	struct _simple_performance_count_tc_stat *psimple_tcstat =
		&profile_entry->simple_tcstat[tc];
	char txt[128], txt2[128];
	struct _roundrobin_perf_ctrl_tc_stat *table =
		profile_entry->roundrobin_tc_stat[tc];
	struct _roundrobin_perf_ctrl_tc_stat *start = table;
	unsigned int reg, nozero_flag;
	unsigned long long msec;
	unsigned long all_inst_per_sec, inst_per_sec, cnt_per_sec;
	unsigned long cycle_cnt;

	msec = profile_entry->tsum;
	do_div(msec, (gCycle_per_usec * 1000));
	if (msec == 0) {
		return;
	}
	all_inst_per_sec = norm_per_sec(all_instruction_completed(profile_entry), profile_entry->tsum);
	inst_per_sec     = norm_per_sec(psimple_tcstat->sum_tc_sched, profile_entry->tsum);
	if ((inst_per_sec / 1000) == 0) {
		/*--- dieser Thread tut nicht wirklich etwas ... ---*/
		return;
	}
	cycle_cnt = norm_cycle_sec(profile_entry);
	snprintf(txt, sizeof(txt), "Core%u - TC%u", profile_entry->core, tc);

	/*
	 * erstmal der Instruction Complete aus tcschefback (per-TC)
	 */
	if (format == 0) {
		seq_printf(seq, "%u;%u;%u;%u;%llu;%lu;%s\n",
			   profile_entry->core, tc, 0,
			   GET_MIPS_PERFORMANCE_EVENT(1),
			   psimple_tcstat->sum_tc_sched, (unsigned long)msec,
			   performance_counter_options(0, 1));
	} else if (format == 1) {
		seq_printf(
			seq,
			"%s: -------------------- %52s  Percent (normed on cycle)\n",
			txt, "Mio Cnts/s");
		seq_printf(seq, "[TCSF]%-46s%30lu.%03lu %s (%llu - %s)\n",
			   performance_counter_options(0, 1),
			   inst_per_sec / 1000 / 1000,
			   (inst_per_sec / 1000) % 1000,
			   get_percent(txt, sizeof(txt), inst_per_sec,
				       cycle_cnt),
			   psimple_tcstat->sum_tc_sched,
			   human_time((unsigned long)msec, txt2));
	} else if (format == 2) {
		seq_printf(seq, "%s: --------------------\n", txt);
		seq_printf(seq, "%-33s: %s (%lu.%03lu Mio Instruction/s)\n",
			   "TC-Instruction-quota:",
			   get_percent(txt, sizeof(txt), inst_per_sec,
				       all_inst_per_sec),
			   inst_per_sec / (1000 * 1000),
			   (inst_per_sec / 1000) % 1000);
	}
	if (tc == (int)profile_entry->profiling_tc) {
		/*--- der Monitor-Thread TC hat nur diesen einen per-TC-Eintrag ---*/
		return;
	}
	if (profile_entry->act_roundrobin_tcstat[tc] == NULL) {
		return;
	}
	while (table) {
		nozero_flag = ARRAY_SIZE(table->perf_ctrl);
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			msec = table->sum_perf_time[reg];
			do_div(msec, (gCycle_per_usec * 1000));
			if (msec == 0) {
				nozero_flag--;
				continue;
			}
			if (table->sum_perf_count[reg] == 0) {
				nozero_flag--;
				continue;
			}
			cnt_per_sec = norm_per_sec(table->sum_perf_count[reg],
						   table->sum_perf_time[reg]);
			if (format == 0) {
				seq_printf(seq, "%u;%u;%u;%u;%llu;%lu;%s\n",
					   profile_entry->core, tc, reg,
					   GET_MIPS_PERFORMANCE_EVENT(table->perf_ctrl[reg]),
					   table->sum_perf_count[reg],
					   (unsigned long)msec,
					   performance_counter_options(0,
								       GET_MIPS_PERFORMANCE_EVENT(table->perf_ctrl[reg])));
			} else if (format == 1) {
				seq_printf(seq,
					   "[%x-%2u]%-46s%30lu.%03lu %s (%llu - %s)\n",
					   reg,
					   GET_MIPS_PERFORMANCE_EVENT(table->perf_ctrl[reg]),
					   performance_counter_options(reg,
								       GET_MIPS_PERFORMANCE_EVENT(table->perf_ctrl[reg])),
					   cnt_per_sec / 1000 / 1000,
					   (cnt_per_sec / 1000) % 1000,
					   get_percent(txt2, sizeof(txt2),
					   cnt_per_sec, cycle_cnt),
					   table->sum_perf_count[reg],
					   human_time((unsigned long)msec, txt));
			}
		}
		if (table->proceed && (format == 2) && nozero_flag) {
			table->proceed(seq, profile_entry, table, inst_per_sec);
		}
		table = table->next;
		if (table == start) {
			break;
		}
	}
}
/**
 * RoundRobin-Statistik per Core
 * format:  0 csv
 * format:  1 alle Counts
 * format:  2 interpretiert
 * csv: Csv-Format:
 * core;tc;ctrl-reg;ctrl-nmb;count,msec
 */
static void roundrobin_performance_statistic_processor(
	struct _yield_profiling *profile_entry, int tc, struct seq_file *seq,
	unsigned int format)
{
	struct _simple_performance_count_tc_stat *psimple_tcstat = &profile_entry->simple_tcstat[tc];
	char txt[128], txt2[128];
	struct _roundrobin_perf_ctrl_tc_stat *table = profile_entry->roundrobin_tc_stat[tc];
	struct _roundrobin_perf_ctrl_tc_stat *start = table;
	unsigned int reg, nozero_flag;
	unsigned long long msec;
	unsigned long inst_per_sec, cnt_per_sec;
	unsigned long cycle_cnt;

	msec = profile_entry->tsum;
	do_div(msec, (gCycle_per_usec * 1000));
	if (msec == 0) {
		return;
	}
	inst_per_sec = norm_per_sec(all_instruction_completed(profile_entry),
				    profile_entry->tsum);
	if ((inst_per_sec / 1000) == 0) {
		/*--- dieser Core tut nicht wirklich etwas ... ---*/
		return;
	}
	cycle_cnt = norm_cycle_sec(profile_entry);
	snprintf(txt, sizeof(txt), "Core%u (P)  ", profile_entry->core);

	if (format == 1) {
		seq_printf(
			seq,
			"%s: -------------------- %52s  Percent (normed on cycle)\n",
			txt, "Mio Cnts/s");
		seq_printf(seq, "[TCSF]%-46s%30lu.%03lu %s (%llu - %s)\n",
			   performance_counter_options(0, 1),
			   inst_per_sec / 1000 / 1000,
			   (inst_per_sec / 1000) % 1000,
			   get_percent(txt, sizeof(txt), inst_per_sec,
				       cycle_cnt),
			   psimple_tcstat->sum_tc_sched,
			   human_time((unsigned long)msec, txt2));
	} else if (format == 2) {
		seq_printf(seq, "%s: --------------------\n", txt);
	}
	while (table) {
		nozero_flag = ARRAY_SIZE(table->perf_ctrl);
		for (reg = 0; reg < ARRAY_SIZE(table->perf_ctrl); reg++) {
			msec = table->sum_perf_time[reg];
			do_div(msec, (gCycle_per_usec * 1000));
			if (msec == 0) {
				nozero_flag--;
				continue;
			}
			if (table->sum_perf_count[reg] == 0) {
				nozero_flag--;
				continue;
			}
			cnt_per_sec = norm_per_sec(table->sum_perf_count[reg],
						   table->sum_perf_time[reg]);
			if (format == 0) {
				seq_printf(seq, "%u;%u;%u;%u;%llu;%lu;%s\n",
					   profile_entry->core, tc, reg,
					   GET_MIPS_PERFORMANCE_EVENT(table->perf_ctrl[reg]),
					   table->sum_perf_count[reg],
					   (unsigned long)msec,
					    performance_counter_options(0,
									GET_MIPS_PERFORMANCE_EVENT(table->perf_ctrl[reg])));
			} else if (format == 1) {
				seq_printf(
					seq,
					"[%x-%2u]%-46s%30lu.%03lu %s (%llu - %s)\n",
					reg,
					GET_MIPS_PERFORMANCE_EVENT(
						table->perf_ctrl[reg]),
					performance_counter_options(reg,
								    GET_MIPS_PERFORMANCE_EVENT(
								    table->perf_ctrl[reg])),
								    cnt_per_sec / 1000 / 1000,
								    (cnt_per_sec / 1000) % 1000,
								    get_percent(txt2, sizeof(txt2),
								    cnt_per_sec, cycle_cnt),
								    table->sum_perf_count[reg],
								    human_time((unsigned long)msec, txt));
			}
		}
		if (table->proceed && (format == 2) && nozero_flag) {
			table->proceed(seq, profile_entry, table, inst_per_sec);
		}
		table = table->next;
		if (table == start) {
			break;
		}
	}
}

/**
 */
static void mips_profiling_performance_statistic(int core, struct seq_file *seq,
						 unsigned int format)
{
	char txt[128];
	unsigned long long tsum_msec;
	unsigned int tc;
	struct _yield_profiling *profile_entry;

	if (core >= (int)ARRAY_SIZE(gYieldProfiler)) {
		seq_puts(seq, "\n");
		return;
	}
	profile_entry = &gYieldProfiler[core];

	tsum_msec = profile_entry->tsum;
	do_div(tsum_msec, (gCycle_per_usec * 1000));

	if (tsum_msec == 0) {
		seq_puts(seq, "\n");
		return;
	}
	seq_printf(seq, " (Measure-Time %s):\n",
		   human_time((unsigned long)tsum_msec, txt));
	if (format == 1) {
		simple_performance_statistic(profile_entry, seq);
	}
	/*--- die Round-Robin-Statistik: ---*/
	if (profile_entry->act_roundrobin_tcstat[profile_entry->profiling_tc]) {
		/*--- Statistik eines Cores ---*/
		roundrobin_performance_statistic_processor(
			profile_entry, profile_entry->profiling_tc, seq,
			format);
	}
	for (tc = 0; tc < profile_entry->ntc; tc++) {
		/*--- Statistik pro TC ---*/
		roundrobin_performance_statistic_per_tc(profile_entry, tc, seq,
							format);
	}
}
/**
 * perfcnt_for_profile nur im on-Fall: verwende TC-Performance-Counter fuer Profiling
 * on: wenn on = sp_enable_uart: Uart-Tracing (soweit von Hardware unterstuetzt)
 */
static void mips_profiling_special_enable(enum _simple_profile_enable_mode on,
					  unsigned int perfcnt_for_profile)
{
	struct _perfctl_param_write perfctl_param;
	unsigned int core, stopped = 0;

	/*--- pr_err("[simple-profiling]%s on=%u\n", __func__, on); ---*/
	for (core = 0; core < ARRAY_SIZE(gYieldProfiler); core++) {
		struct _yield_profiling *profile_entry = &gYieldProfiler[core];

		if (profile_entry->linux_os_cpu >= num_possible_cpus()) {
			pr_err("[simple-profiling]%s no linux on core %u\n",
			       __func__, core);
			continue;
		}
		if (on == sp_enable_off) {
			if (profile_entry->yield_mode & YIELD_TRACE_ON) {
				profile_entry->yield_mode &= ~YIELD_TRACE_ON;
#if defined(CONFIG_AVM_POWER)
				PowerManagmentActivatePowerMode("unlock_governor");
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/
			}
			if (profile_entry->yield_handle >= 0) {
				if (profile_entry->uart_trace) {
					pr_err("[simple-profiling]Uart-Trace stopped core=%u - %u "
					       "trace-samples generated in %lu seconds\n",
					       core,
					       profile_entry->uart_trace_cnt,
					       (jiffies -
						profile_entry->uart_trace) / HZ);
					profile_entry->uart_trace = 0;
				}
				if (profile_entry->yield_mode & YIELD_NOT_STOP) {
					pr_err("[simple-profiling]%s perform-stop core=%u not allowed\n",
					       __func__, core);
					continue;
				}
				arch_yield_map_setup(YIELD_CPU_BY_ID(profile_entry->yield_profile_id),
						     profile_entry->irq_nr,
						     0 /*--- irq ---*/, 0);
				profile_entry->yield_handle = -1;
				free_yield_handler_on(profile_entry->linux_os_cpu,
						      profile_entry->profiling_tc,
						      profile_entry->yield_signal,
						      profile_entry);
				pr_err("[simple-profiling]stop performance-trace on core=%u\n",
				       core);
				stopped++;
			}
		} else {
			if (on == sp_enable_uart) {
				unsigned int kbaud = 6250;

				profile_entry->uart_trace = !arch_uart_init(core, kbaud * 1000);
				if (profile_entry->uart_trace) {
					profile_entry->uart_trace = jiffies | 0x1;
					profile_entry->uart_trace_cnt = 0;
					pr_err("[simple-profiling]Uart-Trace enabled on core=%u rate=%u KBaud\n",
					       core, kbaud);
				}
			} else {
				profile_entry->uart_trace = 0;
			}
			if (perfcnt_for_profile) {
				mips_performance_counter_action("perform_tc 0");
			}
			if (profile_entry->yield_handle >= 0) {
				disable_yield_handler_on(profile_entry->linux_os_cpu,
							 profile_entry->profiling_tc,
							 profile_entry->yield_signal);
			}
			/*--- Counter zuruecksetzen & Perf-Statistik initialisieren ---*/
			pr_err("[simple-profiling]Core%u: cleanup performance-Statistic for new trace\n",
			       core);
			perfctl_param.preset_cnt        = 0;
			perfctl_param.set_val           = perf_set_cnt;
			perfctl_param.counting_reg_mask = (1 << mips_get_performance_counter_nr()) - 1;
			perfctl_param.counting_tc_mask  = (1 << PROFILING_MAX_COUNT_TCS) - 1;
			perfctl_param.yield_profile     = profile_entry; /*--- ermoeglicht, dass Statistik richtig aufgesetzt wird  ---*/
			profile_entry->tsum = 0;
			profile_entry->last_time = avm_get_cycles();
			smp_call_function_single(profile_entry->linux_os_cpu,
						 (smp_call_func_t)setup_perfcounter_per_core,
						 &perfctl_param, true);
			if (profile_entry->yield_handle >= 0) {
				/*--- pr_err("[simple-profiling]yield-handler 0x%x installed -> ignore\n", profile_entry->yield_handle); ---*/
				if ((profile_entry->yield_mode & YIELD_TRACE_ON) == 0) {
					profile_entry->yield_mode |= YIELD_TRACE_ON;
#if defined(CONFIG_AVM_POWER)
					PowerManagmentActivatePowerMode("lock_governor");
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/
				}
				enable_yield_handler_on(profile_entry->linux_os_cpu,
							profile_entry->profiling_tc,
							profile_entry->yield_signal);
				arch_next_trigger_for_cpu(profile_entry->linux_os_cpu,
							  pseudo_random_value(0));
				continue;
			}
			/*--- pr_err("[simple-profiling]%s linux_cpu=%u ntcs=%u irq %d signal=%x\n", __func__, profile_entry->linux_os_cpu, profile_entry->ntc, YIELD_IRQ_BY_ID(profile_entry->yield_profile_id), profile_entry->yield_signal); ---*/
			profile_entry->yield_handle = request_yield_handler_on(profile_entry->linux_os_cpu,
									       profile_entry->profiling_tc,
									       profile_entry->yield_signal,
									       profiling_yield_handler, profile_entry);
			if (profile_entry->yield_handle >= 0) {
				if ((profile_entry->yield_mode & YIELD_TRACE_ON) == 0) {
					profile_entry->yield_mode |= YIELD_TRACE_ON;
#if defined(CONFIG_AVM_POWER)
					PowerManagmentActivatePowerMode("lock_governor");
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/
				}
				arch_yield_map_setup(YIELD_CPU_BY_ID(profile_entry->yield_profile_id),
						     profile_entry->irq_nr,
						     2 /*--- yield ---*/,
						     profile_entry->yield_signal);

				arch_next_trigger_for_cpu(profile_entry->linux_os_cpu,
							  pseudo_random_value(0));
			}
		}
	}
	pr_err("[simple-profiling]Use: cat /proc/avm/profile/perform for result %s\n",
	       stopped == 0 ? "(no stop needed)" : "");
}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
/**
 */
static int __init arch_mips_profiler_init(void)
{
	unsigned int new_34k;
	unsigned int perf_count;
	unsigned int config7 = read_c0_config7();

#if defined(PROFILING_IN_YIELD)
	arch_init_mips_cpu_config();
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
	arch_profile_ctrl.cpu_profile		       = mips_cpu_config;
	arch_profile_ctrl.performance_counter_action   = mips_performance_counter_action;
	arch_profile_ctrl.performance_counter_help     = mips_performance_counter_help;
	arch_profile_ctrl.get_performance_counter_nr   = mips_get_performance_counter_nr;
	arch_profile_ctrl.get_performance_counter_mode = mips_get_performance_counter_mode;
#if defined(PROFILING_IN_YIELD)
	arch_profile_ctrl.profiling_special_enable        = mips_profiling_special_enable;
	arch_profile_ctrl.profiling_performance_statistic = mips_profiling_performance_statistic;
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
	perf_count = mips_get_performance_counter_nr();
#if defined(PROFILING_CPU_HAS_TC)
	if (perf_count)
		perf_count--;
#endif /*--- #if defined(PROFILING_CPU_HAS_TC) ---*/
	new_34k = config7 & (1 << 19);
	pr_err("[simple-profiling]:%d performance counters implemented, %s\n",
	       perf_count, new_34k ? "NEW_34K" : "OLD_34K");
#if defined(PROFILING_IN_YIELD)
	mips_profiling_special_init();
	if (perf_count) {
		unsigned int tc, ntc = supported_tcs();

		for (tc = 0; tc < ntc; tc++) {
			char preference[32];
			/*--- Preset der Performance-Counter ---*/
			if (tc != 2) {
				snprintf(preference, sizeof(preference),
					 "set 0 10 per_tc %u", tc);
				mips_performance_counter_action(preference);

				snprintf(preference, sizeof(preference),
					 "set 1 11 per_tc %u", tc);
				mips_performance_counter_action(preference);
			} else {
				/*--- TC2 ist der hiesige Yield: hier zaehlen wir zusaetzlich per-Core: Stalls und Cycles ---*/
				snprintf(preference, sizeof(preference),
					 "set 0 18 per_tc %u", tc);
				mips_performance_counter_action(preference);

				snprintf(preference, sizeof(preference),
					 "set 1 0 per_tc %u", tc);
				mips_performance_counter_action(preference);
			}
		}
		mips_performance_counter_action("perform_cpu 1");
#if defined(CONFIG_SOC_GRX500)
		{
			unsigned int i;

			mips_performance_counter_action("start");
			for (i = 0; i < ARRAY_SIZE(gYieldProfiler); i++) {
				struct _yield_profiling *profile_entry = &gYieldProfiler[i];

				if (profile_entry->yield_mode & YIELD_TRACE_ON) {
#if defined(CONFIG_AVM_POWER)
					PowerManagmentActivatePowerMode("unlock_governor");
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/
				}
				profile_entry->yield_mode = YIELD_NOT_STOP;
			}
		}
#endif /*--- #if defined(CONFIG_SOC_GRX500) ---*/
	}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
	return 0;
}
device_initcall(arch_mips_profiler_init);

#if defined(CONFIG_SOC_GRX500)

/**
 */
static struct _instruction_complete_stat {
	unsigned long last_jiffies;
	unsigned long long last_instruction_complete;
	unsigned int last_load;
} instruction_all[ARRAY_SIZE(gYieldProfiler)];

/**
 * die aktuelle Prozessor-Auslastung (beruecksichtigt auch die Non-Linux-Threads)
 * cpufreq in kHz
 */
unsigned int avm_get_load(unsigned int cpu, unsigned int cpufreq)
{
	unsigned int core, load;
	unsigned long difftime, time_jiffies, msec;
	unsigned long long diffinstruction, instructions;

	if (cpufreq == 0) {
		return 0;
	}
	if (cpu >= ARRAY_SIZE(cpu_nr_to_tc_and_core)) {
		return 0;
	}
	core = cpu_nr_to_tc_and_core[cpu].core;
	if (core >= ARRAY_SIZE(instruction_all)) {
		return 0;
	}
	time_jiffies = jiffies;
	difftime = jiffies - instruction_all[core].last_jiffies;
	msec = jiffies_to_msecs(difftime);
	if (msec == 0) {
		return instruction_all[core].last_load;
	}
	instructions = all_instruction_completed(&gYieldProfiler[core]);

	diffinstruction =
		instructions - instruction_all[core].last_instruction_complete;

	instruction_all[core].last_instruction_complete = instructions;
	instruction_all[core].last_jiffies = time_jiffies;

	do_div(diffinstruction, msec);
	/*--- pauschal: 1/3 Verlust durch Stalls ---*/
	load = min(100U,
		   (unsigned int)(diffinstruction) / ((cpufreq * 7) / 1000));
	instruction_all[core].last_load = load;
#if 0
	 pr_info("%s: cpu=%u -> core=%u load=%u %% msec=%lu mips=%u cpufreq=%u MHz\n", __func__,
		   cpu, core, instruction_all[core].last_load, msec,  (unsigned int)(diffinstruction) / 1000, cpufreq / 1000);
#endif
	return instruction_all[core].last_load;
}
#endif /*--- #if defined(CONFIG_SOC_GRX500) ---*/
