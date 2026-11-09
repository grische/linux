/*
 * Packet Accelerator Interface - Hardware Support definitions
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

#ifndef _LINUX_AVM_PA_HW_H
#define _LINUX_AVM_PA_HW_H

/* ------------------------------------------------------------------------ */

#include <linux/version.h>
#include <linux/list.h>

#define AVM_PA_HW_FEATURE_DSLITE    0x00000001
#define AVM_PA_HW_FEATURE_6TO4      0x00000002
#define AVM_PA_HW_FEATURE_L2TP      0x00000004
#define AVM_PA_HW_FEATURE_GRE       0x00000008

#define ALLOC_VIRT_DEV_FAILED         -1
#define FREE_VIRT_DEV_FAILED          -1

void avm_pa_rx_channel_suspend(avm_pid_handle pid_handle);
void avm_pa_rx_channel_resume(avm_pid_handle pid_handle);
void avm_pa_rx_channel_packet_not_accelerated(avm_pid_handle pid_handle, struct sk_buff *skb);
void avm_pa_tx_channel_accelerated_packet(avm_pid_handle pid_handle, avm_session_handle session_handle, struct sk_buff *skb);

struct avm_pa_virt_rx_dev {
   avm_pid_handle   pid_handle;
   /* 3 bytes hole */

   /* HW Accelerator private */
   unsigned long  hw_dev_id;
   unsigned long  hw_pkt_try_to_acc;
   unsigned long  hw_pkt_try_to_acc_dropped;
   unsigned long  hw_pkt_try_to_acc_virt_chan_not_ready;
   unsigned long  hw_pkt_slow_cnt;
};

struct avm_pa_virt_tx_dev {
   avm_pid_handle   pid_handle;
   /* 3 bytes hole */

   /* HW Accelerator private */
   unsigned long    hw_dev_id;
   unsigned long    hw_pkt_tx;
   unsigned long    hw_pkt_tx_session_lookup_failed;
};

struct avm_pa_session_hw_stats {
   u64 bytes;     /** absolute count of hardware bytes for a session */
   u64 pkts;      /** absolute count of hardware packets for a session */
   u16 byte_bits; /** size of the byte counter to detect overflow (initialized to 64)*/
   u16 pkt_bits;  /** size of the packet counter to detect overflow (initialized to 64) */
};

struct avm_hardware_pa {
#define AVM_HW_F_NO_BSESSION (1<<0)
#define AVM_HW_F_ALL         (AVM_HW_F_NO_BSESSION)
   unsigned long flags;
#define AVM_PA_HARDWARE_PA_HAS_PROBE_SESSION
   /** Probe if a session is accelerable by hardware or not
    *
    * This doesn't add a session but evaluates if it could be offloaded,
    * thus providing an indication if the session would violate the
    * hardware constraints.
    *
    * This will be called before activating a session. In this context there
    * may be race conditions w.r.t. to probing the same session concurrently,
    * if two packets are processed by avm_pa concurrently, as the race is
    * decided after probe_session(), by activating one of them. In particular,
    * one of those session might be in or past bind_session() already.
    *
    * On success, bind_session is typically called following this but it can't be
    * guaranteed (see discussion about possible races above). You may use
    * avm_pa_set_hw_session() to stash some data for bind_session().
    * You may free data in remove_session, which is always called following a OK
    * return here, regardless of bind_session().
    *
    * If the vendor APIs require a suitable SKB, for example with a certain
    * field set by the hardware, and the SKB doesn't meet that requirement,
    * then the special return code AVM_HW_PROBE_BYPASS can be used to indicate that.
    * As a result, avm_pa will not accelerate the session yet, not even in software.
    * avm_pa will retry with later SKBs. Be careful to not enter a systematic bypass
    * condition: only return it if later SKBs for the same type of traffic will
    * meet the requirement, otherwise let avm_pa do it in software.
    *
    * @param session the session to be probed
    * @param skb the SKB that triggers the session (on egress)
    *
    * @retval AVM_HW_PROBE_OK if the session is OK to offload
    * @retval AVM_HW_PROBE_LOCAL if the session terminates locally and needs to be exempted from anti-DoS ratelimiting
    * @retval AVM_HW_PROBE_ERR if the offload is not possible (more specific reasons below)
    * @retval AVM_HW_PROBE_BUSY if the session is OK to offload but resource outage doesn't permit
    * @retval AVM_HW_PROBE_NOTSUP if the type of traffic isn't offloadable in general
    * @retval AVM_HW_PROBE_BYPASS if offload may work OK but not for this skb.
    * @retval AVM_HW_PROBE_ALREADY if the session is already offloaded (by race)
    */
   int (*probe_session)(struct avm_pa_session *s, struct sk_buff *skb);
#define AVM_HW_PROBE_LOCAL        2
#define AVM_HW_PROBE_OK           0
#define AVM_HW_PROBE_ERR         -1
#define AVM_HW_PROBE_BUSY        -2
#define AVM_HW_PROBE_NOTSUP      -3
#define AVM_HW_PROBE_BYPASS      -4
#define AVM_HW_PROBE_ALREADY     -5

   /** Bind a session to the hardware, making offload effective.
    *
    * Following a successful probe_session, this will be called to make
    * the offload effective.
    *
    * This will be called after activating a session. This context is race-free w.r.t.
    * to other sessions for the same flow, i.e. only one of multiple concurrent sessions
    * for the same flow can possibly become active. And bind_session is called
    * exactly once (or not at all) for a given avm_pa_session. Therefore, bind_session
    * does not need to be concerned with race conditions.
    *
    * While race conditions carefully avoided, avm_pa can still call this
    * for the same flow, if the original session is flushed. Typically this results
    * in a new session (for a subsequent ingress packet) before the original session
    * is unbound via unbind_session, because of a grace period after flushing.
    *
    * Session binding can still fail, in which case the avm_pa will continue with
    * the software fast path (temporary resource outage should be indicated by
    * a preceding probe_session()).
    *
    * Session binding can be asynchronous, it doesn't really matter for avm_pa.
    * If asynchronous completion is required return AVM_HW_BIND_ASYNC (which is an
    * alias for AVM_HW_BIND_OK) and do the actual binding in the background. In the
    * meantime, avm_pa would simply use the fast path for further packets.
    *
    * @param s the session to be offloaded
    * @param skb the SKB that triggers the session (on egress)
    *
    * @retval AVM_HW_BIND_OK on success (offload established)
    * @retval AVM_HW_BIND_ASYNC indicate that actual bind is done in the background (assume success)
    * @retval AVM_HW_BIND_ERR session not bound (no retry -> no offload)
    */
   int (*bind_session)(struct avm_pa_session *s, struct sk_buff *skb);
#define AVM_HW_BIND_OK       0
#define AVM_HW_BIND_ASYNC    AVM_HW_BIND_OK
#define AVM_HW_BIND_ERR      -1

   /** Unbind a sesion from the hardware.
    *
    * this will be called if, and only if, bind_session() was successful at some point
    * to reverse that operation. The call may be repeated until success is returned.
    *
    * The purpose is really just to cancel the offload. The session is still "owned"
    * by the hardware_pa and avm_pa will not delete it until a successful
    * remove_session.
    *
    * If AVM_HW_UNBIND_STATS is returned, then other callbacks can still be called as
    * for the session. Most prominently this includes session_stats() to retrieve
    * final counters for the session. Otherwise, all further callbacks (except
    * remove_sessions) are supressed.
    *
    * Like for bind_session(), the operation can be asynchronous, which is currently
    * not handled differently.
    *
    * @param s session to cancel offload for
    * 
    * @retval AVM_HW_UNBIND_OK session unbound successfully
    * @retval AVM_HW_UNBIND_STATS session unbound but statistics are dated (call session_stats)
    * @retval AVM_HW_UNBIND_ASYNC indicate that actual unbind is done in the background (assume success)
    * @retval AVM_HW_UNBIND_ERR session not unbound, retry
    */
   int (*unbind_session)(struct avm_pa_session *s);
#define AVM_HW_UNBIND_STATS    1
#define AVM_HW_UNBIND_OK       0
#define AVM_HW_UNBIND_ASYNC    AVM_HW_UNBIND_OK
#define AVM_HW_UNBIND_ERR      -1
   /** Change an existing session to add an egress.
    *
    * This optional callback is only ever invoked for multicast sessions to add one
    * egress. The session is still initially offloaded by bind_session().
    *
    * If the change cannot be made it is expected that the session becomes unbound.
    * That is, avm_pa will only call remove_session but not unbind_session().
    *
    * If the callback is not provided, unbind_session() will be called directly.
    *
    * Either way, remove_session() will be called as soon as possible to allow freeing
    * hardware resources. However, the session will remain active in the software fastpath.
    *
    * @retval AVM_HW_CHANGE_OK on success (egress added)
    * @retval AVM_HW_CHANGE_ERR egress not added, session now unbound (no retry!)
    */
   int (*change_session)(struct avm_pa_session *s, struct sk_buff *skb);
#define AVM_HW_CHANGE_OK          0
#define AVM_HW_CHANGE_ERR        -1

   /** Remove a session from the hardware_pa supervision.
    *
    * Following a successful probe_session, this will be called to release
    * session resources in the hardware_pa as well as references in the hardware.
    *
    * It will be called even if bind_session() failed or wasn't called at all.
    *
    * The purpose is two-fold: First, if the hardware_pa aquires resources
    * during probe_session(), this is the place to release them. Second, if
    * the hardware still has a handle to the flow after an effective offload,
    * this is the place to tell it to let loose. For example, avm_pa will
    * still attempt to get counters after unbind_session(), among other things.
    * On the other hand, remove_session() is the very last hardware_pa call.
    *
    * This is called typically from the internal timer, once a session reaches
    * its timeout. As a special case, this is also called from the datapath
    * when adding an egress to a multicast session and change_session() is not
    * implemented.
    *
    * Also, this is called repeatedly unless it returns success. Typically, this means
    * it's called exactly once. But for some return codes avm_pa will retry:
    * For AVM_HW_REM_ERR, avm_pa assumes the session is in an error condition and will
    * avoid calling any other callback. For AVM_HW_REM_AGAIN, on the other hand, avm_pa
    * will continue to poll statistics through session_stats() (until something else is
    * returned).
    *
    * Like for bind_session(), the operation can be asynchronous, which is currently
    * not handled differently.
    *
    * @param s the session to be removed
    *
    * @retval AVM_HW_REM_OK session removed successfully
    * @retval AVM_HW_REM_ASYNC indicate that actual removal is deferred (check hw_session for completion)
    * @retval AVM_HW_REM_AGAIN session not removed, retry while still collecting statistics
    * @retval AVM_HW_REM_ERR session not removed, retry without any other callbacks
    */
   int (*remove_session)(struct avm_pa_session *s);
#define AVM_HW_REM_ASYNC    2
#define AVM_HW_REM_AGAIN    1
#define AVM_HW_REM_OK       0
#define AVM_HW_REM_ERR      -1

   const char *(*session_state)(struct avm_pa_session *avm_session, char *buf, size_t sz);

   /* virtual device handling */
   int  (*alloc_rx_channel)(avm_pid_handle pid_handle);
   int  (*alloc_tx_channel)(avm_pid_handle pid_handle);
   int  (*free_rx_channel)(avm_pid_handle pid_handle);
   int  (*free_tx_channel)(avm_pid_handle pid_handle);
   /** Try to offload a packet as early as possible
    *
    * The intent is to send the packet to a special code path that ultimately
    * results in the packet transmission being offloaded. Therefore, depending
    * on the return code, avm_pa assumes the packet is gone.
    *
    * You can use the alloc_rx_channel callback to prepare such a code path
    * for a given pid.
    *
    * Alternatively you can use avm_pa_pid_session_receive() if you can
    * extract the avm_pa session from the packet, this counts as offloaded.
    *
    * avm_pa calls this function only from avm_pa_dev_pid_receive() (and not
    * avm_pa_dev_receve()!) so this call must be integrated into the relevant
    * networking drivers.
    *
    * @param pid_handle The avm_pa pid handle on which the packet is received
    * @param skb The packet
    *
    * @retval AVM_PA_RX_ERROR_* There was an error and the packet has been dropped
    * @revtal AVM_PA_RX_STOLEN The packet has been consumed (e.g. offloaded)
    * @retval AVM_PA_RX_OK The packet cannot be offloaded and avm_pa should accelerate
    * @retval AVM_PA_RX_BYPASS The packet cannot be offloaded and avm_pa should not process it any further
    */
   int  (*try_to_accelerate)(avm_pid_handle pid_handle, struct sk_buff *skb);

   /** Retrieve counters for offloaded sessions
    *
    * The counters are assumed to correspond to ingress traffic, i.e. bytes are
    * accounted for packets at the ingress. avm_pa will perform adjustment
    * when egress counters are requested via session group counters
    * (see @ref avm_pa_sg_set_count_egress() und @ref avm_pa_sg_get_stats()).
    *
    * Both flavors, relative and absolute counters are supported. To support
    * relative counters, avm_pa fills the previous counter values in @a stats.
    * Then the new counter values can be added. For absolute counters simply
    * overwrite the values. Don't touch the counters if the they can't be provided,
    * but potentially AVM_HW_STAT_HIT explicitly, as avm_pa will deduce timeout
    * without packet progress.
    *
    * The return code indicates what avm_pa should do with the session. AVM_HW_STAT_OK
    * indicates that everything is normal and avm_pa shall detect session timeout
    * just based on the traffic counters. AVM_HW_STAT_HIT can be returned to indicate
    * ongoing offload even if the counters woud suggest no progress. Use this if
    * the HW cannot provide valid counters but has a indication for traffic progress.
    *
    * Effectively, AVM_HW_STAT_HIT prevents session timeout for as long as this is
    * returned, while AVM_HW_STAT_OK allows for the normal, counter-based timeout mechanism.
    *
    * @param s session to get byte and packet counters for
    * @param stats statistic struct to fill with counters
    *
    * @retval AVM_HW_STAT_OK Everything normal, session timeout based on packet counters
    * @retval AVM_HW_STAT_HIT Explicit hit flag, session will not timeout
    */
#define AVM_PA_HARDWARE_PA_HAS_SESSION_STATS 2
   int  (*session_stats)(struct avm_pa_session *s,
                         struct avm_pa_session_hw_stats *stats);
#define AVM_HW_STAT_OK  0
#define AVM_HW_STAT_HIT 1

#define AVM_PA_HARDWARE_PA_HAS_CHECK_SESSION
   /** Check if hardware session is still valid.
    *
    * Not implementing this is equivalent to always returning AVM_HW_CHK_OK.
    */
   int (*check_session)(struct avm_pa_session *avm_session);
   /** AVM_HW_CHK_OK: hardware session remains valid
    * (but can still timeout via session_stats callback)
    */
#define AVM_HW_CHK_OK     0
   /** AVM_HW_CHK_FLUSH: hardware session became invalid, request
    * session to be flushed (including the software session). Return
    * this is if the session must be replaced by a new one.
    *
    * Beware that avm_pa will collect stats one last time for that session.
    */
#define AVM_HW_CHK_FLUSH  1
#define AVM_HW_CHK_ERR   -1
};

#ifdef CONFIG_AVM_PA

/**
 * Register a hardware_pa implementation.
 *
 * As of now, only one hardware_pa instance is supported at a time.
 *
 * @param pa_functions Set of callbacks to add/remove sessions etc.
 *
 * @return 0 on success, < 0 on error.
 */
int avm_pa_register_hardware_pa(struct avm_hardware_pa *pa_functions);

/**
 * Un-register a hardware_pa implementation.
 *
 * The function set passed when registering must be passed again.
 *
 * The function returns immediately but the effect may be delayed.
 * Iff 0 is returned you can use the completion to get signalled appropriately.
 *
 * @see avm_pa_unregister_hardware_pa_sync()
 *
 * @param pa_functions Set of callbacks to add/remove sessions etc.
 * @param done completion to be signaled when the function is truly done.
 *
 * @retval 0 Success, may wait for the completion (if any).
 * @retval -ENODEV This hardware_pa wasn't registered before.
 */
int avm_pa_unregister_hardware_pa(struct avm_hardware_pa *pa_functions,
                                  struct completion *done);
#define AVM_PA_UNREGISTER_HARDWARE_PA 1

/**
 * Un-register a hardware_pa implementation (blocking).
 *
 * Same as @ref avm_pa_unregister_hardware_pa() but uses a completion internally
 * to synchronize, so it may block!
 *
 * The function set passed when registering must be passed again.
 *
 * @see avm_pa_unregister_hardware_pa()
 *
 * @param pa_functions Set of callbacks to add/remove sessions etc.
 * 
 * @retval 0 Sucess.
 * @retval -ENODEV This hardware_pa wasn't registered before.
 */
int avm_pa_unregister_hardware_pa_sync(struct avm_hardware_pa *pa_functions);
#define AVM_PA_UNREGISTER_HARDWARE_PA_SYNC 1

/**
 * Return hardware_pa state (installed and enabled).
 *
 * @reval 1 hardware_pa is registered and enabled
 * @retval 0 hardware_pa is not registered or currently disabled
 */
int avm_pa_is_hardware_pa_active(void);
#define AVM_PA_IS_HARDWARE_PA_ACTIVE 1

static inline void *avm_pa_get_hw_session( struct avm_pa_session *session)
{
   smp_rmb();
   return session->hw_session;
}

static inline void avm_pa_set_hw_session( struct avm_pa_session *session,
                                    void *hw_session)
{
   session->hw_session = hw_session;
   smp_wmb();
}

/** Modify and forward a packet directly
 *
 * If the session can be looked up by the hardware_pa, then this function
 * can enable an even faster fast path by avoiding to parse the packet.
 *
 * The packet will be modified and forwarded directly, according to the session.
 * The packet is still verified, e.g. to detect TCP FIN. Thus, acceleration
 * may not succeed.
 *
 * @param pid_handle ingress pid
 * @param session_handle session handle to which the packet belongs to
 * @param skb the packet to forward
 */
int avm_pa_pid_session_receive(avm_pid_handle pid_handle, avm_session_handle session_handle, struct sk_buff *skb);
#define AVM_PA_PID_SESSION_RECEIVE 1

#else
/* TODO: add stubs for !CONFIG_AVM_PA when necessasry */
static inline int avm_pa_pid_session_receive(avm_pid_handle pid_handle, avm_session_handle session_handle, struct sk_buff *skb)
{
   return AVM_PA_RX_OK;
}
#endif

/* ------------------------------------------------------------------------ */

#endif /* _LINUX_AVM_PA_HW_H */
