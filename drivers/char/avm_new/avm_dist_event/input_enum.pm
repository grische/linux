#! /usr/bin/perl -w
package input_enum;

use strict;
use warnings;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw(%enum &input_enum_init);
@EXPORT_OK = qw();

our %enum;

$enum{_avm_event_id} = {
    "name" => "_avm_event_id",
    "enum" => [
        { "name" => "avm_event_id_wlan_client_status", "value" => "0" },
        { "name" => "avm_event_id_wlan_event", "value" => "1" },
        { "name" => "avm_event_id_wlan_power", "value" => "2" },
        { "name" => "avm_event_id_autoprov", "value" => "7" },
        { "name" => "avm_event_id_usb_status", "value" => "8" },
        { "name" => "avm_event_id_dsl_get_arch_kernel", "value" => "11" },
        { "name" => "avm_event_id_dsl_set_arch", "value" => "12" },
        { "name" => "avm_event_id_dsl_get_arch", "value" => "13" },
        { "name" => "avm_event_id_dsl_set", "value" => "14" },
        { "name" => "avm_event_id_dsl_get", "value" => "15" },
        { "name" => "avm_event_id_dsl_status", "value" => "16" },
        { "name" => "avm_event_id_dsl_connect_status", "value" => "17" },
        { "name" => "avm_event_id_push_button", "value" => "19" },
        { "name" => "avm_event_id_telefon_wlan_command", "value" => "20" },
        { "name" => "avm_event_id_capiotcp_startstop", "value" => "21" },
        { "name" => "avm_event_id_telefon_up", "value" => "22" },
        { "name" => "avm_event_id_reboot_req", "value" => "23" },
        { "name" => "avm_event_id_appl_status", "value" => "24" },
        { "name" => "avm_event_id_led_status", "value" => "25" },
        { "name" => "avm_event_id_led_info", "value" => "26" },
        { "name" => "avm_event_id_telefonprofile", "value" => "27" },
        { "name" => "avm_event_id_temperature", "value" => "28" },
        { "name" => "avm_event_id_cpu_idle", "value" => "29" },
        { "name" => "avm_event_id_powermanagment_status", "value" => "30" },
        { "name" => "avm_event_id_powerline_status", "value" => "31" },
        { "name" => "avm_event_id_ethernet_connect_status", "value" => "33" },
        { "name" => "avm_event_id_powermanagment_remote", "value" => "34" },
        { "name" => "avm_event_id_log", "value" => "35" },
        { "name" => "avm_event_id_remotewatchdog", "value" => "36" },
        { "name" => "avm_event_id_rpc", "value" => "37" },
        { "name" => "avm_event_id_remotepcmlink", "value" => "38" },
        { "name" => "avm_event_id_piglet", "value" => "39" },
        { "name" => "avm_event_id_pm_ressourceinfo_status", "value" => "40" },
        { "name" => "avm_event_id_telephony_missed_call", "value" => "41" },
        { "name" => "avm_event_id_telephony_tam_call", "value" => "42" },
        { "name" => "avm_event_id_telephony_fax_received", "value" => "43" },
        { "name" => "avm_event_id_internet_new_ip", "value" => "44" },
        { "name" => "avm_event_id_firmware_update_available", "value" => "45" },
        { "name" => "avm_event_id_smarthome_switch_status", "value" => "46" },
        { "name" => "avm_event_id_telephony_incoming_call", "value" => "47" },
        { "name" => "avm_event_id_mass_storage_mount", "value" => "48" },
        { "name" => "avm_event_id_mass_storage_unmount", "value" => "49" },
        { "name" => "avm_event_id_checkpoint", "value" => "50" },
        { "name" => "avm_event_id_cpu_run", "value" => "51" },
        { "name" => "avm_event_id_ambient_brightness", "value" => "52" },
        { "name" => "avm_event_id_fax_status_change", "value" => "53" },
        { "name" => "avm_event_id_fax_file", "value" => "54" },
        { "name" => "avm_event_id_telephony_call_finished", "value" => "55"},
        { "name" => "avm_event_id_smarthome", "value" => "56" },
        { "name" => "avm_event_id_fiber_set_arch", "value" => "57" },
        { "name" => "avm_event_id_fiber_get_arch", "value" => "58" },
        { "name" => "avm_event_id_user_source_notify", "value" => "63" },
        { "name" => "avm_event_last" },
    ]
};

$enum{avm_event_switch_type} = {
    "name" => "avm_event_switch_type",
    "enum" => [
        { "name" => "binary", "value" => "0" },
        { "name" => "percent", "value" => "1" },
    ]
};

$enum{avm_event_internet_new_ip_param_sel} = {
    "name" => "avm_event_internet_new_ip_param_sel",
    "enum" => [
        { "name" => "avm_event_internet_new_ip_v4", "value" => "0" },
        { "name" => "avm_event_internet_new_ip_v6", "value" => "1" },
    ]
};

$enum{avm_event_telephony_param_sel} = {
    "name" => "avm_event_telephony_param_sel",
    "enum" => [
        { "name" => "avm_event_telephony_params_name", "value" => "0" },
        { "name" => "avm_event_telephony_params_msn_name", "value" => "1" },
        { "name" => "avm_event_telephony_params_calling", "value" => "2" },
        { "name" => "avm_event_telephony_params_called", "value" => "3" },
        { "name" => "avm_event_telephony_params_duration", "value" => "4" },
        { "name" => "avm_event_telephony_params_port", "value" => "5" },
        { "name" => "avm_event_telephony_params_portname", "value" => "6" },
        { "name" => "avm_event_telephony_params_id", "value" => "7" },
        { "name" => "avm_event_telephony_params_tam_path", "value" => "8" }
    ]
};

$enum{__avm_event_cmd} = {
    "name" => "__avm_event_cmd",
    "enum" => [
        { "name" => "avm_event_cmd_register", "value" => "0" },
        { "name" => "avm_event_cmd_release", "value" => "1" },
        { "name" => "avm_event_cmd_source_register", "value" => "2" },
        { "name" => "avm_event_cmd_source_release", "value" => "3" },
        { "name" => "avm_event_cmd_source_trigger", "value" => "4" },
        { "name" => "avm_event_cmd_trigger", "value" => "5" },
        { "name" => "avm_event_cmd_undef", "value" => "6" },
    ]
};

$enum{_avm_event_push_button_key} = {
    "name" => "_avm_event_push_button_key",
    "enum" => [
        { "name" => "avm_event_push_button_wlan_on_off", "value" => "2" },
        { "name" => "avm_event_push_button_wlan_wps", "value" => "4" },
        { "name" => "avm_event_push_button_wlan_standby", "value" => "32" },
        { "name" => "avm_event_push_button_wlan_wps_station", "value" => "33" },
        { "name" => "avm_event_push_button_dect_paging", "value" => "11" },
        { "name" => "avm_event_push_button_dect_pairing", "value" => "13" },
        { "name" => "avm_event_push_button_dect_on_off", "value" => "42" },
        { "name" => "avm_event_push_button_dect_standby", "value" => "43" },
        { "name" => "avm_event_push_button_power_set_factory", "value" => "7" },
        { "name" => "avm_event_push_button_power_set_factory_kernel", "value" => "8" },
        { "name" => "avm_event_push_button_power_on_off", "value" => "51" },
        { "name" => "avm_event_push_button_power_standby", "value" => "52" },
        { "name" => "avm_event_push_button_power_socket_on_off", "value" => "53" },
        { "name" => "avm_event_push_button_tools_profiling", "value" => "61" },
        { "name" => "avm_event_push_button_plc_on_off", "value" => "71" },
        { "name" => "avm_event_push_button_plc_pairing", "value" => "72" },
        { "name" => "avm_event_push_button_led_standby", "value" => "81" },
        { "name" => "avm_event_push_button_2fa_success", "value" => "91" },
        { "name" => "avm_event_push_button_lte_wakeup", "value" => "92" },
        { "name" => "avm_event_push_button_plc_pairing_off", "value" => "93" },
        { "name" => "avm_event_push_button_wlan_wps_off", "value" => "94" },
        { "name" => "avm_event_push_button_dect_pairing_off", "value" => "95" },
        { "name" => "avm_event_push_button_nexus_pairing_off", "value" => "96" },
        { "name" => "avm_event_push_button_nexus_pairing", "value" => "97" },
        { "name" => "avm_event_push_button_wlan_wps_station_off", "value" => "98" },
        { "name" => "avm_event_push_button_nexus_pairing_box", "value" => "99" },
        { "name" => "avm_event_push_button_wlan_sta_wps_no_connect", "value" => "100" },
        { "name" => "avm_event_push_button_wlan_sta_wps_no_connect_off", "value" => "101" },
        { "name" => "avm_event_push_button_connect_methode1", "value" => "102" },
        { "name" => "avm_event_push_button_connect_methode2", "value" => "103" },
        { "name" => "avm_event_push_button_connect_methode3", "value" => "104" },
        { "name" => "avm_event_push_button_connect_methode4", "value" => "105" },
        { "name" => "avm_event_push_button_connect_methode5", "value" => "106" },
        { "name" => "avm_event_push_button_connect_methode6", "value" => "107" },
        { "name" => "avm_event_push_button_connect_off", "value" => "108" },
        { "name" => "avm_event_push_button_connect_timeout", "value" => "109" },
        { "name" => "avm_event_push_button_connect_error", "value" => "110" },
        { "name" => "avm_event_push_button_connect_success", "value" => "111" },
        { "name" => "avm_event_push_button_connect_started", "value" => "112" },
        { "name" => "avm_event_push_button_lan_pairing", "value" => "113" },
        { "name" => "avm_event_push_button_lan_pairing_off", "value" => "114" },
        { "name" => "avm_event_push_button_last" },
    ],
};

$enum{_avm_event_ethernet_speed} = {
    "name" => "_avm_event_ethernet_speed",
    "enum" => [
        { "name" => "avm_event_ethernet_speed_no_link", "value" => "0" },
        { "name" => "avm_event_ethernet_speed_10M", "value" => "1" },
        { "name" => "avm_event_ethernet_speed_100M", "value" => "2" },
        { "name" => "avm_event_ethernet_speed_1G", "value" => "3" },
        { "name" => "avm_event_ethernet_speed_2_5G", "value" => "4" },
        { "name" => "avm_event_ethernet_speed_5G", "value" => "5" },
        { "name" => "avm_event_ethernet_speed_10G", "value" => "6" },
        { "name" => "avm_event_ethernet_speed_error", "value" => "7" },
        { "name" => "avm_event_ethernet_speed_items", "value" => "8" },
     ],
};

$enum{avm_event_led_id} = {
    "name" => "avm_event_led_id",
    "enum" => [
        { "name" => "avm_logical_led_inval", "value" => "0" },
        { "name" => "avm_logical_led_ppp", "value" => "2" },
        { "name" => "avm_logical_led_error", "value" => "17" },
        { "name" => "avm_logical_led_pots", "value" => "13" },
        { "name" => "avm_logical_led_info", "value" => "7" },
        { "name" => "avm_logical_led_traffic", "value" => "18" },
        { "name" => "avm_logical_led_freecall", "value" => "16" },
        { "name" => "avm_logical_led_avmusbwlan", "value" => "19" },
        { "name" => "avm_logical_led_sip", "value" => "14" },
        { "name" => "avm_logical_led_mwi", "value" => "20" },
        { "name" => "avm_logical_led_fest_mwi", "value" => "21" },
        { "name" => "avm_logical_led_isdn_d", "value" => "12" },
        { "name" => "avm_logical_led_isdn_b1", "value" => "10" },
        { "name" => "avm_logical_led_isdn_b2", "value" => "11" },
        { "name" => "avm_logical_led_lan", "value" => "3" },
        { "name" => "avm_logical_led_lan1", "value" => "15" },
        { "name" => "avm_logical_led_adsl", "value" => "1" },
        { "name" => "avm_logical_led_power", "value" => "8" },
        { "name" => "avm_logical_led_usb", "value" => "5" },
        { "name" => "avm_logical_led_wifi", "value" => "4" },
        { "name" => "avm_logical_led_last", "value" => "99" },
    ]
};

$enum{_powermanagment_device} = {
    "name" => "_powermanagment_device",
    "enum" => [
        { "name" => "powerdevice_none", "value" => "0" },
        { "name" => "powerdevice_cpuclock", "value" => "1", "comment" => " power_rate in % Bezug: NormFrequenz 212 MHz " },
        { "name" => "powerdevice_dspclock", "value" => "2", "comment" => " power_rate in % Bezug: NormFrequenz 250 MHz " },
        { "name" => "powerdevice_systemclock", "value" => "3", "comment" => " power_rate in % Bezug: NormFrequenz 150 MHz " },
        { "name" => "powerdevice_wlan", "value" => "4", "comment" => " power_rate in % Maximal-Last " },
        { "name" => "powerdevice_isdnnt", "value" => "5", "comment" => " power_rate 0 oder 100 % (Ebene 1 aktiv)  " },
        { "name" => "powerdevice_isdnte", "value" => "6", "comment" => " power_rate 0 oder 100 % (Ebene 1 aktiv)  " },
        { "name" => "powerdevice_analog", "value" => "7", "comment" => " power_rate 100 % pro abgehobenen Telefon " },
        { "name" => "powerdevice_dect", "value" => "8", "comment" => " power_rate in % Maximal-Last " },
        { "name" => "powerdevice_ethernet", "value" => "9", "comment" => " power_rate 100 % pro aktiven Port " },
        { "name" => "powerdevice_dsl", "value" => "10", "comment" => " power_rate in % Maximal-Last (????) " },
        { "name" => "powerdevice_usb_host", "value" => "11", "comment" => " power_rate in Milli-Ampere " },
        { "name" => "powerdevice_usb_client", "value" => "12", "comment" => " power_rate 100 % der Maximal-Last " },
        { "name" => "powerdevice_charge", "value" => "13", "comment" => " power_rate in Milli-Watt " },
        { "name" => "powerdevice_loadrate", "value" => "14", "comment" => " power_rate in % (100 - % Idle-Wert) falls SMP: je 8 Bit eine CPU " },
        { "name" => "powerdevice_temperature", "value" => "15", "comment" => " power_rate in Grad Celcius " },
        { "name" => "powerdevice_dectsync", "value" => "16", "comment" => " power_rate clks_per_jiffies" },
        { "name" => "powerdevice_usb_host2", "value" => "17", "comment" => " power_rate in Milli-Ampere " },
        { "name" => "powerdevice_usb_host3", "value" => "18", "comment" => " power_rate in Milli-Ampere " },
        { "name" => "powerdevice_dsp_loadrate", "value" => "19", "comment" => " (ADSL/VDSL-)DSP power_rate in % (100 - % Idle-Wert) " },
        { "name" => "powerdevice_vdsp_loadrate", "value" => "20", "comment" => " Voice-DSP power_rate in % (100 - % Idle-Wert) " },
        { "name" => "powerdevice_lte", "value" => "21", "comment" => " power_rate in Milliwatt " },
        { "name" => "powerdevice_loadrate2", "value" => "22", "comment" => " Remote CPU power_rate in % falls SMP: je 8 Bit eine CPU" },
        { "name" => "powerdevice_dvbc", "value" => "23", "comment" => " power_rate in %" },
        { "name" => "powerdevice_plc", "value" => "24", "comment" => " power_rate in %" },
        { "name" => "powerdevice_maxdevices", "value" => "25" },
    ]
};

$enum{_powermanagment_status_type} = {
    "name" => "_powermanagment_status_type",
    "enum" => [
        { "name" => "dsl_status", "value" => "0" },
    ]
};
$enum{_cputype} = {
    "name" => "_cputype",
    "enum" => [
        { "name" => "host_cpu",   "value"  => "0" },
        { "name" => "remote_cpu", "value"  => "1" },
    ]
};

$enum{_avm_logtype} = {
    "name" => "_avm_logtype",
    "enum" => [
        { "name" => "local_panic", "value"  => "0" },
        { "name" => "local_crash", "value"  => "1" },
        { "name" => "remote_panic", "value" => "2" },
        { "name" => "remote_crash", "value" => "3" },
    ]
};
$enum{_avm_remote_wdt_cmd} = {
    "name" => "_avm_remote_wdt_cmd",
    "enum" => [
        { "name" => "wdt_register", "value"  =>  "0", "comment" => "RemoteCPU: param1: time to trigger"},
        { "name" => "wdt_release",  "value"  =>  "1", "comment" => "RemoteCPU"},
        { "name" => "wdt_trigger",  "value"  =>  "2", "comment" => "RemoteCPU: trigger"},
    ]
};

$enum{_avm_rpctype} = {
    "name" => "_avm_rpctype",
    "enum" => [
        { "name" => "command_to_arm",  "value" => "0" },
        { "name" => "command_to_atom", "value" => "1" },
        { "name" => "reply_to_arm",    "value" => "2" },
        { "name" => "reply_to_atom",   "value" => "3" },
    ]
};

$enum{_avm_remotepcmlinktype} = {
    "name" => "_avm_remotepcmlinktype",
    "enum" => [
        { "name" => "rpcmlink_register", "value" => "0" },
        { "name" => "rpcmlink_release",	 "value" => "1" },
    ]
};
$enum{_avm_piglettype} = {
    "name" => "_avm_piglettype",
    "enum" => [
        { "name" => "piglet_tdm_down",  "value" => "0" },
        { "name" => "piglet_tdm_ready", "value" => "1" },
    ]
};


$enum{avm_event_powermanagment_remote_action} = {
    "name" => "avm_event_powermanagment_remote_action",
    "enum" => [
        { "name" => "avm_event_powermanagment_ressourceinfo",     "value" => "0" },
        { "name" => "avm_event_powermanagment_activatepowermode", "value" => "1" },
    ]
};

$enum{avm_event_tffs_open_mode} = {
    "name" => "avm_event_tffs_open_mode",
    "enum" => [
        { "name" => "avm_event_tffs_mode_read",    "value" => "0" },
        { "name" => "avm_event_tffs_mode_write",   "value" => "1" },
        { "name" => "avm_event_tffs_mode_panic",   "value" => "2" },
    ]
};

$enum{avm_event_tffs_notify_event} = {
    "name" => "avm_event_tffs_notify_event",
    "enum" => [
        { "name" => "avm_event_tffs_notify_clear",  "value" => "0" },
        { "name" => "avm_event_tffs_notify_update", "value" => "1" },
        { "name" => "avm_event_tffs_notify_reinit", "value" => "2" },
    ]
};

$enum{avm_event_tffs_call_type} = {
    "name" => "avm_event_tffs_call_type",
    "enum" => [
        { "name" => "avm_event_tffs_call_open",    "value" => "0" },
        { "name" => "avm_event_tffs_call_close",   "value" => "1" },
        { "name" => "avm_event_tffs_call_read",    "value" => "2" },
        { "name" => "avm_event_tffs_call_write",   "value" => "3" },
        { "name" => "avm_event_tffs_call_cleanup", "value" => "4" },
        { "name" => "avm_event_tffs_call_reindex", "value" => "5" },
        { "name" => "avm_event_tffs_call_info",    "value" => "6" },
        { "name" => "avm_event_tffs_call_init",    "value" => "7" },
        { "name" => "avm_event_tffs_call_deinit",  "value" => "8" },
        { "name" => "avm_event_tffs_call_notify",  "value" => "9" },
        { "name" => "avm_event_tffs_call_paniclog","value" => "10" },
    ]
};

$enum{avm_event_msg_type} = {
    "name" => "avm_event_msg_type",
    "enum" => [
        { "name" => "avm_event_source_register_type",               "value" => "0" },
        { "name" => "avm_event_source_unregister_type",             "value" => "1" },
        { "name" => "avm_event_source_notifier_type",               "value" => "2" },
        { "name" => "avm_event_remote_source_trigger_request_type", "value" => "3" },
        { "name" => "avm_event_ping_type",                          "value" => "4" },
        { "name" => "avm_event_tffs_type",                          "value" => "5" },
    ]
};

$enum{avm_event_firmware_type} = {
    "name" => "avm_event_firmware_type",
    "enum" => [
        { "name" => "box_firmware",                     "value" => "0" },
        { "name" => "fritz_fon_firmware",               "value" => "1" },
        { "name" => "fritz_dect_repeater",              "value" => "2" },
        { "name" => "fritz_plug_switch",                "value" => "3" },
        { "name" => "fritz_hkr",                        "value" => "4" },
    ]
};

$enum{ePLCState} = {
    "name" => "ePLCState",
    "enum" => [
        { "name" => "PLCStateRunningNotConnected",   "value"  => "0" },
        { "name" => "PLCStateRunningConnected", "value"  => "1" },
        { "name" => "PLCStateNotRunning", "value"  => "2" },
    ]
};

$enum{_wlan_event} = {
    "name" => "wlan_event_sel",
    "typedef" => "wlan_event",
    "enum" => [
        { "name" => "INPUT_AUTH_1_OS_A", "value" => "0" },
        { "name" => "INPUT_AUTH_1_SK_A", "value" => "1" },
        { "name" => "INPUT_AUTH_1_D", "value" => "2" },
        { "name" => "INPUT_AUTH_3_A", "value" => "3" },
        { "name" => "INPUT_AUTH_3_D", "value" => "4" },
        { "name" => "INPUT_DE_AUTH_STATION", "value" => "5", "comment" => " A station sent DeAuthentication message " },
        { "name" => "INPUT_ASSOC_REQ_CHECK", "value" => "6" },
        { "name" => "INPUT_ASSOC_REQ_A", "value" => "7" },
        { "name" => "INPUT_ASSOC_REQ_D", "value" => "8" },
        { "name" => "INPUT_ASSOC_REQ_SEC_D", "value" => "9" },
        { "name" => "INPUT_RE_ASSOC_REQ_CHECK", "value" => "10" },
        { "name" => "INPUT_RE_ASSOC_REQ_A", "value" => "11" },
        { "name" => "INPUT_RE_ASSOC_REQ_D", "value" => "12" },
        { "name" => "INPUT_RE_ASSOC_REQ_SEC_D", "value" => "13" },
        { "name" => "INPUT_DIS_ASSOC_STATION", "value" => "14", "comment" => " A station sent Disassociation message " },
        { "name" => "INPUT_CLASS_3", "value" => "15" },
        { "name" => "INPUT_AUTH_TIMEOUT", "value" => "16" },
        { "name" => "INPUT_DE_AUTH_MNG_UNICAST", "value" => "17", "comment" => " Management unicast DeAuthentication  - by console command " },
        { "name" => "INPUT_DE_AUTH_MNG_BROADCAST", "value" => "18", "comment" => " Management broadcast DeAuthentication  - by console command " },
        { "name" => "INPUT_DIS_ASSOC_MNG_UNICAST", "value" => "19", "comment" => " Management unicast Disassociation  - by console command " },
        { "name" => "INPUT_DIS_ASSOC_MNG_BROADCAST", "value" => "20", "comment" => " Management broadcast Disassociation  - by console command " },
        { "name" => "INPUT_MAC_AUTHORIZE", "value" => "21", "comment" => " Management broadcast Disassociation  - by console command " },
        { "name" => "INPUT_MAC_DE_AUTHORIZE", "value" => "22", "comment" => " Management broadcast Disassociation  - by console command " },
        { "name" => "INPUT_WDS_LINK_UP", "value" => "23", "comment" => " WDS link establihed " },
        { "name" => "INPUT_WDS_LINK_DOWN", "value" => "24", "comment" => " WDS link lost " },
        { "name" => "INPUT_FRAME_TX_COMPLETE", "value" => "25", "comment" => " Frame TX complete " },
        { "name" => "INPUT_MADWIFI_WRONG_PSK", "value" => "26", "comment" => " Atheros (7270) registration error due wrong WPA key " },
        { "name" => "INPUT_WPS", "value" => "27" },
        { "name" => "INPUT_MINI", "value" => "28" },
        { "name" => "INPUT_RADAR", "value" => "29" },
        { "name" => "INPUT_WPS_ENROLLEE", "value" => "30" },
        { "name" => "INPUT_STA", "value" => "31" },
        { "name" => "INPUT_GREENAP_PS", "value" => "32" },
        { "name" => "INPUT_EAP_AUTHORIZED", "value" => "33", "comment" => " EAP has authorized client " },
        { "name" => "INPUT_MWO_INTERFERENCE", "value" => "34", "comment" => " Detect microwave and switch to HT20 " },
        { "name" => "INPUT_AUTH_EXPIRED", "value" => "35", "comment" => " Authentication expired, deauthenticate station " },
        { "name" => "INPUT_COEXIST_SWITCH", "value" => "36", "comment" => " Switch channel width due to 802.11 Coexistence feature " },
        { "name" => "INPUT_STA_ASSOC", "value" => "37", "comment" => " Station uplink association events " },
        { "name" => "INPUT_STA_AUTH", "value" => "38", "comment" => " Station uplink authentication events " },
        { "name" => "INPUT_STA_AUTHORIZATION", "value" => "39", "comment" => " Station uplink authorization events " },
        { "name" => "INPUT_WDS_NO_TIAGGR", "value" => "40", "comment" => " Warning, no TI G++ aggregation support " },
        { "name" => "INPUT_MAX_NODE_REACHED", "value" => "41", "comment" => " Max. number of connected stations reached " },
        { "name" => "INPUT_RADAR_DFS_WAIT", "value" => "42", "comment" => " Start/stop of DFS wait period " },
        { "name" => "INPUT_INTERFERENCE_CHAN_CHANGE", "value" => "43", "comment" => " Channel interference detection" },
        { "name" => "INPUT_RADAR_END", "value" => "44", "comment" => " Channel interference ended " },
        { "name" => "INPUT_RADAR_DFS_WAIT_PRECAC", "value" => "45", "comment" => " Start of DFS/PreCAC wait period " },
    ]
};

$enum{_wlan_info} = {
    "name" => "wlan_info_sel",
    "typedef" => "wlan_info",
    "enum" => [
		{ "name" => "STATUS_SUCCESSFUL", "value" => "0" },
		{ "name" => "STATUS_UNSPECIFIED", "value" => "1" },
		{ "name" => "STATUS_AUTH_NO_LONGER_VALID", "value" => "2" },
		{ "name" => "STATUS_DE_AUTH_STATION_IS_LEAVING", "value" => "3" },
		{ "name" => "STATUS_DIS_ASSOC_INACTIVITY", "value" => "4" },
		{ "name" => "STATUS_DIS_ASSOC_UNABLE_TO_HANDLE_ALL_CURRENTLY_ASSOC_STATIONS", "value" => "5" },
		{ "name" => "STATUS_CLASS_2_FROM_NON_AUTH_STATION", "value" => "6" },
		{ "name" => "STATUS_CLASS_3_FROM_NON_ASSOC_STATION", "value" => "7" },
		{ "name" => "STATUS_DIS_ASSOC_STATION_IS_LEAVING", "value" => "8" },
		{ "name" => "STATUS_STATION_REQUESTING_ASSOC_IS_NOT_AUTH", "value" => "9" },
		{ "name" => "STATUS_CAPABILITIES_FAILURE", "value" => "10" },
		{ "name" => "STATUS_REASSOC_DENIED_UNABLE_TO_CONFIRM_ASSOC", "value" => "11" },
		{ "name" => "STATUS_ASSOC_DENIED_OUT_OF_SCOPE", "value" => "12" },
		{ "name" => "STATUS_ALGO_IS_NOT_SUPPORTTED", "value" => "13" },
		{ "name" => "STATUS_AUTH_TRANSC_NUMBER_OUT_OF_SEQUENCE", "value" => "14" },
		{ "name" => "STATUS_AUTH_REJ_CHALLENGE_FAILURE", "value" => "15" },
		{ "name" => "STATUS_AUTH_REJ_TIMEOUT", "value" => "16" },
		{ "name" => "STATUS_ASSOC_DENIED_UNABLE_TO_HANDLE_ADDITIONAL_ASSOC", "value" => "17" },
		{ "name" => "STATUS_ASSOC_DENIED_RATE_FAILURE", "value" => "18" },
		{ "name" => "STATUS_ASSOC_DENIED_PREAMBLE_FAILURE", "value" => "19" },
		{ "name" => "STATUS_ASSOC_DENIED_PBCC_FAILURE", "value" => "20" },
		{ "name" => "STATUS_ASSOC_DENIED_AGILITY_FAILURE", "value" => "21" },
		{ "name" => "STATUS_DEAUTH_TX_COMPLETE_TIMEOUT", "value" => "22" },
		{ "name" => "STATUS_DEAUTH_TX_COMPLETE_OK", "value" => "23" },
		{ "name" => "STATUS_MAX", "value" => "24" },
		{ "name" => "STATUS_AUTHOR_SECMODE_FAILURE", "value" => "25" },
		{ "name" => "STATUS_ASSOC_DENIED_MODE_FAILURE", "value" => "26" },
		{ "name" => "STATUS_INVALID", "value" => "65535" },
        ]
};

$enum{wlan_info_special} = {
    "name" => "wlan_info_special",
    "enum" => [
		{ "name" => "STATUS_SUCCESS", "value" => "1" },
		{ "name" => "STATUS_FAILURE", "value" => "2" },
		{ "name" => "STATUS_TIMEOUT", "value" => "3" },
		{ "name" => "STATUS_WPS_START", "value" => "4" },
		{ "name" => "STATUS_WPS_DISCOVERY", "value" => "5" },
		{ "name" => "STATUS_WPS_OVERLAP", "value" => "6" },
		{ "name" => "STATUS_WPS_STOP", "value" => "7" },
        ]
};
$enum{wlan_sm_states} = {
    "name" => "wlan_sm_states",
    "enum" => [
		{ "name" => "WLAN_SM_STATE_IDLE", "value" => "0" },
		{ "name" => "WLAN_SM_STATE_AUTH_KEY", "value" => "1" },
		{ "name" => "WLAN_SM_STATE_AUTHENTICATED", "value" => "2" },
		{ "name" => "WLAN_SM_STATE_WAIT_FOR_ASS_RES", "value" => "3" },
		{ "name" => "WLAN_SM_STATE_ASSOCIATED", "value" => "4" },
		{ "name" => "WLAN_SM_STATE_AUTHORIZED", "value" => "5" },
		{ "name" => "WLAN_SM_STATE_DEAUTHENTICATE", "value" => "6" },
		{ "name" => "WLAN_SM_NUM_STATES", "value" => "7" },
        ]
};
##########################################################################################
# NEW WLAN EVENTS
##########################################################################################
$enum{wlan_event_id} = {
    "name" => "wlan_event_id",
    "enum" => [
        { "name" => "CLIENT_STATE_CHANGE", "value" => "0" },
        { "name" => "CLIENT_CONNECT_INFO", "value" => "1" },
        { "name" => "WLAN_EVENT_SCAN"    , "value" => "2" },
        { "name" => "CLIENT_STATE_IDLE"  , "value" => "3" },
        { "name" => "WLAN_RADIO_RECOVERY", "value" => "4" },
    ]
};

$enum{wlan_event_scan_type} = {
    "name" => "wlan_event_scan_type",
    "enum" => [
        { "name" => "WLAN_EVENT_SCAN_FINISHED", "value" => "0" },
    ]
};

$enum{wlan_event_radio_recovery_state} = {
    "name" => "wlan_event_radio_recovery_state",
    "enum" => [
        { "name" => "WLAN_EVENT_RADIO_RECOVERY_RADIO_DOWN", "value" => "0" },
        { "name" => "WLAN_EVENT_RADIO_RECOVERY_RADIO_UP"  , "value" => "1" },
    ]
};

##########################################################################################

$enum{fax_storage_dest} = {
    "name" => "fax_storage_dest",
    "enum" => [
        { "name" => "FAX_STORAGE_INTERNAL", "value" => "0", "comment" => "z. B. interner Speicher" },
        { "name" => "FAX_STORAGE_EXTERNAL", "value" => "1", "comment" => "z. B. USB" },
    ]
};

$enum{fax_receive_mode} = {
    "name" => "fax_receive_mode",
    "enum" => [
        { "name" => "FAX_RECEIVE_MODE_OFF", "value" => "0" },
        { "name" => "FAX_RECEIVE_MODE_MAIL_ONLY", "value" => "1" },
        { "name" => "FAX_RECEIVE_MODE_STORE_ONLY", "value" => "2" },
        { "name" => "FAX_RECEIVE_MODE_MAIL_AND_STORE", "value" => "3" },
    ]
};

$enum{fax_file_event_type} = {
    "name" => "fax_file_event_type",
    "enum" => [
        { "name" => "FAX_FILE_EVENT_NEW_FILE", "value" => "0" },
        { "name" => "FAX_FILE_EVENT_REMOVED_FILE", "value" => "1" },
        { "name" => "FAX_FILE_EVENT_READ", "value" => "2" },
        { "name" => "FAX_FILE_EVENT_UNREAD", "value" => "3" },
    ]
};

##########################################################################################
##########################################################################################
sub input_enum_init {
    my $count = 0;
    foreach my $i ( sort %enum ) {
        $count++;
    }
    print STDERR "[avm_event_input_enum_init] " . $count . " structs defined\n";
}

1;
