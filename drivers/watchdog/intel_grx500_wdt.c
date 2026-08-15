// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016-2017 Intel Corporation (original BSP driver)
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * Watchdog driver for the Intel/Lantiq GRX500 (xRX500) SoC family.
 *
 * The SoC has no watchdog block of its own: the timer is the architectural
 * per-VP watchdog inside the MIPS Global Interrupt Controller, plus one SoC
 * register, RCU_IAP_WDT_RST_EN, that decides whether an expiry is visible
 * outside the GIC.
 *
 * Every watchdog register is VP-local, so all four VPs must be armed and fed
 * — an unfed VP resets the board however well the others are fed. One
 * watchdog device is registered whose ops fan out with on_each_cpu().
 */

#define pr_fmt(fmt) "grx500-wdt: " fmt

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/math64.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/percpu.h>
#include <linux/platform_device.h>
#include <linux/processor.h>
#include <linux/regmap.h>
#include <linux/sched/debug.h>
#include <linux/watchdog.h>

#include <asm/irq_regs.h>
#include <asm/mips-cps.h>
#include <asm/mipsregs.h>
#include <asm/time.h>

/*
 * RCU_IAP_WDT_RST_EN. One enable bit per VP: with the bit set, that VP's
 * GIC watchdog reset request reaches the SoC reset controller. The vendor
 * driver writes all four bits in one go and never clears them
 * (grx500_wdt.c:37 and :384), so matching that byte for byte keeps the
 * register in the state the working vendor system runs with.
 */
#define GRX500_WDT_RCU_RST_EN		0x50
#define GRX500_WDT_RCU_RST_EN_ALL_VPS	0xf

/*
 * RCU_IAP_WDT_RST_STAT bit 31 is the vendor driver's "the last reset came
 * from the watchdog" indication (grx500_wdt.c:36 and :338-344).
 *
 * The same word is RCU_RST_STAT, which drivers/reset/reset-lantiq.c polls as
 * its deassert status register (it writes only to +0x10) and whose bit 31
 * xrx500_phy_fw.c takes for the GPHYF reset line. Those two readings cannot
 * both be right, and whether the bit is write-1-to-clear or plain read/write
 * is unresolved -- a blind write-back would clobber GPHY reset status if it
 * is the latter. So the value is reported and left alone; it may therefore be
 * stale rather than a true boot cause. Probe logs the raw word, which is what
 * settles it on hardware.
 */
#define GRX500_WDT_RCU_RST_STAT		0x14
#define GRX500_WDT_RCU_RST_STAT_WDT	BIT(31)

/*
 * The countdown is armed at its full 32-bit range once, at probe, exactly
 * as the vendor does (grx500_wdt.c:367-368 passes U32_MAX to
 * gic_wd_setup_on). Deriving it from the requested timeout instead would
 * need the overflow clamp the vendor driver carries and would make the
 * arithmetic depend on the SCD expiry count; the watchdog core's keepalive
 * worker already bridges any timeout longer than the resulting window.
 */
#define GRX500_WDT_COUNT_MAX		U32_MAX

#define GRX500_WDT_DEFAULT_TIMEOUT	30

/*
 * Margin taken off the measured hardware window before it is handed to the
 * core, so that the core's keepalive always lands inside the real window
 * even if the measurement is a little optimistic.
 */
#define GRX500_WDT_WINDOW_MARGIN_PCT	90

/*
 * Sanity floor on the derived hardware window. Below this the counter is
 * not running at anything like the rate the binding describes, and arming
 * on it would produce a reset loop rather than a watchdog.
 */
#define GRX500_WDT_MIN_WINDOW_MS	1000

/* CP0-count interval the rate measurement runs over, as a Hz divisor. */
#define GRX500_WDT_CAL_HZ		100

/* Warn if the measured rate is more than this far from the DT clock. */
#define GRX500_WDT_CAL_TOLERANCE_PPM	50000

/*
 * Standing GIC_Vx_WD_CONFIG0 contents. START is excluded, and so are the
 * two status bits, so that this value can be written absolutely without
 * asserting anything: writing a 0 to WDINTR/WDRESET is inert under
 * write-1-to-clear semantics and clears them under plain read/write, which
 * is safe under both readings. Only probe writes them as 1s, once.
 *
 * SCD is the only mode that ends in a reset -- interrupt-only stops the
 * countdown at zero and PIT reloads forever -- so it is not a preference
 * inherited from the vendor, it is the mode. NWAIT keeps the counter
 * running while the VP sits in WAIT, which is where an idle router spends
 * most of its time and where a hang is just as fatal. The vendor writes
 * exactly this pair (grx500_wdt.c:367).
 */
#define GRX500_WDT_CONFIG_BASE						  \
	(FIELD_PREP(GIC_VX_WD_CONFIG0_TYPE, GIC_VX_WD_CONFIG0_TYPE_SCD) | \
	 GIC_VX_WD_CONFIG0_NWAIT)

struct grx500_wdt {
	struct watchdog_device wdd;
	struct regmap *rcu;
	unsigned long rate;
	unsigned int window_ms;
	int irq;
	bool irq_masked;
};

/*
 * The GIC watchdog interrupt is a VP-local one, so the core hands the
 * handler a per-CPU cookie. Only the boot VP ever enables it.
 */
static DEFINE_PER_CPU(struct grx500_wdt *, grx500_wdt_pcpu);

static unsigned int timeout;
module_param(timeout, uint, 0444);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (0: use DT/default)");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0444);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
		 __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

/*
 * Feeding is a read-modify-write of GIC_Vx_WD_CONFIG0: the register carries
 * the mode and enable bits alongside the reload, so a blind write would
 * disarm the timer it is supposed to reload.
 */
static void grx500_wdt_arm(void)
{
	unsigned long flags;
	u32 config0;

	local_irq_save(flags);
	config0 = read_gic_vl_wd_config0();
	write_gic_vl_wd_config0(config0 | GIC_VX_WD_CONFIG0_START);
	local_irq_restore(flags);
}

/* The same read-modify-write, clearing START (grx500_wdt.c:168-175). */
static void grx500_wdt_disarm(void)
{
	unsigned long flags;
	u32 config0;

	local_irq_save(flags);
	config0 = read_gic_vl_wd_config0();
	write_gic_vl_wd_config0(config0 & ~GIC_VX_WD_CONFIG0_START);
	local_irq_restore(flags);
}

/*
 * Bring the expiry interrupt back after a survivable stall, once the
 * countdown has been reloaded — unmasking earlier would re-enter immediately.
 */
static void grx500_wdt_recover_irq(struct grx500_wdt *wdt)
{
	if (!wdt->irq || !READ_ONCE(wdt->irq_masked))
		return;

	if (read_gic_vl_wd_config0() & GIC_VX_WD_CONFIG0_WDINTR)
		return;

	WRITE_ONCE(wdt->irq_masked, false);
	enable_percpu_irq(wdt->irq, IRQ_TYPE_NONE);
}

static int grx500_wdt_start(struct watchdog_device *wdd)
{
	struct grx500_wdt *wdt = watchdog_get_drvdata(wdd);

	grx500_wdt_arm();
	grx500_wdt_recover_irq(wdt);

	return 0;
}

static int grx500_wdt_ping(struct watchdog_device *wdd)
{
	struct grx500_wdt *wdt = watchdog_get_drvdata(wdd);

	grx500_wdt_arm();
	grx500_wdt_recover_irq(wdt);

	return 0;
}

static int grx500_wdt_stop(struct watchdog_device *wdd)
{
	grx500_wdt_disarm();

	return 0;
}

/*
 * What is left of the hardware window, converted back to seconds from the
 * counter's own rate.
 */
static unsigned int grx500_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct grx500_wdt *wdt = watchdog_get_drvdata(wdd);

	return read_gic_vl_wd_count0() / wdt->rate;
}

/*
 * Measure what the countdown actually counts at: the rate is taken at probe
 * from the counter itself rather than from a clock property. A zero delta
 * fails probe — a watchdog whose rate is unknown cannot honour a timeout.
 */
static int grx500_wdt_measure_rate(unsigned long dt_rate, unsigned long *out)
{
	u32 c0_begin, c0_end, c0_now, wd_begin, wd_end;
	u32 c0_window, c0_ticks, wd_ticks;
	unsigned long flags;
	u64 rate;

	if (!mips_hpt_frequency)
		return -EOPNOTSUPP;

	c0_window = mips_hpt_frequency / GRX500_WDT_CAL_HZ;

	local_irq_save(flags);

	/*
	 * Free-run the countdown from its full range. This is not an arming:
	 * the measurement window is milliseconds against a window of
	 * seconds, and the counter is stopped again before anything can
	 * expire. Both writes carry WDINTR and WDRESET as zeroes, so neither
	 * asserts a status bit under either register semantics.
	 */
	write_gic_vl_wd_config0(GRX500_WDT_CONFIG_BASE);
	write_gic_vl_wd_initial0(GRX500_WDT_COUNT_MAX);
	write_gic_vl_wd_config0(GRX500_WDT_CONFIG_BASE |
				GIC_VX_WD_CONFIG0_START);

	c0_begin = read_c0_count();
	wd_begin = read_gic_vl_wd_count0();

	do {
		cpu_relax();
		c0_now = read_c0_count();
	} while (c0_now - c0_begin < c0_window);

	wd_end = read_gic_vl_wd_count0();
	c0_end = read_c0_count();

	write_gic_vl_wd_config0(GRX500_WDT_CONFIG_BASE);

	local_irq_restore(flags);

	wd_ticks = wd_begin - wd_end;	/* the watchdog counts down */
	c0_ticks = c0_end - c0_begin;	/* CP0 counts up */

	if (!wd_ticks)
		return -ENODEV;

	if (!c0_ticks)
		return -EAGAIN;

	/*
	 * Range-check before narrowing. unsigned long is 32 bits here, so a
	 * quotient that has already wrapped would sail through a check made
	 * after the assignment.
	 */
	rate = div_u64((u64)wd_ticks * mips_hpt_frequency, c0_ticks);
	if (rate < dt_rate / 4 || rate > (u64)dt_rate * 4)
		return -ERANGE;

	*out = rate;

	return 0;
}

static irqreturn_t grx500_wdt_irq(int irq, void *dev_id)
{
	struct grx500_wdt *wdt = *(struct grx500_wdt **)dev_id;
	struct pt_regs *regs = get_irq_regs();

	/*
	 * In SCD mode the first expiry raises the interrupt and the second
	 * resets the board, so the handler must mask its own source: an
	 * unhandled, unmasked expiry would re-enter for the whole of the
	 * second countdown.
	 */
	disable_percpu_irq(irq);
	WRITE_ONCE(wdt->irq_masked, true);

	pr_emerg("countdown expired, SoC reset follows in ~%u ms\n",
		 wdt->window_ms);

	if (regs)
		show_regs(regs);
	else
		dump_stack();

	/*
	 * Hand the event to the pretimeout governor last, because the panic
	 * governor does not return and the register dump above is the part
	 * that must survive either way.
	 */
	watchdog_notify_pretimeout(&wdt->wdd);

	return IRQ_HANDLED;
}

static void grx500_wdt_free_irq(void *data)
{
	struct grx500_wdt *wdt = data;

	disable_percpu_irq(wdt->irq);
	free_percpu_irq(wdt->irq, &grx500_wdt_pcpu);
}

/*
 * Nothing has run this path on this silicon: the board ships
 * CONFIG_AVM_GRX500_IAP_WDT_NMI=y (defconfig:2252), which routes the expiry
 * to the NMI vector and leaves the vendor's own handler unreachable.
 */
static void grx500_wdt_setup_irq(struct grx500_wdt *wdt,
				 struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int cpu, ret;

	wdt->irq = platform_get_irq_optional(pdev, 0);
	if (wdt->irq <= 0) {
		wdt->irq = 0;
		return;
	}

	for_each_possible_cpu(cpu)
		per_cpu(grx500_wdt_pcpu, cpu) = wdt;

	/*
	 * request_percpu_irq(), not request_irq(): gic_irq_domain_map()
	 * gives local interrupts handle_percpu_devid_irq() and calls
	 * irq_set_percpu_devid(), so the shared-IRQ path would be refused.
	 */
	ret = request_percpu_irq(wdt->irq, grx500_wdt_irq, "grx500-wdt",
				 &grx500_wdt_pcpu);
	if (ret) {
		dev_warn(dev, "request_percpu_irq: %d\n", ret);
		wdt->irq = 0;
		return;
	}

	if (devm_add_action_or_reset(dev, grx500_wdt_free_irq, wdt)) {
		wdt->irq = 0;
		return;
	}

	/*
	 * Unmask through the recovery path rather than directly, so that a
	 * WDINTR which survived the clear above -- which is what a register
	 * that is not write-1-to-clear looks like -- leaves the interrupt
	 * masked instead of re-entering the moment it is enabled.
	 */
	WRITE_ONCE(wdt->irq_masked, true);
	grx500_wdt_recover_irq(wdt);
}

/*
 * WDIOF_PRETIMEOUT is what makes watchdog_have_pretimeout()
 * (drivers/watchdog/watchdog_core.h:73) true, and that in turn is what
 * stops wdt_is_visible() hiding the pretimeout_governor sysfs knob. There
 * is no max_pretimeout field in this kernel; the flag is the whole
 * condition. Without it the governors can be built and still have no way
 * to be selected at runtime.
 */
static const struct watchdog_info grx500_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE |
		   WDIOF_PRETIMEOUT | WDIOF_CARDRESET,
	.identity = "GRX500 GIC Watchdog",
};

/*
 * No .set_timeout and no .set_pretimeout. Both intervals are fixed by the
 * hardware -- one full 32-bit countdown each -- so the only honest thing a
 * setter could do is refuse. The core stores wdd->timeout itself when
 * .set_timeout is absent (watchdog_dev.c watchdog_set_timeout()) and
 * bridges the difference with its keepalive worker.
 *
 * No .restart either: the platform's restart handler is the RCU
 * syscon-reboot node, which registers at priority 192 and is already
 * hardware-proven. Adding one here at the conventional 128 would only
 * create a second candidate for no gain.
 */
static const struct watchdog_ops grx500_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= grx500_wdt_start,
	.stop		= grx500_wdt_stop,
	.ping		= grx500_wdt_ping,
	.get_timeleft	= grx500_wdt_get_timeleft,
};

static int grx500_wdt_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct watchdog_device *wdd;
	struct grx500_wdt *wdt;
	unsigned long dt_rate;
	struct clk *clk;
	u32 config0, rst_en, stat;
	s64 delta_ppm;
	int ret;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdd = &wdt->wdd;

	/*
	 * No reg property: the timer lives in the GIC's VP-local window,
	 * which the CPS accessors reach directly.
	 */
	wdt->rcu = syscon_regmap_lookup_by_phandle(dev->of_node,
						   "intel,rcu-syscon");
	if (IS_ERR(wdt->rcu))
		return dev_err_probe(dev, PTR_ERR(wdt->rcu),
				     "failed to get the RCU syscon\n");

	clk = devm_clk_get(dev, NULL);
	if (IS_ERR(clk))
		return dev_err_probe(dev, PTR_ERR(clk),
				     "failed to get the counter clock\n");

	dt_rate = clk_get_rate(clk);
	if (!dt_rate)
		return dev_err_probe(dev, -EINVAL, "counter clock has no rate\n");

	/*
	 * A zero delta fails probe rather than warning. Either the countdown
	 * is not running -- a watchdog that can never bite, and one that
	 * would look healthy to procd for as long as it was never needed --
	 * or GIC_Vx_WD_COUNT0 does not read back, in which case the rate is
	 * unknowable and get_timeleft would lie. Refusing to register is the
	 * visible failure; the alternative is the silent one.
	 */
	ret = grx500_wdt_measure_rate(dt_rate, &wdt->rate);
	if (ret == -ENODEV)
		return dev_err_probe(dev, ret,
				     "GIC_Vx_WD_COUNT0 did not move: countdown stopped, or the register does not read back\n");

	if (ret) {
		wdt->rate = dt_rate;
		dev_warn(dev, "rate measurement unusable (%d)\n", ret);
		pr_info("tick rate unmeasured, DT %lu Hz (delta n/a)\n",
			dt_rate);
	} else {
		delta_ppm = div_s64(((s64)wdt->rate - (s64)dt_rate) * 1000000,
				    dt_rate);
		pr_info("tick rate measured %lu Hz, DT %lu Hz (delta %ld ppm)\n",
			wdt->rate, dt_rate, (long)delta_ppm);

		if (abs(delta_ppm) > GRX500_WDT_CAL_TOLERANCE_PPM)
			dev_warn(dev, "measured rate disagrees with the DT clock\n");
	}

	wdd->info = &grx500_wdt_info;
	wdd->ops = &grx500_wdt_ops;
	wdd->parent = dev;
	wdd->min_timeout = 1;

	/*
	 * The hardware window is one full countdown; SCD puts the reset on
	 * the second expiry, so the userspace-visible timeout is half of what
	 * the counter runs.
	 */
	wdt->window_ms = div_u64((u64)GRX500_WDT_COUNT_MAX * 1000, wdt->rate);
	wdd->max_hw_heartbeat_ms = wdt->window_ms *
				   GRX500_WDT_WINDOW_MARGIN_PCT / 100;
	if (wdd->max_hw_heartbeat_ms < GRX500_WDT_MIN_WINDOW_MS)
		return dev_err_probe(dev, -ERANGE,
				     "counter rate %lu Hz leaves a %u ms window\n",
				     wdt->rate, wdd->max_hw_heartbeat_ms);

	pr_info("max_hw_heartbeat_ms=%u initial0=0x%08x\n",
		wdd->max_hw_heartbeat_ms, GRX500_WDT_COUNT_MAX);

	wdd->timeout = GRX500_WDT_DEFAULT_TIMEOUT;
	ret = watchdog_init_timeout(wdd, timeout, dev);
	if (ret)
		dev_warn(dev, "using the default %u s timeout\n", wdd->timeout);

	/*
	 * Reported, not settable. The core's nominal meaning for this field
	 * is "seconds before the timeout"; here it is the gap the hardware
	 * puts between the warning and the reset, because both countdowns
	 * are the same fixed length. It has to stay below wdd->timeout, both
	 * to satisfy watchdog_pretimeout_invalid() and because
	 * watchdog_set_timeout() zeroes it otherwise.
	 */
	wdd->pretimeout = min(wdt->window_ms / 1000, wdd->timeout - 1);

	watchdog_set_nowayout(wdd, nowayout);
	watchdog_set_drvdata(wdd, wdt);

	/*
	 * Without this an expiry never leaves the GIC. It used to be written
	 * blind from the CBM ethernet driver for vendor boot-state parity;
	 * this driver is now its only writer, so the value is read back and
	 * reported rather than assumed.
	 */
	ret = regmap_write(wdt->rcu, GRX500_WDT_RCU_RST_EN,
			   GRX500_WDT_RCU_RST_EN_ALL_VPS);
	if (!ret)
		ret = regmap_read(wdt->rcu, GRX500_WDT_RCU_RST_EN, &rst_en);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to enable the watchdog reset\n");

	pr_info("RCU_IAP_WDT_RST_EN readback=0x%08x\n", rst_en);

	ret = regmap_read(wdt->rcu, GRX500_WDT_RCU_RST_STAT, &stat);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read RCU_RST_STAT\n");

	if (stat & GRX500_WDT_RCU_RST_STAT_WDT)
		wdd->bootstatus |= WDIOF_CARDRESET;

	pr_info("RCU_IAP_WDT_RST_STAT=0x%08x bootstatus=0x%x\n",
		stat, wdd->bootstatus);

	/*
	 * Retire whatever status the previous boot left behind: the
	 * reset-cause bits are sticky across a warm reset, so they must be
	 * reported once and then cleared or every later boot inherits them.
	 */
	write_gic_vl_wd_config0(GRX500_WDT_CONFIG_BASE |
				GIC_VX_WD_CONFIG0_WDINTR |
				GIC_VX_WD_CONFIG0_WDRESET);

	config0 = read_gic_vl_wd_config0();
	pr_info("WD_CONFIG0=0x%08x after writing WDINTR back: %s\n", config0,
		(config0 & GIC_VX_WD_CONFIG0_WDINTR) ?
			"WDINTR sticky, not write-1-to-clear" :
			"WDINTR reads 0 after writing 1 (write-1-to-clear, or not implemented)");

	if (config0 & GIC_VX_WD_CONFIG0_WDINTR)
		write_gic_vl_wd_config0(GRX500_WDT_CONFIG_BASE);

	write_gic_vl_wd_initial0(GRX500_WDT_COUNT_MAX);

	grx500_wdt_setup_irq(wdt, pdev);

	if (wdt->irq)
		pr_info("pretimeout irq %d\n", wdt->irq);
	else
		pr_info("pretimeout irq absent\n");

	/*
	 * Arm now and tell the core the hardware is already running, so that
	 * the boot is covered rather than only the part of it after procd
	 * opens the device. The vendor does the same, from a kernel timer
	 * about a second into init. It is safe because
	 * CONFIG_WATCHDOG_HANDLE_BOOT_ENABLED makes the core start feeding
	 * at registration and CONFIG_WATCHDOG_OPEN_TIMEOUT is 0, so
	 * open_deadline is KTIME_MAX and a late or absent procd can never
	 * make it stop.
	 */
	grx500_wdt_arm();
	set_bit(WDOG_HW_RUNNING, &wdd->status);

	ret = devm_watchdog_register_device(dev, wdd);
	if (ret) {
		/* Armed with nobody left to feed it. */
		grx500_wdt_disarm();
		return dev_err_probe(dev, ret, "failed to register\n");
	}

	return 0;
}

/*
 * No .shutdown handler, and no watchdog_stop_on_reboot(): an armed
 * watchdog should survive into the reboot it is supposed to be
 * protecting, and on this platform procd's graceful reboot is known not to
 * reach the syscall at all.
 */
static const struct of_device_id grx500_wdt_match[] = {
	{ .compatible = "intel,grx500-wdt" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, grx500_wdt_match);

static struct platform_driver grx500_wdt_driver = {
	.probe = grx500_wdt_probe,
	.driver = {
		.name = "intel-grx500-wdt",
		.of_match_table = grx500_wdt_match,
	},
};
builtin_platform_driver(grx500_wdt_driver);

MODULE_DESCRIPTION("Intel GRX500 GIC watchdog driver");
MODULE_LICENSE("GPL");
