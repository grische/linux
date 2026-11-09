/*
 * Description: PP session manager driver
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018-2020 Intel Corporation
 */

#define pr_fmt(fmt) "[PP_SMGR]:%s:%d: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/bug.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/ip.h>
#include <linux/if_pppox.h>
#include <linux/if_vlan.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <net/checksum.h>
#include <linux/learning_layer_api.h>
#include <linux/pp_api.h>

#include "pp_desc.h"
#include "pp_si.h"
#include "pp_fv.h"
#include "uc.h"
#include "pp_common.h"
#include "classifier.h"
#include "checker.h"
#include "modifier.h"
#include "pp_buffer_mgr.h"
#include "pp_port_mgr.h"
#include "pp_session_mgr.h"
#include "pp_session_mgr_internal.h"
#include "pp_misc.h"

/**
 * @struct base_args
 * @brief Basic session manager work arguments
 */
struct base_args {
	struct delayed_work dwork; /*! linux delayed work instance */
	unsigned long req_id; /*! user request id */
	unsigned long req_prio; /*! user request priority */
	pp_cb cb; /*! callback for sending request result */
};

/**
 * @struct Session create work arguments
 * @base mandatory basic work argument
 * @in_pkt ingress packet buffer
 * @eg_pkt egress packet buffer
 * @nhdr new header packet buffer
 * @args session create args reference
 * @args_buf session create args buffer used in async mode to save
 *       the user args in order to work in async mode
 * @cb_args callback arguments, in sync request, this used by the
 *          session manager to return the answer to the user, in async
 *          request, it returned directly to the user
 */
struct sess_cr_work_args {
	struct base_args base;
	struct fv_pkt_info in_pkt;
	struct fv_pkt_info eg_pkt;
	struct new_hdr nhdr;
	struct pp_sess_create_args *args;
	struct pp_sess_create_args args_buf;
	struct pp_sess_create_cb_args cb_args;
};

/**
 * @struct Session delete work arguments
 * @base mandatory basic work argument
 * @cb_args callback arguments, in sync request, this used by the
 *          session manager to return the answer to the user, in async
 *          request, it returned directly to the user
 * @sess_id session id to delete
 */
struct sess_del_work_args {
	struct base_args base;
	struct pp_sess_delete_cb_args cb_args;
	u32 sess_id;
};

/**
 * @struct Port's sessions flush work arguments
 * @base mandatory basic work argument
 * @cb_args callback arguments, in sync request, this used by the
 *          session manager to return the answer to the user, in async
 *          request, it returned directly to the user
 * @port_id PP port id
 */
struct port_flush_work_args {
	struct base_args base;
	struct pp_port_flush_cb_args cb_args;
	u16 port_id;
};

/**
 * @struct inactive_work_args
 * @brief Get inactive sessions list work arguments
 *        HW is able to scan maximum of 'CHK_CRWLR_MAX_ITR_SIZE'
 *        sessions in 1 iteration, the sessions manager will scan all
 *        sessions by scheduling delayed tasks where each task will
 *        initiate a crawler scan and also schedule the next one. the
 *        last task will gather all the sessions indexes got from the
 *        crawler HW and will return an answer to the user
 */
struct inactive_work_args {
	struct base_args base; /*! mandatory basic work argument*/
	u32 base_id; /*! session id to start the scan from */
	u32 it_sz; /*! number of sessions to scan per
				      * iteration
				      */
	u32 n_sessions; /*! total number of sessions to scan */
	u32 delay; /*! time in microseconds between
				      * iterations
				      */
	dma_addr_t bmap_phys; /*! bitmap for receiving stalled
				      *  sessions from checker
				      */
	size_t bmap_sz; /* bitmap size in bytes */
	u32 *inact; /*! user's array of inactive sessions */
	u32 inact_n; /*! user's array number of entries */
	u32 inact_found; /*! number of inactive sessions found */
};

/**
 * @brief Session manager work arguments for all types of works, this
 *        is used only for creating 1 cache for all types of works
 */
union work_args {
	struct base_args base;
	struct sess_cr_work_args cr;
	struct sess_del_work_args del;
	struct port_flush_work_args flush;
	struct inactive_work_args inactive;
};

/**
 * @brief Shortcuts for accessing the session's nat packets, nat
 *        packets are the packets which the nat changes are based on.
 * @param s session info pointer (struct sess_info)
 */
#define SESS_NAT_INGRS_BMAP(s) ((s)->nat.ingrs_bmap)
#define SESS_NAT_INGRS_L2(s) ((struct fv_l2 *)(s)->nat.ingrs_l2)
#define SESS_NAT_INGRS_L3(s) ((union fv_l3 *)(s)->nat.ingrs_l3)
#define SESS_NAT_INGRS_L4(s) ((union fv_l4 *)(s)->nat.ingrs_l4)
#define SESS_NAT_EGRS_BMAP(s) ((s)->nat.egrs_bmap)
#define SESS_NAT_EGRS_L2(s) ((struct fv_l2 *)(s)->nat.egrs_l2)
#define SESS_NAT_EGRS_L3(s) ((union fv_l3 *)(s)->nat.egrs_l3)
#define SESS_NAT_EGRS_L4(s) ((union fv_l4 *)(s)->nat.egrs_l4)

/**
 * @brief Shortcuts for session modification flags operations
 * @param s session info pointer (struct sess_info)
 * @param f flag
 */
#define SESS_MOD_FLAG_SET(s, f) set_bit(f, (s)->mod_flags)
#define SESS_MOD_FLAG_CLR(s, f) clear_bit(f, (s)->mod_flags)

/**
 * @brief Shortcuts for testing session flags
 * @param s session info pointer (struct sess_info)
 * @param f flag
 */
#define __smgr_is_sess_routed(s) smgr_is_sess_routed(&(s)->db_ent->info)
#define __smgr_is_sess_bridged(s) smgr_is_sess_bridged(&(s)->db_ent->info)

/**
 * @enum inact_scan_state
 * @brief Inactive sessions scan states
 */
enum inact_scan_state {
	INACT_SCAN_IDLE,
	INACT_SCAN_BUSY,
};

/**
 * @brief Session manager database definitions
 * @n_sessions number of sessions that the session manager supports
 * @lock spin lock
 * @open_map bitmap specifying which sessions are currently
 *        opened
 * @ipv4_map bitmap specifying which session are ipv4 sessions
 * @ipv6_map bitmap specifying which session are ipv6 sessions
 * @tcp_map bitmap specifying which session are tcp sessions
 * @udp_map bitmap specifying which session are udp sessions
 * @inact_map bitmap of size n_sessions bits to use for scanning for
 *            inactive sessions
 * @inact_map_phys
 * @inact_map_sz inactive bitmap size in bytes
 * @scan_state inactive sessions scan state
 * @state session manager state, disable/enable
 * @free_list free session list head
 * @sessions sessions array
 * @stats session manager statistics
 * @in_ports per ingress ports associated sessions list
 * @eg_ports per egress ports associated sessions list
 * @ev_args_cache event arguments cache
 * @work_args_cache async work arguments cache
 */
struct smgr_database {
	u32 n_sessions;
	spinlock_t lock; /*! locks database and HW accesses */
	ulong *open_bmap;
	ulong *ipv4_bmap;
	ulong *ipv6_bmap;
	ulong *tcp_bmap;
	ulong *udp_bmap;
	ulong *inact_bmap;
	dma_addr_t inact_bmap_phys;
	size_t inact_bmap_sz;
	enum inact_scan_state scan_state;
	enum smgr_state state;
	struct list_head free_list;
	struct sess_db_entry *sessions;
	struct smgr_stats stats;
	struct list_head in_port[PP_MAX_PORT];
	struct list_head eg_port[PP_MAX_PORT];
	struct kmem_cache *work_args_cache;
	struct workqueue_struct *workq;
	struct workqueue_struct *workq_hi;
	enum smgr_frag_mode frag_mode;
};

/* functions declarations */
static s32 __smgr_port_flush(u16 pid);

/* Shortcuts for database accesses */
#define DB_FREE_LIST_GET_NODE(_db) \
	list_first_entry(&(_db)->free_list, struct sess_db_entry, free_node)
#define DB_FREE_LIST_ADD_NODE(_db, _ent) \
	list_add_tail(&(_ent)->free_node, &(_db)->free_list)
#define DB_FREE_LIST_DEL_NODE(_ent) \
	list_del_init(&(_ent)->free_node)
#define DB_IN_PORT_LIST_ADD_NODE(_db, _ent)  \
	list_add_tail(&(_ent)->in_port_node, \
		      &(_db)->in_port[(_ent)->info.in_port])
#define DB_EG_PORT_LIST_ADD_NODE(_db, _ent)  \
	list_add_tail(&(_ent)->eg_port_node, \
		      &(_db)->eg_port[(_ent)->info.eg_port])
#define DB_IN_PORT_LIST_DEL_NODE(_ent) \
	list_del_init(&(_ent)->in_port_node)
#define DB_EG_PORT_LIST_DEL_NODE(_ent) list_del_init(&(_ent)->eg_port_node)

/**
 * @brief Module database
 */
static struct smgr_database *db;

/* ========================================================================== */
/*                               Shortcuts                                    */
/* ========================================================================== */

#define pr_pp_req(req, fmt, ...)                                                      \
	do {                                                                          \
		if (req)                                                              \
			pr_debug("req_id %lu, req_prio %lu, cb %p, " fmt,             \
				 req->req_id, req->req_prio, req->cb, ##__VA_ARGS__); \
		else                                                                  \
			pr_debug("req NULL, " fmt, ##__VA_ARGS__);                    \
	} while (0)

/**
 * @brief Acquire session manager lock
 */
static inline void __smgr_lock(void)
{
	pr_debug("caller %ps\n", __builtin_return_address(0));

	/* we are not allowed to run in irq context */
	WARN(in_irq(), "session manager shouldn't be used from irq context\n");
	spin_lock_bh(&db->lock);
}

/**
 * @brief Release session manager lock
 */
static inline void __smgr_unlock(void)
{
	pr_debug("caller %ps\n", __builtin_return_address(0));

	spin_unlock_bh(&db->lock);
}

/**
 * @brief Test whether the session manager is enable or not
 * @return bool true in case he is, false otherwise
 */
static inline bool __smgr_is_enable(void)
{
	return (pp_is_ready() && db->state == SMGR_ENABLE);
}

/**
 * @brief Test whether the session manager is disable or not
 * @return bool true in case he is, false otherwise
 */
static inline bool __smgr_is_disable(void)
{
	return !__smgr_is_enable();
}

/**
 * @brief Test whether session exist or not
 * @note Existing session doesn't mean he is active
 * @param id session id
 * @return bool true is session exist, false otherwise
 */
static inline bool __smgr_is_sess_exist(u32 id)
{
	if (unlikely(!smgr_is_sess_id_valid(id)))
		return false;

	if (likely(test_bit(id, db->open_bmap)))
		return true;

	pr_err("session %u doesn't exist\n", id);
	return false;
}

/**
 * @brief Get session database entry reference
 * @param id session id
 * @return struct sess_db_entry* entry reference on success, NULL
 *         otherwise
 */
static inline struct sess_db_entry *__smgr_get_sess_db_ent(u32 id)
{
	if (unlikely(!smgr_is_sess_id_valid(id)))
		return NULL;

	return &db->sessions[id];
}

/**
 * @brief Dispatch session manager work, schedule work queue in async
 *        mode or call the work function directly in sync mode
 * @param req pp request
 * @param args work base args
 * @param delay delay in nanoseconds before queueing the work
 */
static inline void __smgr_dispatch_work(struct pp_request *req,
					struct base_args *args,
					ulong delay)
{
	if (PP_IS_ASYNC_REQUEST(req)) {
		/* convert delay to jiffies */
		delay = usecs_to_jiffies(delay);
		pr_debug("dispatching work in %lu jiffies\n", delay);
		/* async request */
		if (req->req_prio)
			queue_delayed_work(db->workq_hi, &args->dwork, delay);
		else
			queue_delayed_work(db->workq, &args->dwork, delay);
	} else {
		/* sync request */
		args->dwork.work.func(&args->dwork.work);
	}
}

/**
 * @brief Allocate session work arguments
 * @return struct sess_cr_args* valid session work args pointer if
 *         available, NULL otherwise
 */
static inline union work_args *__must_check
__smgr_work_args_prepare(struct pp_request *req, work_func_t func)
{
	union work_args *args = NULL;

	args = (union work_args *)
		kmem_cache_alloc_node(db->work_args_cache,
				      GFP_ATOMIC, NUMA_NO_NODE);
	if (likely(args)) {
		memset(args, 0, sizeof(*args));
		if (req) {
			args->base.cb = req->cb;
			args->base.req_id = req->req_id;
			args->base.req_prio = req->req_prio;
		}
		/* always set func so we can use it later */
		if (PP_IS_ASYNC_REQUEST(req))
			INIT_DELAYED_WORK(&args->base.dwork, func);
		else
			args->base.dwork.work.func = func;

		return args;
	}

	atomic_inc(&db->stats.work_args_cache_err);
	pr_err("Failed to allocate session argument list\n");

	return args;
}

s32 smgr_get_queue_phy_id(u16 logical, u16 *physical)
{
	struct pp_qos_queue_info q_info;
	struct pp_qos_dev *qdev;
	s32 ret = 0;

	if (unlikely(ptr_is_null(physical)))
		return -EINVAL;

	/* get qos device */
	qdev = pp_qos_dev_open(PP_QOS_INSTANCE_ID);
	if (unlikely(ptr_is_null(qdev)))
		return -EINVAL;

	ret = pp_qos_queue_info_get(qdev, logical, &q_info);
	if (unlikely(ret)) {
		pr_err("Failed to get queue %u info\n", logical);
		return ret;
	}

	*physical = (u16)q_info.physical_id;
	return ret;
}

/**
 * @brief Shortcut for testing if a packet has L3 IPv4 header
 * @param bmap bitmap created by learning driver, describing the
 *             packet's layers
 * @return bool true in case ipv4 exist, false otherwise
 */
static inline bool __smgr_is_ipv4_pkt(u32 bmap)
{
	return !!(bmap & FV_L3_IPV4);
}

/**
 * @brief Shortcut for testing if a packet has L3 IPv6 header
 * @param bmap bitmap created by learning driver, describing the
 *             packet's layers
 * @return bool true in case ipv6 exist, false otherwise
 */
static inline bool __smgr_is_ipv6_pkt(u32 bmap)
{
	return !!(bmap & FV_L3_IPV6);
}

/**
 * @brief Shortcut for testing if a packet has L3 IPv6 or IPv4 header
 * @param bmap bitmap created by learning driver, describing the
 *             packet's layers
 * @return bool true in case ipv6 or ipv4 exist, false otherwise
 */
static inline bool __smgr_is_ip_pkt(u32 bmap)
{
	return __smgr_is_ipv4_pkt(bmap) || __smgr_is_ipv6_pkt(bmap);
}

/**
 * @brief Shortcut for testing whether a packet contains supported l3
 *        header
 * @param bmap bitmap specifying which layers the packet have
 * @return bool true if the l3 is supported, false
 *         otherwise
 */
static inline bool __smgr_is_pkt_l3_supported(u32 bmap)
{
	return __smgr_is_ip_pkt(bmap);
}

/**
 * @brief Shortcut for testing whether a packet contains supported l3
 *        header
 * @param bmap bitmap specifying which layers the packet have
 * @return bool true if the l3 is supported, false
 *         otherwise
 */
static inline bool __smgr_is_pkt_l4_supported(u32 bmap)
{
	return bmap & (FV_L4_TCP | FV_L4_UDP | FV_L4_ESP);
}

/**
 * @brief Shortcut for testing whether session is a tunnel termination
 *        or not, USE only in the session create flow
 * @param sess the session to test
 * @return bool true if the sess is tunnel termination, false
 *         otherwise
 */
static inline bool __smgr_is_tunn_term_sess(struct sess_info *sess)
{
	return (SESS_INGRS_TUNN_BMAP(sess) || SESS_EGRS_TUNN_BMAP(sess)) &&
	       (SESS_INGRS_TUNN_BMAP(sess) != SESS_EGRS_TUNN_BMAP(sess));
}

/**
 * @brief Shortcut for testing whether session is a tunnel bypass
 *        or not, USE only in the session create flow
 * @param sess the session to test
 * @return bool true if the sess is tunnel bypass, false
 *         otherwise
 */
static inline bool __smgr_is_tunn_bypass_sess(struct sess_info *sess)
{
	return (SESS_INGRS_TUNN_BMAP(sess) && SESS_EGRS_TUNN_BMAP(sess)) &&
	       (SESS_INGRS_TUNN_BMAP(sess) == SESS_EGRS_TUNN_BMAP(sess));
}

/**
 * @brief Check whether a session includes inner IP layer at
 *        ingress and missing inner IP layer at egress
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_iptun_decp_sess(struct sess_info *sess)
{
	return __smgr_is_ip_pkt(SESS_INGRS_INNER_BMAP(sess)) &&
	       !__smgr_is_ip_pkt(SESS_EGRS_INNER_BMAP(sess));
}

/**
 * @brief Check whether a session missing inner IP layer at
 *        ingress and includes inner IP layer at egress
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_iptun_encp_sess(struct sess_info *sess)
{
	return !__smgr_is_ip_pkt(SESS_INGRS_INNER_BMAP(sess)) &&
	       __smgr_is_ip_pkt(SESS_EGRS_INNER_BMAP(sess));
}

/**
 * @brief Check whether a session support fragments acceleration
 * @param sess the session
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_frag_accl_sess(struct sess_info *sess)
{
	if (SESS_INGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST &&
	    SESS_EGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST)
		return true;

	if (SESS_INGRS_TUNN_BMAP(sess) &&
	    SESS_INGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST &&
	    SESS_EGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST)
		return true;

	return false;
}

/**
 * @brief Test whether the inner ipv6 extension header has changed
 * @param sess the session to test
 * @return bool true if changed
 */
static inline bool __smgr_is_ipv6_opt_inner_changed(struct sess_info *sess)
{
	if ((SESS_INGRS_INNER_BMAP(sess) & FV_L3_IPV6_EXTENSION) !=
	    (SESS_EGRS_INNER_BMAP(sess) & FV_L3_IPV6_EXTENSION))
		return true;

	return false;
}

/**
 * @brief Test whether the outer ipv6 extension header has changed
 * @param sess the session to test
 * @return bool true if changed
 */
static inline bool __smgr_is_ipv6_opt_outer_changed(struct sess_info *sess)
{
	if ((SESS_INGRS_OUTER_BMAP(sess) & FV_L3_IPV6_EXTENSION) !=
	    (SESS_EGRS_OUTER_BMAP(sess) & FV_L3_IPV6_EXTENSION))
		return true;

	return false;
}

/**
 * @brief Check whether a session is pppoe encapsulation or not
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_pppoe_encap(struct sess_info *sess)
{
	/* is pppoe header was added on the outer packet */
	if (!(SESS_INGRS_OUTER_BMAP(sess) & FV_L2_PPPOE) &&
	    (SESS_EGRS_OUTER_BMAP(sess) & FV_L2_PPPOE))
		return true;

	if (!(SESS_INGRS_INNER_BMAP(sess) & FV_L2_PPPOE) &&
	    (SESS_EGRS_INNER_BMAP(sess) & FV_L2_PPPOE))
		return true;

	return false;
}

/**
 * @brief Check whether a session inner pppoe has been
 *        added/removed/changed
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_inner_pppoe_changed(struct sess_info *sess)
{
	/* pppoe header was added or removed? */
	if ((SESS_INGRS_INNER_BMAP(sess) & FV_L2_PPPOE) !=
	    (SESS_EGRS_INNER_BMAP(sess) & FV_L2_PPPOE))
		return true;

	/* pppoe session id changed? */
	if (SESS_INGRS_INNER_L2(sess).pppoe_id !=
	    SESS_EGRS_INNER_L2(sess).pppoe_id)
		return true;

	return false;
}

/**
 * @brief Test whether fragments can be supported for the session
 * @param sess the session
 * @return bool true if it is supported, false otherwise
 */
static inline bool __smgr_is_frag_supported(struct sess_info *sess)
{
	u32 frags_flags = FV_L3_FRAG_1ST | FV_L3_FRAG_2ND;

	/* no frags, supported */
	if (!(SESS_INGRS_OUTER_BMAP(sess) & frags_flags) &&
	    !(SESS_INGRS_INNER_BMAP(sess) & frags_flags) &&
	    !(SESS_EGRS_OUTER_BMAP(sess) & frags_flags) &&
	    !(SESS_EGRS_INNER_BMAP(sess) & frags_flags))
		return true;

	/* outer reassembly */
	if ((SESS_INGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST) &&
	    !(SESS_EGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST)) {
		/* supported only if reassembly NF is enabled */
		return pp_misc_is_nf_en(PP_NF_REASSEMBLY);
	}

	/* inner reassembly */
	if ((SESS_INGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST) &&
	    !(SESS_EGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST)) {
		/* supported only if reassembly NF is enabled */
		return pp_misc_is_nf_en(PP_NF_REASSEMBLY);
	}

	/* outer fragmentation with tunnel encapsulation */
	if (!(SESS_INGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST) &&
	    (SESS_EGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST)) {
		/* Tunnel encapsulation */
		if (!SESS_INGRS_TUNN_BMAP(sess) && SESS_EGRS_TUNN_BMAP(sess))
			/* supported only if frag NF is enabled */
			return pp_misc_is_nf_en(PP_NF_FRAGMENTER);

		/* PPPoE encapsulation */
		if (__smgr_is_pppoe_encap(sess))
			/* supported only if frag NF is enabled */
			return pp_misc_is_nf_en(PP_NF_FRAGMENTER);
	}

	/* inner fragmentation with tunnel encapsulation */
	if (!(SESS_INGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST) &&
	    (SESS_EGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST)) {
		/* Tunnel encapsulation */
		if (!SESS_INGRS_TUNN_BMAP(sess) && SESS_EGRS_TUNN_BMAP(sess))
			/* supported only if frag NF is enabled */
			return pp_misc_is_nf_en(PP_NF_FRAGMENTER);

		/* PPPoE encapsulation */
		if (__smgr_is_pppoe_encap(sess))
			/* supported only if frag NF is enabled */
			return pp_misc_is_nf_en(PP_NF_FRAGMENTER);
	}

	return false;
}

/**
 * @brief Test whether a changes in the ipv6 extension headers are supported
 * @param sess the session to test
 * @return bool true if supported
 */
static inline bool __smgr_is_ipv6_ext_supported(struct sess_info *sess)
{
	if (__smgr_is_iptun_decp_sess(sess)) {
		/* for ip tunnel decapsulation, the interesting case
		 * is only where ipv6 in the ingress inner and egress outer
		 */
		if (!__smgr_is_ipv6_pkt(SESS_INGRS_INNER_BMAP(sess)) ||
		    !__smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			return true;
		if ((SESS_INGRS_INNER_BMAP(sess) & FV_L3_IPV6_EXTENSION) ==
		    (SESS_EGRS_OUTER_BMAP(sess) & FV_L3_IPV6_EXTENSION))
			return true;
	} else if (__smgr_is_iptun_encp_sess(sess)) {
		/* for ip tunnel encapsulation, the interesting case
		 * is only where ipv6 in the ingress outer and egress inner
		 */
		if (!__smgr_is_ipv6_pkt(SESS_INGRS_OUTER_BMAP(sess)) ||
		    !__smgr_is_ipv6_pkt(SESS_EGRS_INNER_BMAP(sess)))
			return true;
		if ((SESS_INGRS_OUTER_BMAP(sess) & FV_L3_IPV6_EXTENSION) ==
		    (SESS_EGRS_INNER_BMAP(sess) & FV_L3_IPV6_EXTENSION))
			return true;
	} else {
		if (!__smgr_is_ipv6_opt_inner_changed(sess) &&
		    !__smgr_is_ipv6_opt_outer_changed(sess))
			return true;
	}

	return false;
}

/**
 * @brief Check whether a session is EoGRE termination session
 *        ONLY w/o any other tunnels
 * @note this API also set the mod_flags to avoid double
 *       checking later
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_eogre_term_sess(struct sess_info *sess)
{
	/* decapsulation */
	if ((SESS_INGRS_TUNN_BMAP(sess) == FV_TUNNEL_GRE_ETH) &&
	    (SESS_EGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (SESS_INGRS_INNER_BMAP(sess) & FV_L2_BASE)) {
		SESS_MOD_FLAG_SET(sess, MOD_EOGRE_DECP_BIT);
		return true;
	}
	/* encapsulation */
	if ((SESS_EGRS_TUNN_BMAP(sess) == FV_TUNNEL_GRE_ETH) &&
	    (SESS_INGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (SESS_EGRS_INNER_BMAP(sess) & FV_L2_BASE)) {
		if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_EOGRE_V4_ENCP_BIT);
		else if (__smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_EOGRE_V6_ENCP_BIT);
		return true;
	}

	return false;
}

/**
 * @brief Check whether a session is IPoGRE termination session
 *        ONLY w/o any other tunnels
 * @note this API also set the mod_flags to avoid double
 *       checking later
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_ipogre_term_sess(struct sess_info *sess)
{
	/* decapsulation */
	if ((SESS_INGRS_TUNN_BMAP(sess) == FV_TUNNEL_GRE_IP) &&
	    (SESS_EGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_INGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_IPOGRE_DECP_BIT);
		return true;
	}
	/* encapsulation */
	if ((SESS_EGRS_TUNN_BMAP(sess) == FV_TUNNEL_GRE_IP) &&
	    (SESS_INGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_EGRS_INNER_BMAP(sess)))) {
		if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_IPOGRE_V4_ENCP_BIT);
		else if (__smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_IPOGRE_V6_ENCP_BIT);
		return true;
	}

	return false;
}

/**
 * @brief Check whether a session is DSLITE termination session
 *        ONLY w/o any other tunnels
 * @note this API also set the mod_flags to avoid double
 *       checking later
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_dslite_term_sess(struct sess_info *sess)
{
	/* decapsulation */
	if ((SESS_INGRS_TUNN_BMAP(sess) == FV_TUNNEL_DSLITE) &&
	    (SESS_EGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_INGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_DSLITE_DECP_BIT);
		return true;
	}
	/* encapsulation */
	if ((SESS_EGRS_TUNN_BMAP(sess) == FV_TUNNEL_DSLITE) &&
	    (SESS_INGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_EGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_DSLITE_ENCP_BIT);
		return true;
	}

	return false;
}

/**
 * @brief Check whether a session is 6RD termination session
 *        ONLY w/o any other tunnels
 * @note this API also set the mod_flags to avoid double
 *       checking later
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_6rd_term_sess(struct sess_info *sess)
{
	/* decapsulation */
	if ((SESS_INGRS_TUNN_BMAP(sess) == FV_TUNNEL_6RD) &&
	    (SESS_EGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_INGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_6RD_DECP_BIT);
		return true;
	}
	/* encapsulation */
	if ((SESS_EGRS_TUNN_BMAP(sess) == FV_TUNNEL_6RD) &&
	    (SESS_INGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_EGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_6RD_ENCP_BIT);
		return true;
	}

	return false;
}

/**
 * @brief Check whether a session is a supported L2TP UDP termination session
 *        ONLY w/o any other tunnels
 * @note this API also set the mod_flags to avoid double
 *       checking later
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_l2tpudp_term_sess(struct sess_info *sess)
{
	__u32 tunnel_bitmap = SESS_INGRS_TUNN_BMAP(sess) &
			      ~FV_TUNNEL_L2TP_FLAGS;

	/* decapsulation v2/v3 */
	if ((tunnel_bitmap == FV_TUNNEL_L2TPV2_UDP ||
	     tunnel_bitmap == FV_TUNNEL_L2TPV3_UDP) &&
	    (SESS_INGRS_TUNN_BMAP(sess) & FV_TUNNEL_L2TP_PPP) &&
	    !(SESS_INGRS_TUNN_BMAP(sess) & FV_TUNNEL_L2TP_O) &&
	    !(SESS_INGRS_TUNN_BMAP(sess) & FV_TUNNEL_L2TP_CK) &&
	    (SESS_EGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_INGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_L2TPUDP_DECP_BIT);
		return true;
	}
	/* encapsulation v2/v3 */
	tunnel_bitmap = SESS_EGRS_TUNN_BMAP(sess) & ~FV_TUNNEL_L2TP_FLAGS;
	if ((tunnel_bitmap == FV_TUNNEL_L2TPV2_UDP ||
	     tunnel_bitmap == FV_TUNNEL_L2TPV3_UDP) &&
	    (SESS_INGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_EGRS_INNER_BMAP(sess)))) {
		if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_L2TPUDP_IPV4_ENCP_BIT);
		else if (__smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_L2TPUDP_IPV6_ENCP_BIT);
		return true;
	}

	return false;
}

/**
 * @brief Check whether a session is L2TPIP termination session
 *        ONLY w/o any other tunnels
 * @note this API also set the mod_flags to avoid double
 *       checking later
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_l2tpip_term_sess(struct sess_info *sess)
{
	__u32 tunnel_bitmap = SESS_INGRS_TUNN_BMAP(sess) &
			      ~FV_TUNNEL_L2TP_FLAGS;

	/* decapsulation */
	/* Do not support cookie or session_id == 0 (control message) */
	if (tunnel_bitmap == FV_TUNNEL_L2TPV3_IP &&
	    (SESS_EGRS_TUNN_BMAP(sess) == 0) &&
	    !(SESS_INGRS_TUNN_BMAP(sess) & FV_TUNNEL_L2TP_CK) &&
	    (SESS_INGRS_OUTER_L4(sess).l2tpoip.session_id != 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_INGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_INGRS_INNER_BMAP(sess)))) {
		SESS_MOD_FLAG_SET(sess, MOD_L2TPIP_DECP_BIT);
		return true;
	}
	/* encapsulation */
	tunnel_bitmap = SESS_EGRS_TUNN_BMAP(sess) & ~FV_TUNNEL_L2TP_FLAGS;
	if (tunnel_bitmap == FV_TUNNEL_L2TPV3_IP &&
	    (SESS_INGRS_TUNN_BMAP(sess) == 0) &&
	    (__smgr_is_pkt_l3_supported(SESS_EGRS_INNER_BMAP(sess))) &&
	    (__smgr_is_pkt_l4_supported(SESS_EGRS_INNER_BMAP(sess)))) {
		if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_L2TPIP_IPV4_ENCP_BIT);
		else if (__smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			SESS_MOD_FLAG_SET(sess, MOD_L2TPIP_IPV6_ENCP_BIT);
		return true;
	}

	return false;
}

/**
 * @brief Check whether a session is ESP at ingress
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_esp_ingress_sess(struct sess_info *sess)
{
	if ((SESS_INGRS_OUTER_BMAP(sess) & FV_L4_ESP) ||
	    ((SESS_INGRS_OUTER_BMAP(sess) & FV_L4_UDP) &&
	     (SESS_INGRS_INNER_BMAP(sess) & FV_L4_ESP)))
		return true;

	return false;
}

/**
 * @brief Check whether a session is ESP at egress
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_esp_egress_sess(struct sess_info *sess)
{
	if ((SESS_EGRS_OUTER_BMAP(sess) & FV_L4_ESP) ||
	    ((SESS_EGRS_OUTER_BMAP(sess) & FV_L4_UDP) &&
	     (SESS_EGRS_INNER_BMAP(sess) & FV_L4_ESP)))
		return true;

	return false;
}

/**
 * @brief Check whether a session is ESP encapsulation
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_esp_encp_sess(struct sess_info *sess)
{
	return !__smgr_is_esp_ingress_sess(sess) &&
	       __smgr_is_esp_egress_sess(sess);
}

/**
 * @brief Check whether a session outer mac addresses has changed
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_outer_mac_changed(struct sess_info *sess)
{
	return !!memcmp(&SESS_INGRS_OUTER_L2(sess).h_dest,
			&SESS_EGRS_OUTER_L2(sess).h_dest, ETH_ALEN);
}

/**
 * @brief Check whether a session inner mac addresses has changed
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_inner_mac_changed(struct sess_info *sess)
{
	return !!memcmp(&SESS_INGRS_INNER_L2(sess).h_dest,
			&SESS_EGRS_INNER_L2(sess).h_dest, ETH_ALEN);
}

/**
 * @brief Check whether a session outer vlans has been
 *        added/removed/changed
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_outer_vlan_changed(struct sess_info *sess)
{
	u32 vlans = (FV_L2_INT_VLAN | FV_L2_EXT_VLAN | FV_L2_SKB_VLAN_TAG);

	/* vlan was added or removed? */
	if ((SESS_INGRS_OUTER_BMAP(sess) & vlans) !=
	    (SESS_EGRS_OUTER_BMAP(sess) & vlans))
		return true;
	/* external vlan value changed? */
	if (SESS_INGRS_OUTER_L2(sess).ext_vlan !=
	    SESS_EGRS_OUTER_L2(sess).ext_vlan)
		return true;
	/* internal vlan value changed? */
	if (SESS_INGRS_OUTER_L2(sess).int_vlan !=
	    SESS_EGRS_OUTER_L2(sess).int_vlan)
		return true;

	return false;
}

/**
 * @brief Check whether a session inner vlans has been
 *        added/removed/changed
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_inner_vlan_changed(struct sess_info *sess)
{
	u32 vlans = (FV_L2_INT_VLAN | FV_L2_EXT_VLAN);

	/* vlan was added or removed? */
	if ((SESS_INGRS_INNER_BMAP(sess) & vlans) !=
	    (SESS_EGRS_INNER_BMAP(sess) & vlans))
		return true;
	/* external vlan value changed? */
	if (SESS_INGRS_INNER_L2(sess).ext_vlan !=
	    SESS_EGRS_INNER_L2(sess).ext_vlan)
		return true;
	/* internal vlan value changed? */
	if (SESS_INGRS_INNER_L2(sess).int_vlan !=
	    SESS_EGRS_INNER_L2(sess).int_vlan)
		return true;

	return false;
}

/**
 * @brief Check whether a session outer pppoe has been
 *        added/removed/changed
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_outer_pppoe_changed(struct sess_info *sess)
{
	/* pppoe header was added or removed? */
	if ((SESS_INGRS_OUTER_BMAP(sess) & FV_L2_PPPOE) !=
	    (SESS_EGRS_OUTER_BMAP(sess) & FV_L2_PPPOE))
		return true;
	/* pppoe session id changed? */
	if (SESS_INGRS_OUTER_L2(sess).pppoe_id !=
	    SESS_EGRS_OUTER_L2(sess).pppoe_id)
		return true;

	return false;
}

static inline bool __smgr_is_outer_l2_changed(struct sess_info *sess)
{
	return __smgr_is_outer_mac_changed(sess) ||
	       __smgr_is_outer_vlan_changed(sess) ||
	       __smgr_is_outer_pppoe_changed(sess);
}

/**
 * @brief Check whether a session nat packets are ipv4, the nat
 *        packets are the packets that needs to compare for doing the
 *        nat
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_ipv4_nat(struct sess_info *sess)
{
	return __smgr_is_ipv4_pkt(SESS_NAT_EGRS_BMAP(sess)) &&
	       __smgr_is_ipv4_pkt(SESS_NAT_INGRS_BMAP(sess));
}

/**
 * @brief Check whether the session's nat packets are ipv6, the nat
 *        packets are the packets that needs to compare for doing the
 *        nat
 * @param sess the session to test
 * @return bool true if it is, false otherwise
 */
static inline bool __smgr_is_ipv6_nat(struct sess_info *sess)
{
	return __smgr_is_ipv6_pkt(SESS_NAT_EGRS_BMAP(sess)) &&
	       __smgr_is_ipv6_pkt(SESS_NAT_INGRS_BMAP(sess));
}

/**
 * @brief Check whether a nat ipv4 session ip addresses has changed
 * @param sess the session to test
 * @return bool true if they did, false otherwise
 */
static inline bool __smgr_is_ipv4_nat_addr_changed(struct sess_info *sess)
{
	/* source address changed? */
	if (SESS_NAT_INGRS_L3(sess)->v4.saddr !=
	    SESS_NAT_EGRS_L3(sess)->v4.saddr)
		return true;
	/* dest address changed? */
	if (SESS_NAT_INGRS_L3(sess)->v4.daddr !=
	    SESS_NAT_EGRS_L3(sess)->v4.daddr)
		return true;

	return false;
}

/**
 * @brief Check whether a nat ipv6 session ip addresses has changed
 * @param sess the session to test
 * @return bool true if they did, false otherwise
 */
static inline bool __smgr_is_ipv6_nat_addr_changed(struct sess_info *sess)
{
	/* source address changed? */
	if (memcmp(&SESS_NAT_INGRS_L3(sess)->v6.saddr,
		   &SESS_NAT_EGRS_L3(sess)->v6.saddr,
		   sizeof(struct in6_addr)))
		return true;
	/* dest address changed? */
	if (memcmp(&SESS_NAT_INGRS_L3(sess)->v6.daddr,
		   &SESS_NAT_EGRS_L3(sess)->v6.daddr,
		   sizeof(struct in6_addr)))
		return true;

	return false;
}

/**
 * @brief Check whether the session's nat ip addresses has changed
 * @param sess the session to test
 * @return bool true if they did, false otherwise
 */
static inline bool __smgr_is_ip_nat_addr_changed(struct sess_info *sess)
{
	/* IPv4 */
	if (__smgr_is_ipv4_nat(sess))
		return __smgr_is_ipv4_nat_addr_changed(sess);
	/* IPv6 */
	else if (__smgr_is_ipv6_nat(sess))
		return __smgr_is_ipv6_nat_addr_changed(sess);

	return false;
}

/* ========================================================================== */
/*                               Stats                                        */
/* ========================================================================== */
s32 pp_smgr_stats_get(struct smgr_stats *stats)
{
	atomic_t *db_cnt, *cnt;

	if (unlikely(ptr_is_null(stats)))
		return -EINVAL;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (ptr_is_null(stats))
		return -EINVAL;

	cnt = (atomic_t *)stats;
	for_each_struct_mem(&db->stats, db_cnt, cnt++)
		atomic_set(cnt, atomic_read(db_cnt));

	return 0;
}

s32 pp_smgr_stats_reset(void)
{
	u32 free, open;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	__smgr_lock();
	free = atomic_read(&db->stats.sess_free);
	open = atomic_read(&db->stats.sess_open);

	memset(&db->stats, 0, sizeof(db->stats));

	atomic_set(&db->stats.sess_free, free);
	atomic_set(&db->stats.sess_open, open);

	__smgr_unlock();
	return 0;
}

/**
 * @brief Update stats upon session create request
 */
static inline void __smgr_stats_sess_cr_req(void)
{
	atomic_inc(&db->stats.sess_create_req);
}

/**
 * @brief Update stats upon session delete request
 */
static inline void __smgr_stats_sess_del_req(void)
{
	atomic_inc(&db->stats.sess_delete_req);
}

/**
 * @brief Update stats upon port flush request
 */
static inline void __smgr_stats_port_flush_req(void)
{
	atomic_inc(&db->stats.port_flush_req);
}

/**
 * @brief Update stats upon port flush request
 */
static inline void __smgr_stats_flush_all_req(void)
{
	atomic_inc(&db->stats.flush_all_req);
}

/**
 * @brief Update stats upon port flush request
 */
static inline void __smgr_stats_inactive_req(void)
{
	atomic_inc(&db->stats.inactive_req);
}

/**
 * @brief Update stats upon session created
 */
static inline void __smgr_stats_sess_created(struct sess_info *sess)
{
	struct smgr_stats *stats = &db->stats;

	atomic_inc(&stats->sess_created);
	atomic_inc(&stats->sess_open);
	atomic_dec(&stats->sess_free);

	if (atomic_read(&stats->sess_open_hi_wm) <
	    atomic_read(&stats->sess_open))
		atomic_set(&stats->sess_open_hi_wm,
			   atomic_read(&stats->sess_open));

	/* TBD: update session type based stats: IPv4, IPv6, TCP, UDP */
}

/**
 * @brief Update stats upon session deleted
 */
static inline void __smgr_stats_sess_deleted(struct sess_db_entry *ent)
{
	struct smgr_stats *stats = &db->stats;

	atomic_inc(&stats->sess_deleted);
	atomic_dec(&stats->sess_open);
	atomic_inc(&stats->sess_free);
	/* TBD: update session type based stats: IPv4, IPv6, TCP, UDP */
}

/**
 * @brief Update stats upon port flushed
 */
static inline void __smgr_stats_port_flushed(void)
{
	atomic_inc(&db->stats.port_flushed);
}

/**
 * @brief Update stats upon flush all done
 */
static inline void __smgr_stats_inactive_done(void)
{
	atomic_inc(&db->stats.inactive_done);
}

/**
 * @brief Update stats upon flush all done
 */
static inline void __smgr_stats_flush_all_done(void)
{
	atomic_inc(&db->stats.flush_all_done);
}

/**
 * @brief Update stats upon invalid session create args error
 */
static inline void __smgr_stats_invalid_args(void)
{
	atomic_inc(&db->stats.invalid_args);
}

/**
 * @brief Update stats upon session create failure
 */
static inline void __smgr_stats_sess_cr_failed(struct sess_info *sess)
{
	atomic_inc(&db->stats.sess_create_fail);
}

/**
 * @brief Update stats upon session delete failure
 */
static inline void __smgr_stats_sess_del_failed(void)
{
	atomic_inc(&db->stats.sess_delete_fail);
}

/**
 * @brief Update stats upon session update failure
 */
static inline void __smgr_stats_sess_update_failed(void)
{
	atomic_inc(&db->stats.sess_update_fail);
}

/**
 * @brief Update stats upon port flush failure
 */
static inline void __smgr_stats_port_flush_failed(void)
{
	atomic_inc(&db->stats.port_flush_fail);
}

/**
 * @brief Update stats upon flush all failure
 */
static inline void __smgr_stats_flush_all_failed(void)
{
	atomic_inc(&db->stats.flush_all_fail);
}

/**
 * @brief Update stats upon flush all failure
 */
static inline void __smgr_stats_inactive_failed(void)
{
	atomic_inc(&db->stats.inactive_fail);
}

/**
 * @brief Update stats upon session lookup failure
 */
static inline void __smgr_stats_sess_lu_fail(struct sess_info *sess)
{
	atomic_inc(&db->stats.sess_lu_fail);
}

/**
 * @brief Update stats upon successful session lookup
 */
static inline void __smgr_stats_sess_lu_succ(struct sess_info *sess)
{
	atomic_inc(&db->stats.sess_lu_succ);
}

/**
 * @brief Update stats upon session lookup error
 */
static inline void __smgr_stats_sess_lu_err(struct sess_info *sess)
{
	atomic_inc(&db->stats.sess_lookup_err);
}

/**
 * @brief Update stats upon a request to create session for
 *        unsupported packet
 */
static inline void __smgr_stats_sess_no_supported(struct sess_info *sess)
{
	atomic_inc(&db->stats.sess_not_supported);
}

/**
 * @brief Update stats upon si create error
 */
static inline void __smgr_stats_sess_si_create_err(struct sess_info *sess)
{
	atomic_inc(&db->stats.sess_si_create_err);
}

/**
 * @brief Update stats upon session delete request for session that
 *        isn't exist in hw
 */
static inline void __smgr_stats_hw_err(void)
{
	atomic_inc(&db->stats.hw_err);
}

/**
 * @brief Update stats upon resource busy
 */
static inline void __smgr_stats_resource_busy(void)
{
	atomic_inc(&db->stats.resource_busy);
}

/* ========================================================================== */
/*                               MISC                                         */
/* ========================================================================== */
s32 smgr_sessions_bmap_alloc(ulong **bmap, u32 *n_bits)
{
	if (unlikely(ptr_is_null(bmap) || ptr_is_null(n_bits)))
		return -EINVAL;

	*bmap = kcalloc(BITS_TO_LONGS(db->n_sessions), sizeof(**bmap),
			GFP_KERNEL);
	if (unlikely(!*bmap)) {
		pr_err("Failed to allocate bitmap for %u sessions\n",
		       db->n_sessions);
		return -ENOMEM;
	}
	*n_bits = db->n_sessions;

	return 0;
}

s32 smgr_sessions_arr_alloc(u32 **sessions, u32 *n_sessions)
{
	if (unlikely(ptr_is_null(sessions) || ptr_is_null(n_sessions)))
		return -EINVAL;

	*sessions = kcalloc(db->n_sessions, sizeof(**sessions), GFP_KERNEL);
	if (unlikely(!*sessions)) {
		pr_err("Failed to allocate sessions array for %u sessions\n",
		       db->n_sessions);
		return -ENOMEM;
	}
	*n_sessions = db->n_sessions;

	return 0;
}

bool smgr_is_sess_id_valid(u32 id)
{
	if (unlikely(!pp_is_ready()))
		return false;

	if (likely(id < db->n_sessions))
		return true;

	pr_err("invalid session id %u, max id %u\n",
	       id, db->n_sessions - 1);
	return false;
}

s32 smgr_session_si_get(u32 id, struct pp_si *si)
{
	struct pp_hw_si hw_si;
	s32 ret;

	if (unlikely(!smgr_is_sess_id_valid(id)))
		return -EINVAL;

	ret = cls_session_si_get(id, &hw_si);
	if (unlikely(ret))
		return ret;

	return pp_si_decode(si, &hw_si);
}

s32 smgr_session_dsi_get(u32 id, struct pp_dsi *dsi)
{
	struct pp_hw_dsi hw_dsi;
	s32 ret;

	if (unlikely(!smgr_is_sess_id_valid(id)))
		return -EINVAL;

	ret = chk_session_dsi_get(id, &hw_dsi);
	if (unlikely(ret))
		return ret;

	return pp_dsi_decode(dsi, &hw_dsi);
}

s32 pp_max_sessions_get(u32 *max_sessions)
{
	if (unlikely(ptr_is_null(max_sessions)))
		return -EINVAL;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	*max_sessions = db->n_sessions;
	return 0;
}
EXPORT_SYMBOL(pp_max_sessions_get);

s32 pp_session_stats_get(u32 id, struct pp_stats *stats)
{
	struct pp_dsi dsi;
	s32 ret;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(ptr_is_null(stats)))
		return -EINVAL;

	__smgr_lock();
	if (unlikely(!__smgr_is_sess_exist(id))) {
		ret = -EINVAL;
		goto unlock;
	}
	/* decode the si */
	ret = smgr_session_dsi_get(id, &dsi);
	if (unlikely(ret))
		goto unlock;

	stats->bytes = dsi.bytes_cnt;
	stats->packets = dsi.pkts_cnt;
unlock:
	__smgr_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_session_stats_get);

s32 pp_session_stats_mod(u32 id, enum pp_stats_op act, u8 pkts, u32 bytes)
{
	bool exception = false;
	s32 ret;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	__smgr_lock();
	if (unlikely(!__smgr_is_sess_exist(id))) {
		ret = -EINVAL;
		goto unlock;
	}

	switch (act) {
	case PP_STATS_SUB:
		ret = chk_session_stats_mod(id, false, pkts, bytes, exception);
		break;
	case PP_STATS_ADD:
		ret = chk_session_stats_mod(id, true, pkts, bytes, exception);
		break;
	case PP_STATS_RESET:
		ret = chk_session_stats_reset(id);
		break;
	default:
		pr_err("Invalid operation %u\n", act);
		ret = -EINVAL;
	}

	if (unlikely(ret))
		pr_err("failed to %s session %u counters\n",
		       PP_STATS_OP_STR(act), id);

unlock:
	__smgr_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_session_stats_mod);

s32 pp_session_dst_queue_update(u32 id, u16 dst_q)
{
	u16 phy_dst_q;
	s32 ret;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	__smgr_lock();
	if (unlikely(!__smgr_is_sess_exist(id))) {
		ret = -EINVAL;
		goto unlock;
	}
	ret = smgr_get_queue_phy_id(dst_q, &phy_dst_q);
	if (unlikely(ret))
		goto unlock;

	ret = chk_session_dsi_q_update(id, phy_dst_q);
	if (unlikely(ret))
		pr_err("failed to update session %u dest queue to %u\n",
		       id, dst_q);

unlock:
	__smgr_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_session_dst_queue_update);

s32 pp_session_active_state_get(u32 id, bool *active)
{
	struct pp_dsi dsi;
	s32 ret;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(ptr_is_null(active)))
		return -EINVAL;

	__smgr_lock();
	if (unlikely(!__smgr_is_sess_exist(id))) {
		ret = -EINVAL;
		goto unlock;
	}
	/* decode the si */
	ret = smgr_session_dsi_get(id, &dsi);
	if (unlikely(ret))
		goto unlock;

	*active = dsi.active;
unlock:
	__smgr_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_session_active_state_get);

s32 pp_session_size_get(size_t *sz)
{
	if (unlikely(ptr_is_null(sz)))
		return -EINVAL;

	*sz = sizeof(struct pp_hw_si);
	return 0;
}
EXPORT_SYMBOL(pp_session_size_get);

s32 pp_session_hash_get(u32 id, struct pp_hash *hash)
{
	struct sess_db_entry *sess;
	s32 ret = 0;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(ptr_is_null(hash)))
		return -EINVAL;

	__smgr_lock();
	if (unlikely(!__smgr_is_sess_exist(id))) {
		ret = -EINVAL;
		goto unlock;
	}

	sess = __smgr_get_sess_db_ent(id);
	if (unlikely(!sess)) {
		ret = -EPERM;
		goto unlock;
	}

	memcpy(hash, &sess->info.hash, sizeof(*hash));
unlock:
	__smgr_unlock();
	return ret;
}
EXPORT_SYMBOL(pp_session_hash_get);

s32 pp_session_dbg_hash_get(struct pp_hash *hash)
{
	return cls_dbg_hash_get(hash);
}
EXPORT_SYMBOL(pp_session_dbg_hash_get);

s32 smgr_open_sessions_bmap_get(ulong *bmap, u32 n_bits)
{
	u32 sz;

	if (unlikely(ptr_is_null(bmap)))
		return -EINVAL;

	__smgr_lock();
	/* copy as many sessions as we can */
	sz = sizeof(*db->open_bmap) *
	     BITS_TO_LONGS(min_t(u32, db->n_sessions, n_bits));
	memcpy(bmap, db->open_bmap, sz);
	__smgr_unlock();

	return 0;
}

s32 smgr_session_info_get(u32 id, struct sess_db_info *info)
{
	struct sess_db_entry *ent;
	s32 ret = 0;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(ptr_is_null(info)))
		return -EINVAL;

	__smgr_lock();
	if (!__smgr_is_sess_exist(id)) {
		ret = -EINVAL;
		goto unlock;
	}

	ent = __smgr_get_sess_db_ent(id);
	memcpy(info, &ent->info, sizeof(ent->info));
unlock:
	__smgr_unlock();
	return ret;
}

s32 smgr_state_set(enum smgr_state state)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(!(state >= SMGR_DISABLE &&
		       state <= SMGR_ENABLE))) {
		pr_err("invalid state %u\n", state);
		return -EINVAL;
	}

	__smgr_lock();
	if (state == SMGR_DISABLE)
		__smgr_port_flush(PP_MAX_PORT);
	db->state = state;
	__smgr_unlock();
	pr_debug("PP Session Manager %s\n", state == SMGR_ENABLE ?
						    "enabled" :
						    "disabled");

	return 0;
}

enum smgr_state smgr_state_get(void)
{
	if (unlikely(!pp_is_ready()))
		return SMGR_DISABLE;

	return db->state;
}

s32 pp_smgr_reassembly_stats_get(struct smgr_reass_stats *stats)
{
	struct reassembly_stats cpu_stats;
	s32 ret;
	u32 cid;

	for (cid = 0; cid < UC_CPUS_MAX; cid++) {
		if (!uc_is_cpu_active(UC_IS_EGR, cid))
			continue;

		ret = uc_reass_stats_get(cid, &cpu_stats);
		if (unlikely(ret)) {
			pr_err("failed to get reassembly uc cpu %u counters\n",
			       cid);
			return ret;
		}
		stats->rx_pkts += cpu_stats.rx_pkts;
		stats->tx_pkts += cpu_stats.tx_pkts;
		stats->reassembled_pkts += cpu_stats.reassembled;
		stats->accelerated_pkts += cpu_stats.accelerated;
		stats->diverted_pkts += cpu_stats.diverted;
		stats->starvation_pkts += cpu_stats.cntxs_starv;
		stats->starvation_pkts += cpu_stats.frags_starv;
		stats->dropped_pkts += cpu_stats.dropped;
		stats->errors += cpu_stats.cntx_overflow;
		stats->errors += cpu_stats.duplicates;
		stats->errors += cpu_stats.unsupported;
		stats->errors += cpu_stats.invalid_len;
		stats->errors += cpu_stats.timedout;
	}

	return 0;
}

s32 smgr_frag_uc_cpu_stats_get(u32 cid, struct frag_stats *stats)
{
	if (unlikely(ptr_is_null(stats)))
		return -EINVAL;

	if (unlikely(!uc_is_cpu_active(UC_IS_EGR, cid)))
		return -EPERM;

	/* send command */
	return uc_egr_mbox_cmd_send(UC_CMD_FRAG_STATS, cid, NULL, 0, stats,
				    sizeof(*stats));
}

s32 pp_smgr_frag_stats_get(struct smgr_frag_stats *stats)
{
	struct frag_stats cpu_stats;
	s32 ret;
	u32 i;

	for (i = 0; i < UC_CPUS_MAX; i++) {
		if (!uc_is_cpu_active(UC_IS_EGR, i))
			continue;

		ret = smgr_frag_uc_cpu_stats_get(i, &cpu_stats);
		if (unlikely(ret)) {
			pr_err("failed to get frag uc cpu %u counters\n", i);
			return ret;
		}
		stats->rx_pkt += cpu_stats.rx_pkt;
		stats->tx_pkt += cpu_stats.tx_pkt;
		stats->total_drops += cpu_stats.total_drops;
		stats->bmgr_drops += cpu_stats.bmgr_drops;
		stats->df_drops += cpu_stats.df_drops;
		stats->max_frags_drops += cpu_stats.max_frags_drops;
		stats->reserved1 += cpu_stats.reserved1;
		stats->reserved2 += cpu_stats.reserved2;
	}

	return 0;
}

/* ========================================================================== */
/*                               Session Create                               */
/* ========================================================================== */

/**
 * @brief Save some extra stuff in the database for better debugging
 * @param sess the session
 */
static inline void __smgr_sess_debug(struct sess_info *sess)
{
#ifdef CONFIG_DEBUG_FS
	struct sess_db_info *info = &sess->db_ent->info;

	/* save ingress packet info */
	if (SESS_INGRS_PKT(sess))
		memcpy(&info->ingress, SESS_INGRS_PKT(sess),
		       sizeof(info->ingress));
	/* save egress packet info */
	if (SESS_EGRS_PKT(sess))
		memcpy(&info->egress, SESS_EGRS_PKT(sess),
		       sizeof(info->egress));
#endif
}

/**
 * @brief Test whether session args pointers are valid, args pointers
 *        is considered to be invalid in case that ingress packet
 *        isn't NULL but egress packet is or vice versa
 * @param args args to verify
 * @return bool true in case all pointers are ok, false otherwise
 */
static bool __smgr_is_sess_args_ptrs_valid(struct pp_sess_create_args *args)
{
	bool res = true;

	if (unlikely(ptr_is_null(args)))
		return false;

	if (unlikely(args->in_pkt && !args->eg_pkt)) {
		pr_err("ingress packet without egress packet\n");
		res = false;
	}

	if (unlikely(!args->in_pkt && args->eg_pkt)) {
		pr_err("egress packet without ingress packet\n");
		res = false;
	}

	return res;
}

/**
 * @brief Test whether session args ud info is valid, ud info is
 *        considered to be invalid when the user request to copy ud to
 *        STW PS region by setting 'ps_off' to non-zero value but
 *        didn't define the ud by setting the 'ud_sz' to zero
 * @param args the args to test
 * @return bool true in case ud info is valid, false otherwise
 */
static bool __smgr_is_ud_info_valid(struct pp_sess_create_args *args)
{
	/* for multicast group sessions, session manager
	 * will set the ud info internally
	 */
	if (test_bit(PP_SESS_FLAG_MCAST_GRP_BIT, &args->flags))
		return true;

	if (unlikely((args->ps_sz + args->ud_sz) > PP_UD_REGION_SZ)) {
		pr_err("ps_sz (%u) + ud_sz (%u) must be less than %u\n",
		       args->ps_sz, args->ud_sz, PP_UD_REGION_SZ);
		return false;
	}

	if (unlikely(args->ps_sz && (args->ps_off == PP_INVALID_PS_OFF))) {
		pr_err("ps_sz (%u) cannot be set if ps_off not chosen\n",
		       args->ps_sz);
		return false;
	}

	if (unlikely((args->ps_off + args->ps_sz) != PP_PS_REGION_SZ)) {
		pr_err("ps_sz (%u) + ps_off (%u) must equal %u\n",
		       args->ps_sz, args->ps_off, PP_PS_REGION_SZ);
		return false;
	}

	return true;
}

/**
 * @brief Test whether session classification info is valid or not
 *        Session's classification info is considered to be valid in
 *        case all fields length combined doesn't exceeds the maximum
 *        allowed
 * @param args session args
 * @return bool true in case the classification info is valid, false
 *         otherwise
 */
static bool __smgr_is_cls_info_valid(struct pp_sess_create_args *args)
{
	struct pp_port_cls_info port_cls;
	struct pp_port_cls_data *dcls;
	unsigned long mask;
	bool res = true;
	s32 ret, i;

	/* for multicast dst sessions, session manager
	 * will set the cls info internally
	 */
	if (test_bit(PP_SESS_FLAG_MCAST_DST_BIT, &args->flags))
		return true;

	ret = pp_port_cls_info_get(args->in_port, &port_cls);
	if (unlikely(ret))
		return false;

	dcls = &args->cls;

	if (unlikely(dcls->n_flds != port_cls.n_flds)) {
		pr_err("session classification is different from port classification\n");
		pr_err("session n_flds = %u, port n_flds = %u\n",
		       dcls->n_flds, port_cls.n_flds);
		res = false;
	}

	for (i = 0; i < dcls->n_flds; i++) {
		if (!port_cls.cp[i].copy_size)
			continue;
		mask = GENMASK(port_cls.cp[i].copy_size - 1, 0);
		if (unlikely(!__pp_mask_value_valid(mask, dcls->fld_data[i]))) {
			pr_err("field data %#x at index %u doesn't match copy_size of %u\n",
			       dcls->fld_data[i], i,
			       port_cls.cp[i].copy_size);
			res = false;
		}
	}

	/* Hash calculation is not supported in A Step */
	if (test_bit(PP_SESS_FLAG_INTERNAL_HASH_CALC_BIT, &args->flags)) {
		if (unlikely(pp_silicon_step_get() == PP_SSTEP_A)) {
			pr_err("hash calc is not supported in A step\n");
			res = false;
		}
	}

	return res;
}

/**
 * @brief Test whether session create parameters are valid or not
 * @param args the args to check
 * @return bool true if args are valid, false otherwise
 */
static bool __smgr_is_sess_args_valid(struct pp_sess_create_args *args)
{
	bool res = true;

	if (unlikely(!__smgr_is_sess_args_ptrs_valid(args)))
		return false;
	if (unlikely(!__pp_is_color_valid(args->color)))
		res = false;
	if (unlikely(!__smgr_is_ud_info_valid(args)))
		res = false;
	if (unlikely(!pmgr_port_is_active(args->in_port))) {
		pr_err("port %u isn't active\n", args->in_port);
		res = false;
	}
	if (unlikely(!pmgr_port_is_active(args->eg_port))) {
		pr_err("port %u isn't active\n", args->eg_port);
		res = false;
	}
	if (unlikely(!__smgr_is_cls_info_valid(args)))
		res = false;
	if (unlikely(!smgr_mcast_is_info_valid(args)))
		res = false;
	if (unlikely(!smgr_tdox_is_info_valid(args)))
		res = false;
	if (false == res)
		__smgr_stats_invalid_args();

	return res;
}

/**
 * @brief Calculate packet processor field vector size based on the
 *        learning field vector information
 *        the calculation is pretty simple, if a layer exist we set
 *        the size to be everything from the start of the field vector
 *        up to that layer including the layer itself
 * @param sess session info to save the real fv size
 * @return 0 always
 */
static s32 __smgr_calc_fv_real_sz(struct sess_info *sess)
{
	/* oob always exist */
	sess->si.fv_sz = sizeof(sess->si.fv.oob);
	if (!SESS_INGRS_PKT(sess))
		return 0;

	/* outer l2 */
	if (SESS_INGRS_OUTER_BMAP(sess) & LEARN_FV_L2_FLAGS)
		sess->si.fv_sz = offsetof(struct pp_fv, first.l3);
	/* outer l3 */
	if (SESS_INGRS_OUTER_BMAP(sess) & LEARN_FV_L3_FLAGS)
		sess->si.fv_sz = offsetof(struct pp_fv, first.l4);
	/* outer l4 */
	if (SESS_INGRS_OUTER_BMAP(sess) & LEARN_FV_L4_FLAGS)
		sess->si.fv_sz = offsetof(struct pp_fv, tunn);
	/* tunnel */
	if (SESS_INGRS_TUNN_BMAP(sess) & LEARN_FV_TUNN_FLAGS)
		sess->si.fv_sz = offsetof(struct pp_fv, second);
	/* inner l2 */
	if (SESS_INGRS_INNER_BMAP(sess) & LEARN_FV_L2_FLAGS)
		sess->si.fv_sz = offsetof(struct pp_fv, second.l3);
	/* inner l3 */
	if (SESS_INGRS_INNER_BMAP(sess) & LEARN_FV_L3_FLAGS)
		sess->si.fv_sz = offsetof(struct pp_fv, second.l4);
	/* inner l4 */
	if (SESS_INGRS_INNER_BMAP(sess) & LEARN_FV_L4_FLAGS)
		sess->si.fv_sz = sizeof(sess->si.fv);

	return 0;
}

/**
 * @brief Helper function for building the pp field vector out of band
 *        info for the ingress packet
 * @param sess session info
 */
static void __smgr_fv_oob_set(struct sess_info *sess)
{
	struct pp_fv_oob *oob = &sess->si.fv.oob;
	struct pp_port_cls_data *dcls;
	struct pp_port_cls_info port_cls;
	u8 ext_proto, int_proto, i;
	unsigned long mask, start, end;

	/* set ingress port */
	oob->in_pid = sess->args->in_port;
	/* set classification data */
	pp_port_cls_info_get(oob->in_pid, &port_cls);
	dcls = &sess->args->cls;
	for (start = 0, i = 0; i < dcls->n_flds; i++) {
		end = start + port_cls.cp[i].copy_size - 1;
		mask = GENMASK(end, start);
		start += port_cls.cp[i].copy_size;
		oob->stw_data = PP_FIELD_MOD(mask, dcls->fld_data[i],
					     oob->stw_data);
		pr_debug("data %#x, end %lu, start %lu, mask %#lx, stw_data %#x\n",
			 dcls->fld_data[i], end, start, mask, oob->stw_data);
	}
	/* To support oob sessions (sessions with field vector only with
	 * oob info) we need to set external and internal exactly as
	 * the parser will do, PRSR_PROTO_NO_PARSE
	 */
	if (!SESS_INGRS_PKT(sess)) {
		ext_proto = PP_FV_OTHER_L3_UNKNOWN_L4;
		int_proto = PP_FV_UNKNOWN_L3;
		goto done;
	}

	/* set proto info */
	ext_proto = pp_learn_pkt_to_pp_fv_proto(SESS_INGRS_OUTER_BMAP(sess),
						SESS_INGRS_TUNN_BMAP(sess));
	int_proto = pp_learn_pkt_to_pp_fv_proto(SESS_INGRS_INNER_BMAP(sess),
						0);

done:
	if (ext_proto != PP_FV_UNKNOWN_L3 &&
	    int_proto == PP_FV_UNKNOWN_L3) {
		int_proto = ext_proto;
		ext_proto = PP_FV_UNKNOWN_L3;
	}

	oob->proto_info = PP_FV_OOB_EXT_PROTO_INFO_SET(oob->proto_info,
						       ext_proto);
	oob->proto_info = PP_FV_OOB_INT_PROTO_INFO_SET(oob->proto_info,
						       int_proto);

	pr_debug("ext_proto %u, int_proto %u, proto_info %#x\n",
		 ext_proto, int_proto, oob->proto_info);
}

static s32 hash_calc(struct sess_info *sess, void *fv, u32 fv_sz,
		     struct pp_hash *hash)
{
	if (SESS_ARGS_IS_FLAG_OFF(sess, PP_SESS_FLAG_INTERNAL_HASH_CALC_BIT))
		return 0;

	return cls_hash_calc(fv, fv_sz, hash);
}

/**
 * @brief Preform session lookup based on the ingress packet
 * @param sess session info
 * @return s32 -EEXIST in case session was found, 0 if session wasn't
 *         found, error code otherwise
 */
static s32 __smgr_sess_lookup(struct sess_info *sess)
{
	struct pp_sess_create_args *args = sess->args;
	struct pp_fv lu_fv;
	s32 ret = 0;
	u32 id;

	/* check if the group/dst exist for multicast sessions */
	ret = smgr_mcast_sess_lookup(sess);
	if (unlikely(ret))
		goto done;

	/* convert the learning ingress fv to pp fv */
	if (SESS_INGRS_PKT(sess)) {
		ret = pp_learn_pkt_to_pp_fv(SESS_INGRS_PKT(sess), &sess->si.fv);
		if (unlikely(ret))
			goto done;
	}

	__smgr_fv_oob_set(sess);

	/* calculate the real size of the field vector */
	ret = __smgr_calc_fv_real_sz(sess);
	if (unlikely(ret))
		goto done;
	pr_debug("fv sz set to %u\n", sess->si.fv_sz);

	/* prepare field vector for the lookup, the field vector is
	 * swapped in the SI so we swap it before we lookup
	 */
	__buf_swap(&lu_fv, sizeof(lu_fv), &sess->si.fv, sess->si.fv_sz);

	ret = hash_calc(sess, &lu_fv, sess->si.fv_sz, &args->hash);
	if (unlikely(ret))
		goto done;

	/* lookup */
	ret = cls_session_lookup(&lu_fv, sess->si.fv_sz, &args->hash, &id);
	if (unlikely(ret))
		goto done;

	if (id == CLS_SESSION_INVALID)
		/* session do not exist, done */
		goto done;

	/* just make sure the result from classifier is good */
	if (unlikely(!__smgr_is_sess_exist(id))) {
		pr_err("session id %u returned from classifier does not exist\n",
		       id);
		ret = -EFAULT;
		goto done;
	}

	/* get the reference to the session entry */
	sess->db_ent = __smgr_get_sess_db_ent(id);
	ret = -EEXIST;

done:
	if (ret == (-EEXIST))
		__smgr_stats_sess_lu_succ(sess);
	else if (unlikely(ret))
		__smgr_stats_sess_lu_err(sess);
	else
		__smgr_stats_sess_lu_fail(sess);

	return ret;
}

/**
 * @brief Test if a session is supported for acceleration
 * @param sess the session
 * @return bool true in case the can be accelerated, false otherwise
 */
static bool __smgr_is_sess_supported(struct sess_info *sess)
{
	/* support no packets based sessions */
	if (!SESS_INGRS_PKT(sess) || !SESS_EGRS_PKT(sess))
		return true;

	pr_debug("ingress: outer bmap %#x, inner bmap %#x, tunnel bmap %#x\n",
		 SESS_INGRS_OUTER_BMAP(sess), SESS_INGRS_INNER_BMAP(sess),
		 SESS_INGRS_TUNN_BMAP(sess));
	pr_debug("egress: outer bmap %#x, inner bmap %#x, tunnel bmap %#x\n",
		 SESS_EGRS_OUTER_BMAP(sess), SESS_EGRS_INNER_BMAP(sess),
		 SESS_EGRS_TUNN_BMAP(sess));

	if (!__smgr_is_pkt_l3_supported(SESS_INGRS_OUTER_BMAP(sess))) {
		pr_debug("ingrss outer packet unsupported l3\n");
		goto not_supported;
	}
	if (!__smgr_is_pkt_l3_supported(SESS_EGRS_OUTER_BMAP(sess))) {
		pr_debug("egress outer packet unsupported l3\n");
		goto not_supported;
	}

	if (!__smgr_is_frag_supported(sess))
		goto not_supported;

	if (!__smgr_is_ipv6_ext_supported(sess)) {
		pr_debug("unsupported modifications in the ipv6 extension header\n");
		goto not_supported;
	}

	/* tunnel bypass? supported */
	if (__smgr_is_tunn_bypass_sess(sess)) {
		pr_debug("tunnel bypass session\n");
		return true;
	}

	/* no tunnel termination? check l4 */
	if (!__smgr_is_tunn_term_sess(sess)) {
		if (!__smgr_is_pkt_l4_supported(SESS_INGRS_OUTER_BMAP(sess))) {
			pr_debug("non tunnel termination ingrss outer packet unsupported l4\n");
			goto not_supported;
		}
		if (!__smgr_is_pkt_l4_supported(SESS_EGRS_OUTER_BMAP(sess))) {
			pr_debug("non tunnel termination egress outer packet unsupported l4\n");
			goto not_supported;
		}
		pr_debug("non tunnel termination session\n");
		return true;
	}

	/* tunnel termination */
	/* DSLITE */
	if (__smgr_is_dslite_term_sess(sess)) {
		pr_debug("dslite termination session\n");
		return true;
	}

	/* 6RD */
	if (__smgr_is_6rd_term_sess(sess)) {
		pr_debug("6rd termination session\n");
		return true;
	}

	/* EoGRE */
	if (__smgr_is_eogre_term_sess(sess)) {
		pr_debug("eogre termination session\n");
		return true;
	}

	/* IPoGRE */
	if (__smgr_is_ipogre_term_sess(sess)) {
		pr_debug("ipogre termination session\n");
		return true;
	}

	/* L2TPUDP */
	if (__smgr_is_l2tpudp_term_sess(sess)) {
		pr_debug("l2tpudp termination session\n");
		return true;
	}

	/* L2TPIP */
	if (__smgr_is_l2tpip_term_sess(sess)) {
		pr_debug("l2tpip termination session\n");
		return true;
	}

	/* don't support all other tunnels terminations */
not_supported:
	pr_debug("unsupported session\n");
	return false;
}

/**
 * @brief Prepare a session database entry for a newly created session
 * @param sess session info to save the session db entry
 * @return s32 0 on successful session entry allocation, error
 *        code otherwise
 * @note Caller MUST acquire database lock
 */
static inline s32 __smgr_sess_ent_prepare(struct sess_info *sess)
{
	struct pp_sess_create_args *args = sess->args;
	struct sess_db_entry *sess_ent;
	u32 sess_id;

	/* get the first entry from the free list */
	if (unlikely(list_empty(&db->free_list))) {
		pr_err("No free session available\n");
		return -ENOSPC;
	}
	sess_ent = DB_FREE_LIST_GET_NODE(db);
	DB_FREE_LIST_DEL_NODE(sess_ent);

	/* Reset the info struct before using it */
	sess_id = sess_ent->info.sess_id;
	memset(&sess_ent->info, 0, sizeof(sess_ent->info));

	/* Prepare the entry */
	memcpy(&sess_ent->info.hash, &args->hash, sizeof(args->hash));
	sess_ent->info.sess_id = sess_id;
	sess_ent->info.in_port = args->in_port;
	sess_ent->info.eg_port = args->eg_port;
	sess_ent->info.fv_sz = sess->si.fv_sz;
	sess_ent->info.mod_flags = *sess->mod_flags;
	sess->mod_flags = &sess_ent->info.mod_flags;
	sess->db_ent = sess_ent;

	smgr_mcast_sess_ent_prepare(sess);
	return smgr_tdox_sess_ent_prepare(sess);
}

/**
 * @brief Free session database entry back to the free list
 * @param ent session's database entry
 * @return s32 0 on successful session entry allocation, error
 *        code otherwise
 * @note Assume the function is called under session manager lock
 */
static s32 __smgr_sess_ent_free(struct sess_db_entry *ent)
{
	if (unlikely(ptr_is_null(ent)))
		return -EINVAL;

	/* Remove tdox entry if linked to this session */
	smgr_tdox_session_remove(ent->info.tdox_ent);

	/* add the session back to the free list */
	DB_FREE_LIST_ADD_NODE(db, ent);

	return 0;
}

/**
 * @brief Set session's flags, the flags will be saved in the session
 *        db entry
 * @param sess session info
 * @return s32 0 on success, error code otherwise
 */
static void __smgr_sess_flags_set(struct sess_info *sess)
{
	if (!SESS_INGRS_PKT(sess) || !SESS_EGRS_PKT(sess))
		return;

	if (memcmp(&SESS_NAT_INGRS_L2(sess)->h_dest,
		   &SESS_NAT_EGRS_L2(sess)->h_dest, ETH_ALEN))
		SESS_FLAG_SET(sess->db_ent, SESS_FLAG_ROUTED);

	if (SESS_ARGS_IS_FLAG_ON(sess, PP_SESS_FLAG_SYNCQ_BIT))
		SESS_FLAG_SET(sess->db_ent, SESS_FLAG_SYNCQ);

	/* Fragmentation should be potentialy done when adding a tunnel */
	if (pp_misc_is_nf_en(PP_NF_FRAGMENTER)) {
		if ((!SESS_INGRS_TUNN_BMAP(sess) &&
		     SESS_EGRS_TUNN_BMAP(sess)) ||
		    (__smgr_is_pppoe_encap(sess))) {
			SESS_FLAG_SET(sess->db_ent, SESS_FLAG_MTU_CHCK);
			pr_debug("SESS_FLAG_MTU_CHCK is set\n");
		}
	}

	if (SESS_ARGS_IS_FLAG_ON(sess, PP_SESS_FLAG_MCAST_GRP_BIT))
		SESS_FLAG_SET(sess->db_ent, SESS_FLAG_MCAST_GRP);
	else if (SESS_ARGS_IS_FLAG_ON(sess, PP_SESS_FLAG_MCAST_DST_BIT))
		SESS_FLAG_SET(sess->db_ent, SESS_FLAG_MCAST_DST);
}

/**
 * @brief Set the bitmaps and headers to use for NAT modifications
 * @param sess the session
 */
static inline void __smgr_nat_info_init(struct sess_info *sess)
{
	if (!SESS_INGRS_PKT(sess) || !SESS_EGRS_PKT(sess))
		return;

	/* start by setting outer packets */
	pr_debug("set both nat packets to outer\n");
	sess->nat.ingrs_bmap = SESS_INGRS_OUTER_BMAP(sess);
	sess->nat.ingrs_l2 = &SESS_INGRS_OUTER_L2(sess);
	sess->nat.ingrs_l3 = &SESS_INGRS_OUTER_L3(sess);
	sess->nat.ingrs_l4 = &SESS_INGRS_OUTER_L4(sess);
	sess->nat.egrs_bmap = SESS_EGRS_OUTER_BMAP(sess);
	sess->nat.egrs_l2 = &SESS_EGRS_OUTER_L2(sess);
	sess->nat.egrs_l3 = &SESS_EGRS_OUTER_L3(sess);
	sess->nat.egrs_l4 = &SESS_EGRS_OUTER_L4(sess);

	if (__smgr_is_iptun_decp_sess(sess)) {
		pr_debug("iptun decap: set nat ingress to inner\n");
		sess->nat.ingrs_bmap = SESS_INGRS_INNER_BMAP(sess);
		sess->nat.ingrs_l2 = &SESS_INGRS_INNER_L2(sess);
		sess->nat.ingrs_l3 = &SESS_INGRS_INNER_L3(sess);
		sess->nat.ingrs_l4 = &SESS_INGRS_INNER_L4(sess);
	} else if (__smgr_is_iptun_encp_sess(sess)) {
		pr_debug("iptun encap: set nat egress to inner\n");
		sess->nat.egrs_bmap = SESS_EGRS_INNER_BMAP(sess);
		sess->nat.egrs_l2 = &SESS_EGRS_INNER_L2(sess);
		sess->nat.egrs_l3 = &SESS_EGRS_INNER_L3(sess);
		sess->nat.egrs_l4 = &SESS_EGRS_INNER_L4(sess);
	}
}

/**
 * @brief Set all si l4 related info, in general, we "always" do l4
 *        nat to spare recipes
 * @param sess session info
 */
static void __smgr_si_napt_set(struct sess_info *sess, __sum16 l3_csum)
{
	struct fv_l4_ports *in_ports;
	struct fv_l4_ports *eg_ports;
	struct pp_si_sce *sce = &sess->si.sce;
	struct pp_si_dpu *dpu = &sess->si.dpu;
	__sum16 l4_csum = l3_csum;

	/* do napt only for TCP and UDP, TBD: ICMP */
	if (!(SESS_NAT_EGRS_BMAP(sess) & (FV_L4_UDP | FV_L4_TCP)))
		return;

	sce->valid = true;
	/* set napt flag */
	SESS_MOD_FLAG_SET(sess, MOD_L4_NAPT_BIT);
	/* set the offset to the checksum field */
	if (SESS_NAT_EGRS_BMAP(sess) & FV_L4_UDP) {
		/* UDP */
		in_ports = &SESS_NAT_INGRS_L4(sess)->udp;
		eg_ports = &SESS_NAT_EGRS_L4(sess)->udp;
		dpu->lyr_fld_off = offsetof(struct udphdr, check);
	} else {
		/* TCP */
		in_ports = &SESS_NAT_INGRS_L4(sess)->tcp;
		eg_ports = &SESS_NAT_EGRS_L4(sess)->tcp;
		dpu->lyr_fld_off = offsetof(struct tcphdr, check);
	}

	/* calc checksum for the ports */
	if (in_ports->source != eg_ports->source)
		csum_replace2(&l4_csum, in_ports->source, eg_ports->source);
	if (in_ports->dest != eg_ports->dest)
		csum_replace2(&l4_csum, in_ports->dest, eg_ports->dest);
	sce->l4_csum_delta = ntohs(l4_csum);
	/* for udp we save the csum zero value so the recipe will
	 * not calculate the csum in case it is disabled (set to zero)
	 */
	if (SESS_NAT_EGRS_BMAP(sess) & FV_L4_UDP)
		sce->l4_csum_zero = sce->l4_csum_delta;

	/* save ports */
	sce->new_dst_port = ntohs(eg_ports->dest);
	sce->new_src_port = ntohs(eg_ports->source);
}

/**
 * @brief Set all si ipv4 related info
 * @param sess session info
 */
static s32 __smgr_si_ipv4_nat_set(struct sess_info *sess)
{
	struct fv_ipv4 *in_v4 = &SESS_NAT_INGRS_L3(sess)->v4;
	struct fv_ipv4 *eg_v4 = &SESS_NAT_EGRS_L3(sess)->v4;
	struct pp_si *si = &sess->si;
	__sum16 l3_csum = 0;

	SESS_MOD_FLAG_SET(sess, MOD_IPV4_NAT_BIT);
	si->sce.valid = true;

	/* set l3 csum delta */
	if (in_v4->saddr != eg_v4->saddr)
		csum_replace4(&l3_csum, in_v4->saddr, eg_v4->saddr);
	if (in_v4->daddr != eg_v4->daddr)
		csum_replace4(&l3_csum, in_v4->daddr, eg_v4->daddr);
	/* do l4 nat */
	__smgr_si_napt_set(sess, l3_csum);
	/* ttl */
	if (__smgr_is_sess_routed(sess)) {
		si->sce.ttl_diff = in_v4->ttl - eg_v4->ttl;
		/* set IPv4 routed session ttl flag and update the l3 csum
		 * the checksum is calculated on both ttl and protocol fields
		 * and ttl is the MSB on the combined u16 word so every change
		 * in the ttl is in 0x100 granularity so decrementing ttl by 1
		 * will outcome with a change of 0x100 in the checksum
		 */
		csum_replace2(&l3_csum, htons(si->sce.ttl_diff << 8), htons(0));
	}

	/* to reduce number of recipes in the modifier we always
	 * modify the ip addresses
	 */
	si->dpu.nat_sz = FIELD_SIZEOF(struct fv_ipv4, saddr) +
			 FIELD_SIZEOF(struct fv_ipv4, daddr);
	si->bce.nat.v4.saddr = eg_v4->saddr;
	si->bce.nat.v4.daddr = eg_v4->daddr;

	/* dscp/tos */
	si->sce.dscp = eg_v4->tos;
	if (in_v4->tos != eg_v4->tos)
		csum_replace2(&l3_csum, htons(in_v4->tos), htons(eg_v4->tos));

	si->sce.l3_csum_delta = ntohs(l3_csum);
	return 0;
}

/**
 * @brief Set all si ipv6 related info
 * @note Not fully implemented
 * @param sess session info
 */
static s32 __smgr_si_ipv6_nat_set(struct sess_info *sess)
{
	struct pp_si *si = &sess->si;

	/* set IPv6 routed session hop list flag */
	SESS_MOD_FLAG_SET(sess, MOD_IPV6_HOPL_BIT);
	si->sce.valid = true;
	if (__smgr_is_sess_routed(sess))
		si->sce.ttl_diff = 1;
	else
		si->sce.ttl_diff = 0;

	/* if the source or dest address didn't changed, do nothing */
	if (!__smgr_is_ipv6_nat_addr_changed(sess))
		return 0;

	pr_debug("IPv6 NAT isn't supported\n");
	return -EPROTONOSUPPORT;
}

/**
 * @brief Add NAT info to the session SI, only L3 and L4 NAT is
 *        handled here, L2 NAT is already handled by the new header
 *        To save HW recipes, we always do NAT for IPv4
 * @param sess the session
 */
static s32 __smgr_si_nat_set(struct sess_info *sess)
{
	/* if the session is bridged and not tunnel termination,
	 * or if the session is esp encapsulation (TBD: adding esp NAT support)
	 * no nat/napt is needed
	 */
	if ((__smgr_is_sess_bridged(sess) && !__smgr_is_tunn_term_sess(sess)) ||
	    __smgr_is_esp_encp_sess(sess))
		return 0;

	if (__smgr_is_sess_bridged(sess) &&
	    __smgr_is_ip_nat_addr_changed(sess)) {
		pr_debug("Nat on bridge session isn't supported\n");
		return -EPROTONOSUPPORT;
	}

	/* clear header replacement flag as all modifications from that point
	 * on includes header replacement
	 */
	SESS_MOD_FLAG_CLR(sess, MOD_HDR_RPLC_BIT);
	/* IPv4 */
	if (__smgr_is_ipv4_nat(sess))
		return __smgr_si_ipv4_nat_set(sess);
	/* IPv6 */
	else if (__smgr_is_ipv6_nat(sess))
		return __smgr_si_ipv6_nat_set(sess);

	pr_err("unsupported l3 protocol ?\n");
	return -EPROTONOSUPPORT;
}

/**
 * @brief Update the ipv4 header template
 * @param sess session
 */
static void __smgr_newhdr_ipv4_update(struct sess_info *sess)
{
	struct pp_si *si = &sess->si;
	struct iphdr *iphdr;

	iphdr = (struct iphdr *)(si->bce.nhdr + si->dpu.nhdr_l3_off);

	iphdr->id = 0; /* will be set by the modifier */
	iphdr->tot_len = 0; /* will be set by the modifier */
	iphdr->frag_off &= htons(IP_DF); /* preserve only the DF bit    */
	ip_send_check(iphdr); /* calculate the new checksum  */
	si->sce.nhdr_csum = ntohs(iphdr->check);
	pr_debug("nhdr_csum = 0x%x l3 offset %d\n",
		 si->sce.nhdr_csum, si->dpu.nhdr_l3_off);
}

/**
 * @brief Set the new header info in the session si
 * @param sess the session
 */
static void __smgr_si_newhdr_set(struct sess_info *sess)
{
	struct pp_si_bce *bce = &sess->si.bce;
	struct pp_si_dpu *dpu = &sess->si.dpu;
	struct new_hdr *nhdr = sess->args->nhdr;

	/* if we don't have new header, done */
	if (!(nhdr && nhdr->len))
		return;

	dpu->nhdr_sz = 0;

	if (__smgr_is_iptun_encp_sess(sess)) {
		/* tunnel encapsulation ip in ip, the new header
		 * including the L2 & L3 & tunnel(optional) & inner L2(optional)
		 */
		dpu->nhdr_sz = nhdr->len;
		dpu->nhdr_l3_off = nhdr->outer_len.l2_len;
	} else if (__smgr_is_outer_l2_changed(sess)) {
		/* add the L2 */
		dpu->nhdr_sz = nhdr->outer_len.l2_len;
		dpu->nhdr_l3_off = nhdr->outer_len.l2_len;
		if (__smgr_is_esp_encp_sess(sess)) {
			/* ESP encapsulation,
			 * add the L3/UDP(optional)/ESP to new header
			 */
			dpu->nhdr_sz += nhdr->outer_len.l3_len + /* IP */
					nhdr->outer_len.l4_len + /* UDP/ESP */
					nhdr->inner_len.l4_len; /* ESP */
			if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess)))
				SESS_MOD_FLAG_SET(sess,
						  MOD_ESP_V4_TUNNL_ENCP_BIT);
			else
				SESS_MOD_FLAG_SET(sess,
						  MOD_ESP_V6_TUNNL_ENCP_BIT);
		}
	}

	/* if no new header, done. */
	if (!dpu->nhdr_sz)
		return;

	/* copy the new header to SI bce region */
	dpu->nhdr_sz = min3((size_t)dpu->nhdr_sz, sizeof(nhdr->buf),
			    sizeof(bce->nhdr));
	memcpy(bce->nhdr, nhdr->buf, dpu->nhdr_sz);

	/* if new header including ipv4, update the ip layer */
	if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess)) &&
	    (dpu->nhdr_sz > dpu->nhdr_l3_off))
		__smgr_newhdr_ipv4_update(sess);
}

/**
 * @brief Checks if the fragmentation was added to the inner egress packet
 * @param sess the session
 */
static bool __smgr_is_inner_frag_egress(struct sess_info *sess)
{
	if (SESS_EGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST)
		return true;

	return false;
}

/**
 * @brief Set the frag info in SI UD
 * @param sess the session
 */
static s32 __smgr_frag_si_ud_set(struct sess_info *sess)
{
	struct si_ud_frag_info frag_info = { 0 };
	struct pp_nf_info nf_info;
	u16 phys_q;
	s32 ret = 0;

	/* No need to set frag info */
	if (!test_bit(SESS_FLAG_MTU_CHCK, &sess->db_ent->info.flags))
		return 0;

	if (sess->args->ud_sz) {
		pr_err("Non zero UD size is not supported\n");
		return -EINVAL;
	}

	sess->si.tmpl_ud_sz = PP_TEMPLATE_UD_SZ;

	/* Set relevant flag bits for the uc */
	if (test_bit(PP_SESS_FLAG_IGNORE_INT_DFRAG_BIT, &sess->args->flags))
		frag_info.flags = FRAG_INFO_FLAG_IGNORE_INT_DF;
	if (test_bit(PP_SESS_FLAG_IGNORE_EXT_DFRAG_BIT, &sess->args->flags))
		frag_info.flags |= FRAG_INFO_FLAG_IGNORE_EXT_DF;
	if (!__smgr_is_inner_frag_egress(sess))
		frag_info.flags |= FRAG_INFO_FLAG_FRAG_EXT;

	frag_info.dst_q = sess->si.dst_q;

	if (db->frag_mode == SMGR_FRAG_MODE_CHECK_MTU_IN_UC) {
		ret = pp_nf_get(PP_NF_FRAGMENTER, &nf_info);
		if (unlikely(ret)) {
			pr_err("pp_nf_get err %d\n", ret);
			return ret;
		}

		ret = smgr_get_queue_phy_id(nf_info.q, &phys_q);
		if (unlikely(ret)) {
			pr_err("smgr_get_queue_phy_id err %d\n", ret);
			return ret;
		}

		sess->si.dst_q = phys_q;
	}

	/* Update l3 offset.
	 * In case of inner frag, take the offset from the learning
	 */
	if (frag_info.flags & FRAG_INFO_FLAG_FRAG_EXT) {
		frag_info.l3_off = sess->si.dpu.nhdr_l3_off;
		if (__smgr_is_ipv4_pkt(SESS_EGRS_OUTER_BMAP(sess))) {
			frag_info.flags |= FRAG_INFO_FLAG_IPV4;
			if (SESS_EGRS_OUTER_L3(sess).v4.frag_off & htons(IP_DF))
				frag_info.flags |= FRAG_INFO_FLAG_EXT_DF;
		}
	} else {
		frag_info.l3_off = SESS_EGRS_INNER_OFFSETS(sess).l3;
		if (__smgr_is_ipv4_pkt(SESS_EGRS_INNER_BMAP(sess))) {
			frag_info.flags |= FRAG_INFO_FLAG_IPV4;
			if (SESS_EGRS_INNER_L3(sess).v4.frag_off & htons(IP_DF))
				frag_info.flags |= FRAG_INFO_FLAG_INT_DF;
		}
	}

	if (__smgr_is_pppoe_encap(sess))
		frag_info.flags |= FRAG_INFO_FLAG_PPPOE;

	ret = pp_port_tx_max_pkt_size_get(sess->args->eg_port,
					  &frag_info.max_pkt_size);
	if (unlikely(ret)) {
		pr_err("pp_port_tx_max_pkt_size_get err %d\n", ret);
		return ret;
	}

	pr_debug("flags %#x, q %d (%d), max_pkt_size %d, l3_off %d\n",
		 frag_info.flags, frag_info.dst_q, sess->si.dst_q,
		 frag_info.max_pkt_size, frag_info.l3_off);

	memcpy(sess->si.ud + PP_PS_REGION_SZ, &frag_info, sizeof(frag_info));
	sess->si.si_ud_sz = PP_PS_REGION_SZ + sizeof(frag_info);

	return ret;
}

/**
 * @brief Set the UD info into the session si
 * @param sess the session
 */
static s32 __smgr_si_ud_set(struct sess_info *sess)
{
	if (sess->args->ps_off < PP_INVALID_PS_OFF)
		sess->si.ps_off = sess->args->ps_off;
	else
		sess->si.ps_off = PP_INVALID_PS_OFF;

	sess->si.si_ps_sz = sess->args->ps_sz;

	if (sess->args->ud_sz)
		sess->si.si_ud_sz = sess->args->ps_sz + sess->args->ud_sz;

	/* if this is a session for fragments, make sure UD will be copied
	 * for the egress UC, otherwise, use the user arguments
	 */
	if (SESS_INGRS_PKT(sess) &&
	    (SESS_INGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST ||
	     SESS_INGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST)) {
		sess->si.tmpl_ud_sz = PP_TEMPLATE_UD_SZ;
		sess->si.ps_copy = true;
	} else {
		sess->si.tmpl_ud_sz = sess->args->tmp_ud_sz;
		sess->si.ps_copy =
			test_bit(PP_SESS_FLAG_PS_COPY_BIT, &sess->args->flags);
	}

	if (sess->args->ps_sz)
		memcpy(sess->si.ud, sess->args->ps, sess->args->ps_sz);

	if (sess->args->ud_sz)
		memcpy(sess->si.ud + sess->args->ps_sz, sess->args->ud,
		       sess->args->ud_sz);

	return 0;
}

/**
 * @brief Set ingress and egress packet length diff to the session si
 * @note Need to be called after __smgr_si_newhdr_set for using
 *       the updated dpu values
 * @param sess the session
 */
static void __smgr_si_pkt_len_diff_set(struct sess_info *sess)
{
	struct pp_si_dpu *dpu = &sess->si.dpu;
	struct pp_si_sce *sce = &sess->si.sce;

	/* Calculate the ip layer tot_len/payload_len diff */
	if (__smgr_is_iptun_encp_sess(sess) || __smgr_is_esp_encp_sess(sess)) {
		/* to calculate the len diff, we use the new header values
		 * as they doesn't contains any extensions headers for v6
		 */
		sce->tot_len_diff = dpu->nhdr_sz - dpu->nhdr_l3_off;

		if (__smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			/* remove the raw ipv6 header length as it should not
			 * be included in the ipv6 payload length field
			 */
			sce->tot_len_diff -= sizeof(struct ipv6hdr);
	}

	/* Calculate the packet length diff */
	if (__smgr_is_esp_encp_sess(sess)) {
		/* for ESP encapsulation the egress header
		 * len excluded any inner headers
		 */
		sess->si.pkt_len_diff = SESS_EGRS_HDR_LEN(sess) -
					SESS_INGRS_OUTER_OFFSETS(sess).l3;
	} else {
		sess->si.pkt_len_diff = SESS_EGRS_HDR_LEN(sess) -
					SESS_INGRS_HDR_LEN(sess);
	}

	/* in case the ingress included skb vlan, add to calculation */
	if (SESS_INGRS_OUTER_BMAP(sess) & FV_L2_SKB_VLAN_TAG)
		sess->si.pkt_len_diff -= VLAN_HLEN;
}

/**
 * @brief Set flags in the session si
 * @param sess the session
 */
static void __smgr_si_flags_set(struct sess_info *sess)
{
	ulong chck_flags = SESS_CHCK_DFLT_FLAGS;
	u8 ext_proto, int_proto;

	if (test_bit(PP_SESS_FLAG_MTU_CHCK_BIT, &sess->args->flags))
		pr_info("Session mtu check flag is not supported\n");
	//		set_bit(SI_CHCK_FLAG_DVRT_MTU_OVFL, &chck_flags);

	if (!SESS_INGRS_PKT(sess) || !SESS_EGRS_PKT(sess))
		goto set_checker_flags;

	if (test_bit(SESS_FLAG_MTU_CHCK, &sess->db_ent->info.flags)) {
		if (db->frag_mode == SMGR_FRAG_MODE_CHECK_MTU_IN_CHECKER)
			set_bit(SI_CHCK_FLAG_DVRT_MTU_OVFL, &chck_flags);
	}

	/* set reassembly flags */
	if (SESS_INGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST ||
	    SESS_INGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST)
		set_bit(SI_CHCK_FLAG_DVRT_FRAG, &chck_flags);

	if ((SESS_INGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST) &&
	    !(SESS_EGRS_OUTER_BMAP(sess) & FV_L3_FRAG_1ST))
		sess->si.ext_reassembly = true;
	if ((SESS_INGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST) &&
	    !(SESS_EGRS_INNER_BMAP(sess) & FV_L3_FRAG_1ST))
		sess->si.int_reassembly = true;

	/* align the reassembly flags to the ones set by the parser in the UD
	 * reminder: the parser first set the inner flags
	 */
	ext_proto = PP_FV_OOB_EXT_PROTO_INFO_GET(sess->si.fv.oob.proto_info);
	int_proto = PP_FV_OOB_INT_PROTO_INFO_GET(sess->si.fv.oob.proto_info);

	if (ext_proto == PP_FV_UNKNOWN_L3 && sess->si.ext_reassembly) {
		sess->si.ext_reassembly = false;
		sess->si.int_reassembly = true;
	}

set_checker_flags:
	/* save flags in the si */
	sess->si.chck_flags = (u16)chck_flags;
}

/**
 * @brief Set session's PPPoE header modification info
 * @param sess the session
 */
static void __smgr_pppoe_mod_set(struct sess_info *sess)
{
	struct pp_si_sce *sce = &sess->si.sce;
	struct pp_si_dpu *dpu = &sess->si.dpu;
	struct new_hdr *nhdr = sess->args->nhdr;

	sce->valid = true;

	if (__smgr_is_pppoe_encap(sess)) {
		/* pppoe encapsulation, set the sce parameters so that
		 * the recipe will calculate the pppoe header length and
		 * will update the new header accordingly
		 */
		sce->pppoe_diff = 2 /* for the eth last protocol */;
		/* in case we have new header with ipv6 we must add
		 * must add the ipv6 header length also to the pppoe_diff
		 * cause its length field doesn't include it
		 */
		if (__smgr_is_iptun_encp_sess(sess) &&
		    __smgr_is_ipv6_pkt(SESS_EGRS_OUTER_BMAP(sess)))
			sce->pppoe_diff += sess->args->nhdr->outer_len.l3_len;

		sce->l2_org_val = 0;
		/* calculate the pppoe header offset */
		/* start with outer l3 */
		dpu->pppoe_off = nhdr->outer_len.l2_len;
		/* if the pppoe is inner, goto the inner l3 */
		if ((SESS_EGRS_INNER_BMAP(sess) & FV_L2_PPPOE)) {
			dpu->pppoe_off += nhdr->outer_len.l3_len +
					  nhdr->outer_len.l4_len +
					  nhdr->tunnel_header_len +
					  nhdr->inner_len.l2_len;
		}
		/* go back to the start of the pppoe header */
		dpu->pppoe_off -= PPPOE_SES_HLEN;
		return;
	}

	/* not pppoe encapsulation, to reduce number of recipes, we
	 * always do the pppoe length calculations, but when we don't
	 * have pppoe encapsulation, we need to set the right values
	 * so the recipe will overwrite the new header with the same
	 * value so it will remain unchanged.
	 * we read the first 2 bytes of the l2 and rewrite them
	 */
	dpu->pppoe_off = 0;
	sce->pppoe_diff = 0;
	sce->l2_org_val =
		be16_to_cpup((__be16 *)(nhdr->buf + dpu->pppoe_off +
					offsetof(struct pppoe_hdr, length)));
}

/**
 * @brief Set session's L2 modification info
 * @param sess the session
 */
static void __smgr_si_l2_mod_set(struct sess_info *sess)
{
	if (__smgr_is_outer_l2_changed(sess))
		SESS_MOD_FLAG_SET(sess, MOD_HDR_RPLC_BIT);

	if (__smgr_is_inner_pppoe_changed(sess))
		SESS_MOD_FLAG_SET(sess, MOD_INNER_PPPOE_BIT);

	__smgr_pppoe_mod_set(sess);
}

/**
 * @brief Set SI's modification info
 * @param sess the session
 */
static s32 __smgr_si_mod_info_set(struct sess_info *sess)
{
	s32 ret;

	if (!(SESS_INGRS_PKT(sess) && SESS_EGRS_PKT(sess)))
		return 0;

	if (!__smgr_is_sess_bridged(sess) && __smgr_is_frag_accl_sess(sess)) {
		pr_debug("Fragments acceleration supported only for bridge sessions\n");
		return -EPROTONOSUPPORT;
	}

	/* @todo remove to enable fragments acceleration */
	if (__smgr_is_sess_bridged(sess) && __smgr_is_frag_accl_sess(sess)) {
		pr_debug("Fragments acceleration isn't implemented yet\n");
		return -EPROTONOSUPPORT;
	}

	__smgr_si_l2_mod_set(sess);
	ret = __smgr_si_nat_set(sess);
	if (unlikely(ret))
		return ret;
	__smgr_si_newhdr_set(sess);
	__smgr_si_pkt_len_diff_set(sess);

	return ret;
}

/**
 * @brief __smgr_si_df_mask_set() - Sets dont frag mask info
 * @param si Session information
 * @param: User flags
 *
 * The function configures the DF mask bits according to the user input.
 * In frag mode checker, we want to mask the internal DF,
 * so that the checker will divert the packets to the uc anyway.
 * In that case, fragmenter uc can check where to fragment based on
 * the learning output.
 * The real value from the user is copied to the SI UD in both modes
 *
 * @return 0 on Success.
 */
static s32 __smgr_si_df_mask_set(struct pp_si *si, unsigned long flags)
{
	u32 int_df_mask = 1;

	si->ext_df_mask = test_bit(PP_SESS_FLAG_IGNORE_EXT_DFRAG_BIT, &flags);

	if (db->frag_mode == SMGR_FRAG_MODE_CHECK_MTU_IN_UC) {
		int_df_mask = test_bit(PP_SESS_FLAG_IGNORE_INT_DFRAG_BIT,
				       &flags);
	}

	si->int_df_mask = int_df_mask;

	return 0;
}

/**
 * @brief Create an si structure for the HW based on the specified
 *        session
 * @param sess_info the session
 * @return s32 0 on success, error code otherwise
 */
static s32 __smgr_si_create(struct sess_info *sess)
{
	struct pp_sess_create_args *args;
	struct pp_si *si;
	s32 ret;

	args = sess->args;
	si = &sess->si;

	/* set session id first, cause some of the below APIs use it */
	si->sess_id = sess->db_ent->info.sess_id;

	ret = smgr_get_queue_phy_id(sess->args->dst_q, &sess->si.dst_q);
	if (unlikely(ret))
		goto done;
	ret = pp_si_sgc_info_set(args->sgc, ARRAY_SIZE(args->sgc), si);
	if (unlikely(ret))
		goto done;
	ret = pp_si_tbm_info_set(args->tbm, ARRAY_SIZE(args->tbm), si);
	if (unlikely(ret))
		goto done;
	ret = pp_si_port_info_set(args->in_port, args->eg_port, &sess->si);
	if (unlikely(ret))
		goto done;
	ret = __smgr_si_mod_info_set(sess);
	if (unlikely(ret))
		goto done;
	ret = __smgr_si_df_mask_set(si, args->flags);
	if (unlikely(ret))
		goto done;

	si->color = args->color;
	si->fsqm_prio = args->fsqm_prio;
	si->bce_ext = false; /* not supported in PPv4 */
	si->trim_l3_id = SI_NO_TRIM_VALUE;

	if (SESS_ARGS_IS_FLAG_ON(sess, PP_SESS_FLAG_LRO_INFO_BIT)) {
		si->tdox_flow = args->lro_info;
	} else if (SESS_IS_FLAG_ON(sess->db_ent, SESS_FLAG_TDOX)) {
		if (unlikely(ptr_is_null(sess->db_ent->info.tdox_ent))) {
			pr_err("Session %u is missing Tdox entry in db\n",
			       sess->db_ent->info.sess_id);
			ret = -EINVAL;
			goto done;
		}
		si->tdox_flow = sess->db_ent->info.tdox_ent->tdox_id;
	} else {
		si->tdox_flow = SMGR_INVALID_TDOX_SESS_ID;
	}

	__smgr_si_flags_set(sess);

	ret = __smgr_si_ud_set(sess);
	if (unlikely(ret))
		goto done;

	ret = __smgr_frag_si_ud_set(sess);
	if (unlikely(ret))
		goto done;

	ret = mod_recipe_select(sess->db_ent->info.mod_flags,
				&sess->si.recipe_idx);
	if (unlikely(ret))
		goto done;

	ret = pp_si_encode(&sess->hw_si, &sess->si);

	/* si encoding might change the field vector size so we must save
	 * the latest one
	 */
	sess->db_ent->info.fv_sz = si->fv_sz;
done:
	if (unlikely(ret)) {
		pr_err("failed to create si\n");
		__smgr_stats_sess_si_create_err(sess);
	}
	return ret;
}

/**
 * @brief Track session in relevant bitmaps and lists
 * @param sess session to track
 */
static inline void __smgr_sess_track(struct sess_info *sess)
{
	set_bit(sess->db_ent->info.sess_id, db->open_bmap);
	DB_IN_PORT_LIST_ADD_NODE(db, sess->db_ent);
	DB_EG_PORT_LIST_ADD_NODE(db, sess->db_ent);
	smgr_mcast_sess_track(sess);
}

/**
 * @brief Create a PP session, in case the session will be created,
 *        'sess' structure will be filled with all the session
 *        information like session id, db entry, si and so on.
 * @param sess session info
 * @return s32 -EEXIST in case session already exist, 0 on success
 *         creation, error code otherwise
 */
static s32 __smgr_sess_create(struct sess_info *sess)
{
	s32 ret = 0;

	if (unlikely(ptr_is_null(sess)))
		return -EINVAL;

	if (unlikely(!__smgr_is_sess_args_valid(sess->args))) {
		ret = -EINVAL;
		goto err;
	}

	if (unlikely(!__smgr_is_sess_supported(sess))) {
		ret = -EPROTONOSUPPORT;
		goto err;
	}

	__smgr_lock();
	ret = __smgr_sess_lookup(sess);
	if (ret == -EEXIST)
		goto unlock;
	else if (unlikely(ret))
		goto err_unlock;

	ret = __smgr_sess_ent_prepare(sess);
	if (unlikely(ret))
		goto err_unlock;

	__smgr_nat_info_init(sess);
	__smgr_sess_flags_set(sess);

	ret = __smgr_si_create(sess);
	if (unlikely(ret))
		goto err_unlock;

	smgr_sq_alloc(sess);
	ret = cls_session_add(&sess->hw_si, &sess->args->hash);
	if (unlikely(ret)) {
		pr_err("failed to create session in HW\n");
		__smgr_stats_hw_err();
		goto err_unlock;
	}
	smgr_sq_start(sess);
	smgr_tdox_record_create(sess->db_ent);

	pr_debug("session %u created\n", sess->db_ent->info.sess_id);
	__smgr_stats_sess_created(sess);

	__smgr_sess_track(sess);

	__smgr_sess_debug(sess);
unlock:
	__smgr_unlock();
	return ret;

err_unlock:
	/* if we already allocated a session entry, free it */
	if (sess->db_ent) {
		__smgr_sess_ent_free(sess->db_ent);
		sess->db_ent = NULL;
	}
	__smgr_unlock();
err:
	pr_debug("session create failed, ret = %d\n", ret);
	if (ret == -EPROTONOSUPPORT)
		__smgr_stats_sess_no_supported(sess);
	__smgr_stats_sess_cr_failed(sess);
	return ret;
}

s32 smgr_session_update(u32 sess_id, struct pp_hw_si *hw_si)
{
	struct sess_db_entry *ent;
	struct sess_db_info *info;
	s32 ret = 0;

	pr_debug("updating session %u\n", sess_id);

	__smgr_lock();

	if (unlikely(!__smgr_is_sess_exist(sess_id))) {
		__smgr_stats_invalid_args();
		ret = -ENOENT;
		goto done;
	}

	ent = __smgr_get_sess_db_ent(sess_id);
	if (unlikely(!ent)) {
		pr_crit("Session exist with DB no entry!!!\n");
		ret = -ENOENT;
		goto done;
	}
	info = &ent->info;

	/* Update session in classifier */
	ret = cls_session_update(sess_id, info->fv_sz, &info->hash, hw_si);
	if (unlikely(ret)) {
		__smgr_stats_hw_err();
		pr_err("update session %u from HW failed\n", sess_id);
		goto done;
	}

	pr_debug("session %u was updated\n", sess_id);

done:
	if (unlikely(ret)) {
		pr_err("Failed to update session %u, ret = %d\n",
		       sess_id, ret);
		__smgr_stats_sess_update_failed();
	}

	__smgr_unlock();

	return ret;
}

/**
 * @brief Session create work to be schedule using a work queue in
 *        async mode or to be called directly in sync mode
 * @param work work to do
 */
static void __smgr_sess_create_work(struct work_struct *work)
{
	struct pp_sess_create_cb_args *cb_args;
	struct sess_cr_work_args *_work;
	struct sess_info sess;
	s32 ret;
	ulong tmp_mod_flags; /* temp modification flags to use before
			      * allocating session entry
			      */

	if (unlikely(ptr_is_null(work)))
		return;

	_work = container_of(work, struct sess_cr_work_args, base.dwork.work);
	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		ret = -EPERM;
		goto callback;
	}

	memset(&sess, 0, sizeof(sess));
	tmp_mod_flags = 0;
	sess.args = _work->args;
	sess.mod_flags = &tmp_mod_flags;

	/* try to create the session */
	ret = __smgr_sess_create(&sess);

callback:
	/* ref to callback args struct */
	cb_args = &_work->cb_args;
	cb_args->base.ret = ret;
	if (likely(!ret))
		cb_args->sess_id = sess.db_ent->info.sess_id;

	/* set callback parameters and notify user and free the memory,
	 * in sync request, when callback is null,
	 * the memory is freed by pp_session_create
	 */
	if (_work->base.cb) {
		cb_args->base.req_id = _work->base.req_id;
		cb_args->base.ev = PP_SESS_CREATE;
		_work->base.cb(&cb_args->base);
		kmem_cache_free(db->work_args_cache, _work);
	}
}

/**
 * @brief Prepare session create arguments for session create work
 * @param req session create request
 * @return struct sess_cr_args* valid session create args pointer if
 *         available, NULL otherwise
 */
static struct sess_cr_work_args *
__smgr_sess_cr_args_prepare(struct pp_request *req,
			    struct pp_sess_create_args *sess_args)
{
	struct sess_cr_work_args *work_args = NULL;

	pr_pp_req(req, "");

	work_args = (struct sess_cr_work_args *)
		__smgr_work_args_prepare(req, __smgr_sess_create_work);
	if (unlikely(!work_args))
		return work_args;

	/* set arguments */
	if (PP_IS_SYNC_REQUEST(req)) {
		work_args->args = sess_args;
		return work_args;
	}

	/* in async mode, we must copy all arguments */
	memcpy(&work_args->args_buf, sess_args, sizeof(*sess_args));
	work_args->args = &work_args->args_buf;

	if (sess_args->nhdr) {
		memcpy(&work_args->nhdr, sess_args->nhdr,
		       sizeof(*sess_args->nhdr));
		work_args->args->nhdr = &work_args->nhdr;
	}
	if (sess_args->in_pkt) {
		memcpy(&work_args->in_pkt, sess_args->in_pkt,
		       sizeof(*sess_args->in_pkt));
		work_args->args->in_pkt = &work_args->in_pkt;
	}
	if (sess_args->eg_pkt) {
		memcpy(&work_args->eg_pkt, sess_args->eg_pkt,
		       sizeof(*sess_args->eg_pkt));
		work_args->args->eg_pkt = &work_args->eg_pkt;
	}

	return work_args;
}

s32 pp_session_create(struct pp_sess_create_args *args, u32 *sess_id,
		      struct pp_request *req)
{
	struct sess_cr_work_args *wargs;
	s32 ret = 0;

	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		return -EPERM;
	}

	__smgr_stats_sess_cr_req();
	if (unlikely(ptr_is_null(args))) {
		__smgr_stats_invalid_args();
		__smgr_stats_sess_cr_failed(NULL);
		return -EINVAL;
	}

	/* in sync request, user must provide a pointer to save the session id*/
	if (unlikely(PP_IS_SYNC_REQUEST(req) && !sess_id)) {
		pr_err("sess_id null pointer in sync mode\n");
		__smgr_stats_invalid_args();
		__smgr_stats_sess_cr_failed(NULL);
		return -EINVAL;
	}

	wargs = __smgr_sess_cr_args_prepare(req, args);
	if (unlikely(!wargs)) {
		__smgr_stats_sess_cr_failed(NULL);
		return -ENOMEM;
	}

	__smgr_dispatch_work(req, &wargs->base, 0);

	/* sync request - copy the session id back to the user's pointer
	 * and return the return code
	 */
	if (PP_IS_SYNC_REQUEST(req)) {
		*sess_id = wargs->cb_args.sess_id;
		ret = wargs->cb_args.base.ret;
		kmem_cache_free(db->work_args_cache, wargs);
	}

	return ret;
}
EXPORT_SYMBOL(pp_session_create);

/* ========================================================================== */
/*                               Session Delete                               */
/* ========================================================================== */
/**
 * @brief Untrack session from relevant bitmaps and lists
 * @param sess session to untrack
 */
static inline void __smgr_sess_untrack(struct sess_db_entry *ent)
{
	clear_bit(ent->info.sess_id, db->open_bmap);
	DB_IN_PORT_LIST_DEL_NODE(ent);
	DB_EG_PORT_LIST_DEL_NODE(ent);
	smgr_mcast_sess_untrack(ent);
}

/**
 * @brief Delete an existing PP session
 * @note MUST called with lock held
 * @param sess_id session id to delete
 * @return s32 0 on success deletion, error code otherwise
 */
static s32 __smgr_sess_delete(u32 sess_id)
{
	struct sess_db_entry *ent;
	struct sess_db_info *info;
	struct pp_si si;
	s32 ret = 0;
	u32 i;

	pr_debug("deleting session %u\n", sess_id);
	if (unlikely(!__smgr_is_sess_exist(sess_id))) {
		__smgr_stats_invalid_args();
		ret = -ENOENT;
		goto done;
	}

	ent = __smgr_get_sess_db_ent(sess_id);
	if (unlikely(!ent)) {
		pr_crit("Session exist with DB no entry!!!\n");
		ret = -ENOENT;
		goto done;
	}
	info = &ent->info;

	/* detach the sync queue entry (if exist) */
	smgr_sq_del(ent);

	/* Get session SI */
	ret = smgr_session_si_get(sess_id, &si);
	if (unlikely(ret)) {
		__smgr_stats_hw_err();
		pr_err("failed to get session %u SI\n", sess_id);
		goto done;
	}

	/* detach the session from all the SGCs */
	for (i = 0; i < ARRAY_SIZE(si.sgc); i++)
		if (si.sgc[i] != PP_SGC_INVALID)
			chk_sgc_session_detach(i, si.sgc[i]);

	/* detach the session from all the TBMs */
	for (i = 0; i < ARRAY_SIZE(si.tbm); i++) {
		if (si.tbm[i] != PP_TBM_INVALID)
			chk_dual_tbm_session_detach(si.tbm[i]);
	}

	ret = cls_session_del(sess_id, info->fv_sz, &info->hash);
	if (unlikely(ret)) {
		__smgr_stats_hw_err();
		if (ret == -ENOENT)
			pr_err("session %u doesn't exist in HW!!!\n", sess_id);
		else
			pr_err("delete session %u from HW failed\n", sess_id);
		goto done;
	}

	__smgr_stats_sess_deleted(ent);
	__smgr_sess_untrack(ent);
	__smgr_sess_ent_free(ent);
	pr_debug("session %u deleted\n", sess_id);

done:
	if (unlikely(ret)) {
		pr_err("Failed to delete session %u, ret = %d\n",
		       sess_id, ret);
		__smgr_stats_sess_del_failed();
	}
	return ret;
}

/**
 * @brief Session delete work to be schedule using a work queue in
 *        async mode or to be called directly in sync mode
 * @param work work to do
 */
static void __smgr_sess_delete_work(struct work_struct *work)
{
	struct pp_sess_delete_cb_args *cb_args;
	struct sess_del_work_args *_work;
	s32 ret;

	if (unlikely(ptr_is_null(work)))
		return;

	_work = container_of(work, struct sess_del_work_args, base.dwork.work);
	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		ret = -EPERM;
		goto callback;
	}

	/* try to delete the session */
	__smgr_lock();
	ret = __smgr_sess_delete(_work->sess_id);
	__smgr_unlock();

callback:
	cb_args = &_work->cb_args;
	cb_args->base.ret = ret;
	cb_args->sess_id = _work->sess_id;

	/* set callback parameters and notify user and free the memory,
	 * in sync request, when callback is null,
	 * the memory is freed by pp_session_delete
	 */
	if (_work->base.cb) {
		cb_args->base.req_id = _work->base.req_id;
		cb_args->base.ev = PP_SESS_DELETE;
		_work->base.cb(&cb_args->base);
		kmem_cache_free(db->work_args_cache, _work);
	}
}

/**
 * @brief Prepare session delete arguments for the work function
 * @return struct sess_del_work_args* valid session delete args pointer if
 *         available, NULL otherwise
 */
static struct sess_del_work_args *
__smgr_sess_del_args_prepare(struct pp_request *req, u32 sess_id)
{
	struct sess_del_work_args *args = NULL;

	pr_pp_req(req, "sess_id %u\n", sess_id);

	args = (struct sess_del_work_args *)
		__smgr_work_args_prepare(req, __smgr_sess_delete_work);
	if (unlikely(!args))
		return args;

	/* set arguments */
	args->sess_id = sess_id;

	return args;
}

/**
 * @brief delete pp session
 * @param req session delete request
 * @return s32 return 0 for success
 */
s32 pp_session_delete(u32 sess_id, struct pp_request *req)
{
	struct sess_del_work_args *wargs;
	s32 ret = 0;

	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		return -EPERM;
	}

	__smgr_stats_sess_del_req();
	wargs = __smgr_sess_del_args_prepare(req, sess_id);
	if (unlikely(!wargs)) {
		__smgr_stats_sess_del_failed();
		return -ENOMEM;
	}

	__smgr_dispatch_work(req, &wargs->base, 0);

	/* sync request - return the real return code and free the memory */
	if (PP_IS_SYNC_REQUEST(req)) {
		ret = wargs->cb_args.base.ret;
		kmem_cache_free(db->work_args_cache, wargs);
	}

	return ret;
}
EXPORT_SYMBOL(pp_session_delete);

/* ========================================================================== */
/*                            Port Sessions Flush                             */
/* ========================================================================== */
/**
 * @brief Flush port's sessions
 * @note MUST called with lock held
 * @param pid port id, use 'PP_MAX_PORT' to flush all ports
 * @return s32 0 on success, error code otherwise
 */
static s32 __smgr_port_flush(u16 pid)
{
	struct sess_db_entry *ent, *tmp;
	u16 start, end, i;
	bool flush_all;
	s32 ret = 0;

	flush_all = (pid == PP_MAX_PORT);
	if (flush_all) {
		start = 0;
		end = PP_MAX_PORT - 1;
	} else {
		if (unlikely(!pmgr_port_is_active(pid))) {
			pr_err("port %u isn't active\n", pid);
			__smgr_stats_invalid_args();
			ret = -EFAULT;
			goto done;
		}
		start = pid;
		end = pid;
	}

	pr_debug("pid %u, start %u, end %u\n", pid, start, end);

	for (i = start; i <= end; i++) {
		if (!pmgr_port_is_active(i))
			continue;

		pr_debug("flushing port %u\n", i);
		/* flush ingress port sessions */
		list_for_each_entry_safe (ent, tmp, &db->in_port[i],
					  in_port_node) {
			ret = __smgr_sess_delete(ent->info.sess_id);
			if (unlikely(ret))
				pr_err("failed to delete port %u session id %u\n",
				       i, ent->info.sess_id);
		}
		/* flush egress port sessions */
		list_for_each_entry_safe (ent, tmp, &db->eg_port[i],
					  eg_port_node) {
			ret = __smgr_sess_delete(ent->info.sess_id);
			if (unlikely(ret))
				pr_err("failed to delete port %u session id %u\n",
				       i, ent->info.sess_id);
		}

		pr_debug("port %u flushed\n", i);
		if (!flush_all)
			__smgr_stats_port_flushed();
	}
	if (flush_all)
		__smgr_stats_flush_all_done();
	return ret;

done:
	if (flush_all)
		__smgr_stats_flush_all_failed();
	else
		__smgr_stats_port_flush_failed();
	pr_err("fail to flush port %u sessions, ret = %d\n", pid, ret);
	return ret;
}

/**
 * @brief Flush all ports work to be schedule using a work queue in
 *        async mode or to be called directly in sync mode
 * @param work work to do
 */
static void __smgr_port_flush_work(struct work_struct *work)
{
	struct pp_port_flush_cb_args *cb_args;
	struct port_flush_work_args *_work;
	s32 ret;

	if (unlikely(ptr_is_null(work)))
		return;

	_work = container_of(work, struct port_flush_work_args,
			     base.dwork.work);
	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		ret = -EPERM;
		goto callback;
	}

	/* try to flush the port's sessions */
	__smgr_lock();
	ret = __smgr_port_flush(_work->port_id);
	__smgr_unlock();

callback:
	cb_args = &_work->cb_args;
	cb_args->port_id = _work->port_id;
	cb_args->base.ret = ret;

	/* set callback parameters and notify user and free the memory,
	 * in sync request, when callback is null,
	 * the memory is freed by pp_port_flush or pp_flush_all
	 */
	if (_work->base.cb) {
		cb_args->base.req_id = _work->base.req_id;
		cb_args->base.ev = (_work->port_id == PP_MAX_PORT) ?
					   PP_FLUSH_ALL :
					   PP_PORT_FLUSH;
		_work->base.cb(&cb_args->base);
		kmem_cache_free(db->work_args_cache, _work);
	}
}

/**
 * @brief Prepare port flush arguments for the work function
 * @param req async request
 * @param port_id port id to flush
 * @return struct port_flush_work_args* valid sport flush args
 *         pointer if available, NULL otherwise
 */
static struct port_flush_work_args *
__smgr_port_flush_args_prepare(struct pp_request *req, u16 port_id)
{
	struct port_flush_work_args *args = NULL;

	pr_pp_req(req, "port_id %u\n", port_id);

	args = (struct port_flush_work_args *)
		__smgr_work_args_prepare(req, __smgr_port_flush_work);
	if (unlikely(!args))
		return args;

	/* set arguments */
	args->port_id = port_id;

	return args;
}

s32 pp_port_flush(u16 port_id, struct pp_request *req)
{
	struct port_flush_work_args *wargs;
	s32 ret = 0;

	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		return -EPERM;
	}

	__smgr_stats_port_flush_req();
	wargs = __smgr_port_flush_args_prepare(req, port_id);
	if (unlikely(!wargs)) {
		__smgr_stats_port_flush_failed();
		return -ENOMEM;
	}

	__smgr_dispatch_work(req, &wargs->base, 0);

	/* sync request - return the real return code and free the memory */
	if (PP_IS_SYNC_REQUEST(req)) {
		ret = wargs->cb_args.base.ret;
		kmem_cache_free(db->work_args_cache, wargs);
	}

	return ret;
}
EXPORT_SYMBOL(pp_port_flush);

/* ========================================================================== */
/*                            Flush All Sessions                              */
/* ========================================================================== */
s32 pp_flush_all(struct pp_request *req)
{
	struct port_flush_work_args *wargs;
	s32 ret = 0;

	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		return -EPERM;
	}

	__smgr_stats_flush_all_req();
	/* we use port flush with PP_MAX_PORT to flush all ports */
	wargs = __smgr_port_flush_args_prepare(req, PP_MAX_PORT);
	if (unlikely(!wargs)) {
		__smgr_stats_flush_all_failed();
		return -ENOMEM;
	}

	__smgr_dispatch_work(req, &wargs->base, 0);

	/* sync request - return the real return code and free the memory */
	if (PP_IS_SYNC_REQUEST(req)) {
		ret = wargs->cb_args.base.ret;
		kmem_cache_free(db->work_args_cache, wargs);
	}

	return ret;
}
EXPORT_SYMBOL(pp_flush_all);

/* ========================================================================== */
/*                            Inactive List Sessions                          */
/* ========================================================================== */

/**
 * @brief Convert stalled sessions bitmap created by the crawler into
 *        an array with real sessions indexes, we can start converting
 *        the sessions only from the second scheduled work so we scan
 *        the previous chunk
 * @param w what to do
 */
static void __smgr_inactive_sess_list_get(struct inactive_work_args *w)
{
	ulong *map;
	u32 id;

	map = db->inact_bmap;
	id = w->base_id - w->it_sz;
	pr_debug("converting sessions [%u - %u)\n", id, w->base_id);

	for_each_set_bit_from (id, map, w->n_sessions) {
		w->inact[w->inact_found++] = id;
		/* did we use all entries ?*/
		if (w->inact_found == w->inact_n || id == w->base_id)
			break;
	}
}

/**
 * @brief Start scanning for inactive session in a specific block
 *        specified by the work argument and schedule next iteration
 *        work
 * @param w what to scan
 * @return s32 0 on success, error code otherwise
 */
static inline s32 __smgr_inactive_scan_start(struct inactive_work_args *w)
{
	s32 ret = 0;

	pr_debug("Start crawler scan for sessions %u - %u\n",
		 w->base_id, w->base_id + w->it_sz - 1);

	/* start crawler */
	ret = chk_crawler_start(w->base_id, w->it_sz, w->bmap_phys, w->bmap_sz);
	if (unlikely(ret)) {
		__smgr_stats_hw_err();
		pr_err("failed to start crawler scan, ret %d\n", ret);
	}

	return ret;
}

/**
 * @brief Get inactive sessions work to be schedule using a work queue
 *        in async mode or to be called directly in sync mode
 * @param work work to do
 */
static void __smgr_inactive_work(struct work_struct *work)
{
	struct pp_inactive_list_cb_args cb_args = { 0 };
	struct inactive_work_args *w;
	struct pp_request req;
	u32 tries = 0;
	s32 ret;

	if (unlikely(ptr_is_null(work)))
		return;

	w = container_of(work, struct inactive_work_args, base.dwork.work);
	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		ret = -EPERM;
		goto callback;
	}

	/* wait to crawler to finish */
	ret = chk_crawler_idle_wait(&tries);
	atomic_add(tries, &db->stats.crwlr_idle_wait);
	if (unlikely(ret)) {
		__smgr_stats_hw_err();
		goto callback;
	}

	/* did we scan all sessions? */
	if (w->base_id < w->n_sessions) {
		ret = __smgr_inactive_scan_start(w);
		if (unlikely(ret))
			goto callback;
	}

	/* from second work and above, while the checker is scanning the new
	 * chunk, we populate the user's array with sessions indexes we found
	 * so far
	 */
	if (w->base_id)
		__smgr_inactive_sess_list_get(w);

	/* do we need to schedule more works? */
	if (w->base_id < w->n_sessions && w->inact_found < w->inact_n) {
		/* prepare work for next scan */
		w->base_id += w->it_sz;
		/* create dummy request */
		req.cb = w->base.cb;
		req.req_id = w->base.req_id;
		req.req_prio = w->base.req_prio;
		/* dispatch */
		pr_debug("Scheduling next scan, base_id %u\n", w->base_id);
		__smgr_dispatch_work(&req, &w->base, w->delay);
		return;
	}

	__smgr_stats_inactive_done();
	ret = 0;

callback:
	cb_args.inact_sess = w->inact;
	cb_args.n_sessions = w->inact_found;
	cb_args.base.ret = ret;
	cb_args.base.req_id = w->base.req_id;
	cb_args.base.ev = PP_INACTIVE_LIST;
	w->base.cb(&cb_args.base);
	/* free memory and restore scan state back to idle */
	kmem_cache_free(db->work_args_cache, w);
	__smgr_lock();
	db->scan_state = INACT_SCAN_IDLE;
	__smgr_unlock();
}

/**
 * @brief Prepare getting inactive sessions list work arguments
 * @return struct inactive_work_args* valid pointer if available, NULL
 *         otherwise
 */
static struct inactive_work_args *
__smgr_inactive_args_prepare(struct pp_request *req, u32 *inact, u32 inact_sz)
{
	struct inactive_work_args *args = NULL;
	u32 delay;
	u32 it_sz;

	it_sz = min(CHK_CRWLR_MAX_ITR_SIZE, db->n_sessions);
	/* scan_time_get will also fix it_sz to be the real number of
	 * session that will be scanned in each iteration
	 */
	if (chk_scan_time_get(&it_sz, &delay))
		return NULL;

	pr_pp_req(req, "inact %p, inact_sz %u, it_sz %u, delay %u us\n",
		  inact, inact_sz, it_sz, delay);

	args = (struct inactive_work_args *)
		__smgr_work_args_prepare(req, __smgr_inactive_work);
	if (unlikely(!args))
		return args;

	/* set arguments for scanning all sessions */
	args->base_id = 0;
	args->n_sessions = db->n_sessions;
	args->it_sz = it_sz;
	args->bmap_phys = db->inact_bmap_phys;
	args->bmap_sz = db->inact_bmap_sz;
	args->inact = inact;
	args->inact_n = inact_sz;
	args->inact_found = 0;
	args->delay = (ulong)delay;

	return args;
}

s32 pp_inactive_sessions_get(struct pp_request *req, u32 *inact, u32 n)
{
	struct inactive_work_args *wargs;
	s32 ret = 0;

	if (unlikely(__smgr_is_disable())) {
		pr_err("PP disabled\n");
		return -EPERM;
	}

	__smgr_stats_inactive_req();
	if (unlikely(!(!ptr_is_null(req) &&
		       !ptr_is_null(req->cb) &&
		       !ptr_is_null(inact)))) {
		__smgr_stats_invalid_args();
		ret = -EINVAL;
		goto done;
	}
	if (unlikely(n == 0)) {
		pr_err("zero array size\n");
		__smgr_stats_invalid_args();
		ret = -EINVAL;
		goto done;
	}

	__smgr_lock();
	if (db->scan_state == INACT_SCAN_BUSY) {
		pr_err("scan in progress, cannot start new one\n");
		__smgr_stats_resource_busy();
		ret = -EBUSY;
		__smgr_unlock();
		goto done;
	}
	db->scan_state = INACT_SCAN_BUSY;
	__smgr_unlock();

	memset(db->inact_bmap, 0, db->inact_bmap_sz); /* init bitmap */
	wargs = __smgr_inactive_args_prepare(req, inact, n);
	if (unlikely(!wargs)) {
		ret = -ENOMEM;
		__smgr_lock();
		db->scan_state = INACT_SCAN_IDLE;
		__smgr_unlock();
		goto done;
	}
	__smgr_dispatch_work(req, &wargs->base, 0);

done:
	if (unlikely(ret))
		__smgr_stats_inactive_failed();

	return ret;
}
EXPORT_SYMBOL(pp_inactive_sessions_get);

/* ========================================================================== */
/*                            Session Group Counters                          */
/* ========================================================================== */

s32 smgr_sgc_sessions_bmap_get(u8 grp, u16 cntr, ulong *bmap, u32 n_bits)
{
	struct pp_si si = { 0 };
	u32 sess;
	s32 ret;

	if (unlikely(ptr_is_null(bmap) || !n_bits))
		return -EINVAL;

	if (unlikely(!chk_sgc_is_group_valid(grp)))
		return -EINVAL;

	/* get all opened sessions */
	ret = smgr_open_sessions_bmap_get(bmap, n_bits);
	if (unlikely(ret))
		return ret;

	/* iterate over all sessions and check who is using the sgc */
	for_each_set_bit (sess, bmap, n_bits) {
		ret = smgr_session_si_get(sess, &si);
		if (unlikely(ret)) {
			pr_err("failed to get session %u si, ret %d\n", sess,
			       ret);
			return ret;
		}

		/* if no counter is used in the group or it is no
		 * the counter, clear the session bit
		 */
		if (!test_bit(grp, &si.sgc_en_map) || si.sgc[grp] != cntr)
			clear_bit(sess, bmap);
	}

	return 0;
}

s32 pp_sgc_sessions_get(u8 grp, u16 cntr, u32 *sessions, u32 *n_sessions)
{
	ulong *bmap;
	u32 sess, n_bits, i;
	s32 ret;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(ptr_is_null(sessions) || ptr_is_null(n_sessions)))
		return -EINVAL;

	/* allocate bitmap for all sessions */
	ret = smgr_sessions_bmap_alloc(&bmap, &n_bits);
	if (unlikely(ret))
		return ret;

	/* get all sgc sessions in bitmap form */
	ret = smgr_sgc_sessions_bmap_get(grp, cntr, bmap, n_bits);
	if (unlikely(ret))
		goto done;

	/* convert the bitmap into indexes */
	i = 0;
	for_each_set_bit (sess, bmap, n_bits) {
		/* break if we reach the end of the user array */
		if (*n_sessions <= i)
			break;
		sessions[i++] = sess;
	}
	*n_sessions = i;

done:
	kfree(bmap);
	return ret;
}

s32 pp_sgc_group_reserve(u8 grp, u32 *owner)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	return chk_sgc_group_reserve(grp, owner);
}

s32 pp_sgc_group_unreserve(u8 grp, u32 owner)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	return chk_sgc_group_unreserve(grp, owner);
}

s32 pp_sgc_alloc(u32 owner, u8 grp, u16 *counters, u16 count)
{
	s32 ret;
	u32 i;

	if (unlikely(!pp_is_ready()))
		return -EPERM;
	if (unlikely(ptr_is_null(counters)))
		return -EINVAL;
	if (unlikely(count == 0)) {
		pr_err("cannot allocate 0 counters!\n");
		return -EINVAL;
	}

	/* try to allocate */
	for (i = 0; i < count; i++) {
		ret = chk_sgc_alloc(owner, grp, &counters[i]);
		if (likely(ret == 0))
			continue;
		pr_err("failed to allocate SGC counter on group %u\n", grp);
		goto alloc_fail;
	}

	return ret;
alloc_fail:
	/* free the ones we manage to allocate */
	while (i--)
		chk_sgc_free(owner, grp, counters[i]);

	return ret;
}
EXPORT_SYMBOL(pp_sgc_alloc);

s32 pp_sgc_free(u32 owner, u8 grp, u16 *counters, u16 count)
{
	s32 ret;
	u32 i;

	if (unlikely(!pp_is_ready()))
		return -EPERM;
	if (unlikely(ptr_is_null(counters)))
		return -EINVAL;
	if (unlikely(count == 0)) {
		pr_err("cannot free 0 counters!\n");
		return -EINVAL;
	}

	/* free the counters */
	for (i = 0; i < count; i++) {
		ret = chk_sgc_free(owner, grp, counters[i]);
		if (unlikely(ret))
			pr_err("failed to free SGC counter [%u][%04u]\n",
			       grp, counters[i]);
	}

	return ret;
}
EXPORT_SYMBOL(pp_sgc_free);

s32 pp_sgc_get(u8 grp, u16 sgc_id, struct pp_stats *stats, u32 *num_sessions)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;
	if (unlikely(ptr_is_null(stats)))
		return -EINVAL;

	return chk_sgc_get(grp, sgc_id, &stats->packets, &stats->bytes,
			   num_sessions);
}
EXPORT_SYMBOL(pp_sgc_get);

s32 pp_sgc_mod(u32 owner, u8 grp, u16 sgc_id, enum pp_stats_op op, u32 pkts,
	       u32 bytes)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	return chk_sgc_mod(owner, grp, sgc_id, op, pkts, bytes);
}
EXPORT_SYMBOL(pp_sgc_mod);

/* ========================================================================== */
/*                            Token Bucket Meter                              */
/* ========================================================================== */

s32 pp_dual_tbm_alloc(u16 *idx, struct pp_dual_tbm *cfg)
{
	s32 ret;
	u16 _idx;

	if (unlikely(!pp_is_ready()))
		return -EPERM;

	if (unlikely(ptr_is_null(idx)))
		return -EINVAL;

	ret = chk_dual_tbm_alloc(&_idx);
	if (unlikely(ret)) {
		pr_err("failed to allocate dual tbm, ret %d\n", ret);
		return ret;
	}

	/* if cfg isn't provided, we are done */
	if (!cfg)
		goto done;

	/* cfg provided, try to set the tbm configuration */
	ret = chk_dual_tbm_set(_idx, cfg);
	if (unlikely(ret)) {
		pr_err("failed to set tbm %u configuration, ret %d\n", _idx,
		       ret);
		chk_dual_tbm_free(_idx);
		return ret;
	}

done:
	*idx = _idx;
	return ret;
}

s32 pp_dual_tbm_free(u16 idx)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	return chk_dual_tbm_free(idx);
}

s32 pp_dual_tbm_get(u16 idx, struct pp_dual_tbm *cfg)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	return chk_dual_tbm_get(idx, cfg);
}

s32 pp_dual_tbm_set(u16 idx, struct pp_dual_tbm *cfg)
{
	if (unlikely(!pp_is_ready()))
		return -EPERM;

	return chk_dual_tbm_set(idx, cfg);
}

/* ========================================================================== */
/*                                Module APIs                                 */
/* ========================================================================== */
/**
 * @brief Helper function initialize all session manager ports info
 * @return s32 0 on success, error code otherwise
 */
static void __smgr_db_ports_init(void)
{
	u32 i;

	for (i = 0; i < PP_MAX_PORT; i++) {
		/* ingress ports sessions lists */
		INIT_LIST_HEAD(&db->in_port[i]);
		/* egress ports sessions lists */
		INIT_LIST_HEAD(&db->eg_port[i]);
	}
}

/**
 * @brief Helper function to initialize session manager sessions info
 * @return s32 0 on success, error code otherwise
 */
static s32 __smgr_db_sessions_init(struct device *dev)
{
	struct sess_db_entry *sess;
	u32 i;

	/* init free sessions list */
	INIT_LIST_HEAD(&db->free_list);

	/* allocate sessions db entries */
	db->sessions = pp_devm_kvcalloc(dev, db->n_sessions,
					sizeof(*db->sessions), GFP_KERNEL);
	if (unlikely(!db->sessions)) {
		pr_err("Failed to allocate %u sessions memory\n",
		       db->n_sessions);
		return -ENOMEM;
	}

	for (i = 0; i < db->n_sessions; i++) {
		sess = &db->sessions[i];
		/* set session id */
		sess->info.sess_id = i;
		/* init sessions lists nodes */
		INIT_LIST_HEAD(&sess->free_node);
		INIT_LIST_HEAD(&sess->in_port_node);
		INIT_LIST_HEAD(&sess->eg_port_node);
		/* add the session to the free list */
		DB_FREE_LIST_ADD_NODE(db, sess);
	}
	atomic_set(&db->stats.sess_free, db->n_sessions);

	return 0;
}

/**
 * @brief Helper function to initialize database bitmaps
 * @param dev dev to use for memory allocations
 * @return s32 0 on success, error code otherwise
 */
static s32 __smgr_db_maps_init(struct device *dev)
{
	/* allocate open sessions bitmap */
	db->open_bmap = devm_kcalloc(dev, BITS_TO_LONGS(db->n_sessions),
				     sizeof(*db->open_bmap), GFP_KERNEL);
	if (unlikely(!db->open_bmap)) {
		pr_err("Failed to allocate busy sessions bitmap\n");
		return -ENOMEM;
	}
	/* allocate ipv4 sessions bitmap */
	db->ipv4_bmap = devm_kcalloc(dev, BITS_TO_LONGS(db->n_sessions),
				     sizeof(*db->ipv4_bmap), GFP_KERNEL);
	if (unlikely(!db->ipv4_bmap)) {
		pr_err("Failed to allocate ipv4 sessions bitmap\n");
		return -ENOMEM;
	}
	/* allocate ipv6 sessions bitmap */
	db->ipv6_bmap = devm_kcalloc(dev, BITS_TO_LONGS(db->n_sessions),
				     sizeof(*db->ipv6_bmap), GFP_KERNEL);
	if (unlikely(!db->ipv6_bmap)) {
		pr_err("Failed to allocate ipv6 sessions bitmap\n");
		return -ENOMEM;
	}
	/* allocate tcp sessions bitmap */
	db->tcp_bmap = devm_kcalloc(dev, BITS_TO_LONGS(db->n_sessions),
				    sizeof(*db->tcp_bmap), GFP_KERNEL);
	if (unlikely(!db->tcp_bmap)) {
		pr_err("Failed to allocate tcp sessions bitmap\n");
		return -ENOMEM;
	}
	/* allocate udp sessions bitmap */
	db->udp_bmap = devm_kcalloc(dev, BITS_TO_LONGS(db->n_sessions),
				    sizeof(*db->udp_bmap), GFP_KERNEL);
	if (unlikely(!db->udp_bmap)) {
		pr_err("Failed to allocate udp sessions bitmap\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief Helper function to initialize database caches
 * @return s32 0 on success, error code otherwise
 */
static s32 __smgr_db_caches_init(void)
{
	/* allocate work args cache */
	db->work_args_cache =
		kmem_cache_create("smgr_work_args", sizeof(union work_args), 0,
				  SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);
	if (unlikely(!db->work_args_cache)) {
		pr_err("Allocating work args cache failed\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief Helper function to initialize database stuff for async work
 *        mode
 * @return s32 0 on success, error code otherwise
 */
static s32 __smgr_db_async_init(void)
{
	u32 flags = WQ_MEM_RECLAIM | WQ_UNBOUND;

	/* Create work queue */
	db->workq = alloc_workqueue("%s", flags, 2, "Session Manager");
	if (!db->workq) {
		pr_err("Failed to create work queue");
		return -ENOMEM;
	}

	/* Create high priority work queue */
	flags |= WQ_HIGHPRI;
	db->workq_hi = alloc_workqueue("%s", flags, 2, "Session Manager High");
	if (!db->workq_hi) {
		pr_err("Failed to create high work queue");
		return -ENOMEM;
	}
	return 0;
}

s32 smgr_nf_set(enum pp_nf_type type, struct pp_nf_info *nf)
{
	switch (type) {
	case PP_NF_REASSEMBLY:
		break;
	case PP_NF_TURBODOX:
		return smgr_tdox_late_init();
		break;
	case PP_NF_FRAGMENTER:
		break;
	case PP_NF_MULTICAST:
		break;
	case PP_NF_IPSEC:
		break;
	default:
		pr_err("Unknown nf type %u\n", type);
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Initialized session manager database
 * @dev device to use for all memories allocations, we allocate using
 *      device structure just for extra safety in case we didn't free
 *      some memory when the PP module goes down
 * @init_param
 * @return s32 0 for success, non-zero otherwise
 */
static s32 __smgr_db_init(struct pp_smgr_init_param *init_param)
{
	struct device *dev = NULL;
	s32 ret;

	pr_debug("Session manager init db\n");

	/* verify number of required sessions is can be supported by HW
	 * before doing anything
	 */
	if (unlikely(init_param->num_sessions > SMGR_MAX_HW_SESSIONS)) {
		pr_err("To much sessions to support, max is %u\n",
		       SMGR_MAX_HW_SESSIONS);
		return -E2BIG;
	}

	/* Get pp device for memory allocations */
	dev = pp_dev_get();

	/* allocate database structure */
	db = devm_kzalloc(dev, sizeof(*db), GFP_KERNEL);
	if (unlikely(!db)) {
		pr_err("Failed to allocate database memory\n");
		return -ENOMEM;
	}
	/* save number of sessions */
	db->n_sessions = init_param->num_sessions;
	/* enable by default */
	db->state = SMGR_ENABLE;
	db->frag_mode = init_param->frag_mode;
	/* init lock */
	spin_lock_init(&db->lock);
	/* allocate bitmap for HW crawler */
	/* roundup session number to fit crawler hw limitations */
	db->inact_bmap_sz =
		max(CHK_CRWLR_MIN_ITR_SIZE * 2, db->n_sessions);
	db->inact_bmap_sz = roundup_pow_of_two(db->inact_bmap_sz);
	db->inact_bmap_sz =
		BITS_TO_LONGS(db->inact_bmap_sz) * sizeof(ulong);

	db->inact_bmap_sz = PAGE_ALIGN(db->inact_bmap_sz);
	db->inact_bmap = dma_zalloc_coherent(dev, db->inact_bmap_sz,
					     &db->inact_bmap_phys,
					     GFP_KERNEL);
	if (!db->inact_bmap) {
		pr_err("Failed to allocate %zu bytes for inactive sessions map\n",
		       db->inact_bmap_sz);
		return -ENOMEM;
	}

	pr_debug("inact_map_phys %#x, inact_map_sz %zu\n",
		 (u32)db->inact_bmap_phys, db->inact_bmap_sz);

	/* init all database bitmaps */
	ret = __smgr_db_maps_init(dev);
	if (unlikely(ret))
		return ret;
	/* sessions entries init */
	ret = __smgr_db_sessions_init(dev);
	if (unlikely(ret))
		return ret;
	/* tdox sessions entries init */
	ret = smgr_tdox_init(dev);
	if (unlikely(ret))
		return ret;
	/* caches */
	ret = __smgr_db_caches_init();
	if (unlikely(ret))
		return ret;
	/* async mode */
	ret = __smgr_db_async_init();
	if (unlikely(ret))
		return ret;
	/* ports info init */
	__smgr_db_ports_init();

	return 0;
}

/**
 * @brief Destroy database
 */
static void __smgr_db_destroy(void)
{
	struct device *dev = NULL;

	if (unlikely(!db))
		return;
	pr_debug("Destroying session manager db\n");

	/* Get pp device for memory deallocation */
	dev = pp_dev_get();
	if (likely(db->workq))
		destroy_workqueue(db->workq);
	if (likely(db->workq_hi))
		destroy_workqueue(db->workq_hi);
	if (likely(db->sessions))
		pp_devm_kvfree(dev, db->sessions);
	if (likely(db->open_bmap))
		devm_kfree(dev, db->open_bmap);
	if (likely(db->ipv4_bmap))
		devm_kfree(dev, db->ipv4_bmap);
	if (likely(db->ipv6_bmap))
		devm_kfree(dev, db->ipv6_bmap);
	if (likely(db->tcp_bmap))
		devm_kfree(dev, db->tcp_bmap);
	if (likely(db->udp_bmap))
		devm_kfree(dev, db->udp_bmap);

	kmem_cache_destroy(db->work_args_cache);
	dma_free_coherent(dev, db->inact_bmap_sz,
			  db->inact_bmap, db->inact_bmap_phys);
	devm_kfree(dev, db);
	db = NULL;
}

s32 pp_smgr_init(struct pp_smgr_init_param *init_param)
{
	s32 ret = 0;

	if (unlikely(ptr_is_null(init_param)))
		return -EINVAL;

	if (!init_param->valid)
		return -EINVAL;

	pr_debug("PP session manager init start, num_sess %u\n",
		 init_param->num_sessions);

	/* Init database */
	ret = __smgr_db_init(init_param);
	if (unlikely(ret)) {
		pr_err("Failed to initialized module db\n");
		goto done;
	}

	/* multicast init */
	ret = smgr_mcast_init(init_param->num_sessions);
	if (unlikely(ret)) {
		pr_err("Failed to initialized multicast db\n");
		goto done;
	}

	/* Init debug */
	ret = smgr_dbg_init(init_param->dbgfs);
	if (unlikely(ret)) {
		pr_err("Failed to initialized module debugfs\n");
		goto done;
	}

done:
	pr_debug("PP session manager init %s, ret = %d\n",
		 ret ? "failed" : "done", ret);
	return ret;
}

void pp_smgr_exit(void)
{
	pr_debug("PP session manager exit start\n");

	/* Exit syncq */
	smgr_sq_exit();
	/* Clean debug */
	smgr_dbg_clean();
	/* Clean multicast */
	smgr_mcast_exit();
	/* Clean tdox */
	smgr_tdox_exit();

	/* Free database memory */
	__smgr_db_destroy();

	pr_debug("PP session manager exit done\n");
}
