/*
 * vim:set noexpandtab shiftwidth=8:
 *
 * Low Latency Queueing
 *   Class-Based Weighted Fair Queueing with strict priorities
 *
 * Copyright (C) 2007 AVM GmbH
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * Authors: Carsten Paeth, <c.paeth@avm.de>
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <net/ip.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>

/* tcf_classify and tcf_block_put */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
#include <net/pkt_cls.h>
#endif

/*  LLQ algorithm.
 *  =======================================
 *  The qdisc contains several classes each has assigned
 *  a priority and a weight. Classes with higher priorities
 *  are only served, when there are no packets in all classes
 *  with lower priority. Classes with same priorities will be served
 *  proportionaly to there weights using "Deficit Round Robin".
 */

/* global struct definitions in uapi/linux/pkt_sched.h */

/* -------------------------------------------------------------------- */
#define LLQ_TRACE   0	/* trace output for most functions */
#define LLQ_QTRACE	0   /* trace output for enqueue/dequeue/requeue/drop */
#define LLQ_DEBUG   0   /* show class structure after change */
/* -------------------------------------------------------------------- */

#define LLQ_MAX_NPRIO	        32  /* maximal number of different priorities */
#define LLQ_MAX_CLASSIDMINOR	32  /* maximal minor of a class */

/* -------------------------------------------------------------------- */

struct llq_sched_data;
struct llq_class_head;

struct llq_class {
	struct llq_class                 *next;  /* list of all classes */
	struct Qdisc                     *qdisc; /* LLQ Qdisc (back link) */
	struct llq_class_head            *head;  /* list of same prio classes */

	u32                               classid;

	struct tc_llq_copt params;

	struct gnet_stats_basic_packed    bstats; /* enqueued bytes */
	struct gnet_stats_queue           qstats;
	struct gnet_stats_basic_packed    tbstats; /* transmitted bytes */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	struct gnet_stats_rate_est64        rate_est;
#else
	struct net_rate_estimator  __rcu   *rate_est;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	int                               refcnt;
#endif

	/*
	 * queue for this class, defaults to fifo
	 */
	struct Qdisc           *q;

	/*
	 * values calculate using all weigths of classes with same priority
	 */
	long                    quantum; /* max bytes transmited per round */
	long                    deficit;

	struct llq_class       *sibling; /* classes with same priority */
	struct llq_class       *prevsibling;
};

struct llq_class_head {
	struct llq_class_head *next;
	struct llq_class_head *prev;
	struct llq_sched_data *scheddata;
	unsigned char          priority;  /* priority for the classes */
	unsigned char          offset;
	__u32                  qlen;      /* # packets queued for this prio */
	struct llq_class      *classes;   /* list of classes (siblings) */
	unsigned char          nclasses;  /* # of classes in list */
};


/*
 * qdisc
 */

struct llq_sched_data {
	struct llq_class      *classes; /* all classes defined  (next) */
	struct tcf_proto      *filter_list;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	struct tcf_block      *block;
#endif
	int                    nfilter;

	struct llq_class_head *priofirst;  /* next */
	struct llq_class_head *priolast;   /* prev */

	int                    nprio; /* number of different priorities */
	struct llq_class_head *prioarray[LLQ_MAX_NPRIO];
	struct llq_class      *minor2class[LLQ_MAX_CLASSIDMINOR];

	struct tc_llq_qopt     params;
};

/* -------------------------------------------------------------------- */
#if LLQ_TRACE
static void llq_trace(const char *fmt, ...)
{
	va_list args;

	printk(KERN_DEBUG "");
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
	printk("\n");
}
#else
#define llq_trace(s, ...)	/* */
#endif

#if LLQ_QTRACE
static void llq_qtrace(const char *fmt, ...)
{
	va_list args;

	printk(KERN_DEBUG "");
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);
	printk("\n");
}
#else
#define llq_qtrace(s, ...)	/* */
#endif

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static unsigned int llq_drop(struct Qdisc *sch, struct sk_buff **to_free);
#else
static unsigned int llq_drop(struct Qdisc *sch);
#endif
static void llq_class_unlink(struct Qdisc *sch, struct llq_class *cl);

/* -------------------------------------------------------------------- */

static inline struct llq_class *llq_find(u32 handle, struct Qdisc *sch)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	unsigned int idx = TC_H_MIN(handle);

	return (idx >= LLQ_MAX_CLASSIDMINOR) ? 0 : llq->minor2class[idx];
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static unsigned long llq_find_cl(struct Qdisc *sch, u32 classid)
{
	return (unsigned long) llq_find(classid, sch);
}
#endif

static struct llq_class *llq_classify(struct sk_buff *skb, struct Qdisc *sch)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class *cl;
	struct tcf_result res;
	u32 classid;

	classid = skb->priority;

	/*
	 * If skb->priority points to one of our classes, use it.
	 */
	cl = llq_find(classid, sch);
	if (TC_H_MAJ(classid) == sch->handle && cl != 0)
		return cl;

	/*
	 * check classifiers
	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	if (llq->filter_list &&
	    tc_classify(skb, llq->filter_list, &res, false) == 0) {
#else
	if (llq->filter_list &&
	    tcf_classify(skb, llq->filter_list, &res, false) == 0) {
#endif
		cl = llq_find(res.classid, sch);
		if (cl != 0)
			return cl;
	}

	/*
	 * get default class
	 */
	cl = llq_find(llq->params.defaultclass, sch);
	if (cl != 0)
		return cl;
	return llq->classes;
}

/* -------------------------------------------------------------------- */

#if LLQ_DEBUG
static void show_all(struct Qdisc *sch)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *head;
	struct llq_class *cl;
	int count = 0;

	printk(KERN_INFO "llq: show first to last\n");

	for (head = llq->priofirst; head; head = head->next) {
		count++;
		printk(KERN_INFO "llq(%d): prio %u, %d classes\n",
			count, (unsigned int)head->priority, head->nclasses);

		cl = head->classes;
		if (head->nclasses == 1) {
			printk(KERN_INFO "llq(%d): prio %u, prio %u\n",
			       count, (unsigned int)head->priority,
			       (unsigned int)cl->params.priority);
			continue;
		}

		do {
			printk(KERN_INFO "llq(%d): prio %u, prio %u, weight %u, quantum %ld\n",
			       count, (unsigned int)head->priority,
			       (unsigned int)cl->params.priority,
			       (unsigned int)cl->params.weight, cl->quantum);
		} while ((cl = cl->sibling) != head->classes);
	}

	printk(KERN_INFO "llq: show last to first\n");

	for (head = llq->priolast; head; head = head->prev) {
		printk(KERN_INFO "llq(%d): prio %u, %d classes\n",
		       count, (unsigned int)head->priority, head->nclasses);

		cl = head->classes;
		if (head->nclasses == 1) {
			printk(KERN_INFO "llq(%d): prio %u, prio %u\n",
				   count, (unsigned int)head->priority,
				   (unsigned int)cl->params.priority);
			count--;
			continue;
		}

		do {
			printk(KERN_INFO "llq(%d): prio %u, prio %u, weight %u, quantum %ld\n",
			       count, (unsigned int)head->priority,
			       (unsigned int)cl->params.priority,
			       (unsigned int)cl->params.weight, cl->quantum);
		} while ((cl = cl->sibling) != head->classes);
		count--;
	}
}
#endif

/* -------------------------------------------------------------------- */

static inline void llq_activate_head(struct llq_class_head *head)
{
	head->scheddata->prioarray[head->offset] = head;
}

static inline void llq_deactivate_head(struct llq_class_head *head)
{
	head->scheddata->prioarray[head->offset] = 0;
}

static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
llq_enqueue(struct sk_buff *skb, struct Qdisc *sch, struct sk_buff **to_free)
#else
llq_enqueue(struct sk_buff *skb, struct Qdisc *sch)
#endif
{
	struct llq_class *cl;

	llq_qtrace("llq_enqueue: start");

	if (sch->q.qlen > qdisc_dev(sch)->tx_queue_len)
		goto drop;

	cl = llq_classify(skb, sch);
	if (!cl)
		goto drop;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	if (cl->q->enqueue(skb, cl->q, to_free) == NET_XMIT_SUCCESS) {
#else
	if (cl->q->enqueue(skb, cl->q) == NET_XMIT_SUCCESS) {
#endif
		struct llq_class_head *head;

		sch->bstats.packets++;
		sch->bstats.bytes += skb->len;
		cl->bstats.packets++;
		cl->bstats.bytes += skb->len;
		sch->q.qlen++;
		head = cl->head;
		if (head->qlen++ == 0)
			llq_activate_head(head);
		if (sch->q.qlen <= qdisc_dev(sch)->tx_queue_len) {
			llq_qtrace("llq_enqueue: done");
			return NET_XMIT_SUCCESS;
		}
		llq_qtrace("llq_enqueue: overlimit");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		return llq_drop(sch, to_free) ? NET_XMIT_DROP : NET_XMIT_CN;
#else
		return llq_drop(sch) ? NET_XMIT_DROP : NET_XMIT_CN;
#endif
	}
	/*
	 * we count overlimit if low level queue drops a paket
	 */
	cl->qstats.overlimits++;
	sch->qstats.overlimits++;
	llq_qtrace("llq_enqueue: dropped (enqueue)");
	return NET_XMIT_DROP;

drop:
	sch->qstats.drops++;
	kfree_skb(skb);
	llq_qtrace("llq_enqueue: dropped (no class)");
	return NET_XMIT_DROP;
}

static struct sk_buff *llq_dequeue(struct Qdisc *sch)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *head;
	struct llq_class *cl;
	struct sk_buff *skb = 0;
	int i, packets_there;

	llq_qtrace("llq_dequeue: start");

	for (i = 0; i < llq->nprio; i++) {
		head = llq->prioarray[i];
		if (head == 0)
			continue;
		if (head->nclasses == 1) {
			cl = head->classes;
			skb = cl->q->dequeue(cl->q);
			if (skb == 0)
				continue; /* check next priority */
			cl->tbstats.packets++;
			cl->tbstats.bytes += skb->len;
			sch->q.qlen--;
				if (--head->qlen == 0)
					llq_deactivate_head(head);
			llq_qtrace("llq_dequeue: done");
			return skb;
		}

		cl = head->classes;

		do {
			/*
			 * round starts
			 */
			packets_there = 0;
			do {
				if (cl->deficit <= 0)  {
					cl->deficit += cl->quantum;
					if (cl->q->q.qlen) {
						/*
						 * perhaps we get packet in a later round
						 */
						packets_there = 1;
					}
					continue;
				}
				if (cl->q->q.qlen == 0)
					continue;
				skb = cl->q->dequeue(cl->q);
				if (skb == 0)
					continue;
				cl->deficit -= skb->len;
				head->classes = (cl->deficit <= 0) ? cl->sibling : cl;
				cl->tbstats.packets++;
				cl->tbstats.bytes += skb->len;
				sch->q.qlen--;
					if (--head->qlen == 0)
						llq_deactivate_head(head);
				llq_qtrace("llq_dequeue: done");
				return skb;

			} while ((cl = cl->sibling) != head->classes);
			/*
			 * round end, got no paket, but there are pakets
			 */
			if (packets_there) {
				int factor = 0;
				/*
				 * warp deficit forward
				 */
				do {
					int clfactor;

					if (cl->deficit > 0) {
						if (cl->q->q.qlen) {
							factor = 0;
							cl = head->classes;
							break;
						}
						continue;
					}
					clfactor = (-cl->deficit+cl->quantum)/cl->quantum;
					if (factor == 0 || factor > clfactor)
						factor = clfactor;
				} while ((cl = cl->sibling) != head->classes);

				if (factor) {
					do {
						if (cl->deficit <= 0)
							cl->deficit += factor*cl->quantum;
					} while ((cl = cl->sibling) != head->classes);
				}
			}
		} while (packets_there);
	}

	llq_qtrace("llq_dequeue: failed");
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static unsigned int llq_drop(struct Qdisc *sch, struct sk_buff **to_free)
#else
static unsigned int llq_drop(struct Qdisc *sch)
#endif
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *head;
	struct llq_class *cl, *search;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	struct sk_buff *skb;
#else
	unsigned int len;
#endif
	int i;

	llq_qtrace("llq_drop: start");

	for (i = llq->nprio-1; i >= 0; i--) {
		head = llq->prioarray[i];
		if (head == 0)
			continue;
		cl = head->classes;
		if (head->nclasses == 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
			skb = cl->q->ops->dequeue(cl->q);
			if (skb != NULL) {
				__qdisc_drop(skb, to_free);
#else
			if (cl->q->q.qlen <= 1 || cl->q->ops->drop == 0)
				continue;
			len = cl->q->ops->drop(cl->q);
			if (len != 0) {
#endif
				cl->qstats.drops++;
				sch->qstats.drops++;
				sch->q.qlen--;
				if (--head->qlen == 0)
					llq_deactivate_head(head);
				llq_qtrace("llq_drop: done (prio %u)",
				     (unsigned int)cl->params.priority);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
				return skb->len;
#else
				return len;
#endif
			}
			continue;
		}

		cl = 0;
		search = head->classes;
		do {
			if (search->q->q.qlen
			    && (cl == 0 || search->q->q.qlen > cl->q->q.qlen))
				cl = search;
		} while ((search = search->sibling) != head->classes);

		if (cl == 0)
			cl = head->classes; /* should not happen */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		skb = cl->q->ops->dequeue(cl->q);
		if (skb != NULL) {
			__qdisc_drop(skb, to_free);
#else
		if (cl->q->q.qlen <= 1 || cl->q->ops->drop == 0)
			continue;
		len = cl->q->ops->drop(cl->q);
		if (len != 0) {
#endif
			cl->qstats.drops++;
			sch->qstats.drops++;
			sch->q.qlen--;
			if (--head->qlen == 0)
				llq_deactivate_head(head);
			llq_qtrace("llq_drop: done (prio %u, weight %u)",
			     (unsigned int)cl->params.priority,
			     (unsigned int)cl->params.weight);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
			return skb->len;
#else
			return len;
#endif
		}
	}

	llq_qtrace("llq_drop: nothing to drop");
	return 0;
}

/* -------------------------------------------------------------------- */

static void llq_reset(struct Qdisc *sch)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *head;
	struct llq_class *cl;

	llq_trace("llq_reset: start");

	for (head = llq->priofirst; head; head = head->next) {
		cl = head->classes;
		head->qlen = 0;
		if (head->nclasses == 1) {
			llq_trace("llq_reset: prio %u",
			          (unsigned int)cl->params.priority);
			if (cl->q)
				qdisc_reset(cl->q);
			continue;
		}
		do {
			llq_trace("llq_reset: prio %u, weight %u",
			          (unsigned int)cl->params.priority,
			          (unsigned int)cl->params.weight);
			if (cl->q)
				qdisc_reset(cl->q);
			cl->deficit = cl->quantum;
		} while ((cl = cl->sibling) != head->classes);
	}

	sch->q.qlen = 0;

	llq_trace("llq_reset: done");
}

static const struct nla_policy llq_policy[TCA_LLQ_MAX + 1] = {
	[TCA_LLQ_OPTIONS] = { .len = sizeof(struct tc_llq_copt) },
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
static int llq_init(struct Qdisc *sch, struct nlattr *opt)
#else
static int llq_init(struct Qdisc *sch, struct nlattr *opt,
					struct netlink_ext_ack *extack)
#endif
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct tc_llq_qopt *qopt = nla_data(opt);
	int __maybe_unused err;

	llq_trace("llq_init: start");

	if (opt == 0 || nla_len(opt) < sizeof(*qopt)) {
		llq_trace("llq_init: failed");
		return -EINVAL;
	}
	llq->params = *qopt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
	err = tcf_block_get(&llq->block, &llq->filter_list, sch, extack);
	if (err) {
		llq_trace("llq_init: failed tcf_block_get");
		return err;
	}
#endif

	llq_trace("llq_init: done");
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
static int llq_change(struct Qdisc *sch, struct nlattr *opt)
#else
static int llq_change(struct Qdisc *sch, struct nlattr *opt,
					struct netlink_ext_ack *extack)
#endif
{
	struct llq_sched_data *llq = qdisc_priv(sch);
	struct tc_llq_qopt *ctl = nla_data(opt);
	u32 classid;

	if (opt == 0 || nla_len(opt) < sizeof(*ctl)) {
		llq_trace("llq_change: failed (opt/len)");
		return -EINVAL;
	}

	if (llq->params.minq == 0)
		llq->params.minq = 10;
	if (llq->params.maxq == 0)
		llq->params.maxq = psched_mtu(qdisc_dev(sch));

	if (llq->params.maxq < 100 || llq->params.minq < 1) {
		llq_trace("llq_change: failed (maxq/minq)");
		return -EINVAL;
	}
	if (llq->params.maxq > 4*psched_mtu(qdisc_dev(sch))) {
		llq_trace("llq_change: failed (maxq)");
		return -EINVAL;
	}

	classid = TC_H_MAJ(sch->handle) | TC_H_MIN(ctl->defaultclass);

	if (ctl->maxq != llq->params.maxq || ctl->minq != llq->params.minq) {
		sch_tree_lock(sch);
		ctl->maxq = llq->params.maxq;
		ctl->minq = llq->params.minq;
		sch_tree_unlock(sch);
	}
	llq->params.defaultclass = classid;

	llq_trace("llq_change: done");
	return 0;
}

static int llq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);

	nla_put(skb, TCA_OPTIONS, sizeof(llq->params), &llq->params);
	return skb->len;
}

/* -------------------------------------------------------------------- */

static int
llq_dump_class(struct Qdisc *sch, unsigned long arg,
	       struct sk_buff *skb, struct tcmsg *tcm)
{
	struct llq_class *cl = (struct llq_class *)arg;
	struct tc_llq_cinfo cinfo;

	tcm->tcm_parent = sch->handle;
	tcm->tcm_handle = cl->classid;
	tcm->tcm_info = cl->q->handle;

	cinfo.priority = cl->params.priority;
	cinfo.weight = cl->params.weight;
	cinfo.deficit = cl->deficit;
	cinfo.quantum = cl->quantum;
	nla_put(skb, TCA_OPTIONS, sizeof(cinfo), &cinfo);
	return skb->len;
}

static int
llq_dump_class_stats(struct Qdisc *sch, unsigned long arg,
	             struct gnet_dump *d)
{
	struct llq_class *cl = (struct llq_class *)arg;

	cl->qstats.qlen = cl->q->q.qlen;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	if (gnet_stats_copy_basic(qdisc_root_sleeping_running(sch), d,
				  NULL, &cl->bstats) < 0 ||
	    gnet_stats_copy_rate_est(d, &cl->rate_est) < 0 ||
	    gnet_stats_copy_queue(d, NULL, &cl->qstats, cl->qstats.qlen) < 0)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
	if (gnet_stats_copy_basic(qdisc_root_sleeping_running(sch), d,
				  NULL, &cl->bstats) < 0 ||
	    gnet_stats_copy_rate_est(d, &cl->bstats, &cl->rate_est) < 0 ||
	    gnet_stats_copy_queue(d, NULL, &cl->qstats, cl->qstats.qlen) < 0)
#else
	if (gnet_stats_copy_basic(d, NULL, &cl->bstats) < 0 ||
	    gnet_stats_copy_rate_est(d, &cl->bstats, &cl->rate_est) < 0 ||
	    gnet_stats_copy_queue(d, NULL, &cl->qstats, cl->qstats.qlen) < 0)
#endif
		return -1;

	return gnet_stats_copy_app(d, &cl->tbstats, sizeof(cl->tbstats));
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
static int llq_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
#else
static int llq_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old, struct netlink_ext_ack *extack)
#endif
{
	struct llq_class *cl = (struct llq_class *)arg;

	llq_trace("llq_graft: start");
	llq_trace("llq_graft: class %sset", cl ? "" : "NOT ");
	llq_trace("llq_graft: new %sset", new ? "" : "NOT ");

	if (!cl)
		return -ENOENT;

	llq_trace("llq_graft: prio %u, weight %u",
		  (unsigned int)cl->params.priority,
		  (unsigned int)cl->params.weight);
	if (new == NULL) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
		new = qdisc_create_dflt(sch->dev_queue, &pfifo_qdisc_ops,
				        cl->classid);
#else
		new = qdisc_create_dflt(sch->dev_queue, &pfifo_qdisc_ops,
				        cl->classid, extack);
#endif
		if (new == NULL)
			return -ENOBUFS;
	}
	*old = xchg(&cl->q, new);
	if (*old != NULL)
		qdisc_reset(*old);

	llq_trace("llq_graft: done");
	return 0;
}

static struct Qdisc *llq_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct llq_class *cl = (struct llq_class *)arg;

	llq_trace("llq_leaf: class %sset", cl ? "" : "NOT ");

	return cl ? cl->q : NULL;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static unsigned long llq_get(struct Qdisc *sch, u32 classid)
{
	struct llq_class *cl = llq_find(classid, sch);

	if (cl) {
		cl->refcnt++;
		llq_trace("llq_get: refcnt %d (prio %u, weight %u)", cl->refcnt,
		           (unsigned int)cl->params.priority,
		           (unsigned int)cl->params.weight);
	} else {
		llq_trace("llq_get: not found");
	}

	return (unsigned long)cl;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
static void llq_destroy_filters(struct llq_sched_data *llq)
{
	struct tcf_proto *tp;

	while ((tp = llq->filter_list) != NULL) {
		llq->filter_list = tp->next;
		tp->ops->destroy(tp, true);
	}
}
#endif

static void llq_destroy_class(struct llq_class *cl)
{
	llq_trace("llq_destroy_class: start");

	qdisc_destroy(cl->q);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	gen_kill_estimator(&cl->tbstats, &cl->rate_est);
#else
	gen_kill_estimator(&cl->rate_est);
#endif
	kfree(cl);

	llq_trace("llq_destroy_class: end");
}

static void
llq_destroy(struct Qdisc *sch)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *priofirst, *head;
	struct llq_class *classes, *cl;

	llq_trace("llq_destroy: start");

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	llq_destroy_filters(llq);
#else
	tcf_block_put(llq->block);
#endif

	sch_tree_lock(sch);
	priofirst = llq->priofirst;
	llq->priofirst = llq->priolast = 0;
	classes = llq->classes;
	llq->classes = 0;
	memset(&llq->prioarray, 0, sizeof(llq->prioarray));
	sch_tree_unlock(sch);

	while ((head = priofirst) != 0) {
		priofirst = head->next;
		kfree(head);
	}

	while ((cl = classes) != 0) {
		classes = cl->next;
		llq_destroy_class(cl);
	}

	llq_trace("llq_destroy: done");
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static void llq_put(struct Qdisc *sch, unsigned long arg)
{
	struct llq_class *cl = (struct llq_class *)arg;

	llq_trace("llq_put: refcnt %d (prio %u, weight %u)", cl->refcnt,
	          (unsigned int)cl->params.priority,
	          (unsigned int)cl->params.weight);
	if (--cl->refcnt == 0)
		llq_destroy_class(cl);
}
#endif

/* -------------------------------------------------------------------- */

static void llq_recalc_siblings(struct Qdisc *sch, struct llq_class_head *head)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class *cl;
	unsigned char maxweight = 0;
	unsigned char t, ggt = 1;

	llq_trace("llq_recalc_siblings: start");

	cl = head->classes;
	do {
		if (cl->params.weight > maxweight)
			maxweight = cl->params.weight;
	} while ((cl = cl->sibling) != head->classes);

	for (t = 2; t < maxweight/2; t++) {
		int failed = 0;

		cl = head->classes;
		do {
			if ((cl->params.weight%t) != 0) {
				failed = 1;
				break;
			}
		} while ((cl = cl->sibling) != head->classes);
		if (!failed)
			ggt = t;
	}

	llq_trace("llq_recalc_siblings: max %u ggt %u (minq %u, maxq %u)",
	          (unsigned int)maxweight, (unsigned int)ggt,
	          llq->params.minq, llq->params.maxq);

	cl = head->classes;
	do {
		cl->quantum = cl->params.weight/ggt;
		if (cl->quantum < llq->params.minq)
			cl->quantum = llq->params.minq;
		if (cl->quantum > llq->params.maxq)
			cl->quantum = llq->params.maxq;
		llq_trace("llq_recalc_siblings: weight %u, quantum %ld",
		          (unsigned int)cl->params.weight, cl->quantum);
	} while ((cl = cl->sibling) != head->classes);

	llq_trace("llq_recalc_siblings: done");
}

static void llq_recalc_offsets(struct llq_sched_data *llq)
{
	struct llq_class_head *head;
	int offset = 0;

	llq_trace("llq_recalc_offsets: start");

	memset(&llq->prioarray, 0, sizeof(llq->prioarray));
	for (head = llq->priofirst; head; head = head->next) {
		head->offset = offset++;
		if (head->qlen)
			llq_activate_head(head);
	}
	llq->nprio = offset;

	llq_trace("llq_recalc_offsets: done (%d)", llq->nprio);
}

static int llq_class_add(struct Qdisc *sch, struct llq_class *cl)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *head;

	llq_trace("llq_class_add: start (prio %u)",
		  (unsigned int)cl->params.priority);

	if (TC_H_MIN(cl->classid) >= LLQ_MAX_CLASSIDMINOR) {
		printk(KERN_ERR "llq: classid minor out of range\n");
		return -ERANGE;
	}

	for (head = llq->priofirst;
	     head && head->priority < cl->params.priority;
	     head = head->next)
		;

	if (!head || head->priority != cl->params.priority) {
		struct llq_class_head *nhead;

		llq_trace("llq_class_add: new head needed");
		if (llq->nprio == LLQ_MAX_NPRIO) {
			printk(KERN_ERR "llq: too much different priorities\n");
			return -ENOBUFS;
		}
		nhead = kmalloc(sizeof(*nhead), GFP_ATOMIC);
		if (nhead == 0)
			return -ENOMEM;

		memset(nhead, 0, sizeof(*nhead));
		nhead->priority = cl->params.priority;
		nhead->scheddata = llq;
		if (head) {
			nhead->prev = head->prev;
			if (nhead->prev != 0)
				nhead->prev->next = nhead;
			else
				llq->priofirst = nhead;
			nhead->next = head;
			head->prev = nhead;
		} else {
			nhead->next = 0;
			nhead->prev = llq->priolast;
			if (nhead->prev != 0)
				nhead->prev->next = nhead;
			else
				llq->priofirst = nhead;
			llq->priolast = nhead;
		}
		head = nhead;
		llq_recalc_offsets(llq);
		llq_trace("llq_class_add: new head added");
	} else {
		llq_trace("llq_class_add: use existing head");
	}

	llq->minor2class[TC_H_MIN(cl->classid)] = cl;
	if (head->classes == 0) {
		cl->sibling = cl->prevsibling = head->classes = cl;
	} else {
		cl->sibling = head->classes;
		cl->prevsibling = head->classes->prevsibling;
		head->classes->prevsibling = cl;
		cl->prevsibling->sibling = cl;
	}
	head->nclasses++;
	cl->head = head;
	llq_recalc_siblings(sch, head);
	cl->deficit = cl->quantum;

	llq_trace("llq_class_add: done");
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
static int
llq_change_class(struct Qdisc *sch, u32 classid, u32 parentid,
		 struct nlattr **tca, unsigned long *arg)
#else
static int
llq_change_class(struct Qdisc *sch, u32 classid, u32 parentid,
		 struct nlattr **tca, unsigned long *arg, struct netlink_ext_ack *extack)
#endif
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class *cl = (struct llq_class *)*arg;
	struct nlattr *opt = tca[TCA_OPTIONS];
	struct nlattr *tb[TCA_LLQ_MAX+1];
	struct tc_llq_copt *params;
	int err = -EINVAL;

	llq_trace("llq_change_class: start");

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
	if (opt == NULL || nla_parse_nested(tb, TCA_LLQ_MAX, opt, llq_policy))
#else
	if (opt == NULL || nla_parse_nested(tb, TCA_LLQ_MAX, opt, llq_policy, extack))
#endif
		goto failure;

	if (!tb[TCA_LLQ_OPTIONS])
		goto failure;
	params = nla_data(tb[TCA_LLQ_OPTIONS]);

	if (cl == 0) {
		struct llq_class **pp, *p;

		llq_trace("llq_change_class: new class (prio %u, weight %u)",
		          (unsigned int)params->priority,
			  (unsigned int)params->weight);
		err = -ENOMEM;
		cl = kmalloc(sizeof(*cl), GFP_ATOMIC);
		if (cl == 0)
			goto failure;
		memset(cl, 0, sizeof(*cl));
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		cl->refcnt = 1;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
		cl->q = qdisc_create_dflt(sch->dev_queue, &pfifo_qdisc_ops,
					  classid);
#else
		cl->q = qdisc_create_dflt(sch->dev_queue, &pfifo_qdisc_ops,
					  classid, extack);
#endif
		if (!cl->q) {
			printk(KERN_ERR "llq: create pfifo qdisc failed, using noop\n");
			cl->q = &noop_qdisc;
		}
		cl->classid = classid;
		cl->qdisc = sch;
		cl->params = *params;
		sch_tree_lock(sch);
		err = llq_class_add(sch, cl);
		if (err < 0) {
			kfree(cl);
			llq_trace("llq_change_class: failed (llq_class_add)");
			goto failure;
		}
		*arg = (unsigned long)cl;
		for (pp = &llq->classes; (p = *pp) != 0; pp = &(*pp)->next) {
			if (p->params.priority >= cl->params.priority) {
				if (p->params.priority > cl->params.priority)
					break;
				if (p->params.weight > cl->params.weight)
				break;
			}
		}
		cl->next = *pp;
		*pp = cl;
		if (tca[TCA_RATE])
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
			gen_replace_estimator(&cl->tbstats, NULL, &cl->rate_est,
			                      qdisc_root_sleeping_lock(sch),
			                      qdisc_root_sleeping_running(sch),
			                      tca[TCA_RATE]);
#else
			gen_replace_estimator(&cl->tbstats, NULL, &cl->rate_est,
			                      qdisc_root_sleeping_lock(sch),
			                      tca[TCA_RATE]);
#endif
		sch_tree_unlock(sch);
		llq_trace("llq_change_class: done");
#if LLQ_DEBUG
		show_all(sch);
#endif
		return 0;
	}

	if (tca[TCA_RATE])
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
		gen_new_estimator(&cl->tbstats, NULL, &cl->rate_est,
		                  qdisc_root_sleeping_lock(sch),
		                  qdisc_root_sleeping_running(sch),
		                  tca[TCA_RATE]);
#else
		gen_new_estimator(&cl->tbstats, NULL, &cl->rate_est,
		                  qdisc_root_sleeping_lock(sch),
		                  tca[TCA_RATE]);
#endif

	if (cl->params.priority != params->priority) {
		llq_trace("llq_change_class: prio %u changed to %u",
		          (unsigned int)cl->params.priority,
		          (unsigned int)params->priority);
		sch_tree_lock(sch);
		llq_class_unlink(sch, cl);
		err = llq_class_add(sch, cl);
		sch_tree_unlock(sch);
		if (err < 0)
			goto failure;
		llq_trace("llq_change_class: done (prio changed)");
	} else if (cl->params.weight != params->weight) {
		llq_trace("llq_change_class: weight %u changed to %u",
		          (unsigned int)cl->params.weight,
		          (unsigned int)params->weight);
		llq_recalc_siblings(sch, cl->head);
		llq_trace("llq_change_class: done (weight changed)");
	}
	return 0;

failure:
	llq_trace("llq_change_class: failed");
	return err;
}

static void llq_class_unlink(struct Qdisc *sch, struct llq_class *cl)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class_head *head;
	struct llq_class **pp;

	llq_trace("llq_remove: start");

	llq->minor2class[TC_H_MIN(cl->classid)] = 0;
	cl->sibling->prevsibling = cl->prevsibling;
	cl->prevsibling->sibling = cl->sibling;
	head = cl->head;
	cl->head = 0;
	if (--head->nclasses) {
		if (head->classes == cl)
			head->classes = cl->sibling;
	} else {
		head->classes = 0;
		if (head->prev)
			head->prev->next = head->next;
		else
			llq->priofirst = head->next;
		if (head->next)
			head->next->prev = head->prev;
		else
			llq->priolast = head->prev;
		kfree(head);
		llq_recalc_offsets(llq);
	}
	for (pp = &llq->classes; *pp && *pp != cl; pp = &(*pp)->next)
		;
	if (*pp == cl)
		*pp = cl->next;

	llq_trace("llq_remove: end");
}

static int llq_delete(struct Qdisc *sch, unsigned long arg)
{
	struct llq_class *cl = (struct llq_class *)arg;

	sch_tree_lock(sch);
	llq_class_unlink(sch, cl);
	sch_tree_unlock(sch);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	if (--cl->refcnt == 0)
#endif
		llq_destroy_class(cl);
	return 0;
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
static struct tcf_proto **llq_find_tcf(struct Qdisc *sch, unsigned long arg)
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
static struct tcf_block *llq_tcf_block(struct Qdisc *sch, unsigned long arg,
				       struct netlink_ext_ack *extack)
#endif
#endif
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	return arg ? NULL : &llq->filter_list;
#else
	return arg ? NULL : llq->block;
#endif
}

static unsigned long llq_bind_filter(struct Qdisc *sch, unsigned long parent,
				     u32 classid)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);

	llq->nfilter++;
	return 0;
}

static void llq_unbind_filter(struct Qdisc *sch, unsigned long arg)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);

	llq->nfilter--;
}

static void llq_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct llq_sched_data *llq = (struct llq_sched_data *)qdisc_priv(sch);
	struct llq_class *cl;

	if (arg->stop)
		return;

	for (cl = llq->classes; cl; cl = cl->next) {
		if (arg->count < arg->skip) {
			arg->count++;
			continue;
		}
		if (arg->fn(sch, (unsigned long)cl, arg) < 0) {
			arg->stop = 1;
			return;
		}
		arg->count++;
	}
}

/* -------------------------------------------------------------------- */

static struct Qdisc_class_ops llq_class_ops = {
	.graft      =   llq_graft,
	.leaf       =   llq_leaf,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	.get        =   llq_get,
	.put        =   llq_put,
#else
	.find       =	llq_find_cl,
#endif
	.change     =   llq_change_class,
	.delete     =   llq_delete,
	.walk       =   llq_walk,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
	.tcf_chain  =   llq_find_tcf,
#else
	.tcf_block  =   llq_tcf_block,
#endif
	.bind_tcf   =   llq_bind_filter,
	.unbind_tcf =   llq_unbind_filter,
	.dump       =   llq_dump_class,
	.dump_stats =   llq_dump_class_stats,
};

struct Qdisc_ops llq_qdisc_ops = {
	.next       = NULL,
	.cl_ops     = &llq_class_ops,
	.id         =   "llq",
	.priv_size  = sizeof(struct llq_sched_data),
	.enqueue    = llq_enqueue,
	.dequeue    = llq_dequeue,
	.peek       = qdisc_peek_dequeued,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
	.drop       = llq_drop,
#endif
	.init       = llq_init,
	.reset      = llq_reset,
	.destroy    = llq_destroy,
	.change     = llq_change,
	.dump       = llq_dump,
	.dump_stats = 0, /* llq_dump_stats, */
	.owner      = THIS_MODULE,
};

/* -------------------------------------------------------------------- */

static int __init llq_module_init(void)
{
	/*--- printk(KERN_INFO "sch_llq: %s %s\n", __DATE__, __TIME__); ---*/
	return register_qdisc(&llq_qdisc_ops);
}
static void __exit llq_module_exit(void)
{
	unregister_qdisc(&llq_qdisc_ops);
}
module_init(llq_module_init)
module_exit(llq_module_exit)
MODULE_LICENSE("GPL");
