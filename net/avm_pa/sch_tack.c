/*
 * net/sched/sch_tack.c	The turbo ack queue
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Thomas Martitz <tmartitz@avm.de>
 *		Matthias Scheer <m.scheer@avm.de>
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/jhash.h>
#include <net/pkt_sched.h>
#include <net/flow_dissector.h>
#include <net/ip.h>
#include <net/tcp.h>

#ifdef CONFIG_AVM_PA
#include <avm/pa/avm_pa.h>
#endif

/* Single band ack compression accelerator (turbo ack) */

#define TACK_MAX_HASH 32
#define TACK_HASH_MASK (TACK_MAX_HASH-1)
#define TACK_PERIOD_DEFAULT 1 /* jiffies */
#define TACK_COUNT_DEFAULT 4

static inline unsigned int tack_hash_get(struct sk_buff *skb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
	return skb->rxhash;
#else
	return skb->hash;
#endif
}

static inline void tack_hash_set(struct sk_buff *skb, unsigned int hash)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
	skb->rxhash = hash;
#else
	skb->hash = hash;
#endif
}

#define TACK_SLOT(skb) (tack_hash_get(skb) & TACK_HASH_MASK)

static unsigned int tack_period = TACK_PERIOD_DEFAULT;
module_param(tack_period, uint, 0644);
MODULE_PARM_DESC(tack_period, "max jiffies to elapse before sending an ACK");

static unsigned int tack_count = TACK_COUNT_DEFAULT;
module_param(tack_count, uint, 0644);
MODULE_PARM_DESC(tack_count, "max number of subsequent ACKs to compress");

struct tack_priv {
	struct sk_buff *hash_map[TACK_MAX_HASH];
	unsigned long   time_out[TACK_MAX_HASH];
	unsigned int    agg_acks[TACK_MAX_HASH];
	unsigned int    acks_in_flight;
};

struct tack_skb_cb {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	struct flow_keys_digest keys;
	u16                     thoff;
	u8                      ip_proto;
#else
	struct flow_keys        keys;
#endif
};

static inline struct tack_skb_cb *tack_skb_cb(const struct sk_buff *skb)
{
	qdisc_cb_private_validate(skb, sizeof(struct tack_skb_cb));
	return (struct tack_skb_cb *)qdisc_skb_cb(skb)->data;
}

static bool make_hash(struct sk_buff *skb, struct flow_keys *keys)
{
	if (!skb_flow_dissect_flow_keys(skb, keys, 0))
		return false;

	tack_hash_set(skb, flow_hash_from_keys(keys));

	return true;
}

static bool tack_candidate(struct sk_buff *skb)
{
	struct tcphdr *tcph;
	__be32 flags;
	__be32 bad_flags = TCP_FLAG_SYN | TCP_FLAG_URG | TCP_FLAG_RST;
	__be32 mandatory_flags = TCP_FLAG_ACK;
	u8 *opt, *opt_end, *opt_start;

	/* skb_flow_dissect() checks for ip fragementation
	 * and transport layer protocol
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	if (tack_skb_cb(skb)->ip_proto != IPPROTO_TCP)
#else
	if (tack_skb_cb(skb)->keys.ip_proto != IPPROTO_TCP)
#endif
		return false;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	tcph = (struct tcphdr *) (skb->data + tack_skb_cb(skb)->thoff);
#else
	tcph = (struct tcphdr *) (skb->data + tack_skb_cb(skb)->keys.thoff);
#endif

#ifdef CONFIG_AVM_PA
	/* avm_pa stores the prioack result within the skb's pkt_info to
	 * signal the priority of the new session to the transmit hook.
	 * Prevent avm_pa from creating a hardware session from ACK flows.
	 */
	if (AVM_PKT_INFO(skb)->prioack_result == PRIOACK_ACK)
		AVM_PKT_INFO(skb)->no_hw = 1;
#endif

	/* on GRX sch_tack receives data from all classes
	 * -> reject skbs with priority != 5
	 */
	if ((skb->priority & 0xF) != 5)
		return false;

	/* do not pass if any of bad_flags is set,
	 * or if any of mandatory flags is unset
	 */
	flags = tcp_flag_word(tcph) ^ mandatory_flags;
	if (flags & (bad_flags | mandatory_flags))
		return false;

	/* do not pass ACK with payload */
	if ((u32)skb->tail - (u32)tcph - (tcph->doff * 4))
		return false;

	/* Process options and deny when we hit unsupported ones */
	opt = opt_start = (u8 *)(tcph + 1);
	opt_end = opt + (tcph->doff * 4) - sizeof(struct tcphdr);
	while (opt < opt_end) {
		switch (*opt) {
		case TCPOPT_EOL:
			opt = opt_end;
			break;
		case TCPOPT_NOP:
			opt += 1;
			continue;
		case TCPOPT_MSS:
		case TCPOPT_WINDOW:
		case TCPOPT_TIMESTAMP:
			if (opt[1] > 0) {
				opt += opt[1];
				continue;
			}
			/* fall through */
		default:
			/* all unhandled ones are unsupported */
			return false;
		}
	}

	return true;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static struct sk_buff *merge_tcp_acks(struct sk_buff *skb, struct sk_buff *skb0,
				      struct sk_buff **to_free)
#else
static struct sk_buff *merge_tcp_acks(struct sk_buff *skb, struct sk_buff *skb0)
#endif
{
	struct tcphdr *tcph, *tcph0;
	struct sk_buff *nskb = NULL;
	unsigned long ack, ack0;

	/* we know here's a tcp header because both skbs passed tack_candidate() */
	tcph = tcp_hdr(skb);
	tcph0 = tcp_hdr(skb0);

	ack = ntohl(tcph->ack_seq);
	ack0 = ntohl(tcph0->ack_seq);

	/* Common case: ack if the more recent skb is larger. Replace the already-queued
	 * skb. Caveat: ack0+1 as a simple, state-less measure to avoid merging the final
	 * hand-shake ACK (which carries a relative ACK number of 1). Use time_* since
	 * they handle wrapping
	 */
	if (time_after(ack, ack0 + 1)) {
		/* nskb == skb: Replace skb0 with skb in the queue.
		 * remember that we're called with sch->q.lock taken so
		 * we can freely modify the queue
		 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		struct sk_buff *tmp = skb0->next;
		nskb = skb_morph(skb0, skb);
		nskb->next = tmp;
		__qdisc_drop(skb, to_free);
#else
		__skb_queue_after(&sch->q, skb0, skb);
		__skb_unlink(skb0, &sch->q);
		nskb = skb;
		kfree_skb(skb0);
#endif
	/* less common case: Received an older ACK, due to reordering */
	} else if (time_before(ack, ack0)) {
		/* do not queue, the already queued is re-used */
		nskb = skb0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		__qdisc_drop(skb, to_free);
#else
		kfree_skb(skb);
#endif
	}

	/* do not accelerate duplicated ACK as they indicate packet loss */
	return nskb;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static int tack_enqueue(struct sk_buff *skb, struct Qdisc *sch, struct sk_buff **to_free)
#else
static int tack_enqueue(struct sk_buff *skb, struct Qdisc *sch)
#endif
{
	struct tack_priv *priv = qdisc_priv(sch);
	struct tack_skb_cb *cb = tack_skb_cb(skb);
	struct sk_buff *skb0;
	struct flow_keys keys;

	if (!make_hash(skb, &keys))
		goto end;

	make_flow_keys_digest(&cb->keys, &keys);
	cb->thoff = keys.control.thoff;
	cb->ip_proto = keys.basic.ip_proto;

	skb0 = priv->hash_map[TACK_SLOT(skb)];
	if (skb0) {
		if (!memcmp(&cb->keys, &tack_skb_cb(skb0)->keys, sizeof(cb->keys))) {
			/* the session truly matches, check if we can accelerate */
			if (tack_candidate(skb)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
				struct sk_buff *nskb = merge_tcp_acks(skb, skb0, to_free);
#else
				struct sk_buff *nskb = merge_tcp_acks(skb, skb0);
#endif
				if (nskb) {
					priv->hash_map[TACK_SLOT(skb)] = nskb;
					priv->agg_acks[TACK_SLOT(skb)]++;
					return NET_XMIT_DROP;
				}
			} else {
				/* If we encounter an unsuitable ACK forget about any
				 * previous candidate to avoid potential ACK reordering
				 */
				priv->hash_map[TACK_SLOT(skb)] = NULL;
				priv->acks_in_flight--;
			}
		}
		/* FIXME: only the first flow is accelerated */
	} else if (tack_candidate(skb)) {
		priv->hash_map[TACK_SLOT(skb)] = skb;
		priv->time_out[TACK_SLOT(skb)] = jiffies + tack_period;
		priv->agg_acks[TACK_SLOT(skb)] = 1;
		priv->acks_in_flight++;
	}

end:
	return qdisc_enqueue_tail(skb, sch);
}

static struct sk_buff *tack_dequeue(struct Qdisc *sch)
{
	struct sk_buff *skb = qdisc_dequeue_head(sch);
	struct tack_priv *priv = qdisc_priv(sch);

again:
	if (skb && priv->hash_map[TACK_SLOT(skb)] == skb) {
		/* skb is ACK in hash map, shall we send it now? */
		if (priv->agg_acks[TACK_SLOT(skb)] >= tack_count ||
		time_after_eq(jiffies, priv->time_out[TACK_SLOT(skb)])) {
			/* time or count elapsed, send */
			priv->hash_map[TACK_SLOT(skb)] = NULL;
			priv->acks_in_flight--;
		} else /* keep back */ {
			qdisc_enqueue_tail(skb, sch);
			if (sch->q.qlen > priv->acks_in_flight) {
				skb = qdisc_dequeue_head(sch);
				goto again;
			}
			return NULL;
		}
	}
	return skb;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
static unsigned int tack_drop(struct Qdisc *sch)
{
	struct sk_buff *skb = qdisc_peek_head(sch);
	struct tack_priv *priv = qdisc_priv(sch);

	/* first skb not in hash map can be dropped safely */
	while (skb && priv->hash_map[TACK_SLOT(skb)] == skb)
		skb = skb->next;

	if (likely(skb != NULL)) {
		unsigned int len = qdisc_pkt_len(skb);

		__skb_unlink(skb, &sch->q);
		kfree(skb);
		return len;
	}

	return 0;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
static int tack_init(struct Qdisc *sch, struct nlattr *opt)
#else
static int tack_init(struct Qdisc *sch, struct nlattr *opt,
					struct netlink_ext_ack *extack)
#endif
{
	struct tack_priv *priv = qdisc_priv(sch);
	unsigned int i;

	sch->flags |= TCQ_F_ONETXQUEUE;
	priv->acks_in_flight = 0;
	for (i = 0; i < TACK_MAX_HASH; i++) {
		priv->hash_map[i] = NULL;
		priv->time_out[i] = jiffies;
		priv->agg_acks[i] = 0;
	}
	return 0;
}

struct Qdisc_ops tack_qdisc_ops __read_mostly = {
	.id		=	"tack",
	.priv_size	=	sizeof(struct tack_priv),
	.enqueue	=	tack_enqueue,
	.dequeue	=	tack_dequeue,
	.peek		=	qdisc_peek_head,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	.drop		=	tack_drop,
#endif
	.init		=	tack_init,
	.reset		=	qdisc_reset_queue,
	.change		=	tack_init,
	.owner		=	THIS_MODULE,
};
EXPORT_SYMBOL(tack_qdisc_ops);

static int __init tack_module_init(void)
{
	return register_qdisc(&tack_qdisc_ops);
}
static void __exit tack_module_exit(void)
{
	unregister_qdisc(&tack_qdisc_ops);
}
module_init(tack_module_init)
module_exit(tack_module_exit)
MODULE_LICENSE("GPL");
