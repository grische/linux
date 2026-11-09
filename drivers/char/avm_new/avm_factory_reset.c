// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2018-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/reboot.h>
#include <linux/kmod.h>
#include <avm/event/event.h>

static void *factory_reset_kernel_handle;

static void factory_reset_received(void *context,
				   unsigned char *buf,
				   unsigned int len)
{
	struct _avm_event_push_button *event =
		(struct _avm_event_push_button *)buf;

	if (!event)
		return;

	if (event->key == avm_event_push_button_power_set_factory_kernel) {
		char *argv[] = { "/bin/sh", "/bin/setfactorydefaults", NULL };
		char *envp[] = { "HOME=/", "TERM=linux",
				 "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };

		pr_info("Trigger factory reset using usermodehelper\n");

		call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
		printk_avm_console_bend(0);
		pr_emerg("Restart for factory reset\n");
		kernel_restart(NULL);
	}
}

void __init avm_factory_reset_init(void)
{
	struct _avm_event_id_mask id_mask;

	pr_info("Register push button event to receive the set_factory_kernel event\n");
	factory_reset_kernel_handle = avm_event_sink_register(
		"factory_reset_kernel",
		avm_event_build_id_mask(&id_mask, 1, avm_event_id_push_button),
		factory_reset_received, NULL);
	if (factory_reset_kernel_handle == NULL) {
		pr_err("%s factory_reset_kernel event sink not registered\n",
		       __func__);
	}
}
