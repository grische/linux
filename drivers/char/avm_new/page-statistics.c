// SPDX-License-Identifier: GPL-2.0+

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/vmstat.h>
#include <linux/sched.h>
#include <linux/math64.h>
#include <linux/writeback.h>
#include <linux/compaction.h>
#include <linux/ratelimit.h>
#include <linux/init.h>
#include <linux/oom.h>
#include <avm/sammel/simple_proc.h>
#include <avm/sammel/debug.h>
#include <avm/sammel/helpers.h>
#include <avm/sammel/page-statistics.h>
#include <avm/sammel/page-trace.h>

#include <avm/enh/semi_seq.h>

#include "avm_sammel.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0) && \
	!LINUX_STABLE_VERSION_HAS_SUBLEVEL(4, 4, 216)
static inline int page_ref_count(struct page *page)
{
	return atomic_read(&page->_count);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
// only PG_head exist
#define PAGEFLAGS_HEADEXIST
#define NON_CLASSIFIED_PAGEMASK  ((BIT(NR_PAGEFLAGS) - 1) & ~(BIT(PG_head)))
#elif IS_ENABLED(CONFIG_PAGEFLAGS_EXTENDED)
// both PG_head and PG_tail exist
#define PAGEFLAGS_HEADEXIST
#define PAGEFLAGS_TAILEXIST
#define NON_CLASSIFIED_PAGEMASK  ((BIT(NR_PAGEFLAGS) - 1) & ~(BIT(PG_head) | BIT(PG_tail)))
#else
// only PG_compound exist
#define PAGEFLAGS_COMPOUNDEXIST
#define NON_CLASSIFIED_PAGEMASK  ((BIT(NR_PAGEFLAGS) - 1) & ~(BIT(PG_compound)))
#endif

#define DBG_TRC(args...) \
	if (IS_ENABLED(CONFIG_AVM_PAGE_STATISTICS_DEBUG)) \
		{ pr_info(args); }

#define DBG_LIST(args...) \
	if (IS_ENABLED(CONFIG_AVM_PAGE_STATISTICS_DEBUG_NON_FREE_PAGES)) \
		{ pr_info(args); }

static atomic_t lock_memstat;

#define MAX_MEMORY_FREE_STATISTIC (1 << (MAX_ORDER + 1))

#if defined(CONFIG_AVM_PAGE_TRACE)
#define MAX_SCORELIST_ENRIES 32
#define MAX_SCORETABLE	     4
#else /*--- #if defined(CONFIG_AVM_PAGE_TRACE) ---*/
#define MAX_SCORELIST_ENRIES 1
#define MAX_SCORETABLE 1
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_PAGE_TRACE) ---*/

struct _avm_pagealloc_score {
	unsigned long current_pc;
	unsigned short sum;
	unsigned short refcnt_more_than_one; /* ... refcnt ist groesser 1 */
};
struct _avm_pagealloc_score_table {
	struct _avm_pagealloc_score pagealloc_score[MAX_ORDER + 1][MAX_SCORELIST_ENRIES + 1];
};

static struct _avm_pagealloc_score_table pagealloc_score_table[MAX_SCORETABLE];

static struct _scorelist {
	const char *prefix;
	struct _avm_pagealloc_score_table *table;
	unsigned int mask;
	unsigned int result;
} scorelist[MAX_SCORETABLE + 1] = {
#if defined(CONFIG_AVM_PAGE_TRACE)
	{ .prefix = "non-classified ", .mask = NON_CLASSIFIED_PAGEMASK, .result = 0                },
	{ .prefix = "lru ",	       .mask = BIT(PG_lru),		.result = BIT(PG_lru)	   },
	{ .prefix = "reserved ",       .mask = BIT(PG_reserved),	.result = BIT(PG_reserved) },
	// exclude slab:
	{ .prefix = "other ",	       .mask = BIT(PG_slab),		.result = 0		   },
#endif /*--- #if defined(CONFIG_AVM_PAGE_TRACE) ---*/
	{ .prefix = NULL,}, /*--- END-Marker ---*/
};

#if IS_ENABLED(CONFIG_MIPS) && ! IS_ENABLED(CONFIG_SOC_GRX500)
/*--- unsere MIPS-Boxen auszer GRX haben max. 256 MByte (max. 65 k Pages) ---*/
typedef unsigned short mem_count_t;
#else
typedef unsigned int mem_count_t;
#endif

struct _avm_memory_statistic {
	mem_count_t free_statistic[MAX_MEMORY_FREE_STATISTIC];
	mem_count_t lru_free_statistic[MAX_MEMORY_FREE_STATISTIC];
	mem_count_t buddy_free_statistic[MAX_ORDER + 1];
	mem_count_t flag_statistic[32];
};
static struct _avm_memory_statistic avm_memory_statistic;


/**
 */
static inline void increase_page_sum(struct _avm_pagealloc_score *pscore, unsigned int refcount)
{
	pscore->sum++;
	if (refcount > 1)
		pscore->refcnt_more_than_one++;
}

/**
 * Eintrag 0 fuer current_pc = 0;
 */
static void add_to_page_scorelist(struct _avm_pagealloc_score_table *score_table,
				  unsigned long current_pc, unsigned int order,
				  unsigned int refcnt)
{
	unsigned long i;

	if (order > MAX_ORDER) {
		pr_err("%s: warning: invalid order=%u\n", __func__, order);
		return;
	}
	if ((current_pc == 0)) {
		/*--- pr_err("warning: no current_pc order=%u\n", order); ---*/
		increase_page_sum(&score_table->pagealloc_score[order][0], refcnt);
		return;
	}
	for (i = 1; i < ARRAY_SIZE(score_table->pagealloc_score[0]); i++) {
		struct _avm_pagealloc_score *pscore = &score_table->pagealloc_score[order][i];

		if (pscore->current_pc == 0) {
			pscore->current_pc = current_pc;
			increase_page_sum(pscore, refcnt);
			return;
		}
		if (pscore->current_pc == current_pc) {
			increase_page_sum(pscore, refcnt);
			return;
		}
	}
	printk_ratelimited(KERN_ERR
			   "warning: no alloc %pS order =%u -> add to [0]\n",
			   (void *)current_pc, order);
	increase_page_sum(&score_table->pagealloc_score[order][0], refcnt);
}
/**
 */
static void add_to_page_all_scorelist(unsigned long current_pc,
				      unsigned int order,
				      unsigned int refcnt,
				      unsigned int page_flag)
{
	struct _scorelist *pscore = scorelist;

	while (pscore->table) {
		if ((page_flag & pscore->mask) == pscore->result) {
			add_to_page_scorelist(pscore->table, current_pc, order, refcnt);
			return;
		}
		pscore++;
	}
}
#if 1
// clang-format off
static char *name_pgflag(char *txt, unsigned int txt_len, unsigned int pg_flag)
{
	snprintf(txt, txt_len, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
#ifdef PAGEFLAGS_HEADEXIST
		 "%s"
#endif
#ifdef PAGEFLAGS_TAILEXIST
		 "%s"
#endif
#ifdef PAGEFLAGS_COMPOUNDEXIST
		 "%s"
#endif
#if IS_ENABLED(CONFIG_MMU)
		 "%s"
#endif
#if IS_ENABLED(CONFIG_ARCH_USES_PG_UNCACHED)
		 "%s"
#endif
	,
	pg_flag & (1 << PG_locked)       ? " locked"             : "",	/* Page is locked. Don't touch. */
	pg_flag & (1 << PG_error)        ? " error"              : "",
	pg_flag & (1 << PG_referenced)   ? " referenced"         : "",
	pg_flag & (1 << PG_uptodate)     ? " uptodate"           : "",
	pg_flag & (1 << PG_dirty)        ? " dirty"              : "",
	pg_flag & (1 << PG_lru)          ? " lru"                : "",
	pg_flag & (1 << PG_active)       ? " active"             : "",
	pg_flag & (1 << PG_slab)         ? " slab"               : "",
	pg_flag & (1 << PG_owner_priv_1) ? " owner_priv_1"       : "", /* Owner use. If pagecache, fs may use*/
	pg_flag & (1 << PG_arch_1)       ? " arch_1"             : "",
	pg_flag & (1 << PG_reserved)     ? " reserved"           : "",
	pg_flag & (1 << PG_private)      ? " private"	         : "", /* If pagecache, has fs-private data */
	pg_flag & (1 << PG_private_2)    ? " private_2"          : "", /* If pagecache, has fs aux data */
	pg_flag & (1 << PG_writeback)    ? " writeback"          : "", /* Page is under writeback */
#ifdef PAGEFLAGS_HEADEXIST
	pg_flag & (1 << PG_head)         ? " head"               : "",	/* A head page */
#endif
#ifdef PAGEFLAGS_TAILEXIST
	pg_flag & (1 << PG_tail)         ? " tail"               : "",	/* A tail page */
#endif
#ifdef PAGEFLAGS_COMPOUNDEXIST
	pg_flag & (1 << PG_compound)     ? " compound"           : "", /* A compound page */
#endif
	pg_flag & (1 << PG_swapcache)    ? " swapcache"          : "",	/* Swap page: swp_entry_t in private */
	pg_flag & (1 << PG_mappedtodisk) ? " mappedtodisk"       : "", /* Has blocks allocated on-disk */
	pg_flag & (1 << PG_reclaim)      ? " reclaim"            : "", /* To be reclaimed asap */
	pg_flag & (1 << PG_swapbacked)   ? " swapbacked"         : "", /* Page is backed by RAM/swap */
	pg_flag & (1 << PG_unevictable)  ? " unevictable"        : "", /* Page is "unevictable"  */
#if IS_ENABLED(CONFIG_MMU)
	pg_flag & (1 << PG_mlocked)      ? " mlocked"            : "",	/* Page is vma mlocked */
#endif
#if IS_ENABLED(CONFIG_ARCH_USES_PG_UNCACHED)
	pg_flag & (1 << PG_uncached)     ? " uncached"           : "", /* Page has been mapped as uncached */
#endif
	""
	);
	return txt;
}
// clang-format on
#endif
/**
 */
static inline unsigned int __mark_continues_free(unsigned int free,
						 mem_count_t table[],
						 unsigned int max_entries)
{
	if (free == 0) {
		return free;
	}
	if (free >= max_entries) {
		free = max_entries - 1;
	}
	table[free]++;
	return 0;
}
#define mark_continues_free(free, table)                                       \
	__mark_continues_free(free, table, ARRAY_SIZE(table))

/**
 */
static void display_page_scorelist(struct seq_file *file,
				  const char *prefix,
				  struct _avm_pagealloc_score_table *score_table)
{
	struct _avm_pagealloc_score *score;
	unsigned int i, order, sum = 0;
	char txt[64];

	for (order = 0; order < (MAX_ORDER + 1); order++) {
		score = &score_table->pagealloc_score[order][0];
		if ((score[0].sum == 0) && (score[1].sum == 0)) {
			continue;
		}
		sseq_printf(file, "%s%s-Pages order=%u  (%u KiB):\n", prefix,
			    order ? "Head" : "Single", order, 4 << order);
		for (i = 0; i < ARRAY_SIZE(score_table->pagealloc_score[0]);
		     i++) {
			score = &score_table->pagealloc_score[order][i];

			if (score->sum == 0) {
				if (i == 0) {
					continue;
				}
				break;
			}
			sum += (unsigned int)score->sum << order;

			if (score->refcnt_more_than_one) {
				snprintf(txt, sizeof(txt), " [refcnt>1: pages=%5u (%6u KiB)]",
					 (unsigned int)score->refcnt_more_than_one << order,
					 ((unsigned int)score->refcnt_more_than_one << order) * 4);
			} else
				txt[0] = 0;

			if (order == 0) {
				sseq_printf(file, "[%3u] %5u (%6u KiB)%s %pS\n", i,
					    score->sum,
					    score->sum * 4,
					    txt,
					    (void *)score->current_pc);
			} else {
				sseq_printf(file,
					    "[%3u] %5u - pages=%5u (%6u KiB)%s %pS\n",
					    i,
					    score->sum,
					    (unsigned int)score->sum << order,
					    ((unsigned int)score->sum << order) * 4,
					    txt,
					    (void *)score->current_pc);
			}
		}
	}
	if (sum) {
		sseq_printf(file, "Sum of %sPages: %u (%6u KiB)\n", prefix, sum,
			    sum * 4);
	}
}
/**
 */
static void dislay_page_all_scorelist(struct seq_file *file)
{
	struct _scorelist *pscore = scorelist;

	while (pscore->table) {
		display_page_scorelist(file, pscore->prefix, pscore->table);
		pscore++;
	}
}
/**
 */
static inline int is_free_page(struct page *page)
{
	if ((page_mapcount(page) | (page->mapping != NULL) |
	     (page_ref_count(page) != 0) |
	     (page->flags & PAGE_FLAGS_CHECK_AT_FREE))) {
		return 0;
	}
	return 1;
}
/**
 */
static void add_pagetype_field(unsigned int page_flags,
			       mem_count_t flag_statistic[], unsigned int order)
{
	unsigned int i = 0;

	while (page_flags) {
		if (page_flags & 0x01) {
			flag_statistic[i] += (1 << order);
		}
		i++;
		page_flags >>= 1;
	}
}
/**
 * ret: 0 in relevanten parts identisch
 */
static int save_page_cmp(struct page *dst, unsigned long *last_pc, struct page *act)
{
	unsigned long act_pc;
	int ret = 1;

	if (!IS_ENABLED(CONFIG_AVM_PAGE_STATISTICS_DEBUG))
	    return ret;

	act_pc = avm_get_page_current_pc(act);

	if ((dst->flags == act->flags) &&
	    (compound_order(dst) == compound_order(act)) &&
	    (page_ref_count(dst) == page_ref_count(act)) &&
	    (*last_pc == act_pc)) {
		ret = 0;
	}
	memcpy(dst, act, sizeof(*dst));
	*last_pc = act_pc;

	return ret;
}

/**
 * die pages fuer den memmap-Bereich muessen reserviert sein!
 */
static int check_memmap_page_reserved(void)
{
	int gaps = 0;
	pg_data_t *pgdat;
	void *memmap_start, *memmap_end;
	struct page *pagememmap_start, *pagememmap_end;
	unsigned long pfnmemmap_start, pfnmemmap_end;

	for (pgdat = first_online_pgdat(); pgdat;
	     pgdat = next_online_pgdat(pgdat)) {
		if (!pgdat_end_pfn(pgdat)) {
			pr_err("[%s] pgdat 0x%lx is empty\n", __func__,
			       (unsigned long)pgdat);
			continue;
		}
		/*--- die virtuellen Addressen der memmap ermitteln .. ---*/
		memmap_start = pfn_to_page(pgdat->node_start_pfn);
		memmap_end   = pfn_to_page(pgdat_end_pfn(pgdat));

		/*--- ... das zugehoerige page-struct-Area in mem_map ---*/
		pagememmap_start = virt_to_page(memmap_start);
		pagememmap_end   = virt_to_page(memmap_end);

		pfnmemmap_start = page_to_pfn(pagememmap_start);
		pfnmemmap_end   = page_to_pfn(pagememmap_end);
		pr_info("[%s] pfn %lu - %lu (mem_map= 0x%p - 0x%p) should be reserved - because contain page-structs",
			__func__, pfnmemmap_start, pfnmemmap_end,
			pagememmap_start, pagememmap_end);

		while (pagememmap_start &&
		       (pagememmap_start < pagememmap_end)) {
			if (pfn_valid((unsigned long)page_address(
				    pagememmap_start))) {
				if (!PageReserved(pagememmap_start)) {
					pr_err("%s: error page 0x%p (pfn=%lu) flags=0x%08lx (addr=0x%p) should be reserved!",
					       __func__, pagememmap_start,
					       page_to_pfn(pagememmap_start),
					       pagememmap_start->flags,
					       page_address(pagememmap_start));
					return -1;
				}
			} else {
				gaps++;
			}
			pagememmap_start++;
		}
	}
	pr_info("%s: mem_map-area: pfn %lu - %lu marked as reserved: ok %s\n",
		__func__, pfnmemmap_start, pfnmemmap_end,
		gaps ? "(include gaps)" : "");
	return 0;
}

/**
 */
static void avm_page_statistic_print(struct seq_file *file, bool _complete)
{
	struct _avm_memory_statistic *pmemstat = &avm_memory_statistic;
	char txt[256];
	unsigned int page_flags, order;
	unsigned long flags;
	unsigned int buddy_pages, _count;
	struct page *page;
	struct zone *zone;
	pg_data_t *pgdat;
	unsigned int i = 0, n_order = 0;
	unsigned int page_count, lru_free_pages, sum_pages, free_pages;
	struct page last_page;
	unsigned long current_pc, last_pc = 0;
	int point_flag = 0, gap_flag = 0;

	if (atomic_add_return(1, &lock_memstat) > 1) {
		return;
	}
	memset(&last_page, -1, sizeof(last_page));
	memset(pagealloc_score_table, 0, sizeof(pagealloc_score_table));
	memset(pmemstat, 0, sizeof(struct _avm_memory_statistic));
	lru_free_pages = 0;
	free_pages = 0;
	sum_pages = 0;
	for (pgdat = first_online_pgdat(); pgdat; pgdat = next_online_pgdat(pgdat)) {
		unsigned long pfn;
		unsigned int is_free = 0;
		unsigned int is_lru_free = 0;

		if (!pgdat_end_pfn(pgdat)) {
			pr_err("[%s] pgdat 0x%lx is empty\n", __func__, (unsigned long)pgdat);
			continue;
		}
		pfn = pgdat->node_start_pfn;
		DBG_TRC("[%s] scan pgdat: start 0x%lx(mem_map=0x%p) present 0x%lx spanned 0x%lx\n",
			__func__, pgdat->node_start_pfn,
			pfn_to_page(pfn), pgdat->node_present_pages,
			pgdat->node_spanned_pages);
		sseq_printf(file,
			    "scan pgdat: start 0x%lx(mem_map=0x%p) present 0x%lx spanned 0x%lx\n",
			    pgdat->node_start_pfn, pfn_to_page(pfn),
			    pgdat->node_present_pages, pgdat->node_spanned_pages);

		for (page_count = 0; page_count < pgdat->node_spanned_pages; page_count++) {
			pfn = pgdat->node_start_pfn + page_count;
			if (!pfn_valid(pfn)) {
				/*--- es kann gaps in der page struct-Table geben, da auch der memory gaps enthalten kann ! ---*/
				if (gap_flag == 0) {
					DBG_TRC("gaps at page #%u\n", page_count);
				} else if (gap_flag == 1) {
					DBG_TRC("...\n");
				}
				gap_flag++;
				continue;
			}
			page = pfn_to_page(pfn);
			gap_flag = 0;
			zone = page_zone(page);
			spin_lock_irqsave(&zone->lock, flags);
			page_flags = page->flags;
			if (PageBuddy(page)) {
				order = page_private(page); /*--- PageBuddy must check ! ---*/
				spin_unlock_irqrestore(&zone->lock, flags);
				is_free = mark_continues_free(is_free, pmemstat->free_statistic);
				is_lru_free = mark_continues_free(is_lru_free, pmemstat->lru_free_statistic);
				if (order <= MAX_ORDER) {
					pmemstat->buddy_free_statistic[order]++;
					DBG_TRC("Buddy: page=0x%p pfn=%lu-%lu (addr=0x%p) order %2u %s (0x%08x)\n",
						page, page_to_pfn(page),
						page_to_pfn(page) + (1 << order) - 1,
						page_address(page), order,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags);
					page_count += (1 << order) - 1;
				} else {
					pr_info("obscure buddy: page=0x%p pfn=%lu (addr=0x%p) order %2u %s (0x%08x)\n",
						page, page_to_pfn(page),
						page_address(page), order,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags);
				}
				memset(&last_page, -1, sizeof(last_page));
				last_pc = 0;
				continue;
			}
			current_pc = avm_get_page_current_pc(page);
			if (save_page_cmp(&last_page, &last_pc, page)) {
				point_flag = 0;
			} else {
				point_flag++;
			}
			if (PageHead(page)) {
				order = compound_order(page);
				if (order > MAX_ORDER) {
					spin_unlock_irqrestore(&zone->lock, flags);
					pr_info("obscure head: page=0x%p pfn=%lu (addr=0x%p) invalid order %2u %s (0x%08x)\n",
						page, page_to_pfn(page),
						page_address(page), order,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags);
					continue;
				}
				_count = page_ref_count(page);
				if (_count == 0) {
					pr_info("obscure head: page=0x%p pfn=%lu (addr=0x%p) order %2u invalid _count=%u %s (0x%08x) %pS\n",
						page, page_to_pfn(page),
						page_address(page),
						compound_order(page), _count,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags, (void *)current_pc);
				}
			}
			_count = page_ref_count(page);
			if (_count) {
				order = compound_order(page);
				/*--- hier alle non-freed Pages tracen: ---*/
				if (point_flag == 0) {
					DBG_LIST("page=0x%p pfn=%lu (addr=0x%p) order=%2u _count=%u pg_flag=%s (0x%08x) caller=%pS\n",
						page, page_to_pfn(page),
						page_address(page), order,
						_count,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags, (void *)current_pc);
				} else if (point_flag == 1) {
					DBG_TRC("...\n");
					point_flag++;
				}
				add_to_page_all_scorelist(current_pc, order,
							  _count,
							  page_flags);
			}
			if (PageReserved(page)) {
				order = compound_order(page);
				_count = page_ref_count(page);
				spin_unlock_irqrestore(&zone->lock, flags);
				if (point_flag == 0) {
					DBG_TRC("reserved: page=0x%p pfn=%lu (addr=0x%p) order=%2u _count=%u %s (0x%08x) %pS\n",
						page, page_to_pfn(page),
						page_address(page), order,
						_count,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags, (void *)current_pc);
				} else if (point_flag == 1) {
					DBG_TRC("...\n");
				}
				is_free = mark_continues_free(is_free, pmemstat->free_statistic);
				is_lru_free = mark_continues_free(is_lru_free, pmemstat->lru_free_statistic);
				pmemstat->flag_statistic[PG_reserved]++;
				continue;
			}
			add_pagetype_field(page_flags,
					   &pmemstat->flag_statistic[0],
					   PageHead(page) ? compound_order(page) : 0);

			if (PageHead(page)) {
				order = compound_order(page);
				_count = page_ref_count(page);
				spin_unlock_irqrestore(&zone->lock, flags);
				DBG_TRC("head: page=%p pfn=%lu (addr=0x%p) order=%2u _count=%u %s (0x%08x) %pS\n",
					page, page_to_pfn(page),
					page_address(page), order, _count, name_pgflag(txt, sizeof(txt), page_flags),
					page_flags, (void *)current_pc);
				page_count += (1 << order) - 1;
				is_free = mark_continues_free(is_free, pmemstat->free_statistic);
				is_lru_free = mark_continues_free(is_lru_free, pmemstat->lru_free_statistic);
				continue;
			}
			if (is_free_page(page)) {
				_count = page_ref_count(page);
				spin_unlock_irqrestore(&zone->lock, flags);
				if (point_flag == 0) {
					DBG_TRC("free-page: page=0x%p pfn=%lu (addr=0x%p) _count=%u %s (0x%08x) %pS\n",
						page, page_to_pfn(page),
						page_address(page), _count,
						name_pgflag(txt, sizeof(txt), page_flags),
						page_flags, (void *)current_pc);
				} else if (point_flag == 1) {
					DBG_TRC("...\n");
				}
				free_pages++;
				is_free++;
				continue;
			}
			spin_unlock_irqrestore(&zone->lock, flags);
			is_free = mark_continues_free(is_free,
						      pmemstat->free_statistic);

			if (page_flags & (1 << PG_lru)) {
				if (!(page_flags & (1 << PG_dirty))) {
					lru_free_pages++;
					is_lru_free++;
					continue;
				}
			}
			is_lru_free = mark_continues_free(
				is_lru_free, pmemstat->lru_free_statistic);
		}
		is_free =
			mark_continues_free(is_free, pmemstat->free_statistic);
		is_lru_free = mark_continues_free(is_lru_free,
						  pmemstat->lru_free_statistic);
		sum_pages += pgdat->node_spanned_pages;
	}
	/*------------ Ausgabe -------------*/
	if (_complete) {
		sseq_printf(file, "Free pages statistic\n");
	}
	if (_complete) {
		sseq_printf(file, "Count([KiB]): on-LRU   free    Buddy Sum( max)\n");
	}
	buddy_pages = 0;
	for (i = 1; i < MAX_MEMORY_FREE_STATISTIC; i++) {
		n_order = (i & ~((1 << ffs(i)) - 1)) == 0 ? ffs(i) : 0;
		if (pmemstat->lru_free_statistic[i] ||
		    pmemstat->free_statistic[i] || (n_order)) {
			unsigned int buddy = n_order ?
				pmemstat->buddy_free_statistic[n_order - 1] : 0;

			if (_complete) {
				sseq_printf(file,
					    "%4u (%5u): %5u   %5u   %5u    %5u%s\n",
					     i, i * 4,
					    pmemstat->lru_free_statistic[i],
					    pmemstat->free_statistic[i], buddy,
					    (unsigned int)pmemstat->lru_free_statistic[i] +
					    (unsigned int)pmemstat->free_statistic[i] +
					    buddy,
					    n_order ? " =======" : "");
			}
			buddy_pages += buddy * i;
		}
	}
	dislay_page_all_scorelist(file);
#if 0
	sseq_printf(file, "Free Buddy-List statistic\n");
	sseq_printf(file, "Count([KiB]):\n");
	for (i = 0; i < ARRAY_SIZE(pmemstat->buddy_free_statistic); i++) {
		if (pmemstat->buddy_free_statistic[i]) {
			sseq_printf(file, "%4u (%5u): %5u\n", (1U << i), (1U << i) * 4,
				    pmemstat->buddy_free_statistic[i]);
		}
	}
#endif
	sseq_printf(file, "%5u (%6u KiB) total pages\n", sum_pages,
		    sum_pages * 4);
	sseq_printf(file, "%5u (%6u KiB) free pages\n", free_pages,
		    free_pages * 4);
	sseq_printf(file, "%5u (%6u KiB) free pages on Buddy\n", buddy_pages,
		    buddy_pages * 4);
	sseq_printf(file, "%5u (%6u KiB) pages on LRU (%u freeable pages)\n",
		    pmemstat->flag_statistic[PG_lru],
		    pmemstat->flag_statistic[PG_lru] * 4, lru_free_pages);
	sseq_printf(file, "%5u (%6u KiB) pages allocated on-disc\n",
		    pmemstat->flag_statistic[PG_mappedtodisk],
		    pmemstat->flag_statistic[PG_mappedtodisk] * 4);
	sseq_printf(file, "%5u (%6u KiB) pages for slab\n",
		    pmemstat->flag_statistic[PG_slab],
		    pmemstat->flag_statistic[PG_slab] * 4);
	sseq_printf(file, "%5u (%6u KiB) pages to be reclaimed asap\n",
		    pmemstat->flag_statistic[PG_reclaim],
		    pmemstat->flag_statistic[PG_reclaim] * 4);
	sseq_printf(file, "%5u (%6u KiB) pages are reserved\n",
		    pmemstat->flag_statistic[PG_reserved],
		    pmemstat->flag_statistic[PG_reserved] * 4);

#ifdef PAGEFLAGS_TAILEXIST
	sseq_printf(file, "%5u (%6u KiB) pages as tail\n",
		    pmemstat->flag_statistic[PG_tail],
		    pmemstat->flag_statistic[PG_tail] * 4);
#endif
#ifdef PAGEFLAGS_HEADEXIST
	sseq_printf(file, "%5u (%6u KiB) pages as head\n",
		    pmemstat->flag_statistic[PG_head],
		    pmemstat->flag_statistic[PG_head] * 4);
#endif
#ifdef PAGEFLAGS_COMPOUNDEXIST
	sseq_printf(file, "%5u (%6u KiB) pages as compound\n",
		    pmemstat->flag_statistic[PG_compound],
		    pmemstat->flag_statistic[PG_compound] * 4);
#endif

	if (pmemstat->flag_statistic[PG_mlocked]) {
		sseq_printf(file, "%5u (%6u KiB) pages are mlocked\n",
			    pmemstat->flag_statistic[PG_mlocked],
			    pmemstat->flag_statistic[PG_mlocked] * 4);
	}
	if (pmemstat->flag_statistic[PG_swapcache]) {
		sseq_printf(file, "%5u (%6u KiB) pages are swap pages\n",
			    pmemstat->flag_statistic[PG_swapcache],
			    pmemstat->flag_statistic[PG_swapcache] * 4);
	}
#if 0
	if (pmemstat->flag_statistic[PG_swapbacked]) {
		sseq_printf(file, "%5u (%6u KiB) pages are swapbacked\n",
			    pmemstat->flag_statistic[PG_swapbacked],
			    pmemstat->flag_statistic[PG_swapbacked] * 4);
	}
#endif
	atomic_set(&lock_memstat, 0);
}
/**
 */
void show_avm_page_statistic(bool complete)
{
	DEFINE_SEMI_SEQ(ssseq, KERN_ERR);

	avm_page_statistic_print(ssseq_to_seq(ssseq), complete);
}
/**
 */
static void __init init_scorelist(void)
{
	int i = 0;
	struct _scorelist *pscore = scorelist;

	while (pscore->prefix) {
		if (i >= (int)ARRAY_SIZE(pagealloc_score_table)) {
			pr_err("%s: can't fill complete score-list\n", __func__);
			break;
		}
		pscore->table = &pagealloc_score_table[i++];
		DBG_TRC("%s: complete score-list[%u] %s: table=%p mask=%x result=%x\n",
			__func__, i - 1, pscore->prefix, pscore->table,
			pscore->mask, pscore->result);
		pscore++;
	}
}

/**
 */
static void avm_proc_page_statistic(struct seq_file *file, void *priv)
{
	avm_page_statistic_print(file, true);
}

/**
 */
static int dump_task_memory(struct notifier_block *block,
			    unsigned long event, void *_data)
{
	struct seq_file *file = _data;

	avm_page_statistic_print(file, false);

	return NOTIFY_DONE;
}

static struct notifier_block page_statistic_block = {
	.notifier_call = dump_task_memory
};

/**
 * registrieren:
 */
static int __init init_avm_page_statistic(void)
{
	DBG_TRC("%s: memory-sizes: avm_memory_statistic=%zu pagealloc_score_table=%zu page-size=%zu\n",
		__func__, sizeof(struct _avm_memory_statistic),
		sizeof(pagealloc_score_table), sizeof(struct page));
	init_scorelist();
	add_simple_proc_file("avm/page_statistic", NULL,
			     avm_proc_page_statistic, NULL);
	avm_oom_info_chain_register(&page_statistic_block);

	if (IS_ENABLED(CONFIG_AVM_PAGE_STATISTICS_DEBUG)) {
		check_memmap_page_reserved();
		show_avm_page_statistic(true);
	}

	return 0;
}

late_initcall(init_avm_page_statistic);
