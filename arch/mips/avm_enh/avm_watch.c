/*
 *
 *   Copyright (C) 2017 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/kdebug.h>

#include <asm/processor.h>
#include <asm/watch.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <avm/sammel/simple_proc.h>
#include <linux/ctype.h>
#include <linux/mm.h>

#include <asm/avm_enh/avm_watch.h>
#include <asm/avm_enh/avm_enh.h>
#include <asm/mach_avm.h>

#include <asm/branch.h>

#if defined(CONFIG_AVM_WP)

#define SET_WATCHPOINT_FAIL_WRONG_TYPE	-1
#define SET_WATCHPOINT_FAIL_NO_REG	-2
#define SET_WATCHPOINT_SUCCESS		 0

#define NR_WATCHPOINTS 4

#define WATCHPOINT_HISTORY_LEN 32
#ifndef WATCHPOINT_HISTORY_NO_SUPPRESS_NEIGHBOURS
/*
 * To avoid recording too many consecutive accesses to the same data structure
 * by instructions immediately following each other, we only record an access
 * as new if the EPC value was not seen before and is significantly after or
 * before the previously recorded EPC. Note that this is only done if the RA
 * is the same for both EPCs.
 */
#define WATCHPOINT_HISTORY_SUPPRESS_N_NEIGHBOURS 8
#else
#define WATCHPOINT_HISTORY_SUPPRESS_N_NEIGHBOURS 0
#endif

struct debug_info {
	unsigned long ra;
	unsigned long epc;
	unsigned long nhits;
};

/*
 * The last N unique accesses are stored in a ring buffer of size N.
 */
struct wp_history {
	struct debug_info infos[WATCHPOINT_HISTORY_LEN];

	unsigned int last_idx; /* If size > 0, the index of the last written element */
	unsigned int next_idx; /* The index of the next element to write. */
	unsigned int size;
};

/**
 */
struct _avm_wp {
	void *ref;
	void *expr;
	spinlock_t wp_lock;
	unsigned int real_exception_count;
	unsigned int exception_count;
	unsigned long addr;
	int mask;
	int hit;
	struct wp_history history;
	cpumask_t cpu_mask;
	int watch_nr;
#define wp_active   0x1
#define wp_inactive 0x2
	atomic_t active;
	enum _wp_type type;
	int panic_on_hit;
	wp_handler_func handler;
};

struct _avm_wp watchpoints[NR_CPUS][NR_WATCHPOINTS];

static void wp_move_local(struct _avm_wp *wp, uint32_t addr, enum _wp_type new_type);
static void wp_disable_local(struct _avm_wp *wp);
static void wp_enable_local(struct _avm_wp *wp);

static inline uint32_t compute_target_address(union mips_instruction *instr, struct pt_regs *regs);
static inline uint32_t compute_base_address(union mips_instruction *instr, uint32_t target_address);
static inline int32_t compute_access_width(union mips_instruction *instr, uint32_t target_address);
static union mips_instruction instr_at(uint32_t epc);
static uint32_t executing_epc(struct pt_regs *regs);

static int __init wp_dump_on_die(void);
static int handle_wp(struct _avm_wp *pwp, enum _wp_type status, int deferred, struct pt_regs *regs);

static char *print_active_state(struct _avm_wp *pwp, int act_cpu, char *txt, unsigned int txt_len);
static int wp_history_print_all(void);

static int check_addr_range(unsigned long addr1, unsigned long addr2, int mask);
static void __del_watchpoint(int watch_nr, const cpumask_t *cpu_mask);
static int get_expression_value(void *ref, enum _cond_type type, unsigned long *value);
static int get_expression_symbol(void *ref, enum _cond_type type, char *symbol, unsigned long *value);

/**
 */
static const char *name_wp_type(char *txt, unsigned int txtlen, enum _wp_type type)
{
	snprintf(txt, txtlen, "%s%s%s%s", type ==  0      ? "no support "   : "",
		      type & wp_type_write        ? "write+" : "",
		      type & wp_type_read         ? "read "  : "",
		      type & wp_type_instruction  ? "instruction "  : "");
	return txt;
}

/**
 */
static void write_c0_watchloX(int wd_nr, unsigned int value)
{
	switch (wd_nr) {
	case 0:
		write_c0_watchlo0(value);
		break;
	case 1:
		write_c0_watchlo1(value);
		break;
	case 2:
		write_c0_watchlo2(value);
		break;
	case 3:
		write_c0_watchlo3(value);
		break;
	case 4:
		write_c0_watchlo4(value);
		break;
	case 5:
		write_c0_watchlo5(value);
		break;
	case 6:
		write_c0_watchlo6(value);
		break;
	case 7:
		write_c0_watchlo7(value);
		break;
	}
}
/**
 */
static unsigned int read_c0_watchloX(int wd_nr)
{
	switch (wd_nr) {
	case 0:
		return read_c0_watchlo0();
	case 1:
		return read_c0_watchlo1();
	case 2:
		return read_c0_watchlo2();
	case 3:
		return read_c0_watchlo3();
	case 4:
		return read_c0_watchlo4();
	case 5:
		return read_c0_watchlo5();
	case 6:
		return read_c0_watchlo6();
	case 7:
		return read_c0_watchlo7();
	}
	return 0;
}

/**
 */
static void write_c0_watchhiX(int wd_nr, unsigned int value)
{
	switch (wd_nr) {
	case 0:
		write_c0_watchhi0(value);
		break;
	case 1:
		write_c0_watchhi1(value);
		break;
	case 2:
		write_c0_watchhi2(value);
		break;
	case 3:
		write_c0_watchhi3(value);
		break;
	case 4:
		write_c0_watchhi4(value);
		break;
	case 5:
		write_c0_watchhi5(value);
		break;
	case 6:
		write_c0_watchhi6(value);
		break;
	case 7:
		write_c0_watchhi7(value);
		break;
	}
}

/**
 */
static unsigned int read_c0_watchhiX(int wd_nr)
{
	switch (wd_nr) {
	case 0: return read_c0_watchhi0();
	case 1: return read_c0_watchhi1();
	case 2: return read_c0_watchhi2();
	case 3: return read_c0_watchhi3();
	case 4: return read_c0_watchhi4();
	case 5: return read_c0_watchhi5();
	case 6: return read_c0_watchhi6();
	case 7: return read_c0_watchhi7();
	}
	return 0;
}
#if 0
/**
 */
static void clear_watchpoint_per_cpu(void *info)
{
	struct _avm_wp *pwp = (struct _avm_wp *)info;
	unsigned long flags;

	spin_lock_irqsave(&pwp->wp_lock, flags);
	atomic_set(&pwp->active, wp_inactive);

	write_c0_watchhiX(pwp->watch_nr, read_c0_watchhiX(pwp->watch_nr) & 0x7);
	write_c0_watchloX(pwp->watch_nr, 0);

	pr_err("[%x]%s: %u: loX:%x hiX:%x\n", raw_smp_processor_id(), __func__,  pwp->watch_nr,
					read_c0_watchloX(pwp->watch_nr),
					read_c0_watchhiX(pwp->watch_nr));

	spin_unlock_irqrestore(&pwp->wp_lock, flags);
}
#endif

/**
 */
static void set_watchpoint_per_cpu(void *info)
{
	struct _avm_wp *pwp = (struct _avm_wp *)info;
	unsigned long flags;

	spin_lock_irqsave(&pwp->wp_lock, flags);
	atomic_set(&pwp->active, wp_active);

	write_c0_watchloX(pwp->watch_nr, (pwp->addr & 0xFFFFFFF8) | (pwp->type & 0x7));
	write_c0_watchhiX(pwp->watch_nr,
					   (1 << 30) /*--- global ---*/ |
					   (pwp->mask & (0x1FF << 3)) |
					   (read_c0_watchhiX(pwp->watch_nr) & 0x7));
#if 0
	{
	char txt[32];

	pr_err("[%x]%s: %u: %pS %s loX:%x hiX:%x\n", raw_smp_processor_id(), __func__,
				pwp->watch_nr, (void *)pwp->addr,
				name_wp_type(txt, sizeof(txt), pwp->type),
				read_c0_watchloX(pwp->watch_nr),
				read_c0_watchhiX(pwp->watch_nr));
	}
#endif
	spin_unlock_irqrestore(&pwp->wp_lock, flags);
}

/**
 */
static int check_type(int wd_nr, enum _wp_type type)
{
	unsigned int lo_reg;

	write_c0_watchloX(wd_nr, 7);
	mb();
	lo_reg = read_c0_watchloX(wd_nr);
	if ((lo_reg & type) == 0) {
		char txt[2][32];

		pr_err("%s: wrong type %x: (%s) for watch_nr=%u (%x %s)\n", __func__,
				type, name_wp_type(txt[0], sizeof(txt[0]), type),
				wd_nr,
				lo_reg, name_wp_type(txt[1], sizeof(txt[1]), lo_reg));
		return -1;
	}
	return 0;
}

/**
 */
static int lset_watchpoint_expression(void *expression, int watch_nr, const cpumask_t *cpu_mask)
{
	int cpu;

	/*--- vorhergehende austragen ---*/
	if ((watch_nr >= current_cpu_data.watch_reg_count) || (watch_nr >=  NR_WATCHPOINTS)) {
		pr_err("%s: unsupported watchpoint-number %u\n", __func__, watch_nr);
		return -1;
	}
	__del_watchpoint(watch_nr, cpu_mask);
	for_each_online_cpu(cpu) {
		if (cpumask_test_cpu(cpu, cpu_mask)) {
			struct _avm_wp *pwp = &watchpoints[cpu][watch_nr];

			condition_put(pwp->expr);                /*--- alten freigeben ---*/
			condition_get(expression);
			pwp->expr = expression;
			/*--- pr_err("%s: %p:%p\n", __func__, pwp,pwp->expr); ---*/
			pwp->exception_count      = 0;
			pwp->real_exception_count = 0;
			pwp->history.next_idx     = 0;
			pwp->history.last_idx     = 0;
			pwp->history.size         = 0;
		}
	}
	return 0;
}

/**
 * watch_nr: 0, 1 only instruction
 *           2, 3 only data
 * mask:     coressponding adressbits ignored (bit 11-3)
 * type:     bit0: write, bit1: read, bit2:instruction
 * cpu_mask:  0: all cpu's (instead choosed cpus's)
 * ref:      ref == NULL -> wp-instance for handler
 */
static int lset_watchpoint_handler(int watch_nr, unsigned long addr, unsigned long mask, enum _wp_type type,
			wp_handler_func wp_handler, void *ref, const cpumask_t *cpu_mask, int panic_on_hit)
{
	char txt[32];
	int cpu;

	if ((watch_nr >= current_cpu_data.watch_reg_count) || (watch_nr >= NR_WATCHPOINTS)) {
		pr_err("%s: unsupported watchpoint-number %u\n", __func__, watch_nr);
		return SET_WATCHPOINT_FAIL_NO_REG;
	}
	/*
	 * wir gehen mal davon aus, dass alle Cores identisch sind: nur aktuelle CPU testen
	 */
	if (check_type(watch_nr, type)) {
		return SET_WATCHPOINT_FAIL_WRONG_TYPE;
	}
	mb();
	/*--- vorhergehende austragen ---*/
	__del_watchpoint(watch_nr, cpu_mask);
	pr_err("%s: set watchpoint %u on addr %pS (mask=%lx) type=%x(%s) cpu_mask=%lx%s\n", __func__,
				watch_nr, (void *)addr, mask, type, name_wp_type(txt, sizeof(txt), type),
				cpumask_bits(cpu_mask)[0], panic_on_hit ? " (panic on hit)" : "");

	for_each_online_cpu(cpu) {
		if (cpumask_test_cpu(cpu, cpu_mask)) {
			struct _avm_wp *pwp     = &watchpoints[cpu][watch_nr];

			pwp->ref                = ref ? ref : pwp;
			pwp->addr               = addr;
			pwp->mask               = mask;
			pwp->type               = type;
			pwp->handler            = wp_handler;
			pwp->watch_nr           = watch_nr;
			pwp->exception_count    = 0;
			pwp->hit                = 0;
			pwp->panic_on_hit	= panic_on_hit;
			cpumask_copy(&pwp->cpu_mask, cpu_mask);
			smp_call_function_single(cpu, (smp_call_func_t)set_watchpoint_per_cpu, pwp, true);
		}
	}
	return SET_WATCHPOINT_SUCCESS;
}

/**
 */
static cpumask_t *cpu_id_to_cpumask(cpumask_t *cpu_mask, int cpu_id)
{
	if ((cpu_id >= num_possible_cpus()) || (cpu_id < 0)) {
		cpumask_setall(cpu_mask);
	} else {
		cpumask_clear(cpu_mask);
		cpumask_set_cpu(cpu_id, cpu_mask);
	}
	return cpu_mask;
}

/**
 */
int set_watchpoint(int watch_nr, unsigned long addr, int mask, enum _wp_type type, int cpu_id,
				wp_handler_func wp_handler, void *ref)
{
	cpumask_t cpu_mask;

	if (!wp_handler)
		ref = NULL;

	cpu_id_to_cpumask(&cpu_mask, cpu_id);
	lset_watchpoint_expression(NULL, watch_nr, &cpu_mask);
	return lset_watchpoint_handler(watch_nr, addr, mask, type, wp_handler, ref, &cpu_mask, 0);
}
EXPORT_SYMBOL(set_watchpoint);

/**
 */
int set_watchpoint_fatal(int watch_nr, unsigned long addr, int mask, enum _wp_type type, int cpu_id)
{
	cpumask_t cpu_mask;

	cpu_id_to_cpumask(&cpu_mask, cpu_id);
	lset_watchpoint_expression(NULL, watch_nr, &cpu_mask);
	return lset_watchpoint_handler(watch_nr, addr, mask, type, NULL /* no custom handler */, NULL, &cpu_mask, 1);
}
EXPORT_SYMBOL(set_watchpoint_fatal);

/**
 * delete all watch_nr corresponding with cpu_mask and also cpu_mask from wp-instance
 */
static void __del_watchpoint(int watch_nr, const cpumask_t *pcpu_mask)
{
	cpumask_t cpu_mask;
	struct _avm_wp *pwp;
	int cpu;

	/*--- pr_err("%s: %u cpu_mask=%lx\n", __func__, watch_nr, cpumask_bits(cpu_mask)[0]); ---*/
	if ((watch_nr >= current_cpu_data.watch_reg_count) || (watch_nr >=  NR_WATCHPOINTS)) {
		pr_err("%s: unsupported watchpoint-number %u\n", __func__, watch_nr);
		return;
	}
	cpumask_copy(&cpu_mask, pcpu_mask);
	/*--- markiere alle per mask gebundelten zum austragen ---*/
	for_each_online_cpu(cpu) {
		pwp = &watchpoints[cpu][watch_nr];
		if (cpumask_test_cpu(cpu, &cpu_mask)) {
			cpumask_clear(&pwp->cpu_mask);
			atomic_set(&pwp->active, wp_inactive);
		} else {
			/*--- auch wenn nicht gebundelt: austragen ---*/
			cpumask_andnot(&pwp->cpu_mask, &pwp->cpu_mask, &cpu_mask);
		}
	}
	cpu = get_cpu();
	/*--- im aktuellen CPU-Kontext austragen: ---*/
	pwp = &watchpoints[cpu][watch_nr];
	if (atomic_read(&pwp->active) == wp_inactive) {
		unsigned long flags;

		spin_lock_irqsave(&pwp->wp_lock, flags);
		/*--- falls aktuelle CPU real clearen ---*/
		write_c0_watchhiX(watch_nr, read_c0_watchhiX(watch_nr) & 0x7); /*--- Bit0-2: W1C ---*/
		write_c0_watchloX(watch_nr, 0);
		spin_unlock_irqrestore(&pwp->wp_lock, flags);
		/*--- pr_err("[%x]%s: cleared: hiX: %x lox%x\n", raw_smp_processor_id(), __func__,  read_c0_watchhiX(watch_nr), read_c0_watchloX(watch_nr)); ---*/
	}
	put_cpu();
}

/**
 */
static void reset_watchpoints_per_cpu(void *info)
{
	unsigned int watch_nr;

	for (watch_nr = 0; watch_nr <= min_t(int, current_cpu_data.watch_reg_count, NR_WATCHPOINTS); watch_nr++) {
		write_c0_watchloX(watch_nr, 0x0);
		write_c0_watchhiX(watch_nr, 0x7);
	}
}

/**
 * watchpoint-unit fuer Userland (ptrace) wieder nutzbar machen
 */
static void reset_all_watchpoints(void)
{
	unsigned int cpu, watch_nr;

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, (smp_call_func_t)reset_watchpoints_per_cpu, NULL, true);
		for (watch_nr = 0; watch_nr <= min_t(int, current_cpu_data.watch_reg_count, NR_WATCHPOINTS); watch_nr++) {
			struct _avm_wp *pwp     = &watchpoints[cpu][watch_nr];

			atomic_set(&pwp->active, 0);
		}
	}
}

/**
 */
void del_watchpoint(int watch_nr, int cpu_id)
{
	cpumask_t cpu_mask;

	__del_watchpoint(watch_nr, cpu_id_to_cpumask(&cpu_mask, cpu_id));
}
EXPORT_SYMBOL(del_watchpoint);

/**
 */
int watchpoint_busy(int watch_nr)
{
	return read_c0_watchloX(watch_nr);
}
EXPORT_SYMBOL(watchpoint_busy);

/**
 * Records an access in wp->history.
 * If the access has already been stored in the history, 0 is returned.
 * Otherwise, the function returns 1.
 */
static int wp_history_record(struct _avm_wp *wp, struct pt_regs *regs)
{
	unsigned int idx;
	unsigned long epc, ra;
	struct wp_history *wph;

	epc = executing_epc(regs);
	ra  = regs->regs[31];

	wph = &wp->history;

	/*
	 * Search for a previous occurrence of this access.
	 *
	 * We treat accesses as "new" if there is no entry in the history
	 * with the same ra or when the epc of the access is sufficiently
	 * different from previous occurrences.
	 */
	for (idx = 0; idx < wph->size; ++idx) {
		if (wph->infos[idx].ra == ra) {
			/*
			 * Number of instructions between saved epc and current epc.
			 */
			const size_t instr_dist = (epc >= wph->infos[idx].epc)
				? (epc - wph->infos[idx].epc) / 4
				: (wph->infos[idx].epc - epc) / 4;

			if (instr_dist <= WATCHPOINT_HISTORY_SUPPRESS_N_NEIGHBOURS) {
				/*
				 * The accesses are considered to be the same. When
				 * looking at the relevant code locations, the accesses
				 * that are masked here are easily spotted because they
				 * are located directly next to them.
				 */
				wph->infos[idx].nhits++;
				return 0;
			}
		}
	}

	if (wph->size == WATCHPOINT_HISTORY_LEN) {
		if (printk_ratelimit()) {
			pr_info("Watchpoint history is full, overwriting elements."
				"Consider increasing the history's size to silence this message.\n");
		}
	}

	/*
	 * Add the element, because it is not contained in the history yet.
	 */
	idx = wph->last_idx = wph->next_idx;

	wph->infos[idx].epc = epc;
	wph->infos[idx].ra  = ra;
	wph->infos[idx].nhits = 1;

	wph->next_idx = (wph->next_idx + 1) % WATCHPOINT_HISTORY_LEN;
	wph->size = min(wph->size + 1, (typeof(wph->size))WATCHPOINT_HISTORY_LEN);

	return 1;
}

static void wp_history_print(int cpu, int wp_nr)
{
	unsigned int idx;
	char statetxt[32];
	char txt[32];

	struct _avm_wp *wp = &watchpoints[cpu][wp_nr];
	struct wp_history *wph = &wp->history;

	pr_info("[cpu%d]: %u: %s %s addr: [<%08lx>] (%pS) mask=%x (%d history entries)\n",
		cpu, wp_nr,
		print_active_state(wp, cpu, statetxt, sizeof(statetxt)),
		name_wp_type(txt, sizeof(txt), wp->type),
		wp->addr,
		(void *)wp->addr,
		wp->mask,
		wph->size);

	for (idx = 0; idx < wph->size; ++idx) {
		pr_info(" %u: [<%08lx>] (%pS) from [<%08lx>] (%pS) with %lu hits\n",
			idx,
			wph->infos[idx].epc,
			(void *)wph->infos[idx].epc,
			wph->infos[idx].ra,
			(void *)wph->infos[idx].ra,
			wph->infos[idx].nhits);
	}
}

static int wp_history_print_all(void)
{
	int cpu, wp_num;
	bool header_printed = false;

	for (cpu = 0; cpu < num_possible_cpus(); ++cpu) {
		for (wp_num = 0; wp_num < NR_WATCHPOINTS; ++wp_num) {
			struct _avm_wp *wp = &watchpoints[cpu][wp_num];

			if (wp->hit) {
				/*
				 * Preserve log space: Print the header only when
				 * there are actual log entries.
				 */
				if (!header_printed) {
					pr_info("watchpoint history:\n");
					header_printed = true;
				}
				wp_history_print(cpu, wp_num);
			}
		}
	}

	return NOTIFY_DONE;
}

static int wp_dump_history(struct notifier_block *this, unsigned long ev, void *ptr)
{
	wp_history_print_all();
	return NOTIFY_DONE;
}

/*
 * Dump the watchpoint history if the kernel dies so that the debug logs contain
 * potentially useful debug information.
 */
static struct notifier_block wp_die_block = {
	.notifier_call = wp_dump_history,
};

static int __init wp_dump_on_die(void)
{
	return register_die_notifier(&wp_die_block);
}

early_initcall(wp_dump_on_die);

/**
 * ret: 0 watchpoint loeschen
 * ref muss auf _avm_wp zeigen
 */
static int handle_wp(struct _avm_wp *pwp, enum _wp_type status, int deferred, struct pt_regs *regs)
{
	char txt[32], oldlog_level;
	char condtxt[512];
	unsigned long value = 0;
	int new_access;

	condtxt[0] = 0;
	new_access = 0;
	pwp->exception_count++;

	condition_get(pwp->expr);
	if (pwp->expr) {
		if (condition_calculate(pwp->expr, regs, &value) || (value == 0)) {
			condition_put(pwp->expr);
			return 1;
		}
		condition_print(pwp->expr, condtxt, sizeof(condtxt));
	}
	condition_put(pwp->expr);

	new_access = wp_history_record(pwp, regs);
	pwp->hit = 1;

	if (pwp->handler) {
		return pwp->handler(pwp->ref, status, deferred, regs);
	} else if (new_access) {
		oldlog_level = console_loglevel;
		console_verbose();

		pr_err("--------------------------------------------------------------------------------\n"
				"WATCHPOINT%u occured (%s), deferred=%d epc=%pS %s%s%s\n", pwp->watch_nr,
				name_wp_type(txt, sizeof(txt), status),
				deferred, (void *)regs->cp0_epc,
				condtxt[0] ? "(" : "", condtxt, condtxt[0] ? " is true)" : "");
		show_registers(regs);
#if 0
		if (status & wp_type_write) {
			unsigned long start_addr, end_addr;
			unsigned long addr = read_c0_watchloX(pwp->watch_nr) & 0xFFFFFFF8;
			unsigned long mask = read_c0_watchhiX(pwp->watch_nr) & (0x1FF << 3);

			start_addr = addr & ~(mask | 0x7);
			end_addr   = start_addr | (mask | 0x7);
			addr = start_addr;
			pr_err("Memorydump:");
			while (addr <= end_addr) {
				pr_err("%08lx: %*ph\n", addr, (int)min(32UL, end_addr - addr + 4), (unsigned char *)addr);
				addr += 32;
			}
		}
#endif
		console_loglevel = oldlog_level;
	}
	return 1;
}

/**
 * Check whether a watchpoint `wp`, given the current CPU state
 * contained in `pt_regs`, accesses an address that is watched.
 *
 * MIPS processors can only set watchpoints on 8-byte aligned
 * addresses. This implementation however uses "virtual" watchpoints
 * by setting a hardware watchpoint on the properly aligned 8-byte address
 * and then deciding whether a given access really accesses the data structure
 * the user set a watchpoint on. If so, the watchpoint handler function
 * is invoked. Otherwise, the watchpoint is not reported to the user
 */
int wp_access_is_relevant(struct _avm_wp *wp, struct pt_regs *regs)
{
	uint32_t target_address, base_address;
	uint32_t epc;
	int32_t access_width;

	union mips_instruction instr;

	if (atomic_read(&wp->active) == 0) {
		return 0;
	}

	epc = executing_epc(regs);

	if (wp->type == wp_type_instruction) {
		return check_addr_range(epc, wp->addr, wp->mask);
	} else /* data watchpoint */ {
		instr = instr_at(epc);
		target_address = compute_target_address(&instr, regs);
		base_address = compute_base_address(&instr, target_address);
		access_width = compute_access_width(&instr, target_address);

		/*
		 * Bail on error: We don't know the exact size of the operand
		 */
		if (access_width == -1) {
			pr_info("Unable to get size of data load / store. Ignoring watchpoint...\n");
			return 0;
		}

		/*
		 * Does wp->addr overlap with target_address when considering the wp->mask?
		 * If not, does the last byte of target_address to be accessed overlap with
		 * wp->addr considering the wp->mask?
		 *
		 * Examples:
		 *	- wp->addr is 0xf3, wp->mask is 0x00 (only accesses to this single byte
		 *	  address are instrumented). Does an LH instruction with target address
		 *	  0xf2 access the instrumented byte?
		 *		-> Yes, because 0xf2 + 2 - 1 = 0xf3.
		 *	- wp->addr is 0xf2, wp->mask is 0x01 (accesses to the half-word are
		 *	  instrumented.). Does an LB instruction with target address 0xf1 access
		 *	  the instrumented address?
		 *		-> No, because 0xf1 + 1 - 1 < 0xf2.
		 */
		return check_addr_range(base_address, wp->addr, wp->mask)
			|| check_addr_range(base_address + access_width - 1, wp->addr, wp->mask);
	}
}

#define GENERAL_OPCODE(a)  (((a) >> 26) & 0x3F)
#define SPECIAL_OPCODE(a)  (((a) >> 0) & 0x3F)
#define REGIMM_OPCODE(a)   (((a) >> 16) & 0x1F)
/**
 */
static int is_branch_or_jump(unsigned int instruction)
{
	unsigned int s_op, g_op = GENERAL_OPCODE(instruction);

	switch (g_op) {
	case 0:
		s_op = SPECIAL_OPCODE(instruction);
		if ((s_op >= 8) && (s_op <= 9)) {
			/*--- JR, JALR ---*/
			return 1;
		}
		break;
	case 1:
		s_op = REGIMM_OPCODE(instruction);
		if ((s_op >= 0) && (s_op <= 4)) {
			/*--- BLTZ, BGEZ, BLTZL, BGEZL ---*/
			return 1;
		}
		if ((s_op >= 16) && (s_op <= 20)) {
			/*--- BLTZAL, BGEZAL, BLTZ, BGEZLAL ---*/
			return 1;
		}
		if ((s_op >= 28) && (s_op <= 29)) {
			/*--- BPOSGE32, BPOSGE64 ---*/
			return 1;
		}
		break;
	case  2 ... 7:
		/*--- J, JAL, BEQ, BNE, BLEZ, BGTZ ---*/
		return 1;
	case 20 ... 27:
		/*--- BEQL, BNEL, BLEZL, BGTZL ---*/
		return 1;
	}
	return 0;
}

/**
 * Returns the EPC of the next instruction.
 */
static uint32_t next_epc(struct pt_regs *regs)
{
	uint32_t old_epc, new_epc;
	mm_segment_t seg;

	seg = get_fs();
	old_epc = regs->cp0_epc;

	if (!user_mode(regs))
		set_fs(KERNEL_DS);

	WARN_ON(compute_return_epc(regs) < 0);

	set_fs(seg);

	new_epc = regs->cp0_epc;
	regs->cp0_epc = old_epc;

	return new_epc;
}

/**
 * Obtain the epc of the currently executing instruction.
 *
 * Note: This function properly handles branch delay slots,
 * where the epc does not point to the currently executing
 * instruction.
 */
static uint32_t executing_epc(struct pt_regs *regs)
{
	if (delay_slot(regs)) {
		return regs->cp0_epc + 0x4;
	}

	return regs->cp0_epc;
}

#define DBG_WP_DISPATCHER(args...)
/*--- #define DBG_WP_DISPATCHER(args...)     pr_info(args) ---*/
/**
 * da die Watchunit vor Ausfuehrung zuschlaegt, wird eine Endlosloop getriggert, wenn
 * man gleich neu aufsetzt
 * Loesung:     Watchpoint um 8 Byte weitersetzen und dann wieder zurueck
 * Probleme:   (1) ein branch oder jump fuehrt zu keinem Neuaufsetzen
 *                 -> schon abfangen beim Aufsetzen - is_branch_or_jump()
 *             (2) die zu überwachende Adresse muss Doubleword-aligned sein
 *             (3) nicht Anwenden falls Adress-Range überwacht wird
 */
static int workaround_for_instruction(struct _avm_wp *pwp, unsigned int loX)
{
	if (pwp->type & wp_type_instruction) {
		/*  No workaround currently possible if mask is specified */
		if (pwp->mask != 0) {
			return -1;
		}

		if ((loX & 0xFFFFFFF8) == ((pwp->addr) & 0xFFFFFFF8)) {
			/*
			 * Set a workaround watchpoint
			 */
			wp_move_local(pwp, ((pwp->addr + 8) & 0xFFFFFFF8), pwp->type);

			DBG_WP_DISPATCHER("%s: loX=%x hiX=%x", __func__, read_c0_watchloX(pwp->watch_nr),
					read_c0_watchhiX(pwp->watch_nr));
			return 0;
		} else {
			DBG_WP_DISPATCHER("%s: wk hit loX=%x hiX=%x %pS", __func__, read_c0_watchloX(pwp->watch_nr),
					read_c0_watchhiX(pwp->watch_nr),
					(void *)read_c0_epc());

			wp_enable_local(pwp);

			return 1;
		}
	}
	return 0;
}

static inline uint32_t sign_extend(uint32_t value, int bits)
{
	if (value & (1 << (bits - 1))) {
		value |= 0xffffffff << bits;
	}

	return value;
}

/**
 * Compute the address given to the instruction.
 *
 * We assume the instruction uses the I instruction format and is a load or
 * store instruction!
 */
static inline uint32_t compute_target_address(union mips_instruction *instr, struct pt_regs *regs)
{
	uint32_t addr;
	uint32_t offset;

	switch (instr->i_format.opcode) {
	case spec3_op:
		if (instr->dsp_format.func == lx_op) {
			offset = regs->regs[instr->dsp_format.index];
			addr = regs->regs[instr->dsp_format.base] + offset;
		}
		break;
	default:
		offset  = sign_extend((uint32_t)instr->i_format.simmediate, 16);
		addr    = regs->regs[instr->i_format.rs] + offset;
	}

	return addr;
}

/**
 * Compute the address a given instruction will act upon when passed the
 * given target address.
 *
 * This uses the same assumptions as compute_target_address.
 */
static inline uint32_t compute_base_address(union mips_instruction *instr, uint32_t target_address)
{
	switch (instr->i_format.opcode) {
	case lwr_op:
	case swr_op:
		target_address = ALIGN_DOWN(target_address, 4);
		break;
	}

	return target_address;
}

/**
 * Compute the width of the memory area a given instruction will act upon
 * when passed the given target address.
 *
 * This uses the same assumptions as compute_target_address.
 *
 * On unknown instructions this function returns -1.
 */
static inline int32_t compute_access_width(union mips_instruction *instr, uint32_t target_address)
{
	int32_t access_width;

	switch (instr->i_format.opcode) {
	case lb_op:
	case lbu_op:
	case sb_op:
		access_width = 1;
		break;
	case lh_op:
	case lhu_op:
	case sh_op:
		access_width = 2;
		break;
	case lw_op:
	case lwu_op:
	case sw_op:
		access_width = 4;
		break;
	case lwl_op:
	case swl_op:
		access_width = 4 - (target_address & 0x3);
		break;
	case lwr_op:
	case swr_op:
		access_width = (target_address & 0x3) + 1;
		break;
	case spec3_op:
		if (instr->dsp_format.func == lx_op) {
			switch (instr->dsp_format.op) {
			case lwx_op:
				access_width = 4;
				break;
			case lhx_op:
				access_width = 2;
				break;
			case lbux_op:
				access_width = 1;
				break;
			}
		}
		break;
	default:
		access_width = -1;
		break;
	}

	return access_width;
}

static union mips_instruction instr_at(uint32_t epc)
{
	union mips_instruction instr;

	instr.word = *(unsigned int *)epc;
	return instr;
}

/**
 * Emulate a load / store instruction that originated in kernel code
 * and triggered a data watchpoint.
 *
 * This function shall never be called if the original instruction
 * originated in userland!
 *
 * A function with the same name is included in unaligned.c.
 * However, that function only handles instructions that can fail
 * due to alignment issues.
 */
static int emulate_load_str_insn(struct pt_regs *regs)
{
	uint32_t target_address, base_address;
	int32_t access_width;

	union mips_instruction instr;

	instr = instr_at(executing_epc(regs));

	target_address = compute_target_address(&instr, regs);
	base_address = compute_base_address(&instr, target_address);
	access_width = compute_access_width(&instr, target_address);

	#define STORE_INST_2(inst, source, to, size, dummy) {           \
			asm (inst" %2, 0(%1)\n"                         \
				: "=m"(*(const char (*)[size])(dummy))  \
				: "r"((to)), "r"((source))              \
				); }

	#define STORE_INST(inst, source, to, size) STORE_INST_2(inst, source, to, size, to)

	#define LOAD_INST_2(inst, dest, from, size, dummy) {                        \
			asm (inst" %0, 0(%1)\n"                                     \
				: "+r"((dest))                                      \
				: "r"((from)), "m"(*(const char (*)[size])(dummy))  \
				); }

	#define LOAD_INST(inst, dest, from, size) LOAD_INST_2(inst, dest, from, size, from)

	switch (instr.i_format.opcode) {
	case lb_op:
		LOAD_INST("lb", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case lbu_op:
		LOAD_INST("lbu", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case lh_op:
		LOAD_INST("lh", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case lhu_op:
		LOAD_INST("lhu", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case lw_op:
	case lwu_op:
		LOAD_INST("lw", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case lwl_op:
		LOAD_INST("lwl", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case lwr_op:
		LOAD_INST_2("lwr", regs->regs[instr.i_format.rt], target_address,
			    access_width, base_address);
		break;
	/*
	 * Handle data stores
	 */
	case sb_op:
		STORE_INST("sb", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case sh_op:
		STORE_INST("sh", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case sw_op:
		STORE_INST("sw", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case swl_op:
		STORE_INST("swl", regs->regs[instr.i_format.rt], target_address, access_width);
		break;
	case swr_op:
		STORE_INST_2("swr", regs->regs[instr.i_format.rt], target_address,
			    access_width, base_address);
		break;

	/*
	 * Handle the SPEC3 format
	 */
	case spec3_op:
		if (instr.dsp_format.func == lx_op) {
			switch (instr.dsp_format.op) {
			case lwx_op:
				LOAD_INST("lw", regs->regs[instr.dsp_format.rd], target_address,
					  access_width);
				break;
			case lhx_op:
				LOAD_INST("lh", regs->regs[instr.dsp_format.rd], target_address,
					  access_width);
				break;
			case lbux_op:
				LOAD_INST("lbu", regs->regs[instr.dsp_format.rd], target_address,
					  access_width);
			}
		}
		break;
	default:
		pr_err("Cannot handle instruction at %pS: %08x during watchpoint emulation.\n",
			(void *)regs->cp0_epc, instr.word);
		return -1;
	}

	regs->cp0_epc = next_epc(regs);

	return 0;
}

/**
 * Check watchpoint <-> processor consistency.
 *
 * Returns whether the supplied watchpoint belongs to the current
 * processor.
 */
static inline int wp_cpu_consistent(struct _avm_wp *wp)
{
	int cpu, index;

	cpu = raw_smp_processor_id();
	index = wp - &watchpoints[cpu][0];
	return !(index < 0 || index >= NR_WATCHPOINTS);
}

/**
 * Sets the address of watchpoint `wp` to a new address
 * on the current CPU. Note that this new address is not
 * persisted to the `wp` data structure.
 */
static void wp_move_local(struct _avm_wp *wp, uint32_t addr, enum _wp_type new_type)
{
	unsigned long flags;

	WARN_ON(!wp_cpu_consistent(wp));

	spin_lock_irqsave(&wp->wp_lock, flags);

	write_c0_watchhiX(wp->watch_nr, read_c0_watchhiX(wp->watch_nr));
	write_c0_watchloX(wp->watch_nr, addr | (new_type & 0x7));

	spin_unlock_irqrestore(&wp->wp_lock, flags);
}

static void wp_disable_local(struct _avm_wp *wp)
{
	WARN_ON(!wp_cpu_consistent(wp));

	wp_move_local(wp, 0x00000000, 0x0);
}

static void wp_enable_local(struct _avm_wp *wp)
{
	unsigned long flags;

	WARN_ON(!wp_cpu_consistent(wp));

	spin_lock_irqsave(&wp->wp_lock, flags);

	write_c0_watchhiX(wp->watch_nr, read_c0_watchhiX(wp->watch_nr));
	write_c0_watchloX(wp->watch_nr, (wp->addr & 0xFFFFFFF8) | (wp->type & 0x7));

	spin_unlock_irqrestore(&wp->wp_lock, flags);
}

void local_wp_off(int watch_nr)
{
	write_c0_watchhiX(watch_nr, read_c0_watchhiX(watch_nr));
	write_c0_watchloX(watch_nr, 0);
}
EXPORT_SYMBOL(local_wp_off);

void local_wp_on(int watch_nr, unsigned long addr, enum _wp_type type)
{
	write_c0_watchhiX(watch_nr, read_c0_watchhiX(watch_nr));
	write_c0_watchloX(watch_nr, (addr & 0XFFFFFFF8) | (type & 0x7));
}
EXPORT_SYMBOL(local_wp_on);

/**
 * workaround_for_data - Emulate a data load / store to avoid executing the faulting
 * instruction again.
 *
 * This function returns -1 on failure and 0 on success.
 */
static int workaround_for_data(struct _avm_wp *pwp, struct pt_regs *regs)
{
	int status;

	/*  Correct watchpoint type? */
	if (!((pwp->type & wp_type_read)
		|| (pwp->type & wp_type_write))) {
		return 0;
	}

	wp_disable_local(pwp);
	status = emulate_load_str_insn(regs);
	wp_enable_local(pwp);

	return (status < 0) ? -1 : 0;
}

/**
 */
static int check_addr_range(unsigned long addr1, unsigned long addr2, int mask)
{
	return (addr1 & ~mask) == (addr2 & ~mask);
}

/**
 * wie kann man die Exceptions unterscheiden, wenn sie zeitgleich auftreten ?
 */
int avm_wp_dispatcher(struct pt_regs *regs)
{
	int act_cpu;
	struct _avm_wp *pwp = NULL;
	unsigned int hi_reg, lo_reg, cause;
	int handled = 0;
	int watch_nr;
	int status;
	int deferred = 0;
	int wp_keep = -1;

	act_cpu = raw_smp_processor_id();
	/*---  Clear WP (bit 22) bit of cause register so we don't loop forever. ---*/
	cause    = read_c0_cause();
	deferred = (cause & (1 < 22));
	cause   &= ~(1 << 22);
	write_c0_cause(cause);
	DBG_WP_DISPATCHER("[%x]%s cause=%x %pS %pS a0=%lx\n", act_cpu, __func__, cause, (void *)read_c0_epc(),
			(void *)regs->regs[31], regs->regs[4]);

	for (watch_nr = 0; watch_nr < NR_WATCHPOINTS; watch_nr++) {
		unsigned long flags;

		pwp = &watchpoints[act_cpu][watch_nr];

		spin_lock_irqsave(&pwp->wp_lock, flags);
		hi_reg = read_c0_watchhiX(watch_nr);
		lo_reg = read_c0_watchloX(watch_nr);
		status = hi_reg & 7;
		if (status) {

			if (atomic_read(&pwp->active) == 0) {
				spin_unlock_irqrestore(&pwp->wp_lock, flags);
				continue;
			}
			/*--- nur kernel-watchdog: ---*/
			pwp->real_exception_count++;
			write_c0_watchhiX(watch_nr, hi_reg);
			spin_unlock_irqrestore(&pwp->wp_lock, flags);
			handled = 1;

			DBG_WP_DISPATCHER("[%x]%s watch_nr=%u hi_reg=%x(%x) lo_reg=%x active=%u\n", act_cpu, __func__,
						watch_nr,
						hi_reg, read_c0_watchhiX(watch_nr),
						read_c0_watchloX(watch_nr), atomic_read(&pwp->active));
			if (atomic_read(&pwp->active) == wp_inactive) {
				/*--- exception aufgetreten aber schon als inactive markiert ---*/
				__del_watchpoint(watch_nr, &pwp->cpu_mask);
			} else if (wp_access_is_relevant(pwp, regs)) {
				if ((pwp->type & wp_type_instruction) &&
				   !check_addr_range(pwp->addr, regs->cp0_epc, pwp->mask | 0x7)) {
					DBG_WP_DISPATCHER("[%x]%s watch_nr=%u not the watching addr=%x epc=%lx\n",
								act_cpu, __func__, watch_nr,
								pwp->addr,
								regs->cp0_epc);
					continue;
				}
				if (cpumask_test_cpu(act_cpu, &pwp->cpu_mask)) {
					wp_keep = handle_wp(pwp, status, deferred, regs);

					if (pwp->panic_on_hit) {
						panic("kernel-watchpoint occur\n");
					}
				}
			}

			/*
			 * Watchpoint exceptions happen _before_ the instruction that causes the
			 * exception is executed. If we simply continue execution at this point,
			 * the same instruction will immediately trigger the watchpoint again.
			 * Hence, this code either disables the watchpoint or works around the
			 * problem.
			 */
			if (!wp_keep) {
				__del_watchpoint(watch_nr, &pwp->cpu_mask);
			} else {
				if (workaround_for_instruction(pwp, lo_reg) < 0 || workaround_for_data(pwp, regs) < 0) {
					pr_err("Failed to execute workaround for watchpoint. Removing watchpoint and continuing as usual...\n");
					__del_watchpoint(watch_nr, &pwp->cpu_mask);
				}
			}

			wp_keep = -1;
		} else {
			spin_unlock_irqrestore(&pwp->wp_lock, flags);
		}
	}
	return handled;
}
/**
 * complete: 1
 */
static char *find_word(char *string, char *match, int complete)
{
	int val;
	char *p;

	p = strstr(string, match);
	if (p) {
		if (p != string) {
			val = *(p-1);
			if ((val != ' ') && (val  != '\t') && (val != '\r') && (val != '\n')) {
				return NULL;
			}
		}
		if (complete) {
			int val = *(p + strlen(match));

			if ((val != 0) && (val != ' ') && ((val != '\t') && (val != '\r') && (val != '\n'))) {
				return NULL;
			}
		}
	}
	return p;
}
#define SKIP_UNTIL_SEP_OR_SIGN(txt, sign) { while (*txt && ((*txt != '\r') && (*txt != '\n') && (*txt != ' ') && (*txt != '\t') && (*txt != sign))) txt++; }
#define SKIP_UNTIL_SPACES(txt) { while (*txt && ((*txt != ' ') && (*txt != '\t'))) txt++; }
#define SKIP_SPACES(txt) { while (*txt && (((*txt == ' ') || (*txt == '\t')))) txt++; }
/*
 * ret: negval -> no match
 */
static int generic_param_parse(char *string, char *match, char **pos)
{
	char *p = string;
	int ret = -1;

	if (pos)
		*pos = "";
	p = find_word(string, match, 0);
	if (p) {
		p += strlen(match);
		if (*p == '0' && *(p+1) == 'x') {
			sscanf(p, "0x%x", &ret);
		} else if (isdigit(*p)) {
			sscanf(p, "%d", &ret);
		} else  {
			ret = 0;
		}
		SKIP_UNTIL_SPACES(p);
		SKIP_SPACES(p);
		if (pos)
			*pos = p;
	}
	return ret;
}

/**
 */
static unsigned long get_addr(char *p, char *endmarker)
{
	char condtxt[512];
	void *expression_handle;
	unsigned long value;
	unsigned long addr = 0;
	unsigned int len   = 0;

	if (endmarker) {
		len = endmarker - p;
	}
	expression_handle = condition_alloc(get_expression_value, get_expression_symbol, 50);
	if (condition_parse(expression_handle, NULL, p, 0)) {
		pr_err("invalid expression to get watchaddr: '%s'\n", p);
		goto fini;
	}
	if (condition_calculate(expression_handle, NULL, &value) || (value == 0)) {
		pr_err("can't resolve expression '%s' to get watchaddr\n",
				condition_print(expression_handle, condtxt, sizeof(condtxt)));
		goto fini;
	}
	addr = value;
fini:
	condition_free(expression_handle);
	return addr;
}

/**
 */
static int lproc_wp_set(char *string, void *priv __attribute__((unused)))
{
	char txtbuf[256];
	cpumask_t cpu_mask;
	unsigned long addr;
	unsigned int mask;
	char *expr, *endmarker = NULL;
	void *expression_handle = NULL;
	char *pos = NULL;
	int i, iwatch_nr, watch_nr, del_watch_nr, cpu, reset;

	if (find_word(string, "help", 1)) {
		pr_err("del[<num>] [cpu=<x>] <num>: 0-1 instruction 2-3 data\n");
		pr_err("reset\n");
		pr_err("iwatch[<num>] <addr> [mask=0x0-0xfff] [cpu=<x>] [if <expr>]\n");
		pr_err("watch[<num>]  <addr> [mask=0x0-0xfff] [cpu=<x>] (read+write-watch)\n");
		pr_err("rwatch[<num>] <addr> [mask=0x0-0xfff] [cpu=<x>] (read-watch)\n");
		pr_err("wwatch[<num>] <addr> [mask=0x0-0xfff] [cpu=<x>] (write-watch)\n");
		pr_err("<mask>, <cpu>, if <expr>: optional\n");
		pr_err("<addr>/<expr> as expression include register $a0, symbol-name, memory-access (*addr)\n");
		pr_err("<mask>: for instruction watchpoints:\n");
		pr_err("           start_addr = addr & ~(mask | 0x7); end_addr = start_addr | (mask | 0x7)\n");
		pr_err("<mask>: for data watchpoints:\n");
		pr_err("           start_addr = addr & ~mask; end_addr = start_addr | mask\n");
		pr_err("example: echo 'iwatch1 __kmalloc + 0x4 cpu=0 if $a0 < 10' > /proc/avm/wp\n");
		return 0;
	}
	reset = generic_param_parse(string, "reset", &pos);
	if (reset != -1) {
		/*--- fuer ptrace wieder nutzbar machen ---*/
		reset_all_watchpoints();
		return 0;
	}
	cpu = generic_param_parse(string, "cpu=", &pos);
	cpu_id_to_cpumask(&cpu_mask, cpu);

	if (pos) {
		endmarker = pos;
	}
	mask = generic_param_parse(string, "mask=", &pos);
	if (endmarker && (pos < endmarker)) {
		endmarker = pos;
	}
	if (mask == -1) {
		mask = 0;
	}
	del_watch_nr = generic_param_parse(string, "del", &pos);
	if (del_watch_nr != -1) {
		pr_err("delete watch%u on cpumask=%lx\n", del_watch_nr, cpumask_bits(&cpu_mask)[0]);
		__del_watchpoint(del_watch_nr, &cpu_mask);
		return 0;
	}
	expr = find_word(string, "if", 1);
	if (expr) {
		/*--- int len = strlen(expr) - 1; ---*/
		/*--- if (expr[len] != '\n') len = 0; ---*/
		endmarker = expr;
		expr += sizeof("if") - 1;
		/*--- len  -= sizeof("if") - 1; ---*/
		expression_handle = condition_alloc(get_expression_value, get_expression_symbol, 50);
		if (condition_parse(expression_handle, NULL, expr, 0)) {
			pr_err("invalid expr '%s'\n", expr);
			goto fini;
		}
		pr_err("expression: %s\n", condition_print(expression_handle, txtbuf, sizeof(txtbuf)));
	}
	if (mask && expression_handle) {
		pr_err("no support for mask in combination with condition\n");
		goto fini;
	}
	iwatch_nr = generic_param_parse(string, "iwatch", &pos);
	if (iwatch_nr != -1) {
		addr = get_addr(pos, endmarker) & ~0x3;
		if (mask) {
			unsigned int start_addr, end_addr;

			start_addr = addr & ~(mask | 0x7);
			end_addr   = start_addr | (mask | 0x7);
			pr_err("mask=0x%x<<3: extended addr-range for iwatch %08x - %8x\n",
					mask, start_addr, end_addr);
			addr = start_addr;
		} else if (addr & 0x7) {
			addr = (addr + 4) & ~0x7;
			pr_err("addr not aligned on doubleword - forward-correction: addr=%08lx", addr);
		}
		if (!virt_addr_valid(addr) && !is_vmalloc_or_module_addr((void *)addr)) {
			pr_err("invalid addr/symbol");
		} else if ((mask == 0) && (is_branch_or_jump(*((unsigned int *)(addr & ~0x7))) ||
					is_branch_or_jump(*((unsigned int *)((addr & ~0x7) + 4))))
				  ) {
			pr_err("can't install watchpoint on jump-instruction\n");
		} else {
			lset_watchpoint_expression(expression_handle, iwatch_nr, &cpu_mask);
			lset_watchpoint_handler(iwatch_nr, addr, mask, wp_type_instruction,
						NULL, NULL, &cpu_mask, 0);
		}
		goto fini;
	}
	for (i = 0; i < 3; i++) {
		watch_nr = generic_param_parse(string, i == 0 ? "watch"  :
					i == 1 ? "rwatch" : "wwatch", &pos);
		if (watch_nr != -1) {
			if (watch_nr < 2)
				watch_nr += 2;
			addr = get_addr(pos, endmarker) & ~0x3;
			if (!virt_addr_valid(addr) && !is_vmalloc_or_module_addr((void *)addr)) {
				pr_err("invalid addr/symbol");
			} else if (expression_handle) {
				pr_err("no support for expressions on data-watchpoint\n");
			} else {
				if (mask) {
					unsigned int start_addr, end_addr;

					start_addr = addr & ~mask;
					end_addr   = start_addr | mask;
					pr_err("mask=0x%x<<3: extended addr-range: %08x - %8x\n",
							mask, start_addr, end_addr);
					addr = start_addr;
				}

				lset_watchpoint_expression(expression_handle, watch_nr, &cpu_mask);
				lset_watchpoint_handler(watch_nr, addr, mask,
							i == 0 ? wp_type_read | wp_type_write :
							i == 1 ? wp_type_read : wp_type_write,
							NULL, NULL, &cpu_mask, 0);
			}
			goto fini;
		}
	}
fini:
	if (expression_handle)
		condition_free(expression_handle);
	return 0;
}

/**
 */
static int get_expression_value(void *ref, enum _cond_type type, unsigned long *value)
{
	unsigned long *addr = (unsigned long *)*value;
	struct pt_regs *pptregs = (struct pt_regs *)ref;

	/*--- pr_err("%s: type=%u value=%x\n", __func__, type, *value); ---*/
	switch (type) {
	case cond_type_register:
		if (pptregs) {
			*value = pptregs->regs[*value];
			break;
		}
		return -1;
	case cond_type_addr:
		if (virt_addr_valid(addr) || is_vmalloc_or_module_addr(addr)) {
			*value = *addr;
			break;
		}
		/*--- kein break ---*/
	default:
		pr_err("%s: type=%u addr=%p failed\n", __func__, type, addr);
		return -1;
	}
	return 0;
}

/**
 */
static const char * const map_register[] = {
					  "zero", "at", "v0", "v1",
					  "a0",   "a1", "a2", "a3",
					  "t0",   "t1", "t2", "t3",
					  "t4",   "t5", "t6", "t7",
					  "s0",   "s1", "s2", "s3",
					  "s4",   "s5", "s6", "s7",
					  "t8",   "t9", "k0", "k1",
					  "gp",   "sp", "s8", "ra",
					   NULL
};

/**
 */
static int map_regname_to_idx(const char *regname)
{
	unsigned int idx = 0;

	while (map_register[idx] && strcmp(regname, map_register[idx]))
		idx++;
	return map_register[idx] ? idx : -1;
}

/**
 * symbol == NULL: (nur bei cond_type_register) checke register-idx
 */
static int get_expression_symbol(void *ref, enum _cond_type type, char *symbol, unsigned long *value)
{
	int ret = -1;

	/*--- pr_err("%s: %s(%d) '%s' value=0x%lx\n", __func__, type == cond_type_register ? "register" : "addr", type, symbol, *value); ---*/
	switch (type) {
	case cond_type_register:
		if (symbol == NULL) {
			if (*value < ARRAY_SIZE(map_register) - 1) {
				ret = 0;
			}
			break;
		}
		ret = map_regname_to_idx(symbol);
		*value = ret;
		break;
	default:
		*value = (unsigned long)kallsyms_lookup_name(symbol);
		if (*value) {
			/*--- pr_err("%s: symbol '%s'to  value=%lx\n", __func__, symbol, *value); ---*/
			ret = 0;
		}
	}
	if (ret < 0)
		pr_err("%s: type=%d '%s' value=0x%lx failed\n", __func__, type, symbol, *value);
	return ret < 0 ? ret : 0;
}

#define snprintf_add(ptxt, txtlen, args...)	do {					\
	if (ptxt == NULL) {								\
		pr_err(args);								\
	} else {									\
		int local_add_len;							\
											\
		local_add_len = snprintf(ptxt, txtlen, args);				\
		if (local_add_len > 0) {						\
			int tail = min_t(int, txtlen, local_add_len);			\
											\
			(ptxt) += tail, (txtlen) -= tail;				\
		}									\
	}										\
	} while (0)

/**
 */
static char *print_exception_count(int watch_nr, char *txt, unsigned int txt_len)
{
	char *start_txt = txt;
	int cpu;

	for_each_online_cpu(cpu) {
		struct _avm_wp *pwp = &watchpoints[cpu][watch_nr];

		/*--- if (cpumask_test_cpu(cpu, &pwp->cpu_mask)) { ---*/
		snprintf_add(txt, txt_len, "%u/", pwp->exception_count);
		/*--- } ---*/
	}
	if (txt > start_txt) {
		*(txt-1) = 0;
	}
	return start_txt;
}

/**
 */
static char *print_active_state(struct _avm_wp *pwp, int act_cpu, char *txt, unsigned int txt_len)
{

	if (pwp->hit) {
		snprintf(txt, txt_len, "hit on cpu%u", act_cpu);
	} else if (atomic_read(&pwp->active) == wp_active) {
		snprintf(txt, txt_len, "active on cpumask=0x%lx", cpumask_bits(&pwp->cpu_mask)[0]);
	} else {
		snprintf(txt, txt_len, "off");
	}
	return txt;
}

/**
 */
static void lproc_wp_get(struct seq_file *seq, void *priv)
{
	struct _avm_wp *pwp, *lwp;
	char txt[32];
	char condtxt[512];
	char counttxt[128];
	char statetxt[32];
	int cpu, local_cpu, watch_nr;
	int idx;
	bool should_print;

	for (watch_nr = 0; watch_nr < NR_WATCHPOINTS; watch_nr++) {
		cpumask_t cpu_print_mask;

		cpumask_clear(&cpu_print_mask);
		for_each_online_cpu(cpu) {
			pwp = &watchpoints[cpu][watch_nr];
			should_print = false;

			if (!cpumask_test_cpu(cpu, &pwp->cpu_mask) || cpumask_test_cpu(cpu, &cpu_print_mask)) {
				continue;
			}

			/* Check whether the watchpoint was hit or is active this CPU */
			for_each_cpu(local_cpu, &pwp->cpu_mask) {
				struct _avm_wp *lwp;

				lwp = &watchpoints[local_cpu][watch_nr];

				if ((atomic_read(&lwp->active) == wp_active) ||
				   ((atomic_read(&lwp->active) == wp_inactive) && lwp->hit)) {
					should_print = true;
					break;
				}
			}

			if (!should_print) {
				continue;
			}

			condtxt[0] = 0;
			counttxt[0] = 0;
			condition_get(pwp->expr);
			if (pwp->expr) {
				condition_print(pwp->expr, condtxt, sizeof(condtxt));
			}
			condition_put(pwp->expr);

			seq_printf(seq, "%u: %s %s addr: [<%08lx>] (%pS) mask=%x '%s' %s exceptions(%s)\n",
					watch_nr,
					print_active_state(pwp, cpu, statetxt, sizeof(statetxt)),
					name_wp_type(txt, sizeof(txt), pwp->type),
					pwp->addr,
					(void *)pwp->addr,
					pwp->mask,
					condtxt, condtxt[0] && pwp->hit ? " == true" : "        ",
					print_exception_count(watch_nr, counttxt, sizeof(counttxt)));

			/* Output history */
			for_each_cpu(local_cpu, &pwp->cpu_mask) {
				lwp = &watchpoints[local_cpu][watch_nr];

				for (idx = 0; idx < lwp->history.size; ++idx) {
					seq_printf(seq, " [cpu%d]: [<%08lx>] (%pS) from [<%08lx>] (%pS) with %lu hits\n",
						local_cpu,
						lwp->history.infos[idx].epc, (void *)lwp->history.infos[idx].epc,
						lwp->history.infos[idx].ra, (void *)lwp->history.infos[idx].ra,
						lwp->history.infos[idx].nhits);
				}
			}

			cpumask_or(&cpu_print_mask, &cpu_print_mask, &pwp->cpu_mask);
		}
	}
}

/**
 */
int __init avm_wp_init(void)
{
	int cpu, watch_nr;

	add_simple_proc_file("avm/wp", lproc_wp_set, lproc_wp_get, NULL);
	for (cpu = 0 ; cpu < num_possible_cpus(); cpu++) {
		for (watch_nr = 0; watch_nr < NR_WATCHPOINTS; watch_nr++) {
			struct _avm_wp *pwp = &watchpoints[cpu][watch_nr];

			spin_lock_init(&pwp->wp_lock);
		}
	}
	return 0;
}
late_initcall(avm_wp_init);

#endif
