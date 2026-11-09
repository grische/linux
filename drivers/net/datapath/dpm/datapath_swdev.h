// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#ifndef DATAPATH_SWDEV_H
#define DATAPATH_SWDEV_H

#include <linux/if.h>
#include <linux/etherdevice.h>
#include <net/switchdev.h>
#include <net/switch_api/lantiq_gsw_api.h> /*Switch related structures */
#include <net/switch_api/lantiq_gsw.h>

#define ENABLE 1
#define DISABLE 0

#define BRIDGE_ID_ENTRY_HASH_LENGTH 6
#define BR_ID_ENTRY_HASH_TABLE_SIZE BIT(BRIDGE_ID_ENTRY_HASH_LENGTH)

#define ADD_BRENTRY 0x00000001
#define DEL_BRENTRY 0x00000002
#define BRIDGE_NO_ACTION 0x00000004
#define LOGIC_DEV_REGISTER 0x00000008

struct bridge_id_entry_item {
	u8 br_device_name[IFNAMSIZ];
	u16 fid;
	u32 flags;
	u32 portid; /*Bridge port*/
	u32 dp_port; /*DP port*/
	u32 inst;
	u32 priv_flag;
	u32 no_loop_flag; /* same as loop_dis flag during dp_alloc_port*/
	u32 hairpin; /* to store if dev has special bridge member config */
	struct switchdev_trans_item tritem;
};

struct br_info {
	struct hlist_node br_hlist;
	u8 br_device_name[IFNAMSIZ];
	u16 fid;
	u32 flag;
	u32 inst;
	u32 bp_priv_flag; /* to store bp dev priv flags (like IFF_NO_QUEUE) */
	u32 no_loop_flag; /* same as loop_dis flag during dp_alloc_port*/
	u32 hairpin; /* to store if dev has special bridge member config */
	u32 dp_port;
	struct list_head bp_list;
};

struct bridge_member_port {
	struct list_head list;
	u32 portid; /* bridge port */
	u32 dev_reg_flag;
	u16 bport[8];
	u32 dev_priv_flag; /* to store bp dev priv flags (like IFF_NO_QUEUE) */
	u32 dp_port;
	u32 no_loop_flag; /* same as loop_dis flag during dp_alloc_port*/
	u32 hairpin; /* if bridge port needs special bridge member config */
};

struct fdb_tbl {
	struct list_head fdb_list;
	struct net_device *port_dev;
	u8 addr[ETH_ALEN];
	__be16 vid;
};

extern struct list_head fdb_tbl_list;
extern struct hlist_head
	g_bridge_id_entry_hash_table[DP_MAX_INST][BR_ID_ENTRY_HASH_TABLE_SIZE];
int dp_swdev_bridge_id_entry_free(int instance);
int dp_swdev_bridge_id_entry_init(void);
struct br_info *dp_swdev_bridge_entry_lookup(char *br_name);
int dp_swdev_chk_bport_in_br(struct net_device *bp_dev, int bport, int inst);
u16 crc_cal(const u8 *data, u16 len);
u16 dp_swdev_cal_hash(u8 *dev_name);
void dp_switchdev_exit(void);

#endif /*DATAPATH_SWDEV_H*/
