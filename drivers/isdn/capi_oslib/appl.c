// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/string.h>
#pragma GCC pop_options

#include "debug.h"
#include "appl.h"
#include "ca.h"

struct _ApplData *ApplData;
unsigned int MaxApplData;             /* Array, groﬂe bei Treiber Init gesetzt   */
unsigned int Karte;                   /* CAPI_INDEX                              */
static DEFINE_SPINLOCK(ApplData_Sema);
/*--- NU_SEMAPHORE S_ToLink; ---*/                      /* Semaphore zum Senden auf Linkinterface  */

void (*Stack_Register)(void *adata, unsigned int Mapper); /*  Callback Register im Stack       */
void (*Stack_Release)(void *adata);        /* Callback Release im Stack               */
void (*Stack_Shutdown)(void);               /* Callback in Stack (E1_Exit)             */

/**
 */
unsigned int Appl_Find_ApplId(enum _capi_source CapiSource, unsigned int ApplId)
{
	unsigned int Count;
	struct _ApplData *A;

	spin_lock_bh(&ApplData_Sema);
	A = &ApplData[0];
	for (Count = 0 ; Count < MaxApplData ; Count++) {
		if (A->InUse == _entry_in_use_ && A->ApplId == ApplId && A->CapiSource == CapiSource) {
			spin_unlock_bh(&ApplData_Sema);
			return Count + 1;
		}
		A++;
	}
	spin_unlock_bh(&ApplData_Sema);
	return 0;
}

/**
 */
unsigned int Appl_Find_EmptyApplId(void)
{
	unsigned int Count;

	spin_lock_bh(&ApplData_Sema);
	for (Count = 0 ; Count < MaxApplData ; Count++) {
		if (ApplData[Count].InUse == _entry_not_used_) {
			ApplData[Count].InUse = _entry_register_pending;
			spin_unlock_bh(&ApplData_Sema);
			return Count + 1;
		}
	}
	spin_unlock_bh(&ApplData_Sema);
	return 0;
}

/**
 */
struct _NCCIData  *Appl_Find_NCCIData(struct _ApplData *A, unsigned int NCCI)
{
	struct _NCCIData *N;
	unsigned int      Count;

	N = &A->NCCIData[0];
	if (N == NULL)
		return NULL;

	for (Count = 0 ; Count < A->NCCIs ; Count++) {
		if (N->InUse == _entry_in_use_ && N->NCCI == NCCI)
			return N;
		N++;
	}
	return NULL;
}

/**
 */
char *Appl_PrintOneAppl(struct _ApplData *A)
{
	struct _NCCIData *N;
	unsigned int Count;
	static char Buffer[256];
	int len;
	unsigned int maxlen = sizeof(Buffer);
	char *p = Buffer;

	len = snprintf(p, maxlen, "MapperId %u (Host %u", A->Nr + 1, A->ApplId);
	if (len > 0) {
		p += len, maxlen -= len;
	}
	len = snprintf(p, maxlen, ") NCCIs %u B3BlockSize %u WindowSize %u", A->NCCIs, A->B3BlockSize, A->WindowSize);
	if (len > 0) {
		p += len, maxlen -= len;
	}
	N = A->NCCIData;
	if (N == NULL) {
		return Buffer;
	}
	for (Count = 0 ; Count < A->NCCIs ; Count++, A++) {
		if (N->InUse == _entry_not_used_)
			continue;
		len = snprintf(p, maxlen, "\nNCCI 0x%8x WindowSize %u/%u ", N->NCCI, N->RxWindowSize, N->TxWindowSize);
		if (len > 0) {
			p += len, maxlen -= len;
		}
		len = snprintf(p, maxlen, "Rx[%c%c%c%c%c%c%c%c] ",
			(N->RxWindowSize > 0) ? (N->RxBuffer[0].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 1) ? (N->RxBuffer[1].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 2) ? (N->RxBuffer[2].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 3) ? (N->RxBuffer[3].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 4) ? (N->RxBuffer[4].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 5) ? (N->RxBuffer[5].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 6) ? (N->RxBuffer[6].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->RxWindowSize > 7) ? (N->RxBuffer[7].InUse == _entry_in_use_ ? 'A' : 'F') : '.'
		);
		if (len > 0) {
			p += len, maxlen -= len;
		}
		len = snprintf(p, maxlen, "Tx[%c%c%c%c%c%c%c%c]\n",
			(N->TxWindowSize > 0) ? (N->TxBuffer[0].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 1) ? (N->TxBuffer[1].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 2) ? (N->TxBuffer[2].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 3) ? (N->TxBuffer[3].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 4) ? (N->TxBuffer[4].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 5) ? (N->TxBuffer[5].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 6) ? (N->TxBuffer[6].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
			(N->TxWindowSize > 7) ? (N->TxBuffer[7].InUse == _entry_in_use_ ? 'A' : 'F') : '.'
		);
		if (len > 0) {
			p += len, maxlen -= len;
		}
	}
	return Buffer;
}

/**
 */
void Debug_PrintAppls(unsigned int MapperId)
{
	unsigned int Count;
	struct _ApplData *A;

	if (MapperId > 0) {
		A = &ApplData[MapperId - 1];
		if (A->InUse == _entry_not_used_)
			pr_err("\nMapperId %d not registered", MapperId);
#ifndef NDEBUG
		else
			DEB_INFO("%s", Appl_PrintOneAppl(A));
#endif/*--- #ifndef NDEBUG ---*/
		return;
	}
	A = &ApplData[0];
	for (Count = 0 ; Count < MaxApplData ; Count++, A++) {
#ifndef NDEBUG
		if (A->InUse == _entry_in_use_)
			DEB_INFO("%s", Appl_PrintOneAppl(A));
#endif/*--- #ifndef NDEBUG ---*/
	}
}
