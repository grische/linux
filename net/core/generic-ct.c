/*
 *	Routines to support generic conntrack
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
#include <linux/generic-ct.h>

static struct kmem_cache *generic_ct_cache __read_mostly;

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
