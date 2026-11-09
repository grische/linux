/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _B1INTERF_H
#define _B1INTERF_H

enum _sendcmd {
	send_init                   = 0x11,
	send_register               = 0x12,
	send_data_b3_req            = 0x13,
	send_release                = 0x14,
	send_message                = 0x15,
	send_deinstall              = 0x16,
	send_debug_signal           = 0x17,
	send_task_message           = 0x18,
	send_task_order_msg         = 0x19,
	send_config                 = 0x21,
	send_poll                   = 0x72,
	send_startstop_ack          = 0x73,

	send_query_config           = 0x22,
	send_rpc_function_start     = 0x41,
	send_rpc_function_data      = 0x42
};

enum _receivecmd {
	receive_message             = 0x21,
	receive_data_b3_ind         = 0x22,
	receive_start               = 0x23,
	receive_stop                = 0x24,
	receive_new_ncci            = 0x25,
	receive_free_ncci           = 0x26,
	receive_version             = 0x27,
	receive_debug_message       = 0x28,
	receive_task_message        = 0x29,
	receive_task_message_got    = 0x30,
	receive_task_init           = 0x31,
	receive_poll                = 0x32,
	receive_puts                = 0x71,
	receive_ack_dword_align     = 0x75,

	receive_get_config          = 0x40,
	receive_rpc_function_start  = 0x41,
	receive_rpc_function_data   = 0x42,
	receive_rpc_event           = 0x43
};
#endif
