#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <avm/power/power.h>
#include <asm/uaccess.h>
#include "avm_power.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#	define NR_ANON_MAPPED NR_ANON_PAGES
#endif

#if KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE
#define global_page_state(a) global_zone_page_state(a)
#endif

#if IS_ENABLED(CONFIG_AVM_EVENT)
static void *cpu_idle_eventhandle;

/**
 */
static void avmevent_cpu_idle_notify(void *context, enum _avm_event_id id)
{
	struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)context;
	struct _avm_event_cpu_idle *event;
	struct sysinfo meminfo;
	unsigned long active;
	unsigned long free;
	unsigned long hard_mem;
	int handled;

	if (id != avm_event_id_cpu_idle) {
		pr_warn("[avm_power]unknown event: %d\n", id);
		return;
	}
	event = kzalloc(sizeof(struct _avm_event_cpu_idle), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("[avm_power]can't alloc event: %d\n", id);
		return;
	}
#if defined(CONFIG_AVM_EVENT_20)
	event->cputype = (avm_power_cpu_mode() == cpu_mode_remote) ? remote_cpu : host_cpu;
#endif/*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
	event->event_header.id = id;
	event->cpu_idle       = pm_info->deviceinfo[powerdevice_loadrate].power_rate > 100 ?
						0 : 100 - pm_info->deviceinfo[powerdevice_loadrate].power_rate;
	event->dsl_dsp_idle   = pm_info->deviceinfo[powerdevice_dsp_loadrate].power_rate > 100 ?
							0 : 100 - pm_info->deviceinfo[powerdevice_dsp_loadrate].power_rate;
	event->voice_dsp_idle = pm_info->deviceinfo[powerdevice_vdsp_loadrate].power_rate > 100 ?
							0 : 100 - pm_info->deviceinfo[powerdevice_vdsp_loadrate].power_rate;
	free     = global_page_state(NR_FREE_PAGES);
	/*--- inactive = global_page_state(NR_INACTIVE_ANON) + global_page_state(NR_INACTIVE_FILE); ---*/
	active   = global_page_state(NR_ACTIVE_ANON) + global_page_state(NR_ACTIVE_FILE);

	si_meminfo(&meminfo);
	si_swapinfo(&meminfo);

	event->mem_physfree = (unsigned char)((meminfo.freeram * 100) / (meminfo.totalram|1));
	hard_mem = global_page_state(NR_FILE_DIRTY) + global_page_state(NR_WRITEBACK) + global_page_state(NR_FILE_MAPPED) +
			   global_page_state(NR_SLAB_UNRECLAIMABLE) +
			   global_page_state(NR_ANON_MAPPED);
	if (active > hard_mem)
		hard_mem = active;

	event->mem_strictlyused = (unsigned char)((hard_mem * 100) / (meminfo.totalram|1));
	event->mem_cacheused    = (unsigned char)(100 - event->mem_strictlyused - event->mem_physfree);

	if (avm_power_disp_loadrate & 4) {
		pr_err("[avm_power_disp_loadrate] cpu-idle-event: MEM: %d %d %d %%, active=%ld free=%ld/%ld\n",
						event->mem_strictlyused, event->mem_cacheused, event->mem_physfree,
						active, free, meminfo.freeram);
	}
	handled = avm_event_source_trigger(cpu_idle_eventhandle, id, sizeof(struct _avm_event_cpu_idle), event);

	if (handled == 0) {
		pr_warn("[%s]event: %d not handled\n", __func__, id);
	}
}
#if defined(CONFIG_AVM_EVENT_20)
static void *cpu_run_eventhandle;

/**
 * \brief: liefere auf Anforderung oder bei cpu_run-change den cpu-load fuer alle cpus
 */
void avmevent_cpu_run_notify(void *context, enum _avm_event_id id)
{
	struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)context;
	struct _avm_event_cpu_run *event;
	unsigned int cpu;

	if (cpu_run_eventhandle == NULL) {
		return;
	}
	if (id != avm_event_id_cpu_run) {
		pr_warn("[avm_power]unknown event: %d\n", id);
		return;
	}
	event = kzalloc(sizeof(struct _avm_event_cpu_run), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("[avm_power]can't alloc event: %d\n", id);
		return;
	}
	event->event_header.id = id;
	event->cputype = (avm_power_cpu_mode() == cpu_mode_remote) ? remote_cpu : host_cpu;
	for (cpu = 0; cpu < ARRAY_SIZE(event->cpu_run); cpu++) {
		if (cpu >= num_possible_cpus()) {
			event->cpu_run[cpu] = 0xFF;
		} else {
			event->cpu_run[cpu] = pm_info->cpu_run[cpu];
		}
	}
	/*--- pr_err("%s: cpu_run: %4ph\n", __func__, event->cpu_run); ---*/
	avm_event_source_trigger(cpu_run_eventhandle, id, sizeof(struct _avm_event_cpu_run), event);
}
#endif/*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
int avm_power_cpuidle_init(void)
{
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_id_mask id_mask;

	cpu_idle_eventhandle =  avm_event_source_register("cpu_idle",
							avm_event_build_id_mask(&id_mask, 1, avm_event_id_cpu_idle),
							avmevent_cpu_idle_notify,
							&pm_ressourceinfo
							);
	if (cpu_idle_eventhandle == NULL) {
		pr_err("[avm_power] %s register failed !\n", __func__);
		return -1;
	}
#if defined(CONFIG_AVM_EVENT_20)
	cpu_run_eventhandle =  avm_event_source_register("cpu_run",
							avm_event_build_id_mask(&id_mask, 1, avm_event_id_cpu_run),
							avmevent_cpu_run_notify,
							&pm_ressourceinfo
							);
	if (cpu_run_eventhandle == NULL) {
		pr_err("[avm_power] %s register failed !\n", __func__);
		return -1;
	}
#endif/*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	return 0;
}
