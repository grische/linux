/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _cm_h
#define _cm_h

#ifdef __cplusplus
extern "C" {
#endif

#if defined(C_PASSIV)
extern unsigned int	CM_Start(void);
extern char	*CM_Init(unsigned int BaseAdr, unsigned int IRQ);
#elif defined(_transputer)
#include <chan.h>
extern unsigned int	CM_Start(void);
extern unsigned char	*CM_Init(CHAN *Irq, unsigned char *Stack, unsigned int Size);
extern void	CM_Switch_Led(unsigned int LedON, unsigned int Led);
extern char	PATCH_FAXPOLL2[12];
extern unsigned int T1T_LedExitCause;
#elif defined(C4) || defined(ARM) || defined(BLKFN)
extern unsigned int	CM_Start(void);
extern unsigned char *CM_Init(void);
#else
#error UNKNOWN OS ENVIRONMENT
#endif

#if defined(RTOS)
#include <sema.h>
extern SEMA	SemaSched[1];
#endif

/**
 * Aktivieren - ab jetzt IRQs
 */
int CM_Activate(void);

/**
 * Stack incl. HW runterfahren, Ressourcen freigeben
 */
int CM_Exit(void);

/*
 * CM_Schedule - der Scheduler
 */
int CM_Schedule(void);

/**
 * CM_HandleEvent - IRQ-Handler-Aufruf
 */
#if defined(C_PASSIV)
unsigned int __cdecl CM_HandleEvents(void);
#else
unsigned int CM_HandleEvents(void);
#endif

/**
 *
 * CM_BufSize - aufgerufen im Callback liefert Grˆsse des ApplBuffers
 */
unsigned int CM_BufSize(void);

#if !defined(FRITZX) && !defined(BT_ACCESS)
/**
 * Steuerung eines Timer-Interrupt durch die OS-Umgebung
 *
 * Wird erst nach CM_Activate, dann aber aus beliebigen Kontext auch
 * ggf. mehrfach aufgerufen.
 */
void CM_TimerIrqControl(unsigned int bEnable);

/**
 * Ist es ¸berhaupt sinnvoll wakeup zu unterst¸tzen, wird f¸r User-Control
 * genutzt. Wakeup macht z.B. keinen Sinn bei D2_ON
 *
 * A: CM_WakeupSupportable () == TRUE --> es gibt mind. eine Situtation in der
 *    CM_PowerControl (x, TRUE, y) == TRUE liefern kann
 * U: Wird erst nach CM_Activate () aufgerufen
 */
unsigned int CM_WakeupSupportable(void);

/**
 * PowerManagement Control
 *
 * bPowerDown == TRUE: Device soll runtergefahren werden
 * bForce == TRUE:	unbedingt, ohne Mˆglichkeit zur Ablehnung
 * in diesem Fall bWakeupControl ohne Bedeutung
 * bForce == FALSE:	Ablehnung, wenn nicht mˆglich/sinnvoll
 * bWakeupControl == TRUE:	wenn keine IRQs/Aktivit‰ten mehr
 * bWakeupControl == FALSE: wenn keine Anwendung im LISTEN o.‰.
 *
 * --- nur aktive Karten: ---
 * pStackData : Daten des Stacks f¸r sp‰teres Aufwecken
 * pStackDataLen : Input:  MaxStackDatalen
 * Output: ActStackDataLen
 * --- nur aktive Karten ---
 *
 *
 * bPowerDown == FALSE:Aufwecken nach erfolgtem Powerdown
 * bForce == TRUE:	HW komplett neu aufsetzen
 * bForce == FALSE:	HW ist in altem Zustand
 * bWakeupControl == TRUE: Powerup wegen Wakeup (aus der E1)
 * bWakeupControll == FALSE: PowerUp wegen Useraktivit‰t (z.B. CAPIMessage)
 *
 * --- nur aktive Karten: ---
 * pStackData : Daten des Stacks vom fr¸heren PowerDown
 * pStackDataLen : StackDatalen
 * --- nur aktive Karten: ---
 *
 * Returnwert:
 * 0: Anforderung abgelehnt, sollte nur bei (BPowerDown && !bForce) auftreten
 * ansonsten: ok
 *
 * U: Wird erst nach CM_Activate () aufgerufen
 */
#if defined(C_PASSIV)
unsigned int CM_PowerControl(unsigned int bPowerDown, unsigned int bWakeupEnabled, unsigned int bForce);
#else
unsigned int CM_PowerControl(unsigned int bPowerDown, unsigned int bWakeupEnabled, unsigned int bForce,
						  void *pStackData, unsigned int *pStackDataLen);
#endif

/**
 * Power-Management Wakeup Control
 *
 * E: CA_WakeupControl (TRUE) -- Remote Wakeup erw¸nscht
 *    CA_WakeupControl (FALSE) -- Remote Wakeup nicht sinnvoll
 *
 * U: darf erst nach CM_Activate () aufgerufen werden
 */
typedef void (*PCA_WakeupControl)(unsigned int bEnable);

/**
 * Scheduler Control
 *
 * CA_SchedulerControl (FALSE) --> Scheduler muﬂ nicht mehr aufgerufen werden bis
 * n‰chste CAPI-Message bzw. CM_HandleEvents () auftritt;
 * Powerdown jetzt sinnvoll, es laufen keine CAPI Timer mehr
 *
 * CA_SchedulerControl (TRUE) --> Scheduler muﬂ weiterhin aufgerufen werden
 * Powerdown jetzt nicht erlaubt, es laufen CAPI Aktivit‰ten
 */
typedef void (*PCA_SchedulerControl) (unsigned int bEnable);

/**
 *	never change the order of functions, add new functons only to the end
 */
typedef struct _DynamicCAFunctions {
	unsigned int			NoOfFunctions;
	PCA_SchedulerControl		SchedulerControl;
	PCA_WakeupControl		WakeupControl;
} DynamicCAFunctions, *PDynamicCAFunctions;

/**
 * Register dynamic CA_Functions ()
 * Wird VOR dem CM_Start aufgerufen
 */
extern PCA_SchedulerControl		pfCA_SchedulerControl;
extern PCA_WakeupControl		pfCA_WakeupControl;

void CM_RegisterDynamicCAFunctions(PDynamicCAFunctions pFunctions);

#else /*--- #if !defined(FRITZX) && !defined(BT_ACCESS) ---*/
#define pfCA_SchedulerControl	NULL
#define pfCA_WakeupControl	NULL
#endif /*--- #else ---*/ /*--- #if !defined(FRITZX) && !defined(BT_ACCESS) ---*/


#ifdef __cplusplus
}
#endif

#endif
