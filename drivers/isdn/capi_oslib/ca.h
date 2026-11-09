#ifndef _CA_H_
#define _CA_H_

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#include "appl.h"
#include "consts.h"

/*-------------------------------------------------------------------------------------*\
	ca.c
\*-------------------------------------------------------------------------------------*/
unsigned int CA_DATA_B3_IND_DATA_LENGTH (unsigned char *Msg);
unsigned char *CA_DATA_B3_IND_DATA (unsigned char *Msg);
void CA_INIT(unsigned int Len, void (*Register)(void *, unsigned int), void (*Release)(void *), void (*Down)(void));
unsigned int CA_NEW_NCCI(unsigned int MapperId, unsigned int NCCI, unsigned int WindowSize, unsigned int BlockSize);
void CA_FREE_NCCI(unsigned int MapperId, unsigned int NCCI);
unsigned char *CA_NEW_DATA_B3_IND(unsigned int MapperId, unsigned int NCCI, unsigned int Index);
unsigned char *CA_NEW_DATA_B3_REQ(unsigned int MapperId, unsigned int NCCI);
void CA_FREE_DATA_B3_IND(unsigned int MapperId, unsigned char *Data);
void CA_FREE_DATA_B3_REQ(unsigned int MapperId, unsigned char *Data);
unsigned int CA_GET_MESSAGE(unsigned char *Msg);
void CA_PUT_MESSAGE(unsigned char *Msg);
unsigned int CA_KARTE(void);
void CA_VERSION(unsigned char *Version);
void CA_SWITCH_TO_DWORD (unsigned int fDWORD);
unsigned int CA_BLOCKSIZE(unsigned int MapperId);
unsigned int CA_WINDOWSIZE(unsigned int MapperId);
void *CA_APPLDATA (unsigned int MapperId);
#if defined(CONFIG_CAPI_OSLIB_DEBUG)
void *_CA_MALLOC(unsigned int size, char *File, unsigned int Line);
void _CA_FREE(void *p, char *File, unsigned int Line);
#define CA_MALLOC(size) _CA_MALLOC( size, __FILE__, __LINE__ )
#define CA_FREE(p) _CA_FREE( p, __FILE__, __LINE__ )
#else
void *CA_MALLOC(unsigned int size);
void CA_FREE(void *p );
#endif
void CA_MEM_SHOW( void );
void CA_MEM_EXIT( void );
unsigned char *CA_PARAMS(void);
unsigned int CA_MSEC(void);
unsigned long long CA_MSEC64(void);
unsigned int CA_Timer_Init(void);
struct _ApplsFirstNext *CA_APPLDATA_FIRST(struct _ApplsFirstNext *s);
struct _ApplsFirstNext *CA_APPLDATA_NEXT(struct _ApplsFirstNext *s);
void CA_SEND_CONFIG(void);
void CA_Wakeup(void);

/*-------------------------------------------------------------------------------------*\
	ca_debug.c
\*-------------------------------------------------------------------------------------*/
void CA_DEBUG_INIT(void);
void CA_DEBUG_START(void);
void CA_DEBUG_TASK(unsigned int argc, void *argv[]);
void CA_DEBUG_PUTS (char *Buffer);
void CA_DEBUG_PUT_MESSAGE (char *msg);

/*-------------------------------------------------------------------------------------*\
	ca_poll.c
\*-------------------------------------------------------------------------------------*/
void CA_POLL(void);
void CA_POLL_STOP(void);
void CA_POLL_START(void);
void CA_WATCHDOG_POLL(void);

/*-------------------------------------------------------------------------------------*\
	ca_sched.c
\*-------------------------------------------------------------------------------------*/
struct _ScheduleWakeEvents {
	unsigned int WakeEvent_Timer           : 1;
	unsigned int WakeEvent_MessageFrom_Os  : 1;
	unsigned int WakeEvent_MessageFrom_Blk : 1;

	unsigned int WakeEvent_Unused          : 29;
};

void CA_Schedule_Init(void);
void CA_Schedule_Event(struct _ScheduleWakeEvents Event);
struct _ScheduleWakeEvents CA_Schedule_Wait(void);

/*-------------------------------------------------------------------------------------*\
	ca_timer.c
\*-------------------------------------------------------------------------------------*/
enum CA_RESTARTTIMER {
	CA_TIMER_END = 0,
	CA_TIMER_RESTART = 1
};

struct capi_timer {
	unsigned int  Start;
	unsigned int  Tics;
	unsigned int  Param;
	enum CA_RESTARTTIMER (*Func)(unsigned int param);
};

void CA_TIMER_POLL (void);
int CA_TIMER_STOP (unsigned index);
unsigned int CA_TIMER_START(unsigned int index, unsigned int TimeoutValue, unsigned int _Param, enum CA_RESTARTTIMER (*_Func)(unsigned int param));
void CA_TIMER_DELETE (void);
int CA_TIMER_NEW (unsigned MaxTimer);


#endif /*--- #ifndef _CA_H_ ---*/
