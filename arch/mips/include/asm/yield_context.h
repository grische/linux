/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
 *
 *   author: mbahr@avm.de
 *   description: yield-thread-interface mips34k
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#ifndef __yield_context_h__
#define __yield_context_h__

#include <asm/yield_context_os.h>
#if defined(CONFIG_AVM_IPI_YIELD)
#if defined(CONFIG_LANTIQ)
#include <lantiq_yield-avm.h>
#endif /*--- #if defined(CONFIG_LANTIQ) ---*/
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/mm.h>

#define YIELD_HANDLED 1
/*--------------------------------------------------------------------------------*\
 * start function in non-linux-yield-context
 * Attention ! Only kseg0/kseg1 segment allowed - also data access in yield_handler !
 * ret: >= 0 number of registered signal < 0: errno
 *
 * return of request_yield_handler() handled -> YIELD_HANDLED
\*--------------------------------------------------------------------------------*/
int request_yield_handler_on(int cpu, int tc, int signal,
			     int (*yield_handler)(int signal, void *ref),
			     void *ref);
/*--------------------------------------------------------------------------------*\
 * ret: == 0 ok
\*--------------------------------------------------------------------------------*/
int free_yield_handler_on(int cpu, int tc, int signal, void *ref);

static inline enum irqreturn dummy_isr(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void disable_yield_handler_on(int cpu, int tc, int signal);
void enable_yield_handler_on(int cpu, int tc, int signal);

/*
 * High-Level API, does everything: registers the yield handler, reroutes the irq, ...
 */
int request_yield_on(int cpu, int tc, int irq, int signal,
		int (*handler)(int signal, void *dev_id),
		unsigned long irqflags, const char *devname, void *dev_id);
int free_yield_on(int cpu, int tc, int signal);

/**--------------------------------------------------------------------------------**\
 * Linux-cpus setting to lowest prio (0)
\**--------------------------------------------------------------------------------**/
#define YIELD_TC_SCHED_PRIORITY_MAX 3
#define YIELD_TC_SCHED_PRIORITY_MED 2
#define YIELD_TC_SCHED_PRIORITY_LOW 1
#define YIELD_TC_SCHED_GROUP_MASK 0x3
/*--------------------------------------------------------------------------------*\
 * yield_tc:   tc to use for yield
 * yield_mask: wich signal(s) would be catched
 * core:       really core (non-vpe)
 *
 * actually YIELD_MAX_TC tc possible, no crossover of yield_mask allowed
 * only on yield per cpu possible
\*--------------------------------------------------------------------------------*/
int yield_context_init_on(int linux_cpu, unsigned int yield_tc,
			  unsigned int yield_mask, unsigned int tc_prio);

/*--------------------------------------------------------------------------------*\
 * same as cat /proc/yield/stat - but dump as printk
\*--------------------------------------------------------------------------------*/
void yield_context_dump(void);

#define yield_spin_lock_init(_lock) spin_lock_init(_lock)
#define yield_raw_spin_lock_init(_rlock) raw_spin_lock_init(_rlock)

/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_spin_lock(spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_lock(&lock->rlock.raw_lock);
		return;
	}
	spin_lock(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_spin_unlock(spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->rlock.raw_lock);
		return;
	}
	spin_unlock(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_spin_lock_bh(spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_lock(&lock->rlock.raw_lock);
		return;
	}
	spin_lock_bh(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_spin_unlock_bh(spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->rlock.raw_lock);
		return;
	}
	spin_unlock_bh(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
#define yield_spin_lock_irqsave(lock, flags)                                   \
	do {                                                                   \
		if (!yield_is_linux_context()) {                               \
			flags = 0;                                             \
			arch_spin_lock(&(lock)->rlock.raw_lock);               \
		} else {                                                       \
			spin_lock_irqsave(lock, flags);                        \
		}                                                              \
	} while (0)

/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
#define yield_spin_trylock_irqsave(lock, flags)                                \
	({                                                                     \
		int ret;                                                       \
		if (!yield_is_linux_context()) {                               \
			flags = 0;                                             \
			ret = arch_spin_trylock(&(lock)->rlock.raw_lock);      \
		} else {                                                       \
			ret = spin_trylock_irqsave(lock, flags);               \
		}                                                              \
		ret ? 1 : 0;                                                   \
	})

/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_spin_unlock_irqrestore(spinlock_t *lock,
						unsigned long flags)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->rlock.raw_lock);
		return;
	}
	spin_unlock_irqrestore(lock, flags);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_raw_spin_lock(raw_spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_lock(&lock->raw_lock);
		return;
	}
	raw_spin_lock(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_raw_spin_unlock(raw_spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->raw_lock);
		return;
	}
	raw_spin_unlock(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_raw_spin_lock_bh(raw_spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_lock(&lock->raw_lock);
		return;
	}
	raw_spin_lock_bh(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_raw_spin_unlock_bh(raw_spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->raw_lock);
		return;
	}
	raw_spin_unlock_bh(lock);
}
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
#define yield_raw_spin_lock_irqsave(lock, flags)                               \
	do {                                                                   \
		if (!yield_is_linux_context()) {                               \
			flags = 0;                                             \
			arch_spin_lock(&((lock)->raw_lock));                   \
		} else {                                                       \
			raw_spin_lock_irqsave(lock, flags);                    \
		}                                                              \
	} while (0)

/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
#define yield_raw_spin_trylock_irqsave(lock, flags)                            \
	({                                                                     \
		int ret;                                                       \
		if (!yield_is_linux_context()) {                               \
			flags = 0;                                             \
			ret = arch_spin_trylock(&((lock)->raw_lock));          \
		} else {                                                       \
			local_irq_save(flags);                                 \
			ret = raw_spin_trylock(lock);                          \
			if (ret == 0)                                          \
				local_irq_restore(flags);                      \
		}                                                              \
		ret ? 1 : 0;                                                   \
	})
/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_raw_spin_unlock_irqrestore(raw_spinlock_t *lock,
						    unsigned long flags)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->raw_lock);
		return;
	}
	raw_spin_unlock_irqrestore(lock, flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#define yield_local_irq_save(flags)                                            \
	if (yield_is_linux_context())                                          \
	local_irq_save(flags)
#define yield_local_irq_restore(flags)                                         \
	if (yield_is_linux_context())                                          \
	local_irq_restore(flags)

/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
#define yield_raw_spin_lock_irq(lock)                               \
	do {                                                                   \
		if (!yield_is_linux_context()) {                               \
			arch_spin_lock(&((lock)->raw_lock));                   \
		} else {                                                       \
			raw_spin_lock_irq(lock);                    \
		}                                                              \
	} while (0)

/*--------------------------------------------------------------------------------*\
 * Speziallock: ist auch aus non-Linux-TC-Kontext aufrufbar
\*--------------------------------------------------------------------------------*/
static inline void yield_raw_spin_unlock_irq(raw_spinlock_t *lock)
{
	if (!yield_is_linux_context()) {
		arch_spin_unlock(&lock->raw_lock);
		return;
	}
	raw_spin_unlock_irq(lock);
}

/*--------------------------------------------------------------------------------*\
 * bestimmte Linuxfunktionalitaeten vom Yield aus triggern
\*--------------------------------------------------------------------------------*/
struct _yield_to_linux_ipi {
	enum _yield_to_linux_ipi_func_type {
		wake_up_type = 0,
		schedule_work_type,
		schedule_delayed_work_type,
		queue_work_on_type,
		tasklet_hi_schedule_type,
		try_module_get_type,
		module_put_type,
		panic_type,
		yieldexception_type,
		call_type,
		wake_up_state_type,
		tasklet_schedule_type,
		max_ipi_type
	} ipi_func_type;
	union _yield_to_linux_ipi_params {
		struct _yield_wake_up_param {
			wait_queue_head_t *q;
			unsigned int mode;
			int nr_exclusive;
			void *key;
		} wake_up_param;
		struct _yield_schedule_work_param {
			struct work_struct *work;
		} schedule_work_param;
		struct _yield_schedule_delayed_work_param {
			struct delayed_work *dwork;
			unsigned long delay;
		} schedule_delayed_work_param;
		struct _yield_queue_work_on_param {
			int cpu;
			struct workqueue_struct *wq;
			struct work_struct *work;
		} queue_work_on_param;
		struct _yield_tasklet_schedule_param {
			struct tasklet_struct *t;
		} tasklet_schedule_param;
		struct _yield_module_param {
			struct module *module;
		} module_param;
		struct _panic_param {
			const char *debugstr;
		} panic_param;
		struct _yieldexception_param {
			void *handle;
		} yieldexception_param;
		struct _call_param {
			void (*func)(void *func_param);
			void *func_param;
		} call_param;
		struct _yield_wake_up_state_param {
			struct task_struct *tsk;
			unsigned int state;
		} wake_up_state_param;
	} u;
	unsigned long ts_jiffies;
	unsigned long cycle;
	unsigned long ret_ip;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
extern int yield_trigger_linux_ipi(int cpu, struct _yield_to_linux_ipi *obj);

/*--- #define TEST_YIELD_IPI ---*/
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline void __yield_wake_up(wait_queue_head_t *q, unsigned int mode,
				   int nr_exclusive, void *key)
{
	struct _yield_to_linux_ipi params;
#if !defined(TEST_YIELD_IPI)
	if (yield_is_linux_context()) {
		wake_up_interruptible(q);
		return;
	}
#else
	unsigned long flags;
	unsigned int is_yield = !yield_is_linux_context();
	if (is_yield == 0) {
		local_irq_save(flags);
	}
	printk(KERN_ERR "[%s](%p)\n", __func__, q);
#endif
	params.ipi_func_type = wake_up_type;
	params.u.wake_up_param.q = q;
	params.u.wake_up_param.mode = mode;
	params.u.wake_up_param.nr_exclusive = nr_exclusive;
	params.u.wake_up_param.key = key;
	yield_trigger_linux_ipi(raw_smp_processor_id(), &params);
#if defined(TEST_YIELD_IPI)
	if (is_yield == 0) {
		local_irq_restore(flags);
	}
#endif /*--- #if defined(TEST_YIELD_IP) ---*/
}

#define yield_wake_up(x) __yield_wake_up(x, TASK_NORMAL, 1, NULL)
#define yield_wake_up_nr(x, nr) __yield_wake_up(x, TASK_NORMAL, nr, NULL)
#define yield_wake_up_all(x) __yield_wake_up(x, TASK_NORMAL, 0, NULL)

#define yield_wake_up_interruptible(x)                                         \
	__yield_wake_up(x, TASK_INTERRUPTIBLE, 1, NULL)
#define yield_wake_up_interruptible_nr(x, nr)                                  \
	__yield_wake_up(x, TASK_INTERRUPTIBLE, nr, NULL)
#define yield_wake_up_interruptible_all(x)                                     \
	__yield_wake_up(x, TASK_INTERRUPTIBLE, 0, NULL)

/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline bool yield_schedule_work(struct work_struct *work)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		return schedule_work(work);
	}
#if 0
    if(WORK_STRUCT_PENDING & *work_data_bits(work)) {
        return false;
    }
#endif
	params.ipi_func_type = schedule_work_type;
	params.u.schedule_work_param.work = work;
	yield_trigger_linux_ipi(raw_smp_processor_id(), &params);
	return true;
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
\*--------------------------------------------------------------------------------*/
static inline bool yield_queue_work_on(int cpu, struct workqueue_struct *wq,
				       struct work_struct *work)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		return queue_work_on(cpu, wq, work);
	}
#if 0
    if((WORK_STRUCT_PENDING & *work_data_bits(work))) {
        return false;
    }
#endif
	params.ipi_func_type = queue_work_on_type;
	params.u.queue_work_on_param.cpu = cpu;
	params.u.queue_work_on_param.wq = wq;
	params.u.queue_work_on_param.work = work;
	yield_trigger_linux_ipi(cpu, &params);
	return true;
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline bool yield_schedule_delayed_work(struct delayed_work *dwork,
					       unsigned long delay)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		return schedule_delayed_work(dwork, delay);
	}
#if 0
    if((WORK_STRUCT_PENDING & *work_data_bits(work))) {
        return false;
    }
#endif
	params.ipi_func_type = schedule_delayed_work_type;
	params.u.schedule_delayed_work_param.dwork = dwork;
	params.u.schedule_delayed_work_param.delay = delay;
	yield_trigger_linux_ipi(raw_smp_processor_id(), &params);
	return true;
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
\*--------------------------------------------------------------------------------*/
static inline bool yield_try_module_get(struct module *module)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		return try_module_get(module);
	}
	params.ipi_func_type = try_module_get_type;
	params.u.module_param.module = module;
	/*--- gleiche CPU wie yield_module_put() um Nebenlaeufigkeiten zu verhindern! ---*/
	yield_trigger_linux_ipi(0, &params);
	return true;
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
\*--------------------------------------------------------------------------------*/
static inline void yield_module_put(struct module *module)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		module_put(module);
		return;
	}
	params.ipi_func_type = module_put_type;
	params.u.module_param.module = module;
	/*--- gleiche CPU wie yield_module_put() um Nebenlaeufigkeiten zu verhindern! ---*/
	yield_trigger_linux_ipi(0, &params);
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline void yield_tasklet_hi_schedule(struct tasklet_struct *t)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		tasklet_hi_schedule(t);
	}
	params.ipi_func_type = tasklet_hi_schedule_type;
	params.u.tasklet_schedule_param.t = t;
	yield_trigger_linux_ipi(raw_smp_processor_id(), &params);
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline void yield_tasklet_schedule(struct tasklet_struct *t)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		tasklet_schedule(t);
	}
	params.ipi_func_type = tasklet_schedule_type;
	params.u.tasklet_schedule_param.t = t;
	yield_trigger_linux_ipi(raw_smp_processor_id(), &params);
}
/*--------------------------------------------------------------------------------*\
 * aus beliebigen Kontext verwendbar
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline void yield_panic(const char *debugstr)
{
	struct _yield_to_linux_ipi params;
	int cpu;

	if (yield_is_linux_context()) {
		panic("%s\n", debugstr);
		return;
	}
	params.ipi_func_type = panic_type;
	params.u.panic_param.debugstr = debugstr;
	for_each_online_cpu (cpu) {
		yield_trigger_linux_ipi(cpu, &params);
	}
	for (;;)
		;
}
/*--------------------------------------------------------------------------------*\
 * nur aus Yield-Kontext verwenden
 * raw_smp_processor_id() funktioniert auch im YIELD-Thread (gp in thread_info mit initialisierter cpu)
\*--------------------------------------------------------------------------------*/
static inline void yield_exception(void *handle)
{
	struct _yield_to_linux_ipi params;

	if (yield_is_linux_context()) {
		/*--- do nothing if called from Linux-OS ---*/
		return;
	}
	params.ipi_func_type = yieldexception_type;
	params.u.yieldexception_param.handle = handle;
	yield_trigger_linux_ipi(raw_smp_processor_id(), &params);
}
/*--------------------------------------------------------------------------------*\
 * auch aus Linux-Kontext verwendbar
\*--------------------------------------------------------------------------------*/
static inline void yield_call(int cpu, void (*call_func)(void *func_param),
			      void *func_param)
{
	unsigned long flags, is_linux_context;
	struct _yield_to_linux_ipi params;

	params.ipi_func_type = call_type;
	params.u.call_param.func = call_func;
	params.u.call_param.func_param = func_param;
	is_linux_context = yield_is_linux_context();
	if (is_linux_context)
		local_irq_save(flags);
	yield_trigger_linux_ipi(cpu, &params);
	if (is_linux_context)
		local_irq_restore(flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int yield_wake_up_state(struct task_struct *p, unsigned int state)
{
	struct _yield_to_linux_ipi params;
	if (yield_is_linux_context()) {
		return wake_up_state(p, state);
	}
	params.ipi_func_type = wake_up_state_type;
	params.u.wake_up_state_param.tsk = p;
	params.u.wake_up_state_param.state = state;
	get_task_struct(p);
	if (yield_trigger_linux_ipi(raw_smp_processor_id(), &params)) {
		/*--------------------------------------------------------------------------------*\
		  Achtung! - Unsauber falls task gerade auf einer Linux-CPU beendet wird:
		  es werden dann unerlaubte Linux-Funkionen in __put_task_struct() im Yield-Kontext aufgerufen
		  -> aber yield_trigger_linux_ipi() sollte immer gut gehen!
		\*--------------------------------------------------------------------------------*/
		put_task_struct(p);
		return false;
	}
	return true;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int yield_wake_up_process(struct task_struct *p)
{
	return yield_wake_up_state(p, TASK_NORMAL);
}

/**
 * very important!
 * if ipi-functions used in yield-context use it to flush/sync ipi-queues before
 * any free of linux-depend-data-structs (e.g. workitem)
 * (prevent use-after-free-accesses)
 * only linux-kthread-context
 *
 * timeout: in jiffies
 *
 * ret: 1 all (cpu-)queues) synced
 *	0 timeout
 *
 */
int yield_to_linux_sync_ipi(int timeout);

/**
 * in kernel/semaphore.c
 */
extern void yield_up(struct semaphore *sem);
extern int yield_down_trylock(struct semaphore *sem);

/**
 * Attention! local_clock() -> sched_clock() have to use rte_spin_lock_irqsave()
 */
static inline u64 yield_local_clock(void)
{
	return local_clock();
}

/**
 */
static inline void yield_start_critical_timings(void)
{
	if (yield_is_linux_context())
		start_critical_timings();
}

/**
 */
static inline void yield_stop_critical_timings(void)
{
	if (yield_is_linux_context())
		stop_critical_timings();
}

#else /*--- #if defined(CONFIG_AVM_IPI_YIELD) ---*/

#define yield_up(sema)                                                         \
	if (yield_is_linux_context())                                          \
		up(sema)
#define yield_down_trylock(sema)                                               \
	yield_is_linux_context() ? down_trylock(sema) : 1
#define yield_down(sema)                                                       \
	if (yield_is_linux_context())                                          \
		down(sema)

#define yield_spin_lock(lock) spin_lock(lock)
#define yield_spin_unlock(lock) spin_unlock(lock)
#define yield_spin_lock_bh(lock) spin_lock_bh(lock)
#define yield_spin_unlock_bh(lock) spin_unlock_bh(lock)
#define yield_spin_lock_irqsave(lock, flags) spin_lock_irqsave(lock, flags)
#define yield_spin_trylock_irqsave(lock, flags) spin_trylock_irqsave(lock, flags)
#define yield_spin_unlock_irqrestore(lock, flags) spin_unlock_irqrestore(lock, flags)
#define yield_raw_spin_lock(lock) raw_spin_lock(lock)
#define yield_raw_spin_unlock(lock) raw_spin_unlock(lock)
#define yield_raw_spin_lock_bh(lock) raw_spin_lock_bh(lock)
#define yield_raw_spin_unlock_bh(lock) raw_spin_unlock_bh(lock)
#define yield_raw_spin_lock_irqsave(lock, flags) raw_spin_lock_irqsave(lock, flags)
#define yield_raw_spin_trylock_irqsave(lock, flags) raw_spin_trylock_irqsave(lock, flags)
#define yield_raw_spin_unlock_irqrestore(lock, flags) raw_spin_unlock_irqrestore(lock, flags)
#define yield_local_irq_save(flags) local_irq_save(flags)
#define yield_local_irq_restore(flags) local_irq_restore(flags)
#define yield_raw_spin_lock_irq(lock) raw_spin_lock_irq(lock)
#define yield_raw_spin_unlock_irq(lock) raw_spin_unlock_irq(lock)
#define yield_raw_spin_lock_init(_lock) raw_spin_lock_init(_lock)

#define yield_start_critical_timings() start_critical_timings()
#define yield_stop_critical_timings()  stop_critical_timings()
#define yield_local_clock()	       local_clock()

#endif /* --- #else --- */ /*--- #if defined(CONFIG_AVM_IPI_YIELD) ---*/
#endif /*--- #ifndef __yield_context_h__ ---*/
