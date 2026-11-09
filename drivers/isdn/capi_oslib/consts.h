#ifndef _consts_h_
#define _consts_h_

#include <linux/version.h>

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define MAX_CAPI_MESSAGE_SIZE               2048U
#define MAX_CONTROLLERS                     10
/*--- #define NEED_PAGE_LOCK ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- Realtime-Workqueues sind deutlich performanter als Kernelthreads! ---*/
#if defined(CONFIG_SMP) 
#define USE_WORKQUEUES
#else
#define USE_TASKLETS
#endif


#endif /*--- #ifndef _consts_h_ ---*/
#if __has_include(<avm/pcmlink/ul.h>)
#include <avm/pcmlink/ul.h>
#else
#include <linux/pcmlink_ul.h>
#endif
