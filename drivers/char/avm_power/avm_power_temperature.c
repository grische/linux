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
 */

/**
 */
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#pragma GCC pop_options

#include <avm/power/power.h>
#if __has_include(<avm/sammel/simple_proc.h>)
#include <avm/sammel/simple_proc.h>
#else
#include <linux/simple_proc.h>
#endif
#if __has_include(<avm/sammel/kpi.h>)
#include <avm/sammel/kpi.h>
#else
#include <linux/avm_kpi.h>
#endif
#include "avm_power.h"

#define TEMPERATURE_SENSOR_POISON		INT_MAX
#define TEMPERATURE_SENSOR_POLL_INTERVALL_MS	1000u

static void TemperaturSensor_Statistic(struct seq_file *seq, void *_complete);
static void reclaim_temp_sensor(struct work_struct *work);
static void *temperature_eventhandle;
static struct kpi_node *kpi_temp_node;
static const struct kpi_sliding_window kpi_temp_window = {
	.size_seconds = 24 * 60 * 60, /* 24h */
	.num_samples = 24 * 6, /* every 10m */
};
static const enum kpi_reducer kpi_temp_reducers[] = {
	KPI_REDUCER_MIN,
	KPI_REDUCER_MAX,
	KPI_REDUCER_AVG,
	_KPI_REDUCER_BOTTOM_,
};

/**
 */
int avm_power_temperature(void)
{
	int err, val;
	const char *name;

	err = TemperatureSensorGetCurrentTemp(0, &name, &val);

	/* If you hit this error you must register your CPU temperature
	 * by calling TemperaturSensorRegister. CPU temperature must be
	 * registered as index 0 which means it must be registered first.
	 */
	if (err == -ENOENT)
		pr_err_once("avm_power: [%s] No cpu sensor registered\n",
			__func__);

	val = val / 10;
	return val;
}

/**
 */
static void avmevent_temperature_notify(void *context __maybe_unused,
					enum _avm_event_id id)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_temperature *event;
	int handled;

	if (id != avm_event_id_temperature) {
		pr_warn("[%s]unknown event: %d\n", __func__, id);
		return;
	}
	event = kmalloc(sizeof(struct _avm_event_temperature), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("[%s]can't alloc event: %d\n", __func__,
			   id);
		return;
	}
	event->event_header.id = id;
	event->temperature = avm_power_temperature();

	if (avm_power_disp_loadrate & 8) {
		pr_err("[%s]temperature event: %d\n", __func__,
			   event->temperature);
	}
	handled =
		avm_event_source_trigger(temperature_eventhandle, id,
					 sizeof(struct _avm_event_temperature),
					 event);
	if (handled == 0) {
		pr_warn("[%s]event: %d not handled\n", __func__,
			   id);
	}
#endif /*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
}

/**
 */
int avm_power_temperature_init(void)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_id_mask id_mask;

	temperature_eventhandle = avm_event_source_register(
		"temperature",
		avm_event_build_id_mask(&id_mask, 1, avm_event_id_temperature),
		avmevent_temperature_notify, NULL);
	if (temperature_eventhandle == NULL) {
		pr_err("[%s] avm event temperature register failed !\n",
			   __func__);
		return -1;
	}
#endif /*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	PowerManagmentRessourceInfo(powerdevice_temperature, 0);
	add_simple_proc_file("avm/temp_sensors", NULL,
				 TemperaturSensor_Statistic, NULL);

	if (IS_ENABLED(CONFIG_AVM_KPI))
		kpi_temp_node = kpi_create_dict("temperatures",
						kpi_get_section("system"));

	return 0;
}

/**
 * Generisches Temperatur-Sensor-Modell
 */
static LIST_HEAD(gTemperaturSensor_list);
static DEFINE_SPINLOCK(gTemperaturSensor_write_lock);
struct _temperatursensor_priv {
	struct list_head list;
	struct rcu_head rcu_head;
	void *context;
	TemperaturSensorReadTemperatureCallback_t callback;
	char name[64];
	struct task_struct *reader_thread;
	struct work_struct reclaim_work;
	atomic_t cached_value;
	struct kpi_sampler *kpi;
};

/**
 */
static int TemperaturSensor_Reader_Thread(void *data)
{
	struct _temperatursensor_priv *sensor = data;
	int err, val;

	while (!kthread_should_stop()) {
		err = sensor->callback(sensor, sensor->context, &val);
		if (err)
			atomic_set(&sensor->cached_value, TEMPERATURE_SENSOR_POISON);
		else
			atomic_set(&sensor->cached_value, val);

		msleep_interruptible(TEMPERATURE_SENSOR_POLL_INTERVALL_MS);
	}

	return 0;
}

/**
 */
static void TemperaturSensor_Statistic(struct seq_file *seq,
					   void *_complete __maybe_unused)
{
	int value;
	struct _temperatursensor_priv *ptsensor;

	rcu_read_lock();
	list_for_each_entry_rcu(ptsensor, &gTemperaturSensor_list, list) {
		value = atomic_read(&ptsensor->cached_value);
		if (value != TEMPERATURE_SENSOR_POISON) {
			seq_printf(seq, "%-64s: %d.%u °C\n", ptsensor->name,
				   value / 10, value % 10);
		} else {
			seq_printf(seq, "%-64s: error=%d\n", ptsensor->name,
				   -EIO);
		}
	}
	rcu_read_unlock();
}

static s32 avm_temp_kpi_collector(void *ctx)
{
	struct _temperatursensor_priv *ptsensor = ctx;
	int ret;

	ret = atomic_read(&ptsensor->cached_value);
	if (ret == TEMPERATURE_SENSOR_POISON)
		ret = 0;

	return ret;
}

/**
 * Temperatur-Sensor anmelden
 */
void *TemperaturSensorRegister(
	const char *name, TemperaturSensorReadTemperatureCallback_t callback,
	void *context)
{
	struct _temperatursensor_priv *ptsensor;

	if ((name == NULL) || (callback == NULL)) {
		pr_err("%s invalid parameter\n", __func__);
		return NULL;
	}
	ptsensor = kzalloc(sizeof(struct _temperatursensor_priv), GFP_ATOMIC);
	if (ptsensor == NULL) {
		pr_err("%s can't get memory\n", __func__);
		return NULL;
	}

	INIT_LIST_HEAD(&ptsensor->list);
	INIT_WORK(&ptsensor->reclaim_work, reclaim_temp_sensor);
	strlcpy(ptsensor->name, name, sizeof(ptsensor->name));
	ptsensor->callback = callback;
	ptsensor->context = context;
	atomic_set(&ptsensor->cached_value, TEMPERATURE_SENSOR_POISON);

	ptsensor->reader_thread = kthread_run(TemperaturSensor_Reader_Thread,
						  ptsensor, "temp_reader_%s", name);

	if (IS_ERR(ptsensor->reader_thread)) {
		kzfree(ptsensor);
		return NULL;
	}

	spin_lock_bh(&gTemperaturSensor_write_lock);
	list_add_tail_rcu(&ptsensor->list, &gTemperaturSensor_list);
	spin_unlock_bh(&gTemperaturSensor_write_lock);

	if (IS_ENABLED(CONFIG_AVM_KPI) && kpi_temp_node)
		ptsensor->kpi = kpi_add_sampler_s32(name, kpi_temp_node,
						    avm_temp_kpi_collector,
						    ptsensor, kpi_temp_window,
						    kpi_temp_reducers);

	return ptsensor;
}
EXPORT_SYMBOL(TemperaturSensorRegister);

/**
 * Temperatur-Sensor abmelden
 */
static void reclaim_temp_sensor(struct work_struct *work)
{
	struct _temperatursensor_priv *sensor =
		container_of(work, typeof(*sensor), reclaim_work);

	if (IS_ENABLED(CONFIG_AVM_KPI) && sensor->kpi)
		kpi_delete_sampler(sensor->kpi);

	kthread_stop(sensor->reader_thread);
	kzfree(sensor);
}

static void rcu_callback(struct rcu_head *head)
{
	struct _temperatursensor_priv *sensor =
		container_of(head, typeof(*sensor), rcu_head);

	schedule_work(&sensor->reclaim_work);
}

void TemperaturSensorDeregister(void *handle)
{
	struct _temperatursensor_priv *ptsensor, *tmp;
	struct rcu_head *delete_me = NULL;

	spin_lock_bh(&gTemperaturSensor_write_lock);
	list_for_each_entry_safe(ptsensor, tmp, &gTemperaturSensor_list, list) {
		if (ptsensor != handle)
			continue;

		delete_me = &ptsensor->rcu_head;
		list_del_rcu(&ptsensor->list);
		break;
	}
	spin_unlock_bh(&gTemperaturSensor_write_lock);

	if (delete_me)
		call_rcu(delete_me, rcu_callback);
	else
		pr_err("%s invalid handle %p\n", __func__, handle);
}
EXPORT_SYMBOL(TemperaturSensorDeregister);

/**
 * \\brief liefere Temperatur entsprechend angemeldeten Sensor
 *	index: z.Z. auf 0 setzten
 *	name:  liefert einen Zeiger auf den Namen.
 *	value: liefert die Temperatur in 1/10°C
 */
int TemperatureSensorGetCurrentTemp(int index, const char **name,
					int *value)
{
	int ret, idx_cnt = index;
	struct _temperatursensor_priv *ptsensor;

	if (name)
		*name = NULL;

	if (value == NULL)
		return -EINVAL;

	*value = 0;
	rcu_read_lock();
	list_for_each_entry_rcu(ptsensor, &gTemperaturSensor_list, list) {
		if (idx_cnt--)
			continue;

		if (name)
			*name = ptsensor->name;

		ret = atomic_read(&ptsensor->cached_value);
		if (ret == TEMPERATURE_SENSOR_POISON) {
			ret = -EIO;
		} else {
			*value = ret;
			ret = 0;
		}
		rcu_read_unlock();
		return ret;
	}
	rcu_read_unlock();

	return -ENOENT;
}
