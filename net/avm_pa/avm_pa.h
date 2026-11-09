/*
 * Packet Accelerator Interface
 *
 * vim:set expandtab shiftwidth=3 softtabstop=3:
 *
 * Copyright (c) 2011-2015 AVM GmbH <info@avm.de>
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

#ifndef _LINUX_AVM_PA_H
#define _LINUX_AVM_PA_H

#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/version.h>
#include <linux/gfp.h>
/* Older kernels prevent us from using skbuff.h due to include cycle.
 * Newer kernels are fixed and define SKBUFF_H_NO_AVM_PA_H.
 * For a transition period, we will adapt to those older kernels.
 */
#if defined(NETDEVICE_H_NO_AVM_PA_H) || defined(SKBUFF_H_NO_AVM_PA_H)
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#else
struct sk_buff;
#include <uapi/linux/if_ether.h>
#endif
#ifdef CONFIG_AVM_GENERIC_CONNTRACK
#include <linux/generic-ct.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
#define AVM_PA_SKBUFF_HAS_VLAN_PROTO
#endif

#if defined(CONFIG_MACH_PUMA7)
#if defined(CONFIG_TI_PACKET_PROCESSOR) && defined(CONFIG_TI_PACKET_PROCESSOR_STATS)
#define AVM_PA_SESSIONGROUPS_WITH_PACKET_PROCESSOR
#endif
#endif

#define PKT struct sk_buff

/* ------------------------------------------------------------------------ */
#define AVM_PA_HAS_LISP_SUPPORT
#define AVM_PA_RTP_SESSION_AVAILABLE

/* ------------------------------------------------------------------------ */

struct avm_pa_stats {
   unsigned short nsessions;
   unsigned short nbsessions;
   unsigned short maxsessions;
   /* packet/sec */
   u32 rx_pps;
   u32 fw_pps;
   u32 overlimit_pps;
   /* in avm_pa_pid_receive */
   u32 rx_pkts;
   u32 rx_bypass;
   u32 rx_ttl;
   u32 rx_broadcast;
   u32 rx_search;
   u32 rx_match;
   u32 rx_lispchanged;
   u32 rx_df;
   u32 rx_mod;
   u32 rx_overlimit;
   u32 rx_dropped;
   u32 rx_irq;
   u32 rx_irqdropped;
   u32 rx_headroom_too_small;
   u32 rx_realloc_headroom_failed;
   u32 rx_frag_list;
   u32 fw_output;
   u32 fw_output_drop;
   u32 fw_local;
   u32 fw_rtp;
   u32 fw_rtp_drop;
   u32 fw_ill;
   /* in avm_pa_pid_snoop_transmit */
   u32 tx_accelerated;
   u32 tx_local;
   u32 tx_already;
   u32 tx_bypass;
   u32 tx_sess_error;
   u32 tx_sess_ok;
   u32 tx_sess_exists;
   u32 tx_egress_error;
   u32 tx_egress_ok;
   u32 tx_pid_change;
   u32 xfrm_sess_ok;
   /* in _pa_do_modify_and_send */
   u32 tx_fast_gso;
   /* in avm_pa_add_local_session */
   u32 local_sess_error;
   u32 local_sess_ok;
   u32 local_sess_exists;
   /* in avm_pa_add_rtp_session */
   u32 rtp_sess_error;
   u32 rtp_sess_ok;
   u32 rtp_sess_exists;
   /* in pa_transmit() */
   u32 fw_pkts;
   u32 fw_frags;
   u32 fw_drop;
   u32 fw_fail;
   u32 fw_frag_fail;
   u32 fw_drop_gone;
   /* in avm_pa_tbf_schedule() */
   u32 tbf_schedule;
   u32 tbf_reschedule;
   /* in session_gc() */
   u32 sess_timedout;
   u32 sess_flushed;
   u32 sess_pidchanged;
   /* rx channel */
   u32 rx_channel_no_rx_slow;
   u32 rx_channel_stopped;
   /* tx channel */
   u32 tx_channel_dropped;
   /* cputime per second */
   u32 userms;
   u32 idlems;
   u32 irqms;
};

enum avm_pa_framing {
   avm_pa_framing_ether = 1,    /* start with ethernet header */
   avm_pa_framing_ppp = 2,      /* start with ppp header */
   avm_pa_framing_ip = 3,       /* start with ipv4/ipv6 header */
   avm_pa_framing_dev = 4,      /* start after ethhdr, check skb->protocol */
   avm_pa_framing_ptype = 5,    /* packet_type */
   avm_pa_framing_llcsnap = 6,  /* llc snap (WLAN) */
   avm_pa_framing_ipdev = 7,    /* start with ipv4/ipv6, use skb_network_header() instead of skb->data */
};

#define AVM_PA_MAX_NAME 32

struct avm_pa_pid_cfg {
   char                   name[AVM_PA_MAX_NAME];
   enum avm_pa_framing    framing;
   u16                    default_mtu;
   void                 (*tx_func)(void *arg, PKT *pkt);
   void                  *tx_arg;
   struct packet_type    *ptype; /* avm_pa_framing_ptype */
};

#define AVM_PA_PID_ECFG_VERSION 3
struct avm_pa_pid_ecfg {
   int           version;
   /* version 0 */
#define AVM_PA_PID_FLAG_NONE                  0x00000000
#define AVM_PA_PID_FLAG_NO_PID_CHANGED_CHECK  0x00000001
#define AVM_PA_PID_FLAG_HSTART_ON_INGRESS     0x00000002
#define AVM_PA_PID_FLAG_HSTART_ON_EGRESS      0x00000004
   unsigned long flags;
   /* version 1 */
   unsigned int  cb_start;
   unsigned int  cb_len;
   /* version 2 */
   void  (*rx_slow)(void *arg, PKT *pkt);
   void  *rx_slow_arg;
   /* version 3 */
#define AVM_PA_PID_GROUP_WLAN_STA             1
   int   pid_group;
};

struct avm_pa_vpid_cfg {
   char                name[AVM_PA_MAX_NAME];
   u16                 v4_mtu;
   u16                 v6_mtu;
};

#define AVM_PA_IS_UNICAST    0 /* ((u32 *)(&stats->rx_unicast_pkt))[0] */
#define AVM_PA_IS_MULTICAST  1 /* ((u32 *)(&stats->rx_unicast_pkt))[1] */
#define AVM_PA_IS_BROADCAST  2 /* ((u32 *)(&stats->rx_unicast_pkt))[2] */

#define AVM_PA_MC_STATS

#define AVM_PA_HAS_VPID_STATS_TIMESTAMP

struct avm_pa_vpid_stats
{
   u32 rx_unicast_pkt;    /* ((u32 *)(&stats->rx_unicast_pkt))[0] */
   u32 rx_multicast_pkt;  /* ((u32 *)(&stats->rx_unicast_pkt))[1] */
   u32 rx_broadcast_pkt;  /* ((u32 *)(&stats->rx_unicast_pkt))[2] */
   u64 rx_bytes;          /* ((u64 *)(&stats->rx_bytes))[0] */
   u64 rx_multicast_bytes;/* ((u64 *)(&stats->rx_bytes))[1] */
   u64 rx_broadcast_bytes;/* ((u64 *)(&stats->rx_bytes))[2] */
   u32 rx_discard;
   u32 tx_unicast_pkt;    /* ((u32 *)(&stats->tx_unicast_pkt))[0] */
   u32 tx_multicast_pkt;  /* ((u32 *)(&stats->tx_unicast_pkt))[1] */
   u32 tx_broadcast_pkt;  /* ((u32 *)(&stats->tx_unicast_pkt))[2] */
   u64 tx_bytes;
   u32 tx_error;
   u32 tx_discard;

   ktime_t timestamp;
};

#define AVM_PA_MAX_PRIOS       10
#define AVM_PA_TRAFFIC_STATS

struct avm_pa_traffic_stats {
   u64 bytes;
   u32 pkts;
};

#define AVM_PA_HAS_PRIO_STATS
#define AVM_PA_HAS_PRIO_STATS_TIMESTAMP

struct avm_pa_prio_stats {
   struct avm_pa_traffic_stats sw;
   struct avm_pa_traffic_stats hw;
   struct avm_pa_traffic_stats associated_sw;
   struct avm_pa_traffic_stats associated_hw;
   ktime_t timestamp;
};

/* ------------------------------------------------------------------------ */
/* -------- session group ------------------------------------------------- */
/* ------------------------------------------------------------------------ */

#define AVM_PA_HAS_SESSIONGROUPS

#define AVM_PA_MAX_SESSIONGROUP 512
#define AVM_PA_SESSION_GROUPS_PER_SESSION 8

struct avm_pa_sg_stats {
   ktime_t          timestamp;
   u64              total_bytes;
   u64              total_pkts;
   u64              multicast_bytes;
   u64              multicast_pkts;
};

struct avm_pa_sessiongroupids { /* in skb */
   unsigned short ncounter;
   unsigned short counterid[AVM_PA_SESSION_GROUPS_PER_SESSION];
};

/* ------------------------------------------------------------------------ */
/* -------- pkt info structs and definitions ------------------------------ */
/* ------------------------------------------------------------------------ */

typedef unsigned char  avm_pid_handle;     /* 1 - AVM_PA_MAX_PID */
typedef unsigned char  avm_vpid_handle;    /* 1 - AVM_PA_MAX_VPID */
typedef unsigned short avm_session_handle; /* 1 - AVM_PA_MAX_SESSION */

#define AVM_PA_MAX_HEADER 128 /* maximum bytes save from header */
#define AVM_PA_MAX_HDROFF   4 /* aligned to 4 bytes */

/* Possible match types that can be recorded in a avm_pa_match_info array */
enum avm_pa_match_type {
   AVM_PA_ETH,
   AVM_PA_VLAN,
   AVM_PA_PPPOE,
   AVM_PA_PPP,
   AVM_PA_IPV4,
   AVM_PA_IPV6,
   AVM_PA_PORTS,
   AVM_PA_ICMPV4,
   AVM_PA_ICMPV6,
   AVM_PA_LLC_SNAP,
   AVM_PA_LISP,
   AVM_PA_L2TP,
   AVM_PA_GRE,
   AVM_PA_ESP,

   AVM_PA_NUM_MATCH_TYPES,

   /* states, part of the enum so that they can be used on the same state machine.*/
   AVM_PA_ETH_PROTO,
   AVM_PA_IP_PROTO,
};

#define AVM_PA_MAX_MATCH 16

struct avm_pa_match_info {
   unsigned char type;
   unsigned char offset;
};

/*
 * IPv6 + IPv4 + UDP => IPv4 + UDP
 *        IPv4 + UDP => IPv4 + UDP
 *
 * IPV6 + UDP + LISP + IPV4 => IPV4
 * IPV6 + UDP + LISP + IPV6 => IPV6
 * IPV4 + UDP + LISP + IPV4 => IPV4
 * IPV4 + UDP + LISP + IPV6 => IPV6
 *
 * IPV6 + L2TP + ETH + IPV4 => IPV4
 * IPV6 + L2TP + ETH + IPV6 => IPV6
 * IPV4 + L2TP + ETH + IPV4 => IPV4
 * IPV4 + L2TP + ETH + IPV6 => IPV6
 */
#define AVM_PA_PKTTYPE_PROTO_MASK    0x00FF
#define AVM_PA_PKTTYPE_IPPROTO(t)    ((t) & AVM_PA_PKTTYPE_PROTO_MASK)
#define AVM_PA_PKTTYPE_NONE          0x0000
#define AVM_PA_PKTTYPE_LISP          0x0100
/*  used               IPBIT1        0x0200 */
/*  used               IPBIT2        0x0400 */
#define AVM_PA_PKTTYPE_L2TP          0x0800
#define AVM_PA_PKTTYPE_IPV6          0x0600
#define AVM_PA_PKTTYPE_IPV4          0x0400
#define AVM_PA_PKTTYPE_IP_MASK       0x0600
#define AVM_PA_PKTTYPE_IP_VERSION(t) (((t)>>8)&0x6)
#define AVM_PA_PKTTYPE_GRE           0x1000
/*  used               IPENCAPBIT1   0x2000 */
/*  used               IPENCAPBIT2   0x4000 */
/*                     FREE          0x8000 */
#define AVM_PA_PKTTYPE_IPV6ENCAP     0x6000
#define AVM_PA_PKTTYPE_IPV4ENCAP     0x4000
#define AVM_PA_PKTTYPE_IPENCAP_MASK  0x6000

#define AVM_PA_PKTTYPE_IPENCAP_VERSION(t) (((t)>>12)&0x6)
#define AVM_PA_IPENCAP_VERSION_PKTTYPE(v) ((v&0x6)<<12)

#define AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(t) AVM_PA_IPENCAP_VERSION_PKTTYPE(AVM_PA_PKTTYPE_IP_VERSION(t))


#define AVM_PA_PKTTYPE_BASE_MASK     (AVM_PA_PKTTYPE_IP_MASK|AVM_PA_PKTTYPE_PROTO_MASK)

#define AVM_PA_PKTTYPE_BASE_EQ(t1,t2) (((t1) & AVM_PA_PKTTYPE_BASE_MASK) == ((t2) & AVM_PA_PKTTYPE_BASE_MASK))
#define AVM_PA_PKTTYPE_EQ(t1,t2) (t1 == t2)


#define AVM_PA_OFFSET_NOT_SET   0xff

#define AVM_PA_MATCH_HAS_PKTLEN

struct avm_pa_pkt_match {
   unsigned char               casttype;   /* unicast, multicast, broadcast */
   unsigned char               fragok:1,   /* IPv4 without DF-Bit */
                               fin:1,      /* TCP with fin or rst */
                               syn:1,      /* TCP with syn */
                               ack_only:1; /* TCP with ack and without data */
   /* 4 bits hole */
   u16                         pkttype;
   u32                         hash;
   struct avm_pa_match_info    match[AVM_PA_MAX_MATCH];
   u8                          hdrcopy[AVM_PA_MAX_HEADER];
   unsigned char               hdroff; /* where hdrcopy starts (alignment) */
#define HDRCOPY(info)   ((info)->hdrcopy+(info)->hdroff)
   unsigned char               nmatch;
   unsigned char               hdrlen;
   unsigned char               pppoe_offset; /* offset of pppoe header */
   unsigned char               encap_offset; /* offset of tunnel header */
   unsigned char               lisp_offset;  /* offset of lisp data header */
   unsigned char               ip_offset;    /* offset of (inner) ip header */
   unsigned char               full_hdrlen;  /* hdrlen including bytes not stored in hdrcopy */
   u16                         pktlen;
   u16                         vlan_tci;
#ifdef AVM_PA_SKBUFF_HAS_VLAN_PROTO
   __be16                      vlan_proto;
#endif
};

struct avm_pa_pkt_info {
   avm_pid_handle              ingress_pid_handle;
   avm_vpid_handle             ingress_vpid_handle;
   avm_vpid_handle             egress_vpid_handle;
   avm_pid_handle              egress_pid_handle;
   avm_pid_handle              ptype_pid_handle;
   avm_session_handle          session_handle;
   struct avm_pa_egress       *forced_egress; /* valid if already_modified == 1 */
   u8                          routed;
   u8                          do_not_accelerate;
   u8                          is_accelerated;
   u8                          already_modified;
   u8                          vpid_counted_slow;
   /* Use bits field for less used fields to save space */
   u8                          shaped : 1,
                               use_protocol_specific : 1,
                               no_hw : 1,
                               rps_override : 1;
#ifdef CONFIG_AVM_PA_RPS
   u8                          rps_done;
   struct cpumask              rps_allowed_mask;
   struct cpumask              rps_fallback_mask;
#endif
   struct avm_pa_pkt_match     match;
   u32                         session_uniq_id;
   unsigned int                hstart;
   /* Internally set if set_pkt_match() must defer l2tp session lookup to softirq
    * context */
   __be32                      l2tp_session_id;
#ifndef AVM_PA_SESSIONGROUPS_WITH_PACKET_PROCESSOR
   struct avm_pa_sessiongroupids assigned_sessiongroupids;
#endif
   avm_pid_handle              local_out_pid_handle;
};

struct avm_pa_dev_info {
   avm_pid_handle  pid_handle;
   avm_vpid_handle vpid_handle;
};


/* ------------------------------------------------------------------------ */

#define AVM_PA_V4_MOD_SADDR         0x0001
#define AVM_PA_V4_MOD_DADDR         0x0002
#define AVM_PA_V4_MOD_ADDR          (AVM_PA_V4_MOD_SADDR|AVM_PA_V4_MOD_DADDR)
#define AVM_PA_V4_MOD_TOS           0x0004
#define AVM_PA_V4_MOD_UPDATE_TTL    0x0008
#define AVM_PA_V4_MOD_IPHDR         (AVM_PA_V4_MOD_ADDR|AVM_PA_V4_MOD_TOS)
#define AVM_PA_V4_MOD_IPHDR_CSUM    0x0010
#define AVM_PA_V4_MOD_SPORT         0x0020
#define AVM_PA_V4_MOD_DPORT         0x0040
#define AVM_PA_V4_MOD_ICMPID        0x0080
#define AVM_PA_V4_MOD_PORT          (AVM_PA_V4_MOD_SPORT|AVM_PA_V4_MOD_DPORT)
#define AVM_PA_V4_MOD_PROTOHDR_CSUM 0x0100

struct avm_pa_v4_mod_rec {
   u16 flags;
   /* 2 bytes hole */
   u32 saddr; /* saddr + daddr must have same order as in ip header */
   u32 daddr;
   u8  tos;
   u8  iphlen;
   u16 l3crc_update;  /* iphdr checksum */
   union {
      u16 sport;
      u16 id;
   };
   u16 dport;
   u16 l4crc_update; /* tcp|udp checksum */
   u16 l4crc_offset; /* offsetof(struct udp|tcphdr, check) */
};

struct avm_pa_mod_rec {
   u8                       hdrcopy[AVM_PA_MAX_HEADER];
   unsigned char            hdroff; /* where hdrcopy starts (alignment) */
   unsigned char            hdrlen;
   unsigned char            pull_l2_len;     /* to strip l2 header */
   unsigned char            pull_encap_len;  /* to strip tunnel header */
   unsigned char            ipversion;
   unsigned char            v6_decrease_hop_limit;
   u16                      pkttype;         /* for pa_show_session */
   struct avm_pa_v4_mod_rec v4_mod;
   unsigned char            push_encap_len;  /* to add tunnel header */
   unsigned char            push_ipversion;
   unsigned char            push_udpoffset;  /* if lisp */
   unsigned char            push_l2_len;
   u16                      protocol;
   /* 2 byte padding */
};

/* ------------------------------------------------------------------------ */

struct avm_pa_macaddr {
   struct avm_pa_macaddr *link;
   unsigned char          mac[ETH_ALEN];
   u16                    vlan_id;
   avm_pid_handle         pid_handle;
   /* 1 byte hole */
   unsigned long          refcount;
};

#define AVM_PA_SESSION_STATS_VALID_HIT      0x01
#define AVM_PA_SESSION_STATS_VALID_PKTS     0x02
#define AVM_PA_SESSION_STATS_VALID_BYTES    0x04
struct avm_pa_session_stats {
   unsigned      validflags;
   u32           tx_pkts;
   u64           tx_bytes;
};

enum avm_pa_egresstype {
   avm_pa_egresstype_output,
   avm_pa_egresstype_local,
   avm_pa_egresstype_rtp,
   avm_pa_egresstype_xfrm,
} __attribute__((packed));


#ifdef CONFIG_TI_PACKET_PROCESSOR
#include <linux/skbuff.h> /* PP_PACKET_INFO_t */
#endif

struct avm_pa_egress {
   struct hlist_node               egress_list;
   struct avm_pa_pkt_match         match;
   avm_pid_handle                  pid_handle;
   avm_vpid_handle                 vpid_handle;
   unsigned char                   push_l2_len;
   unsigned char                   pppoe_offset;
   unsigned char                   pppoe_hdrlen; /* L2 up to PPPoE payload */
   enum avm_pa_egresstype          type;
   u16                             mtu;
   union {
      struct avm_pa_outputinfo {
         u8                        cb[48];
         u8                        mac_len;
         u16                       tc_index;
#ifdef CONFIG_NET_CLS_ACT
         u16                       tc_verd;
#endif
         u32                       orig_priority;
         u32                       priority;
         u32                       tack_priority;
         int                       skb_iif;
         struct dst_entry         *dst;
#ifdef CONFIG_TI_PACKET_PROCESSOR
         PP_PACKET_INFO_t          puma_pktinfo;
#ifdef CONFIG_TI_META_DATA
         unsigned int              ti_meta_info;
         unsigned int              ti_meta_info2;
#endif
#endif
      } output;
      struct avm_pa_localinfo {
         struct net_device        *dev;
         struct dst_entry         *dst;
         int                       skb_iif;
      } local;
      struct avm_pa_rtpinfo {
         struct net_device        *dev;
         int                       skb_iif;
         struct sock              *sk;
         void                    (*transmit)(struct sock *sk,
                                             struct sk_buff *skb);
      } rtp;
      struct avm_pa_xfrm_info {
         struct net_device        *dev;
         struct dst_entry         *dst;
         struct xfrm_state        *x;
         u16                       tc_index;
      } xfrm;
   };

   struct avm_pa_macaddr          *destmac;
   /* statistic */
   struct avm_pa_session_stats     last_sw_stats;
   struct avm_pa_session_stats     sw_stats;
   struct avm_pa_session_stats     hw_stats;
   /* TCP ACK .... */
   u32                             tx_pkts;
   u32                             tcpack_pkts;
};

struct avm_pa_session_list {
   struct list_head                sessions;
   unsigned short                  nsessions;
   unsigned short                  maxsessions;
};

#define AVM_PA_BSESSION_STRUCT 2 /* increase if struct avm_pa_bsession changes */
struct avm_pa_bsession {
   struct hlist_node       hash_list;
   struct vlan_ethhdr     *hdr;
   u32                     hash;
   avm_session_handle      session_handle;
};

#define AVM_PA_LIST_ACTIVE  0
#define AVM_PA_LIST_DEAD    1
#define AVM_PA_LIST_FREE    2
#define AVM_PA_LIST_MAX     3

#define AVM_PA_MAX_EGRESS   4

struct avm_pa_session {
   struct hlist_node               hash_list;
   struct list_head                session_list;
   struct rcu_head                 kill_rcu;
   struct hlist_head               egress_head;
   avm_session_handle              session_handle;
   u8                              routed:1,
                                   in_hw:1,
                                   realtime:1,
                                   prioack_check:1,
                                   flushed:1,
                                   no_hw:1,
                                   suspicious:1,
                                   guilty:1;
   u8                              on_list;
   u8                              needed_headroom;
   u8                              ingress_priority;
   avm_session_handle              associated_session_handle;
   u32                             uniq_id; /* monotonically increasing to detect recycle */
   avm_pid_handle                  ingress_pid_handle;
   avm_vpid_handle                 ingress_vpid_handle;
   unsigned short                  timeout;
   unsigned long                   endtime;
   struct avm_pa_pkt_match         ingress;  /* key */
   struct avm_pa_mod_rec           mod;
   struct avm_pa_egress            static_egress;
   unsigned char                   negress;
#ifdef CONFIG_AVM_PA_RPS
   unsigned char                   rps_cpu; /* CPU+1, 0 means not set */
#endif
   u16                             tget_checked_bypassed_pkts;
   struct avm_pa_bsession         *bsession;
   void                           *hw_session; /* only used by harware pa */
   const char                     *why_killed;
#ifdef CONFIG_AVM_GENERIC_CONNTRACK
   struct generic_ct              *generic_ct;
   enum generic_ct_dir             generic_ct_dir;
#endif
   struct avm_pa_session_stats     ingress_last_sw_stats;
   struct avm_pa_session_stats     ingress_sw_stats;
   struct avm_pa_session_stats     ingress_hw_stats;
   struct hlist_head               groups; /* session group of session */
   ktime_t                         stats_timestamp;
};


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
/**
 * Iterate over all egress of a session.
 *
 * rcu_read_lock() must be held as egress can be added concurrently. This
 * is held already if used inside hardware_pa callbacks.
 *
 * @param egress Loop cursor (may be uninitialized)
 * @param session The session
 */
#define avm_pa_for_each_egress(egress_, session) \
   hlist_for_each_entry_rcu((egress_), &(session)->egress_head, egress_list)

#else
/* open-code updated hlist_for_each_entry_rcu() macro introduced by
 * mainline commit b67bfe0d42 */
#define avm_pa_for_each_egress(egress_, session)                                               \
   for ((egress_) = hlist_entry(rcu_dereference_raw(hlist_first_rcu(&(session)->egress_head)), \
                              struct avm_pa_egress, egress_list);                              \
        NULL != &((egress_))->egress_list;                                                     \
        (egress_) = hlist_entry(rcu_dereference_raw(hlist_next_rcu(&(egress_)->egress_list)),  \
                              struct avm_pa_egress, egress_list))
#endif

/**
 * Get the first egress of a session.
 *
 * The first egress always part of the session so this cannot fail.
 *
 * @param session
 */
#define avm_pa_first_egress(session) (&(session)->static_egress)

#define AVM_PA_IS_ENABLED
/*
 * avm_pa_is_enabled()
 *    get enabled status
 */
int avm_pa_is_enabled(void);

/*
 * avm_pa_get_stats()
 *    get global stats
 */
void avm_pa_get_stats(struct avm_pa_stats *stats);

/*
 * avm_pa_reset_stats()
 *    reset global stats
 */
void avm_pa_reset_stats(void);

/*
 * avm_pa_dev_init()
 *    initialize dev_info
 * should be called in
 * - net/core/dev.c: alloc_netdev_mq() just before call to setup()
 */
void avm_pa_dev_init(struct avm_pa_dev_info *devinfo);

/**
 * Register a pid for dev, with another pid that will be used on ingress
 *
 * @param devinfo devinfo where pid_handle will bestored
 * @param cfg pid configuration
 * @param ingress_pid_handle pid to be used on ingress (must registered before)
 *
 * @retval 0 on success, the handle is recorded in devinfo
 * @retval -BUSY pid is already registered
 * @retval -EINVAL ingress_pid_handle is not valid
 * @retval -ENOMEM there are already too many pids in the system
 */
int avm_pa_dev_pid_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                         struct avm_pa_pid_cfg *cfg,
                                         avm_pid_handle ingress_pid_handle);

/**
 * Register a pid for dev
 *
 * A pid is an abstract endpoint for the accelerated data path. For
 * example, it may refer to a net_device.
 *
 * @param devinfo devinfo where pid_handle will bestored
 * @param cfg pid configuration
 *
 * @return @see avm_pa_dev_pid_register_with_ingress
 */
int avm_pa_dev_pid_register(struct avm_pa_dev_info *devinfo,
                            struct avm_pa_pid_cfg *cfg);

/**
 * Register a pid for a net_device
 *
 * A pid is an abstract endpoint for the accelerated data path. Commonly,
 * pids refer to a net_device and this function simplifies registration
 * in such cases. The pid will be automatically configured to use the
 * generic hooks in the networking core.
 *
 * It is not mandatory to use this function for a net_device based pid.
 * @ref avm_pa_dev_pid_register is still available for fine tuning pids
 * in any case, for example to specify more performant transmit functions.
 *
 * @param dev net_device this pid refers to
 *
 * @return @see avm_pa_dev_pid_register_with_ingress
 */
int avm_pa_dev_register(struct net_device *dev);
#define AVM_PA_DEV_REGISTER 1


#ifdef CONFIG_AVM_PA_TX_NAPI
/*
 * avm_pa_dev_pid_register_tx_napi()
 *    register a pid for dev. in contrast to avm_pa_dev_pid_register() it will
 *    use NAPI for calling the tx_func(), for this reason the net_device pointer must
 *    be given.
 * returns:
 *    0: pid_handle registered
 *   <0: no handle left to register pid
 */
struct net_device;
int avm_pa_dev_pid_register_tx_napi(struct avm_pa_dev_info *devinfo,
                                    struct avm_pa_pid_cfg *cfg,
                                    struct net_device *dev);
#endif

/* deprecated */
int avm_pa_dev_pidhandle_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                               avm_pid_handle pid_handle,
                                               struct avm_pa_pid_cfg *cfg,
                                               avm_pid_handle ingress_pid_handle);

/* deprecated */
int avm_pa_dev_pidhandle_register(struct avm_pa_dev_info *devinfo,
                                  avm_pid_handle pid_handle,
                                  struct avm_pa_pid_cfg *cfg);

/*
 * avm_pa_pid_set_ecfg()
 *    set extended config for a pid
 * returns:
 *       0: extended config set
 *      -1: pid_handle not registered
 *      -2: cb save parameter illegal
 */
int avm_pa_pid_set_ecfg(avm_pid_handle pid_handle,
                        struct avm_pa_pid_ecfg *ecfg);

/*
 * avm_pa_pid_set_framing()
 *    set framing for a pid
 * returns:
 *       0: framing set
 *      -1: pid_handle not registered
 *      -2: illegal ingress_framing
 *      -3: illegal egress_framing
 */
#define AVM_PA_PID_SET_FRAMING_IMPLEMENTED
int avm_pa_pid_set_framing(avm_pid_handle pid_handle,
                           enum avm_pa_framing ingress_framing,
                           enum avm_pa_framing egress_framing);
/*
 * avm_pa_dev_pid_set_hwinfo()
 *    set hw info for a registered pid for dev
 * returns:
 *     0: hw info set
 *    <0: pid not registered
 */

//#if defined (CONFIG_IFX_PPA) || defined (CONFIG_AVM_CPMAC_ATH_PPA)
#define AVMNET_DEVICE_IFXPPA_ETH_WAN          (1 << 10)
#define AVMNET_DEVICE_IFXPPA_ETH_LAN          (1 << 11)
#define AVMNET_DEVICE_IFXPPA_PTM_WAN          (1 << 12)
#define AVMNET_DEVICE_IFXPPA_ATM_WAN          (1 << 13)
#define AVMNET_DEVICE_IFXPPA_VIRT_RX          (1 << 14)
#define AVMNET_DEVICE_IFXPPA_VIRT_TX          (1 << 15)
#define AVMNET_DEVICE_IFXPPA_DISABLE_TX_ACL   (1 << 16)
//#endif

#ifdef CONFIG_MACH_FUSIV
#define AVM_PA_HWINFO_HAS_APID
#endif
#define AVM_PA_HWINFO_HAS_ATMVCC
struct avm_pa_pid_hwinfo {
   void *atmvcc; /* points to struct atm_vcc for ATM */
   struct avm_pa_virt_rx_dev *virt_rx_dev;
   struct avm_pa_virt_tx_dev *virt_tx_dev;
#ifdef CONFIG_IFX_PPA
   int mac_nr;
   int flags;
#endif
#if defined(CONFIG_AVM_CPMAC_ATH_PPA)
   void *avmnet_switch_module;
   int   port_number;
   int   flags;
#endif /*--- #if defined(CONFIG_AVM_CPMAC_ATH_PPA) ---*/
#ifdef CONFIG_GRX5
   struct {
      /* 
       * Set superdev for DirectPath only, e.g. netdev=ath0 superdev=wifi0.
       * wifi0 will represent a physical port, while ath0 will be registered
       * as a subif, acting like a logical endpoint.
       * If there is no need for SubIFs, it's sufficient to set netdev.
       */
      struct net_device *netdev;
      struct net_device *superdev;
      
      /* Datapath port handles (internal state)*/
      union {
         uint32_t pmac_port;
         /* legacy compat */
         uint32_t dp_subif_id;
      };
      uint32_t subif_id;
      
      /* 
       * set this to slowpath if the driver deals with datapath_api on its
       * own.
       */
      union {
         enum {
            AVM_PA_HWINFO_SLOWPATH=0,
            AVM_PA_HWINFO_DIRECTPATH=1,
            AVM_PA_HWINFO_DIRECTLINK
         } mode;
         /* anonymous union for legacy compat */
         uint32_t dp_enabled;
      };
      
      /* This pid should be used for local tx_channel acceleration */
      bool local_stack;
   } ppa;
#endif
#ifdef CONFIG_ARCH_IPQ40XX
   int port_bmp;
   int flags;
   struct net_device *netdev;
#endif
   /* All the above is semi-deprecated. The below fields support driver-managed
    * hwinfo without requiring changes to avm_pa itself.
    *
    * Call avm_pa_pid_set_hwinfo2() request extra data to be allocated.
    */
   uintptr_t priv; /*!< Reserved for internal use */
   uintptr_t hw;   /*!< Available for hwpa */
   char extra[];   /*!< For hwpa as well if hw is not sufficient */
};

#ifdef CONFIG_IFX_PPA
void avm_pa_disable_atm_hw_tx_acl(void);
void avm_pa_enable_atm_hw_tx_acl(void);
#endif

/*
 * set hwinfo for pid, hwinfo will be copied
 */
int avm_pa_pid_set_hwinfo(avm_pid_handle pid_handle,
                          struct avm_pa_pid_hwinfo *hw);
/**
 * Set hwinfo for a pid
 *
 * A copy of @c hw is stored with the pid. Extra data can
 * be requested, by passing @a sizeof_hwinfo > sizeof(struct avm_pa_pid_hwinfo).
 *
 * That extra data follows after the struct and can be accessed through the
 * @c extra array member, probably after casting to an appropriate type.
 *
 * @param pid_handle The pid to operate on
 * @param hw The hwinfo to be copied
 * @param sizeof_hwinfo size to allocate, at least sizeof(struct avm_pa_pid_hwinfo)
 * @param gfp Allocation flags
 *
 * @return 0 on success, <0 on error.
 *
 */
int avm_pa_pid_set_hwinfo2(avm_pid_handle pid_handle,
                           struct avm_pa_pid_hwinfo *hw,
                           size_t sizeof_hwinfo,
                           gfp_t gfp);
#define AVM_PA_PID_SET_HWINFO2 1
struct avm_pa_pid_hwinfo *avm_pa_pid_get_hwinfo(avm_pid_handle pid_handle);

/*
 * call after all pid parameters set
 */
int avm_pa_pid_activate_hw_accelaration(avm_pid_handle pid_handle);

#define AVM_PA_PRIO_MAP_TACK 0x0000
#define AVM_PA_PRIO_MAP_TGET 0x0001

#define AVM_PA_PID_HAS_PRIO_MAP_INTERFACE

/*
* avm_pa_pid_prio_map_enable()
*   enable or disable a priority map attached to a pid
* returns:
*       0: prio_map enabled/disabled
*      -1: pid_handle not registered
*      -2: prio_map does not exist
*/
int avm_pa_pid_prio_map_enable(avm_pid_handle pid_handle, unsigned short prio_map, int enable);

/*
* avm_pa_pid_prio_map_reset()
*   resets a priority map attached to a pid
* returns:
*       0: prio_map resetted
*      -1: pid_handle not registered
*      -2: prio_map does not exist
*/
int avm_pa_pid_prio_map_reset(avm_pid_handle pid_handle, unsigned short prio_map);

/*
 * avm_pa_pid_prio_map_set_prio_per_queue()
 *   sets the priority for a queue in a priority map attached to a pid
 * returns:
 *       0: priority set successfully
 *      -1: pid_handle not registered
 *      -2: prio_map does not exist
 *      -3: prio_map queue out of bounds
 */
int avm_pa_pid_prio_map_set_prio_per_queue(avm_pid_handle pid_handle, unsigned short prio_map, unsigned int queue, unsigned int prio);

/*
 * avm_pa_pid_activate_tcpackprio
 *    enable tcpackprio handling for a pid
 * returns:
 *       0: prioack enabled
 *      -1: pid_handle not registered
 */
#define AVM_PA_PID_ACTIVATE_TCPACKPRIO
int avm_pa_pid_activate_tcpackprio(avm_pid_handle pid_handle, int enable, unsigned int prio);

/*
 * avm_pa_pid_activate_tgetprio
 *    enable tget (turbo http-get) handling for a pid
 * returns:
 *       0: tget enabled
 *      -1: pid_handle not registered
 */
#define AVM_PA_PID_ACTIVATE_TGETPRIO
int avm_pa_pid_activate_tgetprio(avm_pid_handle pid_handle, int enable, unsigned int prio);

/*
 * avm_pa_dev_vpid_register()
 *    register a vpid for dev
 * returns:
 *    0: vpid registered
 *   <0: no handle left to register vpid
 */
int avm_pa_dev_vpid_register(struct avm_pa_dev_info *devinfo,
                             struct avm_pa_vpid_cfg *cfg);

/*
 * avm_pa_dev_vpid_register_with_tx_func()
 *    register a vpid for dev
 * returns:
 *    0: vpid registered
 *   <0: no handle left to register vpid
 */
int avm_pa_dev_vpid_register_with_tx_func(struct avm_pa_dev_info *devinfo,
                                          struct avm_pa_vpid_cfg *cfg,
                                          void (*tx_func)(void *arg, PKT *pkt),
                                          void *tx_arg);

/*
 * avm_pa_dev_vpidhandle_register()
 *    register a vpid for dev, use fixed vpid_handle
 * returns:
 *    vpid_handle != 0
 *       0: vpid_handle registered
 *      <0: different vpid_handle already registered for devinfo
 *    vpid_handle == 0
 *       0: vpid_handle registered
 *      <0: no handle left to register vpid
 */
int avm_pa_dev_vpidhandle_register(struct avm_pa_dev_info *devinfo,
                                   avm_vpid_handle vpid_handle,
                                   struct avm_pa_vpid_cfg *cfg);

/*
 * avm_pa_dev_vpidhandle_register_with_tx_func()
 *    register a vpid for dev, use fixed vpid_handle
 * returns:
 *    vpid_handle != 0
 *       0: vpid_handle registered
 *      <0: different vpid_handle already registered for devinfo
 *    vpid_handle == 0
 *       0: vpid_handle registered
 *      <0: no handle left to register vpid
 */
int avm_pa_dev_vpidhandle_register_with_tx_func(
                                    struct avm_pa_dev_info *devinfo,
                                    avm_vpid_handle vpid_handle,
                                    struct avm_pa_vpid_cfg *cfg,
                                    void (*tx_func)(void *arg, PKT *pkt),
                                    void *tx_arg);

/**
 * De-register a pid via avm_pa_dev_info
 *
 * De-regsiters a pid previously registered with one of the avm_pa_dev_register
 * functions.
 *
 * If any vpid is associated, it is de-registered as well.
 *
 * The function returns immediately, without blocking, but the effect may be delayed.
 * Iff 0 is returned you can use the completion to get signalled appropriately.
 *
 * @see avm_pa_dev_unregister_sync()
 *
 * @param devinfo structure indicating the pid to be removed.
 * @param done completion to be signaled when the function is truly done.
 *
 * @retval 0 Success, may wait for the completion (if any).
 * @retval -ENODEV This pid wasn't registered before (or already de-registered).
 */
int avm_pa_dev_unregister(struct avm_pa_dev_info *devinfo, struct completion *done);
#define AVM_PA_DEV_UNREGISTER 2

/**
 * De-register a pid via avm_pa_dev_info (blocking)
 *
 * Same as @ref avm_pa_dev_unregister() but uses a completion internally
 * to synchronize, so it may block!
 *
 * @see avm_pa_dev_unregister()
 *
 * @param devinfo structure indicating the pid to be removed.
 *
 * @retval 0 Success.
 * @retval -ENODEV This pid wasn't registered before (or already de-registered).
 */
int avm_pa_dev_unregister_sync(struct avm_pa_dev_info *devinfo);
#define AVM_PA_DEV_UNREGISTER_SYNC 1

/*
 * avm_pa_vpid_set_ipv4_mtu
 *    set mtu for ipv4
 */
void avm_pa_dev_set_ipv4_mtu(struct avm_pa_dev_info *devinfo, u16 mtu);

/*
 * avm_pa_vpid_set_ipv6_mtu
 *    set mtu for ipv6
 */
void avm_pa_dev_set_ipv6_mtu(struct avm_pa_dev_info *devinfo, u16 mtu);

/*
 * avm_pa_dev_get_stats()
 *    get vpid stats
 * returns:
 *    < 0: vpid not registered, "stats" set to zero
 *      0: statistic copied to "stats"
 */
int avm_pa_dev_get_stats(struct avm_pa_dev_info *devinfo,
                         struct avm_pa_vpid_stats *stats);
/* Unaccelerated packets are counted in the vpid stats */
#define AVM_PA_VPID_STATS_COUNT_SLOW

/*
 * avm_pa_dev_reset_stats()
 *    reset vpid stats
 * returns:
 *    < 0: vpid not registered.
 *      0: statistic reseted to zero
 */
int avm_pa_dev_reset_stats(struct avm_pa_dev_info *devinfo);

/*
 * avm_pa_flush_sessions()
 *    flushes all sessions
 */
void avm_pa_flush_sessions(void);

/*
 * avm_pa_flush_lispencap_sessions()
 *    flushes lisp encap sessions
 */
void avm_pa_flush_lispencap_sessions(void);

/*
 * avm_pa_flush_rtp_session()
 *    flushes a rtp session
 */
void avm_pa_flush_rtp_session(struct sock *sk);

/*
 * avm_pa_flush_multicast_sessions()
 *    flushes multicast sessions
 */
void avm_pa_flush_multicast_sessions(void);

/*
 * avm_pa_flush_multicast_sessions_for_group()
 *    flushes multicast sessions for specified group
 */
void avm_pa_flush_multicast_sessions_for_group(u32 group);

/*
 * avm_pa_flush_sessions_for_vpid()
 *    flushes all sessions having vpid as ingress or egress
 */
void avm_pa_flush_sessions_for_vpid(avm_vpid_handle vpid_handle);

/*
 * avm_pa_flush_sessions_for_pid()
 *    flushes all sessions having pid as ingress or egress
 */
void avm_pa_flush_sessions_for_pid(avm_pid_handle pid_handle);

/*
 * flush sessions of a session group
 */
void avm_pa_flush_sessions_for_sg(unsigned short groupid);


/**
 * flush sessions for a destination mac address
 *
 * VLAN will not be considered, i.e. any VLAN will match.
 * Also, only the destination MAC address will be considered.
 *
 * @param mac MAC address to flush sessions for
 */
void avm_pa_flush_sessions_for_mac(const unsigned char mac[ETH_ALEN]);
#define AVM_PA_FLUSH_SESSIONS_FOR_MAC 1

#define AVM_PA_RX_BROADCAST         5  /* is broadcast */
#define AVM_PA_RX_TTL               4  /* ttl/hoplimit is 1 */
#define AVM_PA_RX_FRAGMENT          3  /* is fragment, cannot be accelerated */
#define AVM_PA_RX_BYPASS            2  /* packet cannot be accelerated */
#define AVM_PA_RX_OK                1  /* packet maybe be accelerated */
#define AVM_PA_RX_STOLEN            0  /* packet consumed (accelerated) */
#define AVM_PA_RX_ACCELERATED     AVM_PA_RX_STOLEN
#define AVM_PA_RX_ERROR_STATE      -1  /* state machine problem ? */
#define AVM_PA_RX_ERROR_LEN        -2  /* packet too short */
#define AVM_PA_RX_ERROR_IPVERSION  -3  /* illegal ip version */
#define AVM_PA_RX_ERROR_MATCH      -4  /* too much header */
#define AVM_PA_RX_ERROR_HDR        -5  /* too much ip header */

/*
 * avm_pa_dev_receive()
 *   if vpid set: remember ingress vpid, for statistics
 *   if pid set:  function tries to accelerate packet
 * returns:
 *   < 0: you can free the packet or send it the slow way to get it dropped
 *     0: packet was accelerated (so it is gone, don't free it)
 *   > 0: send packet the slow way
 * should be called in
 * - net/core/dev.c: netif_receive_skb() before protocol handling
 */
int avm_pa_dev_receive(struct avm_pa_dev_info *devinfo, PKT *pkt);
/*
 * avm_pa_dev_pid_receive()
 *   if pid set:  function tries to accelerate packet
 * returns:
 *   < 0: you can free the packet or send it the slow way to get it dropped
 *     0: packet was accelerated (so it is gone, don't free it)
 *   > 0: send packet the slow way
 * should be called when pid may have rx channel handling
 */
int avm_pa_dev_pid_receive(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_dev_vpid_snoop_receive()
 *   if vpid set: remember ingress vpid, for statistics
 */
void avm_pa_dev_vpid_snoop_receive(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_mark_routed()
 *   mark packet as routed
 * should be called in
 * - net/ipv4/ip_forward.c: ip_forward_finish()
 * - net/ipv6/ip6_output.c: ip6_forward_finish()
 */
void avm_pa_mark_routed(PKT *pkt);

/*
 * avm_pa_mark_shaped()
 *   mark packet as bypassing tc, allowing low-level egress pids to take over sessions
 *   that need no shaping
 * should be called in:
 * - net/core/dev.c: __dev_xmit_skb() before q->enqueue()
 */
void avm_pa_mark_shaped(PKT *pkt);
#define AVM_PA_MARK_SHAPED

/**
 * Set the RPS configuration that sessions created from the packet will follow.
 *
 * This will override automatic RPS based on the session hash. Typically you
 * want automatic RPS but there may be use cases where a set of cores shouldn't be
 * considered for RPS because they run other workloads (e.g. crypto for ipsec traffic).
 *
 * If @a allow and @a fallback are empty masks, RPS is disabled entirely. If @a is a
 * full mask, automatic RPS is effectively restored.
 *
 * @note is a no-op if CONFIG_AVM_PA_RPS is not enabled.
 *
 * @param skb the packet to mark
 * @param allow mask of allowed cpus
 * @param fallback mask of fallback cpus
 */
void avm_pa_skb_set_rps(struct sk_buff *skb,
                        const struct cpumask *allow,
                        const struct cpumask *fallback);
#define AVM_PA_SKB_SET_RPS 1

/*
 * avm_pa_use_protocol_specific_session()
 *   if this packet creats a session, it should be
 *   a protocol specific session (no bridge session).
 * should be called when protocol specific filting
 * or protocol specific queuing is done on datapath.
 */
void avm_pa_use_protocol_specific_session(PKT *pkt);

/*
 * avm_pa_do_not_accelerate()
 *    mark packet to not create a session
 * should be called
 * - for packets manipulate by ALG (FTP-Control, tftp, ...)
 * - in net/ipv4/mcfastforward.c: mcfw_multicast_forward() (for now)
 * - in net/bridge/br_forward.c: for flooded packets
 */
void avm_pa_do_not_accelerate(PKT *pkt);


/*
 * avm_pa_set_hstart()
 *   store offset where encap header starts
 * should be called, when hstart != 0 before calling
 *   avm_pa_dev_receive()
 *   avm_pa_dev_snoop_transmit()
 */
void avm_pa_set_hstart(PKT *pkt, unsigned int hstart);

#define AVM_PA_TX_BYPASS            4  /* packet cannot be accelerated */
#define AVM_PA_TX_SESSION_EXISTS    3  /* session already exists */
#define AVM_PA_TX_EGRESS_ADDED      2  /* egress in session added */
#define AVM_PA_TX_SESSION_ADDED     1  /* session added */
#define AVM_PA_TX_OK                0  /* nothing done */
#define AVM_PA_TX_ERROR_SESSION    -1  /* session creation failed */
#define AVM_PA_TX_ERROR_EGRESS     -2  /* egress creation failed */

/*
 * avm_pa_dev_snoop_transmit()
 *   if vpid set: remember egress vpid, for statistics
 *   if pid set:  try to create a session for this packet
 * returns:
 *   > 0: some action done
 *     0: nothing done or egress vpid remembered
 *   < 0: some error
 * should be called in
 * - net/core/dev.c: dev_queue_xmit() before picking tx queue
 *   if dev has an pid set.
 */
int avm_pa_dev_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_dev_vpid_snoop_transmit()
 *   if vpid set: remember egress vpid, for statistics
 */
void avm_pa_dev_vpid_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt);

/* This was moved from skbuff.h; the declarations are guarded
 * with transitional defines to allow build against older kernel tags
 *
 * Some versions of skbuff.h (QCA, VR9, PUMA6) have no forward declaration of
 * _avm_pa_add_local_sesion() themselves, so it needs to be outside the guard.
 */
void _avm_pa_add_local_session(PKT *pkt, struct sock *sk);
#if defined(NETDEVICE_H_NO_AVM_PA_H) || defined(SKBUFF_H_NO_AVM_PA_H)
/**
 * Create a locally-terminating session
 *
 * Try to create a session for this packet when the packet terminates
 * lcoally, i.e. is send out to a socket for user space. The socket is
 * not truly necessary, dependening on the actual egress associated with
 * the pid.
 *
 * The pid is specified by setting AVM_PKT_INFO(pkt)->ptype_pid_handle
 * before calling this function.
 *
 * @param skb the packet on egress
 * @param sk the destination socket (may be NULL)
 */
static inline void avm_pa_add_local_session(PKT *pkt, struct sock *sk)
{
       struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);

       if (info->ptype_pid_handle && info->is_accelerated == 0)
               _avm_pa_add_local_session(pkt, sk);
}
#define AVM_PA_ADD_LOCAL_SESSION 1

/**
 * create a local session for egress to xfrm
 *
 * This is much like a normal session but is specialized for xfrm egress:
 *  - session information will display information about the xfrm state
 *  - acceleration operates on layer 3 only, especially egress won't
 *    add or modify any L2 headers
 *  - the tx_func of the pid will get the xfrm_state as parameter
 *
 * The latter requires the pid to pass NULL for tx_arg. In practice, this
 * means that the pid cannot be used for other session types.
 *
 * @param devinfo devinfo for the pid
 * @param skb the packet on egress
 * @param x xfrm_state that is recorded with the session and passed to the pid's tx_func
 */
void avm_pa_add_xfrm_session(struct avm_pa_dev_info *devinfo, struct sk_buff *skb, struct xfrm_state *x);
#define AVM_PA_ADD_XFRM_SESSION 1


int _avm_pa_local_out_receive(avm_pid_handle pid_handle, struct sk_buff *skb);
/**
 * Try to accelerate a locally-created packet.
 *
 * If a session exists, then this function conumes the packet for
 * bypassing the IP stack. Otherwise, this function classifies the packet
 * and gives it back for the slow path.
 *
 * Usually this is for packets that originate from user space process
 * and enter the IP stack from a socket. But this not always true and
 * it's not an error if there is no socket.
 *
 * This function is much like avm_pa_dev_receive() but it doesn't expected
 * an Ethernet frame and works only on the IP level.
 *
 * @param skb the packet to be send
 * @param sk the socket where the packet came from, maybe NULL.
 * @param family AF_INET or AF_INET6, indicating what header skb->data points to.
 *
 * @return AVM_RX_ACCELERATED if the packet is consumed for bypass, otherwise AVM_PA_RX_OK.
 */
static inline int avm_pa_local_out_receive(struct sk_buff *skb, struct sock *sk, int family)
{
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(skb);

   if (family == AF_INET || family == AF_INET6) {
      if (info->local_out_pid_handle) {
         skb->protocol = (family == AF_INET) ? htons(ETH_P_IP) : htons(ETH_P_IPV6);
         return _avm_pa_local_out_receive(info->local_out_pid_handle, skb);
      }
   }
   return AVM_PA_RX_OK;
}
#define AVM_PA_LOCAL_OUT_RECEIVE 1
#endif

/*
 * avm_pa_add_rtp_session()
 *   local session for this packet must exist.
 * session is rewritten to rtp session
 */
void avm_pa_add_rtp_session(PKT *pkt,
                            struct sock *sk,
                            void (*transmit)(struct sock *sk, PKT *pkt));

/** Not implemented !!
 *
 * Add a drop session for this packet. Future packets will be dropped immediately.
 * This function does nothing at the moment because drop sessions are not implemented.
 */
void avm_pa_filter_packet(PKT *pkt);

/*
 * avm_pa_dev_get_hw_stats()
 * get upstream statistics for HW accelerated packets
 * @prio: upstream priority
 */
int avm_pa_dev_get_hw_stats(struct avm_pa_dev_info *devinfo,
                            struct avm_pa_traffic_stats *stats,
                            unsigned int prio);

/*
 * avm_pa_dev_get_prio_stats()
 * get upstream statistics for SW and HW accelerated packets
 * @prio: upstream priority
 */
int avm_pa_dev_get_prio_stats(struct avm_pa_dev_info *devinfo,
                              struct avm_pa_prio_stats *stats,
                              unsigned int prio);

/*
 * avm_pa_dev_get_ingress_prio_stats()
 * get ingress (downstream) statistics for SW and HW accelerated packets
 * @prio: ingress priority
 */
#define AVM_PA_DEV_GET_INGRESS_PRIO_STATS
int avm_pa_dev_get_ingress_prio_stats(struct avm_pa_dev_info *devinfo,
                                      struct avm_pa_prio_stats *stats,
                                      unsigned int prio);
/*
 * avm_pa_telefon_state()
 *   notify avm pa about telephony acitivity.
 *   state == 0, no phone calls active
 *   state != 0, phone calls active or in progress
 */
void avm_pa_telefon_state(int state);

#define AVM_PA_SELECTOR_INTERFACES 2 /* increment when interfaces are added or removed */
/**
 * Whether or not a session is selected by the selector list.
 *
 * @param selector_list The list containing applicable selectors
 * @param sess the session to be checked
 *
 * @retval 1 - the session is selected one or more selectors
 * @retval 0 - the session is not selected by any selector
 */
int avm_pa_session_is_selected(struct list_head *selector_list, struct avm_pa_session *sess);

/**
 * Parse selector list from string.
 *
 * Upon successful parsing, the selector list is populated with entries that select
 * (match) sessions (@see avm_pa_session_is_selected). You must free those using
 * @ref avm_pa_selector_free().
 *
 * This function allocates memory using GFP_KERNEL so it must not be
 * used in atomic contexts.
 *
 * @note Do not use the pid+vpid selectors when caching selector_list for longer
 * periods, since pids/vipds could be unregestiered in the meantime, which makes
 * the selector invalid. This can't be detected currently so consider the behavior
 * as undefined. If you need pid+vpid selectors always create them freshly,
 * but it's recommended to avoid them entirely if possible.
 *
 * @param selector_list The list containing applicable selectors.
 * @param buffer String containing the match.
 * @param gfp_mask gfp used for internal memory allocations.
 * @return 0 on success
 * @retval -EINVAL on parsing error
 * @retval -EIO on too long selector string
 * @retval -ENOMEM on out-of-memory
 */
ssize_t avm_pa_parse_selector(struct list_head *selector_list,
                              const char *buffer,
                              gfp_t gfp_mask);

/**
 * Iterate over sessions, calling a function for selected ones.
 *
 * The callback function is called for every session that is selected by
 * @a selector_list.
 *
 * The iteration is stopped once the callback function returns a non-zero value,
 * so you can use it to find the first selected session. In that case,
 * the return value of the callback is passed through.
 *
 * The callback function must not sleep!
 *
 * @param selector_list The list containing applicable selectors.
 * @param func Function to be invoked for every selected session.
 * @param data User-defined data passed to the callback.
 * @return 0 if the iteration completes, or the return value of the callback.
 */
int avm_pa_selector_foreach(struct list_head *selector_list,
                            int (*func)(struct avm_pa_session *session, void *data),
                            void *data);

/**
 * Free entries and clear selector list (make it empty).
 *
 * @param selector_list The list containing applicable selectors.
 */
void avm_pa_selector_free(struct list_head *selector_list);

/**
 * Alloc a session group by it's session group id
 *
 * @param groupid id of session group
 * @return 0 if alloc was successfull, or < 0 no error
 */
int avm_pa_sg_alloc_by_groupid(unsigned short groupid);

/**
 * Alloc a session group
 *
 * @return groupid if alloc was successfull, or 0 no error
 */
unsigned short avm_pa_sg_alloc(void);

/**
 * free a session group
 *
 * @return 0 if free was successfull, or <0 on error
 */
int avm_pa_sg_free(unsigned short groupid);

/**
 * restart a session group
 * remove sessions from session group and reset 'closed' session counters
 *
 * @return 0 if restart was successfull, or <0 on error
 */
int avm_pa_sg_restart(unsigned short groupid);

/**
 * mark a session group to count egress or ingress (default is ingress)
 *
 * @return 0 if group was marked, or <0 on error
 */
#define AVM_PA_HAS_SG_SET_COUNT_EGRESS
int avm_pa_sg_set_count_egress(unsigned short groupid, bool enable);

/**
 * Get stats for session group
 *
 * @param [in] groupid id of session group
 * @param [out] values of session group
 *
 * @return 0 if session group exists
 */
int avm_pa_sg_get_stats(unsigned short groupid,
                        struct avm_pa_sg_stats *counter);

/**
 * Assign session group to a skb, if a session will be created by this skb,
 * packets accelerated by AVM PA will be counted in that session group
 *
 * @param [in] skb sk_buff to assign goup counter to
 * @param [in] id of session group
 *
 * @return 0 if session group was assign, < 0 on error
 */
int avm_pa_sg_mark_skb(struct sk_buff *skb, unsigned short groupid);

#endif /* _LINUX_AVM_PA_H */
