
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/net.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <net/tcp.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <net/protocol.h>
#include <net/sock.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/kthread.h>
#pragma GCC pop_options

#if __has_include(<avm/pcmlink/ul.h>)
#include <avm/pcmlink/ul.h>
#else
#include <linux/pcmlink_ul.h>
#endif

#include "transparent.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
static inline size_t msg_data_left(struct msghdr *msg)
{
	size_t count = 0;
	unsigned long iovlen;
	struct iovec *iov;

	for (iov = msg->msg_iov, iovlen = msg->msg_iovlen; iovlen > 0; iovlen--, iov++)
		count += iov->iov_len;
	return count;
}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
static inline size_t msg_data_left(struct msghdr *msg)
{
	return iov_iter_count(&msg->msg_iter);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
#	define sock_sendmsg(sock, msg)	\
		sock_sendmsg(sock, msg, msg_data_left(msg))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
#	define sock_recvmsg(sock, msg, flags)	\
		sock_recvmsg(sock, msg, msg_data_left(msg), flags)
#endif

/*--- #define TRANSP_DEBUG ---*/
/*--- #define CA_PRINTF printk ---*/

static struct socket *MainSocket=NULL;
static struct task_struct *thread_pid;

static volatile int txin_count, txout_count;
static unsigned char txbuffer[2048];
static volatile int	close_transp = 1;
static volatile int	exit_transp = 0;
static int tx_wait_condition;
static DECLARE_WAIT_QUEUE_HEAD(tx_wait);
static void (*CAP_DataInd)(const unsigned char *, int) = NULL;

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void Flush(struct socket *sock)
{
	struct msghdr		msg;
	struct iovec		iov;
	int			len;

	mm_segment_t		oldfs;


	if (sock->sk==NULL)
		return;

	len = 1;

	while (len>0)
	{
		char		Buffer[128];
		msg.msg_name     = 0;
		msg.msg_namelen  = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
		msg.msg_iov	 = &iov;
		msg.msg_iovlen   = 1;
		msg.msg_iov->iov_base = &Buffer[0];
		msg.msg_iov->iov_len  = (__kernel_size_t)128;
#else
		iov.iov_base = &Buffer[0];
		iov.iov_len  = (__kernel_size_t)128;
		iov_iter_init(&msg.msg_iter, READ, &iov, 1, iov.iov_len);
#endif
		msg.msg_control  = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags    = MSG_DONTWAIT;

		oldfs = get_fs(); set_fs(KERNEL_DS);
		len = sock_recvmsg(sock,&msg,MSG_DONTWAIT);
		set_fs(oldfs);
	}
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void Read(struct socket *sock, unsigned char* buffer, unsigned *length, int wait)
{
	struct msghdr		msg;
	struct iovec		iov;

	mm_segment_t		oldfs;


	if (sock->sk==NULL) {
		*length = 0;
		return;
	}

	iov.iov_base = (void *)buffer;
	iov.iov_len = *length;
	msg.msg_name     = 0;
	msg.msg_namelen  = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	msg.msg_iov	 = &iov;
	msg.msg_iovlen   = 1;
#else
	iov_iter_init(&msg.msg_iter, READ, &iov, 1, iov.iov_len);
#endif
	msg.msg_control  = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = MSG_DONTWAIT;

	oldfs = get_fs(); set_fs(KERNEL_DS);
	*length = sock_recvmsg(sock,&msg,wait ? 0 : MSG_DONTWAIT);
	set_fs(oldfs);
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static unsigned int Write(struct socket *sock, unsigned char* buffer, unsigned length, int wait) {
	struct msghdr		msg;
	struct iovec		iov;

	mm_segment_t oldfs;

	if (sock->sk==NULL) {
		return 0;
	}

	iov.iov_base = (void *)buffer;
	iov.iov_len = length;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#else
	iov_iter_init(&msg.msg_iter, WRITE, &iov, 1, iov.iov_len);
#endif
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags    = wait ? 0 : MSG_DONTWAIT;

	oldfs = get_fs(); set_fs(KERNEL_DS);
	length = sock_sendmsg(sock, &msg);
	set_fs(oldfs);
	return length;
}

/*-------------------------------------------------------------------------------------*\
 * may block while connecting...
\*-------------------------------------------------------------------------------------*/
static int E1Tx_Transparent(void *unused __attribute__((unused)))
{

	int len;
	char buf[128];
	struct sockaddr_in addr;
	int transp_open;

	transp_open = 0;

	for (;(exit_transp == 0);) {
		if (transp_open == 0) {
			wait_event_interruptible_timeout(tx_wait, tx_wait_condition, (HZ*10)); /*--- Verbindunsaufbau nur alle 10s ---*/
			tx_wait_condition = 0;
		} else transp_open = 0;

		if (sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&MainSocket)<0)
		{
#if defined(TRANSP_DEBUG)
			CA_PRINTF("E1Tx_Transparent(): Error during creation of socket\n");
#endif
			MainSocket = NULL;
			continue;
		}
		/*--- Connect to 127.0.0.1 port 1011 ---*/
		addr.sin_family = AF_INET;
		addr.sin_port = htons((unsigned short)1011);
		addr.sin_addr.s_addr = htonl(0x7F000001);

		if (MainSocket->ops->connect(MainSocket, (struct sockaddr*)&addr, sizeof(addr), O_RDWR)) {
#if defined(TRANSP_DEBUG)
			CA_PRINTF("E1Tx_Transparent(): connect error\n");
#endif
			sock_release(MainSocket);
			MainSocket = NULL;
			continue;
		}
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_Transparent(): connect done\n");
#endif

		while (!kthread_should_stop()) {
#if 0
			if (signal_pending (current)) {
				sigset_t these;
				siginfo_t info;
				int signr;

				spin_lock_irq(&current->sigmask_lock);
				signr = dequeue_signal(&these, &info);
				spin_unlock_irq(&current->sigmask_lock);
				switch (signr) {
				case SIGKILL:
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(1): SIGKILL\n");
#endif
					printk("E1Tx_Transparent(1): SIGKILL\n");
					exit_transp = 1;
					break;
				case SIGTERM :
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(1): SIGTERM\n");
#endif
					printk("E1Tx_Transparent(1): SIGTERM\n");
					exit_transp = 1;
					break;
				}
			}
#endif
			/* If the connection is lost, remove from queue */
			if ((MainSocket->sk->sk_state != TCP_ESTABLISHED
				 /*&& MainSocket->sk->sk_state != TCP_CLOSE_WAIT */) ||
				(exit_transp) || ((transp_open) && (close_transp))
				/*--- || signal_pending (current) ---*/
				)
			{
#if defined(TRANSP_DEBUG)
				CA_PRINTF("E1Tx_Transparent(): socket %s\n",(transp_open) ? "close" : "lost");
#endif
				/* Close the socket ....*/
				if ((MainSocket!=NULL)&&(MainSocket->sk!=NULL))
				{
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(): Flush\n");
#endif
					Flush(MainSocket);
					sock_release(MainSocket);
				}
				MainSocket = NULL;
				break;
			}

			if (close_transp) {
				wait_event_interruptible_timeout(tx_wait, tx_wait_condition, (HZ*10)); /*--- Keepalivetest alle 10s ---*/
				tx_wait_condition = 0;
#if defined(TRANSP_DEBUG)
				/*--- CA_PRINTF("E1Tx_Transparent(): wakup2\n"); ---*/
#endif
				continue;
			}

			transp_open = 1;

			len = (txin_count - txout_count);
			if (len != 0) {
				if (len < 0) {
					len = (sizeof(txbuffer) - txout_count);
				}

				if (((int)Write(MainSocket, &txbuffer[txout_count], len, 0)) > 0) {
					if (txout_count >= (int)(sizeof(txbuffer) - len)) txout_count = 0;
					else txout_count += len;
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(): %d bytes sent\n", len);
#endif
				} else{
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(): %d bytes not sent\n", len);
#endif
					wait_event_interruptible_timeout(tx_wait, tx_wait_condition, (HZ/100)); /*--- Aufwachen nach 10ms ---*/
					tx_wait_condition = 0;
				}
			} else {
				wait_event_interruptible_timeout(tx_wait, tx_wait_condition, (HZ/100)); /*--- Aufwachen nach 10ms ---*/
				tx_wait_condition = 0;
			}
			len = 128;
			Read(MainSocket, buf, &len, 0);
			if (len > 0) {
#if defined(TRANSP_DEBUG)
				CA_PRINTF("E1Tx_Transparent(): got %d bytes\n", len);
#endif
				if (CAP_DataInd != NULL) CAP_DataInd(buf, len);
			}
		}
	}
#if defined(TRANSP_DEBUG)
	CA_PRINTF("E1Tx_Transparent(): Close done!\n");
#endif
	return 0;
}
EXPORT_SYMBOL(E1Tx_Transparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int E1Tx_OpenTransparent(void) {

	pcmlink_ul_assert_on_yield_context();
	if (close_transp == 0) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_OpenTransparent(): Error already open\n");
#endif
		return 1;
	}

	if ((MainSocket == NULL) || (MainSocket->sk==NULL) || (MainSocket->sk->sk_state != TCP_ESTABLISHED)) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_OpenTransparent(): Error no socket open\n");
#endif
		tx_wait_condition = 1;
		wake_up_interruptible (&tx_wait);
		return 1;
	}

	txin_count = 0;
	txout_count = 0;
	close_transp = 0;

#if defined(TRANSP_DEBUG)
	CA_PRINTF("E1Tx_OpenTransparent(): Open start\n");
#endif
	return 0;
}
EXPORT_SYMBOL(E1Tx_OpenTransparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int E1Tx_CloseTransparent(void) {

	pcmlink_ul_assert_on_yield_context();
	if (close_transp == 0) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_CloseTransparent(): Start close!\n");
#endif
		close_transp = 1;
		tx_wait_condition = 1;
		wake_up_interruptible (&tx_wait);
	}
#if defined(TRANSP_DEBUG)
	else
		CA_PRINTF("E1Tx_CloseTransparent(): Not open!\n");
#endif
	return 0;
}
EXPORT_SYMBOL(E1Tx_CloseTransparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int E1Tx_SendTransparent(unsigned char *Buffer, unsigned int BufferLength) {
	int len;
	pcmlink_ul_assert_on_yield_context();
	if ((close_transp) || (MainSocket == NULL) || (MainSocket->sk==NULL) || (MainSocket->sk->sk_state != TCP_ESTABLISHED)) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_SendTransparent(): MainSocket == NULL (%u bytes)\n", BufferLength);
#endif
		tx_wait_condition = 1;
		wake_up_interruptible (&tx_wait);
		return 1;
	}
	if (BufferLength >= sizeof(txbuffer)) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_SendTransparent(): too much bytes (%u)\n", BufferLength);
#endif
		return 1;
	}
	len = txin_count;
	if ((len + BufferLength) < sizeof(txbuffer)) {
		memcpy(&txbuffer[len], Buffer, BufferLength);
#if defined(TRANSP_DEBUG)
		if ((txin_count < txout_count) && ((len + BufferLength) >= txout_count))
			CA_PRINTF("E1Tx_SendTransparent(): Overrun\n");
#endif
		txin_count = (len + BufferLength);
	} else {
		int len1 = sizeof(txbuffer) - len;
		memcpy(&txbuffer[len], Buffer, len1);
#if defined(TRANSP_DEBUG)
		if (len < txout_count)
			CA_PRINTF("E1Tx_SendTransparent(): Overrun\n");
#endif
		len = BufferLength - len1;
		memcpy(&txbuffer[0], &Buffer[len1], len);
#if defined(TRANSP_DEBUG)
		if (len >= txout_count)
			CA_PRINTF("E1Tx_SendTransparent(): Overrun\n");
#endif
		txin_count = len;
	}
	tx_wait_condition = 1;
	wake_up_interruptible (&tx_wait);

	return 0;
}
EXPORT_SYMBOL(E1Tx_SendTransparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int Transparent_Init(void (*DataInd)(const unsigned char *, int)) {

	txin_count = 0;
	txout_count = 0;
	exit_transp = 0;

	CAP_DataInd = DataInd;
	tx_wait_condition = 0;
	init_waitqueue_head (&tx_wait);

	thread_pid = kthread_run(E1Tx_Transparent, NULL, "capitransp");
	if((thread_pid == NULL) || IS_ERR(thread_pid)) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("Transparent_Init(): Error during kernel thread creation = %d\n", pid);
#endif
		CAP_DataInd = NULL;
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(Transparent_Init);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void Transparent_Deinit(void) {

#if defined(TRANSP_DEBUG)
	CA_PRINTF("Transparent_Deinit(): Start \n");
#endif
	if(CAP_DataInd == NULL) {
		return;
	}
	close_transp = 1;
	exit_transp = 1;
	tx_wait_condition = 1;
	wake_up_interruptible (&tx_wait);

	if(thread_pid) {
		kthread_stop(thread_pid);
		thread_pid = NULL;
	}
	CAP_DataInd = NULL;
}
EXPORT_SYMBOL(Transparent_Deinit);
