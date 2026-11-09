/*
 * Packet Accelerator Group Counter Implementation
 *
 * vim:set noexpandtab shiftwidth=4 softtabstop=4:
 *
 * Copyright (c) 2019-2020 AVM GmbH <info@avm.de>
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

#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include "avm_pa.h"
#include "avm_pa_intern.h"

/* ------------------------------------------------------------------------ */

#ifdef AVM_PA_SESSIONGROUPS_WITH_PACKET_PROCESSOR
#include <linux/avalanche/puma7/ppsgc_db.h> // PPSGC_GROUP_COUNTER_NTOH
#endif

/* ------------------------------------------------------------------------ */

struct pa_sg_id {
	struct hlist_node      id_list; /* next & prev */
	union {
		avm_session_handle sessionid;
		unsigned short     groupid;
	} u;
};

struct pa_sg_group {
	/* struct pa_sg_id with sessionid */
	struct hlist_head      sessions;
	struct avm_pa_sg_stats closed;
	unsigned short         groupid;
	bool                   count_egress;
};

/* ------------------------------------------------------------------------ */

static DEFINE_SPINLOCK(avm_pa_sg_lock);

#ifdef AVM_PA_SESSIONGROUPS_WITH_PACKET_PROCESSOR
#define AVM_PA_SG_LOCK(flags) spin_lock_irqsave(&avm_pa_sg_lock, flags)
#define AVM_PA_SG_UNLOCK(flags) spin_unlock_irqrestore(&avm_pa_sg_lock, flags)
#else
#define AVM_PA_SG_LOCK(flags) spin_lock_bh(&avm_pa_sg_lock)
#define AVM_PA_SG_UNLOCK(flags) spin_unlock_bh(&avm_pa_sg_lock)
#endif

/* ------------------------------------------------------------------------ */

static struct avm_pa_sg_global {
	/* pa_sg_id */
	struct kmem_cache      *id_cache;
	/* memory and direct access to group counters */
	struct pa_sg_group       groups[AVM_PA_MAX_SESSIONGROUP];
	unsigned long           id_count;
} pa_sg_glob;

#define PASG_GROUP(group) (&pa_sg_glob.groups[(group)%AVM_PA_MAX_SESSIONGROUP])
#define PASG_SESSION(sessionid) (&pa_data.sessions[(sessionid)%CONFIG_AVM_PA_MAX_SESSION])

#define DIM(array) (sizeof(array)/sizeof((array)[0]))

/* -------- pa_sg_id ------------------------------------------------------- */

static inline struct pa_sg_id *pa_sg_id_alloc(void)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;
	struct pa_sg_id *p;

	p = kmem_cache_zalloc(ctx->id_cache, GFP_ATOMIC);
	if (p)
		pa_sg_glob.id_count++;
	return p;
}

static inline void pa_sg_id_free(struct pa_sg_id *p)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;

	pa_sg_glob.id_count--;
	kmem_cache_free(ctx->id_cache, p);
}

static void pa_sg_id_ctor(void *ptr)
{
	struct pa_sg_id *p = (struct pa_sg_id *)ptr;

	INIT_HLIST_NODE(&p->id_list);
}

static int pa_sg_id_mng_init(void)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;

	ctx->id_cache = kmem_cache_create("avm_pa_sg_id", sizeof(struct pa_sg_id), 0, 0, pa_sg_id_ctor);
	return ctx->id_cache ? 0 : -1;
}

static void pa_sg_id_mng_exit(void)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;

	kmem_cache_destroy(ctx->id_cache);
	ctx->id_cache = NULL;
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

static struct pa_sg_group *pa_sg_group_get(unsigned short groupid)
{
	struct pa_sg_group *p;

	if (groupid > 0 && groupid < AVM_PA_MAX_SESSIONGROUP) {
		p = PASG_GROUP(groupid);
		if (p->groupid == groupid)
			return p;
	}
	return NULL;
}

static int
pa_sg_group_del_session(struct pa_sg_group *group, avm_session_handle sid)
{
	struct pa_sg_id *p;

	hlist_for_each_entry(p, &group->sessions, id_list) {
		if (p->u.sessionid == sid) {
			hlist_del(&p->id_list);
			pa_sg_id_free(p);
			return 0;
		}
	}

	return -1;
}

/* ------------------------------------------------------------------------ */

static struct avm_pa_session *pa_sg_session_get(avm_session_handle sid)
{
	if (sid < CONFIG_AVM_PA_MAX_SESSION)
		return PASG_SESSION(sid);
	return NULL;
}

static int
pa_sg_session_del_group(struct avm_pa_session *session, unsigned short groupid)
{
	struct pa_sg_id *p;

	hlist_for_each_entry(p, &session->groups, id_list) {
		if (p->u.groupid == groupid) {
			hlist_del(&p->id_list);
			pa_sg_id_free(p);
			return 0;
		}
	}

	return -1;
}

/* -------- struct pa_sg_group --------------------------------------------- */

static int
pa_sg_group_add_session(struct pa_sg_group *group, avm_session_handle sid)
{
	struct pa_sg_id *p;

	hlist_for_each_entry(p, &group->sessions, id_list) {
		if (p->u.sessionid == sid) {
			pr_err("avm_pa_sg: duplicate session %d in skb for group %hu\n",
				(int)sid, group->groupid);
			return 0;
		}
	}

	p = pa_sg_id_alloc();
	if (p == NULL) {
		pr_err("avm_pa_sg: pa_sg_id_alloc(session) failed\n");
		return -1;
	}
	p->u.sessionid = sid;
	hlist_add_head(&p->id_list, &group->sessions);
	return 0;
}

static void
pa_sg_group_remove_sessions(struct pa_sg_group *group)
{
	struct avm_pa_session *session;
	struct hlist_node *tmp;
	struct pa_sg_id *p;

	hlist_for_each_entry_safe(p, tmp, &group->sessions, id_list) {
		session = pa_sg_session_get(p->u.sessionid);
		if (session)
			pa_sg_session_del_group(session, group->groupid);
		else
			pr_err("avm_pa_sg: session in grouplist not valid\n");
		hlist_del(&p->id_list);
		pa_sg_id_free(p);
	}
}


static int
pa_sg_group_mng_init(void)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;
	struct pa_sg_group *group;
	unsigned short groupid;

	for (groupid = 0; groupid < AVM_PA_MAX_SESSIONGROUP; groupid++) {
		group = &ctx->groups[groupid];
		group->groupid = AVM_PA_MAX_SESSIONGROUP;
		INIT_HLIST_HEAD(&group->sessions);
		memset(&group->closed, 0, sizeof(group->closed));
	}
	return 0;
}

static void
pa_sg_group_mng_exit(void)
{
	struct pa_sg_group *group;
	unsigned short groupid;

	for (groupid = 0; groupid < AVM_PA_MAX_SESSIONGROUP; groupid++) {
		group = pa_sg_group_get(groupid);
		if (group)
			pa_sg_group_remove_sessions(group);
	}
}

/* -------- session ------------------------------------------------------- */

static int
pa_sg_session_add_group(struct avm_pa_session *session, unsigned short groupid)
{
	struct pa_sg_id *p;

	hlist_for_each_entry(p, &session->groups, id_list) {
		if (p->u.groupid == groupid)
			return 0;
	}

	p = pa_sg_id_alloc();
	if (p == NULL) {
		pr_err("avm_pa_sg: pa_sg_id_alloc(group) failed\n");
		return -1;
	}
	p->u.groupid = groupid;
	hlist_add_head(&p->id_list, &session->groups);
	return 0;
}

static int
pa_sg_session_insert_group(struct avm_pa_session *session,
			  unsigned short groupid)
{
	struct pa_sg_group *group = pa_sg_group_get(groupid);

	if (group == NULL)
		return -1;
	if (pa_sg_session_add_group(session, groupid) < 0)
		return -1;
	if (pa_sg_group_add_session(group, session->session_handle) < 0) {
		pa_sg_session_del_group(session, groupid);
		return -1;
	}
	return 0;
}

static u64 pa_sg_adjust_bytes(u64 bytes, u64 pkts, int adjust_per_packet)
{
	if (adjust_per_packet < 0)
		return bytes - pkts * -adjust_per_packet;
	return bytes + pkts * adjust_per_packet;
}

static int pa_sg_session_get_egress_ajdust(struct avm_pa_session *session)
{
	struct avm_pa_egress *egress = avm_pa_first_egress(session);

	return (session->mod.push_encap_len + egress->push_l2_len)
	     - (session->mod.pull_l2_len + session->mod.pull_encap_len);
}

static void pa_sg_add_session_stats(struct avm_pa_session *session,
				 struct avm_pa_sg_stats *counter,
				 int adjust_per_packet)
{
	struct avm_pa_session_stats *stats;
	u64 bytes;
	u64 pkts;

	if (ktime_after(session->stats_timestamp, counter->timestamp))
		counter->timestamp = session->stats_timestamp;
	stats = &session->ingress_sw_stats;
	pkts = stats->tx_pkts;
	bytes = stats->tx_bytes;
	stats = &session->ingress_hw_stats;
	if (stats->validflags & AVM_PA_SESSION_STATS_VALID_PKTS)
		pkts += stats->tx_pkts;
	if (stats->validflags & AVM_PA_SESSION_STATS_VALID_BYTES)
		bytes += stats->tx_bytes;
	bytes = pa_sg_adjust_bytes(bytes, pkts, adjust_per_packet);
	counter->total_pkts += pkts;
	counter->total_bytes += bytes;
	if (session->ingress.casttype == AVM_PA_IS_MULTICAST) {
		counter->multicast_pkts  += pkts;
		counter->multicast_bytes += bytes;
	}
}

/*------------------------------------------------------------------------ */


static inline void pa_sg_counter_add(struct avm_pa_sg_stats *p,
				    struct avm_pa_sg_stats *counter,
				    int adjust_per_packet)
{
	p->total_bytes += pa_sg_adjust_bytes(counter->total_bytes,
					     counter->total_pkts, adjust_per_packet);
	p->total_pkts += counter->total_pkts;
	p->multicast_bytes += pa_sg_adjust_bytes(counter->multicast_bytes,
						 counter->multicast_pkts, adjust_per_packet);
	p->multicast_pkts += counter->multicast_pkts;
}

static void
pa_sg_session_remove_groups(struct avm_pa_session *session)
{
	struct avm_pa_sg_stats counter;
	struct pa_sg_group *group;
	struct hlist_node *tmp;
	struct pa_sg_id *p;
	int adjust_per_packet;
	ktime_t now;

	if (hlist_empty(&session->groups))
		return;
	now = ktime_get_boottime();
	memset(&counter, 0, sizeof(counter));
	pa_sg_add_session_stats(session, &counter, 0);
	adjust_per_packet = pa_sg_session_get_egress_ajdust(session);

	hlist_for_each_entry_safe(p, tmp, &session->groups, id_list) {
		group = pa_sg_group_get(p->u.groupid);
		if (group) {
			if (group->count_egress)
				pa_sg_counter_add(&group->closed, &counter, adjust_per_packet);
			else
				pa_sg_counter_add(&group->closed, &counter, 0);
			group->closed.timestamp = now;
			pa_sg_group_del_session(group, session->session_handle);
		} else
			pr_err("avm_pa_sg: group in sessionlist not valid\n");
		hlist_del(&p->id_list);
		pa_sg_id_free(p);
	}
}

/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void avm_pa_sg_session_link(struct avm_pa_session *session,
			    struct sk_buff *skb)
{
	unsigned long flags __maybe_unused;
	unsigned short groupid;
	unsigned int i;

	/*
	 * TODO:
	 * think about handling of out of memory or other error conditions
	 */

	AVM_PA_SG_LOCK(flags);
	{
#ifdef AVM_PA_SESSIONGROUPS_WITH_PACKET_PROCESSOR
		struct PpsgcAssignedGroupCounters *gc;
		PP_PACKET_INFO_t *ppinfo;

		ppinfo = SKB_GET_PP_INFO_P(skb);
		gc = &ppinfo->assignedGroupCounters;
		for (i = 0; i < gc->numAssignedGroupCounters; i++) {
			groupid = PPSGC_GROUP_COUNTER_NTOH(gc->groupCounterIds[i]);
			if (groupid == PPSGC_GROUP_COUNTER_UNASSIGNED)
				continue;
			(void)pa_sg_session_insert_group(session, groupid);
		}
#else
		struct avm_pa_sessiongroupids *gc;
		struct avm_pa_pkt_info *info;

		info = AVM_PKT_INFO(skb);
		gc = &info->assigned_sessiongroupids;

		for (i = 0; i < gc->ncounter; i++) {
			groupid = gc->counterid[i];
			if (groupid == 0)
				break;
			(void)pa_sg_session_insert_group(session, groupid);
		}
#endif
	}
	AVM_PA_SG_UNLOCK(flags);
}

void avm_pa_sg_session_unlink(struct avm_pa_session *session)
{
	unsigned long flags __maybe_unused;

	AVM_PA_SG_LOCK(flags);
	pa_sg_session_remove_groups(session);
	AVM_PA_SG_UNLOCK(flags);
}

void avm_pa_sg_show_session(struct avm_pa_session *session,
			    pa_fprintf fprintffunc, void *arg)
{
	struct pa_sg_id *p;

	if (!hlist_empty(&session->groups)) {
		(*fprintffunc)(arg, "Group Counter  :");
		hlist_for_each_entry(p, &session->groups, id_list) {
			(*fprintffunc)(arg, " %hu", p->u.groupid);
		}
		(*fprintffunc)(arg, "\n");
	}
}

int avm_pa_session_belongs_to_sg(struct avm_pa_session *session,
				 unsigned short groupid)
{
	struct pa_sg_id *p;

	hlist_for_each_entry(p, &session->groups, id_list) {
		if (p->u.groupid == groupid)
			return 1;
	}
	return 0;
}

/* -------- session group EXPORT ------------------------------------------ */

int avm_pa_sg_alloc_by_groupid(unsigned short groupid)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;
	unsigned long flags __maybe_unused;
	struct pa_sg_group *group;
	int ret = -1;

	BUG_ON(groupid == 0 || groupid >= AVM_PA_MAX_SESSIONGROUP);

	AVM_PA_SG_LOCK(flags);
	group = &ctx->groups[groupid];
	if (group->groupid != groupid) {
		memset(&group->closed, 0, sizeof(group->closed));
		group->groupid = groupid;
		ret = 0;
	}
	AVM_PA_SG_UNLOCK(flags);
	return ret;
}
EXPORT_SYMBOL(avm_pa_sg_alloc_by_groupid);

unsigned short avm_pa_sg_alloc(void)
{
	struct avm_pa_sg_global *ctx = &pa_sg_glob;
	unsigned long flags __maybe_unused;
	struct pa_sg_group *group;
	unsigned short groupid;

	AVM_PA_SG_LOCK(flags);
	for (groupid = AVM_PA_MAX_SESSIONGROUP-1; groupid > 0; groupid--) {
		group = &ctx->groups[groupid];
		if (group->groupid != groupid) {
			memset(&group->closed, 0, sizeof(group->closed));
			group->groupid = groupid;
			break;
		}
	}
	AVM_PA_SG_UNLOCK(flags);

	return groupid;
}
EXPORT_SYMBOL(avm_pa_sg_alloc);

int avm_pa_sg_free(unsigned short groupid)
{
	unsigned long flags __maybe_unused;
	struct pa_sg_group *group;
	int ret = -1;

	BUG_ON(groupid == 0 || groupid >= AVM_PA_MAX_SESSIONGROUP);

	AVM_PA_SG_LOCK(flags);
	group = pa_sg_group_get(groupid);
	if (group) {
		pa_sg_group_remove_sessions(group);
		group->groupid = AVM_PA_MAX_SESSIONGROUP;
		ret = 0;
	}
	AVM_PA_SG_UNLOCK(flags);
	return ret;
}
EXPORT_SYMBOL(avm_pa_sg_free);

int avm_pa_sg_restart(unsigned short groupid)
{
	unsigned long flags __maybe_unused;
	struct pa_sg_group *group;
	int ret = -1;

	BUG_ON(groupid == 0 || groupid >= AVM_PA_MAX_SESSIONGROUP);

	AVM_PA_SG_LOCK(flags);
	group = pa_sg_group_get(groupid);
	if (group) {
		pa_sg_group_remove_sessions(group);
		memset(&group->closed, 0, sizeof(group->closed));
		ret = 0;
	}
	AVM_PA_SG_UNLOCK(flags);
	return ret;
}
EXPORT_SYMBOL(avm_pa_sg_restart);

int avm_pa_sg_set_count_egress(unsigned short groupid, bool enable)
{
	struct pa_sg_group *group;

	group = pa_sg_group_get(groupid);
	if (group == NULL)
		return -1;
	group->count_egress = enable;
	return 0;
}
EXPORT_SYMBOL(avm_pa_sg_set_count_egress);

int avm_pa_sg_get_stats(unsigned short groupid,
				struct avm_pa_sg_stats *counter)
{
	unsigned long flags __maybe_unused;
	struct avm_pa_session *session;
	struct pa_sg_group *group;
	struct pa_sg_id *p;
	int adjust_per_packet;

	group = pa_sg_group_get(groupid);
	if (group == NULL)
		return -1;

	*counter = group->closed;

	AVM_PA_SG_LOCK(flags);
	hlist_for_each_entry(p, &group->sessions, id_list) {
		session = pa_sg_session_get(p->u.sessionid);
		if (session) {
			if (group->count_egress)
				adjust_per_packet = pa_sg_session_get_egress_ajdust(session);
			else
				adjust_per_packet = 0;
			pa_sg_add_session_stats(session, counter, adjust_per_packet);
		}
	}
	AVM_PA_SG_UNLOCK(flags);

	return 0;
}
EXPORT_SYMBOL(avm_pa_sg_get_stats);

int avm_pa_sg_mark_skb(struct sk_buff *skb, unsigned short groupid)
{
#ifdef AVM_PA_SESSIONGROUPS_WITH_PACKET_PROCESSOR
	if (PPSGC_Db_AssignGroupCounterId(&(SKB_GET_PP_INFO_P(skb)->assignedGroupCounters), groupid) == PPSGC_RC_SUCCESS)
		return 0;
	return -1;
#else
	struct avm_pa_sessiongroupids *ids;

	if (groupid >= AVM_PA_MAX_SESSIONGROUP)
		return -1;
	ids = &AVM_PKT_INFO(skb)->assigned_sessiongroupids;
	if (ids->ncounter >= AVM_PA_SESSION_GROUPS_PER_SESSION)
		return -1; /* full */
	ids->counterid[ids->ncounter++] = groupid;
	return 0;
#endif
}
EXPORT_SYMBOL(avm_pa_sg_mark_skb);

int avm_pa_sg_init(void)
{
	if (pa_sg_id_mng_init() < 0) {
		pr_err("avm_pa_sg: pa_sg_id_mng_init failed\n");
		return -1;
	}
	if (pa_sg_group_mng_init() < 0) {
		pa_sg_id_mng_exit();
		pr_err("avm_pa_sg: pa_sg_group_mng_init failed\n");
		return -1;
	}

	return 0;
}

void avm_pa_sg_exit(void)
{
	pa_sg_group_mng_exit();
	pa_sg_id_mng_exit();
}

#ifdef CONFIG_PROC_FS
/* ------------------------------------------------------------------------ */
/* -------- group counter /proc ------------------------------------------- */
/* ------------------------------------------------------------------------ */

static s64 pa_age_in_secs(ktime_t now, ktime_t tstamp)
{
	if (ktime_to_ns(tstamp))
		return ktime_divns(ktime_sub(now, tstamp), NSEC_PER_MSEC*1000);
	return -1;
}

static void pa_show_sessiongroups(pa_fprintf fprintffunc, void *arg)
{
	struct avm_pa_sg_stats counter;
	unsigned short groupid;
	ktime_t now = ktime_get_boottime();

	fprintffunc(arg, "     %20s %20s %10s %24s\n",
			"Bytes", "Pkts", "Age", "Multicast Bytes/Pkts");
	for (groupid = 0; groupid < AVM_PA_MAX_SESSIONGROUP; groupid++) {
		if (avm_pa_sg_get_stats(groupid, &counter) == 0) {
			fprintffunc(arg, "%3u: %20llu %20llu %10lld %20llu/%llu\n",
					groupid,
					counter.total_bytes,
					counter.total_pkts,
					pa_age_in_secs(now, counter.timestamp),
					counter.multicast_bytes,
					counter.multicast_pkts);
		}
	}
}

static int sessiongroups_show(struct seq_file *m, void *v)
{
	pa_show_sessiongroups((pa_fprintf *)seq_printf, m);
	return 0;
}

static int sessiongroup_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, sessiongroups_show, PDE_DATA(inode));
}

static const struct file_operations sessiongroups_show_fops = {
	.open    = sessiongroup_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	/* sessiongroup_show_open() uses single_open() */
	.release = single_release,
};

/* ------------------------------------------------------------------------ */

static void pa_show_groupclosed(pa_fprintf fprintffunc, void *arg)
{
	struct pa_sg_group *group;
	unsigned short groupid;
	ktime_t now = ktime_get_boottime();

	fprintffunc(arg, "     %20s %20s %10s %24s\n",
			"Bytes", "Pkts", "Age", "Multicast Bytes/Pkts");
	for (groupid = 0; groupid < AVM_PA_MAX_SESSIONGROUP; groupid++) {
		group = pa_sg_group_get(groupid);
		if (group == NULL)
			continue;
		fprintffunc(arg, "%3u: %20llu %20llu %10lld %20llu/%llu\n",
					groupid,
					group->closed.total_bytes,
					group->closed.total_pkts,
					pa_age_in_secs(now, group->closed.timestamp),
					group->closed.multicast_bytes,
					group->closed.multicast_pkts);
	}
}

static int groupclosed_show(struct seq_file *m, void *v)
{
	pa_show_groupclosed((pa_fprintf *)seq_printf, m);
	return 0;
}

static int groupclosed_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, groupclosed_show, PDE_DATA(inode));
}

static const struct file_operations groupclosed_show_fops = {
	.open    = groupclosed_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	/* groupclosed_show_open() uses single_open() */
	.release = single_release,
};

/* ------------------------------------------------------------------------ */

static void pa_show_pa_sg_group(pa_fprintf fprintffunc, void *arg)
{
	struct pa_sg_group *group;
	unsigned short groupid;
	struct pa_sg_id *id;

	fprintffunc(arg, "%lu active ids\n", pa_sg_glob.id_count);
	for (groupid = 0; groupid < AVM_PA_MAX_SESSIONGROUP; groupid++) {
		group = pa_sg_group_get(groupid);
		if (group == NULL)
			continue;
		fprintffunc(arg, "%3d:", group->groupid);
		hlist_for_each_entry(id, &group->sessions, id_list) {
			fprintffunc(arg, " %hu", id->u.sessionid);
		}
		fprintffunc(arg, "\n");
	}
}

static int pa_sg_group_show(struct seq_file *m, void *v)
{
	pa_show_pa_sg_group((pa_fprintf *)seq_printf, m);
	return 0;
}

static int pa_sg_group_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, pa_sg_group_show, PDE_DATA(inode));
}

static const struct file_operations pa_sg_group_show_fops = {
	.open    = pa_sg_group_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	/* pa_sg_group_show_open() uses single_open() */
	.release = single_release,
};

/* ------------------------------------------------------------------------ */

static void pa_show_pa_sg_session(pa_fprintf fprintffunc, void *arg)
{
	struct avm_pa_session *session;
	avm_session_handle sid;
	struct pa_sg_id *id;

	fprintffunc(arg, "%lu active ids\n", pa_sg_glob.id_count);
	for (sid = 0; sid < CONFIG_AVM_PA_MAX_SESSION; sid++) {
		session = PASG_SESSION(sid);
		if (hlist_empty(&session->groups))
			continue;
		if (avm_pa_session_valid(session))
			fprintffunc(arg, "%3d:", sid);
		else
			fprintffunc(arg, "%3d: not valid", sid);
		hlist_for_each_entry(id, &session->groups, id_list) {
			fprintffunc(arg, " %hu", id->u.groupid);
		}
		fprintffunc(arg, "\n");
	}
}

static int pa_sg_session_show(struct seq_file *m, void *v)
{
	pa_show_pa_sg_session((pa_fprintf *)seq_printf, m);
	return 0;
}

static int pa_sg_session_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, pa_sg_session_show, PDE_DATA(inode));
}

static const struct file_operations pa_sg_session_show_fops = {
	.open    = pa_sg_session_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	/* pa_sg_session_show_open() uses single_open() */
	.release = single_release,
};

/* ------------------------------------------------------------------------ */

void avm_pa_sg_proc_init(struct proc_dir_entry *dir_entry)
{
	proc_create("groups", 0444, dir_entry, &sessiongroups_show_fops);
	proc_create("sg_closed", 0444, dir_entry, &groupclosed_show_fops);
	proc_create("sg_groups", 0444, dir_entry, &pa_sg_group_show_fops);
	proc_create("sg_sessions", 0444, dir_entry, &pa_sg_session_show_fops);
}

void avm_pa_sg_proc_exit(struct proc_dir_entry *dir_entry)
{
	remove_proc_entry("groups", dir_entry);
	remove_proc_entry("sg_groups", dir_entry);
	remove_proc_entry("sg_sessions", dir_entry);
	remove_proc_entry("sg_closed", dir_entry);
}
#endif // CONFIG_PROC_FS
