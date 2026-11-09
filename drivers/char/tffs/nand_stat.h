/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2025 AVM GmbH <fritzbox_info@avm.de> */
#ifndef _TFFS_NAND_STAT_
#define _TFFS_NAND_STAT_

#include <linux/mtd/mtd.h>

#define TFFS_BLOCK_BAD -1
/*
 * The TFFS can technically ignore blocks, although this is likely a sign
 * of a greater malfunction of the hardware and/or software. This is
 * because it only ignores a non-bad block on which erase/write operations
 * failed. As such it's unlikely to see this on a functional device. Currently
 * the code does not call tffs_update_erase_cnt for these blocks and they
 * are updated on valid/bad. Basically - it's used as both a (technically)
 * valid to be displayed value, but also as a default value.
 */
#define TFFS_BLOCK_IGNORED -2

/**
 * keep the erase count statistics up to date
 * blk_addr: Address of erase block to record in statistics
 * mtd: mtd_info of mtd related to the erase block
 * cnt: amount of erases experienced by this block. -1 to indicate a bad block.
 */
void tffs_update_erase_cnt(loff_t blk_addr, struct mtd_info *mtd, int cnt);

#if CONFIG_TFFS_DEV_MTDNAND
/**
 * Initialize the stats, printing a summary and adding a procfs entry
 * returns 0 on succes and -err on failure
 */
int tffs_erase_stat_init(void);
#else
int tffs_erase_stat_init(void) { return 0; }
#endif
#endif
