/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __avm_profile_h__
#define __avm_profile_h__

/**
 */
struct _cpu_nr_to_tc_core {
	unsigned int core;
	unsigned int tc;
};
extern struct _cpu_nr_to_tc_core cpu_nr_to_tc_and_core
	[NR_CPUS]; /*--- im jeweiligen arch_profile initialisieren ---*/

/**
 * each entry means one realcore - can divided on mips in virtual cores (vpe_nr)
 */
struct _cpucore_profile {
	unsigned int cpu_nr_offset;
	unsigned int vpe_nr; /*--- on mips vpe_nr ---*/
	unsigned char linux_os_mask; /*--- which tc has linux-os ---*/
	const struct _cpucore_profile *next_core;
};
#if defined(__KERNEL__)

#include <linux/seq_file.h>

/**
 */
enum _simple_profile_enable_mode;
struct _arch_profile_ctrl {
	const struct _cpucore_profile *cpu_profile;

	/* set performance-counter */
	void (*performance_counter_action)(char *str);

	/* info about performance-counter settings */
	void (*performance_counter_help)(struct seq_file *seq);

	/* count of performance-counter */
	unsigned int (*get_performance_counter_nr)(void);

	/* configuration of performance_counter <nr> */
	int (*get_performance_counter_mode)(char *str, int str_len,
			unsigned int nr);

	/* aktuell nur Lantiq: Profiling per Yield-Thread */
	void (*profiling_special_enable)(enum _simple_profile_enable_mode on,
			unsigned int enable_perfcnt);

	/* liefert weitergehende Statistik MIPS: per TC etc. */
	void (*profiling_performance_statistic)(int core, struct seq_file *seq,
			unsigned int format);
};
extern struct _arch_profile_ctrl arch_profile_ctrl;

extern int profilestat_category(char *txtbuf, int txtbuf_len, unsigned int core, unsigned int cpu_offset,
				unsigned int cpus, unsigned int full);
extern int profilestat_totalcall(char *txtbuf, int txtbuf_len, int core, unsigned int cpu_offset,
				unsigned int cpus, unsigned int weight);
extern char *comm_short_cpy(char *txt, unsigned int txt_size, char *comm, unsigned int comm_size, int act_pid);
extern int get_user_info(char *buf, unsigned int maxbuflen, pid_t pid, unsigned long addr);
extern int __get_userinfo(char *buf, unsigned int maxbuflen, struct mm_struct *mmm, unsigned long addr);
void __avm_simple_profiling_code_from_other_context(unsigned long pc, unsigned long lr,
				struct task_struct *curr_tsk, int cpu_id, int core, int tc_id,
				unsigned int perfcnt1, unsigned int perfcnt2, unsigned int sp);
extern unsigned long gCycle_per_usec;

extern char *avm_profile_data_short_names[avm_profile_data_type_unknown + 1];
void boot_profiling_stop(void);
#endif /*--- #if defined(__KERNEL__) ---*/

#endif /*--- #ifndef __avm_profile_h__ ---*/
