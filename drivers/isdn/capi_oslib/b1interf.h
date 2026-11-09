/*-------------------------------------------------##########################*\
 * Modul : b1interf.h  created at  Thu  27-Apr-1995  15:00:43 by jochen
 * $Id: b1interf.h 1.1 2001/07/04 15:34:14Z MPommerenke Exp $
 * $Log: b1interf.h $
 * Revision 1.1  2001/07/04 15:34:14Z  MPommerenke
 * Initial revision
 * Revision 1.1  2001/07/04 11:05:15Z  MPommerenke
 * Initial revision
 * Revision 1.1  2001/06/11 12:48:01Z  MPommerenke
 * Initial revision
 * Revision 1.5  2000/11/14 12:05:26Z  MPommerenke
 * Revision 1.4  2000/10/25 12:25:52Z  MPommerenke
 * - MaPom: link Erweiterung f¸r Power down
 * Revision 1.3  2000/08/24 11:37:33Z  MPommerenke
 * - MaPom: Erweitrungen f¸r power down
 * Revision 1.2  1999/11/23 11:48:07Z  MPommerenke
 * - MaPom: neue oslib
 * Revision 1.1  1999/07/23 08:38:21Z  MPommerenke
 * Initial revision
 * Revision 1.2  1998/12/09 16:03:44  DFriedel
 * - Diego: b1_ack_dword_align hinzugefÅgt
 * Revision 1.1  1998/11/10 13:43:11  DFriedel
 * Initial revision
 * Revision 1.1  1997/04/08 17:00:38  JSchieberlein.sw.avm
 * Initial revision
 * Revision 1.2  1995/12/13 11:01:03  JSchieberlein.sw.avm
 * CAPI20, neuer Release Mechanismus
 * Revision 1.1  1995/05/10 13:35:17  JOSH
 * Initial revision
 * Revision 1.1  95/04/27 15:10:14  jochen
 * Initial revision
\*---------------------------------------------------------------------------*/
#ifndef _B1INTERF_H
#define _B1INTERF_H

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
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


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
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
