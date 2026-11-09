/*
 * Description: Packet Processor Session Manager Internal Definitions
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018-2020 Intel Corporation
 */

#ifndef __PP_SESSION_MGR_INTERNAL_H__
#define __PP_SESSION_MGR_INTERNAL_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/pp_api.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/learning_layer_api.h>
#endif

#include "pp_si.h"
#include "pp_fv.h"
#include "uc_common.h"
#include "pp_session_mgr.h"

/**
 * @define DDR cache line size in bytes
 */
#define CACHE_LINE_SIZE 64

/**
 * @define Maximum number of sessions supported by the PP HW, derived
 *        from the SI session index field which is 3 bytes where
 *        0xffffff cannot be used
 */
#define SMGR_MAX_HW_SESSIONS (0xFFFFFE)
/**
 * @define Invalid session id
 */
#define SMGR_INVALID_SESS_ID (0xFFFFFF)

/**
 * @enum session manager sessions flags, these flags are set in the
 *       session db entry
 * @ROUTED specifying if session is a routed session, session is
 *         considered to be a routed session if the mac destination
 *         address has changed from ingress to egress
 * @SYNCQ specifying if session has a sync q attached
 * @MTU_CHCK specifying if MTU should be checked on the session
 * @MCAST_GRP specifying if session is a multicast group session
 *            (first cycle)
 * @MCAST_DST specifying if session is a multicast dst session
 *            (second cycle)
 */
enum smgr_sess_flags {
	SESS_FLAG_ROUTED,
	SESS_FLAG_SYNCQ,
	SESS_FLAG_MTU_CHCK,
	SESS_FLAG_MCAST_GRP,
	SESS_FLAG_MCAST_DST,
	SESS_FLAG_TDOX,
	SESS_FLAG_TDOX_SUPP,
	SMGR_FLAGS_NUM,
};

/**
 * @define Defines the learning packet info flags which specify if L2
 *        exist in a learning field vector
 */
#define LEARN_FV_L2_FLAGS \
	(FV_L2_BASE | FV_L2_PPPOE | FV_L2_EXT_VLAN | FV_L2_INT_VLAN)

/**
 * @define Defines the learning packet info flags which specify if L3
 *        exist in a learning field vector
 */
#define LEARN_FV_L3_FLAGS \
	(FV_L3_IPV4 | FV_L3_IPV6)

/**
 * @define Defines the learning packet info flags which specify if L4
 *        exist in a learning field vector
 */
#define LEARN_FV_L4_FLAGS \
	(FV_L4_TCP | FV_L4_UDP | FV_L4_SCTP | FV_L4_ICMP4 | FV_L4_ICMP6)

/**
 * @define Defines the learning packet info flags which specify if a
 *        tunnel exist in a learning field vector
 */
#define LEARN_FV_TUNN_FLAGS                \
	(FV_TUNNEL_VXLAN | FV_TUNNEL_GNV | \
	 FV_TUNNEL_L2TPV2_UDP | FV_TUNNEL_L2TPV3_UDP)

/**
 * @brief Shortcuts for session entry flags operations
 * @param e session db entry pointer (struct sess_db_entry)
 * @param f flag
 */
#define SESS_FLAG_SET(e, f) set_bit(f, &(e)->info.flags)
#define SESS_FLAG_CLR(e, f) clear_bit(f, &(e)->info.flags)

/**
 * @brief Shortcuts for session entry flags testing
 * @param s session db entry pointer (struct sess_db_entry)
 * @param f flag
 */
#define SESS_IS_FLAG_ON(e, f) test_bit(f, &(e)->info.flags)
#define SESS_IS_FLAG_OFF(e, f) !test_bit(f, &(e)->info.flags)

/**
 * @brief Shortcuts for session args flags testing
 * @param s session info pointer
 * @param f flag
 */
#define SESS_ARGS_IS_FLAG_ON(s, f) test_bit(f, &(s)->args->flags)
#define SESS_ARGS_IS_FLAG_OFF(s, f) !test_bit(f, &(s)->args->flags)

/**
 * @brief Shortcuts for accessing the session's ingress outer and
 *        inner packets
 * @param sess session info pointer
 */
#define SESS_INGRS_PKT(s) ((struct fv_pkt_info *)(s)->args->in_pkt)
#define SESS_INGRS_HDR_LEN(s) SESS_INGRS_OFFSETS(s).pkt_hdr_len
#define SESS_INGRS_OFFSETS(s) SESS_INGRS_PKT(s)->fv_offsets
#define SESS_INGRS_OUTER_HDRS(s) (&SESS_INGRS_PKT(s)->fv.outer)
#define SESS_INGRS_OUTER_L2(s) SESS_INGRS_OUTER_HDRS(s)->l2
#define SESS_INGRS_OUTER_L3(s) SESS_INGRS_OUTER_HDRS(s)->l3
#define SESS_INGRS_OUTER_L4(s) SESS_INGRS_OUTER_HDRS(s)->l4
#define SESS_INGRS_OUTER_BMAP(s) SESS_INGRS_PKT(s)->fv_outer_bitmap
#define SESS_INGRS_OUTER_OFFSETS(s) SESS_INGRS_OFFSETS(s).outer_offsets
#define SESS_INGRS_TUNN(s) SESS_INGRS_PKT(s)->fv.tunnel
#define SESS_INGRS_TUNN_BMAP(s) SESS_INGRS_PKT(s)->fv_tunnel_bitmap
#define SESS_INGRS_INNER_HDRS(s) (&SESS_INGRS_PKT(s)->fv.inner)
#define SESS_INGRS_INNER_L2(s) SESS_INGRS_INNER_HDRS(s)->l2
#define SESS_INGRS_INNER_L3(s) SESS_INGRS_INNER_HDRS(s)->l3
#define SESS_INGRS_INNER_L4(s) SESS_INGRS_INNER_HDRS(s)->l4
#define SESS_INGRS_INNER_BMAP(s) SESS_INGRS_PKT(s)->fv_inner_bitmap
#define SESS_INGRS_INNER_OFFSETS(s) SESS_INGRS_OFFSETS(s).inner_offsets

/**
 * @brief Shortcuts for accessing the session's egress outer and inner
 *        packets
 * @param s session info pointer (struct sess_info)
 */
#define SESS_EGRS_PKT(s) ((struct fv_pkt_info *)(s)->args->eg_pkt)
#define SESS_EGRS_HDR_LEN(s) SESS_EGRS_OFFSETS(s).pkt_hdr_len
#define SESS_EGRS_OFFSETS(s) SESS_EGRS_PKT(s)->fv_offsets
#define SESS_EGRS_OUTER_HDRS(s) (&SESS_EGRS_PKT(s)->fv.outer)
#define SESS_EGRS_OUTER_L2(s) SESS_EGRS_OUTER_HDRS(s)->l2
#define SESS_EGRS_OUTER_L3(s) SESS_EGRS_OUTER_HDRS(s)->l3
#define SESS_EGRS_OUTER_L4(s) SESS_EGRS_OUTER_HDRS(s)->l4
#define SESS_EGRS_OUTER_BMAP(s) SESS_EGRS_PKT(s)->fv_outer_bitmap
#define SESS_EGRS_OUTER_OFFSETS(s) SESS_EGRS_OFFSETS(s).outer_offsets
#define SESS_EGRS_TUNN(s) SESS_EGRS_PKT(s)->fv.tunnel
#define SESS_EGRS_TUNN_BMAP(s) SESS_EGRS_PKT(s)->fv_tunnel_bitmap
#define SESS_EGRS_INNER_HDRS(s) (&SESS_EGRS_PKT(s)->fv.inner)
#define SESS_EGRS_INNER_L2(s) SESS_EGRS_INNER_HDRS(s)->l2
#define SESS_EGRS_INNER_L3(s) SESS_EGRS_INNER_HDRS(s)->l3
#define SESS_EGRS_INNER_L4(s) SESS_EGRS_INNER_HDRS(s)->l4
#define SESS_EGRS_INNER_BMAP(s) SESS_EGRS_PKT(s)->fv_inner_bitmap
#define SESS_EGRS_INNER_OFFSETS(s) SESS_EGRS_OFFSETS(s).inner_offsets

/**
 * @enum Session manager mode
 * @disable disable mode, no sessions are created in this mode
 * @enable enable mode
 */
enum smgr_state {
	SMGR_DISABLE,
	SMGR_ENABLE,
	SMGR_STATES_NUM,
};

/**
 * @brief Session manager tdox session database entry definition
 * @tdox_id tdox id
 * @info relevant session info
 * @sess_ent corresponding session entry
 * @tdox_node list node for attaching the session to the tdox lists
 */
struct tdox_db_entry {
	u16 tdox_id;
	struct tdox_info info;
	struct sess_db_entry *sess_ent;
	struct list_head tdox_node;
} __aligned(CACHE_LINE_SIZE);

/**
 * @brief Session manager tdox update session database entry definition
 * @tdox_id tdox id
 * @sess_id corresponding session id
 * @tdox_node list node for attaching the session to the tdox lists
 */
struct tdox_sess_update_entry {
	u32 tdox_id;
	u32 sess_id;
	struct list_head tdox_node;
} __aligned(CACHE_LINE_SIZE);

/**
 * @brief Session manager session database entry definition
 * @info session info
 * @free_node list node for attaching the session to the free sessions
 *            list
 * @in_port_node list note for attaching the session to ingress port
 *               sessions list
 * @in_port_node list note for attaching the session to ingress port
 *               sessions list
 */
struct sess_db_entry {
	struct sess_db_info info;
	struct list_head free_node;
	struct list_head in_port_node;
	struct list_head eg_port_node;
} __aligned(CACHE_LINE_SIZE);

/**
 * @struct sess_info
 * @brief Session info definition, it defines the input
 *        information received by the user to create the session
 *        and all the information collected along the session
 *        create flow
 */
struct sess_info {
	/*! session create args received by the user */
	struct pp_sess_create_args *args;
	/*! session database entry */
	struct sess_db_entry *db_ent;
	/*! host si structure which is being built along the flow */
	struct pp_si si;
	/*! hw si structure which was encoded using the host si, this is
	 *  the si which is used to the create the actual session in the HW
	 */
	struct pp_hw_si hw_si;
	/*! pointer for pointing to the right modification flags variable
	 *  along the session create flow
	 */
	ulong *mod_flags;
	/*! Struct defines the info required for nat modifications */
	struct {
		u32 ingrs_bmap;
		u32 egrs_bmap;
		struct fv_l2 *ingrs_l2;
		union fv_l3 *ingrs_l3;
		union fv_l4 *ingrs_l4;
		struct fv_l2 *egrs_l2;
		union fv_l3 *egrs_l3;
		union fv_l4 *egrs_l4;
	} nat;
};

/**
 * @define FRAG_INFO_FLAG defines, to be used with struct si_ud_frag_info.flags
 */
#define FRAG_INFO_FLAG_INT_DF (BIT(0))
#define FRAG_INFO_FLAG_EXT_DF (BIT(1))
#define FRAG_INFO_FLAG_IGNORE_INT_DF (BIT(2))
#define FRAG_INFO_FLAG_IGNORE_EXT_DF (BIT(3))
#define FRAG_INFO_FLAG_FRAG_EXT (BIT(4))
#define FRAG_INFO_FLAG_IPV4 (BIT(5))
#define FRAG_INFO_FLAG_PPPOE (BIT(6))

/**
 * struct si_ud_frag_info - Fragmentation info
 * @l3_off: l3 offset. In case of inner fragmentation,
 * 	    the offset will be to the inner l3 hdr
 * @flags: Fragmentation flags
 * @dst_q: Destination queue
 * @max_pkt_size: Port's max_pkt_size
 *
 * This structure defines the fragmentation information saved in the si ud
 * right after the ps (Starts at si.ud + PP_PS_REGION_SZ).
 * Used in the fragmenter uc
 */
struct si_ud_frag_info {
	u8 flags;
	u8 l3_off;
	u16 dst_q;
	u16 max_pkt_size;
} __packed;

/**
 * @brief Test whether a session is routed session.<br>
 *        Routed session is a session where the ingress destination
 *        mac address is different then the egress one.
 * @param sess the session to test
 * @return bool true if the session is routed, false otherwise
 */
static inline bool smgr_is_sess_routed(const struct sess_db_info *sess)
{
	if (ptr_is_null(sess))
		return false;

	return test_bit(SESS_FLAG_ROUTED, &sess->flags);
}

/**
 * @brief Test whether a session is bridged session.<br>
 *        Bridged session is a session where the ingress destination
 *        mac address is equal to the egress one
 * @param sess the session to test
 * @return bool true if the session is bridged, false otherwise
 */
static inline bool smgr_is_sess_bridged(const struct sess_db_info *sess)
{
	if (ptr_is_null(sess))
		return false;

	return !test_bit(SESS_FLAG_ROUTED, &sess->flags);
}

/**
 * @brief Test whether a session is multicast group session
 * @param sess the session to test
 * @return bool true if the session is multicast group, false
 *         otherwise
 */
static inline bool smgr_is_sess_mcast_grp(const struct sess_db_info *sess)
{
	if (ptr_is_null(sess))
		return false;

	return test_bit(SESS_FLAG_MCAST_GRP, &sess->flags);
}

/**
 * @brief Test whether a session is multicast dst session
 * @param sess the session to test
 * @return bool true if the session is multicast dst, false
 *         otherwise
 */
static inline bool smgr_is_sess_mcast_dst(const struct sess_db_info *sess)
{
	if (ptr_is_null(sess))
		return false;

	return test_bit(SESS_FLAG_MCAST_DST, &sess->flags);
}

/**
 * @brief Test whether session id is valid or not
 * @note Valid session id doesn't mean the session exist
 * @param id session id to test
 * @return bool true if the id is a valid session id
 */
bool smgr_is_sess_id_valid(u32 id);

/**
 * @brief Get session SI
 * @param id session id
 * @param si buffer to write the si
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_session_si_get(u32 id, struct pp_si *si);

/**
 * @brief Get session dynamic SI
 * @param id session id
 * @param dsi buffer to write the dsi
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_session_dsi_get(u32 id, struct pp_dsi *dsi);

/**
 * @brief Get bitmap indicating which sessions uses the specified SGC
 * @param grp SGC group index
 * @param cntr SGC index
 * @param bmap bitmap buffer to save the sessions list
 * @param n_bits bitmap size in bits
 */
s32 smgr_sgc_sessions_bmap_get(u8 grp, u16 cntr, ulong *bmap, u32 n_bits);

/**
 * @brief Shortcut for converting QoS logical ID to physical ID
 * @param logical the logical ID
 * @param physical result physical ID
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_get_queue_phy_id(u16 logical, u16 *physical);

/**
 * @brief Update a PP session
 * @param sess_id session id to update
 * @param si new si
 * @return s32 0 on success update, error code otherwise
 */
s32 smgr_session_update(u32 sess_id, struct pp_hw_si *hw_si);
/* ========================================================================== */
/*                       Synchronization queues                               */
/* ========================================================================== */
/**
 * @brief allocate new syncq
 * @note exported internally only for debufs
 * @param session session id
 * @param dst_queue_id dst queue id
 * @return s32 0 on success, error code otherwise
 */
s32 sq_alloc(u32 session, u32 dst_queue_id);

/**
 * @brief allocate new syncq
 * @note called before the session is added to the hw,
 *       after the allocation the queue is available to use
 * @param sess session entry
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_sq_alloc(struct sess_info *sess);

/**
 * @brief start the sync queue state machine
 * @note exported internally only for debufs
 * @param session session id
 * @return s32 0 on success, error code otherwise
 */
s32 sq_start(u32 session);
/**
 * @brief start the sync queue state machine
 * @note called after the session was added in HW
 * @param sess session entry
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_sq_start(struct sess_info *sess);

/**
 * @brief delete the synch queue
 * @note exported internally only for debufs
 * @param session session id
 * @return s32 0 on success, error code otherwise
 */
s32 sq_del(u32 session);

/**
 * @brief delete the synch queue
 * @note called when session is being deleted
 * @param ent session db entry
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_sq_del(struct sess_db_entry *ent);

/**
 * @brief print all the sync queue from database
 * @note debug only
 */
void smgr_sq_dbg_dump(void);

/**
 * @brief get the "synch timeout" in micro sec units
 * @note debug only
 * @param tout timeout
 */
void smgr_sq_dbg_sync_tout_get(u32 *tout);

/**
 * @brief set the "synch timeout" in micro sec units
 * @note debug only
 * @param tout timeout
 */
void smgr_sq_dbg_sync_tout_set(u32 tout);

/**
 * @brief get the "done timeout" in micro sec units
 * @note debug only
 * @param tout timeout
 */
void smgr_sq_dbg_done_tout_get(u32 *tout);

/**
 * @brief set the "done timeout" in micro sec units
 * @note debug only
 * @param tout timeout
 */
void smgr_sq_dbg_done_tout_set(u32 tout);

/**
 * @brief get the "lspp timeout" in micro sec units
 * @note debug only
 * @note lspp: Last Slow Path Packet
 * @param tout timeout
 */
void smgr_sq_dbg_lspp_tout_get(u32 *tout);

/**
 * @brief set the "lspp timeout" in micro sec units
 * @note debug only
 * @note lspp: Last Slow Path Packet
 * @param tout timeout
 */
void smgr_sq_dbg_lspp_tout_set(u32 tout);

/**
 * @brief get the max qlen
 * @param qlen queue length
 */
void smgr_sq_dbg_qlen_get(u32 *qlen);

/**
 * @brief set the max qlen
 * @param qlen queue length
 */
void smgr_sq_dbg_qlen_set(u32 qlen);

/**
 * @brief exit the sync queue module
 */
void smgr_sq_exit(void);

/* ========================================================================== */
/*                               Multicast                                    */
/* ========================================================================== */
/**
 * @brief Track session in the multicast group db
 * @param sess session to track
 */
void smgr_mcast_sess_track(struct sess_info *sess);

/**
 * @brief Untrack session from the multicast group db
 * @param sess session to untrack
 */
void smgr_mcast_sess_untrack(struct sess_db_entry *ent);

/**
 * @brief Test whether session args mcast info is valid, mcast
 *        info is considered to be invalid when the user set one
 *        of the mcast flags and the mcast info is
 *        invalid, another invalid option is to set more than
 *        one mcast flag
 * @param args the args to test
 * @return bool true in case mcast info is valid, false
 *         otherwise
 */
bool smgr_mcast_is_info_valid(struct pp_sess_create_args *args);

/**
 * @brief update the session args for multicast sessions
 *        oob information for mcast dst sessions
 *        ud information for mcast group sessions
 * @param sess session info
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_mcast_sess_args_update(struct sess_info *sess);

/**
 * @brief Prepare a session multicast database entry
 * @param sess session info to save the session db entry
 * @note Caller MUST acquire database lock
 */
void smgr_mcast_sess_ent_prepare(struct sess_info *sess);

/**
 * @brief Perform session multicast lookup based on the
 *        multicast information (group or dst)
 * @param sess session info
 * @return s32 -EEXIST in case session was found, 0 if session wasn't
 *         found, error code otherwise
 */
s32 smgr_mcast_sess_lookup(struct sess_info *sess);

/**
 * @brief get the nf multicast packet counters
 * @param cid cpu index
 * @param stats multicast packet statistics
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_mcast_uc_cpu_stats_get(u32 cid, struct mcast_stats *stats);

/**
 * @brief get the nf frag packet counters
 * @param cid cpu index
 * @param stats fragmentation statistics
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_frag_uc_cpu_stats_get(u32 cid, struct frag_stats *stats);

/**
 * @brief Helper function to initialize multicast info
 * @param invalid_sess_id invalid session index
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_mcast_init(u32 invalid_sess_id);

/**
 * @brief Clean multicast resources
 */
void smgr_mcast_exit(void);

/* ========================================================================== */
/*                               Turbodox                                     */
/* ========================================================================== */

/**
 * @brief Helper function to initialize tdox manager
 * @return s32 0 on success, error code otherwise
 */
s32 smgr_tdox_init(struct device *dev);

/**
 * @brief Fetching tdox record info and sending mailbox to uC
 * @param ent session entry in db
 * @return 0 if success
 */
s32 smgr_tdox_record_create(struct sess_db_entry *sess_ent);

/**
 * @brief Validate args are valid for tdox session
 * @param ent session args
 * @return true if args are valid
 */
bool smgr_tdox_is_info_valid(struct pp_sess_create_args *args);

/**
 * @brief Prepare tdox db before seession creation
 * @param ent session info
 * @return 0 if success
 */
s32 smgr_tdox_sess_ent_prepare(struct sess_info *sess);

/**
 * @brief Remove tdox entry
 * @param ent tdox entry in db
 */
void smgr_tdox_session_remove(struct tdox_db_entry *ent);

/**
 * @brief Set tdox enable state
 */
void smgr_tdox_enable_set(bool enable);

/**
 * @brief Clean tdox resources
 */
void smgr_tdox_exit(void);

#ifdef CONFIG_DEBUG_FS
/**
 * @brief Set PP session manager state (enable/disable)
 * @param state new state
 * @return s32 0 on successful state change, error code otherwise
 */
s32 smgr_state_set(enum smgr_state state);

/**
 * @brief Get session manager current state
 * @return enum smgr_state
 */
enum smgr_state smgr_state_get(void);

/**
 * @brief Get nf status in smgr
 * @param nf nf type
 * @return true if nf enabled
 */
bool smgr_is_nf_en(enum pp_nf_type nf);

/**
 * @brief Session manager debug init
 * @return s32 0 for success, non-zero otherwise
 */
s32 smgr_dbg_init(struct dentry *parent);

/**
 * @brief Session manager debug cleanup
 * @return s32 0 for success, non-zero otherwise
 */
s32 smgr_dbg_clean(void);

#else /* !CONFIG_DEBUG_FS */
static inline s32 smgr_dbg_init(struct dentry *parent)
{
	return 0;
}

static inline s32 smgr_dbg_clean(void)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */
#endif /* __PP_SESSION_MGR_INTERNAL_H__ */
