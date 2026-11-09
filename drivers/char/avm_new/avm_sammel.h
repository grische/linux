/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_SAMMEL_H_
#define _AVM_SAMMEL_H_

#include <linux/version.h>
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
 * avm_page_statistic.c
 */
void show_avm_page_statistic(int complete);

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
#define _oom_score(task, totalpages) oom_badness(task, NULL, NULL, totalpages)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#define _oom_score(task, totalpages)                                           \
	((oom_badness(task, NULL, NULL, totalpages) * 1000) / totalpages)
#else /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0) ---*/
#define _oom_score(task, totalpages) oom_badness(task, NULL, NULL, totalpages)
#endif /*--- #else ---*/ /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0) ---*/

#endif /*--- #ifmdef _AVM_SAMMEL_H_ ---*/
