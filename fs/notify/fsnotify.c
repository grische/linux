/*
 *  Copyright (C) 2008 Red Hat, Inc., Eric Paris <eparis@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/srcu.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

#ifndef CONFIG_TRACE_FSNOTIFY_EVENTS
#  define NOTRACE
#endif
#define CREATE_TRACE_POINTS
#include "trace-fsnotify.h"

/*
 * Clear all of the marks on an inode when it is being evicted from core
 */
void __fsnotify_inode_delete(struct inode *inode)
{
	fsnotify_clear_marks_by_inode(inode);
}
EXPORT_SYMBOL_GPL(__fsnotify_inode_delete);

void __fsnotify_vfsmount_delete(struct vfsmount *mnt)
{
	fsnotify_clear_marks_by_mount(mnt);
}

#if defined(CONFIG_FSNOTIFY_RECURSIVE)
static void __fsnotify_update_child_dentry_flags_recursivly(struct dentry *dentry)
{
	struct dentry *child;
	int watched_recursivly = 0;

	if (dentry->d_flags & DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY)
		watched_recursivly = 1;

	list_for_each_entry(child, &dentry->d_subdirs, d_child) {
		if (!child->d_inode)
			continue;

		spin_lock_nested(&child->d_lock, DENTRY_D_LOCK_NESTED);
		if (watched_recursivly)
			child->d_flags |= DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY;
		else
			child->d_flags &= ~DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY;

		__fsnotify_update_child_dentry_flags_recursivly(child);
		spin_unlock(&child->d_lock);
	}
}
#endif /* CONFIG_FSNOTIFY_RECURSIVE */

/*
 * Given an inode, first check if we care what happens to our children.  Inotify
 * and dnotify both tell their parents about events.  If we care about any event
 * on a child we run all of our children and set a dentry flag saying that the
 * parent cares.  Thus when an event happens on a child it can quickly tell if
 * if there is a need to find a parent and send the event to the parent.
 */
void __fsnotify_update_child_dentry_flags(struct inode *inode)
{
	struct dentry *alias;
	int watched;
#if defined(CONFIG_FSNOTIFY_RECURSIVE)
	int watched_recursivly;
#endif /* CONFIG_FSNOTIFY_RECURSIVE */

	if (!S_ISDIR(inode->i_mode))
		return;

	/* determine if the children should tell inode about their events */
	watched = fsnotify_inode_watches_children(inode);
#if defined(CONFIG_FSNOTIFY_RECURSIVE)
	watched_recursivly = fsnotify_inode_watches_children_recursivly(inode);
#endif /* CONFIG_FSNOTIFY_RECURSIVE */

	spin_lock(&inode->i_lock);
	/* run all of the dentries associated with this inode.  Since this is a
	 * directory, there damn well better only be one item on this list */
	hlist_for_each_entry(alias, &inode->i_dentry, d_u.d_alias) {
		struct dentry *child;

#if defined(CONFIG_FSNOTIFY_RECURSIVE)
		if (alias->d_flags & DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY)
			watched_recursivly = 1;
#endif /* CONFIG_FSNOTIFY_RECURSIVE */

		/* run all of the children of the original inode and fix their
		 * d_flags to indicate parental interest (their parent is the
		 * original inode) */
		spin_lock(&alias->d_lock);
		list_for_each_entry(child, &alias->d_subdirs, d_child) {
			if (!child->d_inode)
				continue;

			spin_lock_nested(&child->d_lock, DENTRY_D_LOCK_NESTED);
			if (watched)
				child->d_flags |= DCACHE_FSNOTIFY_PARENT_WATCHED;
			else
				child->d_flags &= ~DCACHE_FSNOTIFY_PARENT_WATCHED;

#if defined(CONFIG_FSNOTIFY_RECURSIVE)
			if (watched_recursivly)
				child->d_flags |= DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY;
			else
				child->d_flags &= ~DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY;

			__fsnotify_update_child_dentry_flags_recursivly(child);
#endif /* CONFIG_FSNOTIFY_RECURSIVE */
			spin_unlock(&child->d_lock);
		}
		spin_unlock(&alias->d_lock);
	}
	spin_unlock(&inode->i_lock);
}

/* Notify this dentry's parent about a child's events. */
int __fsnotify_parent(struct path *path, struct dentry *dentry, __u32 mask)
{
	struct dentry *parent;
	struct inode *p_inode;
	int ret = 0;

	if (!dentry)
		dentry = path->dentry;

	if (!(dentry->d_flags & DCACHE_FSNOTIFY_PARENT_WATCHED))
		return 0;

	parent = dget_parent(dentry);
	p_inode = parent->d_inode;

	if (unlikely(!fsnotify_inode_watches_children(p_inode))) {
		__fsnotify_update_child_dentry_flags(p_inode);
	} else if (p_inode->i_fsnotify_mask & mask & ~FS_EVENT_ON_CHILD) {
		struct name_snapshot name;

		/* we are notifying a parent so come up with the new mask which
		 * specifies these are events which came from a child. */
		mask |= FS_EVENT_ON_CHILD;

		take_dentry_name_snapshot(&name, dentry);
		if (path)
			ret = fsnotify(p_inode, mask, path, FSNOTIFY_EVENT_PATH,
				       name.name, 0);
		else
			ret = fsnotify(p_inode, mask, dentry->d_inode, FSNOTIFY_EVENT_INODE,
				       name.name, 0);
		release_dentry_name_snapshot(&name);
	}

	dput(parent);

	return ret;
}
EXPORT_SYMBOL_GPL(__fsnotify_parent);

#if defined(CONFIG_FSNOTIFY_RECURSIVE)
/**
 * Formats a relative path beginning at an entry with the given name up to top.
 *
 * @param parent The parent of the name
 * @param root   The parent dentry from which to start the path
 * @param name   The name of the last component
 * @return       A kmalloc'd string representing the path
 */
static char *alloc_relative_path(struct dentry *root,
				 struct dentry *parent,
				 const char *name)
{
	size_t size, name_size;
	struct dentry *d;
	int seq = 0;
	char *buf;

	WARN_ON_ONCE(!rcu_read_lock_held());
	name_size = strlen(name) + 1;
again:
	size = name_size;
	read_seqbegin_or_lock(&rename_lock, &seq);

	/*
	 * First we go upwards the tree to calculate the needed size.
	 * __getname is supposedly faster, but wastes memory.
	 */
	for (d = parent;
			d != root && !IS_ROOT(d);
			d = ACCESS_ONCE(d->d_parent))
		size += ACCESS_ONCE(d->d_name.len) + sizeof("/")-1;

	buf = kmalloc(size, GFP_ATOMIC);
	if (!buf) {
		done_seqretry(&rename_lock, seq);
		return ERR_PTR(-ENOMEM);
	}

	size -= name_size;
	memcpy(&buf[size], name, name_size);

	/*
	 * Traverse the path upwards again, building the path.
	 */
	for (d = parent;
			d != root && !IS_ROOT(d);
			d = ACCESS_ONCE(d->d_parent)) {
		unsigned int dlen = ACCESS_ONCE(d->d_name.len);
		const char *dname = ACCESS_ONCE(d->d_name.name);

		/*
		 * Dentry names may change anytime.  Here, just make sure we
		 * don't overflow our buffer.  Any inconsistencies will be
		 * caught by the seq check below.
		 */
		if (dlen + sizeof("/")-1 > size)
			break;

		buf[--size] = '/';
		size -= dlen;
		memcpy(&buf[size], dname, dlen);
	}

	/*
	 * If no renames happened inbetween, we're done.  Else, redo with lock.
	 */
	if (need_seqretry(&rename_lock, seq)) {
		kfree(buf);
		seq = 1;
		goto again;
	}
	done_seqretry(&rename_lock, seq);

	return buf;
}

/**
 * Find an ancestor dentry that is watching its children recursively for
 * selected events.
 *
 * @param dentry The dentry for which to find an ancestor
 * @param mask   The event bitmask in question
 * @return       An interested ancestor dentry, with a reference held, or
 *               NULL if there are none.
 */
static struct dentry *__dget_watching_ancestor(struct dentry *dentry, __u32 mask)
{
	WARN_ON_ONCE(!rcu_read_lock_held());

	while (!IS_ROOT(dentry)) {
		struct inode *inode;

		dentry = ACCESS_ONCE(dentry->d_parent);
		inode  = ACCESS_ONCE(dentry->d_inode);

		if (inode && fsnotify_inode_watches_children_recursivly(inode) &&
				    (inode->i_fsnotify_mask & mask))
			return dget(dentry);

		if (!(dentry->d_flags & DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY))
			break;
	}

	return NULL;
}

/**
 * Notifies the ancestor interested.
 *
 * Walks the dentry tree upwards to find an interested ancestor and forwards the
 * event to it.
 *
 * DOES NOT cross fs boundaries
 * DOES ONLY notify the first interested ancestor dentry that is found
 * The immediate parent is not notified here, but by the standard
 * fsnotify_parent().
 *
 * @param path   path for the event
 * @param dentry the dentry for the event
 * @param parent the parent of dentry, if it is different because dentry has been moved
 * @param name   the name of dentry, if it is different because dentry has been renamed
 * @param mask   the event mask to pass
 * @param cookie the cookie to pass
 * @return 0 on success, !=0 otherwise
 */
int __fsnotify_parent_recursive(struct path *path,
				struct dentry *dentry,
				struct dentry *parent,
				const char *name,
				__u32 mask,
				__u32 cookie)
{
	struct name_snapshot d_name;
	struct dentry *d_to_tell;
	struct inode *to_tell;
	char *rel_path;
	int ret = 0;

	if (!dentry)
		dentry = path->dentry;

	/*
	 * No one above us in the hierarchy is interested in our events
	 */
	if (!(dentry->d_flags & DCACHE_FSNOTIFY_PARENT_WATCHED_RECURSIVLY))
		return 0;

	/*
	 * The event is not allowed to propagate upwards
	 */
	if (!(mask & FS_EVENTS_POSS_ON_CHILD_RECURSIVLY))
		return 0;

	/*
	 * If there is an ancestor watching recursively, we try to construct
	 * the path from that ancestor down to the dentry that generated the
	 * event.
	 *
	 * This is far from being bullet-proof.  For example, since the dentry
	 * chain can be changed anytime by concurrent renames, traversing it
	 * need not yield reproducible results.  We might fail to find a
	 * watching ancestor, because the chain got destroyed before we came
	 * to traverse it.  In this case we lose events.
	 *
	 * Also, no critical section ties the fsnotify hooks to the events
	 * that triggered them, so things can get mixed up.
	 *
	 * Also, maintaining the d_flags in a sub-tree is prone to races.
	 *
	 * But if there's not too much activity on the directory tree,
	 * results should be good enough.  Just don't count on it. ;)
	 */

	/*
	 * Protect against dentries perishing beneath us.
	 */
	rcu_read_lock();

	if (!parent)
		parent = ACCESS_ONCE(dentry->d_parent);

	/*
	 * Traverse upwards, looking for a watching ancestor.  This skips the
	 * parent, which is already notified non-recursively.
	 */
	d_to_tell = __dget_watching_ancestor(parent, mask);
	if (!d_to_tell) {
		rcu_read_unlock();
		return 0;
	}

	/*
	 * Try to construct the path.  This traverses the path again.
	 */
	if (!name) {
		take_dentry_name_snapshot(&d_name, dentry);
		name = d_name.name;
	}
	rel_path = alloc_relative_path(d_to_tell, parent, name);
	if (IS_ERR(rel_path)) {
		ret = PTR_ERR(rel_path);
		rcu_read_unlock();
		goto err_dput;
	}

	/*
	 * If the existing chain has been modified inbetween, the path might
	 * not end up at the watching dentry, but at the root.  This cannot be
	 * repaired without taking the global rename_lock.  Assume that we got
	 * it right if there is still a linking chain.  Otherwise, drop the
	 * event, since we lack useful data to report.
	 */
	if (!d_ancestor(d_to_tell, parent)) {
		ret = 0;
		rcu_read_unlock();
		goto err_free;
	}

	rcu_read_unlock();

	to_tell = ACCESS_ONCE(d_to_tell->d_inode);

	/*
	 * We are notifying a parent so come up with the new mask which
	 * specifies these are events which came from a child.
	 */
	mask |= FS_EVENT_ON_CHILD_RECURSIVLY;

	if (path)
		ret = fsnotify(to_tell, mask, path,
				FSNOTIFY_EVENT_PATH, rel_path, cookie);
	else
		ret = fsnotify(to_tell, mask, dentry->d_inode,
				FSNOTIFY_EVENT_INODE, rel_path, cookie);
err_free:
	kfree(rel_path);
err_dput:
	dput(d_to_tell);

	/*
	 * Should we repeat, looking for more watchers upwards the path?
	 * This would require dgetting the parent, since we drop rcu
	 * when calling fsnotify().
	 */

	if (name == d_name.name)
		release_dentry_name_snapshot(&d_name);

	return ret;
}
EXPORT_SYMBOL_GPL(__fsnotify_parent_recursive);
#endif /* CONFIG_FSNOTIFY_RECURSIVE */

static int send_to_group(struct inode *to_tell,
			 struct fsnotify_mark *inode_mark,
			 struct fsnotify_mark *vfsmount_mark,
			 __u32 mask, void *data,
			 int data_is, u32 cookie,
			 const unsigned char *file_name)
{
	struct fsnotify_group *group = NULL;
	__u32 inode_test_mask = 0;
	__u32 vfsmount_test_mask = 0;

	if (unlikely(!inode_mark && !vfsmount_mark)) {
		BUG();
		return 0;
	}

	/* clear ignored on inode modification */
	if (mask & FS_MODIFY) {
		if (inode_mark &&
		    !(inode_mark->flags & FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY))
			inode_mark->ignored_mask = 0;
		if (vfsmount_mark &&
		    !(vfsmount_mark->flags & FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY))
			vfsmount_mark->ignored_mask = 0;
	}

	/* does the inode mark tell us to do something? */
	if (inode_mark) {
		group = inode_mark->group;
		inode_test_mask = (mask & ~FS_EVENT_ON_CHILD);
		inode_test_mask &= inode_mark->mask;
		inode_test_mask &= ~inode_mark->ignored_mask;
	}

	/* does the vfsmount_mark tell us to do something? */
	if (vfsmount_mark) {
		vfsmount_test_mask = (mask & ~FS_EVENT_ON_CHILD);
		group = vfsmount_mark->group;
		vfsmount_test_mask &= vfsmount_mark->mask;
		vfsmount_test_mask &= ~vfsmount_mark->ignored_mask;
		if (inode_mark)
			vfsmount_test_mask &= ~inode_mark->ignored_mask;
	}

	pr_debug("%s: group=%p to_tell=%p mask=%x inode_mark=%p"
		 " inode_test_mask=%x vfsmount_mark=%p vfsmount_test_mask=%x"
		 " data=%p data_is=%d cookie=%d\n",
		 __func__, group, to_tell, mask, inode_mark,
		 inode_test_mask, vfsmount_mark, vfsmount_test_mask, data,
		 data_is, cookie);

	if (!inode_test_mask && !vfsmount_test_mask)
		return 0;

	return group->ops->handle_event(group, to_tell, inode_mark,
					vfsmount_mark, mask, data, data_is,
					file_name, cookie);
}

/*
 * This is the main call to fsnotify.  The VFS calls into hook specific functions
 * in linux/fsnotify.h.  Those functions then in turn call here.  Here will call
 * out to all of the registered fsnotify_group.  Those groups can then use the
 * notification event in whatever means they feel necessary.
 */
int fsnotify(struct inode *to_tell, __u32 mask, void *data, int data_is,
	     const unsigned char *file_name, u32 cookie)
{
	struct hlist_node *inode_node = NULL, *vfsmount_node = NULL;
	struct fsnotify_mark *inode_mark = NULL, *vfsmount_mark = NULL;
	struct fsnotify_group *inode_group, *vfsmount_group;
	struct mount *mnt;
	int idx, ret = 0;
	/* global tests shouldn't care about events on child only the specific event */
	__u32 test_mask = (mask & ~FS_EVENT_ON_CHILD);

	if (data_is == FSNOTIFY_EVENT_PATH)
		mnt = real_mount(((struct path *)data)->mnt);
	else
		mnt = NULL;

	/* An event "on child" is not intended for a mount mark */
	if (mask & FS_EVENT_ON_CHILD)
		mnt = NULL;

	/*
	 * Optimization: srcu_read_lock() has a memory barrier which can
	 * be expensive.  It protects walking the *_fsnotify_marks lists.
	 * However, if we do not walk the lists, we do not have to do
	 * SRCU because we have no references to any objects and do not
	 * need SRCU to keep them "alive".
	 */
	if (hlist_empty(&to_tell->i_fsnotify_marks) &&
	    (!mnt || hlist_empty(&mnt->mnt_fsnotify_marks)))
		return 0;
	/*
	 * if this is a modify event we may need to clear the ignored masks
	 * otherwise return if neither the inode nor the vfsmount care about
	 * this type of event.
	 */
	if (!(mask & FS_MODIFY) &&
	    !(test_mask & to_tell->i_fsnotify_mask) &&
	    !(mnt && test_mask & mnt->mnt_fsnotify_mask))
		return 0;

	trace_fsnotify_event(to_tell, mask, file_name);

	idx = srcu_read_lock(&fsnotify_mark_srcu);

	if ((mask & FS_MODIFY) ||
	    (test_mask & to_tell->i_fsnotify_mask))
		inode_node = srcu_dereference(to_tell->i_fsnotify_marks.first,
					      &fsnotify_mark_srcu);

	if (mnt && ((mask & FS_MODIFY) ||
		    (test_mask & mnt->mnt_fsnotify_mask))) {
		vfsmount_node = srcu_dereference(mnt->mnt_fsnotify_marks.first,
						 &fsnotify_mark_srcu);
		inode_node = srcu_dereference(to_tell->i_fsnotify_marks.first,
					      &fsnotify_mark_srcu);
	}

	/*
	 * We need to merge inode & vfsmount mark lists so that inode mark
	 * ignore masks are properly reflected for mount mark notifications.
	 * That's why this traversal is so complicated...
	 */
	while (inode_node || vfsmount_node) {
		inode_group = NULL;
		inode_mark = NULL;
		vfsmount_group = NULL;
		vfsmount_mark = NULL;

		if (inode_node) {
			inode_mark = hlist_entry(srcu_dereference(inode_node, &fsnotify_mark_srcu),
						 struct fsnotify_mark, obj_list);
			inode_group = inode_mark->group;
		}

		if (vfsmount_node) {
			vfsmount_mark = hlist_entry(srcu_dereference(vfsmount_node, &fsnotify_mark_srcu),
						    struct fsnotify_mark, obj_list);
			vfsmount_group = vfsmount_mark->group;
		}

		if (inode_group && vfsmount_group) {
			int cmp = fsnotify_compare_groups(inode_group,
							  vfsmount_group);
			if (cmp > 0) {
				inode_group = NULL;
				inode_mark = NULL;
			} else if (cmp < 0) {
				vfsmount_group = NULL;
				vfsmount_mark = NULL;
			}
		}
		ret = send_to_group(to_tell, inode_mark, vfsmount_mark, mask,
				    data, data_is, cookie, file_name);

		if (ret && (mask & ALL_FSNOTIFY_PERM_EVENTS))
			goto out;

		if (inode_group)
			inode_node = srcu_dereference(inode_node->next,
						      &fsnotify_mark_srcu);
		if (vfsmount_group)
			vfsmount_node = srcu_dereference(vfsmount_node->next,
							 &fsnotify_mark_srcu);
	}
	ret = 0;
out:
	srcu_read_unlock(&fsnotify_mark_srcu, idx);

	return ret;
}
EXPORT_SYMBOL_GPL(fsnotify);

static __init int fsnotify_init(void)
{
	int ret;

	BUG_ON(hweight32(ALL_FSNOTIFY_EVENTS) != 23);

	ret = init_srcu_struct(&fsnotify_mark_srcu);
	if (ret)
		panic("initializing fsnotify_mark_srcu");

	return 0;
}
core_initcall(fsnotify_init);
