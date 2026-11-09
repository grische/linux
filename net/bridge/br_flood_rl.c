/*
 * Linux ethernet bridge, flood rate limiting
 *
 * Copyright (c) 2016 AVM GmbH (www.avm.de)
 * Author: Thomas Martitz (t.martitz@avm.de)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter_bridge.h>
#include <linux/etherdevice.h>
#include <linux/atomic.h>
#include <linux/version.h>

#include "br_private.h"

#define RECOVER_TICK (HZ / 4)

/* Simple, credit based rate limiter
 *
 * Works like this, as an example:
 * - packet rate capped to 1000
 *   (via /sys/devices/virtual/net/<if>/bridge/avm_flood_ratelimit_recover_rate)
 * - max. packet credit set to 2000
 *   (via /sys/devices/virtual/net/<if>/bridge/avm_flood_ratelimit)
 *
 * Once a packet is forwarded on the flood path, one credit is taken away and
 * the recovery timer is started. After HZ/4 the credit is balanced again.
 *
 * Now if a Packet storm commences (caused by loop): A single packet is
 * forwarded one one port (starting the recovery timer) and immediately
 * received on another bridge port, and forwarded again (and so on). Each
 * received packet takes one credit but the packet rate is too high for the
 * recovery timer to balance the credit. Once the credit reaches zero, the
 * packet is simly dropped which immediately stops the packet storm.
 *
 * For example, if the packet rate during the storm is 2 packet/ms:
 * The first packet starts the recovery timer. The balance after each
 * timer interval:
 * 0.00s: ~2000
 * 0.25s: ~1500 + 250
 * 0.50s: ~1250 + 250
 * 0.75s: ~1000 + 250
 * 1.00s:  ~750 + 250
 * 1.25s:  ~500 + 250
 * 1.50s:  ~250 + 250
 * 1.75s:    ~0 + 250
 * 2.00s:    ~0 + 250
 *
 * Once the credit becomes zero, the effective packet rate is equal to the
 * recovery rate. After the storm ends, it takes ~8s to fully balance the
 * packet credits.
 */

static void br_flood_rl_timer(unsigned long data)
{
	struct net_bridge *br = (struct net_bridge *) data;

	int curr = atomic_read(&br->avm_flood_credits);
	int diff = br->avm_flood_credits_max - curr;
	int rate = br->avm_flood_credits_recov_rate * RECOVER_TICK / HZ;

	/* Reaching the maximum suspends the timer */
	if (diff > rate)
		mod_timer(&br->avm_flood_rl_timer, jiffies + RECOVER_TICK);

	atomic_add(min(rate, diff), &br->avm_flood_credits);
}

unsigned int br_flood_rl(struct net_bridge *br,
			 struct sk_buff *skb,
			 struct net_device *src_dev)
{
	int flood;

	/* rate limiter disabled? */
	if (br->avm_flood_credits_max == 0)
		return 1;

	/* Packets are dropped once all credits are consumed
	 * but the timer must be running regardless.
	 */
	flood = atomic_dec_if_positive(&br->avm_flood_credits) > 0;

	if (!timer_pending(&br->avm_flood_rl_timer))
		mod_timer(&br->avm_flood_rl_timer, jiffies + RECOVER_TICK);

	return flood ? 1 : 0;
}

static int br_flood_rl_dev_event(struct notifier_block *nb,
				 unsigned long event,
				 void *netdev)
{
	struct net_bridge *br;
	struct net_device *dev;

	/* not a bridge device? */
	dev = netdev_notifier_info_to_dev((const struct netdev_notifier_info *)netdev);
	if ((dev->priv_flags & IFF_EBRIDGE) == 0)
		return NOTIFY_DONE;

	br = netdev_priv(dev);

	/* Don't run timers for down bridges */
	switch (event) {
	case NETDEV_UP:
		mod_timer(&br->avm_flood_rl_timer, jiffies + RECOVER_TICK);
	break;
	case NETDEV_DOWN:
		del_timer_sync(&br->avm_flood_rl_timer);
		atomic_set(&br->avm_flood_credits, br->avm_flood_credits_max);
	break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block br_flood_rl_netdev_notifier = {
	.notifier_call = br_flood_rl_dev_event,
};

int br_flood_rl_set_credits(struct net_bridge *br, unsigned long v)
{
	if (v > INT_MAX)
		return -EINVAL;

	del_timer_sync(&br->avm_flood_rl_timer);
	/* The maximum can be higher than the recovery rate. This allows for
	 * higher, initial bursts. Zero disables the rate limiter.
	 */
	br->avm_flood_credits_max = v;

	if (v)
		mod_timer(&br->avm_flood_rl_timer, jiffies + RECOVER_TICK);

	br_notice(br, "Bridge flood ratelimiter new credits %d\n", (int) v);
	return 0;
}

void br_flood_rl_setup(struct net_bridge *br)
{
	unsigned long data = (unsigned long) br;

	setup_timer(&br->avm_flood_rl_timer, br_flood_rl_timer, data);
}

int __init br_flood_rl_init(void)
{
	int ret;

	ret = register_netdevice_notifier(&br_flood_rl_netdev_notifier);

	pr_notice("Bridge flood ratelimiter registered\n");
	return 0;
}

void br_flood_rl_fini(void)
{
	unregister_netdevice_notifier(&br_flood_rl_netdev_notifier);
}
