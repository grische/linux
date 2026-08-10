/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Intel Corporation Author: Shao Guohua <guohua.shao@intel.com>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * include/net/datapath_api_skb.h.
 *
 * Datapath socket-buffer helper types.
 */

#ifndef DATAPATH_API_SKB_H
#define DATAPATH_API_SKB_H

#include <linux/types.h>
#include <linux/skbuff.h>

/* Wrapper carrying datapath-specific metadata attached to an skb. */
struct ltq_dp_skb {
	int _ltq_dp_skb_placeholder; /* avoid empty-struct UB warnings */
};

static inline void dp_skb_cp(const struct ltq_dp_skb *old,
			     struct ltq_dp_skb *new)
{
	(void)old;
	(void)new;
}

static inline void dp_skb_free(struct sk_buff *skb)
{
	(void)skb;
}

#endif /* DATAPATH_API_SKB_H */
