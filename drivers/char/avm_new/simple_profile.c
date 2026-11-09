// SPDX-License-Identifier: GPL-2.0+

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#ifdef CONFIG_MIPS
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/bitops.h>
#endif /*--- #ifdef CONFIG_MIPS ---*/
#pragma GCC pop_options

#include <asm/mach_avm.h>

#if defined(CONFIG_AVM_SIMPLE_PROFILING)
#include <avm/profile/profile.h>

#include "avm_sammel.h"
#include "avm_profile.h"
#include "arch_profile.h"

unsigned int profile_BlockNeeded = (CONFIG_AVM_PROFILING_TRACE_MODE * 10);
static unsigned int profile_current_mask;
static enum _simple_profile_enable_mode profile_current_start_handle;

struct _simple_profiling simple_profiling;
EXPORT_SYMBOL(simple_profiling);

struct _cpu_nr_to_tc_core cpu_nr_to_tc_and_core[NR_CPUS];
static struct timer_list profilestatus_timer;
static volatile unsigned int profilestatus_ena;

/**
 */
struct _save_current_param {
	unsigned long lr;
	unsigned long pc;
	unsigned int pid;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	char comm[AVM_PROFILE_CURRENT_COMM_INCLUDED]; /*--- short current name ---*/
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
};

/**
 */
int avm_simple_profiling_is_enabled_func(void)
{
	return avm_simple_profiling_is_enabled();
}
EXPORT_SYMBOL(avm_simple_profiling_is_enabled_func);

/**
 */
unsigned int avm_get_cycles_func(void)
{
	if (!(avm_simple_profiling_is_enabled())) {
		return 0;
	}
	if (!(simple_profiling.mask &
	      (1 << avm_profile_data_type_trace_spinlock))) {
		return 0;
	}
	return avm_get_cycles();
}
EXPORT_SYMBOL(avm_get_cycles_func);

/**
 */
static inline unsigned int lavm_simple_profiling_incpos(
	unsigned int *time, struct _save_current_param *curr_param, int *cpu_id,
	unsigned int *perfcnt1, unsigned int *perfcnt2, unsigned int *sp)
{
	unsigned int pos = (unsigned int)-1;

	*time = avm_get_cycles();
	if (simple_profiling.enabled == 1) {
		pos = atomic_inc_with_max_return(&simple_profiling.pos,
						 simple_profiling.len);
		if (unlikely(pos == (unsigned int)-1)) {
			simple_profiling.enabled = 0;
		}
	} else if (simple_profiling.enabled == 2) {
		unsigned int lastpos = atomic_read(&simple_profiling.pos);

		pos = atomic_inc_with_wrap_return(&simple_profiling.pos,
						  simple_profiling.len);
		if (unlikely(lastpos > pos)) {
			simple_profiling.wraparround++;
		}
	}
	if (sp)
		*sp = arch_profile_sp();
	if (curr_param) {
		curr_param->pid = task_pid_nr(current);
		curr_param->lr = KSTK_RA(current);
		curr_param->pc = KSTK_EIP(current);
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
		memcpy(curr_param->comm, current->comm,
			sizeof(curr_param->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	}
	if (cpu_id)
		*cpu_id = raw_smp_processor_id();
	if (perfcnt1)
		*perfcnt1 = arch_profile_perfcnt1();
	if (perfcnt2)
		*perfcnt2 = arch_profile_perfcnt2();
	return pos;
}

static struct _avm_profile_data *profiling_data(unsigned int pos)
{
	struct _avm_profile_data *data =
		simple_profiling.data[pos / profile_DataSetsPerBlock];

	return &data[pos % profile_DataSetsPerBlock];
}
/**
 * mode Bit0: 1 irqs disabled, 0 irqs enabled
 */

void __avm_simple_profiling_irq_disabled(unsigned long epc, unsigned long ra,
					 unsigned int mode)
{
	struct _avm_profile_data *data;
	struct _save_current_param curr_param;
	unsigned int pos, time, cpu_id, sp, total_access, total_activate;

	if (!(simple_profiling.mask & ((1 << avm_profile_data_type_irq_disable_begin) |
				       (1 << avm_profile_data_type_irq_disable_end))))
		return;

	pos = lavm_simple_profiling_incpos(&time, &curr_param, &cpu_id, &total_access,
					   &total_activate, &sp);
	if (pos == (unsigned int)-1) {
		return;
	}
	sp &= THREAD_MASK;

	data = profiling_data(pos);
	data->type = mode & 0x1 ? avm_profile_data_type_irq_disable_begin : avm_profile_data_type_irq_disable_end;
	data->id = curr_param.pid & 0xFFFF;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	memcpy(data->comm, curr_param.comm, sizeof(data->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	data->cpu_id = cpu_id;
	data->core_id = cpu_nr_to_tc_and_core[cpu_id].core;
	data->tc_id = cpu_nr_to_tc_and_core[cpu_id].tc;
	data->time = time;
	data->addr = epc;
	data->lr = ra;
	data->stack_pos = sp;
	data->total_access = total_access;
	data->total_activate = total_activate;
}
EXPORT_SYMBOL(__avm_simple_profiling_irq_disabled);

/**
 * interrupted epc
 * interrupted lr (special case on x86: frame-pointer)
 */
void __avm_simple_profiling_irqcontext(unsigned long epc,
				       unsigned long ra)
{
#if !defined(PROFILING_IN_YIELD) && !defined(PROFILING_IN_FIQ) &&              \
	!defined(PROFILING_IN_NMI)
	struct _save_current_param curr_param;
	int cpu_id;
	unsigned int pos, time, total_access, total_activate, sp;
	struct _avm_profile_data *data;

	if (!(simple_profiling.mask &
	      (1 << avm_profile_data_type_code_address_info)))
		return;

	pos = lavm_simple_profiling_incpos(&time, &curr_param, &cpu_id,
					   &total_access, &total_activate, &sp);
	if (pos == (unsigned int)-1) {
		return;
	}
	sp &= THREAD_MASK;

	data = profiling_data(pos);
	data->id = curr_param.pid & 0xFFFF;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	memcpy(data->comm, curr_param.comm, sizeof(data->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	data->type = avm_profile_data_type_code_address_info;
	data->cpu_id = cpu_id;
	data->core_id = cpu_nr_to_tc_and_core[cpu_id].core;
	data->tc_id = cpu_nr_to_tc_and_core[cpu_id].tc;
#if defined(CONFIG_X86)
	data->lr = get_ra_from_bp(0, ra);
#else /*--- #if defined(CONFIG_X86) ---*/
	data->lr = ra;
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_X86) ---*/
	data->addr = epc;
	data->time = time;
	data->stack_pos = sp;
	data->total_access = total_access;
	data->total_activate = total_activate;
#endif /*--- #if !defined(PROFILING_IN_YIELD) ---*/
}

/**
 * Achtung! in dieser Funktion
 */
void __avm_simple_profiling_spinlock(unsigned long addr, unsigned int lock,
				     unsigned int locktime, unsigned int id)
{
	unsigned int pos, time;
	int cpu_id;
	struct _avm_profile_data *data;

	if (!(simple_profiling.mask &
	      (1 << avm_profile_data_type_trace_spinlock)))
		return;

	pos = lavm_simple_profiling_incpos(&time, NULL, &cpu_id, NULL, NULL,
					   NULL);
	if (pos == (unsigned int)-1) {
		return;
	}
	data = profiling_data(pos);
	data->id = id;
	data->cpu_id = cpu_id;
	data->core_id = cpu_nr_to_tc_and_core[cpu_id].core;
	data->tc_id = cpu_nr_to_tc_and_core[cpu_id].tc;
	data->type = avm_profile_data_type_trace_spinlock;
	data->addr = _RET_IP_;
	data->lr = addr;
	data->time = time;
	data->total_access = (unsigned int)lock;
	data->total_activate = locktime;
}
EXPORT_SYMBOL(__avm_simple_profiling_spinlock);

/**
 */
void __avm_simple_profiling_skb(unsigned long addr,
				unsigned long where,
				struct sk_buff *skb)
{
	unsigned int pos, time;
	int cpu_id;
	struct _avm_profile_data *data;

	if (!(simple_profiling.mask & (1 << avm_profile_data_type_trace_skb)))
		return;

	pos = lavm_simple_profiling_incpos(&time, NULL, &cpu_id, NULL, NULL,
					   NULL);
	if (pos == (unsigned int)-1) {
		return;
	}
	data = profiling_data(pos);
	data->id = skb ? (skb_cloned(skb) & 0xFFFF) : 0;
	data->cpu_id = cpu_id;
	data->core_id = cpu_nr_to_tc_and_core[cpu_id].core;
	data->tc_id = cpu_nr_to_tc_and_core[cpu_id].tc;
	data->type = avm_profile_data_type_trace_skb;
	data->addr = _RET_IP_;
	data->lr = addr;
	data->time = time;
	data->total_access = (unsigned int)skb;
	data->total_activate = where;
}
EXPORT_SYMBOL(__avm_simple_profiling_skb);

/**
 */
void __avm_simple_profiling_enter_irq(unsigned int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin,
				 (unsigned int)desc, irq);
}

/**
 */
void __avm_simple_profiling_leave_irq(unsigned int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	avm_simple_profiling_log(avm_profile_data_type_hw_irq_end,
				 (unsigned int)desc, irq);
}

/**
 */
void __avm_simple_profiling_log(enum _avm_profile_data_type type,
				unsigned long addr, unsigned int id)
{
	struct _save_current_param curr_param, *pcurr;
	struct task_struct *tsk;
	unsigned int pos, time, total_access, total_activate, sp, lr = 0;
	int cpu_id;
	struct _avm_profile_data *data;

	if (!(simple_profiling.mask & (1 << type))) {
		return;
	}
	if ((type == avm_profile_data_type_code_begin) ||
	    (type == avm_profile_data_type_code_end)) {
		pcurr = NULL;
	} else {
		pcurr = &curr_param;
	}
	pos = lavm_simple_profiling_incpos(&time, pcurr, &cpu_id, &total_access,
					   &total_activate, &sp);
	if (pos == (unsigned int)-1) {
		return;
	}
	sp &= THREAD_MASK;

	data = profiling_data(pos);
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	data->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	switch (type) {
	default:
	case avm_profile_data_type_unknown:
		data->type = type;
		break;
	case avm_profile_data_type_sched:
		id = curr_param.pid & 0xFFFF;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
		memcpy(data->comm, curr_param.comm, sizeof(data->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
		lr = (unsigned int)curr_param.lr;
		addr = (unsigned int)curr_param.pc;
		break;
	case avm_profile_data_type_code_begin:
	case avm_profile_data_type_code_end:
		tsk = (struct task_struct *)addr;
		if (virt_addr_valid(tsk)) {
			addr = (unsigned int)KSTK_EIP(
				tsk); /*--- ... aus dem task den EPC ermitteln  ---*/
			lr = (unsigned int)KSTK_RA(tsk);
			id = task_pid_nr(tsk);
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
			memcpy(data->comm, tsk->comm, sizeof(data->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
		}
		break;
	case avm_profile_data_type_code_address_info:
		if (id != AVM_PROFILE_IDLE_ID) {
			/*--- if no reserved id used - remark with pid ---*/
			id = curr_param.pid & 0xFFFF;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
			memcpy(data->comm, curr_param.comm,
				sizeof(data->comm));
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
			lr = (unsigned int)curr_param.lr;
			addr = (unsigned int)curr_param.pc;
		} else {
			lr = 0;
			addr = 0;
		}
		break;
	case avm_profile_data_type_hw_irq_begin:
	case avm_profile_data_type_hw_irq_end:
	case avm_profile_data_type_sw_irq_begin:
	case avm_profile_data_type_sw_irq_end:
	case avm_profile_data_type_timer_begin:
	case avm_profile_data_type_timer_end:
	case avm_profile_data_type_tasklet_begin:
	case avm_profile_data_type_tasklet_end:
	case avm_profile_data_type_hi_tasklet_begin:
	case avm_profile_data_type_hi_tasklet_end:
	case avm_profile_data_type_workitem_begin:
	case avm_profile_data_type_workitem_end:
	case avm_profile_data_type_func_begin:
	case avm_profile_data_type_func_end:
	case avm_profile_data_type_trigger_tasklet_begin:
	case avm_profile_data_type_trigger_tasklet_end:
	case avm_profile_data_type_trigger_user_begin:
	case avm_profile_data_type_trigger_user_end:
		break;
	}
	data->id = id & 0xFFFF;
	data->cpu_id = cpu_id;
	data->core_id = cpu_nr_to_tc_and_core[cpu_id].core;
	data->tc_id = cpu_nr_to_tc_and_core[cpu_id].tc;
	data->type = type;
	data->addr = addr;
	data->lr = lr;
	data->time = time;
	data->stack_pos = sp;
	data->total_access = total_access;
	data->total_activate = total_activate;
}
EXPORT_SYMBOL(__avm_simple_profiling_log);
#if defined(PROFILING_IN_YIELD) || defined(PROFILING_IN_FIQ) ||                \
	defined(PROFILING_IN_NMI)
/**
 * Code-Watch aus YIELD
 */
void __avm_simple_profiling_code_from_other_context(unsigned long pc, unsigned long lr, struct task_struct *curr_tsk,
						    int cpu_id, int core, int tc_id, unsigned int perfcnt1,
						    unsigned int perfcnt2, unsigned int sp)
{
	struct _avm_profile_data *data;
	unsigned int pos, time;
	int id = 0;

	if (!(simple_profiling.mask &
	      (1 << avm_profile_data_type_code_address_info))) {
		return;
	}
	pos = lavm_simple_profiling_incpos(&time, NULL, NULL, NULL, NULL, NULL);
	if (pos == (unsigned int)-1) {
		return;
	}
	sp &= THREAD_MASK;
	data = profiling_data(pos);
	if (curr_tsk) {
		id = task_pid_nr(curr_tsk);
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
		memcpy(data->comm, curr_tsk->comm, sizeof(data->comm));
	} else {
		data->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	}
	data->id = id & 0xFFFF;
	data->cpu_id = cpu_id;
	data->core_id = core;
	data->tc_id = tc_id;
	data->type = avm_profile_data_type_code_address_info;
	data->addr = pc;
	data->lr = lr;
	data->time = time;
	data->stack_pos = sp;
	data->total_access = perfcnt1;
	data->total_activate = perfcnt2;
	/*--- pr_err("cpu_id=%x core_id=%x tc_id=%x id=%x addr=%08x current=%p perfcnt1=%x perfcnt2=%x\n", cpu_id, core, tc_id, id, pc, curr_tsk, perfcnt1, perfcnt2); ---*/
}

/**
 * Backtrace aus YIELD
 */
void __avm_simple_profiling_backtrace_from_other_context(unsigned long pc, unsigned long lr, unsigned int sp,
							 struct task_struct *curr_tsk,
							 int cpu_id, int core, int tc_id,
							 int no_backtrace)
{
	struct _avm_profile_data *data;
	unsigned int pos, time, stack_pos;
	int id = 0;
	unsigned int bt[3] = { 0, 0, 0 };

	if (!(simple_profiling.mask & (1 << avm_profile_data_type_backtrace))) {
		return;
	}
	pos = lavm_simple_profiling_incpos(&time, NULL, NULL, NULL, NULL, NULL);
	if (pos == (unsigned int)-1) {
		return;
	}
	data = profiling_data(pos);
	if (curr_tsk) {
		id = task_pid_nr(curr_tsk);
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
		memcpy(data->comm, curr_tsk->comm, sizeof(data->comm));
	} else {
		data->comm[0] = 0;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	}
	if (unlikely(no_backtrace)) {
		bt[0] = lr;
	} else {
		arch_backtrace(sp, pc, lr, bt, ARRAY_SIZE(bt));
	}
	stack_pos = sp & THREAD_MASK;

	data->id = id & 0xFFFF;
	data->cpu_id = cpu_id;
	data->core_id = core;
	data->tc_id = tc_id;
	data->type = avm_profile_data_type_backtrace;
	data->addr = pc;
	data->lr = bt[0]; //PClvl2
	data->time = time;
	data->stack_pos = stack_pos;
	data->total_access = bt[1]; //PClvl3
	data->total_activate = bt[2]; //PClvl4
	/*--- pr_err("cpu_id=%x core_id=%x tc_id=%x id=%x addr=%08x current=%p perfcnt1=%x perfcnt2=%x\n", cpu_id, core, tc_id, id, pc, curr_tsk, perfcnt1, perfcnt2); ---*/
}
#endif /*--- #if defined(PROFILING_IN_YIELD) ---*/
/**
 */
static int alloc_profiler_memory(void)
{
	unsigned int i;

	simple_profiling.data =
		kmalloc(sizeof(void *) * profile_BlockNeeded, GFP_ATOMIC);
	if (simple_profiling.data == NULL) {
		return -ENOMEM;
	}
	memset(simple_profiling.data, 0, sizeof(void *) * profile_BlockNeeded);
	simple_profiling.len = profile_DataSetsPerBlock * profile_BlockNeeded;
	for (i = 0; i < profile_BlockNeeded; i++) {
		struct page *ptr = alloc_pages(__GFP_NORETRY, 0);

		if (ptr == NULL) {
			simple_profiling.len = profile_DataSetsPerBlock * i;
			pr_info("[simple-profiling]resize %d pages instead %d pages\n",
				i, profile_BlockNeeded);
			profile_BlockNeeded = i;
			break;
		}
		simple_profiling.data[i] = (void *)lowmem_page_address(ptr);
		/*--- pr_info("\t%d: %x ==> %x\n", i, ptr, simple_profiling.data[i]); ---*/
	}
	pr_info("[simple-profiling]need %d pages for %d entries (%d samples per page)\n",
		i, simple_profiling.len, profile_DataSetsPerBlock);
	return 0;
}

/**
 */
static void free_profiler_memory(void)
{
	unsigned int i;

	if (simple_profiling.data == NULL) {
		return;
	}
	for (i = 0; i < profile_BlockNeeded; i++) {
		free_pages((unsigned int)simple_profiling.data[i], 0);
	}
	kfree(simple_profiling.data);
	atomic_set(&simple_profiling.pos, 0);
	simple_profiling.len = 0;
	simple_profiling.wraparround = 0;
	simple_profiling.data = NULL;
}

/**
 */
void avm_simple_profiling_memresize(unsigned int BlockNeeded)
{
	if (profile_BlockNeeded != BlockNeeded) {
		avm_simple_profiling_enable(sp_enable_off, 0, 0, NULL, NULL, 1);
		free_profiler_memory();
		atomic_set(&simple_profiling.busy, 0);
		profile_BlockNeeded = BlockNeeded;
	}
}

/**
 */
static inline __u32 CLK_TO_MSEC(__u64 cycles)
{
	do_div(cycles, gCycle_per_usec * 1000UL);
	return (__u32)cycles;
}

/**
 */
unsigned long timediff_msec(unsigned int count)
{
	struct _avm_profile_data *pdata = avm_simple_profiling_by_idx(0);
	unsigned long i, msec = 0, last_time = 0;
	unsigned long long cycle_time = 0;

	if (likely(pdata)) {
		last_time = pdata->time;
	}
	for (i = 1; i < count; i++) {
		pdata = avm_simple_profiling_by_idx(i);
		if (likely(pdata)) {
			unsigned long diff_time = (pdata->time - last_time);

			if (likely(diff_time < UINT_MAX / 2)) {
				cycle_time += (__u64)diff_time;
			}
			last_time = pdata->time;
		}
	}
	if (gCycle_per_usec) {
		msec = CLK_TO_MSEC(cycle_time);
	}
	return msec;
}

/**
 */
static void profilestatus_timerfunction(unsigned long context)
{
	if (profilestatus_ena) {
		if (simple_profiling.enabled == 0) {
			unsigned int count = atomic_read(&simple_profiling.pos);

			profilestatus_ena = 0;
			pr_info("[simple-profiling]profiler off: %u entries\n",
				count);
		} else if (simple_profiling.enabled == 2) {
			if (simple_profiling.wraparround >= profilestatus_ena) {
				pr_info("[simple-profiling]profiler wrap %u - %u entries\n",
					simple_profiling.wraparround,
					simple_profiling.len);
				profilestatus_ena =
					simple_profiling.wraparround + 1;
			}
		}
		mod_timer(&profilestatus_timer, profilestatus_timer.expires + HZ);
	}
}

/**
 * on = 1: normal mode
 * on = 2: wraparround
 * on = 3: Uart-Trace
 * enable_perfcnt: Performance-Counter nutzen
 * setbusy: busy-Flag setzen
 * timediff in msec
 */
int avm_simple_profiling_enable(enum _simple_profile_enable_mode on,
				unsigned int enable_perfcnt, unsigned int mask,
				unsigned int *count, unsigned long *timediff,
				unsigned int set_busy)
{
	unsigned int _count, disabled = 0;
	unsigned long td;

	/*--- pr_info("[simple-profiling]%s on=%u\n", __func__, on); ---*/
	profilestatus_ena = 0;
	del_timer(&profilestatus_timer);
	if (on) {
		if (atomic_add_return(1, &simple_profiling.busy) > 1) {
			pr_err("[simple-profiling]analyzing busy - can't enable profiler, try it later\n");
			return -1;
		}
		if (mask) {
			profile_current_mask = mask;
			profile_current_start_handle = on;
		}
		if (simple_profiling.data == NULL) {
			if (alloc_profiler_memory()) {
				pr_err("[simple-profiling]can't enable profiler (memory-error)\n");
				return -1;
			}
		}
		atomic_set(&simple_profiling.pos, 0);
		simple_profiling.wraparround = 0;
		simple_profiling.mask = mask;
		atomic_set(&simple_profiling.busy, 0);
		switch (on) {
		default:
			pr_err("[simple-profiling]%s Unknown profiling mode.Assuming normal profiling without wraparround.\n",
			       __func__);
			/* FALLTHRU */
		case sp_enable_on:
			simple_profiling.enabled = 1;
			break;
		case sp_enable_wrap:
		case sp_enable_uart:
			simple_profiling.enabled =
				2; /*--- wrap-arround mode ---*/
			break;
		}
		if (count)
			*count = 0;
		if (timediff)
			*timediff = 0;
		if (arch_profile_ctrl.profiling_special_enable) {
			arch_profile_ctrl.profiling_special_enable(
				on, enable_perfcnt);
		}
		pr_info("[simple-profiling]profiler on %u free entries %s\n",
			simple_profiling.len,
			simple_profiling.enabled == 2 ? "(wrap-mode)" : "");
		profilestatus_timer.data = 0;

		profilestatus_timer.function = profilestatus_timerfunction;
		profilestatus_timer.expires = jiffies + HZ;
		profilestatus_ena = 1;
		add_timer(&profilestatus_timer);
		return 0;
	}
	if (set_busy) {
		atomic_set(&simple_profiling.busy, 1);
	}
	disabled = simple_profiling.enabled;
	//    if(simple_profiling.enabled) {
	simple_profiling.enabled = 0;
	if (arch_profile_ctrl.profiling_special_enable) {
		arch_profile_ctrl.profiling_special_enable(0, enable_perfcnt);
	}
	//    }
	if (simple_profiling.wraparround == 0) {
		_count = atomic_read(&simple_profiling.pos);
	} else {
		_count = simple_profiling.len;
	}
	if (count) {
		*count = _count;
	}
	td = timediff_msec(_count);
	if (timediff) {
		*timediff = td;
	}
	if (disabled) {
		char tmp[64];

		if (simple_profiling.wraparround) {
			snprintf(tmp, sizeof(tmp), " (%u wraparrounds)",
				 simple_profiling.wraparround);
		} else {
			tmp[0] = 0;
		}
		pr_info("[simple-profiling]profiler off: %u entries%s (%lu ms)\n",
			_count, tmp, td);
	}
	return 0;
}

/**
 */
void avm_simple_profiling_restart(void)
{
	if (profile_current_start_handle || profile_current_mask) {
		avm_simple_profiling_enable(profile_current_start_handle, 1,
					    profile_current_mask, NULL, NULL,
					    0);
	} else {
		pr_info("[%s] Push-Button for profiling ignored (not initialized)\n",
			__func__);
	}
}
EXPORT_SYMBOL(avm_simple_profiling_restart);

/**
 */
struct _avm_profile_data *avm_simple_profiling_by_idx(unsigned int idx)
{
	unsigned int pos;

	if (simple_profiling.wraparround == 0) {
		pos = idx +
		      1; /*--- die Eintraege sind mit ein pre-inkrement angelegt! ---*/
		if (pos <= (unsigned int)atomic_read(&simple_profiling.pos)) {
			return profiling_data(pos);
		}
		return NULL;
	}
	pos = atomic_read(&simple_profiling.pos) + idx +
	      1 /*--- pre-inkrement ---*/;
	if (pos >= simple_profiling.len) {
		pos -= simple_profiling.len;
		if (pos >= (unsigned int)atomic_read(&simple_profiling.pos)) {
			return NULL;
		}
	}

	return profiling_data(pos);
}

/**
 */
int avm_simple_profiling_init(void)
{
	simple_profiling.len = 0;
	simple_profiling.enabled = 0;
	atomic_set(&simple_profiling.pos, 0);
	atomic_set(&simple_profiling.busy, 0);
	gCycle_per_usec = avm_get_cyclefreq() / (1000 * 1000);

	init_timer(&profilestatus_timer);
	/*--- pr_info("[simple-profiling] %u entries %u min\n", simple_profiling.len, CONFIG_AVM_PROFILING_TRACE_MODE); ---*/
	return 0;
}

arch_initcall(avm_simple_profiling_init);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
