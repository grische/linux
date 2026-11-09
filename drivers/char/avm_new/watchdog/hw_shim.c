// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2018-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/watchdog.h>
#include <linux/module.h>
#include <avm/watchdog/watchdog.h>
#include <linux/kdebug.h>
#include <linux/sched.h>
#include <linux/nmi.h>
#include <asm/traps.h>
#include <asm/hpet.h>
#ifdef CONFIG_X86
#include <asm/nmi.h>
#endif

#include <asm/avm_enh/avm_enh.h>
#include <avm/enh/reboot_status.h>
#include "avm_sammel.h"
#include "ar7wdt_private.h"

#define DRV_NAME "ar7wdt_hw_shim"
#define MAX_WDTS  NR_CPUS

enum wdt_target {
	AR7WDT_TARGET_READY = 0,
	AR7WDT_TARGET_ACTIVE,
	AR7WDT_TARGET_DISABLED
};

struct ar7wdt_shim_info {
	struct watchdog_device *watchdog[MAX_WDTS];
	int			nwdts;
	enum wdt_target         wdt_target;
	struct notifier_block   wdt_device_notifier;
	struct mutex            lock;
};

static struct ar7wdt_shim_info ar7wdt_shim_info = {
	.lock = __MUTEX_INITIALIZER(ar7wdt_shim_info.lock),
};

#ifdef CONFIG_AVM_LOCKUP_DETECTOR
static atomic64_t watchdog_touch_ts;
static u64 watchdog_bark_delay __read_mostly;
static int watchdog_cpu __read_mostly;

static void hit_watchdog_snooze_button(void)
{
	u64 now;

	/*
	 * Order is important here.  First re-arm the timer, only then appease
	 * the nmi handler, otherwise there would be a window with no watchdog
	 * protection.
	 *
	 * Disable preemption so that we get a reasonably accurate timestamp,
	 * and record the timestamp before arming the timer, so that the point
	 * in time being recorded is guaranteed to be earlier than the one
	 * used when programming the timer h/w, and a deviation is harmless.
	 */
	preempt_disable();
	now = cpu_clock(watchdog_cpu);
	avm_nmi_timer_update();

	smp_mb__before_atomic();
	atomic64_set(&watchdog_touch_ts, now);
	preempt_enable();
}

static void watchdog_nmi_configure_bark(unsigned long nmi_delay)
{
	atomic64_set(&watchdog_touch_ts, cpu_clock(watchdog_cpu));
	watchdog_bark_delay = (u64)nmi_delay * NSEC_PER_SEC;
}

/* A 64-bit clone of time_after() */
static bool clock_after(u64 a, u64 b)
{
	return (s64)(b - a) < 0;
}

/*
 * Since the nmi timer is a hpet and manipulating hpet h/w is slow, we don't
 * query the hpet but use the cpu-local clock to estimate if the hpet timer
 * has fired and the watchdog should bark.
 *
 * This can't be exact, but we can tolerate some deviation that results in
 * false positives around the expected timer firing time, if only there are no
 * false negatives which would lead to watchdog failure.
 */
static bool watchdog_should_bark(void)
{
	u64 touch_ts, bark_after, now;
	bool should_bark;

	touch_ts = atomic64_read(&watchdog_touch_ts);
	bark_after = touch_ts + watchdog_bark_delay;
	now = local_clock();

	/* Add a safety margin rather than fully trust the timer */
	now += NSEC_PER_SEC / 2;

	should_bark = clock_after(now, bark_after);

	if (should_bark)
		pr_devel("%s: now=%llu, bark_after=%llu, should_bark=%d\n",
				__func__, now, bark_after, should_bark);

	return should_bark;
}

static int nmi_timer_handler(unsigned int cmd, struct pt_regs *regs)
{
	static atomic_t lock = ATOMIC_INIT(0);

	if (watchdog_should_bark() && !atomic_xchg(&lock, 1))
		nmi_exception(regs);

	return NMI_DONE;
}

static int watchdog_nmi_enable(void)
{
	int result, nmi_delay;

	nmi_delay = 5 * WDT_DEFAULT_TIME;

	result = register_nmi_handler(NMI_LOCAL, nmi_timer_handler, 0, "nmi_wd");
	if (result != 0) {
		pr_err("[%s] register_nmi_handler() failed\n", __func__);
		return result;
	}

	/*
	 * The watchdog uses a cpu-local clock; be careful to set up the
	 * watchdog and the timekeeping on the same cpu.  TODO: Handle this
	 * cpu going offline.
	 */
	watchdog_cpu = smp_processor_id();
	watchdog_nmi_configure_bark(nmi_delay);

	result = avm_nmi_timer_start(nmi_delay, cpumask_of(watchdog_cpu));
	if (result != 0) {
		pr_err("[%s] avm_nmi_timer_start() failed\n", __func__);
		unregister_nmi_handler(NMI_LOCAL, "nmi_wd");
	}

	return result;
}

static void watchdog_nmi_disable(void)
{
	avm_nmi_timer_stop();
	unregister_nmi_handler(NMI_LOCAL, "nmi_wd");
}

static void backtrace_all_cpus(void)
{
	struct cpu_context *ctx;
	int this_cpu, cpu;

	this_cpu = smp_processor_id();

	oops_enter();

	ctx = nmi_get_context(this_cpu);
	if (!ctx->task) {
		pr_emerg("No context for CPU#%d which raised the exception?!\n",
				this_cpu);
	} else {
		avm_stack_check(ctx->task);
		show_task_regs(ctx->task, ctx->regs);
	}

	if (num_online_cpus() > 1)
		pr_emerg("All other CPUs:\n");

	for_each_online_cpu(cpu) {
		if (cpu == this_cpu)
			continue;

		ctx = nmi_get_context(cpu);
		if (!ctx->task) {
			pr_emerg("No context for CPU#%d\n", cpu);
			continue;
		}

		avm_stack_check(ctx->task);
		show_task_regs(ctx->task, ctx->regs);
	}

	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);

	oops_exit(); /* This triggers kmsg_dump */
}
#else /* !CONFIG_AVM_LOCKUP_DETECTOR */
static void hit_watchdog_snooze_button(void) { }
static void watchdog_nmi_enable(void) { }
static void watchdog_nmi_disable(void) { }
#endif

/* avm_tick_jiffies_update() is not present in all F!B kernels */
void __weak avm_tick_jiffies_update(void) { }

static void cpu_self_stop(void)
{
	while (true)
		cpu_relax();
}

static int nmi_notify_first(struct notifier_block *self,
			    unsigned long unused, void *unused2)
{
	static atomic_t oneshot_lock = ATOMIC_INIT(0);

	if (atomic_xchg(&oneshot_lock, 1))
		cpu_self_stop();

	printk_avm_console_bend(0);
	bust_spinlocks(1);
	console_verbose();

	avm_set_reset_status(RS_NMIWATCHDOG);
	avm_tick_jiffies_update();

	return NOTIFY_OK;
}

static int nmi_notify_last(struct notifier_block *self,
			   unsigned long unused, void *unused2)
{

	avm_stack_check(NULL);

#ifdef CONFIG_AVM_WATCHDOG_SHIM_LOCKUP_PANIC
	backtrace_all_cpus();

	/* Tainting the kernel avoids an extra stack dump */
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);

	panic("Fatal exception: NMI");
#endif
	return NOTIFY_OK;
}

static struct notifier_block nmi_exception_nb[] = {
	{
		.notifier_call = nmi_notify_first,
		.priority = INT_MAX,
	}, {
		.notifier_call = nmi_notify_last,
		.priority = 0,
	}
};

static struct ar7wdt_shim_info *ar7wdt_hw_get_shim(void)
{
	return &ar7wdt_shim_info;
}

static int ar7wdt_hw_shim_get_device(struct ar7wdt_shim_info *shim,
				     struct watchdog_device *wdd)
{
	if (shim->nwdts == MAX_WDTS) {
		pr_info("%s: Ignoring extra watchdog device (%s)\n", DRV_NAME,
			wdd->info->identity);
		return -ENOMEM;
	}

	BUG_ON(!wdd->ops->avm_hw_ping);

	if (!try_module_get(wdd->ops->owner)) {
		pr_err("%s: Can't get watchdog module %s\n", DRV_NAME,
		       wdd->ops->owner->name);
		return -ENODEV;
	}

	shim->watchdog[shim->nwdts++] = wdd;
	pr_info("%s: Device #%d attached (%s)\n", DRV_NAME, wdd->id,
			wdd->info->identity);

	return 0;
}

static int ar7wdt_hw_shim_start_watchdog(struct ar7wdt_shim_info *shim)
{
	struct watchdog_device *wdd;
	int i, ret;

	pr_info("%s: Starting watchdog\n", DRV_NAME);

	for (i = 0; i < shim->nwdts; ++i) {
		wdd = shim->watchdog[i];

		ret = watchdog_start(wdd);
		if (ret) {
			pr_err("%s: Starting watchdog failed: %d\n", DRV_NAME, ret);
			return ret;
		}
	}

	watchdog_nmi_enable();

	return 0;
}

static int ar7wdt_hw_shim_stop_watchdog(struct ar7wdt_shim_info *shim)
{
	struct watchdog_device *wdd;
	int i, ret;

	if (!shim->nwdts)
		return 0;

	pr_info("%s: Stopping watchdog\n", DRV_NAME);
	for (i = 0; i < shim->nwdts; ++i) {
		wdd = shim->watchdog[i];

		ret = watchdog_stop(wdd);
		if (ret) {
			pr_err("%s: Error stopping watchdog: %d\n", DRV_NAME, ret);
			return ret;
		}
	}

	watchdog_nmi_disable();

	return 0;
}

static int ar7wdt_hw_shim_put_device(struct ar7wdt_shim_info *shim)
{
	struct watchdog_device *wdd;

	while (shim->nwdts) {
		wdd = shim->watchdog[--shim->nwdts];
		wmb();
		module_put(wdd->ops->owner);
	}

	return 0;
}

static int wdt_device_notify(struct notifier_block *nb, unsigned long action,
			     void *data)
{
	struct ar7wdt_shim_info *shim =
		container_of(nb, struct ar7wdt_shim_info, wdt_device_notifier);
	struct watchdog_device *wdd = data;
	int notified = NOTIFY_OK;
	int ret;

	mutex_lock(&shim->lock);
	switch (action) {
	case WATCHDOG_NOTIFY_ADD_DEVICE:
		ret = ar7wdt_hw_shim_get_device(shim, wdd);
		if (!ret) {
			if (shim->wdt_target == AR7WDT_TARGET_ACTIVE)
				ar7wdt_hw_shim_start_watchdog(shim);
			else
				ar7wdt_hw_shim_stop_watchdog(shim);
		}
		break;
	case WATCHDOG_NOTIFY_DEL_DEVICE:
		WARN(1, "%s: Unsolicited removal of watchdog device!\n",
		     DRV_NAME);
		ar7wdt_hw_shim_stop_watchdog(shim);
		ar7wdt_hw_shim_put_device(shim);
		break;
	default:
		notified = NOTIFY_DONE;
		break;
	}
	mutex_unlock(&shim->lock);

	return notified;
}

static int __init ar7wdt_hw_shim_init(void)
{
	struct ar7wdt_shim_info *shim = ar7wdt_hw_get_shim();

	shim->wdt_device_notifier.notifier_call = wdt_device_notify;
	watchdog_register_notifier(&shim->wdt_device_notifier);

	return 0;
}

/***** ar7wdt interface functions *****/

void ar7wdt_hw_init(void)
{
	struct ar7wdt_shim_info *shim = ar7wdt_hw_get_shim();
	int i;

	mutex_lock(&shim->lock);

	if (shim->wdt_target == AR7WDT_TARGET_DISABLED)
		goto out;
	shim->wdt_target = AR7WDT_TARGET_ACTIVE;

	if (shim->nwdts)
		ar7wdt_hw_shim_start_watchdog(shim);
	else
		pr_info("%s: %s: No watchdog device available (yet)\n",
			DRV_NAME, __func__);
out:
	mutex_unlock(&shim->lock);

	for (i = 0; i < ARRAY_SIZE(nmi_exception_nb); ++i)
		register_nmi_notifier(&nmi_exception_nb[i]);
}

int ar7wdt_hw_is_wdt_running(void)
{
	struct ar7wdt_shim_info *shim = ar7wdt_hw_get_shim();
	struct watchdog_device *wdd;
	bool active = false;
	int i;

	mutex_lock(&shim->lock);

	for (i = 0; i < shim->nwdts; ++i) {
		wdd = shim->watchdog[i];
		active = watchdog_active(wdd);
		if (active)
			break;
	}

	mutex_unlock(&shim->lock);

	return active;
}

void ar7wdt_hw_secure_wdt_disable(void)
{
	struct ar7wdt_shim_info *shim = ar7wdt_hw_get_shim();

	mutex_lock(&shim->lock);

	ar7wdt_hw_shim_stop_watchdog(shim);
	shim->wdt_target = AR7WDT_TARGET_DISABLED;

	mutex_unlock(&shim->lock);
}

void ar7wdt_hw_deinit(void)
{
	struct ar7wdt_shim_info *shim = ar7wdt_hw_get_shim();

	mutex_lock(&shim->lock);

	ar7wdt_hw_shim_stop_watchdog(shim);
	ar7wdt_hw_shim_put_device(shim);

	mutex_unlock(&shim->lock);
}

void ar7wdt_hw_reboot(void)
{
	panic("%s: soft reboot\n", __func__);
}

/* Handle LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0) */
#ifndef smp_mb__before_atomic
#  define smp_mb__before_atomic smp_mb__before_atomic_inc
#endif

void ar7wdt_hw_trigger(void)
{
	struct ar7wdt_shim_info *shim = ar7wdt_hw_get_shim();
	struct watchdog_device *wdd;
	int i;

	/* This must not sleep or take locks */
	for (i = 0; i < shim->nwdts; ++i) {
		wdd = shim->watchdog[i];

		wdd->ops->avm_hw_ping(wdd);
	}

	hit_watchdog_snooze_button();
}

module_init(ar7wdt_hw_shim_init);
