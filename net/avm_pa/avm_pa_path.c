/*
 * AVM packet accelerator - paths and nodes
 *
 * SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100:
 *
 * Copyright (c) 2011-2020 AVM GmbH <info@avm.de>
 * All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/sort.h>

#include "avm_pa.h"
#include "avm_pa_intern.h"

#define ACT_NODES_HSIZ 128

static struct {
	struct hlist_head active_list[ACT_NODES_HSIZ];
	struct hlist_head free_list;
	spinlock_t lock;
	atomic_t num_handles;
	atomic_t next_handle;
} pa_node_glob;


static struct avm_pa_node *find_active_nolock(const char *name)
{
	struct avm_pa_node *e;
	int i;

	for (i = 0; i < ACT_NODES_HSIZ; i++) {
		hlist_for_each_entry(e, &pa_node_glob.active_list[i], list) {
			if (!strcmp(e->name, name))
				return e;
		}
	}

	return NULL;
}

struct avm_pa_node *avm_pa_node_lookup(const char *name)
{
	struct avm_pa_node *node;

	if (!name)
		return NULL;

	spin_lock_bh(&pa_node_glob.lock);
	node = find_active_nolock(name);
	spin_unlock_bh(&pa_node_glob.lock);

	return node;
}

static struct avm_pa_node *find_active_handle_nolock(avm_pa_node_handle handle)
{
	struct avm_pa_node *e;
	int n;

	n = handle % ACT_NODES_HSIZ;
	hlist_for_each_entry(e, &pa_node_glob.active_list[n], list) {
		if (e->handle == handle)
			return e;
	}

	return NULL;
}

struct avm_pa_node *avm_pa_node_lookup_handle(avm_pa_node_handle handle)
{
	struct avm_pa_node *node;

	if (!handle)
		return NULL;

	spin_lock_bh(&pa_node_glob.lock);
	node = find_active_handle_nolock(handle);
	spin_unlock_bh(&pa_node_glob.lock);

	return node;
}

static struct avm_pa_node *find_free_nolock(const char *name)
{
	struct avm_pa_node *e;

	hlist_for_each_entry(e, &pa_node_glob.free_list, list) {
		if (!strcmp(e->name, name))
			return e;
	}

	return NULL;
}

int avm_pa_node_register(const char *name, struct avm_pa_node_cfg *cfg, gfp_t gfp_mask)
{
	struct avm_pa_node *node;

	spin_lock_bh(&pa_node_glob.lock);
	node = find_active_nolock(name);
	if (node) {
		pr_err("avm_pa_node %s already registered\n", name);
		spin_unlock_bh(&pa_node_glob.lock);
		return -EEXIST;
	}
	node = find_free_nolock(name);
	if (node)
		hlist_del(&node->list);
	spin_unlock_bh(&pa_node_glob.lock);

	if (!node) {
		/* If not found on the free list we allocate a fresh node.
		 *
		 * Only newly allocated nodes get a new handle. Previously allocated
		 * nodes are returned to the free list and retain their handle (and
		 * name for lookup).
		 */
		int handle;

		/* This places an upper bound on the number of nodes that may exist */
		handle = atomic_fetch_add_unless(&pa_node_glob.next_handle, 1, AVM_PA_MAX_NODE);
		if (handle == AVM_PA_MAX_NODE) {
			return -EOVERFLOW;
		}

		node = kzalloc(sizeof(struct avm_pa_node), gfp_mask);
		if (!node)
			return -ENOMEM;

		node->handle = handle;
		strlcpy(node->name, name, sizeof(node->name));
	}

	node->cfg = *cfg;

	spin_lock_bh(&pa_node_glob.lock);
	hlist_add_head(&node->list, &pa_node_glob.active_list[node->handle%ACT_NODES_HSIZ]);
	atomic_inc(&pa_node_glob.num_handles);
	spin_unlock_bh(&pa_node_glob.lock);

	return node->handle;
}

void avm_pa_node_unregister(avm_pa_node_handle handle)
{
	struct avm_pa_node *node;

	if (!handle)
		return;

	spin_lock_bh(&pa_node_glob.lock);
	node = find_active_handle_nolock(handle);
	if (node) {
		__hlist_del(&node->list);
		hlist_add_head(&node->list, &pa_node_glob.free_list);
		atomic_dec(&pa_node_glob.num_handles);
	} else {
		pr_err("avm_pa_node_handle %d not registered\n", handle);
	}
	spin_unlock_bh(&pa_node_glob.lock);
}


static inline avm_pa_node_handle *last_slot(struct avm_pa_pkt_info *info)
{
	int i;
	const int max = ARRAY_SIZE(info->slow_path);

	for (i = 0; i < max; i++) {
		if (info->slow_path[i] == 0)
			break;
	}

	return &info->slow_path[i > 0 ? i-1 : 0];
}

avm_pa_node_handle avm_pa_path_get_last_node(struct avm_pa_pkt_info *info)
{
	return *last_slot(info);
}

int avm_pa_path_add_node(struct avm_pa_pkt_info *info, avm_pa_node_handle handle)
{
	avm_pa_node_handle *start, *end, *last;

	start = &info->slow_path[0];
	end = &info->slow_path[ARRAY_SIZE(info->slow_path) - 1];
	last = last_slot(info);

	if (*last == 0 && last == start) {
		/* empty path, use slot */
		*last = handle;
		return 0;
	} else if (*last && last == end) {
		/* last slot and occupied, no more space */
		return -ENOSPC;
	}
	/* common case: some node is already in the path and there's room for more */
	*(++last) = handle;
	return last - start;
}

int avm_pa_path_copy(struct avm_pa_pkt_info *info, avm_pa_node_handle *handles, size_t max_handles)
{
	avm_pa_node_handle *first;
	int i;

	/* enough space is mandatory */
	BUG_ON(max_handles < ARRAY_SIZE(info->slow_path));

	first = &info->slow_path[0];

	for (i = 0; i < ARRAY_SIZE(info->slow_path); i++) {
		if (!first[i])
			break;
	}

	memcpy(handles, first, i * sizeof(*first));

	return i;
}

int avm_pa_path_calc_min_mtu(avm_pa_node_handle *path, size_t num_handles,
                             int ipversion, u32 mask, u16 *mtu)
{
	avm_pa_node_handle handle;
	struct avm_pa_node *node;
	u16 mtu4, mtu6;
	int i;

	BUG_ON(ipversion != 4 && ipversion != 6);

	spin_lock_bh(&pa_node_glob.lock);
	mtu4 = mtu6 = 0xffff;
	for (i = 0; i < num_handles; i++) {
		if (((1<<i) & mask) == 0)
			continue; /* skip masked nodes */
		handle = path[i];
		if (handle == 0)
			break;
		node = find_active_handle_nolock(handle);
		if (!node)
			continue;
		if (node->cfg.v4_mtu < mtu4)
			mtu4 = node->cfg.v4_mtu;
		if (node->cfg.v6_mtu < mtu6)
			mtu6 = node->cfg.v6_mtu;
	}
	spin_unlock_bh(&pa_node_glob.lock);

	if (ipversion == 4)
		*mtu = mtu4;
	else if (ipversion == 6)
		*mtu = mtu6;

	return 0;
}

int avm_pa_path_format(avm_pa_node_handle *path, size_t max_handles,
                       char *buf, size_t bufsize)
{
	struct avm_pa_node *node;
	int i, err;

	spin_lock_bh(&pa_node_glob.lock);
	buf[0] = 0;
	err = -ENOENT;
	for (i = 0; i < max_handles && path[i]; i++) {
		if (i)
			strlcat(buf, " > ", bufsize);
		node = find_active_handle_nolock(path[i]);
		err = strlcat(buf, node ? node->name : "???", bufsize);
		if (err >= bufsize) {
			err = -ENOSPC;
			break;
		}
	}
	spin_unlock_bh(&pa_node_glob.lock);

	return err ?: 0;

}

#ifdef CONFIG_PROC_FS

static int node_cmp(const void *p1, const void *p2)
{
	int h1 = (int) ((*(struct avm_pa_node **)p1)->handle);
	int h2 = (int) ((*(struct avm_pa_node **)p2)->handle);

	return h1 - h2;
}

static int pa_show_nodes(pa_fprintf pr, void *arg)
{
	struct avm_pa_node *e, **tmp;
	int i, j, n;

retry:
	n = atomic_read(&pa_node_glob.num_handles);
	tmp = kcalloc(n, sizeof(struct avm_pa_node *), GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	spin_lock_bh(&pa_node_glob.lock);
	if (unlikely(atomic_read(&pa_node_glob.num_handles) != n)) {
		spin_unlock_bh(&pa_node_glob.lock);
		kfree(tmp);
		goto retry; /* handles changed, retry */
	}
	j = 0;
	for (i = 0; i < ACT_NODES_HSIZ; i++) {
		hlist_for_each_entry(e, &pa_node_glob.active_list[i], list)
			tmp[j++] = e;
	}
	sort(tmp, n, sizeof(struct avm_pa_node *), node_cmp, NULL);
	pr(arg, "Active nodes:\n");
	for (j = 0; j < n && (e = tmp[j]); j++) {
		pr(arg, "Node %2d: %16s v4_mtu %4d v6_mtu %4d\n",
		   e->handle, e->name, e->cfg.v4_mtu, e->cfg.v6_mtu);
	}
	kfree(tmp);

	pr(arg, "Free nodes:\n");
	e = hlist_entry(pa_node_glob.free_list.first, struct avm_pa_node, list);
	if (e) {
		pr(arg, "%s", e->name);
		hlist_for_each_entry_continue(e, list)
			pr(arg, ", %s", e->name);
		pr(arg, "\n");
	}
	spin_unlock_bh(&pa_node_glob.lock);

	return 0;
}

static int nodes_show(struct seq_file *m, void *v)
{
	return pa_show_nodes((pa_fprintf *)seq_printf, m);
}

static int nodes_show_open(struct inode *inode, struct file *file)
{
	return single_open(file, nodes_show, PDE_DATA(inode));
}

static const struct proc_ops nodes_show_ops = {
	.proc_open    = nodes_show_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release, /* nodes_show_open() uses single_open() */
};

void avm_pa_path_proc_init(struct proc_dir_entry *dir_entry)
{
	proc_create("nodes", 0444, dir_entry, &nodes_show_ops);
}

void avm_pa_path_proc_exit(struct proc_dir_entry *dir_entry)
{
	remove_proc_entry("nodes", dir_entry);
}

#endif

int __init avm_pa_path_init(void)
{
	int i;

	for (i = 0; i < ACT_NODES_HSIZ; i++)
		INIT_HLIST_HEAD(&pa_node_glob.active_list[i]);
	INIT_HLIST_HEAD(&pa_node_glob.free_list);
	spin_lock_init(&pa_node_glob.lock);
	atomic_set(&pa_node_glob.num_handles, 0);
	atomic_set(&pa_node_glob.next_handle, 1);

	return 0;
}
