// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/atomic.h>
#include <linux/semaphore.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#pragma GCC pop_options

#include <avm/capi_oslib/new.h>
#include <avm/capi_oslib/oslib.h>

#if defined(CONFIG_LANTIQ)
#include <irq.h>
#endif /*--- #if defined(CONFIG_LANTIQ) ---*/
#include "debug.h"
#include "host.h"
#include "ca.h"

/*--- #define OSLIB_STAT ---*/
#if defined(OSLIB_STAT)
#define CLK_TO_USEC(a) ((a) / 250)
static DEFINE_SPINLOCK(stat_lock);
/**
 */
struct _generic_stat {
	signed long cnt;
	signed long avg;
	signed long min;
	signed long max;
};
/**
 */
void init_generic_stat(struct _generic_stat *pgstat)
{
	pgstat->min = LONG_MAX;
	pgstat->max = LONG_MIN;
	pgstat->cnt = 0;
	pgstat->avg = 0;
}
/**
 */
void generic_stat(struct _generic_stat *pgstat, signed long val)
{
	if (pgstat->cnt == 0) {
		init_generic_stat(pgstat);
	}
	if (val > pgstat->max)
		pgstat->max = val;
	if (val < pgstat->min)
		pgstat->min = val;
	pgstat->avg += val;
	pgstat->cnt++;
}
/*
 * reset: Statistik ruecksetzen
 * mode: 0 in msec
 *       1 nur Wert
 *       2 in usec
 */
void display_generic_stat(char *prefix, struct _generic_stat *pgstat, unsigned int mode, unsigned int reset)
{
	struct _generic_stat gstat;
	signed long cnt;
	unsigned long flags;

	rte_spin_lock_irqsave(&stat_lock, flags);
	cnt = pgstat->cnt;
	if (cnt == 0) {
		rte_spin_unlock_irqrestore(&stat_lock, flags);
		return;
	}
	memcpy(&gstat, pgstat, sizeof(gstat));
	if (reset) {
		pgstat->cnt = 0;
	}
	rte_spin_unlock_irqrestore(&stat_lock, flags);
	pr_info("%s[%ld] min=%ld max=%ld avg=%ld %s\n", prefix, cnt, gstat.min, gstat.max,
		gstat.avg / cnt, mode == 0 ? "msec" :
		mode == 2 ? "usec" : "");
}
struct _generic_stat sched_latency;
struct _generic_stat sched_consumption;
struct _generic_stat sched_trigger_latency;
unsigned long trigger_cycle;
unsigned int schedstart_cycle;
#endif/*--- #if defined(OSLIB_STAT) ---*/

static atomic_t scheduler_workqueue_trigger_busy = ATOMIC_INIT(0);
static struct workqueue_struct *p_scheduler_workqueue;
static struct work_struct scheduler_work;

static atomic_t capi_oslib_crit_level;
static void capi_oslib_scheduler(struct work_struct *work);

static struct timer_list capi_oslib_scheduler_timer;

/**
 */
void capi_oslib_scheduler_timer_stop(void)
{
	del_timer(&capi_oslib_scheduler_timer);
}

static unsigned int Oslib_Schedule_TimerCnt;
/**
 */
static void capi_oslib_scheduler_timer_retrigger(void)
{
	int extra = 0;

	Oslib_Schedule_TimerCnt += HZ % 100;
	/*--- unterstuetze unterschiedliche jiffies zur Not eben mit jitter ---*/
	if (Oslib_Schedule_TimerCnt >= 100) {
		Oslib_Schedule_TimerCnt -= 100;
		extra = 1;
	}
	capi_oslib_scheduler_timer.expires += (HZ / 100) + extra; /*--- 10 ms ---*/
	add_timer(&capi_oslib_scheduler_timer);
}

/**
 */
void capi_oslib_trigger_scheduler(void)
{
	struct workqueue_struct *p_local_scheduler_workqueue;

	atomic_inc(&scheduler_workqueue_trigger_busy);
	/* ensure consistency of in-use counter vs. use of
	 * p_scheduler_workqueue.
	 */
	smp_mb__after_atomic();

	p_local_scheduler_workqueue = READ_ONCE(p_scheduler_workqueue);

	if (p_local_scheduler_workqueue) {
		/* yield/firq-context asynchron: see special handling before destroy_workqueue() */
		rte_queue_work_on(WORK_CPU_UNBOUND,
				  p_local_scheduler_workqueue,
				  &scheduler_work);
	}

	/* ensure consistency of in-use counter vs. use of
	 * p_scheduler_workqueue.
	 */
	smp_mb__before_atomic();

	atomic_dec(&scheduler_workqueue_trigger_busy);
}

/**
 */
void os_trigger_scheduler(void)
{
#if defined(OSLIB_STAT)
	if (trigger_cycle == 0) {
		trigger_cycle = avm_get_cycles();
	}
#endif/*--- #if defined(OSLIB_STAT) ---*/
	/*--- DEB_TRACE("os_trigger_scheduler\n"); ---*/
	capi_oslib_trigger_scheduler();
}
EXPORT_SYMBOL(os_trigger_scheduler);

/**
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void capi_oslib_scheduler_timer_handler(unsigned long nr __attribute__((unused)))
#else
static void capi_oslib_scheduler_timer_handler(struct timer_list *timer __attribute__((unused)))
#endif
{
	capi_oslib_scheduler_timer_retrigger();
	capi_oslib_trigger_scheduler();
}

/**
 */
void capi_oslib_scheduler_timer_init(void)
{
	DEB_INFO("%s\n", __func__);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	init_timer(&capi_oslib_scheduler_timer);
	capi_oslib_scheduler_timer.data = 0;
	capi_oslib_scheduler_timer.function = capi_oslib_scheduler_timer_handler;
#else
	timer_setup(&capi_oslib_scheduler_timer, capi_oslib_scheduler_timer_handler, 0);
#endif
	capi_oslib_scheduler_timer.expires = jiffies + HZ / 100;
	add_timer(&capi_oslib_scheduler_timer);
}

/**
 */
void capi_oslib_scheduler_work_init(void (*scheduler_work_func)(struct work_struct *work))
{
	if (!scheduler_work_func)
		return;

	INIT_WORK(&scheduler_work, scheduler_work_func);
	wmb();
	p_scheduler_workqueue = alloc_workqueue("capi_schedw",
						WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
}

void capi_oslib_scheduler_work_deinit(void)
{
	struct workqueue_struct *p_local_scheduler_workqueue;

	p_local_scheduler_workqueue = xchg(&p_scheduler_workqueue, NULL);

	if (!p_local_scheduler_workqueue)
		return;

	/* be sure that we not inside [rte_]queue_work_on()-area: */
	while (atomic_read(&scheduler_workqueue_trigger_busy))
		schedule();

	/* be sure that queue_work_on() not in yield/linux-ipi-queue pending,
	 * because rte_queue_work_on() works asynchron:
	 */
	if (rte_synchronize_timeout(HZ) == 0)
		pr_err("%s: no ipi-sync possible - strange\n", __func__);

	destroy_workqueue(p_local_scheduler_workqueue);
}

/**
 */
static inline void capi_oslib_disable_scheduler(void)
{

	DEB_INFO("Disabling scheduler...\n");
}

/**
 */
void os_disable_scheduler(void)
{
	capi_oslib_disable_scheduler();
}
EXPORT_SYMBOL(os_disable_scheduler);

/**
 */
static void capi_oslib_scheduler(struct work_struct *work)
{
#if defined(OSLIB_STAT)
	unsigned int act_cycles = avm_get_cycles();

	if (trigger_cycle) {
		unsigned long trigger_diff = avm_get_cycles() - trigger_cycle;

		trigger_cycle = 0;
		generic_stat(&sched_trigger_latency, CLK_TO_USEC(trigger_diff));
	}
	generic_stat(&sched_latency, CLK_TO_USEC(act_cycles - schedstart_cycle));
#endif/*--- #if defined(OSLIB_STAT) ---*/

	CA_TIMER_POLL();
	BUG_ON(!capi_oslib_stack);
	(void)(*capi_oslib_stack->cm_schedule)();

#if defined(OSLIB_STAT)
	generic_stat(&sched_consumption, CLK_TO_USEC(avm_get_cycles() - act_cycles));
	schedstart_cycle = act_cycles;
	{
		static int last;

		if (last == 0) {
			last = jiffies;
		}
		if (jiffies - last > 10 * HZ) {
			last = jiffies;
			display_generic_stat("sched-latency", &sched_latency, 2, 1);
			display_generic_stat("trigger-to-sched-latency", &sched_trigger_latency, 2, 1);
			display_generic_stat("sched-consumption", &sched_consumption, 2, 1);
		}
	}
#endif/*--- #if defined(OSLIB_STAT) ---*/
}

/**
 */
static irqreturn_t capi_oslib_irq_handler(int irq __attribute__((unused)), void *args)
{
	struct workqueue_struct *p_local_scheduler_workqueue;

	if (!args)
		return IRQ_NONE;

	BUG_ON(!capi_oslib_stack);
	if (!(*capi_oslib_stack->cm_handle_events)())
		return IRQ_NONE;

	atomic_inc(&scheduler_workqueue_trigger_busy);

	/* ensure consistency of in-use counter vs. use of
	 * p_scheduler_workqueue.
	 */
	smp_mb__after_atomic();

	p_local_scheduler_workqueue = READ_ONCE(p_scheduler_workqueue);

	if (p_local_scheduler_workqueue)
		queue_work_on(WORK_CPU_UNBOUND,
			      p_local_scheduler_workqueue, &scheduler_work);

	/* ensure consistency of in-use counter vs. use of
	 * p_scheduler_workqueue.
	 */
	smp_mb__before_atomic();

	atomic_dec(&scheduler_workqueue_trigger_busy);

	return IRQ_HANDLED;
} /* irq_handler */

#define	IO_RANGE		256

/**
 */
int capi_oslib_install_card(struct _stack_init_params *card)
{
	int result = 0;

	capi_oslib_disable_scheduler();
	capi_oslib_scheduler_work_init(capi_oslib_scheduler);

	if (card) {

		if (card->io_addr) {
			request_mem_region(card->io_addr, IO_RANGE, "capi_oslib");
			DEB_INFO("I/O memory range 0x%08x-0x%08x assigned to capi_oslib driver.\n",
				 card->io_addr,
				 card->io_addr + IO_RANGE - 1);
		}

		if (card->irq_num) {
			result = request_irq(card->irq_num, &capi_oslib_irq_handler, 0, "capi_oslib", card);
			if (result) {
				release_mem_region(card->io_addr, IO_RANGE);
				DEB_ERR("irq: %d registration failed\n", card->irq_num);
				return 1;
			}
			DEB_INFO("irq: %d successfully registred\n", card->irq_num);
		}
	}
	capi_oslib_scheduler_timer_init();
	return 0;
}

void capi_oslib_remove_card(struct _stack_init_params *card)
{

	capi_oslib_scheduler_timer_stop();
	capi_oslib_disable_scheduler();
	capi_oslib_scheduler_work_deinit();

	if (card == NULL)
		return;

	if (card->irq_num) {
		DEB_INFO("Releasing IRQ #%d...\n", card->irq_num);
		free_irq(card->irq_num, card);
	}
	if (card->io_addr) {
		DEB_INFO(
			"Releasing I/O memory range 0x%08x-0x%08x...\n",
			card->io_addr,
			card->io_addr + IO_RANGE - 1
		);
		release_mem_region(card->io_addr, IO_RANGE);
	}
} /* remove_card */

/**
 * not used: direct setting in isdn_fonx
 */
void capi_oslib_init_tasklet_control(void (*tasklet_control)(enum _tasklet_control))
{
	if (capi_oslib_stack == NULL) {
		DEB_ERR("capioslib: not initialized\n");
		return;
	}
	if (capi_oslib_stack->cm_ctrl_tasklet && tasklet_control) {
		DEB_ERR("capioslib: cm_ctrl_tasklet already initialized, ignore reinit!\n");
	}
	capi_oslib_stack->cm_ctrl_tasklet = (void *)tasklet_control;
	DEB_INFO("capioslib: cm_ctrl_tasklet with %p initialized\n", tasklet_control);
}
EXPORT_SYMBOL(capi_oslib_init_tasklet_control);

/**
 */
void EnterCritical(void)
{

	/*--- pr_info("[EnterCritical]"); ---*/
	if (capi_oslib_init_params && capi_oslib_init_params->irq_num)
		disable_irq(capi_oslib_init_params->irq_num);

	BUG_ON(!capi_oslib_stack);
	/*--- pr_info("1"); ---*/
	if (capi_oslib_stack->cm_ctrl_tasklet)
		(*capi_oslib_stack->cm_ctrl_tasklet)(tasklet_control_enter_critical);
	/*--- pr_info("2"); ---*/
	atomic_inc(&capi_oslib_crit_level);
	/*--- pr_info("3\n"); ---*/

}
EXPORT_SYMBOL(EnterCritical);

/**
 */
void _EnterCritical(char *File __attribute__((unused)), int Line __attribute__((unused)))
{
	EnterCritical();
}
EXPORT_SYMBOL(_EnterCritical);

/**
 */
void LeaveCritical(void)
{
	/*--- pr_info("[LeaveCritical]"); ---*/
	atomic_dec(&capi_oslib_crit_level);
	BUG_ON(atomic_read(&capi_oslib_crit_level) < 0);
	/*--- pr_info("1"); ---*/
	if (capi_oslib_init_params && capi_oslib_init_params->irq_num)
		enable_irq(capi_oslib_init_params->irq_num);
	/*--- pr_info("2"); ---*/
	if (capi_oslib_stack->cm_ctrl_tasklet)
		(*capi_oslib_stack->cm_ctrl_tasklet)(tasklet_control_leave_critical);
	/*--- pr_info("3\n"); ---*/
}
EXPORT_SYMBOL(LeaveCritical);

/**
 */
void _LeaveCritical(char *File __attribute__((unused)), int Line __attribute__((unused)))
{
	LeaveCritical();
}
EXPORT_SYMBOL(_LeaveCritical);
