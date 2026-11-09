/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 *                          (C) Copyright 2016~2017 Intel Corporation

	For licensing information, see the file 'LICENSE' in the root folder of
	this software module.

******************************************************************************/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/reset.h>
#include "../../arch/mips/lantiq/clk.h"
#include <linux/irqchip/mips-gic.h>
#include <lantiq.h>
/* #include <cgu.h> */
#include "watchdog_core.h"
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <asm/yield_context.h>

/* RCU MACROs */
static struct regmap *ltq_rcu_base;
#define RCU_IAP_WDT_RST_EN 0x0050
#define RCU_IAP_WDT_RST_STAT 0x0014
#define RCU_WDTx_RESET 0xf
/* Default is 300 seconds > MAX_TIMEOUT so probe reads from DT file timeout */
#define TIMER_MARGIN 300

static unsigned int timeout = TIMER_MARGIN; /* in seconds */
module_param(timeout, uint, 0644);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds\n"
			  " (default=" __MODULE_STRING(TIMER_MARGIN) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0644);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started\n"
			   " (default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct grx500wdt_smp_calldata {
	struct watchdog_device *wdd;
	unsigned int arg;
};

struct grx500wdt_device {
	struct watchdog_device wdd;

	int cpu;
	struct grx500wdt_smp_calldata calldata;
	struct call_single_data csd;
	spinlock_t csd_lock;

	/* Some platform (e.g. prx300) requires explicit resets for
	 * individual modules.
	 */
	int reset_count;
	struct reset_control **resets;
};
#define to_grx500_wdt(wdd) ((struct grx500wdt_device *)(wdd))

static int grx500wdt_cpu(struct watchdog_device *wdd)
{
	struct grx500wdt_device *grxdev = to_grx500_wdt(wdd);

	return grxdev->cpu;
}

static int grx500wdt_smp_call(struct watchdog_device *wdd,
			      smp_call_func_t func,
			      unsigned int arg,
			      bool wait)
{
	struct grx500wdt_smp_calldata calldata_onstack = {
		.wdd = wdd,
		.arg = arg,
	};

	pr_devel("Calling %pf on cpu#%d%s\n",
			func,
			grx500wdt_cpu(wdd),
			wait ? "" : " (async)");

	/* Caution, asynchronous calls have no lock protection on
	 * the remote cpu */

	if (wait) {
		smp_call_function_single(grx500wdt_cpu(wdd), func,
				&calldata_onstack, wait);
	} else {
		struct grx500wdt_device *grxdev = to_grx500_wdt(wdd);
		unsigned long flags;

		/* We must avoid waiting in csd_lock() */

		if (!spin_trylock_irqsave(&grxdev->csd_lock, flags))
			return -EAGAIN;

		if (READ_ONCE(grxdev->csd.flags) == 0) {
			grxdev->calldata = calldata_onstack;
			grxdev->csd.func = func;
			grxdev->csd.info = &grxdev->calldata;

			smp_call_function_single_async(grx500wdt_cpu(wdd),
					&grxdev->csd);
		}

		spin_unlock_irqrestore(&grxdev->csd_lock, flags);
	}

	return 0;
}

#define GRX500WDT_DEFINE_SIMPLE_SMP_FUNC(func)				\
static int func(struct watchdog_device *wdd)				\
{									\
	return grx500wdt_smp_call(wdd, local_ ## func, 0, true);	\
}

static DEFINE_PER_CPU(struct grx500wdt_device, grx500wdt);
static unsigned long cpu_clk;

#ifdef CONFIG_AVM_GRX500_IAP_WDT_NMI
static int grx500wdt_enable_percpu_irq(unsigned int irq)
{
	/* Map irq on target cpu as NMI. The NMI is handled as per notifier. */
	return gic_map_setup(smp_processor_id(), irq, GIC_IRQMODE_NMI, 0);
}
#else
static int grx500wdt_enable_percpu_irq(unsigned int irq)
{
	/* Map to PIN is configured by GIC */

	pr_debug("Enabling irq %d on cpu #%d\n", irq, smp_processor_id());
	enable_percpu_irq(irq, 0);

	return 0;
}
#endif /* CONFIG_AVM_GRX500_IAP_WDT_NMI */

static void __local_grx500wdt_set_start_bit(bool really_start)
{
	uint32_t config0;

	/*
	 * We're in smp call context, where interrupts are disabled,
	 * so the following is atomic.
	 */
	config0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0));
	if (really_start || (config0 & GIC_VPE_WD_START))
		gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0),
			      (config0 | GIC_VPE_WD_START));
}

static void local_grx500wdt_start(void *data)
{
	__local_grx500wdt_set_start_bit(true);
}
GRX500WDT_DEFINE_SIMPLE_SMP_FUNC(grx500wdt_start)

static void local_grx500wdt_stop(void *data)
{
	uint32_t config0;

	config0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0));
	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0),
		      (config0 & ~GIC_VPE_WD_START));
}
GRX500WDT_DEFINE_SIMPLE_SMP_FUNC(grx500wdt_stop)

static void local_grx500wdt_set_timeout(void *data)
{
	struct grx500wdt_smp_calldata *calldata = data;
	struct watchdog_device *grx500_wdt;
	unsigned int new_timeout;

	grx500_wdt = calldata->wdd;
	new_timeout = calldata->arg;

	grx500_wdt->timeout = new_timeout;
	pr_debug("%s: timeout=%d, id=%d wdd=0x%p\n", __func__, new_timeout,
		 grx500_wdt->id, grx500_wdt);

	local_grx500wdt_stop(NULL);

	if (((u64)cpu_clk * (u64)(grx500_wdt->timeout)) <= (u64)U32_MAX) {
		gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_INITIAL0),
			      (cpu_clk * grx500_wdt->timeout));
	} else {
		gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_INITIAL0),
			      (U32_MAX));
	}
	wmb();

	local_grx500wdt_start(NULL);
}

static int grx500wdt_set_timeout(struct watchdog_device *wdd, unsigned int arg)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	return grx500wdt_smp_call(wdd, local_grx500wdt_set_timeout, arg, true);
}

static int local_grx500wdt_get_timeleft(void *data)
{
	struct watchdog_device *wdt_dev = data;
	u32 count0, initial0, config0, rst_en, map0;

	initial0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_INITIAL0));
	rmb();
	config0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0));
	rmb();
	count0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_COUNT0));
	rmb();
	map0 = gic_read_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_MAP));
	rmb();

	regmap_read(ltq_rcu_base, RCU_IAP_WDT_RST_EN, &rst_en);
	pr_info("%s count0=%x config0=%x map0=0x%x left=%lu\n"
		"timeout=%u status=%lu rst_en=0x%x\n",
		__func__,
		count0, config0, map0,
		count0 / cpu_clk, wdt_dev->timeout, wdt_dev->status, rst_en);

	return count0 / cpu_clk;
}

static unsigned int grx500wdt_get_timeleft(struct watchdog_device *wdd)
{
	return (unsigned int)smp_call_on_cpu(grx500wdt_cpu(wdd),
			local_grx500wdt_get_timeleft, wdd, false);
}

static void local_grx500wdt_ping(void *data)
{
	__local_grx500wdt_set_start_bit(false);
}
GRX500WDT_DEFINE_SIMPLE_SMP_FUNC(grx500wdt_ping)

/* Unlocked, instant low-level access for the AVM watchdog */
static int grx500wdt_hw_ping(struct watchdog_device *wdd)
{
	return grx500wdt_smp_call(wdd, local_grx500wdt_ping, 0, false);
}

static irqreturn_t grx500wdt_irq(int irqno, void *param)
{
	struct grx500wdt_device *grx500_wdt;
	int i;
	static int flag;

	grx500_wdt = &per_cpu(grx500wdt, smp_processor_id());

	/* Certain platform (e.g. prx300) does not reset gphy RCU during
	 * global reset. We then explicitly add reset here for workaround.
	 */
	for (i = 0; i < grx500_wdt->reset_count; i++) {
		if (!reset_control_status(grx500_wdt->resets[i]))
			reset_control_assert(grx500_wdt->resets[i]);
	}

	/* enable this for dump data */
	/*	grx500wdt_start(grx500_wdt);*/
	WARN_ONCE(1, " IRQ %d triggered as WDT%d Timer Overflow on CPU %d!\n",
		  irqno, grx500_wdt->wdd.id, smp_processor_id());

	if (flag == 0) {
		show_state();
		flag = 1;
	}
	return IRQ_HANDLED;
}

struct irqaction grx500wdt_irqaction = {
	.handler = grx500wdt_irq,
	.flags = IRQF_PERCPU | IRQF_NO_SUSPEND,
	.name = "watchdog",
};

static const struct watchdog_info grx500wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "Hardware Watchdog for GRX500",
};

static const struct watchdog_ops grx500wdt_ops = {
	.owner = THIS_MODULE,
	.start = grx500wdt_start,
	.stop = grx500wdt_stop,
	.ping = grx500wdt_ping,
	.set_timeout = grx500wdt_set_timeout,
	.get_timeleft = grx500wdt_get_timeleft,
	.avm_hw_ping = grx500wdt_hw_ping,
};

struct grx500wdt_setup_info {
	struct watchdog_device *wdd;
	int irq;
};

static void grx500wdt_init(struct grx500wdt_device *grxdev, int cpu,
			   const struct grx500wdt_device *proto)
{
	grxdev->cpu = cpu;
	spin_lock_init(&grxdev->csd_lock);

	grxdev->reset_count = proto->reset_count;
	grxdev->resets = proto->resets;
}

static void grx500wdt_init_wdd(struct watchdog_device *wdd, struct device *dev)
{
	int ret;

	*wdd = (struct watchdog_device) {
		.info = &grx500wdt_info,
		.ops = &grx500wdt_ops,
		.min_timeout = 1,
		/* .max_timeout = 14,*/
		.max_timeout = U32_MAX / cpu_clk,
		.max_hw_heartbeat_ms = wdd->max_timeout * 1000,
		.min_hw_heartbeat_ms = wdd->min_timeout * 1000,
		/* .timeout = U32_MAX / cpu_clk, */
	};

	ret = watchdog_init_timeout(wdd, timeout, dev);
	if (ret)
		dev_warn(dev, "Error setting timeout to %us: %d\n", timeout, ret);

	watchdog_set_nowayout(wdd, nowayout);
}

static void grx500wdt_init_hw(void *smp_call_arg)
{
	struct grx500wdt_setup_info *setup = smp_call_arg;
	u32 timeout_ticks;

	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_CONFIG0),
			GIC_VPE_WD_TYPE_SCD | GIC_VPE_WD_NWAIT |
			GIC_VPE_WD_WDINTR | GIC_VPE_WD_WDRESET);

	/* FIXME: Since timeout handling is broken WRT cpu clock changes, we
	 * ignore the configured timeout and stick to the maximum. */
	pr_warn_once("Overriding configured wdt timeouts with maximum value\n");
	timeout_ticks = U32_MAX;

	gic_write_reg(GIC_REG(VPE_LOCAL, GIC_VPE_WD_INITIAL0), timeout_ticks);

	grx500wdt_enable_percpu_irq(setup->irq);
}

static int grx500wdt_probe(struct platform_device *pdev)
{
	/* struct resource *wdt_res; */
	struct clk *clk;
	struct grx500wdt_device proto_wdt;
	int ret, cpu, irq, resetcause;
	int i;

	/*
	 * I/O memory need not be taken from Device Tree as WDT is part of
	 * GIC and can be accessed through GIC_[READ/WRITE] and VPE_LOCAL APIs
	 */
	ltq_rcu_base = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						       "lantiq,wdt-rcu");
	if (IS_ERR(ltq_rcu_base)) {
		dev_err(&pdev->dev, "Error retrieving regmap: %ld\n",
				PTR_ERR(ltq_rcu_base));
		return PTR_ERR(ltq_rcu_base);
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Unable to get irq: %d\n", irq);
		return irq;
	}

	proto_wdt.reset_count = of_count_phandle_with_args(pdev->dev.of_node,
						       "resets",
						       "#reset-cells");
	if (proto_wdt.reset_count) {
		proto_wdt.resets =
			devm_kzalloc(&pdev->dev,
				     proto_wdt.reset_count * sizeof(*proto_wdt.resets),
				     GFP_KERNEL);
		if (!proto_wdt.resets)
			return -ENOMEM;
	}

	for (i = 0; i < proto_wdt.reset_count; i++) {
		proto_wdt.resets[i] =
			devm_reset_control_get_shared_by_index(&pdev->dev, i);
		if (IS_ERR(proto_wdt.resets[i])) {
			dev_err(&pdev->dev, "fail to get reset control %d\n",
				i);
			return PTR_ERR(proto_wdt.resets[i]);
		}
	}

	/* set up per-cpu IRQ */
	if (!IS_ENABLED(CONFIG_AVM_GRX500_IAP_WDT_NMI))
		setup_percpu_irq(irq, &grx500wdt_irqaction);

	/* Get the Clock frequency */
	clk = devm_clk_get(&pdev->dev, "freq");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get CPU clock: %ld\n",
			PTR_ERR(clk));
		ret = -1;
		goto exit;
	}

	cpu_clk = clk_get_rate(clk);
	if (!cpu_clk) {
		dev_err(&pdev->dev, "Failed to get CPU clock rate\n");
		ret = -EINVAL;
		goto exit;
	}

	dev_info(&pdev->dev, "[%s]:[%d] cpu_clk=%lu\n",
		 __func__, __LINE__, cpu_clk);
	/* cpu_clk = grx500_clk_get_rate((struct clk *)ltq_grx500_wdt->clk); */

	regmap_read(ltq_rcu_base, RCU_IAP_WDT_RST_STAT, &resetcause);
	dev_info(&pdev->dev, "[%s]WDT reset is Bit31, RCU_IAP_WDT_RST_STAT=0x%x\n",
		 __func__, resetcause);
	if (resetcause & 0x80000000)
		dev_info(&pdev->dev, "[%s]WDT reset.\n", __func__);
	else
		dev_info(&pdev->dev, "[%s]Not WDT reset.\n", __func__);

	/* FIXME: We ignore the "timeout" module parameter */
	timeout = 0;

	for_each_possible_cpu (cpu) {
		struct grx500wdt_setup_info setup;
		struct grx500wdt_device *grx500_wdt;
		struct watchdog_device *wdd;

		grx500_wdt = &per_cpu(grx500wdt, cpu);
		grx500wdt_init(grx500_wdt, cpu, &proto_wdt);

		wdd = &grx500_wdt->wdd;
		grx500wdt_init_wdd(wdd, &pdev->dev);

		if (!cpu_online(cpu)) {
			dev_notice(&pdev->dev, "Skipping initialization of cpu#%d which is not online\n",
					cpu);
			continue;
		}

		setup.wdd = wdd;
		setup.irq = irq;
		ret = smp_call_function_single(cpu, grx500wdt_init_hw, &setup, true);
		if (ret)
			dev_err(&pdev->dev, "Error executing init smp call for wdt on cpu #%d: %d\n",
					cpu, ret);

		ret = watchdog_register_device(wdd);
		if (ret) {
			dev_info(&pdev->dev, "Error registering wachdog device for cpu #%u: %d\n",
					cpu, ret);
			goto exit;
		}
	}

	/* Enable WDT reset to RCU for VPEx */
	regmap_write(ltq_rcu_base, RCU_IAP_WDT_RST_EN, RCU_WDTx_RESET);

	dev_info(&pdev->dev, "H/w Watchdog Timer: timeout %us (max %ld) (nowayout= %d)\n",
		 timeout, (U32_MAX / cpu_clk), nowayout);

	return 0;

exit:
	return ret;
}

static int __exit grx500wdt_remove(struct platform_device *dev)
{
	int cpu;

	for_each_online_cpu (cpu) {
		struct grx500wdt_device *grx500_wdt;

		grx500_wdt = &per_cpu(grx500wdt, cpu);
		dev_info(&dev->dev, "cpu = %d\n", cpu);
		watchdog_unregister_device(&grx500_wdt->wdd);
	}
	return 0;
}

static void grx500wdt_shutdown(struct platform_device *dev)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct grx500wdt_device *grx500_wdt;

		grx500_wdt = &per_cpu(grx500wdt, cpu);
		grx500wdt_stop(&grx500_wdt->wdd);
	}
}

static const struct of_device_id grx500wdt_match[] = {
	{ .compatible = "lantiq,grx500wdt" },
	{},
};

static struct platform_driver grx500wdt_driver = {
	.probe = grx500wdt_probe,
	.remove = __exit_p(grx500wdt_remove),
	.shutdown = grx500wdt_shutdown,
	.driver = {
		.name = "grx500wdt",
		.owner = THIS_MODULE,
		.of_match_table = grx500wdt_match,
	},
};

module_platform_driver(grx500wdt_driver);

MODULE_DESCRIPTION("GRX500 Watchdog Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:grx500wdt");
