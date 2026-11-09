// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2018-2019 AVM GmbH <fritzbox_info@avm.de> */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/oom.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/proc_fs.h>
#pragma GCC pop_options

#include <avm/enh/reboot_status.h>
#include <avm/sammel/debug.h>
#include <avm/sammel/simple_proc.h>
#include <avm/sammel/helpers.h>
#include <avm/enh/semi_seq.h>
#include "avm_sammel.h"

static int oom_notify(struct notifier_block *self, unsigned long dummy,
		      void *param);
extern void show_slab(void) __attribute__((weak));
extern int sysctl_panic_on_oom;
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

#if defined(TAINT_ALLOC_FAIL)
static int panic_notify(struct notifier_block *self, unsigned long dummy,
			void *param);

static struct notifier_block panic_nb = {
	.notifier_call = panic_notify,
	.priority = 0,
};
#endif

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
		show_avm_page_statistic(0);
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
#if defined(TAINT_ALLOC_FAIL)
			// This should probbably be set at this point, but force it
			// to always print the memstat
			add_taint(TAINT_ALLOC_FAIL, LOCKDEP_STILL_OK);
#endif
			avm_set_reset_status(RS_OOM);
			avm_stack_check(NULL);
#if defined(CONFIG_AVM_WATCHDOG)
			set_watchdog_in_progress();
#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/
			console_verbose();
#if !defined(TAINT_ALLOC_FAIL)
			avm_oom_show_memstat(0x1 | 0x2);
#endif
			/*--- make panic here - because oom-kernel infos obsolete ---*/
			panic("Out of memory: panic_on_oom is enabled\n");
		}
	}
	return NOTIFY_OK;
}

#if defined(TAINT_ALLOC_FAIL)
static int panic_notify(struct notifier_block *self,
			unsigned long dummy,
			void *param)
{
	if (test_taint(TAINT_ALLOC_FAIL)) {
		avm_oom_show_memstat(0x1 | 0x2);
	}

	return NOTIFY_OK;
}
#endif

#define PSS_SHIFT 12

struct real_mem_info {
	u64 total;
	u64 anon;
	u64 file;
};

struct walk_private {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	struct vm_area_struct *vma;
#endif
	struct real_mem_info mem_info;
};

#if KERNEL_VERSION(4, 0, 0) > LINUX_VERSION_CODE
#define WALK_GET_VMA(w) (((struct walk_private *)(w)->private)->vma)
#else
#define WALK_GET_VMA(w) w->vma
#endif

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

static void walk_pte_entry(pte_t *pte, unsigned long addr, struct mm_walk *walk)
{
	struct walk_private *data = walk->private;
	struct vm_area_struct *vma = WALK_GET_VMA(walk);
	struct page *page = NULL;
	int mapcount;
	u64 pss_delta = (u64)PAGE_SIZE << PSS_SHIFT;

	if (pte_present(*pte)) {
		page = vm_normal_page(vma, addr, *pte);
	} else if (is_swap_pte(*pte)) {
		swp_entry_t swpent = pte_to_swp_entry(*pte);

		if (is_migration_entry(swpent))
			page = migration_entry_to_page(swpent);
	}

	if (!page)
		return;

	mapcount = page_mapcount(page);
	if (mapcount >= 2)
		do_div(pss_delta, mapcount);

	if (vma->vm_file)
		data->mem_info.file += pss_delta;
	else
		data->mem_info.anon += pss_delta;
	data->mem_info.total += pss_delta;
}

static int walk_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end,
			  struct mm_walk *walk)
{
	struct vm_area_struct *vma = WALK_GET_VMA(walk);
	pte_t *pte;
	spinlock_t *ptl;
	int ret = 0;

	// pmd_trans_unstable was only introduced with HUGPAGE support in 3.4.0
	// therfore before this version, we can just assume that this is never
	// unstable.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	if (pmd_trans_unstable(pmd))
		return 0;
#endif
	ptl = pte_lockptr(vma->vm_mm, pmd);
	pte = pte_offset_map(pmd, addr);

	if (!spin_trylock(ptl)) {
		ret = -EBUSY;
		goto unmap;
	}

	for (; addr != end; pte++, addr += PAGE_SIZE)
		walk_pte_entry(pte, addr, walk);

	spin_unlock(ptl);

unmap:
	pte_unmap(pte - 1);

	return ret;
}

static int collect_real_mem_info(struct mm_struct *mm,
				 struct real_mem_info *mem_info)
{
	struct vm_area_struct *vma;
	struct walk_private data = {
		.mem_info = {
			.total = 0,
			.anon = 0,
		}
	};
	int ret = 0;

	if (mm == NULL)
		goto out;

	if (!down_read_trylock(&mm->mmap_sem))
		return -EBUSY;

	vma = mm->mmap;

	while (vma != NULL && ret == 0) {
		struct mm_walk walk = {
			.pmd_entry = walk_pte_range,
			.mm = vma->vm_mm,
			.private = &data,
		};

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
		data.vma = vma;

		ret = walk_page_range(vma->vm_start, vma->vm_end, &walk);
#else
		ret = walk_page_vma(vma, &walk);
#endif

		vma = vma->vm_next;
	}
	up_read(&mm->mmap_sem);
out:
	if (ret != 0)
		return -EBUSY;

	if (mem_info) {
		mem_info->total = data.mem_info.total >> PSS_SHIFT;
		mem_info->anon = data.mem_info.anon >> PSS_SHIFT;
		mem_info->file = data.mem_info.file >> PSS_SHIFT;
	}

	return 0;
}

/**
 * nicht komplett kontextfest:
 * Probleme wenn Aufruf aus non-kthread-context (irq, nmi, fast-irq, yield):
 * (a) wenn parallel auf andere CPU ein vorletztes mmput gemacht wird, so muss aktueller Kontext mm wegräumen
 *		-> might_sleep()-BUG
 * (b) kein mmap_sem-Schutz fuer walk_page_range()
 */
static int dump_task_memory(struct notifier_block *block,
			    unsigned long event, void *_data)
{
	struct seq_file *file = _data;
	struct task_struct *g, *task;
	unsigned long no_mm = 0, child = 0;
	unsigned long totalpages = totalram_pages + total_swap_pages;

	if (!read_trylock(&tasklist_lock)) {
		return NOTIFY_DONE;
	}

	sseq_printf(file, "Task-Memory in KiB:\n");
	sseq_printf(file, "            "
			  "  ----------------VM---------------"
			  "  ---------PSS-------"
#if IS_ENABLED(CONFIG_SWAP)
#if defined(AVM_ENH_SWAP_STATS)
			  "  --------SWAP-------"
#endif
#endif
			  "\n");
	sseq_printf(file, "   pid score"
			  "  total   code    lib   data  stack"
			  "  total   file   anon"
#if IS_ENABLED(CONFIG_SWAP)
#if defined(AVM_ENH_SWAP_STATS)
			  "  total     in    out"
#else
			  "   swap"
#endif
#endif
			  " name\n");

	do_each_thread(g, task) {
		struct mm_struct *mm = get_task_mm(task);

		if (mm == NULL) {
			no_mm++;
		}
		if (!thread_group_leader(task)) {
			child++;
		}
		if (mm && thread_group_leader(task)) {
			unsigned long oom_score = _oom_score(task, totalpages);
			unsigned long code = (PAGE_ALIGN(mm->end_code) -
					      (mm->start_code & PAGE_MASK)) >>
					     10;
			unsigned long lib =
				(mm->exec_vm << (PAGE_SHIFT - 10)) - code;
			unsigned long stack = mm->stack_vm << (PAGE_SHIFT - 10);
			unsigned long vm_size = mm->total_vm
						<< (PAGE_SHIFT - 10);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
			unsigned long data =
				(mm->total_vm - mm->shared_vm - mm->stack_vm)
				<< (PAGE_SHIFT - 10);
#else
			unsigned long data = mm->data_vm << (PAGE_SHIFT - 10);
#endif
#if IS_ENABLED(CONFIG_SWAP)
			unsigned long swap = get_mm_counter(mm, MM_SWAPENTS)
					     << (PAGE_SHIFT - 10);
#if defined(AVM_ENH_SWAP_STATS)
			unsigned long swap_ins = get_mm_counter(mm, MM_SWAPINS);
			unsigned long swap_outs =
				get_mm_counter(mm, MM_SWAPOUTS);
#endif
#endif
			struct real_mem_info real_mem_info = { 0 };

			collect_real_mem_info(mm, &real_mem_info);

			mmput_avm_context(mm);

			sseq_printf(file,
				// General
				"%6d %5lu "
				// VMZ
				"%6lu %6lu %6lu %6lu %6lu "
				// PSS
				"%6llu %6llu %6llu "
				// Swap
#if IS_ENABLED(CONFIG_SWAP)
				"%6lu "
#if defined(AVM_ENH_SWAP_STATS)
				"%6lu %6lu "
#endif
#endif
				// Process
				"{%s}\n",

				// General
				task->pid, oom_score,
				// VMZ
				vm_size, code, lib, data, stack,
				// PSS
				real_mem_info.total >> 10,
				real_mem_info.file >> 10,
				real_mem_info.anon >> 10,
				// Swap
#if IS_ENABLED(CONFIG_SWAP)
				swap,
#if defined(AVM_ENH_SWAP_STATS)
				swap_ins, swap_outs,
#endif
#endif
				// Process
				task->comm);
		} else if (mm) {
			mmput_avm_context(mm);
		}
	} while_each_thread(g, task);
	sseq_printf(file, "kthreads %lu childs %lu\n", no_mm, child);
	read_unlock(&tasklist_lock);

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
	int low_watermark = 0;
	unsigned int slab_pages = 0;

	if (atomic_add_return(force & 0x1, &print_one_time) > 1) {
		return;
	}
	if (!(force & 0x2)) {
		slab_pages = global_page_state(NR_SLAB_RECLAIMABLE) +
			     global_page_state(NR_SLAB_UNRECLAIMABLE);
		low_watermark = watermark_under_low();
	} else {
		low_watermark = 1;
	}
	/*--- pr_err("%s %u: slab_pages=%lu totalram_pages=%lu\n",  __func__, __LINE__, slab_pages, totalram_pages); ---*/
	if (low_watermark ||
	    critical_slabmem_consume(slab_pages, totalram_pages)) {
		char buf[128];
		struct semi_seq sseq;
		struct seq_file *seq;

		seq = sseq_create(&sseq, KERN_ERR, buf, sizeof(buf));

		pr_debug(
			"-------------------- memory-situation --------------------\n");
		show_mem(0);
		atomic_notifier_call_chain(&oom_info_chain, 0, seq);
		if (!IS_ERR_OR_NULL(&show_slab)) {
			show_slab();
		}
	}
}

static __init int avm_oom_init(void)
{
	avm_oom_info_chain_register(&dump_task_memory_block);

	register_oom_notifier(&oom_nb);
#if defined(TAINT_ALLOC_FAIL)
	atomic_notifier_chain_register(&panic_notifier_list, &panic_nb);
#endif

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
