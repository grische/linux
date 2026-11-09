// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <avm/watchdog/watchdog.h>
#include <avm/event/event.h>
#include <linux/fs.h>
#include <linux/vmstat.h>
#include <linux/oom.h>

#include <avm/enh/reboot_status.h>
#include <avm/sammel/simple_proc.h>
#include <avm/profile/profile.h>
#include <avm/compat/timer.h>
#include <avm/compat/misc.h>
#include <avm/enh/semi_seq.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/mm.h>
#include <linux/sched/debug.h>
#include <linux/sched/task_stack.h>
#endif

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(args...) pr_info(args)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(args...) no_printk(args)
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

#include "avm_sammel.h"
#include "ar7wdt_private.h"

/*--- #define DBG_TRIGGER(args...)     pr_info(args) ---*/
#define DBG_TRIGGER(args...) no_printk(args)

#define MAX_WDT_NAME_LEN 64
#define MAX_WDT_APPLS    63 /*--- letzte Position ist fuer init-Ueberwachung reserviert ---*/
#define WDT_APPL_SYSINIT MAX_WDT_APPLS

#define AR7WDT_PANIC_DELAY (5 * HZ) /* Some time for writing backtraces and logs */

struct ar7wdt_timer {
	struct timer_list timer;
	unsigned int handle;
};

struct ar7wdt_hw_timer {
	struct timer_list timer;
#ifdef CONFIG_AVM_WATCHDOG_PER_CPU
	unsigned int cpu;
#endif
};

struct ar7wdt_reboot_work {
	struct work_struct work;
	struct task_struct *hung_task;
};

struct ar7wdt_panic_timer {
	struct timer_list timer;
	const char *panic_string;
};

struct _ar7wdt_appl_data {
	char Name[MAX_WDT_NAME_LEN + 1];
	unsigned int default_time;
	struct ar7wdt_timer Timer;
	unsigned int *pf_owner;
	struct fasync_struct *fasync;
	wait_queue_head_t wait_queue;
	unsigned int  wait_condition;
	unsigned long req_jiffies;
	unsigned long avg_trigger;
	unsigned long worst_trigger;
	unsigned long pagefaults;
	unsigned int crashflag;
	unsigned int crashlog_read_forbidden; /*--- falls dieses flag gesetzt, so darf der crash-log nicht lesbar sein ---*/
	unsigned int is_announce;
	pid_t        pid;
};

struct _ar7wdt_data {
	DECLARE_BITMAP(mask, MAX_WDT_APPLS);
	DECLARE_BITMAP(triggered, MAX_WDT_APPLS);
	DECLARE_BITMAP(requested, MAX_WDT_APPLS);
	DECLARE_BITMAP(states, MAX_WDT_APPLS); /*--- timer schon einmal erfolglos abgelaufen ---*/
	struct _ar7wdt_appl_data appl[MAX_WDT_APPLS + 1];
	struct workqueue_struct *wq;
};

unsigned int watchdog_in_progress;
EXPORT_SYMBOL(watchdog_in_progress);

/* Should (soft) watchdog timeouts trigger a panic? */
bool ar7wdt_panic_on_timeout __ro_after_init = true;
module_param_named(panic_on_timeout, ar7wdt_panic_on_timeout, bool, 0);
EXPORT_SYMBOL(ar7wdt_panic_on_timeout);

static bool avm_watchdog_enabled = true;
module_param_named(enable, avm_watchdog_enabled, bool, 0);

static bool ar7wdt_hw_active;

static void avm_watchdog_reboot(int hung_idx);
/**
 */
static struct _ar7wdt_data ar7wdt_data;
static struct ar7wdt_hw_timer ar7wdt_hw_timer;
static struct ar7wdt_panic_timer ar7wdt_panic_timer;
static struct ar7wdt_reboot_work ar7wdt_reboot_work;

static BLOCKING_NOTIFIER_HEAD(ar7wdt_reboot_nh);

/**
 */
static void AVM_WATCHDOG_timer_handler(struct timer_list *timer);

static unsigned long get_act_pagefaults(void);

/**
 */
static void AVM_WATCHDOG_timer_handler_action(unsigned int idx);

static DEFINE_SPINLOCK(ar7_wdt_lock);
static DEFINE_SPINLOCK(ar7_wdt_timer_lock);

/**
 */
static int _AVM_WATCHDOG_atoi(char *p)
{
	int value = 0;

	while (p && *p && ((*p < '0') || (*p > '9')))
		p++;

	while (p && *p) {
		if (*p >= '0' && *p <= '9') {
			value *= 10;
			value += *p - '0';
		} else {
			break;
		}
		p++;
	}
	return value;
}

static void watchdog_hw_trigger(void)
{
	spin_lock_bh(&ar7_wdt_lock);
	if (READ_ONCE(ar7wdt_hw_active)) {
		pr_devel("Triggering hw watchdog\n");
		ar7wdt_hw_trigger();
	}
	spin_unlock_bh(&ar7_wdt_lock);
}

void AVM_WATCHDOG_local_trigger(void)
{
#ifdef CONFIG_AVM_WATCHDOG_PER_CPU
	spin_lock_bh(&ar7_wdt_timer_lock);
	if (smp_processor_id() == ar7wdt_hw_timer.cpu)
		watchdog_hw_trigger();
	spin_unlock_bh(&ar7_wdt_timer_lock);
#else
	watchdog_hw_trigger();
#endif
}

static unsigned long avm_watchdog_hw_timer_delta(void)
{
	unsigned long interval = HZ * WDT_DEFAULT_TIME;

	if (IS_ENABLED(CONFIG_AVM_ARCH_HAS_PER_CPU_WATCHDOG))
		interval /= num_online_cpus();
	return interval;
}

static void avm_watchdog_hw_timer_handler(struct timer_list *timer)
{
	struct ar7wdt_hw_timer *ar7wdt_hw_timer __maybe_unused;
	unsigned long now, current_delay, timer_delta;

	watchdog_hw_trigger();

	now = jiffies;
	current_delay = now - timer->expires;
	timer_delta = avm_watchdog_hw_timer_delta();

	if (time_after(current_delay, timer_delta / 2)) {
		pr_err("[%s] Warning! last hw-trigger delayed by %ums (timer delta %ums)\n",
		       __func__,
		       jiffies_to_msecs(current_delay),
		       jiffies_to_msecs(timer_delta));
	}

	spin_lock_bh(&ar7_wdt_timer_lock);
	/*
	 * Avoid adding up timer delays by offsetting from the expiry time.
	 * Important for h/w per-cpu timers, and doesn't hurt for global ones.
	 */
	timer->expires += timer_delta;
#ifdef CONFIG_AVM_WATCHDOG_PER_CPU
	ar7wdt_hw_timer = container_of(timer, struct ar7wdt_hw_timer, timer);
	ar7wdt_hw_timer->cpu = cpumask_next_wrap(smp_processor_id(), cpu_online_mask, -1, true);
	add_timer_on(timer, ar7wdt_hw_timer->cpu);
#else
	add_timer(timer);
#endif
	spin_unlock_bh(&ar7_wdt_timer_lock);
}

static void avm_watchdog_start_hw_timer(void)
{
	get_cpu();
	WRITE_ONCE(ar7wdt_hw_active, true);
	ar7wdt_hw_timer.timer.expires = jiffies;
	avm_watchdog_hw_timer_handler(&ar7wdt_hw_timer.timer);
	put_cpu();
}

#define AVM_WATCHDOG_DEL_TIMER 0x01
#define AVM_WATCHDOG_SET_TIMER 0x02
static void _AVM_WATCHDOG_ctrl_timer(int flags, int i)
{
	struct timer_list *timer;
	unsigned long default_time;

	DBG("%s(flags=0x%x, handle=%d)\n", __func__, flags, i + 1);

	timer = &(ar7wdt_data.appl[i].Timer.timer);
	default_time = ar7wdt_data.appl[i].default_time;

	spin_lock_bh(&ar7_wdt_timer_lock);
	if (flags & AVM_WATCHDOG_DEL_TIMER)
		del_timer(timer);
	if (flags & AVM_WATCHDOG_SET_TIMER)
		mod_timer(timer, jiffies + HZ * default_time);
	spin_unlock_bh(&ar7_wdt_timer_lock);
}

/**
 */
void set_watchdog_in_progress(void)
{
	watchdog_in_progress++;
}
static void lproc_wdt(struct seq_file *seq, void *priv);

/**
 */
int __init AVM_WATCHDOG_init(void)
{
	if (!avm_watchdog_enabled) {
		pr_notice("Watchdog disabled.\n");
		ar7wdt_hw_disable_noinit();
		ar7wdt_panic_on_timeout = false;
		return -ENODEV;
	}

	if (!ar7wdt_panic_on_timeout)
		pr_notice("Panic on soft-watchdog timeout is disabled\n");

	DBG("%s:\n", __func__);
	memset(&ar7wdt_data, 0x00, sizeof(ar7wdt_data));
	ar7wdt_data.wq = alloc_workqueue("ar7wdt_reboot_wq",
					 WQ_HIGHPRI | WQ_UNBOUND, 1);
	timer_setup(&ar7wdt_hw_timer.timer, avm_watchdog_hw_timer_handler, 0);

	add_simple_proc_file("avm/wdt", NULL, lproc_wdt, NULL);
	ar7wdt_hw_init();
	avm_watchdog_start_hw_timer();

	return 0;
}

/**
 */
void AVM_WATCHDOG_deinit(void)
{
	del_timer(&ar7wdt_hw_timer.timer);
	if (READ_ONCE(ar7wdt_hw_active))
		ar7wdt_hw_deinit();
}

/**
 */
static int AVM_WATCHDOG_find_handle(char *name)
{
	int i;

	for (i = 0; i < MAX_WDT_APPLS; i++) {
		if (test_bit(i, ar7wdt_data.mask)) {
			if (!strcmp(ar7wdt_data.appl[i].Name, name)) {
				DBG(" handle=%u\n", i + 1);
				return i + 1;
			}
		}
	}
	DBG(" handle= not found\n");
	return 0;
}

/**
 */
static void AVM_WATCHDOG_timer_init_ctrl_handler(struct timer_list *_timer)
{
	if (ar7wdt_panic_on_timeout)
		avm_watchdog_reboot(WDT_APPL_SYSINIT);
}

/**
 */
int AVM_WATCHDOG_init_start(int handle, char *name, int len)
{
	/*--- ignore handle ---*/
	unsigned int Sekunden = _AVM_WATCHDOG_atoi(name);

	if (Sekunden == 0)
		Sekunden = 120; /*--- 2 Minuten ---*/

	init_waitqueue_head(&(ar7wdt_data.appl[WDT_APPL_SYSINIT].wait_queue));
	ar7wdt_data.appl[WDT_APPL_SYSINIT].fasync = NULL;
	ar7wdt_data.appl[WDT_APPL_SYSINIT].default_time = Sekunden;
	ar7wdt_data.appl[WDT_APPL_SYSINIT].Timer.handle = WDT_APPL_SYSINIT + 1;
	timer_setup(&ar7wdt_data.appl[WDT_APPL_SYSINIT].Timer.timer,
			AVM_WATCHDOG_timer_init_ctrl_handler, 0);

	strcpy(ar7wdt_data.appl[WDT_APPL_SYSINIT].Name, "init-ctrl");

	pr_notice("AVM_WATCHDOG: System Init Ueberwachung %u Sekunden\n",
		Sekunden);
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, WDT_APPL_SYSINIT);
	return WDT_APPL_SYSINIT + 1;
}

/**
 */
int AVM_WATCHDOG_init_done(int handle, char *name, int len)
{
	pr_notice("AVM_WATCHDOG: System Init Ueberwachung abgeschlossen (%lu s noch verfuegbar)\n",
		(ar7wdt_data.appl[MAX_WDT_APPLS].Timer.timer.expires - jiffies) / HZ);
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
	boot_profiling_stop();
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, WDT_APPL_SYSINIT);
	return handle;
}

/**
 */
void AVM_WATCHDOG_ungraceful_release(int handle)
{
	int idx = handle - 1;

	DBG("%s(%u)\n", __func__, handle);
	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return;
	}
	if (test_bit(idx, ar7wdt_data.mask)) {
		if (ar7wdt_data.appl[idx].is_announce == 0) {
			ar7wdt_data.appl[idx].crashflag = 1;
			pr_emerg("%s: handle %u (%s) still registered!\n",
				 __func__, handle, ar7wdt_data.appl[idx].Name);
		} else {
			pr_info("[watchdog] %s announced (timeout=%d s)\n",
				ar7wdt_data.appl[idx].Name,
				ar7wdt_data.appl[idx].default_time);
		}
	}
}

/**
 */
static int _AVM_WATCHDOG_register_or_announce(int handle,
					      char *name, int len,
					      unsigned int announce)
{
	int i;

	DBG("%s(%s): start\n", __func__, name);

	if (!avm_watchdog_enabled)
		return -EINVAL; /*--- inval argument ---*/
	name[len] = 0;
	name = strstrip(name);

	for (i = 1; i < MAX_WDT_APPLS; i++) {
		spin_lock_bh(&ar7_wdt_lock);
		if (test_bit(i, ar7wdt_data.mask)) {
			if (strcmp(ar7wdt_data.appl[i].Name, name)) {
				spin_unlock_bh(&ar7_wdt_lock);
				continue;
			}
			ar7wdt_data.appl[i].crashflag = 0;
			ar7wdt_data.appl[i].pid = task_pid_nr(current);
			spin_unlock_bh(&ar7_wdt_lock);
			DBG("%s(%s): already registered (accept) handle=%u\n",
			    __func__, name, i + 1);
			return i + 1;
		}
		spin_unlock_bh(&ar7_wdt_lock);
	}
	DBG("%s(%s): not yet registered\n", __func__, name);

	for (i = 1; i < MAX_WDT_APPLS; i++) {
		spin_lock_bh(&ar7_wdt_lock);
		if (test_bit(i, ar7wdt_data.mask)) {
			spin_unlock_bh(&ar7_wdt_lock);
			continue;
		}
		set_bit(i, ar7wdt_data.mask);
		clear_bit(i, ar7wdt_data.triggered);
		set_bit(i, ar7wdt_data.requested);
		spin_unlock_bh(&ar7_wdt_lock);

		ar7wdt_data.appl[i].req_jiffies = jiffies;
		ar7wdt_data.appl[i].avg_trigger = 0;
		ar7wdt_data.appl[i].pagefaults  = get_act_pagefaults();
		ar7wdt_data.appl[i].crashflag   = 0;
		ar7wdt_data.appl[i].pid		= task_pid_nr(current);
		ar7wdt_data.appl[i].is_announce = announce;
		ar7wdt_data.appl[i].crashlog_read_forbidden = 0;

		ar7wdt_data.appl[i].wait_condition = 0;
		init_waitqueue_head(&(ar7wdt_data.appl[i].wait_queue));
		ar7wdt_data.appl[i].fasync         = NULL;
		ar7wdt_data.appl[i].default_time   = WDT_DEFAULT_TIME;
		ar7wdt_data.appl[i].Timer.handle = i + 1; /*--- handle ---*/
		timer_setup(&ar7wdt_data.appl[i].Timer.timer, AVM_WATCHDOG_timer_handler, 0);
		strncpy(ar7wdt_data.appl[i].Name, name, MAX_WDT_NAME_LEN);
		ar7wdt_data.appl[i].Name[MAX_WDT_NAME_LEN] = '\0';

		_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, i);

		DBG("%s(%s): handle=%u %s pid=%u\n", __func__, name,
		    i + 1,
		    announce ? "announced" : "register", task_pid_nr(current));

		return i + 1;
	}
	DBG("%s(%s): not registered, too many appls\n", __func__, name);
	return -EUSERS; /*--- to many users ---*/
}
int AVM_WATCHDOG_register(int handle, char *name, int len)
{
	return _AVM_WATCHDOG_register_or_announce(handle, name, len, 0);
}

int AVM_WATCHDOG_announce(int handle, char *name, int len)
{
	return _AVM_WATCHDOG_register_or_announce(handle, name, len, 1);
}

/**
 */
struct fasync_struct **AVM_WATCHDOG_get_fasync_ptr(int handle)
{
	int idx = handle - 1;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return NULL; /*--- io error ---*/
	}
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return NULL; /*--- io error ---*/
	}
	return &(ar7wdt_data.appl[idx].fasync);
}

/**
 */
wait_queue_head_t *AVM_WATCHDOG_get_wait_queue(int handle)
{
	int idx = handle - 1;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return NULL; /*--- io error ---*/
	}
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return NULL; /*--- io error ---*/
	}
	return &(ar7wdt_data.appl[idx].wait_queue);
}

/**
 */
int AVM_WATCHDOG_wait_event_interruptible(int handle)
{
	int idx = handle - 1;
	int ret = -EINVAL;

	DBG("%s(hdl=%u)\n", __func__, handle);
	if (handle < 1 || handle > MAX_WDT_APPLS) {
		pr_err("%s(hdl=%u): invalid handle\n", __func__, handle);
		return ret; /*--- io error ---*/
	}
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		pr_err("%s(hdl=%u): invalid handle (not set in mask)\n",
		       __func__, handle);
		return ret; /*--- io error ---*/
	}
	ret = wait_event_interruptible(ar7wdt_data.appl[idx].wait_queue,
				       ar7wdt_data.appl[idx].wait_condition);
	ar7wdt_data.appl[idx].wait_condition = 0;
	DBG("%s(hdl=%u) done\n", __func__, handle);
	return ret;
}

/**
 */
int AVM_WATCHDOG_poll(int handle)
{
	int idx = handle - 1;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	return test_bit(idx, ar7wdt_data.requested) != 0;
}

/**
 */
int AVM_WATCHDOG_release(int handle, char *name, int len)
{
	int idx = handle - 1;

	DBG("%s(hdl=%u): name='%s', len=%u\n", __func__, handle, name, len);
	name[len] = 0;
	name = strstrip(name);

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	if (name[0]) {
		if (strcmp(name, ar7wdt_data.appl[idx].Name)) {
			pr_info("%s(hdl=%u): name='%s', (name and handle do not correspond)\n",
				__func__, idx + 1, name);
			handle = AVM_WATCHDOG_find_handle(name);
			if (handle == 0) {
				DBG("%s(hdl=%u): invalid name\n", __func__,
				    handle);
				return -EINVAL; /*--- inval argument ---*/
			}
			idx = handle - 1;
		}
	}
	spin_lock_bh(&ar7_wdt_lock);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_bh(&ar7_wdt_lock);
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	clear_bit(idx, ar7wdt_data.mask);
	clear_bit(idx, ar7wdt_data.requested);
	clear_bit(idx, ar7wdt_data.states);
	clear_bit(idx, ar7wdt_data.triggered);
	spin_unlock_bh(&ar7_wdt_lock);

	/*--- timer stoppen ---*/
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, idx);

	pr_err("%s(hdl=%u %s): success\n", __func__, handle,
	       ar7wdt_data.appl[idx].Name);
	return 0;
}

/**
 */
int AVM_WATCHDOG_set_timeout(int handle, char *time, int len)
{
	int idx = handle - 1;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	spin_lock_bh(&ar7_wdt_lock);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_bh(&ar7_wdt_lock);
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	ar7wdt_data.appl[idx].default_time = _AVM_WATCHDOG_atoi(time) / 2;
	spin_unlock_bh(&ar7_wdt_lock);

	_AVM_WATCHDOG_ctrl_timer(
		AVM_WATCHDOG_SET_TIMER | AVM_WATCHDOG_DEL_TIMER, idx);
	DBG("%s(hdl=%u): success (new timeout=%u)\n", __func__, handle,
	    ar7wdt_data.appl[idx].default_time);
	return handle;
}

/**
 */
static unsigned long get_act_pagefaults(void)
{
	unsigned long page_faults = 0;

#if defined(CONFIG_VM_EVENT_COUNTERS) && defined(CONFIG_AVM_POWERMETER)
	struct vm_event_state new;

	__all_vm_events((unsigned long *)&new);
	page_faults = new.event[PGFAULT];
#endif /*--- #ifdef CONFIG_VM_EVENT_COUNTERS ---*/
	return page_faults;
}

/**
 */
int AVM_WATCHDOG_trigger(int handle, char *time, int len)
{
	int idx = handle - 1;

	if (!avm_watchdog_enabled)
		return -EINVAL; /*--- inval argument ---*/
	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	spin_lock_bh(&ar7_wdt_lock);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_bh(&ar7_wdt_lock);
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	set_bit(idx, ar7wdt_data.triggered);
	clear_bit(idx, ar7wdt_data.states);
	ar7wdt_data.appl[idx].crashlog_read_forbidden = 0;
	spin_unlock_bh(&ar7_wdt_lock);

	if (likely(ar7wdt_data.appl[idx].avg_trigger)) {
		ar7wdt_data.appl[idx].avg_trigger +=
			((signed long)((jiffies -
					ar7wdt_data.appl[idx].req_jiffies) -
					ar7wdt_data.appl[idx].avg_trigger)) >> 3;
	} else {
		ar7wdt_data.appl[idx].avg_trigger =
			(jiffies - ar7wdt_data.appl[idx].req_jiffies);
	}
	ar7wdt_data.appl[idx].worst_trigger = max(ar7wdt_data.appl[idx].worst_trigger, (jiffies-ar7wdt_data.appl[idx].req_jiffies));
	ar7wdt_data.appl[idx].req_jiffies = jiffies;
	ar7wdt_data.appl[idx].pagefaults = get_act_pagefaults();
	/*--- timer neu aufsetzen ---*/
	_AVM_WATCHDOG_ctrl_timer(
		AVM_WATCHDOG_DEL_TIMER | AVM_WATCHDOG_SET_TIMER, idx);

	DBG_TRIGGER("%s(hdl=%u): %s avg_trigger = %lu success\n", __func__,
		    handle, ar7wdt_data.appl[idx].Name,
		    ar7wdt_data.appl[idx].avg_trigger);

	if (task_pid_nr(current) != ar7wdt_data.appl[idx].pid) {
		DBG("%s: warning %s: trigger-pid(%d) not same than register-pid(%d)\n",
		    __func__, ar7wdt_data.appl[idx].Name, task_pid_nr(current),
		    ar7wdt_data.appl[idx].pid);
		ar7wdt_data.appl[idx].pid = task_pid_nr(current);
	}
	return handle;
}

/**
 */
int AVM_WATCHDOG_disable(int handle, char *time, int len)
{
	int i;

	pr_warn("%s()\n", __func__);
	pr_info("registered appls:\n");
	for (i = 0; i < MAX_WDT_APPLS; i++) {
		spin_lock_bh(&ar7_wdt_lock);
		if (test_bit(i, ar7wdt_data.mask)) {
			clear_bit(i, ar7wdt_data.mask);
			clear_bit(i, ar7wdt_data.requested);
			clear_bit(i, ar7wdt_data.states);
			clear_bit(i, ar7wdt_data.triggered);
			spin_unlock_bh(&ar7_wdt_lock);
			_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, i);
			pr_info("  hdl=%u, %s, disabled.\n", i + 1,
				ar7wdt_data.appl[i].Name);
		} else {
			spin_unlock_bh(&ar7_wdt_lock);
		}
	}
	del_timer(&ar7wdt_hw_timer.timer);

	/* If stopping the wdt may sleep, we have to live with a race here. */
	if (!IS_ENABLED(CONFIG_AVM_WATCHDOG_HW_STARTSTOP_SLEEPS))
		spin_lock_bh(&ar7_wdt_lock);
	if (READ_ONCE(ar7wdt_hw_active)) {
		WRITE_ONCE(ar7wdt_hw_active, false);
		ar7wdt_hw_deinit();
	}
	avm_watchdog_enabled = false;
	if (!IS_ENABLED(CONFIG_AVM_WATCHDOG_HW_STARTSTOP_SLEEPS))
		spin_unlock_bh(&ar7_wdt_lock);

	return 1; /*--- sonst Endlosschleife, weil private_data == 0 ---*/
}

/**
 */
int AVM_WATCHDOG_read(int handle, char *Buffer, int max_len)
{
	int idx = handle - 1;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	spin_lock_bh(&ar7_wdt_lock);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_bh(&ar7_wdt_lock);
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	if (test_bit(idx, ar7wdt_data.requested) == 0) {
		spin_unlock_bh(&ar7_wdt_lock);
		Buffer[0] = '\0';
		DBG("%s(hdl=%u): no request\n", __func__, handle);
		return handle;
	}
	clear_bit(idx, ar7wdt_data.requested);
	spin_unlock_bh(&ar7_wdt_lock);
	max_len = min(max_len - 1, (int)sizeof("alive ?"));
	strncpy(Buffer, "alive ?", max_len);
	Buffer[max_len] = '\0';
	DBG("%s(hdl=%u): request='%s'\n", __func__, handle, Buffer);
	return handle;
}

/**
 * mode = 0: in jiffies
 * sonst in usec (aber liefere msec)
 */
static char *human_timediff(char *buf, long timediff, int mode)
{
	if (mode == 0) {
		if (timediff >= 0) {
			long msec = (timediff * 1000) / HZ;

			sprintf(buf, "%3lu.%03lu s", msec / 1000, msec % 1000);
		} else {
			strcpy(buf, "never");
		}
	} else if (mode == 1) {
		sprintf(buf, "%10lu.%03lu ms", timediff / 1000,
			(timediff % 1000));
	} else {
		sprintf(buf, "%10lu.%03lu us", timediff / 1000,
			(timediff % 1000));
	}
	return buf;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
/**
 */
static unsigned char task_state_to_char(struct task_struct *tsk)
{
	static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	return tsk->state < sizeof(stat_nam) - 1 ? stat_nam[tsk->state] : '?';
}
#endif

/**
 * \note call put_task_struct() to free reference
 *
 */
static struct task_struct *get_taskstruct_by_pid(pid_t pid)
{
	struct task_struct *tsk = NULL;
	struct pid *ppid;

	ppid = find_get_pid(pid);
	if (ppid) {
		tsk = get_pid_task(ppid, PIDTYPE_PID);
		put_pid(ppid);
	}
	return tsk;
}

/**
 */
static int task_to_handle(struct task_struct *task)
{
	int i;

	for_each_set_bit(i, ar7wdt_data.mask, MAX_WDT_APPLS) {
		struct _ar7wdt_appl_data *appl = &ar7wdt_data.appl[i];

		if (strcmp(task->comm, appl->Name) == 0)
			return i + 1;
	}
	/*--- fallback if thread renamed ---*/
	for_each_set_bit(i, ar7wdt_data.mask, MAX_WDT_APPLS) {
		struct _ar7wdt_appl_data *appl = &ar7wdt_data.appl[i];
		struct task_struct *wd_task;
		bool same_process;

		wd_task = get_taskstruct_by_pid(appl->pid);
		if (!wd_task)
			continue;

		same_process = same_thread_group(wd_task, task);
		put_task_struct(wd_task);
		if (same_process) {
			DBG("%s: %u: %s(%d)[wd-name=%s] in (%s(%d)\n",
			    __func__, __LINE__, wd_task->comm,
			    task_pid_nr(wd_task),
			    appl->Name,
			    task->comm, task_pid_nr(task));
			return i + 1;
		}
	}
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#define PGREFILL PGREFILL_NORMAL
#endif

/**
 */
void show_page_statistic(int now)
{
#if defined(CONFIG_VM_EVENT_COUNTERS) && defined(CONFIG_AVM_POWERMETER)
	static int lastjiffies;
	int tdiff = (jiffies - lastjiffies);

	if (now || (tdiff > 10 * HZ)) {
		int i;
		static struct vm_event_state old;
		struct vm_event_state new;

		lastjiffies = jiffies;

		__all_vm_events((unsigned long *)&new);
		for (i = 0; i < NR_VM_EVENT_ITEMS; i++) {
			old.event[i] = new.event[i] - old.event[i];
		}
		tdiff /= HZ;
		if (tdiff == 0)
			tdiff = 1;
		pr_err("PGIN %lu(%lu) PGOUT %lu(%lu) PGFAULT %lu(%lu) SWPIN %lu(%lu) SWPOUT %lu(%lu) PGREFILL %lu(%lu)\n",
		       new.event[PGPGIN], old.event[PGPGIN] / tdiff,
		       new.event[PGPGOUT], old.event[PGPGOUT] / tdiff,
		       new.event[PGFAULT], old.event[PGFAULT] / tdiff,
		       new.event[PSWPIN], old.event[PSWPIN] / tdiff,
		       new.event[PSWPOUT], old.event[PSWPOUT] / tdiff,
		       new.event[PGREFILL], old.event[PGREFILL] / tdiff);
		memcpy(&old, &new, sizeof(old));
	}
#endif /*--- #ifdef CONFIG_VM_EVENT_COUNTERS ---*/
}

/**
 */
static void __sched_show_user_task(struct task_struct *task)
{
	char txtbuf[2][64];
	struct pt_regs *preg __maybe_unused;
	struct mm_struct *mm;

	mm = get_task_mm(task);
	if (mm == NULL) {
		pr_cont("no usercontext\n");
		return;
	}
#if defined(CONFIG_MIPS)
	preg = ((struct pt_regs *)__KSTK_TOS(task));
#endif /*--- #if defined(CONFIG_MIPS) ---*/

#ifdef CONFIG_AVM_SIMPLE_PROFILING
	if (__get_userinfo(txtbuf[0], sizeof(txtbuf[0]), mm, KSTK_EIP(task))) {
		txtbuf[0][0] = 0;
	}
#if defined(CONFIG_MIPS)
	if (__get_userinfo(txtbuf[1], sizeof(txtbuf[1]), mm, preg->regs[31])) {
		txtbuf[1][0] = 0;
	}
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#else /*--- #ifdef CONFIG_AVM_SIMPLE_PROFILING ---*/
	txtbuf[0][0] = txtbuf[1][0] = '\0';
#endif /*--- CONFIG_AVM_SIMPLE_PROFILING ---*/

	pr_cont("\n"
#if defined(CONFIG_MIPS)
		"\tat: %08lx v0: %08lx v1: %08lx\n"
		"\ta0: %08lx a1: %08lx a2: %08lx a3: %08lx\n"
		"\tt0: %08lx t1: %08lx t2: %08lx t3: %08lx\n"
		"\tt4: %08lx t5: %08lx t6: %08lx t7: %08lx\n"
		"\ts0: %08lx s1: %08lx s2: %08lx s3: %08lx\n"
		"\ts4: %08lx s5: %08lx s6: %08lx s7: %08lx\n"
		"\tt8: %08lx t9: %08lx\n"
		"\tgp: %08lx fp: %08lx\n"
#endif /*--- #if defined(CONFIG_MIPS) ---*/
		"\tsp:  %p (start_stack %p)\n"
		"\tepc: %p %s\n"
#if defined(CONFIG_MIPS)
		"\tra:  %p %s\n",
		preg->regs[1], preg->regs[2], preg->regs[3], preg->regs[4],
		preg->regs[5], preg->regs[6], preg->regs[7], preg->regs[8],
		preg->regs[9], preg->regs[10], preg->regs[11], preg->regs[12],
		preg->regs[13], preg->regs[14], preg->regs[15], preg->regs[16],
		preg->regs[17], preg->regs[18], preg->regs[19], preg->regs[20],
		preg->regs[21], preg->regs[22], preg->regs[23], preg->regs[24],
		preg->regs[25], preg->regs[28], preg->regs[30]
#endif /*--- #if defined(CONFIG_MIPS) ---*/
		,
		(void *)KSTK_ESP(task), (void *)mm->start_stack,
		(void *)KSTK_EIP(task), txtbuf[0]
#if defined(CONFIG_MIPS)
		,
		(void *)preg->regs[31], txtbuf[1]
#endif /*--- #if defined(CONFIG_MIPS) ---*/
	);
	/*--- print_stack(mm, KSTK_ESP(task)); ---*/
	mmput_avm_context(mm);
}

/**
 */
static unsigned long accumulate_process_pagefaults(struct task_struct *p)
{
	unsigned long maj_flt = 0;
	struct task_struct *t;

	for_each_thread(p, t)
		maj_flt += t->maj_flt;

	return maj_flt;
}

static void watchdog_dump_task_state(struct seq_file *seq, const char *prefix,
				     int idx, struct task_struct *t)
{
	struct _ar7wdt_appl_data *appl = &ar7wdt_data.appl[idx];
	char trig_last[64], trig_avg[64], trig_worst[64];
	unsigned int handle = idx+1;

	human_timediff(trig_last, jiffies - appl->req_jiffies, 0);
	human_timediff(trig_avg, appl->avg_trigger, 0);
	human_timediff(trig_worst, appl->worst_trigger, 0);

	if (t) {
		unsigned long oom_score = oom_score(t);

		sseq_printf(seq, "%shdl=%2u %-13s pid %4d triggered before: %s(avg %s) worst %s timeout %4u s state %c cpu%x pgfault %6lu oom_score %lu\n",
			 prefix, handle, appl->Name, task_pid_nr(t),
			 trig_last, trig_avg, trig_worst,
			 appl->default_time * 2,
			 task_state_to_char(t), task_cpu(t),
			 accumulate_process_pagefaults(t),
			 oom_score);
	} else {
		const char *appl_crashed;

		appl_crashed = appl->crashflag ?  "maybe crashed" : "";
		sseq_printf(seq, "%shdl=%2u %-13s triggered before: %s(avg %s) worst %s  timeout %4u s %s\n",
			 prefix, handle, appl->Name,
			 trig_last, trig_avg, trig_worst,
			 appl->default_time * 2,
			 appl_crashed);
	}
}

static void watchdog_dump_all_state(struct seq_file *seq, const char *prefix,
				    int notrigger_idx, struct task_struct **hung_task)
{
	DECLARE_BITMAP(handle_mask, MAX_WDT_APPLS) = {0};
	struct task_struct *g;
	int handle, idx;

	if (hung_task)
		*hung_task = NULL;

	rcu_read_lock();
	for_each_process(g) {
		spin_lock_bh(&ar7_wdt_lock);
		handle = task_to_handle(g);
		if (handle == 0) {
			spin_unlock_bh(&ar7_wdt_lock);
			continue;
		}

		idx = handle - 1;
		set_bit(idx, handle_mask);

		watchdog_dump_task_state(seq, prefix, idx, g);
		spin_unlock_bh(&ar7_wdt_lock);

		if (hung_task && notrigger_idx == idx) {
			if (!*hung_task) {
				get_task_struct(g);
				*hung_task = g;
				sseq_printf(seq, " sending SIGBUS to %s (pid= %d)\n",
					    g->comm, task_pid_nr(g));
			} else {
				sseq_puts(seq, " Multiple tasks match the hanging task!\n");
			}
		}
	}
	rcu_read_unlock();

	for (idx = 0; idx < MAX_WDT_APPLS; idx++) {
		spin_lock_bh(&ar7_wdt_lock);
		if (test_bit(idx, ar7wdt_data.mask) && !test_bit(idx, handle_mask))
			watchdog_dump_task_state(seq, prefix, idx, NULL);
		spin_unlock_bh(&ar7_wdt_lock);
	}
}

static struct task_struct *watchdog_task_list(int notrigger_idx)
{
	DEFINE_SEMI_SEQ_SIZ(ssseq, KERN_EMERG, 192);
	struct task_struct *hungtask;
	unsigned long page_faults;
	unsigned long oom_score;

	watchdog_in_progress = 1;

	oom_score = oom_score(current);
	pr_emerg("[%x]avm_watchdog_reboot(hdl=%u, %s): reboot (current: %s oom_score %lu)\n",
		 smp_processor_id(), notrigger_idx + 1,
		 ar7wdt_data.appl[notrigger_idx].Name, current->comm,
		 oom_score);
	page_faults = get_act_pagefaults();
	pr_emerg("pagefaults absolut %lu since last %s-trigger %lu\n",
		 page_faults, ar7wdt_data.appl[notrigger_idx].Name,
		 page_faults - ar7wdt_data.appl[notrigger_idx].pagefaults);

	watchdog_dump_all_state(ssseq_to_seq(ssseq), "  ", notrigger_idx, &hungtask);

	if (hungtask) {
		pr_emerg("avm_watchdog_reboot: kernel context for %s (pid= %d):\n",
			 hungtask->comm, task_pid_nr(hungtask));
		sched_show_task(hungtask);
		pr_emerg("avm_watchdog_reboot: user context for %s:", hungtask->comm);
		__sched_show_user_task(hungtask);
	}

	watchdog_hw_trigger();
	return hungtask;
}

static void signal_hanging_task(struct task_struct *hungtask)
{
	struct task_struct *thread;
	kernel_siginfo_t info;

	info = (kernel_siginfo_t) {
		.si_signo = SIGBUS,
		.si_errno = ETIME,
		.si_code = BUS_OBJERR,
		.si_addr = (void *)0xEBADEBAD,
	};

	rcu_read_lock();
	for_each_thread(hungtask, thread)
		do_send_sig_info(SIGBUS, &info, thread, SEND_SIGINFO_PID);
	rcu_read_unlock();

	if (get_task_state(hungtask) == TASK_UNINTERRUPTIBLE) {
		pr_emerg("avm_watchdog_reboot: wake up task %s (pid= %d):\n",
			hungtask->comm, task_pid_nr(hungtask));
		wake_up_process(hungtask);
	}
}

static void avm_watchdog_reboot_worker(struct work_struct *work)
{
	struct ar7wdt_reboot_work *rw = container_of(work,
			struct ar7wdt_reboot_work, work);

	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE | AVM_OOM_MEMSTAT_ALL);
	blocking_notifier_call_chain(&ar7wdt_reboot_nh, 0, rw->hung_task);
	if (rw->hung_task)
		put_task_struct(rw->hung_task);
}

static void schedule_avm_watchdog_reboot_work(struct task_struct *hung_task)
{
	INIT_WORK(&ar7wdt_reboot_work.work, avm_watchdog_reboot_worker);
	ar7wdt_reboot_work.hung_task = hung_task;
	queue_work(ar7wdt_data.wq, &ar7wdt_reboot_work.work);
}

static void panic_function(struct timer_list *timer)
{
	struct ar7wdt_panic_timer *pt = from_timer(pt, timer, timer);

	watchdog_hw_trigger();
	panic(pt->panic_string);
}

static void panic_delayed(unsigned long delay, const char *panic_string)
{
	struct timer_list *timer = &ar7wdt_panic_timer.timer;

	ar7wdt_panic_timer.panic_string = panic_string;

	timer_setup(timer, panic_function, 0);
	mod_timer(timer, jiffies + delay);
}

static void avm_watchdog_reboot(int hung_idx)
{
	struct task_struct *hung_task;
	const char *panic_string;
	static DEFINE_SPINLOCK(reboot_lock);


	spin_lock(&reboot_lock);
	if (hung_idx == WDT_APPL_SYSINIT) {
		panic_string = "avm_watchdog_reboot: init sequence hangs!";
		hung_task = NULL;
	} else {
		console_verbose();

		panic_string = "avm_watchdog_reboot: delayed watchdog expired";
		hung_task = watchdog_task_list(hung_idx);

		if (IS_ENABLED(CONFIG_SCHEDSTATS) && !ar7wdt_panic_on_timeout)
			show_state();

		avm_stack_check(NULL);
	}


	if (!ar7wdt_panic_on_timeout) {
		schedule_avm_watchdog_reboot_work(hung_task);
	} else {
		static bool fired;

		if (fired)
			goto out;
		fired = true;

		avm_set_reset_status(RS_SOFTWATCHDOG);
		schedule_avm_watchdog_reboot_work(hung_task);

		if (hung_task)
			signal_hanging_task(hung_task);

		panic_delayed(AR7WDT_PANIC_DELAY, panic_string);
	}

out:
	pr_crit("%s(hdl=%u): timer not triggered\n", __func__, hung_idx+1);
	spin_unlock(&reboot_lock);
}

/**
 */
static void AVM_WATCHDOG_timer_handler_action(unsigned int idx)
{
	spin_lock(&ar7_wdt_lock);
	set_bit(idx, ar7wdt_data.states);
	set_bit(idx, ar7wdt_data.requested);
	spin_unlock(&ar7_wdt_lock);

	if (ar7wdt_data.appl[idx].fasync) {
		kill_fasync(&(ar7wdt_data.appl[idx].fasync), SIGIO, POLL_IN);
	} else {
		ar7wdt_data.appl[idx].wait_condition = 1;
		wake_up_interruptible(&(ar7wdt_data.appl[idx].wait_queue));
	}
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, idx);
	DBG_TRIGGER("%s(hdl=%u):%s timer triggered once\n", __func__, idx + 1,
		    ar7wdt_data.appl[idx].Name);
}

/**
 */
static void AVM_WATCHDOG_timer_handler(struct timer_list *timer)
{
	struct ar7wdt_timer *at = from_timer(at, timer, timer);
	int _handle = at->handle;
	int idx, handle = (int)_handle;

	DBG("%s(hdl=%d)\n", __func__, handle);

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return;
	}
	idx = handle - 1;

	spin_lock(&ar7_wdt_lock);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock(&ar7_wdt_lock);
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return;
	}
	if (test_bit(idx, ar7wdt_data.states)) {
		spin_unlock(&ar7_wdt_lock);

		DBG("%s(hdl=%u): timer triggered twice (reboot)\n", __func__,
		    handle);
		avm_watchdog_reboot(idx);
		return;
	}
	spin_unlock(&ar7_wdt_lock);
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, idx);
	AVM_WATCHDOG_timer_handler_action(idx);
}

/**
 */
void AVM_WATCHDOG_emergency_retrigger(void)
{
	/*
	 * This is racy, but we can't take a lock here and we're about to
	 * shutdown anyway, so if we enable the wdt by accident, it shouldn't
	 * matter.
	 */
	if (READ_ONCE(ar7wdt_hw_active))
		ar7wdt_hw_trigger();
}
EXPORT_SYMBOL(AVM_WATCHDOG_emergency_retrigger);

/**
 * \brief
 * Wird im TFFS aufgerufen, wenn die current-Applikation einen Crash-Log schreibt
 * \retval 0 current in wdt-List gefunden
 *
 * \note
 * siehe https://wiki.avm.de/display/BA/Crashreport+Sendestrategien
 *
 */
int AVM_WATCHDOG_Crashlog_notify(void)
{
	unsigned int idx;
	unsigned int handle;

	DBG("%s: %u: %s pid=%d\n", __func__, __LINE__, current->comm,
	    task_pid_nr(current));

	handle = task_to_handle(current);
	if (handle == 0)
		return 1;

	idx = handle - 1;

	spin_lock_bh(&ar7_wdt_lock);
	if (test_bit(idx, ar7wdt_data.mask)) {
		ar7wdt_data.appl[idx].crashlog_read_forbidden = 1;
	}
	spin_unlock_bh(&ar7_wdt_lock);
	/*--- notify that crashlog was written (reset on next watchdog trigger) ---*/
	return 0;
}

int AVM_WATCHDOG_register_reboot_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ar7wdt_reboot_nh, nb);
}
EXPORT_SYMBOL(AVM_WATCHDOG_register_reboot_notifier);

int AVM_WATCHDOG_unregister_reboot_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ar7wdt_reboot_nh, nb);
}
EXPORT_SYMBOL(AVM_WATCHDOG_unregister_reboot_notifier);

/**
 * \brief
 * Wird im TFFS-Treiber beim Zugriff auf /proc/avm/log_cr/crash aufgerufen.
 * Checkt ob ein Crashlog erzeugt und die entsprechende Applikation anschliessend
 * noch keinen Watchdog getriggert hat. Dann ist sie vermutlich abgestuerzt.
 * Dies soll verhindern, dass ein Crash-Log abgeschickt wird, obwohl
 * eigentlich noch ein panic-Log wegen Watchdogueberwachung folgt (verhindere aufgetrennte CRV's).
 * \retval: 1 - sperre Zugriff auf das Log
 *
 * \note
 * siehe https://wiki.avm.de/display/BA/Crashreport+Sendestrategien
 */
int AVM_WATCHDOG_Crashlog_read_forbidden(void)
{
	int ret = 0;
	unsigned int i;

	for (i = 0; i < MAX_WDT_APPLS; i++) {
		spin_lock_bh(&ar7_wdt_lock);
		if (test_bit(i, ar7wdt_data.mask) &&
		    ar7wdt_data.appl[i].crashlog_read_forbidden) {
			spin_unlock_bh(&ar7_wdt_lock);
			DBG("%s: ret=%d handle=%d name=%s\n", __func__, ret,
			    i + 1, ar7wdt_data.appl[i].Name);
			ret = 1;
			break;
		}
		spin_unlock_bh(&ar7_wdt_lock);
	}
	return ret;
}

static void lproc_wdt(struct seq_file *seq, void *priv)
{
	watchdog_dump_all_state(seq, "", 0, NULL);
}
