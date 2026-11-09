/*
 *	Routines to support generic conntrack
 *
 *	vim:set noexpandtab shiftwidth=8 textwidth=100
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/rculist.h>

#include "generic_ct.h"
#include "generic_ct_ops.h"
#include "generic_ct_events.h"

static struct kmem_cache *generic_ct_cache __read_mostly;
static LIST_HEAD(handlers);
static DEFINE_SPINLOCK(handlers_lock);

struct generic_ct *
generic_ct_create(void *ct_ptr, struct generic_ct_ops *ops, gfp_t gfp_mask)
{
	struct generic_ct *ct = kmem_cache_alloc(generic_ct_cache, gfp_mask);

	if (!ct)
		return NULL;

	memset(ct, 0, sizeof(struct generic_ct));
	ct->ct = ct_ptr;
	ct->ops = ops;
	return generic_ct_get(ct);
}
EXPORT_SYMBOL(generic_ct_create);

void generic_ct_destroy(struct generic_ct *ct)
{
	if (ct) {
		if (ct->ops && ct->ops->destroy)
			(*ct->ops->destroy)(ct);
		WARN_ON(ct->ct);
		kmem_cache_free(generic_ct_cache, ct);
	}
}
EXPORT_SYMBOL(generic_ct_destroy);

void __init generic_ct_init(void)
{
	generic_ct_cache = kmem_cache_create("generic_ct",
					     sizeof(struct generic_ct),
					     0, SLAB_HWCACHE_ALIGN | SLAB_PANIC,
					     NULL);
}

int generic_ct_raise_exception(struct generic_ct *ct, enum generic_ct_exceptions e)
{
	struct generic_ct_event_ops *ops;
	int handled = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(ops, &handlers, list) {
		if (ops->handle_exception && ops->handle_exception(ops, ct, e))
			handled = 1;
	}
	rcu_read_unlock();

	return handled;
}
EXPORT_SYMBOL(generic_ct_raise_exception);

int generic_ct_register_event_ops(struct generic_ct_event_ops *ops)
{
	spin_lock(&handlers_lock);
	list_add_rcu(&ops->list, &handlers);
	spin_unlock(&handlers_lock);

	return 0;
}

void generic_ct_deregister_event_ops(struct generic_ct_event_ops *ops)
{
	spin_lock(&handlers_lock);
	list_del_rcu(&ops->list);
	spin_unlock(&handlers_lock);

	synchronize_rcu();
}
