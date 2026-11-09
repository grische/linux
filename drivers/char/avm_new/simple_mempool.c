// SPDX-License-Identifier: GPL-2.0+

/**
 * Anpassungen speziell fuer YIELD-instead-IRQ oder FASTIRQ (arm) Support
 * Ersatz von k[zm]alloc()/kfree durch Yield/Fastirq-faehige Memorypool-Funktion
 * author: mbahr@avm.de
 */

#if defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>

#include <avm/sammel/debug.h>
#include <avm/sammel/simple_mempool.h>
#define MEMLIST_OFFSET(pmp, addr) ((unsigned long)(addr))
#else /*--- #if !defined(__KERNEL__) ---*/
/**
 * Offline-Test-Tool
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define min(a, b) ((a) > (b) ? (b) : (a))
#define pr_info printf
#define pr_err printf
#define GFP_ATOMIC 0
#define ENOMEM 5
#define kzalloc(a, b) calloc(a, 1)
#define kfree(a) free(a)
#define alloc_pages_exact(a, b) calloc(a, 1)
#define free_pages_exact(a, b) free(a)
#define KERN_ERR
#define mb()
#define unlikely(a) (a)
#define EXPORT_SYMBOL(a)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define yield_spin_lock_irqsave(a, b) (b = 0, b = b)
#define yield_spin_unlock_irqrestore(a, b)
#define yield_spin_lock_init(a)
#define yield_is_avm_rte() 0
#define yield_panic(args...)
#define dump_stack()
#define max(a, b)                                                              \
	({                                                                     \
		__typeof__(a) _a = (a);                                        \
		__typeof__(b) _b = (b);                                        \
		_a > _b ? _a : _b;                                             \
	})
typedef int spinlock_t;
typedef unsigned int atomic_t;
static struct _fake {
	char comm[1];
} *current;

#define atomic_read(a) (*a)
#define atomic_sub(val, a) (*a -= val)
#define atomic_add(val, a) (*a += val)
#define atomic_set(a, b) (*a = b)
#define schedule_timeout(a)
#define MEMLIST_OFFSET(pmp, addr)                                              \
	((addr) ? ((unsigned long)(addr) -                                     \
		   (unsigned long)(pmp->memlist_anchor)) :                     \
		  0)

static void no_printk(const char *format __attribute__((unused)), ...)
{
}


/**
 * Offline-Test-Tool - EOF
 */
#endif /*--- #else ---*/ /*--- #if !defined(__KERNEL__) ---*/

#if defined(__KERNEL__)
#include "avm_sammel.h"

#define DEBUG_MEMORYPOOL_TRACE
#endif /*--- #if defined(__KERNEL__) ---*/

#if defined(MY_TESTMEMPOOL)
#define MEMORY_CORRUPT_DEBUG 0xCC
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/

/*--- #define DEBUG_MEMORYPOOL ---*/

#if defined(DEBUG_MEMORYPOOL)
#define DBG_TRC(args...) pr_info(args)
#else /*--- #if defined(DEBUG_MEMORYPOOL)  ---*/
#define DBG_TRC(args...) no_printk(args)
#endif /*--- #else ---*/ /*--- #if defined(DEBUG_MEMORYPOOL)  ---*/

#define DBG_ERR(args...) pr_err(args)

#define MEMORYPOOL_MAGIC                                                       \
	0x151008 /*--- Bit0 reserviert fuer used, Bit1 reserviert fuer readed ---*/
#define CHECK_MEMORY_ENTRY(a) ((unsigned int)((a) ^ MEMORYPOOL_MAGIC) <= 3)
#define CHECK_MEMORY_USED(a)   ((a) & 0x1)
#define CHECK_MEMORY_READED(a) ((((a) & 0x1) == 0) || (a) & 0x2)

#if defined(MY_TESTMEMPOOL)
#define ALIGN_SHIFT 6
#else /*--- #if defined(MY_TESTMEMPOOL) ---*/
#define ALIGN_SHIFT 5
#endif /*--- #else ---*/ /*--- #if defined(MY_TESTMEMPOOL) ---*/

#define ALIGN_SIZE(size)                                                       \
	((size + ((1 << ALIGN_SHIFT) - 1)) & ~((1 << ALIGN_SHIFT) - 1))

#if defined(MEMORY_CORRUPT_DEBUG)
#define MEMORY_GAP_SIZE 64
#else /*--- #if defined(MEMORY_CORRUPT_DEBUG) ---*/
#define MEMORY_GAP_SIZE 0
#endif /*--- #else ---*/ /*--- #if defined(MEMORY_CORRUPT_DEBUG) ---*/

static void simplemempool_dump(void *_pmp);

#if defined(CONFIG_PROC_FS)
#include <avm/compat/proc_fs.h>
#define PROC_MEMPOOLDIR "avm/mempool"

static const struct proc_ops mempoolstat_fops;
static struct proc_dir_entry *mempoolprocdir;
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
/**
 */
struct _memory_list {
	atomic_t used;
	unsigned int size; /*--- size inklusive Header ---*/
	unsigned int demand_size;
#if defined(DEBUG_MEMORYPOOL_TRACE)
	void *caller;
#endif /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
	union {
		struct _memory_list *next;
		void *memaddr;
	} u;
	struct _memory_list *prev;
#if defined(MY_TESTMEMPOOL)
	char comm_name[12];
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
};

/**
 */
static inline void check_memory_init(struct _memory_list *mem_entry)
{
#if defined(MEMORY_CORRUPT_DEBUG)
	unsigned int _size =
		mem_entry->demand_size + sizeof(struct _memory_list);

	memset(((unsigned char *)mem_entry) + _size, MEMORY_CORRUPT_DEBUG,
	       mem_entry->size - _size);
#endif /*--- #if defined(MEMORY_CORRUPT_DEBUG) ---*/
}

/**
 */
static inline int check_memory(const char *prefix,
			       struct _memory_list *mem_entry,
			       int dump)
{
#if defined(MEMORY_CORRUPT_DEBUG)
	unsigned int i;
	unsigned char *p = (unsigned char *)mem_entry;
	unsigned int _size =
		mem_entry->demand_size + sizeof(struct _memory_list);

	for (i = _size; i < mem_entry->size; i++) {
		if (p[i] != MEMORY_CORRUPT_DEBUG) {
			char txt[128];
			char *ptxt = txt;
			int txt_len = sizeof(txt);
			void *pcaller = NULL;

#if defined(DEBUG_MEMORYPOOL_TRACE)
			pcaller = mem_entry->caller;
#endif /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
			for (i = _size - 8; i < mem_entry->size; i++) {
				int len =
					snprintf(ptxt, txt_len, "%c%02x",
						 i == _size ? '|' : ' ', p[i]);

				if (len >= txt_len)
					break;
				txt_len -= len, ptxt += len;
			}
			pr_err("%s Memory-Entry %p(ptr=%p)(%pS) size=%u(%u) corrupt %p:%s\n",
			       prefix, mem_entry, mem_entry + 1, pcaller,
			       mem_entry->demand_size, mem_entry->size, &p[i],
			       txt);
			if (dump)
				dump_stack();
			return 1;
		}
	}
#endif /*--- #if defined(MEMORY_CORRUPT_DEBUG) ---*/
	return 0;
}

/**
 */
struct _memory_pool {
#if defined(__KERNEL__)
	struct list_head list;
#endif /*--- #if defined(__KERNEL__) ---*/
	atomic_t linkcount;
	spinlock_t memlock;
	unsigned long alloc_size;
	unsigned long pool_size;
	unsigned long act_used;
	unsigned long act_entries;
	unsigned long max_used;
	unsigned long failed;
	unsigned long poolerror;
	struct _memory_list *memlist_anchor;
	char *poolname;
	struct _memory_pool *next;
};

/**
 * fuer PROC_FS
 */
struct _memorypool_proc {
	loff_t pos;

	/*--- beim Dump ermittelte Anzahl der Eintraege ---*/
	unsigned long entries;

	/*--- reservierte Eintraege (muss nicht mit entries uebereinstimmen) ---*/
	unsigned long table_entries;

	/*--- beim Dump ermittelte maxfree_size ---*/
	unsigned long maxfree_size;

	/*--- beim Dump ermittelte groesste Blocksize ---*/
	unsigned long maxblock_size;

	/*--- beim Dump ermittelte pool_size ---*/
	unsigned long pool_size;

	/*--- beim Dump ermittelte Anzahl der belegten Eintraege ---*/
	unsigned long used;

	struct _memory_list *memtable;
	struct _memory_pool *pmp;
};

#if defined(__KERNEL__)
static struct semaphore mempool_list_sema;
static LIST_HEAD(mempool_list);
#endif /*--- #if defined(__KERNEL__) ---*/

/**
 */
static inline void check_complete_memory_pool(struct _memory_pool *pmp)
{
#if defined(MEMORY_CORRUPT_DEBUG)
	char txt[128];
	struct _memory_list *memlist;

	memlist = pmp->memlist_anchor;
	snprintf(txt, sizeof(txt), "%s invalid memorypool(%s)-entry\n",
		 __func__, pmp->poolname);
	while (memlist) {
		unsigned int use = atomic_read(&memlist->used);

		if (CHECK_MEMORY_USED(use) && check_memory(txt, memlist, 0)) {
			break;
		}
		memlist = memlist->u.next;
	}
#endif /*--- #if defined(MEMORY_CORRUPT_DEBUG) ---*/
}

/**
 */
static void register_memory_pool(struct _memory_pool *pmp)
{
#if defined(__KERNEL__)
	if (down_interruptible(&mempool_list_sema)) {
		return;
	}
	list_add(&pmp->list, &mempool_list);
#if defined(CONFIG_PROC_FS)
	if (mempoolprocdir == NULL) {
		mempoolprocdir = proc_mkdir(PROC_MEMPOOLDIR, NULL);
	}
	if (mempoolprocdir) {
		proc_create(pmp->poolname, 0440, mempoolprocdir,
			    &mempoolstat_fops);
	}
	up(&mempool_list_sema);
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#endif /*--- #if defined(__KERNEL__) ---*/
}

#if defined(__KERNEL__)
/**
 */
static struct _memory_pool *get_memory_pool_by_name(const char *poolname)
{
	struct _memory_pool *pmp;

	list_for_each_entry(pmp, &mempool_list, list) {
		if (strcmp(pmp->poolname, poolname) == 0) {
			return pmp;
		}
	}
	return NULL;
}
#endif /*--- #if defined(__KERNEL__) ---*/

/**
 */
static void release_memory_pool(struct _memory_pool *pmp)
{
#if defined(__KERNEL__)

	if (down_interruptible(&mempool_list_sema)) {
		return;
	}
	list_del(&pmp->list);
#if defined(CONFIG_PROC_FS)
	if (mempoolprocdir) {
		/*--- pr_err("%s: remove %s %s\n", __func__, pmp->poolname, mempoolprocdir); ---*/
		remove_proc_entry(pmp->poolname, mempoolprocdir);
	}
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
	up(&mempool_list_sema);
#endif /*--- #if defined(__KERNEL__) ---*/
}

#if defined(__KERNEL__)
/**
 */
static void simplemempool_dump_all(void)
{
	struct _memory_pool *pmp;

	if (down_interruptible(&mempool_list_sema)) {
		return;
	}
	list_for_each_entry(pmp, &mempool_list, list) {
		simplemempool_dump(pmp);
	}
	up(&mempool_list_sema);
}

/**
 */
#if defined(MY_TESTMEMPOOL)
static void simplemempool_check_all(void)
{
	struct _memory_pool *pmp;

	if (down_interruptible(&mempool_list_sema)) {
		return;
	}
	list_for_each_entry(pmp, &mempool_list, list) {
		pr_err("check: mempool: %s act_used=%lu\n", pmp->poolname,
		       pmp->act_used);
		check_complete_memory_pool(pmp);
	}
	up(&mempool_list_sema);
}
#endif
#endif /*--- #if defined(__KERNEL__) ---*/
/**
 */
static int dump_memory_pool(const char *prefix, struct _memory_pool *pmp,
			    struct _memorypool_proc **_pmp_proc,
			    unsigned int minimal)
{
	struct _memorypool_proc *pmp_proc = NULL;
	struct _memory_list *memlist, *memprev = NULL, *memtable = NULL;
	char txt[50];
	unsigned long greatest_entry = 0, greatest_blocksize = 0;
	unsigned long flags, i, entry_idx = 0, used = 0, maxfree_size = 0,
				pool_size = 0;
	unsigned long table_elements = pmp->act_entries + 100;

	check_complete_memory_pool(pmp);
	if (!is_rte_context()) {
		pmp_proc = kzalloc(sizeof(struct _memorypool_proc) +
					   sizeof(struct _memory_list) *
						   table_elements,
				   GFP_ATOMIC);
		if (pmp_proc == NULL) {
			return 1;
		}
		memtable = (struct _memory_list *)(pmp_proc + 1);
	}
	rte_spin_lock_irqsave(&pmp->memlock, flags);
	/*--- im geschuetzten Bereich schnell alle Eintrage uebertragen ---*/
	memlist = pmp->memlist_anchor;
	while (memlist) {
		unsigned int use = atomic_read(&memlist->used);

		if (CHECK_MEMORY_ENTRY(use) == 0) {
			DBG_ERR("%s error: invalid memorypool(%s)-entry %lx(size=%u(%u) next-entry %lx is invalid\n",
				__func__, pmp->poolname,
				MEMLIST_OFFSET(pmp, memlist),
				memlist->demand_size, memlist->size,
				MEMLIST_OFFSET(pmp, memlist->u.next));
			pmp->poolerror = 1;
			break;
		}
		if (memprev != memlist->prev) {
			DBG_ERR("%s error: invalid memorypool(%s)-entry %lx(size=%u(%u) prev-entry %lx != %lx (soll) is invalid\n",
				__func__, pmp->poolname,
				MEMLIST_OFFSET(pmp, memlist),
				memlist->demand_size, memlist->size,
				MEMLIST_OFFSET(pmp, memlist->prev),
				MEMLIST_OFFSET(pmp, memprev));
		}
		if (CHECK_MEMORY_USED(use)) {
			used++;
		} else if (maxfree_size < memlist->size) {
			maxfree_size = memlist->size;
		}
		if (CHECK_MEMORY_USED(use) &&
		    (memlist->size > greatest_blocksize)) {
			greatest_blocksize = memlist->size;
			greatest_entry = entry_idx;
		}
		if (memtable && (entry_idx < table_elements)) {
			memcpy(&memtable[entry_idx], memlist,
			       sizeof(struct _memory_list));
			memtable[entry_idx].u.memaddr =
#if defined(__KERNEL__)
				memlist + 1;
#else
				(void *)MEMLIST_OFFSET(
					pmp,
					memlist); /*--- only for debug ---*/
#endif /*--- #else ---*/
		}
		atomic_set(&memlist->used, use | 0x2);
		pool_size += memlist->size;
		memprev = memlist;
		memlist = memlist->u.next;
		entry_idx++;
	}
	rte_spin_unlock_irqrestore(&pmp->memlock, flags);

	if (_pmp_proc) {
		/**
		 * Ausgabe fuer proc-Device
		 */
		pmp_proc->pos = 0;
		pmp_proc->entries = entry_idx;
		pmp_proc->maxfree_size = maxfree_size;
		pmp_proc->maxblock_size = greatest_blocksize;
		pmp_proc->used = used;
		pmp_proc->table_entries = table_elements;
		pmp_proc->pmp = pmp;
		pmp_proc->pool_size = pool_size;
		pmp_proc->memtable = memtable;
		*_pmp_proc = pmp_proc;
		return 0;
	}
	/**
	 * Kernelausgabe
	 */
	if (prefix)
		pr_err("%s: %s(%lu) %s\n", prefix, pmp->poolname,
		       pmp->pool_size,
		       pmp->poolerror ? "corrupt memory-list" : "");
	for (i = 0; i < min(table_elements, entry_idx) && memtable; i++) {
		char *comm = "";
		unsigned int use;

		memlist = &memtable[i];
		use = atomic_read(&memlist->used);
		if (minimal) {
			if (CHECK_MEMORY_USED(use) == 0) {
				/*--- nicht die ungenutzten Bereiche ---*/
				continue;
			}
			if (minimal++ > 10) {
				/*--- ... limitiert ---*/
				table_elements = i;
				break;
			}
		}
#if defined(MY_TESTMEMPOOL)
		comm = memlist->comm_name;
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
#if defined(DEBUG_MEMORYPOOL_TRACE)
		{
			int is_kstrdup = (((unsigned long)memlist->caller -
					   (unsigned long)kstrdup) < 0x40UL) &&
					 CHECK_MEMORY_USED(use);

			pr_err("[%4lu] %p used=%u %s size=%5u(%5u) (%pS) %s %s",
			       i, memlist->u.memaddr, CHECK_MEMORY_USED(use),
			       CHECK_MEMORY_READED(use) ? "   " : "NEW",
			       memlist->demand_size, memlist->size,
			       memlist->caller, comm, is_kstrdup ? "" : "\n");
			if (is_kstrdup) {
				pr_info(" '%s'\n", (char *)memlist->u.memaddr);
			}
		}
#else /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
		pr_err("[%4lu] %p used=%u %s size=%5u(%5u)%s\n", i,
		       memlist->u.memaddr, CHECK_MEMORY_USED(use),
		       CHECK_MEMORY_READED(use) ? "   " : "NEW",
		       memlist->demand_size, memlist->size, comm);
#endif /*--- #else ---*/ /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
	}
	if (table_elements < entry_idx) {
		pr_err("... further elements exist (%lu)\n",
		       entry_idx - table_elements);
	}
	if (pmp->failed) {
		snprintf(txt, sizeof(txt), "Alloc failed=%lu ", pmp->failed);
	} else {
		txt[0] = 0;
	}
	if (minimal && greatest_blocksize && memtable) {
		if (greatest_entry < table_elements) {
			char *comm = "";
			unsigned int use;

			memlist = &memtable[greatest_entry];
			use = atomic_read(&memlist->used);
#if defined(MY_TESTMEMPOOL)
			comm = memlist->comm_name;
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
#if defined(DEBUG_MEMORYPOOL_TRACE)
			{
				int is_kstrdup =
					(((unsigned long)memlist->caller -
					  (unsigned long)kstrdup) < 0x40UL) &&
					CHECK_MEMORY_USED(use);

				pr_err("[maxb] %p used=%u %s size=%5u(%5u) (%pS) %s %s",
				       memlist->u.memaddr,
				       CHECK_MEMORY_USED(use),
				       CHECK_MEMORY_READED(use) ? "   " : "NEW",
				       memlist->demand_size, memlist->size,
				       memlist->caller, comm,
				       is_kstrdup ? "" : "\n");
				if (is_kstrdup) {
					pr_info(" '%s'\n",
						(char *)memlist->u.memaddr);
				}
			}
#else /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
			pr_err("[maxb] %p used=%u %s size=%5u(%5u)%s\n",
			       memlist->u.memaddr, CHECK_MEMORY_USED(use),
			       CHECK_MEMORY_READED(use) ? "   " : "NEW",
			       memlist->demand_size, memlist->size, comm);
#endif /*--- #else ---*/ /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
		} else {
			pr_err("[maxb] idx=%lu size=%5lu\n", greatest_entry,
			       greatest_blocksize);
		}
	}
	pr_err("Summary: entries=%lu used=%lu max-freesize=%lu max-blocksize=%lu max-used=%lu %scalculated pool_size=%lu%s\n",
	       entry_idx, used, maxfree_size, greatest_blocksize, pmp->max_used,
	       txt, pool_size,
	       pool_size != pmp->pool_size ? " Error: Poolsize mismatch" : "");
	kfree(pmp_proc);
	return 1;
}

/**
 */
static inline int check_memory_entry(struct _memory_list *mem_entry,
				     struct _memory_pool *pmp)
{
	if (mem_entry->u.next) {
		if ((unsigned char *)mem_entry->u.next !=
		    (unsigned char *)mem_entry + mem_entry->size) {
			return 1;
		}
		return 0;
	}
	if (mem_entry->size + ((unsigned long)mem_entry -
			       (unsigned long)pmp->memlist_anchor) >
	    pmp->pool_size) {
		return 2;
	}
	return 0;
}

#define PTR_IN_AREA(ptr, pool, size)                                           \
	((((void *)(ptr) >= (void *)(pool)) &&                                 \
	  ((void *)(ptr) < (void *)((unsigned long)(pool) + (size)))) ?        \
		 1 :                                                           \
		 0)
/**
 * Alloziert den notwendigen Block aus dem Memory-Pool
 * memlist: Memory-Liste (ab hier einhaengen)
 * size:           angeforderte Groesse
 * best_match:     suche Block mit passender Groesse
 * caller:         Caller fuer Tracing
 *
 * Prinzip: der zu allozierende Block wird am Ende von einem passenden freien Block reserviert
 */
static struct _memory_list *get_memory(struct _memory_pool *pmp,
				       struct _memory_list *memlist,
				       unsigned int size,
				       unsigned int best_match,
				       void *caller)
{
	struct _memory_list *memlistmatch = NULL;
	unsigned int minrest_size = (unsigned int)-1;
	unsigned int needed_size;

	needed_size = ALIGN_SIZE(size + sizeof(struct _memory_list) +
				 MEMORY_GAP_SIZE);
	DBG_TRC("%s:'%s' %p best_match=%u firstsize=%u size=%u + %u = %u\n",
		__func__, pmp->poolname, memlist, best_match, memlist->size,
		size, (unsigned int)sizeof(struct _memory_list), needed_size);

	/*--- check_complete_memory_pool(pmp); ---*/
	while (memlist) {
		unsigned int use = atomic_read(&memlist->used);

		if (CHECK_MEMORY_ENTRY(use) == 0) {
			DBG_ERR("%s error#1: invalid memorypool(%s)-entry %p(size=%u(%u) (%pS) next-entry %p is invalid\n",
				__func__, pmp->poolname, memlist,
				memlist->demand_size, memlist->size, caller,
				memlist->u.next);
			return NULL;
		}
		if ((CHECK_MEMORY_USED(use) == 0) &&
		    ((memlist->size >= needed_size))) {
			struct _memory_list *next, *prev;
			unsigned int rest_size = memlist->size - needed_size;

			if (best_match && rest_size) {
				if (rest_size < minrest_size) {
					minrest_size = rest_size;
					memlistmatch = memlist;
				}
				DBG_TRC("%s best_match: memlistsize=%u rest_size=%u minrest_size=%u memlistmatch=%p\n",
					__func__, memlist->size, rest_size,
					minrest_size, memlistmatch);
				memlist = memlist->u.next;
				continue;
			} else if (best_match) {
				DBG_TRC("%s best_match with size=0\n",
					__func__);
			}
			next = memlist->u.next; /*--- aktuellen next merken ---*/
			if (rest_size > sizeof(struct _memory_list)) {
				/*--- vorneweg der (reduzierte) freie Bereich ---*/
				DBG_TRC("%s memlistsize=%u rest_size=%u\n",
					__func__, memlist->size, rest_size);
				if (check_memory_entry(memlist, pmp)) {
					DBG_ERR("%s error#2: invalid memorypool(%s)-entry %p(size=%u(%u) (%pS) next-entry %p is invalid\n",
						__func__, pmp->poolname,
						memlist, memlist->demand_size,
						memlist->size, caller,
						memlist->u.next);
					return NULL;
				}
				memlist->size = rest_size;
				prev = memlist;
				memlist->u.next = (struct _memory_list *)
						 ((char *)memlist +
						    memlist->size);
				DBG_TRC("%s memlistsize=%u rest_size=%u next=%p\n",
					__func__, memlist->size, rest_size,
					memlist->u.next);
				memlist = memlist->u.next;
				memlist->prev = prev;
				memlist->u.next = next;
				memlist->size = needed_size;
				pmp->act_entries++;
			}
			if (next) {
				next->prev = memlist;
			}
#if defined(DEBUG_MEMORYPOOL_TRACE)
			memlist->caller = caller;
#endif /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
#if defined(MY_TESTMEMPOOL)
			if (!is_rte_context()) {
				snprintf(memlist->comm_name,
					 sizeof(memlist->comm_name), "[%s]",
					 current->comm);
			} else {
				memlist->comm_name[0] = 0;
			}
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
			pmp->act_used += memlist->size;
			if (pmp->act_used > pmp->max_used) {
				pmp->max_used = pmp->act_used;
			}
			memlist->demand_size = size;
			check_memory_init(memlist);
			atomic_set(&memlist->used, MEMORYPOOL_MAGIC | 0x1);
			/*--- und nun hintendran der aktuelle Eintrag ---*/
			mb();
			return memlist;
		}
		if (memlist->u.next &&
		    !PTR_IN_AREA(memlist->u.next, pmp->memlist_anchor,
				 pmp->pool_size)) {
			void *_caller = NULL;

#if defined(DEBUG_MEMORYPOOL_TRACE)
			_caller = memlist->caller;
#endif /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
			DBG_ERR("%s error#3: invalid memorypool(%s)-entry %p(size=%u(%u) (%pS) next-entry %p is invalid\n",
				__func__, pmp->poolname, memlist,
				memlist->demand_size, memlist->size, _caller,
				memlist->u.next);
			return NULL;
		}
		memlist = memlist->u.next;
	}
	if (best_match && memlistmatch) {
		return get_memory(pmp, memlistmatch, size, 0, caller);
	}
	DBG_TRC("%s error: can't reserve memory for size=%u (%pS)\n", __func__,
		size, caller);
	return NULL;
}

/**
 * Gibt den Block wieder an den Memory-Pool
 * Dabei wird automatisch defragmentiert, in dem er mit evtl. davorliegenden und nachfolgende freien Block
 * verbunden wird
 */
static int put_memory(struct _memory_pool *pmp, struct _memory_list *entry)
{
	struct _memory_list *memlist = entry;
	unsigned int use = atomic_read(&memlist->used);

	check_complete_memory_pool(pmp);

	DBG_TRC("%s(%p)\n", __func__, entry);
	if (CHECK_MEMORY_ENTRY(use) == 0) {
		char *name = NULL;

		if (!is_rte_context()) {
			name = current->comm;
		}
		DBG_ERR("%s error: invalid memorypool(%s)-entry %p [%s]\n",
			__func__, pmp->poolname, memlist, name ? name : "");
		return -1;
	}
	if (CHECK_MEMORY_USED(use)) {
		struct _memory_list *memnext, *memprev = memlist->prev;

		check_memory(__func__, memlist, 1);
		pmp->act_used -= memlist->size;

		if (memprev &&
		    CHECK_MEMORY_USED(atomic_read(&memprev->used)) == 0) {
			/*--- verbinde den Vorgaenger mit aktuellen Eintrag  ---*/
			memprev->size += memlist->size;
			memprev->u.next = memlist->u.next;
			atomic_set(&memlist->used, 0);
			memlist = memprev;
			pmp->act_entries--;
		}
		memnext = memlist->u.next;
		if (memnext) {
			if (CHECK_MEMORY_USED(atomic_read(&memnext->used)) ==
			    0) {
				/*--- verbinde den Nachfolger mit aktuellen Eintrag  ---*/
				memlist->size += memnext->size;
				memnext = memnext->u.next;
				memlist->u.next = memnext;
				atomic_set(&memlist->used, 0);
				pmp->act_entries--;
			}
			if (memnext) {
				memnext->prev = memlist;
			}
		}
		atomic_set(&memlist->used, MEMORYPOOL_MAGIC);
		mb();
		return 0;
	}
	return -2;
}

/**
 */
static struct _memory_pool *alloc_pool(unsigned int alloc_size)
{
	struct _memory_pool *pmp =
		alloc_pages_exact(alloc_size, GFP_KERNEL | __GFP_ZERO);

	if (pmp) {
		pmp->alloc_size = alloc_size;
	}
	return pmp;
}

/**
 * ret: Handle auf den Memory-Pool
 */
void *simplemempool_alloc_init(const char *name, unsigned int pool_size)
{
	unsigned int header_len;
	unsigned int name_len = 1;
	struct _memory_list *anchor;
	struct _memory_pool *pmp = NULL;

	if (is_rte_context()) {
		pr_err("\n%s GREAT-ERROR: do not use in Yield-Context\n",
		       __func__);
		return NULL;
	}
	if (name) {
		name_len = strlen(name) + 1;
	}
	header_len = ALIGN_SIZE(sizeof(struct _memory_pool) + name_len) +
		     max(sizeof(struct _memory_list), (1U << ALIGN_SHIFT));
	pool_size = ALIGN_SIZE(pool_size);
	pmp = alloc_pool(pool_size + header_len);
	DBG_TRC("%s(%u)\n", __func__, pool_size);
	if (pmp == NULL) {
		return NULL;
	}
	pmp->pool_size = pool_size;
	pmp->poolname = (char *)(pmp + 1);
	rte_spin_lock_init(&pmp->memlock);
	if (name) {
		strcpy(pmp->poolname, name);
	}
	anchor = (struct _memory_list *)
			 ((unsigned long)pmp + header_len -
			    sizeof(struct _memory_list)); /* somit sind alle Memory-Startdressen auf ALIGN_SIZE() aligned */
	anchor->size = pmp->pool_size;
	atomic_set(&anchor->used, MEMORYPOOL_MAGIC);
	pmp->memlist_anchor = anchor;
	DBG_TRC("%s pmp %p %s anchor %p(%p) header_len=%u pool_size=%lu\n", __func__,
		pmp, pmp->poolname, pmp->memlist_anchor, (pmp->memlist_anchor + 1),
		header_len, pmp->pool_size);
	atomic_set(&pmp->linkcount, 1);
	register_memory_pool(pmp);
	return pmp;
}
EXPORT_SYMBOL(simplemempool_alloc_init);

/**
 */
void simplemempool_alloc_exit(void *_pmp)
{
	struct _memory_pool *pmp = (struct _memory_pool *)_pmp;
	int try_cnt = 5;

	if (is_rte_context()) {
		pr_err("\n%s GREAT-ERROR: do not use in Yield-Context\n",
		       __func__);
		return;
	}
	if (pmp) {
		release_memory_pool(pmp);
		atomic_sub(1, &pmp->linkcount);
		while ((atomic_read(&pmp->linkcount) > 0) && try_cnt--) {
			schedule_timeout(HZ / 10);
		}
		if (atomic_read(&pmp->linkcount)) {
			dump_memory_pool(
				"Error - further reference on Memorypool exist",
				pmp, NULL, 0);
		}
		free_pages_exact(pmp, pmp->alloc_size);
	}
}
EXPORT_SYMBOL(simplemempool_alloc_exit);

/**
 */
static void simplemempool_dump(void *_pmp)
{
	struct _memory_pool *pmp = (struct _memory_pool *)_pmp;

	if (!is_rte_context() && pmp) {
		dump_memory_pool("Memorypool", pmp, NULL, 0);
	}
}

/**
 */
void *simplemempool_alloc(void *_pmp, unsigned int size, int zero, void *caller)
{
	struct _memory_pool *pmp = (struct _memory_pool *)_pmp;
	struct _memory_list *mementry;
	unsigned char *ptr;
	unsigned int best_match;
	unsigned long flags;

	DBG_TRC("%s(%u)\n", __func__, size);
	if (!unlikely(pmp)) {
		return NULL;
	}
	atomic_add(1, &pmp->linkcount);
	rte_spin_lock_irqsave(&pmp->memlock, flags);
	/*--- best-Match-Suche, wenn Erster nicht frei bzw Erster kleiner als die Haelfte des Pools ---*/
	best_match =
		CHECK_MEMORY_USED(atomic_read(&pmp->memlist_anchor->used)) ||
		(pmp->memlist_anchor->size < (pmp->pool_size >> 1));
	/*--- best_match = 1; ---*/
	mementry =
		get_memory(pmp, pmp->memlist_anchor, size, best_match, caller);
	rte_spin_unlock_irqrestore(&pmp->memlock, flags);
	if (mementry == NULL) {
		char txt[256];

		dump_stack();
		snprintf(txt, sizeof(txt),
			 "%s: [%s] Error: Can't reserve Memory size=%u %pS\n",
			 __func__, pmp->poolname, size, caller);
		if (pmp->failed++ < 10) {
			dump_memory_pool(txt, pmp, NULL, 1);
		} else {
			rte_panic(txt);
		}
		atomic_sub(1, &pmp->linkcount);
		return mementry;
	}
	ptr = (unsigned char *)(mementry + 1);
#if defined(MEMORY_CORRUPT_DEBUG)
	/*--- pr_err("%s:MemoryPool(%s) %pS entry %p(%p) size=%u(%u)\n", __func__, pmp->poolname, caller, mementry, ptr, mementry->demand_size, mementry->size); ---*/
#endif /*--- #if defined(MEMORY_CORRUPT_DEBUG) ---*/
	if (zero) {
		memset(ptr, 0, size);
	}
	return ptr;
}
EXPORT_SYMBOL(simplemempool_alloc);

/**
 */
void simplemempool_free(void *_pmp, const void *ptr, void *caller)
{
	struct _memory_pool *pmp = (struct _memory_pool *)_pmp;
	struct _memory_list *mementry;
	int ret;
	unsigned long flags;

	if (ptr == NULL) {
		return;
	}
	if (!unlikely(pmp)) {
		return;
	}
	mementry = ((struct _memory_list *)ptr) - 1;
	DBG_TRC("%s(%p)\n", __func__, mementry);
	rte_spin_lock_irqsave(&pmp->memlock, flags);
	ret = put_memory(pmp, mementry);
	rte_spin_unlock_irqrestore(&pmp->memlock, flags);
	if (ret) {
		int in_pool_area =
			PTR_IN_AREA(ptr, pmp->memlist_anchor, pmp->pool_size);

		pr_err("%s: Can't find ptr=%p %s(pool-area: %p - size=%lu)  %pS\n",
		       __func__, ptr, in_pool_area ? "" : "outside pool! ",
		       pmp->memlist_anchor, pmp->pool_size, caller);
		if (in_pool_area) {
			simplemempool_dump(_pmp);
		}
	} else {
		atomic_sub(1, &pmp->linkcount);
	}
}
EXPORT_SYMBOL(simplemempool_free);

/**
 */
int simplemempool_size(void *_pmp, const void *ptr)
{
	struct _memory_pool *pmp = (struct _memory_pool *)_pmp;
	struct _memory_list *mementry;
	unsigned long flags;
	int size = -1;
	unsigned int use;

	if (ptr == NULL) {
		return size;
	}
	if (!unlikely(pmp)) {
		return size;
	}
	mementry = ((struct _memory_list *)ptr) - 1;
	rte_spin_lock_irqsave(&pmp->memlock, flags);
	use = atomic_read(&mementry->used);
	if (CHECK_MEMORY_USED(use)) {
		size = mementry->demand_size;
	} else {
		check_memory(__func__, mementry, 1);
	}
	rte_spin_unlock_irqrestore(&pmp->memlock, flags);
	return size;
}
EXPORT_SYMBOL(simplemempool_size);

#if defined(__KERNEL__)
/**
 */
static struct _memory_pool *get_mempool_by_ptr(const void *ptr)
{
	struct _memory_pool *pmp;

	list_for_each_entry(pmp, &mempool_list, list) {
		atomic_add(1, &pmp->linkcount);
		if (PTR_IN_AREA(ptr, pmp->memlist_anchor, pmp->pool_size)) {
			return pmp;
		}
		atomic_sub(1, &pmp->linkcount);
	}
	return NULL;
}

/**
 */
static void put_mempool(struct _memory_pool *pmp)
{
	atomic_sub(1, &pmp->linkcount);
}

/*
 * @brief get simplemempool-area if addr in range
 * @param  flag 1 freed, 2 range is mempool-ctrl, 3 range is outside demand-size
 * @return start (zero if not exist)
 */
unsigned long get_simplemempool_area(unsigned long addr, unsigned long *caller,
				     char *mempool_name,
				     unsigned int mempool_name_size,
				     unsigned long *size, int *flag)
{
	struct _memory_pool *pmp;
	unsigned long flags, _size = 0, _caller = 0, _flag = 0;
	struct _memory_list *memlist;

	pmp = get_mempool_by_ptr((void *)addr);
	if (pmp == NULL) {
		return 0;
	}
	if (!rte_spin_trylock_irqsave(&pmp->memlock, flags)) {
		put_mempool(pmp);
		return 0;
	}
	if (mempool_name) {
		snprintf(mempool_name, mempool_name_size, "%s", pmp->poolname);
	}
	memlist = pmp->memlist_anchor;
	while (memlist) {
		unsigned int use = atomic_read(&memlist->used);

		if (PTR_IN_AREA(addr, memlist, memlist->size)) {
			if (PTR_IN_AREA(addr, (memlist), sizeof(*memlist))) {
				_flag = 2; /*--- range is mempool-ctrl ---*/
				_size = (unsigned long)memlist->size -
					sizeof(*memlist);
			} else if (!CHECK_MEMORY_USED(use)) {
				_flag = 1; /*--- range is freed ---*/
				_size = (unsigned long)memlist->size -
					sizeof(*memlist);
			} else if (!PTR_IN_AREA(addr, (memlist + 1),
						memlist->demand_size)) {
				_flag = 3; /*--- range is outside demand_size ---*/
				_size = (unsigned long)memlist->demand_size;
			} else {
				_size = (unsigned long)memlist->demand_size;
			}
			addr = (unsigned long)(memlist + 1);
#if defined(DEBUG_MEMORYPOOL_TRACE)
			_caller = (unsigned long)memlist->caller;
#endif /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
			/*--- pr_err("%s: %u: addr=%lx freed=%lx size=%lu\n", __func__, __LINE__, addr, _flag, _size); ---*/
			rte_spin_unlock_irqrestore(&pmp->memlock, flags);
			put_mempool(pmp);
			if (caller)
				*caller = _caller;
			if (flag)
				*flag = _flag;
			if (size)
				*size = _size;
			return addr;
		}
		memlist = memlist->u.next;
	}
	rte_spin_unlock_irqrestore(&pmp->memlock, flags);
	put_mempool(pmp);
	if (flag)
		*flag = 1;
	if (size)
		*size = 0;
	if (caller)
		*caller = 0;
	return (unsigned long)(pmp->memlist_anchor + 1);
}

#if defined(MY_TESTMEMPOOL)
void *TestPoolHandle[8];
unsigned int gTestMempool;

#define MIN_MEMPOOL_SIZE_SHIFT 6
/**
 */
static unsigned int size_to_idx(unsigned int size)
{
	unsigned int idx = fls(size - 1);

	if (idx <= MIN_MEMPOOL_SIZE_SHIFT) {
		return 0;
	}
	idx -= MIN_MEMPOOL_SIZE_SHIFT;
	if (idx >= ARRAY_SIZE(TestPoolHandle)) {
		idx = ARRAY_SIZE(TestPoolHandle) - 1;
	}
	return idx;
}
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
/**
 */
static void cmdline_parse(char *string, void *refdata)
{
#if defined(MY_TESTMEMPOOL)
	if (strstr(string, "deact")) {
		pr_err("deactivate Mempool\n");
		gTestMempool = 0;
		return;
	} else if (strstr(string, "act")) {
		pr_err("activate Mempool\n");
		gTestMempool = 1;
		return;
	} else if (strstr(string, "check")) {
		pr_err("check all Mempools\n");
		simplemempool_check_all();
		return;
	}
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
	simplemempool_dump_all();
}

/*--- #define SEPERATE_MEMPOOL ---*/
#if defined(SEPERATE_MEMPOOL)
static void *SeperateMalloc;

void *__malloc2(unsigned long size, gfp_t gfp_mask, pgprot_t prot)
{
	return simplemempool_alloc(SeperateMalloc, size,
				   (gfp_mask & __GFP_ZERO) ? 1 : 0,
				   (void *)_RET_IP_);
}
EXPORT_SYMBOL(__malloc2);

void free2(void *ptr)
{
	simplemempool_free(SeperateMalloc, ptr, (void *)_RET_IP_);
}
EXPORT_SYMBOL(free2);
#endif /*--- #if defined(SEPERATE_MEMPOOL) ---*/

/**
 */
static int __init avm_mempool_init(void)
{
	sema_init(&mempool_list_sema, 1);
	avm_DebugCallRegister("mempool", cmdline_parse, NULL);

#if defined(MY_TESTMEMPOOL)
	{
		unsigned int i;
		unsigned long part_reserve_size = (1 << 22) - (1 << 16);

		for (i = 0; i < ARRAY_SIZE(TestPoolHandle) - 1; i++) {
			char txt[32];

			/*--- pr_err("%s: %lx %lu\n", __func__, bootmemreserved, part_reserve_size); ---*/
			sprintf(txt, "size-%u",
				1 << (MIN_MEMPOOL_SIZE_SHIFT + i));
			TestPoolHandle[i] = simplemempool_alloc_init(
				txt, part_reserve_size);
			pr_err("%s:[%u] cache-%u size=%lu Testpool=%p\n",
			       __func__, i, (1 << (i + MIN_MEMPOOL_SIZE_SHIFT)),
			       part_reserve_size, TestPoolHandle[i]);
		}
	}
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
#if defined(SEPERATE_MEMPOOL)
	SeperateMalloc = simplemempool_alloc_init("SEPERATE_MALLOC", 1 << 21);
#endif /*--- #if defined(SEPERATE_MEMPOOL) ---*/
	return 0;
}

late_initcall(avm_mempool_init);
#if defined(MY_TESTMEMPOOL)
/**
 * ret: NULL nicht in diesem Mempool angelegt
 */
void *my_kmalloc(unsigned int size, int flag, unsigned long caller)
{
	if (gTestMempool && TestPoolHandle[size_to_idx(size)]) {
		return simplemempool_alloc(TestPoolHandle[size_to_idx(size)],
					   size, (flag & __GFP_ZERO) ? 1 : 0,
					   (void *)caller);
	}
	return NULL;
}
EXPORT_SYMBOL(my_kmalloc);

/**
 * ret:0 konnte aus diesem Mempool freigegeben werden
 */
int my_kfree(const void *ptr, unsigned long caller)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(TestPoolHandle); i++) {
		struct _memory_pool *pmp =
			(struct _memory_pool *)TestPoolHandle[i];

		if (TestPoolHandle[i] &&
		    PTR_IN_AREA(ptr, pmp->memlist_anchor, pmp->pool_size)) {
			simplemempool_free(TestPoolHandle[i], ptr,
					   (void *)caller);
			return 0;
		}
	}
	return 1;
}
EXPORT_SYMBOL(my_kfree);

/**
 * ret: 0 nicht in diesem Mempool
 * ret: < 0 corrupt Mempointer
 */
int my_ksize(const void *ptr)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(TestPoolHandle); i++) {
		struct _memory_pool *pmp =
			(struct _memory_pool *)TestPoolHandle[i];

		if (TestPoolHandle[i] &&
		    PTR_IN_AREA(ptr, pmp->memlist_anchor, pmp->pool_size)) {
			return simplemempool_size(pmp, ptr);
		}
	}
	return 0;
}
EXPORT_SYMBOL(my_ksize);

/**
 * ret: NULL nicht in diesem Mempool angelegt
 */
void *my_krealloc(const void *p, size_t new_size, gfp_t flags,
		  unsigned long caller)
{
	unsigned int i;
	void *ret;

	if (p == NULL) {
		p = my_kmalloc(new_size, flags, caller);
		if (p) {
			return (void *)p;
		}
	}
	for (i = 0; i < ARRAY_SIZE(TestPoolHandle); i++) {
		struct _memory_pool *pmp =
			(struct _memory_pool *)TestPoolHandle[i];

		if (TestPoolHandle[i] &&
		    PTR_IN_AREA(p, pmp->memlist_anchor, pmp->pool_size)) {
			int ks = my_ksize(p);

			if (ks >= (int)new_size) {
				return (void *)p;
			}
			ret = my_kmalloc(new_size, flags, caller);
			if (ret && p) {
				memcpy(ret, p, ks);
			}
			return ret;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(my_krealloc);

#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
#if defined(CONFIG_PROC_FS)
/**
 * Returns false if pos at or past end of file.
 */
static int update_iter(struct _memorypool_proc *iter, loff_t pos)
{
	iter->pos = pos;
	if (iter->pos >= min(iter->entries, iter->table_entries)) {
		return 0;
	}
	return 1;
}

/**
 */
static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	(*pos)++;

	if (!update_iter(m->private, *pos)) {
		return NULL;
	}
	return p;
}

/**
 */
static void *s_start(struct seq_file *m, loff_t *pos)
{
	if (!update_iter(m->private, *pos)) {
		return NULL;
	}
	return m->private;
}

/**
 */
static void s_stop(struct seq_file *m, void *p)
{
}

/**
 */
static int s_show(struct seq_file *m, void *p)
{
	char *comm = "";
	unsigned int use, is_kstrdup;
	struct _memorypool_proc *iter = (struct _memorypool_proc *)m->private;
	struct _memory_list *memlist = &iter->memtable[iter->pos];

	if (unlikely(iter->pos == 0)) {
		seq_printf(m, "%s(%lu) %s\n", iter->pmp->poolname,
			   iter->pmp->pool_size,
			   iter->pmp->poolerror ? "corrupt memory-list" : "");
	}
	use = atomic_read(&memlist->used);
#if defined(MY_TESTMEMPOOL)
	comm = memlist->comm_name;
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/
#if defined(DEBUG_MEMORYPOOL_TRACE)
	is_kstrdup = (((unsigned long)memlist->caller -
		       (unsigned long)kstrdup) < 0x50UL) &&
		     CHECK_MEMORY_USED(use);
	seq_printf(m, "[%4lu] %p used=%u %s size=%5u(%5u) (%pS) %s %s",
		   (unsigned long)iter->pos, memlist->u.memaddr,
		   CHECK_MEMORY_USED(use),
		   CHECK_MEMORY_READED(use) ? "   " : "NEW",
		   memlist->demand_size, memlist->size, memlist->caller, comm,
		   is_kstrdup ? "" : "\n");
	if (is_kstrdup) {
		seq_printf(m, " '%s'\n", (char *)memlist->u.memaddr);
	}
#else /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/
	seq_printf(m, "[%4lu] %p used=%u %s size=%5u(%5u) %s\n",
		   (unsigned long)iter->pos, memlist->u.memaddr,
		   CHECK_MEMORY_USED(use),
		   CHECK_MEMORY_READED(use) ? "   " : "NEW",
		   memlist->demand_size, memlist->size, comm);
#endif /*--- #else ---*/ /*--- #if defined(DEBUG_MEMORYPOOL_TRACE) ---*/

	if (iter->pos &&
	    (iter->pos >= min(iter->entries, iter->table_entries) - 1)) {
		char txt[50];

		if (iter->pmp->failed) {
			snprintf(txt, sizeof(txt), "Alloc failed=%lu ",
				 iter->pmp->failed);
		} else {
			txt[0] = 0;
		}
		if (iter->table_entries < iter->entries) {
			seq_printf(m, "... further elements exist (%lu)\n",
				   iter->entries - iter->table_entries);
		}
		seq_printf(
			m,
			"Summary: entries=%lu used=%lu max-freesize=%lu max-blocksize=%lu max-used=%lu %scalculated pool_size=%lu%s\n",
			iter->entries, iter->used, iter->maxfree_size,
			iter->maxblock_size, iter->pmp->max_used, txt,
			iter->pool_size,
			iter->pool_size != iter->pmp->pool_size ?
				" Error: Poolsize mismatch" :
				"");
	}
	return 0;
}

/**
 */
static const struct seq_operations mempool_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show
};

/**
 */
static int mempooldump_open(struct inode *inode, struct file *file)
{
	struct _memory_pool *pmp;
	char *poolname;
	struct _memorypool_proc *iter;
	int ret;

	poolname = file->f_path.dentry ?
			   (char *)file->f_path.dentry->d_name.name :
			   "";

	if (down_interruptible(&mempool_list_sema)) {
		return -EBUSY;
	}
	pmp = get_memory_pool_by_name(poolname);
	if (pmp == NULL) {
		up(&mempool_list_sema);
		return -EINVAL;
	}
	if (dump_memory_pool("", pmp, &iter, 0)) {
		up(&mempool_list_sema);
		return -ENOMEM;
	}
	/*
	 * We keep iterator in m->private, since normal case is to
	 * s_start from where we left off, so we avoid doing
	 */
	ret = seq_open(file, &mempool_op);
	((struct seq_file *)file->private_data)->private = iter;
	if (ret) {
		up(&mempool_list_sema);
	}
	return ret;
}

/**
 */
static int mempooldump_close(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;

	kfree(seq->private);
	seq->private = NULL;
	up(&mempool_list_sema);
	return seq_release(inode, file);
}

static const struct proc_ops mempoolstat_fops = {
	.proc_open = mempooldump_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = mempooldump_close,
};
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/

#endif /*--- #if defined(__KERNEL__) ---*/

/**
 * gcc simple_mempool.c -Wall -g -m32 -DCONFIG_AVM_IPI_YIELD -Dno_printk=no_printf -o testmemorypool
 */
#if !defined(__KERNEL__)
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

/**
 * time in msec
 */
char *human_time(char *buf, double time)
{
	if (time < 1e-6) {
		sprintf(buf, "%lf psec", time * 1e9);
	} else if (time < 1e-3) {
		sprintf(buf, "%lf nsec", time * 1e6);
	} else if (time < 1e-0) {
		sprintf(buf, "%lf usec", time * 1e3);
	} else if (time < 1e3) {
		sprintf(buf, "%lf msec", time * 1e0);
	} else {
		sprintf(buf, "%lf sec", time / 1e3);
	}
	return buf;
}

unsigned char *p[300000];

int main(int arc, char **argv)
{
	char buf[64];
	long tta, tte;
	struct timeval ta, te;
	void *pmp;
	unsigned int i, idx, size, cnt;
	unsigned long ops;

	pmp = simplemempool_alloc_init("TestPool", 1 << 18);
	simplemempool_dump(pmp);

	gettimeofday(&ta, NULL);
	tta = (ta.tv_sec * 1000) + ta.tv_usec / 1000;
	for (i = 0; i < 500000; i++) {
		idx = rand() % (ARRAY_SIZE(p) - 1);
		size = rand() % 200;
		if (size == 0) {
			continue;
		}
		if (p[idx]) {
			simplemempool_free(pmp, p[idx], NULL);
			ops++;
			p[idx] = NULL;
			/*--- continue; ---*/
		}
		p[idx] = simplemempool_alloc(pmp, size, 1, NULL);
		ops++;
		if (p[idx]) {
			memset(p[idx], size & 0xFF, size);
		}
	}
	gettimeofday(&te, NULL);
	simplemempool_dump(pmp);

	tte = (te.tv_sec * 1000) + te.tv_usec / 1000;
	printf("time for operations %s/op ops=%lu\n",
	       human_time(buf, (double)(tte - tta) / (double)ops),  ops);


	gettimeofday(&ta, NULL);
	tta = (ta.tv_sec * 1000) + ta.tv_usec / 1000;
	cnt = 1;
	for (i = 0; i < ARRAY_SIZE(p); i++) {
		if (p[i]) {
			simplemempool_free(pmp, p[i], NULL);
			cnt++;
		}
	}
	gettimeofday(&te, NULL);
	tte = (te.tv_sec * 1000) + te.tv_usec / 1000;
	simplemempool_dump(pmp);
	simplemempool_alloc_exit(pmp);
	printf("time for free %s/entry entries=%u\n",
	       human_time(buf, (double)(tte - tta) / (double)cnt), cnt);
	return 0;
}
#endif /*--- #if !defined(__KERNEL__) ---*/
