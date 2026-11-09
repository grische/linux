// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2015-2019 AVM GmbH <fritzbox_info@avm.de> */

#define pr_fmt(fmt) "[brcm:watchdog] " fmt

#ifdef CONFIG_AVM_FASTIRQ
#if defined(CONFIG_AVM_GENERIC_RTE)
#include <avm/rte/avm_fiq_groups.h>
#else
#include <asm/avm_enh/avm_fiq_groups.h>
#endif
#define CLIENT_FIQ_PRIO FIQ_PRIO_WATCHDOG
#endif

#include <linux/cpumask.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/nmi.h>
#include <asm/irq_regs.h>
#include <asm/mach_avm.h>

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

#include <avm/rte/rte.h>

#ifdef CONFIG_AVM_FASTIRQ
#if defined(CONFIG_AVM_GENERIC_RTE)
#include <avm/rte/rte.h>
#else
#include <asm/avm_enh/avm_gic.h>
#include <asm/avm_enh/avm_gic_fiq.h>
#include <asm/avm_enh/avm_fiq.h>
#include <asm/avm_enh/avm_fiq_os.h>
#endif
#endif

#include "avm_sammel.h"

#include "ar7wdt_private.h"

static unsigned int wdt_hz;
#define SECS_TO_WDOG_TICKS(x) ((uint32_t)((x) * wdt_hz))
#define MICROSECS_TO_WDOG_TICKS(x) ((uint32_t)((x) * (wdt_hz/1000000)))
#define WDOG_TICKS_TO_SECS(x) ((uint32_t)((x) / wdt_hz))

typedef struct WDTimer {
	uint32_t        WatchDogDefCount;/* Write 0xff00 0x00ff to Start timer
	* Write 0xee00 0x00ee to Stop and re-load default count
	* Read from this register returns current watch dog count
	* */
	uint32_t        WatchDogCtl;

	/* Number of 50-MHz ticks for WD Reset pulse to last */
	uint32_t        WDResetCount;

#define SOFT_RESET              0x00000001
	uint32_t        WDTimerCtl;

	uint32_t        WDAccessCtl;
} WDTimer;


static WDTimer *WDTIMER0;
static int wd_irq;

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

int is_wdt_int(int intNr)
{
	return intNr == wd_irq;
}

void ar7wdt_hw_trigger(void)
{
	bcm_acknowledge_watchdog();
	atomic_set(&wd_count, wd_retry_count);
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);

irqreturn_t wdt_isr(int irq, void *arg)
{
	struct pt_regs __maybe_unused regs, *pregs;

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
			set_ICDIPTR(wd_irq,
				    get_ICDIPTR(wd_irq, IS_ATOMIC) &
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
		_bcm_start_watchdog(SECS_TO_WDOG_TICKS(64));

#ifdef CONFIG_AVM_FASTIRQ
		/* Interrupt Pending löschen */
		set_ICDICPR(wd_irq, 1, IS_ATOMIC);
		/* Interrupt beenden */
		set_ICCEOIR(wd_irq, IS_ATOMIC);
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
			console_verbose();
			bust_spinlocks(1);

			avm_set_reset_status(RS_NMIWATCHDOG);
			avm_stack_check(NULL);
			raw_notifier_call_chain(&nmi_chain, 0, pregs);

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
  * Initialize global data scructures with values from device tree
  */
static int wdt_of_init(void)
{
	struct device_node *node;
	struct clk *clk;
	struct irq_desc *irq_desc = 0;
	int virt_irq;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "brcm,bcm96xxx-wdt");
	if (!node) {
		pr_err("device_node not found!\n");
		ret = -EINVAL;
		goto err_out;
	}
	virt_irq = of_irq_get(node, 0);

	if (virt_irq < 0) {
		pr_err("unable to get irq!\n");
		ret = -EINVAL;
		goto err_out;
	}

	irq_desc = irq_to_desc(virt_irq);
	if (!irq_desc) {
		pr_err("unable to determine hwirq\n");
		ret = -EINVAL;
		goto err_out;
	}

	wd_irq = irq_desc->irq_data.hwirq;

	WDTIMER0 = of_iomap(node, 0);
	if (!WDTIMER0) {
		pr_err("Failed to remap watchdog regs");
		ret = -EINVAL;
		goto err_out;
	}

	clk = of_clk_get(node, 0);
	if (IS_ERR(clk)) {
		pr_err("of_clk_get failed\n");
		ret = -EINVAL;
		goto err_out;
	}

	wdt_hz = clk_get_rate(clk);
	if (wdt_hz == 0) {
		pr_err("incorrect clock frequency\n");
		ret = -EINVAL;
		goto err_out;
	}

err_out:
	if (node)
		of_node_put(node);
	return ret;
}

/**
 */
void ar7wdt_hw_init(void)
{
	pr_err("[%s]: Setting up watchdog for 32sec (bark) and 64sec (bite) ...\n",
	       __func__);


	if (wdt_of_init())
		return;

	__raw_rte_spin_lock_init(&wd_lock);
	atomic_set(&wd_sync, 0);

#ifdef CONFIG_AVM_FASTIRQ
#if defined(CONFIG_BCM963138) || defined(CONFIG_BCM963178) || defined(CONFIG_BCM96764) || defined(CONFIG_BCM96766)
	register_wdt_irq(wd_irq);
#else
#error "unknown chip"
#endif
#endif

	bcm_stop_watchdog();
	mdelay(100);
	bcm_start_watchdog(64);
	atomic_set(&wd_count, wd_retry_count);
	mdelay(100);
}

/**
 * Disable hardware watchdog when the avm-watchdog is disabled.
 * On 1200W7 this is required because u-boot enables the watchdog. On 7530 ax it's
 * not needed but also doesn't cause an issue.
 */
void ar7wdt_hw_disable_noinit(void)
{
	wdt_of_init();
	ar7wdt_hw_deinit();
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
}
