
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/module.h>
#include <avm/capi_oslib/oslib.h>
#include <avm/capi_oslib/new.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#pragma GCC pop_options

#include "capi_pipe.h"
#include "appl.h"
#include "ca.h"
#include "host.h"
#include "debug.h"
#include "local_capi.h"

#define ARRAY_EL(a) (sizeof(a) / sizeof(a[0]))
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(FRITZX) && !defined(BT_ACCESS) && !defined(NO_BCHANNEL)
#define NO_BCHANNEL
#endif /*--- #if defined(FRITZX) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(OSLIB_DEBUG)
/*--- #define DEBUG_CA_C ---*/
/*--- #define DEBUG_CA_DATA ---*/
/*--- #define DEBUG_CA_NCCI ---*/
#define DEBUG_CA_C_ERROR
#endif /*--- #if !defined(OSLIB_DEBUG) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define SYSTEM_ERROR(str, ret)                                      \
									if(Status != 0) {      \
										printk(KERN_ERR str);  \
										return ret;                 \
									}
#define SYSTEM_ERROR_VOID(str)                                      \
									if(Status != 0) {      \
										printk(KERN_ERR str);  \
										return;                     \
									}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(OSLIB_DEBUG)
unsigned int CA_CHECK_APPL_ID(struct _ApplData *A, unsigned int MapperId);
#endif /*--- #if !defined(OSLIB_DEBUG) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned int CA_DATA_B3_IND_DATA_LENGTH(unsigned char *Msg) {
	unsigned int Length;
	/*--- nur CAPI 2.0 ---*/
	Length = Msg[16] | (Msg[17] << 8);
	return Length;
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned char *CA_DATA_B3_IND_DATA(unsigned char *Msg) {
	unsigned char *Data;
	/*--- nur CAPI 2.0 ---*/
	Data = (unsigned char *)((Msg[12] << 0) | (Msg[13] << 8) | (Msg[14] << 16) | (Msg[15] << 24));
	return Data;
}
EXPORT_SYMBOL(CA_DATA_B3_IND_DATA);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void CA_INIT(unsigned int Len, void (*Register)(void *, unsigned int), void (*Release)(void *), void (*Down)(void)) {
	DEB_INFO("[CA_INIT] Len=%u Register=0x%p Release=0x%p Down=0x%p\n", Len, Register, Release, Down);

	if(Len != capi_oslib_stack->cm_bufsize())
		printk(KERN_ERR "Len(%u) != CM_BufSize(%u)", Len, capi_oslib_stack->cm_bufsize());
	Stack_Register  = Register;
	Stack_Release   = Release;
	Stack_Shutdown  = Down;
}
EXPORT_SYMBOL(CA_INIT);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned int CA_NEW_NCCI(unsigned int MapperId, unsigned int NCCI, unsigned int WindowSize, unsigned int BlockSize __attribute__((unused))) {
	struct _ApplData *A;
	struct _NCCIData *N;
	unsigned int      Count;
	unsigned int      n;

	/*--- DEB_INFO("CA_NEW_NCCI: MapperId=%u NCCI=%x WindowSize=%u BlockSize=%d (unused): start\n", MapperId, NCCI, WindowSize, BlockSize); ---*/

	if(MapperId == 0) {
		DEB_ERR("CA_NEW_NCCI: MapperId == %u invallid\n", MapperId);
		return 0; /*--- ACHTUNG hier gilt 0 als Fehler ---*/
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_NEW_NCCI: MapperId %d not registered\n", MapperId);
		return 0; /*--- ACHTUNG hier gilt 0 als Fehler ---*/
	}

	appl_lock(A);
	if(A->InUse == _entry_not_used_) {
		appl_unlock(A);
		DEB_ERR("CA_NEW_NCCI: MapperId %d not registered (released)\n", MapperId);
		return 0; /*--- ACHTUNG hier gilt 0 als Fehler ---*/
	}

	if(Appl_Find_NCCIData(A, NCCI)) {
		appl_unlock(A);
		DEB_ERR("CA_NEW_NCCI: MapperId %u, NCCI %u already exists\n", MapperId, NCCI);
		return 0; /*--- ACHTUNG hier gilt 0 als Fehler ---*/
	}

	N = A->NCCIData; /*--- noch einmal von vorne ---*/

	for(Count = 0 ; Count < A->NCCIs ; Count++, N++) {
		if(N->InUse == _entry_not_used_) {  /*--- freier Eintrag gefunden ---*/
			N->NCCI         = NCCI;
			N->RxWindowSize = min(WindowSize, ARRAY_EL(N->RxBuffer));
			/*-------------------------------------------------------------------------*\
				Rx Buffer Anzahl=WindowSize
			\*-------------------------------------------------------------------------*/
			for(n = 0 ; n < N->RxWindowSize ; n++) {
				N->RxBuffer[n].InUse  = _entry_not_used_;
				if(A->ApplContextRelease == NULL) {
					N->RxBuffer[n].Buffer = CA_MALLOC(A->B3BlockSize);
				}
#if !defined(NDEBUG)
				if(N->RxBuffer[n].Buffer == NULL)
					DEB_WARN("[CA_NEW_NCCI] RxBuffer[%u] = NULL (%s)\n", n, current->comm);
				else
					DEB_INFO("[CA_NEW_NCCI] RxBuffer[%u] = 0x%p\n", n, N->RxBuffer[n].Buffer);
#endif /*--- #if !defined(NDEBUG) ---*/
			}

			/*-------------------------------------------------------------------------*\
				Tx Buffer Anzahl=WindowSize mindestens aber 8
			\*-------------------------------------------------------------------------*/
			if(WindowSize < 8) {
				N->TxWindowSize = 8;
			} else {
				N->TxWindowSize = min(WindowSize, ARRAY_EL(N->TxBuffer));
			}
			A->AllocBlockSize = max((unsigned int)A->B3BlockSize, (unsigned int)B3_DATA_ALLOC_SIZE);

			for(n = 0 ; n < N->TxWindowSize ; n++) {
				N->TxBuffer[n].InUse  = _entry_not_used_;
				if(A->ApplContextRelease == NULL) {
					N->TxBuffer[n].Buffer = CA_MALLOC(A->AllocBlockSize);
				}
#if !defined(NDEBUG)
				if(N->TxBuffer[n].Buffer == NULL)
					DEB_WARN("[CA_NEW_NCCI] TxBuffer[%u] = NULL (%s)\n", n, current->comm);
				else
					DEB_INFO("[CA_NEW_NCCI] TxBuffer[%u] = 0x%p\n", n, N->TxBuffer[n].Buffer);
#endif /*--- #if !defined(NDEBUG) ---*/
			}
			N->InUse      = _entry_in_use_;
			break;
		}
	}
	appl_unlock(A);

	if(Count == A->NCCIs) {
		DEB_NOTE("CA_NEW_NCCI: no empty NCCI-Entry found (MapperId %u)\n", MapperId);
		return 0; /*--- ACHTUNG hier gilt 0 als Fehler ---*/
	}

	/*--- DEB_INFO("CA_NEW_NCCI: done\n"); ---*/

	return 1; /*--- ACHTUNG hier gilt 1 als succes ---*/
}
EXPORT_SYMBOL(CA_NEW_NCCI);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
void CA_FREE_NCCI(unsigned int MapperId, unsigned int NCCI) {
	struct _ApplData *A;
	struct _NCCIData *N;
	unsigned int      ApplId;
	unsigned int      CapiSource;

	DEB_INFO("CA_FREE_NCCI: start NCCI=%x ", NCCI);

	if(MapperId == 0) {
		DEB_WARN("CA_FREE_NCCI: MapperId == %u invallid\n", MapperId);
		return;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_WARN("CA_FREE_NCCI: MapperId %d not registered\n", MapperId);
		return;
	}

	/*--- wenn es sich um ein release handelt ist A->* anschließend nicht mehr gültig ----*/
	ApplId     = A->ApplId;
	CapiSource = A->CapiSource;

	if(NCCI == (unsigned int)-1) {
		HOST_RELEASE(CapiSource, ApplId);
	} else {
		int n;

		appl_lock(A);
		if(A->InUse == _entry_not_used_) {
			appl_unlock(A);
			DEB_WARN("CA_FREE_NCCI: MapperId %d not registered (released)\n", MapperId);
			return;
		}

		N = Appl_Find_NCCIData(A, NCCI);
		if(N == NULL) {
			appl_unlock(A);
			DEB_WARN("CA_FREE_NCCI: MapperId %d, NCCI %u: NCCI not found\n", MapperId, NCCI);
			return;
		}

		N->InUse    = _entry_not_used_;
		for(n = 0 ; n < (int)N->RxWindowSize ; n++) {
			N->RxBuffer[n].InUse  = _entry_not_used_;
			if((A->ApplContextRelease == NULL) && (N->RxBuffer[n].Buffer)) {
				CA_FREE(N->RxBuffer[n].Buffer);
				N->RxBuffer[n].Buffer = NULL;
			}
		}
		for(n = 0 ; n < (int)N->TxWindowSize ; n++) {
			N->TxBuffer[n].InUse  = _entry_not_used_;
			if((A->ApplContextRelease == NULL) && (N->TxBuffer[n].Buffer)) {
				CA_FREE(N->TxBuffer[n].Buffer);
				N->TxBuffer[n].Buffer = NULL;
			}
		}
		appl_unlock(A);
	}

	DEB_INFO("CA_FREE_NCCI: done ");
}
EXPORT_SYMBOL(CA_FREE_NCCI);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#else /*--- #if !defined(NO_BCHANNEL) ---*/
void CA_FREE_NCCI(unsigned int MapperId, unsigned int NCCI) {
	struct _ApplData *A;

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_WARN("CA_FREE_NCCI: MapperId %d not registered\n", MapperId);
		return;
	}

	if(NCCI == (unsigned int)-1) {
		HOST_RELEASE(A->CapiSource, A->ApplId);
	}

	DEB_INFO("CA_FREE_NCCI: done ");
}
EXPORT_SYMBOL(CA_FREE_NCCI);
#endif /*--- #else ---*/ /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
	Buffer aus Rx Pool holen
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned char *CA_NEW_DATA_B3_IND(unsigned int MapperId, unsigned int NCCI, unsigned int Index) {
	struct _ApplData *A;
	struct _NCCIData *N;
	unsigned char    *Buffer;

	/*--- DEB_INFO("CA_NEW_DATA_B3_IND: start "); ---*/

	if(MapperId == 0) {
		DEB_WARN("CA_NEW_DATA_B3_IND: MapperId == 0 invallid\n");
		return NULL;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_WARN("CA_NEW_DATA_B3_IND: MapperId %d not registered\n", MapperId);
		return NULL;
	}

	appl_lock(A);
	if(A->InUse == _entry_not_used_) {
		appl_unlock(A);
		DEB_ERR("CA_NEW_DATA_B3_IND: MapperId %d not registered (released)\n", MapperId);
		return NULL;
	}

	N = Appl_Find_NCCIData(A, NCCI);
	if(N == NULL) {
		appl_unlock(A);
		DEB_WARN("CA_NEW_DATA_B3_IND: MapperId %u: NCCI %u not found\n", MapperId, NCCI);
		return NULL;
	}

	if(N->RxBuffer[Index].InUse == _entry_not_used_) {
		N->RxBuffer[Index].InUse = _entry_in_use_;
		Buffer = N->RxBuffer[Index].Buffer;
	} else {
		Buffer = NULL;
		DEB_ERR("CA_NEW_DATA_B3_IND: MapperId %u, NCCI %u no buffer left\n", MapperId, NCCI);
	}

	appl_unlock(A);
	/*--- DEB_INFO("CA_NEW_DATA_B3_IND: done "); ---*/
	return Buffer;
}
EXPORT_SYMBOL(CA_NEW_DATA_B3_IND);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
	Buffer aus Tx Pool holen
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned char *CA_NEW_DATA_B3_REQ(unsigned int MapperId, unsigned int NCCI) {
	struct _ApplData *A;
	struct _NCCIData *N;
	unsigned char    *Buffer;
	unsigned int      Index;

	/*--- DEB_INFO("CA_NEW_DATA_B3_REQ: start "); ---*/

	if(MapperId == 0) {
		DEB_WARN("CA_NEW_DATA_B3_REQ: MapperId == %u invallid\n", MapperId);
		return NULL;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_WARN("CA_NEW_DATA_B3_REQ: MapperId %d not registered\n", MapperId);
		return NULL;
	}

	appl_lock(A);
	if(A->InUse == _entry_not_used_) {
		appl_unlock(A);
		DEB_WARN("CA_NEW_DATA_B3_REQ: MapperId %d not registered (released)\n", MapperId);
		return NULL;
	}

	N = Appl_Find_NCCIData(A, NCCI);
	if(N == NULL) {
		appl_unlock(A);
		DEB_ERR("CA_NEW_DATA_B3_REQ: MapperId %d NCCI %x not found\n", MapperId, NCCI);
		return NULL;
	}

	if(A->ApplContextRelease) {
		appl_unlock(A);
		DEB_ERR("CA_NEW_DATA_B3_REQ: ERROR: MapperId %d NCCI %x: Illegal Call to CA_NEW_DATA_B3_REQ\n", MapperId, NCCI);
		return NULL;
	}

	Buffer = NULL;
	for(Index = 0 ; Index < N->TxWindowSize ; Index++) {
		if(N->TxBuffer[Index].InUse == _entry_not_used_) {
			N->TxBuffer[Index].InUse = _entry_in_use_;
			Buffer = N->TxBuffer[Index].Buffer;
			break;
		}
	}
#if !defined(NDEBUG)
	if(Index == N->TxWindowSize)
		DEB_WARN("CA_NEW_DATA_B3_REQ: MapperId %d NCCI %x no free buffer\n", MapperId, NCCI);
#endif/*--- #if !defined(NDEBUG) ---*/

	appl_unlock(A);
	/*--- DEB_INFO("CA_NEW_DATA_B3_REQ: done "); ---*/
	return Buffer;
}
EXPORT_SYMBOL(CA_NEW_DATA_B3_REQ);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if 0  /*--- wird nicht genutzt da stack buffer selber verwaltet ---*/
#if !defined(NO_BCHANNEL)
void CA_FREE_DATA_B3_IND(unsigned int MapperId, unsigned char *Data) {
	struct _ApplData *A;
	struct _NCCIData *N;
	unsigned int      Count;
	unsigned int      n;
	struct _critical  C;

	DEB_INFO("CA_FREE_DATA_B3_IND: start ");

	if(MapperId == 0) {
		DEB_ERR("CA_FREE_DATA_B3_IND: MapperId == 0 invallid\n", MapperId);
		return;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_FREE_DATA_B3_IND: MapperId %d not registered\n", MapperId);
		return;
	}

	appl_lock(A);
	if(A->InUse == _entry_not_used_) {
		up(&A->Sema);
		DEB_ERR("CA_FREE_DATA_B3_IND: MapperId %d not registered (released)\n", MapperId);
		return;
	}

	N = A->NCCIData;
	for(n = 0 ; n < A->NCCIs ; n++, N++) {
		for(Count = 0 ; Count < N->RxWindowSize ; Count++) {
			if(N->RxBuffer[Count].Buffer == Data) {
				N->RxBuffer[Count].InUse = _entry_not_used_;
				up(&A->Sema);
				DEB_INFO("CA_FREE_DATA_B3_IND: done ");
				return;
			}
		}
	}
	appl_unlock(A);
	DEB_INFO("CA_FREE_DATA_B3_IND: MapperId %d Data 0x%x not found\n", MapperId, (unsigned int)Data);
	return;
}
EXPORT_SYMBOL(CA_FREE_DATA_B3_IND);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/
#endif

/*-------------------------------------------------------------------------------------*\
	Sendedatenbuffer wird nicht mehr benötigt. Freigeben zur weiteren Verwendung
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
void CA_FREE_DATA_B3_REQ(unsigned int MapperId, unsigned char *Data) {
	struct _ApplData *A;
	struct _NCCIData *N;
	unsigned int      Count;
	unsigned int      n;

	/*--- DEB_INFO("CA_FREE_DATA_B3_REQ: start\n"); ---*/

	if(MapperId == 0) {
		DEB_ERR("CA_FREE_DATA_B3_REQ: MapperId == %u invallid\n", MapperId);
		return;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_FREE_DATA_B3_REQ: MapperId %d not registered\n", MapperId);
		return;
	}

	appl_lock(A);
	if(A->InUse == _entry_not_used_) {
		appl_unlock(A);
		DEB_ERR("CA_FREE_DATA_B3_REQ: MapperId %d not registered (released)\n", MapperId);
		return;
	}

	if(A->ApplContextRelease) {
		appl_unlock(A);
		/*--- DEB_INFO("CA_FREE_DATA_B3_REQ: MapperId %d: ignore CA_FREE_DATA_B3_REQ\n", MapperId); ---*/
		/*--- DEB_INFO("[b3_req] Data=0x%p\n", Data); ---*/
		return;
	}

	N = A->NCCIData;

	for(n = 0 ; n < A->NCCIs ; n++, N++) {
		if(N->InUse == _entry_not_used_) {
			/*--- DEB_INFO("[%d] NCCI %u not in use\n", MapperId - 1, n); ---*/
			continue;
		}
		for(Count = 0 ; Count < N->TxWindowSize ; Count++) {
			if(N->TxBuffer[Count].Buffer == Data) {
				N->TxBuffer[Count].InUse = _entry_not_used_;
				appl_unlock(A);
				/*--- DEB_INFO("CA_FREE_DATA_B3_REQ: MapperId %d Data 0x%x freeed\n", MapperId, (unsigned int)Data); ---*/
				return;
			}
		}
	}
	appl_unlock(A);
	DEB_WARN("CA_FREE_DATA_B3_REQ: MapperId %d Data 0x%x not found\n", MapperId, (unsigned int)Data);

	return;
}
EXPORT_SYMBOL(CA_FREE_DATA_B3_REQ);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
 * Kontext: Capi-Scheduler
\*-------------------------------------------------------------------------------------*/
unsigned int CA_GET_MESSAGE(unsigned char *Msg) {
	int          Status;
	unsigned long ReceivedLength;

	Status = Capi_Receive_From_Pipe(&CapiReceivePipe, Msg, MAX_CAPI_MESSAGE_SIZE, &ReceivedLength, CAPI_NO_SUSPEND);
	if(Status == CAPI_PIPE_EMPTY) {
		/*--- DEB_TRACE("CA_GET_MESSAGE: empty\n\r"); ---*/
		return 0;
	}
	SYSTEM_ERROR("receive from CapiReceivePipe failed", 0);
	/*--- DEB_INFO("CA_GET_MESSAGE: %d bytes (%x, %x)\n\r", (unsigned int)ReceivedLength, Msg[4], Msg[5]); ---*/
	return (unsigned int)ReceivedLength;
}
EXPORT_SYMBOL(CA_GET_MESSAGE);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void CA_PUT_MESSAGE(unsigned char *Msg) {
	unsigned int      MapperId;
	unsigned int      MsgLen;
	struct _ApplData *A;

	MapperId = Msg[2] | (Msg[3] << 8);
	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_PUT_MESSAGE: MapperId %d not registered\n", MapperId);
		return;
	}

	Msg[2] = (unsigned char)A->ApplId;
	Msg[3] = (unsigned char)(A->ApplId >> 8);
	MsgLen = Msg[0] | (Msg[1] << 8);

	/*--- DEB_TRACE("CA_PUT_MESSAGE: %d bytes to 'Local'\r", MsgLen); ---*/
	LOCAL_CAPI_MESSAGE(A->CapiSource, A->ApplId, Msg, MsgLen);
	return;
}
EXPORT_SYMBOL(CA_PUT_MESSAGE);


/*-------------------------------------------------##########################*\
\*---------------------------------------------------------------------------*/
unsigned int CA_KARTE(void) {
	return Karte;
}
EXPORT_SYMBOL(CA_KARTE);

/*-------------------------------------------------##########################*\
\*---------------------------------------------------------------------------*/
unsigned int CA_KARTEN_ANZAHL(void) {
	return capi_oslib_stack->controllers;
}
EXPORT_SYMBOL(CA_KARTEN_ANZAHL);

/*-------------------------------------------------##########################*\
\*---------------------------------------------------------------------------*/
void CA_VERSION (unsigned char *Version) {
	unsigned int  Controller;
	unsigned int  Count;

	for(Controller = 0 ; Controller < capi_oslib_stack->controllers; Controller++, Version += 256) {
		/*-----------------------------------------------------------------------------*\
			patchen des Strings "C4" in "C[MAX_CONTROLLERS]"
		\*-----------------------------------------------------------------------------*/
		for(Count = 0 ; Count < 255 ; Count++) {
			if(Version[Count] == 'C' && Version[Count + 1] == '4') {
				Version[Count + 1] = '0' + capi_oslib_stack->controllers;
				break;
			}
		}
	}
}
EXPORT_SYMBOL(CA_VERSION);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned int CA_BLOCKSIZE(unsigned int MapperId) {
	struct _ApplData *A;

	MapperId &= 0xFFFF;

	if(MapperId == 0) {
		DEB_ERR("CA_BLOCKSIZE: MapperId == 0 invallid\n");
		return 0;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_BLOCKSIZE: MapperId %d registered\n", MapperId);
		return 0;
	}

	return A->RegisterBlockSize;
}
EXPORT_SYMBOL(CA_BLOCKSIZE);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(OSLIB_DEBUG)
unsigned int CA_CHECK_APPL_ID(struct _ApplData *A, unsigned int MapperId) {
	unsigned int Count;

	if(A->WindowSize < 2) {
		goto CA_CHECK_APPL_ID_Fehler;
	}
	if(A->B3BlockSize < 128) {
		goto CA_CHECK_APPL_ID_Fehler;
	}
	if(*((unsigned short *)A->CapiDataStruct) != (unsigned short)MapperId) {
		goto CA_CHECK_APPL_ID_Fehler;
	}
	return 1; /*--- ACHTUNG hier gilt 1 als succes ---*/

	/*---------------------------------------------------------------------------------*\
	\*---------------------------------------------------------------------------------*/
CA_CHECK_APPL_ID_Fehler:
#if defined(DEBUG_CA_C_ERROR)
	DEB_ERR("\nCA_CHECK_APPL_ID: ERROR !:\n");
	DEB_ERR("BlockSize %u WindowSize %u NCCIs %u\n", A->B3BlockSize, A->WindowSize, A->NCCIs);
	DEB_ERR("ApplId %u, MapperId %u, Host %u InUse %u\n", A->Nr + 1, MapperId, A->ApplId, A->InUse);
	DEB_ERR("CapiData: ");
	for(Count = 0 ; Count < capi_oslib_stack->cm_bufsize() ; Count++)
		printk("%02X ", ((unsigned char *)A->CapiDataStruct)[Count]);
	printk("\n");
#endif /*--- #if defined(DEBUG_CA_C_ERROR) ---*/
	return 0;
}
#endif /*--- #if defined(OSLIB_DEBUG) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned int CA_WINDOWSIZE(unsigned int MapperId) {
	struct _ApplData *A;

	MapperId &= 0xFFFF;

	if(MapperId == 0) {
		DEB_ERR("CA_WINDOWSIZE: MapperId == 0 invallid\n");
		return 0;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_WINDOWSIZE: MapperId %d registered\n", MapperId);
		return 0;
	}

	return A->WindowSize;
}
EXPORT_SYMBOL(CA_WINDOWSIZE);
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned char *CA_PARAMS(void) {
	return NULL;
}
EXPORT_SYMBOL(CA_PARAMS);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void *CA_APPLDATA (unsigned int MapperId) {
	struct _ApplData *A;

	MapperId &= 0xFFFF;

	if(MapperId == 0) {
		DEB_ERR("CA_APPLDATA: MapperId == 0 invallid\n");
		return NULL;
	}

	A = &ApplData[MapperId - 1];
	if(A->InUse == _entry_not_used_) {
		DEB_ERR("CA_APPLDATA: MapperId %d registered\n", MapperId);
		return NULL;
	}

	return A->CapiDataStruct;
}
EXPORT_SYMBOL(CA_APPLDATA );

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _ApplsFirstNext DummyNullEintrag = { 0, NULL };

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _ApplsFirstNext *CA_APPLDATA_FIRST(struct _ApplsFirstNext *s) {
	s->Nr = (unsigned int)-1;

	/*--- wird in APPLDATA_NEXT zuerst erhöht also auf 0 gesetzt ---*/
	return CA_APPLDATA_NEXT(s);
}
EXPORT_SYMBOL(CA_APPLDATA_FIRST);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _ApplsFirstNext *CA_APPLDATA_NEXT(struct _ApplsFirstNext *s) {
	struct _ApplData *A;

	do {
		s->Nr++;
		A = &ApplData[s->Nr];

		if(A->InUse == _entry_in_use_) {
			s->Buffer = (unsigned char *)A->CapiDataStruct;
			return s;
		}
	} while(s->Nr < (MaxApplData - 1));

	if(DummyNullEintrag.Buffer == NULL) {
		DummyNullEintrag.Buffer = CA_MALLOC(capi_oslib_stack->cm_bufsize());
		memset(DummyNullEintrag.Buffer, 0, capi_oslib_stack->cm_bufsize());
	}
	memcpy(s, &DummyNullEintrag, sizeof(DummyNullEintrag));
	return NULL;
}
EXPORT_SYMBOL(CA_APPLDATA_NEXT);

