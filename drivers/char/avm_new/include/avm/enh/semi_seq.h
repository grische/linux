/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __AVM_SEMI_SEQ_H__
#define __AVM_SEMI_SEQ_H__

#include <linux/module.h>
#include <linux/seq_file.h>

/*
 * Semi Seq File
 *
 * The semi seq file initializes a seq_file structure that can be used to
 * simulate a seq_file with output to printk.
 *
 * To do this, it needs a small buffer and calls to sseq_flush that does
 * the actual output. A sseq_flush should be calles approx after every \n
 */

struct semi_seq {
	struct seq_file file;

	const char *lvl;
};

/*
 * Initializes a new semi seq file. This function only uses the supplied buffer
 * and does not do any allocations. It can be safely called from any context.
 */
extern struct seq_file *sseq_create(struct semi_seq *sseq, const char *lvl,
				    char *buf, size_t size);

/*
 * Flush all complete lines from the seq_file to printk.
 *
 * If the seq_file is not a semi seq_file the call is a noop.
 *
 * It can be called from any context printk can.
 */
extern void sseq_flush(struct seq_file *seq);

#define sseq_puts(seq, ...)                 \
	do {                                \
		seq_puts(seq, __VA_ARGS__); \
		sseq_flush(seq);            \
	} while (0)
#define sseq_printf(seq, ...)                 \
	do {                                  \
		seq_printf(seq, __VA_ARGS__); \
		sseq_flush(seq);              \
	} while (0)

#endif
