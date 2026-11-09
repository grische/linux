/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __avm_rte_rte_h__
#define __avm_rte_rte_h__

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <avm/sammel/simple_mempool.h>

#ifndef CONFIG_HAVE_AVM_RTE
#  define ___rte_func(func) func
#elif defined(__mips__)
#  include <asm/yield_context.h>
#  define ___rte_func(func) yield_##func
#elif defined(__arm__) || defined(__aarch64__)
#  if defined(CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON)
#    include <asm/avm_enh/avm_fiq_os.h>
#  else
#    include <mach/avm_fiq_os.h>
#  endif
#  define ___rte_func(func) firq_##func
#else
#  error Broken kernel config!
#endif

#ifdef CONFIG_AVM_RTE
#  define __rte_func(func) ___rte_func(func)
#else
#  define __rte_func(func) func
#endif

static inline void rte_spin_lock_init(spinlock_t *lock)
{
	__rte_func(spin_lock_init)(lock);
}

static inline void rte_spin_lock(spinlock_t *lock)
{
	__rte_func(spin_lock)(lock);
}

#define rte_spin_lock_irqsave(lock, flags) \
	__rte_func(spin_lock_irqsave)(lock, flags)
#define rte_spin_trylock_irqsave(lock, flags) \
	__rte_func(spin_trylock_irqsave)(lock, flags)
#define rte_spin_trylock(lock) \
	__rte_func(spin_trylock)(lock)

static inline void rte_spin_unlock(spinlock_t *lock)
{
	__rte_func(spin_unlock)(lock);
}

static inline void rte_spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
	__rte_func(spin_unlock_irqrestore)(lock, flags);
}

#define rte_local_irq_save(flags)    __rte_func(local_irq_save)(flags)
#define rte_local_irq_restore(flags) __rte_func(local_irq_restore)(flags)

#define rte_raw_spin_lock_init(lock) __rte_func(raw_spin_lock_init)(lock)
#define rte_raw_spin_lock(lock)      __rte_func(raw_spin_lock)(lock)
#define rte_raw_spin_lock_irqsave(lock, flags) \
	__rte_func(raw_spin_lock_irqsave)(lock, flags)

#define rte_raw_spin_trylock_irqsave(lock, flags) \
	__rte_func(raw_spin_trylock_irqsave)(lock, flags)

#define rte_raw_spin_unlock(lock)      __rte_func(raw_spin_unlock)(lock)
#define rte_raw_spin_unlock_irqrestore(lock, flags) \
	__rte_func(raw_spin_unlock_irqrestore)(lock, flags)

static inline int __must_check rte_down_trylock(struct semaphore *sem)
{
	return __rte_func(down_trylock(sem));
}

static inline void rte_up(struct semaphore *sem)
{
	__rte_func(up(sem));
}

/**
 * DOC: RTE function wrappers may be asynchronous
 *
 * Many RTE function wrappers are synchronous only in Linux context.
 * In RTE context, they are asynchronous and delegate the actual work to an
 * IPI call which runs in Linux context.
 */

static inline void rte_wake_up_interruptible(wait_queue_head_t *q)
{
	__rte_func(wake_up_interruptible)(q);
}

static inline void rte_wake_up(wait_queue_head_t *q)
{
	__rte_func(wake_up)(q);
}

static inline bool rte_schedule_work(struct work_struct *work)
{
	return __rte_func(schedule_work)(work);
}

static inline bool rte_queue_work_on(int cpu, struct workqueue_struct *wq,
				     struct work_struct *work)
{
	return __rte_func(queue_work_on)(cpu, wq, work);
}

static inline bool rte_schedule_delayed_work(struct delayed_work *dwork,
					     unsigned long delay)
{
	return __rte_func(schedule_delayed_work)(dwork, delay);
}

static inline void rte_tasklet_hi_schedule(struct tasklet_struct *t)
{
	__rte_func(tasklet_hi_schedule)(t);
}

static inline u64 rte_local_clock(void)
{
	return __rte_func(local_clock)();
}

static inline void rte_stop_critical_timings(void)
{
	__rte_func(stop_critical_timings)();
}

static inline void rte_start_critical_timings(void)
{
	__rte_func(start_critical_timings)();
}

static inline bool rte_try_module_get(struct module *module)
{
	return __rte_func(try_module_get)(module);
}

static inline void rte_module_put(struct module *module)
{
	__rte_func(module_put)(module);
}

#define rte_panic(fmt, ...) __rte_func(panic(fmt, ##__VA_ARGS__))

struct mempool_handle;

#ifdef CONFIG_AVM_RTE

/*
 * rte_synchronize_timeout() - Wait for the RTE IPI call queue to drain.
 * @timeout: timeout in jiffies.
 *
 * In RTE context, many rte wrapper functions defer work to IPI calls.
 * Before freeing objects that are referenced by the deferred functions,
 * you therefore must drain the RTE IPI queue to prevent use-after-free
 * errors.
 *
 * Context: Only from linux-context.
 *
 * Return: true if all cpu-ipi-queues drained, false on timeout.
 */
static inline bool rte_synchronize_timeout(int timeout)
{
	return __rte_func(to_linux_sync_ipi)(timeout);
}

/**
 * is_rte_context() - Check for non-linux-context (yield-thread, fastirq)
 *
 * Return: true if non-Linux context, false if Linux context
 */
static inline bool is_rte_context(void)
{
	return __rte_func(is_avm_rte)();
}

/**
 * rte_mempool_create() - Create a memory-pool for rte-safe memory allocations.
 * @pool_name: A name for the memory pool
 * @pool_size: Pool total size. This is fixed after the pool is created.
 *
 * This creates a memory-pool that can be used with rte_kmalloc and its
 * variants.
 *
 * Context: Only from linux-context.
 *
 * Return: A handle to the new memory pool instance, or an ERR_PTR on error.
 */
static inline struct mempool_handle *rte_mempool_create(const char *pool_name,
							size_t pool_size)
{
	return simplemempool_alloc_init(pool_name, pool_size) ?: ERR_PTR(-ENOMEM);
}

/**
 * rte_mempool_destroy() - Destroy an RTE memory pool
 * @mph: Memory pool to destroy
 */
static inline void rte_mempool_destroy(struct mempool_handle *mph)
{
	simplemempool_alloc_exit(mph);
}

static inline void *rte_kmalloc(struct mempool_handle *mph, size_t size)
{
	return simplemempool_alloc(mph, size, 0, (void *)_RET_IP_);
}

static inline void *rte_kzalloc(struct mempool_handle *mph, size_t size)
{
	return simplemempool_alloc(mph, size, 1, (void *)_RET_IP_);
}

static inline void rte_kfree(struct mempool_handle *mph, void *ptr)
{
	simplemempool_free(mph, ptr, (void *)_RET_IP_);
}

#else /* if !RTE */

static inline bool rte_synchronize_timeout(int timeout)
{
	return true;
}

static inline bool is_rte_context(void)
{
	return false;
}

static inline struct mempool_handle *rte_mempool_create(const char *pool_name,
							size_t pool_size)
{
	return NULL;
}

static inline void rte_mempool_destroy(struct mempool_handle *unused)
{ }

static inline void *rte_kmalloc(struct mempool_handle *unused, size_t size)
{
	return kmalloc(size, GFP_ATOMIC);
}

static inline void *rte_kzalloc(struct mempool_handle *unused, size_t size)
{
	return kzalloc(size, GFP_ATOMIC);
}

static inline void rte_kfree(struct mempool_handle *unused, void *ptr)
{
	kfree(ptr);
}

#endif /* !RTE */

/*
 * Low-level API which always maps to the context-safe rte functions if they
 * are available, independent of CONFIG_AVM_RTE.  For use in crash and
 * watchdog handlers, and core code which is called by those.
 */
#ifdef CONFIG_HAVE_AVM_RTE
#  define __raw_is_rte_context             ___rte_func(is_avm_rte)
#else
#  define __raw_is_rte_context()           0
#endif
#define __raw_rte_down_trylock           ___rte_func(down_trylock)
#define __raw_rte_down                   ___rte_func(down)
#define __raw_rte_up                     ___rte_func(up)
#define __raw_rte_stop_critical_timings  ___rte_func(stop_critical_timings)
#define __raw_rte_start_critical_timings ___rte_func(start_critical_timings)
#define __raw_rte_local_clock            ___rte_func(local_clock)
#define __raw_rte_local_irq_save         ___rte_func(local_irq_save)
#define __raw_rte_local_irq_restore      ___rte_func(local_irq_restore)
#define __raw_rte_spin_lock_init         ___rte_func(spin_lock_init)
#define __raw_rte_spin_lock              ___rte_func(spin_lock)
#define __raw_rte_spin_unlock            ___rte_func(spin_unlock)
#define __raw_rte_spin_lock_irqsave      ___rte_func(spin_lock_irqsave)
#define __raw_rte_spin_unlock_irqrestore ___rte_func(spin_unlock_irqrestore)
#define __raw_rte_spin_trylock           ___rte_func(spin_trylock)
#define __raw_rte_raw_spin_lock_init     ___rte_func(raw_spin_lock_init)
#define __raw_rte_raw_spin_lock          ___rte_func(raw_spin_lock)
#define __raw_rte_raw_spin_unlock        ___rte_func(raw_spin_unlock)
#define __raw_rte_raw_spin_lock_irqsave  ___rte_func(raw_spin_lock_irqsave)
#define __raw_rte_raw_spin_trylock_irqsave    ___rte_func(raw_spin_trylock_irqsave)
#define __raw_rte_raw_spin_unlock_irqrestore  ___rte_func(raw_spin_unlock_irqrestore)


static inline bool rte_rcu_read_lock_is_unsafe(void)
{
	/*
	 * RCU quiescent cpu state tracking does not work at all with yield
	 * threads.  If RCU thinks the current cpu has passed a quiescent
	 * state, it may free RCU-protected resources.
	 */
	return IS_ENABLED(CONFIG_AVM_IPI_YIELD) && is_rte_context();
}

#endif
