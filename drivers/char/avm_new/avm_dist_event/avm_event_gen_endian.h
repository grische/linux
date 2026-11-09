#include "avm_event_gen_endian_external_defines.h"

#include "avm_event_gen_enum_range.h"


/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_ambient_brightness'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_ambient_brightness[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_ambient_brightness",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_ambient_brightness, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_ambient_brightness",
		.flags = 0,
		.name = "value",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_ambient_brightness, value),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_ambient_brightness",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "maxvalue",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_ambient_brightness, maxvalue),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_checkpoint'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_checkpoint[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_checkpoint",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_checkpoint, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_checkpoint",
		.flags = 0,
		.name = "node_id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct _avm_event_checkpoint, node_id),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_checkpoint",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "checkpoints",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct _avm_event_checkpoint, checkpoints),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cmd'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cmd[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cmd",
		.flags = 0,
		.name = "cmd",
		.size = sizeof(enum __avm_event_cmd),
		.offset = __builtin_offsetof(struct _avm_event_cmd, cmd),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table___avm_event_cmd,
		.enumName = "enum_table___avm_event_cmd"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_cmd",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union__avm_event_cmd_param,
		.substruct = convert_message_union__avm_event_cmd_param,
		.offset = __builtin_offsetof(struct _avm_event_cmd, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cmd_param_register'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cmd_param_register[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cmd_param_register",
		.flags = 0,
		.name = "mask",
		.substruct = convert_message_struct__avm_event_id_mask,
		.offset = __builtin_offsetof(struct _avm_event_cmd_param_register, mask),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_cmd_param_register",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "Name",
		.size = sizeof(char) * MAX_EVENT_CLIENT_NAME_LEN + 1,
		.offset = __builtin_offsetof(struct _avm_event_cmd_param_register, Name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cmd_param_release'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cmd_param_release[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cmd_param_release",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "Name",
		.size = sizeof(char) * MAX_EVENT_CLIENT_NAME_LEN + 1,
		.offset = __builtin_offsetof(struct _avm_event_cmd_param_release, Name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cmd_param_source_trigger'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cmd_param_source_trigger[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cmd_param_source_trigger",
		.flags = 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct _avm_event_cmd_param_source_trigger, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_cmd_param_source_trigger",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "data_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_cmd_param_source_trigger, data_length),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cmd_param_trigger'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cmd_param_trigger[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cmd_param_trigger",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct _avm_event_cmd_param_trigger, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cpu_idle'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cpu_idle[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "cpu_idle",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, cpu_idle),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "dsl_dsp_idle",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, dsl_dsp_idle),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "voice_dsp_idle",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, voice_dsp_idle),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "mem_strictlyused",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, mem_strictlyused),
	},
	[5] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "mem_cacheused",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, mem_cacheused),
	},
	[6] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = 0,
		.name = "mem_physfree",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, mem_physfree),
	},
	[7] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_idle",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "cputype",
		.size = sizeof(enum _cputype),
		.offset = __builtin_offsetof(struct _avm_event_cpu_idle, cputype),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__cputype,
		.enumName = "enum_table__cputype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_cpu_run'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_cpu_run[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_run",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_cpu_run, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_run",
		.flags = 0,
		.name = "cputype",
		.size = sizeof(enum _cputype),
		.offset = __builtin_offsetof(struct _avm_event_cpu_run, cputype),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__cputype,
		.enumName = "enum_table__cputype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_cpu_run",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "cpu_run",
		.size = sizeof(unsigned char) * 4,
		.offset = __builtin_offsetof(struct _avm_event_cpu_run, cpu_run),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_fax_file'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_fax_file[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_fax_file",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_fax_file, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_fax_file",
		.flags = 0,
		.name = "action",
		.size = sizeof(enum fax_file_event_type),
		.offset = __builtin_offsetof(struct _avm_event_fax_file, action),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_fax_file_event_type,
		.enumName = "enum_table_fax_file_event_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_fax_file",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "date",
		.offset = __builtin_offsetof(struct _avm_event_fax_file, date),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_fax_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_fax_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_fax_status",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_fax_status, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_fax_status",
		.flags = 0,
		.name = "fax_receive_mode",
		.size = sizeof(enum fax_receive_mode),
		.offset = __builtin_offsetof(struct _avm_event_fax_status, fax_receive_mode),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_fax_receive_mode,
		.enumName = "enum_table_fax_receive_mode"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_fax_status",
		.flags = 0,
		.name = "fax_storage_dest",
		.size = sizeof(enum fax_storage_dest),
		.offset = __builtin_offsetof(struct _avm_event_fax_status, fax_storage_dest),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_fax_storage_dest,
		.enumName = "enum_table_fax_storage_dest"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_fax_status",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "dirname_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_fax_status, dirname_length),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_fax_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dirname",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_fax_status, dirname),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_firmware_update_available'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_firmware_update_available[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_firmware_update_available",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_firmware_update_available, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_firmware_update_available",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum avm_event_firmware_type),
		.offset = __builtin_offsetof(struct _avm_event_firmware_update_available, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_firmware_type,
		.enumName = "enum_table_avm_event_firmware_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_firmware_update_available",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "version_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_firmware_update_available, version_length),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_firmware_update_available",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "version",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_firmware_update_available, version),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_header'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_header[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_header",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct _avm_event_header, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_id_mask'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_id_mask[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_id_mask",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "mask",
		.offset = __builtin_offsetof(struct _avm_event_id_mask, mask),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_internet_new_ip'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_internet_new_ip[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_internet_new_ip",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_internet_new_ip, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_internet_new_ip",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "sel",
		.size = sizeof(enum avm_event_internet_new_ip_param_sel),
		.offset = __builtin_offsetof(struct _avm_event_internet_new_ip, sel),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_internet_new_ip_param_sel,
		.enumName = "enum_table_avm_event_internet_new_ip_param_sel"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_internet_new_ip_param_sel,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_internet_new_ip",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "params",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_internet_new_ip_param,
		.substruct = convert_message_union_avm_event_internet_new_ip_param,
		.offset = __builtin_offsetof(struct _avm_event_internet_new_ip, params),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_led_info'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_led_info[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_led_info, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "mode",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_info, mode),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "param1",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_info, param1),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "param2",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_info, param2),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "gpio_driver_type",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_info, gpio_driver_type),
	},
	[5] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "gpio",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_info, gpio),
	},
	[6] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = 0,
		.name = "pos",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_info, pos),
	},
	[7] = {
		.struct_name =  "convert_message_struct__avm_event_led_info",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(char) * 32,
		.offset = __builtin_offsetof(struct _avm_event_led_info, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_led_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_led_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_led_status",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_led_status, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_led_status",
		.flags = 0,
		.name = "led",
		.size = sizeof(enum _led_event),
		.offset = __builtin_offsetof(struct _avm_event_led_status, led),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__led_event,
		.enumName = "enum_table__led_event"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_led_status",
		.flags = 0,
		.name = "state",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_status, state),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_led_status",
		.flags = 0,
		.name = "param_len",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_led_status, param_len),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_led_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "params",
		.size = sizeof(unsigned char) * 245,
		.offset = __builtin_offsetof(struct _avm_event_led_status, params),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_log'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_log[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_log",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_log, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_log",
		.flags = 0,
		.name = "logtype",
		.size = sizeof(enum _avm_logtype),
		.offset = __builtin_offsetof(struct _avm_event_log, logtype),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_logtype,
		.enumName = "enum_table__avm_logtype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_log",
		.flags = 0,
		.name = "loglen",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_log, loglen),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_log",
		.flags = 0,
		.name = "logpointer",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_log, logpointer),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_log",
		.flags = 0,
		.name = "checksum",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_log, checksum),
	},
	[5] = {
		.struct_name =  "convert_message_struct__avm_event_log",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "rebootflag",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_log, rebootflag),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_mass_storage_mount'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_mass_storage_mount[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_mount",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_mount, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_mount",
		.flags = 0,
		.name = "size",
		.size = sizeof(unsigned long long),
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_mount, size),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_mount",
		.flags = 0,
		.name = "free",
		.size = sizeof(unsigned long long),
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_mount, free),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_mount",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "name_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_mount, name_length),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_mount",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_mount, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_mass_storage_unmount'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_mass_storage_unmount[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_unmount",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_unmount, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_unmount",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "name_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_unmount, name_length),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_mass_storage_unmount",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_mass_storage_unmount, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_piglet'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_piglet[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_piglet",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_piglet, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_piglet",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "type",
		.size = sizeof(enum _avm_piglettype),
		.offset = __builtin_offsetof(struct _avm_event_piglet, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_piglettype,
		.enumName = "enum_table__avm_piglettype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_pm_info_stat'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_pm_info_stat[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "reserved1",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, reserved1),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_sumact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_sumact),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_sumcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_sumcum),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_systemact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_systemact),
	},
	[5] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_systemcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_systemcum),
	},
	[6] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "system_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, system_status),
	},
	[7] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dspact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_dspact),
	},
	[8] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dspcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_dspcum),
	},
	[9] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_wlanact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_wlanact),
	},
	[10] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_wlancum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_wlancum),
	},
	[11] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "wlan_devices",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, wlan_devices),
	},
	[12] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "wlan_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, wlan_status),
	},
	[13] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_ethact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_ethact),
	},
	[14] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_ethcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_ethcum),
	},
	[15] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "eth_status",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, eth_status),
	},
	[16] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_abact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_abact),
	},
	[17] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_abcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_abcum),
	},
	[18] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "isdn_status",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, isdn_status),
	},
	[19] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dectact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_dectact),
	},
	[20] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dectcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_dectcum),
	},
	[21] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_battchargeact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_battchargeact),
	},
	[22] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_battchargecum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_battchargecum),
	},
	[23] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "dect_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, dect_status),
	},
	[24] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_usbhostact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_usbhostact),
	},
	[25] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_usbhostcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_usbhostcum),
	},
	[26] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "usb_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, usb_status),
	},
	[27] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "act_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, act_temperature),
	},
	[28] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "min_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, min_temperature),
	},
	[29] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "max_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, max_temperature),
	},
	[30] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "avg_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, avg_temperature),
	},
	[31] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_lteact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_lteact),
	},
	[32] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_ltecum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_ltecum),
	},
	[33] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dvbcact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_dvbcact),
	},
	[34] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dvbccum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_dvbccum),
	},
	[35] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_plcact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_plcact),
	},
	[36] = {
		.struct_name =  "convert_message_struct__avm_event_pm_info_stat",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "rate_plccum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_pm_info_stat, rate_plccum),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_powerline_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_powerline_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_powerline_status",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_powerline_status, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_powerline_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "status",
		.size = sizeof(enum ePLCState),
		.offset = __builtin_offsetof(struct _avm_event_powerline_status, status),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_ePLCState,
		.enumName = "enum_table_ePLCState"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_powermanagment_remote'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_powermanagment_remote[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_remote",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_remote, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_remote",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "remote_action",
		.size = sizeof(enum avm_event_powermanagment_remote_action),
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_remote, remote_action),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_powermanagment_remote_action,
		.enumName = "enum_table_avm_event_powermanagment_remote_action"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_powermanagment_remote_action,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_remote",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_powermanagment_remote_union,
		.substruct = convert_message_union_avm_event_powermanagment_remote_union,
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_remote, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_powermanagment_remote_ressourceinfo'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_powermanagment_remote_ressourceinfo[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_remote_ressourceinfo",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_remote_ressourceinfo, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_remote_ressourceinfo",
		.flags = 0,
		.name = "device",
		.size = sizeof(enum _powermanagment_device),
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_remote_ressourceinfo, device),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__powermanagment_device,
		.enumName = "enum_table__powermanagment_device"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_remote_ressourceinfo",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "power_rate",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_remote_ressourceinfo, power_rate),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_powermanagment_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_powermanagment_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_status",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_status, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_status",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "substatus",
		.size = sizeof(enum _powermanagment_status_type),
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_status, substatus),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__powermanagment_status_type,
		.enumName = "enum_table__powermanagment_status_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range__powermanagment_status_type,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_powermanagment_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union___powermanagment_status_union,
		.substruct = convert_message_union___powermanagment_status_union,
		.offset = __builtin_offsetof(struct _avm_event_powermanagment_status, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_push_button'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_push_button[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_push_button",
		.flags = 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct _avm_event_push_button, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_push_button",
		.flags = 0,
		.name = "key",
		.size = sizeof(enum _avm_event_push_button_key),
		.offset = __builtin_offsetof(struct _avm_event_push_button, key),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_push_button_key,
		.enumName = "enum_table__avm_event_push_button_key"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_push_button",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "pressed",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct _avm_event_push_button, pressed),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_remotepcmlink'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_remotepcmlink[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_remotepcmlink",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_remotepcmlink, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_remotepcmlink",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum _avm_remotepcmlinktype),
		.offset = __builtin_offsetof(struct _avm_event_remotepcmlink, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_remotepcmlinktype,
		.enumName = "enum_table__avm_remotepcmlinktype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_remotepcmlink",
		.flags = 0,
		.name = "sharedlen",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_remotepcmlink, sharedlen),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_remotepcmlink",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "sharedpointer",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_remotepcmlink, sharedpointer),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_remotewatchdog'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_remotewatchdog[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_remotewatchdog",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_remotewatchdog, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_remotewatchdog",
		.flags = 0,
		.name = "cmd",
		.size = sizeof(enum _avm_remote_wdt_cmd),
		.offset = __builtin_offsetof(struct _avm_event_remotewatchdog, cmd),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_remote_wdt_cmd,
		.enumName = "enum_table__avm_remote_wdt_cmd"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_remotewatchdog",
		.flags = 0,
		.name = "name",
		.size = sizeof(char) * 16,
		.offset = __builtin_offsetof(struct _avm_event_remotewatchdog, name),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_remotewatchdog",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_remotewatchdog, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_rpc'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_rpc[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_rpc",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_rpc, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_rpc",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum _avm_rpctype),
		.offset = __builtin_offsetof(struct _avm_event_rpc, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_rpctype,
		.enumName = "enum_table__avm_rpctype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_rpc",
		.flags = 0,
		.name = "id",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_rpc, id),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_rpc",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_rpc, length),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_rpc",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "message",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_rpc, message),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_smarthome'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_smarthome[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_smarthome, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome",
		.flags = 0,
		.name = "length",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct _avm_event_smarthome, length),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome",
		.flags = ENDIAN_CONVERT_ARRAY_USE | 0,
		.name = "ident",
		.size = sizeof(char) * 20,
		.offset = __builtin_offsetof(struct _avm_event_smarthome, ident),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome",
		.flags = 0,
		.name = "type",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct _avm_event_smarthome, type),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "payload",
		.offset = __builtin_offsetof(struct _avm_event_smarthome, payload),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_smarthome_switch_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_smarthome_switch_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome_switch_status",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_smarthome_switch_status, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome_switch_status",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum avm_event_switch_type),
		.offset = __builtin_offsetof(struct _avm_event_smarthome_switch_status, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_switch_type,
		.enumName = "enum_table_avm_event_switch_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome_switch_status",
		.flags = 0,
		.name = "value",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_smarthome_switch_status, value),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome_switch_status",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "ain_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_smarthome_switch_status, ain_length),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_smarthome_switch_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "ain",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct _avm_event_smarthome_switch_status, ain),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_telefonprofile'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_telefonprofile[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_telefonprofile",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_telefonprofile, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_telefonprofile",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "on",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_telefonprofile, on),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_telephony_missed_call'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_telephony_missed_call[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_telephony_missed_call",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_telephony_missed_call, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_telephony_missed_call",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_telephony_missed_call, length),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_telephony_missed_call",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "p",
		.substruct = convert_message_struct__avm_event_telephony_missed_call_params,
		.offset = __builtin_offsetof(struct _avm_event_telephony_missed_call, p),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_telephony_missed_call_params'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_telephony_missed_call_params[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_telephony_missed_call_params",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "id",
		.size = sizeof(enum avm_event_telephony_param_sel),
		.offset = __builtin_offsetof(struct _avm_event_telephony_missed_call_params, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_telephony_param_sel,
		.enumName = "enum_table_avm_event_telephony_param_sel"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_telephony_param_sel,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_telephony_missed_call_params",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "params",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_telephony_call_params,
		.substruct = convert_message_union_avm_event_telephony_call_params,
		.offset = __builtin_offsetof(struct _avm_event_telephony_missed_call_params, params),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_temperature'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_temperature[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_temperature",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_temperature, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_temperature",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "temperature",
		.size = sizeof(int),
		.offset = __builtin_offsetof(struct _avm_event_temperature, temperature),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_user_mode_source_notify'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_user_mode_source_notify[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_user_mode_source_notify",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_user_mode_source_notify, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_user_mode_source_notify",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct _avm_event_user_mode_source_notify, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _avm_event_wlan'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__avm_event_wlan[] = {
	[0] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct _avm_event_wlan, header),
	},
	[1] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "mac",
		.size = sizeof(char) * 6,
		.offset = __builtin_offsetof(struct _avm_event_wlan, mac),
	},
	[2] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "u1",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, u1),
	},
	[3] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "event",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, event),
	},
	[4] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "info",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, info),
	},
	[5] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "status",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, status),
	},
	[6] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "u2",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, u2),
	},
	[7] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "if_name",
		.size = sizeof(char) * IFNAMSIZ,
		.offset = __builtin_offsetof(struct _avm_event_wlan, if_name),
	},
	[8] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "ev_initiator",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, ev_initiator),
	},
	[9] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "ev_reason",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, ev_reason),
	},
	[10] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "avm_capabilities",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, avm_capabilities),
	},
	[11] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = 0,
		.name = "u3",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _avm_event_wlan, u3),
	},
	[12] = {
		.struct_name =  "convert_message_struct__avm_event_wlan",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "credentials",
		.substruct = convert_message_struct_avm_event_wlan_credentials,
		.offset = __builtin_offsetof(struct _avm_event_wlan, credentials),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct _cpmac_event_struct'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct__cpmac_event_struct[] = {
	[0] = {
		.struct_name =  "convert_message_struct__cpmac_event_struct",
		.flags = 0,
		.name = "ports",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct _cpmac_event_struct, ports),
	},
	[1] = {
		.struct_name =  "convert_message_struct__cpmac_event_struct",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "port",
		.substruct = convert_message_struct_cpmac_port,
		.offset = __builtin_offsetof(struct _cpmac_event_struct, port),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_ambient_brightness'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_ambient_brightness[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_ambient_brightness",
		.flags = 0,
		.name = "value",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_ambient_brightness, value),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_ambient_brightness",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "maxvalue",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_ambient_brightness, maxvalue),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_boykott'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_boykott[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_boykott",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy",
		.offset = __builtin_offsetof(struct avm_event_boykott, dummy),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_checkpoint'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_checkpoint[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_checkpoint",
		.flags = 0,
		.name = "node_id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_checkpoint, node_id),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_checkpoint",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "checkpoints",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_checkpoint, checkpoints),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_cpu_idle'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_cpu_idle[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = 0,
		.name = "cpu_idle",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, cpu_idle),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = 0,
		.name = "dsl_dsp_idle",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, dsl_dsp_idle),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = 0,
		.name = "voice_dsp_idle",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, voice_dsp_idle),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = 0,
		.name = "mem_strictlyused",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, mem_strictlyused),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = 0,
		.name = "mem_cacheused",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, mem_cacheused),
	},
	[5] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = 0,
		.name = "mem_physfree",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, mem_physfree),
	},
	[6] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_idle",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "cputype",
		.size = sizeof(enum _cputype),
		.offset = __builtin_offsetof(struct avm_event_cpu_idle, cputype),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__cputype,
		.enumName = "enum_table__cputype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_cpu_run'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_cpu_run[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_run",
		.flags = 0,
		.name = "cputype",
		.size = sizeof(enum _cputype),
		.offset = __builtin_offsetof(struct avm_event_cpu_run, cputype),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__cputype,
		.enumName = "enum_table__cputype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_cpu_run",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "cpu_run",
		.size = sizeof(unsigned char) * 4,
		.offset = __builtin_offsetof(struct avm_event_cpu_run, cpu_run),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_data'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_data[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_data",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct avm_event_data, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range__avm_event_id,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_data",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "data",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_data_union,
		.substruct = convert_message_union_avm_event_data_union,
		.offset = __builtin_offsetof(struct avm_event_data, data),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_fax_file'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_fax_file[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_fax_file",
		.flags = 0,
		.name = "action",
		.size = sizeof(enum fax_file_event_type),
		.offset = __builtin_offsetof(struct avm_event_fax_file, action),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_fax_file_event_type,
		.enumName = "enum_table_fax_file_event_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_fax_file",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "date",
		.offset = __builtin_offsetof(struct avm_event_fax_file, date),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_fax_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_fax_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_fax_status",
		.flags = 0,
		.name = "fax_receive_mode",
		.size = sizeof(enum fax_receive_mode),
		.offset = __builtin_offsetof(struct avm_event_fax_status, fax_receive_mode),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_fax_receive_mode,
		.enumName = "enum_table_fax_receive_mode"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_fax_status",
		.flags = 0,
		.name = "fax_storage_dest",
		.size = sizeof(enum fax_storage_dest),
		.offset = __builtin_offsetof(struct avm_event_fax_status, fax_storage_dest),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_fax_storage_dest,
		.enumName = "enum_table_fax_storage_dest"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_fax_status",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "dirname_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_fax_status, dirname_length),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_fax_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dirname",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_fax_status, dirname),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_firmware_update_available'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_firmware_update_available[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_firmware_update_available",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum avm_event_firmware_type),
		.offset = __builtin_offsetof(struct avm_event_firmware_update_available, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_firmware_type,
		.enumName = "enum_table_avm_event_firmware_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_firmware_update_available",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "version_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_firmware_update_available, version_length),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_firmware_update_available",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "version",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_firmware_update_available, version),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_internet_new_ip'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_internet_new_ip[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_internet_new_ip",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "sel",
		.size = sizeof(enum avm_event_internet_new_ip_param_sel),
		.offset = __builtin_offsetof(struct avm_event_internet_new_ip, sel),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_internet_new_ip_param_sel,
		.enumName = "enum_table_avm_event_internet_new_ip_param_sel"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_internet_new_ip_param_sel,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_internet_new_ip",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "params",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_internet_new_ip_param,
		.substruct = convert_message_union_avm_event_internet_new_ip_param,
		.offset = __builtin_offsetof(struct avm_event_internet_new_ip, params),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_led_info'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_led_info[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = 0,
		.name = "mode",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_info, mode),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = 0,
		.name = "param1",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_info, param1),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = 0,
		.name = "param2",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_info, param2),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = 0,
		.name = "gpio_driver_type",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_info, gpio_driver_type),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = 0,
		.name = "gpio",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_info, gpio),
	},
	[5] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = 0,
		.name = "pos",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_info, pos),
	},
	[6] = {
		.struct_name =  "convert_message_struct_avm_event_led_info",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(char) * 32,
		.offset = __builtin_offsetof(struct avm_event_led_info, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_led_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_led_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_led_status",
		.flags = 0,
		.name = "led",
		.size = sizeof(enum _led_event),
		.offset = __builtin_offsetof(struct avm_event_led_status, led),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__led_event,
		.enumName = "enum_table__led_event"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_led_status",
		.flags = 0,
		.name = "state",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_status, state),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_led_status",
		.flags = 0,
		.name = "param_len",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_led_status, param_len),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_led_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "params",
		.size = sizeof(unsigned char) * 245,
		.offset = __builtin_offsetof(struct avm_event_led_status, params),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_log'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_log[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_log",
		.flags = 0,
		.name = "logtype",
		.size = sizeof(enum _avm_logtype),
		.offset = __builtin_offsetof(struct avm_event_log, logtype),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_logtype,
		.enumName = "enum_table__avm_logtype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_log",
		.flags = 0,
		.name = "loglen",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_log, loglen),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_log",
		.flags = 0,
		.name = "logpointer",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_log, logpointer),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_log",
		.flags = 0,
		.name = "checksum",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_log, checksum),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_log",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "rebootflag",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_log, rebootflag),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_mass_storage_mount'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_mass_storage_mount[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_mass_storage_mount",
		.flags = 0,
		.name = "size",
		.size = sizeof(unsigned long long),
		.offset = __builtin_offsetof(struct avm_event_mass_storage_mount, size),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_mass_storage_mount",
		.flags = 0,
		.name = "free",
		.size = sizeof(unsigned long long),
		.offset = __builtin_offsetof(struct avm_event_mass_storage_mount, free),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_mass_storage_mount",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "name_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_mass_storage_mount, name_length),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_mass_storage_mount",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_mass_storage_mount, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_mass_storage_unmount'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_mass_storage_unmount[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_mass_storage_unmount",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "name_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_mass_storage_unmount, name_length),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_mass_storage_unmount",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_mass_storage_unmount, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_message'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_message[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = ENDIAN_CONVERT_TOTAL_LENGTH | 0,
		.name = "length",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_message, length),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = 0,
		.name = "magic",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_message, magic),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = 0,
		.name = "nonce",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_message, nonce),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = 0,
		.name = "flags",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_message, flags),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = 0,
		.name = "result",
		.size = sizeof(int32_t),
		.offset = __builtin_offsetof(struct avm_event_message, result),
	},
	[5] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = 0,
		.name = "transmitter_handle",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_message, transmitter_handle),
	},
	[6] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = 0,
		.name = "receiver_handle",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_message, receiver_handle),
	},
	[7] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "type",
		.size = sizeof(enum avm_event_msg_type),
		.offset = __builtin_offsetof(struct avm_event_message, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_msg_type,
		.enumName = "enum_table_avm_event_msg_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_msg_type,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[8] = {
		.struct_name =  "convert_message_struct_avm_event_message",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "message",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_message_union,
		.substruct = convert_message_union_avm_event_message_union,
		.offset = __builtin_offsetof(struct avm_event_message, message),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_piglet'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_piglet[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_piglet",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "type",
		.size = sizeof(enum _avm_piglettype),
		.offset = __builtin_offsetof(struct avm_event_piglet, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_piglettype,
		.enumName = "enum_table__avm_piglettype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_ping'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_ping[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_ping",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "seq",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_ping, seq),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_pm_info_stat'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_pm_info_stat[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "reserved1",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, reserved1),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_sumact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_sumact),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_sumcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_sumcum),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_systemact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_systemact),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_systemcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_systemcum),
	},
	[5] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "system_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, system_status),
	},
	[6] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dspact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_dspact),
	},
	[7] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dspcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_dspcum),
	},
	[8] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_wlanact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_wlanact),
	},
	[9] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_wlancum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_wlancum),
	},
	[10] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "wlan_devices",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, wlan_devices),
	},
	[11] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "wlan_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, wlan_status),
	},
	[12] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_ethact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_ethact),
	},
	[13] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_ethcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_ethcum),
	},
	[14] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "eth_status",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, eth_status),
	},
	[15] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_abact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_abact),
	},
	[16] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_abcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_abcum),
	},
	[17] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "isdn_status",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, isdn_status),
	},
	[18] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dectact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_dectact),
	},
	[19] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dectcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_dectcum),
	},
	[20] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_battchargeact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_battchargeact),
	},
	[21] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_battchargecum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_battchargecum),
	},
	[22] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "dect_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, dect_status),
	},
	[23] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_usbhostact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_usbhostact),
	},
	[24] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_usbhostcum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_usbhostcum),
	},
	[25] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "usb_status",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, usb_status),
	},
	[26] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "act_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, act_temperature),
	},
	[27] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "min_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, min_temperature),
	},
	[28] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "max_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, max_temperature),
	},
	[29] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "avg_temperature",
		.size = sizeof(signed char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, avg_temperature),
	},
	[30] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_lteact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_lteact),
	},
	[31] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_ltecum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_ltecum),
	},
	[32] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dvbcact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_dvbcact),
	},
	[33] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_dvbccum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_dvbccum),
	},
	[34] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = 0,
		.name = "rate_plcact",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_plcact),
	},
	[35] = {
		.struct_name =  "convert_message_struct_avm_event_pm_info_stat",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "rate_plccum",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_pm_info_stat, rate_plccum),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_powerline_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_powerline_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_powerline_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "status",
		.size = sizeof(enum ePLCState),
		.offset = __builtin_offsetof(struct avm_event_powerline_status, status),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_ePLCState,
		.enumName = "enum_table_ePLCState"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_powermanagment_remote'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_powermanagment_remote[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_powermanagment_remote",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "remote_action",
		.size = sizeof(enum avm_event_powermanagment_remote_action),
		.offset = __builtin_offsetof(struct avm_event_powermanagment_remote, remote_action),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_powermanagment_remote_action,
		.enumName = "enum_table_avm_event_powermanagment_remote_action"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_powermanagment_remote_action,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_powermanagment_remote",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_powermanagment_remote_union,
		.substruct = convert_message_union_avm_event_powermanagment_remote_union,
		.offset = __builtin_offsetof(struct avm_event_powermanagment_remote, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_powermanagment_remote_ressourceinfo'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_powermanagment_remote_ressourceinfo[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_powermanagment_remote_ressourceinfo",
		.flags = 0,
		.name = "device",
		.size = sizeof(enum _powermanagment_device),
		.offset = __builtin_offsetof(struct avm_event_powermanagment_remote_ressourceinfo, device),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__powermanagment_device,
		.enumName = "enum_table__powermanagment_device"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_powermanagment_remote_ressourceinfo",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "power_rate",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_powermanagment_remote_ressourceinfo, power_rate),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_powermanagment_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_powermanagment_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_powermanagment_status",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "substatus",
		.size = sizeof(enum _powermanagment_status_type),
		.offset = __builtin_offsetof(struct avm_event_powermanagment_status, substatus),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__powermanagment_status_type,
		.enumName = "enum_table__powermanagment_status_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range__powermanagment_status_type,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_powermanagment_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union___powermanagment_status_union,
		.substruct = convert_message_union___powermanagment_status_union,
		.offset = __builtin_offsetof(struct avm_event_powermanagment_status, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_push_button'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_push_button[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_push_button",
		.flags = 0,
		.name = "key",
		.size = sizeof(enum _avm_event_push_button_key),
		.offset = __builtin_offsetof(struct avm_event_push_button, key),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_push_button_key,
		.enumName = "enum_table__avm_event_push_button_key"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_push_button",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "pressed",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_push_button, pressed),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_remote_source_trigger_request'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_remote_source_trigger_request[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_remote_source_trigger_request",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "data",
		.substruct = convert_message_struct_avm_event_data,
		.offset = __builtin_offsetof(struct avm_event_remote_source_trigger_request, data),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_remotepcmlink'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_remotepcmlink[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_remotepcmlink",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum _avm_remotepcmlinktype),
		.offset = __builtin_offsetof(struct avm_event_remotepcmlink, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_remotepcmlinktype,
		.enumName = "enum_table__avm_remotepcmlinktype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_remotepcmlink",
		.flags = 0,
		.name = "sharedlen",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_remotepcmlink, sharedlen),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_remotepcmlink",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "sharedpointer",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_remotepcmlink, sharedpointer),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_remotewatchdog'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_remotewatchdog[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_remotewatchdog",
		.flags = 0,
		.name = "cmd",
		.size = sizeof(enum _avm_remote_wdt_cmd),
		.offset = __builtin_offsetof(struct avm_event_remotewatchdog, cmd),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_remote_wdt_cmd,
		.enumName = "enum_table__avm_remote_wdt_cmd"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_remotewatchdog",
		.flags = 0,
		.name = "name",
		.size = sizeof(char) * 16,
		.offset = __builtin_offsetof(struct avm_event_remotewatchdog, name),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_remotewatchdog",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "param",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_remotewatchdog, param),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_rpc'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_rpc[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_rpc",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum _avm_rpctype),
		.offset = __builtin_offsetof(struct avm_event_rpc, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_rpctype,
		.enumName = "enum_table__avm_rpctype"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_rpc",
		.flags = 0,
		.name = "id",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_rpc, id),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_rpc",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_rpc, length),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_rpc",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "message",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_rpc, message),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_smarthome'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_smarthome[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome",
		.flags = 0,
		.name = "length",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct avm_event_smarthome, length),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome",
		.flags = ENDIAN_CONVERT_ARRAY_USE | 0,
		.name = "ident",
		.size = sizeof(char) * 20,
		.offset = __builtin_offsetof(struct avm_event_smarthome, ident),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome",
		.flags = 0,
		.name = "type",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct avm_event_smarthome, type),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "payload",
		.offset = __builtin_offsetof(struct avm_event_smarthome, payload),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_smarthome_switch_status'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_smarthome_switch_status[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome_switch_status",
		.flags = 0,
		.name = "type",
		.size = sizeof(enum avm_event_switch_type),
		.offset = __builtin_offsetof(struct avm_event_smarthome_switch_status, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_switch_type,
		.enumName = "enum_table_avm_event_switch_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome_switch_status",
		.flags = 0,
		.name = "value",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_smarthome_switch_status, value),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome_switch_status",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "ain_length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_smarthome_switch_status, ain_length),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_smarthome_switch_status",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "ain",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_smarthome_switch_status, ain),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_source_notifier'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_source_notifier[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_source_notifier",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct avm_event_source_notifier, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_source_register'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_source_register[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_source_register",
		.flags = 0,
		.name = "id_mask",
		.substruct = convert_message_struct__avm_event_id_mask,
		.offset = __builtin_offsetof(struct avm_event_source_register, id_mask),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_source_register",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(char) * 32,
		.offset = __builtin_offsetof(struct avm_event_source_register, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_source_unregister'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_source_unregister[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_source_unregister",
		.flags = 0,
		.name = "id_mask",
		.substruct = convert_message_struct__avm_event_id_mask,
		.offset = __builtin_offsetof(struct avm_event_source_unregister, id_mask),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_source_unregister",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "name",
		.size = sizeof(char) * 32,
		.offset = __builtin_offsetof(struct avm_event_source_unregister, name),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_telefon_up'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_telefon_up[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_telefon_up",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy",
		.offset = __builtin_offsetof(struct avm_event_telefon_up, dummy),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_telefonprofile'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_telefonprofile[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_telefonprofile",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "on",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_telefonprofile, on),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_telephony_missed_call'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_telephony_missed_call[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_telephony_missed_call",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_telephony_missed_call, length),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_telephony_missed_call",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "p",
		.substruct = convert_message_struct__avm_event_telephony_missed_call_params,
		.offset = __builtin_offsetof(struct avm_event_telephony_missed_call, p),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_telephony_string'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_telephony_string[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_telephony_string",
		.flags = ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | 0,
		.name = "length",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_telephony_string, length),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_telephony_string",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_ARRAY_USE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "string",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_telephony_string, string),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_temperature'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_temperature[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_temperature",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "temperature",
		.size = sizeof(int),
		.offset = __builtin_offsetof(struct avm_event_temperature, temperature),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "src_id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, src_id),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "dst_id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, dst_id),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "seq_nr",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, seq_nr),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "ack",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, ack),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "srv_handle",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, srv_handle),
	},
	[5] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "clt_handle",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, clt_handle),
	},
	[6] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = 0,
		.name = "result",
		.size = sizeof(int32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs, result),
	},
	[7] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "type",
		.size = sizeof(enum avm_event_tffs_call_type),
		.offset = __builtin_offsetof(struct avm_event_tffs, type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_tffs_call_type,
		.enumName = "enum_table_avm_event_tffs_call_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_avm_event_tffs_call_type,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[8] = {
		.struct_name =  "convert_message_struct_avm_event_tffs",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "call",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_tffs_call_union,
		.substruct = convert_message_union_avm_event_tffs_call_union,
		.offset = __builtin_offsetof(struct avm_event_tffs, call),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_cleanup'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_cleanup[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_cleanup",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_cleanup, dummy),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_close'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_close[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_close",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_close, dummy),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_deinit'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_deinit[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_deinit",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_deinit, dummy),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_info'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_info[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_info",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "fill_level",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_info, fill_level),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_init'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_init[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_init",
		.flags = 0,
		.name = "mem_offset",
		.size = sizeof(int64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_init, mem_offset),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_init",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "max_seg_size",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_init, max_seg_size),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_notify'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_notify[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_notify",
		.flags = 0,
		.name = "id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_notify, id),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_notify",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "event",
		.size = sizeof(enum avm_event_tffs_notify_event),
		.offset = __builtin_offsetof(struct avm_event_tffs_notify, event),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_tffs_notify_event,
		.enumName = "enum_table_avm_event_tffs_notify_event"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_open'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_open[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_open",
		.flags = 0,
		.name = "id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_open, id),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_open",
		.flags = 0,
		.name = "mode",
		.size = sizeof(enum avm_event_tffs_open_mode),
		.offset = __builtin_offsetof(struct avm_event_tffs_open, mode),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_avm_event_tffs_open_mode,
		.enumName = "enum_table_avm_event_tffs_open_mode"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_open",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "max_segment_size",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_open, max_segment_size),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_paniclog'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_paniclog[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_paniclog",
		.flags = 0,
		.name = "buff_addr",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_paniclog, buff_addr),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_paniclog",
		.flags = 0,
		.name = "len",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_paniclog, len),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_paniclog",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "crc",
		.size = sizeof(int32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_paniclog, crc),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_read'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_read[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_read",
		.flags = 0,
		.name = "buff_addr",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_read, buff_addr),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_read",
		.flags = 0,
		.name = "len",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_read, len),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_read",
		.flags = 0,
		.name = "id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_read, id),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_read",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "crc",
		.size = sizeof(int32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_read, crc),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_reindex'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_reindex[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_reindex",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_reindex, dummy),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_tffs_write'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_tffs_write[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_write",
		.flags = 0,
		.name = "buff_addr",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_write, buff_addr),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_write",
		.flags = 0,
		.name = "len",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_write, len),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_write",
		.flags = 0,
		.name = "id",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_write, id),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_write",
		.flags = 0,
		.name = "final",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_write, final),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_tffs_write",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "crc",
		.size = sizeof(int32_t),
		.offset = __builtin_offsetof(struct avm_event_tffs_write, crc),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_unserialised'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_unserialised[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_unserialised",
		.flags = 0,
		.name = "evnt_id",
		.size = sizeof(uint64_t),
		.offset = __builtin_offsetof(struct avm_event_unserialised, evnt_id),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_unserialised",
		.flags = 0,
		.name = "data_len",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct avm_event_unserialised, data_len),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_unserialised",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "data",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(struct avm_event_unserialised, data),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_user_mode_source_notify'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_user_mode_source_notify[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_user_mode_source_notify",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "id",
		.size = sizeof(enum _avm_event_id),
		.offset = __builtin_offsetof(struct avm_event_user_mode_source_notify, id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_id,
		.enumName = "enum_table__avm_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_wlan'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_wlan[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "mac",
		.size = sizeof(char) * 6,
		.offset = __builtin_offsetof(struct avm_event_wlan, mac),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = ENDIAN_CONVERT_IGNORE | 0,
		.name = "u1",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_wlan_client_status_u1,
		.substruct = convert_message_union_avm_event_wlan_client_status_u1,
		.offset = __builtin_offsetof(struct avm_event_wlan, u1),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "event",
		.size = sizeof(wlan_event),
		.offset = __builtin_offsetof(struct avm_event_wlan, event),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "info",
		.size = sizeof(wlan_info),
		.offset = __builtin_offsetof(struct avm_event_wlan, info),
	},
	[4] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "status",
		.size = sizeof(enum wlan_sm_states),
		.offset = __builtin_offsetof(struct avm_event_wlan, status),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_wlan_sm_states,
		.enumName = "enum_table_wlan_sm_states"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[5] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = ENDIAN_CONVERT_IGNORE | 0,
		.name = "u2",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_wlan_client_status_u2,
		.substruct = convert_message_union_avm_event_wlan_client_status_u2,
		.offset = __builtin_offsetof(struct avm_event_wlan, u2),
	},
	[6] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "if_name",
		.size = sizeof(char) * IFNAMSIZ,
		.offset = __builtin_offsetof(struct avm_event_wlan, if_name),
	},
	[7] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "ev_initiator",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_wlan, ev_initiator),
	},
	[8] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "ev_reason",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_wlan, ev_reason),
	},
	[9] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = 0,
		.name = "avm_capabilities",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_wlan, avm_capabilities),
	},
	[10] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = ENDIAN_CONVERT_IGNORE | 0,
		.name = "u3",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_avm_event_wlan_client_status_u3,
		.substruct = convert_message_union_avm_event_wlan_client_status_u3,
		.offset = __builtin_offsetof(struct avm_event_wlan, u3),
	},
	[11] = {
		.struct_name =  "convert_message_struct_avm_event_wlan",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "credentials",
		.substruct = convert_message_struct_avm_event_wlan_credentials,
		.offset = __builtin_offsetof(struct avm_event_wlan, credentials),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_wlan_credentials'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_wlan_credentials[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_wlan_credentials",
		.flags = 0,
		.name = "ssid",
		.size = sizeof(char) * 33,
		.offset = __builtin_offsetof(struct avm_event_wlan_credentials, ssid),
	},
	[1] = {
		.struct_name =  "convert_message_struct_avm_event_wlan_credentials",
		.flags = 0,
		.name = "bssid",
		.size = sizeof(unsigned char) * 6,
		.offset = __builtin_offsetof(struct avm_event_wlan_credentials, bssid),
	},
	[2] = {
		.struct_name =  "convert_message_struct_avm_event_wlan_credentials",
		.flags = 0,
		.name = "key",
		.size = sizeof(char) * 65,
		.offset = __builtin_offsetof(struct avm_event_wlan_credentials, key),
	},
	[3] = {
		.struct_name =  "convert_message_struct_avm_event_wlan_credentials",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "security",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_wlan_credentials, security),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct avm_event_wlan_power'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_avm_event_wlan_power[] = {
	[0] = {
		.struct_name =  "convert_message_struct_avm_event_wlan_power",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "power",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct avm_event_wlan_power, power),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct cpmac_event_struct'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_cpmac_event_struct[] = {
	[0] = {
		.struct_name =  "convert_message_struct_cpmac_event_struct",
		.flags = 0,
		.name = "event_header",
		.substruct = convert_message_struct__avm_event_header,
		.offset = __builtin_offsetof(struct cpmac_event_struct, event_header),
	},
	[1] = {
		.struct_name =  "convert_message_struct_cpmac_event_struct",
		.flags = 0,
		.name = "ports",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(struct cpmac_event_struct, ports),
	},
	[2] = {
		.struct_name =  "convert_message_struct_cpmac_event_struct",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "port",
		.substruct = convert_message_struct_cpmac_port,
		.offset = __builtin_offsetof(struct cpmac_event_struct, port),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct cpmac_port'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_cpmac_port[] = {
	[0] = {
		.struct_name =  "convert_message_struct_cpmac_port",
		.flags = 0,
		.name = "cable",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct cpmac_port, cable),
	},
	[1] = {
		.struct_name =  "convert_message_struct_cpmac_port",
		.flags = 0,
		.name = "link",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct cpmac_port, link),
	},
	[2] = {
		.struct_name =  "convert_message_struct_cpmac_port",
		.flags = 0,
		.name = "speed100",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct cpmac_port, speed100),
	},
	[3] = {
		.struct_name =  "convert_message_struct_cpmac_port",
		.flags = 0,
		.name = "fullduplex",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct cpmac_port, fullduplex),
	},
	[4] = {
		.struct_name =  "convert_message_struct_cpmac_port",
		.flags = 0,
		.name = "speed",
		.size = sizeof(enum _avm_event_ethernet_speed),
		.offset = __builtin_offsetof(struct cpmac_port, speed),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_ethernet_speed,
		.enumName = "enum_table__avm_event_ethernet_speed"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[5] = {
		.struct_name =  "convert_message_struct_cpmac_port",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "maxspeed",
		.size = sizeof(enum _avm_event_ethernet_speed),
		.offset = __builtin_offsetof(struct cpmac_port, maxspeed),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table__avm_event_ethernet_speed,
		.enumName = "enum_table__avm_event_ethernet_speed"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_client_common'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_client_common[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_common",
		.flags = 0,
		.name = "mac",
		.size = sizeof(uint8_t) * 6,
		.offset = __builtin_offsetof(struct wlan_event_data_client_common, mac),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_common",
		.flags = 0,
		.name = "iface",
		.size = sizeof(char) * IFNAMSIZ + 1,
		.offset = __builtin_offsetof(struct wlan_event_data_client_common, iface),
	},
	[2] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_common",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "initiator",
		.size = sizeof(char) * 16 + 1,
		.offset = __builtin_offsetof(struct wlan_event_data_client_common, initiator),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_client_connect_info'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_client_connect_info[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_connect_info",
		.flags = 0,
		.name = "common",
		.substruct = convert_message_struct_wlan_event_data_client_common,
		.offset = __builtin_offsetof(struct wlan_event_data_client_connect_info, common),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_connect_info",
		.flags = 0,
		.name = "info_context",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct wlan_event_data_client_connect_info, info_context),
	},
	[2] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_connect_info",
		.flags = 0,
		.name = "reason",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct wlan_event_data_client_connect_info, reason),
	},
	[3] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_connect_info",
		.flags = 0,
		.name = "max_node_count",
		.size = sizeof(uint32_t),
		.offset = __builtin_offsetof(struct wlan_event_data_client_connect_info, max_node_count),
	},
	[4] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_connect_info",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "ieee80211_code",
		.size = sizeof(unsigned short),
		.offset = __builtin_offsetof(struct wlan_event_data_client_connect_info, ieee80211_code),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_client_state_change'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_client_state_change[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_state_change",
		.flags = 0,
		.name = "common",
		.substruct = convert_message_struct_wlan_event_data_client_common,
		.offset = __builtin_offsetof(struct wlan_event_data_client_state_change, common),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_state_change",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "state",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct wlan_event_data_client_state_change, state),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_client_state_idle'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_client_state_idle[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_state_idle",
		.flags = 0,
		.name = "common",
		.substruct = convert_message_struct_wlan_event_data_client_common,
		.offset = __builtin_offsetof(struct wlan_event_data_client_state_idle, common),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_client_state_idle",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "idle",
		.size = sizeof(uint8_t),
		.offset = __builtin_offsetof(struct wlan_event_data_client_state_idle, idle),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_radio_recovery'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_radio_recovery[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_radio_recovery",
		.flags = 0,
		.name = "radio_id",
		.size = sizeof(int),
		.offset = __builtin_offsetof(struct wlan_event_data_radio_recovery, radio_id),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_radio_recovery",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "state",
		.size = sizeof(enum wlan_event_radio_recovery_state),
		.offset = __builtin_offsetof(struct wlan_event_data_radio_recovery, state),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_wlan_event_radio_recovery_state,
		.enumName = "enum_table_wlan_event_radio_recovery_state"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_scan_common'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_scan_common[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_scan_common",
		.flags = 0,
		.name = "iface",
		.size = sizeof(char) * IFNAMSIZ + 1,
		.offset = __builtin_offsetof(struct wlan_event_data_scan_common, iface),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_scan_common",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "initiator",
		.size = sizeof(char) * 16 + 1,
		.offset = __builtin_offsetof(struct wlan_event_data_scan_common, initiator),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_data_scan_event_info'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_data_scan_event_info[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_data_scan_event_info",
		.flags = 0,
		.name = "common",
		.substruct = convert_message_struct_wlan_event_data_scan_common,
		.offset = __builtin_offsetof(struct wlan_event_data_scan_event_info, common),
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_data_scan_event_info",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "event_type",
		.size = sizeof(enum wlan_event_scan_type),
		.offset = __builtin_offsetof(struct wlan_event_data_scan_event_info, event_type),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_wlan_event_scan_type,
		.enumName = "enum_table_wlan_event_scan_type"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = NULL // enum field, but possibly a mask, so not check function,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'struct wlan_event_def'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_struct_wlan_event_def[] = {
	[0] = {
		.struct_name =  "convert_message_struct_wlan_event_def",
		.flags = ENDIAN_CONVERT_SELECT | 0,
		.name = "event_id",
		.size = sizeof(enum wlan_event_id),
		.offset = __builtin_offsetof(struct wlan_event_def, event_id),
#ifdef WIRESHARK_PLUGIN
		.enumInfo = enum_table_wlan_event_id,
		.enumName = "enum_table_wlan_event_id"
#else /*--- #ifdef WIRESHARK_PLUGIN ---*/
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))avm_event_check_enum_range_wlan_event_id,
#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/
	},
	[1] = {
		.struct_name =  "convert_message_struct_wlan_event_def",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_SELECT_ENTRY | ENDIAN_CONVERT_IGNORE | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "event_data",
		.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))check_enum_for_union_wlan_event_data,
		.substruct = convert_message_union_wlan_event_data,
		.offset = __builtin_offsetof(struct wlan_event_def, event_data),
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union __powermanagment_status_union'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union___powermanagment_status_union[] = {
	[dsl_status] = {
		.struct_name =  "convert_message_union___powermanagment_status_union",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dsl_status",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union __powermanagment_status_union, dsl_status)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union _avm_event_cmd_param'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union__avm_event_cmd_param[] = {
	[avm_event_cmd_register] = {
		.struct_name =  "convert_message_union__avm_event_cmd_param",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "avm_event_cmd_param_register",
		.substruct = convert_message_struct__avm_event_cmd_param_register,
		.offset = __builtin_offsetof(union _avm_event_cmd_param, avm_event_cmd_param_register)
	},
	[avm_event_cmd_release] = {
		.struct_name =  "convert_message_union__avm_event_cmd_param",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "avm_event_cmd_param_release",
		.substruct = convert_message_struct__avm_event_cmd_param_release,
		.offset = __builtin_offsetof(union _avm_event_cmd_param, avm_event_cmd_param_release)
	},
	[avm_event_cmd_trigger] = {
		.struct_name =  "convert_message_union__avm_event_cmd_param",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "avm_event_cmd_param_trigger",
		.substruct = convert_message_struct__avm_event_cmd_param_trigger,
		.offset = __builtin_offsetof(union _avm_event_cmd_param, avm_event_cmd_param_trigger)
	},
	[avm_event_cmd_source_register] = {
		.struct_name =  "convert_message_union__avm_event_cmd_param",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "avm_event_cmd_param_source_register",
		.substruct = convert_message_struct__avm_event_cmd_param_register,
		.offset = __builtin_offsetof(union _avm_event_cmd_param, avm_event_cmd_param_source_register)
	},
	[avm_event_cmd_source_trigger] = {
		.struct_name =  "convert_message_union__avm_event_cmd_param",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "avm_event_cmd_param_source_trigger",
		.substruct = convert_message_struct__avm_event_cmd_param_source_trigger,
		.offset = __builtin_offsetof(union _avm_event_cmd_param, avm_event_cmd_param_source_trigger)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_data_union'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_data_union[] = {
	[avm_event_id_appl_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "appl_status",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, appl_status)
	},
	[avm_event_id_autoprov] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "autoprov",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, autoprov)
	},
	[avm_event_id_capiotcp_startstop] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "capiotcp_startstop",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, capiotcp_startstop)
	},
	[avm_event_id_dsl_connect_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "dsl_connect_status",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, dsl_connect_status)
	},
	[avm_event_id_dsl_get] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "dsl_get",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, dsl_get)
	},
	[avm_event_id_dsl_get_arch] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "dsl_get_arch",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, dsl_get_arch)
	},
	[avm_event_id_dsl_set] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "dsl_set",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, dsl_set)
	},
	[avm_event_id_dsl_set_arch] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "dsl_set_arch",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, dsl_set_arch)
	},
	[avm_event_id_dsl_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "dsl_status",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, dsl_status)
	},
	[avm_event_id_powerline_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "powerline_status",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, powerline_status)
	},
	[avm_event_id_reboot_req] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "reboot_req",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, reboot_req)
	},
	[avm_event_id_telefon_wlan_command] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telefon_wlan_command",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, telefon_wlan_command)
	},
	[avm_event_id_usb_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "usb_status",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, usb_status)
	},
	[avm_event_id_user_source_notify] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "user_source_notify",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, user_source_notify)
	},
	[avm_event_id_push_button] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "push_button",
		.substruct = convert_message_struct_avm_event_push_button,
		.offset = __builtin_offsetof(union avm_event_data_union, push_button)
	},
	[avm_event_id_ethernet_connect_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "cpmac",
		.substruct = convert_message_struct__cpmac_event_struct,
		.offset = __builtin_offsetof(union avm_event_data_union, cpmac)
	},
	[avm_event_id_led_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "led_status",
		.substruct = convert_message_struct_avm_event_led_status,
		.offset = __builtin_offsetof(union avm_event_data_union, led_status)
	},
	[avm_event_id_led_info] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "led_info",
		.substruct = convert_message_struct_avm_event_led_info,
		.offset = __builtin_offsetof(union avm_event_data_union, led_info)
	},
	[avm_event_id_telefonprofile] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telefonprofile",
		.substruct = convert_message_struct_avm_event_telefonprofile,
		.offset = __builtin_offsetof(union avm_event_data_union, telefonprofile)
	},
	[avm_event_id_temperature] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "temperature",
		.substruct = convert_message_struct_avm_event_temperature,
		.offset = __builtin_offsetof(union avm_event_data_union, temperature)
	},
	[avm_event_id_powermanagment_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "powermanagment_status",
		.substruct = convert_message_struct_avm_event_powermanagment_status,
		.offset = __builtin_offsetof(union avm_event_data_union, powermanagment_status)
	},
	[avm_event_id_cpu_idle] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "cpu_idle",
		.substruct = convert_message_struct_avm_event_cpu_idle,
		.offset = __builtin_offsetof(union avm_event_data_union, cpu_idle)
	},
	[avm_event_id_powermanagment_remote] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "powermanagment_remote",
		.substruct = convert_message_struct_avm_event_powermanagment_remote,
		.offset = __builtin_offsetof(union avm_event_data_union, powermanagment_remote)
	},
	[avm_event_id_log] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "id_log",
		.substruct = convert_message_struct_avm_event_log,
		.offset = __builtin_offsetof(union avm_event_data_union, id_log)
	},
	[avm_event_id_remotepcmlink] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "id_remotepcmlink",
		.substruct = convert_message_struct_avm_event_remotepcmlink,
		.offset = __builtin_offsetof(union avm_event_data_union, id_remotepcmlink)
	},
	[avm_event_id_remotewatchdog] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "id_remotewatchdog",
		.substruct = convert_message_struct_avm_event_remotewatchdog,
		.offset = __builtin_offsetof(union avm_event_data_union, id_remotewatchdog)
	},
	[avm_event_id_rpc] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "id_rpc",
		.substruct = convert_message_struct_avm_event_rpc,
		.offset = __builtin_offsetof(union avm_event_data_union, id_rpc)
	},
	[avm_event_id_pm_ressourceinfo_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "pm_info_stat",
		.substruct = convert_message_struct_avm_event_pm_info_stat,
		.offset = __builtin_offsetof(union avm_event_data_union, pm_info_stat)
	},
	[avm_event_id_wlan_client_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "wlan",
		.substruct = convert_message_struct_avm_event_wlan,
		.offset = __builtin_offsetof(union avm_event_data_union, wlan)
	},
	[avm_event_id_wlan_power] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "wlan_power",
		.substruct = convert_message_struct_avm_event_wlan_power,
		.offset = __builtin_offsetof(union avm_event_data_union, wlan_power)
	},
	[avm_event_id_wlan_event] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "wlan_event",
		.substruct = convert_message_struct_wlan_event_def,
		.offset = __builtin_offsetof(union avm_event_data_union, wlan_event)
	},
	[avm_event_id_telephony_missed_call] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telephony_missed_call",
		.substruct = convert_message_struct_avm_event_telephony_missed_call,
		.offset = __builtin_offsetof(union avm_event_data_union, telephony_missed_call)
	},
	[avm_event_id_telephony_tam_call] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telephony_tam_call",
		.substruct = convert_message_struct_avm_event_telephony_missed_call,
		.offset = __builtin_offsetof(union avm_event_data_union, telephony_tam_call)
	},
	[avm_event_id_telephony_fax_received] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telephony_fax_received",
		.substruct = convert_message_struct_avm_event_telephony_missed_call,
		.offset = __builtin_offsetof(union avm_event_data_union, telephony_fax_received)
	},
	[avm_event_id_telephony_incoming_call] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telephony_incoming_call",
		.substruct = convert_message_struct_avm_event_telephony_missed_call,
		.offset = __builtin_offsetof(union avm_event_data_union, telephony_incoming_call)
	},
	[avm_event_id_telephony_call_finished] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telephony_call_finished",
		.substruct = convert_message_struct_avm_event_telephony_missed_call,
		.offset = __builtin_offsetof(union avm_event_data_union, telephony_call_finished)
	},
	[avm_event_id_firmware_update_available] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "firmware_update_available",
		.substruct = convert_message_struct_avm_event_firmware_update_available,
		.offset = __builtin_offsetof(union avm_event_data_union, firmware_update_available)
	},
	[avm_event_id_internet_new_ip] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "internet_new_ip",
		.substruct = convert_message_struct_avm_event_internet_new_ip,
		.offset = __builtin_offsetof(union avm_event_data_union, internet_new_ip)
	},
	[avm_event_id_smarthome_switch_status] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "smarthome_switch_status",
		.substruct = convert_message_struct_avm_event_smarthome_switch_status,
		.offset = __builtin_offsetof(union avm_event_data_union, smarthome_switch_status)
	},
	[avm_event_id_mass_storage_mount] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "mass_storage_mount",
		.substruct = convert_message_struct_avm_event_mass_storage_mount,
		.offset = __builtin_offsetof(union avm_event_data_union, mass_storage_mount)
	},
	[avm_event_id_mass_storage_unmount] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "mass_storage_unmount",
		.substruct = convert_message_struct_avm_event_mass_storage_unmount,
		.offset = __builtin_offsetof(union avm_event_data_union, mass_storage_unmount)
	},
	[avm_event_id_checkpoint] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "checkpoint",
		.substruct = convert_message_struct_avm_event_checkpoint,
		.offset = __builtin_offsetof(union avm_event_data_union, checkpoint)
	},
	[avm_event_id_cpu_run] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "cpu_run",
		.substruct = convert_message_struct_avm_event_cpu_run,
		.offset = __builtin_offsetof(union avm_event_data_union, cpu_run)
	},
	[avm_event_id_ambient_brightness] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "ambient_brightness",
		.substruct = convert_message_struct_avm_event_ambient_brightness,
		.offset = __builtin_offsetof(union avm_event_data_union, ambient_brightness)
	},
	[avm_event_id_fax_status_change] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "fax_status",
		.substruct = convert_message_struct_avm_event_fax_status,
		.offset = __builtin_offsetof(union avm_event_data_union, fax_status)
	},
	[avm_event_id_fax_file] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "fax_file",
		.substruct = convert_message_struct_avm_event_fax_file,
		.offset = __builtin_offsetof(union avm_event_data_union, fax_file)
	},
	[avm_event_id_piglet] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "piglet",
		.substruct = convert_message_struct_avm_event_piglet,
		.offset = __builtin_offsetof(union avm_event_data_union, piglet)
	},
	[avm_event_id_telefon_up] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "telefon_up",
		.substruct = convert_message_struct_avm_event_telefon_up,
		.offset = __builtin_offsetof(union avm_event_data_union, telefon_up)
	},
	[avm_event_id_smarthome] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "smarthome",
		.substruct = convert_message_struct_avm_event_smarthome,
		.offset = __builtin_offsetof(union avm_event_data_union, smarthome)
	},
	[avm_event_id_fiber_set_arch] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "fiber_set_arch",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, fiber_set_arch)
	},
	[avm_event_id_fiber_get_arch] = {
		.struct_name =  "convert_message_union_avm_event_data_union",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "fiber_get_arch",
		.substruct = convert_message_struct_avm_event_boykott,
		.offset = __builtin_offsetof(union avm_event_data_union, fiber_get_arch)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_internet_new_ip_param'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_internet_new_ip_param[] = {
	[avm_event_internet_new_ip_v4] = {
		.struct_name =  "convert_message_union_avm_event_internet_new_ip_param",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "ipv4",
		.size = sizeof(unsigned char) * 4,
		.offset = __builtin_offsetof(union avm_event_internet_new_ip_param, ipv4)
	},
	[avm_event_internet_new_ip_v6] = {
		.struct_name =  "convert_message_union_avm_event_internet_new_ip_param",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "ipv6",
		.size = sizeof(unsigned char) * 16,
		.offset = __builtin_offsetof(union avm_event_internet_new_ip_param, ipv6)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_message_union'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_message_union[] = {
	[avm_event_source_register_type] = {
		.struct_name =  "convert_message_union_avm_event_message_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "source_register",
		.substruct = convert_message_struct_avm_event_source_register,
		.offset = __builtin_offsetof(union avm_event_message_union, source_register)
	},
	[avm_event_source_unregister_type] = {
		.struct_name =  "convert_message_union_avm_event_message_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "source_unregister",
		.substruct = convert_message_struct_avm_event_source_unregister,
		.offset = __builtin_offsetof(union avm_event_message_union, source_unregister)
	},
	[avm_event_source_notifier_type] = {
		.struct_name =  "convert_message_union_avm_event_message_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "source_notifier",
		.substruct = convert_message_struct_avm_event_source_notifier,
		.offset = __builtin_offsetof(union avm_event_message_union, source_notifier)
	},
	[avm_event_remote_source_trigger_request_type] = {
		.struct_name =  "convert_message_union_avm_event_message_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "remote_source_trigger_request",
		.substruct = convert_message_struct_avm_event_remote_source_trigger_request,
		.offset = __builtin_offsetof(union avm_event_message_union, remote_source_trigger_request)
	},
	[avm_event_ping_type] = {
		.struct_name =  "convert_message_union_avm_event_message_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "ping",
		.substruct = convert_message_struct_avm_event_ping,
		.offset = __builtin_offsetof(union avm_event_message_union, ping)
	},
	[avm_event_tffs_type] = {
		.struct_name =  "convert_message_union_avm_event_message_union",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "tffs",
		.substruct = convert_message_struct_avm_event_tffs,
		.offset = __builtin_offsetof(union avm_event_message_union, tffs)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_powermanagment_remote_union'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_powermanagment_remote_union[] = {
	[avm_event_powermanagment_ressourceinfo] = {
		.struct_name =  "convert_message_union_avm_event_powermanagment_remote_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "ressourceinfo",
		.substruct = convert_message_struct_avm_event_powermanagment_remote_ressourceinfo,
		.offset = __builtin_offsetof(union avm_event_powermanagment_remote_union, ressourceinfo)
	},
	[avm_event_powermanagment_activatepowermode] = {
		.struct_name =  "convert_message_union_avm_event_powermanagment_remote_union",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "powermode",
		.size = sizeof(char) * 32,
		.offset = __builtin_offsetof(union avm_event_powermanagment_remote_union, powermode)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_telephony_call_params'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_telephony_call_params[] = {
	[avm_event_telephony_params_name] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "string_avm_event_telephony_params_name",
		.substruct = convert_message_struct_avm_event_telephony_string,
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, string)
	},
	[avm_event_telephony_params_msn_name] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "string_avm_event_telephony_params_msn_name",
		.substruct = convert_message_struct_avm_event_telephony_string,
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, string)
	},
	[avm_event_telephony_params_portname] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "string_avm_event_telephony_params_portname",
		.substruct = convert_message_struct_avm_event_telephony_string,
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, string)
	},
	[avm_event_telephony_params_tam_path] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "string_avm_event_telephony_params_tam_path",
		.substruct = convert_message_struct_avm_event_telephony_string,
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, string)
	},
	[avm_event_telephony_params_calling] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "number",
		.size = sizeof(unsigned char) * 32,
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, number)
	},
	[avm_event_telephony_params_called] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "number",
		.size = sizeof(unsigned char) * 32,
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, number)
	},
	[avm_event_telephony_params_duration] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "duration",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, duration)
	},
	[avm_event_telephony_params_port] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "port",
		.size = sizeof(unsigned char),
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, port)
	},
	[avm_event_telephony_params_id] = {
		.struct_name =  "convert_message_union_avm_event_telephony_call_params",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "id",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_telephony_call_params, id)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_tffs_call_union'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_tffs_call_union[] = {
	[avm_event_tffs_call_open] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "open",
		.substruct = convert_message_struct_avm_event_tffs_open,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, open)
	},
	[avm_event_tffs_call_close] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "close",
		.substruct = convert_message_struct_avm_event_tffs_close,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, close)
	},
	[avm_event_tffs_call_read] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "read",
		.substruct = convert_message_struct_avm_event_tffs_read,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, read)
	},
	[avm_event_tffs_call_write] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "write",
		.substruct = convert_message_struct_avm_event_tffs_write,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, write)
	},
	[avm_event_tffs_call_cleanup] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "cleanup",
		.substruct = convert_message_struct_avm_event_tffs_cleanup,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, cleanup)
	},
	[avm_event_tffs_call_reindex] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "reindex",
		.substruct = convert_message_struct_avm_event_tffs_reindex,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, reindex)
	},
	[avm_event_tffs_call_info] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "info",
		.substruct = convert_message_struct_avm_event_tffs_info,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, info)
	},
	[avm_event_tffs_call_init] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "init",
		.substruct = convert_message_struct_avm_event_tffs_init,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, init)
	},
	[avm_event_tffs_call_deinit] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "deinit",
		.substruct = convert_message_struct_avm_event_tffs_deinit,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, deinit)
	},
	[avm_event_tffs_call_notify] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "notify",
		.substruct = convert_message_struct_avm_event_tffs_notify,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, notify)
	},
	[avm_event_tffs_call_paniclog] = {
		.struct_name =  "convert_message_union_avm_event_tffs_call_union",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "paniclog",
		.substruct = convert_message_struct_avm_event_tffs_paniclog,
		.offset = __builtin_offsetof(union avm_event_tffs_call_union, paniclog)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_wlan_client_status_u1'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_wlan_client_status_u1[] = {
	[INPUT_RADAR_DFS_WAIT] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "sub_event",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, sub_event)
	},
	[INPUT_MAC_AUTHORIZE] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate)
	},
	[INPUT_EAP_AUTHORIZED] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate1",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate1)
	},
	[INPUT_MADWIFI_WRONG_PSK] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate2",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate2)
	},
	[INPUT_AUTH_EXPIRED] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate3",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate3)
	},
	[INPUT_STA] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate4",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate4)
	},
	[INPUT_WDS_LINK_UP] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate5",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate5)
	},
	[INPUT_WDS_LINK_DOWN] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "active_rate6",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, active_rate6)
	},
	[INPUT_RADAR] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "radar_chan",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, radar_chan)
	},
	[INPUT_RADAR_DFS_WAIT_PRECAC] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "radar_chan1",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, radar_chan1)
	},
	[INPUT_GREENAP_PS] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "green_ap_ps_state",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, green_ap_ps_state)
	},
	[INPUT_COEXIST_SWITCH] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "coexist_ht40_state",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, coexist_ht40_state)
	},
	[INPUT_MAX_NODE_REACHED] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "max_node_count",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, max_node_count)
	},
	[INPUT_INTERFERENCE_CHAN_CHANGE] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "channel",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, channel)
	},
	[INPUT_AUTH_1_OS_A] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u1",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy0",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u1, dummy0)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_wlan_client_status_u2'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_wlan_client_status_u2[] = {
	[INPUT_RADAR] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u2",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "radar_freq",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u2, radar_freq)
	},
	[INPUT_RADAR_DFS_WAIT_PRECAC] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u2",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "radar_freq1",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u2, radar_freq1)
	},
	[INPUT_AUTH_1_D] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u2",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "wlan_mode",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u2, wlan_mode)
	},
	[INPUT_AUTH_1_OS_A] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u2",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy0",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u2, dummy0)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union avm_event_wlan_client_status_u3'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_avm_event_wlan_client_status_u3[] = {
	[INPUT_RADAR_DFS_WAIT] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u3",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "radar_time",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u3, radar_time)
	},
	[INPUT_RADAR] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u3",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "flags",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u3, flags)
	},
	[INPUT_AUTH_1_OS_A] = {
		.struct_name =  "convert_message_union_avm_event_wlan_client_status_u3",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "dummy0",
		.size = sizeof(unsigned int),
		.offset = __builtin_offsetof(union avm_event_wlan_client_status_u3, dummy0)
	},
};

/*------------------------------------------------------------------------------------------*\
 * Endian Convert-Table for: 'union wlan_event_data'
\*------------------------------------------------------------------------------------------*/
struct _endian_convert convert_message_union_wlan_event_data[] = {
	[CLIENT_STATE_CHANGE] = {
		.struct_name =  "convert_message_union_wlan_event_data",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "client_state_change",
		.substruct = convert_message_struct_wlan_event_data_client_state_change,
		.offset = __builtin_offsetof(union wlan_event_data, client_state_change)
	},
	[CLIENT_CONNECT_INFO] = {
		.struct_name =  "convert_message_union_wlan_event_data",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "client_connect_info",
		.substruct = convert_message_struct_wlan_event_data_client_connect_info,
		.offset = __builtin_offsetof(union wlan_event_data, client_connect_info)
	},
	[WLAN_EVENT_SCAN] = {
		.struct_name =  "convert_message_union_wlan_event_data",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "scan_event_info",
		.substruct = convert_message_struct_wlan_event_data_scan_event_info,
		.offset = __builtin_offsetof(union wlan_event_data, scan_event_info)
	},
	[CLIENT_STATE_IDLE] = {
		.struct_name =  "convert_message_union_wlan_event_data",
		.flags = ENDIAN_CONVERT_LAST | 0,
		.name = "client_state_idle",
		.substruct = convert_message_struct_wlan_event_data_client_state_idle,
		.offset = __builtin_offsetof(union wlan_event_data, client_state_idle)
	},
	[WLAN_RADIO_RECOVERY] = {
		.struct_name =  "convert_message_union_wlan_event_data",
		.flags = ENDIAN_CONVERT_LAST | ENDIAN_CONVERT_LAST_ARRAY_ITEM | 0,
		.name = "radio_recovery",
		.substruct = convert_message_struct_wlan_event_data_radio_recovery,
		.offset = __builtin_offsetof(union wlan_event_data, radio_recovery)
	},
};
