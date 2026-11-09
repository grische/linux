/*
 * Packet Accelerator Interface
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=80:
 *
 * Copyright (c) 2011-2017 AVM GmbH <info@avm.de>
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

#include <linux/kconfig.h>

#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/list.h>

#include "avm_pa.h"
#include "avm_pa_hw.h"
#include "avm_pa_intern.h"

/**
 * avm_pa session selector framework
 *
 * This extensible framework allows to specify session selectors via procfs.
 * The user-facing side of the selectors is modelled after tcpdump with numerous
 * limitations (the user space tool showpainfo should nullify some of them).
 *
 * The purpose is to filter the output of /proc/net/avm_pa/sessions. However, it
 * is designed such that it can be repurposed to filter sessions at the creation
 * time already later on.
 *
 * For example a user can do the following:
 * echo ip > /proc/net/avm_pa/xsession
 * echo port 80 > /proc/net/avm_pa/xsession
 * echo ip6 dst port 25  > /proc/net/avm_pa/xsession
 * echo not vpid internet > /proc/net/avm_pa/xsession
 *
 * /proc/net/avm_pa/sessions will show only sessions matching the criterea.
 * Other sessions are hidden, but still exist (and packets of hidden sessions
 * are still accelerated).
 *
 * The current set of selectors can be read with:
 * $ cat /proc/net/avm_pa/xsession
 *
 * The kernel side of the framework is described below to help future
 * expansions.
 *
 * all_selectors has a preprogrammed array of available selectors. Each element
 * is of the type struct pa_selector.
 *
 * Each pa_selector defines the following aspects:
 * - a prefix: a selector may support/require a prefix that must be echoed
 *     before the actual type (e.g. echo src pid > [...]). The meaning is
 *     selector-defined but is conventionally used to express a direction, for
 *     example src/dst or ingress/egress.
 * - the label: the label is the string that identifies the selector.
 *     However, a label might occur multiple times within the all_selectors
 *     array, only the tuple of prefix and label must be unique. This is so that
 *     a selector can support more than one prefix or none.
 * - pointer to a struct pa_selector_ops
 *     struct pa_selector_ops defines callbacks that are called
 *     for setting up, checking and showing a selector
 *     * setup() is called during parsing of the xsession input. it'll
 *       receive the input as argument vector, plus the prefix. input that has
 *       been consumed by earlier selectors is not passed.
 *     * check() is called during the output of sessions. it'll receive the
 *       session and should simply return a boolean whether that session is
 *       selected or not. for example a "port 80" selector should return 1
 *       for any session that has port 80 (either source or dest).
 *     * show() is called during the output of xsession. it'll receive a buffer
 *       and should print itself into that buffer. This is so that the user can
 *       view current set of selectors with "cat /proc/net/avm_pa/xsession".
 *       Usually the user input should be mirrored.
 *     For simple selectors, you may implement only the check() callback.
 *       setup() and show() can be NULL, see below.
 *
 * Each pa_selector defines also its data structure (struct *_selector_data)
 * that becomes part of the selector_union union. The data is accssible through
 * the pa_selector_entry that is passed to each method. Generally, setup() sets
 * up the data structure based on the user input, and check() does the
 * selection based on the information in the data structure.
 *
 * The active selectors are stored in a linux doubly-linked list.
 * session_is_selected() goes through that list and calls the check() method of
 * all selectors. Currently, the selectors are logically connected with AND, so
 * for a session to be shown it must be selected by ALL selectors. Logical OR
 * is not implemented yet. However, NOT is implemented. Each selector can be
 * negated by prefixing it with NOT (or prefxing the prefix :-). The NOT is
 * only applied to next selector, any earlier or following ones are unaffected.
 * Therefore:
 *   echo not ip port 80 > [...]
 * is probably *not* doing what you expect. In truth it applies NOT *only* to
 * the ip selector, meaning that only ipv6 packets are selected. These packets
 * must also be match the "port 80" criteria. Check the output of xsession to
 * understand it better.
 *
 * To add new parametrized selectors do the following (named foo as an example):
 * 1) define a data structure foo_selector_data
 * 2) add struct foo_selector_data to the union selector_union
 * 3) implement setup(), check() and show() methods.
 * 4) add one or more entries to all_selectors, depending on the prefixes it
 *    supports (or requires)
 *
 * For unparametrized selectors, that usually consist of only a single label
 * and no prefix, do:
 * 1) implement check() method
 * 2) add one entry to all_selectors, setup() and show() can be NULL.
 *    The label will be echoed back when reading xsession.
 */

#define DIR_INGRESS 0x1
#define DIR_EGRESS  0x2
#define DIR_ANY   (DIR_INGRESS | DIR_EGRESS)

static int prefix_to_dir(const char *prefix)
{
	if (!prefix)
		return DIR_ANY;
	else if (!strcmp(prefix, "dst") || !strcmp(prefix, "egress"))
		return DIR_EGRESS;
	else if (!strcmp(prefix, "src") || !strcmp(prefix, "ingress"))
		return DIR_INGRESS;
	else
		return DIR_ANY;
}


struct port_selector_data {
	int               dir;
	__be16            port;
};


struct pid_selector_data {
	int               dir;
	u32               pid; /* can be either pid or vpid */
};

struct group_selector_data {
	unsigned short    groupid;
};

struct host_selector_data {
	short                dir;
	short                ver;
	union {
		__be32            ip4;
#if IS_ENABLED(CONFIG_IPV6)
		struct in6_addr   ip6;
#endif
	} u;
};


struct state_selector_data {
	u8                list; /* AVM_PA_LIST_* */
	u8                flushed;
};

union pa_selector_union {
	struct port_selector_data    port_data;
	struct pid_selector_data     pid_data;
	struct host_selector_data    host_data;
	struct state_selector_data   state_data;
	struct group_selector_data   group_data;
};


struct pa_selector_entry {
	struct list_head          list;
	union  pa_selector_union  data;
	struct pa_selector       *selector;
	int                       negated;
};

struct pa_selector_ops {
	int           (*setup)(struct pa_selector_entry *sel,
	                       int argc,
	                       const char **argv,
	                       const char *prefix);
	int           (*check)(struct pa_selector_entry *sel,
	                       struct avm_pa_session *sess);
	ssize_t        (*show)(struct pa_selector_entry *sel,
	                       char *buf,
	                       size_t bufsz);
};

#define DEFINE_SELECTOR_OPS_SINGLE(name)  \
struct pa_selector_ops name ## _ops = {   \
	.setup = NULL,                    \
	.check = name ## _selector_check, \
	.show  = NULL                     \
}

#define DEFINE_SELECTOR_OPS(name)         \
struct pa_selector_ops name ## _ops = {   \
	.setup = name ## _selector_setup, \
	.check = name ## _selector_check, \
	.show  = name ## _selector_show   \
}

struct pa_selector {
	const char             *prefix;
	const char             *label;
	int                     args;
	struct pa_selector_ops *ops;
};



/* This function ultimately decides whether a session is selected by calling
 * into the check method of all selectors
 */
int
avm_pa_session_is_selected(struct list_head *selector_list,
			   struct avm_pa_session *sess)
{
	struct pa_selector_entry *sel;

	/* Selectors are connected with logical AND, so the first failed check
	 * kicks it out. Selectors may be negated in which case the check()
	 * return shall be inverted.
	 */
	list_for_each_entry(sel, selector_list, list) {
		if (sel->selector->ops->check(sel, sess) == sel->negated)
			return 0;
	}
	return 1;
}
EXPORT_SYMBOL(avm_pa_session_is_selected);

#ifdef CONFIG_PROC_FS
/* Used to kill the pid/vpid selectors when the actual pid/vpid is unregistered
 * at runtime.
 */
static void
clear_filters_for_pid(struct list_head *selector_list,
		      avm_pid_handle pid,
		      const char *label)
{
	struct pa_selector_entry *sel, *temp;

	list_for_each_entry_safe(sel, temp, selector_list, list) {
		if (!strcmp(sel->selector->label, label)
		    && sel->data.pid_data.pid == pid) {
			list_del(&sel->list);
			kfree(sel);
		}
	}
}


void
avm_pa_selector_clear_for_pid(struct list_head *selector_list,
			      avm_pid_handle pid)
{
	clear_filters_for_pid(selector_list, pid, "pid");
}


void
avm_pa_selector_clear_for_vpid(struct list_head *selector_list,
			       avm_vpid_handle vpid)
{
	clear_filters_for_pid(selector_list, vpid, "vpid");
}
#endif

static int
ip_selector_check(struct pa_selector_entry *sel,
		  struct avm_pa_session *sess)
{
	u16 i_pkttype = sess->ingress.pkttype;
	u16 o_pkttype = avm_pa_first_egress(sess)->match.pkttype;

	if (AVM_PA_PKTTYPE_IP_VERSION(i_pkttype) == 4)
		return 1;

	if (AVM_PA_PKTTYPE_IPENCAP_VERSION(i_pkttype) == 4)
		return 1;

	if (AVM_PA_PKTTYPE_IPENCAP_VERSION(o_pkttype) == 4)
		return 1;

	return 0;
}
static DEFINE_SELECTOR_OPS_SINGLE(ip);


static int
ip6_selector_check(struct pa_selector_entry *sel,
		   struct avm_pa_session *sess)
{
	u16 i_pkttype = sess->ingress.pkttype;
	u16 o_pkttype = avm_pa_first_egress(sess)->match.pkttype;

	if (AVM_PA_PKTTYPE_IP_VERSION(i_pkttype) == 6)
		return 1;

	if (AVM_PA_PKTTYPE_IPENCAP_VERSION(i_pkttype) == 6)
		return 1;

	if (AVM_PA_PKTTYPE_IPENCAP_VERSION(o_pkttype) == 6)
		return 1;

	return 0;
}
static DEFINE_SELECTOR_OPS_SINGLE(ip6);


static int
dslite_selector_check(struct pa_selector_entry *sel,
		      struct avm_pa_session *sess)
{
	u16 i_pkttype = sess->ingress.pkttype;
	u16 o_pkttype = avm_pa_first_egress(sess)->match.pkttype;

	if (AVM_PA_PKTTYPE_IP_VERSION(i_pkttype) == 4) {
		if (AVM_PA_PKTTYPE_IPENCAP_VERSION(i_pkttype) == 6)
			return 1;
	}

	if (AVM_PA_PKTTYPE_IP_VERSION(o_pkttype) == 4) {
		if (AVM_PA_PKTTYPE_IPENCAP_VERSION(o_pkttype) == 6)
			return 1;
	}

	return 0;
}
static DEFINE_SELECTOR_OPS_SINGLE(dslite);


static int
gre_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	u16 i_pkttype = sess->ingress.pkttype;
	u16 o_pkttype = avm_pa_first_egress(sess)->match.pkttype;

	return ((i_pkttype|o_pkttype) & AVM_PA_PKTTYPE_GRE) != 0;
}
static DEFINE_SELECTOR_OPS_SINGLE(gre);


static int
l2tp_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	u16 i_pkttype = sess->ingress.pkttype;
	u16 o_pkttype = avm_pa_first_egress(sess)->match.pkttype;

	return ((i_pkttype|o_pkttype) & AVM_PA_PKTTYPE_L2TP) != 0;
}
static DEFINE_SELECTOR_OPS_SINGLE(l2tp);


static int
lisp_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	u16 i_pkttype = sess->ingress.pkttype;
	u16 o_pkttype = avm_pa_first_egress(sess)->match.pkttype;

	return ((i_pkttype|o_pkttype) & AVM_PA_PKTTYPE_LISP) != 0;
}
static DEFINE_SELECTOR_OPS_SINGLE(lisp);


static int
port_selector_setup(struct pa_selector_entry *sel,
		    int argc, const char **argv,
		    const char *prefix)
{
	int ret;
	long port;
	struct port_selector_data *data = &sel->data.port_data;

	if (argc < 2)
		return -EINVAL;

	ret = kstrtol(argv[1], 10, &port);
	if (ret < 0)
		return ret;

	data->dir = prefix_to_dir(prefix);
	data->port = htons(port&0xffff);

	return 0;
}

static int
port_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	struct avm_pa_pkt_match *info = &sess->ingress;
	struct port_selector_data *data = &sel->data.port_data;
	int ports_off, proto;

	if (AVM_PA_PKTTYPE_IP_VERSION(info->pkttype) == 4) {
		struct iphdr *iph;

		iph = (struct iphdr *) (HDRCOPY(info) + info->ip_offset);
		proto = iph->protocol & 0xff;
		ports_off = info->ip_offset + PA_IPHLEN(iph);
	} else if (AVM_PA_PKTTYPE_IP_VERSION(info->pkttype) == 6) {
		struct ipv6hdr *ip6h;

		ip6h = (struct ipv6hdr *) (HDRCOPY(info) + info->ip_offset);
		proto = ip6h->nexthdr & 0xff;
		ports_off = info->ip_offset + sizeof(struct ipv6hdr);
	} else {
		/* Ports can only match IP packets */
		return 0;
	}

	if (proto == IPPROTO_TCP || proto == IPPROTO_UDP) {
		hdrunion_t *hdr = (hdrunion_t *)(HDRCOPY(info)+ports_off);

		if (data->dir & DIR_INGRESS && data->port == hdr->ports[0])
			return 1;
		if (data->dir & DIR_EGRESS && data->port == hdr->ports[1])
			return 1;
	}
	return 0;
}


static ssize_t
port_selector_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	struct port_selector_data *data = &sel->data.port_data;
	const char *dir;

	if (data->dir == DIR_INGRESS)
		dir = "src ";
	else if (data->dir == DIR_EGRESS)
		dir = "dst ";
	else
		dir = "";

	return snprintf(buf, bufsz, "%sport %d", dir, ntohs(data->port));
}
static DEFINE_SELECTOR_OPS(port);


static int
host_selector_setup(struct pa_selector_entry *sel,
		    int argc,
		    const char **argv,
		    const char *prefix)
{
	struct host_selector_data *data = &sel->data.host_data;

	if (argc < 2)
		return -EINVAL;

	data->dir = prefix_to_dir(prefix);
	if (!strchr(argv[1], ':')) {
		unsigned char *ip = (unsigned char *) &data->u.ip4;

		data->ver = 4;
		if (sscanf(argv[1], "%hhu.%hhu.%hhu.%hhu",
		           &ip[0], &ip[1], &ip[2], &ip[3]) != 4)
			return -EINVAL;
	}
#if IS_ENABLED(CONFIG_IPV6)
	else {
		unsigned short *ip = (unsigned short *) &data->u.ip6;

		data->ver = 6;
		if (sscanf(argv[1], "%hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx",
		           &ip[0], &ip[1], &ip[2], &ip[3], &ip[4], &ip[5], &ip[6], &ip[7]) != 8)
			return -EINVAL;
	}
#endif

	return 0;
}


static int
host_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	struct avm_pa_pkt_match *info = &sess->ingress;
	struct host_selector_data *data = &sel->data.host_data;
	int ing = data->dir & DIR_INGRESS;
	int eg = data->dir & DIR_EGRESS;

	if (data->ver == AVM_PA_PKTTYPE_IP_VERSION(info->pkttype)) {
		if (data->ver == 4) {
			struct iphdr *iph;

			iph = (struct iphdr *) (HDRCOPY(info) + info->ip_offset);
			if (ing && data->u.ip4 == iph->saddr)
				return 1;
			if (eg && data->u.ip4 == iph->daddr)
				return 1;
		}
#if IS_ENABLED(CONFIG_IPV6)
		else {
			struct ipv6hdr *ip6h;

			ip6h = (struct ipv6hdr *) (HDRCOPY(info) + info->ip_offset);
			if (ing && ipv6_addr_equal(&data->u.ip6, &ip6h->saddr))
				return 1;
			if (eg && ipv6_addr_equal(&data->u.ip6, &ip6h->daddr))
				return 1;
		}
#endif
	}

	return 0;
}


static ssize_t
host_selector_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	struct host_selector_data *data = &sel->data.host_data;
	const char *dir;

	if (data->dir == DIR_INGRESS)
		dir = "src ";
	else if (data->dir == DIR_EGRESS)
		dir = "dst ";
	else
		dir = "";

#if IS_ENABLED(CONFIG_IPV6)
	if (data->ver == 6)
		return snprintf(buf, bufsz, "%shost %pI6", dir, &data->u.ip6);
#endif
	return snprintf(buf, bufsz, "%shost %pI4", dir, &data->u.ip4);
}
static DEFINE_SELECTOR_OPS(host);


static int
get_session_proto(struct avm_pa_session *sess)
{
	struct avm_pa_pkt_match *info = &sess->ingress;

	if (info->pkttype & AVM_PA_PKTTYPE_IP_MASK)
		return AVM_PA_PKTTYPE_IPPROTO(info->pkttype);

	return 0;
}


static int
tcp_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	return get_session_proto(sess) == IPPROTO_TCP;
}
static DEFINE_SELECTOR_OPS_SINGLE(tcp);


static int
udp_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	return get_session_proto(sess) == IPPROTO_UDP;
}
static DEFINE_SELECTOR_OPS_SINGLE(udp);


static int
icmp_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	return get_session_proto(sess) == IPPROTO_ICMP;
}
static DEFINE_SELECTOR_OPS_SINGLE(icmp);


static int
esp_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	return get_session_proto(sess) == IPPROTO_ESP;
}
static DEFINE_SELECTOR_OPS_SINGLE(esp);


static int
pid_selector_setup(struct pa_selector_entry *sel,
		   int argc, const char **argv,
		   const char *prefix)
{
	const char *pidname;
	unsigned long pid_l;
	int isvpid, n;
	size_t max_pid;

	/* This function is shared between pid and vpid selectors */
	isvpid = sel->selector->label[0] == 'v';
	max_pid = isvpid ? CONFIG_AVM_PA_MAX_VPID : CONFIG_AVM_PA_MAX_PID;

	if (argc < 2)
		return -EINVAL;

	pidname = argv[1];
	/* PID may be specified by id */
	if (isdigit(*pidname)) {
		if (kstrtoul(pidname, 10, &pid_l) < 0
		    || pid_l > max_pid
		    || pid_l == 0)
			return -ENODEV;
		pidname = 0;
	}

	sel->data.pid_data.pid = 0;
	sel->data.pid_data.dir = prefix_to_dir(prefix);

	for (n = 1; n < max_pid; n++) {
		if (isvpid) {
			struct avm_pa_vpid *vpid = avm_pa_vpid_get_vpid(n);

			if (!vpid)
				continue;
			if (pidname && 0 == strcmp(vpid->cfg.name, pidname))
				sel->data.pid_data.pid = vpid->vpid_handle;
			else if (!pidname && pid_l == vpid->vpid_handle)
				sel->data.pid_data.pid = vpid->vpid_handle;
			avm_pa_vpid_put(n);
		} else {
			struct avm_pa_pid *pid = avm_pa_pid_get_pid(n);

			if (!pid)
				continue;
			if (pidname && 0 == strcmp(pid->cfg.name, pidname))
				sel->data.pid_data.pid = pid->pid_handle;
			else if (!pidname && pid_l == pid->pid_handle)
				sel->data.pid_data.pid = pid->pid_handle;
			avm_pa_pid_put(n);
		}

		if (sel->data.pid_data.pid > 0)
			return 0;
	}

	return -ENODEV;
}


static int
pid_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	int dir = sel->data.pid_data.dir;
	avm_pid_handle pid = sel->data.pid_data.pid;
	struct avm_pa_egress *egress;

	if (dir & DIR_INGRESS) {
		if (pid == sess->ingress_pid_handle)
			return 1;
	}

	if (dir & DIR_EGRESS) {
		hlist_for_each_entry_rcu_bh(egress, &sess->egress_head, egress_list) {
		if (pid == egress->pid_handle)
			return 1;
		}
	}

	return 0;
}


static ssize_t
pid_selector_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	int dir = sel->data.pid_data.dir;
	avm_pid_handle n = sel->data.pid_data.pid;
	const char *pdir;
	const char *name;
	int isvpid;
	const char *label = sel->selector->label;

	/* This function is shared between pid and vpid selectors */
	isvpid = label[0] == 'v';

	if (dir == -1)
		pdir = "ingress ";
	else if (dir == 1)
		pdir = "egress ";
	else
		pdir = "";

	if (isvpid) {
		struct avm_pa_vpid *vpid = avm_pa_vpid_get_vpid(n);

		if (vpid) {
			name = vpid->cfg.name;
			avm_pa_vpid_put(n);
		} else {
			name = "(null vpid)";
		}
	} else {
		struct avm_pa_pid *pid = avm_pa_pid_get_pid(n);

		if (pid) {
			name = pid->cfg.name;
			avm_pa_pid_put(n);
		} else {
			name = "(null pid)";
		}
	}

	return snprintf(buf, bufsz, "%s%s %s", pdir, label, name);
}
static DEFINE_SELECTOR_OPS(pid);

static int
vpid_selector_setup(struct pa_selector_entry *sel,
		    int argc,
		    const char **argv,
		    const char *prefix)
{
	return pid_selector_setup(sel, argc, argv, prefix);
}


static int
vpid_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	int dir = sel->data.pid_data.dir;
	avm_vpid_handle vpid = sel->data.pid_data.pid;
	struct avm_pa_egress *egress;

	if (dir & DIR_INGRESS) {
		if (vpid == sess->ingress_vpid_handle)
			return 1;
	}

	if (dir & DIR_EGRESS) {
		hlist_for_each_entry_rcu_bh(egress, &sess->egress_head, egress_list) {
		if (vpid == egress->vpid_handle)
			return 1;
		}
	}

	return 0;
}


static ssize_t
vpid_selector_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	return pid_selector_show(sel, buf, bufsz);
}
static DEFINE_SELECTOR_OPS(vpid);

static int
group_selector_setup(struct pa_selector_entry *sel,
		   int argc, const char **argv,
		   const char *prefix)
{
	unsigned long group_l;

	if (argc < 2)
		return -EINVAL;

	if (kstrtoul(argv[1], 10, &group_l) < 0
	    || group_l >= AVM_PA_MAX_SESSIONGROUP
	    || group_l == 0)
		return -ENODEV;

	sel->data.group_data.groupid = (unsigned short)group_l;

	return 0;
}

static int
group_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	unsigned short groupid = sel->data.group_data.groupid;

	return avm_pa_session_belongs_to_sg(sess, groupid);
}


static ssize_t
group_selector_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	unsigned short groupid = sel->data.group_data.groupid;
	const char *label = sel->selector->label;

	return snprintf(buf, bufsz, "%s %hu", label, groupid);
}
static DEFINE_SELECTOR_OPS(group);

static int
state_selector_setup(struct pa_selector_entry *sel,
		     int argc, const char **argv,
		     const char *prefix)
{
	struct state_selector_data *data = &sel->data.state_data;
	const char *state = argv[1];

	if (!strcmp(state, "active")) {
		data->list = AVM_PA_LIST_ACTIVE;
		data->flushed = 0;
	} else if (!strcmp(state, "flushed")) {
		data->list = AVM_PA_LIST_ACTIVE;
		data->flushed = 1;
	} else if (!strcmp(state, "dead")) {
		data->list = AVM_PA_LIST_DEAD;
	} else {
		return -EINVAL;
	}

	return 0;
}


static int
state_selector_check(struct pa_selector_entry *sel, struct avm_pa_session *sess)
{
	struct state_selector_data *data = &sel->data.state_data;

	if (sess->on_list == data->list) {
		if (data->list == AVM_PA_LIST_ACTIVE)
			return sess->flushed == data->flushed;
		return 1;
	}

	/* Careful: We always come here for sessions that in state CREATE. This
	 * implies that if the state selector is used for session filtering,
	 * no sessions will be created ever.
	 */
	return 0;
}


static ssize_t
state_selector_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	struct state_selector_data *data = &sel->data.state_data;
	const char *state;

	switch (data->list) {
	case AVM_PA_LIST_ACTIVE:
		state = data->flushed ? "flushed" : "active";
		break;
	case AVM_PA_LIST_DEAD:
		state = "dead";
		break;
	case AVM_PA_LIST_FREE:
		state = "free";
		break;
	default:
		BUG();
		break;
	}

	return strlcpy(buf, state, bufsz);
}
static DEFINE_SELECTOR_OPS(state);


static ssize_t
single_show(struct pa_selector_entry *sel, char *buf, size_t bufsz)
{
	return strlcpy(buf, sel->selector->label, bufsz);
}


static struct pa_selector all_selectors[] = {
	{ "",          "ip",             0, &ip_ops },
	{ "",          "ip6",            0, &ip6_ops },
	{ "",          "tcp",            0, &tcp_ops },
	{ "",          "udp",            0, &udp_ops },
	{ "",          "icmp",           0, &icmp_ops },
	{ "",          "esp",            0, &esp_ops },
	{ "",          "dslite",         0, &dslite_ops },
	{ "",          "gre",            0, &gre_ops },
	{ "",          "l2tp",           0, &l2tp_ops },
	{ "",          "lisp",           0, &lisp_ops },
	{ "src",       "port",           1, &port_ops },
	{ "dst",       "port",           1, &port_ops },
	{ "",          "port",           1, &port_ops },
	{ "src",       "host",           1, &host_ops },
	{ "dst",       "host",           1, &host_ops },
	{ "",          "host",           1, &host_ops },
	{ "ingress",   "pid",            1, &pid_ops },
	{ "egress",    "pid",            1, &pid_ops },
	{ "",          "pid",            1, &pid_ops },
	{ "ingress",   "vpid",           1, &vpid_ops },
	{ "egress",    "vpid",           1, &vpid_ops },
	{ "",          "vpid",           1, &vpid_ops },
	{ "",          "group",          1, &group_ops },
	/* Do not use state selector for session filtering,
	 * no sessions will be created (see state_selector_check).
	 */
	{ "",          "state",          1, &state_ops },
};


static int
is_prefix(const char *prefix)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(all_selectors); i++) {
		if (!strcmp(prefix, all_selectors[i].prefix))
			return 1;
	}

	return 0;
}

static struct pa_selector *
find_selector(const char *label, const char *prefix)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(all_selectors); i++) {
		if (!strcmp(label, all_selectors[i].label)) {
			/* If the selector has a prefix then user space must
			 * provide it and it must match as well. If the
			 * selector has no prefix then user space must not
			 * provide one. However, all_selectors can contain the
			 * same selectors with and without prefix
			 */
			if (!prefix && !all_selectors[i].prefix[0])
				return &all_selectors[i];
			else if (prefix && !strcmp(prefix, all_selectors[i].prefix))
				return &all_selectors[i];
		}
	}

	return 0;
}

#define MAX_SELECTOR_STRING_SIZE 1024

static int
parse_selectors(struct list_head *list,
		int argc, const char **argv,
		gfp_t gfp_mask)
{
	const char *prefix = 0;
	struct pa_selector *_sel;
	struct pa_selector_entry *sel, *temp;
	int i = 0, ret = 0, negate = 0;

	/* all clears the selectors (all sessions will be shown again) */
	if (argc == 1 && !strcmp(argv[0], "all"))
		goto out;

	while (i < argc) {

		if (!strcmp(argv[i], "not")) {
			negate ^= 1;
			i += 1;
			ret = -EINVAL;
			continue;
		} else if (!strcmp(argv[i], "and")) {
			/* and between selectors is implicit, but can be
			 * specified for clarity (as a bonus it helps detecting
			 * errors when it occurs where a selector is expected)
			 */
			i += 1;
			continue;
		} else if (is_prefix(argv[i])) {
			prefix = argv[i];
			i += 1;
			ret = -EINVAL;
			continue;
		}

		/* locate selector which may require a certain prefix as well */
		_sel = find_selector(argv[i], prefix);
		if (!_sel) {
			ret = -EINVAL;
			goto out;
		}

		sel = kmalloc(sizeof(struct pa_selector_entry), gfp_mask);
		if (!sel) {
			ret = -ENOMEM;
			goto out;
		}

		INIT_LIST_HEAD(&sel->list);
		sel->selector = _sel;
		sel->negated = negate;
		if (_sel->ops->setup) {
			ret = _sel->ops->setup(sel, argc - i, argv + i, prefix);
			if (ret < 0)
				goto out;
		}
		i += _sel->args + 1; /* skip args consumed by selector */
		list_add_tail(&sel->list, list);
		prefix = 0;
		negate = 0;
	}

	return ret;

out:
	list_for_each_entry_safe(sel, temp, list, list) {
		list_del(&sel->list);
		kfree(sel);
	}
	return ret;
}

int
avm_pa_parse_selector(struct list_head *selector_list,
		      const char *buffer, gfp_t gfp_mask)
{
	static const char delimitters[] = " ";
	char   *argv[48];
	int     argc = 0;
	char   *tempbuf;
	char   *curr;
	char   *next;
	int     ret = 0;
	LIST_HEAD(new_selectors);

	tempbuf = next = kstrdup(buffer, gfp_mask);
	curr = strsep(&next, delimitters);
	if (curr == NULL)
		return next ? -EINVAL : -ENOMEM;

	do {
		argv[argc++] = curr;

		if (argc >= ARRAY_SIZE(argv)) {
			pr_err("avm_pa: too many parameters dropping the command\n");
			ret = -EIO;
			goto out;
		}

		curr = strsep(&next, delimitters);
	} while (curr != NULL);

	ret = parse_selectors(&new_selectors,
	                      argc, (const char **) argv,
	                      gfp_mask);

	/* Replace current selectors on success, otherwise leave
	 * them unchanged
	 */
	if (ret == 0) {
		avm_pa_selector_free(selector_list);
		list_replace(&new_selectors, selector_list);
	}

out:
	kfree(tempbuf);
	return ret;
}
EXPORT_SYMBOL(avm_pa_parse_selector);

int
avm_pa_selector_foreach(struct list_head *selector_list,
			int (*func)(struct avm_pa_session *session, void *data),
			void *data)
{
	struct avm_pa_data    *pd = &pa_data;
	struct avm_pa_session *sess;
	int i, ret = 0;

	rcu_read_lock_bh();
	for (i = 0; i < CONFIG_AVM_PA_MAX_SESSION && ret == 0; ++i) {
		sess = &pd->sessions[i];
		if (avm_pa_session_valid(sess)) {
			if (avm_pa_session_is_selected(selector_list, sess))
				ret = func(sess, data);
		}
	}
	rcu_read_unlock_bh();

	return ret;
}
EXPORT_SYMBOL(avm_pa_selector_foreach);

void
avm_pa_selector_free(struct list_head *selector_list)
{
	struct pa_selector_entry *sel, *temp;

	list_for_each_entry_safe(sel, temp, selector_list, list) {
		list_del(&sel->list);
		kfree(sel);
	}
}
EXPORT_SYMBOL(avm_pa_selector_free);

#ifdef CONFIG_PROC_FS
ssize_t
avm_pa_dump_selector_user(struct list_head *selector_list,
			  char __user *buffer,
			  size_t count)
{
	struct pa_selector_entry *sel;
	ssize_t pos = 0;
	char *buf;
	ssize_t (*show_func)(struct pa_selector_entry *sel,
	                     char *buf,
	                     size_t bufsz);

	buf = kmalloc(MAX_SELECTOR_STRING_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	list_for_each_entry(sel, selector_list, list) {
		if (sel->negated)
			pos += strlcpy(buf + pos, "( NOT ", count - pos);

		show_func = sel->selector->ops->show ?: single_show;
		show_func(sel, buf + pos, count - pos);

		if (sel->negated)
			pos += strlcpy(buf + pos, " )", count - pos);

		if (!list_is_last(&sel->list, selector_list))
			pos += strlcpy(buf + pos, " AND ", count - pos);

		if (unlikely(pos >= count)) {
			pos = count;
			break;
		}
	}

	pos += strlcpy(buf + pos, "\n", count - pos);
	if (pos >= count)
		pos = -E2BIG;
	else if (copy_to_user(buffer, buf, pos))
		pos = -EFAULT;

	kfree(buf);
	return pos;
}


ssize_t
avm_pa_parse_selector_user(struct list_head *selector_list,
			   const char __user *buffer,
			   size_t count)
{
	char temp[101];
	ssize_t end;

	/* Validate the length of data passed. */
	if (count >= sizeof(temp))
		count = sizeof(temp) - 1;

	/* Copy from user space. */
	if (copy_from_user(&temp, buffer, count))
		return -EFAULT;

	temp[count] = '\0';
	end = count - 1;
	while (end >= 0 && isspace(temp[end]))
		temp[end--] = '\0';

	return avm_pa_parse_selector(selector_list, temp, GFP_KERNEL) ?: count;
}
#endif
