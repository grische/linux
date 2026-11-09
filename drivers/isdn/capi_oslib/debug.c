#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#pragma GCC pop_options

#include "debug.h"
#include <avm/capi_oslib/oslib.h>
#include <avm/capi_oslib/new.h>
#include "appl.h"
#include "host.h"
#include "capi_oslib.h"
#include "local_capi.h"
#include "capi_pipe.h"
#include "capi_oslib.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(OSLIB_DEBUG)
char *CAPI_MESSAGE_NAME(unsigned char Command, unsigned char SubCommand) {
	switch(SubCommand) {
		case CAPI_REQ:
			switch(Command) {
				case CAPI_ALERT:                return "[ALERT_REQ]";
				case CAPI_CONNECT:              return "[CONNECT_REQ]";
				case CAPI_CONNECT_ACTIVE:       return "[CONNECT_ACTIVE_REQ]";
				case CAPI_INTEROPERABILITY:     return "[INTEROPERABILITY_REQ]";
				case CAPI_CONNECT_B3_ACTIVE:    return "[CONNECT_B3_ACTIVE_REQ]";
				case CAPI_CONNECT_B3:           return "[CONNECT_B3_REQ]";
				case CAPI_CONNECT_B3_T90_ACTIVE:return "[CONNECT_B3_T90_ACTIVE_REQ]";
				case CAPI_DATA_B3:              return "[DATA_B3_REQ]";
				case CAPI_DISCONNECT_B3:        return "[DISCONNECT_B3_REQ]";
				case CAPI_DISCONNECT:           return "[DISCONNECT_REQ]";
				case CAPI_FACILITY:             return "[FACILITY_REQ]";
				case CAPI_INFO:                 return "[INFO_REQ]";
				case CAPI_LISTEN:               return "[LISTEN_REQ]";
				case CAPI_MANUFACTURER:         return "[MANUFACTURER_REQ]";
				case CAPI_RESET_B3:             return "[RESET_B3_REQ]";
				case CAPI_SELECT_B_PROTOCOL:    return "[SELECT_B_PROTOCOL_REQ]";
				case CAPI_REMOTE_REGISTER:      return "[REGISTER_REQ]";
				case CAPI_REMOTE_RELEASE:       return "[RELEASE_REQ]";
			}
			break;
		case CAPI_IND:
			switch(Command) {
				case CAPI_ALERT:                return "[ALERT_IND]";
				case CAPI_CONNECT:              return "[CONNECT_IND]";
				case CAPI_CONNECT_ACTIVE:       return "[CONNECT_ACTIVE_IND]";
				case CAPI_INTEROPERABILITY:     return "[INTEROPERABILITY_IND]";
				case CAPI_CONNECT_B3_ACTIVE:    return "[CONNECT_B3_ACTIVE_IND]";
				case CAPI_CONNECT_B3:           return "[CONNECT_B3_IND]";
				case CAPI_CONNECT_B3_T90_ACTIVE:return "[CONNECT_B3_T90_ACTIVE_IND]";
				case CAPI_DATA_B3:              return "[DATA_B3_IND]";
				case CAPI_DISCONNECT_B3:        return "[DISCONNECT_B3_IND]";
				case CAPI_DISCONNECT:           return "[DISCONNECT_IND]";
				case CAPI_FACILITY:             return "[FACILITY_IND]";
				case CAPI_INFO:                 return "[INFO_IND]";
				case CAPI_LISTEN:               return "[LISTEN_IND]";
				case CAPI_MANUFACTURER:         return "[MANUFACTURER_IND]";
				case CAPI_RESET_B3:             return "[RESET_B3_IND]";
				case CAPI_SELECT_B_PROTOCOL:    return "[SELECT_B_PROTOCOL_IND]";
				case CAPI_REMOTE_REGISTER:      return "[REGISTER_IND]";
				case CAPI_REMOTE_RELEASE:       return "[RELEASE_IND]";
			}
			break;
		case CAPI_CONF:
			switch(Command) {
				case CAPI_ALERT:                return "[ALERT_CONF]";
				case CAPI_CONNECT:              return "[CONNECT_CONF]";
				case CAPI_CONNECT_ACTIVE:       return "[CONNECT_ACTIVE_CONF]";
				case CAPI_INTEROPERABILITY:     return "[INTEROPERABILITY_CONF]";
				case CAPI_CONNECT_B3_ACTIVE:    return "[CONNECT_B3_ACTIVE_CONF]";
				case CAPI_CONNECT_B3:           return "[CONNECT_B3_CONF]";
				case CAPI_CONNECT_B3_T90_ACTIVE:return "[CONNECT_B3_T90_ACTIVE_CONF]";
				case CAPI_DATA_B3:              return "[DATA_B3_CONF]";
				case CAPI_DISCONNECT_B3:        return "[DISCONNECT_B3_CONF]";
				case CAPI_DISCONNECT:           return "[DISCONNECT_CONF]";
				case CAPI_FACILITY:             return "[FACILITY_CONF]";
				case CAPI_INFO:                 return "[INFO_CONF]";
				case CAPI_LISTEN:               return "[LISTEN_CONF]";
				case CAPI_MANUFACTURER:         return "[MANUFACTURER_CONF]";
				case CAPI_RESET_B3:             return "[RESET_B3_CONF]";
				case CAPI_SELECT_B_PROTOCOL:    return "[SELECT_B_PROTOCOL_CONF]";
				case CAPI_REMOTE_REGISTER:      return "[REGISTER_CONF]";
				case CAPI_REMOTE_RELEASE:       return "[RELEASE_CONF]";
			}
			break;
		case CAPI_RESP:
			switch(Command) {
				case CAPI_ALERT:                return "[ALERT_RESP]";
				case CAPI_CONNECT:              return "[CONNECT_RESP]";
				case CAPI_CONNECT_ACTIVE:       return "[CONNECT_ACTIVE_RESP]";
				case CAPI_INTEROPERABILITY:     return "[INTEROPERABILITY_RESP]";
				case CAPI_CONNECT_B3_ACTIVE:    return "[CONNECT_B3_ACTIVE_RESP]";
				case CAPI_CONNECT_B3:           return "[CONNECT_B3_RESP]";
				case CAPI_CONNECT_B3_T90_ACTIVE:return "[CONNECT_B3_T90_ACTIVE_RESP]";
				case CAPI_DATA_B3:              return "[DATA_B3_RESP]";
				case CAPI_DISCONNECT_B3:        return "[DISCONNECT_B3_RESP]";
				case CAPI_DISCONNECT:           return "[DISCONNECT_RESP]";
				case CAPI_FACILITY:             return "[FACILITY_RESP]";
				case CAPI_INFO:                 return "[INFO_RESP]";
				case CAPI_LISTEN:               return "[LISTEN_RESP]";
				case CAPI_MANUFACTURER:         return "[MANUFACTURER_RESP]";
				case CAPI_RESET_B3:             return "[RESET_B3_RESP]";
				case CAPI_SELECT_B_PROTOCOL:    return "[SELECT_B_PROTOCOL_RESP]";
				case CAPI_REMOTE_REGISTER:      return "[REGISTER_RESP]";
				case CAPI_REMOTE_RELEASE:       return "[RELEASE_RESP]";
			}
			break;
	}
	return "[unknown]";
}
#endif /*--- #if defined(OSLIB_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(OSLIB_DEBUG)
void CapiTrace(unsigned char *Msg, int (*print)(const char *, ...)) {
#if defined(OSLIB_MSGDEBUG)
	int len = Msg[0] + (Msg[1] << 8), i;
	unsigned char test = 0;
	print("[%u %s:", Msg[2] | (Msg[3] << 8), CAPI_MESSAGE_NAME(Msg[4], Msg[5]) + 1);
	print("len=%u Nr.=%u", Msg[0] | (Msg[1] << 8), Msg[6] | (Msg[7] << 8));
	if(Msg[4] == CAPI_DATA_B3 && (Msg[5] == CAPI_CONF || Msg[5] == CAPI_RESP)) {
		print(" NCCI=%x Hdl=%x", Msg[8] | (Msg[9] << 8) | (Msg[10] << 16) | (Msg[11] << 24),  Msg[12] | (Msg[13] << 8));
	} else if(Msg[4] == CAPI_DATA_B3 && (Msg[5] == CAPI_IND || Msg[5] == CAPI_REQ)) {
		print(" NCCI=%x len=%u Hdl=%x", Msg[8] | (Msg[9] << 8) | (Msg[10] << 16) | (Msg[11] << 24),  /* NCCI */
									   Msg[16] | (Msg[17] << 8),  /* Data len */ Msg[18] | (Msg[19] << 8));
	}
	if((Msg[4] == CAPI_DATA_B3) && ((Msg[5] == CAPI_IND) || (Msg[5] == CAPI_REQ))) {
		unsigned datalen = Msg[16] | (Msg[17] << 8);
		unsigned char *data =  (unsigned char *)(Msg[12] | (Msg[13] << 8) | (Msg[14] << 16) | (Msg[15] << 24));
		for(i = 0 && data; i < datalen; i++) {
			test |= data[i];
		}
		if(test) {
			print("]\nData(%d)[", datalen);
			if(datalen > 80) datalen=80;
			while(datalen--) {
				printk("0x%02x ", *data++);
			}
			print("]\n");
		}
	}
	print("]\nMsg[");
	if(len > 80) len=80;
	while(len--) {
		printk("0x%02x ", *Msg++);
	}
	print("]\n");
#endif/*--- #if defined(OSLIB_MSGDEBUG) ---*/
}
#endif /*--- #if defined(OSLIB_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *capi_oslib_capi_error_name(int err) {
	enum _CapiErrors error = (enum _CapiErrors)err;
	switch(error) {
		case ERR_NoError: return "ERR_NoError";
		case ERR_NCPI_ignored: return "ERR_NCPI_ignored";
		case ERR_Flags_ignored: return "ERR_Flags_ignored";
		case ERR_AlertAlreadySend: return "ERR_AlertAlreadySend";
		case ERR_ToManyApplications: return "ERR_ToManyApplications";
		case ERR_BlockToSmall: return "ERR_BlockToSmall";
		case ERR_Buffer64k: return "ERR_Buffer64k";
		case ERR_MessageToSmall: return "ERR_MessageToSmall";
		case ERR_MaxLogicalConnections: return "ERR_MaxLogicalConnections";
		case ERR_1006_Reserved: return "ERR_1006_Reserved";
		case ERR_SendBusy: return "ERR_SendBusy";
		case ERR_ResourceError: return "ERR_ResourceError";
		case ERR_No_CAPI: return "ERR_No_CAPI";
		case ERR_No_External: return "ERR_No_External";
		case ERR_Only_External: return "ERR_Only_External";
		case ERR_IllegalApplId: return "ERR_IllegalApplId";
		case ERR_IllegalMessage: return "ERR_IllegalMessage";
		case ERR_QueueFull: return "ERR_QueueFull";
		case ERR_QueueEmpty: return "ERR_QueueEmpty";
		case ERR_MessageLost: return "ERR_MessageLost";
		case ERR_UnknownNotification: return "ERR_UnknownNotification";
		case ERR_InternalBusy: return "ERR_InternalBusy";
		case ERR_OS_Resource: return "ERR_OS_Resource";
		case ERR_No_CAPI_11: return "ERR_No_CAPI_11";
		case ERR_No_External_11: return "ERR_No_External_11";
		case ERR_Only_External_11: return "ERR_Only_External_11";
		case ERR_MessageNotSupportedInCurrentState: return "ERR_MessageNotSupportedInCurrentState";
		case ERR_IllegalPLCI: return "ERR_IllegalController/PLCI/NCCI";
		case ERR_OutOfPLCI: return "ERR_OutOfPLCI";
		case ERR_OutOfNCCI: return "ERR_OutOfNCCI";
		case ERR_OutOfLISTEN: return "ERR_OutOfLISTEN";
		case ERR_OutOfFaxResources: return "ERR_OutOfFaxResources";
		case ERR_IllegalMessageParameterCoding: return "ERR_IllegalMessageParameterCoding";
		case ERR_B1ProtocolNotSupported: return "ERR_B1ProtocolNotSupported";
		case ERR_B2ProtocolNotSupported: return "ERR_B2ProtocolNotSupported";
		case ERR_B3ProtocolNotSupported: return "ERR_B3ProtocolNotSupported";
		case ERR_B1ProtocolParameterNotSupported: return "ERR_B1ProtocolParameterNotSupported";
		case ERR_B2ProtocolParameterNotSupported: return "ERR_B2ProtocolParameterNotSupported";
		case ERR_B3ProtocolParameterNotSupported: return "ERR_B3ProtocolParameterNotSupported";
		case ERR_BProtocolCombinationNotSupported: return "ERR_BProtocolCombinationNotSupported";
		case ERR_NCPINotSupported: return "ERR_NCPINotSupported";
		case ERR_CIPValueUnknown: return "ERR_CIPValueUnknown";
		case ERR_FlagsNotSupported: return "ERR_FlagsNotSupported";
		case ERR_FacilitiesNotSupported: return "ERR_FacilitiesNotSupported";
		case ERR_DataLengthNotSupported: return "ERR_DataLengthNotSupported";
		case ERR_ResetProcedureNotSupported: return "ERR_ResetProcedureNotSupported";
		case ERR_SupplServicesNotSupported: return "ERR_SupplServicesNotSupported";
		case ERR_RequestNotAllowedInThisState: return "ERR_RequestNotAllowedInThisState";
		case ERR_AnotherApplicationGotCall: return "ERR_AnotherApplicationGotCall";
		case ERR_L2CAP_No_Error: return "ERR_L2CAP_No_Error";
		case ERR_L2CAP_PSM_Not_Supported: return "ERR_L2CAP_PSM_Not_Supported";
		case ERR_L2CAP_Security_Block: return "ERR_L2CAP_Security_Block";
		case ERR_L2CAP_No_Resources: return "ERR_L2CAP_No_Resources";
		case ERR_L2CAP_Timeout: return "ERR_L2CAP_Timeout";
		case ERR_L2CAP_QoS_Failure: return "ERR_L2CAP_QoS_Failure";
		case ERR_RFCOMM_Protocol_Error: return "ERR_RFCOMM_Protocol_Error";
		case ERR_RFCOMM_Remote_Protocol_Error: return "ERR_RFCOMM_Remote_Protocol_Error";
		case ERR_RFCOMM_Timeout: return "ERR_RFCOMM_Timeout";
		case ERR_HigherLayer_Unknown: return "ERR_HigherLayer_Unknown";
		case ERR_NameResolution_Failed: return "ERR_NameResolution_Failed";
		case ERR_ConnRefused_Role_Reject: return "ERR_ConnRefused_Role_Reject";
		default: return "ERR_unknown";
	}
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *capi_source_name[] = {
	"SOURCE_UNKNOWN",
	"SOURCE_PTR_CAPI",
	"SOURCE_DEV_CAPI",
	"SOURCE_SOCKET_CAPI",
	"SOURCE_KERNEL_CAPI",
	"SOURCE_ANZAHL"
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _capi_oslib_open_data *first_open_data = NULL;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(OSLIB_DEBUG)
void capi_oslib_dump_open_data_list(char *text) {
	struct _capi_oslib_open_data *O = NULL;
	unsigned int i = 1;
	O = first_open_data;
	DEB_ERR("[capi_oslib_dump_open_data_list-%s] first=0x%p\n", text, first_open_data);
	while(O) {
		DEB_ERR("\t[%u] ApplId=%u 0x%p <-- 0x%p --> 0x%p\n", i++, O->ApplId, O->prev, O, O->next);
		O = O->next;
	}
}
#endif /*--- #if defined(OSLIB_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_register_open_data(struct _capi_oslib_open_data *open_data) {
	struct _capi_oslib_open_data *O;

	open_data->prev = NULL;
	open_data->next = NULL;

/*--- capi_oslib_dump_open_data_list("register start"); ---*/

	if(first_open_data == NULL) {
		first_open_data = open_data;
/*--- capi_oslib_dump_open_data_list("register end first "); ---*/
		return;
	}
	O = first_open_data;
	while(O->next)
		O = O->next;
	open_data->prev = O;
	O->next = open_data;
/*--- capi_oslib_dump_open_data_list("register end next"); ---*/
	return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_release_open_data(struct _capi_oslib_open_data *open_data) {
/*--- capi_oslib_dump_open_data_list("release start"); ---*/
	if(first_open_data == open_data)
		first_open_data = open_data->next; /* kann null sein */
	if(open_data->prev) {
		open_data->prev->next = open_data->next; /* kann null sein */
	}
	if(open_data->next) {
		open_data->next->prev = open_data->prev; /* kann null sein */
	}
/*--- capi_oslib_dump_open_data_list("release done"); ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int capi_oslib_dump_open_data(struct file *filp __attribute__((unused)), char *read_buffer, size_t max_read_length, loff_t *read_pos) {
	static struct _capi_oslib_open_data *O = NULL;
	static char Buffer[1024];
	int len;
	unsigned int maxlen = sizeof(Buffer);
	char *p = Buffer;

	if(*read_pos == (loff_t)0) {
		extern struct capi_pipe CapiReceivePipe;
		O = first_open_data;
		DEB_INFO("[dump] first (0x%p)\n", O);
		len = snprintf(p, maxlen, "[Capi] Pipe=%s\n", Capi_Pipe_Status(&CapiReceivePipe));
		if(len > 0) {
			p += len, maxlen -= len;
		}
		goto copy_to_user_appl;
	}

	if(O == NULL) {
		*read_pos = (loff_t)0;
		DEB_INFO("[dump] last\n");
		return 0;
	}

	DEB_INFO("[dump] ApplId %d\n", O->ApplId);

	Buffer[0] = '\0';

	if(O->ApplId == 0) {
		DEB_ERR("illegal applid 0\n");
		Buffer[0] = '\0';
	} else {
		struct _ApplData *A;
		unsigned int MapperId = Appl_Find_ApplId(O->mode, O->ApplId);
		len = snprintf(p, maxlen , "O->ApplId %u ---> MapperId %u\n", O->ApplId, MapperId);
		if(len > 0) {
			p += len, maxlen -= len;
		}
		if(MapperId) {
			A = &ApplData[MapperId - 1];
			len = snprintf(p, maxlen, "%s\n", Appl_PrintOneAppl(A));
			if(len > 0) {
				p += len, maxlen -= len;
			}
		}
	}

	len = snprintf(p, maxlen,
			"[ApplId %u] LastErr %s Mode=%s Pipe=%s\n"
			"B3BlockSize=%u B3WindowSize=%u MaxNCCIs=%u MessageBufferSize=%u\n",
			O->ApplId,
			capi_oslib_capi_error_name(O->last_error),
			capi_source_name[O->mode],
			O->read_pipe ?  Capi_Pipe_Status(O->read_pipe) : "no pipe",
			O->B3BlockSize, O->B3WindowSize, O->MaxNCCIs, O->MessageBufferSize);
	if(len > 0) {
		p += len, maxlen -= len;
	}
	DEB_INFO("O(%p) -> next (%p)\n", O, O->next);

	O = O->next;

copy_to_user_appl:

	len = p - Buffer;
	len = min(max_read_length, (size_t)len);

	len -= copy_to_user(read_buffer, Buffer, len);

	*read_pos += (loff_t)len;

	DEB_INFO("[dump] %u bytes\n", len);

	return len;
}

