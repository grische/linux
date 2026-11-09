// SPDX-License-Identifier: GPL-2.0+

/**
 * gcc profiling/stat.c -O3 -Wall -g -Iinclude -ggdb -o profile_new
 */
#if defined(__KERNEL__)
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task.h>
#endif
#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>
#include <asm/mach_avm.h>

#include "avm_profile.h"
#include "arch_profile.h"

#define DBG_ERR(args...)	no_printk(args)
#define DBG_CALL(args...)	no_printk(args)
#define DBG_ENTRIES(args...)	no_printk(args)

#define DBG_TRC(args...)	no_printk(args)
#define DBG_IDLE(args...)	no_printk(args)
#else /*--- #if defined(__KERNEL__) ---*/
/**
 * SOF: Offline-testtool Offline-testtool Offline-testtool Offline-testtool Offline-testtool
 */
#define __maybe_unused __attribute__((unused))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if !defined(CONFIG_PROC_FS)
#define CONFIG_PROC_FS
#endif /*--- #if !defined(CONFIG_PROC_FS) ---*/
#define NR_CPUS		    4
#define num_possible_cpus() NR_CPUS
#define PAGE_SHIFT      12
#define PAGE_MASK       ((1U << PAGE_SHIFT) - 1)
#define __GFP_NORETRY   0
#define unlikely(a)     a
#define arch_is_linux_cpu(core, tc) ((tc < 2) ? 1 : 0)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <avm/profile/profile.h>
#include "avm_profile.h"
struct page {
	unsigned int page;
};
static int pages_cnt;
static inline struct page *alloc_pages(unsigned int dummy, unsigned int order)
{
	pages_cnt++;
	return malloc((1 << PAGE_SHIFT) * (1 << order));
}

/**
 */
static inline int get_user_info(char *buf, unsigned int maxbuflen, pid_t pid,
				unsigned long addr)
{
	return 1;
}

/**
 */
static inline int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

struct _avm_profile_data *data;
unsigned int avm_profile_data_entries;
static const char *profile_find_symbol_by_addr(void *addr,
					       unsigned long *codesize,
					       unsigned long *offset,
					       char *buf);
static const char *_profile_find_comm_by_pid(pid_t pid, char *txtbuf,
					     size_t txtbuf_size,
					     char *comm,
					     size_t comm_size);

#define free_pages(ptr, a)                                                     \
	do {                                                                   \
		free((void *)ptr), pages_cnt--;                                \
	} while (0)
#define likely(a) (a)
#define lowmem_page_address(ptr) (ptr)
#define avm_simple_profiling_by_idx(idx)                                       \
	(((idx) < avm_profile_data_entries) ? &data[idx] : NULL)
#define KSYM_NAME_LEN 128
#define KERN_INFO ""

FILE *fout;

/*--- #define DBG_ERR(args...)       fprintf(stderr, args) ---*/
#define DBG_ERR(args...) printf(args)
#define pr_info(args...) fprintf(fout, args)
#define seq_printf(a, args...) printf(args)
#define do_div(a, b) ((a) = (a) / (b))
#define __u32 unsigned int
#define __u64 unsigned long long

/*--- #define DEBUG_PROFILE_STAT ---*/
#if defined(DEBUG_PROFILE_STAT)
/*--- #define DBG_TRC(args...) printf(args) ---*/
#define DBG_TRC(args...)
#define DBG_CALL(args...) printf(args)
#define DBG_CSV(args...)
/*--- #define DBG_CSV(args...) printf(args) ---*/
/*--- #define DBG_CALL(args...) ---*/
#define DBG_ENTRIES(args...) printf(args)
/*--- #define DBG_ENTRIES(args...) ---*/
#define DBG_IDLE(args...) printf(args)
#else
#define DBG_TRC(args...)
#define DBG_CALL(args...)
#define DBG_ENTRIES(args...)
#define DBG_IDLE(args...)
#define DBG_CSV(args...)
#endif

struct seq_file {
	unsigned int dummy;
};

#define SYMBOL_LEN 64
#define TASK_COMM_LEN 64
struct _symbols_name {
	unsigned int valid;
	unsigned int fn_addr;
	unsigned int size;
	char name[SYMBOL_LEN];
	char module[TASK_COMM_LEN];
};
static struct _symbols_name *gSymbolname;

struct _current_name {
	unsigned int valid;
	unsigned int curr_pid;
	char current[TASK_COMM_LEN];
};
static struct _current_name *current_name;

#define min(a, b) ((a) > (b) ? (b) : (a))

/**
 * EOF: Offline-testtool Offline-testtool Offline-testtool Offline-testtool Offline-testtool
 */
#endif /*--- #else  ---*/ /*--- #if defined(__KERNEL__) ---*/

unsigned long gCycle_per_usec = 250;

/**
 */
char *avm_profile_data_short_names[avm_profile_data_type_unknown + 1] = {
	[avm_profile_data_type_code_address_info] "CODE",
	[avm_profile_data_type_backtrace] "CDTR",
	[avm_profile_data_type_trace_spinlock] "SLCK",
	[avm_profile_data_type_trace_skb] "SKBT",
	[avm_profile_data_type_hw_irq_begin] "BIRQ",
	[avm_profile_data_type_hw_irq_end] "EIRQ",
	[avm_profile_data_type_sw_irq_begin] "BSWI",
	[avm_profile_data_type_sw_irq_end] "ESWI",
	[avm_profile_data_type_timer_begin] "BTIM",
	[avm_profile_data_type_timer_end] "ETIM",
	[avm_profile_data_type_tasklet_begin] "BLET",
	[avm_profile_data_type_tasklet_end] "ELET",
	[avm_profile_data_type_hi_tasklet_begin] "BLHT",
	[avm_profile_data_type_hi_tasklet_end] "ELHT",
	[avm_profile_data_type_workitem_begin] "BWRK",
	[avm_profile_data_type_workitem_end] "EWRK",
	[avm_profile_data_type_func_begin] "BFUN",
	[avm_profile_data_type_func_end] "EFUN",
	[avm_profile_data_type_trigger_tasklet_begin] "BTLT",
	[avm_profile_data_type_trigger_tasklet_end] "ETLT",
	[avm_profile_data_type_trigger_user_begin] "BUST",
	[avm_profile_data_type_trigger_user_end] "EUST",
	[avm_profile_data_type_code_begin] "BCOD", /* deprecated */
	[avm_profile_data_type_code_end] "ECOD",   /* deprecated */
	[avm_profile_data_type_exception_begin] "BEXC",
	[avm_profile_data_type_exception_end] "EEXC",
	[avm_profile_data_type_sched] "SCHD",
	[avm_profile_data_type_irq_disable_begin] "IRQD",
	[avm_profile_data_type_irq_disable_end] "IRQE",
	[avm_profile_data_type_unknown] "ERROR"
};

#if defined(CONFIG_PROC_FS)

/**
 */
static inline __u64 mylongdiv(__u64 divider, __u32 quotient)
{
	do_div(divider, quotient);
	return divider;
}

/**
 */
static inline __u64 _CLK_TO_USEC(__u64 cycles)
{
	do_div(cycles, gCycle_per_usec);
	return cycles;
}

/**
 */
static inline __u32 _CLK_TO_NSEC(__u32 cycles)
{
	return (cycles * 1000) / gCycle_per_usec;
}

/**
 */
static inline __u32 CLK_TO_USEC(__u32 cycles)
{
	return cycles / gCycle_per_usec;
}

/**
 */
struct _profile_header {
	struct _profile_header *next;
	unsigned int used[NR_CPUS];
};

/**
 */
struct _profile_codeaddrcall_entry {
	void *addr;
	__u32 total_calls;
	__u32 codelength;
	unsigned short id;
	unsigned char delete : 1;
	unsigned char id_is_pid : 1;
	unsigned char id_is_irq : 1;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	char comm[AVM_PROFILE_CURRENT_COMM_INCLUDED]; /*--- short current name ---*/
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
};

#define PROFILE_CALL_ENTRIES                                                   \
	(((1 << PAGE_SHIFT) - sizeof(struct _profile_header)) /                \
	 ((sizeof(struct _profile_codeaddrcall_entry)) +                       \
	  sizeof(struct _profile_header)))
/**
 * vermerke alle Symbole
 */
struct _profile_codeaddrcallheader {
	struct _profile_header head;
	struct _profile_codeaddrcall_entry list[PROFILE_CALL_ENTRIES];
} __attribute__((packed));

/**
 */
struct _profile_list_entry {
	void *addr;
	void *lr;
	unsigned short id;
	unsigned short recursion;
	__u32 total_calls;
	__u32 max_triggertime;
	__u32 min_triggertime;
	__u32 max_consumption;
	__u64 sum_triggertime;
	__u64 part_consumption;
	__u64 sum_consumption;
	__u64 last_btime;
	__u32 total_access;
	__u32 total_activate;
	__u32 last_total_access;
	__u32 last_total_activate;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	char comm[AVM_PROFILE_CURRENT_COMM_INCLUDED]; /*--- short current name ---*/
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	/*--- unsigned long long last_etime; ---*/
};

#define PROFILE_LIST_ENTRIES                                                   \
	(((1 << PAGE_SHIFT) - sizeof(struct _profile_header)) /                \
	 ((sizeof(struct _profile_list_entry) * NR_CPUS)))
/**
 */
struct _profile_listheader {
	struct _profile_header head;
	struct _profile_list_entry list[NR_CPUS][PROFILE_LIST_ENTRIES];
} __attribute__((packed));

/*--- #define PROFILE_LIST_ENTRIES  4 ---*/
static struct _profile_listheader *profileheader[15];
// clang-format off
static struct _profile_list_data_type_mapper {
	const char *name;
	struct _profile_listheader **profileheader;
#define END_ENTRY                 (0x1 << 0)
#define BEGIN_ENTRY               (0x1 << 1)
#define CODE_ENTRY                (BEGIN_ENTRY | END_ENTRY)
#define TRIGGER_ENTRY             (0x1 << 2)		  /*--- no new area only trigger-timestamp ---*/
#define BH_ENTRY                  (0x1 << 3)		  /*--- bh-area ---*/
#define IRQ_ENTRY                 (0x1 << 4)		  /*--- irq-area ---*/
#define USER_ENTRY                (0x1 << 5)		  /*--- area, there should also be code (code not interrupted) ---*/
#define ID_COMPARE                (0x1 << 6)
#define ADDR_COMPARE              (0x1 << 7)
#define ID_IS_PID                 (0x1 << 8)		  /*--- die ID kann als PID ausgewertet werden ---*/
#define IRQ_DISABLE		  (0x1 << 9)
#define CHANGE_ENTRY              (0x1 << 10)		  /*--- fuer Scheduler: neuer Task ---*/

	unsigned int operation;
} data_type_mapper[avm_profile_data_type_unknown] = {
	[avm_profile_data_type_code_address_info]	{ .name = "Code",	.profileheader = &profileheader[0],  .operation = CODE_ENTRY | ID_COMPARE | ID_IS_PID},
	[avm_profile_data_type_backtrace]		{ .name = "Code",	.profileheader = &profileheader[0],  .operation = CODE_ENTRY | ID_COMPARE | ID_IS_PID},
/*--- [avm_profile_data_type_trace_skb]		{ .name = "Skbs",	.profileheader = &profileheader[1],  .operation = TRIGGER_ENTRY | ADDR_COMPARE }, ---*/
/*--- [avm_profile_data_type_trace_spinlock]		{ .name = "Spinlock",	.profileheader = &profileheader[1],  .operation = TRIGGER_ENTRY | ADDR_COMPARE }, ---*/
	[avm_profile_data_type_hw_irq_begin]		{ .name = "Hw-Irqs",	.profileheader = &profileheader[2],  .operation = IRQ_ENTRY | BEGIN_ENTRY | ID_COMPARE},
	[avm_profile_data_type_hw_irq_end]		{ .name = NULL,		.profileheader = &profileheader[2],  .operation = IRQ_ENTRY | END_ENTRY | ID_COMPARE},
	[avm_profile_data_type_exception_begin]		{ .name = "Exception",	.profileheader = &profileheader[3],  .operation = IRQ_ENTRY | BEGIN_ENTRY | ID_COMPARE},
	[avm_profile_data_type_exception_end]		{ .name = NULL,		.profileheader = &profileheader[3],  .operation = IRQ_ENTRY | END_ENTRY | ID_COMPARE},
	[avm_profile_data_type_sw_irq_begin]		{ .name = "Sw-Irqs",	.profileheader = &profileheader[4],  .operation = BH_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_sw_irq_end]		{ .name = NULL,		.profileheader = &profileheader[4],  .operation = BH_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_timer_begin]		{ .name = "Timers",	.profileheader = &profileheader[5],  .operation = BH_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_timer_end]		{ .name = NULL,		.profileheader = &profileheader[5],  .operation = BH_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_tasklet_begin]		{ .name = "Tasklets",	.profileheader = &profileheader[6],  .operation = BH_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_tasklet_end]		{ .name = NULL,		.profileheader = &profileheader[6],  .operation = BH_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_hi_tasklet_begin]	{ .name = "HiTasklets", .profileheader = &profileheader[7],  .operation = BH_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_hi_tasklet_end]		{ .name = NULL,		.profileheader = &profileheader[7],  .operation = BH_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_workitem_begin]		{ .name = "Workitems",	.profileheader = &profileheader[8],  .operation = BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_workitem_end]		{ .name = NULL,		.profileheader = &profileheader[8],  .operation = END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_func_begin]		{ .name = "func",	.profileheader = &profileheader[9],  .operation = USER_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_func_end]		{ .name = NULL,		.profileheader = &profileheader[9],  .operation = USER_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_trigger_tasklet_begin]	{ .name = "TaskletTrg", .profileheader = &profileheader[10], .operation = TRIGGER_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_trigger_tasklet_end]	{ .name = NULL,		.profileheader = &profileheader[10], .operation = TRIGGER_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_trigger_user_begin]	{ .name = "User-Trg",	.profileheader = &profileheader[11], .operation = TRIGGER_ENTRY | BEGIN_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_trigger_user_end]	{ .name = NULL,		.profileheader = &profileheader[11], .operation = TRIGGER_ENTRY | END_ENTRY | ADDR_COMPARE},
	[avm_profile_data_type_code_begin]		{ .name = "Schedule",	.profileheader = &profileheader[13], .operation = BEGIN_ENTRY | ID_COMPARE | ID_IS_PID},  /* deprecated */
	[avm_profile_data_type_code_end]		{ .name = NULL,		.profileheader = &profileheader[13], .operation = END_ENTRY | ID_COMPARE | ID_IS_PID},    /* deprecated */
	[avm_profile_data_type_sched]			{ .name = "Schedule",	.profileheader = &profileheader[13], .operation = CHANGE_ENTRY | ID_COMPARE | ID_IS_PID},
	[avm_profile_data_type_irq_disable_begin]	{ .name = "IrqDisable", .profileheader = &profileheader[14], .operation = BEGIN_ENTRY | ID_IS_PID | ADDR_COMPARE | IRQ_DISABLE },
	[avm_profile_data_type_irq_disable_end]		{ .name = NULL,		.profileheader = &profileheader[14], .operation = END_ENTRY | ID_IS_PID | IRQ_DISABLE },
};
// clang-format on

/**
 * mit diesen Eintrag geht eine CPU in den IDLE-Zustand
 * return: 0 no change
 */
static inline unsigned int
profile_activ_cpu_mask(struct _avm_profile_data *pdata, unsigned int *cpu_mask)
{
	unsigned int old_cpu_mask = *cpu_mask;

	if ((pdata->type == avm_profile_data_type_code_address_info) &&
	    (pdata->id == AVM_PROFILE_IDLE_ID)) {
		*cpu_mask &= ~(1 << pdata->cpu_id);
	} else {
		*cpu_mask |= (1 << pdata->cpu_id);
	}
	DBG_IDLE("\t%s cpu_mask=%x -> %x\n", __func__, old_cpu_mask, *cpu_mask);
	return *cpu_mask != old_cpu_mask;
}

/**
 * zaehle aktive CPU's
 */
static inline unsigned int profile_count_cpus(unsigned int cpu_mask)
{
	unsigned int cpus = 0;

	while (cpu_mask) {
		if (cpu_mask & 1) {
			cpus++;
		}
		cpu_mask >>= 1;
	}
	return cpus;
}

/**
 * comm evtl. nicht NULL-terminiert !
 */
char *comm_short_cpy(char *txt, unsigned int txt_size, char *comm,
		     unsigned int comm_size, int act_pid)
{
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	if (comm && comm[0]) {
		int max_len = min(txt_size - 1, comm_size);
		int str_len;

		memcpy(txt, comm, max_len);
		txt[max_len] = 0;
		if (act_pid) {
			str_len = strlen(txt);
			snprintf(&txt[str_len], txt_size - str_len, " (fin)");
		}
		return txt;
	}
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	snprintf(txt, txt_size, "PID_%u", act_pid);
	return txt;
}

/**
 */
static const char *exception_type(char *txtbuf, int txt_len, int cause)
{
	char *pstrCause;

#if defined(CONFIG_MIPS) || !defined(__KERNEL__)
	pstrCause = cause == 1 ? "Mod" :
		    cause == 2 ? "TLBL" :
		    cause == 3 ? "TLBS" :
		    cause == 4 ? "AdEL" :
		    cause == 5 ? "AdES" :
		    cause == 6 ? "IBE" :
		    cause == 7 ? "DBE" :
		    cause == 25 ? "Thread" :
		    cause == 23 ? "WATCH" :
		    cause == 31 ? "CacheErr" : "";
#else
	pstrCause = "";
#endif /*--- #if defined(CONFIG_MIPS) ---*/
	snprintf(txtbuf, txt_len, "%8s %2u", pstrCause, cause);
	return txtbuf;
}

#if defined(__KERNEL__)
/**
 * txtbuf mindestens KSYM_NAME_LEN gross
 */
static const char *profile_find_symbol_by_addr(void *addr,
					       unsigned long *symbolsize,
					       unsigned long *offset,
					       char *txtbuf)
{
#if defined(CONFIG_KALLSYMS)
	const char *name;
	char *module = NULL;
	char namebuf[KSYM_NAME_LEN];

	name = kallsyms_lookup((unsigned long)addr, symbolsize, offset, &module,
			       namebuf);
	if ((name == NULL) || (txtbuf == NULL)) {
		return name;
	}
	if (offset && symbolsize) {
		snprintf(txtbuf, KSYM_NAME_LEN, "%s+0x%lx/0x%lx%s%s%s", name,
			 *offset, *symbolsize, module ? " [" : "", module ?: "",
			 module ? "]" : "");
	} else {
		snprintf(txtbuf, KSYM_NAME_LEN, "%s%s%s%s", name,
			 module ? " [" : "", module ?: "", module ? "]" : "");
	}
#else /*--- #if defined(CONFIG_KALLSYMS) ---*/
	if (txtbuf) {
		snprintf(txtbuf, KSYM_NAME_LEN, "%pS",
			 addr); /*--- module+offset or raw-kernel-adress ---*/
	}
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_KALLSYMS) ---*/
	return txtbuf;
}

/**
 */
static const char *_profile_find_comm_by_pid(pid_t pid, char *txtbuf,
					     size_t txtbuf_size,
					     char *comm,
					     size_t comm_size)
{
	struct task_struct *tsk;
	struct pid *ppid;

	if (txtbuf == NULL) {
		return txtbuf;
	}
	if (pid == AVM_PROFILE_IDLE_ID) {
		snprintf(txtbuf, txtbuf_size, "IDLE");
		return txtbuf;
	}
	if (pid) {
		ppid = find_get_pid(pid);
		if (ppid) {
			tsk = get_pid_task(ppid, PIDTYPE_PID);
			if (tsk) {
				snprintf(txtbuf, txtbuf_size, "%s", tsk->comm);
				put_task_struct(tsk);
			} else {
				comm_short_cpy(txtbuf, txtbuf_size, comm,
					       comm_size, pid);
			}
			put_pid(ppid);
			return txtbuf;
		}
	}
	/**
	 * falls PID nicht gefunden wird, weil beendet - verwende Kurznamen
	 */
	comm_short_cpy(txtbuf, txtbuf_size, comm, comm_size, pid);
	return txtbuf;
}
#endif /*--- #if defined(__KERNEL__) ---*/

#define PLE_NULL ((struct _profile_list_entry *)NULL)
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
#define profile_find_comm_by_pid(pid, buf, bufsize, ctrl)                      \
	_profile_find_comm_by_pid(pid, buf, bufsize, ctrl ? ctrl->comm : NULL, \
				  sizeof(ctrl->comm))
#else /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
#define profile_find_comm_by_pid(pid, buf, bufsize, ctrl)                      \
	_profile_find_comm_by_pid(pid, buf, bufsize, NULL, 0)
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
/**
 */
static struct _profile_header *alloc_profile_header(void)
{
	struct _profile_header *plh;
	struct page *ptr = alloc_pages(__GFP_NORETRY, 0);

	if (ptr == NULL) {
		return NULL;
	}
	plh = (struct _profile_header *)lowmem_page_address(ptr);
	memset(plh, 0, 1 << PAGE_SHIFT);
	return plh;
}

/**
 */
static void free_profile_list(struct _profile_header **anchor)
{
	struct _profile_header *lh_act = *anchor;

	while (lh_act) {
		unsigned long tmp = (unsigned long)lh_act;

		lh_act = lh_act->next;
		free_pages(tmp, 0);
	}
	*anchor = NULL;
}

/**
 */
static struct _profile_codeaddrcall_entry *
find_maxcodeaddrcallentry(struct _profile_codeaddrcallheader *totalcall_list,
			  unsigned int fbit, unsigned int weight,
			  unsigned int delete)
{
	char txtbuf[KSYM_NAME_LEN] __attribute__((unused));
	unsigned int i;
	struct _profile_codeaddrcall_entry *max_pce = NULL;
	struct _profile_codeaddrcallheader *lh_act = totalcall_list;
	struct _profile_codeaddrcallheader *lh_last = NULL;
	unsigned int last_idx = 0;
	unsigned int max_calls = 0;
	unsigned int max_weight = 0;
	unsigned int act_weight = 0;

	while (lh_act) {
		for (i = 0; i < lh_act->head.used[0]; i++) {
			struct _profile_codeaddrcall_entry *pce =
				&lh_act->list[i];

			if ((pce->delete == 0) && (pce->total_calls)) {
				if (weight) {
					if (pce->codelength) {
						act_weight = (pce->total_calls
							      << fbit) /
							     pce->codelength;
						if (act_weight > max_weight) {
							max_weight = act_weight;
							max_pce = pce;
							last_idx = i;
							lh_last = lh_act;
						}
					}
				} else if (pce->total_calls > max_calls) {
					max_calls = pce->total_calls;
					max_pce = pce;
					last_idx = i;
					lh_last = lh_act;
				}
			}
		}
		lh_act =
			(struct _profile_codeaddrcallheader *)lh_act->head.next;
	}
	if (max_pce && delete) {
		max_pce->delete = 1; /*--- mark as deleted ---*/
		if (lh_last->head.used[0] == last_idx + 1) {
			lh_last->head.used[0] = last_idx;
		}
	}
	/*--- DBG_ERR("%p[%d] match id=%u codelength=%u addr=%p(%s)\n", lh_last, last_idx, max_pce->id, max_pce->codelength, max_pce->addr, profile_find_symbol_by_addr(max_pce->addr, txtbuf)); ---*/
	return max_pce;
}

/**
 * Suche call-Eintrag entsprechend pdata
 * nur fuer CODE-Entries
 */
static struct _profile_codeaddrcall_entry *
find_codeaddrcall_entry(struct _profile_codeaddrcallheader **anchor,
			struct _avm_profile_data *pdata, __u32 *sumcode_length)
{
	char txtbuf[KSYM_NAME_LEN] __maybe_unused;
	struct _profile_codeaddrcall_entry *pce;
	unsigned int i;
	void *addr;
	unsigned long codelength = 0, offset = 0;
	unsigned long _addr = (unsigned long)pdata->addr;
	unsigned int flag, id = pdata->id;
	struct _profile_codeaddrcallheader *last = NULL, *lh_act = *anchor,
					   *new;

	if (profile_find_symbol_by_addr((void *)_addr, &codelength, &offset,
					NULL)) {
		/*--- Kernel/Module-Adresse ---*/
		flag = ADDR_COMPARE;
		_addr -= offset;
		addr = (void *)_addr;
	} else {
		/*--- Userspace: Page-Aligned zusammenfassen ---*/
		flag = ID_IS_PID | ID_COMPARE | ADDR_COMPARE;
		_addr &= ~PAGE_MASK;
		addr = (void *)_addr;
	}
	/*--- DBG_ERR("%s: addr=%p offset=%x ('%s')id=%d\n", __func__, addr, offset, txtbuf, id); ---*/
	while (lh_act) {
		for (i = 0; i < PROFILE_CALL_ENTRIES; i++) {
			struct _profile_codeaddrcall_entry *pce =
				&lh_act->list[i];

			if ((pce->total_calls == 0)) {
				lh_act->head.used[0] = i + 1;
				pce->addr = addr;
				pce->id = id;
				pce->delete = 0;
				pce->id_is_pid = (flag & ID_IS_PID) ? 1 : 0;
				pce->id_is_irq = (flag & IRQ_ENTRY) ? 1 : 0;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
				pce->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
				if (codelength) {
					pce->codelength = codelength;
				} else {
					/*--- approximation ---*/
					pce->codelength = (1 << PAGE_SHIFT);
				}
				if (sumcode_length)
					*sumcode_length += pce->codelength;
				DBG_CALL(
					"%s: alloc:%s: %p[%u] addr=%p(%s) codelength=%u id=%d\n",
					__func__,
					avm_profile_data_short_names[pdata->type],
					lh_act, i, pce->addr,
					profile_find_symbol_by_addr(
						pce->addr, NULL, NULL, txtbuf),
					pce->codelength, pce->id);
				return pce;
			}
			if ((flag & ID_COMPARE) && (pce->id != id)) {
				continue;
			}
			if ((flag & ADDR_COMPARE) && (pce->addr != addr)) {
				continue;
			}
			DBG_CALL(
				"%s: found %s: %p[%d] addr=%p(%s) codelength=%u id=%u\n",
				__func__,
				avm_profile_data_short_names[pdata->type],
				lh_act, i, pce->addr,
				profile_find_symbol_by_addr(pce->addr, NULL,
							    NULL, txtbuf),
				pce->codelength, pce->id);
			return pce;
		}
		last = lh_act;
		lh_act =
			(struct _profile_codeaddrcallheader *)lh_act->head.next;
	}
	new = (struct _profile_codeaddrcallheader *)alloc_profile_header();
	if (new == NULL) {
		return NULL;
	}
	if (last) {
		last->head.next = (struct _profile_header *)new;
	} else {
		*anchor = new;
	}
	new->head.used[0] = 1;
	pce = &new->list[0];
	pce->addr = addr;
	pce->id = id;
	pce->id_is_pid = (flag & ID_IS_PID) ? 1 : 0;
	pce->id_is_irq = (flag & IRQ_ENTRY) ? 1 : 0;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	pce->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	if (codelength) {
		pce->codelength = codelength;
	} else {
		/*--- approximation ---*/
		pce->codelength = (1 << PAGE_SHIFT);
	}
	if (sumcode_length)
		*sumcode_length += pce->codelength;
	DBG_CALL(
		"%s: alloc new page: %s: new=%p[0] addr=%p(%s) codelength=%u id=%d\n",
		__func__, avm_profile_data_short_names[pdata->type], new,
		pce->addr,
		profile_find_symbol_by_addr(pce->addr, NULL, NULL, txtbuf),
		pce->codelength, pce->id);
	return pce;
}

/**
 * Suche Eintrag entsprechend pdata
 * flag: (verodert)
 * BEGIN_ENTRY/CHANGE_ENTRY:  falls nicht vorhanden so allozieren
 * END_ENTRY
 * ID_COMPARE: die ID vergleichen
 * ADDR_COMPARE: die Adresse vergleichen
 */
static struct _profile_list_entry *
find_profile_entry(struct _profile_listheader **anchor,
		   struct _avm_profile_data *pdata, unsigned int flag)
{
	char txtbuf[2][KSYM_NAME_LEN] __maybe_unused;
	struct _profile_list_entry *ple;
	unsigned int i;
	unsigned int cpu_id = pdata->cpu_id;
	void *addr = (void *)(unsigned long)pdata->addr;
	unsigned int id = pdata->id;
	struct _profile_listheader *last = NULL, *lh_act = *anchor, *new;

	if (cpu_id >= ARRAY_SIZE(lh_act->list)) {
		return NULL;
	}
	DBG_TRC("%s: addr=%p id=%d flag %x->", __func__, addr, id, flag);
	while (lh_act) {
		for (i = 0; i < PROFILE_LIST_ENTRIES; i++) {
			struct _profile_list_entry *ple =
				&lh_act->list[cpu_id][i];

			if (ple->last_btime == 0) {
				if (flag & (BEGIN_ENTRY | CHANGE_ENTRY)) {
					lh_act->head.used[cpu_id] = i + 1;
					ple->addr = addr;
					ple->lr = (void *)pdata->lr;
					ple->id = id;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
					ple->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
					DBG_CALL(
						"%s: alloc:%s: %p[%u] addr=%p(%s) id=%d %s\n",
						__func__,
						avm_profile_data_short_names
							[pdata->type],
						lh_act, i, ple->addr,
						profile_find_symbol_by_addr(
							ple->addr, NULL, NULL,
							txtbuf[0]),
						ple->id,
						flag & ID_IS_PID ?
							profile_find_comm_by_pid(
								ple->id,
								txtbuf[1],
								sizeof(txtbuf[1]),
								ple) :
							"");
					return ple;
				}
				return NULL;
			}
			if ((flag & ID_COMPARE) && (ple->id != id)) {
				continue;
			}
			if ((flag & ADDR_COMPARE) && (ple->addr != addr)) {
				continue;
			}
			DBG_CALL(
				"%s:found %s: %p[%d] addr=%p(%s) id=%u %s total-call=%u last_btime=%llu\n",
				__func__,
				avm_profile_data_short_names[pdata->type],
				lh_act, i, ple->addr,
				profile_find_symbol_by_addr(ple->addr, NULL,
							    NULL, txtbuf[0]),
				ple->id,
				flag & ID_IS_PID ?
					profile_find_comm_by_pid(
						ple->id, txtbuf[1],
						sizeof(txtbuf[1]), ple) :
					"",
				ple->total_calls, ple->last_btime);
			return ple;
		}
		last = lh_act;
		lh_act = (struct _profile_listheader *)lh_act->head.next;
	}
	if (!(flag & (BEGIN_ENTRY | CHANGE_ENTRY))) {
		return NULL;
	}
	new = (struct _profile_listheader *)alloc_profile_header();
	if (new == NULL) {
		return NULL;
	}
	if (last) {
		last->head.next = (struct _profile_header *)new;
	} else {
		*anchor = new;
	}
	new->head.used[cpu_id] = 1;
	ple = &new->list[cpu_id][0];
	ple->addr = addr;
	ple->lr = (void *)pdata->lr;
	ple->id = id;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	ple->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	DBG_CALL("%s: alloc new page: %s: new=%p[0] addr=%p(%s) id=%d %s\n",
		 __func__, avm_profile_data_short_names[pdata->type], new,
		 ple->addr,
		 profile_find_symbol_by_addr(ple->addr, NULL, NULL, txtbuf[0]),
		 ple->id,
		 flag & ID_IS_PID ?
			 profile_find_comm_by_pid(ple->id, txtbuf[1],
						  sizeof(txtbuf[1]), ple) :
			 "");
	return ple;
}

/**
 * ret: idle-Eintrag
 */
static struct _profile_list_entry *
profile_idle(struct _profile_listheader *anchor, unsigned int cpu_id)
{
	struct _profile_listheader *lh_act = anchor;
	unsigned int i;

	while (lh_act) {
		for (i = 0; i < lh_act->head.used[cpu_id]; i++) {
			struct _profile_list_entry *ple =
				&lh_act->list[cpu_id][i];

			if (ple->last_btime == 0) {
				continue;
			}
			if (ple->id == AVM_PROFILE_IDLE_ID) {
				return ple;
			}
		}
		lh_act = (struct _profile_listheader *)lh_act->head.next;
	}
	return NULL;
}

/**
 * ret: Summe der verbrauchten Zeit dieses Typs
 */
static __u64 profile_consumption(struct _profile_listheader *anchor,
				 unsigned int cpu_id)
{
	struct _profile_listheader *lh_act = anchor;
	unsigned int i;
	__u64 sum_consumption = 0;

	while (lh_act) {
		for (i = 0; i < lh_act->head.used[cpu_id]; i++) {
			struct _profile_list_entry *ple =
				&lh_act->list[cpu_id][i];

			if (ple->last_btime == 0) {
				continue;
			}
			sum_consumption += ple->sum_consumption;
		}
		lh_act = (struct _profile_listheader *)lh_act->head.next;
	}
	return sum_consumption;
}

#if defined(DEBUG_PROFILE_STAT)
/**
 * ret: Struktur mit groessten Werten
 */
static void print_valid_entries(const char *prefix,
				struct _profile_listheader *anchor,
				unsigned int cpu_id, unsigned int is_code)
{
	char txtbuf[2][KSYM_NAME_LEN * 2];
	struct _profile_listheader *lh_act = anchor;
	unsigned int i;

	pr_info("--------------------\n");
	pr_info("%s: %p %s cpu_id=%u used=%d\n", __func__, anchor, prefix,
		cpu_id, lh_act ? lh_act->head.used[cpu_id] : 0);
	while (lh_act) {
		for (i = 0; i < lh_act->head.used[cpu_id]; i++) {
			struct _profile_list_entry *ple =
				&lh_act->list[cpu_id][i];

			if (ple->last_btime) {
				pr_info("%s: %p[%d][%d]: %s(%p) %s%s id=%u: Calls: %u Consum=%llu(0x%llx) max_triggertime=%u(0x%x) sum_triggertime=%llu\n",
					__func__, lh_act, cpu_id, i,
					profile_find_symbol_by_addr(ple->addr,
								    NULL, NULL,
								    txtbuf[0]),
					ple->addr, is_code ? "pid=" : "",
					is_code ? profile_find_comm_by_pid(
							  ple->id, txtbuf[1],
							  sizeof(txtbuf[1]),
							  ple) :
						  "",
					ple->id, ple->total_calls,
					ple->sum_consumption,
					ple->sum_consumption,
					ple->max_triggertime,
					ple->max_triggertime,
					ple->sum_triggertime);
			}
		}
		lh_act = (struct _profile_listheader *)lh_act->head.next;
	}
}
#endif /*--- #if defined(DEBUG_PROFILE_STAT) ---*/
/**
 * ret: Struktur mit groessten Verbrauch
 * delete: traegt diesen auch aus, so dass beim naechsten Aufruf
 *		   der naechstgroesste gefunden werden kann
 */
static struct _profile_list_entry *
profile_entry_with_max_sum_consumption(struct _profile_listheader *anchor,
				       unsigned int cpu_id, int delete)
{
	struct _profile_listheader *lh_act = anchor;
	struct _profile_listheader *lh_last = NULL;
	unsigned int i, last_idx = 0;
	__u32 max_sum_consumption = 0;
	struct _profile_list_entry *ple_maxconsumption = NULL;

	while (lh_act) {
		for (i = 0; i < lh_act->head.used[cpu_id]; i++) {
			struct _profile_list_entry *ple =
				&lh_act->list[cpu_id][i];

			/*--- DBG_TRC("%s: %d: %p(%d): %llu %lu\n", __func__, i, ple->addr, ple->id, ple->last_btime, ple->max_consumption); ---*/
			if (ple->last_btime) {
				if (ple->sum_consumption >
				    max_sum_consumption) {
					max_sum_consumption =
						ple->sum_consumption;
					ple_maxconsumption = ple;
					last_idx = i;
					lh_last = lh_act;
				}
			}
		}
		lh_act = (struct _profile_listheader *)lh_act->head.next;
	}
	if (ple_maxconsumption && delete) {
		ple_maxconsumption->last_btime = 0; /* invalidiere Eintrag */
		if (lh_last->head.used[cpu_id] == last_idx + 1) {
			lh_last->head.used[cpu_id] = last_idx;
		}
	}
	return ple_maxconsumption;
}

#if !defined(__KERNEL__)
#define FORMAT_STRING_CSV_HEADER                                               \
	"cpu;core;tc;%s;timestamp%s;diff%s;total_access;total_calls"           \
	"consumption;triggertime;type;symbol;lr;cur"

#define FORMAT_STRING_HR_HEADER                                                \
	"cpu core tc;%s timestamp%s diff%s total_access total_calls"           \
	"consumption triggertime type symbol lr cur"

/**
 */
static void print_timelineheader(unsigned int mode, unsigned int cpu_offset,
				 unsigned int cpus)
{
	char *punit = (mode & 0x2) ? "(us)" : "(cyc)";

	if (cpus == 1) {
		pr_info("Cpu: %x\n", cpu_offset);
	} else {
		pr_info("Cpu's: %x-%x\n", cpu_offset, cpu_offset + cpus - 1);
	}
	pr_info("consumption between BEGIN an END of same, or for CODE (exclude interrupts)\n"
		"triggertime for normal  between BEGIN's of same (not for CODE)\n"
		"triggertime for trigger also between BEGIN and END of same instead of consumption\n"
		"total_access,total_activate like consumption\n");
	if (mode & 0x4) {
		pr_info("cpu %-10s  %25s%-5s diff%-5s total_access total_activate consumption%-5s triggertime%-5s type symbol %44slr %48scurrent %10sid\n",
			mode & 0x8 ? "" : "info", "timestamp", punit, punit,
			punit, punit, "", "", "");
	} else {
		pr_info("cpu;%s;%s%s;diff%s;total_access;total_activate;consumption%s;triggertime%s;type;symbol;%slr;%scurrent;%sid\n",
			mode & 0x8 ? "" : "info", "timestamp", punit, punit,
			punit, punit, "", "", "");
	}
}

#define FORMAT_STRING_CSV_PART1                                                \
	"%x;%s;%llu;%u;" /*--- cpu, [core, tc, stack_usage], time, diff ---*/
#define FORMAT_STRING_CSV_PART2                                                \
	"%u;%u;" /*--- total_access, total_activate ---*/
#define FORMAT_STRING_CSV_PART3_A "%u;%s;" /*--- consumption, ---*/
#define FORMAT_STRING_CSV_PART3_B "%s;%u;" /*--- ,triggertime ---*/
#define FORMAT_STRING_CSV_PART3_C "%s;;"
#define FORMAT_STRING_CSV_PART4                                                \
	"%s;%s;%s;%s;%u" /*--- type, addr-symbol, current, id ---*/

#define FORMAT_STRING_HR_PART1                                                 \
	"%x %-25s %12llu %10u " /*--- cpu, [core, tc stack_usage], time, diff ---*/
#define FORMAT_STRING_HR_PART2                                                 \
	"%10u      %10u " /*--- total_access, total_activate ---*/
#define FORMAT_STRING_HR_PART3_A                                               \
	"%12u      %-12s      " /*--- consumption, ---*/
#define FORMAT_STRING_HR_PART3_B                                               \
	"%-12s     %12u       " /*--- ,triggertime ---*/
#define FORMAT_STRING_HR_PART3_C "%-12s     %-12s       "
#define FORMAT_STRING_HR_PART4                                                 \
	"%s %-50s %-50s %-16s %5u" /*--- type, addr-symbol, current, id ---*/

static const char *format_field[6][2] = {
	{ FORMAT_STRING_CSV_PART1,   FORMAT_STRING_HR_PART1 },
	{ FORMAT_STRING_CSV_PART2,   FORMAT_STRING_HR_PART2 },
	{ FORMAT_STRING_CSV_PART3_A, FORMAT_STRING_HR_PART3_A },
	{ FORMAT_STRING_CSV_PART3_B, FORMAT_STRING_HR_PART3_B },
	{ FORMAT_STRING_CSV_PART3_C, FORMAT_STRING_HR_PART3_C },
	{ FORMAT_STRING_CSV_PART4,   FORMAT_STRING_HR_PART4 },

};
/**
 */
static char *cpu_extension(char *txt, int txtlen, unsigned int hr,
			   unsigned int core_id, unsigned int tc_id,
			   unsigned int stack_usage)
{
	char *p = txt;
	int len;

	if (tc_id != 0x7) {
		if (hr) {
			len = snprintf(p, txtlen, "(C%xT%x) ", core_id, tc_id);
		} else {
			len = snprintf(p, txtlen, "%x;%x;", core_id, tc_id);
		}
		len = min(txtlen, len);
		p += len;
		txtlen -= len;
	}
	len = snprintf(p, txtlen, "0x%x", stack_usage);
	return txt;
}

/**
 * mode or'red:
 * mode: 0x2 usec instead cycles
 * mode: 0x4 human readable instead csv
 * mode: 0x8 no cpu extension info
 * mode << 8: ENTRY-Macros
 */
static void print_timeline(unsigned int cpu_id, unsigned int core,
			   unsigned int tc, unsigned int stack_usage,
			   __u64 act_time, __u32 diff_time, __u32 _consumption,
			   __u32 _triggertime, __u32 total_access,
			   __u32 total_activate,
			   enum _avm_profile_data_type type, unsigned long addr,
			   unsigned long lr, pid_t pid, unsigned int id,
			   unsigned int mode)
{
	char txtbuf[512], txtbuf2[128], txtpart[3][KSYM_NAME_LEN * 2], *ptxt;
	int written;
	__u32 consumption, triggertime;
	int hr = (mode & 0x4) ? 1 : 0;
	int len = sizeof(txtbuf);

	ptxt = txtbuf;
	if (mode & 0x2) {
		act_time = _CLK_TO_USEC(act_time);
		diff_time = CLK_TO_USEC(diff_time);
		consumption = CLK_TO_USEC(_consumption);
		triggertime = CLK_TO_USEC(_triggertime);
	} else {
		consumption = _consumption;
		triggertime = _triggertime;
	}
	written = snprintf(ptxt, len, format_field[0][hr], cpu_id,
			   (mode & 0x8) ?
				   "" :
				   cpu_extension(txtbuf2, sizeof(txtbuf2), hr,
						 core, tc, stack_usage),
			   act_time, diff_time);
	written = min(written, len);
	ptxt += written, len -= written;
	mode >>= 0x8;
	written = snprintf(ptxt, len, format_field[1][hr], total_access,
			   total_activate);
	written = min(written, len);
	ptxt += written, len -= written;
	if ((mode & CODE_ENTRY) == CODE_ENTRY) {
		if (_consumption) {
			written = snprintf(ptxt, len, format_field[2][hr],
					   consumption, "");
		} else if (_triggertime) {
			written = snprintf(ptxt, len, format_field[3][hr], "",
					   triggertime);
		} else {
			written = snprintf(ptxt, len, format_field[4][hr], "",
					   "");
		}
	} else if ((mode & BEGIN_ENTRY) || (mode & TRIGGER_ENTRY) ||
		   (mode & CHANGE_ENTRY)) {
		written = snprintf(ptxt, len, format_field[3][hr], "",
				   triggertime);
	} else if ((mode & END_ENTRY) || (mode & CHANGE_ENTRY)) {
		written = snprintf(ptxt, len, format_field[2][hr], consumption,
				   "");
	} else {
		written = snprintf(ptxt, len, format_field[4][hr], "", "");
	}
	written = min(written, len);
	ptxt += written, len -= written;
	snprintf(ptxt, len, format_field[5][hr],
		 avm_profile_data_short_names[type],
		 profile_find_symbol_by_addr((void *)addr, NULL, NULL,
					     txtpart[0]),
		 profile_find_symbol_by_addr((void *)lr, NULL, NULL,
					     txtpart[1]),
		 profile_find_comm_by_pid(pid, txtpart[2], sizeof(txtpart[2]),
					  PLE_NULL),
		 id);

	pr_info("%s\n", txtbuf);
}
#endif /*--- #if !defined(__KERNEL__) ---*/
/**
 */
struct _last_per_cpu {
	struct _profile_list_entry *ple_code;
	struct _profile_list_entry *ple_irqdisable; /* last irq-disable-entry */
	unsigned int irq_flag;
	unsigned int curr_pid;
	/*--- __u32 total_access_code, total_activate_code;   ---*/ /*--- remark for print_timeline - remark only for code ---*/
	__u32 total_access,
		total_activate; /*--- remark for print_timeline - any remark ---*/
};

/**
 * geht alle Eintrage durch vermerkt die Haeufigkeit der entsprechenden Symbole
 * Beruecksichtigt nur Code-Entries
 * weight:      Gewichtung: calls / (total-Length ^ weight)
 */
static __u32
fill_profilecall_lists(struct _profile_codeaddrcallheader **call_list,
		       unsigned int cpu_offset, unsigned int cpus,
		       __u64 *sum_codelength, unsigned int weight)
{
	struct _avm_profile_data *pdata;
	struct _profile_codeaddrcall_entry *pce;
	struct _profile_list_data_type_mapper *dtm;
	unsigned int len = 0, i, total_calls = 0, line = 0, cpu_id;
	unsigned int sum = 0;

	*call_list = NULL;
#if defined(__KERNEL__)
	avm_simple_profiling_enable(sp_enable_off, 0, 0, &len, NULL, 1);
#else
	len = avm_profile_data_entries;
#endif /*--- #else ---*/ /*--- #if defined(__KERNEL__) ---*/
	for (i = 0; i < len; i++) {
		line++;
		pdata = avm_simple_profiling_by_idx(i);
		if (pdata == NULL) {
			break;
		}
		if ((__u32)pdata->type >= avm_profile_data_type_unknown) {
			continue;
		}
		cpu_id = pdata->cpu_id;
		if ((cpu_id < cpu_offset) || (cpu_id >= (cpus + cpu_offset))) {
			continue;
		}
		dtm = &data_type_mapper[pdata->type];
		if (dtm->profileheader == NULL) {
			/*--- pr_info("no analyzing for %s(%u)\n", data_type_mapper[pdata->type].name, pdata->type); ---*/
			continue;
		}
		if ((dtm->operation & CODE_ENTRY) != CODE_ENTRY) {
			/*--- parse only Code-Entries ---*/
			continue;
		}
#if 0
		if ((dtm->operation & CODE_ENTRY) == CODE_ENTRY) {
			if (pdata->id == AVM_PROFILE_IDLE_ID) {
				/*--- do not collect idle ---*/
				continue;
			}
			extend = ADDR_COMPARE;
		} else {
			extend = 0;
		}
#endif

		pce = find_codeaddrcall_entry(call_list, pdata, &sum);
		if (pce) {
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
			if (unlikely(pce->comm[0] == 0) && pce->id_is_pid) {
				memcpy(pce->comm, pdata->comm,
					sizeof(pce->comm));
			}
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
			total_calls++;
			pce->total_calls++;
			if (weight == 2) {
				pce->codelength *= pce->codelength;
			} else if (weight == 3) {
				pce->codelength = pce->codelength *
						  pce->codelength *
						  pce->codelength;
			}
		}
	}
	if (sum_codelength) {
		*sum_codelength = sum;
		if (weight == 2) {
			*sum_codelength *= *sum_codelength;
		} else if (weight == 3) {
			*sum_codelength = *sum_codelength * *sum_codelength *
					  *sum_codelength;
		}
	}
	return total_calls;
}

/**
 */
static __u32 sum_weight_lists(struct _profile_codeaddrcallheader *call_list,
			      unsigned int fbit)
{
	struct _profile_codeaddrcallheader *lh_act = call_list;
	unsigned int i, sum = 0;

	while (lh_act) {
		for (i = 0; i < lh_act->head.used[0]; i++) {
			struct _profile_codeaddrcall_entry *pce =
				&lh_act->list[i];

			if (pce->codelength) {
				sum += (pce->total_calls << fbit) /
				       pce->codelength;
			}
		}
		lh_act =
			(struct _profile_codeaddrcallheader *)lh_act->head.next;
	}
	return sum;
}

/**
 * ret: triggertime
 */
static __u32 set_triggertime(struct _profile_list_entry *ple, __u64 act_time,
			     int set_stat)
{
	__u32 triggertime;

	if (ple->last_btime == 0) {
		return 0;
	}
	triggertime =
		(__u32)min(act_time - ple->last_btime, (0x1ULL << 32) - 1);
	if (!set_stat) {
		return triggertime;
	}
	if (triggertime > ple->max_triggertime) {
		/*--- pr_info("%s: %d: triggertime %x -> max_triggertime %x act_time %llu last_btime %llx\n", __func__, i, triggertime, ple->max_triggertime, act_time, ple->last_btime); ---*/
		ple->max_triggertime = triggertime;
	}
	/*--- DBG_TRC("%s: triggertime %x min_triggertime %x\n", __func__, triggertime, ple->min_triggertime); ---*/
	if ((triggertime < ple->min_triggertime) ||
	    (ple->min_triggertime == 0)) {
		ple->min_triggertime = triggertime;
	}
	ple->sum_triggertime += triggertime;
	return triggertime;
}
/**
 */
static int cumulate_code_consumption(unsigned int cpu, unsigned int cpu_id,
				     struct _last_per_cpu *plast,
				     unsigned int activ_cpus, __u64 act_time,
				     __u64 last_act_time,
				     unsigned int total_access,
				     unsigned int total_activate)
{
	char txtbuf[KSYM_NAME_LEN * 2] __maybe_unused;
	struct _profile_list_entry *ple_code = plast->ple_code;

	if ((plast->irq_flag == 0) && ple_code) {
		/*--- we are not in irq/bh-context and have an code-entry: ---*/
		__u32 consumption = (__u32)(act_time - last_act_time);

		DBG_ENTRIES(
			"\t[cpu=%x]last was CODE: %p: calls=%u addr=%p (%s) pid=%d: act_time=%llx last_btime=%llx consumption %x/%u max_consumption %x\n",
			cpu, ple_code, ple_code->total_calls, ple_code->addr,
			profile_find_comm_by_pid(ple_code->id, txtbuf,
						 sizeof(txtbuf), ple_code),
			ple_code->id, act_time, ple_code->last_btime,
			consumption, activ_cpus, ple_code->max_consumption);

		if (activ_cpus == 0) {
			ple_code->part_consumption += consumption;
			DBG_IDLE(
				"\t\t[cpu=%x]leave IDLE: act_time=%llx last_act_time=%llu activ_cpus=%u part_consumption=%llu (consumption=%u)\n",
				cpu_id, act_time, last_act_time, activ_cpus,
				ple_code->part_consumption, consumption);
			return 1;
		}
		if (ple_code->id == AVM_PROFILE_IDLE_ID) {
			DBG_IDLE(
				"\t[cpu=%x]leave VPE-IDLE: act_time=%llx activ_cpus=%u\n",
				cpu_id, act_time, activ_cpus);
			consumption = 0;
		} else {
			/*--- consumption /= activ_cpus; ---*/
		}
		ple_code->part_consumption += consumption;
		DBG_ENTRIES(
			"\t\tcpu_id=%d->activ_cpus=%u %s: part_consumption=%llu (sum_consumption=%llu)\n",
			cpu_id, activ_cpus,
			profile_find_comm_by_pid(ple_code->id, txtbuf,
						 sizeof(txtbuf), ple_code),
			ple_code->part_consumption, ple_code->sum_consumption);
		if (cpu_id == cpu) {
			DBG_TRC("\t\t->total update(code %s): access: %u/%u -> %u/%u\n",
				profile_find_comm_by_pid(ple_code->id, txtbuf,
							 sizeof(txtbuf),
							 ple_code),
				ple_code->total_access,
				ple_code->total_activate,
				ple_code->total_access +
					(total_access - plast->total_access),
				ple_code->total_activate +
					(total_activate -
					 plast->total_activate));

			ple_code->total_access +=
				total_access - plast->total_access;
			ple_code->total_activate +=
				total_activate - plast->total_activate;
		}
	}
	return 0;
}

#define MAX_HWTHREAD_ENTRIES 12
/**
 * geht alle Eintrage durch und füllt die einzelnen Typen
 * profileheader im data_type_mapper
 * ret: konsumierte Gesamtzeit
 * timeline orred:	0x1 on
 *			0x2 usec instead cycles
 *			0x4 human readable instead csv
 *			0x8 no cpu-extension info
 */
static __u64 fill_profilestat_lists(unsigned int *entries,
				    unsigned int cpu_offset, unsigned int cpus,
				    unsigned int timeline)
{
	char txtbuf[KSYM_NAME_LEN * 2] __maybe_unused;
	struct _last_per_cpu last[MAX_HWTHREAD_ENTRIES];
	unsigned int cpu;
	unsigned int activ_cpus = 0;
	unsigned int activ_cpu_mask = 0;
	__u64 last_act_time = 1;
	struct _avm_profile_data *pdata;
	struct _profile_list_data_type_mapper *dtm;
	struct _profile_list_entry *ple;
	unsigned int len = 0, i, _entries = 0;
	unsigned int line = 0, cpu_id;
	__u32 last_time = 0;
	__u32 diff_time = 0;
	__u64 act_time = 1;

	if ((cpu_offset + cpus) >= MAX_HWTHREAD_ENTRIES)
		return 0;

#if defined(__KERNEL__)
	avm_simple_profiling_enable(sp_enable_off, 0, 0, &len, NULL, 1);
#else
	len = avm_profile_data_entries;
#endif /*--- #else ---*/ /*--- #if defined(__KERNEL__) ---*/
	for (i = 0; i < cpu_offset + cpus; i++) {
		last[i].irq_flag = 0;
		last[i].curr_pid = 0;
		last[i].ple_code = NULL;
		last[i].ple_irqdisable = NULL;
	}
#if !defined(__KERNEL__)
	if (timeline) {
		print_timelineheader(timeline, cpu_offset, cpus);
	}
#endif /*--- #if !defined(__KERNEL__) ---*/
	for (i = 0; i < len; i++) {
		line++;
		pdata = avm_simple_profiling_by_idx(i);
		if (pdata == NULL) {
			break;
		}
		if ((__u32)pdata->type >= avm_profile_data_type_unknown) {
			continue;
		}
		cpu_id = pdata->cpu_id;
		if ((cpu_id < cpu_offset) || (cpu_id >= (cpus + cpu_offset))) {
			continue;
		}
		if (!arch_is_linux_cpu(pdata->core_id, pdata->tc_id)) {
			/*--- keine Vermengung mit Non-Linux-Threads ---*/
			continue;
		}
		dtm = &data_type_mapper[pdata->type];
		if (dtm->profileheader == NULL) {
			/*--- pr_info("no analyzing for %s\n", data_type_mapper[pdata->type].name); ---*/
			continue;
		}
		_entries++;
		if (likely(last_time)) {
			diff_time = ((pdata->time | 0x1) - last_time);
			if (diff_time > UINT_MAX / 2) {
				/**
				 * fuzzy - falls timer-Eintraege falsch einsortiert
				 * durch Verzicht auf spinlock() beim tracen moeglich
				 */
				diff_time = 0;
				pdata->time = last_time;
			}
			act_time += (__u64)diff_time;
		}
		last_time = pdata->time | 0x1;

		if ((dtm->operation & (END_ENTRY | IRQ_DISABLE)) == (END_ENTRY | IRQ_DISABLE)) {
			ple = last[cpu_id].ple_irqdisable;
			last[cpu_id].ple_irqdisable = NULL;
		} else {
			ple = find_profile_entry(dtm->profileheader, pdata, dtm->operation);
		}
		if (ple == NULL) {
			/*--- pr_info("line %u: no entry alloced %s(%u)\n", line, avm_profile_data_short_names[pdata->type], pdata->type); ---*/
			continue;
		}
		if (
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
			unlikely(ple->comm[0] == 0) &&
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
			(dtm->operation & ID_IS_PID)) {
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
			memcpy(ple->comm, pdata->comm, sizeof(ple->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
		}
		DBG_TRC("line=%u:activ_cpus=%u cpu=%u irq_bh=%u curr_id=%u typ=%s act_time=%llu delta=%x: act_total=%u/%u last_total %u/%u %p (%s) id=%d: last_btime=%llx consumption %llx max_consumption %x total=%u/%u last=%u/%u\n",
			line, activ_cpus, cpu_id, last[cpu_id].irq_flag,
			last[cpu_id].curr_pid,
			avm_profile_data_short_names[pdata->type], act_time,
			diff_time, pdata->total_access, pdata->total_activate,
			last[cpu_id].total_access, last[cpu_id].total_access,
			ple->addr,
			profile_find_symbol_by_addr(ple->addr, NULL, NULL,
						    txtbuf),
			ple->id, ple->last_btime, ple->sum_consumption,
			ple->max_consumption, ple->total_activate,
			ple->total_access, ple->last_total_access,
			ple->last_total_activate);
		if (!(dtm->operation & TRIGGER_ENTRY) &&
		    !(dtm->operation & USER_ENTRY)) {
			/*--- this entries do a change of code-status ---*/
			for (cpu = cpu_offset; cpu < cpu_offset + cpus; cpu++) {
				if (cumulate_code_consumption(
					    cpu, cpu_id, &last[cpu], activ_cpus,
					    act_time, last_act_time,
					    pdata->total_access,
					    pdata->total_activate)) {
					break;
				}
			}

#if !defined(__KERNEL__)
			if (timeline) {
				struct _profile_list_entry *ple_code =
					last[cpu_id].ple_code;

				if ((last[cpu_id].irq_flag == 0) && ple_code) {
					/*--- print out the last collected!!! code-entry ---*/
					DBG_TRC("\t\tcpu_id=%d->%s: part_consumption=%llu\n",
						cpu_id,
						profile_find_comm_by_pid(
							ple_code->id, txtbuf,
							sizeof(txtbuf),
							ple_code),
						ple_code->part_consumption);
					print_timeline(
						cpu_id, pdata->core_id,
						pdata->tc_id, pdata->stack_pos,
						act_time, diff_time,
						ple_code->part_consumption, 0,
						pdata->total_access -
							last[cpu_id]
								.total_access,
						pdata->total_activate -
							last[cpu_id]
								.total_activate,
						avm_profile_data_type_code_address_info,
						(unsigned long)ple_code->addr,
						(unsigned long)ple_code->lr,
						ple_code->id, ple_code->id,
						(timeline & 0xFF) |
							(CODE_ENTRY << 8));
					diff_time = 0;
				}
			}
#endif /*--- #if !defined(__KERNEL__) ---*/
		}
		/**
		 * Handling for CODE
		 */
		if ((dtm->operation & CODE_ENTRY) == CODE_ENTRY) {
			/*--- update only if not the same entry: ---*/
			if (ple != last[cpu_id].ple_code) {
				if (last[cpu_id].irq_flag != 0) {
					/*--- error on csv or nested irq-call: ---*/
					/*--- DBG_ERR("Warning:line %u Code-Entry with setting irq_flag=%d (missing End of IRQ - perhaps nested irq)- reset irq_flag\n", line, last[cpu_id].irq_flag); ---*/
					/*--- last[cpu_id].irq_flag = 0; ---*/
				}
				if (last[cpu_id].ple_code) {
					struct _profile_list_entry *ple_code =
						last[cpu_id].ple_code;

					/*--- close-up the old code entry ---*/
					ple_code->sum_consumption +=
						ple_code->part_consumption;
					DBG_ENTRIES(
						"%u: CODE_ENTRY activ_cpus=%x bh_flag=%x %s: part_consumption %llu sum_consumption %llu\n",
						line, activ_cpus,
						last[cpu_id].irq_flag,
						profile_find_comm_by_pid(
							ple_code->id, txtbuf,
							sizeof(txtbuf),
							ple_code),
						ple_code->part_consumption,
						ple_code->sum_consumption);
					if (ple_code->part_consumption >
					    ple_code->max_consumption) {
						ple_code->max_consumption =
							ple_code->part_consumption;
					}
					ple_code->part_consumption = 0;
				}
				/*--- prepare the new entry ---*/
				ple->total_calls++;
				set_triggertime(ple, act_time, 1);
				ple->last_btime = act_time;
				last[cpu_id].ple_code = ple;
				last[cpu_id].curr_pid = pdata->id;
				ple->last_total_access = ple->total_access;
				ple->last_total_activate = ple->total_activate;
				/*--- get active cpus (special idle-code-entry) ---*/
				if (profile_activ_cpu_mask(pdata,
							   &activ_cpu_mask)) {
					activ_cpus = profile_count_cpus(
						activ_cpu_mask);
				}
				DBG_TRC("\tMark as %s %p %s pid=%u last_btime=%llu\n",
					pdata->id == AVM_PROFILE_IDLE_ID ?
						"idle" :
						"code",
					ple->addr,
					profile_find_comm_by_pid(
						pdata->id, txtbuf,
						sizeof(txtbuf), pdata),
					pdata->id, ple->last_btime);
			} else {
				DBG_TRC("\tsame %s %p %s pid=%u last_btime=%llu\n",
					pdata->id == AVM_PROFILE_IDLE_ID ?
						"idle" :
						"code",
					ple->addr,
					profile_find_comm_by_pid(
						pdata->id, txtbuf,
						sizeof(txtbuf), pdata),
					pdata->id, ple->last_btime);
			}
			if (pdata->id == AVM_PROFILE_IDLE_ID) {
				if (activ_cpus == 0) {
					DBG_IDLE(
						"\t\t[cpu=%x]enter IDLE: act_time=%llx activ_cpus=%u\n",
						cpu_id, act_time, activ_cpus);
				} else {
					DBG_IDLE(
						"\t[cpu=%x]enter VPE-IDLE: act_time=%llx activ_cpus=%u\n",
						cpu_id, act_time, activ_cpus);
				}
				if (last[cpu_id].irq_flag) {
					DBG_ERR("Warning:line %u IDLE-Entry with setting irq_flag (%d) -> reset\n",
						line,
						last[cpu_id].irq_flag = 0);
					last[cpu_id].irq_flag = 0;
				}
			}
			/**
		 * Handling for SCHEDULE
		 */
		} else if (dtm->operation & CHANGE_ENTRY) {
			__u32 triggertime __maybe_unused;

			triggertime = set_triggertime(ple, act_time,
						      !(dtm->operation &
							TRIGGER_ENTRY));
#if !defined(__KERNEL__)
			if (timeline) {
				/*--- triggertime also between begin-triggers  ---*/
				print_timeline(cpu_id, pdata->core_id,
					       pdata->tc_id, pdata->stack_pos,
					       act_time, diff_time, 0,
					       triggertime, 0, 0, pdata->type,
					       pdata->addr, pdata->lr,
					       last[cpu_id].curr_pid, pdata->id,
					       (timeline & 0xFF) |
						       (dtm->operation << 8));
			}
#endif /*--- #if !defined(__KERNEL__) ---*/
			ple->total_calls++;
			ple->last_btime = act_time;
			last[cpu_id].ple_code = ple;
			last[cpu_id].curr_pid = pdata->id;
			ple->last_total_access = pdata->total_access;
			ple->last_total_activate = pdata->total_activate;
		} else if (dtm->operation & BEGIN_ENTRY) {
			ple->total_calls++;
			if (ple->recursion == 0) {
				__u32 triggertime __maybe_unused;

				triggertime = set_triggertime(ple, act_time,
							      !(dtm->operation &
								TRIGGER_ENTRY));
#if !defined(__KERNEL__)
				if (timeline) {
					/*--- triggertime also between begin-triggers  ---*/
					print_timeline(
						cpu_id, pdata->core_id,
						pdata->tc_id, pdata->stack_pos,
						act_time, diff_time, 0,
						triggertime, 0, 0, pdata->type,
						pdata->addr, pdata->lr,
						last[cpu_id].curr_pid,
						pdata->id,
						(timeline & 0xFF) |
							(dtm->operation << 8));
				}
#endif /*--- #if !defined(__KERNEL__) ---*/
				ple->last_btime = act_time;
				ple->last_total_access = pdata->total_access;
				ple->last_total_activate =
					pdata->total_activate;
				DBG_TRC("\tMark as irq/trigger begin %p %s last_btime=%llx\n",
					ple->addr,
					profile_find_symbol_by_addr(
						ple->addr, NULL, NULL, txtbuf),
					ple->last_btime);
			}
			if ((dtm->operation & IRQ_ENTRY)) {
				/*--- code interrupted ---*/
				if (pdata->id != AVM_PROFILE_PAGE_FAULT_ID) {
					last[cpu_id].irq_flag++;
				}
				ple->recursion++;
			} else if ((dtm->operation & IRQ_DISABLE))
				last[cpu_id].ple_irqdisable = ple;
		/**
		 * Handling for END_ENTRY
		 */
		} else if (dtm->operation & END_ENTRY) {
			if ((dtm->operation & IRQ_ENTRY)) {
				if (pdata->id != AVM_PROFILE_PAGE_FAULT_ID) {
					if (last[cpu_id].irq_flag) {
						last[cpu_id].irq_flag--;
					} else {
						DBG_ERR("Warning:line %u %s: End-Entry without setting irq_flag\n",
							line,
							avm_profile_data_short_names
								[pdata->type]);
					}
				}
			}
			if (ple->recursion) {
				ple->recursion--;
			}
			if (ple->last_btime && (ple->recursion == 0)) {
				if ((dtm->operation & TRIGGER_ENTRY)) {
					/*--- der Abschnitt zwischen Begin und End interessiert als Latenz: ---*/
					__u32 triggertime = set_triggertime(
						ple, act_time, 1);
#if !defined(__KERNEL__)
					if (timeline) {
						print_timeline(
							cpu_id, pdata->core_id,
							pdata->tc_id,
							pdata->stack_pos,
							act_time, diff_time, 0,
							triggertime, 0, 0,
							pdata->type,
							pdata->addr, pdata->lr,
							last[cpu_id].curr_pid,
							pdata->id,
							(timeline & 0xFF) |
								(dtm->operation
								 << 8));
					}
#endif /*--- #if !defined(__KERNEL__) ---*/
					DBG_TRC("\t trigger end %p %s triggertime=%x\n",
						ple->addr,
						profile_find_symbol_by_addr(
							ple->addr, NULL, NULL,
							txtbuf),
						triggertime);
				} else {
					__u32 consumption = (__u32)(
						act_time - ple->last_btime);

					DBG_TRC("\t irq end %p %s consumption=%x/%d\n",
						ple->addr,
						profile_find_symbol_by_addr(
							ple->addr, NULL, NULL,
							txtbuf),
						consumption, activ_cpus);
					if (activ_cpus > 1) {
						/*--- consumption /= activ_cpus; ---*/
#if defined(DEBUG_PROFILE_STAT)
					} else {
						if (activ_cpus == 0)
							DBG_TRC("error activ_cpus == 0\n");
#endif /*--- #if defined(DEBUG_PROFILE_STAT) ---*/
					}
					if (consumption >
					    ple->max_consumption) {
						/*--- DBG_TRC(" %s: %x %d: consumption %x max_consumption %x\n", __func__, ple->addr, i, consumption, ple->max_consumption); ---*/
						ple->max_consumption =
							consumption;
					}
					ple->sum_consumption += consumption;
					ple->total_access +=
						pdata->total_access -
						ple->last_total_access;
					ple->total_activate +=
						pdata->total_activate -
						ple->last_total_activate;
					/*--- DBG_TRC("\t\t->total update(END_ENTRY): %u %u\n",ple->total_access, ple->total_activate); ---*/
#if !defined(__KERNEL__)
					if (timeline) {
						print_timeline(
							cpu_id, pdata->core_id,
							pdata->tc_id,
							pdata->stack_pos,
							act_time, diff_time,
							consumption, 0,
							pdata->total_access -
								ple->last_total_access,
							pdata->total_activate -
								ple->last_total_activate,
							pdata->type,
							pdata->addr, pdata->lr,
							last[cpu_id].curr_pid,
							pdata->id,
							(timeline & 0xFF) |
								(dtm->operation
								 << 8));
					}
#endif /*--- #if !defined(__KERNEL__) ---*/
				}
			}
		}
		if (!(dtm->operation & TRIGGER_ENTRY) &&
		    !(dtm->operation & USER_ENTRY)) {
			/*--- Remarks fuer die Ermittlung der Codeabschnitte---*/
			last_act_time = act_time;
			last[cpu_id].total_access = pdata->total_access;
			last[cpu_id].total_activate = pdata->total_activate;
		}
	}
	/*--- close-up all code entries ---*/
	for (cpu = cpu_offset; cpu < cpu_offset + cpus; cpu++) {
		if (last[cpu].ple_code) {
			struct _profile_list_entry *ple_code =
				last[cpu].ple_code;

			last[cpu].ple_code->sum_consumption +=
				ple_code->part_consumption;
			if (ple_code->part_consumption >
			    ple_code->max_consumption) {
				ple_code->max_consumption =
					ple_code->part_consumption;
			}
		}
	}
	if (entries)
		*entries = _entries;
	return act_time;
}

#define rprint(buf, buf_len, args...)                                          \
	do {                                                                   \
		if (buf) {                                                     \
			if (buf_len > 0) {                                     \
				int slen = snprintf(buf, buf_len, args);       \
				slen = min(buf_len, slen);                     \
				buf += slen;                                   \
				buf_len -= slen;                               \
			}                                                      \
		} else {                                                       \
			pr_info(args);                                         \
		}                                                              \
	} while (0)

#define MAX_VAL (((0x1ULL << 32) / 100))
/**
 */
static char *percent(__u64 val, __u64 norm, char *buf)
{
	__u32 rem, div;

	while ((val > MAX_VAL) || (norm > MAX_VAL)) {
		norm >>= 1;
		val >>= 1;
	}
	div = (__u32)val * 100U;

	rem = div % (__u32)norm;
	div = div / (__u32)norm;
	rem *= 100U;
	rem /= (__u32)norm;
	rem %= 100;
	sprintf(buf, "%3u.%02u %%", div, rem);
	return buf;
}

/**
 */
static char *human_time(__u64 cycles, char *buf)
{
	__u32 usec = (__u32)_CLK_TO_USEC(cycles);

	if (usec >= (1000 * 1000)) {
		__u32 frac = usec / (1000 * 10); // 1/100s !

		sprintf(buf, "%3u.%02u  s", usec / (1000 * 1000), frac % 100);
	} else if (usec >= 1000) {
		sprintf(buf, "%3u.%02u ms", usec / 1000, (usec % 1000) / 10);
	} else if (usec >= 1) {
		__u32 nsec = _CLK_TO_NSEC((__u32)cycles);

		sprintf(buf, "%3u.%02u us", usec, (nsec % 1000) / 10);
	} else {
		sprintf(buf, "%3u    ns", _CLK_TO_NSEC((__u32)cycles));
	}
	return buf;
}

/**
 */
static void free_profilestat_lists(void)
{
	unsigned int i;

	for (i = 0; i < avm_profile_data_type_unknown; i++) {
		struct _profile_list_data_type_mapper *pdtm =
			&data_type_mapper[i];

		if (pdtm->profileheader == NULL) {
			continue;
		}
		free_profile_list(
			(struct _profile_header **)pdtm->profileheader);
	}
}

/**
 */
char *print_addr_with_mask(unsigned int addr, unsigned int addrlevel, char *buf)
{
	char *end = &buf[8 + 2];

	sprintf(buf, "0x%08x", addr);
	while (addrlevel-- && (end != &buf[1])) {
		*(--end) = 'X';
	}
	return buf;
}

/**
 * export-function: list total call of function
 * core: number of core (-1) all
 * cpus: count of vpes
 * weight: total call weighted with codelength ^ weight
 */
int profilestat_totalcall(char *txtbuf, int txtbuf_len, int core,
			  unsigned int cpu_offset, unsigned int cpus,
			  unsigned int weight)
{
#if defined(__KERNEL__)
	unsigned long last_jiff;
#endif /*--- #if defined(__KERNEL__) ---*/
	char txtsymbol[KSYM_NAME_LEN];
	char txtcomm[KSYM_NAME_LEN * 2];
	char txtpercent[64];
	unsigned int weight_divider;
	struct _profile_codeaddrcallheader *totalcall_list;
	struct _profile_codeaddrcall_entry *pce = NULL;
	char *buf_start = txtbuf;
	__u32 total_calls, fbit = 0;
	__u64 sum_codelength;

	if (weight > 3) {
		weight = 3;
	}
	total_calls = fill_profilecall_lists(&totalcall_list, cpu_offset, cpus,
					     &sum_codelength, weight);
	if (total_calls == 0) {
		return 0;
	}
	switch (weight) {
	case 0:
		rprint(txtbuf, txtbuf_len, "display top of function calls\n");
		break;
	case 1:
		rprint(txtbuf, txtbuf_len,
		       "display top of function calls weighted with codelength\n");
		break;
	case 2:
		rprint(txtbuf, txtbuf_len,
		       "display top of function calls weighted with square-codelength\n");
		break;
	}
	if (core != -1) {
		if (cpus == 1) {
			rprint(txtbuf, txtbuf_len, "CORE%u CPU: %u ", core,
			       cpu_offset);
		} else {
			rprint(txtbuf, txtbuf_len, "CORE%u: CPU: %u-%u ", core,
			       cpu_offset, cpu_offset + cpus - 1);
		}
	}
	rprint(txtbuf, txtbuf_len,
	       "sum of calls: %u (user-function without codelength-info approximated to 10 %% of PAGE-SIZE)\n",
	       total_calls);
	rprint(txtbuf, txtbuf_len, "%-64s (%-8s/%5s) %-16s %s %8s %6s\n",
	       "symbol", "addr", "id", "pid", "percent", "calls",
	       weight <= 1 ? "codelen" : "weight");
	if (weight) {
		fbit = 32 - fls(total_calls);
		weight_divider = sum_weight_lists(totalcall_list, fbit);
		if (weight_divider == 0) {
			weight_divider = 1;
			/*--- fprintf(stderr, "error weight_divider=%d %lld\n", total_calls, sum_codelength); ---*/
		}
	} else {
		weight_divider = total_calls;
	}
#if defined(__KERNEL__)
	last_jiff = jiffies;
#endif /*--- #if defined(__KERNEL__) ---*/
	for (;;) {
		__u64 weight_val;

		pce = find_maxcodeaddrcallentry(totalcall_list, fbit, weight,
						1);
		if (pce == NULL) {
			break;
		}
#if defined(__KERNEL__)
		if ((jiffies - last_jiff) > HZ) {
			/*--- relax kernel ---*/
			last_jiff = jiffies;
			schedule();
		}
#endif /*--- #if defined(__KERNEL__) ---*/
		switch (weight) {
		default:
		case 0:
			weight_val = pce->total_calls;
			break;
		case 1 ... 3:
			weight_val =
				(pce->total_calls << fbit) / pce->codelength;
			break;
		}
		if (pce->id_is_irq) {
			if (pce->id == AVM_PROFILE_PAGE_FAULT_ID) {
				strcpy(txtsymbol, "PAGE-FAULT:");
			} else {
				snprintf(txtsymbol, sizeof(txtsymbol),
					 "IRQ: %u", pce->id);
			}
		} else {
		   /*--- if(pce->id_is_pid) pr_info("%s: pid=%u: addr=%p ppid=%p\n", __func__, pce->id,  pce->addr, find_vpid(pce->id)); ---*/
			if ((pce->id_is_pid == 0) ||
			     get_user_info(txtsymbol, sizeof(txtsymbol), pce->id, (unsigned long)pce->addr)) {
				profile_find_symbol_by_addr(pce->addr, NULL, NULL, txtsymbol);
			}
		   /*--- pr_info("%s: pid=%u: addr=%p %s\n", __func__, pce->id,  pce->addr, txtsymbol); ---*/
		}
		rprint(txtbuf, txtbuf_len, "%-64s (%08lx/%5d) %-16s %s %8u %6u\n",
		       txtsymbol,
		       (unsigned long)pce->addr,
		       pce->id,
		       pce->id_is_pid ? profile_find_comm_by_pid(pce->id, txtcomm, sizeof(txtcomm), pce) : "",
		       percent(weight_val, weight_divider, txtpercent),
		       pce->total_calls,
		       pce->codelength);
		/*--- printk(KERN_INFO"%64ph - '%s'\n", txtsymbol, txtsymbol); ---*/
	}
	free_profile_list((struct _profile_header **)&totalcall_list);
	return txtbuf - buf_start;
}

/**
 * export-function: list complete statistic
 * cpus: count of vpes
 * code works only correct with vpe's (consumption-calculation)
 * for real cpus call with cpu_offset and cpu-idx
 *
 * full: also access-statistic
 */
int profilestat_category(char *txtbuf, int txtbuf_len, unsigned int core,
			 unsigned int cpu_offset, unsigned int cpus,
			 unsigned int full)
{
	char cputype[32];
	char *buf_start = txtbuf;
	__u64 time;
	__u64 idle_sum = 0;
	unsigned int i, cpu, entries;
	char txt[2][32];

	if (gCycle_per_usec == 0) {
		return 0;
	}
	if (cpus > 1) {
		snprintf(cputype, sizeof(cputype), "[CORE%u:CPU %u-%u]", core,
			 cpu_offset, cpu_offset + cpus - 1);
	} else {
		snprintf(cputype, sizeof(cputype), "[CORE%u:CPU %u]", core,
			 cpu_offset);
	}
	time = fill_profilestat_lists(&entries, cpu_offset, cpus, 0);
	rprint(txtbuf, txtbuf_len,
	       "\n%s Measure time: %s  (cycles=%llu) with %u entries", cputype,
	       human_time(time, txt[0]), time, entries);
	if (time == 0) {
		return 0;
	}
	for (cpu = cpu_offset; cpu < (cpus + cpu_offset); cpu++) {
		for (i = 0; i < avm_profile_data_type_unknown; i++) {
			struct _profile_list_data_type_mapper *pdtm =
				&data_type_mapper[i];
			__u64 sum_consumption;

			if ((pdtm->profileheader == NULL) ||
			    (pdtm->name == NULL)) {
				continue;
			}
#if defined(DEBUG_PROFILE_STAT)
			print_valid_entries(
				pdtm->name, *pdtm->profileheader, cpu,
				((pdtm->operation & CODE_ENTRY) == CODE_ENTRY) ?
					1 :
					0);
#endif /*--- #if defined(DEBUG_PROFILE_STAT) ---*/
			if (i == avm_profile_data_type_code_address_info) {
				/*--- delete idle ---*/
				struct _profile_list_entry *pidle =
					profile_idle(*pdtm->profileheader, cpu);

				if (pidle) {
					idle_sum += pidle->sum_consumption;
					DBG_IDLE("cpu=%d: idle_sum=%llu\n", cpu,
						 idle_sum);
					pidle->last_btime = 0;
				}
			}
			sum_consumption =
				profile_consumption(*pdtm->profileheader, cpu);
			if (sum_consumption == 0) {
				continue;
			}
			if ((i ==
			     avm_profile_data_type_code_begin) || /* deprecated */
			    (i == avm_profile_data_type_sched)) {
				rprint(txtbuf, txtbuf_len,
				       "\n[CPU %x] ---------- %s ----------\n",
				       cpu, pdtm->name);
			} else {
				rprint(txtbuf, txtbuf_len,
				       "\n[CPU %x] ---------- %s needs %s (%s) of total time ----------\n",
				       cpu, pdtm->name,
				       percent(sum_consumption, time, txt[0]),
				       human_time(sum_consumption, txt[1]));
			}
			for (;;) {
				char buf[5][64];
				char tmp[KSYM_NAME_LEN * 2];
				char time[3][20];
				struct _profile_list_entry *pmax =
					profile_entry_with_max_sum_consumption(
						*pdtm->profileheader, cpu, 1);

				if (pmax == NULL) {
					break;
				}
				switch (i) {
				case avm_profile_data_type_hw_irq_begin:
					if (pmax->id ==
					    AVM_PROFILE_PAGE_FAULT_ID) {
						strcpy(buf[0],
						       "PAGE-FAULT:                               ");
					} else {
						snprintf(buf[0], sizeof(buf[0]),
							 "IRQ: %5u: %-30s",
							 pmax->id, "");
					}
					break;
				case avm_profile_data_type_exception_begin:
					snprintf(buf[0], sizeof(buf[0]),
						 "%-20s:",
						 exception_type(buf[1],
								sizeof(buf[1]),
								pmax->id));
					break;
				case avm_profile_data_type_code_address_info:
				case avm_profile_data_type_backtrace:
				case avm_profile_data_type_code_begin: /* deprecated */
				case avm_profile_data_type_sched:
					snprintf(buf[0], sizeof(buf[0]),
						 "%-23s(%5u)",
						 profile_find_comm_by_pid(
							 pmax->id, tmp,
							 sizeof(tmp), pmax),
						 pmax->id);
					break;
				default:
					snprintf(buf[0], sizeof(buf[0]),
						 "%-40s(%5u)",
						 profile_find_symbol_by_addr(
							 pmax->addr, NULL, NULL,
							 tmp),
						 pmax->id);
					break;
				}
				percent(pmax->sum_consumption, sum_consumption,
					buf[1]);
				snprintf(buf[2], sizeof(buf[2]),
					 "consume:max %s avg %s",
					 human_time(pmax->max_consumption,
						    time[0]),
					 human_time(
						 (__u32)(mylongdiv(
							 pmax->sum_consumption,
							 pmax->total_calls)),
						 time[1]));
				snprintf(
					buf[3], sizeof(buf[3]),
					"schedule:max %s min %s avg %s",
					human_time(pmax->max_triggertime,
						   time[0]),
					human_time(pmax->min_triggertime,
						   time[1]),
					human_time(
						(__u32)(mylongdiv(
							pmax->sum_triggertime,
							pmax->total_calls > 1 ?
								(pmax->total_calls -
								 1) :
								1)),
						time[2]));
				buf[4][0] = 0;
				if (full) {
					snprintf(buf[4], sizeof(buf[4]),
						 "access:%12u activate:%12u",
						 pmax->total_access,
						 pmax->total_activate);
				}
				rprint(txtbuf, txtbuf_len,
				       "%s%s Calls %6u %s %s%s%s\n", buf[0],
				       buf[1], pmax->total_calls, buf[2],
				       buf[3], buf[4][0] ? " " : "", buf[4]);
			}
		}
	}
	rprint(txtbuf, txtbuf_len, "\nCORE%u idle %s (%s) of total time\n",
	       core, human_time(idle_sum, txt[0]),
	       percent(idle_sum, time, txt[1]));

	free_profilestat_lists();
	return txtbuf - buf_start;
}
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/

#if !defined(__KERNEL__)
/**
 * SOF: Offline-testtool Offline-testtool Offline-testtool Offline-testtool Offline-testtool
 */
#define SKIP_CSV_ENTRY(p)                                                      \
	{                                                                      \
		while (*p && (*p != ';'))                                      \
			p++;                                                   \
		if (*p == ';')                                                 \
			p++;                                                   \
	}
/**
 */
static const char *_profile_find_comm_by_pid(pid_t pid, char *txtbuf, size_t txtbuf_size,
					     char *comm, size_t comm_size)
{
	struct _current_name *pcurr = current_name;

	while (pcurr->valid) {
		if (pid == pcurr->curr_pid) {
			snprintf(txtbuf, txtbuf_size, "%s", pcurr->current);
			return txtbuf;
		}
		pcurr++;
	}
	txtbuf[0] = 0;
	return txtbuf;
}

/**
 */
static const char *profile_find_symbol_by_addr(void *addr,
					       unsigned long *codesize,
					       unsigned long *offset, char *buf)
{
	static char tmpbuf[KSYM_NAME_LEN * 2];
	struct _symbols_name *psym;
	unsigned int _addr = (unsigned long)(addr)&0xFFFFFFFF;

	if (buf == NULL) {
		buf = tmpbuf;
	}
	psym = gSymbolname;
	DBG_CSV("%s: addr=%x\n", __func__, _addr);
	while (psym->valid) {
		if ((_addr >= psym->fn_addr) &&
		    (_addr < (psym->fn_addr + psym->size))) {
			unsigned int of = _addr - psym->fn_addr;

			if (psym->valid == 1) {
				sprintf(buf, "%s+0x%x/%u%s%s%s",
					psym->name ? psym->name : "", of,
					psym->size, psym->module[0] ? " [" : "",
					psym->module,
					psym->module[0] ? "]" : "");
			} else {
				sprintf(buf, "seg=%u of=0x%x/%u [%s]",
					psym->valid - 2, of, psym->size,
					psym->name ? psym->name : "");
			}
			if (offset)
				*offset = of;
			if (codesize)
				*codesize = psym->size;
			DBG_CSV("%s: entry found: %s addr=%x fn_addr=%x size=%u\n",
				__func__, psym->name, _addr, psym->fn_addr,
				psym->size);
			return buf;
		}
		psym++;
	}
	sprintf(buf, "%p", addr);
	/*--- buf[0] = 0; ---*/
	return buf;
}

/**
 * Format:
 * <func>+<offset>/<size> [<module>]
 * oder:
 * seg=<x> of=<offset> [<lib/exe>]
 */
static void profile_add_symbol_by_addr(void *addr, char *txt)
{
	struct _symbols_name *psym;
	char *seg, *ps, *symbol = txt;
	unsigned int txtlen, offset = 0, codelength = 0, mseg;
	unsigned int _addr = (unsigned long)(addr) & 0xFFFFFFFF;

	if (addr == 0) {
		return;
	}
	psym = gSymbolname;
	while (psym->valid) {
		if ((_addr >= psym->fn_addr) &&
		    (_addr < (psym->fn_addr + psym->size))) {
			DBG_CSV("\t->%s: entry found: %s%u %s addr=%x (%x - %x) size=%u\n",
				__func__, psym->valid != 1 ? "seg=" : "kseg",
				psym->valid > 1 ? psym->valid - 2 : 0,
				psym->name, _addr, psym->fn_addr,
				psym->fn_addr + psym->size, psym->size);
			return;
		}
		psym++;
	}
	ps = txt;
	SKIP_CSV_ENTRY(ps);
	seg = strstr(txt, "seg=");
	if (seg && (ps > seg)) {
		seg += sizeof("seg=") - 1;
		sscanf(seg, "%u", &mseg);
		/*--- Userland-Adresse ---*/
		txt = strstr(txt, "of=");
		if (txt) {
			txt += sizeof("of=") - 1;
			sscanf(txt, "0x%x", &offset);
			while (*txt && (*txt != '/') && (*txt != ';'))
				txt++;
			if (*txt == '/') {
				sscanf(&txt[1], "0x%x", &codelength);
			}
			while (*txt && (*txt != '[') && (*txt != ';'))
				txt++;
			txtlen = 0;
			if (*txt == '[') {
				symbol = ++txt;
				while (*txt && (*txt != ']') && (*txt != ';'))
					txt++;
				txtlen = min(sizeof(psym->name), txt - symbol);
				memcpy(psym->name, symbol, txtlen);
			}
			psym->name[txtlen] = 0;
			psym->size = codelength;
			psym->valid = mseg + 2; /*--- User ---*/
			psym->fn_addr = _addr - offset;
			DBG_CSV("\t->%s: add User: addr=%x seg=%u of=%u [%s] 0x%x/%x\n",
				__func__, _addr, psym->valid - 2, offset,
				psym->name, psym->fn_addr, psym->size);
		}
	} else {
		/*--- DBG_CSV("\t->%s: Kernel: %s\n", __func__, txt);  ---*/
		/*--- Kernel-Adresse ---*/
		while (*txt && (*txt != '+') && (*txt != ';'))
			txt++;
		if (*txt == ';') {
			/*--- keine Symbolaufloesung ---*/
			return;
		}
		txtlen = min(sizeof(psym->name), txt - symbol);
		memcpy(psym->name, symbol, txtlen);
		psym->name[txtlen] = 0;
		if (*txt == '+') {
			sscanf(&txt[1], "0x%x", &offset);
		}
		while (*txt && (*txt != '/'))
			txt++;
		if (*txt == '/') {
			sscanf(&txt[1], "0x%x", &codelength);
		}
		while (*txt && (*txt != '[') && (*txt != ';'))
			txt++;
		txtlen = 0;
		if (*txt == '[') {
			symbol = ++txt;
			while (*txt && (*txt != ']') && (*txt != ';'))
				txt++;
			txtlen = min(sizeof(psym->module), txt - symbol);
			memcpy(psym->module, symbol, txtlen);
		}
		psym->module[txtlen] = 0;
		psym->size = codelength;
		psym->fn_addr = _addr - offset;
		if (codelength) {
			psym->valid = 1;
			DBG_CSV("\t->%s: add Kernel: %s (%s) 0x%x/%x\n",
				__func__, psym->name, psym->module,
				psym->fn_addr, psym->size);
		}
	}
}

/**
 */
static struct _current_name *profile_add_curr(struct _current_name *pcurr,
					      char *curr, int pid)
{
	while (pcurr && (pcurr->valid == 1)) {
		if ((pcurr->curr_pid == pid) && pcurr->current[0]) {
			return pcurr;
		}
		pcurr++;
	}
	DBG_CSV("\t->%s '%s' new entry %d\n", __func__, curr, pid);
	strcpy(pcurr->current, curr);
	pcurr->curr_pid = pid;
	pcurr->valid = 1;
	return pcurr;
}

/**
 */
static int fill_type_from_csv(char *string, struct _avm_profile_data *pdata,
			      struct _symbols_name *psym,
			      struct _current_name *pcurr)
{
	unsigned int i;
	unsigned int cpu_id, time, total_access, total_activate, data_id,
		stack_pos = 0, tc_id = 0x7, core_id = 0;
	unsigned long data_addr;
	char *p = string;

	sscanf(p, "%x", &cpu_id);
	SKIP_CSV_ENTRY(p);
	if (*p == 'C') {
		p++;
		sscanf(p, "%x", &core_id);
		SKIP_CSV_ENTRY(p);
	}
	if (*p == 'T') {
		p++;
		sscanf(p, "%x", &tc_id);
		SKIP_CSV_ENTRY(p);
	}
	if (*p == 'S') {
		p++;
		sscanf(p, "0x%x", &stack_pos);
		SKIP_CSV_ENTRY(p);
	}
	sscanf(p, "0x%x", &time);
	SKIP_CSV_ENTRY(p);
	sscanf(p, "0x%x", &total_access);
	SKIP_CSV_ENTRY(p);
	sscanf(p, "0x%x", &total_activate);
	SKIP_CSV_ENTRY(p);

	DBG_CSV("'%s'\t->core_id=%x -tc_id=%x stack_pos=%x\n", string, core_id,
		tc_id, stack_pos);
	for (i = 0; i < avm_profile_data_type_unknown; i++) {
		if (avm_profile_data_short_names[i] &&
		    (strstr(p, avm_profile_data_short_names[i]) == p)) {
			char curr[TASK_COMM_LEN];
			char *ps;

			pdata->cpu_id = cpu_id;
			pdata->core_id = core_id;
			pdata->tc_id = tc_id;
			pdata->stack_pos = stack_pos;
			pdata->time = time;
			pdata->total_access = total_access;
			pdata->total_activate = total_activate;
			pdata->type = i;
			SKIP_CSV_ENTRY(p);
			sscanf(p, "0x%lx", &data_addr);
			SKIP_CSV_ENTRY(p);
			profile_add_symbol_by_addr((void *)data_addr, p);
			SKIP_CSV_ENTRY(p);
			pdata->addr = data_addr;

			/*--- der LR ---*/
			sscanf(p, "0x%lx", &data_addr);
			SKIP_CSV_ENTRY(p);
			profile_add_symbol_by_addr((void *)data_addr, p);
			SKIP_CSV_ENTRY(p);
			pdata->lr = data_addr;
			DBG_CSV("\t->type=0x%x (%s) addr=0x%08lx lr=0x%08lx\n",
				i, avm_profile_data_short_names[i], pdata->addr,
				pdata->lr);

			ps = p;
			SKIP_CSV_ENTRY(ps);
			snprintf(curr, TASK_COMM_LEN, "%.*s",
				 (unsigned int)(ps - p > 0 ? ps - p - 1 : 0),
				 p);

			sscanf(ps, "%d", &data_id);
			pdata->id = (unsigned short)data_id;

			if ((i == avm_profile_data_type_code_address_info) ||
			    (i == avm_profile_data_type_backtrace) ||
			    (i == avm_profile_data_type_code_begin) ||
			    (i == avm_profile_data_type_code_end ||
			    (i == avm_profile_data_type_sched))) {
				profile_add_curr(pcurr, curr, data_id);
			}
			return 0;
		}
	}
	return 1;
}

struct _cpucore_profile cpu_config[NR_CPUS + 1];

/**
 */
int parse_cpu_config(char *txt)
{
	char *ptxt, *p;
	unsigned int i, realcores = 0;

	p = strstr(txt, "real cpu-cores");
	if (p) {
		p += sizeof("real cpu-cores") - 1;
		sscanf(p, "%u", &realcores);
	}
	if (realcores > num_possible_cpus()) {
		fprintf(stderr, "to much real cores %d\n", realcores);
		return -1;
	}
	ptxt = p;
	for (i = 0; i < realcores; i++) {
		char config_entry[16];

		sprintf(config_entry, "[%u] cpu_ofs ", i);
		p = strstr(ptxt, config_entry);
		if (p) {
			p += strlen(config_entry);
			sscanf(p, "%u vpes %u", &cpu_config[i].cpu_nr_offset,
			       &cpu_config[i].vpe_nr);
		}
	}
	return 0;
}

/**
 */
static unsigned int parse_profile_csv(FILE *fp,
				      struct _avm_profile_data **ap_data,
				      struct _current_name **current,
				      unsigned int *pmaxcpus)
{
	struct _avm_profile_data *pdata;
	struct _symbols_name *psym;
	struct _current_name *pcurr;
	unsigned int lines = 0, actline = 0, entries = 0, messure_time = 0;
	__u64 cycsum = 0;
	__u32 last_cycle = 0;

	char txt[1024], *p;

	for (;;) {
		if (fgets(txt, sizeof(txt), fp) == NULL) {
			break;
		}
		if (messure_time == 0) {
			char *p;

			p = strstr(txt, "measure time ");
			if (p) {
				p += sizeof("measure time ") - 1;
				sscanf(p, "%u", &messure_time);
				if (parse_cpu_config(p)) {
					return 0;
				}
			}
		}
		lines++;
	}
	DBG_TRC("%d\n", lines);
	fseek(fp, 0, SEEK_SET);
	pdata = malloc((sizeof(struct _avm_profile_data)) * lines);
	memset(pdata, 0x55, sizeof(struct _avm_profile_data) * lines);
	psym = malloc((sizeof(struct _symbols_name)) * lines * 2);
	memset(psym, 0, (sizeof(struct _symbols_name)) * lines);
	pcurr = malloc((sizeof(struct _current_name)) * lines);
	memset(pcurr, 0, (sizeof(struct _current_name)) * lines);
	*ap_data = pdata;
	*current = pcurr;
	pdata = data;
	gSymbolname = psym;
	*pmaxcpus = 0;

	for (;;) {
		p = fgets(txt, sizeof(txt), fp);
		if (p == NULL) {
			break;
		}
		actline++;
		/*--- DBG_TRC("%8d:   '%s'", actline, p); ---*/
		if (fill_type_from_csv(p, pdata, psym, pcurr) == 0) {
#if 0
			DBG_TRC(" parsed:    %x;0x%08X;0x%08X;0x%08X;%s;0x%08X;%s;%d\n\n", pdata->cpu_id, pdata->time, pdata->total_access, pdata->total_activate,
				avm_profile_data_short_names[pdata->type], pdata->addr, psym->name, pdata->id);
#endif
			if (pdata->cpu_id >= num_possible_cpus()) {
				printf("warning: line %u cpu_id %x > reserved NR_CPUS(%u) -> ignore\n",
				       actline, pdata->cpu_id,
				       num_possible_cpus());
				continue;
			}
			if (pdata->cpu_id > *pmaxcpus) {
				printf("line %u set max cpu_id %x\n", actline,
				       pdata->cpu_id);
				*pmaxcpus = pdata->cpu_id;
			}
			if (last_cycle) {
				if ((pdata->time - last_cycle) < INT_MAX / 2) {
					cycsum += (__u32)(pdata->time -
							  last_cycle);
				}
			}
			last_cycle = pdata->time;
			pdata++, psym += 2, entries++;
		}
	}
	if (cycsum && messure_time) {
		gCycle_per_usec = mylongdiv(cycsum, messure_time) / 1000;
		printf("cycle per usec %lu\n", gCycle_per_usec);
	}
	*pmaxcpus = *pmaxcpus + 1;
	return entries;
}

char *gCsvInfile;
char *gOutfile;
int gHelp, gStatistic, gTimelineCycles, gTimelineuSec, gKernelstatistic, gCsv,
	gExcludePerf, gFreq;
int gKernelstatisticVal = 3, gKSWeight = 1;
int gCpu = -1;
int gExcludeVPE;
int gNoCpuInfo;

/**
 */
// clang-format off
static struct _cmdlinetab {
	char *prefix;
	enum {
		string,
		integer,
		flag
	} type;
	void *value;
	char *help;
} cmdline[] = {
	{ .prefix = "-i",		.type = string,		.value = &gCsvInfile,		.help = "infile (csv)"},
	{ .prefix = "-o",		.type = string,		.value = &gOutfile,		.help = "outfile"},
	{ .prefix = "-s",		.type = flag,		.value = &gStatistic,		.help = "statistic"},
	{ .prefix = "-k",		.type = flag,		.value = &gKernelstatistic,	.help = "kernel-statistic "},
	{ .prefix = "--kweigth",	.type = integer,	.value = &gKSWeight,		.help = "weighted kernelstatistic: calls/code_length^weigth"},
	{ .prefix = "-t",		.type = flag,		.value = &gTimelineuSec,	.help = "timeline in us"},
	{ .prefix = "--nocpuinfo",	.type = flag,		.value = &gNoCpuInfo,		.help = "no cpuinfo on timeline"},
	{ .prefix = "--timeusec",	.type = flag,		.value = &gTimelineuSec,	.help = ""},
	{ .prefix = "-T",		.type = flag,		.value = &gTimelineCycles,	.help = "timeline in cycles"},
	{ .prefix = "--timecycles",	.type = flag,		.value = &gTimelineCycles,	.help = ""},
	{ .prefix = "--csv",		.type = flag,		.value = &gCsv,			.help = "timeline: csv output format"},
	{ .prefix = "--noperf",		.type = flag,		.value = &gExcludePerf,		.help = "statistic: exclude performance-counter"},
	{ .prefix = "--cpu",		.type = integer,	.value = &gCpu,			.help = "timeline/nonvpe: select cpu (default: all)"},
	{ .prefix = "--novpe",		.type = flag,		.value = &gExcludeVPE,		.help = "multicore: no vpe-architecture"},
	{ .prefix = "--freq",		.type = integer,	.value = &gFreq,		.help = "Counter-freq in MHz"},
	{ .prefix = "-h",		.type = flag,		.value = &gHelp,		.help = "this helptext"},
	{ .prefix = "--help",		.type = flag,		.value = &gHelp,		.help = ""},
	{ .prefix = NULL,		.type = string,		.value = NULL },
};
// clang-format on

/**
 */
static struct _cmdlinetab *find_prefix(char **arg)
{
	struct _cmdlinetab *pcmd = cmdline;

	while (pcmd->prefix) {
		if (strncmp(pcmd->prefix, *arg, strlen(pcmd->prefix))) {
			pcmd++;
			continue;
		}
		*arg += strlen(pcmd->prefix);
		return pcmd;
	}
	return NULL;
}

/**
 */
static void print_help(char *name)
{
	struct _cmdlinetab *pcmd = cmdline;

	fprintf(stderr, "%s -i infile -s ...\n", name);
	while (pcmd->prefix) {
		fprintf(stderr, "\t%-12s %7s  %s\n", pcmd->prefix,
			(pcmd->type != flag) ? "<param>" : "", pcmd->help);
		pcmd++;
	}
}

/**
 */
static int parse_cmdline(int argc, char *argv[])
{
	struct _cmdlinetab *pcmd;
	int idx = 1;

	argc--;
	while (argc) {
		char *ptmp;
		char *p = argv[idx++];

		argc--;
		/*--- printf("argv[%d]=%s\n", idx, argv[idx]); ---*/
		ptmp = p;
		switch (*p) {
		case '-':
			pcmd = find_prefix(&p);
			if (pcmd == NULL) {
				fprintf(stderr, "%s: unknown option '%s'\n",
					argv[0], ptmp);
				return -1;
			}
			if (pcmd->type == flag) {
				if (*p) {
					fprintf(stderr,
						"%s: invalid rest on flag '%s'\n",
						argv[0], ptmp);
					return -1;
				}
				*((int *)pcmd->value) = 1;
				break;
			}
			if (*p == 0) {
				if (argc == 0) {
					fprintf(stderr,
						"%s: missing parameter '%s'\n",
						argv[0], ptmp);
					return -1;
				}
				p = argv[idx++];
				argc--;
			}
			if (pcmd->type == string) {
				*((char **)pcmd->value) = p;
			} else {
				sscanf(p, "%d", (unsigned int *)pcmd->value);
			}
			break;
		default:
			fprintf(stderr, "%s: unknown option '%s'\n", argv[0],
				ptmp);
			return -1;
		}
	}
	return 0;
}

/**
 */
int main(int argc, char *argv[0])
{
	FILE *fin = stdin;
	unsigned int i, startcpu = 0, maxcpus, formatflag = 0;

	fout = stdout;
	if (parse_cmdline(argc, argv)) {
		print_help(argv[0]);
		return -1;
	}
	if (gHelp) {
		print_help(argv[0]);
		return 0;
	}
	if (gTimelineCycles && gTimelineuSec) {
		fprintf(stderr, "%s: conflict of parameter\n", argv[0]);
		return -1;
	}
	if (gCsvInfile) {
		fin = fopen(gCsvInfile, "r");
	}
	if (fin == NULL) {
		fprintf(stderr, "%s: unable to open input file %s\n", argv[0],
			gCsvInfile);
		return -errno;
	}
	if (gOutfile) {
		fout = fopen(gOutfile, "w");
	}
	if (fout == NULL) {
		fprintf(stderr, "%s: unable to open outputfile %s\n", argv[0],
			gOutfile);
		return -errno;
	}
	if (gFreq) {
		gCycle_per_usec = gFreq;
	}
	avm_profile_data_entries =
		parse_profile_csv(fin, &data, &current_name, &maxcpus);
	printf("entries=%u PROFILE_LIST_ENTRIES=%lu size=%lu last CPU_NR=%d\n",
	       avm_profile_data_entries, PROFILE_LIST_ENTRIES,
	       sizeof(struct _profile_list_entry), maxcpus - 1);
	if (gCpu != -1) {
		startcpu = gCpu;
		maxcpus = 1;
	}

	if (cpu_config[0].vpe_nr == 0) {
		/*--- old format without any informations about cpu-platform ---*/
		if (maxcpus == 1) {
			cpu_config[0].vpe_nr = 1;
		} else if (gExcludeVPE) {
			for (i = 0; i < maxcpus; i++) {
				cpu_config[i].vpe_nr = 1;
				cpu_config[i].cpu_nr_offset = i;
			}
		} else {
			cpu_config[0].vpe_nr = maxcpus;
		}
	}
	if (gKSWeight > 3) {
		gKSWeight = 3;
	}
	if (gKernelstatistic) {
#if 0
		i = 0;
		while (cpu_config[i].vpe_nr) {
			/*--- printf("CORE%u: (CPU%u-CPU%u)\n", i, cpu_config[i].cpu_nr_offset, cpu_config[i].cpu_nr_offset + cpu_config[i].vpe_nr - 1); ---*/
			profilestat_totalcall(NULL, 0, i, cpu_config[i].cpu_nr_offset, cpu_config[i].vpe_nr, gKSWeight);
			i++;
		}
#endif
		profilestat_totalcall(NULL, 0, -1, 0, maxcpus, gKSWeight);
	}
	if (gStatistic) {
		i = 0;
		while (cpu_config[i].vpe_nr) {
			/*--- printf("CORE%u: (CPU%u-CPU%u)\n", i, cpu_config[i].cpu_nr_offset, cpu_config[i].cpu_nr_offset + cpu_config[i].vpe_nr - 1); ---*/
			profilestat_category(NULL, 0, i,
					     cpu_config[i].cpu_nr_offset,
					     cpu_config[i].vpe_nr,
					     !gExcludePerf);
			i++;
		}
	}
	formatflag = gTimelineCycles ? 0x1 : gTimelineuSec ? 0x3 : 0;
	formatflag |= gCsv ? 0x0 : 0x4;
	formatflag |= gNoCpuInfo ? 0x8 : 0x0;
	if (formatflag & 0x1) {
		fill_profilestat_lists(NULL, startcpu, maxcpus, formatflag);
		free_profilestat_lists();
	}
	/*--- printf("pages_cnt=%d\n", pages_cnt); ---*/
	return 0;
}

/**
 * EOF: Offline-testtool Offline-testtool Offline-testtool Offline-testtool Offline-testtool
 */
#endif /*--- #if !defined(__KERNEL__) ---*/
