/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _avm_profile_h_
#define _avm_profile_h_

#if defined(__KERNEL__)
#include <linux/compiler.h>
#include <linux/types.h>
#endif

#define AVM_PROFILING_VERSION   3
#ifndef CONFIG_AVM_PROFILING_TRACE_MODE
#define CONFIG_AVM_PROFILING_TRACE_MODE 50
#endif /*--- #ifndef CONFIG_AVM_PROFILING_TRACE_MODE ---*/

#define AVM_PROFILE_CURRENT_COMM_INCLUDED 8 /*--- max len of comm ---*/

#define AVM_PROFILE_PAGE_FAULT_ID ((0x1 << 16) - 2)
#define AVM_PROFILE_IDLE_ID       ((0x1 << 16) - 1)

/**
 */
enum _avm_profile_data_type {
	avm_profile_data_type_code_address_info = 0,
	avm_profile_data_type_trace_skb = 1,
	avm_profile_data_type_hw_irq_begin = 2,
	avm_profile_data_type_hw_irq_end = 3,
	avm_profile_data_type_sw_irq_begin = 4,
	avm_profile_data_type_sw_irq_end = 5,
	avm_profile_data_type_timer_begin = 6,
	avm_profile_data_type_timer_end = 7,
	avm_profile_data_type_tasklet_begin = 8,
	avm_profile_data_type_tasklet_end = 9,
	avm_profile_data_type_hi_tasklet_begin = 10,
	avm_profile_data_type_hi_tasklet_end = 11,
	avm_profile_data_type_workitem_begin = 12,
	avm_profile_data_type_workitem_end = 13,
	avm_profile_data_type_func_begin = 14,
	avm_profile_data_type_func_end = 15,
	avm_profile_data_type_trigger_tasklet_begin = 16,
	avm_profile_data_type_trigger_tasklet_end = 17,
	avm_profile_data_type_trigger_user_begin = 18,
	avm_profile_data_type_trigger_user_end = 19,
	avm_profile_data_type_code_begin =
		20, /* obsolet - use avm_profile_data_type_sched */
	avm_profile_data_type_code_end =
		21, /* obsolet - use avm_profile_data_type_sched */
	avm_profile_data_type_trace_spinlock = 22,
	avm_profile_data_type_backtrace = 23,
	avm_profile_data_type_exception_begin = 24,
	avm_profile_data_type_exception_end = 25,
	avm_profile_data_type_sched = 26,
	avm_profile_data_type_irq_disable_begin = 27,
	avm_profile_data_type_irq_disable_end = 28,
	avm_profile_data_type_unknown
};
struct _avm_profile_data {
	enum _avm_profile_data_type type : 8;
	unsigned int core_id : 2;
	unsigned int tc_id : 3;
	unsigned int cpu_id : 3;
	unsigned int id : 16;
	unsigned int stack_pos;
	unsigned long addr;
	unsigned long lr;
	unsigned int time;
	unsigned int total_access;
	unsigned int total_activate;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	char comm[AVM_PROFILE_CURRENT_COMM_INCLUDED]; /*--- short current name ---*/
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
} __packed;

#if defined(CONFIG_AVM_SIMPLE_PROFILING)

struct sk_buff;

#define profile_DataSetsPerBlock        ((1 << PAGE_SHIFT) / sizeof(struct _avm_profile_data))
extern unsigned int profile_BlockNeeded;

#if defined(AVM_PROFILING_VERSION)
struct _simple_profiling {
	void **data;
	atomic_t pos;
	unsigned int len;
	unsigned int enabled;
	unsigned int mask;
	unsigned int wraparround;
	atomic_t busy;
};
#endif /*--- #if defined(AVM_PROFILING_VERSION) ---*/

extern struct _simple_profiling simple_profiling;

#define avm_simple_profiling_is_enabled() unlikely(simple_profiling.enabled)

/**
 */
extern void __avm_simple_profiling_log(enum _avm_profile_data_type type,
				       unsigned long addr, unsigned int id);
static inline void avm_simple_profiling_log(enum _avm_profile_data_type type,
					    unsigned long addr, unsigned int id)
{
	if (avm_simple_profiling_is_enabled()) {
		__avm_simple_profiling_log(type, addr, id);
	}
}

extern void __avm_simple_profiling_irqcontext(unsigned long epc,
					      unsigned long ra);

/**
 * switch to irq-context
 * addr =  interrupted epc
 */
static inline void __deprecated avm_simple_profiling_enter_irqcontext(unsigned long epc)
{
	if (avm_simple_profiling_is_enabled()) {
		__avm_simple_profiling_irqcontext(epc, 0);
	}
}

extern void __avm_simple_profiling_irq_disabled(unsigned long epc, unsigned long ra, unsigned int mode);

/**
 * status set: irq off
 */
static inline void avm_simple_profiling_irq_disabled(unsigned long epc, unsigned long ra,
						     unsigned int old_status, unsigned int new_status)
{
	if (!avm_simple_profiling_is_enabled()) {
		return;
	}
	if (old_status == new_status) {
		return;
	}
	__avm_simple_profiling_irq_disabled(epc, ra, new_status ? 1 : 0);
}

static inline void avm_simple_profiling_enter_irq_context(unsigned long epc, unsigned long ra)
{
	if (avm_simple_profiling_is_enabled()) {
		__avm_simple_profiling_irqcontext(epc, ra);
	}
}

#define avm_simple_profiling_enter_irq_context avm_simple_profiling_enter_irq_context

extern void __avm_simple_profiling_enter_irq(unsigned int irq);

/**
 */
static inline void avm_simple_profiling_enter_irq(unsigned int irq)
{
	if (avm_simple_profiling_is_enabled()) {
		__avm_simple_profiling_enter_irq(irq);
	}
}

extern void __avm_simple_profiling_leave_irq(unsigned int irq);

/**
 */
static inline void avm_simple_profiling_leave_irq(unsigned int irq)
{
	if (avm_simple_profiling_is_enabled()) {
		__avm_simple_profiling_leave_irq(irq);
	}
}

static inline void avm_simple_profiling_sched(void)
{
	if (avm_simple_profiling_is_enabled()) {
		__avm_simple_profiling_log(avm_profile_data_type_sched, 0, 0);
	}
}
#define avm_simple_profiling_sched() avm_simple_profiling_sched()

/**
 */
static inline void avm_simple_profiling_enter_idle(unsigned long addr)
{
	if (avm_simple_profiling_is_enabled()) {
		/*--- entweder ---*/
		__avm_simple_profiling_log(
			avm_profile_data_type_code_address_info, addr,
			AVM_PROFILE_IDLE_ID);
		/*--- oder ... ---*/
		__avm_simple_profiling_log(avm_profile_data_type_backtrace,
					   addr, AVM_PROFILE_IDLE_ID);
	}
}

enum _simple_profile_enable_mode {
	sp_enable_off = 0,
	sp_enable_on = 1,
	sp_enable_wrap = 2,
	sp_enable_perform = 3,
	sp_enable_uart = 4,
	sp_enable_max = 5,
};

/**
 */
int avm_simple_profiling_enable(enum _simple_profile_enable_mode on,
				unsigned int enable_perfcnt, unsigned int mask,
				unsigned int *count, unsigned long *timediff,
				unsigned int set_busy);
void avm_simple_profiling_memresize(unsigned int BlockNeeded);
struct _avm_profile_data *avm_simple_profiling_by_idx(unsigned int idx);
void avm_simple_profiling_restart(void);

#if defined(CONFIG_SOC_GRX500)
unsigned int avm_get_load(unsigned int cpu, unsigned int cpufreq);
#endif /*--- #if defined(CONFIG_SOC_GRX500) ---*/

#else /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
#define avm_simple_profiling_sched()
#define avm_simple_profiling_enter_irqcontext(epc) 0
#define avm_simple_profiling_enter_irq_context(epc, lr) 0
#define avm_simple_profiling_enter_irq(irq)
#define avm_simple_profiling_leave_irq(irq)
#define avm_simple_profiling_enter_idle(addr)
#define avm_simple_profiling_log(type, addr, id)
#define avm_simple_profiling(regs)
#define avm_simple_profiling_enable(on, enable_perfcnt, mask, count, timediff, \
				    setbusy)
#define avm_simple_profiling_is_enabled() 0
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

/**
 * Idee ist, dass man die Hooks immer im Code behalten kann. Will man sie
 * aktivieren definiert man CONFIG_ENABLE_SKB_PROFILING_HOOKS vor dem #include
 * des avm_profile.h
 *
 * Der Parameter dient dazu Zusammengehörigkeiten zu definieren. Üblicherweise
 * sollte dieser 0x00 sein. Wenn ein skb in einem Buffer abgelegt wird und
 * dessen Pointer weiter getraced wird, sollte beim Übergang (also an der Stelle
 * an der der Pointer im Buffer abgelegt wird, der Wert des Parameters mit der
 * Adresse des Buffer gefüllt werden.
 *
 * Alternativ kann in diesem Feld die TCP-Sequence Nummer abgelegt werden. Die
 * Auswertung entscheidet wie dieses Feld dem SKB zuzuordnen ist.
 */
struct sk_buff;
#ifdef CONFIG_ENABLE_SKB_PROFILING_HOOKS
#define avm_simple_profiling_skb(param, skb)                                   \
	({                                                                     \
		if (avm_simple_profiling_is_enabled()) {                       \
			__avm_simple_profiling_skb(_RET_IP_, param, skb);      \
		}                                                              \
	})

extern void __avm_simple_profiling_skb(unsigned long addr, unsigned long param,
				       struct sk_buff *skb);
#else /* #ifdef CONFIG_ENABLE_SKB_PROFILING_HOOKS */
static inline void avm_simple_profiling_skb(unsigned long param,
					    struct sk_buff *skb)
{};
#endif /* #else #ifdef CONFIG_ENABLE_SKB_PROFILING_HOOKS */

struct mm_struct;
extern void boot_profiling_stop(void);
extern int __get_userinfo(char *buf, unsigned int maxbuflen, struct mm_struct *mmm, unsigned long addr);

#endif /*--- #ifndef _avm_profile_h_ ---*/
