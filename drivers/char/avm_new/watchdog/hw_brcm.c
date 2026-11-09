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

#include <linux/cpumask.h>
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
#include <linux/cpumask.h>

#include <asm/mach_avm.h>
#include <avm/rte/rte.h>

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
#else
#include <bcm_intr.h>
#endif

#include "avm_sammel.h"

#include <bcm_map_part.h>

#if defined(CONFIG_BCM_EXT_TIMER)
#include "bcm_ext_timer.h"
#endif
#include "ar7wdt_private.h"

#define WDT_HZ				50000000 /* Fclk */
#define SECS_TO_WDOG_TICKS(x) ((uint32_t)((x) * WDT_HZ))
#define MICROSECS_TO_WDOG_TICKS(x) ((uint32_t)((x) * (WDT_HZ/1000000)))
#define WDOG_TICKS_TO_SECS(x) ((uint32_t)((x) / WDT_HZ))

#if !defined(WDTIMR0_OFFSET)
#define WDTIMR0_OFFSET              0x0480
#define WDTIMR1_OFFSET              0x04c0
#define WDTIMR0_PHYS_BASE           (PERF_PHYS_BASE + WDTIMR0_OFFSET)
#define WDTIMR1_PHYS_BASE           (PERF_PHYS_BASE + WDTIMR1_OFFSET)
#define WDTIMR0_BASE                BCM_IO_MAP(PERF_IDX, PERF_PHYS_BASE, WDTIMR0_OFFSET)
#define WDTIMR1_BASE                BCM_IO_MAP(PERF_IDX, PERF_PHYS_BASE, WDTIMR1_OFFSET)

typedef struct WDTimer {
	uint32        WatchDogDefCount;/* Write 0xff00 0x00ff to Start timer
	* Write 0xee00 0x00ee to Stop and re-load default count
	* Read from this register returns current watch dog count
	* */
	uint32        WatchDogCtl;

	/* Number of 50-MHz ticks for WD Reset pulse to last */
	uint32        WDResetCount;

#define SOFT_RESET              0x00000001
	uint32        WDTimerCtl;

	uint32        WDAccessCtl;
} WDTimer;


#define WDTIMER0 ((WDTimer * const) WDTIMR0_BASE)
#define WDTIMER1 ((WDTimer * const) WDTIMR1_BASE)
#endif

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

static void _bcm_start_watchdog(unsigned int timeout)
{
	writel(timeout, &WDTIMER0->WatchDogDefCount);
	writel(0xFF00, &WDTIMER0->WatchDogCtl);
	writel(0x00FF, &WDTIMER0->WatchDogCtl);
}

static void _bcm_stop_watchdog(void)
{
	writel(0xEE00, &WDTIMER0->WatchDogCtl);
	writel(0x00EE, &WDTIMER0->WatchDogCtl);
}

static void bcm_start_watchdog(unsigned int timeout)
{
	unsigned long flags;
	__raw_rte_spin_lock_irqsave(&wd_lock, flags);
	_bcm_start_watchdog(SECS_TO_WDOG_TICKS(timeout));
	__raw_rte_spin_unlock_irqrestore(&wd_lock, flags);
}

static void bcm_stop_watchdog(void)
{
	unsigned long flags;
	__raw_rte_spin_lock_irqsave(&wd_lock, flags);
	_bcm_stop_watchdog();
	__raw_rte_spin_unlock_irqrestore(&wd_lock, flags);
}

static void bcm_acknowledge_watchdog(void)
{
	unsigned long flags;
	__raw_rte_spin_lock_irqsave(&wd_lock, flags);
	writel(0xFF00, &WDTIMER0->WatchDogCtl);
	writel(0x00FF, &WDTIMER0->WatchDogCtl);
	__raw_rte_spin_unlock_irqrestore(&wd_lock, flags);
}

/* used by brcm code */
int bcmbca_wd_start(unsigned int timeout)
{
	unsigned long flags;
	__raw_rte_spin_lock_irqsave(&wd_lock, flags);
	_bcm_start_watchdog(MICROSECS_TO_WDOG_TICKS(timeout));
	__raw_rte_spin_unlock_irqrestore(&wd_lock, flags);

	return 0;
}
EXPORT_SYMBOL(bcmbca_wd_start);

/**
 */
static void wd_timer_function(struct timer_list *t)
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

	pr_err("[%s]\n", __func__);
	if (!__raw_rte_spin_trylock(&wd_lock)) {
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

		_bcm_stop_watchdog();
		_bcm_start_watchdog(64);

#ifdef CONFIG_AVM_FASTIRQ
		/* Interrupt Pending löschen */
		set_ICDICPR(AVM_IRQ_WD, 1, IS_ATOMIC);
		/* Interrupt beenden */
		set_ICCEOIR(AVM_IRQ_WD, IS_ATOMIC);
#endif

		/*Master-Slave Lock zuruecksetzen */
		__raw_rte_spin_unlock(&wd_lock);

		if ((forced_panic) || (atomic_read(&wd_count) == 0)) {
			if (!is_rte_context()) {
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

	ret = avm_request_fiq_on(cpu_possible_mask, irq, wdt_isr, FIQ_HWIRQ, "Watchdog", 0);
	avm_gic_fiq_setup(irq, FIQ_HWIRQ, cpu_possible_mask, FIQ_PRIO_WATCHDOG, 1, 0);
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

	for (cpu = 0; cpu < num_possible_cpus(); cpu++)
		timer_setup(&WDTimers[cpu], wd_timer_function, 0);

	__raw_rte_spin_lock_init(&wd_lock);
	atomic_set(&wd_sync, 0);

#ifdef CONFIG_AVM_FASTIRQ
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963178)
	register_wdt_irq(AVM_IRQ_WD);
#else
#error "unknown chip"
#endif
#endif

	bcm_stop_watchdog();
	mdelay(100);
	bcm_start_watchdog(64);
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
	bcm_stop_watchdog();
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
	bcm_start_watchdog(10);
	//panic("%s: Watchdog expired\n", __func__);
}
