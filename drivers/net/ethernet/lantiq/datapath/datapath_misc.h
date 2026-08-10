/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * datapath/gswip30/datapath_misc.h.
 *
 * Declarations for the GSWIP-3.0 datapath platform hooks.
 */

#ifndef DATAPATH_MISC30_H
#define DATAPATH_MISC30_H

#include <linux/types.h>
#include <linux/seq_file.h>

#define PMAC_MAX_NUM 16
#define PAMC_LAN_MAX_NUM 7
#define VAP_OFFSET 8
#define VAP_MASK 0xF
#define VAP_DSL_OFFSET 3
#define NEW_CBM_API 1

#define GSWIP_L 0
#define GSWIP_R 1
#define MAX_SUBIF_PER_PORT 16
/* PMAC_SIZE comes from datapath.h - do not redefine here. */

struct gsw_itf {
	u8 ep;    /* -1 means no assigned yet for dynamic case */
	u8 fixed; /* fixed (1) or dynamically allocate (0) */
	u16 start;
	u16 end;
	u16 n;
};

#define SET_PMAC_PORTMAP(pmac, port_id)                          \
	do {                                                     \
		if ((port_id) <= 7)                              \
			(pmac)->port_map2 = 1 << (port_id);      \
		else                                             \
			(pmac)->port_map = (1 << (port_id - 8)); \
	} while (0)

#define SET_PMAC_SUBIF(pmac, subif)                             \
	do {                                                    \
		(pmac)->src_sub_inf_id2 = (subif) & 0xff;       \
		(pmac)->src_sub_inf_id = ((subif) >> 8) & 0x1f; \
	} while (0)

int dp_sub_proc_install_30(void);
void dp_sys_mib_reset_30(u32 flag);
int dp_set_gsw_parser_30(u8 flag, u8 cpu, u8 mpe1, u8 mpe2, u8 mpe3);
int dp_get_gsw_parser_30(u8 *cpu, u8 *mpe1, u8 *mpe2, u8 *mpe3);
int gsw_mib_reset_30(int dev, u32 flag);

ssize_t proc_get_qid_via_index(struct file *file, const char *buf,
			       size_t count, loff_t *ppos);
int lookup_dump30(struct seq_file *s, int pos);
int lookup_start30(void);
ssize_t proc_get_qid_via_index30(struct file *file, const char *buf,
				 size_t count, loff_t *ppos);

#endif /* DATAPATH_MISC30_H */
