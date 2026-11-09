/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifndef _capi_pipe_h_
#define _capi_pipe_h_

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct capi_pipe {
	char *Name;
	unsigned char *Buffer;
	volatile unsigned int ReadPos;
	volatile unsigned int WritePos;
	unsigned int BufferLen;
	unsigned int MaxMessageLen;
	unsigned int with_lock;
	volatile unsigned int delete_pending;
	atomic_t rx_waiting;
	atomic_t tx_waiting;
	struct semaphore Lock;
	struct semaphore rx_Wait;
	struct semaphore tx_Wait;
	struct completion complete;
	wait_queue_head_t *rx_wait_queue;
	wait_queue_head_t *tx_wait_queue;
	void *Conn;
};

#define CAPI_VARIABLE_SIZE           0
#define CAPI_FIXED_SIZE              1
 
#define CAPI_NO_LOCK                 0
#define CAPI_LOCK                    1

#define CAPI_NO_SUSPEND             -1
#define CAPI_SUSPEND                 0

#define CAPI_PIPE_EMPTY             -2
#define CAPI_PIPE_FULL              -3
#define CAPI_PIPE_TIMEOUT           -4
#define CAPI_PIPE_DELETED           -5
#define CAPI_PIPE_BUFFER_TO_SMALL   -6
#define CAPI_PIPE_INTERRUPTED       -7

int Capi_Receive_From_Pipe(struct capi_pipe *, unsigned char *, unsigned int, unsigned long *, unsigned int);
int Capi_Send_To_Pipe(struct capi_pipe *, unsigned char *, unsigned int, unsigned int);
int Capi_Create_Pipe(struct capi_pipe *, char *, unsigned char *, unsigned int, int, unsigned int, int);
int Capi_Pipe_Options(struct capi_pipe *, wait_queue_head_t *rx_wait_queue, wait_queue_head_t *tx_wait_queue);
int Capi_Delete_Pipe(struct capi_pipe *);
char *Capi_Pipe_Status(struct capi_pipe *);
int Capi_Pipe_Init(void);

#endif /*--- #ifndef _capi_pipe_h_ ---*/
