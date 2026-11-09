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
	/* set session id, NULL means: clear session id */
	void (*sessionid_set)(struct generic_ct *ct,
			      enum generic_ct_dir,
			      void *vsessid);
	/* get session id, NULL means: no session id */
	void *(*sessionid_get)(struct generic_ct *ct,
			       enum generic_ct_dir);
};

/*
 * wrapper for ops
 */
static inline void generic_ct_sessionid_set(struct generic_ct *ct,
					    enum generic_ct_dir dir,
					    void *sessid)
{
	if (ct->ops && ct->ops->sessionid_set)
		(*ct->ops->sessionid_set)(ct, dir, sessid);
}

static inline void *generic_ct_sessionid_get(struct generic_ct *ct,
					     enum generic_ct_dir dir)
{
	if (ct->ops && ct->ops->sessionid_get)
		return (*ct->ops->sessionid_get)(ct, dir);
	return 0;
}

#endif /* _LINUX_GENERIC_CT_OPS_H */
