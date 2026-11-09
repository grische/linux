#ifndef _APPL_H_
#define _APPL_H_

#include <linux/semaphore.h>
#include "consts.h"
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define LOCAL_CAPI_APPLIKATIONS             20
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _entry_use {
	_entry_not_used_        = 0,
	_entry_in_use_          = 1,
	_entry_register_pending = 2,
	_entry_release_pending  = 3
};

enum _capi_source {
	SOURCE_UNKNOWN     = 0,  /*--- fuer globale initialisierung ---*/
	SOURCE_PTR_CAPI    = 1,  /*--- new /dev/capi20 ---*/
	SOURCE_DEV_CAPI    = 2,  /*--- old /dev/capi20 ---*/
	SOURCE_SOCKET_CAPI = 3,
	SOURCE_KERNEL_CAPI = 4,
	SOURCE_ANZAHL      = 5
};

extern char *capi_source_name[];

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _DataBlock {
	enum _entry_use    InUse;               /*--- B3 Daten Block ---*/
	unsigned char     *Buffer;
};

struct _NCCIData {                          /* NCCI Verwaltungsstruktur                */
	enum _entry_use    InUse;               /* Flag, TRUE wenn angelegt                */
	unsigned int       NCCI;                /* Wert des NCCIs                          */
	unsigned int       RxWindowSize;        /* aktuelle Windowsize des NCCIs           */
	unsigned int       TxWindowSize;        /* aktuelle Windowsize des NCCIs (min 8)   */
	struct _DataBlock  RxBuffer[8];         /* RxBuffer[WindowSize] Receive Buffer     */    
	struct _DataBlock  TxBuffer[8];         /* RxBuffer[WindowSize] Send Buffer        */    
};

struct _ApplData {
	enum _entry_use    InUse;               /* TRUE wenn registriert                   */
	unsigned int       Nr;                  /* Index in ApplData[MAX_APPL_IDS]         */
	unsigned int       ApplId;              /* Host ApplId                             */
	unsigned int       NCCIs;               /* Max NCCIs der ApplId                    */
	unsigned int       B3BlockSize;         /* Blocksize der ApplId (%4 aufgerundet)   */
	unsigned int       RegisterBlockSize;   /* Blocksize der ApplId (original)         */
	unsigned int       AllocBlockSize;      /* Alloziierte Blocksize                   */
	unsigned int       WindowSize;          /* Maximal WindowSize >= NCCIWindowsize    */
	enum _capi_source  CapiSource;          /* Capi Source HOST / ONBOARD              */
	spinlock_t         lock; 
	void              *CapiDataStruct;      /* Stack Daten (_struct _cap20_appl)       */
	struct _NCCIData  *NCCIData;            /* Array von Verwaltungsstrukturen         */
	void             (*ApplContextRelease)(void *);     
	void              *ApplContext;
};

struct _ApplsFirstNext{
	unsigned int  Nr;
	unsigned char *Buffer;
};

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
extern struct _ApplData *ApplData;
extern unsigned int MaxApplData;            /* Array, große bei Treiber Init gesetzt   */
extern unsigned int Karte;                  /* CAPI_INDEX                              */

extern void (*Stack_Register)(void *, unsigned int); /* Callback Register im Stack */
extern void (*Stack_Release)(void *); /* Callback Release im Stack               */
extern void (*Stack_Shutdown)(void);        /* Callback in Stack (E1_Exit)             */


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _NCCIData  *Appl_Find_NCCIData(struct _ApplData *A, unsigned int NCCI);
unsigned int Appl_Find_EmptyApplId(void);
unsigned int Appl_Find_ApplId(enum _capi_source CapiSource, unsigned int ApplId);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void Debug_PrintAppls(unsigned int MapperId);
char *Appl_PrintOneAppl(struct _ApplData *A);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void appl_lock(struct _ApplData *A) {
	spin_lock_bh(&A->lock);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void appl_unlock(struct _ApplData *A) {
	spin_unlock_bh(&A->lock);
}
#endif /*--- #ifndef _APPL_H_ ---*/
