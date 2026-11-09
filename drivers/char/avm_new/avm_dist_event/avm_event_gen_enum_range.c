#include "avm_event_gen_enum_range.h"

uint32_t avm_event_check_enum_range___avm_event_cmd(enum __avm_event_cmd E) {
	switch(E) {
		case avm_event_cmd_register:
		case avm_event_cmd_release:
		case avm_event_cmd_source_register:
		case avm_event_cmd_source_release:
		case avm_event_cmd_source_trigger:
		case avm_event_cmd_trigger:
		case avm_event_cmd_undef:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_event_ethernet_speed(enum _avm_event_ethernet_speed E) {
	switch(E) {
		case avm_event_ethernet_speed_no_link:
		case avm_event_ethernet_speed_10M:
		case avm_event_ethernet_speed_100M:
		case avm_event_ethernet_speed_1G:
		case avm_event_ethernet_speed_2_5G:
		case avm_event_ethernet_speed_5G:
		case avm_event_ethernet_speed_10G:
		case avm_event_ethernet_speed_error:
		case avm_event_ethernet_speed_items:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_event_id(enum _avm_event_id E) {
	switch(E) {
		case avm_event_id_wlan_client_status:
		case avm_event_id_wlan_event:
		case avm_event_id_wlan_power:
		case avm_event_id_autoprov:
		case avm_event_id_usb_status:
		case avm_event_id_dsl_get_arch_kernel:
		case avm_event_id_dsl_set_arch:
		case avm_event_id_dsl_get_arch:
		case avm_event_id_dsl_set:
		case avm_event_id_dsl_get:
		case avm_event_id_dsl_status:
		case avm_event_id_dsl_connect_status:
		case avm_event_id_push_button:
		case avm_event_id_telefon_wlan_command:
		case avm_event_id_capiotcp_startstop:
		case avm_event_id_telefon_up:
		case avm_event_id_reboot_req:
		case avm_event_id_appl_status:
		case avm_event_id_led_status:
		case avm_event_id_led_info:
		case avm_event_id_telefonprofile:
		case avm_event_id_temperature:
		case avm_event_id_cpu_idle:
		case avm_event_id_powermanagment_status:
		case avm_event_id_powerline_status:
		case avm_event_id_ethernet_connect_status:
		case avm_event_id_powermanagment_remote:
		case avm_event_id_log:
		case avm_event_id_remotewatchdog:
		case avm_event_id_rpc:
		case avm_event_id_remotepcmlink:
		case avm_event_id_piglet:
		case avm_event_id_pm_ressourceinfo_status:
		case avm_event_id_telephony_missed_call:
		case avm_event_id_telephony_tam_call:
		case avm_event_id_telephony_fax_received:
		case avm_event_id_internet_new_ip:
		case avm_event_id_firmware_update_available:
		case avm_event_id_smarthome_switch_status:
		case avm_event_id_telephony_incoming_call:
		case avm_event_id_mass_storage_mount:
		case avm_event_id_mass_storage_unmount:
		case avm_event_id_checkpoint:
		case avm_event_id_cpu_run:
		case avm_event_id_ambient_brightness:
		case avm_event_id_fax_status_change:
		case avm_event_id_fax_file:
		case avm_event_id_telephony_call_finished:
		case avm_event_id_smarthome:
		case avm_event_id_fiber_set_arch:
		case avm_event_id_fiber_get_arch:
		case avm_event_id_user_source_notify:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_event_push_button_key(enum _avm_event_push_button_key E) {
	switch(E) {
		case avm_event_push_button_wlan_on_off:
		case avm_event_push_button_wlan_wps:
		case avm_event_push_button_wlan_standby:
		case avm_event_push_button_wlan_wps_station:
		case avm_event_push_button_dect_paging:
		case avm_event_push_button_dect_pairing:
		case avm_event_push_button_dect_on_off:
		case avm_event_push_button_dect_standby:
		case avm_event_push_button_power_set_factory:
		case avm_event_push_button_power_set_factory_kernel:
		case avm_event_push_button_power_on_off:
		case avm_event_push_button_power_standby:
		case avm_event_push_button_power_socket_on_off:
		case avm_event_push_button_tools_profiling:
		case avm_event_push_button_plc_on_off:
		case avm_event_push_button_plc_pairing:
		case avm_event_push_button_led_standby:
		case avm_event_push_button_2fa_success:
		case avm_event_push_button_lte_wakeup:
		case avm_event_push_button_plc_pairing_off:
		case avm_event_push_button_wlan_wps_off:
		case avm_event_push_button_dect_pairing_off:
		case avm_event_push_button_nexus_pairing_off:
		case avm_event_push_button_nexus_pairing:
		case avm_event_push_button_wlan_wps_station_off:
		case avm_event_push_button_nexus_pairing_box:
		case avm_event_push_button_wlan_sta_wps_no_connect:
		case avm_event_push_button_wlan_sta_wps_no_connect_off:
		case avm_event_push_button_connect_methode1:
		case avm_event_push_button_connect_methode2:
		case avm_event_push_button_connect_methode3:
		case avm_event_push_button_connect_methode4:
		case avm_event_push_button_connect_methode5:
		case avm_event_push_button_connect_methode6:
		case avm_event_push_button_connect_off:
		case avm_event_push_button_connect_timeout:
		case avm_event_push_button_connect_error:
		case avm_event_push_button_connect_success:
		case avm_event_push_button_connect_started:
		case avm_event_push_button_lan_pairing:
		case avm_event_push_button_lan_pairing_off:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_logtype(enum _avm_logtype E) {
	switch(E) {
		case local_panic:
		case local_crash:
		case remote_panic:
		case remote_crash:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_piglettype(enum _avm_piglettype E) {
	switch(E) {
		case piglet_tdm_down:
		case piglet_tdm_ready:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_remote_wdt_cmd(enum _avm_remote_wdt_cmd E) {
	switch(E) {
		case wdt_register:
		case wdt_release:
		case wdt_trigger:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_remotepcmlinktype(enum _avm_remotepcmlinktype E) {
	switch(E) {
		case rpcmlink_register:
		case rpcmlink_release:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__avm_rpctype(enum _avm_rpctype E) {
	switch(E) {
		case command_to_arm:
		case command_to_atom:
		case reply_to_arm:
		case reply_to_atom:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__cputype(enum _cputype E) {
	switch(E) {
		case host_cpu:
		case remote_cpu:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__powermanagment_device(enum _powermanagment_device E) {
	switch(E) {
		case powerdevice_none:
		case powerdevice_cpuclock:
		case powerdevice_dspclock:
		case powerdevice_systemclock:
		case powerdevice_wlan:
		case powerdevice_isdnnt:
		case powerdevice_isdnte:
		case powerdevice_analog:
		case powerdevice_dect:
		case powerdevice_ethernet:
		case powerdevice_dsl:
		case powerdevice_usb_host:
		case powerdevice_usb_client:
		case powerdevice_charge:
		case powerdevice_loadrate:
		case powerdevice_temperature:
		case powerdevice_dectsync:
		case powerdevice_usb_host2:
		case powerdevice_usb_host3:
		case powerdevice_dsp_loadrate:
		case powerdevice_vdsp_loadrate:
		case powerdevice_lte:
		case powerdevice_loadrate2:
		case powerdevice_dvbc:
		case powerdevice_plc:
		case powerdevice_maxdevices:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range__powermanagment_status_type(enum _powermanagment_status_type E) {
	switch(E) {
		case dsl_status:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_event_sel(enum wlan_event_sel E) {
	switch(E) {
		case INPUT_AUTH_1_OS_A:
		case INPUT_AUTH_1_SK_A:
		case INPUT_AUTH_1_D:
		case INPUT_AUTH_3_A:
		case INPUT_AUTH_3_D:
		case INPUT_DE_AUTH_STATION:
		case INPUT_ASSOC_REQ_CHECK:
		case INPUT_ASSOC_REQ_A:
		case INPUT_ASSOC_REQ_D:
		case INPUT_ASSOC_REQ_SEC_D:
		case INPUT_RE_ASSOC_REQ_CHECK:
		case INPUT_RE_ASSOC_REQ_A:
		case INPUT_RE_ASSOC_REQ_D:
		case INPUT_RE_ASSOC_REQ_SEC_D:
		case INPUT_DIS_ASSOC_STATION:
		case INPUT_CLASS_3:
		case INPUT_AUTH_TIMEOUT:
		case INPUT_DE_AUTH_MNG_UNICAST:
		case INPUT_DE_AUTH_MNG_BROADCAST:
		case INPUT_DIS_ASSOC_MNG_UNICAST:
		case INPUT_DIS_ASSOC_MNG_BROADCAST:
		case INPUT_MAC_AUTHORIZE:
		case INPUT_MAC_DE_AUTHORIZE:
		case INPUT_WDS_LINK_UP:
		case INPUT_WDS_LINK_DOWN:
		case INPUT_FRAME_TX_COMPLETE:
		case INPUT_MADWIFI_WRONG_PSK:
		case INPUT_WPS:
		case INPUT_MINI:
		case INPUT_RADAR:
		case INPUT_WPS_ENROLLEE:
		case INPUT_STA:
		case INPUT_GREENAP_PS:
		case INPUT_EAP_AUTHORIZED:
		case INPUT_MWO_INTERFERENCE:
		case INPUT_AUTH_EXPIRED:
		case INPUT_COEXIST_SWITCH:
		case INPUT_STA_ASSOC:
		case INPUT_STA_AUTH:
		case INPUT_STA_AUTHORIZATION:
		case INPUT_WDS_NO_TIAGGR:
		case INPUT_MAX_NODE_REACHED:
		case INPUT_RADAR_DFS_WAIT:
		case INPUT_INTERFERENCE_CHAN_CHANGE:
		case INPUT_RADAR_END:
		case INPUT_RADAR_DFS_WAIT_PRECAC:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_info_sel(enum wlan_info_sel E) {
	switch(E) {
		case STATUS_SUCCESSFUL:
		case STATUS_UNSPECIFIED:
		case STATUS_AUTH_NO_LONGER_VALID:
		case STATUS_DE_AUTH_STATION_IS_LEAVING:
		case STATUS_DIS_ASSOC_INACTIVITY:
		case STATUS_DIS_ASSOC_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOC_STATIONS:
		case STATUS_CLASS_2_FROM_NON_AUTH_STATION:
		case STATUS_CLASS_3_FROM_NON_ASSOC_STATION:
		case STATUS_DIS_ASSOC_STATION_IS_LEAVING:
		case STATUS_STATION_REQUESTING_ASSOC_IS_NOT_AUTH:
		case STATUS_CAPABILITIES_FAILURE:
		case STATUS_REASSOC_DENIED_UNABLE_TO_CONFIRM_ASSOC:
		case STATUS_ASSOC_DENIED_OUT_OF_SCOPE:
		case STATUS_ALGO_IS_NOT_SUPPORTTED:
		case STATUS_AUTH_TRANSC_NUMBER_OUT_OF_SEQUENCE:
		case STATUS_AUTH_REJ_CHALLENGE_FAILURE:
		case STATUS_AUTH_REJ_TIMEOUT:
		case STATUS_ASSOC_DENIED_UNABLE_TO_HANDLE_ADDITIONAL_ASSOC:
		case STATUS_ASSOC_DENIED_RATE_FAILURE:
		case STATUS_ASSOC_DENIED_PREAMBLE_FAILURE:
		case STATUS_ASSOC_DENIED_PBCC_FAILURE:
		case STATUS_ASSOC_DENIED_AGILITY_FAILURE:
		case STATUS_DEAUTH_TX_COMPLETE_TIMEOUT:
		case STATUS_DEAUTH_TX_COMPLETE_OK:
		case STATUS_MAX:
		case STATUS_AUTHOR_SECMODE_FAILURE:
		case STATUS_ASSOC_DENIED_MODE_FAILURE:
		case STATUS_INVALID:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_firmware_type(enum avm_event_firmware_type E) {
	switch(E) {
		case box_firmware:
		case fritz_fon_firmware:
		case fritz_dect_repeater:
		case fritz_plug_switch:
		case fritz_hkr:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_internet_new_ip_param_sel(enum avm_event_internet_new_ip_param_sel E) {
	switch(E) {
		case avm_event_internet_new_ip_v4:
		case avm_event_internet_new_ip_v6:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_led_id(enum avm_event_led_id E) {
	switch(E) {
		case avm_logical_led_inval:
		case avm_logical_led_ppp:
		case avm_logical_led_error:
		case avm_logical_led_pots:
		case avm_logical_led_info:
		case avm_logical_led_traffic:
		case avm_logical_led_freecall:
		case avm_logical_led_avmusbwlan:
		case avm_logical_led_sip:
		case avm_logical_led_mwi:
		case avm_logical_led_fest_mwi:
		case avm_logical_led_isdn_d:
		case avm_logical_led_isdn_b1:
		case avm_logical_led_isdn_b2:
		case avm_logical_led_lan:
		case avm_logical_led_lan1:
		case avm_logical_led_adsl:
		case avm_logical_led_power:
		case avm_logical_led_usb:
		case avm_logical_led_wifi:
		case avm_logical_led_last:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_msg_type(enum avm_event_msg_type E) {
	switch(E) {
		case avm_event_source_register_type:
		case avm_event_source_unregister_type:
		case avm_event_source_notifier_type:
		case avm_event_remote_source_trigger_request_type:
		case avm_event_ping_type:
		case avm_event_tffs_type:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_powermanagment_remote_action(enum avm_event_powermanagment_remote_action E) {
	switch(E) {
		case avm_event_powermanagment_ressourceinfo:
		case avm_event_powermanagment_activatepowermode:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_switch_type(enum avm_event_switch_type E) {
	switch(E) {
		case binary:
		case percent:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_telephony_param_sel(enum avm_event_telephony_param_sel E) {
	switch(E) {
		case avm_event_telephony_params_name:
		case avm_event_telephony_params_msn_name:
		case avm_event_telephony_params_calling:
		case avm_event_telephony_params_called:
		case avm_event_telephony_params_duration:
		case avm_event_telephony_params_port:
		case avm_event_telephony_params_portname:
		case avm_event_telephony_params_id:
		case avm_event_telephony_params_tam_path:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_tffs_call_type(enum avm_event_tffs_call_type E) {
	switch(E) {
		case avm_event_tffs_call_open:
		case avm_event_tffs_call_close:
		case avm_event_tffs_call_read:
		case avm_event_tffs_call_write:
		case avm_event_tffs_call_cleanup:
		case avm_event_tffs_call_reindex:
		case avm_event_tffs_call_info:
		case avm_event_tffs_call_init:
		case avm_event_tffs_call_deinit:
		case avm_event_tffs_call_notify:
		case avm_event_tffs_call_paniclog:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_tffs_notify_event(enum avm_event_tffs_notify_event E) {
	switch(E) {
		case avm_event_tffs_notify_clear:
		case avm_event_tffs_notify_update:
		case avm_event_tffs_notify_reinit:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_avm_event_tffs_open_mode(enum avm_event_tffs_open_mode E) {
	switch(E) {
		case avm_event_tffs_mode_read:
		case avm_event_tffs_mode_write:
		case avm_event_tffs_mode_panic:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_ePLCState(enum ePLCState E) {
	switch(E) {
		case PLCStateRunningNotConnected:
		case PLCStateRunningConnected:
		case PLCStateNotRunning:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_fax_file_event_type(enum fax_file_event_type E) {
	switch(E) {
		case FAX_FILE_EVENT_NEW_FILE:
		case FAX_FILE_EVENT_REMOVED_FILE:
		case FAX_FILE_EVENT_READ:
		case FAX_FILE_EVENT_UNREAD:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_fax_receive_mode(enum fax_receive_mode E) {
	switch(E) {
		case FAX_RECEIVE_MODE_OFF:
		case FAX_RECEIVE_MODE_MAIL_ONLY:
		case FAX_RECEIVE_MODE_STORE_ONLY:
		case FAX_RECEIVE_MODE_MAIL_AND_STORE:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_fax_storage_dest(enum fax_storage_dest E) {
	switch(E) {
		case FAX_STORAGE_INTERNAL:
		case FAX_STORAGE_EXTERNAL:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_event_id(enum wlan_event_id E) {
	switch(E) {
		case CLIENT_STATE_CHANGE:
		case CLIENT_CONNECT_INFO:
		case WLAN_EVENT_SCAN:
		case CLIENT_STATE_IDLE:
		case WLAN_RADIO_RECOVERY:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_event_radio_recovery_state(enum wlan_event_radio_recovery_state E) {
	switch(E) {
		case WLAN_EVENT_RADIO_RECOVERY_RADIO_DOWN:
		case WLAN_EVENT_RADIO_RECOVERY_RADIO_UP:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_event_scan_type(enum wlan_event_scan_type E) {
	switch(E) {
		case WLAN_EVENT_SCAN_FINISHED:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_info_special(enum wlan_info_special E) {
	switch(E) {
		case STATUS_SUCCESS:
		case STATUS_FAILURE:
		case STATUS_TIMEOUT:
		case STATUS_WPS_START:
		case STATUS_WPS_DISCOVERY:
		case STATUS_WPS_OVERLAP:
		case STATUS_WPS_STOP:
			return 0;
		default:
			return 1;
	}
}

uint32_t avm_event_check_enum_range_wlan_sm_states(enum wlan_sm_states E) {
	switch(E) {
		case WLAN_SM_STATE_IDLE:
		case WLAN_SM_STATE_AUTH_KEY:
		case WLAN_SM_STATE_AUTHENTICATED:
		case WLAN_SM_STATE_WAIT_FOR_ASS_RES:
		case WLAN_SM_STATE_ASSOCIATED:
		case WLAN_SM_STATE_AUTHORIZED:
		case WLAN_SM_STATE_DEAUTHENTICATE:
		case WLAN_SM_NUM_STATES:
			return 0;
		default:
			return 1;
	}
}

/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union __powermanagment_status_union'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union___powermanagment_status_union (enum _powermanagment_status_type E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case dsl_status:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union _avm_event_cmd_param'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union__avm_event_cmd_param (enum __avm_event_cmd E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_cmd_register:
			return 0;
		case avm_event_cmd_release:
			return 0;
		case avm_event_cmd_trigger:
			return 0;
		case avm_event_cmd_source_register:
			return 0;
		case avm_event_cmd_source_trigger:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_data_union'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_data_union (enum _avm_event_id E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_id_appl_status:
			return 0;
		case avm_event_id_autoprov:
			return 0;
		case avm_event_id_capiotcp_startstop:
			return 0;
		case avm_event_id_dsl_connect_status:
			return 0;
		case avm_event_id_dsl_get:
			return 0;
		case avm_event_id_dsl_get_arch:
			return 0;
		case avm_event_id_dsl_set:
			return 0;
		case avm_event_id_dsl_set_arch:
			return 0;
		case avm_event_id_dsl_status:
			return 0;
		case avm_event_id_powerline_status:
			return 0;
		case avm_event_id_reboot_req:
			return 0;
		case avm_event_id_telefon_wlan_command:
			return 0;
		case avm_event_id_usb_status:
			return 0;
		case avm_event_id_user_source_notify:
			return 0;
		case avm_event_id_push_button:
			return 0;
		case avm_event_id_ethernet_connect_status:
			return 0;
		case avm_event_id_led_status:
			return 0;
		case avm_event_id_led_info:
			return 0;
		case avm_event_id_telefonprofile:
			return 0;
		case avm_event_id_temperature:
			return 0;
		case avm_event_id_powermanagment_status:
			return 0;
		case avm_event_id_cpu_idle:
			return 0;
		case avm_event_id_powermanagment_remote:
			return 0;
		case avm_event_id_log:
			return 0;
		case avm_event_id_remotepcmlink:
			return 0;
		case avm_event_id_remotewatchdog:
			return 0;
		case avm_event_id_rpc:
			return 0;
		case avm_event_id_pm_ressourceinfo_status:
			return 0;
		case avm_event_id_wlan_client_status:
			return 0;
		case avm_event_id_wlan_power:
			return 0;
		case avm_event_id_wlan_event:
			return 0;
		case avm_event_id_telephony_missed_call:
			return 0;
		case avm_event_id_telephony_tam_call:
			return 0;
		case avm_event_id_telephony_fax_received:
			return 0;
		case avm_event_id_telephony_incoming_call:
			return 0;
		case avm_event_id_telephony_call_finished:
			return 0;
		case avm_event_id_firmware_update_available:
			return 0;
		case avm_event_id_internet_new_ip:
			return 0;
		case avm_event_id_smarthome_switch_status:
			return 0;
		case avm_event_id_mass_storage_mount:
			return 0;
		case avm_event_id_mass_storage_unmount:
			return 0;
		case avm_event_id_checkpoint:
			return 0;
		case avm_event_id_cpu_run:
			return 0;
		case avm_event_id_ambient_brightness:
			return 0;
		case avm_event_id_fax_status_change:
			return 0;
		case avm_event_id_fax_file:
			return 0;
		case avm_event_id_piglet:
			return 0;
		case avm_event_id_telefon_up:
			return 0;
		case avm_event_id_smarthome:
			return 0;
		case avm_event_id_fiber_set_arch:
			return 0;
		case avm_event_id_fiber_get_arch:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_internet_new_ip_param'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_internet_new_ip_param (enum avm_event_internet_new_ip_param_sel E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_internet_new_ip_v4:
			return 0;
		case avm_event_internet_new_ip_v6:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_message_union'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_message_union (enum avm_event_msg_type E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_source_register_type:
			return 0;
		case avm_event_source_unregister_type:
			return 0;
		case avm_event_source_notifier_type:
			return 0;
		case avm_event_remote_source_trigger_request_type:
			return 0;
		case avm_event_ping_type:
			return 0;
		case avm_event_tffs_type:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_powermanagment_remote_union'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_powermanagment_remote_union (enum avm_event_powermanagment_remote_action E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_powermanagment_ressourceinfo:
			return 0;
		case avm_event_powermanagment_activatepowermode:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_telephony_call_params'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_telephony_call_params (enum avm_event_telephony_param_sel E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_telephony_params_name:
			return 0;
		case avm_event_telephony_params_msn_name:
			return 0;
		case avm_event_telephony_params_portname:
			return 0;
		case avm_event_telephony_params_tam_path:
			return 0;
		case avm_event_telephony_params_calling:
			return 0;
		case avm_event_telephony_params_called:
			return 0;
		case avm_event_telephony_params_duration:
			return 0;
		case avm_event_telephony_params_port:
			return 0;
		case avm_event_telephony_params_id:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_tffs_call_union'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_tffs_call_union (enum avm_event_tffs_call_type E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case avm_event_tffs_call_open:
			return 0;
		case avm_event_tffs_call_close:
			return 0;
		case avm_event_tffs_call_read:
			return 0;
		case avm_event_tffs_call_write:
			return 0;
		case avm_event_tffs_call_cleanup:
			return 0;
		case avm_event_tffs_call_reindex:
			return 0;
		case avm_event_tffs_call_info:
			return 0;
		case avm_event_tffs_call_init:
			return 0;
		case avm_event_tffs_call_deinit:
			return 0;
		case avm_event_tffs_call_notify:
			return 0;
		case avm_event_tffs_call_paniclog:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_wlan_client_status_u1'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_wlan_client_status_u1 (enum wlan_event_sel E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case INPUT_RADAR_DFS_WAIT:
			return 0;
		case INPUT_MAC_AUTHORIZE:
			return 0;
		case INPUT_EAP_AUTHORIZED:
			return 0;
		case INPUT_MADWIFI_WRONG_PSK:
			return 0;
		case INPUT_AUTH_EXPIRED:
			return 0;
		case INPUT_STA:
			return 0;
		case INPUT_WDS_LINK_UP:
			return 0;
		case INPUT_WDS_LINK_DOWN:
			return 0;
		case INPUT_RADAR:
			return 0;
		case INPUT_RADAR_DFS_WAIT_PRECAC:
			return 0;
		case INPUT_GREENAP_PS:
			return 0;
		case INPUT_COEXIST_SWITCH:
			return 0;
		case INPUT_MAX_NODE_REACHED:
			return 0;
		case INPUT_INTERFERENCE_CHAN_CHANGE:
			return 0;
		case INPUT_AUTH_1_OS_A:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_wlan_client_status_u2'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_wlan_client_status_u2 (enum wlan_event_sel E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case INPUT_RADAR:
			return 0;
		case INPUT_RADAR_DFS_WAIT_PRECAC:
			return 0;
		case INPUT_AUTH_1_D:
			return 0;
		case INPUT_AUTH_1_OS_A:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union avm_event_wlan_client_status_u3'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_avm_event_wlan_client_status_u3 (enum wlan_event_sel E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case INPUT_RADAR_DFS_WAIT:
			return 0;
		case INPUT_RADAR:
			return 0;
		case INPUT_AUTH_1_OS_A:
			return 0;
		default:
			return 1;
	}
}


/*------------------------------------------------------------------------------------------*\
 * Endian check für : 'union wlan_event_data'
\*------------------------------------------------------------------------------------------*/
int check_enum_for_union_wlan_event_data (enum wlan_event_id E, unsigned int version __attribute__((unused))) {
	switch(E) {
		case CLIENT_STATE_CHANGE:
			return 0;
		case CLIENT_CONNECT_INFO:
			return 0;
		case WLAN_EVENT_SCAN:
			return 0;
		case CLIENT_STATE_IDLE:
			return 0;
		case WLAN_RADIO_RECOVERY:
			return 0;
		default:
			return 1;
	}
}

