// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2014-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <avm/watchdog/watchdog.h>
#include "avm_sammel.h"
#include <avm/event/event.h>

static unsigned int wdt_running;

int ar7wdt_hw_is_wdt_running(void)
{
	return wdt_running;
}

void ar7wdt_hw_secure_wdt_disable(void)
{
}

static void *remote_event_source_handle;
#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)
static void lwdt_remotesend(const struct _avm_event_remotewatchdog *pevent)
{
	struct _avm_event_remotewatchdog *event =
		kmalloc(sizeof(struct _avm_event_remotewatchdog), GFP_ATOMIC);

	if (event == NULL) {
		pr_warn("%s (remote): can't alloc event\n", __func__);
		return;
	}
	memcpy(event, pevent, sizeof(struct _avm_event_remotewatchdog));
	avm_event_source_trigger(remote_event_source_handle,
				 avm_event_id_remotewatchdog,
				 sizeof(struct _avm_event_remotewatchdog),
				 event);
}

static void lwdt_register(void)
{
	const struct _avm_event_remotewatchdog event = {
		.event_header = {
			.id = avm_event_id_remotewatchdog,
		},
		.cmd = wdt_register,
		.name = "ATOM",
		.param = 120 +
			WDT_DEFAULT_TIME * 4,
			/* 2 * lockup detection time + std. timeout */
	};
	lwdt_remotesend(&event);
}

static void lwdt_release(void)
{
	struct _avm_event_remotewatchdog event = {
		.event_header = {
			.id = avm_event_id_remotewatchdog,
		},
		.cmd = wdt_release,
		.name = "ATOM",
		.param = 0,
	};
	lwdt_remotesend(&event);
}

static void lwdt_trigger(void)
{
	struct _avm_event_remotewatchdog event = {
		.event_header = {
			.id = avm_event_id_remotewatchdog,
		},
		.cmd = wdt_trigger,
		.name = "ATOM",
		.param = 0,
	};
	lwdt_remotesend(&event);
}

static void avm_event_remote_notify(void *context, enum _avm_event_id id)
{
}

/* erst wenn Remote-Event-Node oben, Event-Schnittstellen anmelden */
static void remote_event_node_established(void *private,
					  unsigned int param1,
					  unsigned int param2)
{
	struct _avm_event_id_mask id_mask;

	avm_event_build_id_mask(&id_mask, 1,
				avm_event_id_remotewatchdog),

	remote_event_source_handle =
		avm_event_source_register("avm_event_remotewatchdog",
					  &id_mask,
					  avm_event_remote_notify, NULL);

	if (!remote_event_source_handle) {
		pr_err("%s not registered\n", __func__);
		return;
	}
	lwdt_register();
	wdt_running = 1;
}
#endif

void ar7wdt_hw_init(void)
{
	pr_debug("%s\n", __func__);
#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)
	avm_event_node_established(remote_event_node_established, NULL, 0, 0);
#else
#error no watchdog implemented
#endif
}

void ar7wdt_hw_deinit(void)
{
#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)
	if (wdt_running) {
		lwdt_release();
		wdt_running = 0;
	}
#endif
}

void ar7wdt_hw_reboot(void)
{
	panic("%s: watchdog expired\n", __func__);
}

void ar7wdt_hw_trigger(void)
{
	pr_debug("%s!!\n", __func__);
#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)
	if (wdt_running)
		lwdt_trigger();
#endif
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);
