// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#define pr_fmt(fmt) "[qcom_wdt] " fmt

#include <linux/clk.h>
#include <asm/delay.h>
#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/watchdog.h>
#include <asm/mach_avm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/nmi.h>
#include <linux/version.h>
#include <linux/console.h>
#include <linux/kmsg_dump.h>
#include <avm/watchdog/watchdog.h>
#include "../../firmware/qcom_scm.h"

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 4, 0))
#include <linux/qcom_scm.h>
#else
#include <soc/qcom/scm.h>
#endif

#include <asm/avm_enh/avm_crash_handling.h>

#ifdef CONFIG_AVM_FASTIRQ
#ifdef CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON
#include <asm/avm_enh/avm_tz.h>
#include <asm/avm_enh/avm_gic.h>
#include <asm/avm_enh/avm_fiq.h>
#include <asm/avm_enh/avm_fiq_os.h>
#include <asm/avm_enh/avm_gic_fiq.h>
#include <asm/avm_enh/avm_fiq_groups.h>
#else
#include <mach/avm_tz.h>
#include <mach/avm_gic.h>
#include <mach/avm_fiq.h>
#include <mach/avm_fiq_os.h>
#include <mach/avm_gic_smc.h>
#include <mach/avm_gic_fiq.h>
#endif
#endif

#include <avm/rte/rte.h>
#include <avm/sammel/debug.h>
#include "avm_sammel.h"
#include "ar7wdt_private.h"

/* IO_ADDR */
#include <asm/mach/map.h>

#define SCM_CMD_SET_REGSAVE 0x2
#define PSHOLD_REGISTER     0x004AB000
/* these are necessary to disable Secure Debug Image (SDI) and enable ps_hold reset */
#define QTI_SIP_SVC_RESET_DEBUG_ID       0x02000109
#define QTI_SIP_SVC_RESET_DEBUG_PARAM_ID 0x2

#define __get_cpu_var(var)     (*(raw_cpu_ptr((&var))))

struct qcom_wdt {
	struct clk *clk;
	unsigned long rate;
	unsigned long timeout;
	unsigned long initialized;
	struct notifier_block restart_nb;
	void __iomem *base;
	void __iomem *wdt_reset;
	void __iomem *wdt_enable;
	void __iomem *wdt_bark_time;
	void __iomem *wdt_bite_time;
	unsigned int act_wdt_cpu;
	atomic_t wdt_busy;
	int non_sec_wd_irq;
	int sec_wd_irq;
};

static struct timer_list WDTimer[NR_CPUS];
static struct qcom_wdt *wdt;
static atomic_t wdt_active;
static atomic_t wdt_active_int;

static spinlock_t wd_lock;
static spinlock_t avm_rte_die_lock;
static atomic_t wd_sync;
static atomic_t wd_count;
static atomic_t wd_handling;
static atomic_t wd_skip_reboot_cause;
static atomic_t wd_reboot_str;
static atomic_t handling_master;

DEFINE_PER_CPU(struct pt_regs, fregs);

#define RETRY_COUNT 0
unsigned int wd_retry_count = RETRY_COUNT;

static RAW_NOTIFIER_HEAD(avm_nmi_chain);
/**
 * der (pseudo-)nmi-handler ueber fastirq
 */
int register_nmi_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&avm_nmi_chain, nb);
}

static void __iomem *ps_hold_register;
static struct notifier_block early_restart_nb;

/**
 * This is the restart handler that gets used before the qcom watchdog
 * is initialized. Therefore it gets registered in early_initcall and
 * unregistered when the watchdog is ready (see 'ar7wdt_hw_init').
 */
static int qcom_early_restart(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	/* Restart message will not be displayed since its cut off by the system
	 * restart. To print this message during restart put in a sleep or wait
	 * between the printout and the writing to the PSHOLD-Register.
	 */
	pr_info("restart via ps hold\n");

	/* Writing 0 to the PSHOLD-Register triggers a restart. */
	iowrite32(0, ps_hold_register);

	return NOTIFY_DONE;
}

static int __init qcom_early_init(void)
{
	int ret;

	ps_hold_register = ioremap(PSHOLD_REGISTER, 4);

	if (!ps_hold_register) {
		pr_err("ERROR: failed mapping of ps hold register\n");
		return -ENODEV;
	}

	early_restart_nb.notifier_call = qcom_early_restart;
	ret = register_restart_handler(&early_restart_nb);

	if (ret)
		pr_err("ERROR: failed to register early restart handler\n");

	return 0;
}
early_initcall(qcom_early_init);

#if defined(CONFIG_ARCH_IPQ5018)
static __init int enable_ps_hold_reset(void)
{
	register u32 r0 asm("r0") = QTI_SIP_SVC_RESET_DEBUG_ID;
	register u32 r1 asm("r1") = QTI_SIP_SVC_RESET_DEBUG_PARAM_ID;
	register u32 r2 asm("r2") = 1; /* disable wdog debug */
	register u32 r3 asm("r3") = 0; /* disable Secure Debug Image */

	asm volatile(__asmeq("%0", "r0")
		     __asmeq("%1", "r1")
		     __asmeq("%2", "r2")
		     __asmeq("%3", "r3")
		     ".arch_extension sec\n"
		     "smc	#0	@ switch to secure world\n"
		     : "=r"(r0), "=r"(r1), "=r"(r2), "=r"(r3)
		     : "r"(r0), "r"(r1), "r"(r2), "r"(r3)
		     : "cc");

	return r0;
}
early_initcall(enable_ps_hold_reset);
#endif

/**
 */
static long qcom_wdt_configure_bark_dump(void *arg)
{
	long ret = -ENOMEM;
	struct {
		unsigned int addr;
		int len;
	} cmd_buf;

	/* Area for context dump in secure mode */
	void *scm_regsave = (void *)__get_free_page(GFP_KERNEL);

	if (scm_regsave) {
		cmd_buf.addr = virt_to_phys(scm_regsave);
		cmd_buf.len = PAGE_SIZE;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 4, 0))
		ret = qcom_scm_regsave(SCM_SVC_UTIL, SCM_CMD_SET_REGSAVE,
				       scm_regsave, PAGE_SIZE);
#else
		ret = scm_call(SCM_SVC_UTIL, SCM_CMD_SET_REGSAVE, &cmd_buf,
			       sizeof(cmd_buf), NULL, 0);
#endif
	}

	if (ret)
		pr_err("Setting register save address failed.\n"
		       "Registers won't be dumped on a dog bite\n");
	return ret;
}

/**
 */
static int qcom_wdt_start(struct qcom_wdt *wdt)
{
	if (wdt && wdt->initialized) {
		writel_relaxed(0, wdt->wdt_enable);
		dmb();
		writel_relaxed(1, wdt->wdt_reset);
		dmb();
		writel_relaxed(wdt->timeout * wdt->rate, wdt->wdt_bark_time);
		writel_relaxed((wdt->timeout * 2) * wdt->rate,
			       wdt->wdt_bite_time);
		dmb();
		writel_relaxed(1, wdt->wdt_enable);
		dmb();
	} else {
		pr_err("Watchdog is not initialized, can't force reboot\n");
	}
	return 0;
}

/**
 */
static struct pt_regs avm_die_pregs;
#if defined(CONFIG_AVM_FASTIRQ)
static struct pt_regs fiq_regs_save[NR_CPUS];
static struct pt_regs *p_avm_die_pregs = &avm_die_pregs;
#endif
struct pt_regs *collect_pregs(void)
{
	struct pt_regs *pregs;

#if !defined(CONFIG_AVM_FASTIRQ)
	pregs = get_irq_regs();
#else
	avm_tick_jiffies_update(); /*--- auch im FASTIRQ-Fall jiffies korrigieren ---*/

	if (_avm_get_crashed_cpu() == -1) {
		atomic_set(&handling_master, raw_smp_processor_id());
		pr_info("[%s] no crashed_cpu set, yet, handling_master now CPU#%d\n", __func__,
				raw_smp_processor_id());
	}
	pr_info("[%s] set crashed_cpu to handling_master #%d ...\n", __func__, atomic_read(&handling_master));
	pr_info("[%s] ... and switch handling_master to CPU#%d\n", __func__, raw_smp_processor_id());
	avm_set_crashed_cpu(atomic_read(&handling_master));
	atomic_set(&handling_master, raw_smp_processor_id());

	/* pregs einsammeln ...
	 * Skip Reboot-Cause ist in allen nicht-WD-Fällen gesetzt, damit diese den Cause später noch setzen können,
	 * also sind wir hier im WD-Fall
	 */
	if (!(atomic_read(&wd_skip_reboot_cause))) {
		pregs = get_fiq_regs_save_per_cpu(avm_get_crashed_cpu());
		pr_info("[%s] WD-HANDLING CRASHED CPU: PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d\n", __func__,
			(void *)pregs->ARM_pc, (void *)pregs->ARM_lr,
			(void *)pregs->ARM_sp, avm_get_crashed_cpu());
	}
	/* Hier kommen wir also aus einer Exception oder aber panic(...) wurde direkt gerufen */
	else {
		/* EXCEPTION, p_avm_die_pregs transportiert den uns übergebenen ptregs-Struktur-Pointer */
		if (p_avm_die_pregs != NULL) {
			pregs = get_exc_regs(avm_get_crashed_cpu());
			if (pregs == NULL)
				pregs = get_fiq_regs_save_per_cpu(avm_get_crashed_cpu());
			set_fiq_regs_save_per_cpu(pregs, avm_get_crashed_cpu());
			set_exc_regs_per_cpu(NULL, avm_get_crashed_cpu());
			pr_info("[%s] EXCEPTION-HANDLING CRASHED CPU: PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d in mode 0x%x\n", __func__,
				(void *)pregs->ARM_pc,
				(void *)pregs->ARM_lr, (void *)pregs->ARM_sp, avm_get_crashed_cpu(), (unsigned int)processor_mode(pregs));
		/* PANIC, der uns übergebene ptregs-Struktur-Pointer war Null */
		} else {
			p_avm_die_pregs = &avm_die_pregs;
			pregs = get_fiq_regs_save_per_cpu(avm_get_crashed_cpu());
			pr_info("[%s] PANIC-HANDLING CRASHED CPU: PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d\n", __func__,
				(void *)pregs->ARM_pc,
				(void *)pregs->ARM_lr, (void *)pregs->ARM_sp, avm_get_crashed_cpu());
		}
	}
#endif
	avm_die_pregs = *pregs;
	dsb();

	barrier();

	/* Achtung! Diese ausführende CPU muss nicht die gecrashte sein. Diese wurde durch avm_set_crashed_cpu()
	 * gesetzt und kann durch avm_get_crashed_cpu() abgefragt werden. Wir geben ihr aber den Kontext der CRASH-CPU
	 * weiter mit! (Damit alle sich darauf beziehenden Ausgaben in der Folge korrekt sind)
	 */
	return &avm_die_pregs;
}

/**
 */
#ifdef CONFIG_AVM_FASTIRQ
void start_avm_crash_path(struct pt_regs *regs)
{
	set_avm_wdt_handling(0);

	/* prepare_register_for_trap setzt einen ersten Parameter voraus,
	 * der Bestand hat und im weiteren Verlauf des Crash-Handlings gültig bleibt.
	 * Hier wird nur die zuletzt aufgetretene Exception gehalten, daher ist dies auch
	 * in rekursiven Crashes ok. Reentrantes Verhalten wird durch die PER_CPU Define gewahrt.
	 */
	prepare_register_for_trap(&(__get_cpu_var(fregs)), &regs);

	if (!user_mode(regs))
		set_exc_regs(regs);
}

int in_avm_wdt_handling(void)
{
	return atomic_read(&wd_handling);
}
EXPORT_SYMBOL(in_avm_wdt_handling);

void set_avm_wdt_handling(int val)
{
	atomic_set(&wd_handling, val);
}

int in_avm_crash_handling(void)
{
	return atomic_read(&wdt_active_int);
}

int get_wdt_int(void)
{
	return wdt->non_sec_wd_irq;
}

int is_wdt_int(int intNr)
{
	if (wdt && wdt->initialized)
		return ((intNr == wdt->non_sec_wd_irq) || (intNr == wdt->sec_wd_irq) || (intNr == AVM_IRQ_WD_BITE));

	return 0;
}

/**
 */
void avm_wdt_skip_reboot_cause(void)
{
	atomic_set(&wd_skip_reboot_cause, 1);
}

/**
 */
static enum avm_rte_die_return_code force_wd_handling(void)
{
	reinit_GIC();

	avm_bust_mask(FIQ_PRIO_PROFILING, 0);

	return AVM_RTE_DIE_TRIGGER_WD;
}

/**
 */
static enum avm_rte_die_return_code avm_handle_die_panic(const char *str,
							struct pt_regs *regs)
{
	struct pt_regs *pregs;

	console_verbose();

	if (wdt && wdt->initialized) {
		if (regs == NULL) {
			p_avm_die_pregs = NULL;
		}
		atomic_set(&handling_master, raw_smp_processor_id());
		avm_set_crashed_cpu(atomic_read(&handling_master));
		avm_wdt_skip_reboot_cause();
		atomic_set(&wd_reboot_str, (unsigned int)str);
		atomic_set(&wd_count, 0);
		return force_wd_handling();
	} else {
		if (regs == NULL) {
			pregs = collect_pregs();
		} else {
			avm_die_pregs = *regs;
			dsb();

			barrier();

			pregs = regs;
		}
		atomic_set(&wd_handling, 1);

		return AVM_RTE_DIE_CALL_DIE;
	}
}

enum avm_rte_die_return_code avm_rte_panic(const char *str)
{
	if (in_avm_wdt_handling())
		set_exc_regs_per_cpu(NULL, avm_get_crashed_cpu());
	atomic_set(&wd_handling, 0);
	return avm_rte_die(str, NULL);
}

enum avm_rte_die_return_code avm_rte_die(const char *str, struct pt_regs *regs)
{	/* do not print something fom here on, we do not want to risk
	 * to get delayed until the HW-sync signal to all cores was sent
	 */
	if (!in_avm_wdt_handling())
		return avm_handle_die_panic(str, regs);

	if (__raw_rte_spin_trylock(&avm_rte_die_lock)) {
		console_verbose();
		printk_avm_console_bend(0);
		tracing_off();
		debug_locks_off();
		bust_spinlocks(1);

		/* you may start printing again from here on ... */
		pr_emerg("[avm_rte_die]: crashed_cpu is CPU#%d, continuing in SVC\n", avm_get_crashed_cpu());
	} else {
		/* and/or from here on ... */
		pr_emerg("[avm_rte_die, recursive]: crashed_cpu is CPU#%d\n", avm_get_crashed_cpu());
	}

	atomic_set(&wd_handling, 0);

	return AVM_RTE_DIE_CONTINUE;
}

#endif /* CONFIG_AVM_FASTIRQ */

/**
 */
static int qcom_wdt_restart(struct notifier_block *nb,
			    unsigned long action, void *data)
{
#ifdef CONFIG_AVM_FASTIRQ
	pr_err("[%s] ...\n", __func__);

	flush_cache_all();

	avm_secure_wdt_config(1, 10000, 5000);
	/* workaround if reset via secure wdt bite is disabled */
	iowrite32(0, ps_hold_register);
	while (1)
		;
#endif
	return NOTIFY_DONE;
}

#ifdef CONFIG_AVM_FASTIRQ
/**
 * Dies ist eine Einbahnstrasse, es gibt keine Rückkehr.
 * Mode-Umschaltung nach SVC, nachdem die aktuellen Backtraces
 * aus dem FIQ heraus gesichert wurden. Dann das finale Dumpen.
 */

void avm_finalize_session(void)
{
	static DEFINE_SPINLOCK(avm_die_lock);

	if (__raw_rte_spin_trylock(&avm_die_lock)) {
		pr_emerg(
			"FIQ watchdog handling, master CPU calls nmi notifier ...\n");
		raw_notifier_call_chain(&avm_nmi_chain, 0, &avm_die_pregs);

		pr_emerg("FIQ watchdog handling, master CPU calls panic notifier chain ...\n");
		atomic_notifier_call_chain(&panic_notifier_list, 0, NULL);

	} else {
		pr_err("FIQ watchdog handling: recursion in %s...\n", __func__);
		pr_err("... skipping nmi- and/or panic-notifier!\n");
	}


	kmsg_dump(KMSG_DUMP_PANIC);

	pr_emerg(
		"FIQ watchdog handling, master CPU with kmsg_dump() done ...\n");

	console_flush_on_panic();

	do_kernel_restart(NULL);
	// In a panic/die case before the wd is probed, the restart handler ist not
	// registered and thus the system will never restart.
	qcom_wdt_restart(NULL, 0, NULL);
	while (1)
		;
}

/*
 * Das Watchdog-Handling findet im FIQ-Modus statt.
 * NACH dem Backtrace (hier saugen wir ja die unter dem FIQ liegenden,
 * seit dem Handlingstart auch nicht mehr veränderten, Kontexte aus) wechseln
 * wir in den SVC-Modus, um die Restriktionen und Besonderheiten des FIQ nicht in
 * den generischen Notifier-Calls nachziehen zu muessen.
 */
void avm_die(void)
{
	print_oops_end_marker();
	avm_finalize_session();
}
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

/**
 */
int ar7wdt_hw_is_wdt_running(void)
{
	return atomic_read(&wdt_active);
}

/**
 */
void ar7wdt_hw_deinit(void)
{
	/*--- pr_debug( "[qcom:watchdog] stop ...\n"); ---*/
	if (wdt->initialized) {
		writel(0, wdt->wdt_enable);
		ar7wdt_hw_secure_wdt_disable();
		atomic_set(&wdt_active, 0);
	}
}

/**
 */
void ar7wdt_hw_reboot(void)
{
	panic("%s: watchdog expired\n", __func__);
}

/**
 */
static void wd_timer_function(unsigned long context)
{
	/*--- printk(KERN_INFO"%s\n", __func__); ---*/
	if (wdt->initialized) {
		writel(1, wdt->wdt_reset);
		atomic_set(&wd_count, wd_retry_count);
		atomic_set(&wdt->wdt_busy, 0);
	}
}
/**
 * \brief: jede CPU per roundrobin triggern
 */
void ar7wdt_hw_trigger(void)
{
	unsigned int cpu, this_cpu, i;

	if (!wdt->initialized) {
		return;
	}
	if (atomic_read(&wdt->wdt_busy)) {
		return;
	}
	this_cpu = get_cpu();
	/*--- printk(KERN_INFO"[%s] trigger cpu=%u\n", __func__, wdt->act_wdt_cpu); ---*/
	cpu = wdt->act_wdt_cpu;
	for (i = 0; i < num_possible_cpus(); i++, cpu++) {
		if (cpu >= num_possible_cpus())
			cpu = 0;
		if (!cpu_online(cpu)) {
			continue;
		}
		atomic_set(&wdt->wdt_busy, cpu + 1);
		if (cpu == this_cpu) {
			wd_timer_function(0);
			break;
		}
		WDTimer[cpu].expires = jiffies + 1;
		del_timer(&WDTimer[cpu]);
		add_timer_on(&WDTimer[cpu], cpu);
		break;
	}
	put_cpu();
	if (++cpu >= num_possible_cpus())
		cpu = 0;
	wdt->act_wdt_cpu = cpu;
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);

/**
 */
static void die_with_mode_switch(const char *str, struct pt_regs *regs, int err)
{
#if defined(CONFIG_AVM_FASTIRQ)
	static char *priv_str;
	static struct pt_regs *priv_regs;
	static int priv_err;

	priv_str = (char *)str;
	priv_regs = regs;
	priv_err = err;

	dsb();

	barrier();

	/* Mode-Umschaltung, um Memory-Classifier Auflösung zu ermöglichen. Im FIQ würden wir dabei abstürzen ... */
	AVM_CHANGE_MODE(SVC_MODE);

	barrier();

	local_fiq_enable();
#else
#define priv_str str
#define priv_regs regs
#define priv_err err
#endif
	die(priv_str, priv_regs, priv_err);
}

static inline unsigned long read_ISFR(void)
{
	unsigned long value;

	__asm__ __volatile__ ("   MRC p15, 0, %0, c5, c0, 1\n" : "=r" (value));
	return value;
}

static inline unsigned long read_IFAR(void)
{
	unsigned long value;

	__asm__ __volatile__ ("   MRC p15, 0, %0, c6, c0, 2\n" : "=r" (value));
	return value;
}

static inline unsigned long read_DSFR(void)
{
	unsigned long value;

	__asm__ __volatile__ ("   MRC p15, 0, %0, c5, c0, 0\n" : "=r" (value));
	return value;
}

static inline unsigned long read_DFAR(void)
{
	unsigned long value;

	__asm__ __volatile__ ("   MRC p15, 0, %0, c6, c0, 0\n" : "=r" (value));
	return value;
}

/**
 */
static noinline void wait_for_bt_trigger(void)
{
	int cpu = raw_smp_processor_id();

	pr_emerg("setting CPU NR %d offline!\n", cpu);
	set_cpu_online(cpu, 0); /* prevent following smp-calls */

	while (1)
		;
}

/**
 */
static irqreturn_t wdt_bark_isr(int irq, void *arg)
{
#if defined(CONFIG_AVM_FASTIRQ)
	struct pt_regs *regs;
#endif
	struct pt_regs *pregs;
	unsigned int cpu = raw_smp_processor_id();

	int wd_counter = 0;

	pr_debug("[%s] FIQ#%d\n", __func__, irq);

	if (__raw_rte_spin_trylock(&wd_lock)) {
		/* Master Handling, wd_lock war noch frei */
		unsigned int next_cpu;
		int is_recursive_call = in_avm_crash_handling();
		volatile int forced_panic = 0;
		volatile int delay = 0;

		if (wdt && (wdt->initialized) && ((irq == wdt->sec_wd_irq) && (!is_recursive_call))) {
			/* Retry-Count erhöhen */
			atomic_inc(&wd_count);
			pr_debug("[%s] Secure WD master on FIQ#%d, just triggering ...\n", __func__, irq);
		}
		if (is_recursive_call > 1)
			while (1)
				;

		wd_counter = atomic_read(&wd_count);

#if defined(CONFIG_AVM_FASTIRQ)
		/* Secure watchdog reset */
		if (avm_tz_supported_features() & AVM_TZ_IS_AVM_TZ) {
			avm_secure_wdt_pet();
		}
#endif

		/* Non-secure watchdog reset */
		writel_relaxed(1, wdt->wdt_reset);
		dmb();

		/* catch slaves, first set them online */
		for_each_possible_cpu(next_cpu) {
			if (is_recursive_call) {
				if (next_cpu == atomic_read(&handling_master))
					continue;
				pr_emerg("setting CPU NR %d online!\n", next_cpu);
			}
			set_cpu_online(next_cpu, 1); /* allow following smp-calls */
		}
		while (atomic_read(&wd_sync) != (num_possible_cpus() - 1)) {
			if (delay > 100000) {
				pr_emerg(
					"Timeout at FIQ master CPU#%d, slave(s) missing!\n",
					cpu);
				forced_panic = 1;
				break;
			}
			udelay(10);
			delay++;
		}

		/* Master-Slave Lock zuruecksetzen */
		__raw_rte_spin_unlock(&wd_lock);

		barrier();

#if defined(CONFIG_AVM_FASTIRQ)
		/* Jetzt Interrupt-Controller behandeln: spaetes Interrupt-Ack */
		(void)get_ICCIAR(IS_ATOMIC);

		pregs = get_fiq_regs();
		pr_debug("[%s] FIQ WD-HANDLING MASTER: PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d\n", __func__,
				(void *)pregs->ARM_pc, (void *)pregs->ARM_lr,
				(void *)pregs->ARM_sp, cpu);
		regs = &(fiq_regs_save[cpu]);
		copy_banked_regs_full(regs, pregs);
		set_fiq_regs_save(regs);
		pr_debug("[%s] CORRECTED:              PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d\n", __func__,
				(void *)regs->ARM_pc, (void *)regs->ARM_lr,
				(void *)regs->ARM_sp, cpu);
#endif

		if ((forced_panic) || (wd_counter == 0)) {
			const char *buf =
				(const char *)(atomic_read(&wd_reboot_str));

			pr_emerg(
				"FIQ watchdog handling, master: CPU#%d, retry counter=%d, caught slaves: %d\n",
				cpu, wd_counter,
				atomic_read(&wd_sync));

			pr_emerg("Non secure ISFR=0x%lx, DSFR=0x%lx, IFAR=0x%lx, DFAR=0x%lx\n",
				read_ISFR(), read_DSFR(), read_IFAR(), read_DFAR());

#if defined(CONFIG_AVM_FASTIRQ)
			if (!(avm_tz_supported_features() & AVM_TZ_FEAT_FIQ_PREEMPT)) {
				pr_emerg("No FIQ preemption\n");
				/* Disable Wdt */
				avm_gic_fiq_disable(irq, 0);

				/* Interrupt Pending löschen */
				set_ICDICPR(irq, 1, IS_ATOMIC);

				/* Interrupt beenden */
				set_ICCEOIR(irq, IS_ATOMIC);
			} else {
#if defined(CONFIG_AVM_FASTIRQ)
				/* Interrupt Pending löschen */
				set_ICDICPR(irq, 1, IS_ATOMIC);

				wdt->non_sec_wd_irq = wdt->sec_wd_irq;
#endif
			}

			if (!(atomic_read(&wd_skip_reboot_cause))) {
				unsigned int core;

				pr_emerg(
					"no exception or panic, it was the WD\n");

				reinit_GIC();

				for_each_online_cpu(core) {
					set_exc_regs_per_cpu(NULL, core);
				}

				avm_set_reset_status(RS_NMIWATCHDOG);
				buf = "HardwareWatchDog - NMI taken";
			}

			barrier();
#endif
			atomic_set(&wd_handling, 1);

			/* Wir wollen uns nicht nur auf den Restart-Notifier am Ende der
			 *  'die'-Routine verlassen, daher hier schon das Abschalten des
			 * Secure-WDT Bark Triggers (Reset)
			 */
			atomic_inc(&wdt_active_int);

			pregs = collect_pregs();

			pr_emerg(
				"let slaves go awaiting the backtrace trigger\n");
			/* Alle CPUs bereit, hiermit werden die Slaves (bei retry_count = 0)
			 *  zum Erwarten des Backtrace-Trigger geschickt
			 */
			atomic_set(&wd_sync, 0);

			atomic_set(&wd_count, 0);
			atomic_set(&wd_skip_reboot_cause, 0);
			forced_panic = 0;

			pr_emerg("die with mode change\n");

#if defined(CONFIG_AVM_FASTIRQ)
			/* open Monitor FIQ */
			avm_unmask_all_fiqs_up(FIQ_PRIO_WATCHDOG, 0);
#endif
			die_with_mode_switch(buf, pregs, 0);

			// Zur Sicherheit, wir kehren nicht zurueck, Reset wird durch WD gezogen ...
			while (1)
				;
		}

		/* Retry-Count erniedrigen */
		atomic_dec(&wd_count);

#if defined(CONFIG_AVM_FASTIRQ)
		/* Interrupt Pending löschen */
		set_ICDICPR(irq, 1, IS_ATOMIC);

#ifndef CONFIG_AVM_TZ_TYPE_ATF_RTE
		/* Interrupt beenden */
		set_ICCEOIR(irq, IS_ATOMIC);
#endif
#endif
		/* wdt not active any more */
		atomic_set(&wdt_active_int, 0);

		/* Sync löschen, damit Slaves freigeben */
		atomic_set(&wd_sync, 0);

		return IRQ_HANDLED;

	} else {
		/* Slave Handling, wd_lock war schon von einem Master belegt */
#if defined(CONFIG_AVM_FASTIRQ)
		pregs = get_fiq_regs();
		pr_debug("[%s] FIQ WD-HANDLING SLAVE: PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d\n", __func__,
				(void *)pregs->ARM_pc, (void *)pregs->ARM_lr,
				(void *)pregs->ARM_sp, cpu);
		regs = &(fiq_regs_save[cpu]);
		copy_banked_regs_full(regs, pregs);
		set_fiq_regs_save(regs);
		pr_debug("[%s] CORRECTED:              PC=0x%p, LR=0x%p, SP=0x%p on CPU#%d\n", __func__,
				(void *)regs->ARM_pc, (void *)regs->ARM_lr,
				(void *)regs->ARM_sp, cpu);
#endif
		wd_counter = atomic_read(&wd_count);

		if (wd_counter == 0) {
#if defined(CONFIG_AVM_FASTIRQ)
			if (!(avm_tz_supported_features() & AVM_TZ_FEAT_FIQ_PREEMPT)) {
				/* disable target routing of Wdt */
				set_ICDIPTR(irq,
					    get_ICDIPTR(irq, IS_ATOMIC) &
						    ~(1 << cpu),
					    IS_ATOMIC);
			}

			barrier();
#endif
			/* Sync ... */
			atomic_inc(&wd_sync);

			/* Alle CPUs bereit, jetzt werden die Slaves (bei retry_count = 0)
			 *  zum Erwarten des Backtrace-Trigger geschickt
			 */
			while (atomic_read(&wd_sync) != 0)
				;

#if defined(CONFIG_AVM_FASTIRQ)
			/* Interrupt Pending löschen */
			set_ICDICPR(irq, 1, IS_ATOMIC);

			/* open Monitor FIQ */
			avm_unmask_all_fiqs_up(FIQ_PRIO_WATCHDOG, 0);
#endif
			/* Festhalten, IPI-FIRQ-Trigger wird hier reinschlagen ... */
			wait_for_bt_trigger();
		}

		barrier();

		/* Sync ... */
		atomic_inc(&wd_sync);

		/* Festhalten, Slaves werden vom Master koordiniert freigegeben */
		while (atomic_read(&wd_sync) != 0)
			;

		return IRQ_HANDLED;
	}
}

#if defined(CONFIG_AVM_FASTIRQ)
static irqreturn_t wdt_bite_isr(int irq, void *arg)
{
	/* Disable Wdt */
	avm_gic_fiq_disable(AVM_IRQ_WD_BITE, 0);

	atomic_set(&wdt_active_int, 1);
	pr_info("Non-secure watchdog bite triggered. Waiting for secure watchdog handling\n");

	(void)get_ICCIAR(IS_ATOMIC);

	set_ICCEOIR(irq, IS_ATOMIC);

	return IRQ_HANDLED;
}

static irqreturn_t wdt_secure_bark_isr(int irq, void *arg)
{
	if (avm_tz_supported_features() & AVM_TZ_IS_AVM_TZ) {
		wdt_bark_isr(irq, arg);
	}

	return IRQ_HANDLED;
}

/**
 */
static int try_irq_to_desc(int irq)
{
	uint32_t hwirq = irq;
	struct irq_desc *irq_desc = irq_to_desc(irq);

	if (irq_desc)
		hwirq = irq_desc->irq_data.hwirq;

	return hwirq;
}
#endif

void register_wdt_bark_irq(int irq, struct qcom_wdt *wdt)
{
	int ret;
#if defined(CONFIG_AVM_FASTIRQ)
	int cpu;

	if (avm_tz_supported_features() & AVM_TZ_IS_AVM_TZ) {
		ret = avm_request_fiq_on(cpu_possible_mask, irq,
					 wdt_bark_isr, 0,
					 "watchdog_bark", wdt);
		if (!ret) {
			int hwirq = try_irq_to_desc(irq);

			wdt->non_sec_wd_irq = hwirq;

			// Set watchdog to its default priority
			avm_gic_fiq_setup(
				hwirq, cpu_possible_mask, FIQ_PRIO_WATCHDOG, 0x03, 0
			);
			pr_info("[%s] watchdog as fastirq(%u) on all cpus registered with its default prio\n",
				__func__, hwirq);

			ret = avm_request_fiq_on(cpu_possible_mask,
						 AVM_IRQ_WD_BITE, wdt_bite_isr,
						 FIQ_HWIRQ,
						 "watchdog_bite", wdt);
			if (!ret) {
				// Setup of non-secure bite watchdog
				avm_gic_fiq_setup(AVM_IRQ_WD_BITE,
						  cpu_possible_mask,
						  FIQ_PRIO_WATCHDOG, 0x03, 0);
				pr_info("[%s] watchdog as fastirq(%u) on all cpus registered with highest prio\n",
					__func__, AVM_IRQ_WD_BITE);
				return;
			} else {
				for (cpu = 0; cpu < num_possible_cpus();
				     cpu++) {
					(void)avm_free_fiq_on(cpu, irq, wdt);
				}
			}
		}
	}
#endif
	ret = request_irq(irq, wdt_bark_isr, IRQF_TRIGGER_HIGH, "watchdog_bark",
			  wdt);
	if (!ret) {
		pr_info("[%s] watchdog as irq(%u) registered\n", __func__, irq);
		return;
	}
	pr_err("[%s] ERROR request_irq(irq(%d)) ret:%d\n", __func__, irq, ret);
}

void register_secure_wdt_bark_irq(int irq, struct qcom_wdt *wdt)
{
#if defined(CONFIG_AVM_FASTIRQ)
	int ret = 0;

	ret = avm_request_fiq_on(cpu_possible_mask,
				 irq, wdt_secure_bark_isr,
				 FIQ_HWIRQ, "secure-watchdog",
				 wdt);
	if (!ret) {
		// Setup of secure bark watchdog
		avm_gic_fiq_setup(irq,
				  cpu_possible_mask,
				  FIQ_PRIO_MONITOR, 0x03, 0);
		pr_info("[%s] secure watchdog as fastirq(%u) on all cpus registered with highest prio\n",
			__func__, irq);
		return;
	}
#endif
}

/**
 */
void ar7wdt_hw_secure_wdt_disable(void)
{
#if defined(CONFIG_AVM_FASTIRQ)
	/* Auch Secure Config Watchdog triggern */
	/* Disable Secure Watchdog */
	if (avm_tz_supported_features() & AVM_TZ_IS_AVM_TZ) {
		pr_emerg("[%s] disable secure wdt!\n", __func__);
		avm_secure_wdt_config(0, 20000, 40000);
	}
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/
}

/* MODULE INFRASTRUCTURE */
static const struct of_device_id wdt_ipq40xx_match_table[] = {
	{ .compatible = "qcom,kpss-wdt-ipq40xx" },
	{ .compatible = "qcom,kpss-wdt-ipq807x" },
	{ .compatible = "qcom,kpss-wdt-ipq5018" },
	{}
};
MODULE_DEVICE_TABLE(of, wdt_ipq40xx_match_table);

/**
 */
void ar7wdt_hw_init(void)
{
	struct device_node *np = NULL;
	int ret, cpu, i;
	uint32_t val;
	struct resource irqres;

	pr_info("%s...\n", __func__);

	if (!wdt->initialized) {
#if defined(CONFIG_AVM_FASTIRQ)
		if (!(avm_tz_supported_features() & AVM_TZ_IS_AVM_TZ)) {
			pr_err("[%s] ERROR TZ does not support AVM extensions.\n",
			       __func__);
			goto err_out;
		}
#endif

		for (i = 0; !np && (i < ARRAY_SIZE(wdt_ipq40xx_match_table)); i++) {
			np = of_find_compatible_node(NULL, NULL,
						     wdt_ipq40xx_match_table[i].compatible);
		}
		if (!np) {
			pr_err("[%s] ERROR dt-entry\n", __func__);
			goto err_out;
		}

		if (!wdt->base) {
			pr_err("[%s] ERROR base\n", __func__);
			goto err_out;
		}

		if (of_property_read_u32(np, "wdt_res", &val)) {
			pr_err("[%s] ERROR wdt_res not set in dt\n",
			       __func__);
			goto err_out;
		} else {
			wdt->wdt_reset = wdt->base + val;
		}

		if (of_property_read_u32(np, "wdt_en", &val)) {
			pr_err("[%s] ERROR wdt_en not set in dt\n",
			       __func__);
			goto err_out;
		} else {
			wdt->wdt_enable = wdt->base + val;
		}

		if (of_property_read_u32(np, "wdt_bark_time", &val)) {
			pr_err("[%s] ERROR wdt_bark_time not set in dt\n",
			       __func__);
			goto err_out;
		} else {
			wdt->wdt_bark_time = wdt->base + val;
		}

		if (of_property_read_u32(np, "wdt_bite_time", &val)) {
			pr_err("[%s] ERROR wdt_bite_time not set in dt\n",
			       __func__);
			goto err_out;
		} else {
			wdt->wdt_bite_time = wdt->base + val;
		}

		wdt->clk = of_clk_get_by_name(np, NULL);
		if (IS_ERR(wdt->clk)) {
			pr_err("[%s] ERROR failed to get input clock\n",
			       __func__);
			goto err_out;
		}

		ret = of_irq_to_resource_table(np, &irqres, 1);
		if (ret) {
			wdt->non_sec_wd_irq = irqres.start;
			register_wdt_bark_irq(irqres.start, wdt);
		}

		if (of_property_read_u32(np, "wdt_secure_irq", &val)) {
			pr_info("[%s] ERROR wdt_secure_irq not set in dt\n",
				__func__);
		} else {
			wdt->sec_wd_irq = val;
			register_secure_wdt_bark_irq(val, wdt);
		}

		ret = clk_prepare_enable(wdt->clk);
		if (ret) {
			pr_err("[%s] ERROR failed to setup clock\n",
			       __func__);
			goto err_out;
		}

		/*
		 * We use the clock rate to calculate the max timeout, so ensure it's
		 * not zero to avoid a divide-by-zero exception.
		 *
		 * WATCHDOG_CORE assumes units of seconds, if the WDT is clocked such
		 * that it would bite before a second elapses it's usefulness is
		 * limited.  Bail if this is the case.
		 */
		wdt->rate = clk_get_rate(wdt->clk);
		if (wdt->rate == 0 || wdt->rate > 0x10000000U) {
			pr_err("[%s] ERROR invalid clock rate\n",
			       __func__);
			goto err_clk_unprepare;
		}

		if (of_property_read_u32(np, "timeout-sec", &val))
			wdt->timeout = val;
		else
			wdt->timeout = 10;

		/*
		 * WDT restart notifier has priority 0 (use as a last resort)
		 */
		wdt->restart_nb.notifier_call = qcom_wdt_restart;
		ret = register_restart_handler(&wdt->restart_nb);
		if (ret)
			pr_err("[%s] ERROR failed to setup restart handler\n",
			       __func__);
		else
			unregister_restart_handler(&early_restart_nb);

		avm_rte_mark_as_ready();

		wdt->initialized = 1;
	}

	__raw_rte_spin_lock_init(&wd_lock);
	atomic_set(&wd_sync, 0);
	atomic_set(&wd_count, wd_retry_count);
	atomic_set(&wd_handling, 0);
	atomic_set(&wd_skip_reboot_cause, 0);
	atomic_set(&wd_reboot_str, 0);
	atomic_set(&handling_master, 0);
	__raw_rte_spin_lock_init(&avm_rte_die_lock);

	/*--- enable watchdog dump ---*/
	work_on_cpu(0, qcom_wdt_configure_bark_dump, NULL);
	/*--- enable watchdog ---*/
	qcom_wdt_start(wdt);
	pr_info("[%s] Wdt bark time: %d, bite time: %d\n", __func__,
		readl_relaxed(wdt->wdt_bark_time),
		readl_relaxed(wdt->wdt_bite_time));

	atomic_set(&wdt_active, 1);
	atomic_set(&wdt_active_int, 0);
#if defined(CONFIG_AVM_FASTIRQ)
	/* Reconfigure Secure Watchdog */
	if (avm_tz_supported_features() & AVM_TZ_IS_AVM_TZ) {
		avm_secure_wdt_config(1, 20000, 40000);
		pr_info("[%s] Secure watchdog: bark: %ds, bite time: %ds\n",
			__func__, 20, 40);
	}
#endif
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		init_timer(&WDTimer[cpu]);
		WDTimer[cpu].data = (unsigned long)&WDTimer[cpu];
		WDTimer[cpu].function = wd_timer_function;
	}

	return;

err_clk_unprepare:
	clk_disable_unprepare(wdt->clk);
err_out:
	return;
}

static int wdt_ipq40xx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *id;
	struct resource *mem;

	pr_info("%s ...\n", __func__);

	if (!wdt) {
		wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
		if (!wdt)
			return -ENOMEM;
	}

	id = of_match_device(wdt_ipq40xx_match_table, dev);
	if (!id)
		goto err_out;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt->base = devm_ioremap_resource(dev, mem);
	if (IS_ERR(wdt->base))
		goto err_out;

	return 0;

err_out:
	{
		struct qcom_wdt *tmp = wdt;

		wdt = NULL;
		kfree(tmp);
	}

	return -ENODEV;
}

static int wdt_ipq40xx_remove(struct platform_device *pdev)
{
	unregister_restart_handler(&wdt->restart_nb);
	clk_disable_unprepare(wdt->clk);

	{
		struct qcom_wdt *tmp = wdt;

		wdt = NULL;
		kfree(tmp);
	}

	return 0;
}

static struct platform_driver wdt_ipq40xx_driver = {
	.probe = wdt_ipq40xx_probe,
	.remove = wdt_ipq40xx_remove,
	.driver = {
		.name = "qcom,kpss-wdt-ipq40xx",
		.owner = THIS_MODULE,
		.of_match_table = wdt_ipq40xx_match_table,
	},
};
static int __init wdt_ipq40xx_init(void)
{
	return platform_driver_register(&wdt_ipq40xx_driver);
}
core_initcall(wdt_ipq40xx_init);
static void __exit wdt_ipq40xx_exit(void)
{
	platform_driver_unregister(&wdt_ipq40xx_driver);
}
module_exit(wdt_ipq40xx_exit);
