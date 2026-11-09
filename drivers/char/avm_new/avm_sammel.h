/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_SAMMEL_H_
#define _AVM_SAMMEL_H_

#include <linux/swap.h>
#include <linux/version.h>
#include <avm/compat/misc.h>
#include <avm/rte/rte.h>

/*--- #define AVM_LED_DEBUG ---*/
/*--- #define AVM_WATCHDOG_DEBUG ---*/
#define AVM_EVENT_DEBUG

/**
 * avm_factory_reset.c
 */
void avm_factory_reset_init(void);

/**
 * ar7wdt_main.c
 */
void set_watchdog_in_progress(void);

/**
 * avm_oom_status.c
 */

/**
 * @brief show memory-info
 * @param force 0x0 depend on memory-situation
 *              ored with 0x1: signalize that in crash or die-Mode (print only one time)
 *              ored with 0x2 print all infos
 */
#define AVM_OOM_MEMSTAT_ONCE 1
#define AVM_OOM_MEMSTAT_ALL  2
void avm_oom_show_memstat(unsigned int force);

/**
 * @brief mmput() with none-forced might_sleep()
 * it's dirty but mmput() is also called from timer or nmi-context
 * so have to prevent might_sleep() if not necessary
 */
void mmput_avm_context(struct mm_struct *mm);

/**
 * @brief stack checking
 * @param task == NULL over all threads
 */
void avm_stack_check(struct task_struct *task);

/*
 * Handle varying return values of oom_badness, depending on Linux version.
 * Mainline normalizes in proc_oom_score().
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
/* oom_badness() may return LONG_MIN, or a range of (-totalpages,  2 * totalpages) */
#define _oom_normalize(badness, totalpages)			\
	((badness) == LONG_MIN ? 0 :					\
	 ((1000 + (badness) * 1000 / (long)(totalpages)) * 2 / 3))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
/* oom_badness() returns a range of [1,  2 * totalpages) */
#define _oom_normalize(badness, totalpages) \
	((badness) * 1000 / (totalpages))
#else
/* oom_badness() returns a normalized value of [1, 2000) */
#define _oom_normalize(badness, totalpages) (badness)
#endif

/*
 * Handle varying arguments of oom_badness, depending on Linux version
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
#define _oom_score(task, totalpages) \
	_oom_normalize(oom_badness(task, totalpages), totalpages)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
#define _oom_score(task, totalpages) \
	_oom_normalize(oom_badness(task, totalpages), totalpages)
#else
#define _oom_score(task, totalpages) \
	_oom_normalize(oom_badness(task, NULL, NULL, totalpages), totalpages)
#endif

#define oom_score(task) _oom_score(task, totalram_pages() + total_swap_pages)

#endif /*--- #ifmdef _AVM_SAMMEL_H_ ---*/
