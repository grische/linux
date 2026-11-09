// SPDX-License-Identifier: GPL-2.0+

#include <avm/enh/semi_seq.h>

#include <linux/module.h>

struct seq_file *sseq_create(struct semi_seq *sseq, const char *lvl, char *buf,
			     size_t size)
{
	memset(sseq, 0, sizeof(*sseq));
	memset(buf, 0, size);

	sseq->file.size = size;
	sseq->file.buf = buf;
	sseq->lvl = lvl;

	// We use this as marker in seq_flush
	sseq->file.private = &sseq_create;

	return &sseq->file;
}
EXPORT_SYMBOL(sseq_create);

void sseq_flush(struct seq_file *seq)
{
	char *nl;
	struct semi_seq *sseq;

	// Not a semi seq_file, so simply return
	if (seq->private != &sseq_create)
		return;

	sseq = container_of(seq, struct semi_seq, file);

	for (;;) {
		size_t consumed;

		nl = memchr(seq->buf, '\n', seq->count);
		if (!nl)
			break;

		// Make this a terminated string and print it
		*nl = '\0';
		printk("%s%s\n", sseq->lvl, seq->buf);

		// Move the string forward
		consumed = nl - seq->buf + 1;

		seq->count -= consumed;
		memmove(seq->buf, seq->buf + consumed, seq->count);
	}
}
EXPORT_SYMBOL(sseq_flush);
