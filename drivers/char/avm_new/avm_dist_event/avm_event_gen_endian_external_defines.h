#ifndef _AVM_EVENT_GENERATE_EXTERN_DEFS_H_
#define _AVM_EVENT_GENERATE_EXTERN_DEFS_H_
#ifdef WIRESHARK_PLUGIN
	struct enumInformation { 
		char *name;
		uint64_t value;
	};
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/
struct _endian_convert { 
#define ENDIAN_CONVERT_IGNORE               (1)       /* wert wird ignoriert */ 
#define ENDIAN_CONVERT_LENGTH               (2)       /* dieser wert gibt die länge der gesamten struktur, incl untersturkturen an */ 
#define ENDIAN_CONVERT_SUB_LENGTH           (4)       /* dieser wert gibt die länge untersturkturen an */ 
#define ENDIAN_CONVERT_REPEAT               (8)       /* letzter wert der repeat gruppe */ 
#define ENDIAN_CONVERT_SELECT               (16)      /* selectionswert */ 
#define ENDIAN_CONVERT_SELECT_ENTRY         (32)      /* selections eintrag */ 
#define ENDIAN_CONVERT_LAST                 (64)      /* letzter Eintrag in einem struct */ 
#define ENDIAN_CONVERT_ARRAY_ELEMENT_SIZE   (128)     /* Anzahl der Elemente im array */ 
#define ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL (256)     /* Größe der Elemente im array */ 
#define ENDIAN_CONVERT_ARRAY_USE            (512)     /* Nutze Array größen Vorgaben */ 
#define ENDIAN_CONVERT_UNSUPPORTED          (1024)    /* unsupported */ 
#define ENDIAN_CONVERT_TOTAL_LENGTH         (2048)    /* dieser Wert gibt die Länge gesamten Struktur samt Untersturkturen an */ 
#define ENDIAN_CONVERT_LAST_ARRAY_ITEM (1<<31)        /* Dieses Flag markiert den letzten Eintrag in einem Array. (wird genutzt vom Lua-Protokol-Plugin) */ 
                                                      /* Terminieren mit einem 0-Eintrag geht nicht, da einige Array's Lücken aufweisen                  */ 
	uint32_t flags; 
	uint32_t offset; 
	unsigned char bits; 
	unsigned char bit_offset; 
	uint32_t size; 
	struct _endian_convert *substruct; 
	uint32_t (*enum_check_function )(uint32_t, unsigned int E); 
	uint32_t ab_version;  /* 0 immer gültig */
	uint32_t bis_version;  /* 0 immer gültig */
	uint32_t default_value;  /* wird auf type des feldes gecarstet */
	char *name; 
	char *struct_name; 
#ifdef WIRESHARK_PLUGIN
	struct enumInformation *enumInfo;  // Verweis auf die generierten Enum-Hilfstabellen (Wert -> Name als String)
	char *enumName;                    // Name der Enumeration-Tabelle: Wird fuer die Lua-Generierung benoetigt
	struct enumInformation *enumFlags; // Verweis auf Enum-Tabellen, das das definierende Feld als Flag-Makse genutzt wird
#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/
}; 


extern struct _endian_convert convert_message_struct___powermanagment_status_union[];
extern struct _endian_convert convert_message_struct__avm_event_ambient_brightness[];
extern struct _endian_convert convert_message_struct__avm_event_checkpoint[];
extern struct _endian_convert convert_message_struct__avm_event_cmd[];
extern struct _endian_convert convert_message_struct__avm_event_cmd_param[];
extern struct _endian_convert convert_message_struct__avm_event_cmd_param_register[];
extern struct _endian_convert convert_message_struct__avm_event_cmd_param_release[];
extern struct _endian_convert convert_message_struct__avm_event_cmd_param_source_trigger[];
extern struct _endian_convert convert_message_struct__avm_event_cmd_param_trigger[];
extern struct _endian_convert convert_message_struct__avm_event_cpu_idle[];
extern struct _endian_convert convert_message_struct__avm_event_cpu_run[];
extern struct _endian_convert convert_message_struct__avm_event_fax_file[];
extern struct _endian_convert convert_message_struct__avm_event_fax_status[];
extern struct _endian_convert convert_message_struct__avm_event_firmware_update_available[];
extern struct _endian_convert convert_message_struct__avm_event_header[];
extern struct _endian_convert convert_message_struct__avm_event_id_mask[];
extern struct _endian_convert convert_message_struct__avm_event_internet_new_ip[];
extern struct _endian_convert convert_message_struct__avm_event_led_info[];
extern struct _endian_convert convert_message_struct__avm_event_led_status[];
extern struct _endian_convert convert_message_struct__avm_event_log[];
extern struct _endian_convert convert_message_struct__avm_event_mass_storage_mount[];
extern struct _endian_convert convert_message_struct__avm_event_mass_storage_unmount[];
extern struct _endian_convert convert_message_struct__avm_event_piglet[];
extern struct _endian_convert convert_message_struct__avm_event_pm_info_stat[];
extern struct _endian_convert convert_message_struct__avm_event_powerline_status[];
extern struct _endian_convert convert_message_struct__avm_event_powermanagment_remote[];
extern struct _endian_convert convert_message_struct__avm_event_powermanagment_remote_ressourceinfo[];
extern struct _endian_convert convert_message_struct__avm_event_powermanagment_status[];
extern struct _endian_convert convert_message_struct__avm_event_push_button[];
extern struct _endian_convert convert_message_struct__avm_event_remotepcmlink[];
extern struct _endian_convert convert_message_struct__avm_event_remotewatchdog[];
extern struct _endian_convert convert_message_struct__avm_event_rpc[];
extern struct _endian_convert convert_message_struct__avm_event_smarthome[];
extern struct _endian_convert convert_message_struct__avm_event_smarthome_switch_status[];
extern struct _endian_convert convert_message_struct__avm_event_telefonprofile[];
extern struct _endian_convert convert_message_struct__avm_event_telephony_missed_call[];
extern struct _endian_convert convert_message_struct__avm_event_telephony_missed_call_params[];
extern struct _endian_convert convert_message_struct__avm_event_temperature[];
extern struct _endian_convert convert_message_struct__avm_event_user_mode_source_notify[];
extern struct _endian_convert convert_message_struct__avm_event_wlan[];
extern struct _endian_convert convert_message_struct__cpmac_event_struct[];
extern struct _endian_convert convert_message_struct_avm_event_ambient_brightness[];
extern struct _endian_convert convert_message_struct_avm_event_boykott[];
extern struct _endian_convert convert_message_struct_avm_event_checkpoint[];
extern struct _endian_convert convert_message_struct_avm_event_cpu_idle[];
extern struct _endian_convert convert_message_struct_avm_event_cpu_run[];
extern struct _endian_convert convert_message_struct_avm_event_data[];
extern struct _endian_convert convert_message_struct_avm_event_data_union[];
extern struct _endian_convert convert_message_struct_avm_event_fax_file[];
extern struct _endian_convert convert_message_struct_avm_event_fax_status[];
extern struct _endian_convert convert_message_struct_avm_event_firmware_update_available[];
extern struct _endian_convert convert_message_struct_avm_event_internet_new_ip[];
extern struct _endian_convert convert_message_struct_avm_event_internet_new_ip_param[];
extern struct _endian_convert convert_message_struct_avm_event_led_info[];
extern struct _endian_convert convert_message_struct_avm_event_led_status[];
extern struct _endian_convert convert_message_struct_avm_event_log[];
extern struct _endian_convert convert_message_struct_avm_event_mass_storage_mount[];
extern struct _endian_convert convert_message_struct_avm_event_mass_storage_unmount[];
extern struct _endian_convert convert_message_struct_avm_event_message[];
extern struct _endian_convert convert_message_struct_avm_event_message_union[];
extern struct _endian_convert convert_message_struct_avm_event_piglet[];
extern struct _endian_convert convert_message_struct_avm_event_ping[];
extern struct _endian_convert convert_message_struct_avm_event_pm_info_stat[];
extern struct _endian_convert convert_message_struct_avm_event_powerline_status[];
extern struct _endian_convert convert_message_struct_avm_event_powermanagment_remote[];
extern struct _endian_convert convert_message_struct_avm_event_powermanagment_remote_ressourceinfo[];
extern struct _endian_convert convert_message_struct_avm_event_powermanagment_remote_union[];
extern struct _endian_convert convert_message_struct_avm_event_powermanagment_status[];
extern struct _endian_convert convert_message_struct_avm_event_push_button[];
extern struct _endian_convert convert_message_struct_avm_event_remote_source_trigger_request[];
extern struct _endian_convert convert_message_struct_avm_event_remotepcmlink[];
extern struct _endian_convert convert_message_struct_avm_event_remotewatchdog[];
extern struct _endian_convert convert_message_struct_avm_event_rpc[];
extern struct _endian_convert convert_message_struct_avm_event_smarthome[];
extern struct _endian_convert convert_message_struct_avm_event_smarthome_switch_status[];
extern struct _endian_convert convert_message_struct_avm_event_source_notifier[];
extern struct _endian_convert convert_message_struct_avm_event_source_register[];
extern struct _endian_convert convert_message_struct_avm_event_source_unregister[];
extern struct _endian_convert convert_message_struct_avm_event_telefon_up[];
extern struct _endian_convert convert_message_struct_avm_event_telefonprofile[];
extern struct _endian_convert convert_message_struct_avm_event_telephony_call_params[];
extern struct _endian_convert convert_message_struct_avm_event_telephony_missed_call[];
extern struct _endian_convert convert_message_struct_avm_event_telephony_string[];
extern struct _endian_convert convert_message_struct_avm_event_temperature[];
extern struct _endian_convert convert_message_struct_avm_event_tffs[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_call_union[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_cleanup[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_close[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_deinit[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_info[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_init[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_notify[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_open[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_paniclog[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_read[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_reindex[];
extern struct _endian_convert convert_message_struct_avm_event_tffs_write[];
extern struct _endian_convert convert_message_struct_avm_event_unserialised[];
extern struct _endian_convert convert_message_struct_avm_event_user_mode_source_notify[];
extern struct _endian_convert convert_message_struct_avm_event_wlan[];
extern struct _endian_convert convert_message_struct_avm_event_wlan_client_status_u1[];
extern struct _endian_convert convert_message_struct_avm_event_wlan_client_status_u2[];
extern struct _endian_convert convert_message_struct_avm_event_wlan_client_status_u3[];
extern struct _endian_convert convert_message_struct_avm_event_wlan_credentials[];
extern struct _endian_convert convert_message_struct_avm_event_wlan_power[];
extern struct _endian_convert convert_message_struct_cpmac_event_struct[];
extern struct _endian_convert convert_message_struct_cpmac_port[];
extern struct _endian_convert convert_message_struct_wlan_event_data[];
extern struct _endian_convert convert_message_struct_wlan_event_data_client_common[];
extern struct _endian_convert convert_message_struct_wlan_event_data_client_connect_info[];
extern struct _endian_convert convert_message_struct_wlan_event_data_client_state_change[];
extern struct _endian_convert convert_message_struct_wlan_event_data_client_state_idle[];
extern struct _endian_convert convert_message_struct_wlan_event_data_radio_recovery[];
extern struct _endian_convert convert_message_struct_wlan_event_data_scan_common[];
extern struct _endian_convert convert_message_struct_wlan_event_data_scan_event_info[];
extern struct _endian_convert convert_message_struct_wlan_event_def[];
extern struct _endian_convert convert_message_union___powermanagment_status_union[];
extern struct _endian_convert convert_message_union__avm_event_cmd_param[];
extern struct _endian_convert convert_message_union_avm_event_data_union[];
extern struct _endian_convert convert_message_union_avm_event_internet_new_ip_param[];
extern struct _endian_convert convert_message_union_avm_event_message_union[];
extern struct _endian_convert convert_message_union_avm_event_powermanagment_remote_union[];
extern struct _endian_convert convert_message_union_avm_event_telephony_call_params[];
extern struct _endian_convert convert_message_union_avm_event_tffs_call_union[];
extern struct _endian_convert convert_message_union_avm_event_wlan_client_status_u1[];
extern struct _endian_convert convert_message_union_avm_event_wlan_client_status_u2[];
extern struct _endian_convert convert_message_union_avm_event_wlan_client_status_u3[];
extern struct _endian_convert convert_message_union_wlan_event_data[];
#endif /*--- #ifndef _AVM_EVENT_GENERATE_EXTERN_DEFS_H_ ---*/
