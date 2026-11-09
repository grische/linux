#ifndef _capi_events_h_
#define _capi_events_h_

struct capi_events {
	char *Name;
	unsigned int with_lock;
	struct semaphore Lock;
	unsigned int EventMask;
	unsigned int AndMask;
	unsigned int OrMask;
};

#define CAPI_EVENT_OR       0
#define CAPI_EVENT_AND      1

int Capi_Set_Events(struct capi_events *, unsigned int, int);


#endif /*--- #ifndef _capi_events_h_ ---*/
