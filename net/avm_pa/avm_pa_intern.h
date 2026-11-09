/*
 * Packet Accelerator Interface
 *
 * vim:set expandtab shiftwidth=3 softtabstop=3:
 *
 * Copyright (c) 2011-2012 AVM GmbH <info@avm.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed and/or modified under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUX_AVM_PA_INTERN
#define _LINUX_AVM_PA_INTERN

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
#include <linux/processor.h> /* spin_until_cond */
#endif
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/icmp.h>
#include <net/inet_ecn.h>

#define AVM_PA_AVOID_UNALIGNED    1
#if AVM_PA_AVOID_UNALIGNED
#define PA_IPHLEN(iph)          (((((u8 *)iph)[0])&0xf)<<2)
#define PA_IPTOTLEN(iph)        (((u16 *)iph)[1])
#define PA_TCP_FIN(tcph)        (((u8 *)tcph)[13]&0x01)
#define PA_TCP_SYN(tcph)        (((u8 *)tcph)[13]&0x02)
#define PA_TCP_RST(tcph)        (((u8 *)tcph)[13]&0x04)
#define PA_TCP_ACK(tcph)        (((u8 *)tcph)[13]&0x10)
#define PA_TCP_FIN_OR_RST(tcph) (((u8 *)tcph)[13]&0x05)
#define PA_TCP_DOFF(tcph)       (((((u8 *)tcph)[12]&0xf0)>>4)*4)
#define PA_IP6_PAYLOADLEN(ip6h) (((u16 *)ip6h)[2])
#else
#define PA_IPHLEN(iph)          ((iph)->ihl<<2)
#define PA_IPTOTLEN(iph)        ((iph)->tot_len)
#define PA_TCP_FIN(tcph)        ((tcph)->fin)
#define PA_TCP_SYN(tcph)        ((tcph)->syn)
#define PA_TCP_RST(tcph)        ((tcph)->rst)
#define PA_TCP_ACK(tcph)        ((tcph)->ack)
#define PA_TCP_FIN_OR_RST(tcph) ((tcph)->fin || (tcph)->rst)
#define PA_TCP_DOFF(tcph)       (((((tcph)->doff)&0xf000)>>12)*4)
#define PA_IP6_PAYLOADLEN(ip6h) ((ip6h)->payload_len)
#endif

/* ------------------------------------------------------------------------ */

#define ipv6fraghdr frag_hdr
#define IP6_OFFSET  0xFFF8

/* ------------------------------------------------------------------------ */

typedef int pa_fprintf(void *, const char *, ...)
#ifdef __GNUC__
        __attribute__ ((__format__(__printf__, 2, 3)))
#endif
        ;

/* ------------------------------------------------------------------------ */

/* vlan_id:12   802.1Q 9.3.2.3 */
/* vlan_cfi:1   802.1Q 9.1 e2 (== 0, no E-RIF) */
/* vlan_prio:3  802.1Q Appendix H.2 */

struct vlanhdr {
   u16 vlan_tci;
#define VLAN_ID(p)   (ntohs((p)->vlan_tci) & 0xfff)
#define VLAN_PRIO(p) (ntohs((p)->vlan_tci) >> 13)
#define VLAN_CFI(p)  ((ntohs((p)->vlan_tci) & 0x1000) ? 1 : 0)
   u16 vlan_proto;
};

/* ------------------------------------------------------------------------ */

struct pppoehdr {
#if defined (__BIG_ENDIAN_BITFIELD)
   u8 type:4;
   u8 ver:4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
   u8 ver:4;
   u8 type:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
   u8 code;
   u16 sid;
   u16 length;
};
#define ETH_P_PPP_SESS ETH_P_PPP_SES

/* ------------------------------------------------------------------------ */

struct llc_snap_hdr {
   /* RFC 1483 LLC/SNAP encapsulation for routed IP PDUs */
   u8  dsap;    /* Destination Service Access Point (0xAA)     */
   u8  ssap;    /* Source Service Access Point      (0xAA)     */
   u8  ui;      /* Unnumbered Information           (0x03)     */
   u8  org[3];  /* Organizational identification    (0x000000) */
   u16 type;    /* Ether type (for IP)              (0x0800)   */
};

struct l2tp_datahdr {
   u32 session_id;
   u32 default_l2_specific_sublayer;
};

struct tlb_grehdr { /* GRE Header for Transparent LAN Bridging */
   u16 flags_and_version;
   u16 protocol;
   /* all optional fields not present */
};

/* ------------------------------------------------------------------------ */

union hdrunion {
   struct ethhdr   ethh;
   struct vlanhdr  vlanh;
   struct pppoehdr pppoeh;
   u8              ppph[1];
   struct iphdr    iph;
   struct ipv6hdr  ipv6h;
   u32             ipv6_vpfl; /* Version, Priority and Flow Label */
   u16             ports[2];
   struct tcphdr   tcph;
   struct udphdr   udph;
   struct icmphdr  icmph;
   struct icmp6hdr icmpv6h;
   struct llc_snap_hdr llcsnap;
   struct l2tp_datahdr l2tp;
   struct tlb_grehdr   greh;
   struct ip_esp_hdr   esph;
};

typedef union hdrunion hdrunion_t;

#define IPV6_VPFL_TC_OFFSET 20
#define IPV6_VPFL_ECN_MASK (INET_ECN_MASK << IPV6_VPFL_TC_OFFSET)

#define LISP_DATAHDR_SIZE     8

#define AVM_PA_MAX_HASH (CONFIG_AVM_PA_MAX_SESSION > 2048 ? 2048 : CONFIG_AVM_PA_MAX_SESSION)
#define AVM_PA_MAX_MACADDR (CONFIG_AVM_PA_MAX_SESSION)

struct avm_pa_prio_map {
   int enabled;
   unsigned int prios[AVM_PA_MAX_PRIOS];
};

struct avm_pa_pid {
   /* avm_pa_pid is reference counted, and may be unregistered lazily. */
   struct kref                ref;
   struct completion         *release_completion;
#ifdef CONFIG_AVM_PA_TX_NAPI
   struct napi_struct         tx_napi;
   struct sk_buff_head        tx_napi_pkts;
#ifdef CONFIG_SMP
   /* the tasklet is used to switch cores for the napi_poll */
   struct tasklet_struct      tx_napi_tsk;
#endif
#endif
   struct avm_pa_pid_cfg      cfg;
   struct avm_pa_pid_ecfg     ecfg;
   avm_pid_handle             pid_handle;
   avm_pid_handle             ingress_pid_handle;
   enum avm_pa_framing        ingress_framing;
   enum avm_pa_framing        egress_framing;
   struct hlist_head          hash_sess[AVM_PA_MAX_HASH];
   struct hlist_head          hash_bsess[AVM_PA_MAX_HASH];
   struct avm_pa_pid_hwinfo  *hw;
   /* channel acceleration via hw */
   unsigned                   bridging_ok:1,
                              rx_channel_activated:1,
                              tx_channel_activated:1,
                              rx_channel_stopped:1;

   /* Provide an array of avm_pa_prio_map structs to store multiple priority
    * maps which allow us to specificy per priority in which upstream queue
    * classified TCP ACK (tack) and HTTP-GET (tget; JAZZ 10051) traffic should
    * be enqueued. This enables us to configure the upstream prioritization in
    * such way that tack traffic for priority 7 will not be enqeued in
    * queue 5 (important) but in queue 7 (low). This is a prerequisite for the
    * downstream regulation to work properly.
    */
#define AVM_PA_COUNT_PRIO_MAPS  2 /* tack and tget */
   struct avm_pa_prio_map     prio_maps[AVM_PA_COUNT_PRIO_MAPS];
   unsigned                   prioack_acks;
   unsigned                   prioack_accl_acks;

   /* stats */
   u32                        tx_pkts;
};

/**
 * Internal interfaces provided by the avm_pa core.
 **/

/**
 * @internal
 * Given a pid_handle, increase the ref count of the corresponding avm_pa_pid and return
 * it.
 *
 * If the pid is not registered, NULL is returned and the ref count is restored. Otherwise,
 * the reference must be released using @ref avm_pa_pid_put().
 *
 * @param pid_handle Handle to the wanted @c avm_pa_pid.
 * @return The @c avm_pa_pid, must be released after use with @ref avm_pa_pid_put.
 */
struct avm_pa_pid *avm_pa_pid_get_pid(avm_pid_handle pid_handle);

/**
 * @internal
 * Release one reference to a avm_pa_pid.
 *
 * @param pid_handle Handle to the reference to be released
 *
 * @return 1 if the pid_handle was removed, otherwise 0. @see kref_put()
 */
int avm_pa_pid_put(avm_pid_handle pid_handle);

/**
 * @internal
 * Perform fragmentation on skb according to the egress MTU.
 *
 * Returns a list of skbs or the original skb if fragmentation
 * isn't necessary.
 *
 * @param egress egress pid
 * @param skb the packet
 *
 * @return the (new) packet list ready to transmit, or NULL if an error occured
 *
 */
struct sk_buff *avm_pa_fragment(struct avm_pa_egress *egress, struct sk_buff *skb);

#ifdef CONFIG_PROC_FS

/**
 * @internal
 * Clear selectors for a given pid.
 *
 * If a pid is removed, the corresponding selectors must be removed as well.
 *
 * @param selector_list The list containing applicable selectors
 * @param pid handle of the pid being removed
 */
void avm_pa_selector_clear_for_pid(struct list_head *selector_list, avm_pid_handle pid);

/**
 * @internal
 * Clear selectors for a given node.
 *
 * If a node is removed, the corresponding selectors must be removed as well.
 *
 * @param selector_list The list containing applicable selectors
 * @param handle handle of the node being removed
 */
void avm_pa_selector_clear_for_node(struct list_head *selector_list, avm_pa_node_handle handle);

/**
 * @internal
 * Dump selector list into buffer, in humand-readable and machine-parsable format.
 *
 * This is intended to be used for proc files that output the currently configured
 * selector. Therefore, buffer must point to user-space memory.
 *
 * @param[in] selector_list The list containing applicable selectors
 * @param[out] buffer Destination buffer, must point to user-space memory.
 * @param[in] count Size of the buffer.
 * @return Number of bytes written (0 if selector list is empty).
 */
ssize_t avm_pa_dump_selector_user(struct list_head *selector_list,
                                  char __user *buffer,
                                  size_t count);

/**
 * @internal
 * Parse selector list from buffer.
 *
 * This is intended to be used for proc files that that receive input from the user.
 * Therefore, buffer must point to user-space memory.
 *
 * Upon successful parsing, the selector list is populated with entries that select
 * (match) sessions (@see avm_pa_session_is_selected). You must free those using
 * @ref avm_pa_selector_free().
 *
 * @param selector_list The list containing applicable selectors.
 * @param buffer Source buffer, must point to user-space memory.
 * @param count Size of the buffer.
 * @return Number of bytes read, or -EINVAL on error.
 */
ssize_t avm_pa_parse_selector_user(struct list_head *selector_list,
                                   const char __user *buffer,
                                   size_t count);
#endif


/**
 * @internal
 * Cache entry for l2tp sessions
 *
 * Instead of holding a reference to "struct l2tp_session", we copy the fields
 * relevant to us. We don't want to prevent tearing down sessions or tunnels by
 * holding references.
 */
struct avm_pa_l2tp {
   __be32 session_id;
   __be32 peer_session_id;
   u16 hdr_len; /* includes optional cookie, l2specific length and pad (all optional) */
};

#define EGRESS_POOL_SIZE (CONFIG_AVM_PA_MAX_SESSION > 64 ? 256 : 16)

/**
 * @internal
 * Global data (zero-initialized).
 */
struct avm_pa_data {
   /** Global session array */
   struct avm_pa_session sessions[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_egress  egress_pool[EGRESS_POOL_SIZE];
#ifdef CONFIG_L2TP
#define MAX_L2TP_CACHE_ITEMS 16
   struct avm_pa_l2tp    l2tp_cache[MAX_L2TP_CACHE_ITEMS];
#endif
};

extern struct avm_pa_data pa_data;

/* ----- avm_pa_node - previously known as vpids -------------------------- */
struct avm_pa_node {
   struct hlist_node      list;
   avm_pa_node_handle     handle;
   char                   name[IFNAMSIZ];
   struct avm_pa_node_cfg cfg;
};

/** @internal
 *
 * Register a node (internal).
 *
 * This is the internal node allocation function.
 * For what a node is about, @see avm_pa_dev_node_register().
 *
 * If the node was previously registered (matching the name),
 * then the same handle will be returned.
 *
 * @param name The name for this node
 * @param cfg Additional configuration
 *
 * @return The node handle, or <0 on error.
 *
 */
int avm_pa_node_register(const char *name, struct avm_pa_node_cfg *cfg, gfp_t gfp_mask);

/** @internal
 *
 * Unegister a node (internal).
 *
 * This is the internal node de-allocation function. The node is moved
 * to a free list where it can be restored from. It is not truly freed.
 * This implements the promise to return the same handle on
 * re-registration.
 *
 * For what a node is about, @see avm_pa_dev_node_register().
 *
 * @param name The name for this node
 * @param cfg Additional configuration
 *
 * @return The node handle, or <0 on error.
 *
 */
void avm_pa_node_unregister(avm_pa_node_handle handle);

/** @internal
 *
 * Lookup a node by name
 *
 * @param name The name to look for
 *
 * @return Pointer to the node, or @c NULL if no node could be found.
 *
 */
struct avm_pa_node *avm_pa_node_lookup(const char *name);

/** @internal
 *
 * Lookup a node by handle
 *
 * @param handle The handle to look for
 *
 * @return  Pointer to the node, or @c NULL if no node could be found.
 */
struct avm_pa_node *avm_pa_node_lookup_handle(avm_pa_node_handle handle);


/** @internal
 *
 * Get the handle of the most-recently recorded node. May use that
 * to check for duplicates before adding a new node to the path.
 *
 * @note If the path is empty the handle will be 0 which is not a valid
 * node handle.
 *
 * @param info The packet info
 * @return The handle of the most recently added node.
 */
avm_pa_node_handle avm_pa_path_get_last_node(struct avm_pa_pkt_info *info);

/** @internal
 *
 * Remember the node, identified by its handle, in the packet info.
 *
 * The series of nodes as travelled by the packet in the describes its "slow path".
 * Thus, an array of nodes forms a path.
 *
 * They are stored in an array (info->nodes) that can be copied out
 * later using avm_pa_path_copy()
 *
 *
 * @param info The packet info
 * @param handle The node handle to remember
 *
 * @return The count of total nodes in the packet info, including this one.
 */
int avm_pa_path_add_node(struct avm_pa_pkt_info *info, avm_pa_node_handle handle);

/** @internal
 *
 * Copy out the nodes that have been marked by avm_pa_node_mark().
 *
 * The @p handles buffer must be sufficiently large to possibly hold the
 * maximum number of nodes in the packet info.
 *
 * @param info The packet info
 * @param[out] path The path buffer
 * @param num_handles The size of the path buffer
 *
 * @return The count of total nodes in the packet info.
 */
int avm_pa_path_copy(struct avm_pa_pkt_info *info, avm_pa_node_handle *path, size_t num_handles);

/** @internal
 *
 * Calculate the effective MTU by the given path stored in the packet info.
 *
 * Not all nodes should be included in the calculation, if they are known to be
 * ingress nodes. Therefore nodes can be exempted from the calculation by the
 * @p mask parameter.
 *
 * @param path The buffer containng the path
 * @param The number of handles on the path buffer.
 * @param ipversion Whether to calculate IPv4 or IPv6 MTU
 * @param mask
 * @param[out] MTU is stored here.
 *
 * @retval 0 Success.
 */
int avm_pa_path_calc_min_mtu(avm_pa_node_handle *path, size_t num_handles, int ipversion, u32 mask, u16 *mtu);

/** @internal
 *
 * Format the path suitable for printk and similar.
 *
 * @param path The buffer containng the path
 * @param The number of handles on the path buffer.
 * @param[out] buf Pointer to the destination string
 * @param bufsize Size of the destination buffer
 *
 * @retval 0 Success
 */
int avm_pa_path_format(avm_pa_node_handle *path, size_t num_handles, char *buf, size_t bufsize);

/** @internal
 *
 * Initialize the path module.
 *
 * @retval 0 Success
 */
int avm_pa_path_init(void);

#ifdef CONFIG_PROC_FS
void avm_pa_path_proc_init(struct proc_dir_entry *dir_entry);
void avm_pa_path_proc_exit(struct proc_dir_entry *dir_entry);
#endif

/**
 * Validate a session.
 *
 * A session is valid if it's not in the states FREE or CREATE, that is if it's
 * in ACTIVE, FREE or DEAD states.
 *
 * @return @c true if the session is valid, @c false otherwise.
 */
static inline bool avm_pa_session_valid(struct avm_pa_session *session)
{
   int which = session->on_list;

   return (which != AVM_PA_LIST_FREE && which < AVM_PA_LIST_MAX) && session->session_handle != 0;
}

enum avm_pa_session_flags {
   PA_S_CREATING,     /* To serialize creation in face of multiple egress */
   PA_S_CHANGING,     /* Ditto */
   PA_S_FLUSHED,
   PA_S_IN_HW,
   PA_S_HW_BOUND,     /* Only if PA_S_IN_HW is set */
   PA_S_HW_UNBOUND,   /* Only if PA_S_IN_HW is set */
   PA_S_HW_VOID,      /* Only if PA_S_IN_HW is set */
   PA_S_HW_REM_ASYNC, /* Only if PA_S_IN_HW is set */
   PA_S_HW_LOCAL,     /* Only if PA_S_IN_HW is set */
   PA_S_NO_HW,
   PA_S_ROUTED,
   PA_S_REALTIME,
   PA_S_PRIOACK_CHECK,
   PA_S_PRIOACK_DONE,
   PA_S_PRIOACK_ACK,
};

enum pa_egress_flags {
   PA_EG_GSO_OK,
};

/* ------------------------------------------------------------------------ */

/**
 * @internal
 * reset session groups in skb
 *
 * @param skb the paket
 */
void avm_pa_skb_sg_reset(struct sk_buff *skb);

/**
 * @internal
 * link session to session groups
 *
 * @param session session that becomes active
 * @param skb paket that issued the session creation
 */
void avm_pa_sg_session_link(struct avm_pa_session *session, struct sk_buff *skb);

/**
 * @internal
 * called when session is dead and all packets are counted
 *
 * @param session session that is dead
 */
void avm_pa_sg_session_unlink(struct avm_pa_session *session);

/**
 * @internal
 * print session group information, called by avm_pa.c:pa_show_session()
 *
 * @param session session to group information for
 */
void avm_pa_sg_show_session(struct avm_pa_session *session,
                            pa_fprintf fprintffunc, void *arg);

/**
 * @internal
 * query if session belongs to group, called by avm_pa_selector.c:group_selector_check()
 *
 * @param session session to group information for
 * @return 1 if session belongs to sessiongroup, else 0
 */
int avm_pa_session_belongs_to_sg(struct avm_pa_session *session,
                                 unsigned short groupid);

#ifdef CONFIG_PROC_FS
/**
 * @internal
 * create sessiongroup files in /proc, called by avm_pa.c:avm_pa_proc_init()
 *
 * @param dir_entry dir_entry for /proc/net/avm_pa
 */
void avm_pa_sg_proc_init(struct proc_dir_entry *dir_entry);

/**
 * @internal
 * remove sessiongroup files in /proc, called by avm_pa.c:avm_pa_proc_exit()
 *
 * @param dir_entry dir_entry for /proc/net/avm_pa
 */
void avm_pa_sg_proc_exit(struct proc_dir_entry *dir_entry);
#endif

/**
 * @internal
 * initialisation of sessiongroup management, called by avm_pa.c:avm_pa_early_init()
 */
int avm_pa_sg_init(void);

/**
 * @internal
 * deinitialisation of sessiongroup management, called by avm_pa.c:avm_pa_exit()
 */
void avm_pa_sg_exit(void);

/* ------------------------------------------------------------------------ */

int avm_pa_netdev_init(void);

int __init pa_dev_init(void);
void __exit pa_dev_exit(void);

/* ------------------------------------------------------------------------ */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define file_inode(file) (file->f_path.dentry->d_inode)
#define PDE_DATA(_inode) (PDE(_inode)->data)
static inline void kfree_skb_list(struct sk_buff *skb)
{
   while (skb) {
      struct sk_buff *nskb = skb->next;
      kfree_skb(skb);
      skb = nskb;
   }
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 17, 0)
/* See mainline commits:
 * commit 1d023284 list: fix order of arguments for hlist_add_after(_rcu)
 *
 * Note that the macro was renamed and arguments order swapped.
 */
#define hlist_add_behind_rcu(new, prev) hlist_add_after_rcu(prev, new)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 178)
/**
 * hlist_add_tail_rcu
 * @n: the element to add to the hash list.
 * @h: the list to add to.
 *
 * Description:
 * Adds the specified element to the specified hlist,
 * while permitting racing traversals.
 *
 * The caller must take whatever precautions are necessary
 * (such as holding appropriate locks) to avoid racing
 * with another list-mutation primitive, such as hlist_add_head_rcu()
 * or hlist_del_rcu(), running on this same list.
 * However, it is perfectly legal to run concurrently with
 * the _rcu list-traversal primitives, such as
 * hlist_for_each_entry_rcu(), used to prevent memory-consistency
 * problems on Alpha CPUs.  Regardless of the type of CPU, the
 * list-traversal primitive must be guarded by rcu_read_lock().
 */
static inline void hlist_add_tail_rcu(struct hlist_node *n,
                                     struct hlist_head *h)
{
       struct hlist_node *i, *last = NULL;

       for (i = hlist_first_rcu(h); i; i = hlist_next_rcu(i))
               last = i;

       if (last) {
               n->next = last->next;
               n->pprev = &last->next;
               rcu_assign_pointer(hlist_next_rcu(last), n);
       } else {
               hlist_add_head_rcu(n, h);
       }
}
#endif



/* See mainline commits:
 * commit b67bfe0d hlist: drop the node parameter from iterators
 **/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 8, 0)

#define hlist_entry_safe(ptr, type, member) \
	({ typeof(ptr) ____ptr = (ptr); \
	   ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
	})

#undef hlist_for_each_entry
#define hlist_for_each_entry(pos, head, member)				\
	for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member);\
	     pos;							\
	     pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

#undef hlist_for_each_entry_rcu
#define hlist_for_each_entry_rcu(pos, head, member)			\
	for (pos = hlist_entry_safe (rcu_dereference_raw(hlist_first_rcu(head)),\
			typeof(*(pos)), member);			\
		pos;							\
		pos = hlist_entry_safe(rcu_dereference_raw(hlist_next_rcu(\
			&(pos)->member)), typeof(*(pos)), member))

#undef hlist_for_each_entry_rcu_bh
#define hlist_for_each_entry_rcu_bh(pos, head, member)			\
	for (pos = hlist_entry_safe(rcu_dereference_bh(hlist_first_rcu(head)),\
			typeof(*(pos)), member);			\
		pos;							\
		pos = hlist_entry_safe(rcu_dereference_bh(hlist_next_rcu(\
			&(pos)->member)), typeof(*(pos)), member))

#undef hlist_for_each_entry_continue_rcu
#define hlist_for_each_entry_continue_rcu(pos, member)			\
	for (pos = hlist_entry_safe(rcu_dereference((pos)->member.next),\
			typeof(*(pos)), member);			\
	     pos;							\
	     pos = hlist_entry_safe(rcu_dereference((pos)->member.next),\
			typeof(*(pos)), member))

#undef hlist_for_each_entry_safe
#define hlist_for_each_entry_safe(pos, n, head, member) 		\
	for (pos = hlist_entry_safe((head)->first, typeof(*pos), member);\
	     pos && ({ n = pos->member.next; 1; });			\
	     pos = hlist_entry_safe(n, typeof(*pos), member))

#endif


#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0)
/* See mainline commits:
 * commit 398f382c pktgen: clean up ktime_t helpers
 * commit 67cb9366 ktime: add ktime_after and ktime_before helper
 *
 * The first adds ktime_compare, the second one adds ktime_{after,before} helpers.
 */
static inline int ktime_compare(const ktime_t cmp1, const ktime_t cmp2)
{
       if (cmp1.tv64 < cmp2.tv64)
               return -1;
       if (cmp1.tv64 > cmp2.tv64)
               return 1;
       return 0;
}
#endif
#define ktime_after(cmp1, cmp2) (ktime_compare(cmp1, cmp2) > 0)
#define ktime_before(cmp1, cmp2) (ktime_compare(cmp1, cmp2) < 0)
#endif

/* See mainline commit>
 * commit ed067d4a859f linux/kernel.h: Add ALIGN_DOWN macro
 *
 * But older kernels like 4.9 contain the macro as well.
 */
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(x, a)       __ALIGN_KERNEL((x) - ((a) - 1), (a))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
/* See mainline commits:
 * commit 97a32539b956 proc: convert everything to "struct proc_ops"
 * commit d56c0d45f0e2 proc: decouple proc from VFS with "struct proc_ops"
 *
 * proc_ops replaces file_operations and must be passed to proc_* interfaces.
 */
struct proc_ops {
#define proc_open    _fops.open
#define proc_read    _fops.read
#define proc_write   _fops.write
#define proc_lseek   _fops.llseek
#define proc_release _fops.release
	struct file_operations _fops;
};

#define proc_create_data(name, mode, proc_dir, proc_ops, data) \
	proc_create_data(name, mode, proc_dir, &(proc_ops)->_fops, data)

#define proc_create(name, mode, proc_dir, proc_ops) \
	proc_create(name, mode, proc_dir, &(proc_ops)->_fops)

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
/* See mainline commit:
 * commit bfc18e389c7a atomics/treewide: Rename __atomic_add_unless() => atomic_fetch_add_unless()
 */
#define atomic_fetch_add_unless(a,b,c) __atomic_add_unless(a,b,c)
#endif

#ifndef fallthrough /* Linux << v5.4 */
#define fallthrough __attribute__((__fallthrough__))
#endif


#ifndef spin_until_cond
#define spin_until_cond(cond) while(unlikely(!(cond))) { cpu_relax(); }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
static inline void skb_gso_reset(struct sk_buff *skb)
{
	skb_shinfo(skb)->gso_size = 0;
	skb_shinfo(skb)->gso_segs = 0;
	skb_shinfo(skb)->gso_type = 0;
}
#endif

#endif /* _LINUX_AVM_PA_INTERN */
