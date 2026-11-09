// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2015-2019 AVM GmbH <fritzbox_info@avm.de> */

#ifdef CONFIG_AVM_FASTIRQ
#ifdef CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON
#include <asm/avm_enh/avm_fiq_groups.h>
#else
#include <mach/avm_fiq_groups.h>
#endif
#define CLIENT_FIQ_PRIO FIQ_PRIO_WATCHDOG
#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/nmi.h>
#include <asm/irq_regs.h>
#include <linux/kmsg_dump.h>

#include <asm/mach_avm.h>

#ifdef CONFIG_AVM_FASTIRQ
#ifdef CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON
#include <asm/avm_enh/avm_gic.h>
#include <asm/avm_enh/avm_gic_fiq.h>
#include <asm/avm_enh/avm_fiq.h>
#include <asm/avm_enh/avm_fiq_os.h>
#else
#include <mach/avm_gic.h>
#include <mach/avm_gic_fiq.h>
#include <mach/avm_fiq.h>
#include <mach/avm_fiq_os.h>
#endif
#define _BUILD_AVM_CONTEXT_FUNC(func) firq_##func
#else
#include <bcm_intr.h>
#define _BUILD_AVM_CONTEXT_FUNC(func) func
#define is_avm_rte() 0
#endif

#include "avm_sammel.h"

#include <bcm_map_part.h>
#include <board.h>

#if defined(CONFIG_BCM_EXT_TIMER)
#include "bcm_ext_timer.h"
#endif

void bcm_init_watchdog(void);
void bcm_configure_watchdog(unsigned int enabled, unsigned int timer,
			    unsigned int userMode, unsigned int userThreshold);
void bcm_acknowledge_watchdog(void);
int bcm_suspend_watchdog(void);

static struct timer_list WDTimers[NR_CPUS];

static atomic_t wdt_active;
static unsigned int act_wdt_cpu;
static atomic_t wdt_busy;

static spinlock_t wd_lock;
static atomic_t wd_sync;
static atomic_t wd_count;

#define RETRY_COUNT 0
unsigned int wd_retry_count = RETRY_COUNT;

static RAW_NOTIFIER_HEAD(nmi_chain);

/**
 * der (pseudo-)nmi-handler ueber fastirq
 */
int register_nmi_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&nmi_chain, nb);
}

/**
 */
static void wd_timer_function(unsigned long context)
{
	bcm_acknowledge_watchdog();
	atomic_set(&wdt_busy, 0);

	atomic_set(&wd_count, wd_retry_count);
}

/**
 * \brief: jede CPU per roundrobin triggern
 */
unsigned int cancelled;
void ar7wdt_hw_trigger(void)
{
	unsigned int cpu;

	if (atomic_read(&wdt_active) == 0) {
		return;
	}
	if (atomic_read(&wdt_busy)) {
		return;
	}
	//pr_err("[%s]Trigger cpu=%u\n", __func__, act_wdt_cpu);
	for (cpu = act_wdt_cpu; cpu < num_possible_cpus(); cpu++) {
		if (!cpu_online(cpu)) {
			continue;
		}
		atomic_set(&wdt_busy, cpu + 1);
		if (cpu == smp_processor_id()) {
			if (!cancelled)
				wd_timer_function(0);
			break;
		}
		WDTimers[cpu].expires = jiffies + 1;
		del_timer(&WDTimers[cpu]);
		add_timer_on(&WDTimers[cpu], cpu);
		break;
	}
	if (++cpu >= num_possible_cpus())
		cpu = 0;
	act_wdt_cpu = cpu;
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);

extern unsigned int bcm_gpio_get_data(unsigned int gpio_num);
extern void bcm_gpio_set_data(unsigned int gpio_num, unsigned int data);

void bcm_intclear_watchdog(void);

extern void avm_mask_all_fiqs_down(unsigned int fiq_prio,
				   unsigned long *restore_PMR,
				   unsigned long flags);
extern void avm_unmask_all_fiqs_up(unsigned long restore_PMR,
				   unsigned long flags);

irqreturn_t wdt_isr(int irq, void *arg)
{
	struct pt_regs regs, *pregs;

	unsigned int limit;
	unsigned int tstart;

	unsigned int cpu = raw_smp_processor_id();

	if (!_BUILD_AVM_CONTEXT_FUNC(spin_trylock(&wd_lock))) {
		int wd_counter = 0;

		pr_err("FIQ watchdog handling, slave CPU#%d caught!\n", cpu);

		wd_counter = atomic_read(&wd_count);

		/* Sync ... */
		atomic_inc(&wd_sync);

		if (wd_counter == 0) {
			pr_err("FIQ watchdog handling, slave: CPU#%d, waiting for backtrace trigger ...\n",
			       cpu);

#ifdef CONFIG_AVM_FASTIRQ
			/* disable target routing */
			set_ICDIPTR(AVM_IRQ_WD,
				    get_ICDIPTR(AVM_IRQ_WD, IS_ATOMIC) &
					    ~(1 << cpu),
				    IS_ATOMIC);

			local_fiq_enable();
#endif

			/* Festhalten, IPI-FIRQ-Trigger wird hier reinschlagen ... */
			while (1) {
			}
		}

		while (atomic_read(&wd_sync) != 0) {
		}

		return IRQ_HANDLED;
	} else {
		int forced_panic = 0;

		pr_err("FIQ watchdog handling, master: CPU#%d, retry counter=%d\n",
		       cpu, atomic_read(&wd_count));

		/* alle Slaves einsammeln */
		limit = avm_get_cyclefreq() * 3; /* sec */
		tstart = avm_get_cycles();
		while ((unsigned int)atomic_read(&wd_sync) != (num_possible_cpus() - 1)) {
			if ((avm_get_cycles() - tstart) > limit) {
				pr_err("It seems, slave CPU is caught badly while keeping WATCHDOG FIQ masked ...\n");
				pr_err("Sync trigger should come through though, if not, FIQs are erroneously switched off there ...\n");
				forced_panic = 1;
				break;
			}
		}

#ifdef CONFIG_AVM_FASTIRQ
		/* Jetzt Interrupt-Controller behandeln: spaetes Interrupt-Ack */
		(void)get_ICCIAR(IS_ATOMIC);
#endif

#if defined(CONFIG_BCM963158) || defined(CONFIG_BCM963178)
		/* Watchdog anhalten */
		WDTIMER0->WatchDogCtl = 0xEE00;
		WDTIMER0->WatchDogCtl = 0x00EE;
		dmb();

		/* Watchdog wieder starten */
		WDTIMER0->WatchDogCtl = 0xFF00;
		WDTIMER0->WatchDogCtl = 0x00FF;
		dmb();
#else
		/* Watchdog anhalten */
		TIMER->WatchDogCtl = 0xEE00;
		TIMER->WatchDogCtl = 0x00EE;
		dmb();

		/* Watchdog wieder starten */
		TIMER->WatchDogCtl = 0xFF00;
		TIMER->WatchDogCtl = 0x00FF;
		dmb();

		/* Watchdog-Interrupt zurücksetzen */
		TIMER->TimerInts |= WATCHDOG;
		dmb();
#endif

#ifdef CONFIG_AVM_FASTIRQ
		/* Interrupt Pending löschen */
		set_ICDICPR(AVM_IRQ_WD, 1, IS_ATOMIC);
		/* Interrupt beenden */
		set_ICCEOIR(AVM_IRQ_WD, IS_ATOMIC);
#endif

		/*Master-Slave Lock zuruecksetzen */
		_BUILD_AVM_CONTEXT_FUNC(spin_unlock(&wd_lock));

		if ((forced_panic) || (atomic_read(&wd_count) == 0)) {
			if (!_BUILD_AVM_CONTEXT_FUNC(is_avm_rte())) {
				pregs = get_irq_regs();
			} else {
				//avm_tick_jiffies_update(); //--- auch im FASTIRQ-Fall jiffies korrigieren ---
#ifdef CONFIG_AVM_FASTIRQ
				pregs = get_fiq_regs();
#endif
			}
#ifdef CONFIG_AVM_FASTIRQ
			prepare_register_for_trap(&regs, &pregs);
#endif
			avm_set_reset_status(RS_NMIWATCHDOG);
			avm_stack_check(NULL);
			raw_notifier_call_chain(&nmi_chain, 0, pregs);

			console_verbose();
			bust_spinlocks(1);

			flush_cache_all();
			die("HardwareWatchDog - NMI taken", pregs, 0);

			// Zur Sicherheit, wir kehren nicht zurueck, Reset wird durch WD gezogen ...
			while (1) {
			}
		}

		/* Retry Counter erniedrigen, Sync löschen, dann Slaves freigeben */
		atomic_dec(&wd_count);
		atomic_set(&wd_sync, 0);

		return IRQ_HANDLED;
	}
}

#ifdef CONFIG_AVM_FASTIRQ
/**
 */
void register_wdt_irq(int irq)
{
	int ret;

	ret = avm_request_fiq_on(((1 << num_possible_cpus()) - 1), irq, wdt_isr, FIQ_CPUMASK | FIQ_HWIRQ, "Watchdog", 0);
	avm_gic_fiq_setup(irq, FIQ_HWIRQ, ((1 << num_possible_cpus()) - 1), FIQ_PRIO_WATCHDOG, 1, 0);
	if (!ret) {
		pr_err("[%s] Watchdog as fastirq(%u) on all cpus registered\n",
		       __func__, irq);
		return;
	}
	pr_err("[%s] ERROR request_irq(irq(%d)) ret:%d\n", __func__, irq, ret);
}
#endif

/**
 */
void ar7wdt_hw_init(void)
{
	unsigned int cpu;

	pr_err("[%s]: Setting up watchdog for 32sec (bark) and 64sec (bite) ...\n",
	       __func__);

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		init_timer(&WDTimers[cpu]);
		WDTimers[cpu].data = (unsigned long)&WDTimers[cpu];
		WDTimers[cpu].function = wd_timer_function;
	}
	bcm_init_watchdog();
	_BUILD_AVM_CONTEXT_FUNC(spin_lock_init(&wd_lock));
	atomic_set(&wd_sync, 0);

#ifdef CONFIG_AVM_FASTIRQ
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963178)
	register_wdt_irq(AVM_IRQ_WD);
#else
#error "unknown chip"
#endif
#endif

	bcm_configure_watchdog(0, 64000000, 0, 0);
	mdelay(100);
	bcm_configure_watchdog(1, 64000000, 0, 0);
	atomic_set(&wd_count, wd_retry_count);
	mdelay(100);
	atomic_set(&wdt_active, 1);
}

/**
 */
int ar7wdt_hw_is_wdt_running(void)
{
	return atomic_read(&wdt_active);
}

/**
 * dummy
 */
void ar7wdt_hw_secure_wdt_disable(void)
{
}

/**
 */
void ar7wdt_hw_deinit(void)
{
	pr_err("[%s]: Switching off watchdog ...\n", __func__);
	bcm_acknowledge_watchdog();
	mdelay(100);
	bcm_configure_watchdog(0, 64000000, 0, 0);
	mdelay(100);
	bcm_configure_watchdog(1, 64000000, 0, 0);
	atomic_set(&wd_count, 10);
	mdelay(100);
	atomic_set(&wdt_active, 0);
}

/**
 */
void ar7wdt_hw_reboot(void)
{
	pr_err("[%s]: triggered ...\n", __func__);
	bcm_acknowledge_watchdog();
	mdelay(100);
	bcm_configure_watchdog(1, 1000000, 0, 0);
	//panic("%s: Watchdog expired\n", __func__);
}
