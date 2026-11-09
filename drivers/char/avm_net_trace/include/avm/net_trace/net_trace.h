#ifndef _AVM_NET_TRACE_H
#define _AVM_NET_TRACE_H

#include <linux/if.h>
#include <linux/skbuff.h>
#include <avm/net_trace/ioctl.h>
#include <linux/device.h>
#include <linux/version.h>

#define AVM_NET_TRACE_VERSION		4

#define AVM_NET_TRACE_USE_SKB		0
#define AVM_NET_TRACE_CLONE_SKB		1
#define AVM_NET_TRACE_COPY_SKB		2

#define AVM_NET_TRACE_DIRECTION_HOST		0
#define AVM_NET_TRACE_DIRECTION_OUTGOING 	1

#define AVM_NET_TRACE_STATE_CHG_OPEN	0
#define AVM_NET_TRACE_STATE_CHG_RELEASE	1

#define PCAP_ENCAP_RFC1483	11
#define PCAP_ENCAP_ETHER	1
#define PCAP_ENCAP_PPP		9
#define PCAP_ENCAP_IP		14
#define PCAP_ENCAP_IEEE802_11	105
#define PCAP_ENCAP_PRISM_HEADER	119

struct avm_net_trace_device;

struct avm_net_trace {
	struct avm_net_trace_device 	*ntd;
	struct device 			* udev_device;
	unsigned int			minor;
	volatile int			is_open;
	volatile int			do_trace;
	int				got_header;
	unsigned long			dropped;
	int				rbuf_size;
	wait_queue_head_t		recvwait;
	struct sk_buff_head		recvqueue;
	void				(*ant_state_change)(struct avm_net_trace_device *, int);
};

struct avm_net_trace_device {
	/* global */
	char	name[AVM_NET_TRACE_IFNAMSIZ];
	int	minor;
	int	iface;/* 0,1,...*/
	int	type; /* 1 (LAN...), 2 (PVC), 3 (DSLIFACE), 4 (WLAN), 5 (USB) */
	int	pcap_encap;
	int	pcap_protocol;

	/* private */
	struct avm_net_trace *ant;
};

#ifdef CONFIG_AVM_NET_TRACE

extern int __avm_net_trace_func (
		struct avm_net_trace *ant, struct sk_buff *skb, 
		int skb_property, int direction);

static inline int __avm_net_trace_in_progress (struct avm_net_trace_device *ntd) {

	struct avm_net_trace *ant;

	ant = ntd->ant;

	if (ant == NULL)
		return 0;

	if (!ant->do_trace)
		return 0;

	if ((skb_queue_len (&ant->recvqueue) > 20) && (ant->rbuf_size <= 0)) {
		ant->dropped++;
		return 0;
	}

	return 1;
}

static inline int avm_net_trace_func (
		struct avm_net_trace_device *ntd, struct sk_buff *skb, 
		int skb_property, int direction) {

	struct avm_net_trace *ant = ntd->ant;

	if (__avm_net_trace_in_progress (ntd))
		return __avm_net_trace_func (ant, skb, skb_property, direction);

	return -1;
}

extern int register_avm_net_trace_device (struct avm_net_trace_device *ntd);
/**
 * \brief    Register a permanent trace point.
 * \details  The trace point is always open and can receive frames. If the queue
 *           extends it's limit the oldest frame will be discarded.
 *           If the user opens the trace point the content of the queue is
 *           delivered.
 *           While open the trace points behaves like a normal trace point. That
 *           is, frames are delivered on-the-fly.
 *           If the user closes the trace point queuing is reactivated.
 * \params   ntd        Trace point definition
 * \params   rbuf_size  The queue is working like a ring buffer. rbuf_size
 *                      determiens the max. number of frames in the buffer.
 */
extern int register_avm_net_trace_device_permanent (struct avm_net_trace_device *ntd, int rbuf_size);
extern void deregister_avm_net_trace_device (struct avm_net_trace_device *ntd);

extern int register_avm_net_device_state_change_cb (struct avm_net_trace_device *ntd, void (*cb)(struct avm_net_trace_device *, int));
extern int deregister_avm_net_device_state_change_cb (struct avm_net_trace_device *ntd);

#else /* CONFIG_AVM_NET_TRACE  */

static inline int __avm_net_trace_in_progress (struct avm_net_trace_device *ntd) { return 0; }

static inline int avm_net_trace_func (
		struct avm_net_trace_device *ntd, struct sk_buff *skb, 
		int skb_property, int direction) { return 0; }

static inline int register_avm_net_trace_device_permanent (struct avm_net_trace_device *ntd, int rbuf_size) { return 0; }
static inline int register_avm_net_trace_device (struct avm_net_trace_device *ntd) { return 0; }
static inline void deregister_avm_net_trace_device (struct avm_net_trace_device *ntd) { return; }

#endif /* CONFIG_AVM_NET_TRACE  */

#endif /* _AVM_NET_TRACE_H */

