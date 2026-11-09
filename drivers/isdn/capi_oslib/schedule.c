/*-------------------------------------------------------------------------------------*\

	$Header: //avm-fs01/entwicklung/basis/generierung/arm-runtime/oslib/src/rcs/schedule.c 1.2 2001/08/24 10:50:56Z MPommerenke Exp $

	$Id: schedule.c 1.2 2001/08/24 10:50:56Z MPommerenke Exp $

	$Log: schedule.c $
	Revision 1.2  2001/08/24 10:50:56Z  MPommerenke
	Revision 1.1  2001/08/21 07:09:54Z  MPommerenke
	Initial revision
	Revision 1.1  2001/08/06 09:32:57Z  ugillieron
	Initial revision

\*-------------------------------------------------------------------------------------*/
#include "nucleus.h"
#include "semaphor.h"
#include "system.h"
#include "stdlib.h"
#include "c4_sys.h"
#include "string.h"
#include "monitor.h"


#if defined(OSLIB_DEBUG)
/*--- #define DEBUG_SCHEDULE_C ---*/
#endif /*--- #if !defined(OSLIB_DEBUG) ---*/


/* this is for l2cap lib */
#define SCHEDULE_INIT_VALUE 0
#define SCHEDULE_SEMA_NAME "S_L2CAP_SCHEDULE"
static NU_SEMAPHORE ScheduleSema;
static unsigned int Initialized = 0;

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
STATUS ScheduleInit(NU_SEMAPHORE *Sema, int InitValue, char *SemaName) 
{
	STATUS Status;
#if defined(DEBUG_SEMA_C)
	DebugPrintf("ScheduleInit(%s, %d)\n", SemaName, InitValue);
#endif /*--- #if defined(DEBUG_SEMA_C) ---*/
	memset (Sema, 0, sizeof(NU_SEMAPHORE));
	Status = NU_Create_Semaphore(Sema, SemaName, InitValue, NU_FIFO);
	if(Status != NU_SUCCESS) {
		SystemError(Status, "ScheduleInit, NU_Create_Semaphore failed");
	}
	return Status;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void SchedulerStop() {
	STATUS Status;

	if (!Initialized) {
		Initialized = 1;
		ScheduleInit (&ScheduleSema, SCHEDULE_INIT_VALUE, SCHEDULE_SEMA_NAME);
	}
	Status = NU_Obtain_Semaphore(&ScheduleSema, NU_SUSPEND);
	if(Status != NU_SUCCESS) {
		SystemError(Status, "ScheduleWait: NU_Obtain_Semaphore: failed");
	}
	return;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void SchedulerTrigger() {
	STATUS Status;

	if (!Initialized) {
		Initialized = 1;
		ScheduleInit (&ScheduleSema, SCHEDULE_INIT_VALUE, SCHEDULE_SEMA_NAME);
	}
	Status = NU_Release_Semaphore(&ScheduleSema);
	if(Status != NU_SUCCESS) {
		SystemError(Status, "ScheduleSignal: NU_Release_Semaphore: failed");
	}
	return;
}
