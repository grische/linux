#ifndef _avm_event_gen_enum_range_h_
#define _avm_event_gen_enum_range_h_

#include "avm_event_gen_types.h"

extern uint32_t avm_event_check_enum_range___avm_event_cmd(enum __avm_event_cmd E);
extern uint32_t avm_event_check_enum_range__avm_event_ethernet_speed(enum _avm_event_ethernet_speed E);
extern uint32_t avm_event_check_enum_range__avm_event_id(enum _avm_event_id E);
extern uint32_t avm_event_check_enum_range__avm_event_push_button_key(enum _avm_event_push_button_key E);
extern uint32_t avm_event_check_enum_range__avm_logtype(enum _avm_logtype E);
extern uint32_t avm_event_check_enum_range__avm_piglettype(enum _avm_piglettype E);
extern uint32_t avm_event_check_enum_range__avm_remote_wdt_cmd(enum _avm_remote_wdt_cmd E);
extern uint32_t avm_event_check_enum_range__avm_remotepcmlinktype(enum _avm_remotepcmlinktype E);
extern uint32_t avm_event_check_enum_range__avm_rpctype(enum _avm_rpctype E);
extern uint32_t avm_event_check_enum_range__cputype(enum _cputype E);
extern uint32_t avm_event_check_enum_range__powermanagment_device(enum _powermanagment_device E);
extern uint32_t avm_event_check_enum_range__powermanagment_status_type(enum _powermanagment_status_type E);
extern uint32_t avm_event_check_enum_range_wlan_event_sel(enum wlan_event_sel E);
extern uint32_t avm_event_check_enum_range_wlan_info_sel(enum wlan_info_sel E);
extern uint32_t avm_event_check_enum_range_avm_event_firmware_type(enum avm_event_firmware_type E);
extern uint32_t avm_event_check_enum_range_avm_event_internet_new_ip_param_sel(enum avm_event_internet_new_ip_param_sel E);
extern uint32_t avm_event_check_enum_range_avm_event_led_id(enum avm_event_led_id E);
extern uint32_t avm_event_check_enum_range_avm_event_msg_type(enum avm_event_msg_type E);
extern uint32_t avm_event_check_enum_range_avm_event_powermanagment_remote_action(enum avm_event_powermanagment_remote_action E);
extern uint32_t avm_event_check_enum_range_avm_event_switch_type(enum avm_event_switch_type E);
extern uint32_t avm_event_check_enum_range_avm_event_telephony_param_sel(enum avm_event_telephony_param_sel E);
extern uint32_t avm_event_check_enum_range_avm_event_tffs_call_type(enum avm_event_tffs_call_type E);
extern uint32_t avm_event_check_enum_range_avm_event_tffs_notify_event(enum avm_event_tffs_notify_event E);
extern uint32_t avm_event_check_enum_range_avm_event_tffs_open_mode(enum avm_event_tffs_open_mode E);
extern uint32_t avm_event_check_enum_range_ePLCState(enum ePLCState E);
extern uint32_t avm_event_check_enum_range_fax_file_event_type(enum fax_file_event_type E);
extern uint32_t avm_event_check_enum_range_fax_receive_mode(enum fax_receive_mode E);
extern uint32_t avm_event_check_enum_range_fax_storage_dest(enum fax_storage_dest E);
extern uint32_t avm_event_check_enum_range_wlan_event_id(enum wlan_event_id E);
extern uint32_t avm_event_check_enum_range_wlan_event_radio_recovery_state(enum wlan_event_radio_recovery_state E);
extern uint32_t avm_event_check_enum_range_wlan_event_scan_type(enum wlan_event_scan_type E);
extern uint32_t avm_event_check_enum_range_wlan_info_special(enum wlan_info_special E);
extern uint32_t avm_event_check_enum_range_wlan_sm_states(enum wlan_sm_states E);

#endif /*--- #ifndef _avm_event_gen_enum_range_h_ ---*/
extern int check_enum_for_union___powermanagment_status_union (enum _powermanagment_status_type E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union__avm_event_cmd_param (enum __avm_event_cmd E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_data_union (enum _avm_event_id E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_internet_new_ip_param (enum avm_event_internet_new_ip_param_sel E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_message_union (enum avm_event_msg_type E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_powermanagment_remote_union (enum avm_event_powermanagment_remote_action E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_telephony_call_params (enum avm_event_telephony_param_sel E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_tffs_call_union (enum avm_event_tffs_call_type E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_wlan_client_status_u1 (enum wlan_event_sel E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_wlan_client_status_u2 (enum wlan_event_sel E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_avm_event_wlan_client_status_u3 (enum wlan_event_sel E, unsigned int version __attribute__((unused)));
extern int check_enum_for_union_wlan_event_data (enum wlan_event_id E, unsigned int version __attribute__((unused)));
