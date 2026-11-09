/*-------------------------------------------------------------------------------------*\
  ..\oslib\include\crtos_debug.h generated from ..\oslib\include\crtos.h by mk_debug.exe (c) AVM Berlin   Martin Pommerenke 
\*-------------------------------------------------------------------------------------*/
#ifndef _crtos_debug_h_
#define _crtos_debug_h_

#include "crtos.h"

#ifdef __cplusplus
extern "C" {
#endif /*--- #ifdef __cplusplus ---*/

#ifndef GENERATE_Name__bt20_profile
char *Name__bt20_profile(enum _bt20_profile Value);
#else
char *Name__bt20_profile(enum _bt20_profile Value) {
	return
	Value == bt20_profile_reserved ? "bt20_profile_reserved" : 
	Value == bt20_profile_cmtp ? "bt20_profile_cmtp" : 
	Value == bt20_profile_dun ? "bt20_profile_dun" : 
	Value == bt20_profile_pan ? "bt20_profile_pan" : 
	Value == bt20_profile_spp ? "bt20_profile_spp" : 
	Value == bt20_profile_fax ? "bt20_profile_fax" : 
	Value == bt20_profile_lan ? "bt20_profile_lan" : 
	Value == bt20_profile_headset ? "bt20_profile_headset" : 
	Value == bt20_profile_cts ? "bt20_profile_cts" : 
	Value == bt20_profile_management ? "bt20_profile_management" : 
	Value == bt20_profile_sdp_client ? "bt20_profile_sdp_client" : 
	Value == bt20_profile_l2cap ? "bt20_profile_l2cap" : 
	Value == bt20_profile_acl ? "bt20_profile_acl" : 
	Value == bt20_profile_hci ? "bt20_profile_hci" : 
	"_bt20_profile_unknown";
}
#endif

#ifndef GENERATE_Name__CapiErrors
char *Name__CapiErrors(enum _CapiErrors Value);
#else
char *Name__CapiErrors(enum _CapiErrors Value) {
	return
	Value == ERR_NoError ? "Request accepted" : 
	Value == ERR_NCPI_ignored ? "NCPI not supported by current protocol, NCPI ignored" : 
	Value == ERR_Flags_ignored ? "Flags not supported by current protocol, flags ignored" : 
	Value == ERR_AlertAlreadySend ? "Alert already sent by another application" : 
	Value == ERR_ToManyApplications ? "Too many applications" : 
	Value == ERR_BlockToSmall ? "Logical block size too small; must be at least 128 bytes" : 
	Value == ERR_Buffer64k ? "Buffer exceeds 64 kbytes" : 
	Value == ERR_MessageToSmall ? "Message buffer size too small, must be at least 1024 bytes" : 
	Value == ERR_MaxLogicalConnections ? "Max. number of logical connections not supported" : 
	Value == ERR_1006_Reserved ? "reserved" : 
	Value == ERR_SendBusy ? "The message could not be accepted because of an internal busy condition" : 
	Value == ERR_ResourceError ? "OS resource error (e.g. no memory)" : 
	Value == ERR_No_CAPI ? "COMMON-ISDN-API not installed" : 
	Value == ERR_No_External ? "Controller does not support external equipment" : 
	Value == ERR_Only_External ? "Controller does only support external equipment" : 
	Value == ERR_IllegalApplId ? "Illegal application number" : 
	Value == ERR_IllegalMessage ? "Illegal command or subcommand, or message length less than 12 octets" : 
	Value == ERR_QueueFull ? "The message could not be accepted because of a queue full condition. The error code does not imply that COMMON-ISDN-API cannot receive messages directed to another controller, PLCI or NCCI." : 
	Value == ERR_QueueEmpty ? "Queue is empty" : 
	Value == ERR_MessageLost ? "Queue overflow: a message was lost. This indicates a configuration error. The only recovery from this error is to do the CAPI_RELEASE operation." : 
	Value == ERR_UnknownNotification ? "Unknown notification parameter" : 
	Value == ERR_InternalBusy ? "The message could not be accepted because of an internal busy condition" : 
	Value == ERR_OS_Resource ? "0x1108 OS resource error (e.g. no memory)" : 
	Value == ERR_No_CAPI_11 ? "0x1109 COMMON-ISDN-API not installed" : 
	Value == ERR_No_External_11 ? "0x110A Controller does not support external equipment" : 
	Value == ERR_Only_External_11 ? "0x110B Controller supports only external equipment" : 
	Value == ERR_MessageNotSupportedInCurrentState ? "ERR_MessageNotSupportedInCurrentState" : 
	Value == ERR_IllegalController ? "ERR_IllegalController" : 
	Value == ERR_IllegalPLCI ? "ERR_IllegalPLCI" : 
	Value == ERR_IllegalNCCI ? "ERR_IllegalNCCI" : 
	Value == ERR_OutOfPLCI ? "ERR_OutOfPLCI" : 
	Value == ERR_OutOfNCCI ? "ERR_OutOfNCCI" : 
	Value == ERR_OutOfLISTEN ? "ERR_OutOfLISTEN" : 
	Value == ERR_OutOfFaxResources ? "ERR_OutOfFaxResources" : 
	Value == ERR_IllegalMessageParameterCoding ? "ERR_IllegalMessageParameterCoding" : 
	Value == ERR_B1ProtocolNotSupported ? "ERR_B1ProtocolNotSupported" : 
	Value == ERR_B2ProtocolNotSupported ? "ERR_B2ProtocolNotSupported" : 
	Value == ERR_B3ProtocolNotSupported ? "ERR_B3ProtocolNotSupported" : 
	Value == ERR_B1ProtocolParameterNotSupported ? "ERR_B1ProtocolParameterNotSupported" : 
	Value == ERR_B2ProtocolParameterNotSupported ? "ERR_B2ProtocolParameterNotSupported" : 
	Value == ERR_B3ProtocolParameterNotSupported ? "ERR_B3ProtocolParameterNotSupported" : 
	Value == ERR_BProtocolCombinationNotSupported ? "ERR_BProtocolCombinationNotSupported" : 
	Value == ERR_NCPINotSupported ? "ERR_NCPINotSupported" : 
	Value == ERR_CIPValueUnknown ? "ERR_CIPValueUnknown" : 
	Value == ERR_FlagsNotSupported ? "ERR_FlagsNotSupported" : 
	Value == ERR_FacilitiesNotSupported ? "ERR_FacilitiesNotSupported" : 
	Value == ERR_DataLengthNotSupported ? "ERR_DataLengthNotSupported" : 
	Value == ERR_ResetProcedureNotSupported ? "ERR_ResetProcedureNotSupported" : 
	Value == ERR_SupplServicesNotSupported ? "ERR_SupplServicesNotSupported" : 
	Value == ERR_RequestNotAllowedInThisState ? "ERR_RequestNotAllowedInThisState" : 
	Value == ERR_AnotherApplicationGotCall ? "ERR_AnotherApplicationGotCall" : 
	Value == ERR_L2CAP_No_Error ? "ERR_L2CAP_No_Error" : 
	Value == ERR_L2CAP_PSM_Not_Supported ? "ERR_L2CAP_PSM_Not_Supported" : 
	Value == ERR_L2CAP_Security_Block ? "ERR_L2CAP_Security_Block" : 
	Value == ERR_L2CAP_No_Resources ? "ERR_L2CAP_No_Resources" : 
	Value == ERR_L2CAP_Timeout ? "ERR_L2CAP_Timeout" : 
	Value == ERR_L2CAP_QoS_Failure ? "ERR_L2CAP_QoS_Failure" : 
	Value == ERR_RFCOMM_Protocol_Error ? "ERR_RFCOMM_Protocol_Error" : 
	Value == ERR_RFCOMM_Remote_Protocol_Error ? "ERR_RFCOMM_Remote_Protocol_Error" : 
	Value == ERR_RFCOMM_Timeout ? "ERR_RFCOMM_Timeout" : 
	Value == ERR_HigherLayer_Unknown ? "ERR_HigherLayer_Unknown" : 
	Value == ERR_NameResolution_Failed ? "ERR_NameResolution_Failed" : 
	Value == ERR_ConnRefused_Role_Reject ? "ERR_ConnRefused_Role_Reject" : 
	"_CapiErrors_unknown";
}
#endif

#ifdef __cplusplus
}
#endif /*--- #ifdef __cplusplus ---*/

#endif /*--- #ifndef _crtos_debug_h_ ---*/
