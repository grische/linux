// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2015-2019 AVM GmbH <fritzbox_info@avm.de> */

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
#include <linux/irqchip/mips-gic.h>
#include <linux/of_irq.h>
#include <asm/traps.h>

#include <avm/enh/reboot_status.h>
#include <asm/yield_context.h>

#include <asm/mach_avm.h>
#include "avm_sammel.h"

/* WDT MACROs */
#define WD_RESET (1 << 7)
#define WD_INTR  (1 << 6)
#define WD_NWAIT (1 << 5)
#define WD_DEBUG                                                               \
	(1 << 4) /*--- Stop countdown if the VPE is in debug mode. ---*/

/* Second Countdown mode */
#define WD_TYPE_SCD (1 << 1)
#define WD_TYPE_PIT (2 << 1)
#define WD_START    (0x1)

#define NVEC_BASE KSEG1ADDR(0x1f2001e4)
#define RVEC_BASE KSEG1ADDR(0x1f2001e0)
extern void except_avm_vec_nmi(void);

/* RCU MACROs */
void __iomem *rcu_membase = (void *)KSEG1ADDR(0x16000000);

#define RCU_IAP_WDT_RST_EN 0x0050
#define RCU_WDTx_RESET     0xf

#define DBG_ERR(args...) pr_err(args)
/*--- #define DBG_TRC(args...)  pr_err(args) ---*/
#define DBG_TRC(args...) no_printk(args)

static atomic_t nmi_trigger_once;
static int nmi_notify_first(struct notifier_block *self, unsigned long dummy,
			    void *param);
static int nmi_notify_last(struct notifier_block *self, unsigned long dummy,
			   void *param);

static struct notifier_block nmi_nb[2] = {
	{ .notifier_call = nmi_notify_first, .priority = INT_MAX },
	{ .notifier_call = nmi_notify_last, .priority = 0 },
};

/**
 */
struct grx_watchdog {
	struct timer_list timer;
	bool active;
};

static DEFINE_PER_CPU(struct grx_watchdog, WD);

/**
 */
static void grxwdt_start(void)
{
	uint32_t config0;

	config0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0));
	rmb();
	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0), (config0 | WD_START));
	wmb();
}

/**
 */
static void grxwdt_stop(void)
{
	uint32_t config0;

	config0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0));
	rmb();
	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0), (config0 & ~WD_START));
	wmb();
}

static void grxwdt_config(int cpu, int wd_irq, int timeout)
{
	unsigned int config0;

	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0), 0x0); /* Reset CONFIG0 to 0x0 */
	config0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0));
	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0), (config0 | WD_TYPE_SCD  | WD_NWAIT));
	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_INITIAL0), timeout);
	wmb();
	/* map to NMI */
	gic_map_setup(cpu, wd_irq, GIC_IRQMODE_NMI, 0);
}

/**
 */
static void wd_timer_trigger(void)
{
	struct grx_watchdog *pwd = this_cpu_ptr(&WD);
	unsigned long flags;

	local_irq_save(flags);
	if (pwd->active) {
		grxwdt_stop();
		grxwdt_start();
	}
	local_irq_restore(flags);
}

/**
 * es werden soviel timer wie CPU's aufgesetzt
 */
static void wd_timer_function(unsigned long context __maybe_unused)
{
	struct grx_watchdog *pwd = this_cpu_ptr(&WD);

	DBG_TRC("[%s]cpu = %d retrigger\n", __func__, smp_processor_id());
	pwd->timer.expires += (HZ * 1);

	wd_timer_trigger();
	add_timer_on(&pwd->timer, smp_processor_id());
}

/**
 */
static void setup_watchog_per_cpu(void *irq)
{
	struct grx_watchdog *pwd;
	int wd_irq = *(int *)irq;
	int cpu = smp_processor_id();

	DBG_ERR("[grx:watchdog] start on cpu%x\n", cpu);
	DBG_TRC("[%s]:[%d] irq= %d cpu = %d\n", __func__, __LINE__, wd_irq,
		cpu);

	pwd = per_cpu_ptr(&WD, cpu);

	grxwdt_config(cpu, wd_irq, UINT_MAX);

	init_timer(&pwd->timer);
	pwd->timer.data = (unsigned long)pwd;
	/*--- leicht zeitversetzt, damit bei Totalblockade nicht alle zur gleichen Zeit zuschlagen  ---*/
	pwd->timer.expires = jiffies + (HZ * 1) + (cpu * HZ) / 5;
	pwd->timer.function = wd_timer_function;
	pwd->active = true;
	add_timer_on(&pwd->timer, cpu);
}

/**
 */
void ar7wdt_hw_init(void)
{
	unsigned int i;
	int cpu;
	struct device_node *node;
	int wd_irq;

	node = of_find_compatible_node(NULL, NULL, "lantiq,grx500wdt");
	if (!node) {
		pr_err("[grx:watchdog] device_node not found!\n");
		return;
	}
	wd_irq = of_irq_get(node, 0);
	of_node_put(node);
	if (wd_irq < 0) {
		pr_err("[grx:watchdog] unable to get irq!\n");
		return;
	}

	atomic_set(&nmi_trigger_once, 0);
	/*--- set NMI-Base ---*/
	__raw_writel((unsigned long)except_avm_vec_nmi,
		     (void *)RVEC_BASE); /*--- this adress will be taken! ---*/
	__raw_writel((unsigned long)except_avm_vec_nmi, (void *)NVEC_BASE);
	wmb();
	/* Enable WDT reset to RCU for VPEx */
	__raw_writel(RCU_WDTx_RESET, rcu_membase + RCU_IAP_WDT_RST_EN);
	wmb();
	for_each_online_cpu(cpu) {
		/*---    Use enable_percpu_irq() for each Online CPU. ---*/
		smp_call_function_single(cpu,
					 (smp_call_func_t)setup_watchog_per_cpu,
					 &wd_irq, true);
	}
	for (i = 0; i < ARRAY_SIZE(nmi_nb); i++) {
		register_nmi_notifier(&nmi_nb[i]);
	}
}

/**
 */
static void stop_per_cpu_wdt(void *data)
{
	struct grx_watchdog *pwd;

	pwd = this_cpu_ptr(&WD);

	del_timer(&pwd->timer);
	DBG_ERR("[grx:watchdog] stop on cpu%x\n", smp_processor_id());
	pwd->active = false;
	grxwdt_stop();
}

/**
 * export-function
 */
void AVM_WATCHDOG_local_stop(void)
{
	struct grx_watchdog *pwd;
	unsigned long flags;

	local_irq_save(flags);
	pwd = this_cpu_ptr(&WD);
	if (pwd->active) {
		pwd->active = false;
		grxwdt_stop();
	}
	local_irq_restore(flags);
}

/**
 */
int ar7wdt_hw_is_wdt_running(void)
{
	return this_cpu_ptr(&WD)->active;
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
	int cpu;

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, stop_per_cpu_wdt, NULL, true);
	}
}

/**
 */
void ar7wdt_hw_reboot(void)
{
	DBG_TRC("%s!!\n", __func__);
	panic("%s: watchdog expired\n", __func__);
}

/**
 */
void ar7wdt_hw_trigger(void)
{
	wd_timer_trigger();
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);

/**
 */
static int nmi_notify_first(struct notifier_block *self,
			    unsigned long dummy, void *param)
{
	struct pt_regs *regs = (struct pt_regs *)param;

	if (regs)
		regs->cp0_epc = read_c0_errorepc(); /*--- damit backtrace vernuenftig funktioniert ---*/

	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_RMASK), 1 << GIC_LOCAL_INT_WD); /*--- switch off NMI-Mask ---*/
	if (atomic_add_return(1, &nmi_trigger_once) > 1) {
		DBG_TRC("%s cpu=%d ->ignore\n", __func__,
			raw_smp_processor_id());
		for (;;)
			;
	}
	wd_timer_trigger();
	avm_set_reset_status(RS_NMIWATCHDOG);
	avm_tick_jiffies_update();

	avm_stop_all_other_cpus();
	return NOTIFY_OK;
}

/**
 */
static int nmi_notify_last(struct notifier_block *self,
			   unsigned long dummy, void *param)
{
	struct pt_regs *regs = (struct pt_regs *)param;
	char str[100];

	bust_spinlocks(1);
	console_verbose();
	avm_stack_check(NULL);

	printk_avm_console_bend(0); /* force serial-output */
	snprintf(str, sizeof(str), "CPU%d NMI taken (err)epc=%pS ",
		 raw_smp_processor_id(),
		 regs ? (void *)regs->cp0_epc : (void *)read_c0_errorepc());
	die(str, regs);
	return NOTIFY_STOP;
}
