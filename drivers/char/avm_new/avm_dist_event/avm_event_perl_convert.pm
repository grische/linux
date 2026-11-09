#! /usr/bin/perl -w
package avm_event_perl_convert;
use strict;
use warnings;
use Exporter ();

our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

$VERSION     = 1.00;
@ISA = qw(Exporter);
@EXPORT = qw(avm_event_read_from_binary_struct__avm_event_ambient_brightness
		&avm_event_read_from_binary_struct__avm_event_checkpoint
		&avm_event_read_from_binary_struct__avm_event_cmd
		&avm_event_read_from_binary_struct__avm_event_cmd_param_register
		&avm_event_read_from_binary_struct__avm_event_cmd_param_release
		&avm_event_read_from_binary_struct__avm_event_cmd_param_source_trigger
		&avm_event_read_from_binary_struct__avm_event_cmd_param_trigger
		&avm_event_read_from_binary_struct__avm_event_cpu_idle
		&avm_event_read_from_binary_struct__avm_event_cpu_run
		&avm_event_read_from_binary_struct__avm_event_fax_file
		&avm_event_read_from_binary_struct__avm_event_fax_status
		&avm_event_read_from_binary_struct__avm_event_firmware_update_available
		&avm_event_read_from_binary_struct__avm_event_header
		&avm_event_read_from_binary_struct__avm_event_id_mask
		&avm_event_read_from_binary_struct__avm_event_internet_new_ip
		&avm_event_read_from_binary_struct__avm_event_led_info
		&avm_event_read_from_binary_struct__avm_event_led_status
		&avm_event_read_from_binary_struct__avm_event_log
		&avm_event_read_from_binary_struct__avm_event_mass_storage_mount
		&avm_event_read_from_binary_struct__avm_event_mass_storage_unmount
		&avm_event_read_from_binary_struct__avm_event_piglet
		&avm_event_read_from_binary_struct__avm_event_pm_info_stat
		&avm_event_read_from_binary_struct__avm_event_powerline_status
		&avm_event_read_from_binary_struct__avm_event_powermanagment_remote
		&avm_event_read_from_binary_struct__avm_event_powermanagment_remote_ressourceinfo
		&avm_event_read_from_binary_struct__avm_event_powermanagment_status
		&avm_event_read_from_binary_struct__avm_event_push_button
		&avm_event_read_from_binary_struct__avm_event_remotepcmlink
		&avm_event_read_from_binary_struct__avm_event_remotewatchdog
		&avm_event_read_from_binary_struct__avm_event_rpc
		&avm_event_read_from_binary_struct__avm_event_smarthome
		&avm_event_read_from_binary_struct__avm_event_smarthome_switch_status
		&avm_event_read_from_binary_struct__avm_event_telefonprofile
		&avm_event_read_from_binary_struct__avm_event_telephony_missed_call
		&avm_event_read_from_binary_struct__avm_event_telephony_missed_call_params
		&avm_event_read_from_binary_struct__avm_event_temperature
		&avm_event_read_from_binary_struct__avm_event_user_mode_source_notify
		&avm_event_read_from_binary_struct__avm_event_wlan
		&avm_event_read_from_binary_struct__cpmac_event_struct
		&avm_event_read_from_binary_struct_avm_event_ambient_brightness
		&avm_event_read_from_binary_struct_avm_event_boykott
		&avm_event_read_from_binary_struct_avm_event_checkpoint
		&avm_event_read_from_binary_struct_avm_event_cpu_idle
		&avm_event_read_from_binary_struct_avm_event_cpu_run
		&avm_event_read_from_binary_struct_avm_event_data
		&avm_event_read_from_binary_struct_avm_event_fax_file
		&avm_event_read_from_binary_struct_avm_event_fax_status
		&avm_event_read_from_binary_struct_avm_event_firmware_update_available
		&avm_event_read_from_binary_struct_avm_event_internet_new_ip
		&avm_event_read_from_binary_struct_avm_event_led_info
		&avm_event_read_from_binary_struct_avm_event_led_status
		&avm_event_read_from_binary_struct_avm_event_log
		&avm_event_read_from_binary_struct_avm_event_mass_storage_mount
		&avm_event_read_from_binary_struct_avm_event_mass_storage_unmount
		&avm_event_read_from_binary_struct_avm_event_message
		&avm_event_read_from_binary_struct_avm_event_piglet
		&avm_event_read_from_binary_struct_avm_event_ping
		&avm_event_read_from_binary_struct_avm_event_pm_info_stat
		&avm_event_read_from_binary_struct_avm_event_powerline_status
		&avm_event_read_from_binary_struct_avm_event_powermanagment_remote
		&avm_event_read_from_binary_struct_avm_event_powermanagment_remote_ressourceinfo
		&avm_event_read_from_binary_struct_avm_event_powermanagment_status
		&avm_event_read_from_binary_struct_avm_event_push_button
		&avm_event_read_from_binary_struct_avm_event_remote_source_trigger_request
		&avm_event_read_from_binary_struct_avm_event_remotepcmlink
		&avm_event_read_from_binary_struct_avm_event_remotewatchdog
		&avm_event_read_from_binary_struct_avm_event_rpc
		&avm_event_read_from_binary_struct_avm_event_smarthome
		&avm_event_read_from_binary_struct_avm_event_smarthome_switch_status
		&avm_event_read_from_binary_struct_avm_event_source_notifier
		&avm_event_read_from_binary_struct_avm_event_source_register
		&avm_event_read_from_binary_struct_avm_event_source_unregister
		&avm_event_read_from_binary_struct_avm_event_telefon_up
		&avm_event_read_from_binary_struct_avm_event_telefonprofile
		&avm_event_read_from_binary_struct_avm_event_telephony_missed_call
		&avm_event_read_from_binary_struct_avm_event_telephony_string
		&avm_event_read_from_binary_struct_avm_event_temperature
		&avm_event_read_from_binary_struct_avm_event_tffs
		&avm_event_read_from_binary_struct_avm_event_tffs_cleanup
		&avm_event_read_from_binary_struct_avm_event_tffs_close
		&avm_event_read_from_binary_struct_avm_event_tffs_deinit
		&avm_event_read_from_binary_struct_avm_event_tffs_info
		&avm_event_read_from_binary_struct_avm_event_tffs_init
		&avm_event_read_from_binary_struct_avm_event_tffs_notify
		&avm_event_read_from_binary_struct_avm_event_tffs_open
		&avm_event_read_from_binary_struct_avm_event_tffs_paniclog
		&avm_event_read_from_binary_struct_avm_event_tffs_read
		&avm_event_read_from_binary_struct_avm_event_tffs_reindex
		&avm_event_read_from_binary_struct_avm_event_tffs_write
		&avm_event_read_from_binary_struct_avm_event_unserialised
		&avm_event_read_from_binary_struct_avm_event_user_mode_source_notify
		&avm_event_read_from_binary_struct_avm_event_wlan
		&avm_event_read_from_binary_struct_avm_event_wlan_credentials
		&avm_event_read_from_binary_struct_avm_event_wlan_power
		&avm_event_read_from_binary_struct_cpmac_event_struct
		&avm_event_read_from_binary_struct_cpmac_port
		&avm_event_read_from_binary_struct_wlan_event_data_client_common
		&avm_event_read_from_binary_struct_wlan_event_data_client_connect_info
		&avm_event_read_from_binary_struct_wlan_event_data_client_state_change
		&avm_event_read_from_binary_struct_wlan_event_data_client_state_idle
		&avm_event_read_from_binary_struct_wlan_event_data_radio_recovery
		&avm_event_read_from_binary_struct_wlan_event_data_scan_common
		&avm_event_read_from_binary_struct_wlan_event_data_scan_event_info
		&avm_event_read_from_binary_struct_wlan_event_def
	);

@EXPORT_OK = qw();


##########################################################################################
# convert function for 'struct _avm_event_ambient_brightness'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_ambient_brightness {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $value;
	my $maxvalue;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'value' at offset '0'
	$value = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'maxvalue' at offset '0'
	$maxvalue = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"value" => $value,
		"maxvalue" => $maxvalue,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_checkpoint'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_checkpoint {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $node_id;
	my $checkpoints;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'node_id' at offset '0'
	$node_id = unpack(">I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'checkpoints' at offset '12'
	$checkpoints = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"node_id" => $node_id,
		"checkpoints" => $checkpoints,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cmd'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cmd {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $cmd;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'cmd' at offset '0'
	$cmd = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'param' at offset '4'
	( $param, $struct_offset ) = avm_event_read_from_binary_union__avm_event_cmd_param(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"cmd" => $cmd,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cmd_param_register'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cmd_param_register {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $mask;
	my $Name;

	####### convert binary message to hash components #######

	# c-struct component 'mask' at offset '0'
	( $mask, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_id_mask(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'Name' at offset '0'
	$Name = unpack("CMAX_EVENT_CLIENT_NAME_LEN + 1", $in_ref);
	$offset += MAX_EVENT_CLIENT_NAME_LEN + 1;


	####### return hash and used length #######
	$return ( {
		"mask" => $mask,
		"Name" => $Name,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cmd_param_release'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cmd_param_release {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $Name;

	####### convert binary message to hash components #######

	# c-struct component 'Name' at offset '0'
	$Name = unpack("CMAX_EVENT_CLIENT_NAME_LEN + 1", $in_ref);
	$offset += MAX_EVENT_CLIENT_NAME_LEN + 1;


	####### return hash and used length #######
	$return ( {
		"Name" => $Name,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cmd_param_source_trigger'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cmd_param_source_trigger {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;
	my $data_length;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'data_length' at offset '4'
	$data_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
		"data_length" => $data_length,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cmd_param_trigger'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cmd_param_trigger {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cpu_idle'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cpu_idle {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $cpu_idle;
	my $dsl_dsp_idle;
	my $voice_dsp_idle;
	my $mem_strictlyused;
	my $mem_cacheused;
	my $mem_physfree;
	my $cputype;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'cpu_idle' at offset '0'
	$cpu_idle = unpack("C", $in_ref);
	$offset += 1;

	# c-struct component 'dsl_dsp_idle' at offset '1'
	$dsl_dsp_idle = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'voice_dsp_idle' at offset '2'
	$voice_dsp_idle = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'mem_strictlyused' at offset '3'
	$mem_strictlyused = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'mem_cacheused' at offset '4'
	$mem_cacheused = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'mem_physfree' at offset '5'
	$mem_physfree = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'cputype' at offset '6'
	$cputype = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"cpu_idle" => $cpu_idle,
		"dsl_dsp_idle" => $dsl_dsp_idle,
		"voice_dsp_idle" => $voice_dsp_idle,
		"mem_strictlyused" => $mem_strictlyused,
		"mem_cacheused" => $mem_cacheused,
		"mem_physfree" => $mem_physfree,
		"cputype" => $cputype,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_cpu_run'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_cpu_run {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $cputype;
	my $cpu_run;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'cputype' at offset '0'
	$cputype = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'cpu_run' at offset '4'
	$cpu_run = unpack("a" . $offset . "/C4", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"cputype" => $cputype,
		"cpu_run" => $cpu_run,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_fax_file'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_fax_file {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $action;
	my $date;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'action' at offset '0'
	$action = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'date' at offset '4'
	( $date, $struct_offset ) = avm_event_read_from_binary_time_t(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"action" => $action,
		"date" => $date,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_fax_status'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_fax_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $fax_receive_mode;
	my $fax_storage_dest;
	my $dirname_length;
	my @dirname = ();

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'fax_receive_mode' at offset '0'
	$fax_receive_mode = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'fax_storage_dest' at offset '4'
	$fax_storage_dest = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'dirname_length' at offset '8'
	$dirname_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $dirname_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$dirname[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"fax_receive_mode" => $fax_receive_mode,
		"fax_storage_dest" => $fax_storage_dest,
		"dirname_length" => $dirname_length,
		"dirname" => [ @dirname ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_firmware_update_available'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_firmware_update_available {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $type;
	my $version_length;
	my @version = ();

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'version_length' at offset '4'
	$version_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $version_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$version[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"type" => $type,
		"version_length" => $version_length,
		"version" => [ @version ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_header'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_header {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_id_mask'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_id_mask {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $mask;

	####### convert binary message to hash components #######

	# c-struct component 'mask' at offset '0'
	( $mask, $struct_offset ) = avm_event_read_from_binary_avm_event_mask_fieldentry(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"mask" => $mask,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_internet_new_ip'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_internet_new_ip {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $sel;
	my $params;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'sel' at offset '0'
	$sel = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'params' at offset '4'
	( $params, $struct_offset ) = avm_event_read_from_binary_union_avm_event_internet_new_ip_param(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"sel" => $sel,
		"params" => $params,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_led_info'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_led_info {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $mode;
	my $param1;
	my $param2;
	my $gpio_driver_type;
	my $gpio;
	my $pos;
	my $name;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'mode' at offset '0'
	$mode = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'param1' at offset '0'
	$param1 = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'param2' at offset '0'
	$param2 = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'gpio_driver_type' at offset '0'
	$gpio_driver_type = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'gpio' at offset '0'
	$gpio = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'pos' at offset '0'
	$pos = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'name' at offset '0'
	$name = unpack("C32", $in_ref);
	$offset += 32;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"mode" => $mode,
		"param1" => $param1,
		"param2" => $param2,
		"gpio_driver_type" => $gpio_driver_type,
		"gpio" => $gpio,
		"pos" => $pos,
		"name" => $name,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_led_status'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_led_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $led;
	my $state;
	my $param_len;
	my $params;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'led' at offset '0'
	$led = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'state' at offset '4'
	$state = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'param_len' at offset '4'
	$param_len = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'params' at offset '4'
	$params = unpack("a" . $offset . "/C245", $in_ref);
	$offset += 245;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"led" => $led,
		"state" => $state,
		"param_len" => $param_len,
		"params" => $params,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_log'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_log {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $logtype;
	my $loglen;
	my $logpointer;
	my $checksum;
	my $rebootflag;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'logtype' at offset '0'
	$logtype = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'loglen' at offset '4'
	$loglen = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'logpointer' at offset '4'
	$logpointer = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'checksum' at offset '4'
	$checksum = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rebootflag' at offset '4'
	$rebootflag = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"logtype" => $logtype,
		"loglen" => $loglen,
		"logpointer" => $logpointer,
		"checksum" => $checksum,
		"rebootflag" => $rebootflag,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_mass_storage_mount'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_mass_storage_mount {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $size;
	my $free;
	my $name_length;
	my @name = ();

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'size' at offset '0'
	$size = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'free' at offset '0'
	$free = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'name_length' at offset '0'
	$name_length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $name_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$name[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"size" => $size,
		"free" => $free,
		"name_length" => $name_length,
		"name" => [ @name ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_mass_storage_unmount'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_mass_storage_unmount {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $name_length;
	my @name = ();

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'name_length' at offset '0'
	$name_length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $name_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$name[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"name_length" => $name_length,
		"name" => [ @name ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_piglet'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_piglet {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $type;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"type" => $type,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_pm_info_stat'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_pm_info_stat {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $reserved1;
	my $rate_sumact;
	my $rate_sumcum;
	my $rate_systemact;
	my $rate_systemcum;
	my $system_status;
	my $rate_dspact;
	my $rate_dspcum;
	my $rate_wlanact;
	my $rate_wlancum;
	my $wlan_devices;
	my $wlan_status;
	my $rate_ethact;
	my $rate_ethcum;
	my $eth_status;
	my $rate_abact;
	my $rate_abcum;
	my $isdn_status;
	my $rate_dectact;
	my $rate_dectcum;
	my $rate_battchargeact;
	my $rate_battchargecum;
	my $dect_status;
	my $rate_usbhostact;
	my $rate_usbhostcum;
	my $usb_status;
	my $act_temperature;
	my $min_temperature;
	my $max_temperature;
	my $avg_temperature;
	my $rate_lteact;
	my $rate_ltecum;
	my $rate_dvbcact;
	my $rate_dvbccum;
	my $rate_plcact;
	my $rate_plccum;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'reserved1' at offset '0'
	$reserved1 = unpack("C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_sumact' at offset '1'
	$rate_sumact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_sumcum' at offset '2'
	$rate_sumcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_systemact' at offset '3'
	$rate_systemact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_systemcum' at offset '4'
	$rate_systemcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'system_status' at offset '5'
	$system_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dspact' at offset '6'
	$rate_dspact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dspcum' at offset '7'
	$rate_dspcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_wlanact' at offset '8'
	$rate_wlanact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_wlancum' at offset '9'
	$rate_wlancum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'wlan_devices' at offset '10'
	$wlan_devices = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'wlan_status' at offset '11'
	$wlan_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_ethact' at offset '12'
	$rate_ethact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_ethcum' at offset '13'
	$rate_ethcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'eth_status' at offset '14'
	$eth_status = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rate_abact' at offset '14'
	$rate_abact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_abcum' at offset '15'
	$rate_abcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'isdn_status' at offset '16'
	$isdn_status = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rate_dectact' at offset '16'
	$rate_dectact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dectcum' at offset '17'
	$rate_dectcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_battchargeact' at offset '18'
	$rate_battchargeact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_battchargecum' at offset '19'
	$rate_battchargecum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'dect_status' at offset '20'
	$dect_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_usbhostact' at offset '21'
	$rate_usbhostact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_usbhostcum' at offset '22'
	$rate_usbhostcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'usb_status' at offset '23'
	$usb_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'act_temperature' at offset '24'
	$act_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'min_temperature' at offset '24'
	$min_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'max_temperature' at offset '24'
	$max_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'avg_temperature' at offset '24'
	$avg_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rate_lteact' at offset '24'
	$rate_lteact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_ltecum' at offset '25'
	$rate_ltecum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dvbcact' at offset '26'
	$rate_dvbcact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dvbccum' at offset '27'
	$rate_dvbccum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_plcact' at offset '28'
	$rate_plcact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_plccum' at offset '29'
	$rate_plccum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"reserved1" => $reserved1,
		"rate_sumact" => $rate_sumact,
		"rate_sumcum" => $rate_sumcum,
		"rate_systemact" => $rate_systemact,
		"rate_systemcum" => $rate_systemcum,
		"system_status" => $system_status,
		"rate_dspact" => $rate_dspact,
		"rate_dspcum" => $rate_dspcum,
		"rate_wlanact" => $rate_wlanact,
		"rate_wlancum" => $rate_wlancum,
		"wlan_devices" => $wlan_devices,
		"wlan_status" => $wlan_status,
		"rate_ethact" => $rate_ethact,
		"rate_ethcum" => $rate_ethcum,
		"eth_status" => $eth_status,
		"rate_abact" => $rate_abact,
		"rate_abcum" => $rate_abcum,
		"isdn_status" => $isdn_status,
		"rate_dectact" => $rate_dectact,
		"rate_dectcum" => $rate_dectcum,
		"rate_battchargeact" => $rate_battchargeact,
		"rate_battchargecum" => $rate_battchargecum,
		"dect_status" => $dect_status,
		"rate_usbhostact" => $rate_usbhostact,
		"rate_usbhostcum" => $rate_usbhostcum,
		"usb_status" => $usb_status,
		"act_temperature" => $act_temperature,
		"min_temperature" => $min_temperature,
		"max_temperature" => $max_temperature,
		"avg_temperature" => $avg_temperature,
		"rate_lteact" => $rate_lteact,
		"rate_ltecum" => $rate_ltecum,
		"rate_dvbcact" => $rate_dvbcact,
		"rate_dvbccum" => $rate_dvbccum,
		"rate_plcact" => $rate_plcact,
		"rate_plccum" => $rate_plccum,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_powerline_status'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_powerline_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $status;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'status' at offset '0'
	$status = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"status" => $status,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_powermanagment_remote'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_powermanagment_remote {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $remote_action;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'remote_action' at offset '0'
	$remote_action = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'param' at offset '4'
	( $param, $struct_offset ) = avm_event_read_from_binary_union_avm_event_powermanagment_remote_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"remote_action" => $remote_action,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_powermanagment_remote_ressourceinfo'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_powermanagment_remote_ressourceinfo {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $device;
	my $power_rate;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'device' at offset '0'
	$device = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'power_rate' at offset '4'
	$power_rate = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"device" => $device,
		"power_rate" => $power_rate,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_powermanagment_status'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_powermanagment_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $substatus;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'substatus' at offset '0'
	$substatus = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'param' at offset '4'
	( $param, $struct_offset ) = avm_event_read_from_binary_union___powermanagment_status_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"substatus" => $substatus,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_push_button'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_push_button {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;
	my $key;
	my $pressed;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'key' at offset '4'
	$key = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'pressed' at offset '8'
	$pressed = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
		"key" => $key,
		"pressed" => $pressed,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_remotepcmlink'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_remotepcmlink {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $type;
	my $sharedlen;
	my $sharedpointer;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'sharedlen' at offset '4'
	$sharedlen = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'sharedpointer' at offset '4'
	$sharedpointer = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"type" => $type,
		"sharedlen" => $sharedlen,
		"sharedpointer" => $sharedpointer,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_remotewatchdog'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_remotewatchdog {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $cmd;
	my $name;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'cmd' at offset '0'
	$cmd = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'name' at offset '4'
	$name = unpack("a" . $offset . "/C16", $in_ref);
	$offset += 16;

	# c-struct component 'param' at offset '20'
	$param = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"cmd" => $cmd,
		"name" => $name,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_rpc'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_rpc {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $type;
	my $id;
	my $length;
	my @message = ();

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'id' at offset '4'
	$id = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'length' at offset '4'
	$length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$message[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"type" => $type,
		"id" => $id,
		"length" => $length,
		"message" => [ @message ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_smarthome'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_smarthome {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $length;
	my @ident = ();
	my $type;
	my $payload;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'length' at offset '0'
	$length = unpack("", $in_ref);
	$offset += 0;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$ident[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}

	# c-struct component 'type' at offset '20'
	$type = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'payload' at offset '20'
	( $payload, $struct_offset ) = avm_event_read_from_binary_struct_{}(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"length" => $length,
		"ident" => [ @ident ],
		"type" => $type,
		"payload" => $payload,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_smarthome_switch_status'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_smarthome_switch_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $type;
	my $value;
	my $ain_length;
	my @ain = ();

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'value' at offset '4'
	$value = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'ain_length' at offset '4'
	$ain_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $ain_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$ain[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"type" => $type,
		"value" => $value,
		"ain_length" => $ain_length,
		"ain" => [ @ain ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_telefonprofile'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_telefonprofile {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $on;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'on' at offset '0'
	$on = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"on" => $on,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_telephony_missed_call'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_telephony_missed_call {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $length;
	my $p;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'length' at offset '0'
	$length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $length;

	# c-struct component 'p' at offset '0'
	( $p, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_telephony_missed_call_params(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"length" => $length,
		"p" => $p,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_telephony_missed_call_params'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_telephony_missed_call_params {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;
	my $params;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'params' at offset '4'
	( $params, $struct_offset ) = avm_event_read_from_binary_union_avm_event_telephony_call_params(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
		"params" => $params,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_temperature'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_temperature {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $temperature;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'temperature' at offset '0'
	$temperature = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"temperature" => $temperature,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_user_mode_source_notify'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_user_mode_source_notify {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $id;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"id" => $id,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _avm_event_wlan'
##########################################################################################
sub avm_event_read_from_binary_struct__avm_event_wlan {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $header;
	my $mac;
	my $u1;
	my $event;
	my $info;
	my $status;
	my $u2;
	my $if_name;
	my $ev_initiator;
	my $ev_reason;
	my $avm_capabilities;
	my $u3;
	my $credentials;

	####### convert binary message to hash components #######

	# c-struct component 'header' at offset '0'
	( $header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'mac' at offset '0'
	$mac = unpack("C6", $in_ref);
	$offset += 6;

	# c-struct component 'u1' at offset '6'
	$u1 = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'event' at offset '6'
	$event = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'info' at offset '6'
	$info = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'status' at offset '6'
	$status = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'u2' at offset '6'
	$u2 = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'if_name' at offset '6'
	$if_name = unpack("a" . $offset . "/CIFNAMSIZ", $in_ref);
	$offset += IFNAMSIZ;

	# c-struct component 'ev_initiator' at offset '7'
	$ev_initiator = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'ev_reason' at offset '7'
	$ev_reason = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'avm_capabilities' at offset '7'
	$avm_capabilities = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'u3' at offset '7'
	$u3 = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'credentials' at offset '7'
	( $credentials, $struct_offset ) = avm_event_read_from_binary_struct_avm_event_wlan_credentials(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"header" => $header,
		"mac" => $mac,
		"u1" => $u1,
		"event" => $event,
		"info" => $info,
		"status" => $status,
		"u2" => $u2,
		"if_name" => $if_name,
		"ev_initiator" => $ev_initiator,
		"ev_reason" => $ev_reason,
		"avm_capabilities" => $avm_capabilities,
		"u3" => $u3,
		"credentials" => $credentials,
	}, $offset );
}


##########################################################################################
# convert function for 'struct _cpmac_event_struct'
##########################################################################################
sub avm_event_read_from_binary_struct__cpmac_event_struct {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $ports;
	my $port;

	####### convert binary message to hash components #######

	# c-struct component 'ports' at offset '0'
	$ports = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'port' at offset '0'
	( $port, $struct_offset ) = avm_event_read_from_binary_struct_cpmac_port(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"ports" => $ports,
		"port" => $port,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_ambient_brightness'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_ambient_brightness {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $value;
	my $maxvalue;

	####### convert binary message to hash components #######

	# c-struct component 'value' at offset '0'
	$value = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'maxvalue' at offset '0'
	$maxvalue = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"value" => $value,
		"maxvalue" => $maxvalue,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_boykott'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_boykott {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $dummy;

	####### convert binary message to hash components #######

	# c-struct component 'dummy' at offset '0'
	( $dummy, $struct_offset ) = avm_event_read_from_binary_struct_{}(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"dummy" => $dummy,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_checkpoint'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_checkpoint {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $node_id;
	my $checkpoints;

	####### convert binary message to hash components #######

	# c-struct component 'node_id' at offset '0'
	$node_id = unpack(">I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'checkpoints' at offset '12'
	$checkpoints = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;


	####### return hash and used length #######
	$return ( {
		"node_id" => $node_id,
		"checkpoints" => $checkpoints,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_cpu_idle'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_cpu_idle {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $cpu_idle;
	my $dsl_dsp_idle;
	my $voice_dsp_idle;
	my $mem_strictlyused;
	my $mem_cacheused;
	my $mem_physfree;
	my $cputype;

	####### convert binary message to hash components #######

	# c-struct component 'cpu_idle' at offset '0'
	$cpu_idle = unpack("C", $in_ref);
	$offset += 1;

	# c-struct component 'dsl_dsp_idle' at offset '1'
	$dsl_dsp_idle = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'voice_dsp_idle' at offset '2'
	$voice_dsp_idle = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'mem_strictlyused' at offset '3'
	$mem_strictlyused = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'mem_cacheused' at offset '4'
	$mem_cacheused = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'mem_physfree' at offset '5'
	$mem_physfree = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'cputype' at offset '6'
	$cputype = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"cpu_idle" => $cpu_idle,
		"dsl_dsp_idle" => $dsl_dsp_idle,
		"voice_dsp_idle" => $voice_dsp_idle,
		"mem_strictlyused" => $mem_strictlyused,
		"mem_cacheused" => $mem_cacheused,
		"mem_physfree" => $mem_physfree,
		"cputype" => $cputype,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_cpu_run'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_cpu_run {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $cputype;
	my $cpu_run;

	####### convert binary message to hash components #######

	# c-struct component 'cputype' at offset '0'
	$cputype = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'cpu_run' at offset '4'
	$cpu_run = unpack("a" . $offset . "/C4", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"cputype" => $cputype,
		"cpu_run" => $cpu_run,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_data'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_data {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;
	my $data;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'data' at offset '4'
	( $data, $struct_offset ) = avm_event_read_from_binary_union_avm_event_data_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
		"data" => $data,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_fax_file'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_fax_file {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $action;
	my $date;

	####### convert binary message to hash components #######

	# c-struct component 'action' at offset '0'
	$action = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'date' at offset '4'
	( $date, $struct_offset ) = avm_event_read_from_binary_time_t(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"action" => $action,
		"date" => $date,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_fax_status'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_fax_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $fax_receive_mode;
	my $fax_storage_dest;
	my $dirname_length;
	my @dirname = ();

	####### convert binary message to hash components #######

	# c-struct component 'fax_receive_mode' at offset '0'
	$fax_receive_mode = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'fax_storage_dest' at offset '4'
	$fax_storage_dest = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'dirname_length' at offset '8'
	$dirname_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $dirname_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$dirname[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"fax_receive_mode" => $fax_receive_mode,
		"fax_storage_dest" => $fax_storage_dest,
		"dirname_length" => $dirname_length,
		"dirname" => [ @dirname ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_firmware_update_available'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_firmware_update_available {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $type;
	my $version_length;
	my @version = ();

	####### convert binary message to hash components #######

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'version_length' at offset '4'
	$version_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $version_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$version[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"type" => $type,
		"version_length" => $version_length,
		"version" => [ @version ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_internet_new_ip'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_internet_new_ip {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $sel;
	my $params;

	####### convert binary message to hash components #######

	# c-struct component 'sel' at offset '0'
	$sel = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'params' at offset '4'
	( $params, $struct_offset ) = avm_event_read_from_binary_union_avm_event_internet_new_ip_param(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"sel" => $sel,
		"params" => $params,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_led_info'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_led_info {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $mode;
	my $param1;
	my $param2;
	my $gpio_driver_type;
	my $gpio;
	my $pos;
	my $name;

	####### convert binary message to hash components #######

	# c-struct component 'mode' at offset '0'
	$mode = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'param1' at offset '0'
	$param1 = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'param2' at offset '0'
	$param2 = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'gpio_driver_type' at offset '0'
	$gpio_driver_type = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'gpio' at offset '0'
	$gpio = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'pos' at offset '0'
	$pos = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'name' at offset '0'
	$name = unpack("C32", $in_ref);
	$offset += 32;


	####### return hash and used length #######
	$return ( {
		"mode" => $mode,
		"param1" => $param1,
		"param2" => $param2,
		"gpio_driver_type" => $gpio_driver_type,
		"gpio" => $gpio,
		"pos" => $pos,
		"name" => $name,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_led_status'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_led_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $led;
	my $state;
	my $param_len;
	my $params;

	####### convert binary message to hash components #######

	# c-struct component 'led' at offset '0'
	$led = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'state' at offset '4'
	$state = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'param_len' at offset '4'
	$param_len = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'params' at offset '4'
	$params = unpack("a" . $offset . "/C245", $in_ref);
	$offset += 245;


	####### return hash and used length #######
	$return ( {
		"led" => $led,
		"state" => $state,
		"param_len" => $param_len,
		"params" => $params,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_log'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_log {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $logtype;
	my $loglen;
	my $logpointer;
	my $checksum;
	my $rebootflag;

	####### convert binary message to hash components #######

	# c-struct component 'logtype' at offset '0'
	$logtype = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'loglen' at offset '4'
	$loglen = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'logpointer' at offset '4'
	$logpointer = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'checksum' at offset '4'
	$checksum = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rebootflag' at offset '4'
	$rebootflag = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"logtype" => $logtype,
		"loglen" => $loglen,
		"logpointer" => $logpointer,
		"checksum" => $checksum,
		"rebootflag" => $rebootflag,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_mass_storage_mount'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_mass_storage_mount {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $size;
	my $free;
	my $name_length;
	my @name = ();

	####### convert binary message to hash components #######

	# c-struct component 'size' at offset '0'
	$size = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'free' at offset '0'
	$free = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'name_length' at offset '0'
	$name_length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $name_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$name[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"size" => $size,
		"free" => $free,
		"name_length" => $name_length,
		"name" => [ @name ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_mass_storage_unmount'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_mass_storage_unmount {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $name_length;
	my @name = ();

	####### convert binary message to hash components #######

	# c-struct component 'name_length' at offset '0'
	$name_length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $name_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$name[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"name_length" => $name_length,
		"name" => [ @name ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_message'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_message {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $length;
	my $magic;
	my $nonce;
	my $flags;
	my $result;
	my $transmitter_handle;
	my $receiver_handle;
	my $type;
	my $message;

	####### convert binary message to hash components #######

	# c-struct component 'length' at offset '0'
	$length = unpack(">I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'magic' at offset '12'
	$magic = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'nonce' at offset '24'
	$nonce = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'flags' at offset '36'
	$flags = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'result' at offset '48'
	$result = unpack("a" . $offset . "/>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'transmitter_handle' at offset '56'
	$transmitter_handle = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'receiver_handle' at offset '68'
	$receiver_handle = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'type' at offset '80'
	$type = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'message' at offset '84'
	( $message, $struct_offset ) = avm_event_read_from_binary_union_avm_event_message_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"length" => $length,
		"magic" => $magic,
		"nonce" => $nonce,
		"flags" => $flags,
		"result" => $result,
		"transmitter_handle" => $transmitter_handle,
		"receiver_handle" => $receiver_handle,
		"type" => $type,
		"message" => $message,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_piglet'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_piglet {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $type;

	####### convert binary message to hash components #######

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"type" => $type,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_ping'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_ping {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $seq;

	####### convert binary message to hash components #######

	# c-struct component 'seq' at offset '0'
	$seq = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"seq" => $seq,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_pm_info_stat'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_pm_info_stat {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $reserved1;
	my $rate_sumact;
	my $rate_sumcum;
	my $rate_systemact;
	my $rate_systemcum;
	my $system_status;
	my $rate_dspact;
	my $rate_dspcum;
	my $rate_wlanact;
	my $rate_wlancum;
	my $wlan_devices;
	my $wlan_status;
	my $rate_ethact;
	my $rate_ethcum;
	my $eth_status;
	my $rate_abact;
	my $rate_abcum;
	my $isdn_status;
	my $rate_dectact;
	my $rate_dectcum;
	my $rate_battchargeact;
	my $rate_battchargecum;
	my $dect_status;
	my $rate_usbhostact;
	my $rate_usbhostcum;
	my $usb_status;
	my $act_temperature;
	my $min_temperature;
	my $max_temperature;
	my $avg_temperature;
	my $rate_lteact;
	my $rate_ltecum;
	my $rate_dvbcact;
	my $rate_dvbccum;
	my $rate_plcact;
	my $rate_plccum;

	####### convert binary message to hash components #######

	# c-struct component 'reserved1' at offset '0'
	$reserved1 = unpack("C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_sumact' at offset '1'
	$rate_sumact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_sumcum' at offset '2'
	$rate_sumcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_systemact' at offset '3'
	$rate_systemact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_systemcum' at offset '4'
	$rate_systemcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'system_status' at offset '5'
	$system_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dspact' at offset '6'
	$rate_dspact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dspcum' at offset '7'
	$rate_dspcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_wlanact' at offset '8'
	$rate_wlanact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_wlancum' at offset '9'
	$rate_wlancum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'wlan_devices' at offset '10'
	$wlan_devices = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'wlan_status' at offset '11'
	$wlan_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_ethact' at offset '12'
	$rate_ethact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_ethcum' at offset '13'
	$rate_ethcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'eth_status' at offset '14'
	$eth_status = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rate_abact' at offset '14'
	$rate_abact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_abcum' at offset '15'
	$rate_abcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'isdn_status' at offset '16'
	$isdn_status = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rate_dectact' at offset '16'
	$rate_dectact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dectcum' at offset '17'
	$rate_dectcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_battchargeact' at offset '18'
	$rate_battchargeact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_battchargecum' at offset '19'
	$rate_battchargecum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'dect_status' at offset '20'
	$dect_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_usbhostact' at offset '21'
	$rate_usbhostact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_usbhostcum' at offset '22'
	$rate_usbhostcum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'usb_status' at offset '23'
	$usb_status = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'act_temperature' at offset '24'
	$act_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'min_temperature' at offset '24'
	$min_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'max_temperature' at offset '24'
	$max_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'avg_temperature' at offset '24'
	$avg_temperature = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'rate_lteact' at offset '24'
	$rate_lteact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_ltecum' at offset '25'
	$rate_ltecum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dvbcact' at offset '26'
	$rate_dvbcact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_dvbccum' at offset '27'
	$rate_dvbccum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_plcact' at offset '28'
	$rate_plcact = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;

	# c-struct component 'rate_plccum' at offset '29'
	$rate_plccum = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;


	####### return hash and used length #######
	$return ( {
		"reserved1" => $reserved1,
		"rate_sumact" => $rate_sumact,
		"rate_sumcum" => $rate_sumcum,
		"rate_systemact" => $rate_systemact,
		"rate_systemcum" => $rate_systemcum,
		"system_status" => $system_status,
		"rate_dspact" => $rate_dspact,
		"rate_dspcum" => $rate_dspcum,
		"rate_wlanact" => $rate_wlanact,
		"rate_wlancum" => $rate_wlancum,
		"wlan_devices" => $wlan_devices,
		"wlan_status" => $wlan_status,
		"rate_ethact" => $rate_ethact,
		"rate_ethcum" => $rate_ethcum,
		"eth_status" => $eth_status,
		"rate_abact" => $rate_abact,
		"rate_abcum" => $rate_abcum,
		"isdn_status" => $isdn_status,
		"rate_dectact" => $rate_dectact,
		"rate_dectcum" => $rate_dectcum,
		"rate_battchargeact" => $rate_battchargeact,
		"rate_battchargecum" => $rate_battchargecum,
		"dect_status" => $dect_status,
		"rate_usbhostact" => $rate_usbhostact,
		"rate_usbhostcum" => $rate_usbhostcum,
		"usb_status" => $usb_status,
		"act_temperature" => $act_temperature,
		"min_temperature" => $min_temperature,
		"max_temperature" => $max_temperature,
		"avg_temperature" => $avg_temperature,
		"rate_lteact" => $rate_lteact,
		"rate_ltecum" => $rate_ltecum,
		"rate_dvbcact" => $rate_dvbcact,
		"rate_dvbccum" => $rate_dvbccum,
		"rate_plcact" => $rate_plcact,
		"rate_plccum" => $rate_plccum,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_powerline_status'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_powerline_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $status;

	####### convert binary message to hash components #######

	# c-struct component 'status' at offset '0'
	$status = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"status" => $status,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_powermanagment_remote'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_powermanagment_remote {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $remote_action;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'remote_action' at offset '0'
	$remote_action = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'param' at offset '4'
	( $param, $struct_offset ) = avm_event_read_from_binary_union_avm_event_powermanagment_remote_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"remote_action" => $remote_action,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_powermanagment_remote_ressourceinfo'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_powermanagment_remote_ressourceinfo {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $device;
	my $power_rate;

	####### convert binary message to hash components #######

	# c-struct component 'device' at offset '0'
	$device = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'power_rate' at offset '4'
	$power_rate = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"device" => $device,
		"power_rate" => $power_rate,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_powermanagment_status'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_powermanagment_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $substatus;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'substatus' at offset '0'
	$substatus = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'param' at offset '4'
	( $param, $struct_offset ) = avm_event_read_from_binary_union___powermanagment_status_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"substatus" => $substatus,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_push_button'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_push_button {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $key;
	my $pressed;

	####### convert binary message to hash components #######

	# c-struct component 'key' at offset '0'
	$key = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'pressed' at offset '4'
	$pressed = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"key" => $key,
		"pressed" => $pressed,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_remote_source_trigger_request'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_remote_source_trigger_request {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $data;

	####### convert binary message to hash components #######

	# c-struct component 'data' at offset '0'
	( $data, $struct_offset ) = avm_event_read_from_binary_struct_avm_event_data(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"data" => $data,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_remotepcmlink'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_remotepcmlink {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $type;
	my $sharedlen;
	my $sharedpointer;

	####### convert binary message to hash components #######

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'sharedlen' at offset '4'
	$sharedlen = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'sharedpointer' at offset '4'
	$sharedpointer = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"type" => $type,
		"sharedlen" => $sharedlen,
		"sharedpointer" => $sharedpointer,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_remotewatchdog'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_remotewatchdog {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $cmd;
	my $name;
	my $param;

	####### convert binary message to hash components #######

	# c-struct component 'cmd' at offset '0'
	$cmd = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'name' at offset '4'
	$name = unpack("a" . $offset . "/C16", $in_ref);
	$offset += 16;

	# c-struct component 'param' at offset '20'
	$param = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"cmd" => $cmd,
		"name" => $name,
		"param" => $param,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_rpc'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_rpc {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $type;
	my $id;
	my $length;
	my @message = ();

	####### convert binary message to hash components #######

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'id' at offset '4'
	$id = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'length' at offset '4'
	$length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$message[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"type" => $type,
		"id" => $id,
		"length" => $length,
		"message" => [ @message ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_smarthome'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_smarthome {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $length;
	my @ident = ();
	my $type;
	my $payload;

	####### convert binary message to hash components #######

	# c-struct component 'length' at offset '0'
	$length = unpack("", $in_ref);
	$offset += 0;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$ident[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}

	# c-struct component 'type' at offset '20'
	$type = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'payload' at offset '20'
	( $payload, $struct_offset ) = avm_event_read_from_binary_struct_{}(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"length" => $length,
		"ident" => [ @ident ],
		"type" => $type,
		"payload" => $payload,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_smarthome_switch_status'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_smarthome_switch_status {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $type;
	my $value;
	my $ain_length;
	my @ain = ();

	####### convert binary message to hash components #######

	# c-struct component 'type' at offset '0'
	$type = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'value' at offset '4'
	$value = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'ain_length' at offset '4'
	$ain_length = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $ain_length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$ain[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"type" => $type,
		"value" => $value,
		"ain_length" => $ain_length,
		"ain" => [ @ain ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_source_notifier'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_source_notifier {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_source_register'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_source_register {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id_mask;
	my $name;

	####### convert binary message to hash components #######

	# c-struct component 'id_mask' at offset '0'
	( $id_mask, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_id_mask(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'name' at offset '0'
	$name = unpack("C32", $in_ref);
	$offset += 32;


	####### return hash and used length #######
	$return ( {
		"id_mask" => $id_mask,
		"name" => $name,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_source_unregister'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_source_unregister {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id_mask;
	my $name;

	####### convert binary message to hash components #######

	# c-struct component 'id_mask' at offset '0'
	( $id_mask, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_id_mask(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'name' at offset '0'
	$name = unpack("C32", $in_ref);
	$offset += 32;


	####### return hash and used length #######
	$return ( {
		"id_mask" => $id_mask,
		"name" => $name,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_telefon_up'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_telefon_up {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $dummy;

	####### convert binary message to hash components #######

	# c-struct component 'dummy' at offset '0'
	( $dummy, $struct_offset ) = avm_event_read_from_binary_struct_{}(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"dummy" => $dummy,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_telefonprofile'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_telefonprofile {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $on;

	####### convert binary message to hash components #######

	# c-struct component 'on' at offset '0'
	$on = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"on" => $on,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_telephony_missed_call'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_telephony_missed_call {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $length;
	my $p;

	####### convert binary message to hash components #######

	# c-struct component 'length' at offset '0'
	$length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $length;

	# c-struct component 'p' at offset '0'
	( $p, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_telephony_missed_call_params(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"length" => $length,
		"p" => $p,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_telephony_string'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_telephony_string {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $length;
	my @string = ();

	####### convert binary message to hash components #######

	# c-struct component 'length' at offset '0'
	$length = unpack("", $in_ref);
	$offset += 0;
	$array_element_anzahl  = $length;

	$for ( my $i = 0 ; $i < $array_element_anzahl ; $i++ ) {

		my @unpack_type = ( " ", "C", ">S", ">C>S", ">I" );

		$string[$i] = unpack("a" . $offset . "/" . $unpack_type[$array_element_size], $in_ref );
	}


	####### return hash and used length #######
	$return ( {
		"length" => $length,
		"string" => [ @string ],
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_temperature'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_temperature {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $temperature;

	####### convert binary message to hash components #######

	# c-struct component 'temperature' at offset '0'
	$temperature = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"temperature" => $temperature,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $src_id;
	my $dst_id;
	my $seq_nr;
	my $ack;
	my $srv_handle;
	my $clt_handle;
	my $result;
	my $type;
	my $call;

	####### convert binary message to hash components #######

	# c-struct component 'src_id' at offset '0'
	$src_id = unpack(">I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'dst_id' at offset '12'
	$dst_id = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'seq_nr' at offset '24'
	$seq_nr = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'ack' at offset '36'
	$ack = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'srv_handle' at offset '48'
	$srv_handle = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;

	# c-struct component 'clt_handle' at offset '56'
	$clt_handle = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;

	# c-struct component 'result' at offset '64'
	$result = unpack("a" . $offset . "/>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'type' at offset '72'
	$type = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'call' at offset '76'
	( $call, $struct_offset ) = avm_event_read_from_binary_union_avm_event_tffs_call_union(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"src_id" => $src_id,
		"dst_id" => $dst_id,
		"seq_nr" => $seq_nr,
		"ack" => $ack,
		"srv_handle" => $srv_handle,
		"clt_handle" => $clt_handle,
		"result" => $result,
		"type" => $type,
		"call" => $call,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_cleanup'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_cleanup {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $dummy;

	####### convert binary message to hash components #######

	# c-struct component 'dummy' at offset '0'
	$dummy = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"dummy" => $dummy,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_close'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_close {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $dummy;

	####### convert binary message to hash components #######

	# c-struct component 'dummy' at offset '0'
	$dummy = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"dummy" => $dummy,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_deinit'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_deinit {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $dummy;

	####### convert binary message to hash components #######

	# c-struct component 'dummy' at offset '0'
	$dummy = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"dummy" => $dummy,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_info'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_info {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $fill_level;

	####### convert binary message to hash components #######

	# c-struct component 'fill_level' at offset '0'
	$fill_level = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"fill_level" => $fill_level,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_init'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_init {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $mem_offset;
	my $max_seg_size;

	####### convert binary message to hash components #######

	# c-struct component 'mem_offset' at offset '0'
	$mem_offset = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'max_seg_size' at offset '0'
	$max_seg_size = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"mem_offset" => $mem_offset,
		"max_seg_size" => $max_seg_size,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_notify'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_notify {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;
	my $event;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'event' at offset '12'
	$event = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
		"event" => $event,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_open'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_open {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;
	my $mode;
	my $max_segment_size;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'mode' at offset '12'
	$mode = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'max_segment_size' at offset '16'
	$max_segment_size = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
		"mode" => $mode,
		"max_segment_size" => $max_segment_size,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_paniclog'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_paniclog {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $buff_addr;
	my $len;
	my $crc;

	####### convert binary message to hash components #######

	# c-struct component 'buff_addr' at offset '0'
	$buff_addr = unpack(">L", $in_ref);
	$offset += 8;

	# c-struct component 'len' at offset '8'
	$len = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;

	# c-struct component 'crc' at offset '16'
	$crc = unpack("a" . $offset . "/>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"buff_addr" => $buff_addr,
		"len" => $len,
		"crc" => $crc,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_read'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_read {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $buff_addr;
	my $len;
	my $id;
	my $crc;

	####### convert binary message to hash components #######

	# c-struct component 'buff_addr' at offset '0'
	$buff_addr = unpack(">L", $in_ref);
	$offset += 8;

	# c-struct component 'len' at offset '8'
	$len = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;

	# c-struct component 'id' at offset '16'
	$id = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'crc' at offset '28'
	$crc = unpack("a" . $offset . "/>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"buff_addr" => $buff_addr,
		"len" => $len,
		"id" => $id,
		"crc" => $crc,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_reindex'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_reindex {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $dummy;

	####### convert binary message to hash components #######

	# c-struct component 'dummy' at offset '0'
	$dummy = unpack(">I>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"dummy" => $dummy,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_tffs_write'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_tffs_write {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $buff_addr;
	my $len;
	my $id;
	my $final;
	my $crc;

	####### convert binary message to hash components #######

	# c-struct component 'buff_addr' at offset '0'
	$buff_addr = unpack(">L", $in_ref);
	$offset += 8;

	# c-struct component 'len' at offset '8'
	$len = unpack("a" . $offset . "/>L", $in_ref);
	$offset += 8;

	# c-struct component 'id' at offset '16'
	$id = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'final' at offset '28'
	$final = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'crc' at offset '40'
	$crc = unpack("a" . $offset . "/>I>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"buff_addr" => $buff_addr,
		"len" => $len,
		"id" => $id,
		"final" => $final,
		"crc" => $crc,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_unserialised'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_unserialised {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $evnt_id;
	my $data_len;
	my $data;

	####### convert binary message to hash components #######

	# c-struct component 'evnt_id' at offset '0'
	$evnt_id = unpack(">L", $in_ref);
	$offset += 8;

	# c-struct component 'data_len' at offset '8'
	$data_len = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'data' at offset '20'
	$data = unpack("a" . $offset . "/C", $in_ref);
	$offset += 1;


	####### return hash and used length #######
	$return ( {
		"evnt_id" => $evnt_id,
		"data_len" => $data_len,
		"data" => $data,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_user_mode_source_notify'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_user_mode_source_notify {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $id;

	####### convert binary message to hash components #######

	# c-struct component 'id' at offset '0'
	$id = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"id" => $id,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_wlan'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_wlan {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $mac;
	my $u1;
	my $event;
	my $info;
	my $status;
	my $u2;
	my $if_name;
	my $ev_initiator;
	my $ev_reason;
	my $avm_capabilities;
	my $u3;
	my $credentials;

	####### convert binary message to hash components #######

	# c-struct component 'mac' at offset '0'
	$mac = unpack("C6", $in_ref);
	$offset += 6;

	# c-struct component 'u1' at offset '6'
	( $u1, $struct_offset ) = avm_event_read_from_binary_union_avm_event_wlan_client_status_u1(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'event' at offset '6'
	$event = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'info' at offset '10'
	$info = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'status' at offset '14'
	$status = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'u2' at offset '18'
	( $u2, $struct_offset ) = avm_event_read_from_binary_union_avm_event_wlan_client_status_u2(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'if_name' at offset '18'
	$if_name = unpack("a" . $offset . "/CIFNAMSIZ", $in_ref);
	$offset += IFNAMSIZ;

	# c-struct component 'ev_initiator' at offset '19'
	$ev_initiator = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'ev_reason' at offset '19'
	$ev_reason = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'avm_capabilities' at offset '19'
	$avm_capabilities = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;

	# c-struct component 'u3' at offset '19'
	( $u3, $struct_offset ) = avm_event_read_from_binary_union_avm_event_wlan_client_status_u3(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'credentials' at offset '19'
	( $credentials, $struct_offset ) = avm_event_read_from_binary_struct_avm_event_wlan_credentials(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"mac" => $mac,
		"u1" => $u1,
		"event" => $event,
		"info" => $info,
		"status" => $status,
		"u2" => $u2,
		"if_name" => $if_name,
		"ev_initiator" => $ev_initiator,
		"ev_reason" => $ev_reason,
		"avm_capabilities" => $avm_capabilities,
		"u3" => $u3,
		"credentials" => $credentials,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_wlan_credentials'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_wlan_credentials {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $ssid;
	my $bssid;
	my $key;
	my $security;

	####### convert binary message to hash components #######

	# c-struct component 'ssid' at offset '0'
	$ssid = unpack("C33", $in_ref);
	$offset += 33;

	# c-struct component 'bssid' at offset '33'
	$bssid = unpack("a" . $offset . "/C6", $in_ref);
	$offset += 6;

	# c-struct component 'key' at offset '39'
	$key = unpack("a" . $offset . "/C65", $in_ref);
	$offset += 65;

	# c-struct component 'security' at offset '104'
	$security = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"ssid" => $ssid,
		"bssid" => $bssid,
		"key" => $key,
		"security" => $security,
	}, $offset );
}


##########################################################################################
# convert function for 'struct avm_event_wlan_power'
##########################################################################################
sub avm_event_read_from_binary_struct_avm_event_wlan_power {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $power;

	####### convert binary message to hash components #######

	# c-struct component 'power' at offset '0'
	$power = unpack("", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"power" => $power,
	}, $offset );
}


##########################################################################################
# convert function for 'struct cpmac_event_struct'
##########################################################################################
sub avm_event_read_from_binary_struct_cpmac_event_struct {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_header;
	my $ports;
	my $port;

	####### convert binary message to hash components #######

	# c-struct component 'event_header' at offset '0'
	( $event_header, $struct_offset ) = avm_event_read_from_binary_struct__avm_event_header(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'ports' at offset '0'
	$ports = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'port' at offset '0'
	( $port, $struct_offset ) = avm_event_read_from_binary_struct_cpmac_port(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"event_header" => $event_header,
		"ports" => $ports,
		"port" => $port,
	}, $offset );
}


##########################################################################################
# convert function for 'struct cpmac_port'
##########################################################################################
sub avm_event_read_from_binary_struct_cpmac_port {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $cable;
	my $link;
	my $speed100;
	my $fullduplex;
	my $speed;
	my $maxspeed;

	####### convert binary message to hash components #######

	# c-struct component 'cable' at offset '0'
	$cable = unpack(">S", $in_ref);
	$offset += 1;

	# c-struct component 'link' at offset '1'
	$link = unpack("a" . $offset . "/>S", $in_ref);
	$offset += 1;

	# c-struct component 'speed100' at offset '2'
	$speed100 = unpack("a" . $offset . "/>S", $in_ref);
	$offset += 1;

	# c-struct component 'fullduplex' at offset '3'
	$fullduplex = unpack("a" . $offset . "/>S", $in_ref);
	$offset += 1;

	# c-struct component 'speed' at offset '4'
	$speed = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;

	# c-struct component 'maxspeed' at offset '8'
	$maxspeed = unpack("a" . $offset . "/>I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"cable" => $cable,
		"link" => $link,
		"speed100" => $speed100,
		"fullduplex" => $fullduplex,
		"speed" => $speed,
		"maxspeed" => $maxspeed,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_client_common'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_client_common {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $mac;
	my $iface;
	my $initiator;

	####### convert binary message to hash components #######

	# c-struct component 'mac' at offset '0'
	$mac = unpack(">S6", $in_ref);
	$offset += 6;

	# c-struct component 'iface' at offset '6'
	$iface = unpack("a" . $offset . "/CIFNAMSIZ + 1", $in_ref);
	$offset += IFNAMSIZ + 1;

	# c-struct component 'initiator' at offset '7'
	$initiator = unpack("a" . $offset . "/C16 + 1", $in_ref);
	$offset += 16 + 1;


	####### return hash and used length #######
	$return ( {
		"mac" => $mac,
		"iface" => $iface,
		"initiator" => $initiator,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_client_connect_info'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_client_connect_info {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $common;
	my $info_context;
	my $reason;
	my $max_node_count;
	my $ieee80211_code;

	####### convert binary message to hash components #######

	# c-struct component 'common' at offset '0'
	( $common, $struct_offset ) = avm_event_read_from_binary_struct_wlan_event_data_client_common(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'info_context' at offset '0'
	$info_context = unpack(">S", $in_ref);
	$offset += 1;

	# c-struct component 'reason' at offset '1'
	$reason = unpack("a" . $offset . "/>S", $in_ref);
	$offset += 1;

	# c-struct component 'max_node_count' at offset '2'
	$max_node_count = unpack("a" . $offset . "/>I>I>I", $in_ref);
	$offset += 4;

	# c-struct component 'ieee80211_code' at offset '14'
	$ieee80211_code = unpack("a" . $offset . "/", $in_ref);
	$offset += 0;


	####### return hash and used length #######
	$return ( {
		"common" => $common,
		"info_context" => $info_context,
		"reason" => $reason,
		"max_node_count" => $max_node_count,
		"ieee80211_code" => $ieee80211_code,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_client_state_change'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_client_state_change {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $common;
	my $state;

	####### convert binary message to hash components #######

	# c-struct component 'common' at offset '0'
	( $common, $struct_offset ) = avm_event_read_from_binary_struct_wlan_event_data_client_common(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'state' at offset '0'
	$state = unpack(">S", $in_ref);
	$offset += 1;


	####### return hash and used length #######
	$return ( {
		"common" => $common,
		"state" => $state,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_client_state_idle'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_client_state_idle {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $common;
	my $idle;

	####### convert binary message to hash components #######

	# c-struct component 'common' at offset '0'
	( $common, $struct_offset ) = avm_event_read_from_binary_struct_wlan_event_data_client_common(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'idle' at offset '0'
	$idle = unpack(">S", $in_ref);
	$offset += 1;


	####### return hash and used length #######
	$return ( {
		"common" => $common,
		"idle" => $idle,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_radio_recovery'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_radio_recovery {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $radio_id;
	my $state;

	####### convert binary message to hash components #######

	# c-struct component 'radio_id' at offset '0'
	$radio_id = unpack("", $in_ref);
	$offset += 0;

	# c-struct component 'state' at offset '0'
	$state = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"radio_id" => $radio_id,
		"state" => $state,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_scan_common'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_scan_common {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $iface;
	my $initiator;

	####### convert binary message to hash components #######

	# c-struct component 'iface' at offset '0'
	$iface = unpack("CIFNAMSIZ + 1", $in_ref);
	$offset += IFNAMSIZ + 1;

	# c-struct component 'initiator' at offset '1'
	$initiator = unpack("a" . $offset . "/C16 + 1", $in_ref);
	$offset += 16 + 1;


	####### return hash and used length #######
	$return ( {
		"iface" => $iface,
		"initiator" => $initiator,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_data_scan_event_info'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_data_scan_event_info {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $common;
	my $event_type;

	####### convert binary message to hash components #######

	# c-struct component 'common' at offset '0'
	( $common, $struct_offset ) = avm_event_read_from_binary_struct_wlan_event_data_scan_common(
				( $msg_length - $offset), unpack("C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;

	# c-struct component 'event_type' at offset '0'
	$event_type = unpack(">I", $in_ref);
	$offset += 4;


	####### return hash and used length #######
	$return ( {
		"common" => $common,
		"event_type" => $event_type,
	}, $offset );
}


##########################################################################################
# convert function for 'struct wlan_event_def'
##########################################################################################
sub avm_event_read_from_binary_struct_wlan_event_def {
	my ( $msg_length, $in_ref ) = $@;
	my $offset = 0;
	my $struct_offset;
	my $array_element_anzahl = 0;
	my $array_element_size = 1;

	####### liste of hash components #######
	my $event_id;
	my $event_data;

	####### convert binary message to hash components #######

	# c-struct component 'event_id' at offset '0'
	$event_id = unpack(">I", $in_ref);
	$offset += 4;

	# c-struct component 'event_data' at offset '4'
	( $event_data, $struct_offset ) = avm_event_read_from_binary_union_wlan_event_data(
				( $msg_length - $offset), unpack("a" . $offset . "/C" . ($msg_length - $offset), $in_ref));
	$offset += $struct_offset;


	####### return hash and used length #######
	$return ( {
		"event_id" => $event_id,
		"event_data" => $event_data,
	}, $offset );
}



1;
