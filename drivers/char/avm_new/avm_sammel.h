/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_SAMMEL_H_
#define _AVM_SAMMEL_H_

#include <linux/version.h>
#if defined(CONFIG_AVM_IPI_YIELD)

#include <asm/yield_context.h>
#define __BUILD_AVM_CONTEXT_FUNC(func) yield_##func

#elif defined(CONFIG_AVM_FASTIRQ) || defined(CONFIG_AVM_FIQ_PUMA6)
#if defined(CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON)
#include <asm/avm_enh/avm_fiq_os.h>
#else
#include <mach/avm_fiq_os.h>
#endif
#define __BUILD_AVM_CONTEXT_FUNC(func) firq_##func

#elif defined(CONFIG_SMP) && defined(CONFIG_MIPS)

#include <asm/yield_context.h>
/*--- shit Kompatibilitaets-hack (AR10 - kein CONFIG_AVM_IPI_YIELD - yield_spinlock().. muss aber existieren) ---*/
#define yield_wake_up_interruptible(a)                                  \
	do {								\
		if (!yield_is_avm_rte())                                \
			wake_up_interruptible(a);			\
	} while (0)
#define yield_wake_up(a)						\
	do {								\
		if (!yield_is_avm_rte())                                \
			wake_up(a);					\
	} while (0)
#define __BUILD_AVM_CONTEXT_FUNC(func) yield_##func

#else

#define __BUILD_AVM_CONTEXT_FUNC(func) func
#define is_avm_rte() 0

#endif

/*--- KOMPATIBILITAETS-DEFINITIONEN FÜR DIE UMBENENNUNG VON AVM_RTE MAKROS ---*/
#ifndef is_avm_rte
#ifdef yield_is_linux_context
#define yield_is_avm_rte() (!yield_is_linux_context())
#endif
#ifdef firq_is_linux_context
#define firq_is_avm_rte() (!firq_is_linux_context())
#endif
#endif

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
