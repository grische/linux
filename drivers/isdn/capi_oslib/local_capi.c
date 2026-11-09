#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/hardirq.h>

#include <linux/ratelimit.h>
#include <linux/delay.h>
#pragma GCC pop_options

#include <avm/capi_oslib/new.h>
#include <avm/capi_oslib/oslib.h>
#include "debug.h"
#include "consts.h"
#include "appl.h"
#include "ca.h"
#include "host.h"
#include "capi_pipe.h"
#include "capi_events.h"
#include "zugriff.h"
#include "local_capi.h"

unsigned char *CAPI_Version;  /*--- CAPI_Version[MAX_CONTROLLERS][256] ---*/


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define SYSTEM_ERROR(str, ret) if(Status != 0) { printk(KERN_ERR str); return ret; }
#define SYSTEM_ERROR_VOID(str) if(Status != 0) { printk(KERN_ERR str); return; }

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _local_capi_appl *LocalCapiAppl[SOURCE_ANZAHL];
struct semaphore         LocalCapiReleaseSema;
unsigned int             LocalCapiMaxAppls;

/**
 */
static inline struct _local_capi_appl *get_local_capi_appl(enum _capi_source capi_source, unsigned int ApplId,
														   enum _entry_use old_state,
														   enum _entry_use new_state,
														   int *error) {
	struct _local_capi_appl *LA;
	enum _entry_use act_state;

	if(unlikely(LocalCapiAppl[capi_source] == NULL)) {
		DEB_ERR("%pS:CAPI for source=%u not installed\n", (void *)_RET_IP_, capi_source);
		if(error) *error = ERR_IllegalController;
		return NULL;
	}
	if(unlikely(ApplId > LocalCapiMaxAppls)) {
		DEB_ERR("%pS:illegal ApplId %d\n", (void *)_RET_IP_, ApplId);
		if(error) *error = ERR_IllegalApplId;
		return NULL;
	}
	LA = &LocalCapiAppl[capi_source][ApplId - 1];
	if(likely(new_state == old_state)) {
		if(unlikely((enum _entry_use)atomic_read(&LA->InUse) != new_state)) {
			DEB_WARN("%pS ApplId %d not registered\n", (void *)_RET_IP_, ApplId);
			if(error) *error = ERR_IllegalApplId;
			return NULL;
		}
	} else {
		if(((act_state = atomic_cmpxchg(&LA->InUse, old_state, new_state)) != old_state)) {
			DEB_WARN("%pS ApplId %d not registered state=%s\n", (void *)_RET_IP_, ApplId,
					name_entry_in_use(act_state));
			if(error) *error = ERR_IllegalApplId;
			return NULL;
		}
	}
	atomic_inc(&LA->ref_count);
	if(error) *error = ERR_NoError;
	return LA;
}
/**
 */
static inline struct _local_capi_appl *alloc_local_capi_appl(enum _capi_source capi_source, unsigned int *ApplId,
														   int *error) {
	struct _local_capi_appl *LA = NULL;
	unsigned int Count;

	if(unlikely(LocalCapiAppl[capi_source] == NULL)) {
		DEB_ERR("%pS:CAPI for source=%u not installed\n", (void *)_RET_IP_, capi_source);
		if(error) *error = ERR_IllegalController;
		return NULL;
	}
	/*--- find free applid ---*/
	for(Count = 0 ; Count < LocalCapiMaxAppls ; Count++) {
		LA = &LocalCapiAppl[capi_source][Count];
		if(unlikely(atomic_cmpxchg(&LA->InUse, _entry_not_used_, _entry_register_pending) != _entry_not_used_)) {
			continue;
		}
		break;
	}
	if(Count == LocalCapiMaxAppls) {
		DEB_ERR("%pS:CAPI no ApplId\n", (void *)_RET_IP_);
		if(error) *error = ERR_ToManyApplications;
		return NULL;
	}
	if(error) *error = ERR_NoError;
	*ApplId = Count + 1;
	init_local_capi_appl(LA);
	atomic_set(&LA->ref_count, 1);
	DEB_INFO("%pS: ApplId=%u\n", (void *)_RET_IP_, *ApplId);
	return LA;
}
/**
 */
static inline void put_local_capi_appl(struct _local_capi_appl *pla) {
	atomic_dec(&pla->ref_count);
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static inline unsigned char *LOCAL_CAPI_GET_DATA_B3_REQ_DATA(unsigned char *Msg);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void LOCAL_CAPI_INIT(enum _capi_source capi_source) {
	unsigned int Count;

	if(capi_source == SOURCE_UNKNOWN) {
		sema_init(&LocalCapiReleaseSema, 0);
		LocalCapiMaxAppls = LOCAL_CAPI_APPLIKATIONS;
		DEB_INFO("%s: first done\n", __func__);
	}

	LocalCapiAppl[capi_source] = (struct _local_capi_appl *)CA_MALLOC(sizeof(struct _local_capi_appl) *
																	  LocalCapiMaxAppls);

	for(Count = 0 ; Count < LocalCapiMaxAppls; Count++) {
		struct _local_capi_appl *lca =  &LocalCapiAppl[capi_source][Count];
		init_local_capi_appl(lca);
		atomic_set(&lca->InUse, _entry_not_used_);
	}
	DEB_INFO("%s: done\n", __func__);
	return;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void LOCAL_CAPI_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char *Msg, unsigned int MsgLen) {
	struct _local_capi_appl *LA;
	int                      Status;
	unsigned int             Suspend;

#if defined(DEBUG_LOCAL_CAPI_DATA)
	{
		unsigned int Count;
		DEB_INFO("%s(ApplId=%d, Msg=%x, MsgLen=%u): ",  __func__, ApplId, Msg, MsgLen);
		for(Count = 0 ; Count < MsgLen ; Count++) {
			printk("%02x ", Msg[Count]);
		}
		printk("\n");
	}
#endif /*--- #if defined(DEBUG_LOCAL_CAPI_DATA) ---*/
	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, NULL);
	if(LA == NULL) {
		return;
	}
#if defined(USE_WORKQUEUES) || defined(USE_THREAD)
	Suspend = CAPI_SUSPEND;
#endif
#if defined(USE_TASKLETS)
	Suspend = CAPI_NO_SUSPEND;
#endif
	Status = Capi_Send_To_Pipe(LA->Pipe, Msg, MsgLen, Suspend); /*--- maximal 100 ms warten dann message lost ---*/
	if(Status == CAPI_PIPE_TIMEOUT || Status == CAPI_PIPE_FULL) {
		DEB_WARN("to-appl-message-pipe overflow %s\n", Capi_Pipe_Status(LA->Pipe));
		LA->MessageLost++;
	} else if(Status != 0) {
		DEB_ERR("LOCAL_CAPI_MESSAGE(%s, %u) Send_To_Pipe(LA->Pipe, Msg, %u, Suspend) failed, Status = %d\n",
																		__FILE__, __LINE__, MsgLen, Status);
		put_local_capi_appl(LA);
		return;
	}
	if(LA->SignalEvent) {
		Status = Capi_Set_Events(LA->SignalEvent, LA->EventBit, CAPI_EVENT_OR);
		DEB_WARN("CAPI_MESSAGE(ApplId=%d, *Msg, MsgLen=%u) signal event failed\n",  ApplId, MsgLen);
	}
	put_local_capi_appl(LA);
	return;
}


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_REGISTER(enum _capi_source capi_source, unsigned int MessageBufferSize,
								unsigned int MaxNCCIs, unsigned int WindowSize,
								unsigned int B3BlockSize, unsigned int *ApplId) {
	unsigned int             Len, MaxLen;
	struct _local_capi_appl *LA = NULL;
	int error;
	int Lock = CAPI_LOCK;
	char                     Name[40];

	DEB_INFO("%s(%s, MessageBufferSize=%u, MaxNCCIs=%u, WindowSize=%u, B3BlockSize=%u)\n", __func__,
				capi_source_name[capi_source], MessageBufferSize, MaxNCCIs, WindowSize, B3BlockSize);
	LA = alloc_local_capi_appl(capi_source, ApplId,  &error);
	if(LA == NULL) {
		return error;
	}
	BUG_ON(in_softirq());

	Len                = MessageBufferSize + 256;
	MaxLen             = min(MessageBufferSize, MAX_CAPI_MESSAGE_SIZE);
	LA->MaxMessageSize = MaxLen;
	LA->MessageBuffer  = CA_MALLOC(Len);
	LA->TmpBufferSize  = 0;
	LA->TmpBuffer      = NULL;
	LA->SignalEvent    = NULL;

	snprintf(Name, sizeof(Name), "%s-%s-%u", capi_source_name[capi_source], current->comm, *ApplId);

	LA->PipePointer    = CA_MALLOC(Len);
	LA->Pipe           = CA_MALLOC(sizeof(struct capi_pipe) * 1);

#if defined(USE_TASKLETS)
	Lock = CAPI_NO_LOCK;
#endif
	Capi_Create_Pipe(LA->Pipe, Name, LA->PipePointer, Len, CAPI_VARIABLE_SIZE, MaxLen, Lock);

	HOST_REGISTER(capi_source, *ApplId, 0, MaxNCCIs, WindowSize, B3BlockSize);
	/*--- capi_oslib_dump_open_data_list("LOCAL_CAPI_REGISTER"); ---*/

	atomic_set(&LA->InUse, _entry_in_use_);  /* jetzt erst */
	put_local_capi_appl(LA);
	DEB_INFO("%s ApplId=%u success\n", __func__, *ApplId);
	return ERR_NoError;
}
/*------------------------------------------------------------------------------------------*\
 * Der Buffer mitt B3BlockSize * 8 * MaxNCCIs gross sein
\*------------------------------------------------------------------------------------------*/
int LOCAL_CAPI_REGISTER_B3_BUFFER(enum _capi_source capi_source, unsigned int ApplId, struct _adr_b3_ind_data *b3Buffers, unsigned int BufferAnzahl, void (*release_buffers)(void *), void *context) {
	int error;
	struct _local_capi_appl *LA;

	DEB_INFO("%s(%s, ApplId=%u)\n", __func__, capi_source_name[capi_source], ApplId);
	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, &error);
	if(LA == NULL) {
		return error;
	}
	error = HOST_REGISTER_B3_BUFFER(capi_source, ApplId, b3Buffers, BufferAnzahl, release_buffers, context);
	put_local_capi_appl(LA);
	return error;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct capi_pipe *LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE(enum _capi_source capi_source, unsigned int ApplId, wait_queue_head_t *rx_wait_queue, wait_queue_head_t *tx_wait_queue) {
	struct _local_capi_appl *LA;
	DEB_INFO("%s(%s, ApplId=%u)\n", __func__, capi_source_name[capi_source], ApplId);
	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, NULL);
	if(LA == NULL) {
		return NULL;
	}
	Capi_Pipe_Options(LA->Pipe, rx_wait_queue, tx_wait_queue);
	put_local_capi_appl(LA);
	return LA->Pipe;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_RELEASE(enum _capi_source capi_source, unsigned int ApplId) {
	unsigned char            RELEASE_REQ[8];
	struct _local_capi_appl *LA;
	int ret;

	DEB_INFO("%s(%s, ApplId=%u)\n", __func__, capi_source_name[capi_source], ApplId);
	/*--- capi_oslib_dump_open_data_list("LOCAL_CAPI_RELEASE"); ---*/

	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_release_pending, &ret);
	if(LA == NULL) {
		return ret;
	}
	put_local_capi_appl(LA);
	SET_WORD( (RELEASE_REQ+0), sizeof(RELEASE_REQ));      /*----- Len --###*/
	SET_WORD( (RELEASE_REQ+2), (unsigned short)ApplId);
	*(unsigned char  *)(RELEASE_REQ+4) = 0xfe;                     /*----- command --###*/
	*(unsigned char  *)(RELEASE_REQ+5) = 0x80;                     /*----- subcommand --###*/

	ret = (unsigned int)HOST_MESSAGE (capi_source, RELEASE_REQ, NULL);
	BUG_ON(in_softirq());
	down(&LocalCapiReleaseSema);

	return ret;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_RELEASE_CONF(enum _capi_source capi_source, unsigned int ApplId) {
	struct _local_capi_appl *LA;
	int Status, error;

	DEB_INFO("%s(%s, ApplId=%u)\n", __func__, capi_source_name[capi_source], ApplId);

	LA = get_local_capi_appl(capi_source, ApplId, _entry_release_pending, _entry_release_pending, &error);
	if(LA == NULL) {
		return error;
	}
	put_local_capi_appl(LA);
	while(atomic_read(&LA->ref_count) > 1) {
		printk_ratelimited(KERN_ERR"%s: refcount=%u \n", __func__, atomic_read(&LA->ref_count));
		msleep(10);
	}
	Status = Capi_Delete_Pipe(LA->Pipe);

	if(Status != 0) {
		printk(KERN_ERR "%s: delete LA->Pipe failed", __func__);
	}
	CA_FREE(LA->PipePointer);
	CA_FREE(LA->Pipe);
	CA_FREE(LA->MessageBuffer);

	if(LA->TmpBufferSize && LA->TmpBuffer) {
		CA_FREE(LA->TmpBuffer);
		LA->TmpBuffer = NULL;
	}
	init_local_capi_appl(LA);
	atomic_set(&LA->InUse, _entry_not_used_);
	up(&LocalCapiReleaseSema);

	return 0x0000;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
static inline unsigned char *LOCAL_CAPI_GET_DATA_B3_REQ_DATA(unsigned char *Msg) {
	unsigned int Buffer;
	/*--- intel byte order ---*/
	Buffer  = Msg[12];
	Buffer |= Msg[13] << 8;
	Buffer |= Msg[14] << 16;
	Buffer |= Msg[15] << 24;
	return (unsigned char *)Buffer;
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned char *LOCAL_CAPI_GET_BUFFER(enum _capi_source capi_source, unsigned int ApplId, unsigned int Size) {
	struct _local_capi_appl *LA;
	DEB_INFO("%s(%s, ApplId=%u Size=%u)\n", __func__, capi_source_name[capi_source], ApplId, Size);

	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, NULL);
	if(LA == NULL) {
		return NULL;
	}
	if(LA->TmpBufferSize != Size) {
		if(LA->TmpBuffer) {
			CA_FREE(LA->TmpBuffer);
		}
		LA->TmpBuffer = CA_MALLOC(Size);
		if(LA->TmpBuffer)
			LA->TmpBufferSize = Size;
	}
	put_local_capi_appl(LA);
	return LA->TmpBuffer;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_PUT_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char *CapiMessage) {
	int error;
	/*--- unsigned char *Buffer; ---*/
	struct _local_capi_appl *LA;
#if defined(OSLIB_DEBUG)
	CapiTrace(CapiMessage, printk);
#endif/*--- #if defined(OSLIB_DEBUG) ---*/
	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, &error);
	if(LA == NULL) {
		return error;
	}
#if !defined(NO_BCHANNEL) && 0
	/*--------------------------------------------------------------------------------------*\
	 * DATA_B3_REQ:
	 * da der Pointer schon korrekt ist muss er nicht mehr umgesetzt werden
	\*--------------------------------------------------------------------------------------*/
	if(CA_IS_DATA_B3_REQ(CapiMessage)) {
		/*--- DEB_INFO(" IS_DATA_B3_REQ == TRUE "); ---*/
		Buffer = LOCAL_CAPI_GET_DATA_B3_REQ_DATA(CapiMessage);
		error =  HOST_MESSAGE(capi_source, CapiMessage, Buffer);
		put_local_capi_appl(LA);
		return error;
	}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/
	error = HOST_MESSAGE(capi_source, CapiMessage, NULL);
	put_local_capi_appl(LA);
	return error;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned char *LOCAL_CAPI_NEW_DATA_B3_REQ_BUFFER(enum _capi_source capi_source, unsigned int ApplId, unsigned int NCCI) {
	unsigned int MapperId;
	MapperId = Appl_Find_ApplId(capi_source, ApplId);     /*--- Mapper Id ermitteln ---*/
	if(MapperId == 0)
		return NULL;
	return CA_NEW_DATA_B3_REQ(MapperId, NCCI);
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_SET_NOTIFY(enum _capi_source capi_source, unsigned int ApplId, void *Conn) {
	struct _local_capi_appl *LA;
	int error;

	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, &error);
	if(LA == NULL) {
		return error;
	}
	LA->Pipe->Conn = Conn;
	put_local_capi_appl(LA);
	return ERR_NoError;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_SET_SIGNAL(enum _capi_source capi_source, unsigned int ApplId, struct capi_events *pEvent, unsigned int Bit) {
	struct _local_capi_appl *LA;
	int error;

	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, &error);
	if(LA == NULL) {
		return error;
	}
	LA->SignalEvent = pEvent;
	LA->EventBit    = Bit;
	put_local_capi_appl(LA);
	return ERR_NoError;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_GET_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char **pCapiMessage, unsigned int Suspend) {
	int                      Status;
	unsigned long            Count;
	struct _local_capi_appl *LA;
	int error;

	LA = get_local_capi_appl(capi_source, ApplId, _entry_in_use_, _entry_in_use_, &error);
	if(LA == NULL) {
		return error;
	}
	if(pCapiMessage == NULL) {
		/*--- DEB_INFO("CAPI_GET_MESSAGE(ApplId=%u, ...) (check message avail)\n", ApplId); ---*/
		;
	/*--- } else { ---*/
		/*--- DEB_INFO("CAPI_GET_MESSAGE(ApplId=%u, *pCapiMessage=%p ...., Suspend=%d)\n", ApplId, *pCapiMessage, Suspend); ---*/
	}
	if(pCapiMessage == NULL) {
		Status = Capi_Receive_From_Pipe(LA->Pipe, NULL, 0, &Count, CAPI_NO_SUSPEND);
		put_local_capi_appl(LA);
		return Status;
	}

	if(*pCapiMessage == NULL)
		*pCapiMessage = LA->MessageBuffer;

	if(*pCapiMessage == NULL) {
		put_local_capi_appl(LA);
		return ERR_ResourceError;
	}

	Status = Capi_Receive_From_Pipe(LA->Pipe, *pCapiMessage, LA->MaxMessageSize, &Count, Suspend);
	if(Status == CAPI_PIPE_EMPTY || Status == CAPI_PIPE_DELETED || Status == CAPI_PIPE_TIMEOUT) {
		put_local_capi_appl(LA);
		return ERR_QueueEmpty;
	}
#if defined(CAPIOSLIB_CHECK_LATENCY)
	/*--- capi_check_latency(ApplId, (char *)__func__, 1); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
	if(Status != 0) {
		/*--- DEB_WARN("ApplId %u Suspend %x local_capi_appl (%#x) =[%*B]\n", ApplId, Suspend, A, sizeof(*A), A); ---*/
		SYSTEM_ERROR("receive from LA->Pipe failed", ERR_ResourceError);
	}

	/*--- DEB_INFO("CAPI_GET_MESSAGE():%s\n", CAPI_MESSAGE_NAME((*pCapiMessage)[4], (*pCapiMessage)[5])); ---*/

	if(LA->MessageLost) {
		LA->MessageLost = 0;
		put_local_capi_appl(LA);
		return ERR_MessageLost;
	}

	if(Count == 0) {
		put_local_capi_appl(LA);
		return ERR_QueueEmpty;
	}
	{
#if defined(OSLIB_DEBUG)
		CapiTrace(*pCapiMessage, printk);
#endif/*--- #if defined(OSLIB_DEBUG) ---*/
	}
	put_local_capi_appl(LA);
	return ERR_NoError;
}
