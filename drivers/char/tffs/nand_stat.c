// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2025 AVM GmbH <fritzbox_info@avm.de> */
#define pr_fmt(fmt) "[TFFS3-NANDSTAT][%s] " fmt, __func__

#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <avm/compat/proc_fs.h>
#include "nand_stat.h"

#define TFFS_NANDSTAT_PROC_ENTRY "tffs_erase_cnt"

#define EDGE_VAL_STR_LEN 128
#define EDGE_VAL_STR "Erase counts: (min/max/mean): %u/%u/%u - %u bad blocks\n"

static unsigned int block_amount;
static int *block_erase_count;

void tffs_update_erase_cnt(loff_t blk_addr, struct mtd_info *mtd, int cnt)
{
	unsigned int block = blk_addr >> mtd->erasesize_shift;

	if (!block_erase_count) {
		int i;

		block_amount = mtd->size >> mtd->erasesize_shift;
		block_erase_count = kmalloc_array(block_amount, sizeof(*block_erase_count), GFP_KERNEL);
		if (!block_erase_count) {
			pr_err("Out of memory\n");
			return;
		}

		for (i = 0; i < block_amount; ++i)
			block_erase_count[i] = TFFS_BLOCK_IGNORED;
	}

	block_erase_count[block] = cnt;
}

static int calc_edge_values(char *buf, size_t buflen)
{
	unsigned int lower = UINT_MAX, mean = 0, upper = 0, amount = 0, bad = 0;
	int printed, i;

	if (!block_erase_count) {
		pr_err("<error> Block list not initialized\n");
		return -EINVAL;
	}

	/* TFFS needs at least MIN_FREE_BLOCKS valid blocks, or it won't boot */
	for (i = 0; i < block_amount; i++) {
		unsigned int erase_cnt = block_erase_count[i];

		if (erase_cnt == TFFS_BLOCK_BAD) {
			bad++;
			continue;
		}

		/*
		 * Skip without counting, since these are unlikely to happen
		 * without bigger problems attached. But also warn.
		 */
		if (erase_cnt == TFFS_BLOCK_IGNORED) {
			pr_err("Warning: Block %d ignored.\n", i);
			continue;
		}

		if (erase_cnt < lower)
			lower = erase_cnt;
		if (erase_cnt > upper)
			upper = erase_cnt;

		mean += erase_cnt;
		amount++;
	}

	mean *= 10;
	mean /= amount;
	/* some basic rounding while avoiding floats */
	if (mean % 10 >= 5)
		mean += 5;
	mean /= 10;

	printed = snprintf(buf, buflen, EDGE_VAL_STR, lower, upper, mean, bad);
	if (printed < 0 || printed >= buflen) {
		pr_err("buffer too small, need more than %zu bytes\n",
		       buflen);
		return -ENOMEM;
	}

	return printed;
}

static void *erase_stat_start(struct seq_file *seq, loff_t *pos)
{
	unsigned int block_number;

	if (*pos == 0)
		return SEQ_START_TOKEN;

	block_number = *pos - 1;
	if (block_number < block_amount)
		return &block_erase_count[block_number];

	return NULL;
}

static void *erase_stat_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned int block_number;

	(*pos)++;

	block_number = *pos - 1;
	if (block_number < block_amount)
		return &block_erase_count[block_number];

	return NULL;
}

static void erase_stat_stop(struct seq_file *seq, void *v)
{
}

static int erase_stat_show(struct seq_file *seq, void *v)
{
	int *list_pos;
	char head_buf[EDGE_VAL_STR_LEN];
	unsigned int erase_cnt;
	ptrdiff_t block_number;

	if (v == SEQ_START_TOKEN) {
		int ret;

		ret = calc_edge_values(head_buf, sizeof(head_buf));
		if (ret < 0)
			return ret;
		seq_printf(seq, "%s", head_buf);
		return 0;
	}

	list_pos = v;
	erase_cnt = *list_pos;
	block_number = list_pos - block_erase_count;
	if (erase_cnt == TFFS_BLOCK_BAD) {
		seq_printf(seq, "block: %4td - erase_cnt %7s\n",
			   block_number, "bad");
	} else if (erase_cnt == TFFS_BLOCK_IGNORED) {
		seq_printf(seq, "block: %4td - erase_cnt %7s\n",
			   block_number, "ignored");
	} else {
		seq_printf(seq, "block: %4td - erase_cnt %7u\n",
			   block_number, erase_cnt);
	}

	return 0;
}

static const struct seq_operations erase_cnt_seq_ops = {
	.start = erase_stat_start,
	.next = erase_stat_next,
	.stop = erase_stat_stop,
	.show = erase_stat_show
};

static int erase_stat_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &erase_cnt_seq_ops);
}

static const struct proc_ops erase_cnt_ops = {
	.proc_open = erase_stat_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

int tffs_erase_stat_init(void)
{
	int ret;
	char buf[EDGE_VAL_STR_LEN];

	proc_create(TFFS_NANDSTAT_PROC_ENTRY, 0444, NULL, &erase_cnt_ops);

	ret = calc_edge_values(buf, sizeof(buf));
	if (ret > 0)
		pr_info("%s", buf);

	return ret < 0 ? ret : 0;
}
