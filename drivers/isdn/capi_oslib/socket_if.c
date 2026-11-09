
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/list.h>
#include <net/tcp.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <net/protocol.h>
#include <net/sock.h>
#include "consts.h"
#include <linux/poll.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/init.h>
#include <avm/capi_oslib/zugriff.h>
#pragma GCC pop_options
#include <avm/capi_oslib/new.h>
#include "debug.h"
#include "appl.h"
#include "capi_pipe.h"
#include "local_capi.h"
#include "zugriff.h"
#include <avm/capi_oslib/oslib.h>


struct _capi_connections {
	struct list_head list;
	unsigned ApplId;
	atomic_t link;
	u8 conindex;
	u16 send_seqnr;
	u16 recv_seqnr;
	struct work_struct rx_work;
	struct work_struct tx_work;
	struct work_struct remove;
	struct sk_buff_head recvqueue;  /* pakets without headers */
	u8 *recombine_buffer;           /* Buffer for recombining messages */
	unsigned recombine_len;
	unsigned B3BlockSize;
};

static struct workqueue_struct *capi_remote_put_workqueue;

struct remotedev_hdr {
   u8 type;                     /* REMOTEDEV_TYPE_APPL */
   u8 conindex;                 /* application identifier */
   u16 seqnr;                   /* Sequence Number */
   u16 len;                     /* Payload length */
};

#define REMOTEDEV_RESERVE   16      /* reserve for cpmac */
#define REMOTEDEV_HEADROOM	(sizeof(struct ethhdr)+sizeof(struct remotedev_hdr))

#define REMOTEDEV_TYPE_RESERVED	     0x00
#define REMOTEDEV_TYPE_APPL			 0x01   /* Message */
#define REMOTEDEV_TYPE_PING          0x02
#define REMOTEDEV_TYPE_APPL_BYE		 0x03
#define REMOTE_TYP(a)				((a) & 0x3F)

#define REMOTEDEV_TYPE_START		0x80   /* verodert Start-Indikator */
#define REMOTEDEV_TYPE_END			0x40   /* verodert End-Indikator */
#define REMOTEDEV_TYPE_COMLETE		(REMOTEDEV_TYPE_START | REMOTEDEV_TYPE_END)

#define REMOTEDEV_SEND_BUFSIZ       (128+2048)
#define REMOTEDEV_MAX_FRAME_SIZE    1500

#define MSG_TYPE_PROFILE    0
#define MSG_TYPE_CAPI       1

static LIST_HEAD(capi_conn_list);
static  DEFINE_SPINLOCK(list_lock);
static struct net_device *capi_netdev;   /* interface if it is there */

static void capi_oslib_socket_get(struct work_struct *work);
static int capi_oslib_netdev_notifier_event(struct notifier_block *notifier, unsigned long event, void *ptr);
static int capi_oslib_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt __attribute__((unused)), struct net_device* orig_dev);
static unsigned int capi_oslib_init_done;
static char g_remote_device[128];

static struct notifier_block capi_oslib_netdev_notifier = {
   .notifier_call = capi_oslib_netdev_notifier_event
};
static struct packet_type rcapi_packet_type = {
	func:	capi_oslib_recv,
};
/*--- #define SOCKET_IF_LOCK_TRC(args...)	printk(KERN_DEBUG args) ---*/
#define SOCKET_IF_LOCK_TRC(args...)

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void __capi_oslib_conn_get(struct _capi_connections *conn, int count, const char *text __maybe_unused) {
	atomic_add(count, &conn->link);
	if(atomic_read(&conn->link) < 4) {
		SOCKET_IF_LOCK_TRC("%s conn=%p conindex=%d link=%d: %s\n", __func__, conn, conn->conindex, atomic_read(&conn->link), text);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void capi_oslib_conn_get(struct _capi_connections *conn, const char *text) {
	__capi_oslib_conn_get(conn, 1, text);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void capi_oslib_conn_put(struct _capi_connections *conn, const char *text __maybe_unused) {

	if(atomic_read(&conn->link) == 0) {
		printk(KERN_ERR"%s conn=%p conindex=%d error on link=%d: %s\n", __func__, conn, conn->conindex, atomic_read(&conn->link), text);
		return;
	}
	if (atomic_dec_and_test(&conn->link)) {
		SOCKET_IF_LOCK_TRC("%s conn=%p conindex=%d free! %s\n", __func__, conn, conn->conindex, text);
		if (conn->recombine_buffer) {
			kfree(conn->recombine_buffer);
		}
		kfree(conn);
		return;
	}
	if(atomic_read(&conn->link) < 3) {
		SOCKET_IF_LOCK_TRC("%s conn=%p conindex=%d link=%d: %s\n", __func__, conn, conn->conindex, atomic_read(&conn->link), text);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void capi_oslib_trigger_rxwork(struct workqueue_struct *rxwork, void *_conn) {
	struct _capi_connections *conn = (struct _capi_connections *)_conn;

	capi_oslib_conn_get(conn, __func__);
	if(queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, rxwork, &conn->rx_work) == 0) {
		capi_oslib_conn_put(conn, __func__);
	}
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static inline struct net_device *capi_device(void) {
   if (capi_netdev) {
	  return capi_netdev;
   }
   capi_netdev = dev_get_by_name( &init_net, g_remote_device);
   if (capi_netdev) {
	  printk(KERN_NOTICE "capi_oslib: device %s now there.\n", capi_netdev->name);
   }
   return capi_netdev;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_remove_conn(struct work_struct *work) {
	struct _capi_connections *conn = container_of(work, struct _capi_connections, remove);

	SOCKET_IF_LOCK_TRC(KERN_DEBUG "capi_oslib_remove_conn for %p connindex %d link %d\n", conn, conn->conindex, conn->link);
	skb_queue_purge(&conn->recvqueue);
	spin_lock_bh(&list_lock);
	list_del(&conn->list);
	spin_unlock_bh(&list_lock);
	capi_oslib_conn_put(conn, "list_del");
	if (conn->ApplId != (unsigned)-1) {
		LOCAL_CAPI_SET_NOTIFY(SOURCE_SOCKET_CAPI, conn->ApplId, NULL);
		LOCAL_CAPI_RELEASE(SOURCE_SOCKET_CAPI, conn->ApplId);
		capi_oslib_conn_put(conn, "LOCAL_CAPI_SET_NOTIFY(0)");
		conn->ApplId = (unsigned)-1;
	}
	capi_oslib_conn_put(conn, "capi_oslib_remove_conn() done"); /*--- end workqueue ---*/
	capi_oslib_conn_put(conn, "free"); /*--- kfree ---*/
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct sk_buff* capi_oslib_allocskb(unsigned size, unsigned char type, unsigned char connindex, unsigned int seqnr, struct net_device *dev, unsigned char** dataptr, int priority)
{
	struct sk_buff* skb;
	struct ethhdr *ethh;
	struct remotedev_hdr *rhdr;
	u8 *data;

	skb = alloc_skb(REMOTEDEV_RESERVE + REMOTEDEV_HEADROOM + size, priority);
	if (unlikely(!skb)) return NULL;

	skb_reserve(skb, REMOTEDEV_RESERVE);                        /* reserve headroom for cpmac */
	(void)skb_put(skb, REMOTEDEV_HEADROOM + size);              /* set length */
	ethh = (struct ethhdr *)skb->data;
	memset(ethh->h_dest, 0xff, ETH_ALEN);
	memcpy(ethh->h_source, dev->dev_addr, ETH_ALEN);
	ethh->h_proto  = rcapi_packet_type.type;
	rhdr = (struct remotedev_hdr *) (ethh + 1);
	rhdr->type     = type;
	rhdr->conindex = connindex;
	rhdr->seqnr	   = htons(seqnr);
	rhdr->len      = htons(size);
	data = (u8 *) (rhdr + 1);
	skb->dev = dev;
	skb->protocol = ethh->h_proto;
	skb->pkt_type = PACKET_OUTGOING;
	skb_reset_network_header(skb);

	if (dataptr) {
		*dataptr = data;
	}
	return skb;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_send_too_big_conf(unsigned char* msg, struct _capi_connections *conn)
{
	struct sk_buff *skb;
	int rc;
	struct net_device *dev;
	unsigned char* data;

	dev = capi_device();
	if (unlikely(!dev)) {
		printk(KERN_WARNING "capi_oslib: send data conf but no dev!\n");
		return;
	}
	skb = capi_oslib_allocskb(17, REMOTEDEV_TYPE_APPL | REMOTEDEV_TYPE_COMLETE, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
	if (unlikely(!skb)) {
		printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
		return;
	} else {
		*data++ = 1;    /* CAPI Message */
		memcpy(data, msg, 16);
		*(unsigned short*)&data[0] = 16;
		data[5] = 0x81;
		*(unsigned short*)&data[12] = *(unsigned short*)&data[18];
		*(unsigned short*)&data[14] = 0x300c;       /* Data Length not supported by current protocol */

		rc = dev_queue_xmit(skb); /* queue paket for transmitting */

		if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
			printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
			return;
		}
		conn->send_seqnr++;
	}
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_socket_put (struct work_struct *work) {
	struct _capi_connections *conn = container_of(work, struct _capi_connections, tx_work);
	unsigned int status = ERR_SendBusy;
	struct sk_buff *skb;
	unsigned short ApplId;
	struct remotedev_hdr *rhdr;

	while ((skb = skb_dequeue(&conn->recvqueue))) {
		/*--- printk(KERN_DEBUG "capi_oslib_socket_put: skb %p\n", skb); ---*/
		rhdr = (struct remotedev_hdr *) skb->data;
		if ((rhdr->type & REMOTEDEV_TYPE_END) == REMOTEDEV_TYPE_END) {
			unsigned msg_len = 0;
			struct __attribute__ ((packed)) _capi_message *C;
			unsigned char* buffer;

			if ((rhdr->type & REMOTEDEV_TYPE_START) == REMOTEDEV_TYPE_START) {
				skb_pull(skb, sizeof(struct remotedev_hdr) + 1); /* strip header + MSG_TYPE_CAPI/MSG_TYPE_PROFILE */
				buffer = (unsigned char*)skb->data;
			} else {
				/*--- printk(KERN_DEBUG "capi_oslib_socket_put: got REMOTEDEV_TYPE_APPL_END\n"); ---*/
				skb_pull(skb, sizeof(struct remotedev_hdr)); /* strip headers */
				/*--- printk(KERN_DEBUG "capi_oslib: appending to recombine buffer\n"); ---*/
				if (conn->recombine_buffer && (conn->recombine_len + ntohs(rhdr->len) < REMOTEDEV_SEND_BUFSIZ)) {
					memcpy(&conn->recombine_buffer[conn->recombine_len], skb->data, ntohs(rhdr->len));
					conn->recombine_len = 0;
					buffer = conn->recombine_buffer;
				} else {
					printk(KERN_ERR "capi_oslib: recombine_buffer too small!\n");
					conn->recombine_len = 0;
					break;
				}
			}
			C = (struct __attribute__ ((packed)) _capi_message *)buffer;
			msg_len = copy_word_from_le_unaligned((unsigned char *)&C->capi_message_header.Length);

			switch (buffer[4]) {
				case 0x86:    /* DATA_B3_REQ */
					if (buffer[5] == 0x80) {
						struct __attribute__ ((packed)) _capi_message *C = (struct __attribute__ ((packed)) _capi_message *)buffer;
						unsigned char* data_buffer = NULL;

						if (conn->B3BlockSize >= copy_word_from_le_unaligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen)) {
							data_buffer = LOCAL_CAPI_NEW_DATA_B3_REQ_BUFFER(SOURCE_SOCKET_CAPI,
									copy_word_from_le_unaligned((unsigned char *)&C->capi_message_header.ApplId), copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.NCCI));
							if (data_buffer) {
								memcpy(data_buffer, &buffer[msg_len], copy_word_from_le_unaligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen));
								set_le_unaligned_dword(&C->capi_message_part.data_b3_req.Data, (unsigned long)data_buffer);
#if defined(CAPIOSLIB_CHECK_LATENCY)
								/*--- capi_generate_timestamp(0x20 + (conn->ApplId & 0x1F), data_buffer, copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen)); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
							}
						} else {
							printk(KERN_ERR "capi_oslib: received B3Msg of len=%d too big, dropping.\n", copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen));
							capi_oslib_send_too_big_conf(buffer, conn);
							goto next_message;
						}
					}
					/* TODO Laenge im skb ggf. setzen */
					break;
				case 0xFD:
					{
						unsigned maxNCCIs;
						unsigned windowsize;
						unsigned b3_blocksize;
						unsigned result;
						struct sk_buff *response_skb;
						u8 *data;
						int rc;
						struct net_device *dev;

						/*--- printk(KERN_DEBUG "capi_oslib: got register\n"); ---*/
						dev = capi_device();
						if (unlikely(!dev)) {
							printk(KERN_WARNING "capi_oslib: send register response but no dev!\n");
							goto next_message;
						}

						if (conn->ApplId != (unsigned)-1) {
							printk(KERN_WARNING "capi_oslib: register but conn %d already has ApplId %d\n", conn->conindex, conn->ApplId);
							LOCAL_CAPI_SET_NOTIFY(SOURCE_SOCKET_CAPI, conn->ApplId, NULL);
							LOCAL_CAPI_RELEASE(SOURCE_SOCKET_CAPI, conn->ApplId);
							capi_oslib_conn_put(conn, "re-register");
							conn->ApplId = (unsigned)-1;
							skb_queue_purge(&conn->recvqueue);
						}
						maxNCCIs	 = copy_dword_from_le_unaligned(&buffer[8]);
						windowsize   = copy_dword_from_le_unaligned(&buffer[12]);
						b3_blocksize = copy_dword_from_le_unaligned(&buffer[16]);
						conn->B3BlockSize = b3_blocksize;
						/*--- printk(KERN_DEBUG "%s: b3_blocksize=%x windowsize=%x maxNCCIs=%x\n", __func__, b3_blocksize, windowsize, maxNCCIs); ---*/
						result = LOCAL_CAPI_REGISTER(SOURCE_SOCKET_CAPI, 1024 + (1024 * maxNCCIs), maxNCCIs, windowsize, b3_blocksize, &conn->ApplId);
						/*--- printk(KERN_DEBUG "register result: %d\n", result); ---*/
						if (result == ERR_NoError) {
							unsigned char response[10];

							/* Antwort mit ApplID schicken */
							memcpy((void*)response, buffer, 10);
							set_le_unaligned_word(&response[2], conn->ApplId);         /* ApplID einsetzen */
							set_le_unaligned_word(&response[8], 0);

							response_skb = capi_oslib_allocskb(11, REMOTEDEV_TYPE_APPL | REMOTEDEV_TYPE_COMLETE, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
							if (unlikely(!response_skb)) {
								printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
								goto next_message;
							} else {
								*data++ = 1;    /* CAPI Message */
								memcpy(data, response, 10);

								rc = dev_queue_xmit(response_skb); /* queue paket for transmitting */

								if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
									printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
									goto next_message;
								}
								conn->send_seqnr++;
							}

							/*--- printk(KERN_DEBUG "register success result sent\n"); ---*/
							/* workqueue item erzeugen und einhaengen */
							INIT_WORK(&conn->rx_work, capi_oslib_socket_get);
							capi_oslib_conn_get(conn, "LOCAL_CAPI_SET_NOTIFY");
							result = LOCAL_CAPI_SET_NOTIFY(SOURCE_SOCKET_CAPI, conn->ApplId, conn);
						} else {
							unsigned char response[10];

							conn->ApplId = (unsigned)-1;
							/* Fehlerwert zurueckschicken */
							memcpy((void*)response, buffer, 10);
							set_le_unaligned_word(&response[8], result);

							response_skb = capi_oslib_allocskb(11, REMOTEDEV_TYPE_APPL | REMOTEDEV_TYPE_COMLETE, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
							if (unlikely(!response_skb)) {
								printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
								goto next_message;
							} else {
								*data++ = 1;    /* CAPI Message */
								memcpy(data, response, 10);

								rc = dev_queue_xmit(response_skb); /* queue paket for transmitting */

								if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
									printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
									goto next_message;
								}
								conn->send_seqnr++;
							}
						}
					}
					break;
				default:
					break;
			}
			ApplId = copy_word_from_le_unaligned(&buffer[2]);
			status = LOCAL_CAPI_PUT_MESSAGE(SOURCE_SOCKET_CAPI, ApplId, buffer);
			if ((status == ERR_SendBusy) || (status == ERR_QueueFull)) {
				if (rhdr->type == REMOTEDEV_TYPE_END) {
					conn->recombine_len -= ntohs(rhdr->len);
				}
				skb_queue_head(&conn->recvqueue, skb);
				capi_oslib_conn_put(conn, "capi_oslib_socket_put() done(#1)");
				return;
			}
			if (status == ERR_OS_Resource) {
				unsigned short len;
				len = buffer[0] | (buffer[1] << 8);
				printk(KERN_ERR "capi_oslib_socket_put got ERR_OS_Resource for ApplId %d, Len=%d!\n", ApplId, len);
			}
		} else {
			int pkt_len = ntohs(rhdr->len);
			if ((rhdr->type & REMOTEDEV_TYPE_START) == REMOTEDEV_TYPE_START) {
				if(conn->recombine_len) {
					printk(KERN_DEBUG "illegal recombine_len=%d\n", conn->recombine_len);
					conn->recombine_len = 0;
				}
				/*--- printk(KERN_DEBUG "erster Block buffer=%p len=%d\n", conn->recombine_buffer, ntohs(rhdr->len)); ---*/
				skb_pull(skb, sizeof(struct remotedev_hdr) + 1); /* strip headers */
				pkt_len--;
			} else {
				/*--- printk(KERN_DEBUG "weiterer Block len=%d\n", ntohs(rhdr->len)); ---*/
				skb_pull(skb, sizeof(struct remotedev_hdr)); /* strip headers */
			}
			if (conn->recombine_buffer == NULL) {
				conn->recombine_buffer = kmalloc(REMOTEDEV_SEND_BUFSIZ, GFP_KERNEL);
				if (conn->recombine_buffer == NULL) {
					printk(KERN_ERR "capi_oslib: kmalloc failed\n");
					goto next_message;
				} else {
					/*--- printk(KERN_DEBUG "capi_oslib: buffer %p alloced\n", conn->recombine_buffer); ---*/
				}
			}
			if ((conn->recombine_len + pkt_len) <= REMOTEDEV_SEND_BUFSIZ) {
				memcpy(&conn->recombine_buffer[conn->recombine_len], skb->data, pkt_len);
				conn->recombine_len += pkt_len;
			} else {
				printk(KERN_ERR "capi_oslib: recombine buffer too small!\n");
			}
		}
next_message:
		kfree_skb(skb);
	}
	capi_oslib_conn_put(conn, "capi_oslib_socket_put() done");
	return;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_socket_get (struct work_struct *work){
	struct _capi_connections *conn = container_of(work, struct _capi_connections, rx_work);
	unsigned int status = ERR_NoError;
	unsigned char* buffer = NULL;

	if (conn->ApplId == (unsigned)-1) {
		capi_oslib_conn_put(conn, "capi_oslib_socket_get() done(#1)");
		return;
	}
	while (status == ERR_NoError) {
		status = LOCAL_CAPI_GET_MESSAGE(SOURCE_SOCKET_CAPI, conn->ApplId, &buffer, CAPI_NO_SUSPEND);
		DEB_INFO("LOCAL_CAPI_GET_MESSAGE(%d, CAPI_NO_SUSPEND) -> %04x, %p\n", conn->ApplId, status, buffer);
		if (status == ERR_NoError) {
			struct __attribute__ ((packed)) _capi_message *C = (struct __attribute__ ((packed)) _capi_message *)buffer;
			unsigned total_msg_len;
			struct sk_buff *skb;
			u8 *data;
			int rc;
			struct net_device *dev;
			unsigned left;
			unsigned data_left;
			unsigned msg_left;

			dev = capi_device();
			if (likely(dev)) {
				total_msg_len = copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length) + 1;
				if (CA_IS_DATA_B3_IND(buffer)) {
					total_msg_len += copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen);
					data_left = copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen);
				} else {
					data_left = 0;
				}
				msg_left = copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length);
				left = total_msg_len;
				while (left) {
					unsigned int thissize;
					unsigned int tocopy = 0;
					unsigned char type;

					thissize = min((unsigned int)left, (unsigned int)(REMOTEDEV_MAX_FRAME_SIZE - REMOTEDEV_HEADROOM));
					type = REMOTEDEV_TYPE_APPL;
					if (thissize == total_msg_len) {
						type |= REMOTEDEV_TYPE_COMLETE;
					} else if (left > thissize) {
						type |= (left == total_msg_len) ? REMOTEDEV_TYPE_START : 0;
					} else {
						type |= REMOTEDEV_TYPE_END;
					}
					skb = capi_oslib_allocskb(thissize, type, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
					if (unlikely(!skb)) {
						printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
						break;
					}
					left -= thissize;
					if ((type & REMOTEDEV_TYPE_START) == REMOTEDEV_TYPE_START) {
						*data++   = MSG_TYPE_CAPI;    /* normale CAPI Message */
						thissize -= 1;
					}
					if (msg_left) {
						/* noch Message uebrig */
						tocopy = min((unsigned int)thissize, msg_left);
						memcpy(data, &buffer[copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length) - msg_left], tocopy);
						msg_left -= tocopy;
						data += tocopy;
						thissize -= tocopy;
					}

					if (CA_IS_DATA_B3_IND(buffer)) {
						if ((data_left) && (thissize > 0)) {
							unsigned char *pdata_ind = (unsigned char *)copy_dword_from_le_aligned(&C->capi_message_part.data_b3_ind.Data);
							unsigned int   offset =  copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen);
							/* Message komplett && noch Platz fuer Daten uebrig */
							tocopy = min((unsigned int)thissize, data_left);
							memcpy(data, &pdata_ind[offset - data_left], tocopy);
							data_left -= tocopy;
						}
					}

					rc = dev_queue_xmit(skb); /* queue paket for transmitting */

					if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
						printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
					}
					conn->send_seqnr++;
				}
			} else {
				printk(KERN_WARNING "capi_oslib: drop msg, no dev\n");
			}
		}
		/*--- printk(KERN_DEBUG "get done\n"); ---*/
	}
	capi_oslib_conn_put(conn, "capi_oslib_socket_get() done");
	return;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _capi_connections *get_capioslib_connection(int conn_index, int alloc) {
	struct _capi_connections *conn = NULL;
	struct list_head *lp;

	spin_lock_bh(&list_lock);
	list_for_each(lp, &capi_conn_list) {
		conn = list_entry(lp, struct _capi_connections, list);
		if (conn->conindex == conn_index) {
			capi_oslib_conn_get(conn, "get_capioslib_connection");
			spin_unlock_bh(&list_lock);
			return conn;
		}
	}
	spin_unlock_bh(&list_lock);
	if(alloc == 0) {
		return NULL;
	}
	conn = kzalloc(sizeof(struct _capi_connections), GFP_KERNEL);
	if (conn == NULL) {
		return NULL;
	}
	conn->conindex		   = conn_index;
	conn->ApplId		   = (unsigned)-1;
	__capi_oslib_conn_get(conn, 3, "alloc+list_add+first_get");
	skb_queue_head_init(&conn->recvqueue);

	spin_lock_bh(&list_lock);
	list_add(&conn->list, &capi_conn_list);
	spin_unlock_bh(&list_lock);

	INIT_WORK(&conn->tx_work, capi_oslib_socket_put);
	return conn;
}
/*-------------------------------------------------------------------------------------*\
 * CAPI Profile etc.pp rueberschicken
 *
 * 4 Bytes Num. Controllers
 *      64 Bytes Manufacturer
 *      4  Bytes CAPI Major
 *      4  Bytes CAPI Minor
 *      4  Bytes Manu Major
 *      4  Bytes Manu Minor
 *      8  Bytes Serial No.
 *      64 Bytes Profile
\*-------------------------------------------------------------------------------------*/
static int capioslib_send_profile(struct _capi_connections *conn) {
	unsigned char info[64];
	unsigned int num_controllers, i;
	struct sk_buff *skb;
	unsigned total_msg_len;
	u8 *data;
	int rc;
	struct net_device *dev;

	dev = capi_device();
	if (unlikely(!dev)) {
		printk(KERN_WARNING "capi_oslib: send profile but no dev!\n");
		return -1;
	}
	CAPI_GET_PROFILE(info, 0);
	num_controllers = EXTRACT_DWORD(info);

	total_msg_len = 4 + (num_controllers * 152) + 1;
	skb = capi_oslib_allocskb(total_msg_len, REMOTEDEV_TYPE_APPL | REMOTEDEV_TYPE_COMLETE, conn->conindex, conn->send_seqnr, dev, &data, GFP_ATOMIC);
	if (unlikely(!skb)) {
		printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
		return -1;
	}
	*data++ = MSG_TYPE_PROFILE;    /* keine CAPI Message */
	memcpy(data, info, sizeof(unsigned int));
	data += sizeof(unsigned int);
	for (i = 1; i <= num_controllers; i++) {
		CAPI_GET_MANUFACTURER(info);
		DEB_INFO("start info controller %d\n", i);
		memcpy(data, info, 64);
		data += 64;

		CAPI_GET_VERSION((unsigned int*)&info[0], (unsigned int*)&info[4], (unsigned int*)&info[8],
						 (unsigned int*)&info[12]);
		memcpy(data, info, 16);
		data += 16;

		CAPI_GET_SERIAL_NUMBER(i, info);
		memcpy(data, info, 8);
		data += 8;

		CAPI_GET_PROFILE(info, i);
		memcpy(data, info, 64);
		data += 64;
	}
	rc = dev_queue_xmit(skb); /* queue paket for transmitting */
	if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
		printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
	}
	conn->send_seqnr++;
	return 0;
}

/**
 */
static void send_pong(void) {
	struct sk_buff *pong_skb;
	int rc;
	struct net_device *dev;

	dev = capi_device();
	if (unlikely(!dev)) {
		printk(KERN_WARNING "capi_oslib: got ping but have no dev!\n");
		return;
	}
	pong_skb = capi_oslib_allocskb(0, REMOTEDEV_TYPE_PING, 0, 0, dev, NULL, GFP_ATOMIC);
	if (unlikely(!pong_skb)) {
		printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
		return;
	}
	rc = dev_queue_xmit(pong_skb); /* queue paket for transmitting */

	if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
		printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
	}
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static int capi_oslib_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt __attribute__((unused)), struct net_device* orig_dev __attribute__((unused))) {
	int ret = NET_RX_SUCCESS;
	struct timeval tv;
	struct remotedev_hdr *rhdr;
	struct _capi_connections *conn = NULL;

	if (dev != capi_device()) {
		kfree_skb(skb);
		return NET_RX_DROP;
	}
#ifndef this_checks_are_not_really_needed_in_our_environment
	if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL) {
		printk(KERN_ERR "capi_oslib: recv: skb_share_check failed\n");
		kfree_skb(skb);
		return NET_RX_DROP;
	}
	if (skb_is_nonlinear(skb)) {
		if (skb_linearize(skb) != 0) {
			printk(KERN_ERR "capi_oslib: recv: skb_linearize failed\n");
			kfree_skb(skb);
			return NET_RX_DROP;
		}
	}
	if (skb_cloned(skb) && !skb->sk) {
		struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);

		if (!nskb) {
			printk(KERN_ERR "capi_oslib: recv: skb_copy failed\n");
			kfree_skb(skb);
			return NET_RX_DROP;
		}
		kfree_skb(skb);
		skb = nskb;
	}
#endif
	if (unlikely(skb->len < sizeof(struct remotedev_hdr))) {
		printk(KERN_ERR "capi_oslib: recv: packet too small\n");
		kfree_skb(skb);
		return NET_RX_DROP;
	}
	rhdr = (struct remotedev_hdr *) skb->data;

	switch (REMOTE_TYP(rhdr->type)) {
		case REMOTEDEV_TYPE_APPL:
			conn = get_capioslib_connection(rhdr->conindex, 1);
			if(conn == NULL) {
				ret = NET_RX_DROP;
				break;
			}
			if(ntohs(rhdr->seqnr) != conn->recv_seqnr) {
				printk(KERN_WARNING "capi_oslib: recv: conn %d lost packet(s) seq-nr %d != %d(cdev) !\n", conn->conindex, ntohs(rhdr->seqnr), conn->recv_seqnr);
			}
			conn->recv_seqnr = ntohs(rhdr->seqnr) + 1;
			skb_get_timestamp(skb, &tv);
			if (tv.tv_sec == 0) {
				__net_timestamp(skb);
			}
			if(likely((rhdr->type & REMOTEDEV_TYPE_START) == REMOTEDEV_TYPE_START)) {
				unsigned char type;
				if (skb->len  <= 0) {
					capi_oslib_conn_put(conn, "false skb_len");
					break;
				}
				type = skb->data[sizeof(struct remotedev_hdr)];
				switch (type) {
					case MSG_TYPE_PROFILE:
						skb_pull(skb, sizeof(struct remotedev_hdr)); /* strip headers */
						if ((skb->len >= 6) && (memcmp(skb->data, "\0Hallo", 6) == 0)) {
							/*--- printk(KERN_DEBUG "capi_oslib: got greetings, send profile\n"); ---*/

							if(capioslib_send_profile(conn)) {
								INIT_WORK(&conn->remove, capi_oslib_remove_conn);
								if(queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->remove) == 0) {
									capi_oslib_conn_put(conn, "capi_oslib_recv() pending remove-wkq(#0)");
								}
								break;
							}
						}
						capi_oslib_conn_put(conn, "MSG_TYPE_PROFILE end");
						break;
					case MSG_TYPE_CAPI:
						skb_queue_tail(&conn->recvqueue, skb); /* add to receive queue */
						skb = NULL;
						if(queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->tx_work) == 0) {
							capi_oslib_conn_put(conn, "capi_oslib_recv() pending tx-wkq(#0)");
						}
						break;
					default:
						printk(KERN_ERR "capi_oslib: unknown type2 %d\n", type);
						capi_oslib_conn_put(conn, "MSG_TYPE_unknown");
						break;
				}
				break;
			}
			skb_queue_tail(&conn->recvqueue, skb); /* add to receive queue */
			skb = NULL;
			if(queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->tx_work) == 0) {
				capi_oslib_conn_put(conn, "capi_oslib_recv() pending tx-wkq(#1)");
			}
			break;
		case REMOTEDEV_TYPE_APPL_BYE:
			conn = get_capioslib_connection(rhdr->conindex, 0);
			if(conn == NULL) {
				ret = NET_RX_DROP;
				break;
			}
			SOCKET_IF_LOCK_TRC(KERN_DEBUG "capi_oslib: bye received conindex=%d\n", conn->conindex);
			INIT_WORK(&conn->remove, capi_oslib_remove_conn);
			if(queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->remove) == 0) {
				capi_oslib_conn_put(conn, "capi_oslib_recv() pending remove-wkq(#1)");
			}
			break;
		case REMOTEDEV_TYPE_PING:
			/*--- printk(KERN_DEBUG "capi_oslib: ping received %d\n", REMOTE_TYP(rhdr->type)); ---*/
			send_pong();
			break;
		default:
			printk(KERN_ERR "capi_oslib: recv: type %x not implemented\n", REMOTE_TYP(rhdr->type));
			break;
	}
	if(skb) {
		kfree_skb(skb);
	}
	return ret;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static int capi_oslib_netdev_notifier_event(struct notifier_block *notifier __attribute__((unused)),
									   unsigned long event, void *ptr) {
   struct net_device *dev = (struct net_device *) ptr;

   switch (event) {
	  case NETDEV_UNREGISTER:
		 if (dev == capi_netdev) {
			printk(KERN_NOTICE "capi_oslib: device %s gone.\n", dev->name);
			capi_netdev = 0;
		 }
		 break;

	  case NETDEV_REGISTER:
		 /*--- printk(KERN_NOTICE "capi_oslib: device %s NETDEV_REGISTER.\n", dev->name); ---*/
		 capi_device();
		 break;
   }
   return NOTIFY_DONE;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void capi_oslib_socket_init(char *remote_device, unsigned short h_proto){
	LOCAL_CAPI_INIT(SOURCE_SOCKET_CAPI);
	snprintf(g_remote_device, sizeof(g_remote_device), "%s", remote_device);
	register_netdevice_notifier(&capi_oslib_netdev_notifier);
	rcapi_packet_type.type = htons(h_proto);
	dev_add_pack(&rcapi_packet_type);
	/*--- kernel_thread(capi_oslib_socket_accept_thread, NULL, 0); ---*/
	capi_remote_put_workqueue = create_workqueue("capi_remote_put");
	capi_oslib_init_done = 1;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void capi_oslib_socket_release(void)
{
	if(capi_oslib_init_done == 0) {
		return;
	}
	capi_oslib_init_done = 0;
#if 0
	if (capi_socket) {
		sock_release(capi_socket);
		capi_socket = NULL;
	}
#endif
	dev_remove_pack(&rcapi_packet_type);
	unregister_netdevice_notifier(&capi_oslib_netdev_notifier);
}
