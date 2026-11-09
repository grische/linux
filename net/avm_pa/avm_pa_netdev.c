/*
 * Packet Accelerator Fragmentation
 *
 * SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100:
 *
 * Copyright (c) 2011-2020 AVM GmbH <info@avm.de>
 * All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/netdevice.h>

#include "avm_pa.h"
#include "avm_pa_intern.h"

static int pa_notifier_event(struct notifier_block *notifier,
                             unsigned long event, void *ptr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
	struct net_device *dev = (struct net_device *) ptr;
#else
	struct net_device *dev = netdev_notifier_info_to_dev((const struct netdev_notifier_info *) ptr);
#endif
	struct avm_pa_pid *pid;

	switch (event) {
	case NETDEV_CHANGEMTU:
		/* Must observe MTU changes to avoid passing oversized frames to the
		 * drivers. Smaller MTU invalidates existing sessions.
		 */
		pid = avm_pa_pid_get_pid(AVM_PA_DEVINFO(dev)->pid_handle);
		if (pid) {
			if (dev->mtu < pid->cfg.default_mtu)
				avm_pa_flush_sessions_for_pid(pid->pid_handle);
			pid->cfg.default_mtu = dev->mtu;
			avm_pa_pid_put(pid->pid_handle);
		}
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
