/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2004-2014 AVM GmbH <fritzbox_info@avm.de> */
/*
 * nand.h
 *
 *  Created on: 29.04.2014
 *      Author: tklaassen
 */

#ifndef TFFS_NAND_H_
#define TFFS_NAND_H_

#include <linux/kernel.h>
#include <avm/tffs/tffs.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/kref.h>
#include "local.h"

#define MIN_FREE_BLOCKS	 0x2
#define OPT_FREE_BLOCKS	 0x4
#define ANY_BLK_SEQ	 0xffffffff
#define TFFS_SEG_CLEARED 0xFFFFFFFF

enum tffs_blk_state {
	tffs_blk_raw = 0,
	tffs_blk_bad,
	tffs_blk_init,
	tffs_blk_rescan,
	tffs_blk_active,
};

enum tffs_srch_param {
	tffs_srch_min_spc,
	tffs_srch_max_spc,
	tffs_srch_min_seq,
	tffs_srch_max_seq,
	tffs_srch_min_erase,
	tffs_srch_max_erase,
};

struct TFFS_NAND_Block {
	struct list_head blk_list;
	struct list_head blk_entry_list;
	uint32_t sect_per_pg;
	uint32_t num_bad_pages;
	loff_t bad_pages[TFFS3_MAX_BADPAGES];
	enum tffs_blk_state state;
	uint32_t needs_rewrite;
	loff_t blk_addr;
	uint32_t blkseq_nr;
	uint32_t erase_cnt;
	loff_t free_start;
	size_t free_space;
	size_t used_space;
};

struct TFFS_Entry_Index;
struct TFFS_NAND_SegChain;

struct TFFS_NAND_Entry {
	struct list_head entry_list;
	struct list_head blk_entry_list;
	struct list_head segment_list;
	struct TFFS_NAND_Block *block_ptr;
	struct TFFS_NAND_SegChain *chain;
	loff_t blk_offset;
	size_t flash_len; // real length in flash, including padding and bad pages
	size_t padded_len; // real length in flash without bad pages
	uint32_t ID;
	uint32_t Length;
	uint32_t timestamp;
	uint32_t revision;
	uint32_t segment_nr;
	uint32_t next_segment;
};

struct TFFS_NAND_SegChain {
	struct list_head rev_list;
	struct list_head segment_list;
	struct kref refcnt;
	struct TFFS_Entry_Index *idx;
	uint32_t ID;
	uint32_t revision;
	uint32_t complete;
	uint32_t corrupt_cnt;
	uint32_t prune;
};

struct TFFS_Entry_Index {
	struct list_head index_list;
	struct list_head rev_list;
	uint32_t ID;
	uint32_t valid_rev; // highest revision number for ID with valid segment chain
	uint32_t max_rev; // highest revision number seen for ID, but data may be incomplete
	struct TFFS_NAND_SegChain *chain_ptr;
};

struct TFFS_NAND_State {
	struct TFFS_NAND_Entry *curr_entry;
	struct TFFS_NAND_SegChain *chain;
	enum _tffs_id id;
	uint32_t corrupt_cnt;
	uint32_t finished;
	loff_t offset;
	uint32_t revision;
	uint32_t segment;
	uint32_t next_segment;
	uint32_t segment_size;
};

struct tffs_nand_ctx {
	unsigned int mtd_num;
	loff_t raw_offset;
	struct mtd_info *mtd;
	uint8_t *rw_buffer;
	size_t buffer_size;
	uint64_t block_cnt;
	uint32_t pages_per_block;
	uint32_t sectors_per_page;
	uint32_t sector_size;
	uint32_t sector_sft;
	uint32_t sector_msk;
	uint32_t max_block_seq;
	struct semaphore lock;
	struct TFFS_NAND_State panic_state;
	struct TFFS_NAND_Entry panic_entries[2];
	struct mtd_info *panic_mtd;
	unsigned int in_panic_mode;
	struct tffs3_nand_funcs *panic_funcs;
	loff_t panic_offset;
	struct list_head blk_list;
	struct list_head entry_list;
	struct list_head index_list;
	void *notify_priv;
	tffs3_notify_fn notify_cb;
};

#endif /* TFFS_NAND_H_ */
