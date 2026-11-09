/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for the generic connection tracking event handlers
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_AVM_PA_GENERIC_CT_EVENTS_H
#define _LINUX_AVM_PA_GENERIC_CT_EVENTS_H

struct generic_ct_event_ops {
	struct list_head list;
	int (*handle_exception)(struct generic_ct_event_ops *ops,
				 struct generic_ct *ct,
				 enum generic_ct_exceptions exception);
};

int generic_ct_register_event_ops(struct generic_ct_event_ops *ops);
void generic_ct_deregister_event_ops(struct generic_ct_event_ops *ops);

#endif /* _LINUX_AVM_PA_GENERIC_CT_EVENTS_H */
