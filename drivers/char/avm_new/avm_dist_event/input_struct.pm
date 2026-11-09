#! /usr/bin/perl -w
package input_struct;

use strict;
use warnings;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw(%struct &input_struct_init);
@EXPORT_OK = qw();

our %struct;
our %union;

$struct{_avm_event_id_mask} = {
    "priority" => 10000,
    "name" => "_avm_event_id_mask",
	"struct" => [
        { "type" => "avm_event_mask_fieldentry", "name" => "mask", "anzahl" => "avm_event_mask_fieldentries" },
    ]
};

$struct{_avm_event_cmd_param_register} = {
    "priority" => 10000,
    "name" => "_avm_event_cmd_param_register",
	"struct" => [
        { "type" => "struct _avm_event_id_mask", "name" => "mask" },
        { "type" => "char", "name" => "Name", "anzahl" => "MAX_EVENT_CLIENT_NAME_LEN + 1" }
    ]
};


$struct{_avm_event_cmd_param_release} = {
    "priority" => 10000,
    "name" => "_avm_event_cmd_param_release",
	"struct" => [
        { "type" => "char", "name" => "Name", "anzahl" => "MAX_EVENT_CLIENT_NAME_LEN + 1" }
    ]
};

$struct{_avm_event_cmd_param_trigger} = {
    "priority" => 10000,
    "name" => "_avm_event_cmd_param_trigger",
	"struct" => [
        { "type" => "enum _avm_event_id", "name" => "id" }
    ]
};

$struct{_avm_event_cmd_param_source_trigger} = {
    "priority" => 10000,
    "name" => "_avm_event_cmd_param_source_trigger",
	"struct" => [
        { "type" => "enum _avm_event_id", "name" => "id" },
        { "type" => "unsigned int", "name" => "data_length" }
    ]
};

$struct{_avm_event_cmd} = {
    "priority" => 10000,
    "name" => "_avm_event_cmd",
	"struct" => [
        { "type" => "enum __avm_event_cmd", "name" => "cmd", "select" => "set" },
        { "type" => "union _avm_event_cmd_param", "name" => "param", "select" => "use" }
    ]
};

$struct{_avm_event_header} = {
    "priority" => 10000,
    "name" => "_avm_event_header",
	"struct" => [
        { "type" => "enum _avm_event_id", "name" => "id" }
    ]
};

$struct{avm_event_user_mode_source_notify} = {
    "priority" => 10000,
    "name" => "avm_event_user_mode_source_notify",
	"struct" => [
        { "type" => "enum _avm_event_id", "name" => "id" }
    ]
};

$struct{_avm_event_user_mode_source_notify} = {
    "priority" => 10000,
    "name" => "_avm_event_user_mode_source_notify",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum _avm_event_id", "name" => "id" }
    ]
};


##########################################################################################
#
##########################################################################################
$struct{_avm_event_push_button} = {
    "priority" => 10000,
    "name" => "_avm_event_push_button",
	"struct" => [
        { "type" => "enum _avm_event_id", "name" => "id" },
        { "type" => "enum _avm_event_push_button_key", "name" => "key" },
        { "type" => "uint32_t", "name" => "pressed" },
    ]
};

$struct{avm_event_push_button} = {
    "priority" => 10000,
    "name" => "avm_event_push_button",
	"struct" => [
        { "type" => "enum _avm_event_push_button_key", "name" => "key" },
        { "type" => "uint32_t", "name" => "pressed" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_powermanagment_remote_ressourceinfo} = {
    "priority" => 10000,
    "name" => "_avm_event_powermanagment_remote_ressourceinfo",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum _powermanagment_device", "name" => "device" },
        { "type" => "unsigned int",  "name" => "power_rate" },
    ]
};

$struct{avm_event_powermanagment_remote_ressourceinfo} = {
    "priority" => 10000,
    "name" => "avm_event_powermanagment_remote_ressourceinfo",
	"struct" => [
        { "type" => "enum _powermanagment_device", "name" => "device" },
        { "type" => "unsigned int",  "name" => "power_rate" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_powermanagment_remote} = {
    "priority" => 10000,
    "name" => "_avm_event_powermanagment_remote",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum avm_event_powermanagment_remote_action", "name" => "remote_action", "selectkey" => "set" },
        { "type" => "union avm_event_powermanagment_remote_union", "name" => "param", "selectkey" => "use" },
    ]
};

$struct{avm_event_powermanagment_remote} = {
    "priority" => 10000,
    "name" => "avm_event_powermanagment_remote",
	"struct" => [
        { "type" => "enum avm_event_powermanagment_remote_action", "name" => "remote_action", "selectkey" => "set" },
        { "type" => "union avm_event_powermanagment_remote_union", "name" => "param", "selectkey" => "use" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{cpmac_port} = {
    "priority" => 10000,
	"name" => "cpmac_port",
	"struct" => [
		{ "type" => "uint8_t", "name" => "cable" },
        { "type" => "uint8_t", "name" => "link" },
        { "type" => "uint8_t", "name" => "speed100" },
        { "type" => "uint8_t", "name" => "fullduplex" },
        { "type" => "enum _avm_event_ethernet_speed", "name" => "speed" },
        { "type" => "enum _avm_event_ethernet_speed", "name" => "maxspeed" },
	]
};

$struct{cpmac_event_struct} = {
    "priority" => 10000,
    "name" => "cpmac_event_struct",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "unsigned int", "name" => "ports" },
        { "type" => "struct cpmac_port", "name" => "port", "anzahl" => "AVM_EVENT_ETH_MAXPORTS" },
    ]
};

$struct{_cpmac_event_struct} = {
    "priority" => 10000,
    "name" => "_cpmac_event_struct",
    "struct" => [
        { "type" => "unsigned int", "name" => "ports" },
        { "type" => "struct cpmac_port", "name" => "port", "anzahl" => "AVM_EVENT_ETH_MAXPORTS" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_wlan} = {
    "priority" => 10000,
    "name" => "_avm_event_wlan",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "char", "anzahl" => "6", "name" => "mac" },
        { "type" => "unsigned int", "name" => "u1" },
        { "type" => "unsigned int", "name" => "event" },
        { "type" => "unsigned int", "name" => "info" },
        { "type" => "unsigned int", "name" => "status" },
        { "type" => "unsigned int", "name" => "u2" },
        { "type" => "char", "anzahl" => "IFNAMSIZ", "name" => "if_name" },
        { "type" => "unsigned int", "name" => "ev_initiator" },
        { "type" => "unsigned int", "name" => "ev_reason" },
        { "type" => "unsigned int", "name" => "avm_capabilities" },
        { "type" => "unsigned int", "name" => "u3" },
        { "type" => "struct avm_event_wlan_credentials", "name" => "credentials" },
    ]
};

$struct{avm_event_wlan} = {
    "priority" => 10000,
    "name" => "avm_event_wlan",
    "struct" => [
        { "type" => "char", "anzahl" => "6", "name" => "mac" },
        { "type" => "union avm_event_wlan_client_status_u1", "name" => "u1" },
        { "type" => "wlan_event", "name" => "event" },
        { "type" => "wlan_info", "name" => "info" },
        { "type" => "enum wlan_sm_states", "name" => "status" },
        { "type" => "union avm_event_wlan_client_status_u2", "name" => "u2" },
        { "type" => "char", "anzahl" => "IFNAMSIZ", "name" => "if_name" },
        { "type" => "unsigned int", "name" => "ev_initiator" },
        { "type" => "unsigned int", "name" => "ev_reason" },
        { "type" => "unsigned int", "name" => "avm_capabilities" },
        { "type" => "union avm_event_wlan_client_status_u3", "name" => "u3" },
        { "type" => "struct avm_event_wlan_credentials", "name" => "credentials" },
    ]
};

$struct{avm_event_wlan_credentials} = {
    "priority" => 10000,
    "name" => "avm_event_wlan_credentials",
    "struct" => [
        { "type" => "char", "anzahl" => "33", "name" => "ssid" },
        { "type" => "unsigned char", "anzahl" => "6", "name" => "bssid" },
        { "type" => "char", "anzahl" => "65", "name" => "key" },
        { "type" => "unsigned int", "name" => "security" },
    ],
};

$struct{avm_event_wlan_power} = {
    "priority" => 10000,
    "name" => "avm_event_wlan_power",
    "struct" => [
        { "type" => "unsigned int", "name" => "power" },
    ],
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_led_status} = {
    "priority" => 10000,
    "name" => "_avm_event_led_status",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum _led_event", "name" => "led" },
        { "type" => "unsigned int", "name" => "state" },
        { "type" => "unsigned int", "name" => "param_len" },
        { "type" => "unsigned char", "name" => "params", "anzahl" => "AVM_LED_STATUS_MAX_PARAMLEN" },
    ]
};

$struct{avm_event_led_status} = {
    "priority" => 10000,
    "name" => "avm_event_led_status",
    "struct" => [
        { "type" => "enum _led_event", "name" => "led" },
        { "type" => "unsigned int", "name" => "state" },
        { "type" => "unsigned int", "name" => "param_len" },
        { "type" => "unsigned char", "name" => "params", "anzahl" => "AVM_LED_STATUS_MAX_PARAMLEN" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_led_info} = {
    "priority" => 10000,
    "name" => "_avm_event_led_info",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "unsigned int", "name" => "mode"  },
        { "type" => "unsigned int", "name" => "param1" },
        { "type" => "unsigned int", "name" => "param2" },
        { "type" => "unsigned int", "name" => "gpio_driver_type" },
        { "type" => "unsigned int", "name" => "gpio" },
        { "type" => "unsigned int", "name" => "pos"},
        { "type" => "char", "name" => "name", "anzahl" => "MAX_EVENT_SOURCE_NAME_LEN" },
    ]
};

$struct{avm_event_led_info} = {
    "priority" => 10000,
    "name" => "avm_event_led_info",
    "struct" => [
        { "type" => "unsigned int", "name" => "mode"  },
        { "type" => "unsigned int", "name" => "param1" },
        { "type" => "unsigned int", "name" => "param2" },
        { "type" => "unsigned int", "name" => "gpio_driver_type" },
        { "type" => "unsigned int", "name" => "gpio" },
        { "type" => "unsigned int", "name" => "pos"},
        { "type" => "char", "name" => "name", "anzahl" => "MAX_EVENT_SOURCE_NAME_LEN" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_telefonprofile} = {
    "priority" => 10000,
    "name" => "_avm_event_telefonprofile",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "unsigned int", "name" => "on" },
    ]
};

$struct{avm_event_telefonprofile} = {
    "priority" => 10000,
    "name" => "avm_event_telefonprofile",
    "struct" => [
        { "type" => "unsigned int", "name" => "on" },
    ]
};

$struct{avm_event_telefon_up} = {
    "priority" => 10000,
    "name" => "avm_event_telefon_up",
    "struct" => [
        { "type" => "struct {}", "name" => "dummy" },
    ]
};

$struct{avm_event_boykott} = {
    "priority" => 10000,
    "name" => "avm_event_boykott",
    "struct" => [
        { "type" => "struct {}", "name" => "dummy" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_powerline_status} = {
    "priority" => 10000,
    "name" => "_avm_event_powerline_status",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum ePLCState", "name" => "status" },
    ]
};

$struct{avm_event_powerline_status} = {
    "priority" => 10000,
    "name" => "avm_event_powerline_status",
    "struct" => [
        { "type" => "enum ePLCState", "name" => "status" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_ambient_brightness} = {
    "priority" => 10000,
    "name" => "_avm_event_ambient_brightness",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "unsigned int", "name" => "value" },
        { "type" => "unsigned int", "name" => "maxvalue" },
    ]
};

$struct{avm_event_ambient_brightness} = {
    "priority" => 10000,
    "name" => "avm_event_ambient_brightness",
    "struct" => [
        { "type" => "unsigned int", "name" => "value" },
        { "type" => "unsigned int", "name" => "maxvalue" },
    ]
};


##########################################################################################
#
##########################################################################################
$struct{_avm_event_temperature} = {
    "priority" => 10000,
    "name" => "_avm_event_temperature",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "int", "name" => "temperature" },
    ]
};

$struct{avm_event_temperature} = {
    "priority" => 10000,
    "name" => "avm_event_temperature",
    "struct" => [
        { "type" => "int", "name" => "temperature" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_powermanagment_status} = {
    "priority" => 10000,
    "name" => "_avm_event_powermanagment_status",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _powermanagment_status_type", "name" => "substatus", "selectkey" => "set" },
        { "type" => "union __powermanagment_status_union", "name" => "param", "selectkey" => "use" },
    ]
};

$struct{avm_event_powermanagment_status} = {
    "priority" => 10000,
    "name" => "avm_event_powermanagment_status",
    "struct" => [
        { "type" => "enum _powermanagment_status_type", "name" => "substatus", "selectkey" => "set" },
        { "type" => "union __powermanagment_status_union", "name" => "param", "selectkey" => "use" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_cpu_idle} = {
    "priority" => 10000,
    "name" => "_avm_event_cpu_idle",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "unsigned char", "name" => "cpu_idle" },
        { "type" => "unsigned char", "name" => "dsl_dsp_idle" },
        { "type" => "unsigned char", "name" => "voice_dsp_idle" },
        { "type" => "unsigned char", "name" => "mem_strictlyused" },
        { "type" => "unsigned char", "name" => "mem_cacheused" },
        { "type" => "unsigned char", "name" => "mem_physfree" },
        { "type" => "enum _cputype", "name" => "cputype" },
    ]
};

$struct{avm_event_cpu_idle} = {
    "priority" => 10000,
    "name" => "avm_event_cpu_idle",
    "struct" => [
        { "type" => "unsigned char", "name" => "cpu_idle" },
        { "type" => "unsigned char", "name" => "dsl_dsp_idle" },
        { "type" => "unsigned char", "name" => "voice_dsp_idle" },
        { "type" => "unsigned char", "name" => "mem_strictlyused" },
        { "type" => "unsigned char", "name" => "mem_cacheused" },
        { "type" => "unsigned char", "name" => "mem_physfree" },
        { "type" => "enum _cputype", "name" => "cputype" },
    ]
};
##########################################################################################
#
##########################################################################################
$struct{_avm_event_cpu_run} = {
    "priority" => 10000,
    "name" => "_avm_event_cpu_run",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _cputype", "name" => "cputype" },
        { "type" => "unsigned char", "name" => "cpu_run", "anzahl" => "4" },
    ]
};

$struct{avm_event_cpu_run} = {
    "priority" => 10000,
    "name" => "avm_event_cpu_run",
    "struct" => [
        { "type" => "enum _cputype", "name" => "cputype" },
        { "type" => "unsigned char", "name" => "cpu_run", "anzahl" => "4" },
    ]
};
##########################################################################################
#
##########################################################################################
$struct{_avm_event_log} = {
    "priority" => 10000,
    "name" => "_avm_event_log",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _avm_logtype", "name" => "logtype" },
        { "type" => "unsigned int", "name" =>  "loglen" },
        { "type" => "unsigned int", "name" =>  "logpointer" },
        { "type" => "unsigned int", "name" =>  "checksum" },
        { "type" => "unsigned int", "name" =>  "rebootflag" },
    ]
};
$struct{avm_event_log} = {
    "priority" => 10000,
    "name" => "avm_event_log",
    "struct" => [
        { "type" => "enum _avm_logtype", "name" => "logtype" },
        { "type" => "unsigned int",  "name" =>  "loglen" },
        { "type" => "unsigned int",  "name" =>  "logpointer" },
        { "type" => "unsigned int",  "name" =>  "checksum" },
        { "type" => "unsigned int",  "name" =>  "rebootflag" },
    ]
};
##########################################################################################
#
##########################################################################################
$struct{_avm_event_remotewatchdog} = {
    "priority" => 10000,
    "name" => "_avm_event_remotewatchdog",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _avm_remote_wdt_cmd", "name" => "cmd" },
        { "type" => "char", "name" => "name", "anzahl" => "16" },
        { "type" => "unsigned int", "name" =>  "param" },
    ]
};
$struct{avm_event_remotewatchdog} = {
    "priority" => 10000,
    "name" => "avm_event_remotewatchdog",
    "struct" => [
        { "type" => "enum _avm_remote_wdt_cmd", "name" => "cmd" },
        { "type" => "char", "name" => "name", "anzahl" => "16" },
        { "type" => "unsigned int", "name" =>  "param" },
    ]
};
##########################################################################################
#
##########################################################################################
$struct{_avm_event_rpc} = {
    "priority" => 10000,
    "name" => "_avm_event_rpc",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _avm_rpctype", "name" => "type" },
        { "type" => "unsigned int", "name" => "id" },
        { "type" => "unsigned int", "name" => "length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "message", "array" => "use" },
    ]
};
$struct{avm_event_rpc} = {
    "priority" => 10000,
    "name" => "avm_event_rpc",
    "struct" => [
        { "type" => "enum _avm_rpctype", "name" => "type" },
        { "type" => "unsigned int", "name" => "id" },
        { "type" => "unsigned int", "name" => "length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "message", "array" => "use" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_remotepcmlink} = {
    "priority" => 10000,
    "name" => "_avm_event_remotepcmlink",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _avm_remotepcmlinktype", "name" => "type" },
        { "type" => "unsigned int", "name" =>  "sharedlen" },
        { "type" => "unsigned int", "name" =>  "sharedpointer" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{avm_event_remotepcmlink} = {
    "priority" => 10000,
    "name" => "avm_event_remotepcmlink",
    "struct" => [
        { "type" => "enum _avm_remotepcmlinktype", "name" => "type" },
        { "type" => "unsigned int", "name" =>  "sharedlen" },
        { "type" => "unsigned int", "name" =>  "sharedpointer" },
    ]
};
##########################################################################################
#
##########################################################################################
$struct{_avm_event_piglet} = {
    "priority" => 10000,
    "name" => "_avm_event_piglet",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "enum _avm_piglettype", "name" => "type" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{avm_event_piglet} = {
    "priority" => 10000,
    "name" => "avm_event_piglet",
    "struct" => [
        { "type" => "enum _avm_piglettype", "name" => "type" },
    ]
};
#
##########################################################################################
#
##########################################################################################
$struct{_avm_event_pm_info_stat} = {
    "priority" => 10000,
    "name" => "_avm_event_pm_info_stat",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "unsigned char", "name" => "reserved1" },
        { "type" => "unsigned char", "name" => "rate_sumact" },
        { "type" => "unsigned char", "name" => "rate_sumcum" },
        { "type" => "unsigned char", "name" => "rate_systemact" },
        { "type" => "unsigned char", "name" => "rate_systemcum" },
        { "type" => "unsigned char", "name" => "system_status" },
        { "type" => "unsigned char", "name" => "rate_dspact" },
        { "type" => "unsigned char", "name" => "rate_dspcum" },
        { "type" => "unsigned char", "name" => "rate_wlanact" },
        { "type" => "unsigned char", "name" => "rate_wlancum" },
        { "type" => "unsigned char", "name" => "wlan_devices" },
        { "type" => "unsigned char", "name" => "wlan_status" },
        { "type" => "unsigned char", "name" => "rate_ethact" },
        { "type" => "unsigned char", "name" => "rate_ethcum" },
        { "type" => "unsigned short", "name" => "eth_status" },
        { "type" => "unsigned char", "name" => "rate_abact" },
        { "type" => "unsigned char", "name" => "rate_abcum" },
        { "type" => "unsigned short", "name" => "isdn_status" },
        { "type" => "unsigned char", "name" => "rate_dectact" },
        { "type" => "unsigned char", "name" => "rate_dectcum" },
        { "type" => "unsigned char", "name" => "rate_battchargeact" },
        { "type" => "unsigned char", "name" => "rate_battchargecum" },
        { "type" => "unsigned char", "name" => "dect_status" },
        { "type" => "unsigned char", "name" => "rate_usbhostact" },
        { "type" => "unsigned char", "name" => "rate_usbhostcum" },
        { "type" => "unsigned char", "name" => "usb_status" },
        { "type" => "signed char", "name" => "act_temperature" },
        { "type" => "signed char", "name" => "min_temperature" },
        { "type" => "signed char", "name" => "max_temperature" },
        { "type" => "signed char", "name" => "avg_temperature" },
        { "type" => "unsigned char", "name" => "rate_lteact" },
        { "type" => "unsigned char", "name" => "rate_ltecum" },
        { "type" => "unsigned char", "name" => "rate_dvbcact" },
        { "type" => "unsigned char", "name" => "rate_dvbccum" },
        { "type" => "unsigned char", "name" => "rate_plcact" },
        { "type" => "unsigned char", "name" => "rate_plccum" },
    ]
};

$struct{avm_event_pm_info_stat} = {
    "priority" => 10000,
    "name" => "avm_event_pm_info_stat",
    "struct" => [
        { "type" => "unsigned char", "name" => "reserved1" },
        { "type" => "unsigned char", "name" => "rate_sumact" },
        { "type" => "unsigned char", "name" => "rate_sumcum" },
        { "type" => "unsigned char", "name" => "rate_systemact" },
        { "type" => "unsigned char", "name" => "rate_systemcum" },
        { "type" => "unsigned char", "name" => "system_status" },
        { "type" => "unsigned char", "name" => "rate_dspact" },
        { "type" => "unsigned char", "name" => "rate_dspcum" },
        { "type" => "unsigned char", "name" => "rate_wlanact" },
        { "type" => "unsigned char", "name" => "rate_wlancum" },
        { "type" => "unsigned char", "name" => "wlan_devices" },
        { "type" => "unsigned char", "name" => "wlan_status" },
        { "type" => "unsigned char", "name" => "rate_ethact" },
        { "type" => "unsigned char", "name" => "rate_ethcum" },
        { "type" => "unsigned short", "name" => "eth_status" },
        { "type" => "unsigned char", "name" => "rate_abact" },
        { "type" => "unsigned char", "name" => "rate_abcum" },
        { "type" => "unsigned short", "name" => "isdn_status" },
        { "type" => "unsigned char", "name" => "rate_dectact" },
        { "type" => "unsigned char", "name" => "rate_dectcum" },
        { "type" => "unsigned char", "name" => "rate_battchargeact" },
        { "type" => "unsigned char", "name" => "rate_battchargecum" },
        { "type" => "unsigned char", "name" => "dect_status" },
        { "type" => "unsigned char", "name" => "rate_usbhostact" },
        { "type" => "unsigned char", "name" => "rate_usbhostcum" },
        { "type" => "unsigned char", "name" => "usb_status" },
        { "type" => "signed char", "name" => "act_temperature" },
        { "type" => "signed char", "name" => "min_temperature" },
        { "type" => "signed char", "name" => "max_temperature" },
        { "type" => "signed char", "name" => "avg_temperature" },
        { "type" => "unsigned char", "name" => "rate_lteact" },
        { "type" => "unsigned char", "name" => "rate_ltecum" },
        { "type" => "unsigned char", "name" => "rate_dvbcact" },
        { "type" => "unsigned char", "name" => "rate_dvbccum" },
        { "type" => "unsigned char", "name" => "rate_plcact" },
        { "type" => "unsigned char", "name" => "rate_plccum" },
    ]
};
##########################################################################################
#
##########################################################################################

$struct{avm_event_unserialised} = {
    "priority" => 10000,
    "name" => "avm_event_unserialised",
    "struct" => [
        { "type" => "uint64_t", "name" => "evnt_id" },
        { "type" => "uint32_t", "name" => "data_len" },
        { "type" => "unsigned char", "name" => "data" },
    ]
};

$struct{avm_event_data} = {
    "priority" => 10000,
    "name" => "avm_event_data",
    "struct" => [
        { "type" => "enum _avm_event_id", "name" => "id", "selectkey" => "set" },
        { "type" => "union avm_event_data_union", "name" => "data", "selectkey" => "use" },
    ]
};

$struct{avm_event_source_register} = {
    "priority" => 10000,
    "name" => "avm_event_source_register",
    "struct" => [
        { "type" => "struct _avm_event_id_mask", "name" => "id_mask" },
        { "type" => "char", "name" => "name", "anzahl" => "MAX_EVENT_SOURCE_NAME_LEN" },
    ]
};

$struct{avm_event_source_unregister} = {
    "priority" => 10000,
    "name" => "avm_event_source_unregister",
    "struct" => [
        { "type" => "struct _avm_event_id_mask", "name" => "id_mask" },
        { "type" => "char", "name" => "name", "anzahl" => "MAX_EVENT_SOURCE_NAME_LEN" },
    ]
};

$struct{avm_event_source_notifier} = {
    "priority" => 10000,
    "name" => "avm_event_source_notifier",
    "struct" => [
        { "type" => "enum _avm_event_id", "name" => "id" },
    ]
};

$struct{avm_event_remote_source_trigger_request} = {
    "priority" => 10000,
    "name" => "avm_event_remote_source_trigger_request",
    "struct" => [
        { "type" => "struct avm_event_data", "name" => "data" },
    ]
};

$struct{avm_event_ping} = {
    "priority" => 10000,
    "name" => "avm_event_ping",
    "struct" => [
        { "type" => "uint32_t", "name" => "seq" },
    ]
};

##########################################################################################
#
##########################################################################################

$struct{avm_event_tffs_open} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_open",
    "struct" => [
        { "type" => "uint32_t",                      "name" => "id" },
        { "type" => "enum avm_event_tffs_open_mode", "name" => "mode" },
        { "type" => "uint32_t",                      "name" => "max_segment_size" },
    ]
};

$struct{avm_event_tffs_close} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_close",
    "struct" => [
        { "type" => "uint32_t", "name" => "dummy" },
    ]
};

$struct{avm_event_tffs_read} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_read",
    "struct" => [
        { "type" => "uint64_t", "name" => "buff_addr" },
        { "type" => "uint64_t", "name" => "len" },
        { "type" => "uint32_t", "name" => "id" },
        { "type" => "int32_t",  "name" => "crc" },
    ]
};

$struct{avm_event_tffs_write} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_write",
    "struct" => [
        { "type" => "uint64_t", "name" => "buff_addr" },
        { "type" => "uint64_t", "name" => "len" },
        { "type" => "uint32_t", "name" => "id" },
        { "type" => "uint32_t", "name" => "final" },
        { "type" => "int32_t",  "name" => "crc" },
    ]
};

$struct{avm_event_tffs_cleanup} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_cleanup",
    "struct" => [
        { "type" => "uint32_t", "name" => "dummy" },
    ]
};

$struct{avm_event_tffs_reindex} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_reindex",
    "struct" => [
        { "type" => "uint32_t", "name" => "dummy" },
    ]
};

$struct{avm_event_tffs_info} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_info",
    "struct" => [
        { "type" => "uint32_t", "name" => "fill_level" },
    ]
};

$struct{avm_event_tffs_init} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_init",
    "struct" => [
        { "type" => "int64_t", "name" => "mem_offset" },
        { "type" => "uint32_t", "name" => "max_seg_size" },
    ]
};

$struct{avm_event_tffs_deinit} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_deinit",
    "struct" => [
        { "type" => "uint32_t", "name" => "dummy" },
    ]
};

$struct{avm_event_tffs_notify} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_notify",
    "struct" => [
        { "type" => "uint32_t", "name" => "id" },
        { "type" => "enum avm_event_tffs_notify_event", "name" => "event" },
    ]
};

$struct{avm_event_tffs_paniclog} = {
    "priority" => 10000,
    "name" => "avm_event_tffs_paniclog",
    "struct" => [
        { "type" => "uint64_t", "name" => "buff_addr" },
        { "type" => "uint64_t", "name" => "len" },
        { "type" => "int32_t",  "name" => "crc" },
    ]
};

$struct{avm_event_tffs} = {
    "priority" => 10000,
    "name" => "avm_event_tffs",
    "struct" => [
        { "type" => "uint32_t", "name" => "src_id" },
        { "type" => "uint32_t", "name" => "dst_id" },
        { "type" => "uint32_t", "name" => "seq_nr" },
        { "type" => "uint32_t", "name" => "ack" },
        { "type" => "uint64_t", "name" => "srv_handle" },
        { "type" => "uint64_t", "name" => "clt_handle" },
        { "type" => "int32_t",  "name" => "result" },
        { "type" => "enum avm_event_tffs_call_type",   "name" => "type", "selectkey" => "set" },
        { "type" => "union avm_event_tffs_call_union", "name" => "call", "selectkey" => "use" },
    ]
};

$struct{avm_event_message} = {
    "priority" => 10000,
    "name" => "avm_event_message",
    "struct" => [
        { "type" => "uint32_t", "name" => "length", "length" => "total" },
        { "type" => "uint32_t", "name" => "magic" },
        { "type" => "uint32_t", "name" => "nonce" },
        { "type" => "uint32_t", "name" => "flags" },
        { "type" => "int32_t",  "name" => "result" },
        { "type" => "uint32_t", "name" => "transmitter_handle" },
        { "type" => "uint32_t", "name" => "receiver_handle" },
        { "type" => "enum avm_event_msg_type",       "name" => "type",    "selectkey" => "set" },
        { "type" => "union avm_event_message_union", "name" => "message", "selectkey" => "use" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{avm_event_telephony_string} = {
    "priority" => 10000,
    "name" => "avm_event_telephony_string",
	"struct" => [
        { "type" => "unsigned int", "name" => "length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "string", "array" => "use" }
    ]
};

$struct{_avm_event_telephony_missed_call_params} = {
    "priority" => 10000,
    "name" => "_avm_event_telephony_missed_call_params",
	"struct" => [
        { "type" => "enum avm_event_telephony_param_sel",    "name" => "id",     "selectkey" => "set" },
        { "type" => "union avm_event_telephony_call_params", "name" => "params", "selectkey" => "use" }
    ]
};



$struct{_avm_event_telephony_missed_call} = {
    "priority" => 10000,
    "name" => "_avm_event_telephony_missed_call",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "unsigned int", "name" => "length", "array" => "element_anzahl" },
        { "type" => "struct _avm_event_telephony_missed_call_params", "name" => "p", "anzahl" => "0", "array" => "use" },
    ]
};

$struct{avm_event_telephony_missed_call} = {
    "priority" => 10000,
    "name" => "avm_event_telephony_missed_call",
	"struct" => [
        { "type" => "unsigned int", "name" => "length", "array" => "element_anzahl" },
        { "type" => "struct _avm_event_telephony_missed_call_params", "name" => "p", "anzahl" => "0", "array" => "use" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_internet_new_ip} = {
    "priority" => 10000,
    "name" => "_avm_event_internet_new_ip",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum avm_event_internet_new_ip_param_sel", "name" => "sel", "selectkey" => "set" },
        { "type" => "union avm_event_internet_new_ip_param", "name" => "params", "selectkey" => "use" }
    ]
};

$struct{avm_event_internet_new_ip} = {
    "priority" => 10000,
    "name" => "avm_event_internet_new_ip",
	"struct" => [
        { "type" => "enum avm_event_internet_new_ip_param_sel", "name" => "sel", "selectkey" => "set" },
        { "type" => "union avm_event_internet_new_ip_param", "name" => "params", "selectkey" => "use" }
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_firmware_update_available} = {
    "priority" => 10000,
    "name" => "_avm_event_firmware_update_available",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum avm_event_firmware_type", "name" => "type" },
        { "type" => "unsigned int", "name" => "version_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "version", "array" => "use" }
    ]
};

$struct{avm_event_firmware_update_available} = {
    "priority" => 10000,
    "name" => "avm_event_firmware_update_available",
	"struct" => [
        { "type" => "enum avm_event_firmware_type", "name" => "type" },
        { "type" => "unsigned int", "name" => "version_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "version", "array" => "use" }
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_smarthome_switch_status} = {
    "priority" => 10000,
    "name" => "_avm_event_smarthome_switch_status",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum avm_event_switch_type", "name" => "type" },
        { "type" => "unsigned int", "name" => "value" },
        { "type" => "unsigned int", "name" => "ain_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "ain", "array" => "use" }
    ]
};

$struct{avm_event_smarthome_switch_status} = {
    "priority" => 10000,
    "name" => "avm_event_smarthome_switch_status",
	"struct" => [
        { "type" => "enum avm_event_switch_type", "name" => "type" },
        { "type" => "unsigned int", "name" => "value" },
        { "type" => "unsigned int", "name" => "ain_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "ain", "array" => "use" }
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_mass_storage_mount} = {
    "priority" => 10000,
    "name" => "_avm_event_mass_storage_mount",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "unsigned long long", "name" => "size" },
        { "type" => "unsigned long long", "name" => "free" },
        { "type" => "unsigned int", "name" => "name_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "name", "array" => "use" }
    ]
};

$struct{avm_event_mass_storage_mount} = {
    "priority" => 10000,
    "name" => "avm_event_mass_storage_mount",
	"struct" => [
        { "type" => "unsigned long long", "name" => "size" },
        { "type" => "unsigned long long", "name" => "free" },
        { "type" => "unsigned int", "name" => "name_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "name", "array" => "use" }
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_mass_storage_unmount} = {
    "priority" => 10000,
    "name" => "_avm_event_mass_storage_unmount",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "unsigned int", "name" => "name_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "name", "array" => "use" }
    ]
};

$struct{avm_event_mass_storage_unmount} = {
    "priority" => 10000,
    "name" => "avm_event_mass_storage_unmount",
	"struct" => [
        { "type" => "unsigned int", "name" => "name_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "name", "array" => "use" }
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_checkpoint} = {
    "priority" => 10000,
    "name" => "_avm_event_checkpoint",
    "struct" => [
        { "type" => "struct _avm_event_header", "name" => "event_header" },
        { "type" => "uint32_t", "name" => "node_id" },
        { "type" => "uint64_t", "name" => "checkpoints" },
    ]
};

$struct{avm_event_checkpoint} = {
    "priority" => 10000,
    "name" => "avm_event_checkpoint",
    "struct" => [
        { "type" => "uint32_t", "name" => "node_id" },
        { "type" => "uint64_t", "name" => "checkpoints" },
    ]
};
##########################################################################################
# NEW WLAN EVENTS
##########################################################################################
$struct{wlan_event_data_client_common} = {
    "priority" => 10000,
    "name" => "wlan_event_data_client_common",
    "struct" => [
		{ "type" => "uint8_t", "anzahl" => "6", "name" => "mac" },
		{ "type" => "char", "anzahl" => "IFNAMSIZ + 1", "name" => "iface" },
	    { "type" => "char", "anzahl" => "16 + 1", "name" => "initiator" },
    ]
};

$struct{wlan_event_data_client_state_change} = {
    "priority" => 10000,
    "name" => "wlan_event_data_client_state_change",
    "struct" => [
        { "type" => "struct wlan_event_data_client_common", "name" => "common" },
        { "type" => "uint8_t", "name" => "state" },
    ]
};

$struct{wlan_event_data_client_connect_info} = {
    "priority" => 10000,
    "name" => "wlan_event_data_client_connect_info",
    "struct" => [
        { "type" => "struct wlan_event_data_client_common", "name" => "common" },
        { "type" => "uint8_t", "name" => "info_context" },
        { "type" => "uint8_t", "name" => "reason" },
        { "type" => "uint32_t", "name" => "max_node_count" },
        { "type" => "uint16_t", "name" => "ieee80211_code" },
    ]
};

$struct{wlan_event_data_client_state_idle} = {
    "priority" => 10000,
    "name" => "wlan_event_data_client_state_idle",
    "struct" => [
        { "type" => "struct wlan_event_data_client_common", "name" => "common" },
        { "type" => "uint8_t", "name" => "idle" },
    ]
};


$struct{wlan_event_data_scan_common} = {
    "priority" => 10000,
    "name" => "wlan_event_data_scan_common",
    "struct" => [
        { "type" => "char", "anzahl" => "IFNAMSIZ + 1", "name" => "iface" },
        { "type" => "char", "anzahl" => "16 + 1"      , "name" => "initiator" },
    ]
};

$struct{wlan_event_data_scan_event_info} = {
    "priority" => 10000,
    "name" => "wlan_event_data_scan_event_info",
    "struct" => [
        { "type" => "struct wlan_event_data_scan_common", "name" => "common" },
        { "type" => "enum wlan_event_scan_type",          "name" => "event_type" },
    ]
};

$struct{wlan_event_data_radio_recovery} = {
    "priority" => 10000,
    "name" => "wlan_event_data_radio_recovery",
    "struct" => [
        { "type" => "int"                                 , "name" => "radio_id" },
        { "type" => "enum wlan_event_radio_recovery_state", "name" => "state"    },
    ]
};


$struct{wlan_event_def} = {
    "priority" => 10000,
    "name" => "wlan_event_def",
    "struct" => [
        { "type" => "enum wlan_event_id", "name" => "event_id", "selectkey" => "set" },
        { "type" => "union wlan_event_data", "name" => "event_data", "selectkey" => "use" },
    ]
};

##########################################################################################
#
##########################################################################################
$struct{_avm_event_fax_status} = {
    "priority" => 10000,
    "name" => "_avm_event_fax_status",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum fax_receive_mode", "name" => "fax_receive_mode" },
        { "type" => "enum fax_storage_dest", "name" => "fax_storage_dest" },
        { "type" => "unsigned int", "name" => "dirname_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "dirname", "array" => "use" }
    ]
};

$struct{avm_event_fax_status} = {
    "priority" => 10000,
    "name" => "avm_event_fax_status",
	"struct" => [
        { "type" => "enum fax_receive_mode", "name" => "fax_receive_mode" },
        { "type" => "enum fax_storage_dest", "name" => "fax_storage_dest" },
        { "type" => "unsigned int", "name" => "dirname_length", "array" => "element_anzahl" },
        { "type" => "unsigned char", "anzahl" => "0", "name" => "dirname", "array" => "use" }
    ]
};

##########################################################################################
#
##########################################################################################

$struct{_avm_event_fax_file} = {
    "priority" => 10000,
    "name" => "_avm_event_fax_file",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "enum fax_file_event_type", "name" => "action" },
        { "type" => "time_t", "name" => "date" }
    ]
};

$struct{avm_event_fax_file} = {
    "priority" => 10000,
    "name" => "avm_event_fax_file",
	"struct" => [
        { "type" => "enum fax_file_event_type", "name" => "action" },
        { "type" => "time_t", "name" => "date" }
    ]
};

##########################################################################################
##########################################################################################
$struct{_avm_event_smarthome} = {
    "priority" => 10000,
    "name" => "_avm_event_smarthome",
	"struct" => [
        { "type" => "struct _avm_event_header", "name" => "header" },
        { "type" => "uint16_t", "name" => "length" },
        { "type" => "char", "anzahl" => "20", "name" => "ident", "array" => "use" },
        { "type" => "uint16_t", "name" => "type" },
        { "type" => "struct {}", "name" => "payload" }
    ]
};

$struct{avm_event_smarthome} = {
    "priority" => 10000,
    "name" => "avm_event_smarthome",
	"struct" => [
        { "type" => "uint16_t", "name" => "length" },
        { "type" => "char", "anzahl" => "20", "name" => "ident", "array" => "use" },
        { "type" => "uint16_t", "name" => "type" },
        { "type" => "struct {}", "name" => "payload" }
    ]
};

##########################################################################################
#
##########################################################################################
sub input_struct_init {
    my $count = 0;
    foreach my $i ( sort keys %struct ) {
        $count++;
    }
    print STDERR "[avm_event_input_struct_init] " . $count . " structs defined\n";
}

1;
