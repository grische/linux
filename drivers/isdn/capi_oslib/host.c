// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/string.h>
#include <avm/capi_oslib/new.h>
#include <avm/capi_oslib/oslib.h>
#include <linux/semaphore.h>
#include <linux/hardirq.h>
#pragma GCC pop_options
#include "debug.h"
#include "capi_pipe.h"
#include "appl.h"
#include "host.h"
#include "ca.h"
#include "local_capi.h"

#define SYSTEM_ERROR(str, ret) { if (Status != 0) { DEB_ERR(str); return ret; } }
#define SYSTEM_ERROR_VOID(str) { if (Status != 0) { DEB_ERR(str); return; } }

struct capi_pipe CapiReceivePipe;
static DEFINE_SPINLOCK(Register_Release_Sema);

/**
 * lokale Funktionen
 */
#if !defined(NO_BCHANNEL)
void HOST_SET_DATA_B3_REQ_DATA(unsigned char *Msg, unsigned char *p);
unsigned int HOST_GET_DATA_B3_REQ_NCCI(unsigned char *Msg);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/**
 */
unsigned int HOST_INIT(enum _capi_source CapiSource, unsigned int AnzAppliktions,
		       unsigned int AnzNCCIs, unsigned int CAPI_INDEX)
{
	void         *Pointer;
	unsigned int  Len;
	unsigned int  Count;
	int           Status;
	int Lock = CAPI_LOCK;

	DEB_INFO("[%s] %u AnzApp %u AnzNCCIs %u Karte %u\n",
		 __func__, CapiSource, AnzAppliktions, AnzNCCIs, CAPI_INDEX);

	if (CapiSource != SOURCE_UNKNOWN) {
		LOCAL_CAPI_INIT(CapiSource);
		return 0;
	}

	AnzAppliktions += LOCAL_CAPI_APPLIKATIONS;
	AnzNCCIs       += LOCAL_CAPI_APPLIKATIONS * 2;

	/*--- AnzNCCIs wird nicht mehr gebraucht ---*/
	ApplData = (struct _ApplData *)CA_MALLOC(sizeof(struct _ApplData) * AnzAppliktions);
	if (ApplData == NULL) {
		DEB_ERR("[%s] %s: no memory for ApplData\n",
			__func__, capi_source_name[CapiSource]);
		return 0;
	}

	MaxApplData = AnzAppliktions;
	Karte       = CAPI_INDEX;

	for (Count = 0 ; Count < AnzAppliktions ; Count++) {
		spin_lock_init(&ApplData[Count].lock);
	}

	Len = 16 * 1024 * capi_oslib_stack->controllers;
	if (Len >= 0x10000)
		Len = 0x10000;

	Pointer = (void *)CA_MALLOC(Len);
	if (Pointer == NULL) {
		DEB_ERR("[%s] %s: no memory for CapiPutMessageQueue (%u bytes)\n",
			 __func__, capi_source_name[CapiSource], Len);
		return 0;
	}
	Status = Capi_Create_Pipe(&CapiReceivePipe, "P_Capi", Pointer, Len,
				  CAPI_VARIABLE_SIZE, MAX_CAPI_MESSAGE_SIZE, Lock);
	SYSTEM_ERROR("create receive pipe failed", 0);

	LOCAL_CAPI_INIT(CapiSource);

	DEB_INFO("[%s] ok\n", __func__);
	return 1;
}

/**
 */
void HOST_REGISTER(enum _capi_source CapiSource, unsigned int ApplId, unsigned int AnzahlMsgs,
		   unsigned int B3Connection, unsigned int B3Blocks, unsigned int SizeB3)
{
	HOST_RE_REGISTER(CapiSource, ApplId, 0, AnzahlMsgs, B3Connection, B3Blocks, SizeB3);
}

/**
 */
void HOST_RE_REGISTER(enum _capi_source CapiSource,
		       unsigned int ApplId,
		       unsigned int MapperId,
		       unsigned int AnzahlMsgs __attribute__((unused)),
		       unsigned int B3Connection,
		       unsigned int B3Blocks, unsigned int SizeB3)
{
	struct _ApplData *A;
	unsigned int      TmpMapperId;
	unsigned int      RegisterBlockSize = SizeB3; /*--- merken für CA_BLOCKSIZE() ---*/

	CA_MEM_SHOW();

	while (SizeB3 & (sizeof(unsigned int) - 1))
		SizeB3++; /*--- word align ---*/

	DEB_INFO("[%s] source %u ApplId %u AnzMsgs %u B3Conn %u WindowSize %u BlockSize %u\n",
		 __func__, CapiSource, ApplId, AnzahlMsgs, B3Connection, B3Blocks, SizeB3);

	/*--- Alle Release und Register Funktionen sind mit dier Semaphore geschützt ---*/
	spin_lock_bh(&Register_Release_Sema);

	/*--- existiert die ApplId schon ? ---*/
	TmpMapperId = Appl_Find_ApplId(CapiSource, ApplId);
	if (TmpMapperId != 0) {
		DEB_ERR("[%s] source %u ApplId %u: already there (MapperId %u)\n",
			__func__, CapiSource, ApplId, TmpMapperId);
		spin_unlock_bh(&Register_Release_Sema);
		return;
	}

	if (MapperId == 0) {
		/*--- neuen Eintrag suchen ---*/
		MapperId = Appl_Find_EmptyApplId();
		if (MapperId == 0) {
			DEB_ERR("[%s] source %u ApplId %u: no free entry\n",
				__func__, CapiSource, ApplId);
			spin_unlock_bh(&Register_Release_Sema);
			return;
		}
	}
	DEB_INFO("[%s] MapperId=%u\n", __func__, MapperId);

	/*--- freinen Eintrag gefunden ---*/
	A = &ApplData[MapperId - 1];

	appl_lock(A);

	A->ApplId            = ApplId;            /*--- host ApplId merken ---*/
	A->Nr                = MapperId - 1;      /*--- eigen Index merken ---*/
	A->NCCIs             = B3Connection;      /*--- maxNCCIs ---*/
	A->B3BlockSize       = SizeB3;            /*--- B3 Block Size der Applikation ---*/
	A->RegisterBlockSize = RegisterBlockSize; /*--- merken für CA_BLOCKSIZE() ---*/
	A->WindowSize        = B3Blocks;          /*--- maximal Windowsize (>= NCCI WindowSize)---*/
	A->CapiSource        = CapiSource;        /*--- welches capi ist gemeint ---*/

	A->CapiDataStruct = CA_MALLOC(capi_oslib_stack->cm_bufsize() * 1);   /*--- CAPI/Stack Datenstructur ---*/
	if (A->CapiDataStruct == NULL) {
		DEB_ERR("[%s] no memory for CapiDataStruct\n", __func__);
		appl_unlock(A);
		spin_unlock_bh(&Register_Release_Sema);
		return;
	}

	/*--- NCCIs Verwaltungsstructur ---*/
	A->NCCIData = (struct _NCCIData *)CA_MALLOC(sizeof(struct _NCCIData) * A->NCCIs);
	if (A->NCCIData == NULL) {
		DEB_ERR("[%s] no memory for NCCIData\n", __func__);
		CA_FREE(A->CapiDataStruct);
		appl_unlock(A);
		spin_unlock_bh(&Register_Release_Sema);
		return;
	}

	A->InUse = _entry_in_use_;
	if (Stack_Register) {
		Stack_Register(A->CapiDataStruct, MapperId);
	} else {
		DEB_INFO("[%s] source %u ApplId %u: Stack_Register not initialized\n",
			 __func__, CapiSource, ApplId);
	}

	appl_unlock(A);
	spin_unlock_bh(&Register_Release_Sema);

	{
		unsigned int Len;

		Len  = sizeof(struct _DataBlock) * 8;
		Len += (A->B3BlockSize + sizeof(unsigned int)) * 8;
		DEB_INFO("[%s]B3-ApplBlockSize should be %u bytes "
				 "(sizeof(struct _DataBlock)[%zu] * [8] + "
				 "(A->B3BlockSize[%u] + sizeof(unsigned int)[%zu]) * [8])\n",
				 __func__,
				 Len,
				 sizeof(struct _DataBlock),
				 A->B3BlockSize,
				 sizeof(unsigned int));
	}
}

/**
 */
unsigned int HOST_MAP_APPL_ID(enum _capi_source CapiSource, unsigned char *Msg)
{
	unsigned int ApplId = 0;
	unsigned int MapperId = 0;

	ApplId = Msg[2] | (Msg[3] << 8);
	MapperId = Appl_Find_ApplId(CapiSource, ApplId);
	if (MapperId == 0) {
		DEB_WARN("[%s] ApplId %d not registerd\n", __func__, ApplId);
		return 0;
	}
	Msg[2] = (unsigned char)MapperId;
	Msg[3] = (unsigned char)(MapperId >> 8);
	return MapperId;
}


/**
 */
#if !defined(NO_BCHANNEL)
void HOST_SET_DATA_B3_REQ_DATA(unsigned char *Msg, unsigned char *p)
{
	/*--- intel byte order ---*/
	Msg[12] = (unsigned char)((unsigned long)p >> 0);
	Msg[13] = (unsigned char)((unsigned long)p >> 8);
	Msg[14] = (unsigned char)((unsigned long)p >> 16);
	Msg[15] = (unsigned char)((unsigned long)p >> 24);
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/**
 */
#if !defined(NO_BCHANNEL)
unsigned int HOST_GET_DATA_B3_REQ_NCCI(unsigned char *Msg)
{
	unsigned int NCCI;

	NCCI = (Msg[8] | (Msg[9] << 8) | (Msg[10] << 16) | (Msg[11] << 24));
	return NCCI;
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/**
 */
enum _CapiErrors HOST_MESSAGE(enum _capi_source CapiSource,
			      unsigned char *Msg, unsigned char *Buffer)
{
	unsigned int Length;
	int          Status;
	unsigned int Suspend;

#if 0
	DEB_INFO("[%s] from '%s' start\n", __func__,
		CapiSource == SOURCE_UNKNOWN     ? "SOURCE_UNKNOWN" :
		CapiSource == SOURCE_LOCAL_CAPI  ? "SOURCE_LOCAL_CAPI" :
		CapiSource == SOURCE_DEV_CAPI    ? "SOURCE_DEV_CAPI" :
		CapiSource == SOURCE_SOCKET_CAPI ? "SOURCE_SOCKET_CAPI" : "unknown");
#endif

	if (HOST_MAP_APPL_ID(CapiSource, Msg) == 0) { /*--- Appl Id nicht mehr registriert ---*/
		DEB_WARN("[%s] Appl Id not registered\n", __func__);
		return ERR_IllegalApplId;
	}

	if (Buffer) {  /*--- wenn buffer dann DATA_B3_REQ ---*/
		HOST_SET_DATA_B3_REQ_DATA(Msg, Buffer);
	}

	Length = Msg[0] | (Msg[1] << 8);
	if (Length >= MAX_CAPI_MESSAGE_SIZE) {
		DEB_WARN("[%s] message too long (%u >= %u)\n",
			 __func__, Length, MAX_CAPI_MESSAGE_SIZE);
		return ERR_OS_Resource;
	}
	Suspend = CAPI_SUSPEND;
	Status = Capi_Send_To_Pipe(&CapiReceivePipe, Msg, Length, Suspend);
	if (Status == CAPI_PIPE_FULL) {
		DEB_WARN("[%s] pipe overflow (ERR_QueueFull)\n", __func__);
		return ERR_QueueFull;
	}
	SYSTEM_ERROR("send to CapiReceivePipe failed", ERR_OS_Resource);

	os_trigger_scheduler();

	/*--- DEB_INFO("[%s] done\n", __func__); ---*/
	return ERR_NoError;
}

/**
 */
void HOST_RELEASE(enum _capi_source CapiSource, unsigned int ApplId)
{
	struct _ApplData *A;
#if !defined(NO_BCHANNEL)
	struct _NCCIData *N;
#endif /*--- #if !defined(NO_BCHANNEL) ---*/
	unsigned int      Count;
	unsigned int      MapperId;

	DEB_INFO("[%s] source %u ApplId %u\n", __func__, CapiSource, ApplId);

	/*--- Alle Release und Register Funktionen sind mit dieser Semaphore geschützt ---*/
	spin_lock_bh(&Register_Release_Sema);

	MapperId = Appl_Find_ApplId(CapiSource, ApplId);     /*--- Mapper Id ermitteln ---*/
	if (MapperId == 0) {                 /*--- ist ApplId (noch) registriert ---*/
		spin_unlock_bh(&Register_Release_Sema);
		DEB_WARN("[%s] ApplId %u not registered\n", __func__, ApplId);
		return;
	}
	A = &ApplData[MapperId - 1];        /*--- Appl Structur aufgrund des Index ---*/
	A->InUse = _entry_not_used_;

	appl_lock(A);

	if (Stack_Release) {
		Stack_Release(A->CapiDataStruct);            /*--- Stack Release, aufräumen ... ---*/
	} else {
		DEB_INFO("[%s] source %u ApplId %u: Stack_Release not initialized\n",
			 __func__, CapiSource, ApplId);
	}

/*--- DebugPrintf("clear NCCIs "); ---*/
	/*--- für alle NCCIs prüfen und gg. freigeben ---*/
	for (Count = 0 ; Count < A->NCCIs ; Count++) {
#if !defined(NO_BCHANNEL)
		int n;

		if (A->NCCIData[Count].InUse == _entry_not_used_)
			continue;
		N = &A->NCCIData[Count];
		N->InUse    = _entry_not_used_;

		for (n = 0 ; n < (int)N->RxWindowSize ; n++) {
			N->RxBuffer[n].InUse  = _entry_not_used_;
			if ((A->ApplContextRelease == NULL) && (N->RxBuffer[n].Buffer)) {
				CA_FREE(N->RxBuffer[n].Buffer);
				N->RxBuffer[n].Buffer = NULL;
			}
		}
		for (n = 0 ; n < (int)N->TxWindowSize ; n++) {
			N->TxBuffer[n].InUse  = _entry_not_used_;
			if ((A->ApplContextRelease == NULL) && (N->TxBuffer[n].Buffer)) {
				CA_FREE(N->TxBuffer[n].Buffer);
				N->TxBuffer[n].Buffer = NULL;
			}
		}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/
	}
/*--- DEB_TRACE("free capi data "); ---*/
	if (A->CapiDataStruct) {               /*--- Capi Daten freigeben ---*/
		CA_FREE(A->CapiDataStruct);
		A->CapiDataStruct = NULL;
	}
/*--- DEB_TRACE("free NCCIs data "); ---*/
	if (A->NCCIData) {
		CA_FREE(A->NCCIData);              /*--- NCCI verwaltungs Structur freigeben ---*/
		A->NCCIData = NULL;
	}
/*--- DEB_TRACE("delete A->Sema "); ---*/

	appl_unlock(A);
	spin_unlock_bh(&Register_Release_Sema);

	LOCAL_CAPI_RELEASE_CONF(CapiSource, ApplId);
	HOST_RELEASE_B3_BUFFER(CapiSource, MapperId);
	DEB_INFO("[%s] done\n", __func__);

	CA_MEM_SHOW();
}

/**
 */
#if !defined(NO_BCHANNEL)
unsigned char *HOST_NEW_DATA_B3_REQ(enum _capi_source CapiSource,
				    unsigned char *Msg, unsigned int *MaxLength)
{
	unsigned int ApplId;
	unsigned int MapperId;
	unsigned int NCCI;
	struct _ApplData *A;

	NCCI = HOST_GET_DATA_B3_REQ_NCCI(Msg);   /*--- NCCI holen ---*/
	ApplId = Msg[2] | (Msg[3] << 8);         /*--- ApplId aus Message ---*/
	MapperId = Appl_Find_ApplId(CapiSource, ApplId);          /*--- MapperId ermitteln ---*/
	if (MapperId == 0) {                      /*--- ist Applikation (noch) registriert ---*/
		*MaxLength = 0;
		DEB_WARN("[%s] ApplId %u not registered\n", __func__, ApplId);
		return NULL;
	}
	A = &ApplData[MapperId - 1];        /*--- Appl Structur aufgrund des Index ---*/
	*MaxLength = A->B3BlockSize;

	return CA_NEW_DATA_B3_REQ(MapperId, NCCI);  /*--- Buffer anfordern ---*/
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/**
 */
/*--- #if !defined(ARM) && !defined(BLKFN) ---*/
/*--- void HOST_DO_POLL(void) { ---*/
	/*--- CA_POLL(); ---*/
/*--- } ---*/
/*--- #endif ---*/ /*--- #if !defined(ARM) && !defined(BLKFN) ---*/

/**
 */
int HOST_REGISTER_B3_BUFFER(enum _capi_source capi_source, unsigned int ApplId,
			    struct _adr_b3_ind_data *b3Buffers, unsigned int BufferAnzahl,
			    void (*release_buffers)(void *buf), void *Context)
{
	unsigned int MapperId;
	struct _ApplData *A;

	DEB_INFO("[%s] %s ApplId %u B3Buffer=0x%p Anzahl=%u release_buffers=0x%p Context=0x%p\n",
		 __func__, capi_source_name[capi_source], ApplId, b3Buffers, BufferAnzahl,
		 release_buffers, Context);

	MapperId = Appl_Find_ApplId(capi_source, ApplId);
	DEB_INFO("[%s] MapperId=%u\n", __func__, MapperId);

	if (MapperId) {
		unsigned int index = 0;
		int ncci, win;

		A = &ApplData[MapperId - 1];
		A->ApplContextRelease   = release_buffers;
		A->ApplContext          = Context;

		index = 0;
		for (ncci = 0 ; ncci < (int)A->NCCIs ; ncci++) {
			for (win = 0 ; (win < (int)A->WindowSize) && (index < BufferAnzahl); win++) {
				A->NCCIData[ncci].RxBuffer[win].Buffer = b3Buffers[index++].Kernel_Buffer;
			}
			if ((win < (int)A->WindowSize) && (index == BufferAnzahl)) {
				DEB_ERR("[%s] too little buffers\n", __func__);
				return -ENOMEM;
			}
		}
		return 0;
	}
	return -EFAULT;
}

/**
 */
int HOST_RELEASE_B3_BUFFER(enum _capi_source capi_source __attribute__((unused)), unsigned int MapperId)
{
	struct _ApplData *A;

	DEB_INFO("[%s] MapperId=%u\n", __func__, MapperId);

	if (MapperId) {
		A = &ApplData[MapperId - 1];

		if (A->ApplContextRelease == NULL) {
			return 0;
		}

		(*A->ApplContextRelease)(A->ApplContext);
		return 0;
	}
	return -EFAULT;
}
