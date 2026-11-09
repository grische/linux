// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2018-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/mm.h>
#endif
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/oom.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>

#include <avm/compat/misc.h>
#include <avm/rte/rte.h>
#include <avm/enh/reboot_status.h>
#include <avm/sammel/debug.h>
#include <avm/sammel/simple_proc.h>
#include <avm/sammel/helpers.h>
#include <avm/sammel/page-statistics.h>
#include <avm/enh/semi_seq.h>
#include "avm_sammel.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
#define show_mem() show_mem(0)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)
#define show_mem() show_mem(0, NULL)
#endif

static int oom_notify(struct notifier_block *self, unsigned long dummy,
		      void *param);
extern void show_slab(void) __attribute__((weak));
static unsigned long oom_show_jiffies;

ATOMIC_NOTIFIER_HEAD(oom_info_chain);

int avm_oom_info_chain_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&oom_info_chain, nb);
}
EXPORT_SYMBOL(avm_oom_info_chain_register);

int avm_oom_info_chain_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&oom_info_chain, nb);
}
EXPORT_SYMBOL(avm_oom_info_chain_unregister);

static struct notifier_block oom_nb = {
	.notifier_call = oom_notify,
	.priority = 0,
};

/**
 * @brief OOM-Retry uesfull on boxes with less user-applications
 * - OOM-Count will be reseted, is 100 s no OOM occur
 * - if OOM-Count = 5: reboot
 * @return 0
 */
static unsigned int oom_retry(void)
{
	static unsigned int count_oom, oom_jiffies;
	unsigned int dt;

	dt = (jiffies - oom_jiffies);
	if (dt > (100 * CONFIG_HZ)) {
		oom_jiffies = jiffies;
		count_oom = 1;
		pr_emerg("\nERROR: OOM [#%u] retry\n", count_oom);
		show_avm_page_statistic(false);
		return 1;
	}
	if (count_oom++ >= 5) {
		/*--- forget it  ---*/
		return 0;
	}
	pr_emerg("\nERROR: OOM [#%u] retry\n", count_oom);
	return 1;
}

/**
 */
static int oom_notify(struct notifier_block *self,
		      unsigned long dummy, void *param)
{
	unsigned long *freed = param;

	if (oom_retry()) {
		*freed = 1;
		schedule();
		avm_write_to_file("/proc/sys/vm/drop_caches", "3", 1);
		return NOTIFY_OK;
	}
	*freed = 0;
	if (sysctl_panic_on_oom ||
	    ((jiffies - oom_show_jiffies) >= 10UL * HZ)) {
		oom_show_jiffies = jiffies | 1;
		mb();
		if (sysctl_panic_on_oom) {
			bust_spinlocks(1);
			avm_set_reset_status(RS_OOM);
			avm_stack_check(NULL);
#if defined(CONFIG_AVM_WATCHDOG)
			set_watchdog_in_progress();
#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/
			console_verbose();
			avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE | AVM_OOM_MEMSTAT_ALL);
			/*--- make panic here - because oom-kernel infos obsolete ---*/
			panic("Out of memory: panic_on_oom is enabled\n");
		}
	}
	return NOTIFY_OK;
}

/**
 * @brief mmput() with none-forced might_sleep()
 * it's dirty but mmput() is also called from timer or nmi-context
 * so have to prevent might_sleep() if not necessary
 */
void mmput_avm_context(struct mm_struct *mm)
{
	if (unlikely(atomic_dec_and_test(&mm->mm_users))) {
		/*--- we hold the last reference: cleanup necessary ---*/
		atomic_inc(&mm->mm_users);
		mmput(mm);
	}
}

static int rte_tasklist_read_trylock(void)
{
	if (!rte_rcu_read_lock_is_unsafe())
		return read_trylock(&tasklist_lock);
	rcu_read_lock();
	return 1;
}

static void rte_tasklist_read_unlock(void)
{
	if (!rte_rcu_read_lock_is_unsafe())
		read_unlock(&tasklist_lock);
	else
		rcu_read_unlock();
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
#define mm_data_vm(mm) ((mm)->total_vm - (mm)->shared_vm - (mm)->stack_vm)
#else
#define mm_data_vm(mm) ((mm)->data_vm)
#endif

#define STATS_HEADER_RSS  "------------RSS------------"
#define STATS_HEADER_VM   "----------------VM----------------"
#ifdef AVM_ENH_SWAP_STATS
/* Long version with space for 'total', 'in', 'out' */
#define STATS_HEADER_SWAP "--------SWAP--------"
#else
/* Short version with only space for 'swap' */
#define STATS_HEADER_SWAP "-SWAP-"
#endif

static void show_task_mem_stats_header(struct seq_file *file)
{
	sseq_printf(file, "Task-Memory in KiB:\n");

	/* General, RSS, VM, SWAP */
	sseq_printf(file, "%12s ", "");
	sseq_printf(file, "%s ", STATS_HEADER_RSS);
	sseq_printf(file, "%s ", STATS_HEADER_VM);
#ifdef CONFIG_SWAP
	sseq_printf(file, "%s ", STATS_HEADER_SWAP);
#endif
	sseq_printf(file, "\n");

	/* General */
	sseq_printf(file, "%6s %5s ", "pid", "score");

	/* RSS */
	sseq_printf(file, "%6s %6s %6s %6s ",
		    "total", "anon", "file", "shmem");
	/* VM */
	sseq_printf(file, "%6s %6s %6s %6s %6s ",
		    "total", "code", "lib", "data", "stack");
	/* SWAP */
#ifdef CONFIG_SWAP
#ifdef AVM_ENH_SWAP_STATS
	sseq_printf(file, "%6s %6s %6s ", "total", "in", "out");
#else
	sseq_printf(file, "%6s ", "swap");
#endif
#endif
	sseq_printf(file, "name\n");
}

#define pages2kib(p) ((p) << (PAGE_SHIFT - 10))
static void show_task_mem_stats(struct seq_file *file, struct task_struct *task,
				struct mm_struct *mm)
{
	unsigned long code = PFN_UP(mm->end_code) - PFN_DOWN(mm->start_code);
	unsigned long total_rss, anon_rss, file_rss, shmem_rss = 0;

	code = min(code, mm->exec_vm);

	anon_rss  = pages2kib(get_mm_counter(mm, MM_ANONPAGES));
	file_rss  = pages2kib(get_mm_counter(mm, MM_FILEPAGES));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	shmem_rss = pages2kib(get_mm_counter(mm, MM_SHMEMPAGES));
#endif
	total_rss = anon_rss + file_rss + shmem_rss;

	/* General: pid, score */
	sseq_printf(file, "%6d ", task->pid);
	sseq_printf(file, "%5lu ", oom_score(task));

	/* RSS: total, anon, file, shmem */
	sseq_printf(file, "%6lu %6lu %6lu %6lu ",
		    total_rss, anon_rss, file_rss, shmem_rss);

	/* VM: total, code, lib, data, stack */
	sseq_printf(file, "%6lu ", pages2kib(mm->total_vm));
	sseq_printf(file, "%6lu ", pages2kib(code));
	sseq_printf(file, "%6lu ", pages2kib(mm->exec_vm - code));
	sseq_printf(file, "%6lu ", pages2kib(mm_data_vm(mm)));
	sseq_printf(file, "%6lu ", pages2kib(mm->stack_vm));

	/* SWAP: swap/total, in, out */
#ifdef CONFIG_SWAP
	sseq_printf(file, "%6lu ", pages2kib(get_mm_counter(mm, MM_SWAPENTS)));
#ifdef AVM_ENH_SWAP_STATS
	sseq_printf(file, "%6lu ", get_mm_counter(mm, MM_SWAPINS));
	sseq_printf(file, "%6lu ", get_mm_counter(mm, MM_SWAPOUTS));
#endif
#endif
	sseq_printf(file, "{%s}\n", task->comm);
}
#undef pages2kib

/**
 * nicht komplett kontextfest:
 * Probleme wenn Aufruf aus non-kthread-context (irq, nmi, fast-irq, yield):
 * (a) wenn parallel auf andere CPU ein vorletztes mmput gemacht wird, so muss aktueller Kontext mm wegräumen
 *		-> might_sleep()-BUG
 */
static int dump_task_memory(struct notifier_block *block,
			    unsigned long event, void *_data)
{
	unsigned long no_mm = 0, child = 0;
	struct seq_file *file = _data;
	struct task_struct *g, *task;
	struct mm_struct *mm;

	if (!rte_tasklist_read_trylock())
		return NOTIFY_DONE;

	show_task_mem_stats_header(file);
	for_each_process_thread(g, task) {
		mm = get_task_mm(task);
		if (mm && thread_group_leader(task))
			show_task_mem_stats(file, task, mm);

		if (!thread_group_leader(task))
			child++;

		if (mm)
			mmput_avm_context(mm);
		else
			no_mm++;
	}

	sseq_printf(file, "kthreads %lu childs %lu\n", no_mm, child);
	rte_tasklist_read_unlock();

	return NOTIFY_DONE;
}

static struct notifier_block dump_task_memory_block = {
	.notifier_call = dump_task_memory
};

/**
 */
static int watermark_under_low(void)
{
	struct zone *zone;

	for_each_populated_zone(zone) {
		if (zone_page_state(zone, NR_FREE_PAGES) <
		    low_wmark_pages(zone)) {
			return 1;
		}
	}
	return 0;
}

static unsigned long total_slab_pages(void)
{
	unsigned long sum;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	sum = global_node_page_state_pages(NR_SLAB_RECLAIMABLE_B) +
	      global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	sum = global_node_page_state(NR_SLAB_RECLAIMABLE) +
	      global_node_page_state(NR_SLAB_UNRECLAIMABLE);
#else
	sum = global_page_state(NR_SLAB_RECLAIMABLE) +
	      global_page_state(NR_SLAB_UNRECLAIMABLE);
#endif

	return sum;
}

/**
 */
static int critical_slabmem_consume(unsigned int slab_pages,
				    unsigned int total_ram_pages)
{
	if (total_ram_pages < 16000) {
		/*--- 64 MByte ---*/
		return slab_pages > total_ram_pages / 5;
	}
	return slab_pages > total_ram_pages / 2;
}

/**
 * @brief show memory-info
 * @param force 0x0 depend on memory-situation
 *              ored with 0x1: print in crash or die-Mode (print only one time)
 *              ored with 0x2 print all infos
 */
void avm_oom_show_memstat(unsigned int force)
{
	static atomic_t print_one_time = ATOMIC_INIT(0);
	unsigned long slab_pages = 0;
	int low = 1;

	if (atomic_add_return(!!(force & AVM_OOM_MEMSTAT_ONCE), &print_one_time) > 1) {
		return;
	}

	if (!(force & AVM_OOM_MEMSTAT_ALL)) {
		slab_pages = total_slab_pages();
		low = watermark_under_low();
	}

	/*--- pr_err("%s %u: slab_pages=%lu totalram_pages=%lu\n",  __func__, __LINE__, slab_pages, totalram_pages); ---*/
	if (low || critical_slabmem_consume(slab_pages, totalram_pages())) {
		DEFINE_SEMI_SEQ(ssseq, KERN_ERR);

		pr_debug("-------------------- memory-situation --------------------\n");
		show_mem();

		atomic_notifier_call_chain(&oom_info_chain, 0, ssseq_to_seq(ssseq));
		if (!IS_ERR_OR_NULL(&show_slab)) {
			show_slab();
		}
	}
}

static __init int avm_oom_init(void)
{
	avm_oom_info_chain_register(&dump_task_memory_block);
	register_oom_notifier(&oom_nb);

	return 0;
}
arch_initcall(avm_oom_init);

#if defined(CONFIG_PROC_FS)

static void avm_proc_mem_summary(struct seq_file *file, void *priv)
{
	atomic_notifier_call_chain(&oom_info_chain, 0, file);
}

static void avm_proc_mem_tasks(struct seq_file *file, void *priv)
{
	dump_task_memory(NULL, 0, file);
}

static __init int avm_oom_lateinit(void)
{
	proc_mkdir("avm/mem", NULL);
	add_simple_proc_file("avm/mem/summary", NULL, avm_proc_mem_summary,
			     NULL);
	add_simple_proc_file("avm/mem/tasks", NULL, avm_proc_mem_tasks, NULL);
	return 0;
}
late_initcall(avm_oom_lateinit);
#endif /* defined(CONFIG_PROC_FS) */
