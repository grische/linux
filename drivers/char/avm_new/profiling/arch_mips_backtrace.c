// SPDX-License-Identifier: GPL-2.0+

/**
 * Backtrace
 */
#include <asm/sections.h>
#include <asm/inst.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>
#include <linux/rculist.h>
#include <linux/moduleloader.h>
#include <linux/version.h>

#if defined(CONFIG_KALLSYMS)
//#include <linux/version.h>
//#include <linux/proc_fs.h>
//#include <linux/timer.h>
//#include <linux/sched.h>
//#include <linux/fs.h>
//#include <linux/slab.h>
//#include <linux/avm_profile.h>
//#include <linux/avm_debug.h>
//#include <asm/mipsregs.h>
//#include <asm/mipsmtregs.h>
//#include <asm/mach_avm.h>
#include <asm/ptrace.h>

/**
 */
//ORINGINAL: mips_frame_info [arch/mips/kernel/process.c +207]
struct mips_frame_info {
	void            *func;
	unsigned long   func_size;
	int             frame_size;
	int             pc_offset;
};

/**
 * ORIGINAL: is_kernel [kernel/kallsyms.c +70]
 */
static inline int is_kernel(unsigned long addr)
{
	//that will only work with CONFIG_KALLSYMS_ALL = 1//
	if (addr >= (unsigned long)_stext && addr <= (unsigned long)_end)
		return 1;
	return 0;
}

/**
 */
extern const unsigned long kallsyms_num_syms
	__attribute__((weak, section(".rodata")));
extern const unsigned long kallsyms_addresses[] __attribute__((weak));

/**
 * ORIGINAL: get_symbol_pos [kernel/kallsyms.c +214]
 */
static unsigned long get_symbol_pos(unsigned long addr)
{
	unsigned long symbol_start = 0, symbol_end = 0;
	unsigned long i, low, high, mid;

	/* This kernel should never had been booted. */
	BUG_ON(!kallsyms_addresses);

	/* Do a binary search on the sorted kallsyms_addresses array. */
	low = 0;
	high = kallsyms_num_syms;

	while (high - low > 1) {
		mid = low + (high - low) / 2;
		if (kallsyms_addresses[mid] <= addr)
			low = mid;
		else
			high = mid;
	}

	/*
	 * Search for the first aliased symbol. Aliased
	 * symbols are symbols with the same address.
	 */
	while (low && kallsyms_addresses[low - 1] == kallsyms_addresses[low])
		--low;

	symbol_start = kallsyms_addresses[low];

	/* Search for next non-aliased symbol. */
	for (i = low + 1; i < kallsyms_num_syms; i++) {
		if (kallsyms_addresses[i] > symbol_start) {
			symbol_end = kallsyms_addresses[i];
			break;
		}
	}

	/* If we found no next symbol, we use the end of the section. */
	if (!symbol_end) {
		if (addr >= (unsigned long)_sinittext &&
		    addr <= (unsigned long)_einittext)
			symbol_end = (unsigned long)_einittext;
		else
			symbol_end = (unsigned long)_end;
	}

	//return the offset
	return addr - symbol_start;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 101)
/**
 */
static const char *symname(struct mod_kallsyms *kallsyms, unsigned int symnum)
{
	return kallsyms->strtab + kallsyms->symtab[symnum].st_name;
}

/**
 * ORIGINAL: get_ksymbol [kernel/module.c +3520]
 */
static unsigned long get_ksymbol(struct module *mod, unsigned long addr)
{
	unsigned int i, best = 0;
	struct mod_kallsyms *kallsyms = rcu_dereference_sched(mod->kallsyms);

	/* Scan for closest preceding symbol. (ELF starts real symbols at 1). */
	for (i = 1; i < kallsyms->num_symtab; i++) {
		if (kallsyms->symtab[i].st_shndx == SHN_UNDEF)
			continue;

		/* We ignore unnamed symbols: they're uninformative * and inserted at a whim. */
		if (*symname(kallsyms, i) == '\0')
			continue;

		if (kallsyms->symtab[i].st_value <= addr &&
		    kallsyms->symtab[i].st_value >
			    kallsyms->symtab[best].st_value)
			best = i;
#if 0
		/* We ignore unnamed symbols: they're uninformative and inserted at a whim. */
		if (kallsyms->symtab[i].st_value <= addr
			&& kallsyms->symtab[i].st_value > kallsyms->symtab[best].st_value
			&& *(mod->strtab + mod->symtab[i].st_name) != '\0'
			&& !is_arm_mapping_symbol(mod->strtab + mod->symtab[i].st_name))
			best = i;
#endif
	}

	if (!best)
		return 0;

	return addr - kallsyms->symtab[best].st_value;
}
#else
static unsigned long get_ksymbol(struct module *mod, unsigned long addr)
{
	unsigned int i, best = 0;

	/* Scan for closest preceding symbol. (ELF starts real symbols at 1). */
	for (i = 1; i < mod->num_symtab; i++) {
		if (mod->symtab[i].st_shndx == SHN_UNDEF)
			continue;

		/* We ignore unnamed symbols: they're uninformative and inserted at a whim. */
		if (mod->symtab[i].st_value <= addr &&
		    mod->symtab[i].st_value > mod->symtab[best].st_value &&
		    *(mod->strtab + mod->symtab[i].st_name) != '\0')
			// && !is_arm_mapping_symbol(mod->strtab + mod->symtab[i].st_name))
			best = i;
	}

	if (!best)
		return 0;

	return addr - mod->symtab[best].st_value;
}
#endif

/**
 * ORIGINAL: kallsyms_lookup_size_offset [kernel/kallsyms.c +274]
 */
static inline int kallsyms_lookup_offset(unsigned long addr,
					 unsigned long *offset)
{
	struct module *mod = NULL;

	//alles was nicht im Kernel ist interessiert uns erstmal nicht
	if (is_kernel(addr)) {
		*offset = get_symbol_pos(addr);
		return 1;
	}
	mod = __module_address(addr);
	if (mod) { //falls wir ein modul gefunden haben müssen wir den offset setzen!
		*offset = get_ksymbol(mod, addr);
		return 1;
	}
	return 0;
}

/**
 * ORIGINIAL: is_ra_save_ins [arch/mips/kernel/process.c +217]
 */
static inline int is_ra_save_ins(union mips_instruction *ip)
{
	/* sw / sd $ra, offset($sp) */
	return (ip->i_format.opcode == sw_op || ip->i_format.opcode == sd_op) &&
	       ip->i_format.rs == 29 && ip->i_format.rt == 31;
}

/**
 * ORIGINIAL: is_jump_ins [arch/mips/kernel/process.c +257]
 */
static inline int is_jump_ins(union mips_instruction *ip)
{
	if (ip->j_format.opcode == j_op)
		return 1;
	if (ip->j_format.opcode == jal_op)
		return 1;
	if (ip->r_format.opcode != spec_op)
		return 0;
	return ip->r_format.func == jalr_op || ip->r_format.func == jr_op;
}

/**
 * ORIGINIAL: is_sp_move_ins [arch/mips/kernel/process.c +291]
 */
static inline int is_sp_move_ins(union mips_instruction *ip)
{
	/* addiu/daddiu sp,sp,-imm */
	if (ip->i_format.rs != 29 || ip->i_format.rt != 29)
		return 0;
	if (ip->i_format.opcode == addiu_op || ip->i_format.opcode == daddiu_op)
		return 1;
	return 0;
}

/**
 * ORINGINAL: get_frame_info [arch/mips/kernel/process.c +323]
 */
static int get_frame_info(struct mips_frame_info *info)
{
	union mips_instruction *ip = info->func;
	unsigned int max_insns =
		info->func_size / sizeof(union mips_instruction);
	unsigned int i;

	info->pc_offset = -1;
	info->frame_size = 0;

	if (!ip)
		goto err;

	if (max_insns == 0)
		max_insns = 128U; /* unknown function size */
	max_insns = min(128U, max_insns);

	for (i = 0; i < max_insns; i++, ip++) {
		if (is_jump_ins(ip))
			break;
		if (!info->frame_size) {
			if (is_sp_move_ins(ip)) {
				info->frame_size = -ip->i_format.simmediate;
			}
			continue;
		}
		if (info->pc_offset == -1 && is_ra_save_ins(ip)) {
			info->pc_offset =
				ip->i_format.simmediate / sizeof(long);
			break;
		}
	}
	if (info->frame_size && info->pc_offset >= 0) /* nested */
		return 0;
	if (info->pc_offset < 0) /* leaf */
		return 1;
	/* prologue seems boggus... */
err:
	return -1;
}

extern void ret_from_irq(void);
extern void ret_from_exception(void);

/**
 * ORIGNIAL: unwind_stack_by_address [arch/mips/kernel/process.c +458]
 */
unsigned int _arch_mips_backtrace(unsigned long stack_page, unsigned int *sp,
				  unsigned int pc, unsigned int *lr)
{
	struct mips_frame_info info;
	unsigned long ofs;
	int leaf;

	/*
	 * If we reached the bottom of interrupt context,
	 * return saved pc in pt_regs.
	 */
	if (pc == (unsigned long)ret_from_irq ||
	    pc == (unsigned long)ret_from_exception) {
		struct pt_regs *regs;

		if (*sp >= stack_page &&
		    *sp + sizeof(*regs) <= stack_page + THREAD_SIZE - 32) {
			regs = (struct pt_regs *)*sp;
			pc = regs->cp0_epc;
			if (__kernel_text_address(pc)) {
				*sp = regs->regs[29];
				*lr = regs->regs[31];
				return pc;
			}
		}
		return 0;
	}

	//alles was nicht im Kernel ist interessiert uns nicht
	if (!kallsyms_lookup_offset(pc, &ofs))
		return 0;

	/*
	 * Return ra if an exception occurred at the first instruction
	 */
	if (unlikely(ofs == 0)) {
		//		pc = *ra;
		//		*ra = 0;
		//		return pc;
		return 0;
	}

	info.func = (void *)(pc - ofs);
	info.func_size = ofs; /* analyze from start to ofs */
	leaf = get_frame_info(&info);
	if (leaf < 0)
		return 0;

	if (*sp < stack_page ||
	    *sp + info.frame_size > stack_page + THREAD_SIZE - 32)
		return 0;
	if (leaf)
		/*
		 * For some extreme cases, get_frame_info() can
		 * consider wrongly a nested function as a leaf
		 * one. In that cases avoid to return always the
		 * same value.
		 */
		pc = pc != *lr ? *lr : 0;
	else {
		pc = ((unsigned long *)(*sp))[info.pc_offset];
	}

	*sp += info.frame_size;
	*lr = 0;
	return __kernel_text_address(pc) ? pc : 0;
}

/**
 */
void arch_mips_backtrace(unsigned int sp, unsigned int pc, unsigned int lr,
			 unsigned int *bt, int size_bt)
{
	unsigned long stack_page;
	unsigned int ra;
	int i;
	unsigned long first_ra = lr;
	unsigned int first_sp = sp;

	stack_page = sp & ~THREAD_MASK;
	if (!stack_page)
		return;

	ra = lr;
	for (i = 0; i < size_bt; i++) {
		if (pc != 0) {
			pc = _arch_mips_backtrace(stack_page, &sp, pc, &ra);
			if ((first_ra) && (pc == 0)) {
				pc = first_ra;
				sp = first_sp;
				pc = _arch_mips_backtrace(stack_page, &sp, pc,
							  &ra);
			}
			first_ra = 0;
			bt[i] = pc;
		} else {
			return;
		}
	}
	return;
}
#endif /*--- #if defined(CONFIG_KALLSYMS) ---*/
