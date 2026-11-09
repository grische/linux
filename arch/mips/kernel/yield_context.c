/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
 *
 *   author: mbahr@avm.de
 *   description: yield-thread-interface mips34k
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>
#include <linux/smp.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/irqchip/mips-gic.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <asm/processor.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/mips_mt.h>
#include <asm/dsp.h>
#include <asm/yield_context.h>
#include <asm/mach_avm.h>
#include <asm/ltq_vmb.h>
#include <avm/sammel/debug.h>
#include <avm/sammel/helpers.h>

#if defined(YIELD_MAX_TC) && (YIELD_MAX_TC > 0)

#define MAX_YIELDSIGNALS 16
#define YIELDMASK ((1 << MAX_YIELDSIGNALS) - 1)

#define write_vpe_c0_yqmask(val) mttc0(1, 4, val)
#define read_vpe_c0_yqmask(val) mftc0(1, 4)

#define YIELD_STAT

#define MAGIC_YIELD_STACK 0x595F5350

#define LINUXOS_TCSCHED_PRIORITY 0

static void yield_to_linux_stat(struct seq_file *m);

#if defined(YIELD_STAT)
struct _generic_stat {
	unsigned long cnt;
	unsigned long min;
	unsigned long max;
	unsigned long long avg;
	unsigned long last_trigger_cycle;
	spinlock_t lock;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void init_generic_stat(struct _generic_stat *pgstat, int lock_init)
{
	pgstat->min = LONG_MAX;
	pgstat->max = 0;
	pgstat->cnt = 0;
	pgstat->avg = 0;
	if (lock_init)
		spin_lock_init(&pgstat->lock);
}
static void generic_stat(struct _generic_stat *pgstat, unsigned long val)
	__attribute__((hot));
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void generic_stat(struct _generic_stat *pgstat, unsigned long val)
{
	arch_spin_lock(&(pgstat->lock.rlock.raw_lock));
	if (val > pgstat->max)
		pgstat->max = val;
	if (val < pgstat->min)
		pgstat->min = val;
	pgstat->avg += (unsigned long long)val;
	pgstat->cnt++;
	arch_spin_unlock(&(pgstat->lock.rlock.raw_lock));
}
#endif /* YIELD_STAT */
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_handler {
	/*--------------------------------------------------------------------------------*\
     * Funktion die im Yield-Kontext ausgefuehrt wird
     * Achtung! kein Linux-Kontext, nur rudimentaere Zugriffe erlaubt!
    \*--------------------------------------------------------------------------------*/
	int (*yield_handler)(int signal, void *ref);
	void *ref;
	spinlock_t progress;
	volatile unsigned long counter;
	volatile unsigned long unhandled;
	atomic_t enable;
#if defined(YIELD_STAT)
	unsigned int last_access;
	struct _generic_stat consumption;
	struct _generic_stat trigger;
#endif
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_per_tc {
	volatile int yield_tc_signal_mask;
	int yield_tc;
	volatile int tc_init;
	volatile unsigned long yield_counter;
	struct _yield_handler handler[MAX_YIELDSIGNALS];
	volatile struct _yield_handler *act_yh;
	struct task_struct tsk;
	union thread_union yield_gp __attribute__((aligned(THREAD_SIZE)));
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_ctrl {
	int yield_all_init;
	volatile int yield_vpe_signal_mask;
	struct _yield_per_tc *per_tc[YIELD_LAST_TC - YIELD_FIRST_TC + 1];
};

static struct _yield_ctrl yield_ctrl[NR_CPUS];

/*--------------------------------------------------------------------------------*\
 MIPS MT 34K-Specification:
 When the rs argument of the yield rs instruction is positive, the thread waits for a hardware condition; the thread
 will wait until the bitwise-and of rs and the hardware signal vector is non-zero. This is a cheap and efficient mecha-
 nism to get a thread to wait on the state of some input signal.

 Cores in the 34K family may have up to 16 external hardware signals attached. Because the yield instruction is
 available to user (low-privilege) software, you might not want it to have sight of all your hardware signals. The CP0
 register YQMask is a bit-field where a “1” bit marks an incoming signal as accessible to the yield instruction.
 In any OS running more threads than TCs you might want to reclaim a TC blocked on such a yield. If you need to
 do that while continuing to monitor the condition, then you’ll probably want your system integrator to ensure that the
 yield condition is also available as an interrupt, so you can get the OS’ attention when the condition happens.
 The OS can zero-out corresponding bits 0-15 of YQMask to prevent them being used - a yield rs which attempts
 to use one of those bits will result in an exception.

 In the two-operand form yield rd,rs the rd register gets a result, which is a bit-map with a 1 for every active
 yield input which is enabled by YQMask (bits which are zeroed in YQMask may return any value, don’t rely on them).
 The single-register form yield rs is really yield $0,rs.
\*--------------------------------------------------------------------------------*/
static inline unsigned int yield_events(unsigned int mask)
{
	int res = 0;

	__asm__ __volatile__(
		"	.set	push                        \n"
		"	.set	noreorder                   \n"
		"	.set	noat                        \n"
		"	.set	mips32r2					\n"
		"	.set	mt                          \n"
		"	nop                                 \n"
		"	yield   %0, %1                      \n"
		"	.set	pop                         \n"
		: "=r"(res)
		: "0"(mask));
	return res;
}
/*--------------------------------------------------------------------------------*\
  MIPS MT 34K-Specification:
 There are very few extra instructions:

 fork rd,rs,rt: fires up a thread on a free TC (if available, see below). rs points to the instruction where the
    new thread is to start, and the new thread’s rd register gets the value from the existing thread’s rt.
    Some vital per-TC state is copied from the parent:

     TCStatus[TKSU]: whether you’re in kernel or user mode — the same as Status[KSU]);
     TCStatus[TASID]: what address space you’re part of — the same as EntryHi[ASID] ;
     UserLocal: some kind of kernel-maintained thread ID, see more in Section C.4.2 “The UserLocal register”.

    When the thread has finished its job it should use yield $0 to free up the TC again.
\*--------------------------------------------------------------------------------*/
static inline unsigned int fork(void *startaddr, void *arg)
{
	int res = 0;

	__asm__ __volatile__(
		"	.set	push						\n"
		"	.set	noreorder					\n"
		"	.set	noat						\n"
		"	.set	mips32r2					\n"
		"	nop                                 \n"
		"	fork   %0, %1, %2                   \n"
		"	.set	pop						    \n"
		: "=r"(res)
		: "0"(startaddr), "r"(arg));
	return res;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _yield_per_tc *get_per_tc_struct(struct _yield_ctrl *pyield_ctrl,
					       int tc)
{
	unsigned int i;

	for (i = 0; i < YIELD_MAX_TC; i++) {
		if (pyield_ctrl->per_tc[i] == NULL) {
			continue;
		}
		if (pyield_ctrl->per_tc[i]->tc_init == tc) {
			return (struct _yield_per_tc *)pyield_ctrl->per_tc[i];
		}
	}
	return NULL;
}
/**--------------------------------------------------------------------------------**\
 * \brief: tell me if we in any yield instead linux context
 * ret: 0 no yield-context
 *      <any> - 1: thread-number
\**--------------------------------------------------------------------------------**/
int is_yield_context(void)
{
	struct _yield_ctrl *pyield_ctrl =
		&yield_ctrl[current_thread_info()->cpu];
	unsigned int act_tc;

	act_tc = (read_c0_tcbind() & TCBIND_CURTC) >> TCBIND_CURTC_SHIFT;
	if (get_per_tc_struct(pyield_ctrl, act_tc)) {
		return act_tc + 1;
	}
	return 0;
}
EXPORT_SYMBOL(is_yield_context);
/**--------------------------------------------------------------------------------**\
 * \brief: start function in non-linux-yield-context
 *
 * ret: >= 0 number of registered signal < 0: errno
 *
 *  return of request_yield_handler() handled -> YIELD_HANDLED
\**--------------------------------------------------------------------------------**/
int request_yield_handler_on(int cpu, int tc, int signal,
			     int (*yield_handler)(int signal, void *ref),
			     void *ref)
{
	struct _yield_ctrl *pyield_ctrl = &yield_ctrl[cpu];
	struct _yield_per_tc *pyield_per_tc;
	unsigned long flags;

	/*--- printk("%s: signal=%x func=%p ref=%p\n", __func__, signal, yield_handler, ref); ---*/
	if (cpu > NR_CPUS) {
		return -ERANGE;
	}
	if (pyield_ctrl->yield_all_init == 0) {
		pr_err("%s not initialized\n", __func__);
		return -ENODEV;
	}
	if (((unsigned int)signal >= MAX_YIELDSIGNALS)) {
		pr_err("%s signal %d to large\n", __func__, signal);
		return -ERANGE;
	}
	if (is_vmalloc_or_module_addr(yield_handler)) {
		pr_err("%s no virtual address for yield_handler (%p) allowed\n",
		       __func__, yield_handler);
		return -ERANGE;
	}
	if (is_vmalloc_or_module_addr(ref)) {
		pr_err("%s  no virtual address for ref (%p) allowed\n",
		       __func__, ref);
		return -ERANGE;
	}
	if ((pyield_per_tc = get_per_tc_struct(pyield_ctrl, tc)) == NULL) {
		pr_err("%s invalid yield-tc %d\n", __func__, tc);
		return -ERANGE;
	}
	yield_spin_lock_irqsave(&pyield_per_tc->handler[signal].progress,
				flags);
	if (pyield_per_tc->handler[signal].yield_handler) {
		yield_spin_unlock_irqrestore(
			&pyield_per_tc->handler[signal].progress, flags);
		pr_err("%s signalhandler for signal %d already installed\n",
		       __func__, signal);
		return -EBUSY;
	}

	pyield_per_tc->handler[signal].yield_handler = yield_handler;
	pyield_per_tc->handler[signal].ref = ref;
	pyield_per_tc->handler[signal].counter = 0;
	pyield_per_tc->handler[signal].unhandled = 0;
	atomic_set(&pyield_per_tc->handler[signal].enable, 1);

#if defined(YIELD_STAT)
	pyield_per_tc->handler[signal].last_access = 0;
	init_generic_stat(&pyield_per_tc->handler[signal].consumption, 1);
	init_generic_stat(&pyield_per_tc->handler[signal].trigger, 1);
#endif
	yield_spin_unlock_irqrestore(&pyield_per_tc->handler[signal].progress,
				     flags);
	return signal;
}
EXPORT_SYMBOL(request_yield_handler_on);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int free_yield_handler_on(int cpu, int tc, int signal, void *ref)
{
	struct _yield_per_tc *pyield_per_tc;
	struct _yield_ctrl *pyield_ctrl = &yield_ctrl[cpu];
	unsigned long flags;

	if (cpu > NR_CPUS) {
		return -ERANGE;
	}
	if (pyield_ctrl->yield_all_init == 0) {
		pr_err("%s not initialized\n", __func__);
		return -ENODEV;
	}
	if (((unsigned int)signal >= MAX_YIELDSIGNALS)) {
		pr_err("%s signal %d to large\n", __func__, signal);
		return -ERANGE;
	}
	if ((pyield_per_tc = get_per_tc_struct(pyield_ctrl, tc)) == NULL) {
		pr_err("%s invalid yield-tc %d\n", __func__, tc);
		return -ERANGE;
	}
	yield_spin_lock_irqsave(&pyield_per_tc->handler[signal].progress,
				flags);
	if (pyield_per_tc->handler[signal].ref == ref) {
		pyield_per_tc->handler[signal].yield_handler = NULL;
		pyield_per_tc->handler[signal].ref = NULL;
		yield_spin_unlock_irqrestore(
			&pyield_per_tc->handler[signal].progress, flags);
		return 0;
	}
	yield_spin_unlock_irqrestore(&pyield_per_tc->handler[signal].progress,
				     flags);
	pr_err("%s false ref\n", __func__);
	return -ERANGE;
}
EXPORT_SYMBOL(free_yield_handler_on);
/*--------------------------------------------------------------------------------*\
 * synchron, nur aus linux-Konext
\*--------------------------------------------------------------------------------*/
void disable_yield_handler_on(int cpu, int tc, int signal)
{
	unsigned long flags;
	struct _yield_per_tc *pyield_per_tc;
	struct _yield_ctrl *pyield_ctrl = &yield_ctrl[cpu];

	if (pyield_ctrl->yield_all_init == 0) {
		return;
	}
	if (is_yield_context()) {
		pr_err("%s error: only from linux-context\n",
		       __func__);
		return;
	}
	if (cpu > NR_CPUS) {
		pr_err("%s invalid cpu %d\n", __func__, cpu);
		return;
	}
	if (((unsigned int)signal >= MAX_YIELDSIGNALS)) {
		pr_err("%s invalid signal %d\n", __func__, signal);
		return;
	}
	if ((pyield_per_tc = get_per_tc_struct(pyield_ctrl, tc)) == NULL) {
		pr_err("%s invalid yield-tc %d\n", __func__, tc);
		return;
	}
	/*--- spinlock gewaehrleistet synchrones disable ---*/
	spin_lock_irqsave(&pyield_per_tc->handler[signal].progress, flags);

	if (atomic_sub_return(1, &pyield_per_tc->handler[signal].enable) < 0) {
		pr_err("%s warning unbalanced disable\n", __func__);
		dump_stack();
	}
	spin_unlock_irqrestore(&pyield_per_tc->handler[signal].progress, flags);
}
EXPORT_SYMBOL(disable_yield_handler_on);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void enable_yield_handler_on(int cpu, int tc, int signal)
{
	struct _yield_per_tc *pyield_per_tc;
	struct _yield_ctrl *pyield_ctrl = &yield_ctrl[cpu];

	if (pyield_ctrl->yield_all_init == 0) {
		return;
	}
	if (cpu > NR_CPUS) {
		pr_err("%s invalid cpu %d\n", __func__, cpu);
		return;
	}
	if (((unsigned int)signal >= MAX_YIELDSIGNALS)) {
		pr_err("%s invalid signal %d\n", __func__, signal);
		return;
	}
	if ((pyield_per_tc = get_per_tc_struct(pyield_ctrl, tc)) == NULL) {
		pr_err("%s invalid yield-tc %d\n", __func__, tc);
		return;
	}
	atomic_add(1, &pyield_per_tc->handler[signal].enable);
}
EXPORT_SYMBOL(enable_yield_handler_on);
extern void prom_printf(const char *, ...);
static void yield_context_thread(void) __attribute__((hot));
/*--------------------------------------------------------------------------------*\
 * own non-Linux-YIELD-Kontext-Thread!
 * use arch_spin_lock() because no error-output and error-handling allowed
\*--------------------------------------------------------------------------------*/
static void yield_context_thread(void)
{
	struct _yield_handler *pyieldh;
	struct _yield_ctrl *pyield_ctrl =
		&yield_ctrl[current_thread_info()->cpu];
	struct _yield_per_tc *pyield_tc = NULL;
	unsigned int settings, mask, i;
	unsigned int first_signal = 0;
	unsigned int yield_predefmask;
#if defined(YIELD_STAT)
	unsigned int start_time;
#endif
	init_dsp();
	for (i = 0; i < YIELD_MAX_TC; i++) {
		if (pyield_ctrl->per_tc[i] &&
		    (pyield_ctrl->per_tc[i]->tc_init == 0)) {
			pyield_ctrl->per_tc[i]->tc_init =
				(read_c0_tcbind() & TCBIND_CURTC) >>
				TCBIND_CURTC_SHIFT;
			pyield_tc =
				(struct _yield_per_tc *)pyield_ctrl->per_tc[i];
			break;
		}
	}
	if (pyield_tc == NULL) {
		/*--- big error ---*/
		yield_events(0);
		pr_err("BIG ERROR");
		return;
	}
	yield_predefmask = pyield_tc->yield_tc_signal_mask;
	mask = yield_predefmask;
	/*--- printk(KERN_ERR"[%s tcstatus=%lx tcbind=%lx yqmask=%lx(%x)]\n", __func__, read_tc_c0_tcstatus(), read_tc_c0_tcbind(), read_vpe_c0_yqmask(), mask);  ---*/
	first_signal = ffs(yield_predefmask) - 1;
	for (;;) {
		unsigned int signal = first_signal;
		settings = (yield_events(mask) & yield_predefmask) >> signal;
		while (settings) {
			if (likely((settings & 0x1) == 0)) {
				signal++;
				settings >>= 1;
				continue;
			}
			pyieldh = &pyield_tc->handler[signal];
			pyieldh->counter++;
			arch_spin_lock(&pyieldh->progress.rlock.raw_lock);
			if (unlikely(pyieldh->yield_handler == NULL)) {
				arch_spin_unlock(
					&pyieldh->progress.rlock.raw_lock);
				signal++;
				settings >>= 1;
				continue;
			}
			if (atomic_read(&pyieldh->enable) <= 0) {
				arch_spin_unlock(
					&pyieldh->progress.rlock.raw_lock);
				signal++;
				settings >>= 1;
				continue;
			}
#if defined(YIELD_STAT)
			start_time = avm_get_cycles() | 1;
			if (pyieldh->last_access) {
				generic_stat(&pyieldh->trigger,
					     start_time - pyieldh->last_access);
			}
			pyieldh->last_access = start_time;
#endif
			pyield_tc->act_yh = pyieldh;
			if (pyieldh->yield_handler(signal, pyieldh->ref) !=
			    YIELD_HANDLED) {
				pyieldh->unhandled++;
			}
			pyield_tc->act_yh = NULL;
#if defined(YIELD_STAT)
			generic_stat(&pyieldh->consumption,
				     avm_get_cycles() - start_time);
#endif
			arch_spin_unlock(&pyieldh->progress.rlock.raw_lock);
			signal++;
			settings >>= 1;
		}
		pyield_tc->yield_counter++;
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void _yield_proc_stat(struct seq_file *m, void *data __maybe_unused)
{
	unsigned int i, tc, stack_used, cpu;
	unsigned long flags, *p;
	unsigned int stacksize = (THREAD_SIZE - 32 - sizeof(struct pt_regs));

	seq_printf(m, "Cycle-Freq: %lu MHz\n",
		   avm_get_cyclefreq() / (1000 * 1000));
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		struct _yield_ctrl *pyield_ctrl = &yield_ctrl[cpu];
		for (tc = 0; tc < YIELD_MAX_TC; tc++) {
			struct _yield_per_tc *pyield_tc =
				(struct _yield_per_tc *)pyield_ctrl->per_tc[tc];
			if (pyield_tc == NULL) {
				continue;
			}
			stack_used = stacksize;
			p = &pyield_tc->yield_gp
				     .stack[sizeof(struct thread_info) /
					    sizeof(unsigned long)];
			while (stack_used) {
				if (*p++ != MAGIC_YIELD_STACK) {
					break;
				}
				stack_used -= sizeof(unsigned long);
			}
			seq_printf(
				m,
				"[cpu=%d tc=%d]yield: mask=0x%x trigger=%lu stack-used=%u(stack-start=%p gp=%p) from %u bytes%s\n",
				pyield_tc->yield_gp.thread_info.cpu,
				pyield_tc->yield_tc,
				pyield_tc->yield_tc_signal_mask,
				pyield_tc->yield_counter, stack_used,
				(unsigned char *)pyield_tc->yield_gp.stack +
					stacksize,
				(unsigned char *)&pyield_tc->yield_gp,
				stacksize,
				stack_used >= stacksize ? "stack overflow!!!" :
							  "");

			for (i = 0; i < MAX_YIELDSIGNALS; i++) {
				struct _yield_handler *pyieldh =
					&pyield_tc->handler[i];
#if defined(YIELD_STAT)
				struct _generic_stat *pstat;
				unsigned long cnt, max, min;
				unsigned long long avg64;
#endif

				if (pyieldh->yield_handler &&
					(pyield_tc->yield_tc_signal_mask & (0x1 << i))) {
					unsigned char busy = (pyield_tc->act_yh) ? '*' : ' ';

					seq_printf(m, "\t[%2d]handler: %pS %c enable=%d "
							"count=%lu unhandled=%lu\n", i,
								   pyieldh->yield_handler,
								   busy,
								   atomic_read(&pyieldh->enable),
								   pyieldh->counter,
								   pyieldh->unhandled);
#if defined(YIELD_STAT)
					pstat = &pyieldh->consumption;
					yield_spin_lock_irqsave(&pstat->lock,
								flags);
					cnt = pstat->cnt;
					max = pstat->max;
					min = pstat->min;
					avg64 = pstat->avg;
					init_generic_stat(pstat, 0);
					yield_spin_unlock_irqrestore(
						&pstat->lock, flags);

					if (cnt) {
						do_div(avg64, cnt);
						seq_printf(
							m,
							"\t\tcycle-stat: "
							"[%10lu]consumption: min=%10lu "
							"max=%10lu avg=%10lu\n",
							cnt, min, max,
							(unsigned long)avg64);
					}
					pstat = &pyieldh->trigger;
					yield_spin_lock_irqsave(&pstat->lock,
								flags);
					cnt = pstat->cnt;
					max = pstat->max;
					min = pstat->min;
					avg64 = pstat->avg;
					init_generic_stat(pstat, 0);
					yield_spin_unlock_irqrestore(
						&pstat->lock, flags);
					if (cnt) {
						do_div(avg64, cnt);
						seq_printf(
							m,
							"\t\tcycle-stat: "
							"[%10lu]trigger    : min=%10lu "
							"max=%10lu avg=%10lu\n",
							cnt, min, max,
							(unsigned long)avg64);
					}
#endif /* YIELD_STAT */
				}
			}
		}
	}
}

#ifdef CONFIG_PROC_FS
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int supported_tcs(void)
{
	unsigned int mvpconf0;
	mvpconf0 = read_c0_mvpconf0();
	return ((mvpconf0 & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int get_first_cpuid_by_core(int core)
{
	int cpu_id, tc, ntc = supported_tcs();

	for (tc = 0; tc < ntc; tc++) {
		if ((cpu_id = get_cpuid_by_mt(core, tc, NULL)) >= 0) {
			return cpu_id;
		}
	}
	return -1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _param_tcsched {
	unsigned int read_only;
	unsigned int tc_mask;
	unsigned int group;
	unsigned int rotsched;
	unsigned int core;
	struct seq_file *m;
};
#define GET_BITS(val, upper, lower)                                            \
	(((val) >> (lower)) & ((1 << ((upper) - (lower) + 1)) - 1))

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void setup_tcsched(struct _param_tcsched *param);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int yield_proc_stat(struct seq_file *m, void *data __maybe_unused)
{
	struct _param_tcsched param;
	unsigned int core;
	int cpu_id;
	_yield_proc_stat(m, data);
	yield_to_linux_stat(m);
	param.read_only = 1;
	param.tc_mask = (1 << supported_tcs()) - 1;
	param.group = 0x0;
	param.m = m;
	preempt_disable();
	for (core = 0; core < YIELD_MONITOR_MAX_CORES; core++) {
		if ((cpu_id = get_first_cpuid_by_core(core)) >= 0) {
			param.core = core;
			smp_call_function_single(cpu_id,
						 (smp_call_func_t)setup_tcsched,
						 &param, true);
		}
	}
	preempt_enable();
	return 0;
}

static int yield_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, yield_proc_stat, NULL);
}

#define vpflags flags_table[0]
#define sys_flag flags_table[1]
#define old_tc flags_table[2]
#define haltval flags_table[3]

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int set_tcmode(unsigned int tc, unsigned long flags_table[])
{
	local_irq_save(sys_flag);
	vpflags = dvpe();
	old_tc = read_c0_vpecontrol() & VPECONTROL_TARGTC;
	settc(tc);
	if (!(read_tc_c0_tcstatus() & TCSTATUS_A)) {
		settc(old_tc);
		evpe(vpflags);
		local_irq_restore(sys_flag);
		return 1;
	}
	if (read_tc_c0_tcbind() == (unsigned)read_c0_tcbind()) {
		/* Are we dumping ourself?  */
		haltval = 0; /* Then we're not halted, and mustn't be */
	} else {
		haltval = read_tc_c0_tchalt();
		write_tc_c0_tchalt(1);
		mips_ihb();
	}
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void restore_tcmode(unsigned long flags_table[])
{
	if (!haltval) {
		write_tc_c0_tchalt(0);
	}
	settc(old_tc);
	evpe(vpflags);
	local_irq_restore(sys_flag);
}
#undef vpflags
#undef sys_flag
#undef old_tc
#undef haltval
#define GET_BITS(val, upper, lower)                                            \
	(((val) >> (lower)) & ((1 << ((upper) - (lower) + 1)) - 1))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void setup_tcsched(struct _param_tcsched *param)
{
	unsigned long flags[4];
	unsigned int val;
	unsigned int tc, ntc = supported_tcs();

	if (param->read_only) {
		seq_printf(param->m, "core=%u VPESchedule = %lx\n", param->core, read_vpe_c0_vpeschedule());
	} else if (param->rotsched != (unsigned int) -1) {
		write_vpe_c0_vpeschedule(param->rotsched ? 0 : (1 << 5)); /*--- set(0)/unset(1) Weighted Round-Robin Policy-Manager ---*/
	}
	/*--- pr_info("core=%u VPESchedule = %lx tcs=%u tc_mask=%x\n", param->core, read_vpe_c0_vpeschedule(), ntc, param->tc_mask); ---*/
	for (tc = 0; tc < ntc; tc++) {
		if (param->tc_mask & (1 << tc)) {
			if (set_tcmode(tc, flags)) {
				continue;
			}
			val = read_tc_c0_tcschedule();
			if (!param->read_only) {
				val &= ~YIELD_TC_SCHED_GROUP_MASK;
				val |= param->group & YIELD_TC_SCHED_GROUP_MASK;
				write_tc_c0_tcschedule(val);
				restore_tcmode(flags);
				seq_printf(
					param->m,
					"set core=%u TC%u.TCschedule = %08x\n",
					param->core, tc, val);
			} else {
				restore_tcmode(flags);
				seq_printf(
					param->m,
					"core=%u TC%u.TCschedule = %08x TC_RATE=%2u STP_PRIO=%u GROUP=%u\n",
					param->core, tc, val,
					GET_BITS(val, 19, 16),
					GET_BITS(val, 13, 12),
					GET_BITS(val, 1, 0));
			}
		}
	}
}
#define SKIP_SPACE(a)                                                          \
	while (*(a) && ((*(a) == ' ') || (*(a) == '\t')))                      \
	(a)++
#define SKIP_NON_SPACE(a)                                                      \
	while (*(a) && ((*(a) != ' ') && (*(a) != '\t')))                      \
	(a)++
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static ssize_t proc_tc_prio_write(struct file *file, const char __user *buffer,
				  size_t count, loff_t *pos)
{
	struct _param_tcsched tcsched;
	unsigned int copy_count;
	char *p;
	unsigned int tc, group, handled = 0;
	int core, cpu_id;
	char buf[128];
	char txt[756];

	struct seq_file s;

	memset(&s, 0, sizeof(s));
	txt[0] = 0;
	s.buf = txt;
	s.size = sizeof(txt);

	copy_count = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, copy_count)) {
		return (unsigned int)-EFAULT;
	}
	buf[copy_count] = 0;
	memset(&tcsched, 0, sizeof(tcsched));
	tcsched.m = &s;
	if ((p = strstr(buf, "rotsched"))) {
		p += sizeof("rotsched") - 1;
		SKIP_SPACE(p);
		sscanf(p, "%u %u", &core, &tcsched.rotsched);
		if ((cpu_id = get_first_cpuid_by_core(core)) >= 0) {
			/*--- printk(KERN_ERR"Change rotsched core=%u(cpu_id=%d) group=%u\n", core, cpu_id, tcsched.rotsched);  ---*/
			tcsched.read_only = 0;
			tcsched.tc_mask = 0;
			tcsched.group = 0;
			tcsched.core = core;
			preempt_disable();
			smp_call_function_single(cpu_id,
						 (smp_call_func_t)setup_tcsched,
						 &tcsched, true);
			preempt_enable();
			handled++;
		}
		handled++;
	} else {
		tcsched.rotsched = (unsigned int)-1;
	}
	if ((p = strstr(buf, "tcgroup"))) {
		p += sizeof("tcgroup") - 1;
		SKIP_SPACE(p);
		sscanf(p, "%d %u %u", &core, &tc, &group);
		if ((cpu_id = get_first_cpuid_by_core(core)) >= 0) {
			/*--- printk(KERN_ERR"Change group core=%u(cpu_id=%d) tc=%u group=%u\n", core, cpu_id, tc, group);  ---*/
			tcsched.read_only = 0;
			tcsched.tc_mask = (1 << tc);
			tcsched.group = group;
			tcsched.core = core;
			preempt_disable();
			smp_call_function_single(cpu_id,
						 (smp_call_func_t)setup_tcsched,
						 &tcsched, true);
			preempt_enable();
			handled++;
		}
	}
	if (handled == 0) {
		seq_printf(
			&s,
			"use tcgroup <core> <tc> <group> (0-3)\n    rotsched <core> <0/1> (rotation schedule)\n");
	}
	tcsched.read_only = 1;
	tcsched.tc_mask = (1 << supported_tcs()) - 1;
	preempt_disable();
	for (core = 0; core < YIELD_MONITOR_MAX_CORES; core++) {
		if ((cpu_id = get_first_cpuid_by_core(core)) >= 0) {
			tcsched.core = core;
			smp_call_function_single(cpu_id,
						 (smp_call_func_t)setup_tcsched,
						 &tcsched, true);
		}
	}
	preempt_enable();
	printk(KERN_INFO "%s", txt);
	*pos += count;
	return count;
}

static struct file_operations yield_proc_fops = { .open = yield_proc_open,
						  .read = seq_read,
						  .llseek = seq_lseek,
						  .release = single_release,
						  .write = proc_tc_prio_write };
#endif /* CONFIG_PROC_FS */
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void print_lines(const char *p)
{
	const char *start = p;
	char buf[256];

	do {
		if ((*p == '\n') || *p == 0) {
			unsigned long len;
			if (*p)
				p++;

			len = p - start;
			if (len > sizeof(buf) - 1) {
				len = sizeof(buf) - 1;
			}
			memcpy(buf, start, len);
			buf[len] = 0;
			pr_err("%s", buf);
			start = p;
		} else {
			p++;
		}
	} while (*p);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void yield_context_dump(void)
{
	static atomic_t used;
	static char buf[4096];
	struct seq_file s;

	if (atomic_add_return(1, &used) > 1) {
		return;
	}
	memset(&s, 0, sizeof(s));
	buf[0] = 0;
	s.buf = buf;
	s.size = sizeof(buf);

	_yield_proc_stat(&s, NULL);
	print_lines(s.buf);

	memset(&s, 0, sizeof(s));
	buf[0] = 0;
	s.buf = buf;
	s.size = sizeof(buf);
	yield_to_linux_stat(&s);
	print_lines(s.buf);
	atomic_set(&used, 0);
}
EXPORT_SYMBOL(yield_context_dump);
#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *yield_proc_dir;

static int yield_proc_init(void)
{
	if (yield_proc_dir == NULL) {
		yield_proc_dir = proc_mkdir("yield", NULL);
		proc_create("stat", 0, yield_proc_dir, &yield_proc_fops);
	}
	return 0;
}
#endif /* CONFIG_PROC_FS */

/*--------------------------------------------------------------------------------*\
TCSTATUS_TCU=M:28,31
TCSTATUS_TMX=M:27,27
TCSTATUS_RNST=M:24,23
TCSTATUS_TDS=M:21,21
TCSTATUS_DT=M:20,20
TCSTATUS_TCEE=M:17,17
TCSTATUS_DA=M:15,15
TCSTATUS_A=M:13,13
TCSTATUS_TKSU=M:12,11
TCSTATUS_IXMT=M:10,10
TCSTATUS_TASID=M:0,7


TCBIND_CurTC =M:21,28
TCBIND_CurVPE=M:17,17
TCBIND_CurVPE=M:0,3

    Attention! Yield-Signalmask is only allowed per-VPE (not per-TC)
\*--------------------------------------------------------------------------------*/
static void yield_context_init(int cpu, int yield_tc,
			       unsigned int yield_signal_mask,
			       unsigned int tc_prio)
{
	struct _yield_ctrl *pyield_ctrl = &yield_ctrl[cpu];
	struct _yield_per_tc *pyield_tc;
	unsigned int long val, mvpval, old_tc, i, tc;
	unsigned long flags, time, ksp;

	pr_err("[%x][%s] cpu=%x tc=%x mask=%04x tc_prio=%u\n",
	       smp_processor_id(), __func__, cpu, yield_tc, yield_signal_mask,
	       tc_prio);

#if defined (CONFIG_LTQ_VMB)
	vmb_tc_reserve(cpu, yield_tc);
#endif

	if (!yield_signal_mask) {
		pr_err("[%s] error yield_signal_mask is zero\n",
		       __func__);
		return;
	}
	if (yield_signal_mask & pyield_ctrl->yield_vpe_signal_mask) {
		printk(KERN_ERR
		       "[%s] yield_signal_mask over-crossed with other tc %x %x\n",
		       __func__, yield_signal_mask,
		       pyield_ctrl->yield_vpe_signal_mask);
		return;
	}
	write_vpe_c0_vpeschedule(1 << 5); /*---no group rotation schedule ---*/
	write_tc_c0_tcschedule(
		LINUXOS_TCSCHED_PRIORITY); /*--- Scheduler-Prio for Linux-OS CPU ---*/

	for (tc = 0; tc < YIELD_MAX_TC; tc++) {
		pyield_tc = (struct _yield_per_tc *)pyield_ctrl->per_tc[tc];
		if (pyield_tc == NULL) {
			pyield_tc = kmalloc(sizeof(struct _yield_per_tc),
					    GFP_ATOMIC);
			if (pyield_tc == NULL) {
				pr_err("[%s] memory error\n",
				       __func__);
				return;
			}
			memset(pyield_tc, 0, sizeof(struct _yield_per_tc));
			for (i = sizeof(struct thread_info) /
				 sizeof(unsigned long);
			     i < ARRAY_SIZE(pyield_tc->yield_gp.stack); i++) {
				pyield_tc->yield_gp.stack[i] =
					MAGIC_YIELD_STACK;
			}
			for (i = 0; i < MAX_YIELDSIGNALS; i++) {
				yield_spin_lock_init(
					&pyield_tc->handler[i].progress);
			}
			break;
		} else {
			if (pyield_tc->yield_tc == yield_tc) {
				printk(KERN_ERR
				       "[%s] error doubles yield_tc %d\n",
				       __func__, yield_tc);
				return;
			}
		}
	}
	if (tc == YIELD_MAX_TC) {
		pr_err("[%s] error no more tc-instances\n", __func__);
		return;
	}
	local_irq_save(flags);
	mips_ihb();
	val = read_c0_vpeconf0();
	if (!(val & VPECONF0_MVP)) {
		printk(KERN_ERR
		       "[%s] error only Master VPE's are allowed to configure MT\n",
		       __func__);
		local_irq_restore(flags);
		kfree(pyield_tc);
		return;
	}
	mvpval = dvpe();
	old_tc = read_c0_vpecontrol() & VPECONTROL_TARGTC;

	/* Yield-Thread-Context aufsetzen */
	set_c0_mvpcontrol(
		MVPCONTROL_VPC); /*--- make configuration registers writeable ---*/

	settc(yield_tc);
	write_tc_c0_tchalt(TCHALT_H);

	/*--- bind on master-vpe  vpe1 only possible if started form vpe1 ??? ---*/
	write_tc_c0_tcbind((read_tc_c0_tcbind() & ~TCBIND_CURVPE) | cpu);

	/* Write the address we want it to start running from in the TCPC register. */
	write_tc_c0_tcrestart((unsigned long)yield_context_thread);
	write_tc_c0_tccontext((unsigned long)0);

	/* stack pointer */
	ksp = (unsigned long)pyield_tc->yield_gp.stack + THREAD_SIZE - 32 -
	      sizeof(struct pt_regs);
	write_tc_gpr_sp(ksp);

	/*--- printk(KERN_ERR"%s:# read_tc_c0_tcstatus=%lx\n", __func__, read_tc_c0_tcstatus()); ---*/

	snprintf(pyield_tc->tsk.comm, sizeof(pyield_tc->tsk.comm),
		 "CPU%u-YIELD-TC%u", cpu, yield_tc);
	pyield_tc->tsk.stack = &pyield_tc->yield_gp;
	pyield_tc->yield_gp.thread_info.task = &pyield_tc->tsk;
	pyield_tc->yield_gp.thread_info.flags = _TIF_FIXADE | _TIF_YIELDCONTEXT;
	pyield_tc->yield_gp.thread_info.preempt_count = INIT_PREEMPT_COUNT;
	pyield_tc->yield_gp.thread_info.addr_limit = KERNEL_DS;
	pyield_tc->yield_gp.thread_info.cpu = cpu;

	/* global pointer */
	write_tc_gpr_gp(&pyield_tc->yield_gp);

	write_tc_c0_tcschedule(tc_prio); /*--- Scheduler-Prio for TC-CPU ---*/

	pyield_tc->yield_tc_signal_mask = yield_signal_mask;
	pyield_tc->yield_tc = yield_tc;
	pyield_ctrl->yield_vpe_signal_mask |= yield_signal_mask;

	/*--- set YieldQMask ---*/
	write_vpe_c0_yqmask(pyield_ctrl->yield_vpe_signal_mask);

	pyield_ctrl->per_tc[tc] = pyield_tc;

	val = read_tc_c0_tcstatus();

	/*--- printk(KERN_ERR"%s: tc=%u ype_signal_mask:%lx\n", __func__, yield_tc, read_vpe_c0_yqmask()); ---*/
#if 0
	val = (val & ~(TCSTATUS_A )) | TCSTATUS_DA | TCSTATUS_TMX | TCSTATUS_IXMT;
	write_tc_c0_tcstatus(val);
	clear_c0_mvpcontrol(MVPCONTROL_VPC); /*--- make configuration registers readonly ---*/
	settc(old_tc);
    fork((void *)yield_context_thread, (void *)pyield_tc);
#else
	/*--- Mark the not dynamically allocatable, TC as activated, DSP ase on, prevent interrupts ---*/
	val = (val & ~(TCSTATUS_DA)) | TCSTATUS_A | TCSTATUS_TMX |
	      TCSTATUS_IXMT;
	write_tc_c0_tcstatus(val);
	/*--- write_c0_vpecontrol( read_c0_vpecontrol() | VPECONTROL_TE);  ---*/ /*--- multithreading enabled ---*/
	write_tc_c0_tchalt(read_tc_c0_tchalt() & ~TCHALT_H);

	/* finally out of configuration and into chaos */
	clear_c0_mvpcontrol(
		MVPCONTROL_VPC); /*--- make configuration registers readonly ---*/
	settc(old_tc);
#endif
	mips_ihb();

	evpe(mvpval);
	emt(EMT_ENABLE);
	mips_ihb();

	/*--- printk("%s:#1 vpecontrol=%x\n", __func__, read_c0_vpecontrol()); ---*/
	time = avm_get_cycles();
	while (pyield_tc->tc_init == 0) {
		if ((avm_get_cycles() - time) >
		    ((1000 /* ms */ * 1000) / 500 /* (@ 500 MHz) */ / 2)) {
			panic("[%s] can't start tc %d\n", __func__, yield_tc);
		}
	}
	/*--- printk(KERN_INFO"[%s] tc=%d mask=0x%x done\n", __func__, yield_tc, yield_mask); ---*/
#if defined(CONFIG_PROC_FS)
	yield_proc_init();
#endif
	pyield_ctrl->yield_all_init++;
	local_irq_restore(flags);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_on_work {
	unsigned int yield_tc;
	unsigned int yield_mask;
	unsigned int tc_prio; /*--- group-param  ---*/
	struct semaphore sema;
	struct workqueue_struct *workqueue;
	struct work_struct work;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void yield_on_startup_work(struct work_struct *data)
{
	struct _yield_on_work *pwork =
		container_of(data, struct _yield_on_work, work);
	yield_context_init(smp_processor_id(), pwork->yield_tc,
			   pwork->yield_mask, pwork->tc_prio);
	up(&pwork->sema);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int workprocess(int cpu, struct _yield_on_work *pwork,
			      const char *funcname)
{
	sema_init(&pwork->sema, 0); /*--- nicht betreten ---*/
	if ((pwork->workqueue = create_workqueue(funcname)) == NULL) {
		return -ENOMEM;
	}
	INIT_WORK_ONSTACK(&pwork->work, yield_on_startup_work);

	queue_work_on(cpu, pwork->workqueue, &pwork->work);
	down(&pwork->sema);
	destroy_workqueue(pwork->workqueue);
	return 0;
}
/*--------------------------------------------------------------------------------*\
 * cpu:      bind tc on this cpu
 * yield_tc: tc to use for yield
 * yield_mask: wich signal(s) would be catched
 *
 * actually YIELD_MAX_TC tc possible, no crossover of yield_mask allowed
\*--------------------------------------------------------------------------------*/
int yield_context_init_on(int linux_cpu, unsigned int yield_tc,
			  unsigned int yield_mask, unsigned int tc_prio)
{
	struct _yield_on_work yield_on_work;
	yield_on_work.yield_mask = yield_mask;
	yield_on_work.yield_tc = yield_tc;
	yield_on_work.tc_prio = tc_prio;
	return workprocess(linux_cpu, &yield_on_work, "yield_w");
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const char *name_exception(unsigned int exception)
{
	return exception == 0 ?
		       "Interrupt" :
		       exception == 1 ?
		       "TLB modiﬁcation exception" :
		       exception == 2 ?
		       "TLB exception (load or instruction fetch)" :
		       exception == 3 ?
		       "TLB exception (store)" :
		       exception == 4 ?
		       "Address error exception (load or instruction fetch)" :
		       exception == 5 ?
		       "Address error exception (store)" :
		       exception == 6 ?
		       "Bus error exception (instruction fetch)" :
		       exception == 7 ?
		       "Bus error exception (data reference: load or store)" :
		       exception == 8 ?
		       "Syscall exception" :
		       exception == 9 ?
		       "Breakpoint exception" :
		       exception == 10 ?
		       "Reserved instruction exception" :
		       exception == 11 ?
		       "Coprocessor Unusable exception" :
		       exception == 12 ?
		       "Arithmetic Overﬂow exception" :
		       exception == 13 ?
		       "Trap exception" :
		       exception == 15 ?
		       "Floating point exception" :
		       exception == 16 ?
		       "Coprocessor 2 implementation speciﬁc exception" :
		       exception == 17 ?
		       "CorExtend Unusable" :
		       exception == 18 ?
		       "Precise Coprocessor 2 exception" :
		       exception == 23 ?
		       "Reference to WatchHi/WatchLo address" :
		       exception == 24 ?
		       "Machine check - will not happen on 34K core" :
		       exception == 25 ?
		       "Thread exception. VPEControlEXCPT speciﬁes the type of the thread exception." :
		       exception == 26 ? "DSP ASE State Disabled exception" :
					 "Reserved";
}
/*--------------------------------------------------------------------------------*\
 * yield-context: wait until kernel angry and make panic-log
\*--------------------------------------------------------------------------------*/
static void while_exception_in_yield_handler(void)
{
	for (;;)
		;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void show_regs_in_yield_context(const struct pt_regs *regs,
				       unsigned int exception)
{
	const int field = 2 * sizeof(unsigned long);
	int i;

	/*
	 * Saved main processor registers
	 */
	for (i = 0; i < 32;) {
		if ((i % 4) == 0)
			printk(KERN_EMERG "$%2d   :", i);
		if (i == 0)
			printk(" %0*lx", field, 0UL);
		else if (i == 26 || i == 27)
			printk(" %*s", field, "");
		else
			printk(" %0*lx", field, regs->regs[i]);

		i++;
		if ((i % 4) == 0)
			printk("\n");
	}
#ifdef CONFIG_CPU_HAS_SMARTMIPS
	printk(KERN_EMERG "Acx    : %0*lx\n", field, regs->acx);
#endif
	printk(KERN_EMERG "Hi    : %0*lx\n", field, regs->hi);
	printk(KERN_EMERG "Lo    : %0*lx\n", field, regs->lo);
#ifdef CONFIG_CPU_HAS_DSP_ASE
	printk(KERN_EMERG "ac1Hi: %0*lx ac1Lo: %0*lx\n", field, regs->ac1hi,
	       field, regs->ac1lo);
	printk(KERN_EMERG "ac2Hi: %0*lx ac2Lo: %0*lx\n", field, regs->ac2hi,
	       field, regs->ac2lo);
	printk(KERN_EMERG "ac3Hi: %0*lx ac3Lo: %0*lx\n", field, regs->ac3hi,
	       field, regs->ac3lo);
	printk(KERN_EMERG "dspcontrol: %0*lx\n", field, regs->dspctrl);
#endif /* CONFIG_CPU_HAS_DSP_ASE */

	printk(KERN_EMERG "errepc: %08lx %pS\n", read_c0_errorepc(),
	       (void *)read_c0_errorepc());
	printk(KERN_EMERG "ra    : %0*lx %pS\n", field, regs->regs[31],
	       (void *)regs->regs[31]);
	printk(KERN_EMERG "Status: %08x %s%s%s\n", (uint32_t)regs->cp0_status,
	       (regs->cp0_status & ST0_ERL) ? "ERL " : "",
	       (regs->cp0_status & ST0_EXL) ? "EXL " : "",
	       (regs->cp0_status & ST0_IE) ? "IE " : "");

	if (1 <= exception && exception <= 5) {
		printk(KERN_EMERG "BadVA : %0*lx\n", field, regs->cp0_badvaddr);
	}
}
/*--------------------------------------------------------------------------------*\

\*--------------------------------------------------------------------------------*/
static struct _yield_exception_info {
	struct _yield_ctrl *pyield_ctrl;
	unsigned int exception;
	unsigned int tc;
	unsigned int cpu;
	int (*yield_handler)(int signal, void *ref);
	atomic_t display;
} yield_exception_info;

extern void show_backtrace(struct task_struct *task,
			   const struct pt_regs *regs);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
unsigned long __do_yield(struct _yield_exception_info *pye)
{
	struct _yield_ctrl *pyield_ctrl = pye->pyield_ctrl;
	struct _yield_per_tc *per_tc;
	unsigned long stackpointer;
	struct pt_regs *regs;
	unsigned long *epc;

	if (atomic_add_return(1, &(pye->display)) > 1) {
		return 0;
	}
	if (yield_is_linux_context()) {
		bust_spinlocks(1);
	}
	per_tc = get_per_tc_struct(pyield_ctrl, pye->tc);
	regs = task_pt_regs(per_tc->yield_gp.thread_info.task);
	epc = (unsigned long *)regs->cp0_epc;
	printk(KERN_EMERG
	       "\n"
	       "FATAL ERROR : YIELD-EXCEPTION in TC%d CPU%d Exception: %x(%s)\n"
	       "EPC          : %p %pS\n"
	       "yieldhandler: %pS\n",
	       pye->tc, pye->cpu, pye->exception,
	       name_exception(pye->exception), epc, epc, pye->yield_handler);
	show_regs_in_yield_context(regs, pye->exception);
	epc -= 2;
	if (!is_vmalloc_or_module_addr(epc) &&
	    ((((unsigned long)epc) & 0x3) == 0)) {
		printk(KERN_EMERG "Code: %08lx %08lx <%08lx> %08lx %08lx\n",
		       epc[0], epc[1], epc[2], epc[3], epc[4]);
	}
	stackpointer = regs->regs[29];
	if ((stackpointer < (unsigned long)per_tc->yield_gp.stack) ||
	    (stackpointer >=
	     (unsigned long)per_tc->yield_gp.stack + THREAD_SIZE - 32)) {
		printk(KERN_EMERG
		       "Fatal Error: Stackpointer %08lx exceed stack!\n",
		       stackpointer);
	} else if (stackpointer && ((stackpointer & 0x3) == 0)) {
		unsigned int linefeed = 0;
		unsigned long *p = (unsigned long *)stackpointer;
		printk(KERN_EMERG "Stack:\n");
		while (p <
		       (unsigned long *)((unsigned long)per_tc->yield_gp.stack +
					 THREAD_SIZE - 32)) {
			printk(KERN_CONT "%08lx%s", *p++,
			       (((++linefeed) & 0x7) == 0) ? "\n" : " ");
		}
		printk(KERN_CONT "\n");
	}
	show_backtrace(per_tc->yield_gp.thread_info.task, regs);
	if (yield_is_linux_context()) {
		panic("Yield-Exception on TC%u CPU%u occurred in %pS -> %pS\n",
		      pye->tc, pye->cpu, pye->yield_handler, epc);
	}
	return 0;
}
/*--------------------------------------------------------------------------------*\
 * regs = NULL if invalid SP not in KSEG0-Area
 * ret: new stackpointer (for lazy handling)
\*--------------------------------------------------------------------------------*/
asmlinkage unsigned long do_yield(struct pt_regs *regs)
{
	struct _yield_ctrl *pyield_ctrl = &yield_ctrl[raw_smp_processor_id()];
	struct _yield_per_tc *per_tc;
	unsigned int exception;
	int tc;

	tc = (read_c0_tcbind() & TCBIND_CURTC) >> TCBIND_CURTC_SHIFT;
	exception = (read_c0_cause() >> CAUSEB_EXCCODE) & 0x1F;
	per_tc = get_per_tc_struct(pyield_ctrl, tc);

	if (yield_exception_info.exception) {
		/*--- recursive exception ---*/
		write_c0_epc(while_exception_in_yield_handler);
		write_c0_errorepc(while_exception_in_yield_handler);
		return 0;
	}
	atomic_set(&yield_exception_info.display, 0);
	yield_exception_info.cpu = raw_smp_processor_id();
	yield_exception_info.pyield_ctrl = pyield_ctrl;
	yield_exception_info.exception = exception;
	yield_exception_info.tc = tc;
	yield_exception_info.yield_handler =
		per_tc ? per_tc->act_yh ? per_tc->act_yh->yield_handler : NULL :
			 NULL;
	if (regs) {
		memcpy(task_pt_regs(per_tc->yield_gp.thread_info.task), regs,
		       sizeof(*regs));
	}
	write_c0_epc(while_exception_in_yield_handler);
	write_c0_errorepc(while_exception_in_yield_handler);
	yield_exception(&yield_exception_info);
	mdelay(500); /*--- relaxed __do_yield()-call if linux-cpu can't display ---*/
	__do_yield(&yield_exception_info);
	return 0;
}

#define MAX_YIELD_TO_LINUX_IPI_ENTRIES_ORDER 7
#define MAX_YIELD_TO_LINUX_IPI_ENTRIES                                         \
	(1 << MAX_YIELD_TO_LINUX_IPI_ENTRIES_ORDER)

#define DEBUG_EXTENDED_IPI
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_to_linux_ipi_queue {
	struct _yield_to_linux_ipi entry[MAX_YIELD_TO_LINUX_IPI_ENTRIES];
	atomic_t read_idx;
	atomic_t write_idx;
	unsigned int last_jiffies;
	unsigned int error_once;
	unsigned int reset_stat; /*--- Statistik nur im irq-Kontext reseten ---*/
	unsigned int max_handled;
	unsigned int max_latency;
	unsigned int useless_trigger;
	unsigned long long trigger_cycle_sum;
	unsigned int trigger_cnt;
	unsigned int trigger_cycle;
	atomic_t queue_ovr;
	int ipi_irq;
	int initialized;
#if defined(DEBUG_EXTENDED_IPI)
	struct _generic_stat stat_value[max_ipi_type];
	void *stat_func[max_ipi_type];
#endif
	spinlock_t qlock;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int yield_queue_inc_idx(unsigned int idx)
{
	return ((idx + 1) & (MAX_YIELD_TO_LINUX_IPI_ENTRIES - 1));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int yield_queue_full(unsigned int read_idx,
					    unsigned int write_idx)
{
	if (write_idx >= read_idx) {
		return write_idx - read_idx;
	}
	return MAX_YIELD_TO_LINUX_IPI_ENTRIES - read_idx + write_idx;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static char *name_ipi_type[] = {
	[wake_up_type] "wake_up_type",
	[schedule_work_type] "schedule_work_type",
	[schedule_delayed_work_type] "schedule_delayed_work_type",
	[queue_work_on_type] "queue_work_on_type",
	[tasklet_hi_schedule_type] "tasklet_hi_schedule_type",
	[tasklet_schedule_type] "tasklet_schedule_type",
	[try_module_get_type] "try_module_get_type",
	[module_put_type] "module_put_type",
	[panic_type] "panic_type",
	[yieldexception_type] "yieldexception_type",
	[call_type] "call_type",
	[wake_up_state_type] "wake_up_state_type",
};
#if defined(DEBUG_EXTENDED_IPI)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void display_extended_ipi_infos(int cpu, struct seq_file *m,
				       struct _yield_to_linux_ipi_queue *pq)
{
	unsigned int MHz = avm_get_cyclefreq() / (1000 * 1000);
	unsigned int i;
	seq_printf(m, "[cpu=%d]Executed ipi-functions-sum%s:\n", cpu,
		   pq->reset_stat ? "" : " (since last call)");
	for (i = 0; i < max_ipi_type; i++) {
		unsigned long flags;
		struct _generic_stat *pstat;
		unsigned long cnt, max, min;
		unsigned long long avg64;

		pstat = &pq->stat_value[i];
		yield_spin_lock_irqsave(&pstat->lock, flags);
		cnt = pstat->cnt;
		max = pstat->max;
		min = pstat->min;
		avg64 = pstat->avg;
		if (cnt) {
			init_generic_stat(pstat, 0);
		}
		yield_spin_unlock_irqrestore(&pstat->lock, flags);
		if (cnt) {
			do_div(avg64, (cnt * MHz));
			seq_printf(
				m,
				"%-26s: [%10lu]schedule: min=%10lu max=%10lu avg=%10lu us (%pS)\n",
				name_ipi_type[i], cnt, min / MHz, max / MHz,
				(unsigned long)avg64, pq->stat_func[i]);
		}
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void display_pending_ipi(int cpu, struct seq_file *m,
				struct _yield_to_linux_ipi_queue *pq)
{
	register unsigned int write_idx, read_idx;
	unsigned long flags;
	struct _yield_to_linux_ipi *param;
	yield_spin_lock_irqsave(&pq->qlock, flags);
	write_idx = atomic_read(&pq->write_idx);
	read_idx = atomic_read(&pq->read_idx);
	if (read_idx != write_idx) {
		seq_printf(m, "[cpu=%d]%u pending ipi-functions:\n", cpu,
			   yield_queue_full(read_idx, write_idx));
	}
	while (read_idx != write_idx) {
		param = &pq->entry[read_idx];
		seq_printf(m, "[%3u] %26s: is pending since %lu s (%pS)\n",
			   read_idx, name_ipi_type[param->ipi_func_type],
			   (jiffies - param->ts_jiffies) / HZ,
			   (void *)param->ret_ip);
		read_idx = yield_queue_inc_idx(read_idx);
	}
	yield_spin_unlock_irqrestore(&pq->qlock, flags);
}
#endif /* DEBUG_EXTENDED_IPI */

struct _yield_to_linux_ipi_queue __percpu *gYield_to_linux_ipi_queue;

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void yield_to_linux_stat(struct seq_file *m)
{
	unsigned int MHz = avm_get_cyclefreq() / (1000 * 1000);
	int cpu;

	for_each_cpu (cpu, cpu_online_mask) {
		struct _yield_to_linux_ipi_queue *pq =
			per_cpu_ptr(gYield_to_linux_ipi_queue, cpu);
		if (pq->initialized == 0) {
			continue;
		}
		if (pq->max_handled) {
			seq_printf(m, "[cpu=%u]Yield-to-Linux-Statistic:\n",
				   cpu);
			seq_printf(m, "\tMax-Burst-Executed:  %20u\n",
				   pq->max_handled);
			seq_printf(m, "\tMax-Trigger-Latency: %20u us %s\n",
				   pq->max_latency / MHz,
				   pq->reset_stat ? "" : " (since last call)");
			if (pq->useless_trigger)
				seq_printf(m, "\tUseless trigger:     %20u\n",
					   pq->useless_trigger);
			if (atomic_read(&pq->queue_ovr))
				seq_printf(m, "\tQueue OVR:           %20u\n",
					   atomic_read(&pq->queue_ovr));
			if (pq->trigger_cnt) {
				unsigned long long period =
					pq->trigger_cycle_sum;
				do_div(period, pq->trigger_cnt);
				do_div(period, MHz * 1000);
				seq_printf(
					m,
					"\tavg Trigger-Period:  %20llu ms %s\n",
					period,
					pq->reset_stat ? "" :
							 " (since last call)");
			}
#if defined(DEBUG_EXTENDED_IPI)
			if (pq->trigger_cnt) {
				display_extended_ipi_infos(cpu, m, pq);
			}
#endif
		}
#if defined(DEBUG_EXTENDED_IPI)
		display_pending_ipi(cpu, m, pq);
#endif
		/*--------------------------------------------------------------------------------*\
		  erst im Irq-Kontext reseten
		  (somit geht Statistik im CRV nicht verloren falls IPI-Irq blockiert ist)
		  \*--------------------------------------------------------------------------------*/
		pq->reset_stat = 1;
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void yield_to_linux_reset_stat(struct _yield_to_linux_ipi_queue *pq)
{
#if defined(DEBUG_EXTENDED_IPI)
	unsigned int i;
	for (i = 0; i < max_ipi_type; i++) {
		init_generic_stat(&pq->stat_value[i], 0);
	}
#endif
	pq->reset_stat = 0;
	pq->trigger_cycle_sum = 0;
	pq->trigger_cnt = 0;
	pq->max_latency = 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void yield_queue_init(struct _yield_to_linux_ipi_queue *pq, int ipi_irq)
{
#if defined(DEBUG_EXTENDED_IPI)
	unsigned int i;
	for (i = 0; i < max_ipi_type; i++) {
		init_generic_stat(&pq->stat_value[i], 1);
	}
#endif
	yield_spin_lock_init(&pq->qlock);
	atomic_set(&pq->read_idx, 0);
	atomic_set(&pq->write_idx, 0);
	pq->ipi_irq = ipi_irq;
	pq->initialized = 1;
}
/*--------------------------------------------------------------------------------*\
 * aus Yield-Kontext
\*--------------------------------------------------------------------------------*/
static int yield_enqueue(struct _yield_to_linux_ipi_queue *pq,
			 struct _yield_to_linux_ipi *param)
{
	register unsigned int write_idx, read_idx, post_write_idx;

	BUG_ON(param->ipi_func_type >= max_ipi_type);
	/*---  lock notwendig, da evtl. aus mehreren yield-threads ---*/
	arch_spin_lock(&pq->qlock.rlock.raw_lock);

	rmb();
	write_idx = atomic_read(&pq->write_idx);
	post_write_idx = yield_queue_inc_idx(write_idx);
	read_idx = atomic_read(&pq->read_idx);
	if (unlikely(post_write_idx == read_idx)) {
		arch_spin_unlock(&pq->qlock.rlock.raw_lock);
		/*--- Achtung! printk ausserhalb des spinlocks (Rekursion!) ---*/
		if (pq->error_once == 0) {
			/*--- ... und nicht als Dauertrigger ---*/
			pq->error_once = pq->last_jiffies | 0x1;
			printk(KERN_ERR
			       "[%s] ERROR ipi-queue overflow for %s %pS %u %u (last linux-ipi-irq before %lu s)\n",
			       __func__, name_ipi_type[param->ipi_func_type],
			       (void *)_RET_IP_, write_idx, read_idx,
			       (jiffies - pq->last_jiffies) / HZ);
		} else if (((jiffies | 0x1) - pq->error_once) > (40 * HZ)) {
			/*--- ... nun reichts aber ... ---*/
			pq->error_once = jiffies | 0x1;
			yield_context_dump();
			yield_panic("ERROR YIELD-IPI-IRQ do not work\n");
		}
		return 1;
	}
	param->ret_ip = _RET_IP_;
	param->ts_jiffies = jiffies;
	param->cycle = avm_get_cycles();
	memcpy(&pq->entry[write_idx], param,
	       sizeof(struct _yield_to_linux_ipi));
	atomic_set(&pq->write_idx, post_write_idx);
	arch_spin_unlock(&pq->qlock.rlock.raw_lock);
	return 0;
}
/*--------------------------------------------------------------------------------*\
 * Linux-IRQ-Kontext
\*--------------------------------------------------------------------------------*/
static int yield_dequeue(struct _yield_to_linux_ipi_queue *pq,
			 struct _yield_to_linux_ipi *param)
{
	register unsigned int write_idx, read_idx;
	spin_lock(&pq->qlock);
	rmb();
	write_idx = atomic_read(&pq->write_idx);
	read_idx = atomic_read(&pq->read_idx);
	if (write_idx == read_idx) {
		spin_unlock(&pq->qlock);
		return 0;
	}
	memcpy(param, &pq->entry[read_idx], sizeof(struct _yield_to_linux_ipi));
#if defined(DEBUG_EXTENDED_IPI)
	if (param->ipi_func_type < max_ipi_type) {
		unsigned long act_time = avm_get_cycles() | 0x1;
		if (pq->stat_value[param->ipi_func_type].last_trigger_cycle) {
			generic_stat(
				&pq->stat_value[param->ipi_func_type],
				act_time - pq->stat_value[param->ipi_func_type]
						   .last_trigger_cycle);
		}
		pq->stat_value[param->ipi_func_type].last_trigger_cycle =
			act_time;
		pq->stat_func[param->ipi_func_type] = (void *)param->ret_ip;
	}
#endif
	atomic_set(&pq->read_idx, yield_queue_inc_idx(read_idx));
	spin_unlock(&pq->qlock);
	return 1;
}
/*--------------------------------------------------------------------------------*\
 * ret: 0 ok
\*--------------------------------------------------------------------------------*/
int yield_trigger_linux_ipi(int cpu, struct _yield_to_linux_ipi *obj)
{
	int ret = 0;
	struct _yield_to_linux_ipi_queue *pq =
		per_cpu_ptr(gYield_to_linux_ipi_queue, cpu);

	if (unlikely(!pq)) {
		ret = -1;
		return ret;
	}
	if (unlikely(yield_enqueue(pq, obj))) {
		atomic_inc(&pq->queue_ovr);
		ret = -1;
	}
	gic_trigger_irq(pq->ipi_irq, 1); /*--- trigger ipi irq ---*/
	return ret;
}
EXPORT_SYMBOL(yield_trigger_linux_ipi);

/*--------------------------------------------------------------------------------*\
 * der (Linux-)IRQ-Kontext fuer Yield-to_Linux-IPI (per-cpu)
\*--------------------------------------------------------------------------------*/
static irqreturn_t yield_to_linux_ipi_irq(int irq __attribute__((unused)),
					  void *handle)
{
	struct _yield_to_linux_ipi_queue *pq =
		(struct _yield_to_linux_ipi_queue *)handle;
	struct _yield_to_linux_ipi params;
	unsigned int max_handled = 0, timediff;

	/*--- printk(KERN_ERR"[%s] read=%u write=%u\n", __func__, pq->read_idx, pq->write_idx); ---*/
	while (yield_dequeue(pq, &params)) {
		timediff = avm_get_cycles() - params.cycle;
		if (timediff > pq->max_latency) {
			pq->max_latency = timediff;
		}
		max_handled++;
		/*--- printk(KERN_ERR"[%s] type %u read=%u write=%u\n", __func__, params.ipi_func_type, pq->read_idx, pq->write_idx); ---*/
		switch (params.ipi_func_type) {
		case wake_up_type:
			/*--- printk(KERN_ERR"[%s] wake_up_trigger(%p) \n", __func__, params.u.wake_up_param.q); ---*/
			__wake_up(params.u.wake_up_param.q,
				   params.u.wake_up_param.mode,
				   params.u.wake_up_param.nr_exclusive,
				   params.u.wake_up_param.key);
			break;
		case schedule_work_type:
			schedule_work(params.u.schedule_work_param.work);
			break;
		case schedule_delayed_work_type:
			schedule_delayed_work(
				params.u.schedule_delayed_work_param.dwork,
				params.u.schedule_delayed_work_param.delay);
			break;
		case queue_work_on_type:
			queue_work_on(params.u.queue_work_on_param.cpu,
				      params.u.queue_work_on_param.wq,
				      params.u.queue_work_on_param.work);
			break;
		case tasklet_hi_schedule_type:
			tasklet_hi_schedule(params.u.tasklet_schedule_param.t);
			break;
		case tasklet_schedule_type:
			tasklet_schedule(params.u.tasklet_schedule_param.t);
			break;
		case try_module_get_type:
			/*--- printk(KERN_ERR"%s: try_module_get(%p)\n", __func__, params.u.module_param.module); ---*/
			try_module_get(params.u.module_param.module);
			break;
		case module_put_type:
			/*--- printk(KERN_ERR"%s: module_put(%p)\n", __func__, params.u.module_param.module); ---*/
			module_put(params.u.module_param.module);
			break;
		case panic_type:
			panic("%s\n", params.u.panic_param.debugstr);
			break;
		case yieldexception_type:
			__do_yield(params.u.yieldexception_param.handle);
			break;
		case call_type:
			if (params.u.call_param.func) {
				params.u.call_param.func(
					params.u.call_param.func_param);
			}
			break;
		case wake_up_state_type:
			/*--- printk(KERN_ERR"[%s] wake_up_state_type(%s:%p, %x) \n", __func__, params.u.wake_up_state_param.tsk->comm, params.u.wake_up_state_param.tsk, params.u.wake_up_state_param.state); ---*/
			wake_up_state(params.u.wake_up_state_param.tsk,
				      params.u.wake_up_state_param.state);
			put_task_struct(params.u.wake_up_state_param.tsk);
			break;
		default:
			pr_err("%s:unknown type %u\n", __func__,
			       params.ipi_func_type);
			break;
		}
	}
	if (pq->reset_stat) {
		yield_to_linux_reset_stat(pq);
	}
	if (pq->max_handled < max_handled) {
		pq->max_handled = max_handled;
		/*--- printk(KERN_ERR"%s: max queuefull %u\n", __func__, max_handled); ---*/
	} else if (max_handled == 0) {
		pq->useless_trigger++;
	}
	if (pq->trigger_cycle) {
		pq->trigger_cycle_sum +=
			(unsigned long long)(avm_get_cycles() -
					     pq->trigger_cycle);
	}
	pq->trigger_cnt++;
	pq->trigger_cycle = avm_get_cycles();
	pq->last_jiffies = jiffies;
	pq->error_once = 0;
	return IRQ_HANDLED;
}

/**
 * very important!
 * if ipi-functions used in yield-context use it to flush/sync ipi-queues before
 * any free of linux-depend-data-structs (e.g. workitem)
 * (prevent use-after-free-accesses)
 * only linux-kthread-context
 *
 * timeout: in jiffies
 *
 * ret: 1 all (cpu-)queues) synced
 *	0 timeout
 *
 */
int yield_to_linux_sync_ipi(int timeout)
{
	struct _yield_to_linux_ipi params;
	struct cpumask cpu_ipi_mask;
	int cpu, ret = -ERESTARTSYS;
	unsigned long flags;
	unsigned long end_jiffies = jiffies + timeout;

	memset(&params, 0, sizeof(params));
	params.ipi_func_type = call_type; /* nop - only for synchronisation */

	cpumask_copy(&cpu_ipi_mask, cpu_online_mask);

	for_each_cpu(cpu, &cpu_ipi_mask) {
		struct _yield_to_linux_ipi_queue *pq = per_cpu_ptr(gYield_to_linux_ipi_queue, cpu);

		local_irq_save(flags);
		yield_enqueue(pq, &params);
		gic_trigger_irq(pq->ipi_irq, 1);     /*--- trigger ipi irq ---*/
		local_irq_restore(flags);
	}
	for (;;) {
		for_each_cpu(cpu, &cpu_ipi_mask) {
			struct _yield_to_linux_ipi_queue *pq = per_cpu_ptr(gYield_to_linux_ipi_queue, cpu);

			if (atomic_read(&pq->write_idx) == atomic_read(&pq->read_idx)) {
				/*--- queue-empty: since this point all ipis executed ---*/
				cpumask_clear_cpu(cpu, &cpu_ipi_mask);
				/*--- pr_err("%s: cpu=%u: %p queue empty\n", __func__, cpu, pq); ---*/
			} else {
				/*--- not really necessary, but ... ---*/
				gic_trigger_irq(pq->ipi_irq, 1);     /*--- trigger ipi irq ---*/
			}
		}
		if (cpumask_empty(&cpu_ipi_mask)) {
			/*--- always synced ---*/
			/*--- pr_err("%s: all queues flushed\n", __func__); ---*/
			ret = 1;
			break;
		}
		if (time_after(jiffies, end_jiffies)) {
			pr_err("%s: cpu=%u: timeout\n", __func__, cpu);
			ret = 0;
			break;
		}
		schedule_timeout(1);
	}
	return ret;
}
EXPORT_SYMBOL(yield_to_linux_sync_ipi);

/**
 * installiere auf jeder CPU ein yield-to-linux-irq
\*--------------------------------------------------------------------------------*/
static int yield_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	/*--- struct cpumask tmask; ---*/
	int cpu;
	unsigned int core, tc;
	struct resource irqres[4];
	int ret;

	gYield_to_linux_ipi_queue =
		alloc_percpu(struct _yield_to_linux_ipi_queue);
	if (!gYield_to_linux_ipi_queue) {
		pr_err("%s: memory allocation failed", __func__);
		return -ENODEV;
	}
	ret = of_irq_to_resource_table(node, irqres, 4);
	if (ret != 4) {
		pr_err("%s: interrupts not found", __func__);
		return -ENODEV;
	}
	for_each_cpu (cpu, cpu_online_mask) {
		struct _yield_to_linux_ipi_queue *pq =
			per_cpu_ptr(gYield_to_linux_ipi_queue, cpu);
		int ipi_irq = irqres[cpu].start;

		pr_err("%s: cpu %d irq %d\n", __func__, cpu, ipi_irq);
		if (get_mt_by_cpuid(cpu, &core, &tc) != 0) {
			continue;
		}
		yield_queue_init(pq, ipi_irq);
		/*--- pr_err("%s: cpu=%u install irq=%u\n", __func__, cpu, ipi_irq); ---*/
		if (request_irq_on(cpu, ipi_irq, yield_to_linux_ipi_irq,
				   irqres[cpu].flags & IRQF_TRIGGER_MASK, "YIELD_TO_LINUX_IPI", pq)) {
			pr_err("%s: error on install irq=%u\n",
			       __func__, ipi_irq);
		}
	}
	pr_err("%s: interrupts set up\n", __func__);
	return 0;
}

static const struct of_device_id yield_match[] = {
	{ .compatible = "avm,yield" },
	{},
};

MODULE_DEVICE_TABLE(of, yield_match);

static struct platform_driver yield_driver = {
	.probe = yield_probe,
	.driver = {
		.name = "avm,yield",
		.of_match_table = yield_match,
		.owner = THIS_MODULE,
	},
};
builtin_platform_driver(yield_driver);

/*--------------------------------------------------------------------------------*\
 * export raw-spin(un)lock for modules
\*--------------------------------------------------------------------------------*/
EXPORT_SYMBOL(do_raw_spin_lock);
EXPORT_SYMBOL(do_raw_spin_unlock);
#endif /* YIELD_MAX_TC && YIELD_MAX_TC > 0 */
