/*
 *
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _AVM_POWER_H_
#define _AVM_POWER_H_

#if IS_ENABLED(CONFIG_AVM_EVENT)
#if __has_include(<avm/event/event.h>)
#include <avm/event/event.h>
#else
#include <linux/avm_event.h>
#endif
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/*--- #define AVM_POWER_DEBUG ---*/

#if defined(AVM_POWER_DEBUG)
#define DEB_ERR(args...)     pr_err(args)
#define DEB_WARN(args...)    pr_warn(args)
#define DEB_NOTE(args...)    pr_info(args)
#define DEB_INFO(args...)    pr_info(args)
#define DEB_TRACE(args...)   pr_info(args)
#else /*--- #if defined(AVM_POWER_DEBUG) ---*/
#define DEB_ERR(args...)     pr_err(args)
#define DEB_WARN(args...)    pr_warn(args)
#define DEB_NOTE(args...)    no_printk(args)
#define DEB_INFO(args...)    no_printk(args)
#define DEB_TRACE(args...)   no_printk(args)
#endif /*--- #else ---*/ /*--- #if defined(AVM_POWER_DEBUG) ---*/

#include <asm/atomic.h>
/**
 * Eintrag in _power_managment
 */
struct _power_managment_dest_entry {
	char *client_name;  /*--- (registrierter) client-name ---*/
#define AVM_PM_CB_IGNORE                       0
	/*--- 1: falls Aktion scheitert(return-Wert von CallBackPowerManagmentControl): Tabelle nicht weiter abarbeiten ---*/
#define AVM_PM_CB_FAILED                       1
	/*--- 2: falls Aktion scheitert UND Funktion nicht registriert ---*/
#define AVM_PM_CB_UNINSTALLED_OR_FAILED        2
	/*--- 4: locke Operation ab hier ---*/
#define AVM_PM_LOCK                            4
	/*--- 8: ab hier asynchron ausfuehren (im Workerthread)  ---*/
#define AVM_PM_ASYNC                           8
	int  mandatory;
	int  state;         /*--- Übergabeparameter der fuer CallBackPowerManagmentControl ---*/
};

/**
 * powermode fuehrt folgende Tabelle (d.h. CallBackPowerManagmentControl() aus)
 */
struct _power_managment {
	const char *powermode;
	const struct _power_managment_dest_entry *dests;
#define PM_ACCESS_DRIVER    0x1
#define PM_ACCESS_APPL      0x2
#define PM_ACCESS_ALL       (PM_ACCESS_DRIVER | PM_ACCESS_APPL)
	const unsigned int Access;   /*--- veroderbar ---*/
};

/**
 * sammelt PowerManagmentRegister
 */
struct _power_managment_clients {
	struct _power_managment_clients *next;
	char *client_name;
	int (*CallBackPowerManagmentControl)(int state);
	atomic_t link;
};

/**
 */
struct _power_managment_device_info {
	int power_rate;  /*--- durch PowerManagmentRessourceInfo gesetzt ---*/
	/*--- Umrechnungsfaktoren lineare Fkt. y = mx + n: (multiplier * power_rate) / divider + offset -> mWatt ---*/
	int multiplier;
	int divider;
	int offset;
	int norm_power_rate; /*--- Zur Ermittlung der Norm-Watt-Wertes ---*/
};

#if IS_ENABLED(CONFIG_AVM_EVENT)
#define PM_TIME_INTERVALL          (24 * 3600) /*--- 24 h Intervall ---*/
#define PM_TIME_ENTRIES            (24 * 6)    /*--- alle 10 Minuten ein Teilzeitfenster ---*/

/**
 */
struct _power_managment_cum_info {
	unsigned long rate_sum;
	unsigned long rate_system;
	unsigned long rate_dsp;
	unsigned long rate_wlan;
	unsigned long rate_eth;
	unsigned long rate_ab;
	unsigned long rate_dect;
	unsigned long rate_battcharge;
	unsigned long rate_usbhost;
	unsigned long rate_lte;
	unsigned long rate_dvbc;
	unsigned long rate_plc;
	long messure_count; /*--- Anzahl der Messungen ---*/
	signed long rate_temp;
	signed char min_temp;
	signed char max_temp;
};
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
struct _power_managment_ressource_info {
	struct _power_managment_device_info deviceinfo[powerdevice_maxdevices];
	unsigned int NormP;             /*--- Norm-Leistungsverbrauch (entspricht 100 %) ---*/
	volatile struct task_struct *kthread;
	struct completion on_exit;
	wait_queue_head_t wait_queue;
	void *event_handle;
	unsigned int Changes;
#if IS_ENABLED(CONFIG_AVM_EVENT)
	struct _avm_event_pm_info_stat stat;
	unsigned long LastMessureTimeStamp;     /*--- letzte Messzeit in Sekunden ---*/
	unsigned long ActTimeEntry;
	struct _power_managment_cum_info cum[PM_TIME_ENTRIES];
#define AVMPOWER_MAX_ETHERNETPORTS   8
	unsigned char eth_status[AVMPOWER_MAX_ETHERNETPORTS];
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	volatile unsigned int loadcntrl; /*--- Kontext des Threads ---*/
	/*--- fuer event avm_event_id_cpu_run ---*/
	unsigned int  cpus_loadrate;
	unsigned int  cpurun_trigger;
	unsigned char cpu_run[NR_CPUS];
};

/*--- ueber die Console: ---*/
#define ETH_THROTTLE_DEMAND_POLICY  (0x1 << 2)
#define ETH_NORMAL_DEMAND_POLICY    (0x1 << 3)
#define ETH_THROTTLE_ACTIVE_POLICY  (0x1 << 4)
/*--- per Policy: ---*/
#define ETH_THROTTLE_DEMAND_CON     (0x1 << 5)
#define ETH_NORMAL_DEMAND_CON       (0x1 << 6)
#define ETH_THROTTLE_ACTIVE_CON     (0x1 << 7)

#define ETH_THROTTLE_DEMAND(a) (((a) & (ETH_THROTTLE_DEMAND_POLICY | ETH_THROTTLE_DEMAND_CON)) ? 1 : 0)
#define ETH_NORMAL_DEMAND(a)   (((a) & (ETH_NORMAL_DEMAND_POLICY | ETH_NORMAL_DEMAND_CON)) ? 1 : 0)

#define ETH_THROTTLE_ACTIVE(a) (((a) & (ETH_THROTTLE_ACTIVE_POLICY | ETH_THROTTLE_ACTIVE_CON)) ? 1 : 0)

#define ETH_MASK_BITS(a, clearmask, setmask) (((a) & ~(clearmask)) | (setmask))

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
extern void get_avenrun(unsigned long *loads, unsigned long offset, int shift);
#endif/*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32) ---*/

/**
 * avm_power_loadcontrol.c
 */
enum _load_control_set {
	load_control_auto = 0x0,
	load_control_off  = 0x1,
	load_control_manu = 0x2 /*--- (+ Offset fuer welcher Mode) ---*/
};
void avm_powermanager_load_control_set(enum _load_control_set mode, int scale);
int avm_powermanager_load_control_handler(int run);
void avm_powermanager_load_control_setflags(unsigned int value, unsigned int mask);

/**
 * avm_power_if.c
 */
const struct _power_managment_dest_entry *find_powermode(char *powermode_name, unsigned int access);
struct _power_managment_clients *find_pwclient_by_name(char *client_name);
int powermode_action(const struct _power_managment_dest_entry *powermodetab, unsigned int async_context);
int powermode_action_nolist(char *client_name, int State);
void avm_power_writeformaterror(char *text);
int PowerManagmentRessourceInfo_Init(void);
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)

#define MODULE_NAME     "avm_power"
/**
 * avm_power_remote_sink.c
 */
int avm_power_remote_sink_init(void);
void avm_power_remote_sink_exit(void);

/**
 * avm_power_file.c
 */
extern unsigned int avm_power_disp_loadrate;
extern int avm_power_file_init(void);
extern void avm_power_file_exit(void);
char *display_per_cpurate(char *txt, int txt_len, unsigned int cpuspowerrate);

/**
 * pm_info-Thread
 * avm_power_pminfo.c
 */
int pm_ressourceinfo_init(void);
void pm_ressourceinfo_exit(void);
int pm_ressourceinfo_limit_set_eth(struct _power_managment_ressource_info *pm_info, unsigned int eth_idx,
								   unsigned int force);
void pm_ressourceinfo_parse(char *line);
void pm_ressourceinfo_scriptparse(char *line);
void display_pminfostat(char *prefix);

extern struct _power_managment_ressource_info pm_ressourceinfo;

/**
 * kind of cpu
 */
enum _avm_cpu_mode {
	cpu_mode_default = 0,        /*--- no Remote-CPU exist - only Host-CPU ---*/
	cpu_mode_host    = 1,        /*--- Remote-CPU exist - Host-CPU ---*/
	cpu_mode_remote  = 2         /*--- Remote-CPU exist - Remote-CPU ---*/
};

extern enum _avm_cpu_mode avm_power_cpu_mode(void);

/**
 * notifier for avm_power_speedstep.c
 */
int avm_power_speedstep_init(void);
void avm_power_speedstep_exit(void);

/**
 * avm_power_pm_qos.c
 */
int avm_power_pm_qos_init(void);
void avm_power_pm_qos_exit(void);

/**
 * notifier for avm_power_temperature.c
 */
int avm_power_temperature(void);
int avm_power_temperature_init(void);
void avm_power_temperature_exit(void);

/**
 * notifier for avm_power_cpudidle.c
 */
int avm_power_cpuidle_init(void);
void avm_power_cpuidle_exit(void);
void avmevent_cpu_run_notify(void *context, enum _avm_event_id id);

/**
 * notifier for avm_power_telefon.c
 */
int avm_power_telefon_init(void);
void avm_power_telefon_exit(void);
void avm_power_dect_syncmode(unsigned int mode);

/**
 * avm_power_usb.c
 */
int avm_power_usb_init(void);
void avm_power_usb_exit(void);

/**
 * avm_power_dsl.c
 */
int avm_power_dsl_init(void);
void avm_power_dsl_exit(void);

/**
 * avm_power_wlan.c
 */
int avm_power_wlan_init(void);
void avm_power_wlan_exit(void);

/**
 * avm_power_remote_source.c
 */
int avm_power_remote_source_init(void);
void avm_power_remote_source_exit(void);

int avm_power_remote_source_temperature_init(void);
void avm_power_temperature_remote_source_exit(void);

/**
 */
static inline const char *pm_name_device(enum  _powermanagment_device device)
{
	switch (device) {
	case powerdevice_none:          return "powerdevice_none";
	case powerdevice_cpuclock:      return "powerdevice_cpuclock";
	case powerdevice_dspclock:      return "powerdevice_dspclock";
	case powerdevice_systemclock:   return "powerdevice_systemclock";
	case powerdevice_wlan:          return "powerdevice_wlan";
	case powerdevice_isdnnt:        return "powerdevice_isdnnt";
	case powerdevice_isdnte:        return "powerdevice_isdnte";
	case powerdevice_analog:        return "powerdevice_analog";
	case powerdevice_dect:          return "powerdevice_dect";
	case powerdevice_ethernet:      return "powerdevice_ethernet";
	case powerdevice_dsl:           return "powerdevice_dsl";
	case powerdevice_usb_host:      return "powerdevice_usb_host";
	case powerdevice_usb_client:    return "powerdevice_usb_client";
	case powerdevice_charge:        return "powerdevice_charge";
	case powerdevice_loadrate:      return "powerdevice_loadrate";
	case powerdevice_temperature:   return "powerdevice_temperature";
	case powerdevice_dectsync:      return "powerdevice_dectsync";
	case powerdevice_usb_host2:     return "powerdevice_usb_host2";
	case powerdevice_usb_host3:     return "powerdevice_usb_host3";
	case powerdevice_dsp_loadrate:  return "powerdevice_dsp_loadrate";
	case powerdevice_vdsp_loadrate: return "powerdevice_vdsp_loadrate";
	case powerdevice_lte:           return "powerdevice_lte";
	case powerdevice_loadrate2:     return "powerdevice_loadrate2";
	case powerdevice_dvbc:          return "powerdevice_dvbc";
	case powerdevice_plc:           return "powerdevice_plc";
	case powerdevice_maxdevices:    return "powerdevice_maxdevices";
	}
	return "powerdevice_unknown";
}

/**
 */
static inline int avm_power_write_find_special_char(char **p, char check_char)
{
	while (**p && **p != check_char) {
		(*p)++;
	}
	if (**p != check_char)
		return 1;
	(*p)++;
	return 0;
}
#endif /*--- #ifndef _AVM_POWER_H_ ---*/
