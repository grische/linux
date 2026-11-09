/*
 *	Definitions for the generic connection tracking handling
 *
 *	vim:set noexpandtab shiftwidth=8 textwidth=100
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_AVM_PA_GENERIC_CT_H
#define _LINUX_AVM_PA_GENERIC_CT_H

enum generic_ct_dir {
	GENERIC_CT_DIR_ORIGINAL = 0,    // AVM PA original
	GENERIC_CT_DIR_REPLY = 1,       // AVM PA reply
#if IS_ENABLED(CONFIG_AVM_PA_GENERIC_CT_PP_SUPPORT)
	GENERIC_CT_DIR_ORIGINAL_PP,     // Vendor PA (e.g. Puma7 PP) original
	GENERIC_CT_DIR_REPLY_PP,        // Vendor PA (e.g. Puma7 PP) reply
#endif
	GENERIC_CT_DIR_MAX
};

enum generic_ct_exceptions {
	GENERIC_CT_NO_EXCEPTION,
	GENERIC_CT_COUNTER_EXCEPTION,
	GENERIC_CT_NUM_EXCEPTIONS,
};

#if IS_ENABLED(CONFIG_AVM_PA_GENERIC_CT_PP_SUPPORT)
#define GENERIC_CT_DIR_PP_OFFSET GENERIC_CT_DIR_ORIGINAL_PP
#endif

struct generic_ct_ops;

struct generic_ct {
	void                    *ct;
	struct generic_ct_ops   *ops;
	atomic_t                 use;
};

struct generic_ct_data {
	enum generic_ct_dir      generic_ct_dir;
	struct generic_ct       *generic_ct;
};

extern void generic_ct_init(void);
extern struct generic_ct *generic_ct_create(void *ct,
					    struct generic_ct_ops *ops,
					    gfp_t gfp_mask);
extern void generic_ct_destroy(struct generic_ct *ct);
int generic_ct_raise_exception(struct generic_ct *ct, enum generic_ct_exceptions e);

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

#endif /* _LINUX_AVM_PA_GENERIC_CT_H */
