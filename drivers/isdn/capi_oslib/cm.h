/*---------------------------------------------------------------------------*\

 * $Id: cm.h 1.4 2002/11/15 12:17:30Z mpommerenke Exp $

 * $Log: cm.h $
 * Revision 1.4  2002/11/15 12:17:30Z  mpommerenke
 * - MaPom: Anpassungen f¸r DSL_2AB
 * Revision 1.3  2001/12/05 13:59:52Z  strevisany
 * Revision 1.2  2001/07/10 07:53:01Z  MPommerenke
 * Revision 1.1  2001/07/04 15:34:13Z  MPommerenke
 * Initial revision
 * Revision 1.1  2001/07/04 11:05:14Z  MPommerenke
 * Initial revision
 * Revision 1.1  2001/06/11 12:47:59Z  MPommerenke
 * Initial revision
 * Revision 1.4  2000/10/25 12:25:52Z  MPommerenke
 * - MaPom: link Erweiterung f¸r Power down
 * Revision 1.13  2000/10/25 10:17:19Z  HJOrtmann
 * Revision 1.12  2000/10/25 09:42:36Z  HJOrtmann
 * Revision 1.11  2000/07/27 12:00:16Z  HJOrtmann
 * - HJO/Diego: Kompatibilit‰t
 * Revision 1.10  2000/07/26 14:23:49Z  HJOrtmann
 * Revision 1.9  2000/07/21 10:04:28Z  HJOrtmann
 * - HJO: ScheduleControl/Powermanagement
 * Revision 1.8  2000/07/13 13:23:42Z  HJOrtmann
 * - HJO: Vorbereitungen Powermanagement (C_PASSIV)
 * Revision 1.7  2000/04/04 14:50:01Z  DFriedel
 * - Diego/OStoyke: MS extension __stdcall usw. f¸r __linux__ wegdefiniert
 *                                    CM_GetRevision nicht mehr notwendig
 * Revision 1.6  2000/03/27 10:55:25Z  OStoyke
 * DFriedel/OStoyke: Linux-Anpassungen (CM_HandleEvents, CM_GetRevision)
 * Revision 1.5  1999/09/08 10:24:48Z  MPommerenke
 * - MaPom: RTOS korrektur
 * Revision 1.4  1999/09/06 14:27:21Z  MPommerenke
 * - MaPom: defines f¸r C4 in RTOS/C4 ge‰ndert
 * Revision 1.3  1999/06/04 12:56:13Z  HJOrtmann
 * - HJO/MaPom: C4
 * Revision 1.2  1997/10/10 12:49:52  DFriedel
 * - Diego: RÅckgabewert fÅr CM_HandleEvents wg. unified Deklaration
 * Revision 1.1  1996/03/08 16:22:36  HJORTMANN
 * Initial revision

\*---------------------------------------------------------------------------*/
#ifndef _cm_h
#define _cm_h

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (C_PASSIV)
extern unsigned	CM_Start (void);
extern char	*CM_Init (unsigned BaseAdr, unsigned IRQ);
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#elif defined (_transputer)
#include <chan.h>
extern unsigned	CM_Start (void);
extern unsigned char	*CM_Init (CHAN *Irq, unsigned char *Stack, unsigned Size);
extern void	CM_Switch_Led(unsigned LedON, unsigned Led);
extern char 	PATCH_FAXPOLL2[12];	
extern unsigned T1T_LedExitCause;
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#elif defined (C4) || defined (ARM) || defined(BLKFN)
extern unsigned	CM_Start (void);
extern unsigned char	*CM_Init (void);
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#else
#error UNKNOWN OS ENVIRONMENT
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined(RTOS)
#include <sema.h>
extern SEMA	SemaSched[1];
#endif

/*---------------------------------------------------------------------------*\
  Aktivieren - ab jetzt IRQs
\*---------------------------------------------------------------------------*/
int CM_Activate (void);

/*---------------------------------------------------------------------------*\
  Stack incl. HW runterfahren, Ressourcen freigeben
\*---------------------------------------------------------------------------*/
int CM_Exit (void);

/*---------------------------------------------------------------------------*\
  CM_Schedule - der Scheduler
\*---------------------------------------------------------------------------*/
int CM_Schedule (void);

/*---------------------------------------------------------------------------*\
  CM_HandleEvent - IRQ-Handler-Aufruf
\*---------------------------------------------------------------------------*/
#if defined (C_PASSIV)
unsigned __cdecl CM_HandleEvents(void);
#else
unsigned CM_HandleEvents(void);
#endif

/*---------------------------------------------------------------------------*\
  CM_BufSize - aufgerufen im Callback liefert Grˆsse des ApplBuffers
\*---------------------------------------------------------------------------*/
unsigned CM_BufSize (void);

#if !defined(FRITZX) && !defined(BT_ACCESS)
/*---------------------------------------------------------------------------*\
	Steuerung eines Timer-Interrupt durch die OS-Umgebung

	U: Wird erst nach CM_Activate, dann aber aus beliebigen Kontext auch 
	   ggf. mehrfach aufgerufen.
\*---------------------------------------------------------------------------*/
void CM_TimerIrqControl (unsigned bEnable);

/*---------------------------------------------------------------------------*\
	Ist es ¸berhaupt sinnvoll wakeup zu unterst¸tzen, wird f¸r User-Control
	genutzt. Wakeup macht z.B. keinen Sinn bei D2_ON

	A: CM_WakeupSupportable () == TRUE --> es gibt mind. eine Situtation in der 
	   CM_PowerControl (x, TRUE, y) == TRUE liefern kann
	U: Wird erst nach CM_Activate () aufgerufen
\*---------------------------------------------------------------------------*/
unsigned CM_WakeupSupportable (void);

/*---------------------------------------------------------------------------*\
	PowerManagement Control

	bPowerDown == TRUE: Device soll runtergefahren werden
		bForce == TRUE:	unbedingt, ohne Mˆglichkeit zur Ablehnung
				in diesem Fall bWakeupControl ohne Bedeutung
		bForce == FALSE:	Ablehnung, wenn nicht mˆglich/sinnvoll
		bWakeupControl == TRUE:	wenn keine IRQs/Aktivit‰ten mehr 
		bWakeupControl == FALSE: wenn keine Anwendung im LISTEN o.‰.

		--- nur aktive Karten: ---
			pStackData : Daten des Stacks f¸r sp‰teres Aufwecken
			pStackDataLen : Input:  MaxStackDatalen
							Output: ActStackDataLen
		--- nur aktive Karten ---


	bPowerDown == FALSE:Aufwecken nach erfolgtem Powerdown
		bForce == TRUE:	HW komplett neu aufsetzen
		bForce == FALSE:	HW ist in altem Zustand
		bWakeupControl == TRUE: Powerup wegen Wakeup (aus der E1)
		bWakeupControll == FALSE: PowerUp wegen Useraktivit‰t (z.B. CAPIMessage)

		--- nur aktive Karten: ---
			pStackData : Daten des Stacks vom fr¸heren PowerDown
			pStackDataLen : StackDatalen
		--- nur aktive Karten: ---

	Returnwert:
		0: Anforderung abgelehnt, sollte nur bei (BPowerDown && !bForce) auftreten
		ansonsten: ok
		
	U: Wird erst nach CM_Activate () aufgerufen
\*---------------------------------------------------------------------------*/
#if defined(C_PASSIV)
unsigned CM_PowerControl (unsigned bPowerDown, unsigned bWakeupEnabled, unsigned bForce);
#else
unsigned CM_PowerControl (unsigned bPowerDown, unsigned bWakeupEnabled, unsigned bForce,
						  void *pStackData, unsigned *pStackDataLen);
#endif

/*---------------------------------------------------------------------------*\
	Power-Management Wakeup Control
	
	E: CA_WakeupControl (TRUE) -- Remote Wakeup erw¸nscht
	   CA_WakeupControl (FALSE) -- Remote Wakeup nicht sinnvoll
	
	U: darf erst nach CM_Activate () aufgerufen werden
\*---------------------------------------------------------------------------*/
typedef void (*PCA_WakeupControl) (unsigned bEnable);

/*---------------------------------------------------------------------------*\
	Scheduler Control
	
	CA_SchedulerControl (FALSE) --> Scheduler muﬂ nicht mehr aufgerufen werden bis
	n‰chste CAPI-Message bzw. CM_HandleEvents () auftritt;
	Powerdown jetzt sinnvoll, es laufen keine CAPI Timer mehr

	CA_SchedulerControl (TRUE) --> Scheduler muﬂ weiterhin aufgerufen werden
	Powerdown jetzt nicht erlaubt, es laufen CAPI Aktivit‰ten
\*---------------------------------------------------------------------------*/
typedef void (*PCA_SchedulerControl) (unsigned bEnable);

/*---------------------------------------------------------------------------*\
	never change the order of functions, add new functons only to the end
\*---------------------------------------------------------------------------*/
typedef struct _DynamicCAFunctions{
	unsigned			NoOfFunctions;
	PCA_SchedulerControl		SchedulerControl;	
	PCA_WakeupControl		WakeupControl;
} DynamicCAFunctions, *PDynamicCAFunctions;

/*---------------------------------------------------------------------------*\
	Register dynamic CA_Functions ()
	Wird VOR dem CM_Start aufgerufen
\*---------------------------------------------------------------------------*/
extern PCA_SchedulerControl		pfCA_SchedulerControl;	
extern PCA_WakeupControl		pfCA_WakeupControl;

void CM_RegisterDynamicCAFunctions (PDynamicCAFunctions pFunctions);

#else /*--- #if !defined(FRITZX) && !defined(BT_ACCESS) ---*/
#define pfCA_SchedulerControl	NULL
#define pfCA_WakeupControl	NULL
#endif /*--- #else ---*/ /*--- #if !defined(FRITZX) && !defined(BT_ACCESS) ---*/


#ifdef __cplusplus
}
#endif

#endif

