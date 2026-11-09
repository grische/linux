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
 *
 *   Export-Funktionen (fuer Remote)
		 PowerManagmentRessourceInfo
		 PowerManagmentActivatePowerMode
		 PowerManagmentRegister
		 PowerManagmentRelease
 */
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#pragma GCC pop_options

#include <avm/power/power.h>
#include "avm_power.h"

/*--- #define DEBUG_AVM_POWER ---*/

/**
 */
struct _pw_remote_source  {
	unsigned int Changes;
	struct task_struct *kthread;
	wait_queue_head_t wait_queue;
	void *remote_event_handle;
} pw_remote_source;

#if IS_ENABLED(CONFIG_AVM_EVENT)

/**
 */
static void avm_event_powermanagment_remote_notify(void *context __attribute__((unused)),
							enum _avm_event_id id __attribute__((unused)))
{
}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
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
#define DBG_TRC(args...)
#endif/*--- #if defined(DEBUG_AVM_POWER) ---*/

/**
 */
static int remote_source_thread(void *data)
{
	struct _pw_remote_source *pwrs  = (struct _pw_remote_source *)data;

	/*--- DEB_ERR("[%s]: start\n", __func__); ---*/
	while (!kthread_should_stop()) {
		if (wait_event_interruptible_timeout(pwrs->wait_queue, pwrs->Changes, 5 * HZ)) {
			break;
		}
		pwrs->Changes = 0;
		PowerManagmentRessourceInfo(powerdevice_temperature, avm_power_temperature());
	}
	pwrs->kthread = NULL;
	/*--- DEB_ERR("[%s]: exit\n", __func__); ---*/
	return 0;
}

#if 0

/**
 */
static void remote_source_thread_trigger(void)
{
	if (pw_remote_source.kthread) {
		pw_remote_source.Changes++;
		wake_up_interruptible(&pw_remote_source.wait_queue);
	}
}
#endif

/**
 * old_model: von der x86-Seite cpuidle-, wlan- und usb-Status zum arm
 * new_model: von der arm-Seite cpuidle-Status (powerdevice_loadrate2) zum x86
 */
int avm_power_remote_source_init(void)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_id_mask id_mask;
	int ret = 0;

	if (pw_remote_source.remote_event_handle) {
		return ret;
	}
	pw_remote_source.remote_event_handle = avm_event_source_register("powermanagment_remote",
								avm_event_build_id_mask(&id_mask, 1, avm_event_id_powermanagment_remote),
								avm_event_powermanagment_remote_notify,
								NULL
								);
	if (pw_remote_source.remote_event_handle == NULL) {
		ret = -1;
		pr_err("[avm_power]%s not registered\n", __func__);
	}
	return ret;
#else/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	return -1;
#endif/*--- #else ---*//*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
}

/**
 * old_model: x86: die Temperatur regelmaessig abfragen, da hier kein pm_info-Thread
 * new_model: nicht verwenden
 */
int avm_power_remote_source_temperature_init(void)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	int ret = 0;

	if (pw_remote_source.kthread) {
		return ret;
	}
	init_waitqueue_head(&pw_remote_source.wait_queue);
	pw_remote_source.kthread = kthread_run(remote_source_thread, (void *) &pw_remote_source, "pm_remote");
	BUG_ON((pw_remote_source.kthread == NULL) || IS_ERR((void *)pw_remote_source.kthread));
	return ret;
#else/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	return -1;
#endif/*--- #else ---*//*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
}

/**
 * Funktion wird von Treibern aufgerufen um Infos ueber den aktuellen Power-Status zu liefern
 */
int PowerManagmentRessourceInfo(enum _powermanagment_device device, int power_rate)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_powermanagment_remote *event;
	/*--- auf Remote-Seite lokal speichern z.B. fuer cpuidle-Event: ---*/
	if (pm_ressourceinfo.deviceinfo[device].power_rate == power_rate) {
		return 0;
	}
	if ((avm_power_disp_loadrate & 1) && (device == powerdevice_loadrate)) {
		char txt[64];
		unsigned long avnrun[3];

		get_avenrun(avnrun, FIXED_1/200, 0);
		pr_err("%s loadavg %lu.%lu %lu.%lu %lu.%lu\n",
			display_per_cpurate(txt, sizeof(txt), power_rate),
			LOAD_INT(avnrun[0]), LOAD_FRAC(avnrun[0]),
			LOAD_INT(avnrun[1]), LOAD_FRAC(avnrun[1]),
			LOAD_INT(avnrun[2]), LOAD_FRAC(avnrun[2]));
	}
	if ((avm_power_disp_loadrate & 2) && (device != powerdevice_loadrate)) {
		pr_err("[avm_power]%s: device: %s value=(0x%x)%d\n", __func__,
					pm_name_device(device), power_rate, power_rate);
	}
	pm_ressourceinfo.deviceinfo[device].power_rate = power_rate;
	if (device == powerdevice_loadrate) {
		/*--- damit kein Konflikt mit Host-CPU ---*/
		device = powerdevice_loadrate2;
	}
	if (pw_remote_source.remote_event_handle == NULL) {
		/*--- pr_err("%s: can't execute %d(%x), because remote_event-device not ready\n", __func__, device, power_rate); ---*/
		return -EAGAIN;
	}
	event = kmalloc(sizeof(struct _avm_event_powermanagment_remote), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("%s (remote): can't alloc event\n", __func__);
		return -ENOMEM;
	}
	event->header.id                      = avm_event_id_powermanagment_remote;
	event->remote_action                  = avm_event_powermanagment_ressourceinfo;
	event->param.ressourceinfo.device     = device;
	event->param.ressourceinfo.power_rate = power_rate;

	DBG_TRC("%s(remote)(%s, 0x%x)\n", __func__, pm_name_device(device), power_rate);
#if defined(DEBUG_AVM_POWER)
	/*--- dump_data(__func__, event, sizeof(*event)); ---*/
#endif/*--- #if defined(DEBUG_AVM_POWER) ---*/
	return avm_event_source_trigger(pw_remote_source.remote_event_handle, avm_event_id_powermanagment_remote,
					sizeof(struct _avm_event_powermanagment_remote), event);
#else
	return -ENODEV;
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
}
EXPORT_SYMBOL(PowerManagmentRessourceInfo);

/**
 */
int PowerManagmentActivatePowerMode(char *powermode_name)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_powermanagment_remote *event;

	if ((pw_remote_source.remote_event_handle == NULL) || (powermode_name == NULL)) {
		return -1;
	}
	event = kmalloc(sizeof(struct _avm_event_powermanagment_remote), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("%s (remote): can't alloc event\n", __func__);
		return -ENOMEM;
	}
	event->header.id     = avm_event_id_powermanagment_remote;
	event->remote_action = avm_event_powermanagment_activatepowermode;
	strncpy(event->param.powermode, powermode_name, sizeof(event->param.powermode));

	DBG_TRC("%s(remote)(%s)\n", __func__, powermode_name);
	return avm_event_source_trigger(pw_remote_source.remote_event_handle, avm_event_id_powermanagment_remote,
					sizeof(struct _avm_event_powermanagment_remote), event);
#else
	return -1;
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
}
EXPORT_SYMBOL(PowerManagmentActivatePowerMode);

/**
 * dummy
 */
void *PowerManagmentRegister(char *client_name __attribute__((unused)),
				int (*CallBackPowerManagmentControl)(int state) __attribute__((unused)))
{
	pr_warn("%s(%s) (remote) not implemented\n", __func__, client_name);
	return NULL;
}
EXPORT_SYMBOL(PowerManagmentRegister);

/**
 * dummy
 */
void PowerManagmentRelease(void *Handle __attribute__((unused)))
{
}
EXPORT_SYMBOL(PowerManagmentRelease);
