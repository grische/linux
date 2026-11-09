/*
 *
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#pragma GCC pop_options

#include <avm/power/power.h>
#if __has_include(<avm/event/event.h>)
#include <avm/event/event.h>
#else
#include <linux/avm_event.h>
#endif
#include "avm_power.h"

#if IS_ENABLED(CONFIG_AVM_EVENT)
static void *powermanagment_remote_event_sink_handle;
/*--- #define DEBUG_AVM_POWER ---*/
#if defined(DEBUG_AVM_POWER)
#define DBG_TRC(args...)  pr_info(args)

/**
 */
static void dump_data(const char *prefix, const unsigned char *data, unsigned int len)
{
	pr_err("%s: data len=%d:", prefix, len);
	while (len--) {
		pr_cont("%02x,", *data++);
	}
	pr_err("\n");
}
#else
#define DBG_TRC(args...) no_printk(args)
#endif/*--- #if defined(DEBUG_AVM_POWER) ---*/

/**
 */
static void avm_event_powermanagment_remote_sink(void *private __attribute__((unused)),
						unsigned char *buf, unsigned int len __maybe_unused)
{
	struct _avm_event_powermanagment_remote *pevent = (struct _avm_event_powermanagment_remote *)buf;

#if defined(DEBUG_AVM_POWER)
	/*--- dump_data(__func__, buf, len); ---*/
	if (pevent->header.id != avm_event_id_powermanagment_remote) {
		pr_err("%s: incompatible event (id=%u)\n", __func__, pevent->header.id);
		return;
	}
#endif/*--- #if defined(DEBUG_AVM_POWER) ---*/
	switch (pevent->remote_action) {
	case avm_event_powermanagment_ressourceinfo:
		DBG_TRC("[%s]PowerManagmentRessourceInfo(%s, %x)\n", __func__,
			pm_name_device(pevent->param.ressourceinfo.device),
			pevent->param.ressourceinfo.power_rate);
		PowerManagmentRessourceInfo(pevent->param.ressourceinfo.device, pevent->param.ressourceinfo.power_rate);
		break;
	case avm_event_powermanagment_activatepowermode:
		DBG_TRC("[%s]PowerManagmentActivatePowerMode(%s)\n", __func__, pevent->param.powermode);
		if (PowerManagmentActivatePowerMode(pevent->param.powermode)) {
			pr_err("%s: PowerManagmentActivatePowerMode(%s) failed\n", __func__, pevent->param.powermode);
		}
		break;
	default:
		pr_err("%s: incompatible remote_action(%u)\n", __func__, pevent->remote_action);
		break;
	}
}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
int avm_power_remote_sink_init(void)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_id_mask id_mask;

	if (powermanagment_remote_event_sink_handle) {
		pr_err("%s already registered\n", __func__);
		return 0;
	}
	powermanagment_remote_event_sink_handle = avm_event_sink_register("powermanagment_remote_sink",
								avm_event_build_id_mask(&id_mask, 1, avm_event_id_powermanagment_remote),
								avm_event_powermanagment_remote_sink,
								NULL
								);
	if (powermanagment_remote_event_sink_handle == NULL) {
		pr_err("%s not registered\n", __func__);
		return -ENOMEM;
	}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
#if defined(DEBUG_AVM_POWER)
	pr_err("%s registered\n", __func__);
#endif/*--- #if defined(DEBUG_AVM_POWER) ---*/
	return 0;
}

/**
 */
void avm_power_remote_sink_exit(void)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	if (powermanagment_remote_event_sink_handle == NULL) {
		pr_err("%s not registered\n", __func__);
		return;
	}
	avm_event_sink_release(powermanagment_remote_event_sink_handle);
	powermanagment_remote_event_sink_handle = NULL;
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
}
