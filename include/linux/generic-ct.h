/*
 *	Definitions for the generic connection tracking handling
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_GENERIC_CT_H
#define _LINUX_GENERIC_CT_H

enum generic_ct_dir {
	GENERIC_CT_DIR_ORIGINAL = 0,
	GENERIC_CT_DIR_REPLY = 1,
	GENERIC_CT_DIR_MAX = 2
};

struct generic_ct;

struct generic_ct_ops {
	void (*destroy)(struct generic_ct *);
	/* set session id, NULL means: clear session id */
	void (*sessionid_set)(struct generic_ct *,
			      enum generic_ct_dir,
			      void *);
	/* get session id, NULL means: no session id */
	void *(*sessionid_get)(struct generic_ct *,
			       enum generic_ct_dir);
};

struct generic_ct {
	void                    *ct;
	struct generic_ct_ops   *ops;
	atomic_t                 use;
};

extern void generic_ct_init(void);
extern struct generic_ct *generic_ct_create(void *ct,
					    struct generic_ct_ops *ops,
					    gfp_t gfp_mask);
extern void generic_ct_destroy(struct generic_ct *ct);

static inline void generic_ct_put(struct generic_ct *ct)
{
	if (ct && atomic_dec_and_test(&ct->use))
		generic_ct_destroy(ct);
}

static inline struct generic_ct *generic_ct_get(struct generic_ct *ct)
{
	if (ct)
		atomic_inc(&ct->use);
	return ct;
}

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

#endif /* _LINUX_GENERIC_CT_H */
