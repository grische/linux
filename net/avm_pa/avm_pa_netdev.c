/*
 * AVM packet accelerator - struct net_device support
 *
 * SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100:
 *
 * Copyright (c) 2011-2020 AVM GmbH <info@avm.de>
 * All rights reserved.
 */

#define pr_fmt(fmt) "avm_pa_netdev: " fmt

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>

#include "avm_pa.h"
#include "avm_pa_intern.h"

void avm_pa_dev_queue_xmit(struct net_device *dev, struct sk_buff *skb)
{
	int rc;
	skb->dev = dev;
	rc = dev_queue_xmit(skb);
	if (dev_xmit_complete(rc) == false && net_ratelimit())
		netdev_err(dev, "dev_queue_xmit() failed: %d\n", rc);
}
EXPORT_SYMBOL(avm_pa_dev_queue_xmit);

static int register_for_dev(struct net_device *dev,
			    void (*tx_func)(struct net_device *dev, struct sk_buff *skb))
{
	struct avm_pa_pid_cfg cfg = {0};
	int ret;

	BUG_ON(!dev);

	if (dev->type == ARPHRD_NONE)
		cfg.framing = avm_pa_framing_ipdev;
#ifdef ARPHRD_RAWIP
	else if (dev->type == ARPHRD_RAWIP) /* since 4.14 */
		cfg.framing = avm_pa_framing_ipdev;
#endif
	else if (tx_func == avm_pa_dev_queue_xmit) /* via avm_pa_dev_register() */
		cfg.framing = avm_pa_framing_dev;
	else /* via avm_pa_dev_register_ll() */
		cfg.framing = avm_pa_framing_ether;

#ifdef CONFIG_BCM_KF_BLOG
	/* If we're called before register_netdev(), the name might not be fixed
	 * yet (you can call it with a template name such has eth%d and have
	 * register_netdev() fix that up through dev_get_valid_name(). We can do that
	 * here too.
	 *
	 * Don't mess with the name if we're past register_netdev() already, or inside (for
	 * example if we're called from a NETDEV_REGISTER notify handler).
	 */
	if (dev->reg_state == NETREG_UNINITIALIZED) {
		int ret = dev_get_valid_name(dev_net(dev), dev, dev->name);

		if (ret < 0)
			return ret;
	}
#endif

	snprintf(cfg.name, sizeof(cfg.name), "%s", dev->name);
	cfg.tx_func = (void (*)(void *, struct sk_buff *)) tx_func;
	cfg.tx_arg = dev;
	ret = avm_pa_dev_pidhandle_register_with_ingress(AVM_PA_DEVINFO(dev), 0, &cfg, 0);

	if (ret == 0) {
		struct avm_pa_pid_ecfg ecfg = {};

		ecfg.version = AVM_PA_PID_ECFG_VERSION;
		ecfg.flags = AVM_PA_PID_FLAG_GSO_CAPABLE;
		if ((ret = avm_pa_pid_set_ecfg(AVM_PA_DEVINFO(dev)->pid_handle, &ecfg)) < 0)
			avm_pa_dev_unregister(AVM_PA_DEVINFO(dev), NULL);
	}

	return ret;
}

int avm_pa_dev_register(struct net_device *dev)
{
	return register_for_dev(dev, avm_pa_dev_queue_xmit);
}
EXPORT_SYMBOL(avm_pa_dev_register);

void avm_pa_dev_start_xmit(struct net_device *dev, struct sk_buff *skb)
{
	int rc;
	struct netdev_queue *txq = netdev_get_tx_queue(dev, skb_get_queue_mapping(skb));
	bool __maybe_unused again = false;

	skb->dev = dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
	if (!(skb = validate_xmit_skb_list(skb, dev, &again))) {
#else
	if (!(skb = validate_xmit_skb_list(skb, dev))) {
#endif
		if (net_ratelimit())
			netdev_err(dev, "validate_xmit_skb_list() failed\n");
		return;
	}
#endif

	HARD_TX_LOCK(dev, txq, smp_processor_id());
	if (!netif_tx_queue_stopped(txq)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
		skb = dev_hard_start_xmit(skb, dev, txq, &rc);
#else
		rc = dev_hard_start_xmit(skb, dev, txq);
#endif
	} else {
		rc = NETDEV_TX_BUSY;
	}
	HARD_TX_UNLOCK(dev, txq);

	if (!dev_xmit_complete(rc)) {
		if (net_ratelimit())
			netdev_err(dev, "dev_hard_start_xmit() failed: %d\n", rc);
		kfree_skb_list(skb);
	}
}
EXPORT_SYMBOL(avm_pa_dev_start_xmit);

int avm_pa_dev_register_ll(struct net_device *dev)
{
	BUG_ON(!dev);
	/* With NETIF_F_LLTX HARD_TX_LOCK() becomes a no-op instead of an expensive spinlock */
	if (!(dev->features & NETIF_F_LLTX))
		netdev_warn(dev, "consider NETIF_F_LLTX for better performance\n");

	return register_for_dev(dev, avm_pa_dev_start_xmit);
}
EXPORT_SYMBOL(avm_pa_dev_register_ll);

static int pa_notifier_event(struct notifier_block *notifier,
                             unsigned long event, void *ptr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
	struct net_device *dev = (struct net_device *) ptr;
#else
	struct net_device *dev = netdev_notifier_info_to_dev((const struct netdev_notifier_info *) ptr);
#endif

	switch (event) {
	case NETDEV_CHANGEMTU:
		/* Must observe MTU changes to avoid passing oversized frames to the
		 * drivers. Smaller MTU invalidates existing sessions.
		 */
		avm_pa_dev_set_mtu(AVM_PA_DEVINFO(dev), dev->mtu, dev->mtu);
		break;
	case NETDEV_REGISTER:
		if (netif_is_bridge_master(dev)) {
			struct avm_pa_node_cfg cfg;

			cfg.v4_mtu = dev->mtu;
			cfg.v6_mtu = dev->mtu;
			avm_pa_dev_node_register(AVM_PA_DEVINFO(dev), dev->name, &cfg, GFP_KERNEL);
		}
		break;
	case NETDEV_UNREGISTER:
		if (netif_is_bridge_master(dev))
			avm_pa_dev_node_unregister(AVM_PA_DEVINFO(dev));
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block pa_netdev_notify = {
	.notifier_call = pa_notifier_event,
};

int __init avm_pa_netdev_init(void)
{
	return register_netdevice_notifier(&pa_netdev_notify);
}

