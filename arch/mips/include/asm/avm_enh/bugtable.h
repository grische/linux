#ifndef __bugtable_h__
#define __bugtable_h__

#ifdef CONFIG_BUG_EXTRA_INFO

#include <linux/compiler.h>
#include <asm/sgidefs.h>
#include <asm/ptrace.h>

struct bug_debug_table_entry {
	unsigned long addr;
	char *filename;
	unsigned int line;
	char *functionname;
	char *condition;
};

extern void register_bug_debug_table(char *name, unsigned long start,
				     unsigned long end) __attribute__((weak));
extern void release_bug_debug_table(char *name) __attribute__((weak));

/**
 * display bug by bugtable
 */
void show_bug_by_bugtable(struct pt_regs *regs);

#endif /*--- #ifdef CONFIG_BUG_EXTRA_INFO ---*/
#endif /*--- #ifndef __bugtable_h__ ---*/
