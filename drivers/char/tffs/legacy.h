/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
#ifndef _tffs_legacy_h_
#define _tffs_legacy_h_

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/zlib.h>
#include <avm/tffs/tffs.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#include <linux/mtd/mtd.h>
struct tffs_mtd {
	unsigned int        mtd_idx;
	unsigned int        segment_id;
	size_t              size;
	loff_t              start_bad;
	struct mtd_info     *mtd;
};

struct TFFS_LGCY_State {
	enum _tffs_id id;
	uint8_t *readbuf;
	loff_t start;
	size_t len;
	size_t offset;
};

#define PANIC_MODE_BIT   0
#define PANIC_MODE       (1 << PANIC_MODE_BIT)

struct tffs_lgcy_ctx {
	int                     mtd_num[2];
	struct tffs_mtd         avail_mtd[2];
	struct tffs_mtd         *active_mtd;
	unsigned int            idx_created;
	volatile unsigned long  panic_mode;
	struct TFFS_LGCY_State  panic_state;

	void                    *notify_priv;
	tffs3_notify_fn         notify_cb;
};

#endif /*--- #ifndef _tffs_legacy_h_ ---*/
