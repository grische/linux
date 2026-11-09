/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __avm_rte_pcmlink_ul_compat_h__
#define __avm_rte_pcmlink_ul_compat_h__

/* avm/rte/pcmlink_ul_compat.h is a temporary compatibility workaround.  It is
 * meant to allow a fast switch from avm/pcmlink/ul.h to avm/rte/rte.h by
 * introducing function aliases and re-definitions of macros with the original
 * pcmlink_ul_* names.
 *
 * When writing new code: do not use anything from here but stick to
 *        #include <avm/rte/rte.h>
 *
 * When maintaining old code:
 *   1. Switch from
 *        #include <avm/pcmlink/ul.h>
 *      to
 *        #include <avm/rte/rte.h>
 *        #include <avm/rte/pcmlink_ul_compat.h>
 *   2. Update function and macro calls that have a 'pcmlink_ul_' or 'PCMLINK_'
 *      prefix by the corresponding one from avm/rte/rte.h; then remove the
 *      include of avm/rte/pcmlink_ul_compat.h.  You might need to add extra
 *      #include statements.
 *
 * When all users of avm/pcmlink/ul.h have switched completely, this very file
 * will be removed.
 */

#ifndef __avm_rte_rte_h__
#error Missing include statement: #include <avm/rte/rte.h>
#endif

#if defined(CONFIG_PLATFORM_WANTS_UBIK2_ISDNSTACK_ON_CPU)
#define PCMLINK_TASKLET_CONTROL_CPU     CONFIG_PLATFORM_WANTS_UBIK2_ISDNSTACK_ON_CPU
#elif defined(CONFIG_UBIK2_ISDNSTACK_ON_CPU)
#define PCMLINK_TASKLET_CONTROL_CPU     CONFIG_UBIK2_ISDNSTACK_ON_CPU
#else/*--- #if defined(CONFIG_UBIK2_ISDNSTACK_ON_CPU) ---*/
#define PCMLINK_TASKLET_CONTROL_CPU     0
#endif/*--- #else ---*//*--- #if defined(CONFIG_UBIK2_ISDNSTACK_ON_CPU) ---*/

static inline void pcmlink_ul_lock_init(spinlock_t *lock)
{
	rte_spin_lock_init(lock);
}

static inline void pcmlink_ul_lock(spinlock_t *lock, unsigned long *flags)
{
	if (flags)
		rte_spin_lock_irqsave(lock, *flags);
	else
		rte_spin_lock(lock);
}

static inline void pcmlink_ul_unlock(spinlock_t *lock, unsigned long *flags)
{
	if (flags)
		rte_spin_unlock_irqrestore(lock, *flags);
	else
		rte_spin_unlock(lock);
}

static inline int pcmlink_ul_yieldcontext(void)
{
	return is_rte_context();
}

static inline int pcmlink_ul_assert_on_yield_context(void)
{
	if (!is_rte_context())
		return 0;

	pr_emerg("\nFATAL-ERROR: Do not use in RTE context pc=%pS ra=%pS\n",
		 (void *)_THIS_IP_, (void *)_RET_IP_);
	return 1;
}

static inline void pcmlink_ul_wake_up_interruptible(wait_queue_head_t *q)
{
	rte_wake_up_interruptible(q);
}

static inline void pcmlink_ul_wake_up(wait_queue_head_t *q)
{
	rte_wake_up(q);
}

static inline bool pcmlink_ul_schedule_work(struct work_struct *work)
{
	return rte_schedule_work(work);
}

static inline bool pcmlink_ul_queue_work_on(int cpu,
					    struct workqueue_struct *wq,
					    struct work_struct *work)
{
	return rte_queue_work_on(cpu, wq, work);
}

static inline bool pcmlink_ul_schedule_delayed_work(struct delayed_work *dwork,
						    unsigned long delay)
{
	return rte_schedule_delayed_work(dwork, delay);
}

static inline void pcmlink_ul_tasklet_hi_schedule(struct tasklet_struct *t)
{
	rte_tasklet_hi_schedule(t);
}

static inline bool pcmlink_ul_try_module_get(struct module *module)
{
	return rte_try_module_get(module);
}

static inline void pcmlink_ul_module_put(struct module *module)
{
	rte_module_put(module);
}

static inline int pcmlink_ul_to_linux_sync_ipi(int timeout)
{
	return rte_synchronize_timeout(timeout);
}

static inline void *pcmlink_ul_alloc_init(char *pool_name,
					  unsigned int pool_size)
{
	if (!IS_ENABLED(CONFIG_AVM_RTE))
		return kmalloc(4, GFP_KERNEL);
	return rte_mempool_create(pool_name, pool_size);
}

static inline void pcmlink_ul_alloc_exit(void *pmp)
{
	if (!IS_ENABLED(CONFIG_AVM_RTE)) {
		kfree(pmp);
		return;
	}
	rte_mempool_destroy(pmp);
}

/* rte_k{malloc,zalloc,free} use _RET_IP, which make no sense when hiding their
 * calls in wrapper functions, thus we use macros here.
 */
#define pcmlink_ul_malloc(mh, sz) rte_kmalloc(mh, sz)
#define pcmlink_ul_zalloc(mh, sz) rte_kzalloc(mh, sz)
#define pcmlink_ul_mfree(mh, ptr) rte_kfree(mh, ptr)

#endif
