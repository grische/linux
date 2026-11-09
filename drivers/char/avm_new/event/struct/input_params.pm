#! /usr/bin/perl -w
package input_params;

use strict;
use warnings;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw( %types %defines @defs @includes @static_defs $version $prefix &input_params_init);
@EXPORT_OK = qw();

our %types;
our %defines;
our @defs;
our @includes;
our @static_defs;


##########################################################################################
# Prefix
##########################################################################################
our $prefix = "avm_event_";

##########################################################################################
# Protokoll Version 2.0
# 0x10002: 'avm_event_smarthome_switch_status': Umbenennung von 2 Feldern: 'name_length' -> 'ain_length', 'name' -> 'ain'
# 0x10003: 'avm_event_telefony': Erweiterung des Telefonie-Events
# 0x10004: 'avm_wlan_event': Anpassung des WLAN-Events (wird ab dieser Version von der WLAN-Lib genutzt)
# 0x10008: 'wlan_event_id': neue event ID 'WLAN_EVENT_SCAN'
# 0x10009:  neue PushButton Events für pairing_off
# 0x1000A: Erweiterung um nexus_pairing_box
# 0x1000B: Ergänzung von enum Wert INPUT_RADAR_END für avm_wlan_event
# 0x1000C: Events für Connect Methoden (JZ-36989 JZ-37275)
# 0x1000D: Events für System Log Connect (JZ-37908 JZ-37779 JZ-37833)
# 0x1000E: Events für Lan pairing (JZ-41685)
# 0x1000F: Eventerweiterung fuer RADAR (JZ-34873)
# 0x10010: add: avm_event_id_telephony_call_finished
# 0x10012: add: struct avm_event_wlan_credentials
# 0x10013: Eventerweiterung fuer DFS/PreCAC wait period (JZ-45011)
# 0x10014: neues Event avm_event_wlan_power (JZ-46387)
# 0x10015: neues Event wlan_event_data_client_state_idle
# 0x10016: neues Event wlan_event_data_radio_recovery (JZ-50403)
# 0x10017: add: avm_event_ethernet_speed_{2_5G,5G,10G} (JZ-62972)
# 0x10018: Erweiterung avm_event_wlan_client_status_u3  (JZ-63020)
# 0x10019: Erweiterung _avm_event_pm_info_stat,_powermanagment_device - powerline (JZ-66994)
# 0x1001A: Erweiterung avm_event_id_fiber_set_arch, avm_event_id_fiber_get_arch (JZ-67871)
# 0x1001B: Erweiterung enum wlan_info_special: STATUS_WPS_STOP (JZ-75951)
##########################################################################################
our $version = 0x0001001B;

##########################################################################################
# includes
##########################################################################################
@includes = (
#        { "file" => "endian.h", "local" => "no" },
#        { "file" => "stdint.h", "local" => "no" },
#        { "file" => "sys/time.h", "local" => "no" },
        { "file" => "linux/types.h", "local" => "no" },
        { "file" => "linux/if.h", "local" => "yes", "kernel" => "yes" },
        { "ifndef" => "AVM_LED_INTERNAL", "file" => "linux/avm_led_event.h", "local" => "no", "kernel" => "yes" },
        { "ifndef" => "AVM_LED_INTERNAL", "file" => "linux/avm_led_event.h", "local" => "yes", "kernel" => "no"},
        { "file" => "stdint.h", "local" => "no", "kernel" => "no" },
        { "file" => "time.h", "local" => "no", "kernel" => "no" },
);


##########################################################################################
# defines
##########################################################################################
@defs = (
    { "name" => "MAX_EVENT_CLIENT_NAME_LEN", "value" => "32" },
    { "name" => "MAX_EVENT_SOURCE_NAME_LEN", "value" => "32" },
    { "name" => "MAX_EVENT_NODE_NAME_LEN", "value" => "32" },
    { "name" => "MAX_AVM_EVENT_SOURCES", "value" => "32" },
    { "name" => "MAX_AVM_EVENT_NODES", "value" => "32" },
    { "name" => "AVM_LED_STATUS_MAX_PARAMLEN", "value" => "245" },
    { "name" => "AVM_DIST_EVENT_VERSION", "value" => sprintf("0x%x", $version) },
    { "name" => "AVM_DIST_EVENT_VERSION__GET_MAJOR(v)", "value" => "((v >> 16) & 0xffff)" },
    { "name" => "AVM_DIST_EVENT_VERSION__GET_MINOR(v)", "value" => "(v & 0xffff)" },
    { "name" => "AVM_EVENT_ETH_MAXPORTS", "value" => "7U" },
    { "name" => "AVM_EVENT_HAVE_ETH_MAXSPEED", "value" => "1" },
    { "name" => "AVM_EVENT_RPC_MAX_MESSAGE_SIZE", "value" => "240" },

    { "name" => "avm_event_push_button_gpio_low" , "value" => "0" },
    { "name" => "avm_event_push_button_gpio_high", "value" => "1" },
    { "name" => "avm_event_push_button_key_1"    , "value" => "2 ", "comment" =>  " Taster1: kurz gedrückt 0,25 - 2 Sekunden " },
    { "name" => "avm_event_push_button_key_2"    , "value" => "3 ", "comment" =>  " Taster1: mittel lang gedrückt 2 - 8 Sekunden" },
    { "name" => "avm_event_push_button_key_3"    , "value" => "4 ", "comment" =>  " Taster1: lang gedrückt mehr als 8 Sekunden" },
    { "name" => "avm_event_push_button_key_4"    , "value" => "5 ", "comment" =>  " Taster2: kurz gedrückt 0,25 - 2 Sekunden" },
    { "name" => "avm_event_push_button_key_5"    , "value" => "6 ", "comment" =>  " Taster2: mittel lang gedrückt 2 - 8 Sekunden" },
    { "name" => "avm_event_push_button_key_6"    , "value" => "7", "comment" =>   " Taster2: lang gedrückt mehr als 8 Sekunden" },
    { "name" => "avm_event_push_button_key_7"    , "value" => "8", "comment" =>   " Schalter: aus" },
    { "name" => "avm_event_push_button_key_8"    , "value" => "9", "comment" =>   " Schalter: an" },
    { "name" => "avm_event_push_button_key_9"    , "value" => "10", "comment" =>  " Schalter: Dummy" },
    { "name" => "avm_event_push_button_key_10"   , "value" => "11", "comment" => " Taster4: kurz gedrückt 0,25 - 2 Sekunden" },
    { "name" => "avm_event_push_button_key_11"   , "value" => "12", "comment" => " Taster4: mittel lang gedrückt 2 - 8 Sekunden" },
    { "name" => "avm_event_push_button_key_12"   , "value" => "13", "comment" => " Taster4: lang gedrückt mehr als 8 Sekunden" },
    { "name" => "avm_event_push_button_key_13"   , "value" => "14", "comment" => " TOUCH1: gedrückt" },
    { "name" => "avm_event_push_button_key_14"   , "value" => "15", "comment" => " TOUCH1: reserved" },
    { "name" => "avm_event_push_button_key_15"   , "value" => "16", "comment" => " TOUCH2: gedrückt" },
    { "name" => "avm_event_push_button_key_16"   , "value" => "17", "comment" => " TOUCH2: reserved" },
    { "name" => "avm_event_push_button_key_17"   , "value" => "18", "comment" => " TOUCH3: gedrückt" },
    { "name" => "avm_event_push_button_key_18"   , "value" => "19", "comment" => " TOUCH3: reserved" },
    { "name" => "avm_event_push_button_key_19"   , "value" => "20", "comment" => " TOUCH4: gedrückt" },
    { "name" => "avm_event_push_button_key_20"   , "value" => "21", "comment" => " TOUCH4: reserved" },
    { "name" => "avm_event_push_button_key_21"   , "value" => "22", "comment" => " Taster5: kurz gedrückt 0,25 - 2 Sekunden " },
    { "name" => "avm_event_push_button_key_22"   , "value" => "23", "comment" => " Taster5: mittel lang gedrückt 2 - 8 Sekunden " },
    { "name" => "avm_event_push_button_key_23"   , "value" => "24", "comment" => " Taster5: lang gedrückt mehr als 8 Sekunden " },
	{ "name" => "avm_event_mask_fieldentry"		 , "value" => "unsigned long long" },
	{ "name" => "avm_event_mask_fieldentries"	 , "value" => "((avm_event_last - 1) / (sizeof(avm_event_mask_fieldentry) * 8) + 1)" },

    { "name" => "AVM_EVENT_TFFS_NODE_NONE", "value" => "0" },
    { "name" => "AVM_EVENT_TFFS_NODE_ATOM", "value" => "1" },
    { "name" => "AVM_EVENT_TFFS_NODE_ARM",  "value" => "2" },
    { "name" => "AVM_EVENT_TFFS_NODE_ANY",  "value" => "255" },

    { "name" => "AVM_EVENT_CHKPNT_ID_ARM",  "value" => "1" },
    { "name" => "AVM_EVENT_CHKPNT_ID_ATOM", "value" => "2" },

);

##########################################################################################
# static defs
##########################################################################################
@static_defs = (
    { "internal" =>"yes", "local" => "yes", "kernel" => "yes", "defs" => "static inline const char *get_enum__led_event_name(enum _led_event led) { return \"\"; }" },
);


##########################################################################################
# Basis typen
##########################################################################################
$types{"void"} = {
    "name" => "void",
    "type" => "native",
    "size" => undef,
    "ref"  => undef
};

$types{"unsigned char"} = {
    "name" => "unsigned char",
    "type" => "native",
    "size" => "sizeof(unsigned char)",
    "ref"  => undef
};

$types{"uint8_t"} = {
    "name" => "uint8_t",
    "type" => "native",
    "size" => "sizeof(uint8_t)",
    "ref"  => undef
};

$types{"signed char"} = {
    "name" => "signed char",
    "type" => "native",
    "size" => "sizeof(signed char)",
    "ref"  => undef
};

$types{"char"} = {
    "name" => "char",
    "type" => "native",
    "size" => "sizeof(char)",
    "ref"  => undef
};

$types{"uint16_t"} = {
    "name" => "unsigned short",
    "type" => "native",
    "size" => "sizeof(unsigned short)",
    "ref"  => undef
};

$types{"unsigned short"} = {
    "name" => "unsigned short",
    "type" => "native",
    "size" => "sizeof(unsigned short)",
    "ref"  => undef
};

$types{"short"} = {
    "name" => "short",
    "type" => "native",
    "size" => "sizeof(short)",
    "ref"  => undef
};

$types{"unsigned int"} = {
    "name" => "unsigned int",
    "type" => "native",
    "size" => "sizeof(unsigned int)",
    "ref"  => undef
};

$types{"uint32_t"} = {
    "name" => "uint32_t",
    "type" => "native",
    "size" => "sizeof(uint32_t)",
    "ref"  => undef
};

$types{"int"} = {
    "name" => "int",
    "type" => "native",
    "size" => "sizeof(int)",
    "ref"  => undef
};

$types{"int32_t"} = {
    "name" => "int32_t",
    "type" => "native",
    "size" => "sizeof(int32_t)",
    "ref"  => undef
};

$types{"unsigned long"} = {
    "name" => "unsigned long",
    "type" => "native",
    "size" => "sizeof(unsigned long)",
    "ref"  => undef
};

$types{"signed long"} = {
    "name" => "signed long",
    "type" => "native",
    "size" => "sizeof(signed long)",
    "ref"  => undef
};

$types{"long"} = {
    "name" => "long",
    "type" => "native",
    "size" => "sizeof(long)",
    "ref"  => undef
};

$types{"double"} = {
    "name" => "double",
    "type" => "native",
    "size" => "sizeof(double)",
    "ref"  => undef
};

$types{"uint64_t"} = {
    "name" => "uint64_t",
    "type" => "native",
    "size" => "sizeof(uint64_t)",
    "ref"  => undef
};

$types{"int64_t"} = {
    "name" => "int64_t",
    "type" => "native",
    "size" => "sizeof(int64_t)",
    "ref"  => undef
};

$types{"unsigned long long"} = {
    "name" => "unsigned long long",
    "type" => "native",
    "size" => "sizeof(unsigned long long)",
    "ref"  => undef
};

$types{"long long"} = {
    "name" => "long long",
    "type" => "native",
    "size" => "sizeof(long long)",
    "ref"  => undef
};

$types{"enum _led_event"} = {
    "name" => "enum _led_event",
    "type" => "native",
    "size" => "sizeof(enum _led_event)",
    "ref"  => undef
};

##########################################################################################
##########################################################################################
sub input_params_init {
    my $count = 0;
    foreach my $i ( sort keys %types) {
        $count++;
    }
    print STDERR "[avm_dist_event_input_params_init] " . $count . " basic types defined\n";
    print STDERR "[avm_dist_event_input_params_init] " . ( $#defs + 1) . " defined defined\n";
    print STDERR "[avm_dist_event_input_params_init] " . ( $#includes + 1) . " includes defined\n";
}



1;
