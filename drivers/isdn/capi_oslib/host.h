#ifndef _HOST_H_
#define _HOST_H_

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include "appl.h"

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
extern struct capi_pipe CapiReceivePipe;
extern struct semaphore S_ToHost;

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int HOST_INIT(enum _capi_source CapiSource, unsigned int AnzAppliktions, unsigned int AnzNCCIs, unsigned int CAPI_INDEX);
void HOST_REGISTER(enum _capi_source CapiSource, unsigned int ApplId, unsigned int AnzahlMsgs, unsigned int B3Connection, unsigned int B3Blocks, unsigned int SizeB3);
void HOST_RE_REGISTER(enum _capi_source CapiSource, unsigned int ApplId, unsigned int MapperId, unsigned int AnzahlMsgs, unsigned int B3Connection, unsigned int B3Blocks, unsigned int SizeB3);
unsigned int HOST_MAP_APPL_ID(enum _capi_source CapiSource, unsigned char *Msg);
enum _CapiErrors HOST_MESSAGE(enum _capi_source CapiSource, unsigned char *Msg, unsigned char *Buffer);
void HOST_RELEASE(enum _capi_source CapiSource, unsigned int ApplId);
unsigned char *HOST_NEW_DATA_B3_REQ(enum _capi_source CapiSource, unsigned char *Msg, unsigned int *MaxLength);
void HOST_DO_POLL(void);
int HOST_RELEASE_B3_BUFFER(enum _capi_source capi_source, unsigned int ApplId);

struct _adr_b3_ind_data;  /* forward decl */
int HOST_REGISTER_B3_BUFFER(enum _capi_source capi_source, unsigned int ApplId, struct _adr_b3_ind_data *b3Buffers, unsigned int BufferAnzahl, void (*release_buffers)(void *), void *context);


#endif /*--- #ifndef _HOST_H_ ---*/
