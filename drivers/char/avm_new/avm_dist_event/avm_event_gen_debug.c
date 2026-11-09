#include "avm_event_gen_types.h"

char *get_enum___avm_event_cmd_name (enum __avm_event_cmd value) {
	switch(value) {
		default: return("__avm_event_cmd_unknown");
		case avm_event_cmd_register: return("avm_event_cmd_register");
		case avm_event_cmd_release: return("avm_event_cmd_release");
		case avm_event_cmd_source_register: return("avm_event_cmd_source_register");
		case avm_event_cmd_source_release: return("avm_event_cmd_source_release");
		case avm_event_cmd_source_trigger: return("avm_event_cmd_source_trigger");
		case avm_event_cmd_trigger: return("avm_event_cmd_trigger");
		case avm_event_cmd_undef: return("avm_event_cmd_undef");
	}
}

char *get_enum__avm_event_ethernet_speed_name (enum _avm_event_ethernet_speed value) {
	switch(value) {
		default: return("_avm_event_ethernet_speed_unknown");
		case avm_event_ethernet_speed_no_link: return("avm_event_ethernet_speed_no_link");
		case avm_event_ethernet_speed_10M: return("avm_event_ethernet_speed_10M");
		case avm_event_ethernet_speed_100M: return("avm_event_ethernet_speed_100M");
		case avm_event_ethernet_speed_1G: return("avm_event_ethernet_speed_1G");
		case avm_event_ethernet_speed_2_5G: return("avm_event_ethernet_speed_2_5G");
		case avm_event_ethernet_speed_5G: return("avm_event_ethernet_speed_5G");
		case avm_event_ethernet_speed_10G: return("avm_event_ethernet_speed_10G");
		case avm_event_ethernet_speed_error: return("avm_event_ethernet_speed_error");
		case avm_event_ethernet_speed_items: return("avm_event_ethernet_speed_items");
	}
}

char *get_enum__avm_event_id_name (enum _avm_event_id value) {
	switch(value) {
		default: return("_avm_event_id_unknown");
		case avm_event_id_wlan_client_status: return("avm_event_id_wlan_client_status");
		case avm_event_id_wlan_event: return("avm_event_id_wlan_event");
		case avm_event_id_wlan_power: return("avm_event_id_wlan_power");
		case avm_event_id_autoprov: return("avm_event_id_autoprov");
		case avm_event_id_usb_status: return("avm_event_id_usb_status");
		case avm_event_id_dsl_get_arch_kernel: return("avm_event_id_dsl_get_arch_kernel");
		case avm_event_id_dsl_set_arch: return("avm_event_id_dsl_set_arch");
		case avm_event_id_dsl_get_arch: return("avm_event_id_dsl_get_arch");
		case avm_event_id_dsl_set: return("avm_event_id_dsl_set");
		case avm_event_id_dsl_get: return("avm_event_id_dsl_get");
		case avm_event_id_dsl_status: return("avm_event_id_dsl_status");
		case avm_event_id_dsl_connect_status: return("avm_event_id_dsl_connect_status");
		case avm_event_id_push_button: return("avm_event_id_push_button");
		case avm_event_id_telefon_wlan_command: return("avm_event_id_telefon_wlan_command");
		case avm_event_id_capiotcp_startstop: return("avm_event_id_capiotcp_startstop");
		case avm_event_id_telefon_up: return("avm_event_id_telefon_up");
		case avm_event_id_reboot_req: return("avm_event_id_reboot_req");
		case avm_event_id_appl_status: return("avm_event_id_appl_status");
		case avm_event_id_led_status: return("avm_event_id_led_status");
		case avm_event_id_led_info: return("avm_event_id_led_info");
		case avm_event_id_telefonprofile: return("avm_event_id_telefonprofile");
		case avm_event_id_temperature: return("avm_event_id_temperature");
		case avm_event_id_cpu_idle: return("avm_event_id_cpu_idle");
		case avm_event_id_powermanagment_status: return("avm_event_id_powermanagment_status");
		case avm_event_id_powerline_status: return("avm_event_id_powerline_status");
		case avm_event_id_ethernet_connect_status: return("avm_event_id_ethernet_connect_status");
		case avm_event_id_powermanagment_remote: return("avm_event_id_powermanagment_remote");
		case avm_event_id_log: return("avm_event_id_log");
		case avm_event_id_remotewatchdog: return("avm_event_id_remotewatchdog");
		case avm_event_id_rpc: return("avm_event_id_rpc");
		case avm_event_id_remotepcmlink: return("avm_event_id_remotepcmlink");
		case avm_event_id_piglet: return("avm_event_id_piglet");
		case avm_event_id_pm_ressourceinfo_status: return("avm_event_id_pm_ressourceinfo_status");
		case avm_event_id_telephony_missed_call: return("avm_event_id_telephony_missed_call");
		case avm_event_id_telephony_tam_call: return("avm_event_id_telephony_tam_call");
		case avm_event_id_telephony_fax_received: return("avm_event_id_telephony_fax_received");
		case avm_event_id_internet_new_ip: return("avm_event_id_internet_new_ip");
		case avm_event_id_firmware_update_available: return("avm_event_id_firmware_update_available");
		case avm_event_id_smarthome_switch_status: return("avm_event_id_smarthome_switch_status");
		case avm_event_id_telephony_incoming_call: return("avm_event_id_telephony_incoming_call");
		case avm_event_id_mass_storage_mount: return("avm_event_id_mass_storage_mount");
		case avm_event_id_mass_storage_unmount: return("avm_event_id_mass_storage_unmount");
		case avm_event_id_checkpoint: return("avm_event_id_checkpoint");
		case avm_event_id_cpu_run: return("avm_event_id_cpu_run");
		case avm_event_id_ambient_brightness: return("avm_event_id_ambient_brightness");
		case avm_event_id_fax_status_change: return("avm_event_id_fax_status_change");
		case avm_event_id_fax_file: return("avm_event_id_fax_file");
		case avm_event_id_telephony_call_finished: return("avm_event_id_telephony_call_finished");
		case avm_event_id_smarthome: return("avm_event_id_smarthome");
		case avm_event_id_fiber_set_arch: return("avm_event_id_fiber_set_arch");
		case avm_event_id_fiber_get_arch: return("avm_event_id_fiber_get_arch");
		case avm_event_id_user_source_notify: return("avm_event_id_user_source_notify");
		case avm_event_last: return("avm_event_last");
	}
}

char *get_enum__avm_event_push_button_key_name (enum _avm_event_push_button_key value) {
	switch(value) {
		default: return("_avm_event_push_button_key_unknown");
		case avm_event_push_button_wlan_on_off: return("avm_event_push_button_wlan_on_off");
		case avm_event_push_button_wlan_wps: return("avm_event_push_button_wlan_wps");
		case avm_event_push_button_wlan_standby: return("avm_event_push_button_wlan_standby");
		case avm_event_push_button_wlan_wps_station: return("avm_event_push_button_wlan_wps_station");
		case avm_event_push_button_dect_paging: return("avm_event_push_button_dect_paging");
		case avm_event_push_button_dect_pairing: return("avm_event_push_button_dect_pairing");
		case avm_event_push_button_dect_on_off: return("avm_event_push_button_dect_on_off");
		case avm_event_push_button_dect_standby: return("avm_event_push_button_dect_standby");
		case avm_event_push_button_power_set_factory: return("avm_event_push_button_power_set_factory");
		case avm_event_push_button_power_set_factory_kernel: return("avm_event_push_button_power_set_factory_kernel");
		case avm_event_push_button_power_on_off: return("avm_event_push_button_power_on_off");
		case avm_event_push_button_power_standby: return("avm_event_push_button_power_standby");
		case avm_event_push_button_power_socket_on_off: return("avm_event_push_button_power_socket_on_off");
		case avm_event_push_button_tools_profiling: return("avm_event_push_button_tools_profiling");
		case avm_event_push_button_plc_on_off: return("avm_event_push_button_plc_on_off");
		case avm_event_push_button_plc_pairing: return("avm_event_push_button_plc_pairing");
		case avm_event_push_button_led_standby: return("avm_event_push_button_led_standby");
		case avm_event_push_button_2fa_success: return("avm_event_push_button_2fa_success");
		case avm_event_push_button_lte_wakeup: return("avm_event_push_button_lte_wakeup");
		case avm_event_push_button_plc_pairing_off: return("avm_event_push_button_plc_pairing_off");
		case avm_event_push_button_wlan_wps_off: return("avm_event_push_button_wlan_wps_off");
		case avm_event_push_button_dect_pairing_off: return("avm_event_push_button_dect_pairing_off");
		case avm_event_push_button_nexus_pairing_off: return("avm_event_push_button_nexus_pairing_off");
		case avm_event_push_button_nexus_pairing: return("avm_event_push_button_nexus_pairing");
		case avm_event_push_button_wlan_wps_station_off: return("avm_event_push_button_wlan_wps_station_off");
		case avm_event_push_button_nexus_pairing_box: return("avm_event_push_button_nexus_pairing_box");
		case avm_event_push_button_wlan_sta_wps_no_connect: return("avm_event_push_button_wlan_sta_wps_no_connect");
		case avm_event_push_button_wlan_sta_wps_no_connect_off: return("avm_event_push_button_wlan_sta_wps_no_connect_off");
		case avm_event_push_button_connect_methode1: return("avm_event_push_button_connect_methode1");
		case avm_event_push_button_connect_methode2: return("avm_event_push_button_connect_methode2");
		case avm_event_push_button_connect_methode3: return("avm_event_push_button_connect_methode3");
		case avm_event_push_button_connect_methode4: return("avm_event_push_button_connect_methode4");
		case avm_event_push_button_connect_methode5: return("avm_event_push_button_connect_methode5");
		case avm_event_push_button_connect_methode6: return("avm_event_push_button_connect_methode6");
		case avm_event_push_button_connect_off: return("avm_event_push_button_connect_off");
		case avm_event_push_button_connect_timeout: return("avm_event_push_button_connect_timeout");
		case avm_event_push_button_connect_error: return("avm_event_push_button_connect_error");
		case avm_event_push_button_connect_success: return("avm_event_push_button_connect_success");
		case avm_event_push_button_connect_started: return("avm_event_push_button_connect_started");
		case avm_event_push_button_lan_pairing: return("avm_event_push_button_lan_pairing");
		case avm_event_push_button_lan_pairing_off: return("avm_event_push_button_lan_pairing_off");
		case avm_event_push_button_last: return("avm_event_push_button_last");
	}
}

char *get_enum__avm_logtype_name (enum _avm_logtype value) {
	switch(value) {
		default: return("_avm_logtype_unknown");
		case local_panic: return("local_panic");
		case local_crash: return("local_crash");
		case remote_panic: return("remote_panic");
		case remote_crash: return("remote_crash");
	}
}

char *get_enum__avm_piglettype_name (enum _avm_piglettype value) {
	switch(value) {
		default: return("_avm_piglettype_unknown");
		case piglet_tdm_down: return("piglet_tdm_down");
		case piglet_tdm_ready: return("piglet_tdm_ready");
	}
}

char *get_enum__avm_remote_wdt_cmd_name (enum _avm_remote_wdt_cmd value) {
	switch(value) {
		default: return("_avm_remote_wdt_cmd_unknown");
		case wdt_register: return("wdt_register");
		case wdt_release: return("wdt_release");
		case wdt_trigger: return("wdt_trigger");
	}
}

char *get_enum__avm_remotepcmlinktype_name (enum _avm_remotepcmlinktype value) {
	switch(value) {
		default: return("_avm_remotepcmlinktype_unknown");
		case rpcmlink_register: return("rpcmlink_register");
		case rpcmlink_release: return("rpcmlink_release");
	}
}

char *get_enum__avm_rpctype_name (enum _avm_rpctype value) {
	switch(value) {
		default: return("_avm_rpctype_unknown");
		case command_to_arm: return("command_to_arm");
		case command_to_atom: return("command_to_atom");
		case reply_to_arm: return("reply_to_arm");
		case reply_to_atom: return("reply_to_atom");
	}
}

char *get_enum__cputype_name (enum _cputype value) {
	switch(value) {
		default: return("_cputype_unknown");
		case host_cpu: return("host_cpu");
		case remote_cpu: return("remote_cpu");
	}
}

char *get_enum__powermanagment_device_name (enum _powermanagment_device value) {
	switch(value) {
		default: return("_powermanagment_device_unknown");
		case powerdevice_none: return("powerdevice_none");
		case powerdevice_cpuclock: return("powerdevice_cpuclock");
		case powerdevice_dspclock: return("powerdevice_dspclock");
		case powerdevice_systemclock: return("powerdevice_systemclock");
		case powerdevice_wlan: return("powerdevice_wlan");
		case powerdevice_isdnnt: return("powerdevice_isdnnt");
		case powerdevice_isdnte: return("powerdevice_isdnte");
		case powerdevice_analog: return("powerdevice_analog");
		case powerdevice_dect: return("powerdevice_dect");
		case powerdevice_ethernet: return("powerdevice_ethernet");
		case powerdevice_dsl: return("powerdevice_dsl");
		case powerdevice_usb_host: return("powerdevice_usb_host");
		case powerdevice_usb_client: return("powerdevice_usb_client");
		case powerdevice_charge: return("powerdevice_charge");
		case powerdevice_loadrate: return("powerdevice_loadrate");
		case powerdevice_temperature: return("powerdevice_temperature");
		case powerdevice_dectsync: return("powerdevice_dectsync");
		case powerdevice_usb_host2: return("powerdevice_usb_host2");
		case powerdevice_usb_host3: return("powerdevice_usb_host3");
		case powerdevice_dsp_loadrate: return("powerdevice_dsp_loadrate");
		case powerdevice_vdsp_loadrate: return("powerdevice_vdsp_loadrate");
		case powerdevice_lte: return("powerdevice_lte");
		case powerdevice_loadrate2: return("powerdevice_loadrate2");
		case powerdevice_dvbc: return("powerdevice_dvbc");
		case powerdevice_plc: return("powerdevice_plc");
		case powerdevice_maxdevices: return("powerdevice_maxdevices");
	}
}

char *get_enum__powermanagment_status_type_name (enum _powermanagment_status_type value) {
	switch(value) {
		default: return("_powermanagment_status_type_unknown");
		case dsl_status: return("dsl_status");
	}
}

char *get_enum_wlan_event_sel_name (enum wlan_event_sel value) {
	switch(value) {
		default: return("wlan_event_sel_unknown");
		case INPUT_AUTH_1_OS_A: return("INPUT_AUTH_1_OS_A");
		case INPUT_AUTH_1_SK_A: return("INPUT_AUTH_1_SK_A");
		case INPUT_AUTH_1_D: return("INPUT_AUTH_1_D");
		case INPUT_AUTH_3_A: return("INPUT_AUTH_3_A");
		case INPUT_AUTH_3_D: return("INPUT_AUTH_3_D");
		case INPUT_DE_AUTH_STATION: return("INPUT_DE_AUTH_STATION");
		case INPUT_ASSOC_REQ_CHECK: return("INPUT_ASSOC_REQ_CHECK");
		case INPUT_ASSOC_REQ_A: return("INPUT_ASSOC_REQ_A");
		case INPUT_ASSOC_REQ_D: return("INPUT_ASSOC_REQ_D");
		case INPUT_ASSOC_REQ_SEC_D: return("INPUT_ASSOC_REQ_SEC_D");
		case INPUT_RE_ASSOC_REQ_CHECK: return("INPUT_RE_ASSOC_REQ_CHECK");
		case INPUT_RE_ASSOC_REQ_A: return("INPUT_RE_ASSOC_REQ_A");
		case INPUT_RE_ASSOC_REQ_D: return("INPUT_RE_ASSOC_REQ_D");
		case INPUT_RE_ASSOC_REQ_SEC_D: return("INPUT_RE_ASSOC_REQ_SEC_D");
		case INPUT_DIS_ASSOC_STATION: return("INPUT_DIS_ASSOC_STATION");
		case INPUT_CLASS_3: return("INPUT_CLASS_3");
		case INPUT_AUTH_TIMEOUT: return("INPUT_AUTH_TIMEOUT");
		case INPUT_DE_AUTH_MNG_UNICAST: return("INPUT_DE_AUTH_MNG_UNICAST");
		case INPUT_DE_AUTH_MNG_BROADCAST: return("INPUT_DE_AUTH_MNG_BROADCAST");
		case INPUT_DIS_ASSOC_MNG_UNICAST: return("INPUT_DIS_ASSOC_MNG_UNICAST");
		case INPUT_DIS_ASSOC_MNG_BROADCAST: return("INPUT_DIS_ASSOC_MNG_BROADCAST");
		case INPUT_MAC_AUTHORIZE: return("INPUT_MAC_AUTHORIZE");
		case INPUT_MAC_DE_AUTHORIZE: return("INPUT_MAC_DE_AUTHORIZE");
		case INPUT_WDS_LINK_UP: return("INPUT_WDS_LINK_UP");
		case INPUT_WDS_LINK_DOWN: return("INPUT_WDS_LINK_DOWN");
		case INPUT_FRAME_TX_COMPLETE: return("INPUT_FRAME_TX_COMPLETE");
		case INPUT_MADWIFI_WRONG_PSK: return("INPUT_MADWIFI_WRONG_PSK");
		case INPUT_WPS: return("INPUT_WPS");
		case INPUT_MINI: return("INPUT_MINI");
		case INPUT_RADAR: return("INPUT_RADAR");
		case INPUT_WPS_ENROLLEE: return("INPUT_WPS_ENROLLEE");
		case INPUT_STA: return("INPUT_STA");
		case INPUT_GREENAP_PS: return("INPUT_GREENAP_PS");
		case INPUT_EAP_AUTHORIZED: return("INPUT_EAP_AUTHORIZED");
		case INPUT_MWO_INTERFERENCE: return("INPUT_MWO_INTERFERENCE");
		case INPUT_AUTH_EXPIRED: return("INPUT_AUTH_EXPIRED");
		case INPUT_COEXIST_SWITCH: return("INPUT_COEXIST_SWITCH");
		case INPUT_STA_ASSOC: return("INPUT_STA_ASSOC");
		case INPUT_STA_AUTH: return("INPUT_STA_AUTH");
		case INPUT_STA_AUTHORIZATION: return("INPUT_STA_AUTHORIZATION");
		case INPUT_WDS_NO_TIAGGR: return("INPUT_WDS_NO_TIAGGR");
		case INPUT_MAX_NODE_REACHED: return("INPUT_MAX_NODE_REACHED");
		case INPUT_RADAR_DFS_WAIT: return("INPUT_RADAR_DFS_WAIT");
		case INPUT_INTERFERENCE_CHAN_CHANGE: return("INPUT_INTERFERENCE_CHAN_CHANGE");
		case INPUT_RADAR_END: return("INPUT_RADAR_END");
		case INPUT_RADAR_DFS_WAIT_PRECAC: return("INPUT_RADAR_DFS_WAIT_PRECAC");
	}
}

char *get_enum_wlan_info_sel_name (enum wlan_info_sel value) {
	switch(value) {
		default: return("wlan_info_sel_unknown");
		case STATUS_SUCCESSFUL: return("STATUS_SUCCESSFUL");
		case STATUS_UNSPECIFIED: return("STATUS_UNSPECIFIED");
		case STATUS_AUTH_NO_LONGER_VALID: return("STATUS_AUTH_NO_LONGER_VALID");
		case STATUS_DE_AUTH_STATION_IS_LEAVING: return("STATUS_DE_AUTH_STATION_IS_LEAVING");
		case STATUS_DIS_ASSOC_INACTIVITY: return("STATUS_DIS_ASSOC_INACTIVITY");
		case STATUS_DIS_ASSOC_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOC_STATIONS: return("STATUS_DIS_ASSOC_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOC_STATIONS");
		case STATUS_CLASS_2_FROM_NON_AUTH_STATION: return("STATUS_CLASS_2_FROM_NON_AUTH_STATION");
		case STATUS_CLASS_3_FROM_NON_ASSOC_STATION: return("STATUS_CLASS_3_FROM_NON_ASSOC_STATION");
		case STATUS_DIS_ASSOC_STATION_IS_LEAVING: return("STATUS_DIS_ASSOC_STATION_IS_LEAVING");
		case STATUS_STATION_REQUESTING_ASSOC_IS_NOT_AUTH: return("STATUS_STATION_REQUESTING_ASSOC_IS_NOT_AUTH");
		case STATUS_CAPABILITIES_FAILURE: return("STATUS_CAPABILITIES_FAILURE");
		case STATUS_REASSOC_DENIED_UNABLE_TO_CONFIRM_ASSOC: return("STATUS_REASSOC_DENIED_UNABLE_TO_CONFIRM_ASSOC");
		case STATUS_ASSOC_DENIED_OUT_OF_SCOPE: return("STATUS_ASSOC_DENIED_OUT_OF_SCOPE");
		case STATUS_ALGO_IS_NOT_SUPPORTTED: return("STATUS_ALGO_IS_NOT_SUPPORTTED");
		case STATUS_AUTH_TRANSC_NUMBER_OUT_OF_SEQUENCE: return("STATUS_AUTH_TRANSC_NUMBER_OUT_OF_SEQUENCE");
		case STATUS_AUTH_REJ_CHALLENGE_FAILURE: return("STATUS_AUTH_REJ_CHALLENGE_FAILURE");
		case STATUS_AUTH_REJ_TIMEOUT: return("STATUS_AUTH_REJ_TIMEOUT");
		case STATUS_ASSOC_DENIED_UNABLE_TO_HANDLE_ADDITIONAL_ASSOC: return("STATUS_ASSOC_DENIED_UNABLE_TO_HANDLE_ADDITIONAL_ASSOC");
		case STATUS_ASSOC_DENIED_RATE_FAILURE: return("STATUS_ASSOC_DENIED_RATE_FAILURE");
		case STATUS_ASSOC_DENIED_PREAMBLE_FAILURE: return("STATUS_ASSOC_DENIED_PREAMBLE_FAILURE");
		case STATUS_ASSOC_DENIED_PBCC_FAILURE: return("STATUS_ASSOC_DENIED_PBCC_FAILURE");
		case STATUS_ASSOC_DENIED_AGILITY_FAILURE: return("STATUS_ASSOC_DENIED_AGILITY_FAILURE");
		case STATUS_DEAUTH_TX_COMPLETE_TIMEOUT: return("STATUS_DEAUTH_TX_COMPLETE_TIMEOUT");
		case STATUS_DEAUTH_TX_COMPLETE_OK: return("STATUS_DEAUTH_TX_COMPLETE_OK");
		case STATUS_MAX: return("STATUS_MAX");
		case STATUS_AUTHOR_SECMODE_FAILURE: return("STATUS_AUTHOR_SECMODE_FAILURE");
		case STATUS_ASSOC_DENIED_MODE_FAILURE: return("STATUS_ASSOC_DENIED_MODE_FAILURE");
		case STATUS_INVALID: return("STATUS_INVALID");
	}
}

char *get_enum_avm_event_firmware_type_name (enum avm_event_firmware_type value) {
	switch(value) {
		default: return("avm_event_firmware_type_unknown");
		case box_firmware: return("box_firmware");
		case fritz_fon_firmware: return("fritz_fon_firmware");
		case fritz_dect_repeater: return("fritz_dect_repeater");
		case fritz_plug_switch: return("fritz_plug_switch");
		case fritz_hkr: return("fritz_hkr");
	}
}

char *get_enum_avm_event_internet_new_ip_param_sel_name (enum avm_event_internet_new_ip_param_sel value) {
	switch(value) {
		default: return("avm_event_internet_new_ip_param_sel_unknown");
		case avm_event_internet_new_ip_v4: return("avm_event_internet_new_ip_v4");
		case avm_event_internet_new_ip_v6: return("avm_event_internet_new_ip_v6");
	}
}

char *get_enum_avm_event_led_id_name (enum avm_event_led_id value) {
	switch(value) {
		default: return("avm_event_led_id_unknown");
		case avm_logical_led_inval: return("avm_logical_led_inval");
		case avm_logical_led_ppp: return("avm_logical_led_ppp");
		case avm_logical_led_error: return("avm_logical_led_error");
		case avm_logical_led_pots: return("avm_logical_led_pots");
		case avm_logical_led_info: return("avm_logical_led_info");
		case avm_logical_led_traffic: return("avm_logical_led_traffic");
		case avm_logical_led_freecall: return("avm_logical_led_freecall");
		case avm_logical_led_avmusbwlan: return("avm_logical_led_avmusbwlan");
		case avm_logical_led_sip: return("avm_logical_led_sip");
		case avm_logical_led_mwi: return("avm_logical_led_mwi");
		case avm_logical_led_fest_mwi: return("avm_logical_led_fest_mwi");
		case avm_logical_led_isdn_d: return("avm_logical_led_isdn_d");
		case avm_logical_led_isdn_b1: return("avm_logical_led_isdn_b1");
		case avm_logical_led_isdn_b2: return("avm_logical_led_isdn_b2");
		case avm_logical_led_lan: return("avm_logical_led_lan");
		case avm_logical_led_lan1: return("avm_logical_led_lan1");
		case avm_logical_led_adsl: return("avm_logical_led_adsl");
		case avm_logical_led_power: return("avm_logical_led_power");
		case avm_logical_led_usb: return("avm_logical_led_usb");
		case avm_logical_led_wifi: return("avm_logical_led_wifi");
		case avm_logical_led_last: return("avm_logical_led_last");
	}
}

char *get_enum_avm_event_msg_type_name (enum avm_event_msg_type value) {
	switch(value) {
		default: return("avm_event_msg_type_unknown");
		case avm_event_source_register_type: return("avm_event_source_register_type");
		case avm_event_source_unregister_type: return("avm_event_source_unregister_type");
		case avm_event_source_notifier_type: return("avm_event_source_notifier_type");
		case avm_event_remote_source_trigger_request_type: return("avm_event_remote_source_trigger_request_type");
		case avm_event_ping_type: return("avm_event_ping_type");
		case avm_event_tffs_type: return("avm_event_tffs_type");
	}
}

char *get_enum_avm_event_powermanagment_remote_action_name (enum avm_event_powermanagment_remote_action value) {
	switch(value) {
		default: return("avm_event_powermanagment_remote_action_unknown");
		case avm_event_powermanagment_ressourceinfo: return("avm_event_powermanagment_ressourceinfo");
		case avm_event_powermanagment_activatepowermode: return("avm_event_powermanagment_activatepowermode");
	}
}

char *get_enum_avm_event_switch_type_name (enum avm_event_switch_type value) {
	switch(value) {
		default: return("avm_event_switch_type_unknown");
		case binary: return("binary");
		case percent: return("percent");
	}
}

char *get_enum_avm_event_telephony_param_sel_name (enum avm_event_telephony_param_sel value) {
	switch(value) {
		default: return("avm_event_telephony_param_sel_unknown");
		case avm_event_telephony_params_name: return("avm_event_telephony_params_name");
		case avm_event_telephony_params_msn_name: return("avm_event_telephony_params_msn_name");
		case avm_event_telephony_params_calling: return("avm_event_telephony_params_calling");
		case avm_event_telephony_params_called: return("avm_event_telephony_params_called");
		case avm_event_telephony_params_duration: return("avm_event_telephony_params_duration");
		case avm_event_telephony_params_port: return("avm_event_telephony_params_port");
		case avm_event_telephony_params_portname: return("avm_event_telephony_params_portname");
		case avm_event_telephony_params_id: return("avm_event_telephony_params_id");
		case avm_event_telephony_params_tam_path: return("avm_event_telephony_params_tam_path");
	}
}

char *get_enum_avm_event_tffs_call_type_name (enum avm_event_tffs_call_type value) {
	switch(value) {
		default: return("avm_event_tffs_call_type_unknown");
		case avm_event_tffs_call_open: return("avm_event_tffs_call_open");
		case avm_event_tffs_call_close: return("avm_event_tffs_call_close");
		case avm_event_tffs_call_read: return("avm_event_tffs_call_read");
		case avm_event_tffs_call_write: return("avm_event_tffs_call_write");
		case avm_event_tffs_call_cleanup: return("avm_event_tffs_call_cleanup");
		case avm_event_tffs_call_reindex: return("avm_event_tffs_call_reindex");
		case avm_event_tffs_call_info: return("avm_event_tffs_call_info");
		case avm_event_tffs_call_init: return("avm_event_tffs_call_init");
		case avm_event_tffs_call_deinit: return("avm_event_tffs_call_deinit");
		case avm_event_tffs_call_notify: return("avm_event_tffs_call_notify");
		case avm_event_tffs_call_paniclog: return("avm_event_tffs_call_paniclog");
	}
}

char *get_enum_avm_event_tffs_notify_event_name (enum avm_event_tffs_notify_event value) {
	switch(value) {
		default: return("avm_event_tffs_notify_event_unknown");
		case avm_event_tffs_notify_clear: return("avm_event_tffs_notify_clear");
		case avm_event_tffs_notify_update: return("avm_event_tffs_notify_update");
		case avm_event_tffs_notify_reinit: return("avm_event_tffs_notify_reinit");
	}
}

char *get_enum_avm_event_tffs_open_mode_name (enum avm_event_tffs_open_mode value) {
	switch(value) {
		default: return("avm_event_tffs_open_mode_unknown");
		case avm_event_tffs_mode_read: return("avm_event_tffs_mode_read");
		case avm_event_tffs_mode_write: return("avm_event_tffs_mode_write");
		case avm_event_tffs_mode_panic: return("avm_event_tffs_mode_panic");
	}
}

char *get_enum_ePLCState_name (enum ePLCState value) {
	switch(value) {
		default: return("ePLCState_unknown");
		case PLCStateRunningNotConnected: return("PLCStateRunningNotConnected");
		case PLCStateRunningConnected: return("PLCStateRunningConnected");
		case PLCStateNotRunning: return("PLCStateNotRunning");
	}
}

char *get_enum_fax_file_event_type_name (enum fax_file_event_type value) {
	switch(value) {
		default: return("fax_file_event_type_unknown");
		case FAX_FILE_EVENT_NEW_FILE: return("FAX_FILE_EVENT_NEW_FILE");
		case FAX_FILE_EVENT_REMOVED_FILE: return("FAX_FILE_EVENT_REMOVED_FILE");
		case FAX_FILE_EVENT_READ: return("FAX_FILE_EVENT_READ");
		case FAX_FILE_EVENT_UNREAD: return("FAX_FILE_EVENT_UNREAD");
	}
}

char *get_enum_fax_receive_mode_name (enum fax_receive_mode value) {
	switch(value) {
		default: return("fax_receive_mode_unknown");
		case FAX_RECEIVE_MODE_OFF: return("FAX_RECEIVE_MODE_OFF");
		case FAX_RECEIVE_MODE_MAIL_ONLY: return("FAX_RECEIVE_MODE_MAIL_ONLY");
		case FAX_RECEIVE_MODE_STORE_ONLY: return("FAX_RECEIVE_MODE_STORE_ONLY");
		case FAX_RECEIVE_MODE_MAIL_AND_STORE: return("FAX_RECEIVE_MODE_MAIL_AND_STORE");
	}
}

char *get_enum_fax_storage_dest_name (enum fax_storage_dest value) {
	switch(value) {
		default: return("fax_storage_dest_unknown");
		case FAX_STORAGE_INTERNAL: return("FAX_STORAGE_INTERNAL");
		case FAX_STORAGE_EXTERNAL: return("FAX_STORAGE_EXTERNAL");
	}
}

char *get_enum_wlan_event_id_name (enum wlan_event_id value) {
	switch(value) {
		default: return("wlan_event_id_unknown");
		case CLIENT_STATE_CHANGE: return("CLIENT_STATE_CHANGE");
		case CLIENT_CONNECT_INFO: return("CLIENT_CONNECT_INFO");
		case WLAN_EVENT_SCAN: return("WLAN_EVENT_SCAN");
		case CLIENT_STATE_IDLE: return("CLIENT_STATE_IDLE");
		case WLAN_RADIO_RECOVERY: return("WLAN_RADIO_RECOVERY");
	}
}

char *get_enum_wlan_event_radio_recovery_state_name (enum wlan_event_radio_recovery_state value) {
	switch(value) {
		default: return("wlan_event_radio_recovery_state_unknown");
		case WLAN_EVENT_RADIO_RECOVERY_RADIO_DOWN: return("WLAN_EVENT_RADIO_RECOVERY_RADIO_DOWN");
		case WLAN_EVENT_RADIO_RECOVERY_RADIO_UP: return("WLAN_EVENT_RADIO_RECOVERY_RADIO_UP");
	}
}

char *get_enum_wlan_event_scan_type_name (enum wlan_event_scan_type value) {
	switch(value) {
		default: return("wlan_event_scan_type_unknown");
		case WLAN_EVENT_SCAN_FINISHED: return("WLAN_EVENT_SCAN_FINISHED");
	}
}

char *get_enum_wlan_info_special_name (enum wlan_info_special value) {
	switch(value) {
		default: return("wlan_info_special_unknown");
		case STATUS_SUCCESS: return("STATUS_SUCCESS");
		case STATUS_FAILURE: return("STATUS_FAILURE");
		case STATUS_TIMEOUT: return("STATUS_TIMEOUT");
		case STATUS_WPS_START: return("STATUS_WPS_START");
		case STATUS_WPS_DISCOVERY: return("STATUS_WPS_DISCOVERY");
		case STATUS_WPS_OVERLAP: return("STATUS_WPS_OVERLAP");
		case STATUS_WPS_STOP: return("STATUS_WPS_STOP");
	}
}

char *get_enum_wlan_sm_states_name (enum wlan_sm_states value) {
	switch(value) {
		default: return("wlan_sm_states_unknown");
		case WLAN_SM_STATE_IDLE: return("WLAN_SM_STATE_IDLE");
		case WLAN_SM_STATE_AUTH_KEY: return("WLAN_SM_STATE_AUTH_KEY");
		case WLAN_SM_STATE_AUTHENTICATED: return("WLAN_SM_STATE_AUTHENTICATED");
		case WLAN_SM_STATE_WAIT_FOR_ASS_RES: return("WLAN_SM_STATE_WAIT_FOR_ASS_RES");
		case WLAN_SM_STATE_ASSOCIATED: return("WLAN_SM_STATE_ASSOCIATED");
		case WLAN_SM_STATE_AUTHORIZED: return("WLAN_SM_STATE_AUTHORIZED");
		case WLAN_SM_STATE_DEAUTHENTICATE: return("WLAN_SM_STATE_DEAUTHENTICATE");
		case WLAN_SM_NUM_STATES: return("WLAN_SM_NUM_STATES");
	}
}


