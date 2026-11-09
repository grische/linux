// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/sysctl.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/swap.h>
#include <avm/watchdog/watchdog.h>
#include <linux/kmsg_dump.h>
#include <avm/event/event.h>
#include <linux/kallsyms.h>
#include <linux/hardirq.h>
#include <linux/unistd.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/slab.h>
#include <linux/oom.h>

#include <avm/enh/reboot_status.h>
#include <avm/sammel/debug.h>
#include <avm/sammel/simple_proc.h>
#include <avm/profile/profile.h>
#include <asm/mach_avm.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/mm.h>
#include <linux/sched/debug.h>
#include <linux/sched/task_stack.h>
#endif

#if defined(CONFIG_AVM_EVENTNODE_PUMA6) && defined(CONFIG_MACH_PUMA6) ||       \
	defined(CONFIG_AVM_EVENTNODE_PUMA7) && defined(CONFIG_ARCH_AVALANCHE)
#define REMOTE_WATCHDOG_HOST
#endif /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA6) && defined(CONFIG_MACH_PUMA6) ---*/

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

struct ar7wdt_timer {
	struct timer_list timer;
	unsigned int handle;
};

struct panic_timer {
	struct timer_list timer;
	struct task_struct *hungtask;
	const char *panic_string;
};

#define STRING_WATCHDOG_EXPIRED   "ar7wdt_hw_reboot: delayed watchdog expired"
#define STRING_INITSEQUENCE_HANGS "ar7wdt_hw_reboot: init sequence hangs !"

static void panic_function(struct timer_list *timer);

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
	unsigned long last_hw_wd_trigger;
	struct _ar7wdt_appl_data appl[MAX_WDT_APPLS + 1];
};

unsigned int watchdog_in_progress;
EXPORT_SYMBOL(watchdog_in_progress);

int ar7wdt_no_reboot;
module_param(ar7wdt_no_reboot, int, 0644);
EXPORT_SYMBOL(ar7wdt_no_reboot);

/**
 */
static struct _ar7wdt_data ar7wdt_data;
static struct ar7wdt_timer ar7wdt_timer;
static struct panic_timer panic_timer;

static ATOMIC_NOTIFIER_HEAD(ar7wdt_reboot_nh);

/**
 */
static void AVM_WATCHDOG_timer_handler(struct timer_list *timer);
static struct semaphore ar7wdt_sema;
extern int ar7wdt_no_reboot;

static unsigned long get_act_pagefaults(void);

/**
 */
static void AVM_WATCHDOG_timer_handler_action(unsigned int idx);
volatile unsigned int AVM_WATCHDOG_locked;

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

/**
 */
#define AVM_WATCHDOG_DEL_TIMER 0x01
#define AVM_WATCHDOG_SET_TIMER 0x02
static void _AVM_WATCHDOG_ctrl_timer(int flags, int i)
{
	unsigned long lockflags;
	struct timer_list *timer;
	unsigned long default_time;

	DBG("%s(flags=0x%x, handle=%d)\n", __func__, flags, i + 1);

	if (i == -1) {
		timer = &ar7wdt_timer.timer;
		default_time = WDT_DEFAULT_TIME;
		if (jiffies - ar7wdt_data.last_hw_wd_trigger >
		    (default_time + default_time / 2) * HZ) {
			int this_cpu = get_cpu();

			pr_err("[%x][%s]Warning! last hw-trigger before %lu s (WDT_DEFAULT_TIME %lu s)\n",
			       this_cpu, __func__,
			       (jiffies - ar7wdt_data.last_hw_wd_trigger) / HZ,
			       default_time);
			put_cpu();
		}
		ar7wdt_data.last_hw_wd_trigger = jiffies;
	} else {
		timer = &(ar7wdt_data.appl[i].Timer.timer);
		default_time = ar7wdt_data.appl[i].default_time;
	}
	spin_lock_irqsave(&ar7_wdt_timer_lock, lockflags);
	if (flags & AVM_WATCHDOG_DEL_TIMER) {
		del_timer(timer);
	}
	if (flags & AVM_WATCHDOG_SET_TIMER) {
		mod_timer(timer, jiffies + HZ * default_time);
	}
	spin_unlock_irqrestore(&ar7_wdt_timer_lock, lockflags);
}

/**
 */
void set_watchdog_in_progress(void)
{
	watchdog_in_progress++;
}
#if defined(REMOTE_WATCHDOG_HOST)
static void *wdt_remote_event_sink_handle;
static int remote_cpu_handle;

/**
 */
static void avm_event_wdt_remote_sink(void *private,
				      unsigned char *buf, unsigned int len)
{
	struct _avm_event_remotewatchdog event;

	if (len < sizeof(struct _avm_event_remotewatchdog)) {
		pr_err("%s: incompatible event len=%u sizeof=%u\n", __func__,
		       len, sizeof(struct _avm_event_remotewatchdog));
		return;
	}
	memcpy(&event, buf, min(len, sizeof(event)));
	if (event.event_header.id != avm_event_id_remotewatchdog) {
		pr_err("%s: incompatible event (id=%u)\n", __func__,
		       event.event_header.id);
		return;
	}
	/*--- pr_err("%s: cmd %d name %s param %d\n", __func__, event.cmd, event.name, event.param);  ---*/
	switch (event.cmd) {
	case wdt_register:
		remote_cpu_handle = AVM_WATCHDOG_register(0, event.name,
							  strlen(event.name));
		if ((remote_cpu_handle >= 0) && event.param) {
			char param[32];

			snprintf(param, sizeof(param), "%d", event.param);
			AVM_WATCHDOG_set_timeout(remote_cpu_handle, param,
						 strlen(param));
		}
		break;
	case wdt_release:
		AVM_WATCHDOG_release(remote_cpu_handle, event.name,
				     strlen(event.name));
		break;
	case wdt_trigger:
		AVM_WATCHDOG_trigger(remote_cpu_handle, event.name,
				     strlen(event.name));
		break;
	default:
		pr_err("%s: incompatible remote_cmd(%u)\n", __func__,
		       event.cmd);
		break;
	}
}

/**
 */
#define is_handle_from_remote_cpu(idx)                                         \
	(((idx + 1) == remote_cpu_handle) ? 1 : 0)
#else
#define is_handle_from_remote_cpu(idx) 0
#endif /*--- #if defined(REMOTE_WATCHDOG_HOST) ---*/
static void lproc_wdt(struct seq_file *seq, void *priv);

/**
 */
void AVM_WATCHDOG_init(void)
{
	DBG("%s:\n", __func__);
	memset(&ar7wdt_data, 0x00, sizeof(ar7wdt_data));
	ar7wdt_data.last_hw_wd_trigger = jiffies;
	sema_init(&ar7wdt_sema, 1);
	timer_setup(&ar7wdt_timer.timer, NULL, 0);
	timer_setup(&panic_timer.timer, NULL, 0);

	if (ar7wdt_no_reboot <= 1) {
		ar7wdt_timer.timer.function = (timer_func_t)AVM_WATCHDOG_timer_handler;
		ar7wdt_timer.handle = -1;
		_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, -1);
	}
#if defined(REMOTE_WATCHDOG_HOST)
	{
		struct _avm_event_id_mask id_mask;

		wdt_remote_event_sink_handle = avm_event_sink_register(
			"avm_event_remotewatchdog_sink",
			avm_event_build_id_mask(&id_mask, 1,
						avm_event_id_remotewatchdog),
			avm_event_wdt_remote_sink, NULL);
		if (wdt_remote_event_sink_handle == NULL) {
			pr_err("%s not registered\n", __func__);
		}
	}
#endif /*--- #if defined(REMOTE_WATCHDOG_HOST) ---*/
	add_simple_proc_file("avm/wdt", NULL, lproc_wdt, NULL);
	if (ar7wdt_no_reboot <= 1) {
		ar7wdt_hw_init();
		ar7wdt_hw_trigger();
	}
}

/**
 */
void AVM_WATCHDOG_deinit(void)
{
	DBG("%s:\n", __func__);
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, -1);
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
static void panic_delayed(struct task_struct *hungtask, unsigned long timeout,
			  const char *panic_string)
{
	if (panic_timer.timer.function)
		return;
	panic_timer.hungtask       = hungtask;
	panic_timer.timer.function = (timer_func_t)panic_function;
	panic_timer.timer.expires = jiffies + timeout;
	panic_timer.panic_string  = panic_string;
	add_timer(&panic_timer.timer);
}

/**
 */
static void AVM_WATCHDOG_timer_init_ctrl_handler(struct timer_list *_timer)
{
	if (ar7wdt_no_reboot == 0) {
		/*
		 * nur im Reboot-Status RS_SOFTWATCHDOG bleibt User-Log
		 * nach Reboot erhalten
		 */
		avm_set_reset_status(RS_SOFTWATCHDOG);
		/*
		 * panic verzögern um asynchrones Schreiben des User-Logs in
		 * Notifier-Callchain zu ermöglichen
		 */
		atomic_notifier_call_chain(&ar7wdt_reboot_nh, 0, get_current());
		panic_delayed(NULL, 5 * HZ, STRING_INITSEQUENCE_HANGS);
	}
}

/**
 */
int AVM_WATCHDOG_init_start(int handle, char *name, int len)
{
	/*--- ignore handle ---*/
	unsigned int Sekunden = _AVM_WATCHDOG_atoi(name);

	if (Sekunden == 0)
		Sekunden = 120; /*--- 2 Minuten ---*/

	if (ar7wdt_no_reboot <= 1) {
		if (!ar7wdt_hw_is_wdt_running()) {
			ar7wdt_hw_init();
			ar7wdt_hw_trigger();
		}
	}
	init_waitqueue_head(&(ar7wdt_data.appl[MAX_WDT_APPLS].wait_queue));
	ar7wdt_data.appl[MAX_WDT_APPLS].fasync = NULL;
	ar7wdt_data.appl[MAX_WDT_APPLS].default_time = Sekunden;
	ar7wdt_data.appl[MAX_WDT_APPLS].Timer.handle = MAX_WDT_APPLS + 1; /*--- handle ---*/
	timer_setup(&ar7wdt_data.appl[MAX_WDT_APPLS].Timer.timer,
			AVM_WATCHDOG_timer_init_ctrl_handler, 0),

	strcpy(ar7wdt_data.appl[MAX_WDT_APPLS].Name, "init-ctrl");

	pr_crit("AVM_WATCHDOG: System Init Ueberwachung %u Sekunden\n",
		Sekunden);
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, MAX_WDT_APPLS);
	return MAX_WDT_APPLS + 1;
}

/**
 */
int AVM_WATCHDOG_init_done(int handle, char *name, int len)
{
	pr_crit("AVM_WATCHDOG: System Init Ueberwachung abgeschlossen (%lu s noch verfuegbar)\n",
		(ar7wdt_data.appl[MAX_WDT_APPLS].Timer.timer.expires - jiffies) / HZ);
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
	boot_profiling_stop();
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, MAX_WDT_APPLS);
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
	unsigned long flags;
	int i;

	DBG("%s(%s): start\n", __func__, name);

	if (ar7wdt_no_reboot >= 2) {
		return -EINVAL; /*--- inval argument ---*/
	}
	name[len] = 0;
	name = strstrip(name);

	for (i = 1; i < MAX_WDT_APPLS; i++) {
		spin_lock_irqsave(&ar7_wdt_lock, flags);
		if (test_bit(i, ar7wdt_data.mask)) {
			if (strcmp(ar7wdt_data.appl[i].Name, name)) {
				spin_unlock_irqrestore(&ar7_wdt_lock, flags);
				continue;
			}
			ar7wdt_data.appl[i].crashflag = 0;
			ar7wdt_data.appl[i].pid = task_pid_nr(current);
			spin_unlock_irqrestore(&ar7_wdt_lock, flags);
			DBG("%s(%s): already registered (accept) handle=%u\n",
			    __func__, name, i + 1);
			return i + 1;
		}
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
	}
	DBG("%s(%s): not yet registered\n", __func__, name);

	for (i = 1; i < MAX_WDT_APPLS; i++) {
		spin_lock_irqsave(&ar7_wdt_lock, flags);
		if (test_bit(i, ar7wdt_data.mask)) {
			spin_unlock_irqrestore(&ar7_wdt_lock, flags);
			continue;
		}
		set_bit(i, ar7wdt_data.mask);
		clear_bit(i, ar7wdt_data.triggered);
		set_bit(i, ar7wdt_data.requested);
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);

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
	unsigned long flags;

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
	spin_lock_irqsave(&ar7_wdt_lock, flags);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	clear_bit(idx, ar7wdt_data.mask);
	clear_bit(idx, ar7wdt_data.requested);
	clear_bit(idx, ar7wdt_data.states);
	clear_bit(idx, ar7wdt_data.triggered);
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);

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
	unsigned long flags;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	spin_lock_irqsave(&ar7_wdt_lock, flags);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	ar7wdt_data.appl[idx].default_time = _AVM_WATCHDOG_atoi(time) / 2;
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);

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
	unsigned long flags;

	if (ar7wdt_no_reboot >= 2) {
		return -EINVAL; /*--- inval argument ---*/
	}
	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	spin_lock_irqsave(&ar7_wdt_lock, flags);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	set_bit(idx, ar7wdt_data.triggered);
	clear_bit(idx, ar7wdt_data.states);
	ar7wdt_data.appl[idx].crashlog_read_forbidden = 0;
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);

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
	unsigned long flags;
	int i;

	pr_crit("%s()\n", __func__);
	pr_info("registered appls:\n");
	for (i = 0; i < MAX_WDT_APPLS; i++) {
		spin_lock_irqsave(&ar7_wdt_lock, flags);
		if (test_bit(i, ar7wdt_data.mask)) {
			clear_bit(i, ar7wdt_data.mask);
			clear_bit(i, ar7wdt_data.requested);
			clear_bit(i, ar7wdt_data.states);
			clear_bit(i, ar7wdt_data.triggered);
			spin_unlock_irqrestore(&ar7_wdt_lock, flags);
			_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, i);
			pr_info("  hdl=%u, %s, disabled.\n", i + 1,
				ar7wdt_data.appl[i].Name);
		} else {
			spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		}
	}
#ifndef CONFIG_MACH_BCM963138
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, -1);
#endif
	ar7wdt_hw_deinit();
	ar7wdt_no_reboot = 3;
	return 1; /*--- sonst Endlosschleife, weil private_data == 0 ---*/
}

/**
 */
int AVM_WATCHDOG_read(int handle, char *Buffer, int max_len)
{
	unsigned long flags;
	int idx = handle - 1;

	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	spin_lock_irqsave(&ar7_wdt_lock, flags);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		DBG("%s(hdl=%u): invalid handle (not set in mask)\n", __func__,
		    handle);
		return -EINVAL; /*--- inval argument ---*/
	}
	if (test_bit(idx, ar7wdt_data.requested) == 0) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		Buffer[0] = '\0';
		DBG("%s(hdl=%u): no request\n", __func__, handle);
		return handle;
	}
	clear_bit(idx, ar7wdt_data.requested);
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);
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
 */
static int cmp_wdt_name(const char *name)
{
	int i;

	for (i = 0; i < MAX_WDT_APPLS; i++) {
		if (test_bit(i, ar7wdt_data.mask)) {
			if (strcmp(name, ar7wdt_data.appl[i].Name) == 0) {
				return i + 1;
			}
		}
	}
	return 0;
}

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
 * \retval handle (0 - nicht gefunden)
 */
static int cmp_wdt_task(struct task_struct *task)
{
	int i;
	struct task_struct *wd_task;

	for (i = 0; i < MAX_WDT_APPLS; i++) {
		if (test_bit(i, ar7wdt_data.mask)) {
			wd_task = get_taskstruct_by_pid(ar7wdt_data.appl[i].pid);
			if (wd_task) {
				int same_process = same_thread_group(wd_task, task);

				put_task_struct(wd_task);
				if (same_process) {
					DBG("%s: %u: %s(%d)[wd-name=%s] in (%s(%d)\n",
					    __func__, __LINE__, wd_task->comm,
					    task_pid_nr(wd_task),
					    ar7wdt_data.appl[i].Name,
					    task->comm, task_pid_nr(task));
					return i + 1;
				}
			}
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

	t = p;
	do {
		maj_flt += t->maj_flt;
	} while_each_thread(p, t);

	return maj_flt;
}

/**
 */
static struct task_struct *watchdog_task_list(int notrigger_idx)
{
	char txtbuf[3][64];
	struct task_struct *g, *hungtask = NULL, *rtnl_cur = NULL;
	unsigned long flags, page_faults;
	int handle, idx, delayed = 0;
	DECLARE_BITMAP(handle_mask, MAX_WDT_APPLS) = {0};
	unsigned int old_printk_status;
	unsigned long oom_score, totalpages = totalram_pages + total_swap_pages;


	watchdog_in_progress = 1;
	old_printk_status = printk_avm_console_bend(0); /* folgende Ausgaben seriell */
	console_verbose();

	oom_score = _oom_score(current, totalpages);
	pr_emerg("[%x]AVM_WATCHDOG_reboot(hdl=%u, %s): reboot (current: %s oom_score %lu)\n",
		 smp_processor_id(), notrigger_idx + 1,
		 ar7wdt_data.appl[notrigger_idx].Name, current->comm,
		 oom_score);
	page_faults = get_act_pagefaults();
	pr_emerg("pagefaults absolut %lu since last %s-trigger %lu\n",
		 page_faults, ar7wdt_data.appl[notrigger_idx].Name,
		 page_faults - ar7wdt_data.appl[notrigger_idx].pagefaults);

	read_lock_irqsave(&tasklist_lock, flags);
	for_each_process(g) {
		handle = cmp_wdt_name(g->comm);
		if (handle == 0) {
			/*--- fallback if thread renamed ---*/
			handle = cmp_wdt_task(g);
			if (handle == 0) {
				continue;
			}
		}
		idx = handle - 1;
		set_bit(idx, handle_mask);
		oom_score = _oom_score(g, totalpages);
		pr_emerg("  hdl=%2u %-13s pid %4d triggered before: %s(avg %s) worst %s timeout %4u s state %c cpu%x"
			 " pgfault %6lu oom_score %lu\n",
			 handle, ar7wdt_data.appl[idx].Name, task_pid_nr(g),
			 human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[idx].req_jiffies, 0),
			 human_timediff(txtbuf[1], ar7wdt_data.appl[idx].avg_trigger, 0),
			 human_timediff(txtbuf[2], ar7wdt_data.appl[idx].worst_trigger, 0),
			 ar7wdt_data.appl[idx].default_time * 2,
			 task_state_to_char(g), task_cpu(g),
			 accumulate_process_pagefaults(g),
			oom_score);
		if ((notrigger_idx == idx) && (delayed == 0)) {
			delayed = 1;
			pr_emerg(" force SIGBUS for %s (pid= %d)\n", g->comm,
				 task_pid_nr(g));
			hungtask = g;
		}
	}
	for (idx = 0; idx < MAX_WDT_APPLS; idx++) {
		if (test_bit(idx, ar7wdt_data.mask) == 0)
			continue;
		if (test_bit(idx, handle_mask))
			continue;
		pr_emerg("  hdl=%2u %-13s%s triggered before: %s(avg %s) worst %s  timeout %4u s %s\n",
			 idx + 1, ar7wdt_data.appl[idx].Name,
			 is_handle_from_remote_cpu(idx) ? " (remote)" : "",
			 human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[idx].req_jiffies, 0),
			 human_timediff(txtbuf[1], ar7wdt_data.appl[idx].avg_trigger, 0),
			 human_timediff(txtbuf[2], ar7wdt_data.appl[idx].worst_trigger, 0),
			 ar7wdt_data.appl[idx].default_time * 2,
			 is_handle_from_remote_cpu(idx) ? "" :
			 ar7wdt_data.appl[idx].crashflag ? "maybe crashed" : "");
	}
	if (hungtask) {
		pr_emerg("ar7wdt_hw_reboot: kernel context for %s (pid= %d):\n",
			 hungtask->comm, task_pid_nr(hungtask));
		sched_show_task(hungtask);
		pr_emerg("ar7wdt_hw_reboot: user context for %s:",
			 hungtask->comm);
		__sched_show_user_task(hungtask);
	}
	read_unlock_irqrestore(&tasklist_lock, flags);
	avm_oom_show_memstat(rtnl_cur ?
				AVM_OOM_MEMSTAT_ONCE :
				AVM_OOM_MEMSTAT_ONCE | AVM_OOM_MEMSTAT_ALL);
	printk_avm_console_bend(old_printk_status);
	ar7wdt_hw_trigger();
	return hungtask;
}

/**
 * 2 stufig:
 * hungtask != 0: hartes wakeup des haengenden Tasks (auch wenn TASK_UNINTERRUPTIBLE)
 * hungtask == 0: panic
 */
static void panic_function(struct timer_list *timer)
{
	struct panic_timer *context = from_timer(context, timer, timer);
	struct task_struct *hungtask = context->hungtask;

	ar7wdt_hw_trigger();
	avm_debug_disable_avm_printk();
	ar7wdt_hw_trigger();
	if (hungtask) {
		struct task_struct *thread;

		siginfo_t info;

		info.si_signo = SIGBUS;
		info.si_errno = ETIME;
		info.si_code = BUS_OBJERR;
		info.si_addr = (void *)0xEBADEBAD;

		thread = hungtask;
		do {
			force_sig_info(SIGBUS, &info, thread);
		} while_each_thread(hungtask, thread);

		if (hungtask->state == TASK_UNINTERRUPTIBLE) {
			pr_emerg(
				"ar7wdt_hw_reboot: wake up task %s (pid= %d):\n",
				hungtask->comm, task_pid_nr(hungtask));
			wake_up_process(hungtask);
		}
		/*--- und nun noch panic verzoegert triggern ---*/
		panic_timer.hungtask = NULL;
		panic_timer.timer.function = (timer_func_t)panic_function;
		panic_timer.timer.expires  = jiffies + HZ * 5;
		add_timer(&panic_timer.timer);
		return;
	}
#if defined(CONFIG_ARCH_AVALANCHE)
	ar7wdt_hw_reboot();
#else
	panic("%s\n", panic_timer.panic_string);
#endif
}

/**
 */
int AVM_WATCHDOG_reboot(int handle)
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
	if (test_bit(idx, ar7wdt_data.states)) {
		struct task_struct *hungtask;

		ar7wdt_hw_trigger();
		hungtask = watchdog_task_list(idx);
#ifdef CONFIG_SCHEDSTATS
		if (ar7wdt_no_reboot == 1) {
			show_state();
		}
#endif
		atomic_notifier_call_chain(&ar7wdt_reboot_nh, 0, hungtask);
		if (ar7wdt_no_reboot == 0) {
			avm_set_reset_status(RS_SOFTWATCHDOG);
			avm_stack_check(NULL);
			AVM_WATCHDOG_deinit();
			panic_delayed(hungtask,
				      hungtask ? (HZ * 1) : (HZ * 5),
				      STRING_WATCHDOG_EXPIRED);
			return 0;
		}
	}
	pr_err("%s(hdl=%u): timer not triggered\n", __func__, handle);
	return 0;
}

/**
 */
static void AVM_WATCHDOG_timer_handler_action(unsigned int idx)
{
	unsigned long flags;

	spin_lock_irqsave(&ar7_wdt_lock, flags);
	set_bit(idx, ar7wdt_data.states);
	set_bit(idx, ar7wdt_data.requested);
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);

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
	unsigned long flags;
	int idx, handle = (int)_handle;

	DBG("%s(hdl=%d)\n", __func__, handle);

	if (handle == -1) {
		_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, handle);
		ar7wdt_hw_trigger();
		return;
	}
	if (handle < 1 || handle > MAX_WDT_APPLS) {
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return;
	}
	idx = handle - 1;

	spin_lock_irqsave(&ar7_wdt_lock, flags);
	if (test_bit(idx, ar7wdt_data.mask) == 0) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		DBG("%s(hdl=%u): invalid handle\n", __func__, handle);
		return;
	}
	if (test_bit(idx, ar7wdt_data.states)) {
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
		DBG("%s(hdl=%u): timer triggered twice (reboot)\n", __func__,
		    handle);
		AVM_WATCHDOG_reboot(handle);
		return;
	}
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);
	_AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, idx);
	AVM_WATCHDOG_timer_handler_action(idx);
}

/**
 */
void AVM_WATCHDOG_emergency_retrigger(void)
{
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
	unsigned long flags;
	unsigned int idx;
	unsigned int handle;

	DBG("%s: %u: %s pid=%d\n", __func__, __LINE__, current->comm,
	    task_pid_nr(current));
	handle = cmp_wdt_name(current->comm);

	if (handle == 0) {
		/*--- fallback if thread renamed ---*/
		handle = cmp_wdt_task(current);
	}
	if (handle == 0) {
		return 1;
	}
	idx = handle - 1;

	spin_lock_irqsave(&ar7_wdt_lock, flags);
	if (test_bit(idx, ar7wdt_data.mask)) {
		ar7wdt_data.appl[idx].crashlog_read_forbidden = 1;
	}
	spin_unlock_irqrestore(&ar7_wdt_lock, flags);
	/*--- notify that crashlog was written (reset on next watchdog trigger) ---*/
	return 0;
}

int AVM_WATCHDOG_register_reboot_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&ar7wdt_reboot_nh, nb);
}
EXPORT_SYMBOL(AVM_WATCHDOG_register_reboot_notifier);

int AVM_WATCHDOG_unregister_reboot_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&ar7wdt_reboot_nh, nb);
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
	unsigned long flags;
	int ret = 0;
	unsigned int i;

	for (i = 0; i < MAX_WDT_APPLS; i++) {
		spin_lock_irqsave(&ar7_wdt_lock, flags);
		if (test_bit(i, ar7wdt_data.mask) &&
		    ar7wdt_data.appl[i].crashlog_read_forbidden) {
			spin_unlock_irqrestore(&ar7_wdt_lock, flags);
			DBG("%s: ret=%d handle=%d name=%s\n", __func__, ret,
			    i + 1, ar7wdt_data.appl[i].Name);
			ret = 1;
			break;
		}
		spin_unlock_irqrestore(&ar7_wdt_lock, flags);
	}
	return ret;
}
/**
 */
static void lproc_wdt(struct seq_file *seq, void *priv)
{
	int idx;
	char txtbuf[3][64];
	struct task_struct *g;
	unsigned long flags;
	unsigned int handle;
	DECLARE_BITMAP(handle_mask, MAX_WDT_APPLS) = {0};
	unsigned long oom_score, totalpages = totalram_pages + total_swap_pages;

	read_lock_irqsave(&tasklist_lock, flags);
	for_each_process(g) {
		handle = cmp_wdt_name(g->comm);

		if (handle == 0) {
			/*--- fallback if thread renamed ---*/
			handle = cmp_wdt_task(g);
			if (handle == 0) {
				continue;
			}
		}
		idx = handle - 1;
		set_bit(idx, handle_mask);
		oom_score = _oom_score(current, totalpages);
		seq_printf(seq,
			 "hdl=%2u %-13s pid %4d triggered before: %s(avg %s) worst %s timeout %4u s state %c cpu%x"
			 " pgfault %6lu oom_score %lu\n",
			 handle, ar7wdt_data.appl[idx].Name, task_pid_nr(g),
			 human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[idx].req_jiffies, 0),
			 human_timediff(txtbuf[1], ar7wdt_data.appl[idx].avg_trigger, 0),
			 human_timediff(txtbuf[2], ar7wdt_data.appl[idx].worst_trigger, 0),
			 ar7wdt_data.appl[idx].default_time * 2,
			 task_state_to_char(g), task_cpu(g),
			 accumulate_process_pagefaults(g),
			oom_score);
	}
	read_unlock_irqrestore(&tasklist_lock, flags);

	for (idx = 0; idx < MAX_WDT_APPLS; idx++) {
		if (test_bit(idx, ar7wdt_data.mask) == 0)
			continue;
		if (test_bit(idx, handle_mask))
			continue;
		seq_printf(seq, "hdl=%2u %-13s%s triggered before: %s(avg %s) worst %s timeout %4u s %s\n",
					idx + 1, ar7wdt_data.appl[idx].Name,
			   is_handle_from_remote_cpu(idx) ?  " (remote)" : "",
			   human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[idx].req_jiffies, 0),
			   human_timediff(txtbuf[1], ar7wdt_data.appl[idx].avg_trigger, 0),
			   human_timediff(txtbuf[2], ar7wdt_data.appl[idx].worst_trigger, 0),
			   ar7wdt_data.appl[idx].default_time * 2,
			   is_handle_from_remote_cpu(idx) ?  "" :
			   ar7wdt_data.appl[idx].crashflag ?  "maybe crashed" : ""
			  );
	}
}
