/*
 *
 *   Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de>
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
 *
 *   mips-function for BUG() about bug_table
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <asm/avm_enh/bugtable.h>

#ifdef CONFIG_BUG_EXTRA_INFO
extern unsigned long __start___bug_debug_table;
extern unsigned long __stop___bug_debug_table;

/*--- #define DEBUG_BUGINFO ---*/

#if defined(DEBUG_BUGINFO)
#define DBG_TRC(args...) pr_info(args)
unsigned int kernel_buglist;
#else
#define DBG_TRC(args...) no_printk(args)
#endif

#define MAX_BUG_TABLES 40
struct bug_debug_tables {
	char *name;
	struct bug_debug_table_entry *start;
	struct bug_debug_table_entry *stop;
} bug_debug_table[MAX_BUG_TABLES] = {
	[0] = {
		.name  = "kernel",
		.start = (struct bug_debug_table_entry *)&__start___bug_debug_table,
		.stop  = (struct bug_debug_table_entry *)&__stop___bug_debug_table
	}
};

__asm__("	.section	__bug_debug_table, \"a\"\n"
	"	.previous			\n");

#if defined(DEBUG_BUGINFO)
/**
 */
static void display_bug_table(struct bug_debug_tables *T)
{
	const struct bug_debug_table_entry *e;
	unsigned int i = 0;

	pr_err("%s: '%s' num_entries=%u\n", __func__, T->name, (T->stop - T->start));
	for (e = T->start; e < T->stop; e++) {
		pr_err("TABLE[%02u] %p BUG%s(%s) at function '%s' line: %d file: %s\n",
		       i++, e,
		       e->condition ? "_ON" : "",
		       e->condition ? e->condition : "",
		       e->functionname,
		       e->line, e->filename);
		if (i > 500) {
			pr_err("...\n");
			break;
		}
	}
}
#endif /* DEBUG_BUGINFO */

/**
 */
void register_bug_debug_table(char *name, unsigned long start,
			      unsigned long end)
{
	unsigned int i;

#if defined(DEBUG_BUGINFO)
	if (kernel_buglist == 0) {
		kernel_buglist = 1;
		display_bug_table(&bug_debug_table[0]);
	}
#endif
	for (i = 0; i < MAX_BUG_TABLES; i++) {
		struct bug_debug_tables *T = &bug_debug_table[i];

		if (T->name == NULL) {
			DBG_TRC("[%s][%02u] name='%s' 0x%lx - 0x%lx\n",
				__func__,
				i, name, start, end);
			T->name = name;
			T->start = (struct bug_debug_table_entry *)start;
			T->stop = (struct bug_debug_table_entry *)end;
#if defined(DEBUG_BUGINFO)
			display_bug_table(T);
#endif
			return;
		}
	}
	pr_err("%s: error: '%s' not allocated!\n", __func__, name);
}
EXPORT_SYMBOL(register_bug_debug_table);

/**
 */
void release_bug_debug_table(char *name)
{
	unsigned int i;

	DBG_TRC("%s: '%s'\n", __func__, name);
	for (i = 0; i < MAX_BUG_TABLES; i++) {
		struct bug_debug_tables *T = &bug_debug_table[i];

		if (T->name == name) {
			T->name = NULL;
			T->start = (struct bug_debug_table_entry *)0UL;
			T->stop = (struct bug_debug_table_entry *)0UL;
			return;
		}
	}
	pr_err("%s: warning: '%s' not found! (driver maybe bugfree)\n", __func__, name);
}
EXPORT_SYMBOL(release_bug_debug_table);

/**
 */
static const struct bug_debug_table_entry *search_bug_debug_tables(unsigned long addr)
{
	const struct bug_debug_table_entry *e;
	unsigned int i;

	/*--- pr_err("[%s] addr 0x%lx %s", __FUNCTION__, addr, addr & 0x1 ? "(mips16)" : ""); ---*/

	addr &= ~0x1;

	for (i = 0; i < MAX_BUG_TABLES; i++) {
		struct bug_debug_tables *T = &bug_debug_table[i];
		/*--- pr_err("[%s] '%s' search from 0x%p to 0x%p\n", __FUNCTION__, T->name, T->start, T->stop); ---*/

		if (T->name == NULL)
			continue;

		for (e = T->start; e < T->stop; e++) {
			/*--- if(i == 1) ---*/
				/*--- pr_err("[%s] '%s' addr 0x%lx  e->addr 0x%lx\n", __FUNCTION__, T->name, addr, e->addr); ---*/
			if (e->addr + 4 == addr) {
				/*--- pr_err("[%s] found: '%s' addr 0x%lx + 4 == e->addr 0x%lx\n", __FUNCTION__, T->name, addr, e->addr); ---*/
				return e;
			}
			if (e->addr == addr) {
				/*--- pr_err("[%s] found: '%s' addr 0x%lx == e->addr 0x%lx\n", __FUNCTION__, T->name, addr, e->addr); ---*/
				return e;
			}
		}
	}
	return NULL;
}
#endif /* CONFIG_BUG_EXTRA_INFO */

/**
 */
void show_bug_by_bugtable(struct pt_regs *regs)
{
#ifdef CONFIG_BUG_EXTRA_INFO
	const struct bug_debug_table_entry *bug_info = search_bug_debug_tables(regs->cp0_epc);

	if (bug_info) {
		pr_err("BUG%s(%s) at function '%s' line: %d file: %s\n",
		       bug_info->condition ? "_ON" : "",
		       bug_info->condition ? bug_info->condition : "",
		       bug_info->functionname, bug_info->line, bug_info->filename);
	} else {
		pr_err("BUG() no bug_debug_table_entry found\n");
	}
#endif
}
