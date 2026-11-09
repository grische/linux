#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <avm/power/power.h>
#include "avm_power.h"

struct _power_managment_ressource_info pm_ressourceinfo; /*--- wird auch im Remote-Sourcefall benoetigt ---*/

enum _init_power_type {
	pw_wlan         = (0x1 << 0),
	pw_usb          = (0x1 << 1),
	pw_dsl          = (0x1 << 2),
	pw_telefon      = (0x1 << 3),
	pw_speedstep    = (0x1 << 4),
	pw_temperature  = (0x1 << 5),
	pw_cpuidle      = (0x1 << 6),
	pw_pminfo       = (0x1 << 7),
	pw_remotesink   = (0x1 << 8),
	pw_remotesource = (0x1 << 9),
	pw_remote_temperature = (0x1 << 10),
	pw_pm_qos       = (0x1 << 11),
	pw_end          = (0x0),
};

#define functiontabset(typ, init, exit) { type:typ, init_func:init}
/**
 * Event/Managment-Notifierlist
 */
static const struct _initlist {
	enum _init_power_type type;
	int  (*init_func)(void);
} ginitFunclist[] = {
	functiontabset(pw_wlan,         avm_power_wlan_init,            avm_power_wlan_exit),
	functiontabset(pw_usb,          avm_power_usb_init,             avm_power_usb_exit),
	functiontabset(pw_temperature,  avm_power_temperature_init,     avm_power_temperature_exit),
	functiontabset(pw_dsl,          avm_power_dsl_init,             avm_power_dsl_exit),
	functiontabset(pw_telefon,      avm_power_telefon_init,         avm_power_telefon_exit),
	functiontabset(pw_speedstep,    avm_power_speedstep_init,       avm_power_speedstep_exit),
	functiontabset(pw_cpuidle,      avm_power_cpuidle_init,         avm_power_cpuidle_exit),
	functiontabset(pw_pm_qos,       avm_power_pm_qos_init,          avm_power_pm_qos_exit),
#if defined(CONFIG_AVM_POWER_REMOTE_SINK)
	functiontabset(pw_remotesink,   avm_power_remote_sink_init,     avm_power_remote_sink_exit),
#endif/*--- #if defined(CONFIG_AVM_POWER_REMOTE_SOURCE)  ---*/
#if defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
	functiontabset(pw_remotesource, avm_power_remote_source_init,   avm_power_remote_source_exit),
	functiontabset(pw_remote_temperature, avm_power_remote_source_temperature_init, avm_power_temperature_remote_source_exit),
#endif/*--- #if defined(CONFIG_AVM_POWER_REMOTE_SOURCE)  ---*/
#if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
#if defined(CONFIG_AVM_POWERMETER)
	functiontabset(pw_pminfo,       pm_ressourceinfo_init,          pm_ressourceinfo_exit),
#endif/*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
#endif/*--- #if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE)  ---*/
	functiontabset(pw_end,          NULL,                           NULL)
};
const unsigned int gInitFuncListMask =
#if defined(CONFIG_DOCSIS_MODEM) || defined(CONFIG_MACH_PUMA7)

/**
 *	new model of F!OS on puma6/puma7
 */
#if defined(CONFIG_AVM_POWER_REMOTE_SINK)
	/*--- x86 ---*/
	pw_remotesink | pw_usb | pw_dsl | pw_wlan | pw_telefon | pw_pminfo | pw_cpuidle | pw_temperature | pw_pm_qos
#elif defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
	/*--- arm ---*/
	pw_remotesource | pw_cpuidle
#endif
#elif defined(CONFIG_AVM_POWER_REMOTE_SINK)
/**
 *	old model of F!OS on puma6
 */
	/*---  actualy only arm on puma6  ---*/
	 pw_remotesink | pw_usb | pw_dsl | pw_telefon | pw_pminfo | pw_cpuidle | pw_temperature
#elif defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
/*---  actualy only atom on puma6  - temperature use PowerManagmentRessourceInfo(powerdevice_temperature,...) in remote-source-thread ---*/
	pw_remotesource | pw_remote_temperature | pw_wlan | pw_usb
#else
/**
 *	all boxes without remote-cpus
 */
	pw_wlan | pw_usb | pw_dsl | pw_telefon | pw_speedstep | pw_temperature | pw_cpuidle | pw_pminfo | pw_pm_qos
#endif
;

/**
 */
enum _avm_cpu_mode avm_power_cpu_mode(void)
{
#if defined(CONFIG_AVM_POWER_REMOTE_SINK)
	return cpu_mode_remote;
#elif defined(CONFIG_AVM_POWER_REMOTE_SOURCE)/*--- #if defined(CONFIG_AVM_POWER_REMOTE_SINK) ---*/
	return cpu_mode_host;
#else/*--- #elif defined(CONFIG_AVM_POWER_REMOTE_SOURCE) ---*//*--- #if defined(CONFIG_AVM_POWER_REMOTE_SINK) ---*/
	return cpu_mode_default;
#endif/*--- #else ---*//*--- #elif defined(CONFIG_AVM_POWER_REMOTE_SOURCE) ---*//*--- #if defined(CONFIG_AVM_POWER_REMOTE_SINK) ---*/
}

/**
 * erst wenn Remote-Event-Node oben, Event-Schnittstellen anmelden
 */
static void powermanagment_remote_event_node_established(void *private __attribute__((unused)),
		unsigned int param1 __attribute__((unused)), unsigned int param2 __attribute__((unused)))
{
	const struct _initlist *pinitlist = &ginitFunclist[0];

	while (pinitlist->type != pw_end) {
		if (gInitFuncListMask & pinitlist->type) {
			/*--- pr_err("start %pS\n", pinitlist->init_func); ---*/
			if (pinitlist->init_func()) {
				pr_err("error on start %pS\n", pinitlist->init_func);
			}
		}
		pinitlist++;
	}
	avm_powermanager_load_control_set(0, 1);
}

/**
 */
int __init avm_power_init(void)
{
	if (avm_power_file_init()) {
		return -ERESTARTSYS;
	}
#if defined(CONFIG_AVM_EVENT_20)
	avm_event_node_established(powermanagment_remote_event_node_established, NULL, 0, 0);
#else/*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
	powermanagment_remote_event_node_established(NULL, 0, 0);
#endif/*--- #else ---*//*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
	return 0;
}
module_init(avm_power_init);
