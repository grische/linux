/*
 * Packet Accelerator Interface
 *
 * vim:set expandtab shiftwidth=3 softtabstop=3:
 *
 * Copyright (c) 2011-2020 AVM GmbH <info@avm.de>
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
 *
 * PID  - pheripheral ID
 *        Identifies a low level device, may be a network driver or
 *        for ATM, every VCC has its own PID
 * VPID - virtual pheripheral ID
 *        Is assigned to a network device or a virtual network device
 *
 *   Sessions can have four states:
 *   - FREE    : session on sess_list[AVM_PA_LIST_FREE]
 *   - CREATE  : session is on no list
 *   - ACTIVE  : session on sess_list[AVM_PA_LIST_ACTIVE], in hashtable and not flushed
 *   - FLUSHED : session on sess_list[AVM_PA_LIST_ACTIVE], in hashtable and flushed
 *   - DEAD    : session on sess_list[AVM_PA_LIST_DEAD]
 *
 *   FREE    -> pa_session_alloc()    -> CREATE
 *   CREATE  -> pa_session_activate() -> ACTIVE
 *   ACTIVE  -> pa_session_flush()    -> FLUSHED
 *   FLUSHED -> pa_session_tick()     -> DEAD
 *   DEAD    -> pa_session_tick()     -> FREE
 *
 *   pa_session_kill() can transition from any state to DEAD. Use it only if you
 *   know that an immediate GC trigger (that moves from DEAD to FREE) won't be
 *   a problem, otherwise use pa_session_flush() which is safe. In general,
 *   this is only the case when a session wasn't ACTIVE yet (before
 *   pa_session_activate() completes). pa_session_flush() guarantees that at least one
 *   complete GC period happens before a session transitions to FREE.
 */

#define AVM_PA_FORCE_PRINTK_ENABLED 0

#if AVM_PA_FORCE_PRINTK_ENABLED
#   ifdef CONFIG_NO_PRINTK
#   define printk __printk
#   endif
#   define DEBUG /* want pr_debug to be compiled in */
#endif


#include <linux/version.h>
#include <linux/utsname.h>
#include <linux/printk.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <net/xfrm.h>
#include <net/ipv6.h>
#include <asm/unaligned.h>
#include <net/checksum.h>
#include <net/pkt_sched.h>
/* Necessary for MIPS Platforms without arch-support for ipv6 chksums */
#include <net/ip6_checksum.h>
#include <linux/pkt_sched.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#define PSCHED_TICKS2NS(x)  PSCHED_US2NS(x)
#define PSCHED_NS2TICKS(x)  PSCHED_NS2US(x)
#endif
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
/* ktime_get() is good enough as a fallback (doesn't account for suspend time) */
#define ktime_get_boottime ktime_get
#endif
#ifdef CONFIG_AVM_POWERMETER
#include <avm/power/power.h>
#endif
#ifdef CONFIG_AVM_SIMPLE_PROFILING
#include <avm/profile/profile.h>
#else
#define avm_simple_profiling_skb(a,b) do { } while(0)
#endif
#include <linux/module.h> // MODULE_NAME_LEN needed by kallsyms.h (who fails to include himself)
#include <linux/kallsyms.h> // sprint_symbol()
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/if_vlan.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

#include <asm/cputime.h>
#include <linux/kernel_stat.h>

#include "avm_pa.h"
#include "avm_pa_hw.h"
#include "avm_pa_intern.h"

#ifdef CONFIG_L2TP
#include <linux/l2tp.h>
#include "../l2tp/l2tp_core.h"
#endif

/* ------------------------------------------------------------------------ */

#ifndef cputime_to_msecs
#define cputime_to_msecs(__ct)      jiffies_to_msecs(__ct)
#endif
#ifndef msecs_to_cputime
#define msecs_to_cputime(__msecs)   msecs_to_jiffies(__msecs)
#endif

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif
#ifndef cputime64_zero
#define cputime64_zero 0ULL
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
#define SKB_IFF(skb) ((skb)->skb_iif)
#else
#define SKB_IFF(skb) ((skb)->iif)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
#define skb_vlan_tag_get     vlan_tx_tag_get
#define skb_vlan_tag_present vlan_tx_tag_present
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
/* For non-broken smp_call_function_single_async() the following commits are needed:
 * commit 5224b961 smp: Fix error case handling in smp_call_function_*()
 * commit 8053871d smp: Fix smp_call_function_single_async() locking
 *
 * The commits landed in Linux 4.1. Any older kernel lacking those have a broken
 * smp_call_function_single_async() and we cannot use RPS (we saw panics every now and then).
 */
#ifdef CONFIG_AVM_PA_RPS
#error Broken smp_call_function_single_async(). Upgrade the kernel, backport 8053871d and 5224b961 or disable CONFIG_AVM_PA_RPS.
#endif
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 17, 0)
/* See mainline commits:
 * commit 1d023284 list: fix order of arguments for hlist_add_after(_rcu)
 *
 * Note that the macro was renamed and arguments order swapped.
 */
#define hlist_add_behind_rcu(new, prev) hlist_add_after_rcu(prev, new)
#endif

/* ------------------------------------------------------------------------ */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
static inline struct dst_entry *skb_dst(const struct sk_buff *skb)
{
   return skb->dst;
}

static inline void skb_dst_set(struct sk_buff *skb, struct dst_entry *dst)
{
   skb->dst = dst;
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
int kstrtol(const char *s, unsigned int base, long *res)
{
    if (isdigit(*s)) {
       *res = simple_strtol(s, 0, base);
       return 0;
    }
    return -EINVAL;
}
int kstrtoul(const char *s, unsigned int base, unsigned long *res)
{
    if (isdigit(*s)) {
       *res = simple_strtoul(s, 0, base);
       return 0;
    }
    return -EINVAL;
}
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 0, 0)
static inline void skb_reset_mac_len(struct sk_buff *skb)
{
	skb->mac_len = skb->network_header - skb->mac_header;
}
#endif


/* ------------------------------------------------------------------------ */

/* tbfqueue is known to be buggy (JZ-47878, JZ-46754), therefore disabled
 * by default to see if we even need it at all */
#define AVM_LOAD_CONTROL_ENABLED 0

#ifdef CONFIG_MIPS_UR8
#define AVM_PA_START_DISABLED     1
#endif

#define AVM_PA_TRACE              1  /* 0: off */
#define AVM_PA_TOKSTATS           0
#define AVM_PA_UNALIGNED_CHECK    0
#define AVM_PA_REF_DEBUG          0  /* 0: off */


/* GSO is disabled for now, since it's not sufficiently tested.
 * Tests need to be done on 4040, 7580, 6490, with vlan and/or pppoe encap on egreess.
 * So far it's been successfully tested on 4040 with plain ethernet+NAT. */
#define AVM_PA_WITH_GSO           0

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
#define skb_has_frag_list(skb) (0)
#define skb_walk_frags(skb, iter) while (0)
#endif

#define TX_NAPI_MAXQUEUE        512
#define TX_NAPI_BUDGET           64

#ifndef ETH_P_8021AD
#define ETH_P_8021AD	0x88A8
#endif

/* ------------------------------------------------------------------------ */

static inline void set_ip_checksum(struct iphdr *iph)
{
   int iphlen = PA_IPHLEN(iph);
   iph->check = 0;
   iph->check = csum_fold(csum_partial((unsigned char *)iph, iphlen, 0));
}

static inline void set_udp_checksum(struct iphdr *iph, struct udphdr *udph)
{
   unsigned short len = ntohs(udph->len);
   __wsum sum;

   udph->check = 0;
   sum = csum_partial((unsigned char *)udph, len, 0);
   udph->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
                                   len, IPPROTO_UDP, sum);
   if (udph->check == 0)
      udph->check = CSUM_MANGLED_0;
}

static inline void set_udpv6_checksum(struct ipv6hdr *ipv6h,
                                      struct udphdr *udph)
{
   unsigned short len = ntohs(udph->len);
   __wsum sum;

   udph->check = 0;
   sum = csum_partial((unsigned char *)udph, len, 0);
   udph->check = csum_ipv6_magic(&ipv6h->saddr, &ipv6h->daddr,
                                 len, IPPROTO_UDP, sum);
   if (udph->check == 0)
      udph->check = CSUM_MANGLED_0;
}

/* Return a uniq id for a give skb. Currently it simply
 * returns its addresses with the always-zero low bits shifted away.
 */
static inline unsigned long pkt_uniq_id(PKT *pkt)
{
   unsigned long addr = (unsigned long) pkt;
   unsigned long shift = max(L1_CACHE_SHIFT, 2);

   return addr >> shift;
}

/* ------------------------------------------------------------------------ */

static inline int rand(void)
{
   int x;
   get_random_bytes(&x, sizeof(x));
   return x;
}

#define PKT_DATA(pkt)   (pkt)->data
/* PKT_LEN has the data in the head skb. For frag_list skbs, this is just L2/3/4 headers
 * without any payload. For normal skbs it includes the payload after the headers. */
#define PKT_LEN(pkt)   (skb_headlen(pkt))
/* For frag_list skbs, PKT_FRAGLEN is the size a single packet (with headers). That is
 * PKT_LEN of the head skb (just headers without payload), plus payload length of the
 * first frag, assuming no other frag is larger. For normal skbs, this is the same as PKT_LEN */
#define PKT_FRAGLEN(pkt) (PKT_LEN(pkt) + (skb_has_frag_list(pkt) ? skb_shinfo(pkt)->frag_list->len : 0))
#define PKT_PULL(pkt, len) skb_pull(pkt, len)
#define PKT_PUSH(pkt, len) skb_push(pkt, len)
#define PKT_FREE(pkt)  dev_kfree_skb_any(pkt)
#define PKT_COPY(pkt)  skb_copy(pkt, GFP_ATOMIC)
#define PKT_TRIM(pkt, len) pskb_trim(pkt, len)
#ifdef AVM_PA_SKBUFF_HAS_VLAN_PROTO
#define PA_VLAN_PROTO(pkt) (pkt)->vlan_proto
#else
#define PA_VLAN_PROTO(pkt) (constant_htons(ETH_P_8021Q))
#endif

static int pa_printk(void *type, const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((__format__(__printf__, 2, 3)))
#endif
;

static int pa_printk(void *type, const char *format, ...)
{
   va_list args;
   int rc;

   va_start(args, format);
   if (type) printk("%s", (char *)type);
   rc = vprintk(format, args);
   va_end(args);
   return rc;
}

/* ------------------------------------------------------------------------ */

#define constant_htons(x)   __constant_htons(x)

#undef IPPROTO_IPENCAP
#define IPPROTO_IPENCAP 4
#ifndef IPPROTO_L2TP
#define IPPROTO_L2TP    115
#endif

/*
 * Accelerating of L2TPv3 only works with
 * pseudowire ethernet or ethernet vlan
 * and default l2-specific header.
 */

/* ------------------------------------------------------------------------ */

#define AVM_PA_TICK_RATE                (500*HZ/1000) /* 0.5 secs */
#define AVM_PA_LC_TIMEOUT                           2 /* secs */
#define AVM_PA_TRAFFIC_IDLE_TBFDISABLE             10 /* secs */

/* ------------------------------------------------------------------------ */

#define AVM_PA_MAX_TBF_QUEUE_LEN     128
#define AVM_PA_MAX_IRQ_QUEUE_LEN      64

#define AVM_PA_DEFAULT_MAXRATE          5000
#define AVM_PA_MINRATE                  1000
#define AVM_PA_DEFAULT_PKTBUFFER        1024
#define AVM_PA_DEFAULT_PKTPEAK           256
#define AVM_PA_DEFAULT_TELEPHONY_REDUCE   65

#define AVM_PA_EST_DEFAULT_IDX                0 /* 0 - 5 => 0.25sec - 8sec */
#define AVM_PA_EST_DEFAULT_EWMA_LOG           3 /* 1 - 31 */
#define AVM_PA_CPUTIME_EST_DEFAULT_IDX        2 /* 0 - 5 => 0.25sec - 8sec */
#define AVM_PA_CPUTIME_EST_DEFAULT_EWMA_LOG   1 /* 1 - 31 */

#define AVM_PA_CPUTIME_IRQ_MSWIN_LOW            300 /* ms/s */
#define AVM_PA_CPUTIME_IRQ_MSWIN_HIGH           400 /* ms/s */
#define AVM_PA_CPUTIME_IDLE_MSWIN_LOW           10  /* ms/s */
#define AVM_PA_CPUTIME_IDLE_MSWIN_HIGH          50  /* ms/s */

#define AVM_PA_PRIOACK_THRESH_PKTS   40   /* wait for X packets to do the TCP-ACK check */
#define AVM_PA_PRIOACK_RATIO         70   /* % of packets have to be TCP-ACKs for positive check */

#define AVM_PA_COUNT_PRIO_MAPS  2 /* tack and tget */

#define AVM_PA_BE_QUEUE 6 /* best-effort queue */

#define AVM_PA_INGRESS_PRIO_NET_MASK   0xFFFF0000U
#define AVM_PA_INGRESS_PRIO_HOST_MASK  0x0000FFFFU

#define AVM_PA_INGRESS_PRIO_NET(prio)  (((prio) & AVM_PA_INGRESS_PRIO_NET_MASK) >> 16)
#define AVM_PA_INGRESS_PRIO_HOST(prio) ( (prio) & AVM_PA_INGRESS_PRIO_HOST_MASK)

/* ------------------------------------------------------------------------ */

static DEFINE_SPINLOCK(avm_pa_lock);

struct avm_pa_est {
   unsigned                  idx;
   unsigned                  ewma_log;
   u32                       last_packets;
   u32                       avpps;
};

struct avm_pa_cputime_est {
   unsigned                  idx;
   unsigned                  ewma_log;
   cputime64_t               last_cputime;
   cputime_t                 avtps;
};

struct avm_pa_tbf
{
   struct hrtimer timer;
   u32            buffer;
   u32            pbuffer;
   u32            pkttime;
   long           tokens;
   long           ptokens;
   psched_time_t  t_c;
};

struct avm_pa_global {
   int                       disabled;
   int                       fw_disabled;
   atomic_t                  misc_is_open; /* means fw_disabled */
   int                       dbgcapture;
   int                       dbgsession;
   int                       dbgnosession;
   int                       dbgtrace;
   int                       dbgmatch;
   int                       dbgcputime;
   int                       dbgprioack;
   int                       dbgprioacktrace;
   int                       dbgstats;
   bool                      bsession_allowed;
   unsigned long             tcp_timeout_secs;
   unsigned long             udp_timeout_secs;
   unsigned long             echo_timeout_secs;
   unsigned long             bridge_timeout_secs;
   struct avm_pa_pid         pid_array[CONFIG_AVM_PA_MAX_PID];
   struct avm_pa_vpid        vpid_array[CONFIG_AVM_PA_MAX_VPID];
   struct avm_pa_session_list sess_list[AVM_PA_LIST_MAX];
   struct avm_pa_bsession    bsess_array[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_macaddr     macaddr_array[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_macaddr    *macaddr_hash[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_stats       stats, stats_copy;
   struct hlist_head         egress_freelist;
   u32                       next_session_uniq_id;

   struct timer_list         tick_timer;
   struct sk_buff_head       irqqueue;
   struct tasklet_struct     irqtasklet;

   /* packet rate estimater */
   char                      est_start[0];
   int                       est_idx;
   int                       ewma_log;
   struct timer_list         est_timer;
   struct avm_pa_est         rx_est;
   struct avm_pa_est         fw_est;
   struct avm_pa_est         overlimit_est;
   /* cputime estimater */
   int                       cputime_est_idx;
   int                       cputime_ewma_log;
   struct timer_list         cputime_est_timer;
   struct avm_pa_cputime_est cputime_user_est;
   struct avm_pa_cputime_est cputime_idle_est;
   struct avm_pa_cputime_est cputime_irq_est;
   char                      est_end[0];
   /* tbf for packets per second */
   int                       load_control;
#define LOADCONTROL_OFF       0x00
#define LOADCONTROL_POWER     0x01
#define LOADCONTROL_IRQ       0x02
#define LOADCONTROL_POWERIRQ  (LOADCONTROL_POWER|LOADCONTROL_IRQ)
#define LOADCONTROL_IDLE      0x04
   int                       load_reduce;
   int                       telephony_active;
   unsigned                  telephony_reduce;
   int                       tbf_enabled;
   unsigned                  irq_mswin_low;   /* max irq ms/s */
   unsigned                  irq_mswin_high;  /* overload irq ms/s */
   unsigned                  idle_mswin_low;   /* overload idle ms/s */
   unsigned                  idle_mswin_high;  /* good idle ms/s */
   unsigned                  maxrate;     /* pkt/s at load_reduce == 0 */
   unsigned                  rate;        /* pkt/s */
   unsigned                  pktbuffer;   /* # pkts */
   unsigned                  pktpeak;     /* # pkts */
   struct avm_pa_tbf         tbf;
   struct sk_buff_head       tbfqueue;
   struct tasklet_struct     tbftasklet;
   int                       rps_enabled;
#ifdef CONFIG_AVM_PA_RPS
   struct avm_pa_rps {
      struct sk_buff_head    q_local; /* enqueue/dequeue from the same core, no locking */
      struct sk_buff_head    q_other; /* enqueue/dequeue form other cores, with locking */
      struct tasklet_struct  dequeue_task;
      struct tasklet_struct  ipi_task;
      struct call_single_data csd;
      unsigned long          rx_enqueued;
      unsigned long          rx_rps_ipis;
      unsigned long          rx_dequeued;
   } rps[CONFIG_AVM_PA_RPS_QUEUES];
#endif
#if AVM_LOAD_CONTROL_ENABLED
   struct timer_list         lc_timer;
   u32                       lc_overlimit; /* rx_overlimit at last tick_timer */
#ifdef CONFIG_AVM_POWERMETER
   void                     *load_control_handle;
#endif
#endif
   /* ... */
   char                      tok_start[0];
   struct task_struct       *tok_task;
   int                       tok_pos;
#define TOK_SAMLES  64
   int                       tok_state[TOK_SAMLES];
   unsigned                  tok_overtime[TOK_SAMLES];
   unsigned                  tok_rate[TOK_SAMLES];
   unsigned                  tok_pps[TOK_SAMLES];
   unsigned long             tok_overlimit[TOK_SAMLES];
   char                      tok_end[0];
   unsigned                  prioack_thresh_packets;
   unsigned                  prioack_ratio;
   struct avm_hardware_pa    hardware_pa;
   int                       hw_ppa_disabled;
   struct completion        *hw_pa_flush_completion;
   struct kref               hw_pa_ref;

#ifdef CONFIG_PROC_FS
   int filter_enabled;
   struct list_head          accel_filter; /* empty to accelerate all sessions (if filter_enabled == 1) */
   struct list_head          show_filter;  /* empty to show all sessions (default) */
#endif
} pa_glob = {
   .disabled = 1,
   .fw_disabled = 1,
   .dbgcapture = 0,
   .dbgsession = 0,
   .dbgnosession = 0,
   .dbgtrace = 0,
   .dbgmatch = 0,
   .dbgcputime = 0,
   .dbgprioack = 0,
   .dbgprioacktrace = 0,
   .dbgstats = 0,
   .bsession_allowed = 1,
   .tcp_timeout_secs = 10,
   .udp_timeout_secs = 10,
   .echo_timeout_secs = 3,
   .bridge_timeout_secs = 30,
   .load_control = LOADCONTROL_IDLE,
   .telephony_reduce = AVM_PA_DEFAULT_TELEPHONY_REDUCE,
   .irq_mswin_low = AVM_PA_CPUTIME_IRQ_MSWIN_LOW,
   .irq_mswin_high = AVM_PA_CPUTIME_IRQ_MSWIN_HIGH,
   .idle_mswin_low = AVM_PA_CPUTIME_IDLE_MSWIN_LOW,
   .idle_mswin_high = AVM_PA_CPUTIME_IDLE_MSWIN_HIGH,
   .maxrate = AVM_PA_DEFAULT_MAXRATE,
   .rate = AVM_PA_DEFAULT_MAXRATE,
   .pktbuffer = AVM_PA_DEFAULT_PKTBUFFER,
   .pktpeak = AVM_PA_DEFAULT_PKTPEAK,
   .est_idx = AVM_PA_EST_DEFAULT_IDX,
   .ewma_log = AVM_PA_EST_DEFAULT_EWMA_LOG,
   .cputime_est_idx = AVM_PA_CPUTIME_EST_DEFAULT_IDX,
   .cputime_ewma_log = AVM_PA_CPUTIME_EST_DEFAULT_EWMA_LOG,
   .prioack_thresh_packets = AVM_PA_PRIOACK_THRESH_PKTS,
   .prioack_ratio = AVM_PA_PRIOACK_RATIO,
   .filter_enabled = 1,
#ifdef CONFIG_AVM_PA_RPS
   .rps_enabled = 1,
#endif
};

struct avm_pa_data pa_data;

#define PA_PID(ctx, handle)      (&ctx->pid_array[(handle)%CONFIG_AVM_PA_MAX_PID])
#define PA_VPID(ctx, handle)     (&ctx->vpid_array[(handle)%CONFIG_AVM_PA_MAX_VPID])
#define PA_SESSION(pd, handle)   (&(pd)->sessions[(handle)%CONFIG_AVM_PA_MAX_SESSION])
#define PA_BSESSION(ctx, handle) (&ctx->bsess_array[(handle)%CONFIG_AVM_PA_MAX_SESSION])

/* ------------------------------------------------------------------------ */

static void pa_session_kill_nolock(struct avm_pa_session *session, const char *why);
static void pa_session_kill(struct avm_pa_session *session, const char *why);
static void pa_session_flush(struct avm_pa_session *session, const char *why);
static int pa_session_handle_stats(struct avm_pa_session *session);
static void pa_show_session(struct avm_pa_session *session,
                            pa_fprintf fprintffunc, void *arg);
static int avm_pa_pid_receive(avm_pid_handle pid_handle, PKT *pkt);
static void avm_pa_flush_sessions_with_destmac(struct avm_pa_macaddr *destmac);
static void avm_pa_flush_hw_sessions(void);

static inline int avm_pa_pid_tack_enabled(struct avm_pa_pid *pid)
{
   return pid->prio_maps[AVM_PA_PRIO_MAP_TACK].enabled;
}

static inline int avm_pa_pid_tget_enabled(struct avm_pa_pid *pid)
{
   return pid->prio_maps[AVM_PA_PRIO_MAP_TGET].enabled;
}

/*
 * Helper functions to retrieve a valid tack or tget priority from a pid's priority map.
 * Remember: prio_maps must include the correct TC_H_MAJ part.
 */
static inline unsigned int avm_pa_pid_tack_prio(struct avm_pa_pid *pid, unsigned int prio)
{
   if (likely((prio & TC_H_MIN_MASK) < AVM_PA_MAX_PRIOS))
      return pid->prio_maps[AVM_PA_PRIO_MAP_TACK].prios[prio & TC_H_MIN_MASK];
   return 0;
}

static inline unsigned int avm_pa_pid_tget_prio(struct avm_pa_pid *pid, unsigned int prio)
{
   if (likely((prio & TC_H_MIN_MASK) < AVM_PA_MAX_PRIOS))
      return pid->prio_maps[AVM_PA_PRIO_MAP_TGET].prios[prio & TC_H_MIN_MASK];
   return prio;
}

static inline void change_tack_prio(struct avm_pa_global *ctx, struct avm_pa_pid *pid,
                                    PKT *pkt, unsigned int org_prio)
{
   unsigned int tack_prio = avm_pa_pid_tack_prio(pid, org_prio);
   if (tack_prio != 0 && pkt->priority > tack_prio) {
      pkt->priority = tack_prio;
#if AVM_PA_TRACE
      if (ctx->dbgprioacktrace) {
          pa_printk(KERN_DEBUG, "avm_pa: %lu - change_tack_prio(%s), reset tack prio to 0x%x\n",
             pkt_uniq_id(pkt), pid->cfg.name, pkt->priority);
      }
#endif
   }
}

/* ------------------------------------------------------------------------ */

static inline int avm_pa_capture_running(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->dbgcapture)
      return 0;
   return atomic_read(&ctx->misc_is_open);
}

/* ------------------------------------------------------------------------ */
/* -------- utilities ----------------------------------------------------- */
/* ------------------------------------------------------------------------ */

static const char *rc2str(int rc)
{
   switch (rc) {
      case AVM_PA_RX_BROADCAST       : return "is broadcast";
      case AVM_PA_RX_TTL             : return "ttl/hoplimit <= 1";
      case AVM_PA_RX_FRAGMENT        : return "is fragment";
      case AVM_PA_RX_BYPASS          : return "bypass";
      case AVM_PA_RX_OK              : return "ok";
      case AVM_PA_RX_ACCELERATED     : return "accelerated";
      case AVM_PA_RX_ERROR_STATE     : return "state machine problem ?";
      case AVM_PA_RX_ERROR_LEN       : return "packet too short";
      case AVM_PA_RX_ERROR_IPVERSION : return "illegal ip version";
      case AVM_PA_RX_ERROR_MATCH     : return "too much header";
      case AVM_PA_RX_ERROR_HDR       : return "too much ip header";
   }
   return "???";
}

static const char *framing2str(enum avm_pa_framing framing)
{
   switch (framing) {
     case avm_pa_framing_ether: return "ether";
     case avm_pa_framing_ppp: return "ppp";
     case avm_pa_framing_ip: return "ip";
     case avm_pa_framing_ipdev: return "ipdev";
     case avm_pa_framing_dev: return "dev";
     case avm_pa_framing_ptype: return "local";
     case avm_pa_framing_llcsnap: return "llcsnap";
   }
   return "undef";
}

static int in6_addr2str(const void *cp, char *buf, size_t size)
{
    const struct in6_addr *s = (const struct in6_addr *)cp;
    return snprintf(buf, size, "%x:%x:%x:%x:%x:%x:%x:%x",
                    ntohs(s->s6_addr16[0]), ntohs(s->s6_addr16[1]),
                    ntohs(s->s6_addr16[2]), ntohs(s->s6_addr16[3]),
                    ntohs(s->s6_addr16[4]), ntohs(s->s6_addr16[5]),
                    ntohs(s->s6_addr16[6]), ntohs(s->s6_addr16[7]));
}

static int in_addr2str(const void *cp, char *buf, size_t size)
{
    const unsigned char *s = (const unsigned char *)cp;
    return snprintf(buf, size, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
}

static int mac2str(const void *cp, char *buf, size_t size)
{
    const unsigned char *mac = (const unsigned char *)cp;
    return snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char *pkttype2str(u16 pkttype, char *buf, size_t size)
{
   char *p = buf;
   char *end = p + size;

   if (pkttype == AVM_PA_PKTTYPE_NONE) {
      snprintf(p, end-p, "none");
      return buf;
   }

   switch (pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) {
      case AVM_PA_PKTTYPE_IPV6ENCAP:
         snprintf(p, end-p, "IPv6+");
         p += strlen(p);
         break;
      case AVM_PA_PKTTYPE_IPV4ENCAP:
         snprintf(p, end-p, "IPv4+");
         p += strlen(p);
         break;
   }
   if (pkttype & AVM_PA_PKTTYPE_LISP) {
      snprintf(p, end-p, "LISP+");
      p += strlen(p);
   }
   if (pkttype & AVM_PA_PKTTYPE_L2TP) {
      snprintf(p, end-p, "L2TPv3+");
      p += strlen(p);
   }
   if (pkttype & AVM_PA_PKTTYPE_GRE) {
      snprintf(p, end-p, "GRE+");
      p += strlen(p);
   }
   switch (pkttype & AVM_PA_PKTTYPE_IP_MASK) {
      case AVM_PA_PKTTYPE_IPV6:
         snprintf(p, end-p, "IPv6");
         p += strlen(p);
         break;
      case AVM_PA_PKTTYPE_IPV4:
         snprintf(p, end-p, "IPv4");
         p += strlen(p);
         break;
   }
   if (AVM_PA_PKTTYPE_IPPROTO(pkttype)) {
      switch (AVM_PA_PKTTYPE_IPPROTO(pkttype)) {
         case IPPROTO_UDP:
            snprintf(p, end-p, "+UDP");
            break;
         case IPPROTO_TCP:
            snprintf(p, end-p, "+TCP");
            break;
         case IPPROTO_ICMP:
            snprintf(p, end-p, "+ICMP");
            break;
         case IPPROTO_ICMPV6:
            snprintf(p, end-p, "+ICMPV6");
            break;
         case IPPROTO_L2TP:
            snprintf(p, end-p, "+L2TPv3");
            break;
         case IPPROTO_ESP:
            snprintf(p, end-p, "+ESP");
            break;
         default:
            snprintf(p, end-p, "+P%u", AVM_PA_PKTTYPE_IPPROTO(pkttype));
            break;
      }
   }
   return buf;
}

static char *data2hex(void *data, int datalen,
                      char *buf, int bufsiz)
{
   static char hexchars[] = "0123456789ABCDEF";
   unsigned char *databuf = (unsigned char *)data;
   char *s = buf;
   char *end = buf+bufsiz;
   int i;

   snprintf(s, end-s, "%d: ", datalen);
   s += strlen(s);

   for (i=0; i < datalen && s + 3 < end; i ++) {
      *s++ = hexchars[(databuf[i] >> 4) & 0xf];
      *s++ = hexchars[databuf[i] & 0xf];
   }
   *s = 0;
   return buf;
}

static char *pidflags2str(unsigned long flags, char *buf, int bufsiz)
{
   char *s = buf;
   char *end = s + bufsiz;
   buf[0] = 0;
   if (flags & AVM_PA_PID_FLAG_NO_PID_CHANGED_CHECK) {
      snprintf(s, end-s, "%sno_pid_changed_check", s == buf ? "" : ",");
      s += strlen(s);
   }
   if (flags & AVM_PA_PID_FLAG_HSTART_ON_INGRESS) {
      snprintf(s, end-s, "%shstart_on_ingress", s == buf ? "" : ",");
      s += strlen(s);
   }
   if (flags & AVM_PA_PID_FLAG_HSTART_ON_EGRESS) {
      snprintf(s, end-s, "%shstart_on_egress", s == buf ? "" : ",");
      s += strlen(s);
   }
   if (s == buf)
      snprintf(s, end-s, "none");
   return buf;
}

/* ------------------------------------------------------------------------ */
/* -------- l2tp session cache -------------------------------------------- */
/* ------------------------------------------------------------------------ */

static struct avm_pa_l2tp *
pa_l2tp_session_search(__be32 session_id)
{
#ifdef CONFIG_L2TP
   struct avm_pa_data *pd = &pa_data;
   int i;

   for (i = 0; i < ARRAY_SIZE(pd->l2tp_cache); i++) {
      if (pd->l2tp_cache[i].session_id == session_id)
         return &pd->l2tp_cache[i];
   }
#endif
   return NULL;
}

static struct avm_pa_l2tp *
pa_l2tp_session_search_by_peer(__be32 peer_session_id)
{
#ifdef CONFIG_L2TP
   struct avm_pa_data *pd = &pa_data;
   int i;

   for (i = 0; i < ARRAY_SIZE(pd->l2tp_cache); i++) {
      if (pd->l2tp_cache[i].peer_session_id == peer_session_id)
         return &pd->l2tp_cache[i];
   }
#endif
   return NULL;
}


#ifdef CONFIG_L2TP
static struct l2tp_session *
pa_l2tp_session_get_local(__be32 session_id)
{
   if (in_irq())
      return NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0) || defined(AVM_L2TP_BACKPORT_4_15)
   return l2tp_session_get(&init_net, NULL, ntohl(session_id));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
   return l2tp_session_get(&init_net, NULL, ntohl(session_id), true);
#else
   return l2tp_session_find(&init_net, NULL, ntohl(session_id));
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
#define pa_l2tp_session_put_local(s) l2tp_session_dec_refcount(s)
#else
/* no-op since this kernel uses l2tp_session_find() w/o refcounting */
#define pa_l2tp_session_put_local(s)
#endif
#endif

static struct avm_pa_l2tp *
pa_l2tp_session_alloc(__be32 session_id)
{
   struct avm_pa_l2tp *l2tp = NULL;

#ifdef CONFIG_L2TP
   struct avm_pa_data *pd = &pa_data;
   struct l2tp_session *local_sess;
   int i;

   local_sess = pa_l2tp_session_get_local(session_id);
   if (local_sess) {
      /* Add to the cache */
      spin_lock(&avm_pa_lock);
      for (i = 0; i < ARRAY_SIZE(pd->l2tp_cache); i++) {
         if (pd->l2tp_cache[i].session_id == 0) {
            l2tp = &pd->l2tp_cache[i];
            l2tp->session_id = session_id;
            /* store so that we can also look up by peer_session_id
             * for ingress packets */
            l2tp->peer_session_id = htonl(local_sess->peer_session_id);
            l2tp->hdr_len = local_sess->hdr_len;
            break;
         }
      }
      spin_unlock(&avm_pa_lock);
      pa_l2tp_session_put_local(local_sess);
   }
#endif

   return l2tp;
}

/* ------------------------------------------------------------------------ */
/* -------- parsing of packets -------------------------------------------- */
/* ------------------------------------------------------------------------ */

#define LISPDATAHDR(info) (HDRCOPY(info)+(info)->lisp_offset)

static inline void pa_reset_match(struct avm_pa_pkt_match *info)
{
   info->nmatch = 0;
   info->casttype = AVM_PA_IS_UNICAST;
   info->fragok = 0;
   info->fin = 0;
   info->syn = 0;
   info->ack_only = 0;
   info->pkttype = AVM_PA_PKTTYPE_NONE;
   info->pppoe_offset = AVM_PA_OFFSET_NOT_SET;
   info->encap_offset = AVM_PA_OFFSET_NOT_SET;
   info->lisp_offset = AVM_PA_OFFSET_NOT_SET;
   info->ip_offset = AVM_PA_OFFSET_NOT_SET;
   info->hdroff = 0;
   info->hdrlen = 0;
   info->pktlen = 0;
   info->vlan_tci = 0;
}


static inline struct avm_pa_match_info *
pa_find_eth_match(struct avm_pa_pkt_match *match)
{
   struct avm_pa_match_info *p, *end = &match->match[match->nmatch]; 

   for (p = &match->match[0]; p != end; p++) {
      if (p->type == AVM_PA_ETH) {
         return p;
      }
   }

   return NULL; /* no ETH found */
}


static inline void pa_change_to_bridge_match(struct avm_pa_pkt_match *match)
{
   struct avm_pa_match_info *p;

   p = pa_find_eth_match(match);
   if (p) {
      if ((p + 1)->type == AVM_PA_VLAN) ++p;
      match->nmatch = p - match->match + 1;
   }
}

static inline int pa_add_match(struct avm_pa_pkt_match *info,
                               unsigned char offset, unsigned char type)
{
   if (info->nmatch < AVM_PA_MAX_MATCH) {
      info->match[info->nmatch].offset = offset;
      info->match[info->nmatch].type = type;
      info->nmatch++;
      return 0;
   }
   return -1;
}

static int set_pkt_match(enum avm_pa_framing framing,
                         unsigned int hstart,
                         PKT *pkt,
                         struct avm_pa_pkt_match *info,
                         int on_egress)
{
#define RETURN(retval) do { ret = retval; goto out; } while (0)
   int ret = AVM_PA_RX_ERROR_LEN;
   int state = 0;
   u8 *data, *p, *end;
   u32 daddr;
   u16 uninitialized_var(ethproto); /* not used uninitialized */
   u16 uninitialized_var(ipproto); /* not used uninitialized */
   int uninitialized_var(ttl); /* not used uninitialized */
   int full_hdrlen = 0;

   data = PKT_DATA(pkt);
   end = data + PKT_LEN(pkt);
   data += hstart;

   switch (framing) {
      case avm_pa_framing_ip:
         if ((data[0] & 0xf0) == 0x40 && (data[0] & 0x0f) >= 5) {
            state = AVM_PA_IPV4;
            break;
         }
         if ((data[0] & 0xf0) == 0x60) {
            state = AVM_PA_IPV6;
            break;
         }
         return AVM_PA_RX_ERROR_IPVERSION;
      case avm_pa_framing_ppp:
         state = AVM_PA_PPP;
         break;
      case avm_pa_framing_ether:
         state = AVM_PA_ETH;
         break;
      case avm_pa_framing_dev:
         data = (u8 *)eth_hdr(pkt);
         state = AVM_PA_ETH;
         break;
      case avm_pa_framing_ipdev:
      case avm_pa_framing_ptype:
         data = (u8 *)skb_network_header(pkt);
         if (pkt->protocol == constant_htons(ETH_P_IP)) {
            state = AVM_PA_IPV4;
         } else if (pkt->protocol == constant_htons(ETH_P_IPV6)) {
            state = AVM_PA_IPV6;
         } else {
            return AVM_PA_RX_BYPASS;
         }
         break;
      case avm_pa_framing_llcsnap:
         state = AVM_PA_LLC_SNAP;
         break;

   }
   if (end - data > AVM_PA_MAX_HEADER - AVM_PA_MAX_HDROFF)
      end = data + AVM_PA_MAX_HEADER - AVM_PA_MAX_HDROFF;
   p = data;

   while (p < end) {
      hdrunion_t *hdr = (hdrunion_t *)p;
      int offset = p-data;

      switch (state) {
         case AVM_PA_ETH:
            if (pa_add_match(info, offset, AVM_PA_ETH) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct ethhdr);
            if (hdr->ethh.h_dest[0] & 1) {
               if (hdr->ethh.h_dest[0] == 0xff) {
                  info->casttype = AVM_PA_IS_BROADCAST;
                  RETURN(AVM_PA_RX_BYPASS);
               } else {
                  info->casttype = AVM_PA_IS_MULTICAST;
               }
            }
            if (skb_vlan_tag_present(pkt)) {
               info->vlan_tci = pkt->vlan_tci;
#ifdef AVM_PA_SKBUFF_HAS_VLAN_PROTO
               info->vlan_proto = pkt->vlan_proto;
#endif
               if (pa_add_match(info, AVM_PA_OFFSET_NOT_SET, AVM_PA_VLAN) < 0)
                  RETURN(AVM_PA_RX_ERROR_MATCH);
            }
            state = AVM_PA_ETH_PROTO;
            ethproto = hdr->ethh.h_proto;
            continue;

         case AVM_PA_VLAN: /* This handles only in-band vlan */
            if (pa_add_match(info, offset, AVM_PA_VLAN) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct vlanhdr);
            state = AVM_PA_ETH_PROTO;
            ethproto = hdr->vlanh.vlan_proto;
            continue;

         case AVM_PA_ETH_PROTO:
            switch (ethproto) {
               case constant_htons(ETH_P_PPP_SESS):
                  state = AVM_PA_PPPOE;
                  continue;
               case constant_htons(ETH_P_IP):
                  state = AVM_PA_IPV4;
                  continue;
               case constant_htons(ETH_P_IPV6):
                  state = AVM_PA_IPV6;
                  continue;
               case constant_htons(ETH_P_8021Q):
               case constant_htons(ETH_P_8021AD):
                  state = AVM_PA_VLAN;
                  continue;
            }
            RETURN(AVM_PA_RX_BYPASS);

         case AVM_PA_PPPOE:
            if (pa_add_match(info, offset, AVM_PA_PPPOE) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct pppoehdr);
            info->pppoe_offset = offset;
            state = AVM_PA_PPP;
            continue;

         case AVM_PA_PPP:
            if (p[0] == 0) {
               p++;
               offset++;
            }
            if (p[0] == 0x21) {
               if (pa_add_match(info, offset, AVM_PA_PPP) < 0)
                  RETURN(AVM_PA_RX_ERROR_MATCH);
               p++;
               state = AVM_PA_IPV4;
               continue;
            }
            if (p[0] == 0x57) {
               if (pa_add_match(info, offset, AVM_PA_PPP) < 0)
                  RETURN(AVM_PA_RX_ERROR_MATCH);
               p++;
               state = AVM_PA_IPV6;
               continue;
            }
            RETURN(AVM_PA_RX_BYPASS);

         case AVM_PA_IPV4:
            if (hdr->iph.version != 4)
               RETURN(AVM_PA_RX_ERROR_IPVERSION);
            if (pa_add_match(info, offset, AVM_PA_IPV4) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            ttl = hdr->iph.ttl;
            p += PA_IPHLEN(&hdr->iph);
            if (hdr->iph.frag_off & constant_htons(IP_OFFSET))
               RETURN(AVM_PA_RX_FRAGMENT);
            /* We don't support forwarding fragments, we may only create them for
             * tunnels, so check if we're on egress.
             */
            if ((hdr->iph.frag_off & constant_htons(IP_MF)) && !on_egress)
               RETURN(AVM_PA_RX_FRAGMENT);
            daddr = get_unaligned(&hdr->iph.daddr);
            if (ipv4_is_lbcast(daddr)) {
               info->casttype = AVM_PA_IS_BROADCAST;
               RETURN(AVM_PA_RX_BYPASS);
            } else if (ipv4_is_multicast(daddr))  {
               info->casttype = AVM_PA_IS_MULTICAST;
            }
            if ((hdr->iph.frag_off & constant_htons(IP_DF)) == 0)
               info->fragok = 1;
            if (hdr->iph.protocol == IPPROTO_IPV6) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE)
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV4ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV6;
               continue;
            }
            if (hdr->iph.protocol == IPPROTO_IPENCAP) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE)
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV4ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV4;
               continue;
            }
            info->pkttype |= AVM_PA_PKTTYPE_IPV4;
            info->ip_offset = offset;
            state = AVM_PA_IP_PROTO;
            ipproto = hdr->iph.protocol;
            if ((offset & 0x3) && info->hdroff == 0)
               info->hdroff = 4 - (offset & 0x3);
            continue;

         case AVM_PA_IPV6:
            if (hdr->ipv6h.version != 6)
               RETURN(AVM_PA_RX_ERROR_IPVERSION);
            if (pa_add_match(info, offset, AVM_PA_IPV6) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            ttl = hdr->ipv6h.hop_limit;
            p += sizeof(struct ipv6hdr);
            if (hdr->ipv6h.daddr.s6_addr[0] == 0xff)
               info->casttype = AVM_PA_IS_MULTICAST;
            if (hdr->ipv6h.nexthdr == IPPROTO_IPV6) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE)
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV6ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV6;
               continue;
            }
            if (hdr->ipv6h.nexthdr == IPPROTO_IPENCAP) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE)
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV6ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV4;
               continue;
            }
            if (hdr->ipv6h.nexthdr == IPPROTO_FRAGMENT) {
               struct ipv6fraghdr *fragh = (struct ipv6fraghdr *)p;
               info->pkttype |= AVM_PA_PKTTYPE_IPV6;
               info->ip_offset = offset;
               if (fragh->frag_off & constant_htons(IP6_OFFSET))
                  RETURN(AVM_PA_RX_FRAGMENT);
               /* We don't support forwarding fragments, we may only create them for
                * tunnels, so check if we're on egress.
                */
               if ((fragh->frag_off & constant_htons(IP6_MF)) && !on_egress)
                  RETURN(AVM_PA_RX_FRAGMENT);
               p += sizeof(struct ipv6fraghdr);
               state = AVM_PA_IP_PROTO;
               ipproto = fragh->nexthdr;
               continue;
            }
            info->pkttype |= AVM_PA_PKTTYPE_IPV6;
            info->ip_offset = offset;
            state = AVM_PA_IP_PROTO;
            ipproto = hdr->ipv6h.nexthdr;
            if ((offset & 0x3) && info->hdroff == 0)
               info->hdroff = 4 - (offset & 0x3);
            continue;

         case AVM_PA_IP_PROTO:
            switch (ipproto) {
               case IPPROTO_TCP:
                  info->pkttype |= ipproto;
                  if (p + sizeof(struct tcphdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (pa_add_match(info, offset, AVM_PA_PORTS) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  if (PA_TCP_FIN_OR_RST(&hdr->tcph))
                     info->fin = 1;
                  if (PA_TCP_SYN(&hdr->tcph))
                     info->syn = 1;
                  if (PA_TCP_ACK(&hdr->tcph)) {
                     if (AVM_PA_PKTTYPE_IP_VERSION(info->pkttype) == 4) {
                        hdrunion_t *iphdr = (hdrunion_t *)(data+info->ip_offset);
                        if (ntohs(PA_IPTOTLEN(&iphdr->iph)) == (PA_IPHLEN(&iphdr->iph)+PA_TCP_DOFF(&hdr->tcph)))
                           info->ack_only = 1;
                     } else if (AVM_PA_PKTTYPE_IP_VERSION(info->pkttype) == 6) {
                        hdrunion_t *ip6hdr = (hdrunion_t *)(data+info->ip_offset);
                        if (ntohs(PA_IP6_PAYLOADLEN(&ip6hdr->iph)) == PA_TCP_DOFF(&hdr->tcph))
                           info->ack_only = 1;
                     }
                  }
                  full_hdrlen = (p - data) + PA_TCP_DOFF(&hdr->tcph);
                  /* Only ports are stored */
                  p += 2 * sizeof(__be16);
                  RETURN(AVM_PA_RX_OK);
               case IPPROTO_UDP:
                  if (p + sizeof(struct udphdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (pa_add_match(info, offset, AVM_PA_PORTS) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  if (hdr->udph.dest == constant_htons(4341)) {
                     p += sizeof(struct udphdr);
                     state = AVM_PA_LISP;
                     continue;
                  }
                  info->pkttype |= ipproto;
                  full_hdrlen = (p - data) + sizeof(struct udphdr);
                  /* Only ports are stored */
                  p += 2 * sizeof(__be16);
                  RETURN(AVM_PA_RX_OK);
               case IPPROTO_ICMP:
                  info->pkttype |= ipproto;
                  if (p + sizeof(struct icmphdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (   hdr->icmph.type == ICMP_ECHO
                      || hdr->icmph.type == ICMP_ECHOREPLY) {
                     if (pa_add_match(info, offset, AVM_PA_ICMPV4) < 0)
                        RETURN(AVM_PA_RX_ERROR_MATCH);
                     p += sizeof(struct icmphdr);
                     RETURN(AVM_PA_RX_OK);
                  }
                  break;
               case IPPROTO_ICMPV6:
                  info->pkttype |= ipproto;
                  if (p + sizeof(struct icmp6hdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (   hdr->icmpv6h.icmp6_type == ICMPV6_ECHO_REQUEST
                      || hdr->icmpv6h.icmp6_type == ICMPV6_ECHO_REPLY) {
                     if (pa_add_match(info, offset, AVM_PA_ICMPV6) < 0)
                        RETURN(AVM_PA_RX_ERROR_MATCH);
                     p += sizeof(struct icmp6hdr);
                     RETURN(AVM_PA_RX_OK);
                  }
                  break;
               case IPPROTO_L2TP:
                  if (AVM_PA_PKTTYPE_IPENCAP_VERSION(info->pkttype))
                     RETURN(AVM_PA_RX_OK);
                  info->encap_offset = info->ip_offset;
                  if (pa_add_match(info, offset, AVM_PA_L2TP) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  {
                     struct avm_pa_l2tp *l2tp = NULL;
                     __be32 be_session_id = hdr->l2tp.session_id;
                     /* check the system has configured sessions...
                      * yes: we check and use the sessions offset (start of eth header)
                      * no: we terminate classification, probably l2tp pass through.
                      */
                     if (be_session_id != 0) {
                        if (on_egress) {
                           l2tp = pa_l2tp_session_search_by_peer(be_session_id);
                        }
                        else {
                           l2tp = pa_l2tp_session_search(be_session_id);
                           if (!l2tp)
                              l2tp = pa_l2tp_session_alloc(be_session_id); /* fails inside irq */
                        }
                     }
                     if (l2tp) {
                        if (p + l2tp->hdr_len > end)
                           RETURN(AVM_PA_RX_ERROR_LEN);
                        p += l2tp->hdr_len;
                        info->pkttype = AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(info->pkttype);
                        info->pkttype |= AVM_PA_PKTTYPE_L2TP;
                        state = AVM_PA_ETH;
                     } else {
                        /* We are in irq context or cache is filled,
                         * or there is no local l2tp session, i.e. pass through.
                         *
                         * We cannot know for sure as long as we might be in
                         * irq context, but we assume pass through and figure
                         * out later whether to add a session.
                         *
                         * Control connections are treated as pass through here
                         * but effectively they won't be accelerated because precheck
                         * on egress always fails (if they terminate locally).
                         */
                        AVM_PKT_INFO(pkt)->l2tp_session_id = be_session_id;
                        info->pkttype |= ipproto;
                        if (p + sizeof(__be32) > end)
                           RETURN(AVM_PA_RX_ERROR_LEN);
                        p += sizeof(__be32);
                        RETURN(AVM_PA_RX_OK);
                     }
                  }
                  continue;
               case IPPROTO_GRE:
                  if (AVM_PA_PKTTYPE_IPENCAP_VERSION(info->pkttype))
                     RETURN(AVM_PA_RX_OK);
                  if (p + sizeof(struct tlb_grehdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  info->encap_offset = info->ip_offset;
                  p += sizeof(struct tlb_grehdr);
                  if (pa_add_match(info, offset, AVM_PA_GRE) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  info->pkttype = AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(info->pkttype);
                  info->pkttype |= AVM_PA_PKTTYPE_GRE;
                  switch (hdr->greh.protocol) {
                     case constant_htons(ETH_P_IP):
                        state = AVM_PA_IPV4;
                        continue;
                     case constant_htons(ETH_P_TEB):
                        state = AVM_PA_ETH;
                        continue;
                  }
                  break;
               case IPPROTO_ESP:
                  if (p + sizeof(struct ip_esp_hdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  p += sizeof(struct ip_esp_hdr);
                  if (pa_add_match(info, offset, AVM_PA_ESP) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  /* Only supporting pass-through... */
                  info->pkttype |= ipproto;
                  /* Encrypted payload follows, terminate parsing. */
                  RETURN(AVM_PA_RX_OK);
            }
            RETURN(AVM_PA_RX_BYPASS);

         case AVM_PA_LLC_SNAP:
            if (   hdr->llcsnap.dsap  != 0xAA
                || hdr->llcsnap.ssap  != 0xAA
                || hdr->llcsnap.ui    != 0x03)
               /* not checking:
                * RFC1042_SNAP 0x00,0x00,0x00
                * BTEP_SNAP    0x00,0x00,0xf8
                */
               RETURN(AVM_PA_RX_BYPASS);

            if (pa_add_match(info, offset, AVM_PA_LLC_SNAP) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct llc_snap_hdr);
            state = AVM_PA_ETH_PROTO;
            ethproto = get_unaligned(&hdr->llcsnap.type);
            continue;

         case AVM_PA_LISP:
           if (AVM_PA_PKTTYPE_IPENCAP_VERSION(info->pkttype))
              RETURN(AVM_PA_RX_OK);
            if (p + LISP_DATAHDR_SIZE > end)
               RETURN(AVM_PA_RX_ERROR_LEN);
            info->encap_offset = info->ip_offset;
            info->lisp_offset = offset;
            p += LISP_DATAHDR_SIZE;
            hdr = (hdrunion_t *)p;
            if (hdr->iph.version == 4)
               state = AVM_PA_IPV4;
            else if (hdr->iph.version == 6)
               state = AVM_PA_IPV6;
            else
               RETURN(AVM_PA_RX_OK); /* not a lisp packet */
            if (pa_add_match(info, offset, AVM_PA_LISP) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            info->pkttype = AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(info->pkttype);
            info->pkttype |= AVM_PA_PKTTYPE_LISP;
            continue;

         default:
            RETURN(AVM_PA_RX_ERROR_STATE);
      }
   }
out:
   if (ret == AVM_PA_RX_OK && ttl == 0)
      ret = AVM_PA_RX_TTL;
   if (ret == AVM_PA_RX_OK && (p - data) > AVM_PA_MAX_HEADER)
      ret = AVM_PA_RX_ERROR_LEN;
   if (ret == AVM_PA_RX_OK || pa_glob.dbgmatch) {
      info->hdrlen = p - data;
      memcpy(HDRCOPY(info), data, info->hdrlen);
   }
   if (ret == AVM_PA_RX_OK) {
      info->pktlen = PKT_LEN(pkt);
      info->full_hdrlen = full_hdrlen ? full_hdrlen : info->hdrlen;
   }
   return ret;
#undef RETURN
}

static inline void
pa_match_postprocess(struct avm_pa_pkt_match *info)
{
   int i;
   info->hash = 0;

   for (i = 0 ; i < info->nmatch; i++) {
      struct avm_pa_match_info *p = &info->match[i];
      hdrunion_t *hdr = (hdrunion_t *)(HDRCOPY(info)+p->offset);
      switch (p->type) {
         case AVM_PA_IPV4:
#if AVM_PA_UNALIGNED_CHECK
            if (((unsigned long)&hdr->iph.saddr) & 0x3)
               if (net_ratelimit())
                  pr_info("avm_pa: unaligned access %p (ipv4)\n",
                          &hdr->iph.saddr);
#endif
            info->hash ^= hdr->iph.saddr;
            info->hash ^= hdr->iph.daddr;
            info->hash ^= hdr->iph.protocol;
            info->hash ^= hdr->iph.tos;
            /*
             * JZ-36233: Gastzugang auf dem Repeater
             *
             * A session may be created by a packet with IP_MF set. If this
             * header is going to be pushed as-is on egress (e.g. in case of L2TP
             * encap) all packets would have IP_MF set, so we need to reset frag_off.
             *
             * TODO: What about IPv6?
             */
            hdr->iph.frag_off = 0;
            break;
         case AVM_PA_IPV6:
#if AVM_PA_UNALIGNED_CHECK
            if (((unsigned long)&hdr->ipv6h.saddr.s6_addr32[2]) & 0x3)
               if (net_ratelimit())
                  pr_info("avm_pa: unaligned access %p (ipv6)\n",
                          &hdr->ipv6h.saddr.s6_addr32[2]);
#endif
            //info->hash ^= hdr->ipv6h.saddr.s6_addr32[0];
            //info->hash ^= hdr->ipv6h.saddr.s6_addr32[1];
            info->hash ^= hdr->ipv6h.saddr.s6_addr32[2];
            info->hash ^= hdr->ipv6h.saddr.s6_addr32[3];
            //info->hash ^= hdr->ipv6h.daddr.s6_addr32[0];
            //info->hash ^= hdr->ipv6h.daddr.s6_addr32[1];
            info->hash ^= hdr->ipv6h.daddr.s6_addr32[2];
            info->hash ^= hdr->ipv6h.daddr.s6_addr32[3];
            info->hash ^= hdr->ipv6h.nexthdr;
            break;
         case AVM_PA_PORTS:
            /* At least Linux seems to prefer even ports when selecting source ports,
             * for RPS we want the lowest bits of the hash to be most significant */
            info->hash ^= ror16(hdr->ports[0], 1);
            info->hash ^= ror16(hdr->ports[1], 1);
            break;
         case AVM_PA_ICMPV4:
         case AVM_PA_ICMPV6:
            info->hash ^= hdr->ports[0]; /* type + code */
            info->hash ^= hdr->ports[2]; /* id */
            break;
         case AVM_PA_ESP:
            info->hash ^= hdr->esph.spi;
            break;
      }
   }
   info->hash = (info->hash >> 16) ^ (info->hash & 0xffff);
   info->hash = (info->hash >> 8) ^ (info->hash & 0xff);
   info->hash %= CONFIG_AVM_PA_MAX_SESSION;
}

static int pa_set_pkt_match(enum avm_pa_framing framing,
                            unsigned int hstart,
                            PKT *pkt,
                            struct avm_pa_pkt_match *match,
                            int on_egress)
{
   int rc;
   pa_reset_match(match);
   rc = set_pkt_match(framing, hstart, pkt, match, on_egress);
   if (rc == AVM_PA_RX_OK)
      pa_match_postprocess(match);
   return rc;
}

/* Compare two packet matches. A slice can be selected by skipping
 * the first few match info items, for example to only compare the
 * L3 part of the packet match. */
static inline int
pa_match_cmp(struct avm_pa_pkt_match *a1, int a1_skip,
             struct avm_pa_pkt_match *a2, int a2_skip)
{
   struct avm_pa_match_info *p;
   hdrunion_t *h1, *h2;
   int rc;
   int i;
   int a1_nmatch = a1->nmatch - a1_skip;
   int a2_nmatch = a2->nmatch - a2_skip;

   /* The match item count must be equal. */
   if ((rc = a1_nmatch - a2_nmatch))
      goto out;

   /* The match slice itself must be equal. */
   if ((rc = memcmp(&a1->match[a1_skip], &a2->match[a2_skip],
                    a1_nmatch*sizeof(struct avm_pa_match_info))))
      goto out;

   /* From here now we determined that the slice is the same, therefore we only
    * use match items from a1 going forward, to test how the relevant fields in
    * the hdrcopy compare. */
   for (i = a1->nmatch-1; i >= a1_skip; i--) {
      p = &a1->match[i];
      /* h1 and h2 must NOT be used if p->offset is AVM_PA_OFFSET_NOT_SET.
       * At this time, AVM_PA_OFFSET_NOT_SET is only possible for type == AVM_PA_VLAN */
      h1 = (hdrunion_t *)(HDRCOPY(a1)+p->offset);
      h2 = (hdrunion_t *)(HDRCOPY(a2)+p->offset);
      switch (p->type) {
         case AVM_PA_ETH:
            rc = memcmp(&h1->ethh, &h2->ethh, sizeof(struct ethhdr));
            if (rc) goto out;
            break;
         case AVM_PA_VLAN:
            if (p->offset == AVM_PA_OFFSET_NOT_SET)
               rc = (a1->vlan_tci ^ a2->vlan_tci) & VLAN_VID_MASK;
            else
               rc = (int)VLAN_ID(&h1->vlanh) - (int)VLAN_ID(&h2->vlanh);
            if (rc) goto out;
            break;
         case AVM_PA_PPPOE:
            rc = (int)h1->pppoeh.sid - (int)h2->pppoeh.sid;
            if (rc) goto out;
            break;
         case AVM_PA_PPP:
            rc = (int)h1->ppph[0] - (int)h2->ppph[0];
            if (rc) goto out;
            break;
         case AVM_PA_IPV4:
            rc = (int)h1->iph.protocol - (int)h2->iph.protocol;
            if (rc) goto out;
            rc = (int)h1->iph.tos - (int)h2->iph.tos;
            if (rc) goto out;
            /* JZ-47728: Windows ICMP has always the same id so ttl
             * is the only difference between ping and tracert packets
             * Also, ttl == 1 must not match existing sessions with higher ttl.
             */
            rc = (int)h1->iph.ttl - (int)h2->iph.ttl;
            if (rc) goto out;
            rc = (int)h1->iph.daddr - (int)h2->iph.daddr;
            if (rc) goto out;
            rc = (int)h1->iph.saddr - (int)h2->iph.saddr;
            if (rc) goto out;
            break;
         case AVM_PA_IPV6:
            rc = (int)h1->ipv6h.nexthdr - (int)h2->ipv6h.nexthdr;
            if (rc) goto out;
            rc = (int)h1->ipv6h.hop_limit - (int)h2->ipv6h.hop_limit;
            if (rc) goto out;
            /* compare both src and dst in a single call */
            rc = memcmp(&h1->ipv6h.saddr, &h2->ipv6h.saddr,
                        sizeof(struct in6_addr) * 2);
            if (rc) goto out;
            break;
         case AVM_PA_PORTS:
            rc = (int)h1->ports[0] - (int)h2->ports[0]; /* source */
            if (rc) goto out;
            rc = (int)h1->ports[1] - (int)h2->ports[1]; /* dest */
            if (rc) goto out;
            break;
         case AVM_PA_ICMPV4:
         case AVM_PA_ICMPV6:
            rc = (int)h1->ports[0] - (int)h2->ports[0]; /* type + code */
            if (rc) goto out;
            rc = (int)h1->ports[2] - (int)h2->ports[2]; /* id */
            if (rc) goto out;
            break;
         case AVM_PA_LLC_SNAP:
            rc = (int)h1->llcsnap.type - (int)h2->llcsnap.type;
            if (rc) goto out;
            break;
         case AVM_PA_L2TP:
            rc = (int)h1->l2tp.session_id - (int)h2->l2tp.session_id;
            if (rc) goto out;
            break;
         case AVM_PA_GRE:
            rc = (int)h1->greh.protocol - (int)h2->greh.protocol;
            if (rc) goto out;
            break;
         case AVM_PA_ESP:
            rc = (int)h1->esph.spi - (int)h2->esph.spi;
            if (rc) goto out;
            break;
      }
   }
out:
   return rc;
}

static inline int pa_match_eq(struct avm_pa_pkt_match *a1,
                              struct avm_pa_pkt_match *a2)
{
   return pa_match_cmp(a1, 0, a2, 0) == 0;
}


/* Returns 1 if two matches are compatible for bridging.
 *
 * This is basically the same as pa_match_eq, except vlan is not considered, since
 * a bsession can cross VLANs (provided that no modifications need to be done
 * to the packet data and that the system's bridge setup allows that). */
static inline int
pa_match_bridged(struct avm_pa_pkt_match *a1, struct avm_pa_pkt_match *a2)
{
   struct avm_pa_match_info *p1, *p2;
   hdrunion_t *h1, *h2;

   if (!(p1 = pa_find_eth_match(a1)))
      return 0;
   if (!(p2 = pa_find_eth_match(a2)))
      return 0;

   h1 = (hdrunion_t *)(HDRCOPY(a1)+p1->offset);
   h2 = (hdrunion_t *)(HDRCOPY(a2)+p2->offset);

   /* MAC addresses must be equal. */
   if (memcmp(&h1->ethh, &h2->ethh, ETH_ALEN * 2))
      return 0;

   /* Different VLANs is OK, even the VID may differ. So just skip the VLAN match */
   /* JZ-63724: ...but only if there's no in-band VLAN header stored in skb->data */
   if ((++p1)->type == AVM_PA_VLAN && p1->offset == AVM_PA_OFFSET_NOT_SET) ++p1;
   if ((++p2)->type == AVM_PA_VLAN && p2->offset == AVM_PA_OFFSET_NOT_SET) ++p2;

   /* Compare the remainder for equality which ensures that modifications
    * to the packet data are not permitted. */
   return pa_match_cmp(a1, p1 - a1->match, a2, p2 - a2->match) == 0;
}

static void pa_show_vlan_match(struct avm_pa_pkt_match *match,
                               struct avm_pa_match_info *info,
                               pa_fprintf fprintffunc, void *arg)
{
   hdrunion_t *hdr = NULL;

   if (!info) {
      info = pa_find_eth_match(match);
      if (!info || (++info)->type != AVM_PA_VLAN)
         return;
   }

   /* At this time, AVM_PA_OFFSET_NOT_SET is only possible for type == AVM_PA_VLAN.
    * Do not use hdr in this case (it's NULL)! */
   if (info->offset != AVM_PA_OFFSET_NOT_SET)
      hdr = (hdrunion_t *) (HDRCOPY(match) + info->offset);
   if (hdr) {
      (*fprintffunc)(arg, "Vlan ID        : %d\n", VLAN_ID(&hdr->vlanh));
      (*fprintffunc)(arg, "Vlan prio      : %u\n", VLAN_PRIO(&hdr->vlanh));
      (*fprintffunc)(arg, "Vlan proto     : 0x%04x\n", ntohs(hdr->vlanh.vlan_proto));
   } else {
      struct vlanhdr v = { .vlan_tci = htons(match->vlan_tci) };
      (*fprintffunc)(arg, "Vlan* ID       : %d\n", VLAN_ID(&v));
      (*fprintffunc)(arg, "Vlan* prio     : %u\n", VLAN_PRIO(&v));
      (*fprintffunc)(arg, "Vlan* proto    : 0x%04x\n", ntohs(PA_VLAN_PROTO(match)));
   }
}


static void pa_show_pkt_match(struct avm_pa_pkt_match *match,
                              int is_bridged, u16 egress_pkttype,
                              pa_fprintf fprintffunc, void *arg)

{
   char buf[128];
   const char *prompt = "PktType";
   unsigned n;
   int s;

   if (is_bridged) {
      pkttype2str(match->pkttype & AVM_PA_PKTTYPE_IP_MASK, buf, sizeof(buf));
      (*fprintffunc)(arg, "%-15s: %s\n", prompt, buf);
   } else {
      if (egress_pkttype && egress_pkttype != match->pkttype) {
         size_t half = sizeof(buf)/2;
         pkttype2str(match->pkttype, buf, half);
         pkttype2str(egress_pkttype, buf+half, half);
         (*fprintffunc)(arg, "%-15s: %s -> %s\n", prompt, buf, buf+half);
      } else {
         pkttype2str(match->pkttype, buf, sizeof(buf));
         (*fprintffunc)(arg, "%-15s: %s\n", prompt, buf);
      }
   }

   if (match->nmatch && !is_bridged) {
      (*fprintffunc)(arg, "FragOk         : %u\n", match->fragok);
      (*fprintffunc)(arg, "Syn, Fin       : %u, %u\n", match->syn, match->fin);
      (*fprintffunc)(arg, "Ack w/o data   : %u\n", match->ack_only);
   }

   for (n=0; n < match->nmatch; n++) {
      struct avm_pa_match_info *p = match->match+n;
      hdrunion_t *hdr = (hdrunion_t *) (HDRCOPY(match) + p->offset);
      switch (p->type) {
         case AVM_PA_ETH:
            s = mac2str(&hdr->ethh.h_dest, buf, sizeof(buf));
            buf[s++] = ' ';
            mac2str(&hdr->ethh.h_source, &buf[s], sizeof(buf) - s);
            (*fprintffunc)(arg, "Eth Hdr        : %s proto %04X\n", buf,
                           ntohs(hdr->ethh.h_proto));
            break;
         case AVM_PA_VLAN:
            /* VLAN match can come from the payload or skb->vlan_tci */
            pa_show_vlan_match(match, p, fprintffunc, arg);
            break;
         case AVM_PA_PPPOE:
            (*fprintffunc)(arg, "PPPoE Sid      : %04X\n", ntohs(hdr->pppoeh.sid));
            break;
         case AVM_PA_PPP:
            (*fprintffunc)(arg, "PPP Proto      : %02X\n", hdr->ppph[0]);
            break;
         case AVM_PA_IPV4:
            s = in_addr2str(&hdr->iph.saddr, buf, sizeof(buf));
            buf[s++] = ' ';
            in_addr2str(&hdr->iph.daddr, &buf[s], sizeof(buf) - s);
            (*fprintffunc)(arg, "IPv4 Hdr       : %s proto %d tos %02X\n", buf, hdr->iph.protocol, hdr->iph.tos);
            break;
         case AVM_PA_IPV6:
            s = in6_addr2str(&hdr->ipv6h.saddr, buf, sizeof(buf));
            buf[s++] = ' ';
            in6_addr2str(&hdr->ipv6h.daddr, &buf[s], sizeof(buf) - s);
            (*fprintffunc)(arg, "IPv6 Hdr       : %s proto %d\n", buf, hdr->ipv6h.nexthdr);
            break;
         case AVM_PA_PORTS:
            (*fprintffunc)(arg, "Ports          : %d -> %d\n",
                     ntohs(hdr->ports[0]), ntohs(hdr->ports[1]));
            break;
         case AVM_PA_ICMPV4:
            prompt = "ICMPv4";
            switch (hdr->icmph.type) {
               case ICMP_ECHOREPLY:
                  (*fprintffunc)(arg, "%-15s: echo reply id=%hu\n",  prompt,
                           hdr->icmph.un.echo.id);
                  break;
               case ICMP_ECHO:
                  (*fprintffunc)(arg, "%-15s: echo request id=%hu\n",  prompt,
                           hdr->icmph.un.echo.id);
                  break;
               default:
                  (*fprintffunc)(arg, "??????\n");
                  break;
            }
            break;
         case AVM_PA_ICMPV6:
            prompt = "ICMPv6";
            switch (hdr->icmpv6h.icmp6_type) {
               case ICMPV6_ECHO_REQUEST:
                  (*fprintffunc)(arg, "%-15s: echo request id=%hu\n", prompt,
                           hdr->icmpv6h.icmp6_identifier);
                  break;
               case ICMPV6_ECHO_REPLY:
                  (*fprintffunc)(arg, "%-15s: echo reply id=%hu\n", prompt,
                           hdr->icmpv6h.icmp6_identifier);
                  break;
               default:
                  (*fprintffunc)(arg, "??????\n");
                  break;
            }
            break;
         case AVM_PA_LLC_SNAP:
            (*fprintffunc)(arg, "LLC SNAP       : %04X\n", ntohs(hdr->llcsnap.type));
            break;
         case AVM_PA_LISP:
            (*fprintffunc)(arg, "LISP           : data header\n");
            break;
         case AVM_PA_L2TP:
            (*fprintffunc)(arg, "L2TP Sess      : %lu\n", (unsigned long)ntohl(hdr->l2tp.session_id));
            break;
         case AVM_PA_GRE:
            (*fprintffunc)(arg, "GRE Proto      : %04X\n", ntohs(hdr->greh.protocol));
            break;
         case AVM_PA_ESP:
            (*fprintffunc)(arg, "ESP SPI        : 0x%08X\n", ntohl(hdr->esph.spi));
            break;
      }
   }
}

static void pa_show_pkt_info(struct avm_pa_pkt_info *info,
                             pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;

   (*fprintffunc)(arg, "In Pid         : %d (%s)\n",
                  info->ingress_pid_handle,
                  PA_PID(ctx, info->ingress_pid_handle)->cfg.name);

   if (info->ingress_vpid_handle) {
      (*fprintffunc)(arg, "In VPid        : %d (%s)\n",
                     info->ingress_vpid_handle,
                     PA_VPID(ctx, info->ingress_vpid_handle)->cfg.name);
   }
   if (info->egress_vpid_handle) {
      (*fprintffunc)(arg, "Out VPid       : %d (%s)\n",
                     info->egress_vpid_handle,
                     PA_VPID(ctx, info->egress_vpid_handle)->cfg.name);
   }

   if (info->routed)
      (*fprintffunc)(arg, "Routed         : yes\n");

   if (info->shaped)
      (*fprintffunc)(arg, "Shaped         : yes\n");

   pa_show_pkt_match(&info->match, 0, 0, fprintffunc, arg);
}

/* ------------------------------------------------------------------------ */
/* -------- mod rec ------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/*
 * From RFC 1624 Incremental Internet Checksum
 *
 * HC  - old checksum in header
 * HC' - new checksum in header
 * m   - old value of a 16-bit field
 * m'  - new value of a 16-bit field
 * HC' = ~(~HC + ~m + m') --    [Eqn. 3]
 * HC' = HC - ~m - m'     --    [Eqn. 4]
 *
 *
 * csum_unfold(): be16 -> u32
 *
 * M   = ~m + m';
 *
 * we use Eqn.3, because we precalculate M.
 * csum_fold(): add the carries
 *
 * HC' = ~csum_fold((~csum_unfold(HC) + ~m + m'));
 *
 * HC' = ~csum_fold(csum_add(~csum_unfold(HC), M);
 *
 */

static inline u32 hcsum_add(u32 sum, u32 addend)
{
   sum += addend;
   if (sum < addend) sum++; /* skip -0 */
   return sum; // + (sum < addend);
}

static inline u32 hcsum_prepare(u16 sum)
{
   return (u16)(~sum);
}

static inline u32 hcsum_u32(u32 sum, u32 from, u32 to)
{
   sum = hcsum_add(sum, ~from);
   sum = hcsum_add(sum, to);
   return sum;
}

static inline u32 hcsum_u16(u32 sum, u16 from, u16 to)
{
   sum = hcsum_u32(sum, from, to);
   return sum;
}

static inline u16 hcsum_fold(u32 sum)
{
   while (sum >> 16)
      sum = (sum & 0xffff) + (sum >> 16);
   return sum;
}

static inline u16 hcsum_finish(u32 sum)
{
   return ~hcsum_fold(sum);
}

static int pa_set_v4_mod_rec(struct avm_pa_v4_mod_rec *mod,
                             int update_ttl, u8 *in, u8 *out)
{
   struct iphdr *iiph = (struct iphdr *)in;
   struct iphdr *oiph = (struct iphdr *)out;
   u32 l3_check = 0;
   u32 l4_check;
   int isicmp = 0;

   mod->flags = 0;

   mod->saddr = oiph->saddr;
   if (iiph->saddr != oiph->saddr) {
      mod->flags |= AVM_PA_V4_MOD_SADDR|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u32(l3_check, iiph->saddr, oiph->saddr);
   }

   mod->daddr = oiph->daddr;
   if (iiph->daddr != oiph->daddr) {
      mod->flags |= AVM_PA_V4_MOD_DADDR|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u32(l3_check, iiph->daddr, oiph->daddr);
   }

   l4_check = l3_check;

   mod->tos = oiph->tos;
   if (iiph->tos != oiph->tos) {
      mod->flags |= AVM_PA_V4_MOD_TOS|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u16(l3_check, htons(iiph->tos), htons(oiph->tos));
   }

   if (update_ttl) {
      mod->flags |= AVM_PA_V4_MOD_UPDATE_TTL|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u16(l3_check, constant_htons(0x0100), 0x0000);
   }

   mod->l3crc_update = hcsum_fold(l3_check);

   switch (iiph->protocol) {
      case IPPROTO_TCP:
         mod->l4crc_offset = offsetof(struct tcphdr, check);
         break;
      case IPPROTO_UDP:
         mod->l4crc_offset = offsetof(struct udphdr, check);
         break;
      case IPPROTO_ICMP:
#ifdef _LINUX_ICMP_H
         mod->l4crc_offset = offsetof(struct icmphdr, checksum);
#else
         mod->l4crc_offset = offsetof(struct icmphdr, check);
#endif
         isicmp = 1;
         break;
      default:
         mod->l4crc_offset = 0;
         break;
   }
   mod->l4crc_update = 0;
   if (mod->l4crc_offset) {
      u16 *iports = (u16 *)(in + PA_IPHLEN(iiph));
      u16 *oports = (u16 *)(out + PA_IPHLEN(oiph));
      if (isicmp) {
         l4_check = 0;
         mod->id = oports[2];
         if (iports[2] != oports[2]) {
            mod->flags |= AVM_PA_V4_MOD_ICMPID|AVM_PA_V4_MOD_PROTOHDR_CSUM;
            l4_check = hcsum_u16(l4_check, iports[2], oports[2]);
         }
      } else {
         if (mod->flags & AVM_PA_V4_MOD_ADDR)
            mod->flags |= AVM_PA_V4_MOD_PROTOHDR_CSUM;
         mod->sport = oports[0];
         if (iports[0] != oports[0]) {
            mod->flags |= AVM_PA_V4_MOD_SPORT|AVM_PA_V4_MOD_PROTOHDR_CSUM;
            l4_check = hcsum_u16(l4_check, iports[0], oports[0]);
         }
         mod->dport = oports[1];
         if (iports[1] != oports[1]) {
            mod->flags |= AVM_PA_V4_MOD_DPORT|AVM_PA_V4_MOD_PROTOHDR_CSUM;
            l4_check = hcsum_u16(l4_check, iports[1], oports[1]);
         }
      }
      mod->l4crc_update = hcsum_fold(l4_check);
   }

   mod->iphlen = PA_IPHLEN(oiph);
   return mod->flags != 0;
}

static void pa_do_v4_mod_rec(struct avm_pa_v4_mod_rec *mod, u8 *data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct iphdr *iph = (struct iphdr *)data;
   u16 *ports = (u16 *)(data + mod->iphlen);
   u32 sum;
   u16 csum;

   ctx->stats.rx_mod++;

   if (((unsigned long)iph) & 0x3) {
      memcpy(&iph->saddr, &mod->saddr, 2*sizeof(u32));
   } else {
      iph->saddr = mod->saddr;
      iph->daddr = mod->daddr;
   }
   iph->tos = mod->tos;
   if (mod->flags & AVM_PA_V4_MOD_UPDATE_TTL)
      iph->ttl--;

   sum = hcsum_prepare(iph->check);
   iph->check = hcsum_finish(hcsum_add(sum, mod->l3crc_update));

   if (mod->flags & AVM_PA_V4_MOD_PORT) {
      ports[0] = mod->sport;
      ports[1] = mod->dport;
   } else if (mod->flags & AVM_PA_V4_MOD_ICMPID) {
      ports[2] = mod->id;
   }
   csum = ports[mod->l4crc_offset>>1];
   if (csum || iph->protocol != IPPROTO_UDP) {
      sum = hcsum_prepare(csum);
      ports[mod->l4crc_offset>>1] = hcsum_finish(hcsum_add(sum, mod->l4crc_update));
   }
}

static void pa_show_v4_mod_rec(struct avm_pa_v4_mod_rec *mod,
                               pa_fprintf fprintffunc, void *arg)

{
   char buf[64];

   if (mod->flags & AVM_PA_V4_MOD_SADDR) {
      in_addr2str(&mod->saddr, buf, sizeof(buf));
      (*fprintffunc)(arg, "*IPv4 Src      : %s\n", buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_DADDR) {
      in_addr2str(&mod->daddr, buf, sizeof(buf));
      (*fprintffunc)(arg, "*IPv4 Dst      : %s\n", buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_TOS)
      (*fprintffunc)(arg, "*IPv4 Tos      : 0x%02x\n", mod->tos);

   if (mod->flags & AVM_PA_V4_MOD_UPDATE_TTL)
      (*fprintffunc)(arg, "*IPv4 TTL      : decrease\n");

   if (mod->flags & AVM_PA_V4_MOD_IPHDR_CSUM)
      (*fprintffunc)(arg, "*L3 Sum        : update 0x%02x\n", mod->l3crc_update);

   if (mod->flags &  AVM_PA_V4_MOD_SPORT)
      (*fprintffunc)(arg, "*Src Port      : %d\n", ntohs(mod->sport));

   if (mod->flags &  AVM_PA_V4_MOD_DPORT)
      (*fprintffunc)(arg, "*Dst Port      : %d\n", ntohs(mod->dport));

   if (mod->flags & AVM_PA_V4_MOD_ICMPID)
      (*fprintffunc)(arg, "*ICMP Id       : %d\n", ntohs(mod->id));

   if (mod->flags & AVM_PA_V4_MOD_PROTOHDR_CSUM)
      (*fprintffunc)(arg, "*L4 Sum        : update 0x%02x\n", mod->l4crc_update);
}

/* ------------------------------------------------------------------------ */

static void pa_show_mod_rec(struct avm_pa_mod_rec *mod,
                            pa_fprintf fprintffunc, void *arg)

{
   (*fprintffunc)(arg, "Hdrlen         : %u\n", (unsigned)mod->hdrlen);

   if (mod->ipversion)
      (*fprintffunc)(arg, "IP version     : %u\n", (unsigned)mod->ipversion);

   if (mod->pull_l2_len)
      (*fprintffunc)(arg, "L2 pull        : %d\n", mod->pull_l2_len);

   if (mod->pull_encap_len)
      (*fprintffunc)(arg, "Encap pull     : %d\n", mod->pull_encap_len);

   if (mod->push_ipversion)
      (*fprintffunc)(arg, "Push IPv       : %u\n", (unsigned)mod->push_ipversion);

   if (mod->push_udpoffset)
      (*fprintffunc)(arg, "Push UDP       : %u\n", (unsigned)mod->push_udpoffset);

   if (mod->push_encap_len) {
      char buf[256];
      data2hex(HDRCOPY(mod)+mod->push_l2_len, mod->push_encap_len, buf, sizeof(buf));
      (*fprintffunc)(arg, "Encap push     : %s\n", buf);
   }

   (*fprintffunc)(arg, "SKB proto      : %04X\n", (unsigned)ntohs(mod->protocol));

   pa_show_v4_mod_rec(&mod->v4_mod, fprintffunc, arg);

   if (mod->v6_decrease_hop_limit)
      (*fprintffunc)(arg, "IPv6 ttl       : decrease\n");
}

static int pa_egress_precheck(struct avm_pa_pid *pid,
                              PKT *pkt,
                              struct avm_pa_pkt_match *ingress,
                              struct avm_pa_pkt_match *egress)
{
   unsigned int hstart;
   int ret;
   if (pid->ecfg.flags & AVM_PA_PID_FLAG_HSTART_ON_EGRESS)
      hstart = AVM_PKT_INFO(pkt)->hstart;
   else
      hstart = 0;
   ret = pa_set_pkt_match(pid->egress_framing, hstart,
                        pkt, egress, 1);
   if (ret != AVM_PA_RX_OK)
      return ret;

   if (!AVM_PA_PKTTYPE_BASE_EQ(egress->pkttype, ingress->pkttype))
      return AVM_PA_RX_BYPASS;

   return AVM_PA_RX_OK;
}

static int pa_calc_modify(struct avm_pa_session *session,
                          struct avm_pa_pkt_match *ingress,
                          struct avm_pa_pkt_match *egress)
{
   /*
    * Precondition: AVM_PA_PKTTYPE_BASE_EQ(egress->pkttype, ingress->pkttype)
    */
   struct avm_pa_mod_rec *mod = &session->mod;
   int change = 0;

   mod->hdrlen = egress->hdrlen;
   mod->hdroff = egress->hdroff;
   memcpy(HDRCOPY(mod), HDRCOPY(egress), mod->hdrlen);
   mod->protocol = 0;
   mod->pkttype = egress->pkttype;
   if (AVM_PA_PKTTYPE_EQ(ingress->pkttype, egress->pkttype)) {
      mod->pull_encap_len = 0;
      if (ingress->encap_offset == AVM_PA_OFFSET_NOT_SET) {
         /* no tunnel, egress->encap_offset also not set */
         mod->pull_l2_len = ingress->ip_offset;
         mod->pull_encap_len = 0;
         mod->ipversion = AVM_PA_PKTTYPE_IP_VERSION(egress->pkttype);
         mod->push_encap_len = 0;
         mod->push_ipversion = 0;
         mod->push_l2_len = egress->ip_offset;
      } else {
         /* untouched tunnel, egress->encap_offset also set */
         mod->pull_l2_len = ingress->encap_offset;
         mod->pull_encap_len = 0;
         mod->ipversion = AVM_PA_PKTTYPE_IPENCAP_VERSION(egress->pkttype);
         mod->push_encap_len = 0;
         mod->push_ipversion = 0;
         mod->push_l2_len = egress->encap_offset;
      }
   } else { /* AVM_PA_PKTTYPE_BASE_EQ because of precheck */
      change++;
      if (ingress->encap_offset == AVM_PA_OFFSET_NOT_SET) {
         /* no tunnel header on input */
         mod->pull_l2_len = ingress->ip_offset;
         mod->pull_encap_len = 0;
         mod->ipversion = AVM_PA_PKTTYPE_IP_VERSION(ingress->pkttype);
      } else {
         /* tunnel header on input */
         mod->pull_l2_len = ingress->encap_offset;
         mod->pull_encap_len = ingress->ip_offset - ingress->encap_offset;
         mod->ipversion = AVM_PA_PKTTYPE_IP_VERSION(ingress->pkttype);
      }
      if (egress->encap_offset == AVM_PA_OFFSET_NOT_SET) {
         mod->push_encap_len = 0;
         mod->push_ipversion = 0;
         mod->push_l2_len = egress->ip_offset;
      } else {
         mod->push_encap_len = egress->ip_offset - egress->encap_offset;
         mod->push_ipversion = AVM_PA_PKTTYPE_IPENCAP_VERSION(egress->pkttype);
         mod->push_l2_len = egress->encap_offset;
      }
   }
   if (mod->push_ipversion) {
      change++;
      if (mod->push_ipversion == 4)
         mod->protocol = constant_htons(ETH_P_IP);
      else if (mod->push_ipversion == 6)
         mod->protocol = constant_htons(ETH_P_IPV6);
      if (egress->lisp_offset != AVM_PA_OFFSET_NOT_SET) {
         mod->push_udpoffset = egress->lisp_offset - egress->encap_offset;
         mod->push_udpoffset -= sizeof(struct udphdr);
      }
   } else {
      if (mod->ipversion == 4)
         mod->protocol = constant_htons(ETH_P_IP);
      else if (mod->ipversion == 6)
         mod->protocol = constant_htons(ETH_P_IPV6);
      mod->push_udpoffset = 0;
   }
   if (mod->ipversion == 4) {
      int ingress_offset = mod->pull_l2_len + mod->pull_encap_len;
      int egress_offset = mod->push_l2_len + mod->push_encap_len;
      if (pa_set_v4_mod_rec(&mod->v4_mod, session->routed,
                            HDRCOPY(ingress)+ingress_offset,
                            HDRCOPY(mod)+egress_offset))
         change++;
   } else if (mod->ipversion == 6) {
      if (session->routed) {
         mod->v6_decrease_hop_limit = 1;
         change++;
      }
   }
   return change;
}

static u8 casttype2pkt_type[] = {
   PACKET_HOST,
   PACKET_MULTICAST,
   PACKET_BROADCAST
};


/* ------------------------------------------------------------------------ */
/* -------- session retrieval and verification ---------------------------- */
/* ------------------------------------------------------------------------ */

static struct avm_pa_session *
pa_session_get(avm_session_handle session_handle)
{
   struct avm_pa_data    *pd = &pa_data;
   struct avm_pa_session *session;

   session = PA_SESSION(pd, session_handle);
   if (!avm_pa_session_valid(session))
      session = NULL;

   return session;
}

/* ------------------------------------------------------------------------ */
/* -------- packet forwarding --------------------------------------------- */
/* ------------------------------------------------------------------------ */

#ifdef CONFIG_AVM_PA_TX_NAPI

static int pa_dev_tx_napi_poll(struct napi_struct *napi, int budget)
{
   int done;
   struct avm_pa_pid *pid = container_of(napi, struct avm_pa_pid, tx_napi);

   for (done = 0; done < budget; done++) {
      PKT *pkt = skb_dequeue_tail(&pid->tx_napi_pkts);
      if (!pkt) break;
      pid->cfg.tx_func(pid->cfg.tx_arg, pkt);
   }

   if (done < budget)
      napi_complete(napi);

   return done;
}

#ifdef CONFIG_SMP
static void __do_schedule_napi(struct napi_struct *napi)
{
   int cpu = smp_processor_id();
   int tcpu = cpumask_any_but(cpu_online_mask, cpu);
   if (tcpu >= nr_cpumask_bits)
      tcpu = cpu;
   /* This runs in a tasklet because we want to run the "core transition" per
    * packet burst, and not per packet. Both napi_schedule_prep() and IPIs (via
    * smp_call_function_single()) on a per packet basis would be too expensive in this
    * smp scenario. (napi_schedule_prep() does atomic accesses which requires snooping
    * the other cores caches, and the napi_poll runs one of the other cores).
    *
    * Furthermore, guarding the IPI with napi_schedule_prep() has been found to
    * perform a bit better than doing the IPI straight in this tasklet. */
   if (napi_schedule_prep(napi))
      smp_call_function_single(tcpu, (void*)__napi_schedule, napi, 0);
}

static void do_schedule_napi(struct avm_pa_pid *pid)
{
   tasklet_schedule(&pid->tx_napi_tsk);
}
#else
static void do_schedule_napi(struct avm_pa_pid *pid)
{
   /* On UP the atomic access is a no-op */
   napi_schedule(&pid->tx_napi);
}
#endif

#endif

static inline void pa_do_push_l2(struct avm_pa_egress *egress, PKT *pkt)
{
   if (egress->push_l2_len) {
      memcpy(PKT_PUSH(pkt, egress->push_l2_len), HDRCOPY(&egress->match), egress->push_l2_len);

      if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET) {
         unsigned char *data = PKT_DATA(pkt) + egress->pppoe_offset;
         struct pppoehdr *pppoehdr = (struct pppoehdr *)data;
         pppoehdr->length = htons(PKT_FRAGLEN(pkt) - egress->pppoe_hdrlen);
      }
   }
}

static int _pa_transmit(struct avm_pa_egress *egress, PKT *pkt, int nfrags)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, egress->pid_handle);
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);

#ifdef CONFIG_AVM_PA_TX_NAPI
   /* A non-NULL dev indicates avm_pa_dev_pid_register_tx_napi() was used */
   if (pid->tx_napi.dev && (skb_queue_len(&pid->tx_napi_pkts) >= TX_NAPI_MAXQUEUE)) {
      PKT_FREE(pkt); /* drop packet, wouldn't fit anyway */
      return NET_XMIT_DROP;
   }
#endif

   /*
    * info->already_modified is set when paket comes
    * from avm_pa_tx_channel_accelerated_packet() and
    * statistics are counted in HW.
    *
    * 2016-03-02, calle
    */
   if (info->already_modified == 0) {
      egress->sw_stats.tx_pkts += nfrags;
      if (skb_has_frag_list(pkt))
         egress->sw_stats.tx_bytes += pkt->data_len + nfrags * (PKT_LEN(pkt) + egress->push_l2_len);
      else
         egress->sw_stats.tx_bytes += PKT_LEN(pkt) + egress->push_l2_len;
   }
   AVM_PKT_INFO(pkt)->is_accelerated = 1;
   egress->tx_pkts += nfrags;
   pid->tx_pkts += nfrags;

   switch (egress->type) {
      case avm_pa_egresstype_output:
         pa_do_push_l2(egress, pkt);
         pkt->tc_index = egress->output.tc_index;
#ifdef CONFIG_NET_CLS_ACT
         pkt->tc_verd = egress->output.tc_verd;
#endif
         if (pid->ecfg.cb_len) {
            memcpy(&pkt->cb[pid->ecfg.cb_start],
                   egress->output.cb, pid->ecfg.cb_len);
         }
         SKB_IFF(pkt) = egress->output.skb_iif;
         pkt->mac_len = egress->output.mac_len;
         if (egress->match.vlan_tci & VLAN_TAG_PRESENT) {
            pkt->vlan_tci = egress->match.vlan_tci;
#ifdef AVM_PA_SKBUFF_HAS_VLAN_PROTO
            pkt->vlan_proto = egress->match.vlan_proto;
#endif
         }

         pkt->pkt_type = PACKET_OUTGOING;
         /* We only modified the checksum for the first fragment which is actually
          * only the header template for skb->frag_list. Therefore checksum
          * calculation is incomplete (partial). Linux' GSO path handles this and
          * potentially uses hardware offloading for this. For non-frag_list
          * traffic we're have calculated the full checksum, none is left. */
         pkt->ip_summed = skb_has_frag_list(pkt) ? CHECKSUM_PARTIAL : CHECKSUM_NONE;
         skb_reset_mac_header(pkt);

         /* set priority */
         if (info->match.ack_only) {
            if (egress->output.tack_priority < egress->output.priority)
               pkt->priority = egress->output.tack_priority;
            else
               pkt->priority = egress->output.priority;
            pid->prioack_accl_acks++;
            egress->tcpack_pkts += nfrags;
         } else {
            pkt->priority = egress->output.priority;
         }
#ifdef CONFIG_TI_PACKET_PROCESSOR
         /*
          * Relevant PP fields must be copied into the egress to ensure the PP handles
          * the packet correctly as if it had taken the entire slow path (via ARM).
          *
          * In JZ-68647 (Puma 7: Cert-Fail SF-02 proc-1.1, root cause), it was found
          * that we copied to little and added skb->ti_meta_info* to the list. In
          * JZ-69391 it was found that we copied too much and overwrite important
          * per-packet PP information and went back to a white list of individual fields.
          *
          * Reasoning: We don't need to store session information as the PP
          * session is already set up (or no session at all). We need to store QoS / SF
          * relevant fields that are used in the xmit routines of the interface drivers,
          * even if there is no PP session at all. Except ti_epi_header which contains
          * per-packet data set by the PP.
          */
         SKB_GET_PP_INFO_P(pkt)->egress_queue = egress->output.puma_pktinfo.egress_queue;
#ifdef CONFIG_TI_META_DATA
         pkt->ti_meta_info = egress->output.ti_meta_info;
         pkt->ti_meta_info2 = egress->output.ti_meta_info2;
#endif
#endif
#if AVM_PA_TRACE
         if (ctx->dbgtrace)
            pa_printk(KERN_DEBUG, "avm_pa: %lu - _pa_transmit(%s), prio=0x%X, info->match.ack_only=%d\n",
                                  pkt_uniq_id(pkt), pid->cfg.name, pkt->priority, info->match.ack_only);
#endif
#ifdef CONFIG_AVM_PA_TX_NAPI
         if (pid->tx_napi.dev) {
            skb_queue_tail(&pid->tx_napi_pkts, pkt);
            do_schedule_napi(pid);
         }
         else
#endif
            if (egress->output.dst) {
               skb_dst_set(pkt, dst_clone(egress->output.dst));
               secpath_reset(pkt);
            }
         {
            (*pid->cfg.tx_func)(pid->cfg.tx_arg, pkt);
            ctx->stats.fw_output += nfrags;
         }
         return NET_XMIT_SUCCESS;

      case avm_pa_egresstype_local:
         {
            struct packet_type *ptype = pid->cfg.ptype;
            skb_set_network_header(pkt, 0);
            pkt->pkt_type = casttype2pkt_type[egress->match.casttype];
            if (egress->local.dst) {
               skb_dst_set(pkt, dst_clone(egress->local.dst));
               secpath_reset(pkt);
            }
            pkt->dev = egress->local.dev;
            SKB_IFF(pkt) = egress->local.skb_iif;
            ctx->stats.fw_local += nfrags;
            (*ptype->func)(pkt, pkt->dev, ptype, 0);
         }
         return NET_XMIT_SUCCESS;

      case avm_pa_egresstype_rtp:
         if (egress->rtp.sk) {
            size_t hsize;
            skb_set_network_header(pkt, 0);
            if (pkt->protocol == constant_htons(ETH_P_IP)) {
               struct iphdr *iph = (struct iphdr *)pkt->data;
               hsize = iph->ihl*4;
            } else {
               hsize = sizeof(struct ipv6hdr);
            }
            skb_pull(pkt, hsize); /* skb->data points to udphdr */
            skb_set_transport_header(pkt, 0);
            pkt->pkt_type = casttype2pkt_type[egress->match.casttype];
            pkt->dev = egress->rtp.dev;
            SKB_IFF(pkt) = egress->rtp.skb_iif;
            ctx->stats.fw_rtp += nfrags;
            (*egress->rtp.transmit)(egress->rtp.sk, pkt);
            return NET_XMIT_SUCCESS;
         } else {
            ctx->stats.fw_rtp_drop += nfrags;
            kfree_skb(pkt);
            return NET_XMIT_SUCCESS;
         }
      case avm_pa_egresstype_xfrm:
         if (IS_ENABLED(CONFIG_XFRM)) {
            pkt->dev = egress->xfrm.dev;
            skb_dst_set(pkt, dst_clone(egress->xfrm.dst));
            secpath_reset(pkt);
            pkt->tc_index = egress->xfrm.tc_index;
            (*pid->cfg.tx_func)(egress->xfrm.x, pkt);
            return NET_XMIT_SUCCESS;
         }
   }

   ctx->stats.fw_ill += nfrags;
   kfree_skb(pkt);
   return NET_XMIT_SUCCESS;
}

static inline u16 calc_frag_size(u16 mtu, u16 len)
{
   u16 frag_num = len/mtu;
   u16 frag_size;
   if (len % mtu) frag_num ++;
   frag_size = len / frag_num;
   if (frag_size & 7) { /* mod 8 */
      if (frag_num > 1 &&
         (((frag_num - 1)*(frag_size & 7) + frag_size ) > mtu)) {
         frag_num++;
         frag_size = len / frag_num;
      }
   }
   frag_size = frag_size & ~7; /* multiple of 8 */
   return frag_size;
}

static void zero_fragment_options(struct iphdr *iph)
{
   unsigned char *p = (unsigned char *)(iph+1);
   unsigned char *e = p + PA_IPHLEN(iph);
   unsigned char olen;
   while (p < e) {
      if (*p == IPOPT_EOL) {
         return;
      } else if (*p == IPOPT_NOP) {
         p++;
      } else {
         olen = *p;
         if (olen < 2 || p+olen > e)
            return;
         if (!IPOPT_COPIED(*p))
            memset(p, IPOPT_NOP, olen);
         p += olen;
      }
   }
}


static inline struct sk_buff *
pa_alloc_fragment(struct sk_buff *src, size_t len)
{
   struct sk_buff *skb;
   /* Remember: src->data points to the network header, and so does the new skb->data.
    * The ethernet header is part of src's headroom must be set separately. */
   size_t headroom = skb_headroom(src);
   skb = alloc_skb(headroom + len, GFP_ATOMIC);
   if (skb) {
      skb->protocol = src->protocol;
      skb_reserve(skb, headroom);
      skb_reset_network_header(skb);
      skb_put(skb, len);
   }
   return skb;
}


static void pa_fragment_ipv4(struct avm_pa_egress *egress, u16 omtu, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   u16 iphlen, len, left, mtu, offset, mf, frag_size = 0;
   unsigned char *data;
   struct iphdr *iph;

   iph = (struct iphdr *)PKT_DATA(pkt);
   iphlen = (u16)PA_IPHLEN(iph);

   mtu = (u16)((omtu - iphlen) & ~7); /* set mtu to multiple of 8 */
   left = (u16)(PKT_LEN(pkt) - iphlen);
   data = PKT_DATA(pkt) + iphlen;

   offset = (u16)((ntohs(iph->frag_off) & IP_OFFSET) << 3);
   mf = (u16)(iph->frag_off & constant_htons(IP_MF));

   frag_size = calc_frag_size(mtu, left);

   /* TODO: This could be optimized of the egress supports GSO
    * (build up pkt->frag_list instead of _pa_transmit() for each single packet) */
   while (left > 0) {
      struct iphdr *niph;
      PKT *npkt;
      if (left > mtu) len = frag_size; /* prevent to small fragments */
      else len = left;
      if ((npkt = pa_alloc_fragment(pkt, iphlen+len)) == 0) {
         ctx->stats.fw_frag_fail++;
         break;
      }
      memcpy(PKT_DATA(npkt), PKT_DATA(pkt), iphlen);
      memcpy(PKT_DATA(npkt) + iphlen, data, len);
      niph = (struct iphdr *)PKT_DATA(npkt);
      niph->frag_off = htons((u16)(offset >> 3));
      left -= len;
      if (offset == 0) zero_fragment_options(iph);
      if (left > 0 || mf)
         niph->frag_off |= constant_htons(IP_MF);
      data += len;
      offset += len;
      niph->tot_len = htons((u16)(iphlen+len));
      set_ip_checksum(niph);
      if (_pa_transmit(egress, npkt, 1) == NET_XMIT_DROP) {
         ctx->stats.fw_frag_fail++;
         break;
      }
      else
         ctx->stats.fw_frags++;
   }
   PKT_FREE(pkt);
}

static void pa_fragment_ipv6(struct avm_pa_egress *egress, u16 omtu, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   u16 phlen, hlen, nhlen, len, left, mtu, offset, frag_size = 0;
   struct ipv6hdr *ipv6h;
   unsigned char *data;
   u32 id;

   ipv6h = (struct ipv6hdr *)PKT_DATA(pkt);
   phlen = sizeof(struct ipv6hdr) + sizeof(struct ipv6fraghdr);
   hlen = (u16)sizeof(struct ipv6hdr);
   nhlen = (u16)hlen + sizeof(struct ipv6fraghdr);

   /* set mtu to multiple of 8 */
   mtu = (u16)((omtu - phlen) & ~7);
   left = (u16)(pkt->len - hlen);
   data = PKT_DATA(pkt) + hlen;

   frag_size = calc_frag_size(mtu, left);

   offset = 0;
   id = rand();

   /* TODO: This could be optimized of the egress supports GSO
    * (build up pkt->frag_list instead of _pa_transmit() for each single packet) */
   while (left > 0) {
      struct ipv6fraghdr *fragh;
      struct ipv6hdr *nipv6h;
      PKT *npkt;

      if (left > mtu) len = frag_size; /* prevent to small fragments */
      else len = left;
      if ((npkt = pa_alloc_fragment(pkt, nhlen+len)) == 0) {
         PKT_FREE(pkt);
         ctx->stats.fw_frag_fail++;
         return;
      }
      memcpy(PKT_DATA(npkt), PKT_DATA(pkt), hlen);
      memcpy(PKT_DATA(npkt) + nhlen, data, len);
      nipv6h = (struct ipv6hdr *)PKT_DATA(npkt);
      fragh = (struct ipv6fraghdr *)(nipv6h + 1);
      memcpy(nipv6h, ipv6h, sizeof(struct ipv6hdr));
      fragh->nexthdr = nipv6h->nexthdr;
      nipv6h->nexthdr = IPPROTO_FRAGMENT;
      fragh->reserved = 0;
      fragh->frag_off = htons((u16)offset);
      fragh->identification = id;
      left -= len;
      if (left > 0)
         fragh->frag_off |= constant_htons(IP6_MF);
      data += len;
      offset += len;
      nipv6h->payload_len = htons((u16)(sizeof(struct ipv6fraghdr)+len));
      if (_pa_transmit(egress, npkt, 1) == NET_XMIT_DROP) {
         ctx->stats.fw_frag_fail++;
         break;
      }
      else
         ctx->stats.fw_frags++;
   }
   PKT_FREE(pkt);
}

static void pa_transmit(struct avm_pa_egress *egress, PKT *pkt, int bridged, int nfrags)
{
   struct avm_pa_global *ctx = &pa_glob;
   u16 total_len;

   avm_simple_profiling_skb(0, pkt);
   /*
    * Bugfix: bridge packets were cut, when third position of
    *         mac address was 0x00, because ethernet header
    *         was used as IP/IPv6 header, and packets were
    *         trimed and perhaps fragmented.
    *
    * packets for bridge sessions arrive with ethernet header,
    * we do not need fragmentation or size check here.
    *
    *             2014-07-08 calle
    */
   if (bridged == 0) {
      if (pkt->protocol == constant_htons(ETH_P_IP)) {
         struct iphdr *iph = (struct iphdr *)PKT_DATA(pkt);
         total_len = ntohs(iph->tot_len);
         PKT_TRIM(pkt, total_len);
         if (PKT_LEN(pkt) > egress->mtu) {
            pa_fragment_ipv4(egress, egress->mtu, pkt);
            return;
         }
      } else if (pkt->protocol == constant_htons(ETH_P_IPV6)) {
         struct ipv6hdr *ipv6h = (struct ipv6hdr *)PKT_DATA(pkt);
         total_len = sizeof(struct ipv6hdr)+ntohs(ipv6h->payload_len);
         PKT_TRIM(pkt, total_len);
         if (PKT_LEN(pkt) > egress->mtu) {
            pa_fragment_ipv6(egress, egress->mtu, pkt);
            return;
         }
      }
   }
   if (_pa_transmit(egress, pkt, nfrags) == NET_XMIT_DROP)
      ctx->stats.fw_drop += nfrags;
   else
      ctx->stats.fw_pkts += nfrags;
}


static void pa_do_modify_l3(struct avm_pa_mod_rec *mod, PKT *pkt)
{
   if (mod->v4_mod.flags) {
      pa_do_v4_mod_rec(&mod->v4_mod, PKT_DATA(pkt));
   } else if (mod->v6_decrease_hop_limit) {
      struct ipv6hdr *ipv6h = (struct ipv6hdr *)PKT_DATA(pkt);
      ipv6h->hop_limit--;
   }
}


static void pa_do_modify_non_l2(struct avm_pa_mod_rec *mod, PKT *pkt, int bridged)
{
   pkt->protocol = mod->protocol;
   /* The actual vlan_tci will be inserted on egress. */
   pkt->vlan_tci = 0;

   if (bridged) {
      /* We have to initialize skb->network_header for Linux' transmit paths.
       * For bridged we can safely assume ethernet (might be vlan tagged,
       * but that's OK as long as pkt->protocol agrees). */
      skb_set_network_header(pkt, ETH_HLEN);
      skb_reset_mac_len(pkt);
      return;
   }

   if (mod->pull_l2_len)
      PKT_PULL(pkt, mod->pull_l2_len);
   if (mod->pull_encap_len)
      PKT_PULL(pkt, mod->pull_encap_len);

   /* We're now at the innermost l3 header, set offsets in the skb appropriately.
    * This is required for Linux' transmit paths and some drivers (but remember that
    * this is not done for bridged sessions). */
   skb_reset_network_header(pkt);
   if (mod->protocol == constant_htons(ETH_P_IP))
      skb_set_transport_header(pkt, mod->v4_mod.iphlen);
   else if (mod->protocol == constant_htons(ETH_P_IPV6))
      skb_set_transport_header(pkt, sizeof(struct ipv6hdr));

   pa_do_modify_l3(mod, pkt);

   if (mod->push_encap_len) {
      unsigned tot_len;
      memcpy(PKT_PUSH(pkt, mod->push_encap_len),
             HDRCOPY(mod)+mod->push_l2_len, mod->push_encap_len);
      tot_len = PKT_LEN(pkt);
      if (mod->push_ipversion == 4) {
         struct iphdr *iph = (struct iphdr *)PKT_DATA(pkt);
         iph->id = rand() & 0xffff;
         iph->tot_len = htons(tot_len);
         set_ip_checksum(iph);
      } else {
         struct ipv6hdr *ipv6h = (struct ipv6hdr *)PKT_DATA(pkt);
         ipv6h->payload_len = htons(tot_len - sizeof(struct ipv6hdr));
      }
      if (mod->push_udpoffset) {
         struct udphdr *udph = (struct udphdr *)(PKT_DATA(pkt)+mod->push_udpoffset);
         udph->len = htons(tot_len - mod->push_udpoffset);
         if (mod->push_ipversion == 4)
            set_udp_checksum((struct iphdr *)PKT_DATA(pkt), udph);
         else
            set_udpv6_checksum((struct ipv6hdr *)PKT_DATA(pkt), udph);
      }
   }
}

static void
_pa_do_send_egress(struct avm_pa_session *session, PKT *pkt, int bridged, int nfrags)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_egress *egress, *first;
   PKT *npkt;

   egress = first = avm_pa_first_egress(session);
   /* We can transmit to the egress in any order as long as the skbs per egress are
    * in order. This is optimized to avoid a copy in the common, single egress case. */
   hlist_for_each_entry_continue_rcu(egress, egress_list) {
      if ((npkt = PKT_COPY(pkt)) != 0)
         pa_transmit(egress, npkt, bridged, nfrags);
      else
         ctx->stats.fw_fail += nfrags;
   }
   pa_transmit(first, pkt, bridged, nfrags);
}
static inline int
_pa_head_skb_has_data(struct sk_buff *skb)
{
#ifdef CONFIG_GRX5
   return 1;
#else
   return 0;
#endif
}

static inline int
_pa_get_header_size(struct avm_pa_session *session, struct sk_buff *skb)
{
   if (_pa_head_skb_has_data(skb)) {
      /* If the head skb has data we can't easily derive the header size from it,
       * see comment in _pa_do_modify_and_send_single() about GRX. Therefore
       * the header size is stored in the session, but this is fragile as later packets
       * may add TCP options. */
      return session->ingress.full_hdrlen;
   }
   else {
      /* If the head skb has no data, but just headers, we can use that
       * as an indication for the header size of fraglist skbs. In contrast to above,
       * this is save as it doesn't rely data recorded at session creation. */
      return skb_headlen(skb);
   }
}


static inline void
_pa_calc_gso_stats(struct avm_pa_session *session, struct sk_buff *skb,
              int *p_packets, int *p_bytes)
{
   struct sk_buff *next;
   int bytes, nfrags, hdr_size;

   hdr_size = _pa_get_header_size(session, skb);
   bytes = skb->len; /* includes payload bytes of fraglist skbs */
   nfrags = _pa_head_skb_has_data(skb) ? 1 : 0;

   next = skb_shinfo(skb)->frag_list;
   do {
      nfrags += 1;
      /* skb->len does not include the header bytes of fraglist skbs */
      bytes += hdr_size;
   } while((skb = next->next) != NULL);

   *p_packets = nfrags;
   *p_bytes = bytes;
}



static void
_pa_do_modify_and_send_single(struct avm_pa_session *session, struct sk_buff *skb)
{
   struct avm_pa_mod_rec *mod = &session->mod;
   int bridged = session->bsession != 0;
   struct sk_buff *next;
   int bytes, nfrags, hdr_size;

   hdr_size = _pa_get_header_size(session, skb);
   bytes = skb->len; /* includes payload bytes of fraglist skbs */
      
   next = skb_shinfo(skb)->frag_list;
   skb_frag_list_init(skb);
   if (_pa_head_skb_has_data(skb)) {
      int headlen;
      /* JZ-28078: 7580: avm_pa ip_local_receive regression
       *
       * Hardware gro on GRX modifies the ip/ipv6 length of the head skb to the length of
       * the entire TCP/UDP packet. And, unlike software gro it keeps data in the head
       * skb. We have to undo the length modification and send the head skb itself too,
       * if we're forwarding fraglist packets sequentially. Fortunately, the hardware
       * doesn't change the checksum so we don't have to update that.
       *
       * This is a temporary hotfix, and it relies on the fact that hardware lro is
       * only enabled on local sessions which never bridged. On other platforms
       * gro/lro packets look different. */
      BUG_ON(bridged);

      /* Clear any signs of gso before transmitting the head skb, that would confuse Linux */
      skb->len -= skb->data_len;
      skb->data_len = 0;
      skb_shinfo(skb)->gso_size = 0;
      pa_do_modify_non_l2(mod, skb, 0);
      /* Undo L3 header modification already done by hardware lro. */
      headlen = skb_headlen(skb);
      if (mod->protocol == constant_htons(ETH_P_IP))
         ((struct iphdr *)PKT_DATA(skb))->tot_len = htons(headlen);
      else if (mod->protocol == constant_htons(ETH_P_IPV6))
         ((struct ipv6hdr *)PKT_DATA(skb))->payload_len = htons(headlen - sizeof(struct ipv6hdr));
      _pa_do_send_egress(session, skb, 0, 1);
      nfrags = 1;
   }
   else {
      kfree_skb(skb);
      nfrags = 0;
   }

   skb = next;
   do {
      next = skb->next;
      skb->next = NULL;
      nfrags += 1;
      /* For fraglist skbs, skb->data points to after the tcp/udp header. That
       * header is still intact so we can simply push back using session information,
       * and then perform NAT. FIXME: This assumes the TCP header size doesn't change
       * which may not be true (e.g. due to SACK). */
      bytes += hdr_size;
      skb_push(skb, hdr_size);
      pa_do_modify_non_l2(mod, skb, bridged);
      _pa_do_send_egress(session, skb, bridged, 1);
   } while((skb = next));

   session->ingress_sw_stats.tx_bytes += bytes;
   session->ingress_sw_stats.tx_pkts  += nfrags;
}

static void
_pa_do_modify_and_send(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_mod_rec *mod = &session->mod;
   int bridged = session->bsession != 0;
   int gso, nfrags, bytes;

   if (skb_has_frag_list(pkt)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
      /* For now, only plain Ethernet+IP can use the fast GSO path, pppoe and tunneling
       * take the slower path. Adopt more traffic types this once a driver supports it,
       * but beware that IP fragmentation must be handled as well.
       * Hack: Peek at the first egress to see if PPPoE is in use, since this
       * is not available in the pkttype. This assumes all egress use PPPoE but this
       * is currently always the case since we don't do multicast on upstream and
       * never do PPPoE on upstream. */
#if AVM_PA_WITH_GSO
      gso = (mod->pkttype & ~AVM_PA_PKTTYPE_BASE_MASK) == 0 && session->egress[0].pppoe_offset == AVM_PA_OFFSET_NOT_SET;
#else
      gso = 0; /* forcefully disabled until more testing has been done */
#endif
      if (gso) {
         ctx->stats.tx_fast_gso += 1;
         _pa_calc_gso_stats(session, pkt, &nfrags, &bytes);
      }
#endif
   }
   else {
      gso = nfrags = 1; /* single, non-frag_list packets also use the normal path */
      bytes = PKT_LEN(pkt);
   }

   /* In the GSO case with frag_list, the head skb must be modified. Linux GSO
    * will then use this as a template for the frag_list skbs, which is possibly
    * done in HW (otherwise we'd do it ourselves). In the non-GSO case
    * we must transmit each fragment sequentially. */
   if (gso) {
      session->ingress_sw_stats.tx_bytes += bytes;
      session->ingress_sw_stats.tx_pkts  += nfrags;

      pa_do_modify_non_l2(mod, pkt, bridged);
      _pa_do_send_egress(session, pkt, bridged, nfrags);
   } else {
      _pa_do_modify_and_send_single(session, pkt);
   }

   if (session->timeout == 0) 
      pa_session_flush(session, "fast timeout");
}


/* Pass NULL for session to to get it from the packet. Do this if there is uncertainty if
 * the session is still valid, i.e. if the packet was queued and the the RCU read side
 * critical section was left. If the session is given, we're still inside
 * the RCU lock of avm_pa_pid_receive(). */
static void
pa_do_modify_and_send(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;

   avm_simple_profiling_skb(0, pkt);

   rcu_read_lock();
   if (!session) {
      /* Protect against possible race with GC timer deleting sessions */
      session = pa_session_get(AVM_PKT_INFO(pkt)->session_handle);
      if (unlikely(!session)) {
         ctx->stats.fw_drop_gone++;
         rcu_read_unlock();
         PKT_FREE(pkt);
         return;
      }
   }

   BUG_ON(AVM_PKT_INFO(pkt)->session_uniq_id != session->uniq_id);
   /* From now on, we can be sure the session remains valid because
    * of the RCU read side critical section. The session may leave
    * the ACTIVE list but n*/
   if (AVM_PKT_INFO(pkt)->already_modified) {
      PKT *npkt;
      struct avm_pa_egress *egress;
      int nfrags = 0;
      skb_walk_frags(pkt, npkt)
         nfrags += 1;
      egress = AVM_PKT_INFO(pkt)->forced_egress;
      pa_transmit(egress, pkt, session->bsession != 0, nfrags ? nfrags : 1);
   }
   else {
      _pa_do_modify_and_send(session, pkt);
   }
   rcu_read_unlock();
}

static int pa_egress_size_check(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);

   if (info->match.fragok)
      return 0;
   if (session->mod.push_encap_len == 0) { /* no tunnel on output */
      struct avm_pa_mod_rec *mod = &session->mod;
      unsigned len = PKT_FRAGLEN(pkt) - mod->pull_l2_len - mod->pull_encap_len;
      struct avm_pa_egress *egress;

      avm_pa_for_each_egress(egress, session) {
         if (len > egress->mtu)
            return -1;
      }
   }
   return 0;
}

/* ------------------------------------------------------------------------ */
/* -------- macaddr management -------------------------------------------- */
/* ------------------------------------------------------------------------ */

static struct vlan_ethhdr *
pa_get_ethhdr(enum avm_pa_framing framing, struct sk_buff *skb)
{
   if (framing == avm_pa_framing_ether)
      return (struct vlan_ethhdr *) skb->data;
   if (framing == avm_pa_framing_dev)
      return vlan_eth_hdr(skb);
   return 0;
}

static u16
pa_get_vlan_tag(enum avm_pa_framing framing, struct sk_buff *skb)
{
   struct vlan_ethhdr *ethh = pa_get_ethhdr(framing, skb);

   if (!ethh)
      return 0;
   else if (skb_vlan_tag_present(skb))
      return skb->vlan_tci;
   else if (  ethh->h_vlan_proto == htons(ETH_P_8021Q)
           || ethh->h_vlan_proto == htons(ETH_P_8021AD))
      return ntohs(ethh->h_vlan_TCI) | VLAN_TAG_PRESENT;
   else
      return 0;
}

static void pa_show_macaddr(struct avm_pa_macaddr *macaddr,
                            pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, macaddr->pid_handle);
   char vlan_buf[32] = "";
   char buf[32];

   if (macaddr->vlan_id & VLAN_TAG_PRESENT) {
      snprintf(vlan_buf, sizeof(vlan_buf), " vlan id %d", macaddr->vlan_id & VLAN_VID_MASK);
   }

   mac2str(&macaddr->mac, buf, sizeof(buf));
   (*fprintffunc)(arg, "Macaddr        : %s%s ref %3lu Pid %2d (%s)\n", buf, vlan_buf,
                      macaddr->refcount, macaddr->pid_handle, pid->cfg.name);
}

static inline u32 macaddr_hash(const unsigned char mac[ETH_ALEN])
{
   u32 h = 0;
   int i;

   for (i=0; i < ETH_ALEN; i++) {
      h += mac[i]; h += (h<<10); h ^= (h>>6);
   }
   h += (h<<3); h ^= (h>>11); h += (h<<15);
   return h;
}

static struct avm_pa_macaddr *
_pa_macaddr_link(unsigned char mac[ETH_ALEN], avm_pid_handle pid_handle, u16 vlan_id)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_macaddr *p;
   u32 hash;
   int i;

   vlan_id &= VLAN_TAG_PRESENT|VLAN_VID_MASK;
   hash = macaddr_hash(mac);

   spin_lock(&avm_pa_lock);

   for (p = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (memcmp(p->mac, mac, ETH_ALEN) == 0) {
         p->refcount++;
         p->pid_handle = pid_handle;
         p->vlan_id = vlan_id;
         goto unlock;
      }
   }

   for (i=0; i < CONFIG_AVM_PA_MAX_SESSION; i++) {
      p = &ctx->macaddr_array[i];
      if (p->refcount == 0) {
         memcpy(p->mac, mac, ETH_ALEN);
         p->pid_handle = pid_handle;
         p->vlan_id = vlan_id;
         p->refcount++;
         p->link = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION];
         ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION] = p;
         if (ctx->dbgsession) {
            pa_printk(KERN_DEBUG, "\navm_pa: new macaddr:\n");
            pa_show_macaddr(p, pa_printk, KERN_DEBUG);
         }
         goto unlock;
      }
   }

unlock:
   spin_unlock(&avm_pa_lock);
   return p;
}

static inline struct avm_pa_macaddr *
pa_macaddr_link(unsigned char mac[ETH_ALEN], avm_pid_handle pid_handle, u16 vlan_id)
{
   if (mac[0] & 1)
      return 0;
   return _pa_macaddr_link(mac, pid_handle, vlan_id);
}

static void pa_macaddr_unlink(struct avm_pa_macaddr *destmac)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_macaddr *p, **pp;
   u32 hash;

   spin_lock(&avm_pa_lock);

   if (--destmac->refcount > 0)
      goto unlock;

   hash = macaddr_hash(destmac->mac);
   pp = &ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION];
   while ((p = *pp) != 0) {
      if (p == destmac) {
         *pp = p->link;
         if (ctx->dbgsession) {
            pa_printk(KERN_DEBUG, "\navm_pa: delete macaddr:\n");
            pa_show_macaddr(p, pa_printk, KERN_DEBUG);
         }
         memset(p, 0, sizeof(struct avm_pa_macaddr));
         goto unlock;
      }
      pp = &p->link;
   }

unlock:
   spin_unlock(&avm_pa_lock);
}

static void pa_check_and_handle_ingress_pid_change(unsigned char mac[ETH_ALEN],
                                                   avm_pid_handle pid_handle,
                                                   u16 vlan_id)
{
   struct avm_pa_global *ctx = &pa_glob;
   u32 hash = macaddr_hash(mac);
   struct avm_pa_macaddr *p;
   int pid_group = PA_PID(ctx, pid_handle)->ecfg.pid_group;
   int pid_changed = 0;

   vlan_id &= VLAN_TAG_PRESENT|VLAN_VID_MASK;

   rcu_read_lock();

   for (p = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (memcmp(mac, &p->mac, ETH_ALEN) == 0) {
         if (p->pid_handle != pid_handle) {
            struct avm_pa_pid *pid = PA_PID(ctx, p->pid_handle);
            if (pid->ingress_pid_handle != pid_handle) {
               if (pid_group == 0 || pid_group != pid->ecfg.pid_group)
                  pid_changed = 1;
            }
         }
         if (p->vlan_id != vlan_id)
            pid_changed = 1;
         break;
      }
   }

   rcu_read_unlock();

   if (pid_changed) {
      int old = ctx->stats.sess_flushed;

      if (net_ratelimit()) {
         char buf[128];
         mac2str(mac, buf, sizeof(buf));
         pr_info("avm_pa: pid changed for %s (%s(%d) vlan id %d -> %s(%d) vlan id %d)\n",
                 buf,
                 PA_PID(ctx, p->pid_handle)->cfg.name, p->pid_handle, p->vlan_id & VLAN_VID_MASK,
                 PA_PID(ctx, pid_handle)->cfg.name, pid_handle, vlan_id & VLAN_VID_MASK);
      }
      avm_pa_flush_sessions_with_destmac(p);
      ctx->stats.sess_pidchanged += ctx->stats.sess_flushed - old;
   }
}

/* ------------------------------------------------------------------------ */
/* -------- pid life cycle management ------------------------------------- */
/* ------------------------------------------------------------------------ */

static void
_pa_hw_pa_release(struct kref *ref)
{
   struct avm_pa_global *ctx = &pa_glob;

   ctx->hardware_pa.flags = 0;
   if (ctx->hw_pa_flush_completion) {
      complete(ctx->hw_pa_flush_completion);
      ctx->hw_pa_flush_completion = NULL;
   }
}

static int
pa_hw_pa_get(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   return kref_get_unless_zero(&ctx->hw_pa_ref);
}

static int
pa_hw_pa_put(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   return kref_put(&ctx->hw_pa_ref, _pa_hw_pa_release);
}

static int
pa_hw_pa_valid(struct avm_hardware_pa *hwpa)
{
   /* exactly one of add_session or add_session_skb must be set */
   if (hwpa->add_session && !hwpa->add_session_skb)
      return 1;
   if (!hwpa->add_session && hwpa->add_session_skb)
      return 1;
   return 0;
}

static void inline
pa_pid_init(avm_pid_handle pid_handle, struct avm_pa_pid_cfg *cfg)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   BUG_ON(pid_handle == 0);

#if AVM_PA_REF_DEBUG
   pr_err("avm_pa: pid %d ref %d (%s) called from %pf (before)\n",
          pid_handle,
          atomic_read(&pid->ref.refcount),
          "pa_pid_init", (void *)_RET_IP_);
#endif

   spin_lock_bh(&avm_pa_lock);
   /* Do not call pa_pid_get() on purpose. That would check "pid->pid_handle == 0"
    * in addition to the actual refcount, and return no new reference in that case.
    * But we want to detect if we're being called while no new reference are allowed
    */
   if (kref_get_unless_zero(&pid->ref) == 0) {
      memset(pid, 0, sizeof(struct avm_pa_pid));
      kref_init(&pid->ref);
#if AVM_PA_REF_DEBUG
      pr_err("avm_pa: pid %d ref %d (%s) called from %pf (after)\n",
             pid_handle,
             atomic_read(&pid->ref.refcount),
             "pa_pid_init(new)", (void *)_RET_IP_);
#endif
   } else {
      pr_err("avm_pa: pid %d (%s) ref %d already registered\n",
             pid_handle, cfg->name,
             atomic_read(&pid->ref.refcount));
      spin_unlock_bh(&avm_pa_lock);
      BUG();
   }
   pid->pid_handle = pid_handle;
   pid->cfg = *cfg;

   if (pid->cfg.default_mtu == 0)
      pid->cfg.default_mtu = 1500;

   pid->ingress_framing = cfg->framing;
   switch (cfg->framing) {
      case avm_pa_framing_llcsnap:
      case avm_pa_framing_ether:
      case avm_pa_framing_ppp:
      case avm_pa_framing_ip:
      case avm_pa_framing_ipdev:
         pid->egress_framing = cfg->framing;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_dev:
         pid->egress_framing = avm_pa_framing_ether;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_ptype:
         pid->egress_framing = cfg->framing;
         pid->cfg.tx_func = 0;
         pid->cfg.tx_arg = 0;
         avm_pa_pid_activate_hw_accelaration(pid_handle);
         break;
   }

   spin_unlock_bh(&avm_pa_lock);
}

static void
_pa_pid_delete(struct kref *ref)
{
   struct avm_pa_pid        *pid  = container_of(ref, struct avm_pa_pid, ref);
   struct avm_pa_pid_hwinfo *hw   = pid->hw;
   struct completion        *done = pid->release_completion;

   /* Only cleared by avm_pa_dev_unregister(). It is a bug if the
    * ref drops to 0 without going through that function.
    */
   BUG_ON(pid->pid_handle != 0);

#if AVM_PA_REF_DEBUG
   pr_err("avm_pa: pid %d ref %d (%s) called from %pf\n",
          pid->pid_handle,
          atomic_read(&pid->ref.refcount),
          "_pa_pid_delete", (void *)_RET_IP_);
#endif
   pid->ingress_pid_handle = 0;
   pid->hw = NULL;
   pid->release_completion = NULL;
   kfree(hw);
   if (done)
      complete(done);
   /* keep cfg for reuse by name */
}

/*
 * Given a pid_handle, decrease the ref count of the corresponding avm_pa_pid.
 * Resources are released if the ref count drops to zero.
 *
 * Returns 1 if the pid_handle was removed, otherwise 0.
 */
static int
pa_pid_put(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid    *pid = PA_PID(ctx, pid_handle);
   int ret;

   BUG_ON(pid_handle == 0);
#if AVM_PA_REF_DEBUG
   pr_err("avm_pa: pid %d ref %d (%s) called from %pf (before)\n",
          pid_handle,
          atomic_read(&pid->ref.refcount),
          "pa_pid_put", (void *)_RET_IP_);
#endif

   ret = kref_put(&pid->ref, _pa_pid_delete);

#if AVM_PA_REF_DEBUG
   pr_err("avm_pa: pid %d ref %d (%s) called from %pf (after)\n",
          pid_handle,
          atomic_read(&pid->ref.refcount),
          "pa_pid_put", (void *)_RET_IP_);
#endif
   return ret;
}

/*
 * Given a pid_handle, increase the ref count of the corresponding avm_pa_pid
 *
 * Each session holds a ref on all pids involved. So if you have a valid session,
 * (as per pa_session_valid()) use PA_PID() instead, especially in the fast path, as
 * refcounting is unecessarily expensive.
 *
 * If the pid is not registered, 0 is returned and the ref count is restored.
 */
static avm_pid_handle
pa_pid_get(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid    *pid = PA_PID(ctx, pid_handle);

   BUG_ON(pid_handle == 0);

#if AVM_PA_REF_DEBUG
   pr_err("avm_pa: pid %d ref %d (%s) called from %pf (before)\n",
          pid_handle,
          atomic_read(&pid->ref.refcount),
          "pa_pid_get", (void *)_RET_IP_);
#endif

   if (kref_get_unless_zero(&pid->ref) == 0)
      return 0;

#if AVM_PA_REF_DEBUG
   pr_err("avm_pa: pid %d ref %d (%s) called from %pf (after)\n",
          pid_handle,
          atomic_read(&pid->ref.refcount),
          "pa_pid_get", (void *)_RET_IP_);
#endif

   if (pid->pid_handle != pid_handle) {
      /* avm_pa_dev_unregister() clears pid->pid_handle to prevent new references */
      kref_put(&pid->ref, _pa_pid_delete);
      return 0;
   }

   return pid->pid_handle;
}

/*
 * Given a pid_handle, increase the ref count of the corresponding avm_pa_pid and return it.
 *
 * If the pid is not registered, NULL is returned and the ref count is restored.
 */
static struct avm_pa_pid *
pa_pid_get_pid(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_pid_handle        n   = pa_pid_get(pid_handle);

   return n ? PA_PID(ctx, n) : NULL;
}


/* Uninlined versions for other modules, hot code paths should use pa_pid_get(). */
struct avm_pa_pid *
avm_pa_pid_get_pid(avm_pid_handle pid_handle)
{
   return pa_pid_get_pid(pid_handle);
}

int
avm_pa_pid_put(avm_pid_handle pid_handle)
{
   return pa_pid_put(pid_handle);
}

/* same for vpid, but don't tell there is no reference counting yet */
struct avm_pa_vpid *
avm_pa_vpid_get_vpid(avm_vpid_handle vpid_handle)
{
   struct avm_pa_global *ctx  = &pa_glob;
   struct avm_pa_vpid   *vpid = PA_VPID(ctx, vpid_handle);

   if (vpid->vpid_handle)
      return vpid;
   return NULL;
}

int
avm_pa_vpid_put(avm_vpid_handle vpid_handle)
{
   return 1;
}


/* ------------------------------------------------------------------------ */
/* -------- bsession management ------------------------------------------- */
/* ------------------------------------------------------------------------ */

static __be16
pa_ethh_l3proto(struct vlan_ethhdr *ethh)
{
   if (  ethh->h_vlan_proto == __constant_htons(ETH_P_8021Q)
      || ethh->h_vlan_proto == __constant_htons(ETH_P_8021AD))
      return ethh->h_vlan_encapsulated_proto;
   else
      return ethh->h_vlan_proto;
}


static inline u32 ethh_hash(struct vlan_ethhdr *ethh)
{
   return jhash_3words(get_unaligned((u32 *)(&ethh->h_source[2])),
                       get_unaligned((u32 *)(&ethh->h_dest[2])),
                       (u32) pa_ethh_l3proto(ethh), 0);
}


static inline struct avm_pa_session *
pa_bsession_search(struct avm_pa_pid *pid, u32 hash, struct vlan_ethhdr *ethh)
{
   struct avm_pa_data     *pd = &pa_data;
   struct avm_pa_bsession *p  = NULL;
   __be16 l3proto             = pa_ethh_l3proto(ethh);
   u32 h                      = hash%AVM_PA_MAX_HASH;

   rcu_read_lock();

   /* bsessions are identified by the client's MAC addresses and the l3 protocol.
    * So ARP won't be accelerated via bsessions. The vlan id is not relevant
    * here because they are tightly coupled to the MAC address, i.e. we don't support
    * MAC addresses appearing in multiple VLANs (changes in VLAN invalidate all
    * corresponding bsessions). */
   hlist_for_each_entry_rcu(p, &pid->hash_bsess[h], hash_list) {
      /* Don't consider flushed sessions */
      if (!memcmp(ethh, p->hdr, ETH_ALEN*2) && l3proto == pa_ethh_l3proto(p->hdr)) {
         if (!PA_SESSION(pd, p->session_handle)->flushed)
            break;
      }
   }

   rcu_read_unlock();

   return p ? PA_SESSION(pd, p->session_handle) : NULL;
}


static struct avm_pa_bsession *
pa_bsession_alloc(struct avm_pa_pkt_match *match, avm_session_handle session_handle)
{
   struct avm_pa_global     *ctx  = &pa_glob;
   struct avm_pa_bsession   *p    = &ctx->bsess_array[session_handle];
   struct avm_pa_match_info *info = pa_find_eth_match(match);

   BUG_ON(!info);

   INIT_HLIST_NODE(&p->hash_list);
   p->hdr = (struct vlan_ethhdr *)HDRCOPY(match) + info->offset;
   p->hash = ethh_hash(p->hdr);
   p->session_handle = session_handle;
   ctx->stats.nbsessions++;

   return p;
}


static void pa_show_bsession(struct avm_pa_bsession *bsession,
                             pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global    *ctx = &pa_glob;
   struct avm_pa_session   *session = PA_SESSION(&pa_data, bsession->session_handle);
   struct avm_pa_egress    *egress;
   unsigned negress;

   (*fprintffunc)(arg, "Session        : %d\n", bsession->session_handle);
   (*fprintffunc)(arg, "In Pid         : %d (%s)\n",
                  session->ingress_pid_handle,
                  PA_PID(ctx, session->ingress_pid_handle)->cfg.name);
   (*fprintffunc)(arg, "Hash           : %lu\n", (unsigned long)bsession->hash);

   pa_show_pkt_match(&session->ingress, 1, session->mod.pkttype, fprintffunc, arg);

   /* In practice, negress is always 1, since multicast uses normal sessions */
   negress = 0;
   avm_pa_for_each_egress(egress, session) {
      (*fprintffunc)(arg, "Egress         : %d of %d\n", ++negress, session->negress);
      if (egress->pid_handle) {
         (*fprintffunc)(arg, "Out Pid        : %d (%s)\n",
                        egress->pid_handle,
                        PA_PID(ctx, egress->pid_handle)->cfg.name);
      }
      if (egress->vpid_handle) {
         (*fprintffunc)(arg, "Out VPid       : %d (%s)\n",
                        egress->vpid_handle,
                        PA_VPID(ctx, egress->vpid_handle)->cfg.name);
      }
      if (egress->destmac)
         pa_show_macaddr(egress->destmac, fprintffunc, arg);

      pa_show_vlan_match(&egress->match, NULL, fprintffunc, arg);
   }
}


/* ------------------------------------------------------------------------ */
/* -------- session management -------------------------------------------- */
/* ------------------------------------------------------------------------ */

/* Search for ACTIVE sessions */
#define pa_session_search(pid, match) pa_session_hash_search(pid, match)

static struct avm_pa_session *
pa_session_hash_search(struct avm_pa_pid *pid, struct avm_pa_pkt_match *ingress)
{
   struct avm_pa_session *p;
   u32 h = ingress->hash%AVM_PA_MAX_HASH;

   rcu_read_lock();
   hlist_for_each_entry_rcu(p, &pid->hash_sess[h], hash_list) {
      /* Don't consider flushed sessions */
      if (pa_match_eq(ingress, &p->ingress) && !p->flushed)
         break;
   }
   rcu_read_unlock();

   return p;
}

static void pa_session_hash_insert(struct avm_pa_pid *pid,
                                   struct avm_pa_session *session)
{
   struct avm_pa_bsession *bsession = session->bsession;
   u32 h = session->ingress.hash%AVM_PA_MAX_HASH;

   hlist_add_head_rcu(&session->hash_list, &pid->hash_sess[h]);
   if (bsession) {
      h = bsession->hash%AVM_PA_MAX_HASH;
      hlist_add_head_rcu(&bsession->hash_list, &pid->hash_bsess[h]);
   }
}

static void pa_session_hash_delete(struct avm_pa_pid *pid,
                                   struct avm_pa_session *session)
{
   struct avm_pa_bsession *bsession = session->bsession;

   hlist_del_init_rcu(&session->hash_list);
   if (bsession)
      hlist_del_init_rcu(&bsession->hash_list);
}

static void
pa_session_list_delete(struct avm_pa_session *session)
{
   if (session->on_list < AVM_PA_LIST_MAX) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_session_list *list = &ctx->sess_list[session->on_list];

      BUG_ON(list->nsessions == 0 || list_empty(&list->sessions));

      session->on_list = AVM_PA_LIST_MAX;
      list_del_rcu(&session->session_list);
      list->nsessions--;
   }
}


static void
pa_session_list_update(struct avm_pa_session *session, int which)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session_list *list = &ctx->sess_list[which];

   pa_session_list_delete(session);

   list->nsessions++;
   if (list->nsessions > list->maxsessions)
      list->maxsessions = list->nsessions;
   list_add_rcu(&session->session_list, &list->sessions);
   session->on_list = which;

   /* Ensure the GC timer runs if sessions are on any list (except FREE).
    * mod_timer() only if necessary, to maintain the ~0.5s interval even if 
    * sessions are constantly added or removed */
   if (which != AVM_PA_LIST_FREE && !timer_pending(&ctx->tick_timer))
      mod_timer(&ctx->tick_timer, jiffies + AVM_PA_TICK_RATE);
}

static void
pa_session_update(struct avm_pa_session *session)
{
   /* Update endtime regardless of the session state, the endtime is only relevant
    * in state ACTIVE (a previous BUG_ON() was regularly triggered, see JZ-43644).  */
   session->endtime = jiffies + session->timeout;
}

static int
pa_session_activate(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *ipid, *epid;
   struct avm_pa_session *s = NULL;

   ipid = PA_PID(ctx, session->ingress_pid_handle);
   epid = PA_PID(ctx, session->static_egress.pid_handle);

   spin_lock(&avm_pa_lock);
   /* Move to ACTIVE only if no "same session" exists and PIDs are ready to use.
    *
    * Session creation can happen concurrently, but after this call only one
    * session of a kind may exist (to avoid confusing hardware acceleration), so the
    * hash lookup finds if anyone else won the race.
    * PID deregistration can also happen concurrently. Therefore we need
    * check if pid->pid_handle is still valid (inside the lock). We don't
    * need a full reference because they are hold by the session.
    */
   if (session->bsession)
      s = pa_bsession_search(ipid, session->bsession->hash, session->bsession->hdr);
   else
      s = pa_session_hash_search(ipid, &session->ingress);

   if (ipid->pid_handle && epid->pid_handle && s == 0) {
      pa_session_hash_insert(ipid, session);
      pa_session_list_update(session, AVM_PA_LIST_ACTIVE);
      pa_session_update(session);
#ifdef CONFIG_AVM_GENERIC_CONNTRACK
      /* session->generic_ct is shared between sessions and access must be locked.
       * See comment at pa_session_kill_nolock(). */
      if (session->generic_ct) {
         generic_ct_sessionid_set(session->generic_ct,
                                  session->generic_ct_dir,
                                  (void *)(unsigned long)(session->session_handle));
      }
#endif
      /* The sessions is now permanent, so are the sessions references to the pids. */
   } else {
      /* Session wasn't on state ACTIVE yet, so it's safe to kill without flush.
       * This will release the session's references as well
       */
      pa_session_kill_nolock(session, s ? "lost creation race" : "pid gone");
   }

   spin_unlock(&avm_pa_lock);

   return s ? AVM_PA_TX_SESSION_EXISTS : AVM_PA_TX_SESSION_ADDED;
}


static void __init avm_pa_init_freelist(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;
   struct avm_pa_session_list *free_list = &ctx->sess_list[AVM_PA_LIST_FREE];
   int i;

   for (i = CONFIG_AVM_PA_MAX_SESSION - 1; i > 0; i--) {
      struct avm_pa_session *session = PA_SESSION(pd, i);
      list_add(&session->session_list, &free_list->sessions);
      session->on_list = AVM_PA_LIST_FREE;
   }
   free_list->maxsessions = free_list->nsessions = CONFIG_AVM_PA_MAX_SESSION - 1;

   for (i = ARRAY_SIZE(pd->egress_pool) - 1; i >= 0; i--) {
      struct avm_pa_egress *egress = &pd->egress_pool[i];
      hlist_add_head(&egress->egress_list, &ctx->egress_freelist);
   }
}

static struct avm_pa_session *pa_session_alloc(struct avm_pa_pkt_match *match)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;
   struct avm_pa_session *session;
   struct avm_pa_session_list *free_list = &ctx->sess_list[AVM_PA_LIST_FREE];

   session = NULL;
   spin_lock(&avm_pa_lock);
   if (!list_empty(&free_list->sessions)) {
      session = list_first_entry(&free_list->sessions, struct avm_pa_session, session_list);
      pa_session_list_delete(session);
      memset(session, 0, sizeof(struct avm_pa_session));
      INIT_HLIST_NODE(&session->hash_list);
      INIT_LIST_HEAD(&session->session_list);
      INIT_HLIST_HEAD(&session->egress_head);
      INIT_HLIST_HEAD(&session->groups);
      hlist_add_head_rcu(&session->static_egress.egress_list, &session->egress_head);
      session->negress = 1;
      session->session_handle = session - pd->sessions;
      session->on_list = AVM_PA_LIST_MAX;
      session->uniq_id = ctx->next_session_uniq_id++;
      session->ingress = *match;
      session->endtime = jiffies;
      switch (AVM_PA_PKTTYPE_IPPROTO(match->pkttype)) {
         case IPPROTO_TCP:
            session->timeout = ctx->tcp_timeout_secs*HZ;
            break;
         case IPPROTO_UDP:
         case IPPROTO_ESP:
            session->timeout = ctx->udp_timeout_secs*HZ;
            break;
         case IPPROTO_ICMPV6:
         case IPPROTO_ICMP:
            session->timeout = ctx->echo_timeout_secs*HZ;
            break;
      }
   }
   spin_unlock(&avm_pa_lock);

   return session;
}

static struct avm_pa_egress *
pa_egress_alloc(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_egress *egress;

   egress = NULL;
   spin_lock(&avm_pa_lock);
   if (!hlist_empty(&ctx->egress_freelist)) {
      egress = hlist_entry(hlist_first_rcu(&ctx->egress_freelist), struct avm_pa_egress, egress_list);
      hlist_del_rcu(&egress->egress_list);
      memset(egress, 0, sizeof(*egress));
      INIT_HLIST_NODE(&egress->egress_list);
   }
   spin_unlock(&avm_pa_lock);

   return egress;
}

void
pa_egress_free(struct avm_pa_egress *egress)
{
   struct avm_pa_global *ctx = &pa_glob;

   spin_lock(&avm_pa_lock);

   if (!hlist_unhashed(&egress->egress_list))
      hlist_del_rcu(&egress->egress_list);
   hlist_add_head_rcu(&egress->egress_list, &ctx->egress_freelist);

   spin_unlock(&avm_pa_lock);
}

static void
avm_pa_set_associated_session_handle(struct avm_pa_session *session)
{
#ifdef CONFIG_AVM_GENERIC_CONNTRACK
    if (session->generic_ct) {
       avm_session_handle handle;
       enum generic_ct_dir dir;

       if (session->generic_ct_dir == GENERIC_CT_DIR_ORIGINAL)
          dir = GENERIC_CT_DIR_REPLY;
       else dir = GENERIC_CT_DIR_ORIGINAL;

       if ((handle = (avm_session_handle)(unsigned long)generic_ct_sessionid_get(session->generic_ct, dir)) != 0) {
          struct avm_pa_session *asession = PA_SESSION(&pa_data, handle);
          if (avm_pa_session_valid(asession)) {
             session->associated_session_handle = handle;
             asession->associated_session_handle = session->session_handle;
          }
       }
    }
#endif
}

static void
avm_pa_unset_associated_session_handle(struct avm_pa_session *session)
{
   avm_session_handle handle;
   if ((handle = session->associated_session_handle) != 0) {
      struct avm_pa_session *asession = PA_SESSION(&pa_data, handle);
      if (avm_pa_session_valid(asession))
         asession->associated_session_handle = 0;
      session->associated_session_handle = 0;
   }
}

static void pa_show_session(struct avm_pa_session *session,
                            pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   char buf[max_t(size_t, KSYM_SYMBOL_LEN, 64ul)];
   struct avm_pa_macaddr *destmac;
   struct net_device *dev;
   struct dst_entry *dst;
   unsigned negress;
   struct avm_pa_egress *egress;

   (*fprintffunc)(arg, "Session        : %u (%d)\n", session->uniq_id, session->session_handle);

   {
      char *state;
      if (session->on_list < AVM_PA_LIST_MAX) {
         const char *why = session->why_killed ? session->why_killed : "???";
         switch (session->on_list) {
            case AVM_PA_LIST_ACTIVE: state = session->flushed ? "flushed" : "active"; break;
            case AVM_PA_LIST_DEAD: snprintf(buf, sizeof(buf), "dead (%s)", why); state = buf; break;
            case AVM_PA_LIST_FREE: state = "free"; break;
            default: state = "BAD STATE"; break;
         }
      } else {
         state = "create";
      }
      (*fprintffunc)(arg, "State          : %s\n", state);
   }

   (*fprintffunc)(arg, "In Pid         : %d (%s)\n",
                  session->ingress_pid_handle,
                  PA_PID(ctx, session->ingress_pid_handle)->cfg.name);

   if (session->ingress_vpid_handle) {
      (*fprintffunc)(arg, "In VPid        : %d (%s)\n",
                     session->ingress_vpid_handle,
                     PA_VPID(ctx, session->ingress_vpid_handle)->cfg.name);
   }

   if (pa_hw_pa_valid(&ctx->hardware_pa)) {
      if ((session->in_hw || avm_pa_get_hw_session(session)) && ctx->hardware_pa.session_state)
         (*fprintffunc)(arg, "In HW          : %s\n", (*ctx->hardware_pa.session_state)(session));
      else
         (*fprintffunc)(arg, "In HW          : %s\n", session->in_hw ? "yes" : "no");
   }
   (*fprintffunc)(arg, "suspicious     : %s\n", session->suspicious ? "yes" : "no");
   (*fprintffunc)(arg, "guilty         : %s\n", session->guilty ? "yes" : "no");

#ifdef CONFIG_AVM_GENERIC_CONNTRACK
   if (session->generic_ct) {
      if (session->generic_ct_dir == GENERIC_CT_DIR_ORIGINAL)
         (*fprintffunc)(arg, "CT dir         : original\n");
      else
         (*fprintffunc)(arg, "CT dir         : reply\n");
   }
#endif
   if (session->associated_session_handle) {
      (*fprintffunc)(arg, "Associated     : %d\n", session->associated_session_handle);
   }

   (*fprintffunc)(arg, "Realtime       : %s\n", session->realtime ? "yes" : "no");
#ifdef CONFIG_AVM_PA_RPS
   if (session->rps_cpu)
      (*fprintffunc)(arg, "RPS cpu        : %d\n", session->rps_cpu - 1);
#endif

   pa_show_pkt_match(&session->ingress,
                     session->bsession != 0, session->mod.pkttype,
                     fprintffunc, arg);

   pa_show_mod_rec(&session->mod, fprintffunc, arg);

   (*fprintffunc)(arg, "Hroom          : %u\n", (unsigned) session->needed_headroom);

   (*fprintffunc)(arg, "Timeout        : %hu\n", session->timeout/HZ);

   (*fprintffunc)(arg, "SW stats       : %lu pkts, %llu bytes\n",
                  (unsigned long)session->ingress_sw_stats.tx_pkts,
                  (unsigned long long)session->ingress_sw_stats.tx_bytes);

   (*fprintffunc)(arg, "HW stats       : %lu pkts, %llu bytes (validflags 0x%x)\n",
                  (unsigned long)session->ingress_hw_stats.tx_pkts,
                  (unsigned long long)session->ingress_hw_stats.tx_bytes,
                  session->ingress_hw_stats.validflags);

   negress = 0;
   avm_pa_for_each_egress(egress, session) {
      (*fprintffunc)(arg, "Egress         : %d of %d\n", ++negress, session->negress);
      if (egress->pid_handle) {
         (*fprintffunc)(arg, "Out Pid        : %d (%s)\n", egress->pid_handle,
                        PA_PID(ctx, egress->pid_handle)->cfg.name);
      }
      else {
         (*fprintffunc)(arg, "Egress under construction\n");
         continue;
      }
      if (egress->vpid_handle) {
         (*fprintffunc)(arg, "Out VPid       : %d (%s)\n", egress->vpid_handle,
                        PA_VPID(ctx, egress->vpid_handle)->cfg.name);
      }
      (*fprintffunc)(arg, "Mtu            : %u\n", (unsigned)egress->mtu);
      if (egress->push_l2_len) {
         data2hex(HDRCOPY(&egress->match), egress->push_l2_len,
                  buf, sizeof(buf));
         (*fprintffunc)(arg, "L2 push        : %s\n", buf);
         if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET) {
            (*fprintffunc)(arg, "PPPoE off      : %u\n", (unsigned)egress->pppoe_offset);
            (*fprintffunc)(arg, "PPPoE hlen     : %u\n", (unsigned)egress->pppoe_hdrlen);
         }
      }
      if ((destmac = egress->destmac) != 0)
         pa_show_macaddr(destmac, fprintffunc, arg);
      pa_show_pkt_match(&egress->match,
                        session->bsession != 0, session->mod.pkttype,
                        fprintffunc, arg);

      switch (egress->type) {
         case avm_pa_egresstype_output:
            {
               struct avm_pa_pid *pid = PA_PID(ctx, egress->pid_handle);
               (*fprintffunc)(arg, "Orig Prio      : %hx:%hx\n",
                              TC_H_MAJ(egress->output.orig_priority)>>16,
                              TC_H_MIN(egress->output.orig_priority));
               (*fprintffunc)(arg, "Prio           : %hx:%hx\n",
                              TC_H_MAJ(egress->output.priority)>>16,
                              TC_H_MIN(egress->output.priority));
               if (egress->output.tack_priority != egress->output.orig_priority) {
                  (*fprintffunc)(arg, "TACK prio      : %hx:%hx\n",
                                 TC_H_MAJ(egress->output.tack_priority)>>16,
                                 TC_H_MIN(egress->output.tack_priority));
               }
               (*fprintffunc)(arg, "TC index       : %hu\n", egress->output.tc_index);
#ifdef CONFIG_NET_CLS_ACT
               (*fprintffunc)(arg, "TC verd       : 0x%04x\n",
                              egress->output.tc_verd);
#endif
               if (avm_pa_pid_tack_enabled(pid)) {
                  (*fprintffunc)(arg, "tack pkts  : %u (accl acks %u)\n",
                                 pid->prioack_acks,
                                 pid->prioack_accl_acks);
               }
            }
            break;

         case avm_pa_egresstype_local:
            if ((dst = egress->local.dst) != 0) {
               sprint_symbol(buf, (unsigned long)dst->input);
               (*fprintffunc)(arg, "Dest           : %s\n", buf);
            } else {
               (*fprintffunc)(arg, "Dest           : <NOT SET>\n");
            }
            if ((dev = egress->local.dev) != 0) {
               (*fprintffunc)(arg, "Input Dev      : %s\n", dev->name);
            } else {
               (*fprintffunc)(arg, "Input Dev      : <NOT SET>\n");
            }
            break;
         case avm_pa_egresstype_rtp:
            sprint_symbol(buf, (unsigned long)egress->rtp.transmit);
            (*fprintffunc)(arg, "transmitfunc   : %s\n", buf);
            if ((dev = egress->rtp.dev) != 0) {
               (*fprintffunc)(arg, "Input Dev      : %s\n", dev->name);
            } else {
               (*fprintffunc)(arg, "Input Dev      : <NOT SET>\n");
            }
            break;
         case avm_pa_egresstype_xfrm:
            if (IS_ENABLED(CONFIG_XFRM)) {
               struct dst_entry *dst = egress->xfrm.dst;
               struct xfrm_state *x = egress->xfrm.x;

               (*fprintffunc)(arg, "TC index       : %hu\n", egress->xfrm.tc_index);
               (*fprintffunc)(arg, "XFRM dst       : %pf\n", dst->input);
               (*fprintffunc)(arg, "XFRM output    : %pf\n", x->type->output);
               if (x->props.family == AF_INET) {
                  (*fprintffunc)(arg, "XFRM saddr     : %pI4\n", &x->props.saddr.a4);
                  (*fprintffunc)(arg, "XFRM daddr     : %pI4\n", &x->id.daddr.a4);
               } else if (x->props.family == AF_INET6) {
                  (*fprintffunc)(arg, "XFRM saddr     : %pI6\n", &x->props.saddr.a6);
                  (*fprintffunc)(arg, "XFRM daddr     : %pI6\n", &x->id.daddr.a6);
               } else {
                  (*fprintffunc)(arg, "XFRM saddr     : ??? (family %d)\n", x->props.family);
                  (*fprintffunc)(arg, "XFRM daddr     : ??? (proto %d)\n", x->id.proto);
               }
               (*fprintffunc)(arg, "XFRM spi       : 0x%08x\n", ntohl(x->id.spi));
            }
            break;
      }
      (*fprintffunc)(arg, "SW stats       : %lu pkts, %llu bytes\n",
                     (unsigned long)egress->sw_stats.tx_pkts,
                     (unsigned long long)egress->sw_stats.tx_bytes);
      (*fprintffunc)(arg, "HW stats       : %lu pkts, %llu bytes\n",
                     (unsigned long)egress->hw_stats.tx_pkts,
                     (unsigned long long)egress->hw_stats.tx_bytes);
      (*fprintffunc)(arg, "Pkts           : TX %lu (acks %lu)\n",
                     (unsigned long)egress->tx_pkts,
                     (unsigned long)egress->tcpack_pkts);
   }

   avm_pa_sg_show_session(session, fprintffunc, arg);
}


static void
pa_session_delete_rcu(struct rcu_head *head)
{
   struct avm_pa_session *session = container_of(head, struct avm_pa_session, kill_rcu);
   struct avm_pa_egress  *egress;
   struct hlist_node *tmp;

   /* Being inside rcu callback, we don't need _rcu traversal. Instead,
    * we need _safe traversal since egress are removed inside the loop. */
   hlist_for_each_entry_safe(egress, tmp, &session->egress_head, egress_list) {
      if (egress->destmac) {
         pa_macaddr_unlink(egress->destmac);
         egress->destmac = 0;
      }
      if (egress->type == avm_pa_egresstype_output) {
         if (egress->output.dst) {
            dst_release(egress->output.dst);
            egress->output.dst = 0;
         }
      }
      if (egress->type == avm_pa_egresstype_local) {
         if (egress->local.dst) {
            dst_release(egress->local.dst);
            egress->local.dst = 0;
         }
      }
      if (egress->type == avm_pa_egresstype_rtp) {
         if (egress->rtp.sk) {
            sock_put(egress->rtp.sk);
            egress->rtp.sk = 0;
         }
      }
      if (IS_ENABLED(CONFIG_XFRM) && egress->type == avm_pa_egresstype_xfrm) {
         dst_release(egress->xfrm.dst);
         xfrm_state_put(egress->xfrm.x);
         dev_put(egress->xfrm.dev);
      }
      pa_pid_put(egress->pid_handle);
      if (egress != &session->static_egress)
         pa_egress_free(egress);
   }
   pa_pid_put(session->ingress_pid_handle);

   spin_lock(&avm_pa_lock);
   pa_session_list_update(session, AVM_PA_LIST_FREE);
   spin_unlock(&avm_pa_lock);
}

static void pa_session_delete(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   const char *why = session->why_killed ? session->why_killed : "???";

   if (ctx->dbgsession) {
      pa_printk(KERN_DEBUG, "\navm_pa: delete session: %s\n", why);
      pa_show_session(session, pa_printk, KERN_DEBUG);
   }

   BUG_ON(session->on_list != AVM_PA_LIST_DEAD);
   pa_session_list_delete(session);

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      struct avm_pa_pid *pid = PA_PID(ctx, session->ingress_pid_handle);
      pa_printk(KERN_DEBUG, "avm_pa: delete session %d (%s) %s\n",
                            session->session_handle, pid->cfg.name, why);
   }
#endif
   /*
    * pa_session_kill() has
    * - removed session from hash
    * - removed session from hardware pa
    * - removed session from generic connection tracking
    */
   BUG_ON(!hlist_unhashed(&session->hash_list));
   BUG_ON(session->bsession && !hlist_unhashed(&session->bsession->hash_list));
   BUG_ON(session->in_hw);
#ifdef CONFIG_AVM_GENERIC_CONNTRACK
   BUG_ON(session->generic_ct);
#endif

   /* There may be packets in-flight at this point. Defer work that prevents
    * transmission of such packets. */
   call_rcu_bh(&session->kill_rcu, pa_session_delete_rcu);
}


static void
pa_session_kill_rcu(struct rcu_head *head)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session = container_of(head, struct avm_pa_session, kill_rcu);

   if (session->in_hw && ctx->hardware_pa.remove_session) {
      session->in_hw = 0;
      (*ctx->hardware_pa.remove_session)(session);
      pa_hw_pa_put();
   }

#ifdef CONFIG_AVM_GENERIC_CONNTRACK
   if (session->generic_ct) {
      struct generic_ct *ct = session->generic_ct;
      avm_session_handle handle;

      /* session->generic_ct is shared between sessions and access must be locked.
       * A newer session may overwrite the sessionid while this session was in FLUSHED
       * state, so only reset sessionid if we still own it. */
      session->generic_ct = 0;
      handle = (avm_session_handle)(unsigned long)generic_ct_sessionid_get(ct, session->generic_ct_dir);
      if (handle == session->session_handle)
        generic_ct_sessionid_set(ct, session->generic_ct_dir, NULL);
      generic_ct_put(ct);
   }
#endif

   /*
    * all packets that were in-flight in pa_session_kill()
    * should be counted here.
    */
   avm_pa_sg_session_unlink(session);

   spin_lock(&avm_pa_lock);
   pa_session_list_update(session, AVM_PA_LIST_DEAD);
   spin_unlock(&avm_pa_lock);

}

static void
pa_session_kill_nolock(struct avm_pa_session *session, const char *why)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, session->ingress_pid_handle);

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      pa_printk(KERN_DEBUG, "avm_pa: kill session %d (%s) %s\n",
                            session->session_handle, pid->cfg.name, why);
   }
#endif
   if (ctx->dbgsession) {
      pa_printk(KERN_DEBUG, "\navm_pa: kill session: %s\n", why);
      if (session->bsession)
         pa_show_bsession(session->bsession, pa_printk, KERN_DEBUG);
      else
         pa_show_session(session, pa_printk, KERN_DEBUG);
   }

   pa_session_list_delete(session);
   pa_session_hash_delete(pid, session);

   avm_pa_unset_associated_session_handle(session);
   session->why_killed = why;

   /* There may be packets in-flight at this point. Defer work that prevents
    * transmission of such packets. */
   call_rcu_bh(&session->kill_rcu, pa_session_kill_rcu);
}

static void pa_session_kill(struct avm_pa_session *session,
                                   const char *why)
{
   spin_lock(&avm_pa_lock);
   pa_session_kill_nolock(session, why);
   spin_unlock(&avm_pa_lock);
}

static void
pa_session_flush(struct avm_pa_session *session, const char *why)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid;

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      pid = PA_PID(ctx, session->ingress_pid_handle);
      pa_printk(KERN_DEBUG, "avm_pa: flush session %d (%s) %s\n",
                            session->session_handle, pid->cfg.name, why);
   }
#endif
   if (ctx->dbgsession) {
      pa_printk(KERN_DEBUG, "\navm_pa: flush session: %s\n", why);
      pa_show_session(session, pa_printk, KERN_DEBUG);
   }

   session->flushed = 1; /* will be killed on next gc */
   session->why_killed = why;
}


/* ------------------------------------------------------------------------ */
/* -------- wall clock ---------------------------------------------------- */
/* ------------------------------------------------------------------------ */

static void pa_session_prioack_check(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_egress *egress = avm_pa_first_egress(session);
   unsigned int oldprio = egress->output.priority;
   int (*add_session)(struct avm_pa_session *avm_session);

   if (egress->tx_pkts > ctx->prioack_thresh_packets) {
      /*
       * Stop using TGET priority.
       * We need to decide if we use TACK priority or restore original priority.
       * (TACK priority is same as original priority, if TACK is not enabled)
       *   2016-10-14 calle
       */
      unsigned long percent_ack = (egress->tcpack_pkts * 100) / egress->tx_pkts;
      int switched_to_tack = 0;
      if (ctx->dbgprioack) {
         pa_printk(KERN_DEBUG, "avm_pa: session %d: %lu%% TCP-ACKs (%u pkts %u ACKs) \n",
                               session->session_handle,
                               percent_ack, egress->tx_pkts, egress->tcpack_pkts);
      }
      if (percent_ack > ctx->prioack_ratio) {
         egress->output.priority = egress->output.tack_priority;
         switched_to_tack = 1;
      } else {
         session->no_hw = 0; /* revert sch_tack decision */
         egress->output.priority = egress->output.orig_priority;
      }

      add_session = rcu_dereference(ctx->hardware_pa.add_session);
      if (add_session && !ctx->hw_ppa_disabled && !session->no_hw && pa_hw_pa_get()) {
         if (add_session(session) == AVM_PA_TX_SESSION_ADDED)
            session->in_hw = 1;
         else
            pa_hw_pa_put();
      }

      if (ctx->dbgprioack) {
         pa_printk(KERN_DEBUG, "avm_pa: session %d: priority %x:%x %s (old %x:%x)\n",
                               session->session_handle,
                               TC_H_MAJ(egress->output.priority)>>16,
                               TC_H_MIN(egress->output.priority),
                               switched_to_tack ? "TACK" : "NORMAL",
                               TC_H_MAJ(oldprio)>>16,
                               TC_H_MIN(oldprio));
      }

      session->prioack_check = 0;
   }
}

static void pa_session_stats_get_diff(u32 *pkts, u64 *bytes,
                                      struct avm_pa_session_stats *last,
                                      struct avm_pa_session_stats *now)
{
   *pkts = now->tx_pkts - last->tx_pkts;
   last->tx_pkts = now->tx_pkts;
   *bytes = now->tx_bytes - last->tx_bytes;
   last->tx_bytes = now->tx_bytes;
}

static inline unsigned int pa_get_priority(unsigned int prio)
{
   prio &= TC_H_MIN_MASK;
   if (prio >= AVM_PA_MAX_PRIOS)
      prio = AVM_PA_MAX_PRIOS-1;
   return prio;
}

static inline unsigned int
pa_get_egress_priority(struct avm_pa_egress *egress)
{
   return pa_get_priority(egress->output.priority);
}

static inline unsigned int
pa_get_ingress_priority(struct avm_pa_session *session)
{
   /*
    * Ensure that the returned ingress priority is always in the range
    * [0, AVM_PA_MAX_PRIOS-1], otherwise Klocwork will complain if
    * the ingress priority is used as index to the VPID ingress priority
    * statistics array.
    */
   return pa_get_priority(session->ingress_priority);
}

static inline unsigned int
pa_get_ingress_priority_from_pkt_mark(u32 pkt_mark)
{
   /*
    * Consider only networks for now, which are encoded as the two
    * most significant bytes.
    */
   unsigned int prio = AVM_PA_INGRESS_PRIO_NET(pkt_mark);
   if (prio >= AVM_PA_MAX_PRIOS) {
      prio = AVM_PA_MAX_PRIOS-1;
   }
   return prio;
}

/* ------------------------------------------------------------------------ */

static void pa_session_check_pa(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;

   if (session->in_hw && ctx->hardware_pa.check_session) {
      unsigned ret = ctx->hardware_pa.check_session(session);

      /* Warn on unknown return codes, indicates too old avm_pa tag */
      WARN_ON_ONCE(ret & ~AVM_HW_CHK_FLUSH);
      if (ret == AVM_HW_CHK_FLUSH)
         pa_session_flush(session, "void by hw");
   }
}

static int pa_session_handle_stats(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;
   struct avm_pa_pid    *pid;
   struct avm_pa_vpid   *vpid;
   struct avm_pa_egress *egress;
   struct avm_pa_session_stats stats;
   u64 bytes, hw_bytes;
   u32 pkts, hw_pkts;
   unsigned validflags;

   stats.validflags = 0;
   pa_session_stats_get_diff(&pkts, &bytes, &session->ingress_last_sw_stats,
                                &session->ingress_sw_stats);
   if (   session->in_hw == 0
       || ctx->hardware_pa.session_stats == 0
       || (*ctx->hardware_pa.session_stats)(session, &stats) != 0) {
      validflags = 0;
   } else {
      validflags = stats.validflags;
   }
   if (validflags & AVM_PA_SESSION_STATS_VALID_PKTS)
      hw_pkts = stats.tx_pkts;
   else
      hw_pkts = 0;
   if (validflags & AVM_PA_SESSION_STATS_VALID_BYTES)
      hw_bytes = stats.tx_bytes;
   else
      hw_bytes = 0;

   if (ctx->dbgstats && validflags)
      pr_debug("session %d valid 0x%x, %lu/%lu pkts, %llu/%llu bytes\n",
               session->session_handle,
               validflags,
               (unsigned long)pkts, (unsigned long)hw_pkts,
               bytes, hw_bytes);

   session->ingress_hw_stats.tx_pkts += hw_pkts;
   session->ingress_hw_stats.tx_bytes += hw_bytes;
   session->ingress_hw_stats.validflags |= validflags;

   if (session->ingress_vpid_handle) {
      struct avm_pa_session *asession = PA_SESSION(pd, session->associated_session_handle);
      unsigned int aprio = pa_get_ingress_priority(asession);
      unsigned int prio = pa_get_ingress_priority(session);

      vpid = PA_VPID(ctx, session->ingress_vpid_handle);
      ((u32 *)(&vpid->stats.rx_unicast_pkt))[session->ingress.casttype] += pkts + hw_pkts;
      ((u64 *)(&vpid->stats.rx_bytes))[session->ingress.casttype] += bytes + hw_bytes;
      vpid->ingress_sw_stats[prio].pkts += pkts;
      vpid->ingress_sw_stats[prio].bytes += bytes;
      vpid->ingress_hw_stats[prio].pkts += hw_pkts;
      vpid->ingress_hw_stats[prio].bytes += hw_bytes;
 
      vpid = PA_VPID(ctx, asession->ingress_vpid_handle);
      vpid->associated_ingress_sw_stats[aprio].pkts += pkts;
      vpid->associated_ingress_sw_stats[aprio].bytes += bytes;
      vpid->associated_ingress_hw_stats[aprio].pkts += hw_pkts;
      vpid->associated_ingress_hw_stats[aprio].bytes += hw_bytes;
   }

   avm_pa_for_each_egress(egress, session) {
      unsigned int prio = pa_get_egress_priority(egress);

      egress->hw_stats.tx_pkts += hw_pkts;
      egress->hw_stats.tx_bytes += hw_bytes;

      if (egress->pid_handle) {
         pid = PA_PID(ctx, egress->pid_handle);
         pid->tx_pkts += pkts + hw_pkts;
      }

      if (egress->vpid_handle) {
         vpid = PA_VPID(ctx, egress->vpid_handle);
         ((u32 *)(&vpid->stats.tx_unicast_pkt))[egress->match.casttype] += pkts + hw_pkts;
         vpid->stats.tx_bytes += bytes + hw_bytes;
         vpid->sw_stats[prio].pkts += pkts;
         vpid->sw_stats[prio].bytes += bytes;
         vpid->hw_stats[prio].pkts += hw_pkts;
         vpid->hw_stats[prio].bytes += hw_bytes;
      }
      if (session->associated_session_handle) {
         struct avm_pa_session *asession;
         struct avm_pa_egress  *aegress;
         asession = PA_SESSION(pd, session->associated_session_handle);
         avm_pa_for_each_egress(aegress, asession) {
            unsigned int aprio = pa_get_egress_priority(aegress);
            vpid = PA_VPID(ctx, aegress->vpid_handle);
            vpid->associated_sw_stats[aprio].pkts += pkts;
            vpid->associated_sw_stats[aprio].bytes += bytes;
            vpid->associated_hw_stats[aprio].pkts += hw_pkts;
            vpid->associated_hw_stats[aprio].bytes += hw_bytes;
         }
      }
   }
   return validflags != 0;
}


static void
pa_tick_collect_slow_stats(void)
{
   struct avm_pa_vpid *vpid;
   avm_vpid_handle vpid_handle;
   ktime_t timestamp;
   int i;

   timestamp = ktime_get_boottime(); 
   for (vpid_handle = 1; vpid_handle < CONFIG_AVM_PA_MAX_VPID; ++vpid_handle) {
      if ((vpid = avm_pa_vpid_get_vpid(vpid_handle))) {
         write_lock(&vpid->slow_stats_lock);
#define ADD_COUNTER(field) (vpid->stats.field += vpid->slow_stats.field)
         ADD_COUNTER(rx_unicast_pkt);
         ADD_COUNTER(rx_multicast_pkt);
         ADD_COUNTER(rx_broadcast_pkt);
         ADD_COUNTER(rx_bytes);
         ADD_COUNTER(rx_multicast_bytes);
         ADD_COUNTER(rx_broadcast_bytes);
         ADD_COUNTER(rx_discard);
         ADD_COUNTER(tx_unicast_pkt);
         ADD_COUNTER(tx_multicast_pkt);
         ADD_COUNTER(tx_broadcast_pkt);
         ADD_COUNTER(tx_bytes);
         ADD_COUNTER(tx_error);
         ADD_COUNTER(tx_discard);
#undef ADD_COUNTER
         for (i = 0; i < AVM_PA_MAX_PRIOS; i++) {
            vpid->sw_stats[i].bytes += vpid->slow_sw_stats[i].bytes;
            vpid->sw_stats[i].pkts += vpid->slow_sw_stats[i].pkts;
            vpid->ingress_sw_stats[i].bytes += vpid->ingress_slow_sw_stats[i].bytes;
            vpid->ingress_sw_stats[i].pkts += vpid->ingress_slow_sw_stats[i].pkts;
         }
         /* Clear slow_stats, because they were merged with the accelerated sw stats */
         memset(&vpid->slow_stats, 0, sizeof(struct avm_pa_vpid) - offsetof(struct avm_pa_vpid, slow_stats));
         write_unlock(&vpid->slow_stats_lock);
         vpid->stats.timestamp = timestamp;
         avm_pa_vpid_put(vpid_handle);
      }
   }
}


static void
pa_tick_sessions(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   struct avm_pa_session_list *list = &ctx->sess_list[AVM_PA_LIST_ACTIVE];
   ktime_t now = ktime_get_boottime();

   /* Collect stats for all sessions, detecting possible timeouts in hardware.
    * This runs lockless. */
   rcu_read_lock();
   list_for_each_entry_rcu(session, &list->sessions, session_list) {
      session->stats_timestamp = now;
      pa_session_check_pa(session);
      if (pa_session_handle_stats(session))
         pa_session_update(session);
      if (session->prioack_check)
         pa_session_prioack_check(session);
   }
   rcu_read_unlock();
}


static void
pa_tick_session_gc_nolock(int force)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data *pd __maybe_unused = &pa_data;
   struct avm_pa_session *session, *next;
   struct avm_pa_session_list *list;
   struct avm_pa_l2tp *l2tp __maybe_unused;
   int i __maybe_unused;

   if (force) {
      list = &ctx->sess_list[AVM_PA_LIST_ACTIVE];
      list_for_each_entry_safe(session, next, &list->sessions, session_list) {
         pa_session_kill_nolock(session, "disable");
         ctx->stats.sess_flushed++;
      }
   }

   list = &ctx->sess_list[AVM_PA_LIST_DEAD];
   list_for_each_entry_safe(session, next, &list->sessions, session_list) {
      if (avm_pa_get_hw_session(session) == NULL) {
         pa_session_delete(session);
      }
   }

   list = &ctx->sess_list[AVM_PA_LIST_ACTIVE];
   list_for_each_entry_safe(session, next, &list->sessions, session_list) {
      if (session->flushed) {
         pa_session_kill_nolock(session, session->why_killed);
      }
      else if (time_is_before_eq_jiffies(session->endtime)) {
         /* flush in case a packet is received right now on another CPU,
          * killing immediately is racy. */
         pa_session_flush(session, session->timeout ? "timeout" : "fin");
         ctx->stats.sess_timedout++;
      }
   }

#ifdef CONFIG_L2TP
   for (i = 0; i < ARRAY_SIZE(pd->l2tp_cache); i++) {
      struct l2tp_session *local_sess;
      l2tp = &pd->l2tp_cache[i];
      /* We only clear the cache entry for now.
       * TODO: Maybe clear out corresponding sessions to truly stop forwarding */
      local_sess = pa_l2tp_session_get_local(l2tp->session_id);
      if (local_sess == NULL)
         l2tp->session_id = 0;
      else
         pa_l2tp_session_put_local(local_sess);
   }
#endif
}


static unsigned long last_tick;

static void
pa_session_tick(unsigned long force)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned long next_tick;

   /* Minimize timer temporal drift */
   next_tick = jiffies + AVM_PA_TICK_RATE;
   last_tick = jiffies;

   pa_tick_sessions();
   pa_tick_collect_slow_stats();

   spin_lock(&avm_pa_lock);
   pa_tick_session_gc_nolock(force);

   /* The tick_timer is only necessary as long as there are any sessions */
   if (   ctx->sess_list[AVM_PA_LIST_ACTIVE].nsessions
       || ctx->sess_list[AVM_PA_LIST_DEAD].nsessions)
      mod_timer(&ctx->tick_timer, next_tick);

   spin_unlock(&avm_pa_lock);
}


static void
pa_session_gc_once(void)
{
   spin_lock_bh(&avm_pa_lock);

   pa_tick_session_gc_nolock(1);

   spin_unlock_bh(&avm_pa_lock);
}

/*------------------------------------------------------------------------ */

static void avm_pa_tbf_schedule(psched_time_t wtime)
{
   struct avm_pa_global *ctx = &pa_glob;
   int ret;
   ktime_t time;

   ret = hrtimer_try_to_cancel(&ctx->tbf.timer);
   if (ret < 0)
      return; /* currently running => tasklet will run anyway, do nothing */

   /* we never wait a second */
   time = ktime_set(0, PSCHED_TICKS2NS(wtime));
   hrtimer_start(&ctx->tbf.timer, time, HRTIMER_MODE_REL);
   if (ret == 0) /* not running => start */
      ctx->stats.tbf_schedule++;
   else /* was scheduled => restart */
      ctx->stats.tbf_reschedule++;

}

static int avm_pa_tbf_tx_ok(u32 wanted)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_tbf *q = &ctx->tbf;
   psched_time_t now;
   long toks;
   long ptoks;
   long pkttime = q->pkttime;
   u32 count = 0;

   now = psched_get_time();
   toks = psched_tdiff_bounded(now, q->t_c, q->buffer);
   // toks = now - q->t_c;

   ptoks = toks + q->ptokens;
   if (ptoks > (long)q->pbuffer)
      ptoks = q->pbuffer;

   toks += q->tokens;
   if (toks > (long)q->buffer)
      toks = q->buffer;

   while (   count < wanted
          && ((toks - pkttime) >= 0 || (ptoks - pkttime) >= 0)) {
      ptoks -= pkttime;
      toks -= pkttime;
      count++;
   }

   if (count) {
      q->t_c = now;
      q->tokens = toks;
      q->ptokens = ptoks;
      return count;
   }
   avm_pa_tbf_schedule(max_t(long, -toks, -ptoks));
   return 0;
}

static inline u32 calc_xmittime(unsigned rate, unsigned size)
{
   u64 x64 = NSEC_PER_SEC*(u64)size;
   do_div(x64, rate);
   return (u32)(PSCHED_NS2TICKS((u32)x64));
}

static void avm_pa_tbf_reset(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_tbf *q = &ctx->tbf;
   q->t_c = psched_get_time();
   q->tokens = q->buffer;
   q->ptokens = q->pbuffer;
}

static void avm_pa_tbf_disable(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   ctx->tbf_enabled = 0;
   avm_pa_tbf_reset();
   if (skb_queue_len(&ctx->tbfqueue))
      tasklet_hi_schedule(&ctx->tbftasklet);
}

static void avm_pa_tbf_update(u32 rate, unsigned buffer, unsigned peak)
{
   struct avm_pa_global *ctx = &pa_glob;
   ctx->tbf.buffer = calc_xmittime(rate, buffer);
   ctx->tbf.pbuffer = calc_xmittime(rate, peak);
   ctx->tbf.pkttime = calc_xmittime(rate, 1);
}

static enum hrtimer_restart avm_pa_tbf_restart(struct hrtimer *timer)
{
   struct avm_pa_global *ctx = &pa_glob;
   tasklet_hi_schedule(&ctx->tbftasklet);
   return HRTIMER_NORESTART;
}

static void avm_pa_tbf_init(u32 rate, unsigned buffer, unsigned peak)
{
   avm_pa_tbf_update(rate, buffer, peak);
   avm_pa_tbf_reset();
}

static void avm_pa_tbf_exit(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct hrtimer *timer = &ctx->tbf.timer;
   hrtimer_cancel(timer);
}

static void avm_pa_tbf_tasklet(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct sk_buff *skb;

   if (ctx->tbf_enabled) {
      u32 len;
      if ((len = skb_queue_len(&ctx->tbfqueue)) > 0) {
         len = avm_pa_tbf_tx_ok(len);
         while (len--) {
            skb = skb_dequeue(&ctx->tbfqueue);
            pa_do_modify_and_send(NULL, skb);
         }
      }
   } else {
      while ((skb = skb_dequeue(&ctx->tbfqueue)) != 0) {
         pa_do_modify_and_send(NULL, skb);
      }
   }
}

static inline void avm_pa_tbf_transmit(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;

   /* Set the session_handle to be sure, although it's not always used during transmit. */
   AVM_PKT_INFO(pkt)->session_handle  = session->session_handle;
   AVM_PKT_INFO(pkt)->session_uniq_id = session->uniq_id;

   if (session->realtime) {
      pa_do_modify_and_send(session, pkt);
      return;
   }

   if (   skb_queue_len(&ctx->tbfqueue) == 0
       && (ctx->tbf_enabled == 0 || avm_pa_tbf_tx_ok(1))) {
      pa_do_modify_and_send(session, pkt);
      return;
   }

   skb_queue_tail(&ctx->tbfqueue, pkt);
   if (ctx->tbf_enabled) {
      ctx->stats.rx_overlimit++;
      if (skb_queue_len(&ctx->tbfqueue) > AVM_PA_MAX_TBF_QUEUE_LEN) {
         if ((pkt = skb_dequeue(&ctx->tbfqueue)) != 0) {
            PKT_FREE(pkt);
            ctx->stats.rx_dropped++;
         }
      }
   }
   if (!hrtimer_active(&ctx->tbf.timer))
      tasklet_hi_schedule(&ctx->tbftasklet);
}

/* ------------------------------------------------------------------------ */

#ifdef CONFIG_AVM_PA_RPS

static void
pa_rps_dequeue_task(unsigned long data)
{
   struct avm_pa_rps    *rps = (struct avm_pa_rps *) data;
   struct sk_buff       *skb;
   struct sk_buff_head   list;

   /* Use temporary list which can be processed lockless */
   __skb_queue_head_init(&list);

   /* q_local is only filled in avm_pa_rps_transmit() on the same cpu. No
    * locking needed because it cannot run at the same time (is a softirq too).
    */
   skb_queue_splice_init(&rps->q_local, &list);

   /* q_other is filled by other cores and access must be locked. */
   if (!skb_queue_empty(&rps->q_other)) {
      spin_lock(&rps->q_other.lock);
      skb_queue_splice_init(&rps->q_other, &list);
      spin_unlock(&rps->q_other.lock);
   }

   rps->rx_dequeued++;
   while ((skb = __skb_dequeue(&list))) {
      pa_do_modify_and_send(NULL, skb);
   }
}

static void
pa_rps_ipi_task(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_rps    *rps = (struct avm_pa_rps *) data;
   int                   tcpu = rps - ctx->rps;

   rps->rx_rps_ipis++;
   /* This eventually calls pa_rps_dequeue_task() above through a
    * tasklet on another CPU.
    * Carefully avoid issuing an ipi if there is one in-flight already,
    * in this case the async call would block, risking a dead lock.
    * smp_call_function_single_async() sets csd.flags to CSD_FLAG_LOCK
    * internally to detect repeated calls itself, so we just re-use that
    * instead of maintaining our own guard. */
   if (rps->csd.flags == 0)
      smp_call_function_single_async(tcpu, &rps->csd);
}

static inline void
avm_pa_rps_transmit(struct avm_pa_session *session, struct sk_buff *skb)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_rps    *rps;
   u32                   tcpu;

   if (session->realtime || !ctx->rps_enabled || AVM_PKT_INFO(skb)->rps_done) {
      avm_pa_tbf_transmit(session, skb);
      return;
   }

   /* Set the session_handle to be sure, although it's not always used during transmit. */
   AVM_PKT_INFO(skb)->session_handle  = session->session_handle;
   AVM_PKT_INFO(skb)->session_uniq_id = session->uniq_id;
   /* Don't do RPS twice, e.g. if there are two sessions for a packet. */
   AVM_PKT_INFO(skb)->rps_done        = 1;

   /* Select CPU via session hash, giving good enough distribution (hopefully) */
   if (!session->rps_cpu)
      tcpu = AVM_PKT_INFO(skb)->match.hash & (CONFIG_AVM_PA_RPS_QUEUES-1);
   else
      tcpu = session->rps_cpu - 1;
   /* Ensure new CPU is online and usable. */
   tcpu = cpumask_next(tcpu-1, cpu_online_mask);
   if (unlikely(tcpu >= min(CONFIG_AVM_PA_RPS_QUEUES, nr_cpu_ids)))
      tcpu = cpumask_first(cpu_online_mask);

   BUG_ON(tcpu >= CONFIG_AVM_PA_RPS_QUEUES);
   rps = &ctx->rps[tcpu];

   rps->rx_enqueued++;
   if (tcpu == smp_processor_id()) {
      __skb_queue_tail(&rps->q_local, skb);
   } else {
      spin_lock(&rps->q_other.lock);
      __skb_queue_tail(&rps->q_other, skb);
      spin_unlock(&rps->q_other.lock);
   }
   /* IPIs are relatively expensive. Hold IPIs up until there is
    * a sufficient number of packets queued up. This comes automatically
    * by deferring via tasklet. */
   tasklet_schedule(&rps->ipi_task);
}
#endif

/* ------------------------------------------------------------------------ */

#define MAX_TASKLET_PACKETS 32
static void avm_pa_irq_tasklet(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   int count = MAX_TASKLET_PACKETS;
   struct sk_buff *skb;

   rcu_read_lock();
   while (count-- > 0 && (skb = skb_dequeue(&ctx->irqqueue)) != 0) {
      struct avm_pa_session *session;
      session = pa_session_get(AVM_PKT_INFO(skb)->session_handle);
      /* Shouldn't happen but better play safe. */
      if (session && session->uniq_id == AVM_PKT_INFO(skb)->session_uniq_id) {
#ifdef CONFIG_AVM_PA_RPS
         avm_pa_rps_transmit(session,  skb);
#else
         avm_pa_tbf_transmit(session, skb);
#endif
      } else {
         ctx->stats.fw_drop_gone++;
         PKT_FREE(skb);
      }
      if (AVM_PKT_INFO(skb)->l2tp_session_id != 0) {
         /* Just populate the cache, don't inspect packet again */
         pa_l2tp_session_alloc(AVM_PKT_INFO(skb)->l2tp_session_id);
         AVM_PKT_INFO(skb)->l2tp_session_id = 0;
      }
   }
   rcu_read_unlock();

   if (skb_queue_len(&ctx->irqqueue))
      tasklet_schedule(&ctx->irqtasklet);
}
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void avm_pa_rx_channel_suspend(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   pid->rx_channel_stopped = 1;
}
EXPORT_SYMBOL(avm_pa_rx_channel_suspend);

void avm_pa_rx_channel_resume(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   pid->rx_channel_stopped = 0;
}
EXPORT_SYMBOL(avm_pa_rx_channel_resume);

void avm_pa_rx_channel_packet_not_accelerated(avm_pid_handle pid_handle,
                                           struct sk_buff *skb)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (avm_pa_pid_receive(pid_handle, skb) == AVM_PA_RX_ACCELERATED)
      return;

   if (likely(pid && pid->ecfg.rx_slow)) {
      (*pid->ecfg.rx_slow)(pid->ecfg.rx_slow_arg, skb);
      return;
   }
   PKT_FREE(skb);
   ctx->stats.rx_channel_no_rx_slow++;
}
EXPORT_SYMBOL(avm_pa_rx_channel_packet_not_accelerated);

void avm_pa_tx_channel_accelerated_packet(avm_pid_handle pid_handle,
                                          avm_session_handle session_handle,
                                          struct sk_buff *skb)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   struct avm_pa_egress  *egress;

   rcu_read_lock();

   session = pa_session_get(session_handle);
   if (session && session->on_list == AVM_PA_LIST_ACTIVE) {

      avm_pa_for_each_egress(egress, session) {
         /* Set some important skb fields, as pa_do_modify_non_l2() would have done */
         if (egress->pid_handle == pid_handle) {
            skb->protocol = session->mod.protocol; // 2016-03-01, calle
            skb_reset_mac_header(skb);
            if (session->bsession == 0) {
               skb_pull(skb, ETH_HLEN); /* Is ETH_HLEN correct for pppoe egress? */
               skb_reset_network_header(skb);
               if (skb->protocol == constant_htons(ETH_P_IP))
                  skb_set_transport_header(skb, session->mod.v4_mod.iphlen);
               else if (skb->protocol == constant_htons(ETH_P_IPV6))
                  skb_set_transport_header(skb, sizeof(struct ipv6hdr));
            }
            AVM_PKT_INFO(skb)->already_modified = 1;
            AVM_PKT_INFO(skb)->forced_egress = egress;
            avm_pa_tbf_transmit(session, skb);
            goto out;
         }
      }
   }

   ctx->stats.tx_channel_dropped++;
   PKT_FREE(skb);

out:
   rcu_read_unlock();
}
EXPORT_SYMBOL(avm_pa_tx_channel_accelerated_packet);

/* ------------------------------------------------------------------------ */
/* -------- exported functions -------------------------------------------- */
/* ------------------------------------------------------------------------ */

int avm_pa_is_enabled(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   return !ctx->disabled;
}
EXPORT_SYMBOL(avm_pa_is_enabled);

void avm_pa_get_stats(struct avm_pa_stats *stats)
{
   struct avm_pa_global *ctx = &pa_glob;
   memcpy(stats, &ctx->stats, sizeof(struct avm_pa_stats));
}
EXPORT_SYMBOL(avm_pa_get_stats);

void avm_pa_reset_stats(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   memset(&ctx->stats, 0, sizeof(struct avm_pa_stats));
}
EXPORT_SYMBOL(avm_pa_reset_stats);

void avm_pa_dev_init(struct avm_pa_dev_info *devinfo)
{
   memset(devinfo, 0, sizeof(struct avm_pa_dev_info));
}
EXPORT_SYMBOL(avm_pa_dev_init);

static int avm_pa_pid_receive(avm_pid_handle pid_handle, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   struct avm_pa_pkt_info *info;
   struct avm_pa_session *session;
   struct vlan_ethhdr *ethh;
   int rc;

   if (ctx->disabled)
      return AVM_PA_RX_OK;

   avm_simple_profiling_skb(0, pkt);

   info = AVM_PKT_INFO(pkt);

   if (info->ingress_pid_handle)
      return AVM_PA_RX_OK;

   ctx->stats.rx_pkts++;

   rcu_read_lock();

   info->ingress_pid_handle = pid_handle;
   info->ingress_vpid_handle = 0;
   info->egress_pid_handle = 0;
   info->egress_vpid_handle = 0;
   info->vpid_counted_slow = 0;
   info->is_accelerated = 0;
   info->routed = info->shaped = 0;
   info->session_handle = 0;

#ifdef CONFIG_AVM_PA_RPS
   /* For RPS, we need info->match.hash be populated even for bsessions, so do it now */
   rc = pa_set_pkt_match(pid->ingress_framing,
                         pid->ecfg.flags & AVM_PA_PID_FLAG_HSTART_ON_INGRESS ? info->hstart : 0,
                         pkt, &info->match, 0);
#endif

   if ((ethh = pa_get_ethhdr(pid->ingress_framing, pkt)) != 0) {
      if ((session = pa_bsession_search(pid, ethh_hash(ethh), ethh)) != 0)
         goto accelerate;
      if ((pid->ecfg.flags & AVM_PA_PID_FLAG_NO_PID_CHANGED_CHECK) == 0) {
         u16 vlan_tci = pa_get_vlan_tag(pid->ingress_framing, pkt);
         pa_check_and_handle_ingress_pid_change(ethh->h_source, pid_handle, vlan_tci);
      }
   }

#ifndef CONFIG_AVM_PA_RPS
   rc = pa_set_pkt_match(pid->ingress_framing,
                         pid->ecfg.flags & AVM_PA_PID_FLAG_HSTART_ON_INGRESS ? info->hstart : 0,
                         pkt, &info->match, 0);
#endif

   if (rc == AVM_PA_RX_OK) {

      ctx->stats.rx_search++;
      if ((session = pa_session_search(pid, &info->match)) == 0) {
         info->ingress_pid_handle = pid_handle;
#if AVM_PA_TRACE
         if (ctx->dbgtrace) {
            pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                                  pkt_uniq_id(pkt), pid->cfg.name,
                                  "no session");
            if (ctx->dbgnosession) {
               char buf[64];
               data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
               pa_printk(KERN_DEBUG, "Data           : %s\n", buf);
               pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
            }
         }
#endif
         if (ctx->fw_disabled || avm_pa_capture_running()) {
#if AVM_PA_TRACE
            if (ctx->dbgtrace)
               pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                     pkt_uniq_id(pkt), pid->cfg.name, "forward disabled");
#endif
            info->do_not_accelerate = 1;
         }
         rc = AVM_PA_RX_OK;
         goto out_unlock;
      }

      if (   avm_pa_first_egress(session)->type == avm_pa_egresstype_xfrm
          && avm_pa_first_egress(session)->xfrm.x->km.state != XFRM_STATE_VALID) {
         pa_session_flush(session, "xfrm state invalid");
         rc = AVM_PA_RX_OK;
         goto out_unlock;
      }

      if (info->match.pkttype & AVM_PA_PKTTYPE_LISP) {
         void *slhdr = LISPDATAHDR(&session->ingress);
         void *ilhdr = LISPDATAHDR(&info->match);
         if (memcmp(slhdr, ilhdr, LISP_DATAHDR_SIZE) != 0) {
            pa_session_flush(session, "lisp data header changed");
            ctx->stats.rx_lispchanged++;
            rc = AVM_PA_RX_OK;
            goto out_unlock;
         }
      }

      ctx->stats.rx_match++;

      if (pa_egress_size_check(session, pkt) < 0) {
         ctx->stats.rx_df++;
         info->ingress_pid_handle = pid_handle;
#if AVM_PA_TRACE
         if (ctx->dbgtrace)
            pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                  pkt_uniq_id(pkt), pid->cfg.name,
                  "size problem");
#endif
         rc = AVM_PA_RX_OK;
         goto out_unlock;
      }

      if (info->match.fin || session->timeout == 0) {
         /* Fin terminates sessions, all further packets (including acks for
          * fin) take the slow path. Only set the timeout to prevent
          * session creation by the very last ack. However, when the tcp
          * socket is reused (indicated by a new syn) quickly, we must create
          * a new session for it immediately. */
         if (info->match.syn)
            pa_session_flush(session, "new flow");
         else
            info->do_not_accelerate = 1;
         session->timeout = 0;
         pa_session_update(session);
         rc = AVM_PA_RX_OK;
         goto out_unlock;
      }

accelerate:
      pa_session_update(session);

      if (ctx->fw_disabled) {
         if (session->timeout == 0)
            pa_session_flush(session, "fast timeout");
#if AVM_PA_TRACE
         if (ctx->dbgtrace)
            pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                  pkt_uniq_id(pkt), pid->cfg.name,
                  "forward disabled");
#endif
            rc = AVM_PA_RX_OK;
            goto out_unlock;
      }

      if (pid->ingress_framing == avm_pa_framing_dev)
         PKT_PUSH(pkt, PKT_DATA(pkt) - skb_mac_header(pkt));

      if (skb_headroom(pkt) < session->needed_headroom) {
         struct sk_buff *npkt;
         if (net_ratelimit())
            pr_err("avm_pa: pid %u (%s): headroom %u < %u\n",
                   pid_handle, pid->cfg.name,
                   skb_headroom(pkt),
                   (unsigned)session->needed_headroom);
         ctx->stats.rx_headroom_too_small++;
         npkt = skb_realloc_headroom(pkt, session->needed_headroom);
         if (npkt == 0) {
            if (net_ratelimit())
               pr_err("avm_pa: pid %u (%s): skb_realloc_headroom(%u) failed\n",
                      pid_handle, pid->cfg.name,
                      (unsigned)session->needed_headroom);
            ctx->stats.rx_realloc_headroom_failed++;
            /* go slow path */
            rc = AVM_PA_RX_OK;
            goto out_unlock;
         } else {
            kfree_skb(pkt);
            pkt = npkt;
         }
      }

#if AVM_PA_TRACE
      if (ctx->dbgtrace)
         pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
               pkt_uniq_id(pkt), pid->cfg.name,
               "accelerated");
#endif

      if (skb_has_frag_list(pkt)) {
         ctx->stats.rx_frag_list += 1;
      }

      if (in_irq() || irqs_disabled()) {
         if (skb_queue_len(&ctx->irqqueue) > AVM_PA_MAX_IRQ_QUEUE_LEN) {
            ctx->stats.rx_irqdropped++;
            PKT_FREE(pkt);
         } else {
            info = AVM_PKT_INFO(pkt);
            info->session_handle  = session->session_handle;
            info->session_uniq_id = session->uniq_id;
            skb_queue_tail(&ctx->irqqueue, pkt);
            ctx->stats.rx_irq++;
            tasklet_schedule(&ctx->irqtasklet);
         }
      } else {
#ifdef CONFIG_AVM_PA_RPS
         avm_pa_rps_transmit(session, pkt);
#else
         avm_pa_tbf_transmit(session, pkt);
#endif
      }

      rc = AVM_PA_RX_ACCELERATED;
      goto out_unlock;
   }

   if (ctx->dbgmatch) {
      char buf[64];
      pa_printk(KERN_DEBUG, "---------->\n");
      pa_printk(KERN_DEBUG, "RC             : %d %s\n", rc, rc2str(rc));
      data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
      pa_printk(KERN_DEBUG, "Data           : %s\n", buf);
      pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
      pa_printk(KERN_DEBUG, "<----------\n");
   }
   pa_reset_match(&info->match);
   switch (rc) {
      case AVM_PA_RX_TTL:
         ctx->stats.rx_ttl++;
         break;
      case AVM_PA_RX_BROADCAST:
         ctx->stats.rx_broadcast++;
         break;
      default:
         ctx->stats.rx_bypass++;
         break;
   }
#if AVM_PA_TRACE
   if (ctx->dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s (rc %d)\n",
                            pkt_uniq_id(pkt), pid->cfg.name,
                            "bypass", rc);
#endif

out_unlock:
   rcu_read_unlock();
   return rc;
}

static inline void avm_pa_vpid_snoop_receive(avm_vpid_handle handle, PKT *pkt)
{
#if AVM_PA_TRACE
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->dbgtrace) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, handle);
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_vpid_snoop_receive(%s)\n",
                            pkt_uniq_id(pkt), vpid->cfg.name);
   }
#endif
   AVM_PKT_INFO(pkt)->ingress_vpid_handle = handle;
}


int avm_pa_dev_receive(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   int rc = AVM_PA_RX_OK;
   if (devinfo->pid_handle) {
      rc = avm_pa_pid_receive(devinfo->pid_handle, pkt);
      if (rc == AVM_PA_RX_ACCELERATED)
         return rc;
   }
   if (devinfo->vpid_handle)
      avm_pa_vpid_snoop_receive(devinfo->vpid_handle, pkt);
   return rc;
}
EXPORT_SYMBOL(avm_pa_dev_receive);

int avm_pa_dev_pid_receive(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   int rc = AVM_PA_RX_OK;

   avm_simple_profiling_skb(0, pkt);

   if (devinfo->pid_handle) {
      struct avm_hardware_pa *hwpa = &ctx->hardware_pa;
      /* We must be careful here since try_to_accelerate might be module code
       * that could be unloaded our the back. Therefore we must get an explicit
       * ref on the hardware_pa since we aren't tied to a session yet.
       */
      if (hwpa->try_to_accelerate && !ctx->hw_ppa_disabled && pa_hw_pa_get()) {
         struct avm_pa_pid *pid = PA_PID(ctx, devinfo->pid_handle);
         if (pid->rx_channel_activated) {
            if (pid->rx_channel_stopped == 0) {
               if (hwpa->try_to_accelerate(devinfo->pid_handle, pkt) <= 0)
                  rc = AVM_PA_RX_STOLEN;
            } else {
               ctx->stats.rx_channel_stopped++;
            }
         }
         pa_hw_pa_put();
      }
      if (rc != AVM_PA_RX_STOLEN) {
         rc = avm_pa_pid_receive(devinfo->pid_handle, pkt);
      }
   }
   return rc;
}
EXPORT_SYMBOL(avm_pa_dev_pid_receive);

void avm_pa_dev_vpid_snoop_receive(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   if (devinfo->vpid_handle)
      avm_pa_vpid_snoop_receive(devinfo->vpid_handle, pkt);
}
EXPORT_SYMBOL(avm_pa_dev_vpid_snoop_receive);

void avm_pa_mark_routed(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->routed = 1;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_mark_routed (ingress %d)\n",
                            pkt_uniq_id(pkt),
                            AVM_PKT_INFO(pkt)->ingress_pid_handle);
#endif
}
EXPORT_SYMBOL(avm_pa_mark_routed);

void avm_pa_mark_shaped(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->shaped = 1;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - %s (ingress %d)\n",
                            pkt_uniq_id(pkt),
                            __func__,
                            AVM_PKT_INFO(pkt)->ingress_pid_handle);
#endif
}
EXPORT_SYMBOL(avm_pa_mark_shaped);

void avm_pa_skb_set_rps(struct sk_buff *skb,
                        const struct cpumask *allow,
                        const struct cpumask *fallback)
{
#ifdef CONFIG_AVM_PA_RPS
   AVM_PKT_INFO(skb)->rps_override = 1;
   AVM_PKT_INFO(skb)->rps_allowed_mask = *allow;
   AVM_PKT_INFO(skb)->rps_fallback_mask = *fallback;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - %s (ingress %d)\n",
                            pkt_uniq_id(skb),
                            __func__,
                            AVM_PKT_INFO(skb)->ingress_pid_handle);
#endif
#endif
}
EXPORT_SYMBOL(avm_pa_skb_set_rps);

void avm_pa_use_protocol_specific_session(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->use_protocol_specific = 1;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_use_protocol_specific_session (ingress %d)\n",
                            pkt_uniq_id(pkt),
                            AVM_PKT_INFO(pkt)->ingress_pid_handle);
#endif
}
EXPORT_SYMBOL(avm_pa_use_protocol_specific_session);

void avm_pa_do_not_accelerate(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->do_not_accelerate = 1;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_do_not_accelerate\n",
                            pkt_uniq_id(pkt));
#endif
}
EXPORT_SYMBOL(avm_pa_do_not_accelerate);

void avm_pa_set_hstart(PKT *pkt, unsigned int hstart)
{
   AVM_PKT_INFO(pkt)->hstart = hstart;
}
EXPORT_SYMBOL(avm_pa_set_hstart);

static inline void avm_pa_vpid_snoop_transmit(avm_vpid_handle handle, PKT *pkt)
{
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);
   if (info->egress_vpid_handle == 0)
      info->egress_vpid_handle = handle;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_vpid *vpid = PA_VPID(ctx, handle);
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_vpid_snoop_transmit(%s)\n",
                            pkt_uniq_id(pkt), vpid->cfg.name);
   }
#endif
}

static inline int avm_pa_sock_is_realtime(struct sock *sk)
{
#ifdef CONFIG_AVM_SK_TC_INDEX
   return sk->sk_protocol == IPPROTO_UDP && sk->sk_tc_index != 0;
#else
   return 0;
#endif
}

static inline unsigned int pa_calc_tack_priority(struct avm_pa_pkt_info *info,
                                                 struct avm_pa_pid *epid,
                                                 unsigned int orig_priority)
{
   unsigned int newprio = orig_priority;
   if (AVM_PA_PKTTYPE_IPPROTO(info->match.pkttype) == IPPROTO_TCP) {
      unsigned int prio;
      prio = avm_pa_pid_tack_enabled(epid) ? avm_pa_pid_tack_prio(epid, orig_priority) : 0;
      if (prio != 0 && prio < newprio) newprio = prio;
   }
   return newprio;
}

static inline unsigned int pa_calc_start_priority(struct avm_pa_pkt_info *info,
                                                  struct avm_pa_pid *epid,
                                                  unsigned int orig_priority)
{
   /*
    * We calculate the priority to use, when session is created.
    * We assume it's an TGET or TACK session. The final decision will be made in
    * pa_session_prioack_check().
    *   2016-10-14 calle
    */
   unsigned int newprio = orig_priority;
   if (AVM_PA_PKTTYPE_IPPROTO(info->match.pkttype) == IPPROTO_TCP) {
      unsigned int prio;
      prio = avm_pa_pid_tget_enabled(epid) ? avm_pa_pid_tget_prio(epid, orig_priority) : 0;
      if (prio != 0 && prio < newprio) newprio = prio;
      prio = avm_pa_pid_tack_enabled(epid) ? avm_pa_pid_tack_prio(epid, orig_priority) : 0;
      if (prio != 0 && prio < newprio) newprio = prio;
   }
   return newprio;
}

static inline int avm_pa_pid_snoop_transmit(avm_pid_handle pid_handle,
                                            PKT *pkt,
                                            enum avm_pa_egresstype etype, void *edata)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);
   struct avm_pa_session *session;
   struct avm_pa_egress *egress;
   struct avm_pa_pkt_match match;
   struct avm_pa_pid *ipid, *epid;
   struct avm_pa_vpid *ivpid, *evpid;
   struct vlan_ethhdr *ethh;
   int headroom;
   char buf[64];
   int ret;
   struct sock *sk = NULL;
   struct xfrm_state *x = NULL;

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      epid = PA_PID(ctx, pid_handle);
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_snoop_transmit(%s)\n",
                            pkt_uniq_id(pkt), epid->cfg.name);
   }
#endif

   if (ctx->disabled)
      return AVM_PA_TX_OK;

   if (info->is_accelerated) {
      ctx->stats.tx_accelerated++;
      return AVM_PA_TX_OK;
   }

   rcu_read_lock();

   epid = PA_PID(ctx, pid_handle);

   /* Update vpid statistics also for packets that took the slow path. This enables using
    * only the vpid counters for the online monitor (provided the avm_pa is enabled). */
   if (!info->vpid_counted_slow
         && (info->ingress_vpid_handle || info->egress_vpid_handle)) {
      PKT *npkt;
      u32 bytes = 0, len = PKT_LEN(pkt);
      int nfrags = 0;
      int casttype = info->match.casttype;
      unsigned int prio, priority;
      unsigned long flags;

      skb_walk_frags(pkt, npkt) {
         bytes += PKT_LEN(npkt) + len;
         nfrags++;
      }
      if (!bytes)
         bytes = len;
      if (!nfrags)
         nfrags = 1;

      if (info->ingress_vpid_handle) {
         ivpid = PA_VPID(ctx, info->ingress_vpid_handle);
         write_lock_irqsave(&ivpid->slow_stats_lock, flags);
         (&ivpid->slow_stats.rx_bytes)[casttype] += bytes;
         (&ivpid->slow_stats.rx_unicast_pkt)[casttype] += nfrags;
         /* update prio stats */
         priority = pa_get_ingress_priority_from_pkt_mark(pkt->mark);
         prio = pa_get_priority(priority);
         ivpid->ingress_slow_sw_stats[prio].pkts += nfrags;
         ivpid->ingress_slow_sw_stats[prio].bytes += bytes;
         write_unlock_irqrestore(&ivpid->slow_stats_lock, flags);
      }
      if (info->egress_vpid_handle) {
         evpid = PA_VPID(ctx, info->egress_vpid_handle);
         write_lock_irqsave(&evpid->slow_stats_lock, flags);
         evpid->slow_stats.tx_bytes += bytes;
         (&evpid->slow_stats.tx_unicast_pkt)[casttype] += nfrags;
         /* update prio stats */
         if (info->match.ack_only)
            priority = pa_calc_tack_priority(info, epid, pkt->priority);
         else
            priority = pkt->priority;
         prio = pa_get_priority(priority);
         evpid->slow_sw_stats[prio].pkts += nfrags;
         evpid->slow_sw_stats[prio].bytes += bytes;
         write_unlock_irqrestore(&evpid->slow_stats_lock, flags);
      }
      /* vpids must be accounted exactly once, in case of multple
       * avm_pa_pid_snoop_transmit() calls */
      info->vpid_counted_slow = 1;
   }

   if (info->do_not_accelerate) {
      ctx->stats.tx_bypass++;
      if (ctx->dbgnosession) {
         pa_printk(KERN_DEBUG, "Bypass         : do not accelerate\n");
         data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
         pa_printk(KERN_DEBUG, "Data           : %s\n", buf);
         pa_printk(KERN_DEBUG, "---------------\n");
      }
      goto tx_bypass;
   }

   if (info->ingress_pid_handle == 0) {
      ctx->stats.tx_local++;
      goto tx_bypass;
   }

   if (etype == avm_pa_egresstype_local) {
      sk = edata;
   } else if (etype == avm_pa_egresstype_xfrm) {
      x = edata;
      if (!x || !IS_ENABLED(CONFIG_XFRM)) {
         ctx->stats.tx_bypass++;
         goto tx_bypass;
      }
   }

   ipid = PA_PID(ctx, info->ingress_pid_handle);
   ethh = pa_get_ethhdr(epid->egress_framing, pkt);

   if (info->match.syn || info->match.fin) {
      ctx->stats.tx_bypass++;
      if (ctx->dbgnosession) {
         pa_printk(KERN_DEBUG, "Bypass         : %s\n", info->match.syn ? "Syn" : "Fin");
         data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
         pa_printk(KERN_DEBUG, "Data           : %s\n", buf);
         pa_printk(KERN_DEBUG, "<- pkt_info  ->\n");
         pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
         pa_printk(KERN_DEBUG, "---------------\n");
      }
      goto tx_bypass;
   }

   ret = pa_egress_precheck(epid, pkt, &info->match, &match);
   if (ret != AVM_PA_RX_OK) {
      ctx->stats.tx_bypass++;
      if (ctx->dbgnosession) {
         pa_printk(KERN_DEBUG, "Bypass         : precheck failed (%d)\n", ret);
         data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
         pa_printk(KERN_DEBUG, "Data           : %s\n", buf);
         pa_printk(KERN_DEBUG, "<- pkt_info  ->\n");
         pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
         pa_printk(KERN_DEBUG, "<- pkt_match ->\n");
         pa_show_pkt_match(&match, 0, 0, pa_printk, KERN_DEBUG);
         pa_printk(KERN_DEBUG, "---------------\n");
      }
      goto tx_bypass;
   }

   if (info->session_handle != 0) {
      BUG_ON(info->egress_pid_handle == 0);
      if (info->shaped == 0 && info->egress_pid_handle != pid_handle) {
         /* A lower-level pid is taking over. This creates a new session,
          * for many reasons:
          * - updating the egress would be subject to race conditions
          *   since the session is already in state ACTIVE
          * - the egress match info and mod record must be parsed again
          *   (for example, vlan may change)
          * - as a result, the classification as bridged session may change
          * - is super easy to implement (really just need to flush here)
          *
          * Keep in mind that this check is only done for the first packet
          * (is_accelerated == 0).
          *
          * If the current pid performs traffic shaping, this take over is
          * prevented, since traffic shaping wouldn't work anymore.
          */
         session = PA_SESSION(pd, info->session_handle);
         ctx->stats.tx_pid_change++;
         pa_session_flush(session, "pid take over");
      } else {
         ctx->stats.tx_already++;
         goto tx_bypass;
      }
   }

   /* This won't find bridge sessions which will create duplicate sessions.
    * Well, temporarly as they don't get past pa_session_activate().
    */
   if ((session = pa_session_search(ipid, &info->match)) == 0) {
      int (*add_session)(struct avm_pa_session *avm_session);
      int (*add_session_skb)(struct avm_pa_session *avm_session, struct sk_buff *skb);
      int hw_ok;
      /* Grab temporary references for use during CREATE state.
       * If the session fails to reach ACTIVE state, then pa_session_kill() will take
       * care of these. Otherwise pa_session_activate() will render them permanent.
       *
       * In any case we don't have to release them ourselves once we have both.
       */
      avm_pid_handle ingress_pid_handle = pa_pid_get(info->ingress_pid_handle);
      avm_pid_handle egress_pid_handle = pa_pid_get(pid_handle);

      if (likely(ingress_pid_handle && egress_pid_handle))
         session = pa_session_alloc(&info->match);

      if (!session) {
         /* Maybe we couldn't ref a PID, release the other one */
         if (ingress_pid_handle)
            pa_pid_put(ingress_pid_handle);
         if (egress_pid_handle)
            pa_pid_put(egress_pid_handle);

         if (sk) ctx->stats.local_sess_error++;
         else ctx->stats.tx_sess_error++;
         ret = AVM_PA_TX_ERROR_SESSION;
         goto out;
      }

      /* Session State: CREATE */
      session->ingress_pid_handle = ingress_pid_handle;
      session->ingress_vpid_handle = info->ingress_vpid_handle;
      session->ingress_priority = pa_get_ingress_priority_from_pkt_mark(pkt->mark);
      session->routed = info->routed ? 1 : 0;
      session->no_hw = info->no_hw ? 1 : 0;
      session->bsession = 0;
      egress = avm_pa_first_egress(session);
      egress->pid_handle = egress_pid_handle;
      egress->vpid_handle = info->egress_vpid_handle;
      egress->match = match;
      egress->type = etype;
      switch (etype) {
         case avm_pa_egresstype_local:
            if (sk && avm_pa_sock_is_realtime(sk))
               session->realtime = 1;
            egress->local.dev = pkt->dev;
            egress->local.dst = dst_clone(skb_dst(pkt));
            egress->local.skb_iif = SKB_IFF(pkt);
            break;
         case avm_pa_egresstype_xfrm:
            if (IS_ENABLED(CONFIG_XFRM)) {
               dev_hold(pkt->dev);
               xfrm_state_hold(x);
               egress->xfrm.dev = pkt->dev;
               egress->xfrm.x = x;
               egress->xfrm.dst = dst_clone(skb_dst(pkt));
               /* Ensure tx_arg == NULL since we always pass the xfrm_state */
               BUG_ON(PA_PID(ctx, egress->pid_handle)->cfg.tx_arg != NULL);
               egress->xfrm.tc_index = pkt->tc_index;
            }
            break;
         default:
            egress->output.dst = skb_dst(pkt) ? dst_clone(skb_dst(pkt)) : NULL;
            egress->output.orig_priority = pkt->priority;
            egress->output.priority = pkt->priority;
            egress->output.tack_priority = pa_calc_tack_priority(info, epid, pkt->priority);
            egress->output.tc_index = pkt->tc_index;
#ifdef CONFIG_NET_CLS_ACT
            egress->output.tc_verd = pkt->tc_verd;
#endif
            egress->output.skb_iif = SKB_IFF(pkt);
            egress->output.mac_len = pkt->mac_len;
#ifdef CONFIG_TI_PACKET_PROCESSOR
            egress->output.puma_pktinfo = *SKB_GET_PP_INFO_P(pkt);
#ifdef CONFIG_TI_META_DATA
            egress->output.ti_meta_info = pkt->ti_meta_info;
            egress->output.ti_meta_info2 = pkt->ti_meta_info2;
#endif
#endif
            if (epid->ecfg.cb_len) {
               memcpy(egress->output.cb,
                      &pkt->cb[epid->ecfg.cb_start], epid->ecfg.cb_len);
            }
            break;
      }
      if (ethh) {
         u16 vlan_tag = pa_get_vlan_tag(epid->egress_framing, pkt);
         egress->destmac = pa_macaddr_link(ethh->h_dest, pid_handle, vlan_tag);
      }
#ifdef CONFIG_AVM_PA_RPS
      /* For local sessions we try to keep it on the same CPU as the receiving
       * process. For now we assume the kernel has already selected the best cpu
       * and follow its decision. If RPS was configured explicitly via
       * avm_pa_skb_set_rps() then we commit to that CPU at session creation.
       *
       * Otherwise, CPU selection (via hash based on the flow) is deferred
       * to the fast path because there may be multiple flows within a single
       * bridge session.
       */
      if (info->rps_override || etype == avm_pa_egresstype_local) {
         int cpu = info->match.hash & (CONFIG_AVM_PA_RPS_QUEUES-1);
         if (etype == avm_pa_egresstype_local)
            cpu = smp_processor_id();

         if (info->rps_override) {
            if (!cpumask_test_cpu(cpu, &info->rps_allowed_mask)) {
               cpu = cpumask_any_but(&info->rps_fallback_mask, cpu);
               if (cpu >= nr_cpu_ids) {
                  cpu = cpumask_first(&info->rps_fallback_mask);
                  if (cpu >= nr_cpu_ids)
                     cpu = smp_processor_id(); /* RPS disabled */
               }
            }
         }
         session->rps_cpu = cpu + 1;
      }
#endif

      /* Bridged session are more efficient, but subject to a few restrictions:
       * - ethernet header must match, and nothing else
       * - packets must be bridged, not routed (obviously)
       * - must be unicast as broadcast/multicast means multiple egress, which might require
       *   different framings or even local input, which make plain bridging impossible
       * - avm_pa_use_protocol_specific_session() wasn't used to enforce normal sessions
       * - hardware_pa permits bridged sessions
       * - bridged sessions aren't disallowed through procfs interface
       * If all conditions are met, bridged sessions can use a few shortcuts such
       * as skipping data modification entirely.
       */
      if (   ethh
          && ctx->bsession_allowed
          && info->routed == 0
          && info->match.casttype == AVM_PA_IS_UNICAST
          && info->use_protocol_specific == 0
          && (ctx->hw_ppa_disabled || !(ctx->hardware_pa.flags & AVM_HW_F_NO_BSESSION))
          && pa_match_bridged(&info->match, &egress->match)) {
         pa_change_to_bridge_match(&session->ingress);
         pa_change_to_bridge_match(&egress->match);
         session->timeout = ctx->bridge_timeout_secs*HZ;
         session->bsession = pa_bsession_alloc(&egress->match, session->session_handle);
         /* VLAN protocol must be preserved, don't think of storing h_vlan_encapsulated_proto */
         session->mod.protocol = ethh->h_vlan_proto;
         egress->pppoe_offset = AVM_PA_OFFSET_NOT_SET;
         egress->push_l2_len = 0;
         egress->mtu = 0xffff;
      } else {
         if (egress->type == avm_pa_egresstype_output) {
            egress->output.priority = pa_calc_start_priority(info, epid, pkt->priority);
            if (egress->output.priority != egress->output.orig_priority) {
               session->prioack_check = 1; /* pa_session_prioack_check() will check priority */
               pkt->priority = egress->output.priority;
               if (ctx->dbgprioack) {
                  pa_printk(KERN_DEBUG, "avm_pa: session %d: priority %x:%x TGET (orignal %x:%x)\n",
                                        session->session_handle,
                                        TC_H_MAJ(egress->output.priority)>>16,
                                        TC_H_MIN(egress->output.priority),
                                        TC_H_MAJ(egress->output.orig_priority)>>16,
                                        TC_H_MIN(egress->output.orig_priority));
               }
            }
         }
         (void)pa_calc_modify(session, &info->match, &match);
         if (match.encap_offset == AVM_PA_OFFSET_NOT_SET)
            egress->push_l2_len = match.ip_offset;
         else egress->push_l2_len = match.encap_offset;
         headroom =   (session->mod.push_encap_len + egress->push_l2_len)
                    - (session->mod.pull_l2_len + session->mod.pull_encap_len);
         if (headroom > 0 && headroom > session->needed_headroom)
            session->needed_headroom = headroom;
         egress->pppoe_offset = match.pppoe_offset;
         if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET)
            egress->pppoe_hdrlen = egress->pppoe_offset + sizeof(struct pppoehdr);
         egress->mtu = epid->cfg.default_mtu;
         if (egress->vpid_handle) {
            evpid = PA_VPID(ctx, egress->vpid_handle);
            if (session->mod.protocol == constant_htons(ETH_P_IP)) {
               if (evpid->cfg.v4_mtu < egress->mtu)
                  egress->mtu = evpid->cfg.v4_mtu;
            } else if (session->mod.protocol == constant_htons(ETH_P_IPV6)) {
               if (evpid->cfg.v6_mtu < egress->mtu)
                  egress->mtu = evpid->cfg.v6_mtu;
            }
         }
      }

#ifdef CONFIG_AVM_GENERIC_CONNTRACK
      if (pkt->generic_ct) {
         session->generic_ct = generic_ct_get(pkt->generic_ct);
         session->generic_ct_dir = skb_get_ct_dir(pkt);
         /* don't do generic_ct_sessionid_set() yet because the session is not
          * activated yet, so don't use the session_handle yet */
      }
#endif

      /*
       * The selector is asked at last, because the session is not fully setup until now.
       * The session framework needs complete session info to make an informed decision.
       */
      if (ctx->filter_enabled && !avm_pa_session_is_selected(&ctx->accel_filter, session)) {
         ctx->stats.tx_bypass++;
         if (ctx->dbgnosession) {
            pa_printk(KERN_DEBUG, "Acceleration filtered\n");
            data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
            pa_printk(KERN_DEBUG, "Data          : %s\n", buf);
            pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
         }
         pa_session_kill(session, "filtered");
         goto tx_bypass;
      }

      /* activate guarantees that only one session of a kind exists but it also
       * hands over the session to the lookup so that newer packets (perhaps
       * on another CPU) can already use this session before we return */
      ret = pa_session_activate(session);
      if (ret != AVM_PA_TX_SESSION_ADDED)
         goto out;

      /* Session State: ACTIVE */
      avm_pa_sg_session_link(session, pkt);

#if AVM_PA_TRACE
      if (ctx->dbgtrace) {
         pa_printk(KERN_DEBUG, "avm_pa: add session %d (%s)\n",
                                session->session_handle, ipid->cfg.name);
      }
#endif
      if (ctx->dbgsession) {
         if (session->bsession) {
            pa_printk(KERN_DEBUG, "\navm_pa: new bsession:\n");
            pa_show_bsession(session->bsession, pa_printk, KERN_DEBUG);
         } else {
            pa_printk(KERN_DEBUG, "\navm_pa: new session:\n");
            pa_show_session(session, pa_printk, KERN_DEBUG);
         }
      }

      /*
       * Add session to hardware is done after activate the session to not confuse hardware pa
       * with multiple, equal sessions (in case of race condition with another CPU).
       * This means that, possibly, the prioack check is done for later packets and
       * not the first but this is not a problem as long as the session doesn't go in_hw
       *
       * Only add session to hardware here if:
       * a) It's not a local session, because HW-PA seems to have a problem with acceleration
       *    to local system (see JZ-26496 stockendes Internet). The problem seems to
       *    be specific to the vr9 platform.
       * b) prioack_check is NOT set. HW-PA dit't provide packet and byte counters so we
       *    must use counters from software acceleration.
       * c) HW-PA is available
       * d) HW-PA is not disabled
       * e) no_hw flag was not set in AVM_PKT_INFO (eg. by sch_tack)
       */
      add_session = rcu_dereference(ctx->hardware_pa.add_session);
      add_session_skb = rcu_dereference(ctx->hardware_pa.add_session_skb);
      hw_ok =  !session->prioack_check           // b
            &&  (add_session || add_session_skb) // c
            && !ctx->hw_ppa_disabled             // d
            && !session->no_hw;                  // e
#ifdef CONFIG_VR9
      if (etype == avm_pa_egresstype_local) hw_ok = 0; // a
#endif
      /* Must get a ref on the hardware pa before adding the session
       * to ensure that avm_pa_unregister_hardware_pa() doesn't do
       * complete(done) prematurely.
       */
      if (hw_ok && pa_hw_pa_get()) {
         int added = add_session_skb ? add_session_skb(session, pkt) : add_session(session);
         if (added == AVM_PA_TX_SESSION_ADDED) {
            session->in_hw = 1;
         } else {
            pa_hw_pa_put();
         }
      }
      avm_pa_set_associated_session_handle(session);
      if (etype == avm_pa_egresstype_local)
         ctx->stats.local_sess_ok++;
      else if (IS_ENABLED(CONFIG_XFRM) && etype == avm_pa_egresstype_xfrm)
         ctx->stats.xfrm_sess_ok++;
      else
         ctx->stats.tx_sess_ok++;
      info->session_handle = session->session_handle;
      info->egress_pid_handle = pid_handle;
      ret = AVM_PA_TX_SESSION_ADDED;
      goto out;
   }

   /*
    * It's a slow packet with existing session, this happens in case of
    * active packet tracing or batched rx processing (i.e. GRX).
    */

   info->session_handle = session->session_handle;
   info->egress_pid_handle = pid_handle;
   avm_pa_for_each_egress(egress, session) {
      if (   egress->pid_handle == pid_handle
          && egress->vpid_handle == info->egress_vpid_handle
          && pa_match_eq(&egress->match, &match)) {
         if (etype == avm_pa_egresstype_local) {
            ctx->stats.local_sess_exists++;
         } else {
            ctx->stats.tx_sess_exists++;
         }
         pa_session_update(session);
         /* use priority we decide to use for this egress */
         if (egress->type == avm_pa_egresstype_output)
            pkt->priority = egress->output.priority;
         ret = AVM_PA_TX_SESSION_EXISTS;
         goto out;
      }
   }

   if ((egress = pa_egress_alloc()) != NULL) {
      u16 mtu;
      int n;
      egress->pid_handle = pa_pid_get(pid_handle);
      if (unlikely(!egress->pid_handle)) {
         pa_egress_free(egress);
         goto no_egress;
      }
      egress->vpid_handle = info->egress_vpid_handle;
      egress->match = match;
      if (etype == avm_pa_egresstype_local) {
         if (sk && avm_pa_sock_is_realtime(sk))
            session->realtime = 1;
         egress->type = avm_pa_egresstype_local;
         egress->local.dev = pkt->dev;
         egress->local.dst = dst_clone(skb_dst(pkt));
         egress->local.skb_iif = SKB_IFF(pkt);
      } else {
         egress->type = avm_pa_egresstype_output;
         egress->output.orig_priority = pkt->priority;
         egress->output.priority = pkt->priority;
         egress->output.tack_priority = pkt->priority;
         egress->output.tc_index = pkt->tc_index;
#ifdef CONFIG_NET_CLS_ACT
         egress->output.tc_verd = pkt->tc_verd;
#endif
         egress->output.skb_iif = SKB_IFF(pkt);
         egress->output.mac_len = pkt->mac_len;
#ifdef CONFIG_TI_PACKET_PROCESSOR
         egress->output.puma_pktinfo = *SKB_GET_PP_INFO_P(pkt);
#endif
      }
      if (ethh) {
         u16 vlan_tag = pa_get_vlan_tag(epid->egress_framing, pkt);
         egress->destmac = pa_macaddr_link(ethh->h_dest, pid_handle, vlan_tag);
      }
      mtu = epid->cfg.default_mtu;
      if (egress->vpid_handle) {
         evpid = PA_VPID(ctx, egress->vpid_handle);
         if (session->mod.protocol == constant_htons(ETH_P_IP)) {
            if (evpid->cfg.v4_mtu < mtu)
               mtu = evpid->cfg.v4_mtu;
         } else if (session->mod.protocol == constant_htons(ETH_P_IPV6)) {
            if (evpid->cfg.v6_mtu < mtu)
               mtu = evpid->cfg.v6_mtu;
         }
      }
      if (session->bsession) {
         egress->pppoe_offset = AVM_PA_OFFSET_NOT_SET;
         egress->push_l2_len = 0;
         mtu = 0xffff;
      } else {
         /*
          * currently we do only TACK/TGET handling only on egress[0].
          * So we keep SKBs original priority.
          *   2016-10-14 calle
          */
         if (match.encap_offset == AVM_PA_OFFSET_NOT_SET)
            egress->push_l2_len = match.ip_offset;
         else egress->push_l2_len = match.encap_offset;
         headroom =   (session->mod.push_encap_len + egress->push_l2_len)
                    - (session->mod.pull_l2_len + session->mod.pull_encap_len);
         if (headroom > 0 && headroom > session->needed_headroom)
            session->needed_headroom = headroom;
         egress->pppoe_offset = match.pppoe_offset;
         if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET)
            egress->pppoe_hdrlen = egress->pppoe_offset + sizeof(struct pppoehdr);
      }
      egress->mtu = mtu;

      /* Atomically add the egress, after initialization. Fixes JZ-26868. */
      spin_lock(&avm_pa_lock);
      hlist_add_behind_rcu(&egress->egress_list, &avm_pa_first_egress(session)->egress_list);
      n = ++session->negress;
      spin_unlock(&avm_pa_lock);

      if (session->in_hw && pa_hw_pa_get()) {
         pa_session_handle_stats(session);
         if (ctx->hardware_pa.change_session) {
            if ((*ctx->hardware_pa.change_session)(session) != AVM_PA_TX_EGRESS_ADDED) {
               /* In case of concurrency, only one failing change_session()
                * call may call pa_hw_pa_put(), so check this flag again.
                */
               spin_lock(&avm_pa_lock);
               if (session->in_hw) {
                  session->in_hw = 0;
                  pa_hw_pa_put();
               }
               spin_unlock(&avm_pa_lock);
            }
         } else if (n == 2) {
            /* When removing the session, ensure that remove_session() is not
             * called twice due to concurrency, by allowing only the
             * second egress to reach this code.
             */
            session->in_hw = 0; /* avoid concurrent .session_stats() */
            (*ctx->hardware_pa.remove_session)(session);
            pa_hw_pa_put();
         }
         pa_hw_pa_put();
      }
      ctx->stats.tx_egress_ok++;
      if (ctx->dbgsession) {
         pa_printk(KERN_DEBUG, "\navm_pa: new egress:\n");
         pa_show_session(session, pa_printk, KERN_DEBUG);
      }
      ret = AVM_PA_TX_EGRESS_ADDED;
      goto out;
   }

no_egress:
   /*
    * JZ-56718: flush the entire session and try to allocate
    * all egress ports with the next set of slow path packets
    */
   pa_session_flush(session, "no egress left");
   ctx->stats.tx_egress_error++;
   ret = AVM_PA_TX_ERROR_EGRESS;
   goto out;

tx_bypass:
   /* 
    * set TACK priority for TCP control and ack only packets
    *  2016-10-14 calle
    */
   if (avm_pa_pid_tack_enabled(epid)) {
      if (info->match.syn || info->match.fin || info->match.ack_only) {
         pkt->priority = pa_calc_tack_priority(info, epid, pkt->priority);
         epid->prioack_acks++;
      }
   }
   ret = AVM_PA_TX_BYPASS;
out:
   rcu_read_unlock();
   return ret;
}

int avm_pa_dev_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   if (devinfo->vpid_handle)
      avm_pa_vpid_snoop_transmit(devinfo->vpid_handle, pkt);
   if (devinfo->pid_handle)
      return avm_pa_pid_snoop_transmit(devinfo->pid_handle, pkt, avm_pa_egresstype_output, 0);
   return AVM_PA_TX_OK;
}
EXPORT_SYMBOL(avm_pa_dev_snoop_transmit);

void avm_pa_dev_vpid_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   if (devinfo->vpid_handle)
      avm_pa_vpid_snoop_transmit(devinfo->vpid_handle, pkt);
}
EXPORT_SYMBOL(avm_pa_dev_vpid_snoop_transmit);

void _avm_pa_add_local_session(PKT *pkt, struct sock *sk)
{
   (void)avm_pa_pid_snoop_transmit(AVM_PKT_INFO(pkt)->ptype_pid_handle, pkt, avm_pa_egresstype_local, sk);
}
EXPORT_SYMBOL(_avm_pa_add_local_session);

int _avm_pa_local_out_receive(avm_pid_handle pid_handle, struct sk_buff *skb)
{
   return avm_pa_pid_receive(pid_handle, skb);
}
EXPORT_SYMBOL(_avm_pa_local_out_receive);

void avm_pa_add_xfrm_session(struct avm_pa_dev_info *devinfo, struct sk_buff *skb, struct xfrm_state *x)
{
   if (unlikely(!AVM_PKT_INFO(skb)->is_accelerated) && IS_ENABLED(CONFIG_XFRM))
      avm_pa_pid_snoop_transmit(devinfo->pid_handle, skb, avm_pa_egresstype_xfrm, x);
}

void avm_pa_add_rtp_session(PKT *pkt,
                            struct sock *sk,
                            void (*transmit)(struct sock *sk, PKT *pkt))
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);
   struct avm_pa_session *session;
   struct avm_pa_egress *egress;

   spin_lock_bh(&avm_pa_lock);
   session = pa_session_get(info->session_handle);
   if (session == 0 || session->negress > 1)
      goto unlock;

   egress = avm_pa_first_egress(session);
   if (egress->type != avm_pa_egresstype_local) {
      if (egress->type == avm_pa_egresstype_rtp)
         ctx->stats.rtp_sess_exists++;
      else
         ctx->stats.rtp_sess_error++;
      goto unlock;
   }
   session->realtime = 1;
   egress->type = avm_pa_egresstype_rtp;
   egress->rtp.dev = pkt->dev;
   egress->rtp.skb_iif = SKB_IFF(pkt);
   sock_hold(sk);
   egress->rtp.sk = sk;
   egress->rtp.transmit = transmit;
   ctx->stats.rtp_sess_ok++;

unlock:
   spin_unlock_bh(&avm_pa_lock);

}
EXPORT_SYMBOL(avm_pa_add_rtp_session);

void avm_pa_filter_packet(PKT *pkt)
{
   /* Drop/filter sessions are not implemented yet. */
   WARN_ONCE(1, "%s() does nothing. Remove the call!\n", __func__);
}
EXPORT_SYMBOL(avm_pa_filter_packet);

int avm_pa_dev_pidhandle_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                               avm_pid_handle pid_handle,
                                               struct avm_pa_pid_cfg *cfg,
                                               avm_pid_handle ingress_pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_pid_handle n;

   /* Already registered */
   if (devinfo->pid_handle) {
      BUG_ON(pid_handle && devinfo->pid_handle != pid_handle);
      return -EBUSY;
   }
   if (ingress_pid_handle) {
      if (PA_PID(ctx, ingress_pid_handle)->pid_handle != ingress_pid_handle)
         return -EINVAL; /* ingress pid must be registered beforehand */
   }
   if (pid_handle) {
      n = pid_handle;
      goto slot_found;
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      if (strncmp(cfg->name, PA_PID(ctx, n)->cfg.name, AVM_PA_MAX_NAME) == 0)
         goto slot_found;
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      if (PA_PID(ctx, n)->pid_handle == 0)
         goto slot_found;
   }
   return -ENOMEM;

slot_found:
   pa_pid_init(n, cfg);

   if (ingress_pid_handle) {
      PA_PID(ctx, n)->ingress_pid_handle = ingress_pid_handle;
   } else {
      PA_PID(ctx, n)->ingress_pid_handle = n;
   }
   devinfo->pid_handle = n;

   return 0;
}
EXPORT_SYMBOL(avm_pa_dev_pidhandle_register_with_ingress);

int avm_pa_dev_pidhandle_register(struct avm_pa_dev_info *devinfo,
                                  avm_pid_handle pid_handle,
                                  struct avm_pa_pid_cfg *cfg)
{
   return avm_pa_dev_pidhandle_register_with_ingress(devinfo, pid_handle, cfg,
                                                     0);
}
EXPORT_SYMBOL(avm_pa_dev_pidhandle_register);

int avm_pa_dev_pid_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                         struct avm_pa_pid_cfg *cfg,
                                         avm_pid_handle ingress_pid_handle)
{
   return avm_pa_dev_pidhandle_register_with_ingress(devinfo, 0, cfg,
                                                     ingress_pid_handle);
}
EXPORT_SYMBOL(avm_pa_dev_pid_register_with_ingress);

int avm_pa_dev_pid_register(struct avm_pa_dev_info *devinfo,
                            struct avm_pa_pid_cfg *cfg)
{
   return avm_pa_dev_pidhandle_register_with_ingress(devinfo, 0, cfg, 0);
}
EXPORT_SYMBOL(avm_pa_dev_pid_register);

static void pa_dev_transmit(void *arg, struct sk_buff *skb)
{
   int rc;
   skb->dev = (struct net_device *)arg;
   rc = dev_queue_xmit(skb);
   if (dev_xmit_complete(rc) == false && net_ratelimit()) {
      pr_err("%s(%s): xmit failure: %d\n", __func__, skb->dev->name, rc);
   }
}

int avm_pa_dev_register(struct net_device *dev)
{
   struct avm_pa_pid_cfg cfg = {0};

   BUG_ON(!dev);

   cfg.framing = avm_pa_framing_dev;
   snprintf(cfg.name, sizeof(cfg.name), "%s", dev->name);
   cfg.tx_func = pa_dev_transmit;
   cfg.tx_arg = dev;
   return avm_pa_dev_pidhandle_register_with_ingress(AVM_PA_DEVINFO(dev), 0, &cfg, 0);
}
EXPORT_SYMBOL(avm_pa_dev_register);

#ifdef CONFIG_AVM_PA_TX_NAPI
int avm_pa_dev_pid_register_tx_napi(struct avm_pa_dev_info *devinfo,
                                   struct avm_pa_pid_cfg *cfg,
                                   struct net_device *dev)
{
   int ret;

   ret = avm_pa_dev_pidhandle_register_with_ingress(devinfo, 0, cfg, 0);

   if (!ret) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_pid *pid = PA_PID(ctx, devinfo->pid_handle);

      netif_napi_add(dev, &pid->tx_napi, pa_dev_tx_napi_poll, TX_NAPI_BUDGET);
      napi_enable(&pid->tx_napi);
      skb_queue_head_init(&pid->tx_napi_pkts);
#ifdef CONFIG_SMP
      tasklet_init(&pid->tx_napi_tsk, (void *) __do_schedule_napi, (unsigned long) &pid->tx_napi);
#endif
   }

   return ret;
}
EXPORT_SYMBOL(avm_pa_dev_pid_register_tx_napi);
#endif

int avm_pa_pid_set_ecfg(avm_pid_handle pid_handle,
                        struct avm_pa_pid_ecfg *ecfg)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   unsigned int cbsize = sizeof(((struct sk_buff *)0)->cb);

   if (pid->pid_handle != pid_handle)
      return -1;
   memset(&pid->ecfg, 0, sizeof(struct avm_pa_pid_ecfg));
   switch (ecfg->version) {
      case 3:
        pid->ecfg.pid_group = ecfg->pid_group;
      case 2:
        pid->ecfg.rx_slow = ecfg->rx_slow;
        pid->ecfg.rx_slow_arg = ecfg->rx_slow_arg;
      case 1:
        pid->ecfg.cb_start = ecfg->cb_start;
        pid->ecfg.cb_len = ecfg->cb_len;
      case 0:
        pid->ecfg.flags = ecfg->flags;
   }
   if (pid->ecfg.cb_start + pid->ecfg.cb_len > cbsize)
      return -2;
   pid->ecfg.version = ecfg->version;
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_set_ecfg);

int avm_pa_pid_set_framing(avm_pid_handle pid_handle,
                           enum avm_pa_framing ingress_framing,
                           enum avm_pa_framing egress_framing)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (pid->pid_handle != pid_handle)
      return -1;

   switch (ingress_framing) {
      case avm_pa_framing_llcsnap:
      case avm_pa_framing_ether:
      case avm_pa_framing_ppp:
      case avm_pa_framing_ip:
      case avm_pa_framing_ipdev:
      case avm_pa_framing_dev:
         pid->ingress_framing = ingress_framing;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_ptype:
         if (pid->ingress_framing != ingress_framing)
            return -2;
         pid->cfg.tx_func = 0;
         pid->cfg.tx_arg = 0;
         break;
   }
   switch (egress_framing) {
      case avm_pa_framing_llcsnap:
      case avm_pa_framing_ether:
      case avm_pa_framing_ppp:
      case avm_pa_framing_ip:
      case avm_pa_framing_ipdev:
         pid->egress_framing = egress_framing;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_dev:
         pid->egress_framing = avm_pa_framing_ether;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_ptype:
         return -3;
   }
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_set_framing);

static void pa_show_pids(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   char buf[128];
   avm_pid_handle n;
   unsigned int i;

#define INDENT "    "
   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      struct avm_pa_pid *pid = PA_PID(ctx, n);
      struct avm_pa_pid_ecfg *ecfg = &pid->ecfg;
      int refcount;

      if (pid->pid_handle == 0)
         continue;

      refcount = atomic_read(&pid->ref.refcount);
      if (pid->ingress_pid_handle == pid->pid_handle) {
         (*fprintffunc)(arg, "PID%-3d: (%5d) %-5s %-5s %10lu %s %s (ref %d)\n",
                             pid->pid_handle,
                             pid->cfg.default_mtu,
                             framing2str(pid->ingress_framing),
                             framing2str(pid->egress_framing),
                             (unsigned long)pid->tx_pkts,
                             pid->cfg.name,
                             pidflags2str(ecfg->flags, buf, sizeof(buf)),
                             refcount);
      } else {
         (*fprintffunc)(arg, "PID%-3d: (%5d) %-5s %-5s %10lu %s (ingress %d %s) %s (ref %d)\n",
                             pid->pid_handle,
                             pid->cfg.default_mtu,
                             framing2str(pid->ingress_framing),
                             framing2str(pid->egress_framing),
                             (unsigned long)pid->tx_pkts,
                             pid->cfg.name,
                             pid->ingress_pid_handle,
                             PA_PID(ctx, pid->ingress_pid_handle)->cfg.name,
                             pidflags2str(ecfg->flags, buf, sizeof(buf)),
                             refcount);
      }
      if (pid->rx_channel_activated || pid->tx_channel_activated) {
         (*fprintffunc)(arg, INDENT "rx_channel %d tx_channel %d\n",
                             pid->rx_channel_activated ? 1 : 0,
                             pid->tx_channel_activated ? 1 : 0);
      }

      if (ecfg->pid_group)
         (*fprintffunc)(arg, INDENT "pid_group %d\n", ecfg->pid_group);
      if (ecfg->rx_slow)
         (*fprintffunc)(arg, INDENT "rx_slow %pf\n", ecfg->rx_slow);
      if (ecfg->cb_start || ecfg->cb_len)
         (*fprintffunc)(arg, INDENT "cb_start %d cb_len %d\n", ecfg->cb_start, ecfg->cb_len);

      if (avm_pa_pid_tack_enabled(pid) || avm_pa_pid_tget_enabled(pid)) {
         for (i = 0; i < AVM_PA_MAX_PRIOS; ++i) {
            unsigned int tackprio = 0;
            unsigned int tgetprio = 0;
            if (avm_pa_pid_tack_enabled(pid))
               tackprio = avm_pa_pid_tack_prio(pid, i);
            if (avm_pa_pid_tget_enabled(pid))
               tgetprio = avm_pa_pid_tget_prio(pid, i);
            if (tackprio || tgetprio) {
               (*fprintffunc)(arg, INDENT "prio[%u]:", i);
               if (tackprio)
                  (*fprintffunc)(arg, " tack_prio = 0x%x", tackprio);
               if (tgetprio)
                  (*fprintffunc)(arg, " tack_prio = 0x%x", tgetprio);
               (*fprintffunc)(arg, "\n");
            }
         }
      }
   }
#undef INDENT
}

int avm_pa_dev_vpidhandle_register(struct avm_pa_dev_info *devinfo,
                                   avm_vpid_handle vpid_handle,
                                   struct avm_pa_vpid_cfg *cfg)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;

   if (devinfo->vpid_handle) {
      if (vpid_handle && devinfo->vpid_handle != vpid_handle)
         return 0;
      n = devinfo->vpid_handle;
      goto slot_found;
   }
   if (vpid_handle) {
      n = vpid_handle;
      goto slot_found;
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      if (strncmp(cfg->name, PA_VPID(ctx, n)->cfg.name, AVM_PA_MAX_NAME) == 0) {
         goto slot_found;
      }
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      if (PA_VPID(ctx, n)->vpid_handle == 0)
         goto slot_found;
   }
   return -1;
slot_found:
   if (cfg->v4_mtu == 0)
      cfg->v4_mtu = 1500;
   if (cfg->v6_mtu == 0)
      cfg->v6_mtu = 1500;

   memset(PA_VPID(ctx, n), 0, sizeof(*PA_VPID(ctx, n)));
   PA_VPID(ctx, n)->cfg = *cfg;
   PA_VPID(ctx, n)->vpid_handle = n;
   rwlock_init(&PA_VPID(ctx, n)->slow_stats_lock);
   devinfo->vpid_handle = n;
   return 0;
}
EXPORT_SYMBOL(avm_pa_dev_vpidhandle_register);

int avm_pa_dev_vpid_register(struct avm_pa_dev_info *devinfo,
                             struct avm_pa_vpid_cfg *cfg)
{
   return avm_pa_dev_vpidhandle_register(devinfo, 0, cfg);
}
EXPORT_SYMBOL(avm_pa_dev_vpid_register);

int avm_pa_dev_unregister(struct avm_pa_dev_info *devinfo, struct completion *done)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_hardware_pa *hwpa;
   (void)avm_pa_dev_reset_stats(devinfo);

   if (devinfo->vpid_handle) {
      struct avm_pa_vpid *my_vpid = PA_VPID(ctx, devinfo->vpid_handle);
      avm_vpid_handle my_vpid_handle = my_vpid->vpid_handle;

      devinfo->vpid_handle = 0;
      if (my_vpid_handle != 0) {
         my_vpid->vpid_handle = 0;
         avm_pa_selector_clear_for_vpid(&ctx->show_filter, my_vpid_handle);
         avm_pa_flush_sessions_for_vpid(my_vpid_handle);
      }
   }

   if (devinfo->pid_handle) {
      /*
       * Unregister so that no new sessions can be created for the pid.
       *
       * Deleting the pid may be deferred if there are still sessions alive,
       * this is handled in the GC (through ref counts).
       * A reregister is possible until all sessions are gone.
       *
       * The ref added at avm_pa_dev_pid_register() is still valid therefore
       * we don't add another ref here but use PA_PID().
       */
      struct avm_pa_pid *my_pid = PA_PID(ctx, devinfo->pid_handle);
      avm_pid_handle my_pid_handle = my_pid->pid_handle;

      devinfo->pid_handle = 0;
      if (my_pid_handle != 0) {
         avm_pid_handle n;
         /* check if pid is used as ingress pid */
         for (n = 1; n < CONFIG_AVM_PA_MAX_PID; n++) {
            struct avm_pa_pid *pid = PA_PID(ctx, n);
            if (pid->ingress_pid_handle == my_pid_handle)
               pid->ingress_pid_handle = pid->pid_handle;
         }
         avm_pa_selector_clear_for_pid(&ctx->show_filter, my_pid_handle);

         /* free virtual channels */
         hwpa = &ctx->hardware_pa;
         my_pid->rx_channel_stopped = 1;
         if (my_pid->tx_channel_activated) {
            my_pid->tx_channel_activated = 0;
            if (hwpa->free_tx_channel)
               hwpa->free_tx_channel(my_pid_handle);
         }
         if (my_pid->rx_channel_activated) {
            my_pid->rx_channel_activated = 0;
            if (hwpa->free_rx_channel)
               hwpa->free_rx_channel(my_pid_handle);
         }

         /* At session creation, referencing the PID and moving the session
          * to ACTIVE state is not fully atomic outside the lock (cannot flush essions
          * that are in CREATE state). But inside the lock, any sessions in CREATE state
          * cannot become ACTIVE, because clearing pid->pid_handle prevents new refs.
          */
         spin_lock_bh(&avm_pa_lock);
         my_pid->pid_handle = 0;
         my_pid->release_completion = done;
         if (!pa_pid_put(my_pid_handle)) {
            avm_pa_flush_sessions_for_pid(my_pid_handle);
         }
         spin_unlock_bh(&avm_pa_lock);
         return 0;
      }
   }

   return -ENODEV;
}
EXPORT_SYMBOL(avm_pa_dev_unregister);

int avm_pa_dev_unregister_sync(struct avm_pa_dev_info *devinfo)
{
   int ret;
   int my_pid_handle = devinfo->pid_handle;
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, my_pid_handle);
   DECLARE_COMPLETION_ONSTACK(done);

   might_sleep();
   ret = avm_pa_dev_unregister(devinfo, &done);
   if (ret != 0)
      return ret;

   /* Normally there is no way to block indefinitely, but mark killable
    * in case of a bug somewhere.
    */
   ret = wait_for_completion_killable_timeout(&done, HZ * 10);
   if (ret == 0) {
      /* Timeout. This is fatal. Maybe some session hangs? */
      int i;
      pr_crit("FATAL in %s!\n  PID %s is not removed. Refcount: %d.\n  Done: %p vs %p",
              __func__,
              pid->cfg.name,
              atomic_read(&pid->ref.refcount),
              &done, pid->release_completion /* should be NULL */);
      for (i = 0; i < CONFIG_AVM_PA_MAX_SESSION; i++) {
         struct avm_pa_session *s = PA_SESSION(&pa_data, i);
         if (s->on_list != AVM_PA_LIST_FREE) {
            pa_show_session(s, pa_printk, KERN_CRIT);
            pr_crit("hw_session     : %p\n\n", avm_pa_get_hw_session(s));
         }
      }
      pr_crit("last tick     : %ld\n", (long)(jiffies - last_tick));
      pr_crit("next tick     : %ld\n", (long)(pa_glob.tick_timer.expires - jiffies));
      BUG();
   } else if (ret < 0) {
      /* Apparently we can get here during reboot. We continue without completing.
       * The PID is not fully unregistered yet but we have an additional
       * safe guard at registration to prevent double registration.
       */
      pr_err("avm_pa: %s: interrupted prematurely\n", __func__);
      pid->release_completion = NULL;
      return ret;
   } else {
      return 0; /* completed! */
   }
}
EXPORT_SYMBOL(avm_pa_dev_unregister_sync);

/* ------------------------------------------------------------------------ */
/* -------- pid extra functions ------------------------------------------- */
/* ------------------------------------------------------------------------ */

int
avm_pa_pid_set_hwinfo(avm_pid_handle pid_handle,
                      struct avm_pa_pid_hwinfo *hw)
{
   return avm_pa_pid_set_hwinfo2(pid_handle, hw, sizeof(*hw), GFP_ATOMIC);
}
EXPORT_SYMBOL(avm_pa_pid_set_hwinfo);

int
avm_pa_pid_set_hwinfo2(avm_pid_handle pid_handle,
                       struct avm_pa_pid_hwinfo *hw,
                       size_t sizeof_hwinfo,
                       gfp_t gfp)
{
   struct avm_pa_pid *pid = pa_pid_get_pid(pid_handle);

   /* Perhaps the module calling this must be recompiled */
   BUG_ON(hw && sizeof_hwinfo < sizeof(struct avm_pa_pid_hwinfo));
   BUG_ON(hw == NULL && sizeof_hwinfo != 0);

   if (!pid) {
      pr_err("avm_pa_pid_set_hwinfo: pid %u not registered\n",
             pid_handle);
      return -ENODEV;
   }
   kfree(pid->hw);
   pid->hw = NULL;

   if (hw) {
      pid->hw = kmemdup(hw, sizeof_hwinfo, gfp);
      if (!pid->hw) {
         pr_err("avm_pa_pid_set_hwinfo: kmalloc failed\n");
         pa_pid_put(pid_handle);
         return -ENOMEM;
      }
   }
   pa_pid_put(pid_handle);
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_set_hwinfo2);

struct avm_pa_pid_hwinfo *
avm_pa_pid_get_hwinfo(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid    *pid = PA_PID(ctx, pid_handle); /* no need to ref for read access */

   /* Allow to be called with pid_handle == 0, for convinience. */
   if (pid->pid_handle != pid_handle) {
      if (net_ratelimit())
         pr_err("avm_pa_pid_get_hwinfo: pid %u not registered\n",
                pid_handle);
      return NULL;
   }
   return pid->hw;
}
EXPORT_SYMBOL(avm_pa_pid_get_hwinfo);

int avm_pa_pid_activate_hw_accelaration(avm_pid_handle pid_handle)
{
   struct avm_pa_global   *ctx = &pa_glob;
   struct avm_pa_pid      *pid = pa_pid_get_pid(pid_handle);
   struct avm_hardware_pa *hwpa;

   if (!pid) {
      pr_err("avm_pa_pid_activate_hw_accelaration: pid %u not registered\n",
             pid_handle);
      return -1;
   }

   pr_info("avm_pa: try to activate hw accelaration for pid %u (%s) called from %pf\n",
           pid_handle, pid->cfg.name, (void *)_RET_IP_);
   hwpa = &ctx->hardware_pa;
   if (   pid->rx_channel_activated == 0
       && pid->ingress_framing == avm_pa_framing_ether
       && hwpa->alloc_rx_channel) {
      if ((*hwpa->alloc_rx_channel)(pid_handle) < 0) {
         pr_err("avm_pa: can't activate rx channel, pid %u (%s)\n",
                pid_handle, pid->cfg.name);
      } else {
         pid->rx_channel_stopped = 0;
         pid->rx_channel_activated = 1;
         pr_info("avm_pa: rx channel activated, pid %u (%s)\n",
                 pid_handle, pid->cfg.name);
      }
   }
   if (   pid->tx_channel_activated == 0
       && (   pid->egress_framing == avm_pa_framing_ether
           || pid->egress_framing == avm_pa_framing_ptype)
       && hwpa->alloc_tx_channel) {
      if ((*hwpa->alloc_tx_channel)(pid_handle) < 0) {
         pr_err("avm_pa: can't activate tx channel, pid %u (%s)\n",
                pid_handle, pid->cfg.name);
      } else {
         pid->tx_channel_activated = 1;
         pr_info("avm_pa: tx channel activated, pid %u (%s)\n",
                 pid_handle, pid->cfg.name);
      }
   }

   pa_pid_put(pid_handle);
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_activate_hw_accelaration);


/* Enables or disables a priority map */
int avm_pa_pid_prio_map_enable(avm_pid_handle pid_handle, unsigned short prio_map,
   int enable)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (pid_handle != pid->pid_handle) {
       pr_err("%s: pid handle %u not registered\n", __FUNCTION__, pid_handle);
       return -1;
   }
   if (prio_map >= AVM_PA_COUNT_PRIO_MAPS) {
      pr_err("%s: prio map %hu does not exist\n", __FUNCTION__, prio_map);
      return -2;
   }
   pid->prio_maps[prio_map].enabled = enable ? 1 : 0;
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_prio_map_enable);

/* Resets a priority map */
int avm_pa_pid_prio_map_reset(avm_pid_handle pid_handle, unsigned short prio_map)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (pid_handle != pid->pid_handle) {
       pr_err("%s: pid handle %u not registered\n", __FUNCTION__, pid_handle);
       return -1;
   }
   if (prio_map >= AVM_PA_COUNT_PRIO_MAPS) {
      pr_err("%s: prio map %hu does not exist\n", __FUNCTION__, prio_map);
      return -2;
   }
   memset(pid->prio_maps[prio_map].prios, 0, sizeof(pid->prio_maps[prio_map].prios));
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_prio_map_reset);

/* Sets the priority per queue */
int avm_pa_pid_prio_map_set_prio_per_queue(avm_pid_handle pid_handle, unsigned short
   prio_map, unsigned int queue, unsigned int prio)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (pid_handle != pid->pid_handle) {
      pr_err("%s: pid handle %u not registered\n", __FUNCTION__, pid_handle);
      return -1;
   }
   if (prio_map >= AVM_PA_COUNT_PRIO_MAPS) {
      pr_err("%s: prio map %hu does not exist\n", __FUNCTION__, prio_map);
      return -2;
   }
   if (queue >= AVM_PA_MAX_PRIOS) {
      pr_err("%s: prio map %hu queue %u out of bounds\n", __FUNCTION__,
         prio_map, queue);
      return -3;
   }
   /* A value of 0 for the prio parameter will render the underlying priority
    * unspecified. An unspecified priority will not be used for setting any
    * skb priority.
    */
   pid->prio_maps[prio_map].prios[queue] = prio;
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_prio_map_set_prio_per_queue);

int avm_pa_pid_activate_tcpackprio(avm_pid_handle pid_handle, int enable, unsigned int prio)
{
   /* Enable / disable the tack priority map to retain backwards compatibility with the old prioack procfs interface */
   if (avm_pa_pid_prio_map_enable(pid_handle, AVM_PA_PRIO_MAP_TACK, enable)) {
      return -1;
   }
   return avm_pa_pid_prio_map_set_prio_per_queue(pid_handle, AVM_PA_PRIO_MAP_TACK, AVM_PA_BE_QUEUE,
      enable ? prio : 0);
}
EXPORT_SYMBOL(avm_pa_pid_activate_tcpackprio);

int avm_pa_pid_activate_tgetprio(avm_pid_handle pid_handle, int enable, unsigned int prio)
{
   /* Enable / disable the tget priority map to retain backwards compatibility with the old prioack procfs interface */
   if (avm_pa_pid_prio_map_enable(pid_handle, AVM_PA_PRIO_MAP_TGET, enable)) {
      return -1;
   }
   return avm_pa_pid_prio_map_set_prio_per_queue(pid_handle, AVM_PA_PRIO_MAP_TGET, AVM_PA_BE_QUEUE,
      enable ? prio : 0);
}
EXPORT_SYMBOL(avm_pa_pid_activate_tgetprio);

int avm_pa_register_hardware_pa(struct avm_hardware_pa *pa_functions)
{
   struct avm_pa_global *ctx = &pa_glob;

   if (!pa_functions || (pa_functions->flags & ~AVM_HW_F_ALL))
      return -EINVAL;

   if (!pa_hw_pa_valid(pa_functions))
      return -EINVAL;

   if (pa_hw_pa_valid(&ctx->hardware_pa)) {
      pr_err("avm_pa: hardware_pa already registered\n");
      return -EADDRINUSE;
   }

   if (pa_hw_pa_get()) {
      /* We can only get here if an avm_pa_unregister_hardware_pa() call
       * didn't complete yet.
       */
      pr_err("avm_pa: deregistration pending\n");
      pa_hw_pa_put();
      return -EAGAIN;
   }

   kref_init(&ctx->hw_pa_ref);
   ctx->hardware_pa = *pa_functions;
   if (pa_functions->alloc_tx_channel || pa_functions->alloc_rx_channel) {
      avm_pid_handle n;
      for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
         struct avm_pa_pid *pid = PA_PID(ctx, n);
         if (   pid->pid_handle == n
             && pid->egress_framing == avm_pa_framing_ptype) {
            avm_pa_pid_activate_hw_accelaration(n);
         }
      }
   }
   return 0;
}
EXPORT_SYMBOL(avm_pa_register_hardware_pa);

int avm_pa_unregister_hardware_pa(struct avm_hardware_pa *pa_functions, struct completion *done)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_hardware_pa *hwpa = &ctx->hardware_pa;
   int n;

   if (!pa_functions)
      return -ENODEV;

   BUG_ON(hwpa->add_session != pa_functions->add_session);
   BUG_ON(hwpa->add_session_skb != pa_functions->add_session_skb);

   /* Stop adding hw sessions. the read side might still have a cached pointer
    * and add sessions but this is OK since they hold a ref on the hw_pa
    * and we're not doing the complete(done) here.
    */
   rcu_assign_pointer(hwpa->add_session, NULL);
   rcu_assign_pointer(hwpa->add_session_skb, NULL);

   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      struct avm_pa_pid *pid = pa_pid_get_pid(n);
      if (pid) {
         pid->rx_channel_stopped = 1;
         if (pid->tx_channel_activated) {
            pid->tx_channel_activated = 0;
            if (hwpa->free_tx_channel)
               hwpa->free_tx_channel(n);
         }
         if (pid->rx_channel_activated) {
            pid->rx_channel_activated = 0;
            if (hwpa->free_rx_channel)
               hwpa->free_rx_channel(n);
         }
         avm_pa_pid_put(n);
      }
   }

   ctx->hw_pa_flush_completion = done;
   if (!pa_hw_pa_put()) {
      /* kill all sessions in hw pa if necessary. pa_hw_pa_put returns 0
       * if any session is in_hw (has a reference).
       */
      avm_pa_flush_hw_sessions();
   }
   return 0;
}
EXPORT_SYMBOL(avm_pa_unregister_hardware_pa);

int avm_pa_unregister_hardware_pa_sync(struct avm_hardware_pa *pa_functions)
{
   DECLARE_COMPLETION_ONSTACK(done);
   int ret;
   struct avm_pa_global *ctx = &pa_glob;

   might_sleep();
   ret = avm_pa_unregister_hardware_pa(pa_functions, &done);

   /* Normally there is no way to block indefinitely, but mark killable
    * in case of a bug somewhere.
    */
   if (ret == 0 && wait_for_completion_killable(&done)) {
      pr_err("avm_pa: %s: interrupted prematurely\n", __func__);
      /* If we get here we must continue before completing.
       * The hardware_pa is not fully unregistered yet but there are additional
       * safe guards at registration to prevent double registration.
       */
      ctx->hw_pa_flush_completion = NULL;
   }

   return ret;
}
EXPORT_SYMBOL(avm_pa_unregister_hardware_pa_sync);


int avm_pa_is_hardware_pa_active(void)
{
   struct avm_pa_global *ctx = &pa_glob;

   return pa_hw_pa_valid(&ctx->hardware_pa) && !ctx->hw_ppa_disabled;
}
EXPORT_SYMBOL(avm_pa_is_hardware_pa_active);

/* ------------------------------------------------------------------------ */

static void pa_show_brief_status_header(pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   const char *mode;

   if (ctx->disabled)
      mode = "disabled";
   else if (ctx->fw_disabled)
      mode = "testmode";
   else if (avm_pa_capture_running())
      mode = "capture";
   else
      mode = "enabled";
   (*fprintffunc)(arg, "State          : %s\n", mode);

   if (pa_hw_pa_valid(&ctx->hardware_pa)) {
      mode = ctx->hw_ppa_disabled ? "disabled" : "enable";
      (*fprintffunc)(arg, "HW State       : %s\n", mode);
   }

   if (pa_hw_pa_valid(&ctx->hardware_pa) && (ctx->hardware_pa.flags & AVM_HW_F_NO_BSESSION))
      mode = "no (by hw)";
   else if (!ctx->bsession_allowed)
      mode = "no";
   else
      mode = "yes";
   (*fprintffunc)(arg, "BSession allow : %s\n", mode);

   if (ctx->filter_enabled && list_empty(&ctx->accel_filter))
      mode = "empty";
   else if (ctx->filter_enabled)
      mode = "yes";
   else
      mode = "no";
   (*fprintffunc)(arg, "Filter active  : %s\n", mode);
}


static void pa_show_num_sessions(pa_fprintf fprintffunc, void *arg, int right_align)
{
   struct avm_pa_global *ctx = &pa_glob;

   (*fprintffunc)(arg, "BSessions      : %*u\n", right_align,
                       (unsigned)ctx->stats.nbsessions);
   (*fprintffunc)(arg, "Sessions       : %*hu\n", right_align,
                       ctx->sess_list[AVM_PA_LIST_ACTIVE].nsessions);
   /* There is a ref for every in_hw session plus one that's taken on registration */
   (*fprintffunc)(arg, "HW Sessions    : %*d\n", right_align,
                       atomic_read(&ctx->hw_pa_ref.refcount) - (ctx->hardware_pa.add_session ? 1 : 0));
   (*fprintffunc)(arg, "Max Sessions   : %*hu\n", right_align,
                       ctx->sess_list[AVM_PA_LIST_ACTIVE].maxsessions);
   (*fprintffunc)(arg, "Sessions (dead): %*hu\n", right_align,
                       ctx->sess_list[AVM_PA_LIST_DEAD].nsessions);
   (*fprintffunc)(arg, "Sessions (free): %*hu\n", right_align,
                       ctx->sess_list[AVM_PA_LIST_FREE].nsessions);
}


static void pa_show_linux_banner(pa_fprintf fprintffunc, void *arg)
{
   struct new_utsname *uts;

   uts = utsname();
   BUG_ON(!uts);

   /* cp. fs/proc/version.c, v2.6.27..v4.16+: */
   (*fprintffunc)(arg, linux_proc_banner,
                  uts->sysname, uts->release, uts->version);
}


static void pa_show_brief(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;
   unsigned free_egress = 0;
   struct hlist_node *node;

   (*fprintffunc)(arg, "Version        : For ");
   pa_show_linux_banner(fprintffunc, arg);

   pa_show_brief_status_header(fprintffunc, arg);
   pa_show_num_sessions(fprintffunc, arg, 0);

   hlist_for_each(node, &ctx->egress_freelist)
      ++free_egress;

   (*fprintffunc)(arg, "Egress pool    : %u/%u\n",
                       free_egress, ARRAY_SIZE(pa_data.egress_pool));
   (*fprintffunc)(arg, "Queuelen       : %lu\n",
                       (unsigned long)skb_queue_len(&ctx->tbfqueue));
   (*fprintffunc)(arg, "Rx pkts/secs   : %lu\n",
                       (unsigned long)ctx->stats.rx_pps);
   if (ctx->tbf_enabled) {
      (*fprintffunc)(arg, "Limit pkts/sec : %lu\n",
                          (unsigned long)ctx->rate);
   }
   (*fprintffunc)(arg, "Fw pkts/sec    : %lu\n",
                       (unsigned long)ctx->stats.fw_pps);
   (*fprintffunc)(arg, "Ov pkts/sec    : %lu\n",
                       (unsigned long)ctx->stats.overlimit_pps);
   (*fprintffunc)(arg, "Rx pakets      : %lu\n",
                       (unsigned long)ctx->stats.rx_pkts);
   (*fprintffunc)(arg, "Rx bypass      : %lu\n",
                       (unsigned long)ctx->stats.rx_bypass);
   (*fprintffunc)(arg, "Rx ttl <= 1    : %lu\n",
                       (unsigned long)ctx->stats.rx_ttl);
   (*fprintffunc)(arg, "Rx broadcast   : %lu\n",
                       (unsigned long)ctx->stats.rx_broadcast);
   (*fprintffunc)(arg, "Rx search      : %lu\n",
                       (unsigned long)ctx->stats.rx_search);
   (*fprintffunc)(arg, "Rx match       : %lu\n",
                       (unsigned long)ctx->stats.rx_match);
   (*fprintffunc)(arg, "Rx modified    : %lu\n",
                       (unsigned long)ctx->stats.rx_mod);
   (*fprintffunc)(arg, "Fw pakets      : %lu\n",
                       (unsigned long)ctx->stats.fw_pkts);
   (*fprintffunc)(arg, "Fw local       : %lu\n",
                       (unsigned long)ctx->stats.fw_local);
   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, n);
      unsigned long rx, tx;
      if (vpid->vpid_handle == 0) continue;
      rx =   vpid->stats.rx_unicast_pkt
           + vpid->stats.rx_multicast_pkt
           + vpid->stats.rx_broadcast_pkt;
      tx =   vpid->stats.tx_unicast_pkt
           + vpid->stats.tx_multicast_pkt
           + vpid->stats.tx_broadcast_pkt;
      (*fprintffunc)(arg, "VPID%-2d: RX %10lu TX %10lu %s\n",
                          vpid->vpid_handle, rx, tx, vpid->cfg.name);
   }
}

static void
pa_show_memory(pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;

   pa_show_brief_status_header(fprintffunc, arg);

#define FMT "%4zd.%02d KB"
#define ARG(x) (x)/1000, ((x)%1000)/10

   (*fprintffunc)(arg, "avm_pa_global  : " FMT "\n", ARG(sizeof(struct avm_pa_global)));
   (*fprintffunc)(arg, "avm_pa_data    : " FMT "\n", ARG(sizeof(struct avm_pa_data)));
   (*fprintffunc)(arg, "global + data  : " FMT "\n", ARG(sizeof(struct avm_pa_global) + sizeof(struct avm_pa_data)));
   (*fprintffunc)(arg, "One session    : " FMT "\n", ARG(sizeof(struct avm_pa_session)));
   (*fprintffunc)(arg, "All sessions   : " FMT "\n", ARG(sizeof(pd->sessions)));
   (*fprintffunc)(arg, "One bsession   : " FMT "\n", ARG(sizeof(struct avm_pa_bsession)));
   (*fprintffunc)(arg, "All bsessions  : " FMT "\n", ARG(sizeof(ctx->bsess_array)));
   (*fprintffunc)(arg, "One ingress    : " FMT "\n", ARG(sizeof(struct avm_pa_pkt_match)));
   (*fprintffunc)(arg, "One egress     : " FMT "\n", ARG(sizeof(struct avm_pa_egress)));
   (*fprintffunc)(arg, "Egress pool    : " FMT "\n", ARG(sizeof(pd->egress_pool)));
   (*fprintffunc)(arg, "One macaddr    : " FMT "\n", ARG(sizeof(struct avm_pa_macaddr)));
   (*fprintffunc)(arg, "All macaddrs   : " FMT "\n", ARG(sizeof(ctx->macaddr_array)));
   (*fprintffunc)(arg, "One pid        : " FMT "\n", ARG(sizeof(struct avm_pa_pid)));
   (*fprintffunc)(arg, "All pids       : " FMT "\n", ARG(sizeof(ctx->pid_array)));
   (*fprintffunc)(arg, "One vpid       : " FMT "\n", ARG(sizeof(struct avm_pa_vpid)));
   (*fprintffunc)(arg, "All vpids      : " FMT "\n", ARG(sizeof(ctx->vpid_array)));
   (*fprintffunc)(arg, "Stats          : " FMT "\n", ARG(sizeof(struct avm_pa_stats)));
   (*fprintffunc)(arg, "TOK Stats      : " FMT "\n", ARG(ctx->tok_end - ctx->tok_start));
   (*fprintffunc)(arg, "Estimator data : " FMT "\n", ARG(ctx->est_end - ctx->est_start));
}


static void
pa_show_stats(pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   pa_show_num_sessions(fprintffunc, arg, 9);

#define PRINT_STAT(t, member) do {                                                       \
      (*fprintffunc)(arg, "%-15s: %9lu (%+7ld)\n", t,                                    \
         (unsigned long)ctx->stats. member,                                              \
         (long)(ctx->stats. member - ctx->stats_copy. member)) ;                         \
   } while(0)

   PRINT_STAT("Rx packets/sec",  rx_pps);
   PRINT_STAT("Fw packets/sec",  fw_pps);
   PRINT_STAT("Ov packets/sec",  overlimit_pps);
   PRINT_STAT("Rx pakets",       rx_pkts);
   PRINT_STAT("Rx bypass",       rx_bypass);
   PRINT_STAT("Rx frag list",    rx_frag_list);
   PRINT_STAT("Rx ttl <= 1",     rx_ttl);
   PRINT_STAT("Rx broadcast",    rx_broadcast);
   PRINT_STAT("Rx search",       rx_search);
   PRINT_STAT("Rx match",        rx_match);
   PRINT_STAT("Rx lisp changed", rx_lispchanged);
   PRINT_STAT("Rx df",           rx_df);
   PRINT_STAT("Rx modified",     rx_mod);
   PRINT_STAT("Rx overlimit",    rx_overlimit);
   PRINT_STAT("Rx dropped",      rx_dropped);
   PRINT_STAT("Rx irq",          rx_irq);
   PRINT_STAT("Rx irq dropped",  rx_irqdropped);
   PRINT_STAT("Rx hroom",        rx_headroom_too_small);
   PRINT_STAT("Rx hroom fail",   rx_realloc_headroom_failed);
   PRINT_STAT("Fw pakets",       fw_pkts);
   PRINT_STAT("Fw output",       fw_output);
   PRINT_STAT("Fw output drop",  fw_output_drop);
   PRINT_STAT("Fw local",        fw_local);
   PRINT_STAT("Fw rtp",          fw_rtp);
   PRINT_STAT("Fw rtp drop",     fw_rtp_drop);
   PRINT_STAT("Fw illegal",      fw_ill);
   PRINT_STAT("Fw frags",        fw_frags);
   PRINT_STAT("Fw drop",         fw_drop);
   PRINT_STAT("Fw drop gone",    fw_drop_gone);
   PRINT_STAT("Fw fail",         fw_fail);
   PRINT_STAT("Fw frag fail",    fw_frag_fail);
   PRINT_STAT("Tx accelerated",  tx_accelerated);
   PRINT_STAT("Tx local",        tx_local);
   PRINT_STAT("Tx already",      tx_already);
   PRINT_STAT("Tx bypass",       tx_bypass);
   PRINT_STAT("Tx sess error",   tx_sess_error);
   PRINT_STAT("Tx sess ok",      tx_sess_ok);
   PRINT_STAT("Tx sess exists",  tx_sess_exists);
   PRINT_STAT("Tx egress error", tx_egress_error);
   PRINT_STAT("Tx egress ok",    tx_egress_ok);
   PRINT_STAT("Tx pid change",   tx_pid_change);
   PRINT_STAT("Tx fast gso",     tx_fast_gso);
   PRINT_STAT("Loc sess error",  local_sess_error);
   PRINT_STAT("Loc sess ok",     local_sess_ok);
   PRINT_STAT("Loc sess exists", local_sess_exists);
   PRINT_STAT("XFRM sess ok",    xfrm_sess_ok);
   PRINT_STAT("RTP sess error",  rtp_sess_error);
   PRINT_STAT("RTP sess ok",     rtp_sess_ok);
   PRINT_STAT("RTP sess exists", rtp_sess_exists);
   PRINT_STAT("TBF schedule",    tbf_schedule);
   PRINT_STAT("TBF reschedule",  tbf_reschedule);
#ifdef CONFIG_AVM_PA_RPS
   {
      int i;
      for (i = 0; i < CONFIG_AVM_PA_RPS_QUEUES; i++) {
         (*fprintffunc)(arg, "RPS enqueue %2d : %9lu\n", i, ctx->rps[i].rx_enqueued);
         (*fprintffunc)(arg, "RPS ipis    %2d : %9lu\n", i, ctx->rps[i].rx_rps_ipis);
         (*fprintffunc)(arg, "RPS dequeue %2d : %9lu\n", i, ctx->rps[i].rx_dequeued);
      }
   }
#endif
   PRINT_STAT("sess flushed",    sess_flushed);
   PRINT_STAT("sess timedout",   sess_timedout);
   PRINT_STAT("sess pid change", sess_pidchanged);
   PRINT_STAT("rxch no rx slow", rx_channel_no_rx_slow);
   PRINT_STAT("rxch stopped",    rx_channel_stopped);
   PRINT_STAT("txch dropped",    tx_channel_dropped);
   PRINT_STAT("user msecs/sec",  userms);
   PRINT_STAT("idle msecs/sec",  idlems);
   PRINT_STAT("irq msecs/sec",   irqms);

   ctx->stats_copy = ctx->stats;
};


static void pa_show_status(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   char *mode;

   pa_show_brief_status_header(fprintffunc, arg);

   switch (ctx->load_control) {
      case LOADCONTROL_IDLE: mode = "idle"; break;
      case LOADCONTROL_POWER:   mode = "power"; break;
      case LOADCONTROL_IRQ:      mode = "irq"; break;
      case LOADCONTROL_POWERIRQ: mode = "powerirq"; break;
      default: mode = "????"; break;
   }
   (*fprintffunc)(arg, "Loadcontrol    : %s\n", mode);
   (*fprintffunc)(arg, "IDLE mswin     : %u %u\n",
                       ctx->idle_mswin_low, ctx->idle_mswin_high);
   (*fprintffunc)(arg, "IRQ mswin      : %u %u\n",
                       ctx->irq_mswin_low, ctx->irq_mswin_high);
#if AVM_LOAD_CONTROL_ENABLED
   (*fprintffunc)(arg, "TelephonyReduce: %u\n", ctx->telephony_reduce);
#else
   (*fprintffunc)(arg, "TelephonyReduce: Disabled\n");
#endif
   (*fprintffunc)(arg, "Maxrate        : %u\n", ctx->maxrate);
   mode = ctx->tbf_enabled ? "enabled" : "disabled";
   (*fprintffunc)(arg, "TBF            : %s\n", mode);
   (*fprintffunc)(arg, "Limit Rate     : %u\n", ctx->rate);
   (*fprintffunc)(arg, "Current Rate   : %lu\n",
                       (unsigned long)ctx->stats.fw_pps);
   (*fprintffunc)(arg, "user msecs/sec : %lu\n",
                       (unsigned long)ctx->stats.userms);
   (*fprintffunc)(arg, "idle msecs/sec : %lu\n",
                       (unsigned long)ctx->stats.idlems);
   (*fprintffunc)(arg, "irq msecs/sec  : %lu\n",
                       (unsigned long)ctx->stats.irqms);
}

static void pa_show_vpids(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;

   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, n);
      if (vpid->vpid_handle == 0) continue;
      (*fprintffunc)(arg, "VPID%-2d: %4d/%4d  %s\n",
                          vpid->vpid_handle,
                          vpid->cfg.v4_mtu,
                          vpid->cfg.v6_mtu,
                          vpid->cfg.name);
      (*fprintffunc)(arg, "       %10s %10s %10s %10s %10s %10s %10s %10s\n",
                               "unicast",
                               "multicast",
                               "broadcast",
                               "discard",
                               "error",
                               "bytes",
                               "mc bytes",
                               "bc bytes");
      (*fprintffunc)(arg, "  RX   %10lu %10lu %10lu %10lu %10s %10Lu %10Lu %10Lu\n",
                               (unsigned long)vpid->stats.rx_unicast_pkt,
                               (unsigned long)vpid->stats.rx_multicast_pkt,
                               (unsigned long)vpid->stats.rx_broadcast_pkt,
                               (unsigned long)vpid->stats.rx_discard,
                               "-",
                               (unsigned long long)vpid->stats.rx_bytes,
                               (unsigned long long)vpid->stats.rx_multicast_bytes,
                               (unsigned long long)vpid->stats.rx_broadcast_bytes);
      (*fprintffunc)(arg, "  TX   %10lu %10lu %10lu %10lu %10lu %10Lu\n",
                               (unsigned long)vpid->stats.tx_unicast_pkt,
                               (unsigned long)vpid->stats.tx_multicast_pkt,
                               (unsigned long)vpid->stats.tx_broadcast_pkt,
                               (unsigned long)vpid->stats.tx_discard,
                               (unsigned long)vpid->stats.tx_error,
                               (unsigned long long)vpid->stats.tx_bytes);
   }
}

static void pa_show_vpids_hw_stats(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;

   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, n);
      unsigned int prio;
      if (vpid->vpid_handle == 0) continue;
      (*fprintffunc)(arg, "VPID %-2d () %s\n",
                          vpid->vpid_handle,
                          vpid->cfg.name);
      for (prio = 0; prio < AVM_PA_MAX_PRIOS; prio++) {
         if (vpid->hw_stats[prio].pkts || vpid->hw_stats[prio].bytes) {
            (*fprintffunc)(arg, "    %u: %lu pkts / %llu bytes\n",
                           prio,
                           (unsigned long)vpid->hw_stats[prio].pkts,
                           (unsigned long long)vpid->hw_stats[prio].bytes);
         }
      }
   }
}

static void pa_show_vpids_all_stats(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;

   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, n);
      unsigned int prio;
      if (vpid->vpid_handle == 0)
         continue;
      (*fprintffunc)(arg, "VPID %-2d () %s\n",
                          vpid->vpid_handle,
                          vpid->cfg.name);

      (*fprintffunc)(arg, "   Egress:\n");
      for (prio = 0; prio < AVM_PA_MAX_PRIOS; prio++) {
         unsigned long sw_pkts, hw_pkts;
         unsigned long long sw_bytes, hw_bytes;
         unsigned long asw_pkts, ahw_pkts;
         unsigned long long asw_bytes, ahw_bytes;

         sw_pkts = vpid->sw_stats[prio].pkts + vpid->slow_sw_stats[prio].pkts;
         hw_pkts = vpid->hw_stats[prio].pkts;
         asw_pkts = vpid->associated_sw_stats[prio].pkts;
         ahw_pkts = vpid->associated_hw_stats[prio].pkts;
         sw_bytes = vpid->sw_stats[prio].bytes + vpid->slow_sw_stats[prio].bytes;
         hw_bytes = vpid->hw_stats[prio].bytes;
         asw_bytes = vpid->associated_sw_stats[prio].bytes;
         ahw_bytes = vpid->associated_hw_stats[prio].bytes;

         if (   sw_pkts || hw_pkts || asw_pkts || ahw_pkts
             || sw_bytes || hw_bytes || asw_bytes || ahw_bytes) {
            (*fprintffunc)(arg, "   %u: pkts ", prio);
            (*fprintffunc)(arg, " %lu+%lu = %lu",
                                    sw_pkts, hw_pkts,
                                    sw_pkts + hw_pkts);
            (*fprintffunc)(arg, " / %lu+%lu = %lu\n",
                                    asw_pkts, ahw_pkts,
                                    asw_pkts + ahw_pkts);

            (*fprintffunc)(arg, "   %u: bytes", prio);
            (*fprintffunc)(arg, " %llu+%llu = %llu",
                                    sw_bytes, hw_bytes,
                                    sw_bytes + hw_bytes);
            (*fprintffunc)(arg, " / %llu+%llu = %llu\n",
                                    asw_bytes, ahw_bytes,
                                    asw_bytes + ahw_bytes);
         }
      }
      (*fprintffunc)(arg, "   Ingress:\n");
      for (prio = 0; prio < AVM_PA_MAX_PRIOS; prio++) {
         unsigned long sw_pkts, hw_pkts;
         unsigned long long sw_bytes, hw_bytes;
         unsigned long asw_pkts, ahw_pkts;
         unsigned long long asw_bytes, ahw_bytes;

         sw_pkts = vpid->ingress_sw_stats[prio].pkts + vpid->ingress_slow_sw_stats[prio].pkts;
         hw_pkts = vpid->ingress_hw_stats[prio].pkts;
         asw_pkts = vpid->associated_ingress_sw_stats[prio].pkts;
         ahw_pkts = vpid->associated_ingress_hw_stats[prio].pkts;
         sw_bytes = vpid->ingress_sw_stats[prio].bytes + vpid->ingress_slow_sw_stats[prio].bytes;
         hw_bytes = vpid->ingress_hw_stats[prio].bytes;
         asw_bytes = vpid->associated_ingress_sw_stats[prio].bytes;
         ahw_bytes = vpid->associated_ingress_hw_stats[prio].bytes;

         if (   sw_pkts || hw_pkts || asw_pkts || ahw_pkts
             || sw_bytes || hw_bytes || asw_bytes || ahw_bytes) {
            (*fprintffunc)(arg, "   %u: pkts ", prio);
            (*fprintffunc)(arg, " %lu+%lu = %lu",
                                    sw_pkts, hw_pkts,
                                    sw_pkts + hw_pkts);
            (*fprintffunc)(arg, " / %lu+%lu = %lu\n",
                                    asw_pkts, ahw_pkts,
                                    asw_pkts + ahw_pkts);

            (*fprintffunc)(arg, "   %u: bytes", prio);
            (*fprintffunc)(arg, " %llu+%llu = %llu",
                                    sw_bytes, hw_bytes,
                                    sw_bytes + hw_bytes);
            (*fprintffunc)(arg, " / %llu+%llu = %llu\n",
                                    asw_bytes, ahw_bytes,
                                    asw_bytes + ahw_bytes);
         }
      }
   }
}

void avm_pa_dev_set_ipv4_mtu(struct avm_pa_dev_info *devinfo, u16 mtu)
{
   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      PA_VPID(ctx, devinfo->vpid_handle)->cfg.v4_mtu = mtu;
   }
}
EXPORT_SYMBOL(avm_pa_dev_set_ipv4_mtu);

void avm_pa_dev_set_ipv6_mtu(struct avm_pa_dev_info *devinfo, u16 mtu)
{
   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      PA_VPID(ctx, devinfo->vpid_handle)->cfg.v6_mtu = mtu;
   }
}
EXPORT_SYMBOL(avm_pa_dev_set_ipv6_mtu);

static inline bool
should_add_slow_stats(struct avm_pa_vpid *vpid)
{
   struct avm_pa_global *ctx = &pa_glob;
   ktime_t now;

   /* only add slow stats if the timer doesn't do it regularly
    *
    * The tick timer provides clocked statistics, according to vpid->timestamp.
    * If we added slow stats outside the clock they would be wrong with regards to
    * the timestamp.
    *
    * If the tick isn't running because there are no accelerated sessions,
    * we can pass slow stats directly, with timestamp as of now.
    **/
   if (!timer_pending(&ctx->tick_timer))
      return true;

   /* There is one special case: if the timer has been started but didn't elapse yet.
    * So the timestamp value set by the tick must be valid (recent) also. */
   now = ktime_get_boottime();
   if (ktime_us_delta(now, vpid->stats.timestamp) >= jiffies_to_usecs(AVM_PA_TICK_RATE))
      return true;

   return false;
}


int
avm_pa_dev_get_stats(struct avm_pa_dev_info *devinfo, struct avm_pa_vpid_stats *stats)
{
   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
      if (vpid->vpid_handle == devinfo->vpid_handle) {
         *stats = vpid->stats;
         if (should_add_slow_stats(vpid)) {
            read_lock_bh(&vpid->slow_stats_lock);
#define ADD_COUNTER(field) (vpid->stats.field += vpid->slow_stats.field)
            ADD_COUNTER(rx_unicast_pkt);
            ADD_COUNTER(rx_multicast_pkt);
            ADD_COUNTER(rx_broadcast_pkt);
            ADD_COUNTER(rx_bytes);
            ADD_COUNTER(rx_multicast_bytes);
            ADD_COUNTER(rx_broadcast_bytes);
            ADD_COUNTER(rx_discard);
            ADD_COUNTER(tx_unicast_pkt);
            ADD_COUNTER(tx_multicast_pkt);
            ADD_COUNTER(tx_broadcast_pkt);
            ADD_COUNTER(tx_bytes);
            ADD_COUNTER(tx_error);
            ADD_COUNTER(tx_discard);
#undef ADD_COUNTER
            read_unlock_bh(&vpid->slow_stats_lock);
            stats->timestamp = ktime_get_boottime();
         }
         return 0;
      }
   }
   memset(stats, 0, sizeof(struct avm_pa_vpid_stats));
   return -1;
}
EXPORT_SYMBOL(avm_pa_dev_get_stats);

int avm_pa_dev_get_hw_stats(struct avm_pa_dev_info *devinfo,
                            struct avm_pa_traffic_stats *stats,
                            unsigned int prio)
{
    if (prio >= AVM_PA_MAX_PRIOS) return -1;

    if (devinfo->vpid_handle) {
       struct avm_pa_global *ctx = &pa_glob;
       struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
       if (vpid->vpid_handle == devinfo->vpid_handle) {
          stats->pkts = vpid->hw_stats[prio].pkts;
          stats->bytes = vpid->hw_stats[prio].bytes;
          return 0;
       }
    }
    memset(stats, 0, sizeof(struct avm_pa_traffic_stats));
    return -1;
}
EXPORT_SYMBOL(avm_pa_dev_get_hw_stats);


int
avm_pa_dev_get_prio_stats(struct avm_pa_dev_info *devinfo,
                          struct avm_pa_prio_stats *stats,
                          unsigned int prio)
{
   if (prio >= AVM_PA_MAX_PRIOS) return -1;

   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
      if (vpid->vpid_handle == devinfo->vpid_handle) {
         stats->sw = vpid->sw_stats[prio];
         stats->associated_sw = vpid->associated_sw_stats[prio];
         stats->hw = vpid->hw_stats[prio];
         stats->associated_hw = vpid->associated_hw_stats[prio];
         stats->timestamp = vpid->stats.timestamp;
         if (should_add_slow_stats(vpid)) {
            /* only add slow stats if the timer doesn't do it regularly */
            read_lock_bh(&vpid->slow_stats_lock);
            stats->sw.bytes += vpid->slow_sw_stats[prio].bytes;
            stats->sw.pkts  += vpid->slow_sw_stats[prio].pkts;
            read_unlock_bh(&vpid->slow_stats_lock);
            stats->timestamp = ktime_get_boottime();
         }
         return 0;
      }
   }
   memset(stats, 0, sizeof(struct avm_pa_prio_stats));
   return -1;
}
EXPORT_SYMBOL(avm_pa_dev_get_prio_stats);

int
avm_pa_dev_get_ingress_prio_stats(struct avm_pa_dev_info *devinfo,
                                  struct avm_pa_prio_stats *stats,
                                  unsigned int prio)
{
   if (prio >= AVM_PA_MAX_PRIOS) return -1;

   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
      if (vpid->vpid_handle == devinfo->vpid_handle) {
         stats->sw = vpid->ingress_sw_stats[prio];
         stats->associated_sw = vpid->associated_ingress_sw_stats[prio];
         stats->hw = vpid->ingress_hw_stats[prio];
         stats->associated_hw = vpid->associated_ingress_hw_stats[prio];
         stats->timestamp = vpid->stats.timestamp;
         if (should_add_slow_stats(vpid)) {
            /* only add slow stats if the timer doesn't do it regularly */
            read_lock_bh(&vpid->slow_stats_lock);
            stats->sw.bytes += vpid->ingress_slow_sw_stats[prio].bytes;
            stats->sw.pkts  += vpid->ingress_slow_sw_stats[prio].pkts;
            read_unlock_bh(&vpid->slow_stats_lock);
            stats->timestamp = ktime_get_boottime();
         }
         return 0;
      }
   }
   memset(stats, 0, sizeof(struct avm_pa_prio_stats));
   return -1;
}
EXPORT_SYMBOL(avm_pa_dev_get_ingress_prio_stats);

int avm_pa_dev_reset_stats(struct avm_pa_dev_info *devinfo)
{
   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
      if (vpid->vpid_handle == devinfo->vpid_handle) {
         memset(&vpid->stats, 0, sizeof(struct avm_pa_vpid_stats));
         memset(vpid->sw_stats, 0, sizeof(vpid->sw_stats));
         memset(vpid->hw_stats, 0, sizeof(vpid->hw_stats));
         write_lock_bh(&vpid->slow_stats_lock);
         memset(&vpid->slow_stats, 0, sizeof(struct avm_pa_vpid) - offsetof(struct avm_pa_vpid, slow_stats));
         write_unlock_bh(&vpid->slow_stats_lock);
         return 0;
      }
   }
   return -1;
}
EXPORT_SYMBOL(avm_pa_dev_reset_stats);


static void
pa_flush_sessions_selective(bool (*match_session)(struct avm_pa_session *sess, va_list args),
                            const char *reason,
                            ...)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   struct avm_pa_session_list *list = &ctx->sess_list[AVM_PA_LIST_ACTIVE];
   unsigned count = 0;
   va_list ap;

   /* There is a linker error on mips if the nested functions access stack variables
    * of the outer functions. So we pass them va variadic args. Change to static
    * functions if this also breaks down */
   va_start(ap, reason);

   /* We are potentially called from process context. Make sure this is called
    * rarely in softirq, try to use pa_session_flush() directly. */
   rcu_read_lock_bh();
   list_for_each_entry_rcu(session, &list->sessions, session_list) {
      if (match_session(session, ap)) {
         pa_session_flush(session, reason);
         count += 1;
      }
   }
   rcu_read_unlock_bh();

   va_end(ap);
   ctx->stats.sess_flushed += count;
}


void avm_pa_flush_sessions(void)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      return true;
   }
   pa_flush_sessions_selective(fn, "flush");
}
EXPORT_SYMBOL(avm_pa_flush_sessions);

static void avm_pa_flush_bsessions(void)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      return session->bsession != NULL;
   }
   pa_flush_sessions_selective(fn, "bsession flush");
}

static void avm_pa_flush_hw_sessions(void)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      return session->in_hw != 0;
   }
   pa_flush_sessions_selective(fn, "hw flush");
}


void avm_pa_flush_lispencap_sessions(void)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      return session->mod.pkttype & AVM_PA_PKTTYPE_LISP;
   }
   pa_flush_sessions_selective(fn, "lispencap flush");
}
EXPORT_SYMBOL(avm_pa_flush_lispencap_sessions);


void avm_pa_flush_rtp_session(struct sock *sk)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      struct avm_pa_egress *egress = avm_pa_first_egress(session);
      return egress->type == avm_pa_egresstype_rtp
          && egress->rtp.sk == va_arg(args, struct sock *);
   }
   pa_flush_sessions_selective(fn, "rtp flush", sk);
}
EXPORT_SYMBOL(avm_pa_flush_rtp_session);


void avm_pa_flush_multicast_sessions(void)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      return session->ingress.casttype == AVM_PA_IS_MULTICAST;
   }
   pa_flush_sessions_selective(fn, "multicast flush");
}
EXPORT_SYMBOL(avm_pa_flush_multicast_sessions);


void avm_pa_flush_multicast_sessions_for_group(u32 group)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      if (session->ingress.casttype == AVM_PA_IS_MULTICAST) {
         int i;
         for (i = 0; i < session->ingress.nmatch; i++) {
            struct avm_pa_match_info *p = &session->ingress.match[i];
            if (p->type == AVM_PA_IPV4) {
               hdrunion_t *hdr = (hdrunion_t *)&session->ingress.hdrcopy[p->offset + session->ingress.hdroff];
               if (va_arg(args, u32) == hdr->iph.daddr)
                  return true;
            }
         }
      }
      return false;
   }
   pa_flush_sessions_selective(fn, "multicast flush", group);
}
EXPORT_SYMBOL(avm_pa_flush_multicast_sessions_for_group);


void avm_pa_flush_sessions_for_vpid(avm_vpid_handle vpid_handle)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      avm_vpid_handle vpid_handle = (avm_vpid_handle) va_arg(args, int); 
      if (session->ingress_vpid_handle == vpid_handle) {
         return true;
      } else {
         struct avm_pa_egress *egress;
         avm_pa_for_each_egress(egress, session) {
            if (egress->vpid_handle == vpid_handle)
               return true;
         }
         return false;
      }
   }
   pa_flush_sessions_selective(fn, "vpid flush", (int) vpid_handle);
}
EXPORT_SYMBOL(avm_pa_flush_sessions_for_vpid);


void avm_pa_flush_sessions_for_pid(avm_pid_handle pid_handle)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      avm_vpid_handle pid_handle = (avm_pid_handle) va_arg(args, int);
      if (session->ingress_pid_handle == pid_handle) {
         return true;
      } else {
         struct avm_pa_egress *egress;
         avm_pa_for_each_egress(egress, session) {
            if (egress->pid_handle == pid_handle)
               return true;
         }
         return false;
      }
   }
   pa_flush_sessions_selective(fn, "pid flush", (int) pid_handle);
}
EXPORT_SYMBOL(avm_pa_flush_sessions_for_pid);

void avm_pa_flush_sessions_for_sg(unsigned short groupid)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      unsigned short groupid = (unsigned short) va_arg(args, int);
      return avm_pa_session_belongs_to_sg(session, groupid) != 0;
   }
   pa_flush_sessions_selective(fn, "group flush", (int) groupid);
}
EXPORT_SYMBOL(avm_pa_flush_sessions_for_sg);

static void
avm_pa_flush_sessions_with_destmac(struct avm_pa_macaddr *destmac)
{
   bool fn(struct avm_pa_session *session, va_list args)
   {
      struct avm_pa_egress *egress;
      avm_pa_for_each_egress(egress, session) {
         if (egress->destmac == va_arg(args, struct avm_pa_macaddr *)) {
            return true;
         }
      }
      return false;
   }
   pa_flush_sessions_selective(fn, "destmac", destmac);
}

void
avm_pa_flush_sessions_for_mac(const unsigned char mac[ETH_ALEN])
{
   struct avm_pa_global *ctx = &pa_glob;
   u32 hash = macaddr_hash(mac);
   struct avm_pa_macaddr *p;

   rcu_read_lock();
   for (p = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (memcmp(mac, &p->mac, ETH_ALEN) == 0) {
         avm_pa_flush_sessions_with_destmac(p);
         break;
      }
   }
   rcu_read_unlock();
}
EXPORT_SYMBOL(avm_pa_flush_sessions_for_mac);


static void avm_pa_sip_is_active(int state)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned rate;

   if (ctx->disabled)
      return;

   if (state) {
      if (ctx->telephony_active == 0) {
#if AVM_LOAD_CONTROL_ENABLED
         rate = ctx->tbf_enabled ? ctx->rate : ctx->maxrate;
         ctx->rate = rate - (rate*ctx->telephony_reduce)/100;
         ctx->load_control = LOADCONTROL_POWERIRQ;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         ctx->tbf_enabled = 1;
#endif
         pr_info("avm_pa: telephony active%s\n",
                 AVM_LOAD_CONTROL_ENABLED && ctx->rate != rate ? " (reduce)" : "");
      }
      ctx->telephony_active = 1;
   } else {
      if (ctx->telephony_active) {
#if AVM_LOAD_CONTROL_ENABLED
         ctx->load_control = LOADCONTROL_IDLE;
         avm_pa_tbf_disable();
#endif
         pr_info("avm_pa: telephony inactive\n");
      }
      ctx->telephony_active = 0;
   }
   if (ctx->hardware_pa.telephony_state)
      (*ctx->hardware_pa.telephony_state)(ctx->telephony_active);
}

void avm_pa_telefon_state(int state)
{
   pr_info("avm_pa: avm_pa_telefon_state\n");
}
EXPORT_SYMBOL(avm_pa_telefon_state);

/* ------------------------------------------------------------------------ */
/* ------- packet rate estimater ------------------------------------------ */
/* ------------------------------------------------------------------------ */

static void avm_pa_est_timer(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_est *e;
   u32 npackets;
   u32 rate;

   /* fw pkts/s */
   e = &ctx->fw_est;
   npackets = ctx->stats.fw_pkts;
   if (npackets >= e->last_packets) {
      rate = (npackets - e->last_packets)<<(12 - ctx->est_idx);
      e->last_packets = npackets;
      e->avpps += (rate >> e->ewma_log) - (e->avpps >> e->ewma_log);
      ctx->stats.fw_pps = (e->avpps+0x1FF)>>10;
   } else {
      e->last_packets = npackets;
   }
   if (   ctx->load_reduce == 0
       && ctx->stats.fw_pps > ctx->maxrate)
      ctx->maxrate = ctx->stats.fw_pps;

   /* rx pkts/s */
   e = &ctx->rx_est;
   npackets = ctx->stats.rx_pkts;
   if (npackets >= e->last_packets) {
      rate = (npackets - e->last_packets)<<(12 - ctx->est_idx);
      e->last_packets = npackets;
      e->avpps += (rate >> e->ewma_log) - (e->avpps >> e->ewma_log);
      ctx->stats.rx_pps = (e->avpps+0x1FF)>>10;
   } else {
      e->last_packets = npackets;
   }

   /* queued pkts/s */
   e = &ctx->overlimit_est;
   npackets = ctx->stats.rx_overlimit;
   if (npackets >= e->last_packets) {
      rate = (npackets - e->last_packets)<<(12 - ctx->est_idx);
      e->last_packets = npackets;
      e->avpps += (rate >> e->ewma_log) - (e->avpps >> e->ewma_log);
      ctx->stats.overlimit_pps = (e->avpps+0x1FF)>>10;
   } else {
      e->last_packets = npackets;
   }

   mod_timer(&ctx->est_timer, jiffies + ((HZ/4) << ctx->est_idx));
}

static void avm_pa_setup_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_est *e;
   del_timer(&ctx->est_timer);
   e = &ctx->fw_est;
   e->ewma_log = ctx->ewma_log;
   e->last_packets = ctx->stats.fw_pkts;
   e = &ctx->rx_est;
   e->ewma_log = ctx->ewma_log;
   e->last_packets = ctx->stats.rx_pkts;
   e = &ctx->overlimit_est;
   e->ewma_log = ctx->ewma_log;
   e->last_packets = ctx->stats.rx_overlimit;

   mod_timer(&ctx->est_timer, jiffies + ((HZ/4) << ctx->est_idx));
}

static void avm_pa_unsetup_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   del_timer(&ctx->est_timer);
}

/* ------------------------------------------------------------------------ */
/* -------- cputime estimater --------------------------------------------- */
/* ------------------------------------------------------------------------ */

static void avm_pa_add_cputimes(int cpu, cputime64_t *usersum,
                                cputime64_t *idlesum, cputime64_t *irqsum)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
   *usersum += kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
   *usersum += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];
   *usersum += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
   *idlesum += kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
   *idlesum += arch_idle_time(cpu);
   *idlesum += kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
   *irqsum += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
   *irqsum += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
#else
   *usersum = cputime64_add(*usersum, kstat_cpu(cpu).cpustat.user);
   *usersum = cputime64_add(*usersum, kstat_cpu(cpu).cpustat.nice);
   *usersum = cputime64_add(*usersum, kstat_cpu(cpu).cpustat.system);
   *idlesum = cputime64_add(*idlesum, kstat_cpu(cpu).cpustat.idle);
   *idlesum = cputime64_add(*idlesum, arch_idle_time(cpu));
   *idlesum = cputime64_add(*idlesum, kstat_cpu(cpu).cpustat.iowait);
   *irqsum = cputime64_add(*irqsum, kstat_cpu(cpu).cpustat.irq);
   *irqsum = cputime64_add(*irqsum, kstat_cpu(cpu).cpustat.softirq);
#endif
}

static inline void avm_pa_get_cputimes(cputime64_t *usertime,
                                       cputime64_t *idletime,
                                       cputime64_t *irqtime)
{
   cputime64_t usersum, idlesum, irqsum;
   int i;

   usersum = idlesum = irqsum = cputime64_zero;
   for_each_possible_cpu(i) {
      avm_pa_add_cputimes(i, &usersum, &idlesum, &irqsum);
   }
   irqsum += arch_irq_stat();
   *usertime = usersum;
   *idletime = idlesum;
   *irqtime = irqsum;
}

static void avm_pa_cputime_est_timer(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_cputime_est *e;
   cputime64_t usersum, idlesum, irqsum;
   cputime64_t cputime;
   u32 rate;
   u32 userdiff = 0;
   u32 idlediff = 0;
   u32 irqdiff = 0;

   avm_pa_get_cputimes(&usersum, &idlesum, &irqsum);

   /* usertime/s */
   e = &ctx->cputime_user_est;
   cputime = usersum;
   if (cputime >= e->last_cputime) {
      userdiff = cputime_to_msecs(cputime - e->last_cputime);
      rate = userdiff<<(12 - ctx->cputime_est_idx);
      e->last_cputime = cputime;
      e->avtps += (rate >> e->ewma_log) - (e->avtps >> e->ewma_log);
      ctx->stats.userms = (e->avtps+0x1FF)>>10;
   } else {
      e->last_cputime = cputime;
   }

   /* idletime/s */
   e = &ctx->cputime_idle_est;
   cputime = idlesum;
   if (cputime >= e->last_cputime) {
      idlediff = cputime_to_msecs(cputime - e->last_cputime);
      rate = idlediff<<(12 - ctx->cputime_est_idx);
      e->last_cputime = cputime;
      e->avtps += (rate >> e->ewma_log) - (e->avtps >> e->ewma_log);
      ctx->stats.idlems = (e->avtps+0x1FF)>>10;
   } else {
      e->last_cputime = cputime;
   }

   /* irqtime/s */
   e = &ctx->cputime_irq_est;
   cputime = irqsum;
   if (cputime >= e->last_cputime) {
      irqdiff = cputime_to_msecs(cputime - e->last_cputime);
      rate = irqdiff<<(12 - ctx->cputime_est_idx);
      e->last_cputime = cputime;
      e->avtps += (rate >> e->ewma_log) - (e->avtps >> e->ewma_log);
      ctx->stats.irqms = (e->avtps+0x1FF)>>10;
   } else {
      e->last_cputime = cputime;
   }

   if (ctx->dbgcputime)
      pr_info("avm_pa: %lu/%lu/%lu (%lu/%lu/%lu)\n",
              (unsigned long)userdiff,
              (unsigned long)idlediff,
              (unsigned long)irqdiff,
              (unsigned long)ctx->stats.userms,
              (unsigned long)ctx->stats.idlems,
              (unsigned long)ctx->stats.irqms);

   mod_timer(&ctx->cputime_est_timer, jiffies + ((HZ/4)<<ctx->cputime_est_idx));
}

static void avm_pa_setup_cputime_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_cputime_est *e;
   cputime64_t usersum, idlesum, irqsum;

   del_timer(&ctx->cputime_est_timer);

   avm_pa_get_cputimes(&usersum, &idlesum, &irqsum);
   e = &ctx->cputime_user_est;
   e->ewma_log = ctx->cputime_ewma_log;
   e->last_cputime = cputime_to_msecs(usersum);
   e = &ctx->cputime_idle_est;
   e->ewma_log = ctx->cputime_ewma_log;
   e->last_cputime = cputime_to_msecs(idlesum);
   e = &ctx->cputime_irq_est;
   e->ewma_log = ctx->cputime_ewma_log;
   e->last_cputime = cputime_to_msecs(irqsum);

   mod_timer(&ctx->cputime_est_timer, jiffies + ((HZ/4)<<ctx->cputime_est_idx));
}

static void avm_pa_unsetup_cputime_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   del_timer(&ctx->cputime_est_timer);
}

/* ------------------------------------------------------------------------ */
/* -------- value log ----------------------------------------------------- */
/* ------------------------------------------------------------------------ */

#if AVM_PA_TOKSTATS
static int avm_pa_thread(void *reply_data)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned long wtime = msecs_to_jiffies(100);
   unsigned long rx_overlimit;

   set_user_nice(current, 19);
   {
      sigset_t blocked;
      sigfillset(&blocked);
      sigprocmask(SIG_BLOCK, &blocked, NULL);
      flush_signals(current);
   }

   rx_overlimit = ctx->stats.rx_overlimit;

   while (!kthread_should_stop()) {
      unsigned long endtime = jiffies + wtime;
      unsigned long overtime;
      unsigned long overlimit;
      unsigned long pps;

      schedule_timeout_interruptible(wtime);
      overlimit = ctx->stats.rx_overlimit - rx_overlimit;
      rx_overlimit = ctx->stats.rx_overlimit;
      overtime = jiffies - endtime;
      pps = ctx->stats.fw_pps;

      ctx->tok_pos = (ctx->tok_pos+1)%TOK_SAMLES;
      ctx->tok_state[ctx->tok_pos] = ctx->load_reduce;
      ctx->tok_overtime[ctx->tok_pos] = overtime;
      ctx->tok_rate[ctx->tok_pos] = ctx->rate;
      ctx->tok_pps[ctx->tok_pos] = pps;
      ctx->tok_overlimit[ctx->tok_pos] = overlimit;
   }
   return 0;
}
#endif

/* ------------------------------------------------------------------------ */

static inline void avm_pa_start_lc_timer(void)
{
#if AVM_LOAD_CONTROL_ENABLED
   struct avm_pa_global *ctx = &pa_glob;
   if (mod_timer(&ctx->lc_timer, jiffies + AVM_PA_LC_TIMEOUT*HZ) == 0)
      ctx->lc_overlimit = ctx->stats.rx_overlimit;
#endif
}

static inline void avm_pa_stop_lc_timer(void)
{
#if AVM_LOAD_CONTROL_ENABLED
   struct avm_pa_global *ctx = &pa_glob;
   del_timer(&ctx->lc_timer);
#endif
}

#if AVM_LOAD_CONTROL_ENABLED
static void avm_pa_lc_timer_expired(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   u32 overlimit = ctx->stats.rx_overlimit - ctx->lc_overlimit;
   unsigned rate;

   ctx->lc_overlimit = ctx->stats.rx_overlimit;

   if (ctx->load_control & LOADCONTROL_IRQ) {
      if (   ctx->stats.irqms >= ctx->irq_mswin_high
          && ctx->stats.fw_pps > AVM_PA_MINRATE) {
         unsigned percent = 1;
         if (ctx->tbf_enabled == 0) {
            ctx->rate = ctx->maxrate;
            percent = 4;
         }
         rate = ctx->rate;
         rate = rate - (rate*percent)/100;
         ctx->rate = rate;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         ctx->tbf_enabled = 1;
         pr_info("avm_pa: load reduce 0, rate %u down (pps %lu ov_pps %lu irqms %lu)\n",
                 ctx->rate,
                 (unsigned long)ctx->stats.fw_pps,
                 (unsigned long)ctx->stats.overlimit_pps,
                 (unsigned long)ctx->stats.irqms);
      } else if (   overlimit
                 && ctx->load_reduce == 0
                 && ctx->tbf_enabled
                 && ctx->stats.irqms < ctx->irq_mswin_low) {
         unsigned rate = ctx->rate;
         unsigned percent = 1;
         rate = rate + (rate*percent)/100;
         ctx->rate = rate;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         pr_info("avm_pa: load reduce 0, rate %u up (pps %lu ov_pps %lu irqms %lu)\n",
                 ctx->rate,
                 (unsigned long)ctx->stats.fw_pps,
                 (unsigned long)ctx->stats.overlimit_pps,
                 (unsigned long)ctx->stats.irqms);
      }
   }
   if (ctx->load_control & LOADCONTROL_IDLE) {
      static unsigned count = 0;
      static unsigned good = 0;
      static unsigned lowcount = 0;
      if (ctx->tbf_enabled) {
         if (ctx->stats.fw_pps > AVM_PA_MINRATE)
            lowcount = 0;
         else lowcount++;
         if (lowcount*AVM_PA_LC_TIMEOUT >= AVM_PA_TRAFFIC_IDLE_TBFDISABLE) {
            avm_pa_tbf_disable();
            pr_info("avm_pa: %d seconds idle, tbf deactivated\n",
                    lowcount*AVM_PA_LC_TIMEOUT);
            lowcount = 0;
         }
      }
      if (   ctx->stats.idlems <= ctx->idle_mswin_low
          && ctx->stats.fw_pps > AVM_PA_MINRATE) {
         unsigned percent;
         if (ctx->tbf_enabled == 0) {
            ctx->rate = ctx->maxrate;
            percent = 5;
         } else if (good) {
            percent = 5;
         } else {
            if (count < 3) percent = 1;
            else if (count < 5) percent = 2;
            else percent = 5;
         }
         good = 0;
         count++;
         rate = ctx->rate;
         rate = rate - (rate*percent)/100;
         ctx->rate = rate;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         ctx->tbf_enabled = 1;
         pr_info("avm_pa: rate %u down (pps %lu ov_pps %lu idlems %lu count %u)\n",
                 ctx->rate,
                 (unsigned long)ctx->stats.fw_pps,
                 (unsigned long)ctx->stats.overlimit_pps,
                 (unsigned long)ctx->stats.idlems,
                 count);
      } else {
         count = 0;
         if (   overlimit
             && good
             && ctx->load_reduce == 0
             && ctx->tbf_enabled
             && ctx->stats.idlems > ctx->idle_mswin_high) {
            unsigned rate = ctx->rate;
            unsigned percent = 1;
            rate = rate + (rate*percent)/100;
            ctx->rate = rate;
            avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
            pr_info("avm_pa: rate %u up (pps %lu ov_pps %lu idlems %lu)\n",
                    ctx->rate,
                    (unsigned long)ctx->stats.fw_pps,
                    (unsigned long)ctx->stats.overlimit_pps,
                    (unsigned long)ctx->stats.idlems);
         }
         good++;
      }
   }
   avm_pa_start_lc_timer();
}

#ifdef CONFIG_AVM_POWERMETER
static void avm_pa_load_control_cb(int load_reduce, void *context)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned rate;

   if (ctx->disabled || (ctx->load_control & LOADCONTROL_POWER) == 0) {
      ctx->load_reduce = 0;
      return;
   }
   load_reduce = LOAD_CONTROL_REDUCE(load_reduce);
   if (load_reduce < 0) load_reduce = 0;
   else if (load_reduce > 10) load_reduce = 10;

   if (load_reduce == 0) {
      if (ctx->load_reduce) {
         pr_info("avm_pa: load reduce %d => %d, rate %u (pps %lu ov_pps %lu)\n",
                 ctx->load_reduce, load_reduce,
                 ctx->rate,
                 (unsigned long)ctx->stats.fw_pps,
                 (unsigned long)ctx->stats.overlimit_pps);
      }
   } else if (ctx->stats.fw_pps > AVM_PA_MINRATE) {
      int change = ctx->load_reduce - load_reduce;
      unsigned percent;
      if (ctx->tbf_enabled == 0)
         ctx->rate = ctx->maxrate;
      rate = ctx->rate;
      if (change <= 0) { /* get worth */
         if (ctx->load_reduce == 0) {
            if (ctx->tbf_enabled)
               percent = (-change)*4;
            else
               percent = (-change)*20;
         } else {
            percent = (-change)*8;
         }
         rate = rate - (rate*percent)/100;
      } else { /* get better */
         percent = change*4;
         rate = rate + (rate*percent)/100;
      }
      pr_info("avm_pa: load reduce %d => %d, rate %u => %u (change %d %u%% pps %lu ov_pps %lu)\n",
              ctx->load_reduce, load_reduce,
              ctx->rate, rate,
              change, percent,
              (unsigned long)ctx->stats.fw_pps,
              (unsigned long)ctx->stats.overlimit_pps);
      ctx->rate = rate;
      avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
      ctx->tbf_enabled = 1;
   } else {
      pr_info("avm_pa: load reduce %d => %d, rate %u (pps %lu)\n",
              ctx->load_reduce, load_reduce,
              ctx->rate, (unsigned long)ctx->stats.fw_pps);
   }
   ctx->load_reduce = load_reduce;
}
#endif
#endif

#if AVM_PA_TOKSTATS
static void pa_show_tstats(pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_tbf *tbf = &ctx->tbf;
   int i = TOK_SAMLES;
   int pos = ctx->tok_pos;

   (*fprintffunc)(arg, "load_reduce %d tbf_enabled %d maxrate %u\n",
                       ctx->load_reduce, ctx->tbf_enabled,
                       ctx->maxrate);
   (*fprintffunc)(arg, "rate %u buffer %u peak %u\n",
                       ctx->rate, ctx->pktbuffer, ctx->pktpeak);
   (*fprintffunc)(arg, "tbf: buffer %u peak %u pkttime %u tokens %ld/%ld\n",
                       tbf->buffer, tbf->pbuffer, tbf->pkttime,
                       tbf->tokens, tbf->ptokens);

   while (i--) {
      if (--pos < 0) pos = TOK_SAMLES-1;
      (*fprintffunc)(arg, "%d/%u/%u-%u/%lu%s",
                     ctx->tok_state[pos],
                     ctx->tok_overtime[pos],
                     ctx->tok_rate[pos],
                     ctx->tok_pps[pos],
                     ctx->tok_overlimit[pos],
                     i % 8 ? " " : "\n");
   }
}

static void avm_pa_thread_start(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->tok_task == 0) {
      ctx->tok_task = kthread_run(avm_pa_thread, 0, "avm_pa");
      if (IS_ERR(ctx->tok_task)) {
         pr_crit("avm_pa: failed to start task\n");
         ctx->tok_task = 0;
      }
   }
}

static void avm_pa_thread_stop(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->tok_task) {
      (void)kthread_stop(ctx->tok_task);
      ctx->tok_task = 0;
   }
}
#endif

static void avm_pa_enable(void)
{
   struct avm_pa_global *ctx = &pa_glob;
#if AVM_PA_TOKSTATS
   avm_pa_thread_start();
#endif
   avm_pa_setup_est();
   avm_pa_setup_cputime_est();
   avm_pa_tbf_init(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
   avm_pa_start_lc_timer();
}

static void avm_pa_disable(void)
{
   avm_pa_tbf_exit();
#if AVM_PA_TOKSTATS
   avm_pa_thread_stop();
#endif
   avm_pa_unsetup_cputime_est();
   avm_pa_unsetup_est();
   avm_pa_stop_lc_timer();
}

#ifdef CONFIG_PROC_FS
/* ------------------------------------------------------------------------ */
/* -------- procfs functions ---------------------------------------------- */
/* ------------------------------------------------------------------------ */

static int brief_show(struct seq_file *m, void *v)
{
   pa_show_brief((pa_fprintf *)seq_printf, m);
   return 0;
}

static int brief_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, brief_show, PDE_DATA(inode));
}

static const struct file_operations brief_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = brief_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* brief_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

static int memory_show(struct seq_file *m, void *v)
{
   pa_show_memory((pa_fprintf *)seq_printf, m);
   return 0;
}

static int memory_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, memory_show, PDE_DATA(inode));
}

static const struct file_operations memory_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = memory_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* memory_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

static int status_show(struct seq_file *m, void *v)
{
   pa_show_status((pa_fprintf *)seq_printf, m);
   return 0;
}

static int status_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, status_show, PDE_DATA(inode));
}

static const struct file_operations status_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = status_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* status_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

static int stats_show(struct seq_file *m, void *v)
{
   pa_show_stats((pa_fprintf *)seq_printf, m);
   return 0;
}

static int stats_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, stats_show, PDE_DATA(inode));
}

static const struct file_operations stats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = stats_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* stats_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

static int pids_show(struct seq_file *m, void *v)
{
   pa_show_pids((pa_fprintf *)seq_printf, m);
   return 0;
}

static int pids_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, pids_show, PDE_DATA(inode));
}

static const struct file_operations pids_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = pids_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* pids_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

static int vpids_show(struct seq_file *m, void *v)
{
   pa_show_vpids((pa_fprintf *)seq_printf, m);
   return 0;
}

static int vpids_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, vpids_show, PDE_DATA(inode));
}

static const struct file_operations vpids_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = vpids_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* vpids_show_open() uses single_open() */
};

static int vpids_hw_stats_show(struct seq_file *m, void *v)
{
   pa_show_vpids_hw_stats((pa_fprintf *)seq_printf, m);
   return 0;
}

static int vpids_hw_stats_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, vpids_hw_stats_show, PDE_DATA(inode));
}

static const struct file_operations vpids_hw_stats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = vpids_hw_stats_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* vpids_hw_stats_show_open() uses single_open() */
};

static int vpids_all_stats_show(struct seq_file *m, void *v)
{
   pa_show_vpids_all_stats((pa_fprintf *)seq_printf, m);
   return 0;
}

static int vpids_all_stats_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, vpids_all_stats_show, PDE_DATA(inode));
}

static const struct file_operations vpids_all_stats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = vpids_all_stats_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* vpids_all_stats_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

struct handle_iter {
   unsigned short handle;
};


static inline unsigned short
next_session(struct avm_pa_global *ctx, unsigned short handle)
{
   while (++handle < CONFIG_AVM_PA_MAX_SESSION) {
      struct avm_pa_session *sess = pa_session_get(handle);
      if (sess && avm_pa_session_is_selected(&ctx->show_filter, sess))
         return handle;
   }
   return 0;
}

static void *sess_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;
   struct handle_iter   *it  = seq->private;
   loff_t i;

   if ((it->handle = next_session(ctx, 0)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_session(ctx, it->handle)) == 0)
         return 0;
   }
   return PA_SESSION(pd, it->handle);
}

static void *sess_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_data   *pd  = &pa_data;
   struct handle_iter   *it  = seq->private;

   ++*pos;
   if ((it->handle = next_session(ctx, it->handle)) == 0)
      return 0;
   return PA_SESSION(pd, it->handle);
}

static void sess_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int sess_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_data   *pd  = &pa_data;
   const struct handle_iter *it = seq->private;
   seq_printf(seq, "\n");
   pa_show_session(PA_SESSION(pd, it->handle),
                   (pa_fprintf *)seq_printf, seq);
   return 0;
}

static struct seq_operations sess_show_seq_ops = {
   .start = sess_show_seq_start,
   .next  = sess_show_seq_next,
   .stop  = sess_show_seq_stop,
   .show  = sess_show_seq_show,
};

static int sess_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &sess_show_seq_ops, sizeof(struct handle_iter));
}

static const struct file_operations sess_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = sess_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release_private, /* sess_show_open() uses seq_open_private() */
};

/* ------------------------------------------------------------------------ */

static inline unsigned short
next_bsession(struct avm_pa_global *ctx, unsigned short handle)
{
   while (++handle < CONFIG_AVM_PA_MAX_SESSION) {
      struct avm_pa_session *session;
      if ((session = pa_session_get(handle)) != 0 && session->bsession)
         return handle;
   }
   return 0;
}

static void *bsess_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_bsession(ctx, 0)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_bsession(ctx, it->handle)) == 0)
         return 0;
   }
   return PA_BSESSION(ctx, it->handle);
}

static void *bsess_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_bsession(ctx, it->handle)) == 0)
    return 0;
   return PA_BSESSION(ctx, it->handle);
}

static void bsess_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int bsess_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   seq_printf(seq, "\n");
   pa_show_bsession(PA_BSESSION(ctx, it->handle),
                   (pa_fprintf *)seq_printf, seq);
   return 0;
}

static struct seq_operations bsess_show_seq_ops = {
   .start = bsess_show_seq_start,
   .next  = bsess_show_seq_next,
   .stop  = bsess_show_seq_stop,
   .show  = bsess_show_seq_show,
};

static int bsess_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &bsess_show_seq_ops, sizeof(struct handle_iter));
}


static const struct file_operations bsess_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = bsess_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release_private, /* bsess_show_open() uses seq_open_private() */
};

/* ------------------------------------------------------------------------ */

static inline int
next_macaddrhash(struct avm_pa_global *ctx, int idx)
{
   while (++idx < CONFIG_AVM_PA_MAX_SESSION) {
      if (ctx->macaddr_hash[idx])
         return idx;
   }
   return 0;
}

static void *macaddr_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_macaddrhash(ctx, -1)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_macaddrhash(ctx, it->handle)) == 0)
         return 0;
   }
   return ctx->macaddr_hash[it->handle];
}

static void *macaddr_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_macaddrhash(ctx, it->handle)) == 0)
      return 0;
   return ctx->macaddr_hash[it->handle];
}

static void macaddr_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int macaddr_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   struct avm_pa_macaddr *p;
   char buf[128];
   char vlan_buf[32];


   seq_printf(seq, "%3d: ", it->handle);
   for (p = ctx->macaddr_hash[it->handle]; p; p = p->link) {
      vlan_buf[0] = '\0';
      if (p->vlan_id & VLAN_TAG_PRESENT) {
         snprintf(vlan_buf, sizeof(vlan_buf), " vlan id %d", p->vlan_id & VLAN_VID_MASK);
      }
      mac2str(&p->mac, buf, sizeof(buf));
      seq_printf(seq, " %s%s (%lu %d/%s)",
                      buf, vlan_buf,
                      p->refcount,
                      p->pid_handle,
                      PA_PID(ctx, p->pid_handle)->cfg.name);
   }
   seq_printf(seq, "\n");
   return 0;
}

static struct seq_operations macaddr_show_seq_ops = {
   .start = macaddr_show_seq_start,
   .next  = macaddr_show_seq_next,
   .stop  = macaddr_show_seq_stop,
   .show  = macaddr_show_seq_show,
};

static int macaddr_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &macaddr_show_seq_ops, sizeof(struct handle_iter));
}

static const struct file_operations macaddr_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = macaddr_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release_private, /* macaddr_show_open() uses seq_open_private() */
};

/* ------------------------------------------------------------------------ */

static inline unsigned short
next_pid(struct avm_pa_global *ctx, unsigned short handle)
{
   while (++handle < CONFIG_AVM_PA_MAX_PID) {
      if (PA_PID(ctx, handle)->pid_handle)
         return handle;
   }
   return 0;
}

static void *pid_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_pid(ctx, 0)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_pid(ctx, it->handle)) == 0)
         return 0;
   }
   return PA_PID(ctx, it->handle);
}

static void *pid_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_pid(ctx, it->handle)) == 0)
      return 0;
   return PA_PID(ctx, it->handle);
}

static void pid_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int hash_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   struct avm_pa_pid *pid = PA_PID(ctx, it->handle);
   struct avm_pa_session *p;
   int i;
   seq_printf(seq, "PID%-3d: %s\n",
         it->handle, PA_PID(ctx, it->handle)->cfg.name);

   rcu_read_lock_bh();
   for (i = 0; i < ARRAY_SIZE(pid->hash_sess); i++) {
      if (!hlist_empty(&pid->hash_sess[i])) {
         seq_printf(seq, "%3d: ", i);
         hlist_for_each_entry_rcu_bh(p, &pid->hash_sess[i], hash_list)
            seq_printf(seq, " %3d", p->session_handle);
         seq_printf(seq, "\n");
      }
   }
   rcu_read_unlock_bh();
   return 0;
}

static struct seq_operations hash_show_seq_ops = {
   .start = pid_show_seq_start,
   .next  = pid_show_seq_next,
   .stop  = pid_show_seq_stop,
   .show  = hash_show_seq_show,
};

static int hash_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &hash_show_seq_ops, sizeof(struct handle_iter));
}

static const struct file_operations hash_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = hash_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release_private, /* hash_show_open() uses seq_open_private() */
};

/* ------------------------------------------------------------------------ */

static int prioack_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   int i, j;

   seq_printf(seq, "Packet Threshold      : %u\n", ctx->prioack_thresh_packets);
   seq_printf(seq, "Ratio                 : %u\n", ctx->prioack_ratio);

   for (i = 1; i < CONFIG_AVM_PA_MAX_PID; ++i) {
      struct avm_pa_pid *pid = PA_PID(ctx, i);
      if (avm_pa_pid_tack_enabled(pid)) {
         seq_printf(seq, "PID%d: Detected ACKs   : %u\n", pid->pid_handle, pid->prioack_acks);
         seq_printf(seq, "PID%d: Accelerated ACK : %u\n", pid->pid_handle, pid->prioack_accl_acks);
         for (j = 0; j < AVM_PA_MAX_PRIOS; ++j) {
            seq_printf(seq, "PID%d: TACK Priority[%d]: %x\n", pid->pid_handle, j, pid->prio_maps[AVM_PA_PRIO_MAP_TACK].prios[j]);
         }
      }
      if (avm_pa_pid_tget_enabled(pid)) {
         for (j = 0; j < AVM_PA_MAX_PRIOS; ++j) {
            seq_printf(seq, "PID%d: TGET Priority[%d]: %x\n", pid->pid_handle, j, pid->prio_maps[AVM_PA_PRIO_MAP_TGET].prios[j]);
         }
      }
   }

   return 0;
}

static int prioack_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, prioack_show, PDE_DATA(inode));
}

static const struct file_operations prioack_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = prioack_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* prioack_show_open() uses single_open() */
};

static int priomaps_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   int i, j, k;

   for (i = 1; i < CONFIG_AVM_PA_MAX_PID; ++i) {
      struct avm_pa_pid *pid = PA_PID(ctx, i);
      if (pid->pid_handle == 0) {
         continue;
      }
      seq_printf(seq, "PID %d Prio Maps\n", pid->pid_handle);
      for (j = 0; j < AVM_PA_COUNT_PRIO_MAPS; ++j) {
         if (!pid->prio_maps[j].enabled) {
            continue;
         }
         seq_printf(seq, "Prio Map[%d]\n", j);
         for (k = 0; k < AVM_PA_MAX_PRIOS; ++k) {
            seq_printf(seq, "Queue[%d]: %x\n", k, pid->prio_maps[j].prios[k]);
         }
      }
   }

   return 0;
}

static int priomaps_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, priomaps_show, PDE_DATA(inode));
}

static const struct file_operations priomaps_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = priomaps_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* priomaps_show_open() uses single_open() */
};

/* ------------------------------------------------------------------------ */

#if AVM_PA_TOKSTATS
static int tstats_show(struct seq_file *m, void *v)
{
   pa_show_tstats((pa_fprintf *)seq_printf, m);
   return 0;
}

static int tstats_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, tstats_show, PDE_DATA(inode));
}

static const struct file_operations tstats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = tstats_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = single_release, /* tstats_show_open() uses single_open() */
};
#endif

/* ------------------------------------------------------------------------ */

static avm_pid_handle pa_find_pid_by_name(const char *pidname)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_pid_handle pid_handle;
   for (pid_handle = 1; pid_handle < CONFIG_AVM_PA_MAX_PID; pid_handle++) {
      struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
      if (   pid->pid_handle == pid_handle
          && strcmp(pid->cfg.name, pidname) == 0) {
         return pid_handle;
      }
   }
   return 0;
}


/* Normal strsep returns empty strings for duplicated delimtiers */
static char *strsep_nonempty(char **stringp, const char *delim)
{
   char *p = strsep(stringp, delim);
   while (p && *p == 0)
      p = strsep(stringp, delim);
   return p;
}

static ssize_t avm_pa_write_cmds(struct file *file,
                                 const char __user *buffer,
                                 size_t count, loff_t *offset)
{
   struct avm_pa_global *ctx = &pa_glob;
   char    pp_cmd[101];
   char*   argv[10];
   int     argc;
   char*   ptr_next_tok;
   char*   ptr_next_line;
   avm_pid_handle pid_handle;

   /* Validate the length of data passed. */
   if (count >= sizeof(pp_cmd))
      return -E2BIG;

   /* Initialize the buffer before using it. */
   memset ((void *)&pp_cmd[0], 0, sizeof(pp_cmd));

   /* Copy from user space. */
   if (copy_from_user (&pp_cmd, buffer, count))
      return -EFAULT;

   ptr_next_line = pp_cmd;
   /* one command (with arguments) per line */
   while ((ptr_next_tok = strsep_nonempty(&ptr_next_line, "\n"))) {
      /* exract arguments */
      for (argc = 0; argc < ARRAY_SIZE(argv); argc++)
         argv[argc] = strsep_nonempty(&ptr_next_tok, " \t");

      if (ptr_next_tok)
         return -E2BIG;

      /* enable | disable | testmode */
      if (strcmp(argv[0], "enable") == 0) {
         ctx->fw_disabled = 0;
         ctx->disabled = 0;
         avm_pa_enable();
         pr_debug("avm_pa: enabled\n");
      } else if (strcmp(argv[0], "disable") == 0) {
         ctx->disabled = 1;
         ctx->fw_disabled = 1;
         avm_pa_disable();
         avm_pa_flush_sessions();
         pr_debug("avm_pa: disabled\n");
      } else if (strcmp(argv[0], "testmode") == 0) {
         ctx->fw_disabled = 1;
         ctx->disabled = 0;
         avm_pa_disable();
         pr_debug("avm_pa: testmode\n");

      /* hw_enable | hw_disable */
      } else if (strcmp(argv[0], "hw_enable") == 0) {
         ctx->hw_ppa_disabled = 0;
         pr_debug("avm_pa: hw enabled\n");
      } else if (strcmp(argv[0], "hw_disable") == 0) {
         ctx->hw_ppa_disabled = 1;
         avm_pa_flush_hw_sessions();
         pr_debug("avm_pa: hw disabled\n");
      } else if (strcmp(argv[0], "filter") == 0) {
         int old = ctx->filter_enabled;
         if (argc > 1)
            ctx->filter_enabled = argc > 1 ? *argv[1] != '0' : 1;
         else
            ctx->filter_enabled = 1;
         if (ctx->filter_enabled && !old && !list_empty(&ctx->accel_filter))
            avm_pa_flush_sessions();
      } else if (strcmp(argv[0], "nofilter") == 0) {
         ctx->filter_enabled = 0;
      /* flush */
      } else if (strcmp(argv[0], "flush") == 0) {
         if (argv[1]) {
            avm_vpid_handle vpid_handle = simple_strtoul(argv[1], 0, 10);
            if (   vpid_handle
                && PA_VPID(ctx, vpid_handle)->vpid_handle == vpid_handle) {
               avm_pa_flush_sessions_for_vpid(vpid_handle);
               pr_debug("avm_pa: flush %u\n", (unsigned)vpid_handle);
            } else {
               pr_debug("avm_pa: flush %s: illegal vpid\n", argv[1]);
            }
         } else {
            avm_pa_flush_sessions();
            pr_debug("avm_pa: flush\n");
         }
      /* loadcontrol | noloadcontrol */
      } else if (strcmp(argv[0], "loadcontrol") == 0) {
         if (argv[1]) {
            if (strcmp(argv[1], "irq") == 0) {
               ctx->load_control = LOADCONTROL_IRQ;
            } else if (strcmp(argv[1], "idle") == 0) {
               ctx->load_control = LOADCONTROL_IDLE;
            } else if (strcmp(argv[1], "off") == 0) {
               ctx->load_control = LOADCONTROL_OFF;
            } else {
               ctx->load_control = LOADCONTROL_POWERIRQ;
            }
         } else {
            ctx->load_control = LOADCONTROL_POWERIRQ;
         }
         if (   ctx->load_control == LOADCONTROL_OFF
             || (   (ctx->load_control & LOADCONTROL_POWER)
                 && ctx->load_reduce == 0)) {
            avm_pa_tbf_disable();
         } else {
            ctx->rate = ctx->maxrate;
            avm_pa_start_lc_timer();
            if ((ctx->load_control & LOADCONTROL_POWER) && ctx->load_reduce) {
               avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
               ctx->tbf_enabled = 1;
            }
         }
         switch (ctx->load_control) {
            case LOADCONTROL_OFF:
               pr_debug("avm_pa: loadcontrol off\n");
               break;
            case LOADCONTROL_IRQ:
               pr_debug("avm_pa: loadcontrol irq\n");
               break;
            case LOADCONTROL_IDLE:
               pr_debug("avm_pa: loadcontrol idle\n");
               break;
            case LOADCONTROL_POWERIRQ:
               pr_debug("avm_pa: loadcontrol powerirq\n");
               break;
         }

      } else if (strcmp(argv[0], "noloadcontrol") == 0) {
         ctx->load_control = LOADCONTROL_OFF;
         avm_pa_tbf_disable();
         pr_debug("avm_pa: loadcontrol off\n");

      /* tbfenable | tbfdisable */
      } else if (strcmp(argv[0], "tbfenable") == 0) {
         ctx->tbf_enabled = 1;
         pr_debug("avm_pa: tbf enabled\n");
      } else if (strcmp(argv[0], "tbfdisable") == 0) {
         ctx->tbf_enabled = 0;
         pr_debug("avm_pa: tbf disabled\n");

      /* rpsenable | rpsdisable */
      } else if (strcmp(argv[0], "rpsenable") == 0) {
#ifdef CONFIG_AVM_PA_RPS
         ctx->rps_enabled = 1;
         pr_debug("avm_pa: rps enabled\n");
#else
         pr_debug("avm_pa: rps support not built-in\n");
#endif
      } else if (strcmp(argv[0], "rpsdisable") == 0) {
         ctx->rps_enabled = 0;
         pr_debug("avm_pa: rps disabled\n");

      /* mswin 800 900 */
      } else if (strcmp(argv[0], "mswin") == 0) {
         unsigned mswin;
         if (argv[1]) {
            mswin = simple_strtoul(argv[1], 0, 10);
            if (mswin > 0) ctx->irq_mswin_low = mswin;
         }
         if (argv[2]) {
            mswin = simple_strtoul(argv[2], 0, 10);
            if (mswin > 0) ctx->irq_mswin_high = mswin;
         }
         pr_debug("avm_pa: mswin %u %u\n",
                  ctx->irq_mswin_low, ctx->irq_mswin_high);
      /* idlewin 10 20 */
      } else if (strcmp(argv[0], "idlewin") == 0) {
         unsigned mswin;
         if (argv[1]) {
            mswin = simple_strtoul(argv[1], 0, 10);
            if (mswin > 0) ctx->idle_mswin_low = mswin;
         }
         if (argv[2]) {
            mswin = simple_strtoul(argv[2], 0, 10);
            if (mswin > 0) ctx->idle_mswin_high = mswin;
         }
         pr_debug("avm_pa: idlewin %u %u\n",
                  ctx->idle_mswin_low, ctx->idle_mswin_high);
      /* ewma 0-31 */
      } else if (strcmp(argv[0], "ewma") == 0) {
         if (argv[1]) {
            unsigned ewma = simple_strtoul(argv[1], 0, 10);
            if (ewma <= 31) {
               struct avm_pa_cputime_est *e;
               ctx->cputime_ewma_log = ewma;
               e = &ctx->cputime_user_est;
               e->ewma_log = ctx->cputime_ewma_log;
               e = &ctx->cputime_idle_est;
               e->ewma_log = ctx->cputime_ewma_log;
               e = &ctx->cputime_irq_est;
               e->ewma_log = ctx->cputime_ewma_log;
               pr_debug("avm_pa: ewma %d\n", ctx->cputime_ewma_log);
            }
         }

      /* rate pps */
      } else if (strcmp(argv[0], "rate") == 0) {
         if (argv[1]) {
            unsigned rate = simple_strtoul(argv[1], 0, 10);
            if (rate > 0) {
               ctx->rate = rate;
               ctx->maxrate = rate;
               avm_pa_tbf_update(ctx->rate, ctx->pktbuffer,
                     ctx->pktpeak);
               if (ctx->load_control == 0) {
                  if (ctx->tbf_enabled == 0) {
                     ctx->tbf_enabled = 1;
                     avm_pa_tbf_reset();
                  }
               }
               pr_debug("avm_pa: rate %u\n", ctx->rate);
            }
         }

      /* buffer pkts */
      } else if (strcmp(argv[0], "buffer") == 0) {
         if (argv[1]) {
            unsigned pktbuffer = simple_strtoul(argv[1], 0, 10);
            if (pktbuffer > 0) {
               ctx->pktbuffer = pktbuffer;
               avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
               pr_debug("avm_pa: buffer %u\n", ctx->pktbuffer);
            }
         }

      /* peak pkts */
      } else if (strcmp(argv[0], "peak") == 0) {
         if (argv[1]) {
            unsigned peak = simple_strtoul(argv[1], 0, 10);
            if (buffer > 0) {
               ctx->pktpeak = peak;
               avm_pa_tbf_update(ctx->rate, ctx->pktbuffer,
                     ctx->pktpeak);
               pr_debug("avm_pa: peak %u\n", ctx->pktpeak);
            }
         }
      } else if (strcmp(argv[0], "treduce") == 0) {
         unsigned reduce;
         if (argv[1]) {
            reduce = simple_strtoul(argv[1], 0, 10);
            if (reduce > 0 && reduce <= 80)
               ctx->telephony_reduce = reduce;
         }
         pr_debug("avm_pa: telephony_reduce %u\n",
                  ctx->telephony_reduce);
      } else if (strcmp(argv[0], "sipactive") == 0) {
         int sip_is_active;
         if (argv[1]) {
            sip_is_active = simple_strtoul(argv[1], 0, 10);
            avm_pa_sip_is_active(sip_is_active);
            pr_debug("avm_pa: sip telephony is %sactive\n",
                     sip_is_active ? "" : "not ");
         }

      /* nodbg */
      } else if (strcmp(argv[0], "nodbg") == 0) {
         ctx->dbgcapture = 0;
         ctx->dbgsession = 0;
         ctx->dbgnosession = 0;
         ctx->dbgtrace = 0;
         ctx->dbgmatch = 0;
         ctx->dbgcputime = 0;
         ctx->dbgprioack = 0;
         ctx->dbgprioacktrace = 0;
         ctx->dbgstats = 0;
         pr_debug("avm_pa: all debugs off\n");

      /* dbgcapture | nodbgcapture */
      } else if (strcmp(argv[0], "dbgcapture") == 0) {
         ctx->dbgcapture = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "nodbgcapture") == 0) {
         ctx->dbgcapture = 0;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgsession | nodbgsession */
      } else if (strcmp(argv[0], "dbgsession") == 0) {
         ctx->dbgsession = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "nodbgsession") == 0) {
         ctx->dbgsession = 0;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgnosession | nodbgnosession */
      } else if (strcmp(argv[0], "dbgnosession") == 0) {
         ctx->dbgnosession = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "nodbgnosession") == 0) {
         ctx->dbgnosession = 0;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* trace | notrace */
      } else if (strcmp(argv[0], "trace") == 0) {
#if AVM_PA_TRACE
         ctx->dbgtrace = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
#else
         pr_err("avm_pa: trace not compiled in\n");
#endif
      } else if (strcmp(argv[0], "notrace") == 0) {
         ctx->dbgtrace = 0;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgmatch | nodbgmatch */
      } else if (strcmp(argv[0], "nodbgmatch") == 0) {
         ctx->dbgmatch = 0;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "dbgmatch") == 0) {
         ctx->dbgmatch = 1;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgcputime | nodbgcputime */
      } else if (strcmp(argv[0], "nodbgcputime") == 0) {
         ctx->dbgcputime = 0;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "dbgcputime") == 0) {
         ctx->dbgcputime = 1;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgprioack | nodbgprioack */
      } else if (strcmp(argv[0], "dbgprioack") == 0) {
         ctx->dbgprioack = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "nodbgprioack") == 0) {
         ctx->dbgprioack = 0;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgprioacktrace | nodbgprioacktrace */
      } else if (strcmp(argv[0], "dbgprioacktrace") == 0) {
         ctx->dbgprioacktrace = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "nodbgprioacktrace") == 0) {
         ctx->dbgprioacktrace = 0;
         pr_debug("avm_pa: %s\n", argv[0]);

      /* dbgstats | nodbgstats */
      } else if (strcmp(argv[0], "dbgstats") == 0) {
         ctx->dbgstats = 1;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strcmp(argv[0], "nodbgstats") == 0) {
         ctx->dbgstats = 0;
         pr_debug("avm_pa: %s\n", argv[0]);
      } else if (strstr(argv[0], "bsessions")) {
         ctx->bsession_allowed = strcmp(argv[0], "nobsessions") != 0;
         if (!ctx->bsession_allowed)
            avm_pa_flush_bsessions();
         pr_debug("avm_pa: %s: bsessions_allowed = %d\n", argv[0], ctx->bsession_allowed);
      /* pid <device> */
      } else if (strcmp(argv[0], "pid") == 0 && argv[1]) {
         struct net_device *dev = dev_get_by_name(&init_net, argv[1]);

         if (dev) {
            if (avm_pa_dev_register(dev) < 0)
               pr_err("%s: failed to register PA PID\n", argv[1]);
            dev_put(dev);
         } else {
            pr_err("avm_pa_write_cmds(pid): dev %s not found\n", argv[1]);
         }

      /* vpid <device> */
      } else if (strcmp(argv[0], "vpid") == 0 && argv[1]) {
         struct net_device *dev = dev_get_by_name(&init_net, argv[1]);

         if (dev) {
            struct avm_pa_vpid_cfg cfg;
            snprintf(cfg.name, sizeof(cfg.name), "%s", argv[1]);
            cfg.v4_mtu = 1500;
            cfg.v6_mtu = 1500;
            if (avm_pa_dev_vpid_register(AVM_PA_DEVINFO(dev), &cfg) < 0)
               pr_err("%s: failed to register PA VPID\n", argv[1]);
            dev_put(dev);
         } else {
            pr_err("avm_pa_write_cmds(vpid): dev %s not found\n", argv[1]);
         }

      /* unreg <device> */
      } else if (strcmp(argv[0], "unreg") == 0 && argv[1]) {
         int ret;
         struct net_device *dev = dev_get_by_name(&init_net, argv[1]);
         DECLARE_COMPLETION_ONSTACK(done);

         if (dev) {
            avm_pa_dev_unregister(AVM_PA_DEVINFO(dev), &done);
            ret = wait_for_completion_interruptible(&done);
            if (ret != 0)
                return ret;
         } else {
            pr_err("avm_pa_write_cmds(unreg): dev %s not found\n", argv[1]);
         }

      /* prioack <enable|disable|psize x|pthresh x|prio x|ratio x>
       *
       * Note: This interface is now partially obsolete (prioack <enable|disable>)
       * in favour of the priomap interface defined below.
       */
      } else if (strcmp(argv[0], "prioack") == 0) {
         unsigned val = 0;

         if (argv[1]) {
            pr_debug("avm_pa: prioack %s %s %s\n",
                     argv[1], argv[2] ? argv[2] : "", argv[3] ? argv[3] : "");
            if (strcmp(argv[1], "enable") == 0) {
               if (argv[2] && argv[3]) {
                  if ((pid_handle = pa_find_pid_by_name(argv[2])) != 0) {
                     avm_pa_pid_activate_tcpackprio(pid_handle, 1, simple_strtoul(argv[3], 0, 0));
                  } else {
                     pr_err("avm_pa: %s %s: %s not found\n",
                            argv[0], argv[1], argv[2]);
                  }
               }
            } else if (strcmp(argv[1], "disable") == 0) {
               if (argv[2]) {
                  if ((pid_handle = pa_find_pid_by_name(argv[2])) != 0) {
                     avm_pa_pid_activate_tcpackprio(pid_handle, 0, 0);
                  } else {
                     pr_err("avm_pa: prioack %s: %s not found\n",
                            argv[1], argv[2]);
                  }
               } else {
                  int n;
                  for (n = 1; n < CONFIG_AVM_PA_MAX_PID; ++n) {
                     avm_pa_pid_activate_tcpackprio(n, 0, 0);
                  }
               }
            } else if (strcmp(argv[1], "tgetenable") == 0) {
               if (argv[2] && argv[3]) {
                  if ((pid_handle = pa_find_pid_by_name(argv[2])) != 0) {
                     avm_pa_pid_activate_tgetprio(pid_handle, 1, simple_strtoul(argv[3], 0, 0));
                  } else {
                     pr_err("avm_pa: %s %s: %s not found\n",
                            argv[0], argv[1], argv[2]);
                  }
               }
            } else if (strcmp(argv[1], "tgetdisable") == 0) {
               if (argv[2]) {
                  if ((pid_handle = pa_find_pid_by_name(argv[2])) != 0) {
                     avm_pa_pid_activate_tgetprio(pid_handle, 0, 0);
                  } else {
                     pr_err("avm_pa: %s %s: %s not found\n",
                            argv[0], argv[1], argv[2]);
                  }
               } else {
                  int n;
                  for (n = 1; n < CONFIG_AVM_PA_MAX_PID; ++n) {
                     avm_pa_pid_activate_tgetprio(n, 0, 0);
                  }
               }
            } else if (strcmp(argv[1], "pthresh") == 0) {
               if (argv[2]) val = simple_strtoul(argv[2], 0, 0);
               if (val) ctx->prioack_thresh_packets = val;
            } else if (strcmp(argv[1], "ratio") == 0) {
               if (argv[2]) val = simple_strtoul(argv[2], 0, 0);
               if (val) ctx->prioack_ratio = val;
            } else {
               pr_debug("avm_pa: prioack unknown command %s \n (available commands: enable,disable,psize,pthresh,prio,ratio)\n", argv[1]);
            }
         }
      /* The priomap interface supersedes the old prioack interface. */
      } else if (strcmp(argv[0], "priomap") == 0) {
         if (argv[1] && argv[2] && argv[3]) {
            unsigned short prio_map = simple_strtoul(argv[1], 0, 0);
            if ((pid_handle = pa_find_pid_by_name(argv[2])) != 0) {
               /* Command: priomap <priomap> <pidname> <enable|disable>
                *
                * Effect: Enables or disables the priority map attached to the
                * device specified by the 'dev' parameter. The 'priomap' parameter
                * MUST equal either AVM_PA_PRIO_MAP_TACK (= 0x0000) or
                * AVM_PA_PRIO_MAP_TGET (= 0x0001) as defined in avm_pa.h.
                */
               if (strcmp(argv[3], "enable") == 0) {
                  avm_pa_pid_prio_map_enable(pid_handle, prio_map, 1);
               } else if (strcmp(argv[3], "disable") == 0) {
                  avm_pa_pid_prio_map_enable(pid_handle, prio_map, 0);
               /* Command: priomap <priomap> <pidname> reset
                *
                * Effect: Resets the priority map attached to the device specified by
                * the 'dev' parameter. The 'priomap' parameter MUST equal either
                * AVM_PA_PRIO_MAP_TACK (= 0x0000) or AVM_PA_PRIO_MAP_TGET (= 0x0001)
                * as defined in avm_pa.h.
                */
               } else if (strcmp(argv[3], "reset") == 0) {
                  avm_pa_pid_prio_map_reset(pid_handle, prio_map);
               /* Command: priomap <priomap> <pidname> set_prio <queue> <prio>
                *
                * Effect: Manipulates the priority map entry specified by the
                * 'queue' parameter which is stored in the priority map attached
                * to the device specified by the 'dev' parameter. The 'priomap'
                * parameter MUST equal either AVM_PA_PRIO_MAP_TACK (= 0x0000) or
                * AVM_PA_PRIO_MAP_TGET (= 0x0001) as defined in avm_pa.h.
                */
               } else if (strcmp(argv[3], "setprio") == 0 && argv[4] && argv[5]) {
                  avm_pa_pid_prio_map_set_prio_per_queue(pid_handle, prio_map,
                     simple_strtoul(argv[4], 0, 0),  /* queue */
                     simple_strtoul(argv[5], 0, 0)); /* prio */
               } else {
                  pr_err("avm_pa: priomap unknown command '%s'\n (available commands: enable, disable, reset, setprio)\n", argv[3]);
               }
            } else {
               pr_err("avm_pa: %s %s %s %s: %s not found\n",
                      argv[0], argv[1], argv[2], argv[3], argv[2]);
            }
         } else {
            pr_err("avm_pa: %s: not enough parameters\n", argv[0]);
         }
      } else if (!strcmp(argv[0], "timeout")) {
         unsigned int val;

         if (!argv[1] || !argv[2])
            return -EINVAL;

         val = simple_strtoul(argv[2], 0, 0);
         if (!strcmp(argv[1], "tcp"))
            ctx->tcp_timeout_secs = val;
         else if (!strcmp(argv[1], "udp"))
            ctx->udp_timeout_secs = val;
         else if (!strcmp(argv[1], "echo"))
            ctx->echo_timeout_secs = val;
         else if (!strcmp(argv[1], "bridge"))
            ctx->bridge_timeout_secs = val;
         else
            return -EINVAL;

         pr_info("avm_pa: setting timeout for %s to %u seconds\n", argv[1], val);
      } else if (!strcmp(argv[0], "debug")) {
         if (argv[1] && !strcmp(argv[1], "unreg-hw-pa")) {
            int ret;
            DECLARE_COMPLETION_ONSTACK(done);
            struct avm_hardware_pa tmp = ctx->hardware_pa;
            avm_pa_unregister_hardware_pa(&tmp, &done);
            ret = wait_for_completion_interruptible(&done);
            if (ret != 0)
                return ret;

            /* Give some time for new sessions in case this
             * test is driven with parallel sessions. Of course,
             * hardware sessions must not be created.
             */
            msleep(100);
            if (pa_hw_pa_get()) {
               pa_hw_pa_put();
               pr_err("avm_pa: hw_pa refcount should be 0 but really is %d\n", atomic_read(&ctx->hw_pa_ref.refcount));
               return -EIO;
            }
            ret = avm_pa_register_hardware_pa(&tmp);
            if (ret != 0) {
               pr_err("avm_pa: re-register hardware_pa failed: %d\n", ret);
               return ret;
            }
         }
      } else {
         pr_err("avm_pa_write_cmds: %s: unknown command\n", argv[0]);
      }
   }

   return count;
}

/* ------------------------------------------------------------------------ */


const struct file_operations avm_pa_control_fops = {
   .write = avm_pa_write_cmds,
};


static ssize_t
avm_pa_read_show_filter(struct file *file,
                        char __user *buffer,
                        size_t count,
                        loff_t *offset)
{
   struct list_head *selector_list = PDE_DATA(file_inode(file));
   ssize_t ret;

   if (*offset || list_empty(selector_list))
      return 0;

   ret = avm_pa_dump_selector_user(selector_list, buffer, count);
   *offset += ret;
   return ret;
}

static ssize_t
avm_pa_write_show_filter(struct file *file,
                         const char __user *buffer,
                         size_t count,
                         loff_t *offset)
{
   ssize_t ret;
   struct list_head *selector_list = PDE_DATA(file_inode(file));

   ret = avm_pa_parse_selector_user(selector_list, buffer, count);
   if (ret < 0)
      return ret;
   *offset += ret;
   return ret;
}

const struct file_operations selector_fops = {
   .read = avm_pa_read_show_filter,
   .write = avm_pa_write_show_filter,
};

static struct proc_dir_entry *dir_entry = 0;

static void __init avm_pa_proc_init(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   dir_entry = proc_net_mkdir(&init_net, "avm_pa", init_net.proc_net);

   proc_create("control", S_IFREG|S_IWUSR, dir_entry, &avm_pa_control_fops);
   proc_create("brief", S_IRUGO, dir_entry, &brief_show_fops);
   proc_create("memory", S_IRUGO, dir_entry, &memory_show_fops);
   proc_create("status", S_IRUGO, dir_entry, &status_show_fops);
   proc_create("stats", S_IRUGO, dir_entry, &stats_show_fops);
   proc_create("pids", S_IRUGO, dir_entry, &pids_show_fops);
   proc_create("vpids", S_IRUGO, dir_entry, &vpids_show_fops);
   proc_create("sessions", S_IRUGO, dir_entry, &sess_show_fops);
   proc_create_data("filter", S_IRUGO|S_IWUSR, dir_entry, &selector_fops, &ctx->accel_filter);
   proc_create_data("xsession", S_IRUGO|S_IWUSR, dir_entry, &selector_fops, &ctx->show_filter);
   proc_create("bsessions", S_IRUGO, dir_entry, &bsess_show_fops);
   proc_create("macaddrs", S_IRUGO, dir_entry, &macaddr_show_fops);
#if AVM_PA_TOKSTATS
   proc_create("tokstats", S_IRUGO, dir_entry, &tstats_show_fops);
#endif
   proc_create("hashes", S_IRUGO, dir_entry, &hash_show_fops);
   proc_create("prioack", S_IRUGO, dir_entry, &prioack_show_fops);
   proc_create("priomaps", S_IRUGO, dir_entry, &priomaps_show_fops);
   proc_create("vpidpriostats", S_IRUGO, dir_entry, &vpids_all_stats_show_fops);

   /* directly in /proc/net */
   proc_create("avm_pp_queue_stats", S_IRUGO, init_net.proc_net, &vpids_hw_stats_show_fops);

   avm_pa_sg_proc_init(dir_entry);
}

static void __exit avm_pa_proc_exit(void)
{
   remove_proc_entry("control", dir_entry);
   remove_proc_entry("brief", dir_entry);
   remove_proc_entry("memory", dir_entry);
   remove_proc_entry("status", dir_entry);
   remove_proc_entry("stats", dir_entry);
   remove_proc_entry("pids", dir_entry);
   remove_proc_entry("vpids", dir_entry);
   remove_proc_entry("sessions", dir_entry);
   remove_proc_entry("filter", dir_entry);
   remove_proc_entry("xsession", dir_entry);
   remove_proc_entry("bsessions", dir_entry);
   remove_proc_entry("macaddrs", dir_entry);
#if AVM_PA_TOKSTATS
   remove_proc_entry("tokstats", dir_entry);
#endif
   remove_proc_entry("hashes", dir_entry);
   remove_proc_entry("prioack", dir_entry);
   remove_proc_entry("priomaps", dir_entry);
   remove_proc_entry("vpidpriostats", dir_entry);

   avm_pa_sg_proc_exit(dir_entry);

   remove_proc_entry("avm_pa", init_net.proc_net);
   remove_proc_entry("avm_pp_queue_stats", init_net.proc_net);

}
#endif

/* ------------------------------------------------------------------------ */
/* -------- misc device for capture tracking ------------------------------ */
/* ------------------------------------------------------------------------ */

static ssize_t avm_pa_misc_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
   return 0;
}

static unsigned int avm_pa_misc_poll(struct file *file, poll_table *wait)
{
   return 0;
}

static int avm_pa_misc_open(struct inode *inode, struct file *file)
{
   struct avm_pa_global *ctx = &pa_glob;
   atomic_inc(&ctx->misc_is_open);
   return 0;
}

static int avm_pa_misc_release(struct inode *inode, struct file *file)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (atomic_read(&ctx->misc_is_open) > 0)
      atomic_dec(&ctx->misc_is_open);
   return 0;
}


static const struct file_operations avm_pa_misc_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        .owner   =    THIS_MODULE,
#endif
        .llseek  = no_llseek,
        .read    = avm_pa_misc_read,
        .poll    = avm_pa_misc_poll,
        .open    = avm_pa_misc_open,
        .release = avm_pa_misc_release,
};

static struct miscdevice avm_pa_misc_dev = {
        .minor =    MISC_DYNAMIC_MINOR,
        .name =     "avm_pa",
        .fops =     &avm_pa_misc_fops
};

/* ------------------------------------------------------------------------ */
/* -------- init & exit functions ----------------------------------------- */
/* ------------------------------------------------------------------------ */

/*
 * early init is called before the init functions of all device drivers.
 */
int __init avm_pa_early_init(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_pid_handle pid_handle;
   int i;

   pr_info("AVM PA for %s (early init)\n", linux_banner);

   for (i = 0; i < AVM_PA_LIST_MAX; i++)
      INIT_LIST_HEAD(&ctx->sess_list[i].sessions);

   INIT_HLIST_HEAD(&ctx->egress_freelist);

   for (pid_handle = 1; pid_handle < CONFIG_AVM_PA_MAX_PID; pid_handle++) {
      struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
      atomic_set(&pid->ref.refcount, 0);
   }

   setup_timer(&ctx->tick_timer, pa_session_tick, 0);
   setup_timer(&ctx->est_timer, avm_pa_est_timer, 0);
   setup_timer(&ctx->cputime_est_timer, avm_pa_cputime_est_timer, 0);
#if AVM_LOAD_CONTROL_ENABLED
   setup_timer(&ctx->lc_timer, avm_pa_lc_timer_expired, 0);
#endif
   skb_queue_head_init(&ctx->irqqueue);
   tasklet_init(&ctx->irqtasklet, avm_pa_irq_tasklet, 0);
   hrtimer_init(&ctx->tbf.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
   ctx->tbf.timer.function = avm_pa_tbf_restart;
   skb_queue_head_init(&ctx->tbfqueue);
   tasklet_init(&ctx->tbftasklet, avm_pa_tbf_tasklet, 0);

#ifdef CONFIG_AVM_PA_RPS
   for (i = 0; i < CONFIG_AVM_PA_RPS_QUEUES; i++) {
      skb_queue_head_init(&ctx->rps[i].q_local);
      skb_queue_head_init(&ctx->rps[i].q_other);
      tasklet_init(&ctx->rps[i].ipi_task, pa_rps_ipi_task, (unsigned long) &ctx->rps[i]);
      tasklet_init(&ctx->rps[i].dequeue_task, pa_rps_dequeue_task, (unsigned long) &ctx->rps[i]);
      ctx->rps[i].csd.func = (smp_call_func_t) tasklet_schedule;
      ctx->rps[i].csd.info = &ctx->rps[i].dequeue_task;
      ctx->rps[i].csd.flags = 0;
   }
#endif

   avm_pa_init_freelist();
   avm_pa_sg_init();

   return 0;
}

/*
 * avm_pa_init is called together with the init functions
 * of the device drivers.
 */
int __init avm_pa_init(void)
{
   struct avm_pa_global *ctx = &pa_glob;

   {
      /* complain if avm_pa_pkt_info or avm_pa_dev_info crosses the reserved
       * area (usually 256 and 32 bytes respectively)
       */
      struct sk_buff *skb __maybe_unused = NULL;
      struct net_device *dev __maybe_unused = NULL;
      struct packet_type *ptype __maybe_unused = NULL;
#if defined(AVM_PKT_INFO_MAX)
      BUILD_BUG_ON(sizeof(struct avm_pa_pkt_info) > AVM_PKT_INFO_MAX);
#else
      BUILD_BUG_ON(sizeof(struct avm_pa_pkt_info) > sizeof(skb->avm_pa));
#endif
      BUILD_BUG_ON(sizeof(struct avm_pa_dev_info) > sizeof(dev->avm_pa));
      BUILD_BUG_ON(sizeof(struct avm_pa_dev_info) > sizeof(ptype->avm_pa));
   }

   pr_info("AVM PA for Linux %s (late init)\n", linux_banner);

#ifndef AVM_PA_START_DISABLED
   ctx->disabled = ctx->fw_disabled = 0;
   avm_pa_enable();
#endif

   if (misc_register(&avm_pa_misc_dev) < 0)
      pr_err("avm_pa: misc_register() failed");

#ifdef CONFIG_PROC_FS
   INIT_LIST_HEAD(&ctx->accel_filter);
   INIT_LIST_HEAD(&ctx->show_filter);

   avm_pa_proc_init();
#endif
#if defined(CONFIG_AVM_POWERMETER) && AVM_LOAD_CONTROL_ENABLED
   ctx->load_control_handle =
       avm_powermanager_load_control_register("avm_pa",
                                              avm_pa_load_control_cb,
                                              0);
#endif
   return 0;
}

void __exit avm_pa_exit(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct sk_buff *skb;

#if defined(CONFIG_AVM_POWERMETER) && AVM_LOAD_CONTROL_ENABLED
   if (ctx->load_control_handle) {
      avm_powermanager_load_control_release(ctx->load_control_handle);
      ctx->load_control_handle = 0;
   }
#endif

   ctx->disabled = 1;
   ctx->fw_disabled = 1;
   avm_pa_disable();
   tasklet_kill(&ctx->irqtasklet);
   while ((skb = skb_dequeue(&ctx->irqqueue)) != 0)
      kfree_skb(skb);
   while ((skb = skb_dequeue(&ctx->tbfqueue)) != 0)
      kfree_skb(skb);

   del_timer_sync(&ctx->tick_timer);
   pa_session_gc_once();
   pa_session_gc_once();

   avm_pa_sg_exit();

#ifdef CONFIG_PROC_FS
   avm_pa_proc_exit();

   avm_pa_selector_free(&ctx->show_filter);
   avm_pa_selector_free(&ctx->accel_filter);
#endif
   misc_deregister(&avm_pa_misc_dev);
   avm_pa_reset_stats();
}

#ifdef CONFIG_IFX_PPA
void avm_pa_disable_atm_hw_tx_acl(void){
   int n;
   struct avm_pa_global *ctx = &pa_glob;

   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      struct avm_pa_pid *pid = PA_PID(ctx, n);
      if (pid->pid_handle == 0) continue;
      if (pid->hw && pid->hw->atmvcc){
        pid->hw->flags |= AVMNET_DEVICE_IFXPPA_DISABLE_TX_ACL;
      }
   }
}
EXPORT_SYMBOL(avm_pa_disable_atm_hw_tx_acl);

void avm_pa_enable_atm_hw_tx_acl(void){
   struct avm_pa_global *ctx = &pa_glob;
   int n;
   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      struct avm_pa_pid *pid = PA_PID(ctx, n);
      if (pid->pid_handle == 0) continue;
      if (pid->hw && pid->hw->atmvcc){
        pid->hw->flags &= ~AVMNET_DEVICE_IFXPPA_DISABLE_TX_ACL;
      }
   }
}
EXPORT_SYMBOL(avm_pa_enable_atm_hw_tx_acl);
#endif

subsys_initcall(avm_pa_early_init); /* init avm pa before devices */
module_init(avm_pa_init);
module_exit(avm_pa_exit);
