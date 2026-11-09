#ifndef _avm_event_gen_types_h_
#define _avm_event_gen_types_h_

#include <linux/types.h>
#ifdef	__KERNEL__
#include "linux/if.h"
#endif
#ifndef	AVM_LED_INTERNAL
#ifdef	__KERNEL__
#include <linux/avm_led_event.h>
#endif
#endif
#ifndef	AVM_LED_INTERNAL
#ifndef	__KERNEL__
#include "linux/avm_led_event.h"
#endif
#endif
#ifndef	__KERNEL__
#include <stdint.h>
#endif
#ifndef	__KERNEL__
#include <time.h>
#endif

#define MAX_EVENT_CLIENT_NAME_LEN 32
#define MAX_EVENT_SOURCE_NAME_LEN 32
#define MAX_EVENT_NODE_NAME_LEN 32
#define MAX_AVM_EVENT_SOURCES 32
#define MAX_AVM_EVENT_NODES 32
#define AVM_LED_STATUS_MAX_PARAMLEN 245
#define AVM_DIST_EVENT_VERSION 0x1001b
#define AVM_DIST_EVENT_VERSION__GET_MAJOR(v) ((v >> 16) & 0xffff)
#define AVM_DIST_EVENT_VERSION__GET_MINOR(v) (v & 0xffff)
#define AVM_EVENT_ETH_MAXPORTS 7U
#define AVM_EVENT_HAVE_ETH_MAXSPEED 1
#define AVM_EVENT_RPC_MAX_MESSAGE_SIZE 240
#define avm_event_push_button_gpio_low 0
#define avm_event_push_button_gpio_high 1
#define avm_event_push_button_key_1 2 
#define avm_event_push_button_key_2 3 
#define avm_event_push_button_key_3 4 
#define avm_event_push_button_key_4 5 
#define avm_event_push_button_key_5 6 
#define avm_event_push_button_key_6 7
#define avm_event_push_button_key_7 8
#define avm_event_push_button_key_8 9
#define avm_event_push_button_key_9 10
#define avm_event_push_button_key_10 11
#define avm_event_push_button_key_11 12
#define avm_event_push_button_key_12 13
#define avm_event_push_button_key_13 14
#define avm_event_push_button_key_14 15
#define avm_event_push_button_key_15 16
#define avm_event_push_button_key_16 17
#define avm_event_push_button_key_17 18
#define avm_event_push_button_key_18 19
#define avm_event_push_button_key_19 20
#define avm_event_push_button_key_20 21
#define avm_event_push_button_key_21 22
#define avm_event_push_button_key_22 23
#define avm_event_push_button_key_23 24
#define avm_event_mask_fieldentry unsigned long long
#define avm_event_mask_fieldentries ((avm_event_last - 1) / (sizeof(avm_event_mask_fieldentry) * 8) + 1)
#define AVM_EVENT_TFFS_NODE_NONE 0
#define AVM_EVENT_TFFS_NODE_ATOM 1
#define AVM_EVENT_TFFS_NODE_ARM 2
#define AVM_EVENT_TFFS_NODE_ANY 255
#define AVM_EVENT_CHKPNT_ID_ARM 1
#define AVM_EVENT_CHKPNT_ID_ATOM 2

#ifdef   __KERNEL__
#ifdef   AVM_EVENT__INTERNAL
static inline const char *get_enum__led_event_name(enum _led_event led) { return ""; }
#endif
#endif

enum __avm_event_cmd {
	avm_event_cmd_register = 0,
	avm_event_cmd_release = 1,
	avm_event_cmd_source_register = 2,
	avm_event_cmd_source_release = 3,
	avm_event_cmd_source_trigger = 4,
	avm_event_cmd_trigger = 5,
	avm_event_cmd_undef = 6,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table___avm_event_cmd[] = {
	{ .name = "avm_event_cmd_register", .value = 0 },
	{ .name = "avm_event_cmd_release", .value = 1 },
	{ .name = "avm_event_cmd_source_register", .value = 2 },
	{ .name = "avm_event_cmd_source_release", .value = 3 },
	{ .name = "avm_event_cmd_source_trigger", .value = 4 },
	{ .name = "avm_event_cmd_trigger", .value = 5 },
	{ .name = "avm_event_cmd_undef", .value = 6 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_event_ethernet_speed {
	avm_event_ethernet_speed_no_link = 0,
	avm_event_ethernet_speed_10M = 1,
	avm_event_ethernet_speed_100M = 2,
	avm_event_ethernet_speed_1G = 3,
	avm_event_ethernet_speed_2_5G = 4,
	avm_event_ethernet_speed_5G = 5,
	avm_event_ethernet_speed_10G = 6,
	avm_event_ethernet_speed_error = 7,
	avm_event_ethernet_speed_items = 8,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_event_ethernet_speed[] = {
	{ .name = "avm_event_ethernet_speed_no_link", .value = 0 },
	{ .name = "avm_event_ethernet_speed_10M", .value = 1 },
	{ .name = "avm_event_ethernet_speed_100M", .value = 2 },
	{ .name = "avm_event_ethernet_speed_1G", .value = 3 },
	{ .name = "avm_event_ethernet_speed_2_5G", .value = 4 },
	{ .name = "avm_event_ethernet_speed_5G", .value = 5 },
	{ .name = "avm_event_ethernet_speed_10G", .value = 6 },
	{ .name = "avm_event_ethernet_speed_error", .value = 7 },
	{ .name = "avm_event_ethernet_speed_items", .value = 8 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_event_id {
	avm_event_id_wlan_client_status = 0,
	avm_event_id_wlan_event = 1,
	avm_event_id_wlan_power = 2,
	avm_event_id_autoprov = 7,
	avm_event_id_usb_status = 8,
	avm_event_id_dsl_get_arch_kernel = 11,
	avm_event_id_dsl_set_arch = 12,
	avm_event_id_dsl_get_arch = 13,
	avm_event_id_dsl_set = 14,
	avm_event_id_dsl_get = 15,
	avm_event_id_dsl_status = 16,
	avm_event_id_dsl_connect_status = 17,
	avm_event_id_push_button = 19,
	avm_event_id_telefon_wlan_command = 20,
	avm_event_id_capiotcp_startstop = 21,
	avm_event_id_telefon_up = 22,
	avm_event_id_reboot_req = 23,
	avm_event_id_appl_status = 24,
	avm_event_id_led_status = 25,
	avm_event_id_led_info = 26,
	avm_event_id_telefonprofile = 27,
	avm_event_id_temperature = 28,
	avm_event_id_cpu_idle = 29,
	avm_event_id_powermanagment_status = 30,
	avm_event_id_powerline_status = 31,
	avm_event_id_ethernet_connect_status = 33,
	avm_event_id_powermanagment_remote = 34,
	avm_event_id_log = 35,
	avm_event_id_remotewatchdog = 36,
	avm_event_id_rpc = 37,
	avm_event_id_remotepcmlink = 38,
	avm_event_id_piglet = 39,
	avm_event_id_pm_ressourceinfo_status = 40,
	avm_event_id_telephony_missed_call = 41,
	avm_event_id_telephony_tam_call = 42,
	avm_event_id_telephony_fax_received = 43,
	avm_event_id_internet_new_ip = 44,
	avm_event_id_firmware_update_available = 45,
	avm_event_id_smarthome_switch_status = 46,
	avm_event_id_telephony_incoming_call = 47,
	avm_event_id_mass_storage_mount = 48,
	avm_event_id_mass_storage_unmount = 49,
	avm_event_id_checkpoint = 50,
	avm_event_id_cpu_run = 51,
	avm_event_id_ambient_brightness = 52,
	avm_event_id_fax_status_change = 53,
	avm_event_id_fax_file = 54,
	avm_event_id_telephony_call_finished = 55,
	avm_event_id_smarthome = 56,
	avm_event_id_fiber_set_arch = 57,
	avm_event_id_fiber_get_arch = 58,
	avm_event_id_user_source_notify = 63,
	avm_event_last,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_event_id[] = {
	{ .name = "avm_event_id_wlan_client_status", .value = 0 },
	{ .name = "avm_event_id_wlan_event", .value = 1 },
	{ .name = "avm_event_id_wlan_power", .value = 2 },
	{ .name = "avm_event_id_autoprov", .value = 7 },
	{ .name = "avm_event_id_usb_status", .value = 8 },
	{ .name = "avm_event_id_dsl_get_arch_kernel", .value = 11 },
	{ .name = "avm_event_id_dsl_set_arch", .value = 12 },
	{ .name = "avm_event_id_dsl_get_arch", .value = 13 },
	{ .name = "avm_event_id_dsl_set", .value = 14 },
	{ .name = "avm_event_id_dsl_get", .value = 15 },
	{ .name = "avm_event_id_dsl_status", .value = 16 },
	{ .name = "avm_event_id_dsl_connect_status", .value = 17 },
	{ .name = "avm_event_id_push_button", .value = 19 },
	{ .name = "avm_event_id_telefon_wlan_command", .value = 20 },
	{ .name = "avm_event_id_capiotcp_startstop", .value = 21 },
	{ .name = "avm_event_id_telefon_up", .value = 22 },
	{ .name = "avm_event_id_reboot_req", .value = 23 },
	{ .name = "avm_event_id_appl_status", .value = 24 },
	{ .name = "avm_event_id_led_status", .value = 25 },
	{ .name = "avm_event_id_led_info", .value = 26 },
	{ .name = "avm_event_id_telefonprofile", .value = 27 },
	{ .name = "avm_event_id_temperature", .value = 28 },
	{ .name = "avm_event_id_cpu_idle", .value = 29 },
	{ .name = "avm_event_id_powermanagment_status", .value = 30 },
	{ .name = "avm_event_id_powerline_status", .value = 31 },
	{ .name = "avm_event_id_ethernet_connect_status", .value = 33 },
	{ .name = "avm_event_id_powermanagment_remote", .value = 34 },
	{ .name = "avm_event_id_log", .value = 35 },
	{ .name = "avm_event_id_remotewatchdog", .value = 36 },
	{ .name = "avm_event_id_rpc", .value = 37 },
	{ .name = "avm_event_id_remotepcmlink", .value = 38 },
	{ .name = "avm_event_id_piglet", .value = 39 },
	{ .name = "avm_event_id_pm_ressourceinfo_status", .value = 40 },
	{ .name = "avm_event_id_telephony_missed_call", .value = 41 },
	{ .name = "avm_event_id_telephony_tam_call", .value = 42 },
	{ .name = "avm_event_id_telephony_fax_received", .value = 43 },
	{ .name = "avm_event_id_internet_new_ip", .value = 44 },
	{ .name = "avm_event_id_firmware_update_available", .value = 45 },
	{ .name = "avm_event_id_smarthome_switch_status", .value = 46 },
	{ .name = "avm_event_id_telephony_incoming_call", .value = 47 },
	{ .name = "avm_event_id_mass_storage_mount", .value = 48 },
	{ .name = "avm_event_id_mass_storage_unmount", .value = 49 },
	{ .name = "avm_event_id_checkpoint", .value = 50 },
	{ .name = "avm_event_id_cpu_run", .value = 51 },
	{ .name = "avm_event_id_ambient_brightness", .value = 52 },
	{ .name = "avm_event_id_fax_status_change", .value = 53 },
	{ .name = "avm_event_id_fax_file", .value = 54 },
	{ .name = "avm_event_id_telephony_call_finished", .value = 55 },
	{ .name = "avm_event_id_smarthome", .value = 56 },
	{ .name = "avm_event_id_fiber_set_arch", .value = 57 },
	{ .name = "avm_event_id_fiber_get_arch", .value = 58 },
	{ .name = "avm_event_id_user_source_notify", .value = 63 },
	{ .name = "avm_event_last", .value = avm_event_last },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_event_push_button_key {
	avm_event_push_button_wlan_on_off = 2,
	avm_event_push_button_wlan_wps = 4,
	avm_event_push_button_wlan_standby = 32,
	avm_event_push_button_wlan_wps_station = 33,
	avm_event_push_button_dect_paging = 11,
	avm_event_push_button_dect_pairing = 13,
	avm_event_push_button_dect_on_off = 42,
	avm_event_push_button_dect_standby = 43,
	avm_event_push_button_power_set_factory = 7,
	avm_event_push_button_power_set_factory_kernel = 8,
	avm_event_push_button_power_on_off = 51,
	avm_event_push_button_power_standby = 52,
	avm_event_push_button_power_socket_on_off = 53,
	avm_event_push_button_tools_profiling = 61,
	avm_event_push_button_plc_on_off = 71,
	avm_event_push_button_plc_pairing = 72,
	avm_event_push_button_led_standby = 81,
	avm_event_push_button_2fa_success = 91,
	avm_event_push_button_lte_wakeup = 92,
	avm_event_push_button_plc_pairing_off = 93,
	avm_event_push_button_wlan_wps_off = 94,
	avm_event_push_button_dect_pairing_off = 95,
	avm_event_push_button_nexus_pairing_off = 96,
	avm_event_push_button_nexus_pairing = 97,
	avm_event_push_button_wlan_wps_station_off = 98,
	avm_event_push_button_nexus_pairing_box = 99,
	avm_event_push_button_wlan_sta_wps_no_connect = 100,
	avm_event_push_button_wlan_sta_wps_no_connect_off = 101,
	avm_event_push_button_connect_methode1 = 102,
	avm_event_push_button_connect_methode2 = 103,
	avm_event_push_button_connect_methode3 = 104,
	avm_event_push_button_connect_methode4 = 105,
	avm_event_push_button_connect_methode5 = 106,
	avm_event_push_button_connect_methode6 = 107,
	avm_event_push_button_connect_off = 108,
	avm_event_push_button_connect_timeout = 109,
	avm_event_push_button_connect_error = 110,
	avm_event_push_button_connect_success = 111,
	avm_event_push_button_connect_started = 112,
	avm_event_push_button_lan_pairing = 113,
	avm_event_push_button_lan_pairing_off = 114,
	avm_event_push_button_last,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_event_push_button_key[] = {
	{ .name = "avm_event_push_button_wlan_on_off", .value = 2 },
	{ .name = "avm_event_push_button_wlan_wps", .value = 4 },
	{ .name = "avm_event_push_button_wlan_standby", .value = 32 },
	{ .name = "avm_event_push_button_wlan_wps_station", .value = 33 },
	{ .name = "avm_event_push_button_dect_paging", .value = 11 },
	{ .name = "avm_event_push_button_dect_pairing", .value = 13 },
	{ .name = "avm_event_push_button_dect_on_off", .value = 42 },
	{ .name = "avm_event_push_button_dect_standby", .value = 43 },
	{ .name = "avm_event_push_button_power_set_factory", .value = 7 },
	{ .name = "avm_event_push_button_power_set_factory_kernel", .value = 8 },
	{ .name = "avm_event_push_button_power_on_off", .value = 51 },
	{ .name = "avm_event_push_button_power_standby", .value = 52 },
	{ .name = "avm_event_push_button_power_socket_on_off", .value = 53 },
	{ .name = "avm_event_push_button_tools_profiling", .value = 61 },
	{ .name = "avm_event_push_button_plc_on_off", .value = 71 },
	{ .name = "avm_event_push_button_plc_pairing", .value = 72 },
	{ .name = "avm_event_push_button_led_standby", .value = 81 },
	{ .name = "avm_event_push_button_2fa_success", .value = 91 },
	{ .name = "avm_event_push_button_lte_wakeup", .value = 92 },
	{ .name = "avm_event_push_button_plc_pairing_off", .value = 93 },
	{ .name = "avm_event_push_button_wlan_wps_off", .value = 94 },
	{ .name = "avm_event_push_button_dect_pairing_off", .value = 95 },
	{ .name = "avm_event_push_button_nexus_pairing_off", .value = 96 },
	{ .name = "avm_event_push_button_nexus_pairing", .value = 97 },
	{ .name = "avm_event_push_button_wlan_wps_station_off", .value = 98 },
	{ .name = "avm_event_push_button_nexus_pairing_box", .value = 99 },
	{ .name = "avm_event_push_button_wlan_sta_wps_no_connect", .value = 100 },
	{ .name = "avm_event_push_button_wlan_sta_wps_no_connect_off", .value = 101 },
	{ .name = "avm_event_push_button_connect_methode1", .value = 102 },
	{ .name = "avm_event_push_button_connect_methode2", .value = 103 },
	{ .name = "avm_event_push_button_connect_methode3", .value = 104 },
	{ .name = "avm_event_push_button_connect_methode4", .value = 105 },
	{ .name = "avm_event_push_button_connect_methode5", .value = 106 },
	{ .name = "avm_event_push_button_connect_methode6", .value = 107 },
	{ .name = "avm_event_push_button_connect_off", .value = 108 },
	{ .name = "avm_event_push_button_connect_timeout", .value = 109 },
	{ .name = "avm_event_push_button_connect_error", .value = 110 },
	{ .name = "avm_event_push_button_connect_success", .value = 111 },
	{ .name = "avm_event_push_button_connect_started", .value = 112 },
	{ .name = "avm_event_push_button_lan_pairing", .value = 113 },
	{ .name = "avm_event_push_button_lan_pairing_off", .value = 114 },
	{ .name = "avm_event_push_button_last", .value = avm_event_push_button_last },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_logtype {
	local_panic = 0,
	local_crash = 1,
	remote_panic = 2,
	remote_crash = 3,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_logtype[] = {
	{ .name = "local_panic", .value = 0 },
	{ .name = "local_crash", .value = 1 },
	{ .name = "remote_panic", .value = 2 },
	{ .name = "remote_crash", .value = 3 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_piglettype {
	piglet_tdm_down = 0,
	piglet_tdm_ready = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_piglettype[] = {
	{ .name = "piglet_tdm_down", .value = 0 },
	{ .name = "piglet_tdm_ready", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_remote_wdt_cmd {
	wdt_register = 0, /*--- RemoteCPU: param1: time to trigger ---*/ 
	wdt_release = 1, /*--- RemoteCPU ---*/ 
	wdt_trigger = 2, /*--- RemoteCPU: trigger ---*/ 
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_remote_wdt_cmd[] = {
	{ .name = "wdt_register", .value = 0 },
	{ .name = "wdt_release", .value = 1 },
	{ .name = "wdt_trigger", .value = 2 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_remotepcmlinktype {
	rpcmlink_register = 0,
	rpcmlink_release = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_remotepcmlinktype[] = {
	{ .name = "rpcmlink_register", .value = 0 },
	{ .name = "rpcmlink_release", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _avm_rpctype {
	command_to_arm = 0,
	command_to_atom = 1,
	reply_to_arm = 2,
	reply_to_atom = 3,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__avm_rpctype[] = {
	{ .name = "command_to_arm", .value = 0 },
	{ .name = "command_to_atom", .value = 1 },
	{ .name = "reply_to_arm", .value = 2 },
	{ .name = "reply_to_atom", .value = 3 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _cputype {
	host_cpu = 0,
	remote_cpu = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__cputype[] = {
	{ .name = "host_cpu", .value = 0 },
	{ .name = "remote_cpu", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _powermanagment_device {
	powerdevice_none = 0,
	powerdevice_cpuclock = 1, /*---  power_rate in % Bezug: NormFrequenz 212 MHz  ---*/ 
	powerdevice_dspclock = 2, /*---  power_rate in % Bezug: NormFrequenz 250 MHz  ---*/ 
	powerdevice_systemclock = 3, /*---  power_rate in % Bezug: NormFrequenz 150 MHz  ---*/ 
	powerdevice_wlan = 4, /*---  power_rate in % Maximal-Last  ---*/ 
	powerdevice_isdnnt = 5, /*---  power_rate 0 oder 100 % (Ebene 1 aktiv)   ---*/ 
	powerdevice_isdnte = 6, /*---  power_rate 0 oder 100 % (Ebene 1 aktiv)   ---*/ 
	powerdevice_analog = 7, /*---  power_rate 100 % pro abgehobenen Telefon  ---*/ 
	powerdevice_dect = 8, /*---  power_rate in % Maximal-Last  ---*/ 
	powerdevice_ethernet = 9, /*---  power_rate 100 % pro aktiven Port  ---*/ 
	powerdevice_dsl = 10, /*---  power_rate in % Maximal-Last (????)  ---*/ 
	powerdevice_usb_host = 11, /*---  power_rate in Milli-Ampere  ---*/ 
	powerdevice_usb_client = 12, /*---  power_rate 100 % der Maximal-Last  ---*/ 
	powerdevice_charge = 13, /*---  power_rate in Milli-Watt  ---*/ 
	powerdevice_loadrate = 14, /*---  power_rate in % (100 - % Idle-Wert) falls SMP: je 8 Bit eine CPU  ---*/ 
	powerdevice_temperature = 15, /*---  power_rate in Grad Celcius  ---*/ 
	powerdevice_dectsync = 16, /*---  power_rate clks_per_jiffies ---*/ 
	powerdevice_usb_host2 = 17, /*---  power_rate in Milli-Ampere  ---*/ 
	powerdevice_usb_host3 = 18, /*---  power_rate in Milli-Ampere  ---*/ 
	powerdevice_dsp_loadrate = 19, /*---  (ADSL/VDSL-)DSP power_rate in % (100 - % Idle-Wert)  ---*/ 
	powerdevice_vdsp_loadrate = 20, /*---  Voice-DSP power_rate in % (100 - % Idle-Wert)  ---*/ 
	powerdevice_lte = 21, /*---  power_rate in Milliwatt  ---*/ 
	powerdevice_loadrate2 = 22, /*---  Remote CPU power_rate in % falls SMP: je 8 Bit eine CPU ---*/ 
	powerdevice_dvbc = 23, /*---  power_rate in % ---*/ 
	powerdevice_plc = 24, /*---  power_rate in % ---*/ 
	powerdevice_maxdevices = 25,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__powermanagment_device[] = {
	{ .name = "powerdevice_none", .value = 0 },
	{ .name = "powerdevice_cpuclock", .value = 1 },
	{ .name = "powerdevice_dspclock", .value = 2 },
	{ .name = "powerdevice_systemclock", .value = 3 },
	{ .name = "powerdevice_wlan", .value = 4 },
	{ .name = "powerdevice_isdnnt", .value = 5 },
	{ .name = "powerdevice_isdnte", .value = 6 },
	{ .name = "powerdevice_analog", .value = 7 },
	{ .name = "powerdevice_dect", .value = 8 },
	{ .name = "powerdevice_ethernet", .value = 9 },
	{ .name = "powerdevice_dsl", .value = 10 },
	{ .name = "powerdevice_usb_host", .value = 11 },
	{ .name = "powerdevice_usb_client", .value = 12 },
	{ .name = "powerdevice_charge", .value = 13 },
	{ .name = "powerdevice_loadrate", .value = 14 },
	{ .name = "powerdevice_temperature", .value = 15 },
	{ .name = "powerdevice_dectsync", .value = 16 },
	{ .name = "powerdevice_usb_host2", .value = 17 },
	{ .name = "powerdevice_usb_host3", .value = 18 },
	{ .name = "powerdevice_dsp_loadrate", .value = 19 },
	{ .name = "powerdevice_vdsp_loadrate", .value = 20 },
	{ .name = "powerdevice_lte", .value = 21 },
	{ .name = "powerdevice_loadrate2", .value = 22 },
	{ .name = "powerdevice_dvbc", .value = 23 },
	{ .name = "powerdevice_plc", .value = 24 },
	{ .name = "powerdevice_maxdevices", .value = 25 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum _powermanagment_status_type {
	dsl_status = 0,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table__powermanagment_status_type[] = {
	{ .name = "dsl_status", .value = 0 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_event_sel {
	INPUT_AUTH_1_OS_A = 0,
	INPUT_AUTH_1_SK_A = 1,
	INPUT_AUTH_1_D = 2,
	INPUT_AUTH_3_A = 3,
	INPUT_AUTH_3_D = 4,
	INPUT_DE_AUTH_STATION = 5, /*---  A station sent DeAuthentication message  ---*/ 
	INPUT_ASSOC_REQ_CHECK = 6,
	INPUT_ASSOC_REQ_A = 7,
	INPUT_ASSOC_REQ_D = 8,
	INPUT_ASSOC_REQ_SEC_D = 9,
	INPUT_RE_ASSOC_REQ_CHECK = 10,
	INPUT_RE_ASSOC_REQ_A = 11,
	INPUT_RE_ASSOC_REQ_D = 12,
	INPUT_RE_ASSOC_REQ_SEC_D = 13,
	INPUT_DIS_ASSOC_STATION = 14, /*---  A station sent Disassociation message  ---*/ 
	INPUT_CLASS_3 = 15,
	INPUT_AUTH_TIMEOUT = 16,
	INPUT_DE_AUTH_MNG_UNICAST = 17, /*---  Management unicast DeAuthentication  - by console command  ---*/ 
	INPUT_DE_AUTH_MNG_BROADCAST = 18, /*---  Management broadcast DeAuthentication  - by console command  ---*/ 
	INPUT_DIS_ASSOC_MNG_UNICAST = 19, /*---  Management unicast Disassociation  - by console command  ---*/ 
	INPUT_DIS_ASSOC_MNG_BROADCAST = 20, /*---  Management broadcast Disassociation  - by console command  ---*/ 
	INPUT_MAC_AUTHORIZE = 21, /*---  Management broadcast Disassociation  - by console command  ---*/ 
	INPUT_MAC_DE_AUTHORIZE = 22, /*---  Management broadcast Disassociation  - by console command  ---*/ 
	INPUT_WDS_LINK_UP = 23, /*---  WDS link establihed  ---*/ 
	INPUT_WDS_LINK_DOWN = 24, /*---  WDS link lost  ---*/ 
	INPUT_FRAME_TX_COMPLETE = 25, /*---  Frame TX complete  ---*/ 
	INPUT_MADWIFI_WRONG_PSK = 26, /*---  Atheros (7270) registration error due wrong WPA key  ---*/ 
	INPUT_WPS = 27,
	INPUT_MINI = 28,
	INPUT_RADAR = 29,
	INPUT_WPS_ENROLLEE = 30,
	INPUT_STA = 31,
	INPUT_GREENAP_PS = 32,
	INPUT_EAP_AUTHORIZED = 33, /*---  EAP has authorized client  ---*/ 
	INPUT_MWO_INTERFERENCE = 34, /*---  Detect microwave and switch to HT20  ---*/ 
	INPUT_AUTH_EXPIRED = 35, /*---  Authentication expired, deauthenticate station  ---*/ 
	INPUT_COEXIST_SWITCH = 36, /*---  Switch channel width due to 802.11 Coexistence feature  ---*/ 
	INPUT_STA_ASSOC = 37, /*---  Station uplink association events  ---*/ 
	INPUT_STA_AUTH = 38, /*---  Station uplink authentication events  ---*/ 
	INPUT_STA_AUTHORIZATION = 39, /*---  Station uplink authorization events  ---*/ 
	INPUT_WDS_NO_TIAGGR = 40, /*---  Warning, no TI G++ aggregation support  ---*/ 
	INPUT_MAX_NODE_REACHED = 41, /*---  Max. number of connected stations reached  ---*/ 
	INPUT_RADAR_DFS_WAIT = 42, /*---  Start/stop of DFS wait period  ---*/ 
	INPUT_INTERFERENCE_CHAN_CHANGE = 43, /*---  Channel interference detection ---*/ 
	INPUT_RADAR_END = 44, /*---  Channel interference ended  ---*/ 
	INPUT_RADAR_DFS_WAIT_PRECAC = 45, /*---  Start of DFS/PreCAC wait period  ---*/ 
};
typedef enum wlan_event_sel wlan_event;

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_event_sel[] = {
	{ .name = "INPUT_AUTH_1_OS_A", .value = 0 },
	{ .name = "INPUT_AUTH_1_SK_A", .value = 1 },
	{ .name = "INPUT_AUTH_1_D", .value = 2 },
	{ .name = "INPUT_AUTH_3_A", .value = 3 },
	{ .name = "INPUT_AUTH_3_D", .value = 4 },
	{ .name = "INPUT_DE_AUTH_STATION", .value = 5 },
	{ .name = "INPUT_ASSOC_REQ_CHECK", .value = 6 },
	{ .name = "INPUT_ASSOC_REQ_A", .value = 7 },
	{ .name = "INPUT_ASSOC_REQ_D", .value = 8 },
	{ .name = "INPUT_ASSOC_REQ_SEC_D", .value = 9 },
	{ .name = "INPUT_RE_ASSOC_REQ_CHECK", .value = 10 },
	{ .name = "INPUT_RE_ASSOC_REQ_A", .value = 11 },
	{ .name = "INPUT_RE_ASSOC_REQ_D", .value = 12 },
	{ .name = "INPUT_RE_ASSOC_REQ_SEC_D", .value = 13 },
	{ .name = "INPUT_DIS_ASSOC_STATION", .value = 14 },
	{ .name = "INPUT_CLASS_3", .value = 15 },
	{ .name = "INPUT_AUTH_TIMEOUT", .value = 16 },
	{ .name = "INPUT_DE_AUTH_MNG_UNICAST", .value = 17 },
	{ .name = "INPUT_DE_AUTH_MNG_BROADCAST", .value = 18 },
	{ .name = "INPUT_DIS_ASSOC_MNG_UNICAST", .value = 19 },
	{ .name = "INPUT_DIS_ASSOC_MNG_BROADCAST", .value = 20 },
	{ .name = "INPUT_MAC_AUTHORIZE", .value = 21 },
	{ .name = "INPUT_MAC_DE_AUTHORIZE", .value = 22 },
	{ .name = "INPUT_WDS_LINK_UP", .value = 23 },
	{ .name = "INPUT_WDS_LINK_DOWN", .value = 24 },
	{ .name = "INPUT_FRAME_TX_COMPLETE", .value = 25 },
	{ .name = "INPUT_MADWIFI_WRONG_PSK", .value = 26 },
	{ .name = "INPUT_WPS", .value = 27 },
	{ .name = "INPUT_MINI", .value = 28 },
	{ .name = "INPUT_RADAR", .value = 29 },
	{ .name = "INPUT_WPS_ENROLLEE", .value = 30 },
	{ .name = "INPUT_STA", .value = 31 },
	{ .name = "INPUT_GREENAP_PS", .value = 32 },
	{ .name = "INPUT_EAP_AUTHORIZED", .value = 33 },
	{ .name = "INPUT_MWO_INTERFERENCE", .value = 34 },
	{ .name = "INPUT_AUTH_EXPIRED", .value = 35 },
	{ .name = "INPUT_COEXIST_SWITCH", .value = 36 },
	{ .name = "INPUT_STA_ASSOC", .value = 37 },
	{ .name = "INPUT_STA_AUTH", .value = 38 },
	{ .name = "INPUT_STA_AUTHORIZATION", .value = 39 },
	{ .name = "INPUT_WDS_NO_TIAGGR", .value = 40 },
	{ .name = "INPUT_MAX_NODE_REACHED", .value = 41 },
	{ .name = "INPUT_RADAR_DFS_WAIT", .value = 42 },
	{ .name = "INPUT_INTERFERENCE_CHAN_CHANGE", .value = 43 },
	{ .name = "INPUT_RADAR_END", .value = 44 },
	{ .name = "INPUT_RADAR_DFS_WAIT_PRECAC", .value = 45 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_info_sel {
	STATUS_SUCCESSFUL = 0,
	STATUS_UNSPECIFIED = 1,
	STATUS_AUTH_NO_LONGER_VALID = 2,
	STATUS_DE_AUTH_STATION_IS_LEAVING = 3,
	STATUS_DIS_ASSOC_INACTIVITY = 4,
	STATUS_DIS_ASSOC_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOC_STATIONS = 5,
	STATUS_CLASS_2_FROM_NON_AUTH_STATION = 6,
	STATUS_CLASS_3_FROM_NON_ASSOC_STATION = 7,
	STATUS_DIS_ASSOC_STATION_IS_LEAVING = 8,
	STATUS_STATION_REQUESTING_ASSOC_IS_NOT_AUTH = 9,
	STATUS_CAPABILITIES_FAILURE = 10,
	STATUS_REASSOC_DENIED_UNABLE_TO_CONFIRM_ASSOC = 11,
	STATUS_ASSOC_DENIED_OUT_OF_SCOPE = 12,
	STATUS_ALGO_IS_NOT_SUPPORTTED = 13,
	STATUS_AUTH_TRANSC_NUMBER_OUT_OF_SEQUENCE = 14,
	STATUS_AUTH_REJ_CHALLENGE_FAILURE = 15,
	STATUS_AUTH_REJ_TIMEOUT = 16,
	STATUS_ASSOC_DENIED_UNABLE_TO_HANDLE_ADDITIONAL_ASSOC = 17,
	STATUS_ASSOC_DENIED_RATE_FAILURE = 18,
	STATUS_ASSOC_DENIED_PREAMBLE_FAILURE = 19,
	STATUS_ASSOC_DENIED_PBCC_FAILURE = 20,
	STATUS_ASSOC_DENIED_AGILITY_FAILURE = 21,
	STATUS_DEAUTH_TX_COMPLETE_TIMEOUT = 22,
	STATUS_DEAUTH_TX_COMPLETE_OK = 23,
	STATUS_MAX = 24,
	STATUS_AUTHOR_SECMODE_FAILURE = 25,
	STATUS_ASSOC_DENIED_MODE_FAILURE = 26,
	STATUS_INVALID = 65535,
};
typedef enum wlan_info_sel wlan_info;

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_info_sel[] = {
	{ .name = "STATUS_SUCCESSFUL", .value = 0 },
	{ .name = "STATUS_UNSPECIFIED", .value = 1 },
	{ .name = "STATUS_AUTH_NO_LONGER_VALID", .value = 2 },
	{ .name = "STATUS_DE_AUTH_STATION_IS_LEAVING", .value = 3 },
	{ .name = "STATUS_DIS_ASSOC_INACTIVITY", .value = 4 },
	{ .name = "STATUS_DIS_ASSOC_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOC_STATIONS", .value = 5 },
	{ .name = "STATUS_CLASS_2_FROM_NON_AUTH_STATION", .value = 6 },
	{ .name = "STATUS_CLASS_3_FROM_NON_ASSOC_STATION", .value = 7 },
	{ .name = "STATUS_DIS_ASSOC_STATION_IS_LEAVING", .value = 8 },
	{ .name = "STATUS_STATION_REQUESTING_ASSOC_IS_NOT_AUTH", .value = 9 },
	{ .name = "STATUS_CAPABILITIES_FAILURE", .value = 10 },
	{ .name = "STATUS_REASSOC_DENIED_UNABLE_TO_CONFIRM_ASSOC", .value = 11 },
	{ .name = "STATUS_ASSOC_DENIED_OUT_OF_SCOPE", .value = 12 },
	{ .name = "STATUS_ALGO_IS_NOT_SUPPORTTED", .value = 13 },
	{ .name = "STATUS_AUTH_TRANSC_NUMBER_OUT_OF_SEQUENCE", .value = 14 },
	{ .name = "STATUS_AUTH_REJ_CHALLENGE_FAILURE", .value = 15 },
	{ .name = "STATUS_AUTH_REJ_TIMEOUT", .value = 16 },
	{ .name = "STATUS_ASSOC_DENIED_UNABLE_TO_HANDLE_ADDITIONAL_ASSOC", .value = 17 },
	{ .name = "STATUS_ASSOC_DENIED_RATE_FAILURE", .value = 18 },
	{ .name = "STATUS_ASSOC_DENIED_PREAMBLE_FAILURE", .value = 19 },
	{ .name = "STATUS_ASSOC_DENIED_PBCC_FAILURE", .value = 20 },
	{ .name = "STATUS_ASSOC_DENIED_AGILITY_FAILURE", .value = 21 },
	{ .name = "STATUS_DEAUTH_TX_COMPLETE_TIMEOUT", .value = 22 },
	{ .name = "STATUS_DEAUTH_TX_COMPLETE_OK", .value = 23 },
	{ .name = "STATUS_MAX", .value = 24 },
	{ .name = "STATUS_AUTHOR_SECMODE_FAILURE", .value = 25 },
	{ .name = "STATUS_ASSOC_DENIED_MODE_FAILURE", .value = 26 },
	{ .name = "STATUS_INVALID", .value = 65535 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_firmware_type {
	box_firmware = 0,
	fritz_fon_firmware = 1,
	fritz_dect_repeater = 2,
	fritz_plug_switch = 3,
	fritz_hkr = 4,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_firmware_type[] = {
	{ .name = "box_firmware", .value = 0 },
	{ .name = "fritz_fon_firmware", .value = 1 },
	{ .name = "fritz_dect_repeater", .value = 2 },
	{ .name = "fritz_plug_switch", .value = 3 },
	{ .name = "fritz_hkr", .value = 4 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_internet_new_ip_param_sel {
	avm_event_internet_new_ip_v4 = 0,
	avm_event_internet_new_ip_v6 = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_internet_new_ip_param_sel[] = {
	{ .name = "avm_event_internet_new_ip_v4", .value = 0 },
	{ .name = "avm_event_internet_new_ip_v6", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_led_id {
	avm_logical_led_inval = 0,
	avm_logical_led_ppp = 2,
	avm_logical_led_error = 17,
	avm_logical_led_pots = 13,
	avm_logical_led_info = 7,
	avm_logical_led_traffic = 18,
	avm_logical_led_freecall = 16,
	avm_logical_led_avmusbwlan = 19,
	avm_logical_led_sip = 14,
	avm_logical_led_mwi = 20,
	avm_logical_led_fest_mwi = 21,
	avm_logical_led_isdn_d = 12,
	avm_logical_led_isdn_b1 = 10,
	avm_logical_led_isdn_b2 = 11,
	avm_logical_led_lan = 3,
	avm_logical_led_lan1 = 15,
	avm_logical_led_adsl = 1,
	avm_logical_led_power = 8,
	avm_logical_led_usb = 5,
	avm_logical_led_wifi = 4,
	avm_logical_led_last = 99,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_led_id[] = {
	{ .name = "avm_logical_led_inval", .value = 0 },
	{ .name = "avm_logical_led_ppp", .value = 2 },
	{ .name = "avm_logical_led_error", .value = 17 },
	{ .name = "avm_logical_led_pots", .value = 13 },
	{ .name = "avm_logical_led_info", .value = 7 },
	{ .name = "avm_logical_led_traffic", .value = 18 },
	{ .name = "avm_logical_led_freecall", .value = 16 },
	{ .name = "avm_logical_led_avmusbwlan", .value = 19 },
	{ .name = "avm_logical_led_sip", .value = 14 },
	{ .name = "avm_logical_led_mwi", .value = 20 },
	{ .name = "avm_logical_led_fest_mwi", .value = 21 },
	{ .name = "avm_logical_led_isdn_d", .value = 12 },
	{ .name = "avm_logical_led_isdn_b1", .value = 10 },
	{ .name = "avm_logical_led_isdn_b2", .value = 11 },
	{ .name = "avm_logical_led_lan", .value = 3 },
	{ .name = "avm_logical_led_lan1", .value = 15 },
	{ .name = "avm_logical_led_adsl", .value = 1 },
	{ .name = "avm_logical_led_power", .value = 8 },
	{ .name = "avm_logical_led_usb", .value = 5 },
	{ .name = "avm_logical_led_wifi", .value = 4 },
	{ .name = "avm_logical_led_last", .value = 99 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_msg_type {
	avm_event_source_register_type = 0,
	avm_event_source_unregister_type = 1,
	avm_event_source_notifier_type = 2,
	avm_event_remote_source_trigger_request_type = 3,
	avm_event_ping_type = 4,
	avm_event_tffs_type = 5,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_msg_type[] = {
	{ .name = "avm_event_source_register_type", .value = 0 },
	{ .name = "avm_event_source_unregister_type", .value = 1 },
	{ .name = "avm_event_source_notifier_type", .value = 2 },
	{ .name = "avm_event_remote_source_trigger_request_type", .value = 3 },
	{ .name = "avm_event_ping_type", .value = 4 },
	{ .name = "avm_event_tffs_type", .value = 5 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_powermanagment_remote_action {
	avm_event_powermanagment_ressourceinfo = 0,
	avm_event_powermanagment_activatepowermode = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_powermanagment_remote_action[] = {
	{ .name = "avm_event_powermanagment_ressourceinfo", .value = 0 },
	{ .name = "avm_event_powermanagment_activatepowermode", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_switch_type {
	binary = 0,
	percent = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_switch_type[] = {
	{ .name = "binary", .value = 0 },
	{ .name = "percent", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_telephony_param_sel {
	avm_event_telephony_params_name = 0,
	avm_event_telephony_params_msn_name = 1,
	avm_event_telephony_params_calling = 2,
	avm_event_telephony_params_called = 3,
	avm_event_telephony_params_duration = 4,
	avm_event_telephony_params_port = 5,
	avm_event_telephony_params_portname = 6,
	avm_event_telephony_params_id = 7,
	avm_event_telephony_params_tam_path = 8,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_telephony_param_sel[] = {
	{ .name = "avm_event_telephony_params_name", .value = 0 },
	{ .name = "avm_event_telephony_params_msn_name", .value = 1 },
	{ .name = "avm_event_telephony_params_calling", .value = 2 },
	{ .name = "avm_event_telephony_params_called", .value = 3 },
	{ .name = "avm_event_telephony_params_duration", .value = 4 },
	{ .name = "avm_event_telephony_params_port", .value = 5 },
	{ .name = "avm_event_telephony_params_portname", .value = 6 },
	{ .name = "avm_event_telephony_params_id", .value = 7 },
	{ .name = "avm_event_telephony_params_tam_path", .value = 8 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_tffs_call_type {
	avm_event_tffs_call_open = 0,
	avm_event_tffs_call_close = 1,
	avm_event_tffs_call_read = 2,
	avm_event_tffs_call_write = 3,
	avm_event_tffs_call_cleanup = 4,
	avm_event_tffs_call_reindex = 5,
	avm_event_tffs_call_info = 6,
	avm_event_tffs_call_init = 7,
	avm_event_tffs_call_deinit = 8,
	avm_event_tffs_call_notify = 9,
	avm_event_tffs_call_paniclog = 10,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_tffs_call_type[] = {
	{ .name = "avm_event_tffs_call_open", .value = 0 },
	{ .name = "avm_event_tffs_call_close", .value = 1 },
	{ .name = "avm_event_tffs_call_read", .value = 2 },
	{ .name = "avm_event_tffs_call_write", .value = 3 },
	{ .name = "avm_event_tffs_call_cleanup", .value = 4 },
	{ .name = "avm_event_tffs_call_reindex", .value = 5 },
	{ .name = "avm_event_tffs_call_info", .value = 6 },
	{ .name = "avm_event_tffs_call_init", .value = 7 },
	{ .name = "avm_event_tffs_call_deinit", .value = 8 },
	{ .name = "avm_event_tffs_call_notify", .value = 9 },
	{ .name = "avm_event_tffs_call_paniclog", .value = 10 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_tffs_notify_event {
	avm_event_tffs_notify_clear = 0,
	avm_event_tffs_notify_update = 1,
	avm_event_tffs_notify_reinit = 2,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_tffs_notify_event[] = {
	{ .name = "avm_event_tffs_notify_clear", .value = 0 },
	{ .name = "avm_event_tffs_notify_update", .value = 1 },
	{ .name = "avm_event_tffs_notify_reinit", .value = 2 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum avm_event_tffs_open_mode {
	avm_event_tffs_mode_read = 0,
	avm_event_tffs_mode_write = 1,
	avm_event_tffs_mode_panic = 2,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_avm_event_tffs_open_mode[] = {
	{ .name = "avm_event_tffs_mode_read", .value = 0 },
	{ .name = "avm_event_tffs_mode_write", .value = 1 },
	{ .name = "avm_event_tffs_mode_panic", .value = 2 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum ePLCState {
	PLCStateRunningNotConnected = 0,
	PLCStateRunningConnected = 1,
	PLCStateNotRunning = 2,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_ePLCState[] = {
	{ .name = "PLCStateRunningNotConnected", .value = 0 },
	{ .name = "PLCStateRunningConnected", .value = 1 },
	{ .name = "PLCStateNotRunning", .value = 2 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum fax_file_event_type {
	FAX_FILE_EVENT_NEW_FILE = 0,
	FAX_FILE_EVENT_REMOVED_FILE = 1,
	FAX_FILE_EVENT_READ = 2,
	FAX_FILE_EVENT_UNREAD = 3,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_fax_file_event_type[] = {
	{ .name = "FAX_FILE_EVENT_NEW_FILE", .value = 0 },
	{ .name = "FAX_FILE_EVENT_REMOVED_FILE", .value = 1 },
	{ .name = "FAX_FILE_EVENT_READ", .value = 2 },
	{ .name = "FAX_FILE_EVENT_UNREAD", .value = 3 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum fax_receive_mode {
	FAX_RECEIVE_MODE_OFF = 0,
	FAX_RECEIVE_MODE_MAIL_ONLY = 1,
	FAX_RECEIVE_MODE_STORE_ONLY = 2,
	FAX_RECEIVE_MODE_MAIL_AND_STORE = 3,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_fax_receive_mode[] = {
	{ .name = "FAX_RECEIVE_MODE_OFF", .value = 0 },
	{ .name = "FAX_RECEIVE_MODE_MAIL_ONLY", .value = 1 },
	{ .name = "FAX_RECEIVE_MODE_STORE_ONLY", .value = 2 },
	{ .name = "FAX_RECEIVE_MODE_MAIL_AND_STORE", .value = 3 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum fax_storage_dest {
	FAX_STORAGE_INTERNAL = 0, /*--- z. B. interner Speicher ---*/ 
	FAX_STORAGE_EXTERNAL = 1, /*--- z. B. USB ---*/ 
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_fax_storage_dest[] = {
	{ .name = "FAX_STORAGE_INTERNAL", .value = 0 },
	{ .name = "FAX_STORAGE_EXTERNAL", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_event_id {
	CLIENT_STATE_CHANGE = 0,
	CLIENT_CONNECT_INFO = 1,
	WLAN_EVENT_SCAN = 2,
	CLIENT_STATE_IDLE = 3,
	WLAN_RADIO_RECOVERY = 4,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_event_id[] = {
	{ .name = "CLIENT_STATE_CHANGE", .value = 0 },
	{ .name = "CLIENT_CONNECT_INFO", .value = 1 },
	{ .name = "WLAN_EVENT_SCAN", .value = 2 },
	{ .name = "CLIENT_STATE_IDLE", .value = 3 },
	{ .name = "WLAN_RADIO_RECOVERY", .value = 4 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_event_radio_recovery_state {
	WLAN_EVENT_RADIO_RECOVERY_RADIO_DOWN = 0,
	WLAN_EVENT_RADIO_RECOVERY_RADIO_UP = 1,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_event_radio_recovery_state[] = {
	{ .name = "WLAN_EVENT_RADIO_RECOVERY_RADIO_DOWN", .value = 0 },
	{ .name = "WLAN_EVENT_RADIO_RECOVERY_RADIO_UP", .value = 1 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_event_scan_type {
	WLAN_EVENT_SCAN_FINISHED = 0,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_event_scan_type[] = {
	{ .name = "WLAN_EVENT_SCAN_FINISHED", .value = 0 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_info_special {
	STATUS_SUCCESS = 1,
	STATUS_FAILURE = 2,
	STATUS_TIMEOUT = 3,
	STATUS_WPS_START = 4,
	STATUS_WPS_DISCOVERY = 5,
	STATUS_WPS_OVERLAP = 6,
	STATUS_WPS_STOP = 7,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_info_special[] = {
	{ .name = "STATUS_SUCCESS", .value = 1 },
	{ .name = "STATUS_FAILURE", .value = 2 },
	{ .name = "STATUS_TIMEOUT", .value = 3 },
	{ .name = "STATUS_WPS_START", .value = 4 },
	{ .name = "STATUS_WPS_DISCOVERY", .value = 5 },
	{ .name = "STATUS_WPS_OVERLAP", .value = 6 },
	{ .name = "STATUS_WPS_STOP", .value = 7 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/

enum wlan_sm_states {
	WLAN_SM_STATE_IDLE = 0,
	WLAN_SM_STATE_AUTH_KEY = 1,
	WLAN_SM_STATE_AUTHENTICATED = 2,
	WLAN_SM_STATE_WAIT_FOR_ASS_RES = 3,
	WLAN_SM_STATE_ASSOCIATED = 4,
	WLAN_SM_STATE_AUTHORIZED = 5,
	WLAN_SM_STATE_DEAUTHENTICATE = 6,
	WLAN_SM_NUM_STATES = 7,
};

#ifdef WIRESHARK_PLUGIN
struct enumInformation enum_table_wlan_sm_states[] = {
	{ .name = "WLAN_SM_STATE_IDLE", .value = 0 },
	{ .name = "WLAN_SM_STATE_AUTH_KEY", .value = 1 },
	{ .name = "WLAN_SM_STATE_AUTHENTICATED", .value = 2 },
	{ .name = "WLAN_SM_STATE_WAIT_FOR_ASS_RES", .value = 3 },
	{ .name = "WLAN_SM_STATE_ASSOCIATED", .value = 4 },
	{ .name = "WLAN_SM_STATE_AUTHORIZED", .value = 5 },
	{ .name = "WLAN_SM_STATE_DEAUTHENTICATE", .value = 6 },
	{ .name = "WLAN_SM_NUM_STATES", .value = 7 },
	{0} // last element
};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/


char *get_enum___avm_event_cmd_name (enum __avm_event_cmd value);
char *get_enum__avm_event_ethernet_speed_name (enum _avm_event_ethernet_speed value);
char *get_enum__avm_event_id_name (enum _avm_event_id value);
char *get_enum__avm_event_push_button_key_name (enum _avm_event_push_button_key value);
char *get_enum__avm_logtype_name (enum _avm_logtype value);
char *get_enum__avm_piglettype_name (enum _avm_piglettype value);
char *get_enum__avm_remote_wdt_cmd_name (enum _avm_remote_wdt_cmd value);
char *get_enum__avm_remotepcmlinktype_name (enum _avm_remotepcmlinktype value);
char *get_enum__avm_rpctype_name (enum _avm_rpctype value);
char *get_enum__cputype_name (enum _cputype value);
char *get_enum__powermanagment_device_name (enum _powermanagment_device value);
char *get_enum__powermanagment_status_type_name (enum _powermanagment_status_type value);
char *get_enum_wlan_event_sel_name (enum wlan_event_sel value);
char *get_enum_wlan_info_sel_name (enum wlan_info_sel value);
char *get_enum_avm_event_firmware_type_name (enum avm_event_firmware_type value);
char *get_enum_avm_event_internet_new_ip_param_sel_name (enum avm_event_internet_new_ip_param_sel value);
char *get_enum_avm_event_led_id_name (enum avm_event_led_id value);
char *get_enum_avm_event_msg_type_name (enum avm_event_msg_type value);
char *get_enum_avm_event_powermanagment_remote_action_name (enum avm_event_powermanagment_remote_action value);
char *get_enum_avm_event_switch_type_name (enum avm_event_switch_type value);
char *get_enum_avm_event_telephony_param_sel_name (enum avm_event_telephony_param_sel value);
char *get_enum_avm_event_tffs_call_type_name (enum avm_event_tffs_call_type value);
char *get_enum_avm_event_tffs_notify_event_name (enum avm_event_tffs_notify_event value);
char *get_enum_avm_event_tffs_open_mode_name (enum avm_event_tffs_open_mode value);
char *get_enum_ePLCState_name (enum ePLCState value);
char *get_enum_fax_file_event_type_name (enum fax_file_event_type value);
char *get_enum_fax_receive_mode_name (enum fax_receive_mode value);
char *get_enum_fax_storage_dest_name (enum fax_storage_dest value);
char *get_enum_wlan_event_id_name (enum wlan_event_id value);
char *get_enum_wlan_event_radio_recovery_state_name (enum wlan_event_radio_recovery_state value);
char *get_enum_wlan_event_scan_type_name (enum wlan_event_scan_type value);
char *get_enum_wlan_info_special_name (enum wlan_info_special value);
char *get_enum_wlan_sm_states_name (enum wlan_sm_states value);
/* pos 9500 (37) */
struct avm_event_powermanagment_remote_ressourceinfo {
	enum _powermanagment_device device;
	unsigned int power_rate;
} __attribute__((packed));

/* pos 9500 (2) */
struct avm_event_telephony_string {
	unsigned int length;
	unsigned char string[0];
} __attribute__((packed));

/* pos 9500 (8) */
struct wlan_event_data_client_common {
	uint8_t mac[6];
	char iface[IFNAMSIZ + 1];
	char initiator[16 + 1];
} __attribute__((packed));

/* pos 9500 (9) */
struct wlan_event_data_scan_common {
	char iface[IFNAMSIZ + 1];
	char initiator[16 + 1];
} __attribute__((packed));

/* pos 9525 (0) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum avm_event_telephony_param_sel'
-----------------------------------------------------------------------------------*/
union avm_event_telephony_call_params {
	/*--- select by [select-variable] one of (avm_event_telephony_params_name avm_event_telephony_params_msn_name avm_event_telephony_params_portname avm_event_telephony_params_tam_path) ---*/
	struct avm_event_telephony_string string;
	/*--- select by [select-variable] one of (avm_event_telephony_params_calling avm_event_telephony_params_called) ---*/
	unsigned char number[32];
	/*--- select by [select-variable] one of (avm_event_telephony_params_duration) ---*/
	unsigned int duration;
	/*--- select by [select-variable] one of (avm_event_telephony_params_port) ---*/
	unsigned char port;
	/*--- select by [select-variable] one of (avm_event_telephony_params_id) ---*/
	unsigned int id;
};

/* pos 9525 (4) */
struct wlan_event_data_client_connect_info {
	struct wlan_event_data_client_common common;
	uint8_t info_context;
	uint8_t reason;
	uint32_t max_node_count;
	uint16_t ieee80211_code;
} __attribute__((packed));

/* pos 9525 (3) */
struct wlan_event_data_client_state_change {
	struct wlan_event_data_client_common common;
	uint8_t state;
} __attribute__((packed));

/* pos 9525 (6) */
struct wlan_event_data_client_state_idle {
	struct wlan_event_data_client_common common;
	uint8_t idle;
} __attribute__((packed));

/* pos 9525 (7) */
struct wlan_event_data_radio_recovery {
	int radio_id;
	enum wlan_event_radio_recovery_state state;
} __attribute__((packed));

/* pos 9525 (5) */
struct wlan_event_data_scan_event_info {
	struct wlan_event_data_scan_common common;
	enum wlan_event_scan_type event_type;
} __attribute__((packed));

/* pos 9550 (38) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum _powermanagment_status_type'
-----------------------------------------------------------------------------------*/
union __powermanagment_status_union {
	/*--- select by [select-variable] one of (dsl_status) ---*/
	unsigned int dsl_status;
};

/* pos 9550 (40) */
struct _avm_event_telephony_missed_call_params {
	enum avm_event_telephony_param_sel id;
	union avm_event_telephony_call_params params;
} __attribute__((packed));

/* pos 9550 (35) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum avm_event_internet_new_ip_param_sel'
-----------------------------------------------------------------------------------*/
union avm_event_internet_new_ip_param {
	/*--- select by [select-variable] one of (avm_event_internet_new_ip_v4) ---*/
	unsigned char ipv4[4];
	/*--- select by [select-variable] one of (avm_event_internet_new_ip_v6) ---*/
	unsigned char ipv6[16];
};

/* pos 9550 (36) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum avm_event_powermanagment_remote_action'
-----------------------------------------------------------------------------------*/
union avm_event_powermanagment_remote_union {
	/*--- select by [select-variable] one of (avm_event_powermanagment_ressourceinfo) ---*/
	struct avm_event_powermanagment_remote_ressourceinfo ressourceinfo;
	/*--- select by [select-variable] one of (avm_event_powermanagment_activatepowermode) ---*/
	char powermode[32];
};

/* pos 9550 (41) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum wlan_event_sel'
-----------------------------------------------------------------------------------*/
union avm_event_wlan_client_status_u1 {
	/*--- select by [select-variable] one of (INPUT_RADAR_DFS_WAIT) ---*/
	unsigned int sub_event;
	/*--- select by [select-variable] one of (INPUT_MAC_AUTHORIZE) ---*/
	unsigned int active_rate;
	/*--- select by [select-variable] one of (INPUT_EAP_AUTHORIZED) ---*/
	unsigned int active_rate1;
	/*--- select by [select-variable] one of (INPUT_MADWIFI_WRONG_PSK) ---*/
	unsigned int active_rate2;
	/*--- select by [select-variable] one of (INPUT_AUTH_EXPIRED) ---*/
	unsigned int active_rate3;
	/*--- select by [select-variable] one of (INPUT_STA) ---*/
	unsigned int active_rate4;
	/*--- select by [select-variable] one of (INPUT_WDS_LINK_UP) ---*/
	unsigned int active_rate5;
	/*--- select by [select-variable] one of (INPUT_WDS_LINK_DOWN) ---*/
	unsigned int active_rate6;
	/*--- select by [select-variable] one of (INPUT_RADAR) ---*/
	unsigned int radar_chan;
	/*--- select by [select-variable] one of (INPUT_RADAR_DFS_WAIT_PRECAC) ---*/
	unsigned int radar_chan1;
	/*--- select by [select-variable] one of (INPUT_GREENAP_PS) ---*/
	unsigned int green_ap_ps_state;
	/*--- select by [select-variable] one of (INPUT_COEXIST_SWITCH) ---*/
	unsigned int coexist_ht40_state;
	/*--- select by [select-variable] one of (INPUT_MAX_NODE_REACHED) ---*/
	unsigned int max_node_count;
	/*--- select by [select-variable] one of (INPUT_INTERFERENCE_CHAN_CHANGE) ---*/
	unsigned int channel;
	/*--- select by [select-variable] one of (INPUT_AUTH_1_OS_A) ---*/
	unsigned int dummy0;
};

/* pos 9550 (42) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum wlan_event_sel'
-----------------------------------------------------------------------------------*/
union avm_event_wlan_client_status_u2 {
	/*--- select by [select-variable] one of (INPUT_RADAR) ---*/
	unsigned int radar_freq;
	/*--- select by [select-variable] one of (INPUT_RADAR_DFS_WAIT_PRECAC) ---*/
	unsigned int radar_freq1;
	/*--- select by [select-variable] one of (INPUT_AUTH_1_D) ---*/
	unsigned int wlan_mode;
	/*--- select by [select-variable] one of (INPUT_AUTH_1_OS_A) ---*/
	unsigned int dummy0;
};

/* pos 9550 (43) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum wlan_event_sel'
-----------------------------------------------------------------------------------*/
union avm_event_wlan_client_status_u3 {
	/*--- select by [select-variable] one of (INPUT_RADAR_DFS_WAIT) ---*/
	unsigned int radar_time;
	/*--- select by [select-variable] one of (INPUT_RADAR) ---*/
	unsigned int flags;
	/*--- select by [select-variable] one of (INPUT_AUTH_1_OS_A) ---*/
	unsigned int dummy0;
};

/* pos 9550 (44) */
struct avm_event_wlan_credentials {
	char ssid[33];
	unsigned char bssid[6];
	char key[65];
	unsigned int security;
} __attribute__((packed));

/* pos 9550 (52) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum wlan_event_id'
-----------------------------------------------------------------------------------*/
union wlan_event_data {
	/*--- select by [select-variable] one of (CLIENT_STATE_CHANGE) ---*/
	struct wlan_event_data_client_state_change client_state_change;
	/*--- select by [select-variable] one of (CLIENT_CONNECT_INFO) ---*/
	struct wlan_event_data_client_connect_info client_connect_info;
	/*--- select by [select-variable] one of (WLAN_EVENT_SCAN) ---*/
	struct wlan_event_data_scan_event_info scan_event_info;
	/*--- select by [select-variable] one of (CLIENT_STATE_IDLE) ---*/
	struct wlan_event_data_client_state_idle client_state_idle;
	/*--- select by [select-variable] one of (WLAN_RADIO_RECOVERY) ---*/
	struct wlan_event_data_radio_recovery radio_recovery;
};

/* pos 9575 (1) */
struct cpmac_port {
	uint8_t cable;
	uint8_t link;
	uint8_t speed100;
	uint8_t fullduplex;
	enum _avm_event_ethernet_speed speed;
	enum _avm_event_ethernet_speed maxspeed;
} __attribute__((packed));

/* pos 9600 (5) */
struct _cpmac_event_struct {
	unsigned int ports;
	struct cpmac_port port[AVM_EVENT_ETH_MAXPORTS];
} __attribute__((packed));

/* pos 9600 (29) */
struct avm_event_ambient_brightness {
	unsigned int value;
	unsigned int maxvalue;
} __attribute__((packed));

/* pos 9600 (3) */
struct avm_event_boykott {
	struct {} dummy;
} __attribute__((packed));

/* pos 9600 (27) */
struct avm_event_checkpoint {
	uint32_t node_id;
	uint64_t checkpoints;
} __attribute__((packed));

/* pos 9600 (11) */
struct avm_event_cpu_idle {
	unsigned char cpu_idle;
	unsigned char dsl_dsp_idle;
	unsigned char voice_dsp_idle;
	unsigned char mem_strictlyused;
	unsigned char mem_cacheused;
	unsigned char mem_physfree;
	enum _cputype cputype;
} __attribute__((packed));

/* pos 9600 (28) */
struct avm_event_cpu_run {
	enum _cputype cputype;
	unsigned char cpu_run[4];
} __attribute__((packed));

/* pos 9600 (31) */
struct avm_event_fax_file {
	enum fax_file_event_type action;
	time_t date;
} __attribute__((packed));

/* pos 9600 (30) */
struct avm_event_fax_status {
	enum fax_receive_mode fax_receive_mode;
	enum fax_storage_dest fax_storage_dest;
	unsigned int dirname_length;
	unsigned char dirname[0];
} __attribute__((packed));

/* pos 9600 (22) */
struct avm_event_firmware_update_available {
	enum avm_event_firmware_type type;
	unsigned int version_length;
	unsigned char version[0];
} __attribute__((packed));

/* pos 9600 (23) */
struct avm_event_internet_new_ip {
	enum avm_event_internet_new_ip_param_sel sel;
	union avm_event_internet_new_ip_param params;
} __attribute__((packed));

/* pos 9600 (7) */
struct avm_event_led_info {
	unsigned int mode;
	unsigned int param1;
	unsigned int param2;
	unsigned int gpio_driver_type;
	unsigned int gpio;
	unsigned int pos;
	char name[MAX_EVENT_SOURCE_NAME_LEN];
} __attribute__((packed));

/* pos 9600 (6) */
struct avm_event_led_status {
	enum _led_event led;
	unsigned int state;
	unsigned int param_len;
	unsigned char params[AVM_LED_STATUS_MAX_PARAMLEN];
} __attribute__((packed));

/* pos 9600 (13) */
struct avm_event_log {
	enum _avm_logtype logtype;
	unsigned int loglen;
	unsigned int logpointer;
	unsigned int checksum;
	unsigned int rebootflag;
} __attribute__((packed));

/* pos 9600 (25) */
struct avm_event_mass_storage_mount {
	unsigned long long size;
	unsigned long long free;
	unsigned int name_length;
	unsigned char name[0];
} __attribute__((packed));

/* pos 9600 (26) */
struct avm_event_mass_storage_unmount {
	unsigned int name_length;
	unsigned char name[0];
} __attribute__((packed));

/* pos 9600 (32) */
struct avm_event_piglet {
	enum _avm_piglettype type;
} __attribute__((packed));

/* pos 9600 (17) */
struct avm_event_pm_info_stat {
	unsigned char reserved1;
	unsigned char rate_sumact;
	unsigned char rate_sumcum;
	unsigned char rate_systemact;
	unsigned char rate_systemcum;
	unsigned char system_status;
	unsigned char rate_dspact;
	unsigned char rate_dspcum;
	unsigned char rate_wlanact;
	unsigned char rate_wlancum;
	unsigned char wlan_devices;
	unsigned char wlan_status;
	unsigned char rate_ethact;
	unsigned char rate_ethcum;
	unsigned short eth_status;
	unsigned char rate_abact;
	unsigned char rate_abcum;
	unsigned short isdn_status;
	unsigned char rate_dectact;
	unsigned char rate_dectcum;
	unsigned char rate_battchargeact;
	unsigned char rate_battchargecum;
	unsigned char dect_status;
	unsigned char rate_usbhostact;
	unsigned char rate_usbhostcum;
	unsigned char usb_status;
	signed char act_temperature;
	signed char min_temperature;
	signed char max_temperature;
	signed char avg_temperature;
	unsigned char rate_lteact;
	unsigned char rate_ltecum;
	unsigned char rate_dvbcact;
	unsigned char rate_dvbccum;
	unsigned char rate_plcact;
	unsigned char rate_plccum;
} __attribute__((packed));

/* pos 9600 (12) */
struct avm_event_powermanagment_remote {
	enum avm_event_powermanagment_remote_action remote_action;
	union avm_event_powermanagment_remote_union param;
} __attribute__((packed));

/* pos 9600 (10) */
struct avm_event_powermanagment_status {
	enum _powermanagment_status_type substatus;
	union __powermanagment_status_union param;
} __attribute__((packed));

/* pos 9600 (4) */
struct avm_event_push_button {
	enum _avm_event_push_button_key key;
	uint32_t pressed;
} __attribute__((packed));

/* pos 9600 (14) */
struct avm_event_remotepcmlink {
	enum _avm_remotepcmlinktype type;
	unsigned int sharedlen;
	unsigned int sharedpointer;
} __attribute__((packed));

/* pos 9600 (15) */
struct avm_event_remotewatchdog {
	enum _avm_remote_wdt_cmd cmd;
	char name[16];
	unsigned int param;
} __attribute__((packed));

/* pos 9600 (16) */
struct avm_event_rpc {
	enum _avm_rpctype type;
	unsigned int id;
	unsigned int length;
	unsigned char message[0];
} __attribute__((packed));

/* pos 9600 (34) */
struct avm_event_smarthome {
	uint16_t length;
	char ident[20];
	uint16_t type;
	struct {} payload;
} __attribute__((packed));

/* pos 9600 (24) */
struct avm_event_smarthome_switch_status {
	enum avm_event_switch_type type;
	unsigned int value;
	unsigned int ain_length;
	unsigned char ain[0];
} __attribute__((packed));

/* pos 9600 (33) */
struct avm_event_telefon_up {
	struct {} dummy;
} __attribute__((packed));

/* pos 9600 (8) */
struct avm_event_telefonprofile {
	unsigned int on;
} __attribute__((packed));

/* pos 9600 (21) */
struct avm_event_telephony_missed_call {
	unsigned int length;
	struct _avm_event_telephony_missed_call_params p[0];
} __attribute__((packed));

/* pos 9600 (9) */
struct avm_event_temperature {
	int temperature;
} __attribute__((packed));

/* pos 9600 (66) */
struct avm_event_tffs_cleanup {
	uint32_t dummy;
} __attribute__((packed));

/* pos 9600 (63) */
struct avm_event_tffs_close {
	uint32_t dummy;
} __attribute__((packed));

/* pos 9600 (70) */
struct avm_event_tffs_deinit {
	uint32_t dummy;
} __attribute__((packed));

/* pos 9600 (68) */
struct avm_event_tffs_info {
	uint32_t fill_level;
} __attribute__((packed));

/* pos 9600 (69) */
struct avm_event_tffs_init {
	int64_t mem_offset;
	uint32_t max_seg_size;
} __attribute__((packed));

/* pos 9600 (71) */
struct avm_event_tffs_notify {
	uint32_t id;
	enum avm_event_tffs_notify_event event;
} __attribute__((packed));

/* pos 9600 (62) */
struct avm_event_tffs_open {
	uint32_t id;
	enum avm_event_tffs_open_mode mode;
	uint32_t max_segment_size;
} __attribute__((packed));

/* pos 9600 (72) */
struct avm_event_tffs_paniclog {
	uint64_t buff_addr;
	uint64_t len;
	int32_t crc;
} __attribute__((packed));

/* pos 9600 (64) */
struct avm_event_tffs_read {
	uint64_t buff_addr;
	uint64_t len;
	uint32_t id;
	int32_t crc;
} __attribute__((packed));

/* pos 9600 (67) */
struct avm_event_tffs_reindex {
	uint32_t dummy;
} __attribute__((packed));

/* pos 9600 (65) */
struct avm_event_tffs_write {
	uint64_t buff_addr;
	uint64_t len;
	uint32_t id;
	uint32_t final;
	int32_t crc;
} __attribute__((packed));

/* pos 9600 (18) */
struct avm_event_wlan {
	char mac[6];
	union avm_event_wlan_client_status_u1 u1;
	wlan_event event;
	wlan_info info;
	enum wlan_sm_states status;
	union avm_event_wlan_client_status_u2 u2;
	char if_name[IFNAMSIZ];
	unsigned int ev_initiator;
	unsigned int ev_reason;
	unsigned int avm_capabilities;
	union avm_event_wlan_client_status_u3 u3;
	struct avm_event_wlan_credentials credentials;
} __attribute__((packed));

/* pos 9600 (19) */
struct avm_event_wlan_power {
	unsigned int power;
} __attribute__((packed));

/* pos 9600 (20) */
struct wlan_event_def {
	enum wlan_event_id event_id;
	union wlan_event_data event_data;
} __attribute__((packed));

/* pos 9650 (2) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum _avm_event_id'
-----------------------------------------------------------------------------------*/
union avm_event_data_union {
	/*--- select by [select-variable] one of (avm_event_id_appl_status) ---*/
	struct avm_event_boykott appl_status;
	/*--- select by [select-variable] one of (avm_event_id_autoprov) ---*/
	struct avm_event_boykott autoprov;
	/*--- select by [select-variable] one of (avm_event_id_capiotcp_startstop) ---*/
	struct avm_event_boykott capiotcp_startstop;
	/*--- select by [select-variable] one of (avm_event_id_dsl_connect_status) ---*/
	struct avm_event_boykott dsl_connect_status;
	/*--- select by [select-variable] one of (avm_event_id_dsl_get) ---*/
	struct avm_event_boykott dsl_get;
	/*--- select by [select-variable] one of (avm_event_id_dsl_get_arch) ---*/
	struct avm_event_boykott dsl_get_arch;
	/*--- select by [select-variable] one of (avm_event_id_dsl_set) ---*/
	struct avm_event_boykott dsl_set;
	/*--- select by [select-variable] one of (avm_event_id_dsl_set_arch) ---*/
	struct avm_event_boykott dsl_set_arch;
	/*--- select by [select-variable] one of (avm_event_id_dsl_status) ---*/
	struct avm_event_boykott dsl_status;
	/*--- select by [select-variable] one of (avm_event_id_powerline_status) ---*/
	struct avm_event_boykott powerline_status;
	/*--- select by [select-variable] one of (avm_event_id_reboot_req) ---*/
	struct avm_event_boykott reboot_req;
	/*--- select by [select-variable] one of (avm_event_id_telefon_wlan_command) ---*/
	struct avm_event_boykott telefon_wlan_command;
	/*--- select by [select-variable] one of (avm_event_id_usb_status) ---*/
	struct avm_event_boykott usb_status;
	/*--- select by [select-variable] one of (avm_event_id_user_source_notify) ---*/
	struct avm_event_boykott user_source_notify;
	/*--- select by [select-variable] one of (avm_event_id_push_button) ---*/
	struct avm_event_push_button push_button;
	/*--- select by [select-variable] one of (avm_event_id_ethernet_connect_status) ---*/
	struct _cpmac_event_struct cpmac;
	/*--- select by [select-variable] one of (avm_event_id_led_status) ---*/
	struct avm_event_led_status led_status;
	/*--- select by [select-variable] one of (avm_event_id_led_info) ---*/
	struct avm_event_led_info led_info;
	/*--- select by [select-variable] one of (avm_event_id_telefonprofile) ---*/
	struct avm_event_telefonprofile telefonprofile;
	/*--- select by [select-variable] one of (avm_event_id_temperature) ---*/
	struct avm_event_temperature temperature;
	/*--- select by [select-variable] one of (avm_event_id_powermanagment_status) ---*/
	struct avm_event_powermanagment_status powermanagment_status;
	/*--- select by [select-variable] one of (avm_event_id_cpu_idle) ---*/
	struct avm_event_cpu_idle cpu_idle;
	/*--- select by [select-variable] one of (avm_event_id_powermanagment_remote) ---*/
	struct avm_event_powermanagment_remote powermanagment_remote;
	/*--- select by [select-variable] one of (avm_event_id_log) ---*/
	struct avm_event_log id_log;
	/*--- select by [select-variable] one of (avm_event_id_remotepcmlink) ---*/
	struct avm_event_remotepcmlink id_remotepcmlink;
	/*--- select by [select-variable] one of (avm_event_id_remotewatchdog) ---*/
	struct avm_event_remotewatchdog id_remotewatchdog;
	/*--- select by [select-variable] one of (avm_event_id_rpc) ---*/
	struct avm_event_rpc id_rpc;
	/*--- select by [select-variable] one of (avm_event_id_pm_ressourceinfo_status) ---*/
	struct avm_event_pm_info_stat pm_info_stat;
	/*--- select by [select-variable] one of (avm_event_id_wlan_client_status) ---*/
	struct avm_event_wlan wlan;
	/*--- select by [select-variable] one of (avm_event_id_wlan_power) ---*/
	struct avm_event_wlan_power wlan_power;
	/*--- select by [select-variable] one of (avm_event_id_wlan_event) ---*/
	struct wlan_event_def wlan_event;
	/*--- select by [select-variable] one of (avm_event_id_telephony_missed_call) ---*/
	struct avm_event_telephony_missed_call telephony_missed_call;
	/*--- select by [select-variable] one of (avm_event_id_telephony_tam_call) ---*/
	struct avm_event_telephony_missed_call telephony_tam_call;
	/*--- select by [select-variable] one of (avm_event_id_telephony_fax_received) ---*/
	struct avm_event_telephony_missed_call telephony_fax_received;
	/*--- select by [select-variable] one of (avm_event_id_telephony_incoming_call) ---*/
	struct avm_event_telephony_missed_call telephony_incoming_call;
	/*--- select by [select-variable] one of (avm_event_id_telephony_call_finished) ---*/
	struct avm_event_telephony_missed_call telephony_call_finished;
	/*--- select by [select-variable] one of (avm_event_id_firmware_update_available) ---*/
	struct avm_event_firmware_update_available firmware_update_available;
	/*--- select by [select-variable] one of (avm_event_id_internet_new_ip) ---*/
	struct avm_event_internet_new_ip internet_new_ip;
	/*--- select by [select-variable] one of (avm_event_id_smarthome_switch_status) ---*/
	struct avm_event_smarthome_switch_status smarthome_switch_status;
	/*--- select by [select-variable] one of (avm_event_id_mass_storage_mount) ---*/
	struct avm_event_mass_storage_mount mass_storage_mount;
	/*--- select by [select-variable] one of (avm_event_id_mass_storage_unmount) ---*/
	struct avm_event_mass_storage_unmount mass_storage_unmount;
	/*--- select by [select-variable] one of (avm_event_id_checkpoint) ---*/
	struct avm_event_checkpoint checkpoint;
	/*--- select by [select-variable] one of (avm_event_id_cpu_run) ---*/
	struct avm_event_cpu_run cpu_run;
	/*--- select by [select-variable] one of (avm_event_id_ambient_brightness) ---*/
	struct avm_event_ambient_brightness ambient_brightness;
	/*--- select by [select-variable] one of (avm_event_id_fax_status_change) ---*/
	struct avm_event_fax_status fax_status;
	/*--- select by [select-variable] one of (avm_event_id_fax_file) ---*/
	struct avm_event_fax_file fax_file;
	/*--- select by [select-variable] one of (avm_event_id_piglet) ---*/
	struct avm_event_piglet piglet;
	/*--- select by [select-variable] one of (avm_event_id_telefon_up) ---*/
	struct avm_event_telefon_up telefon_up;
	/*--- select by [select-variable] one of (avm_event_id_smarthome) ---*/
	struct avm_event_smarthome smarthome;
	/*--- select by [select-variable] one of (avm_event_id_fiber_set_arch) ---*/
	struct avm_event_boykott fiber_set_arch;
	/*--- select by [select-variable] one of (avm_event_id_fiber_get_arch) ---*/
	struct avm_event_boykott fiber_get_arch;
};

/* pos 9700 (6) */
struct _avm_event_id_mask {
	avm_event_mask_fieldentry mask[avm_event_mask_fieldentries];
} __attribute__((packed));

/* pos 9700 (58) */
struct avm_event_data {
	enum _avm_event_id id;
	union avm_event_data_union data;
} __attribute__((packed));

/* pos 9700 (61) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum avm_event_tffs_call_type'
-----------------------------------------------------------------------------------*/
union avm_event_tffs_call_union {
	/*--- select by [select-variable] one of (avm_event_tffs_call_open) ---*/
	struct avm_event_tffs_open open;
	/*--- select by [select-variable] one of (avm_event_tffs_call_close) ---*/
	struct avm_event_tffs_close close;
	/*--- select by [select-variable] one of (avm_event_tffs_call_read) ---*/
	struct avm_event_tffs_read read;
	/*--- select by [select-variable] one of (avm_event_tffs_call_write) ---*/
	struct avm_event_tffs_write write;
	/*--- select by [select-variable] one of (avm_event_tffs_call_cleanup) ---*/
	struct avm_event_tffs_cleanup cleanup;
	/*--- select by [select-variable] one of (avm_event_tffs_call_reindex) ---*/
	struct avm_event_tffs_reindex reindex;
	/*--- select by [select-variable] one of (avm_event_tffs_call_info) ---*/
	struct avm_event_tffs_info info;
	/*--- select by [select-variable] one of (avm_event_tffs_call_init) ---*/
	struct avm_event_tffs_init init;
	/*--- select by [select-variable] one of (avm_event_tffs_call_deinit) ---*/
	struct avm_event_tffs_deinit deinit;
	/*--- select by [select-variable] one of (avm_event_tffs_call_notify) ---*/
	struct avm_event_tffs_notify notify;
	/*--- select by [select-variable] one of (avm_event_tffs_call_paniclog) ---*/
	struct avm_event_tffs_paniclog paniclog;
};

/* pos 9800 (2) */
struct _avm_event_cmd_param_register {
	struct _avm_event_id_mask mask;
	char Name[MAX_EVENT_CLIENT_NAME_LEN + 1];
} __attribute__((packed));

/* pos 9800 (3) */
struct _avm_event_cmd_param_release {
	char Name[MAX_EVENT_CLIENT_NAME_LEN + 1];
} __attribute__((packed));

/* pos 9800 (5) */
struct _avm_event_cmd_param_source_trigger {
	enum _avm_event_id id;
	unsigned int data_length;
} __attribute__((packed));

/* pos 9800 (4) */
struct _avm_event_cmd_param_trigger {
	enum _avm_event_id id;
} __attribute__((packed));

/* pos 9800 (53) */
struct avm_event_ping {
	uint32_t seq;
} __attribute__((packed));

/* pos 9800 (52) */
struct avm_event_remote_source_trigger_request {
	struct avm_event_data data;
} __attribute__((packed));

/* pos 9800 (51) */
struct avm_event_source_notifier {
	enum _avm_event_id id;
} __attribute__((packed));

/* pos 9800 (49) */
struct avm_event_source_register {
	struct _avm_event_id_mask id_mask;
	char name[MAX_EVENT_SOURCE_NAME_LEN];
} __attribute__((packed));

/* pos 9800 (50) */
struct avm_event_source_unregister {
	struct _avm_event_id_mask id_mask;
	char name[MAX_EVENT_SOURCE_NAME_LEN];
} __attribute__((packed));

/* pos 9800 (54) */
struct avm_event_tffs {
	uint32_t src_id;
	uint32_t dst_id;
	uint32_t seq_nr;
	uint32_t ack;
	uint64_t srv_handle;
	uint64_t clt_handle;
	int32_t result;
	enum avm_event_tffs_call_type type;
	union avm_event_tffs_call_union call;
} __attribute__((packed));

/* pos 9900 (1) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum __avm_event_cmd'
-----------------------------------------------------------------------------------*/
union _avm_event_cmd_param {
	/*--- select by [select-variable] one of (avm_event_cmd_register) ---*/
	struct _avm_event_cmd_param_register avm_event_cmd_param_register;
	/*--- select by [select-variable] one of (avm_event_cmd_release) ---*/
	struct _avm_event_cmd_param_release avm_event_cmd_param_release;
	/*--- select by [select-variable] one of (avm_event_cmd_trigger) ---*/
	struct _avm_event_cmd_param_trigger avm_event_cmd_param_trigger;
	/*--- select by [select-variable] one of (avm_event_cmd_source_register) ---*/
	struct _avm_event_cmd_param_register avm_event_cmd_param_source_register;
	/*--- select by [select-variable] one of (avm_event_cmd_source_trigger) ---*/
	struct _avm_event_cmd_param_source_trigger avm_event_cmd_param_source_trigger;
};

/* pos 9900 (0) */
struct _avm_event_header {
	enum _avm_event_id id;
} __attribute__((packed));

/* pos 9900 (48) */
/*-----------------------------------------------------------------------------------
	union entry is select by variable of type 'enum avm_event_msg_type'
-----------------------------------------------------------------------------------*/
union avm_event_message_union {
	/*--- select by [select-variable] one of (avm_event_source_register_type) ---*/
	struct avm_event_source_register source_register;
	/*--- select by [select-variable] one of (avm_event_source_unregister_type) ---*/
	struct avm_event_source_unregister source_unregister;
	/*--- select by [select-variable] one of (avm_event_source_notifier_type) ---*/
	struct avm_event_source_notifier source_notifier;
	/*--- select by [select-variable] one of (avm_event_remote_source_trigger_request_type) ---*/
	struct avm_event_remote_source_trigger_request remote_source_trigger_request;
	/*--- select by [select-variable] one of (avm_event_ping_type) ---*/
	struct avm_event_ping ping;
	/*--- select by [select-variable] one of (avm_event_tffs_type) ---*/
	struct avm_event_tffs tffs;
};

struct _avm_event_ambient_brightness {
	struct _avm_event_header event_header;
	unsigned int value;
	unsigned int maxvalue;
} __attribute__((packed));

struct _avm_event_checkpoint {
	struct _avm_event_header event_header;
	uint32_t node_id;
	uint64_t checkpoints;
} __attribute__((packed));

struct _avm_event_cmd {
	enum __avm_event_cmd cmd;
	union _avm_event_cmd_param param;
} __attribute__((packed));

struct _avm_event_cpu_idle {
	struct _avm_event_header event_header;
	unsigned char cpu_idle;
	unsigned char dsl_dsp_idle;
	unsigned char voice_dsp_idle;
	unsigned char mem_strictlyused;
	unsigned char mem_cacheused;
	unsigned char mem_physfree;
	enum _cputype cputype;
} __attribute__((packed));

struct _avm_event_cpu_run {
	struct _avm_event_header event_header;
	enum _cputype cputype;
	unsigned char cpu_run[4];
} __attribute__((packed));

struct _avm_event_fax_file {
	struct _avm_event_header header;
	enum fax_file_event_type action;
	time_t date;
} __attribute__((packed));

struct _avm_event_fax_status {
	struct _avm_event_header header;
	enum fax_receive_mode fax_receive_mode;
	enum fax_storage_dest fax_storage_dest;
	unsigned int dirname_length;
	unsigned char dirname[0];
} __attribute__((packed));

struct _avm_event_firmware_update_available {
	struct _avm_event_header header;
	enum avm_event_firmware_type type;
	unsigned int version_length;
	unsigned char version[0];
} __attribute__((packed));

struct _avm_event_internet_new_ip {
	struct _avm_event_header header;
	enum avm_event_internet_new_ip_param_sel sel;
	union avm_event_internet_new_ip_param params;
} __attribute__((packed));

struct _avm_event_led_info {
	struct _avm_event_header header;
	unsigned int mode;
	unsigned int param1;
	unsigned int param2;
	unsigned int gpio_driver_type;
	unsigned int gpio;
	unsigned int pos;
	char name[MAX_EVENT_SOURCE_NAME_LEN];
} __attribute__((packed));

struct _avm_event_led_status {
	struct _avm_event_header header;
	enum _led_event led;
	unsigned int state;
	unsigned int param_len;
	unsigned char params[AVM_LED_STATUS_MAX_PARAMLEN];
} __attribute__((packed));

struct _avm_event_log {
	struct _avm_event_header event_header;
	enum _avm_logtype logtype;
	unsigned int loglen;
	unsigned int logpointer;
	unsigned int checksum;
	unsigned int rebootflag;
} __attribute__((packed));

struct _avm_event_mass_storage_mount {
	struct _avm_event_header header;
	unsigned long long size;
	unsigned long long free;
	unsigned int name_length;
	unsigned char name[0];
} __attribute__((packed));

struct _avm_event_mass_storage_unmount {
	struct _avm_event_header header;
	unsigned int name_length;
	unsigned char name[0];
} __attribute__((packed));

struct _avm_event_piglet {
	struct _avm_event_header event_header;
	enum _avm_piglettype type;
} __attribute__((packed));

struct _avm_event_pm_info_stat {
	struct _avm_event_header header;
	unsigned char reserved1;
	unsigned char rate_sumact;
	unsigned char rate_sumcum;
	unsigned char rate_systemact;
	unsigned char rate_systemcum;
	unsigned char system_status;
	unsigned char rate_dspact;
	unsigned char rate_dspcum;
	unsigned char rate_wlanact;
	unsigned char rate_wlancum;
	unsigned char wlan_devices;
	unsigned char wlan_status;
	unsigned char rate_ethact;
	unsigned char rate_ethcum;
	unsigned short eth_status;
	unsigned char rate_abact;
	unsigned char rate_abcum;
	unsigned short isdn_status;
	unsigned char rate_dectact;
	unsigned char rate_dectcum;
	unsigned char rate_battchargeact;
	unsigned char rate_battchargecum;
	unsigned char dect_status;
	unsigned char rate_usbhostact;
	unsigned char rate_usbhostcum;
	unsigned char usb_status;
	signed char act_temperature;
	signed char min_temperature;
	signed char max_temperature;
	signed char avg_temperature;
	unsigned char rate_lteact;
	unsigned char rate_ltecum;
	unsigned char rate_dvbcact;
	unsigned char rate_dvbccum;
	unsigned char rate_plcact;
	unsigned char rate_plccum;
} __attribute__((packed));

struct _avm_event_powerline_status {
	struct _avm_event_header event_header;
	enum ePLCState status;
} __attribute__((packed));

struct _avm_event_powermanagment_remote {
	struct _avm_event_header header;
	enum avm_event_powermanagment_remote_action remote_action;
	union avm_event_powermanagment_remote_union param;
} __attribute__((packed));

struct _avm_event_powermanagment_remote_ressourceinfo {
	struct _avm_event_header header;
	enum _powermanagment_device device;
	unsigned int power_rate;
} __attribute__((packed));

struct _avm_event_powermanagment_status {
	struct _avm_event_header event_header;
	enum _powermanagment_status_type substatus;
	union __powermanagment_status_union param;
} __attribute__((packed));

struct _avm_event_push_button {
	enum _avm_event_id id;
	enum _avm_event_push_button_key key;
	uint32_t pressed;
} __attribute__((packed));

struct _avm_event_remotepcmlink {
	struct _avm_event_header event_header;
	enum _avm_remotepcmlinktype type;
	unsigned int sharedlen;
	unsigned int sharedpointer;
} __attribute__((packed));

struct _avm_event_remotewatchdog {
	struct _avm_event_header event_header;
	enum _avm_remote_wdt_cmd cmd;
	char name[16];
	unsigned int param;
} __attribute__((packed));

struct _avm_event_rpc {
	struct _avm_event_header event_header;
	enum _avm_rpctype type;
	unsigned int id;
	unsigned int length;
	unsigned char message[0];
} __attribute__((packed));

struct _avm_event_smarthome {
	struct _avm_event_header header;
	uint16_t length;
	char ident[20];
	uint16_t type;
	struct {} payload;
} __attribute__((packed));

struct _avm_event_smarthome_switch_status {
	struct _avm_event_header header;
	enum avm_event_switch_type type;
	unsigned int value;
	unsigned int ain_length;
	unsigned char ain[0];
} __attribute__((packed));

struct _avm_event_telefonprofile {
	struct _avm_event_header event_header;
	unsigned int on;
} __attribute__((packed));

struct _avm_event_telephony_missed_call {
	struct _avm_event_header header;
	unsigned int length;
	struct _avm_event_telephony_missed_call_params p[0];
} __attribute__((packed));

struct _avm_event_temperature {
	struct _avm_event_header event_header;
	int temperature;
} __attribute__((packed));

struct _avm_event_user_mode_source_notify {
	struct _avm_event_header header;
	enum _avm_event_id id;
} __attribute__((packed));

struct _avm_event_wlan {
	struct _avm_event_header header;
	char mac[6];
	unsigned int u1;
	unsigned int event;
	unsigned int info;
	unsigned int status;
	unsigned int u2;
	char if_name[IFNAMSIZ];
	unsigned int ev_initiator;
	unsigned int ev_reason;
	unsigned int avm_capabilities;
	unsigned int u3;
	struct avm_event_wlan_credentials credentials;
} __attribute__((packed));

struct avm_event_message {
	uint32_t length;
	uint32_t magic;
	uint32_t nonce;
	uint32_t flags;
	int32_t result;
	uint32_t transmitter_handle;
	uint32_t receiver_handle;
	enum avm_event_msg_type type;
	union avm_event_message_union message;
} __attribute__((packed));

struct avm_event_powerline_status {
	enum ePLCState status;
} __attribute__((packed));

struct avm_event_unserialised {
	uint64_t evnt_id;
	uint32_t data_len;
	unsigned char data;
} __attribute__((packed));

struct avm_event_user_mode_source_notify {
	enum _avm_event_id id;
} __attribute__((packed));

struct cpmac_event_struct {
	struct _avm_event_header event_header;
	unsigned int ports;
	struct cpmac_port port[AVM_EVENT_ETH_MAXPORTS];
} __attribute__((packed));


#endif /*--- #ifndef _avm_event_gen_types_h_ ---*/
