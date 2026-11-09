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

struct avm_hardware_pa {
#define AVM_HW_F_NO_BSESSION (1<<0)
#define AVM_HW_F_ALL         (AVM_HW_F_NO_BSESSION)
   unsigned long flags;
#define AVM_PA_HARDWARE_PA_HAS_PROBE_SESSION
   /** Probe if a session is accelerable by hardware or not
    *
    * This doesn't add a session but evaluates if it could be added,
    * thus providing an indication if the session would violate the
    * hardware constraints.
    *
    * This will be called before activating a session. In this context there
    * may be race conditions w.r.t. to probing the same session concurrently
    * as the race is decided by activating one of them. In particular, one of
    * those session might be in add_session() already.
    *
    * add_session is likely called following this but it can't be guaruanteed (see
    * discussion about possibly races above).You may use avm_pa_set_hw_session() to stash
    * some data to add_session() but care must be taked to not leak resources if
    * add_session isn't called. If dynamic memory is needed here you may use RCU to free.
    *
    * @param session the session to be probed
    *
    * @retval AVM_PA_TX_SESSION_ADDED if the session is OK to offload
    * @retval AVM_PA_TX_SESSION_EXISTS if the session is already offloaded (by race)
    * @retval AVM_PA_TX_SESSION_BUSY if the session is OK to offload but resource outage doesn't permit
    * @retval AVM_PA_TX_ERROR_SESSION if the session cannot be offloaded
    */
   int (*probe_session)( struct avm_pa_session *avm_session );
   int (*add_session)( struct avm_pa_session *avm_session );
   int (*change_session)( struct avm_pa_session *avm_session );
   int (*remove_session)( struct avm_pa_session *avm_session );
   const char *(*session_state)( struct avm_pa_session *avm_session );

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

   /* hardware statistic, values since last call */
#define AVM_PA_HARDWARE_PA_HAS_SESSION_STATS
   int  (*session_stats)(struct avm_pa_session *avm_session,
                         struct avm_pa_session_stats *ingress);
#define AVM_PA_HARDWARE_PA_HAS_ADD_SESSION_SKB
   int  (*add_session_skb)(struct avm_pa_session *avm_session, struct sk_buff *skb);

#define AVM_PA_HARDWARE_PA_HAS_CHECK_SESSION
   /** Check if hardware session is still valid.
    *
    * Not implementing this is equivalent to always returning AVM_HW_CHK_OK.
    */
   unsigned int (*check_session)(struct avm_pa_session *avm_session);
   /** AVM_HW_CHK_OK: hardware session remains valid
    * (but can still timeout via session_stats callback)
    */
#define AVM_HW_CHK_OK    0x0
   /** AVM_HW_CHK_FLUSH: hardware session became invalid, request
    * session to be flushed (including the software session). Return
    * this is if the session must be replaced by a new one.
    *
    * Beware that avm_pa will collect stats one last time for that session.
    */
#define AVM_HW_CHK_FLUSH 0x1
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
