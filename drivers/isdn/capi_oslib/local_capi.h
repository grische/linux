#ifndef _local_capi_h_
#define _local_capi_h_

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "capi_events.h"
#include "capi_pipe.h"

#define B3_DATA_ALLOC_SIZE  1024

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _local_capi_appl {
	atomic_t		InUse;
	atomic_t		ref_count;
	struct capi_pipe *Pipe;
	unsigned int    MaxMessageSize;
	unsigned int    MessageLost;
	unsigned char  *MessageBuffer;
	unsigned char  *PipePointer;
	unsigned int    TmpBufferSize;
	unsigned char  *TmpBuffer;
	unsigned int    EventBit;
	struct capi_events *SignalEvent;
};

struct _adr_b3_ind_data {
	enum _entry_use InUse;
	unsigned char *Kernel_Buffer;
	unsigned char *User_Buffer;
};
/**
 */
static inline void init_local_capi_appl(struct _local_capi_appl *lca) {
	memset((unsigned char *)lca + offsetof(struct _local_capi_appl, Pipe), 0,
				sizeof(struct _local_capi_appl)
				- offsetof(struct _local_capi_appl, Pipe));
	atomic_set(&lca->ref_count, 0);
}
/**
 */
static inline const char *name_entry_in_use(enum _entry_use use) {
	if(use == _entry_not_used_)			return "entry_not_used_";
	if(use == _entry_in_use_)			return "entry_in_use_";
	if(use == _entry_register_pending)	return "entry_register_pending";
	if(use == _entry_release_pending)	return "entry_release_pending";
	return "entry_?";
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define CA_IS_DATA_B3_REQ(Msg)    ((Msg[4] == 0x86) && (Msg[5] == 0x80))  /*--- nur CAPI 2.0 ---*/
#define CA_IS_DATA_B3_CONF(Msg)   ((Msg[4] == 0x86) && (Msg[5] == 0x81))  /*--- nur CAPI 2.0 ---*/
#define CA_IS_DATA_B3_IND(Msg)    ((Msg[4] == 0x86) && (Msg[5] == 0x82))  /*--- nur CAPI 2.0 ---*/
#define CA_IS_DATA_B3_RESP(Msg)   ((Msg[4] == 0x86) && (Msg[5] == 0x83))  /*--- nur CAPI 2.0 ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void LOCAL_CAPI_INIT(enum _capi_source capi_source);
void LOCAL_CAPI_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char *Msg, unsigned int MsgLen);
unsigned char *LOCAL_CAPI_GET_VERSION_WORD(unsigned int Controller, unsigned int Word);
unsigned int LOCAL_CAPI_REGISTER(enum _capi_source capi_source, unsigned int MessageBufferSize, unsigned int MaxNCCIs, unsigned int WindowSize, unsigned int B3BlockSize, unsigned int *ApplId);
unsigned int LOCAL_CAPI_RELEASE(enum _capi_source capi_source, unsigned int ApplId);
unsigned int LOCAL_CAPI_RELEASE_CONF(enum _capi_source capi_source, unsigned int ApplId);
unsigned char *LOCAL_CAPI_GET_BUFFER(enum _capi_source capi_source, unsigned int ApplId, unsigned int Size);
unsigned int LOCAL_CAPI_PUT_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char *CapiMessage);
unsigned char *LOCAL_CAPI_NEW_DATA_B3_REQ_BUFFER(enum _capi_source capi_source, unsigned int ApplId, unsigned int NCCI);
unsigned int LOCAL_CAPI_SET_SIGNAL(enum _capi_source capi_source, unsigned int ApplId, struct capi_events *pEvent, unsigned int Bit);
unsigned int LOCAL_CAPI_GET_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char **pCapiMessage, unsigned int Suspend);
struct capi_pipe *LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE(enum _capi_source capi_source, unsigned int ApplId, wait_queue_head_t *rx_wait_queue, wait_queue_head_t *tx_wait_queue);
int LOCAL_CAPI_REGISTER_B3_BUFFER(enum _capi_source capi_source, unsigned int ApplId, struct _adr_b3_ind_data *b3Buffers, unsigned int BufferAnzahl, void (*release_buffers)(void *), void *context);
unsigned int LOCAL_CAPI_SET_NOTIFY(enum _capi_source capi_source, unsigned int ApplId, void *Conn);

#endif /*--- #ifndef _local_capi_h_ ---*/
