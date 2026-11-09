#! /usr/bin/perl -w
package input_union;

use strict;
use warnings;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw(&input_union_init %union);
@EXPORT_OK = qw();

our %union;

$union{_avm_event_cmd_param} = {
    "priority" => 10000,
	"name" => "_avm_event_cmd_param",
	"selecttype" => "enum __avm_event_cmd",
	"union" => [
        { "type" => "struct _avm_event_cmd_param_register", "name" => "avm_event_cmd_param_register", "select" => [ "avm_event_cmd_register" ]  },
        { "type" => "struct _avm_event_cmd_param_release", "name" => "avm_event_cmd_param_release", "select" => [ "avm_event_cmd_release" ]  },
        { "type" => "struct _avm_event_cmd_param_trigger", "name" => "avm_event_cmd_param_trigger", "select" => [ "avm_event_cmd_trigger" ]  },
        { "type" => "struct _avm_event_cmd_param_register", "name" => "avm_event_cmd_param_source_register", "select" => [ "avm_event_cmd_source_register" ]  },
        { "type" => "struct _avm_event_cmd_param_source_trigger", "name" => "avm_event_cmd_param_source_trigger", "select" => [ "avm_event_cmd_source_trigger" ]  },
        ]
};

$union{__powermanagment_status_union} = {
    "priority" => 10000,
	"name" => "__powermanagment_status_union",
	"selecttype" => "enum _powermanagment_status_type",
	"union" => [
        { "type" => "unsigned int", "name" => "dsl_status", "select" => [ "dsl_status" ]  },
    ]
};

$union{avm_event_powermanagment_remote_union} = {
    "priority" => 10000,
	"name" => "avm_event_powermanagment_remote_union",
	"selecttype" => "enum avm_event_powermanagment_remote_action",
	"union" => [
        { "type" => "struct avm_event_powermanagment_remote_ressourceinfo", "name" => "ressourceinfo", "select" => [ "avm_event_powermanagment_ressourceinfo"]  },
        { "type" => "char", "name" => "powermode", "anzahl" => 32, "select" => [ "avm_event_powermanagment_activatepowermode" ]},
    ]
};

$union{avm_event_data_union} = {
    "priority" => 10000,
	"name" => "avm_event_data_union",
	"selecttype" => "enum _avm_event_id",
	"union" => [
        	{ "type" => "struct avm_event_boykott", "name" => "appl_status", "select" => [ "avm_event_id_appl_status"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "autoprov", "select" => [ "avm_event_id_autoprov"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "capiotcp_startstop", "select" => [ "avm_event_id_capiotcp_startstop"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "dsl_connect_status", "select" => [ "avm_event_id_dsl_connect_status"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "dsl_get", "select" => [ "avm_event_id_dsl_get"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "dsl_get_arch", "select" => [ "avm_event_id_dsl_get_arch"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "dsl_set", "select" => [ "avm_event_id_dsl_set"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "dsl_set_arch", "select" => [ "avm_event_id_dsl_set_arch"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "dsl_status", "select" => [ "avm_event_id_dsl_status"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "powerline_status", "select" => [ "avm_event_id_powerline_status"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "reboot_req", "select" => [ "avm_event_id_reboot_req"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "telefon_wlan_command", "select" => [ "avm_event_id_telefon_wlan_command"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "usb_status", "select" => [ "avm_event_id_usb_status"]  },
        	{ "type" => "struct avm_event_boykott", "name" => "user_source_notify", "select" => [ "avm_event_id_user_source_notify"]  },
        	{ "type" => "struct avm_event_push_button", "name" => "push_button", "select" => [ "avm_event_id_push_button"]  },
            { "type" => "struct _cpmac_event_struct", "name" => "cpmac", "select" => [ "avm_event_id_ethernet_connect_status"]  },
            { "type" => "struct avm_event_led_status", "name" => "led_status", "select" => [ "avm_event_id_led_status"]  },
            { "type" => "struct avm_event_led_info", "name" => "led_info", "select" => [ "avm_event_id_led_info"]  },
            { "type" => "struct avm_event_telefonprofile", "name" => "telefonprofile", "select" => [ "avm_event_id_telefonprofile"]  },
            { "type" => "struct avm_event_temperature", "name" => "temperature", "select" => [ "avm_event_id_temperature"]  },
            { "type" => "struct avm_event_powermanagment_status", "name" => "powermanagment_status", "select" => [ "avm_event_id_powermanagment_status"]  },
            { "type" => "struct avm_event_cpu_idle", "name" => "cpu_idle", "select" => [ "avm_event_id_cpu_idle"]  },
            { "type" => "struct avm_event_powermanagment_remote", "name" => "powermanagment_remote", "select" => [ "avm_event_id_powermanagment_remote"]  },
            { "type" => "struct avm_event_log", "name" => "id_log", "select" => [ "avm_event_id_log"]  },
            { "type" => "struct avm_event_remotepcmlink", "name" => "id_remotepcmlink", "select" => [ "avm_event_id_remotepcmlink"]  },
            { "type" => "struct avm_event_remotewatchdog", "name" => "id_remotewatchdog", "select" => [ "avm_event_id_remotewatchdog"]  },
            { "type" => "struct avm_event_rpc", "name" => "id_rpc", "select" => [ "avm_event_id_rpc"]  },
            { "type" => "struct avm_event_pm_info_stat", "name" => "pm_info_stat", "select" => [ "avm_event_id_pm_ressourceinfo_status"]  },
            { "type" => "struct avm_event_wlan", "name" => "wlan", "select" => [ "avm_event_id_wlan_client_status"]  },
            { "type" => "struct avm_event_wlan_power", "name" => "wlan_power", "select" => [ "avm_event_id_wlan_power"]  },
            { "type" => "struct wlan_event_def", "name" => "wlan_event", "select" => [ "avm_event_id_wlan_event"]  },
            { "type" => "struct avm_event_telephony_missed_call", "name" => "telephony_missed_call", "select" => [ "avm_event_id_telephony_missed_call"]  },
            { "type" => "struct avm_event_telephony_missed_call", "name" => "telephony_tam_call", "select" => [ "avm_event_id_telephony_tam_call"]  },
            { "type" => "struct avm_event_telephony_missed_call", "name" => "telephony_fax_received", "select" => [ "avm_event_id_telephony_fax_received"]  },
            { "type" => "struct avm_event_telephony_missed_call", "name" => "telephony_incoming_call", "select" => [ "avm_event_id_telephony_incoming_call"]  },
            { "type" => "struct avm_event_telephony_missed_call", "name" => "telephony_call_finished", "select" => [ "avm_event_id_telephony_call_finished"]  },
            { "type" => "struct avm_event_firmware_update_available", "name" => "firmware_update_available", "select" => [ "avm_event_id_firmware_update_available"]  },
            { "type" => "struct avm_event_internet_new_ip", "name" => "internet_new_ip", "select" => [ "avm_event_id_internet_new_ip"]  },
            { "type" => "struct avm_event_smarthome_switch_status", "name" => "smarthome_switch_status", select => [ "avm_event_id_smarthome_switch_status" ] },
            { "type" => "struct avm_event_mass_storage_mount", "name" => "mass_storage_mount", "select" => [ "avm_event_id_mass_storage_mount"]  },
            { "type" => "struct avm_event_mass_storage_unmount", "name" => "mass_storage_unmount", "select" => [ "avm_event_id_mass_storage_unmount"]  },
            { "type" => "struct avm_event_checkpoint", "name" => "checkpoint", "select" => [ "avm_event_id_checkpoint"]  },
            { "type" => "struct avm_event_cpu_run", "name" => "cpu_run", "select" => [ "avm_event_id_cpu_run"]  },
            { "type" => "struct avm_event_ambient_brightness", "name" => "ambient_brightness", "select" => [ "avm_event_id_ambient_brightness"]  },
            { "type" => "struct avm_event_fax_status", "name" => "fax_status", "select" => [ "avm_event_id_fax_status_change" ] },
            { "type" => "struct avm_event_fax_file", "name" => "fax_file", "select" => [ "avm_event_id_fax_file" ] },
            { "type" => "struct avm_event_piglet", "name" => "piglet", "select" => [ "avm_event_id_piglet" ] },
            { "type" => "struct avm_event_telefon_up", "name" => "telefon_up", "select" => [ "avm_event_id_telefon_up" ] },
            { "type" => "struct avm_event_smarthome", "name" => "smarthome", select => [ "avm_event_id_smarthome" ] },
            { "type" => "struct avm_event_boykott", "name" => "fiber_set_arch", "select" => [ "avm_event_id_fiber_set_arch"] },
            { "type" => "struct avm_event_boykott", "name" => "fiber_get_arch", "select" => [ "avm_event_id_fiber_get_arch"] },

	]
};

$union{avm_event_tffs_call_union} = {
    "priority" => 10000,
	"name" => "avm_event_tffs_call_union",
	"selecttype" => "enum avm_event_tffs_call_type",
	"union" => [
        	{ "type" => "struct avm_event_tffs_open",     "name" => "open",     "select" => [ "avm_event_tffs_call_open"]  },
        	{ "type" => "struct avm_event_tffs_close",    "name" => "close",    "select" => [ "avm_event_tffs_call_close"]  },
        	{ "type" => "struct avm_event_tffs_read",     "name" => "read",     "select" => [ "avm_event_tffs_call_read"]  },
        	{ "type" => "struct avm_event_tffs_write",    "name" => "write",    "select" => [ "avm_event_tffs_call_write"]  },
        	{ "type" => "struct avm_event_tffs_cleanup",  "name" => "cleanup",  "select" => [ "avm_event_tffs_call_cleanup"]  },
        	{ "type" => "struct avm_event_tffs_reindex",  "name" => "reindex",  "select" => [ "avm_event_tffs_call_reindex"]  },
        	{ "type" => "struct avm_event_tffs_info",     "name" => "info",     "select" => [ "avm_event_tffs_call_info"]  },
        	{ "type" => "struct avm_event_tffs_init",     "name" => "init",     "select" => [ "avm_event_tffs_call_init"]  },
        	{ "type" => "struct avm_event_tffs_deinit",   "name" => "deinit",   "select" => [ "avm_event_tffs_call_deinit"]  },
        	{ "type" => "struct avm_event_tffs_notify",   "name" => "notify",   "select" => [ "avm_event_tffs_call_notify"]  },
        	{ "type" => "struct avm_event_tffs_paniclog", "name" => "paniclog", "select" => [ "avm_event_tffs_call_paniclog"]  },
	]
};

$union{avm_event_message_union} = {
    "priority" => 10000,
    "name" => "avm_event_message_union",
    "selecttype" => "enum avm_event_msg_type",
    "union" => [
            { "type" => "struct avm_event_source_register", "name" => "source_register", "select" => [ "avm_event_source_register_type"]  },
            { "type" => "struct avm_event_source_unregister", "name" => "source_unregister", "select" => [ "avm_event_source_unregister_type"]  },
            { "type" => "struct avm_event_source_notifier", "name" => "source_notifier", "select" => [ "avm_event_source_notifier_type"]  },
            { "type" => "struct avm_event_remote_source_trigger_request", "name" => "remote_source_trigger_request", "select" => [ "avm_event_remote_source_trigger_request_type"]  },
            { "type" => "struct avm_event_ping", "name" => "ping", "select" => [ "avm_event_ping_type"]  },
            { "type" => "struct avm_event_tffs", "name" => "tffs", "select" => [ "avm_event_tffs_type"]  },
    ]
};

$union{avm_event_internet_new_ip_param} = {
    "priority" => 10000,
	"name" => "avm_event_internet_new_ip_param",
	"selecttype" => "enum avm_event_internet_new_ip_param_sel",
	"union" => [
        	{ "type" => "unsigned char",  "anzahl" => "4",  "name" => "ipv4", "select" => [ "avm_event_internet_new_ip_v4" ] },
        	{ "type" => "unsigned char",  "anzahl" => "16",  "name" => "ipv6", "select" => [ "avm_event_internet_new_ip_v6" ] }
    ]
};

$union{avm_event_wlan_client_status_u1} = {
    "priority" => 10000,
    "name" => "avm_event_wlan_client_status_u1",
    "selecttype" => "enum wlan_event_sel",
    "union" => [
        { "type" => "unsigned int", "name" => "sub_event",          "select" => [ "INPUT_RADAR_DFS_WAIT" ] },
        { "type" => "unsigned int", "name" => "active_rate",        "select" => [ "INPUT_MAC_AUTHORIZE" ] },
        { "type" => "unsigned int", "name" => "active_rate1",       "select" => [ "INPUT_EAP_AUTHORIZED" ] },
        { "type" => "unsigned int", "name" => "active_rate2",       "select" => [ "INPUT_MADWIFI_WRONG_PSK" ] },
        { "type" => "unsigned int", "name" => "active_rate3",       "select" => [ "INPUT_AUTH_EXPIRED" ] },
        { "type" => "unsigned int", "name" => "active_rate4",       "select" => [ "INPUT_STA" ] },
        { "type" => "unsigned int", "name" => "active_rate5",       "select" => [ "INPUT_WDS_LINK_UP" ] },
        { "type" => "unsigned int", "name" => "active_rate6",       "select" => [ "INPUT_WDS_LINK_DOWN" ] },
        { "type" => "unsigned int", "name" => "radar_chan",         "select" => [ "INPUT_RADAR" ] },
        { "type" => "unsigned int", "name" => "radar_chan1",        "select" => [ "INPUT_RADAR_DFS_WAIT_PRECAC" ] },
        { "type" => "unsigned int", "name" => "green_ap_ps_state",  "select" => [ "INPUT_GREENAP_PS" ] },
        { "type" => "unsigned int", "name" => "coexist_ht40_state", "select" => [ "INPUT_COEXIST_SWITCH" ] },
        { "type" => "unsigned int", "name" => "max_node_count",     "select" => [ "INPUT_MAX_NODE_REACHED" ] },
        { "type" => "unsigned int", "name" => "channel",            "select" => [ "INPUT_INTERFERENCE_CHAN_CHANGE" ] },

        { "type" => "unsigned int", "name" => "dummy0", "select" => [ "INPUT_AUTH_1_OS_A" ] },
    ]
};
$union{avm_event_wlan_client_status_u2} = {
    "priority" => 10000,
    "name" => "avm_event_wlan_client_status_u2",
    "selecttype" => "enum wlan_event_sel",
    "union" => [
        { "type" => "unsigned int", "name" => "radar_freq", "select" =>     [ "INPUT_RADAR" ] },
        { "type" => "unsigned int", "name" => "radar_freq1", "select" =>    [ "INPUT_RADAR_DFS_WAIT_PRECAC" ] },
        { "type" => "unsigned int", "name" => "wlan_mode",  "select" =>     [ "INPUT_AUTH_1_D" ] },
        { "type" => "unsigned int", "name" => "dummy0", "select" =>         [ "INPUT_AUTH_1_OS_A" ] },
    ]
};
$union{avm_event_wlan_client_status_u3} = {
    "priority"   => 10000,
    "name"       => "avm_event_wlan_client_status_u3",
    "selecttype" => "enum wlan_event_sel",
    "union"  => [
        { "type" => "unsigned int", "name" => "radar_time", "select" => [ "INPUT_RADAR_DFS_WAIT" ] },
        { "type" => "unsigned int", "name" => "flags"     , "select" => [ "INPUT_RADAR" ] },
        { "type" => "unsigned int", "name" => "dummy0"    , "select" => [ "INPUT_AUTH_1_OS_A" ] },
    ]
};

$union{avm_event_telephony_call_params} = {
    "priority" => 10000,
	"name" => "avm_event_telephony_call_params",
	"selecttype" => "enum avm_event_telephony_param_sel",
	"union" => [
        	{ "type" => "struct avm_event_telephony_string", "name" => "string",    "select" => [ "avm_event_telephony_params_name", "avm_event_telephony_params_msn_name", "avm_event_telephony_params_portname", "avm_event_telephony_params_tam_path" ]  },
        	{ "type" => "unsigned char",  "anzahl" => "32",  "name" => "number",    "select" => [ "avm_event_telephony_params_calling", "avm_event_telephony_params_called" ] },
            { "type" => "unsigned int", "name" => "duration", "select" => [ "avm_event_telephony_params_duration" ] },
            { "type" => "unsigned char", "name" => "port", "select" => [ "avm_event_telephony_params_port" ] },
            { "type" => "unsigned int", "name" => "id", "select" => [ "avm_event_telephony_params_id" ] }
    ]
};
##########################################################################################
# NEW WLAN EVENTS
##########################################################################################
$union{wlan_event_data} = {
    "priority" => 10000,
    "name" => "wlan_event_data",
    "selecttype" => "enum wlan_event_id",
    "union" => [
        { "type" => "struct wlan_event_data_client_state_change", "name" => "client_state_change", "select" => [ "CLIENT_STATE_CHANGE" ] },
        { "type" => "struct wlan_event_data_client_connect_info", "name" => "client_connect_info", "select" => [ "CLIENT_CONNECT_INFO" ] },
        { "type" => "struct wlan_event_data_scan_event_info"    , "name" => "scan_event_info"    , "select" => [ "WLAN_EVENT_SCAN"     ] },
        { "type" => "struct wlan_event_data_client_state_idle"  , "name" => "client_state_idle"  , "select" => [ "CLIENT_STATE_IDLE"   ] },
        { "type" => "struct wlan_event_data_radio_recovery"     , "name" => "radio_recovery"     , "select" => [ "WLAN_RADIO_RECOVERY" ] },
    ]
};
##########################################################################################
#
##########################################################################################
sub input_union_init {
    my $count = 0;
    foreach my $i ( sort keys %union ) {
        $count++;
    }
    print STDERR "[avm_event_input_union_init] " . $count . " unions defined\n";
}

1;
