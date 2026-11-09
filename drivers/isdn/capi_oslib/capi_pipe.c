#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/hardirq.h>
#pragma GCC pop_options

#include "debug.h"
#include <avm/capi_oslib/new.h>
#include <avm/capi_oslib/oslib.h>


#include "capi_pipe.h"
#include "appl.h"
#include "ca.h"

extern void capi_oslib_trigger_rxwork(struct workqueue_struct *rxwork, void *conn);
DEFINE_SPINLOCK(send_capi_pipe);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct workqueue_struct *pipe_workqueue;

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int capi_pipe_free(struct capi_pipe *P) {
	unsigned int ReadPos, WritePos;
	ReadPos  = P->ReadPos;
	WritePos = P->WritePos;

	if(WritePos >= ReadPos) {
		return (P->BufferLen - sizeof(unsigned int)) - (WritePos - ReadPos);
	}
	return ReadPos - WritePos - sizeof(unsigned int);
}

/*------------------------------------------------------------------------------------------*\
 * mit RxBuffer == NULL wird die Länge der im Buffer befindlichen CapiMessage returned
\*------------------------------------------------------------------------------------------*/
int Capi_Receive_From_Pipe(struct capi_pipe *P, unsigned char *RxBuffer, unsigned int RxBufferLen, unsigned long *received, unsigned int Suspend) {
	unsigned int Len, CopyLength;
	unsigned int ReadPos;


	pcmlink_ul_assert_on_yield_context();
	*received = 0;
	if(P->delete_pending) {
		printk("%s: delete_pending\n", __func__);
		return CAPI_PIPE_DELETED;
	}

	if(P->with_lock) {
		BUG_ON(in_softirq());
		down(&(P->Lock));
	}

Capi_Receive_From_Pipe_restart:
	/*--- if(RxBuffer) ---*/
	  /*--- DEB_INFO("[Capi_Receive_From_Pipe] write=%u read=%u free=%u\n", P->WritePos, P->ReadPos, capi_pipe_free(P)); ---*/

	if(P->WritePos == P->ReadPos) {
		if(Suspend == CAPI_SUSPEND) {
			atomic_inc(&(P->rx_waiting));
		}
		if(P->with_lock)
			up(&(P->Lock));
		if(Suspend == CAPI_SUSPEND) {
			down(&(P->rx_Wait));
			if(P->with_lock) {
				down(&(P->Lock));
			}
			atomic_dec(&(P->rx_waiting));
			if(P->delete_pending) {
				if(P->with_lock)
					up(&(P->Lock));
				complete(&P->complete);
				return CAPI_PIPE_DELETED;
			}
			goto Capi_Receive_From_Pipe_restart;
		}
		return RxBuffer == NULL ? 0 : CAPI_PIPE_EMPTY;
	}
	ReadPos = P->ReadPos;
	Len = *(unsigned int *)&(P->Buffer[ReadPos]);
	/*--- printk("[Capi_Receive_From_Pipe] '%s' %pS write=%u read=%u free=%u Len=%d\n", P->Name, P, P->WritePos, ReadPos, capi_pipe_free(P), Len); ---*/
	*received = Len;
	if(RxBuffer == NULL) {
		if(P->with_lock)
			up(&(P->Lock));
		return Len;
	}

	if(Len > RxBufferLen) {
		if(P->with_lock)
			up(&(P->Lock));
		/*--- dump_stack(); ---*/
		/*--- printk(KERN_ERR"%s: error: RxBuffer Len %d > RxBufferLen %d \n", __func__, Len, RxBufferLen); ---*/
		return CAPI_PIPE_BUFFER_TO_SMALL;
	}

	ReadPos += sizeof(unsigned int);
	if(ReadPos >= P->BufferLen)
		ReadPos = 0;

	/*--------------------------------------------------------------------------------------*\
	 * pruefen ob in zwei Abschnitten kopiert werden muss
	\*--------------------------------------------------------------------------------------*/
	if(Len > P->BufferLen - ReadPos) {
		unsigned int part_Len = P->BufferLen - ReadPos;
		memcpy(RxBuffer, &(P->Buffer[ReadPos]), part_Len);
		ReadPos = 0;
		RxBuffer += part_Len;
		Len -= part_Len;
	}

	/*--------------------------------------------------------------------------------------*\
	 * den rest kopieren
	\*--------------------------------------------------------------------------------------*/
	CopyLength = Len;
	Len += sizeof(unsigned int) - 1;    /*--- align auf sizeof(unsigned int) ---*/
	Len &= ~(sizeof(unsigned int) - 1);

	memcpy(RxBuffer, &(P->Buffer[ReadPos]), CopyLength);
	ReadPos += Len;
	if(ReadPos >= P->BufferLen)
		ReadPos = 0;

	P->ReadPos = ReadPos;
	if(atomic_read(&(P->tx_waiting))) {
		up(&(P->tx_Wait));
	}
	if(P->with_lock)
		up(&(P->Lock));

	if(P->tx_wait_queue)
		wake_up_interruptible(P->tx_wait_queue);
		/*--- wake_up(P->tx_wait_queue); ---*/
	return 0;
}

/*------------------------------------------------------------------------------------------*\
 * kann aus 2 Kontexten aufgerufen werden: - io
 *										   - Scheduler
\*------------------------------------------------------------------------------------------*/
int Capi_Send_To_Pipe(struct capi_pipe *P, unsigned char *TxBuffer, unsigned int TxBufferLen, unsigned int Suspend) {
	unsigned int WritePos;

	pcmlink_ul_assert_on_yield_context();

	if(P->delete_pending) {
		return CAPI_PIPE_DELETED;
	}

	if(P->with_lock) {
		BUG_ON(in_softirq());
		down(&(P->Lock));
	}

Capi_Send_To_Pipe_restart:


	/*--------------------------------------------------------------------------------------*\
	\*--------------------------------------------------------------------------------------*/
	if(TxBufferLen + 3 + sizeof(unsigned int) >= capi_pipe_free(P)) {
		if(Suspend == CAPI_SUSPEND) {
			atomic_inc(&(P->tx_waiting));
		}
		if(P->with_lock)
			up(&(P->Lock));
		if(Suspend == CAPI_SUSPEND) {
			BUG_ON(in_softirq());
			down(&(P->tx_Wait));
			if(P->with_lock) {
				down(&(P->Lock));
			}
			atomic_dec(&(P->tx_waiting));
			if(P->delete_pending) {
				if(P->with_lock)
					up(&(P->Lock));
				complete(&P->complete);
				/*--- printk(KERN_ERR"%s: error: CAPI_PIPE_DELETED \n", __func__); ---*/
				return CAPI_PIPE_DELETED;
			}
			goto Capi_Send_To_Pipe_restart;
		}
		/*--- printk(KERN_ERR"%s: error: CAPI_PIPE_FULL \n", __func__); ---*/
		return CAPI_PIPE_FULL;
	}

	/*--------------------------------------------------------------------------------*\
	 * diesen Bereich schuetzen, da Fkt. aus Hi-Tasklet (wenn verwendet) oder Kernel-Thread-Kontext aufgerufen
	 * werden kann
	\*--------------------------------------------------------------------------------*/
	spin_lock_bh(&send_capi_pipe);
	WritePos = P->WritePos;
	/*--- printk("[Capi_Send_To_Pipe] '%s' %pS write=%u read=%u free=%u TxBufferLen=%d\n", P->Name, P, P->WritePos, P->ReadPos, capi_pipe_free(P), TxBufferLen); ---*/

	/*--------------------------------------------------------------------------------------*\
	 * Laenge speichern
	\*--------------------------------------------------------------------------------------*/
	*(unsigned int *)&(P->Buffer[WritePos]) = TxBufferLen;
	WritePos += sizeof(unsigned int);
	if(WritePos >= P->BufferLen)
		WritePos = 0;

	TxBufferLen += sizeof(unsigned int) - 1;    /*--- align auf sizeof(unsigned int) ---*/
	TxBufferLen &= ~(sizeof(unsigned int) - 1);

	/*--------------------------------------------------------------------------------------*\
	 * pruefen ob in zwei Abschnitten kopiert werden muss
	\*--------------------------------------------------------------------------------------*/
	if(TxBufferLen > P->BufferLen - WritePos) {
		unsigned int Len = P->BufferLen - WritePos;
		memcpy(&(P->Buffer[WritePos]), TxBuffer, Len);
		WritePos     = 0;
		TxBufferLen -= Len;
		TxBuffer    += Len;
	}

	/*--------------------------------------------------------------------------------------*\
	 * den rest kopieren
	\*--------------------------------------------------------------------------------------*/
	memcpy(&(P->Buffer[WritePos]), TxBuffer, TxBufferLen);
	WritePos += TxBufferLen;
	if(WritePos >= P->BufferLen)
		WritePos = 0;

	P->WritePos = WritePos;
	spin_unlock_bh(&send_capi_pipe);
	/*--- printk("[Capi_Send_To_Pipe] done %pS write=%u read=%u free=%u\n", P, P->WritePos, P->ReadPos, capi_pipe_free(P)); ---*/

	if(atomic_read(&(P->rx_waiting))) {
		up(&(P->rx_Wait));
	}
	if(P->with_lock)
		up(&(P->Lock));

	if(P->rx_wait_queue)
		wake_up_interruptible(P->rx_wait_queue);
		/*--- wake_up(P->rx_wait_queue); ---*/

	if(P->Conn && pipe_workqueue) {
		capi_oslib_trigger_rxwork(pipe_workqueue, P->Conn);  /*--- remote-capi  ---*/
	}
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Create_Pipe(struct capi_pipe *P, char *Name, unsigned char *Buffer, unsigned int BufferLen, int type __attribute__((unused)), unsigned int MaxMessageLen, int Lock) {
	int len;
	len = strlen(Name);
	P->Name = CA_MALLOC(len + 1);
	if(P->Name) {
		strcpy(P->Name, Name);
	}
	P->Buffer        = Buffer;
	P->BufferLen     = BufferLen;
	P->MaxMessageLen = MaxMessageLen;
	P->ReadPos       = 0;
	P->WritePos      = 0;
	P->with_lock     = Lock;
	if(P->with_lock == CAPI_LOCK)
		sema_init(&(P->Lock), 1);
	sema_init(&(P->rx_Wait), 0);  /* blockieren beim ersten mal */
	sema_init(&(P->tx_Wait), 0);  /* blockieren beim ersten mal */
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Delete_Pipe(struct capi_pipe *P) {
	P->delete_pending = 1;
	init_completion(&P->complete);
	while(atomic_read(&P->rx_waiting) || atomic_read(&P->tx_waiting)) {
		if(atomic_read(&P->rx_waiting))
			up(&P->rx_Wait);
		if(atomic_read(&P->tx_waiting))
			up(&P->tx_Wait);
		wait_for_completion(&P->complete);
	}
	if(P->Name) {
		CA_FREE(P->Name);
		P->Name = NULL;
	}
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Pipe_Options(struct capi_pipe *P, wait_queue_head_t *rx_wait_queue, wait_queue_head_t *tx_wait_queue) {
	P->rx_wait_queue = rx_wait_queue;
	P->tx_wait_queue = tx_wait_queue;
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *Capi_Pipe_Status(struct capi_pipe *P) {
	static char Buffer[80];
	if(P == NULL)
		return "no pipe";

	DEB_ERR("[Capi_Pipe_Status] Pipe=0x%p\n", P);

	snprintf(Buffer, sizeof(Buffer), "Pipe(%s) wr=%d rd=%d free=%d size=%d",
			P->Name ? P->Name : "noname", P->WritePos, P->ReadPos, capi_pipe_free(P), P->BufferLen);
	return Buffer;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Pipe_Init(void) {
	if(pipe_workqueue == NULL) {
		pipe_workqueue = alloc_workqueue("capi_pipew", WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	}
	if(pipe_workqueue == NULL)
		return -EFAULT;
	return 0;
}
