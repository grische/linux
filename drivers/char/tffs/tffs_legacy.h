/*------------------------------------------------------------------------------------------*\
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
\*------------------------------------------------------------------------------------------*/
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
