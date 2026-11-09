/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2022 AVM GmbH <fritzbox_info@avm.de> */
#ifndef _UAPI_TFFS_ONDISK_H
#include <linux/types.h>

struct _TFFS_Entry {
	unsigned short ID;
	unsigned short Length;
};

/* NAND specific stuff: */

#define TFFS3_MAX_BADPAGES  0x4U

struct _TFFS_Block_Hdr_NAND {
	__be32 blkseq_nr;
	__be32 erase_cnt;
	__be32 sect_per_pg;
	__be32 num_bad_pages;
	__be64 bad_pages[TFFS3_MAX_BADPAGES];
} __attribute__((packed));

struct _TFFS_Block_Hdr {
#define TFFS3_HDR_MAGIC     0x41564d5f54464653ULL // AVM_TFFS
	__be64 magic;
	__be32 version;
	__be32 type;
	union {
		struct _TFFS_Block_Hdr_NAND mtdnand;
	};
} __attribute__((packed));

struct _TFFS_NAND_Entry {
	__be32 ID;
	__be32 Length;
	__be32 timestamp;
	__be32 revision_nr;
	__be32 segment_nr;
	__be32 next_segment;
} __attribute__((packed));

struct _TFFS_NAND_OOB {
	__be32 ID;
	__be32 Length;
	__be32 Revision;
} __attribute__((packed));

struct _TFFS_NAND_Entry_31 {
#define TFFS_ENTRY_MAGIC 0x544646535F4E3331 // TFFS_E31
	__be64 magic;
	__be32 ID;
	__be32 Length;
	__be32 timestamp;
	__be32 revision_nr;
	__be32 segment_nr;
	__be32 next_segment;
	__be32 crc;
} __attribute__((packed));

#define TFFS_VERSION(major, minor) ((((major)&0xffff) << 16) + ((minor)&0xffff))
#define TFFS3_TYPE_MTDNOR	   0x1
#define TFFS3_TYPE_MTDNAND	   0x2
#define TFFS3_TYPE_BDEV		   0x3
#define TFFS3_TYPE_CACHE	   0x4
#define TFFS3_TYPE_REMOTE	   0x5
#define TFFS3_TYPE_LEGACY	   0x6

#endif
