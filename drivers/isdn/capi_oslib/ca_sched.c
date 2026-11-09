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
DEFINE_SPINLOCK(stat_lock);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _generic_stat {
	signed long cnt;
	signed long avg;
	signed long min;
	signed long max;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void init_generic_stat(struct _generic_stat *pgstat) {
	pgstat->min = LONG_MAX;
	pgstat->max = LONG_MIN;
	pgstat->cnt = 0;
	pgstat->avg = 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void generic_stat(struct _generic_stat *pgstat, signed long val) {
	if(pgstat->cnt == 0) {
		init_generic_stat(pgstat);
	}
	if(val > pgstat->max) pgstat->max = val;
	if(val < pgstat->min) pgstat->min = val;
	pgstat->avg += val;
	pgstat->cnt++;
}
/*--------------------------------------------------------------------------------*\
 * reset: Statistik ruecksetzen
 * mode: 0 in msec
 *       1 nur Wert
 *       2 in usec
\*--------------------------------------------------------------------------------*/
void display_generic_stat(char *prefix, struct _generic_stat *pgstat, unsigned int mode, unsigned int reset) {
	struct _generic_stat gstat;
	signed long cnt;
	unsigned long flags;

	pcmlink_ul_lock(&stat_lock, &flags);
	cnt = pgstat->cnt;
	if(cnt == 0) {
		pcmlink_ul_unlock(&stat_lock, &flags);
		return;
	}
	memcpy(&gstat, pgstat, sizeof(gstat));
	if(reset) {
		pgstat->cnt = 0;
	}
	pcmlink_ul_unlock(&stat_lock, &flags);
	printk("%s[%ld] min=%ld max=%ld avg=%ld %s\n", prefix, cnt, gstat.min, gstat.max, gstat.avg / cnt, mode == 0 ? "msec" :
																												   mode == 2 ? "usec" : "");
}
struct _generic_stat sched_latency;
struct _generic_stat sched_consumption;
struct _generic_stat sched_trigger_latency;
volatile unsigned int trigger_cycle;
unsigned int schedstart_cycle;
#endif/*--- #if defined(OSLIB_STAT) ---*/
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(USE_TASKLETS)
struct tasklet_struct scheduler_tasklet;
struct tasklet_struct *p_scheduler_tasklet = NULL;
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
static atomic_t scheduler_workqueue_trigger_busy = ATOMIC_INIT(0);
static struct workqueue_struct *p_scheduler_workqueue = NULL;
static struct work_struct scheduler_work;
#elif defined(USE_THREAD)
static struct _capi_scheduler {
	struct task_struct *thread_pid;
	struct completion on_exit;
	wait_queue_head_t wait_queue;
	volatile unsigned int trigger;
	void (*scheduler)(unsigned long data);
} capi_sched;
static int capi_oslib_thread (void *data);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/


void (*delic_tasklet_func)(unsigned long );
void (*delic_enable_irq)( void );
void (*delic_disable_irq)( void );

struct tasklet_struct delic_tasklet;
struct tasklet_struct *p_delic_tasklet = NULL;

static atomic_t capi_oslib_crit_level;
static void capi_oslib_scheduler (unsigned long data);

#if !defined(USE_THREAD)
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
#endif/*--- #if !defined(USE_THREAD) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_trigger_scheduler(void) {
#if defined(CAPIOSLIB_CHECK_LATENCY)
	/*--- capi_check_latency(0, (char *)__func__, 1); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
#if defined(USE_TASKLETS)
/*--- DEB_INFO("%s %p\n", __func__, p_scheduler_tasklet); ---*/
	if(p_scheduler_tasklet)
		pcmlink_ul_tasklet_hi_schedule(p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
	struct workqueue_struct *p_local_scheduler_workqueue;

	atomic_inc(&scheduler_workqueue_trigger_busy);
	p_local_scheduler_workqueue = p_scheduler_workqueue;
	rmb();
	if(p_local_scheduler_workqueue) {
		/* yield/firq-context asynchron: see special handling before destroy_workqueue() */
		pcmlink_ul_queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, p_local_scheduler_workqueue, &scheduler_work);
	}
	atomic_dec(&scheduler_workqueue_trigger_busy);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
	{
	struct _capi_scheduler *pcapi_sched = &capi_sched;
	pcapi_sched->trigger++;
	/*--- DBG_TRC("<trigger: %d>", pcapi_sched->trigger); ---*/
	pcmlink_ul_wake_up_interruptible(&pcapi_sched->wait_queue);
	}
#endif/*--- #if defined(USE_THREAD) ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void os_trigger_scheduler(void) {
#if defined(OSLIB_STAT)
	if(trigger_cycle == 0) {
		trigger_cycle = avm_get_cycles();
	}
#endif/*--- #if defined(OSLIB_STAT) ---*/
	/*--- DEB_TRACE("os_trigger_scheduler\n"); ---*/
	capi_oslib_trigger_scheduler();
}
EXPORT_SYMBOL(os_trigger_scheduler);

#if !defined(USE_THREAD)
/*--------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------*/
static void capi_oslib_scheduler_timer_handler(unsigned long nr __attribute__((unused))) {
	capi_oslib_scheduler_timer_retrigger();
	capi_oslib_trigger_scheduler();
}

/*--------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------*/
void capi_oslib_scheduler_timer_init(void) {
	DEB_INFO("%s\n", __func__);
	init_timer(&capi_oslib_scheduler_timer);
	capi_oslib_scheduler_timer.expires = jiffies + HZ / 100;
	capi_oslib_scheduler_timer.data = 0;
	capi_oslib_scheduler_timer.function = capi_oslib_scheduler_timer_handler;
	add_timer(&capi_oslib_scheduler_timer);
}
#endif/*--- #if !defined(USE_THREAD) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_delic_tasklet_init(void (*tasklet_func) (unsigned long ), unsigned int enable) {
	if(tasklet_func) {
		if(enable) {
			tasklet_init(&delic_tasklet, tasklet_func, 0);
			p_delic_tasklet = &delic_tasklet;
			return;
		}
		if(p_delic_tasklet) {
			p_delic_tasklet = NULL;
			tasklet_kill(&delic_tasklet);
		}
	}
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_scheduler_tasklet_init(void (*scheduler_tasklet_func) (unsigned long ), unsigned int enable) {
	if(scheduler_tasklet_func) {
		if(enable) {
#if defined(USE_TASKLETS)
			tasklet_init(&scheduler_tasklet, scheduler_tasklet_func, 0);
			p_scheduler_tasklet = &scheduler_tasklet;
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wcast-function-type"
			INIT_WORK(&scheduler_work, (work_func_t)scheduler_tasklet_func);
			wmb();
#pragma GCC pop_options
			p_scheduler_workqueue = alloc_workqueue("capi_schedw", WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
			{
			struct _capi_scheduler *pcapi_sched = &capi_sched;

			init_waitqueue_head(&pcapi_sched->wait_queue);
			init_completion(&pcapi_sched->on_exit);
			pcapi_sched->scheduler = scheduler_tasklet_func;
			kernel_thread(capi_oslib_thread, pcapi_sched, CLONE_SIGHAND);
			}
#endif/*--- #if defined(USE_THREAD) ---*/
			return;
		}
#if defined(USE_TASKLETS)
		if(p_scheduler_tasklet) {
			p_scheduler_tasklet = NULL;
			tasklet_kill(&scheduler_tasklet);
		}
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
		do {
			struct workqueue_struct *p_local_scheduler_workqueue;

			p_local_scheduler_workqueue = xchg(&p_scheduler_workqueue, NULL);
			if (p_local_scheduler_workqueue) {
				/* be sure that we not inside [pcmlink_ul_]queue_work_on()-area: */
				while(atomic_read(&scheduler_workqueue_trigger_busy))
					schedule();
				/* be sure that queue_work_on() not in yield/linux-ipi-queue pending,
				 * because pcmlink_ul_queue_work_on() works asynchron:
				 */
				if (pcmlink_ul_to_linux_sync_ipi(HZ) == 0)
					pr_err("%s: no ipi-sync possible - strange\n", __func__);
				destroy_workqueue(p_local_scheduler_workqueue);
			}
		} while(0);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
			{
			struct _capi_scheduler *pcapi_sched = &capi_sched;
			if(pcapi_sched->thread_pid) {
				send_sig(SIGTERM, pcapi_sched->thread_pid, 1);
				pcapi_sched->thread_pid = NULL;
				wait_for_completion(&pcapi_sched->on_exit);
			}
		}
#endif/*--- #if defined(USE_THREAD) ---*/
	}
}
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline void capi_oslib_disable_scheduler (void) {

	DEB_INFO("Disabling scheduler...\n");
#if defined(USE_TASKLETS)
	if(p_scheduler_tasklet)
		tasklet_disable(p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void os_disable_scheduler (void) {
	capi_oslib_disable_scheduler();
}
EXPORT_SYMBOL(os_disable_scheduler);

#if defined(USE_THREAD)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int capi_oslib_thread (void *data) {
	struct _capi_scheduler *pcapi_sched = (struct _capi_scheduler *)data;
	pcapi_sched->thread_pid = current;
	daemonize("capioslib_sched");
	set_user_nice(current, -20);
	allow_signal(SIGTERM);
	pcapi_sched->scheduler((unsigned long)data);
	return 0;
}
#endif/*--- #if defined(USE_THREAD) ---*/
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void capi_oslib_scheduler (unsigned long data __attribute__((unused))) {
#if defined(USE_THREAD)
	struct _capi_scheduler *pcapi_sched = &capi_sched;
	printk("[%s] init\n", __func__);

	for(;;) {
		int timeout = wait_event_interruptible_timeout(pcapi_sched->wait_queue, pcapi_sched->trigger, HZ / 100);
		if(timeout == -ERESTARTSYS) {
			/* interrupted by signal -> exit */
			break;
		}
		pcapi_sched->trigger = 0;
		CA_TIMER_POLL();
		(void)(*capi_oslib_stack->cm_schedule)();
	}
	pcapi_sched->thread_pid = NULL;
	printk("[%s] exit\n", __func__);
	complete_and_exit(&pcapi_sched->on_exit, 0 );
#else/*--- #if defined(USE_THREAD) ---*/
#if defined(OSLIB_STAT)
	unsigned int act_cycles = avm_get_cycles();

	if(trigger_cycle) {
		unsigned int trigger_diff = avm_get_cycles() - trigger_cycle;
		trigger_cycle = 0;
		generic_stat(&sched_trigger_latency, CLK_TO_USEC(trigger_diff));
	}
	generic_stat(&sched_latency, CLK_TO_USEC(act_cycles - schedstart_cycle));
#endif/*--- #if defined(OSLIB_STAT) ---*/

	CA_TIMER_POLL();
	(void)(*capi_oslib_stack->cm_schedule)();

#if defined(OSLIB_STAT)
	generic_stat(&sched_consumption, CLK_TO_USEC(avm_get_cycles() - act_cycles));
	schedstart_cycle = act_cycles;
	{
		static int last;
		if(last == 0) {
			last = jiffies;
		}
		if(jiffies - last > 10 * HZ) {
			last = jiffies;
			display_generic_stat("sched-latency",&sched_latency, 2, 1);
			display_generic_stat("trigger-to-sched-latency",&sched_trigger_latency, 2, 1);
			display_generic_stat("sched-consumption",&sched_consumption, 2, 1);
		}
	}
#endif/*--- #if defined(OSLIB_STAT) ---*/
#endif/*--- #else ---*//*--- #if defined(USE_THREAD) ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static irqreturn_t capi_oslib_irq_handler (int irq __attribute__((unused)), void * args) {

	if (args != NULL) {
		if ((*capi_oslib_stack->cm_handle_events) ()) {
#if defined(USE_TASKLETS)
			if(p_scheduler_tasklet)
				tasklet_hi_schedule (p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
			struct workqueue_struct *p_local_scheduler_workqueue;

			atomic_inc(&scheduler_workqueue_trigger_busy);
			p_local_scheduler_workqueue = p_scheduler_workqueue;
			rmb();
			if(p_local_scheduler_workqueue)
				queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, p_local_scheduler_workqueue, &scheduler_work);
			atomic_dec(&scheduler_workqueue_trigger_busy);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
			capi_oslib_trigger_scheduler();
#endif/*--- #if defined(USE_THREAD) ---*/
			if(p_delic_tasklet)
				tasklet_schedule(p_delic_tasklet);
			return IRQ_HANDLED;
		}
	}
	return IRQ_NONE;
} /* irq_handler */

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define	IO_RANGE		256

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int capi_oslib_install_card (struct _stack_init_params * card) {
	int result = 0;

	capi_oslib_disable_scheduler ();
	capi_oslib_scheduler_tasklet_init(capi_oslib_scheduler, 1);
	if(capi_oslib_interrupt_library && capi_oslib_interrupt_library->tasklet_func)
		capi_oslib_delic_tasklet_init(capi_oslib_interrupt_library->tasklet_func, 1);

	if(card) {

		if(card->io_addr) {
			request_mem_region (card->io_addr, IO_RANGE, "capi_oslib");
			DEB_INFO(
				"I/O memory range 0x%08x-0x%08x assigned to " "capi_oslib" " driver.\n",
				card->io_addr,
				card->io_addr + IO_RANGE - 1
			);
		}

		if(card->irq_num) {
			result = request_irq (
					card->irq_num,
					&capi_oslib_irq_handler,
					0,
					"capi_oslib",
					card
					);
			if (result) {
				release_mem_region (card->io_addr, IO_RANGE);
				DEB_ERR("irq: %d registration failed\n", card->irq_num );
				return 1;
			} else {
				DEB_INFO("irq: %d successfully registred\n", card->irq_num );
			}
		}
	}
#if !defined(USE_THREAD)
	capi_oslib_scheduler_timer_init();
#endif/*--- #if !defined(USE_THREAD) ---*/
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void capi_oslib_remove_card (struct _stack_init_params* card) {

#if !defined(USE_THREAD)
	capi_oslib_scheduler_timer_stop();
#endif/*--- #if !defined(USE_THREAD) ---*/
	capi_oslib_disable_scheduler ();
	capi_oslib_scheduler_tasklet_init(capi_oslib_scheduler, 0);

	if(capi_oslib_interrupt_library && capi_oslib_interrupt_library->tasklet_func)
		capi_oslib_delic_tasklet_init(capi_oslib_interrupt_library->tasklet_func, 0);

	if(card == NULL)
		return;

	if(card->irq_num) {
		DEB_INFO("Releasing IRQ #%d...\n", card->irq_num);
		free_irq (card->irq_num, card);
	}
	if(card->io_addr) {
		DEB_INFO(
			"Releasing I/O memory range 0x%08x-0x%08x...\n",
			card->io_addr,
			card->io_addr + IO_RANGE - 1
		);
		release_mem_region (card->io_addr, IO_RANGE);
	}
} /* remove_card */
#if 1
/*--------------------------------------------------------------------------------*\
 * not used: direct setting in isdn_fonx
\*--------------------------------------------------------------------------------*/
void  capi_oslib_init_tasklet_control(void (* tasklet_control)(enum _tasklet_control)){
	if(capi_oslib_stack == NULL) {
		DEB_ERR("capioslib: not initialized\n");
		return;
	}
	if(capi_oslib_stack->cm_ctrl_tasklet && tasklet_control) {
		DEB_ERR("capioslib: cm_ctrl_tasklet already initialized, ignore reinit!\n");
	}
	capi_oslib_stack->cm_ctrl_tasklet = (void *)tasklet_control;
	DEB_INFO("capioslib: cm_ctrl_tasklet with %p initialized\n", tasklet_control);
}
EXPORT_SYMBOL(capi_oslib_init_tasklet_control);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void EnterCritical (void) {

	/*--- printk("[EnterCritical]"); ---*/
	if(capi_oslib_init_params && capi_oslib_init_params->irq_num)
		disable_irq (capi_oslib_init_params->irq_num);


	/*--- printk("1"); ---*/
	if(p_delic_tasklet)
		tasklet_disable (p_delic_tasklet);
	/*--- printk("2"); ---*/
	if(capi_oslib_stack->cm_ctrl_tasklet)
		(*capi_oslib_stack->cm_ctrl_tasklet)(tasklet_control_enter_critical);
	/*--- printk("3"); ---*/
	atomic_inc (&capi_oslib_crit_level);
	/*--- printk("4\n"); ---*/

}
EXPORT_SYMBOL(EnterCritical);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void _EnterCritical (char *File __attribute__((unused)), int Line __attribute__((unused))) {
	EnterCritical();
}
EXPORT_SYMBOL(_EnterCritical);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void LeaveCritical (void) {

	/*--- printk("[LeaveCritical]"); ---*/
	atomic_dec (&capi_oslib_crit_level);
	BUG_ON( atomic_read(&capi_oslib_crit_level) < 0 );
	/*--- printk("1"); ---*/
	if(capi_oslib_init_params && capi_oslib_init_params->irq_num)
		enable_irq (capi_oslib_init_params->irq_num);
	/*--- printk("2"); ---*/
	if(p_delic_tasklet)
		tasklet_enable(p_delic_tasklet);
	/*--- printk("3"); ---*/
	if(capi_oslib_stack->cm_ctrl_tasklet)
		(*capi_oslib_stack->cm_ctrl_tasklet)(tasklet_control_leave_critical);
	/*--- printk("4\n"); ---*/
}
EXPORT_SYMBOL(LeaveCritical);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void _LeaveCritical (char *File __attribute__((unused)), int Line __attribute__((unused))) {
	LeaveCritical();
}
EXPORT_SYMBOL(_LeaveCritical);
#else
static volatile unsigned int per_cpu_recursion_level[NR_CPUS];
static DEFINE_SPINLOCK(critical_section);
/*---------------------------------------------------------------------------*\
 * recursion allowed
\*---------------------------------------------------------------------------*/
unsigned long  __attribute__((warn_unused_result)) EnterCritical (void) {
	int cpu;
	unsigned long flags;
	preempt_disable();
	cpu = smp_processor_id();
	local_irq_save(flags); /*--- be sure that not interrupted on this cpu ---*/
	if(per_cpu_recursion_level[cpu] == 0) {
		spin_lock(&critical_section); /*--- per-cpu-protect ---*/
	}
	per_cpu_recursion_level[cpu]++;
	preempt_enable();
	return flags;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long __attribute__((warn_unused_result)) _EnterCritical (char *File __attribute__((unused)), int Line __attribute__((unused))) {
	return EnterCritical();
}
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void LeaveCritical(unsigned long flags) {
	int cpu;
	preempt_disable();
	int cpu = smp_processor_id();
	BUG_ON(per_cpu_recursion_level[cpu] <= 0);
	per_cpu_recursion_level[cpu]--;
	if(per_cpu_recursion_level[cpu] == 0) {
		spin_unlock(&critical_section);
	}
	local_irq_restore(flags);
	preempt_enable();
}
EXPORT_SYMBOL(LeaveCritical);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void _LeaveCritical (unsigned long flags, char *File __attribute__((unused)), int Line __attribute__((unused))) {
	LeaveCritical(flags);
}
EXPORT_SYMBOL(_LeaveCritical);
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if 0
void EnterCacheSensitiveCode (void) {

	(*interface->enter_cache_sensitive_code) ();
} /* EnterCacheSensitiveCode */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if 0
void LeaveCacheSensitiveCode (void) {

	(*interface->leave_cache_sensitive_code) ();
} /* LeaveCacheSensitiveCode */
#endif

