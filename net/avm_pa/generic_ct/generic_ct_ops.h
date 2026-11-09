/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for the generic connection tracking handling ops
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_AVM_PA_GENERIC_CT_OPS_H
#define _LINUX_AVM_PA_GENERIC_CT_OPS_H

struct generic_ct_ops {
	void (*destroy)(struct generic_ct *ct);
	void (*pa_session_add)(struct generic_ct *ct,
			       enum generic_ct_dir dir,
			       unsigned long session_id);
	void (*pa_session_del)(struct generic_ct *ct,
			       enum generic_ct_dir dir,
			       unsigned long session_id);
};

/*
 * wrapper for ops
 */
static inline void generic_ct_pa_session_add(struct generic_ct *ct,
					     enum generic_ct_dir dir,
					     unsigned long session_id)
{
	if (ct->ops && ct->ops->pa_session_add)
		(*ct->ops->pa_session_add)(ct, dir, session_id);
}

static inline void generic_ct_pa_session_del(struct generic_ct *ct,
					     enum generic_ct_dir dir,
					     unsigned long session_id)
{
	if (ct->ops && ct->ops->pa_session_del)
		(*ct->ops->pa_session_del)(ct, dir, session_id);
}

#endif /* _LINUX_GENERIC_CT_OPS_H */
