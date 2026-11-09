// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "[avm_console] " fmt

#include <linux/module.h>
#include <linux/console.h>
#include <linux/kmsg_dump.h>

#include <avm/sammel/debug.h>

#ifndef CON_MUTED
#error AVM extension CON_MUTED is missing
#endif

static void avm_console_write(struct console *con, const char *s, unsigned int c)
{
	avm_debug_console_write(s, c);
}

static struct console avm_console = {
	.name = "avm",
	.write = avm_console_write,
	.flags = CON_ENABLED | CON_MUTED | CON_ANYTIME,
	.index = 0,
};

static int __init avm_console_init(void)
{
	register_console(&avm_console);

	return 0;
}
postcore_initcall(avm_console_init);

int printk_avm_console_bend(unsigned int activate)
{
	static int is_bend;

	struct console *con;
	int ret = is_bend;
	int locked = 1;

	/*
	 * In normal context we can simply take the console_lock and be safe in
	 * when working on console_drivers and the flags.o
	 *
	 * However during panic we're inside an avm_rte context and may not be
	 * able to take this lock. But it is crucial that we "unbend" the
	 * console in this case to make output appear on the serial for
	 * debugging.
	 */
	if (oops_in_progress) {
		if (!console_trylock()) {
			pr_warn("Could not get console log, bend anyway...\n");
			locked = 0;
		}
	} else
		console_lock();

	/*
	 * First we mute/unmute all tty devices
	 */
	for_each_console(con) {
		int index;

		if (!con->device)
			continue;

		if (!con->device(con, &index))
			continue;

		if (activate)
			console_mute_nolock(con);
		else
			console_unmute_nolock(con);
	}

	/*
	 * Now mute/unmute the avm console
	 */
	if (activate)
		console_unmute_nolock(&avm_console);
	else
		console_mute_nolock(&avm_console);

	is_bend = activate;

	if (locked)
		console_unlock();

	return ret;
}
EXPORT_SYMBOL(printk_avm_console_bend);

asmlinkage int printk_linux(const char *fmt, ...)
{
	va_list args;
	int r, old;

	old = printk_avm_console_bend(0);

	va_start(args, fmt);
	r = vprintk_emit(0, -1, NULL, 0, fmt, args);
	va_end(args);

	printk_avm_console_bend(old);

	return r;
}
EXPORT_SYMBOL(printk_linux);

bool kmsg_dump_get_buffer_nocontext(bool syslog, char *buf, size_t size,
				    size_t *len)
{
	struct kmsg_dumper _dumper, *dumper;

	dumper = &_dumper;
	/* initialize iterator with data about the stored records */
	dumper->active = true;
	kmsg_dump_rewind(dumper);

	return kmsg_dump_get_buffer(dumper, 0, buf, size, len);
}
