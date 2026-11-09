// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2007-2019 AVM GmbH <fritzbox_info@avm.de> */

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
#include <linux/poll.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/init.h>
#include <avm/event/event.h>
#include "avm_debug.h"

MODULE_DESCRIPTION("AVM Remote-Device Server");
MODULE_LICENSE("GPL");

/*--- #define RDEV_LOCK_TRC(args...)	printk(KERN_ERR args) ---*/
#define RDEV_LOCK_TRC(args...)
/*--- #define DBG_TRC(args...)	printk(KERN_DEBUG args) ---*/
#define DBG_TRC(args...)
/*--- #define DBG_INFO(args...)	printk(KERN_INFO args) ---*/
#define DBG_INFO(args...)

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _remotedev_server {
	struct list_head list; /*--- eingehaengt in remotedev_server_list ---*/
	struct list_head conn_list; /*--- die connection-list ---*/
	atomic_t link;
	spinlock_t list_lock;
	struct workqueue_struct *rcv_workqueue;
	struct packet_type rpacket_type;
	u16 eth_p;
	/*--------------------------------------------------------------------------------*\
	 * Callback: Remote-Device wurde geoeffnet
	 * conn:    Connection-Handle
	 * h_proto: ETH-Protokoll
	 * ret: NULL Verbindung ablehnen, sonst rhandle
	\*--------------------------------------------------------------------------------*/
	void *(*remotedev_open)(unsigned short h_proto);
	/*--------------------------------------------------------------------------------*\
	* Callback Remote-Device schreibt
	* rhandle: Handle von remotedev_open()
	* buf, len: die Daten
	\*--------------------------------------------------------------------------------*/
	void (*remotedev_rcv)(void *rhandle, unsigned char *buf,
			      unsigned int len);
	/*--------------------------------------------------------------------------------*\
	 * optional: Callback RemoteSeite macht read mit angeforderter Laenge
	 * rhandle: Handle von remotedev_open()
	 * len: die angeforderte Laenge
	\*--------------------------------------------------------------------------------*/
	void (*remotedev_demand)(void *rhandle, unsigned int demandlen);
	/*--------------------------------------------------------------------------------*\
	 * Callback Remote-Device wurde geschlossen
	 * internal_error: 0 von Gegenseite, sonst internes Problem
	\*--------------------------------------------------------------------------------*/
	void (*remotedev_close)(void *rhandle, int internal_error);
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _remotedev_server_conn {
	struct list_head
		list; /*--- eingehaengt in remotedev_server->conn_list ---*/
	atomic_t link;
	struct _remotedev_server *prdevs;
	void *rhandle;
	struct sk_buff_head recvqueue; /* pakets */
	struct work_struct rx_work;
	u8 ignore_packet;
	u8 conindex;
	u16 send_seqnr;
	u16 recv_seqnr;
	u8 *recombine_buffer; /* Buffer for recombining messages */
	unsigned recombine_len;
	unsigned max_recombine_len;
};

struct remotedev_hdr {
	u8 type;     /* REMOTEDEV_TYPE_APPL */
	u8 conindex; /* application identifier */
	u16 seqnr;   /* Sequence Number */
	u16 len;     /* Payload length */
};

#define REMOTEDEV_RESERVE 16 /* reserve for cpmac */
#define REMOTEDEV_HEADROOM                                                     \
	(sizeof(struct ethhdr) + sizeof(struct remotedev_hdr))

#define REMOTEDEV_TYPE_RESERVED    0x00
#define REMOTEDEV_TYPE_APPL        0x01 /* Message */
#define REMOTEDEV_TYPE_PING        0x02
#define REMOTEDEV_TYPE_APPL_BYE    0x03
#define REMOTEDEV_TYPE_APPL_DEMAND 0x04 /* Daten der Laenge x anfordern */
#define REMOTE_TYP(a)              ((a) & 0x3F)

#define REMOTEDEV_TYPE_START       0x80 /* verodert Start-Indikator */
#define REMOTEDEV_TYPE_END         0x40 /* verodert End-Indikator */
#define REMOTEDEV_TYPE_COMLETE     (REMOTEDEV_TYPE_START | REMOTEDEV_TYPE_END)

#define REMOTEDEV_MAX_FRAME_SIZE   1500

static DEFINE_SPINLOCK(remotedev_server_list_lock);
static LIST_HEAD(remotedev_server_list);
static DEFINE_SEMAPHORE(remotedev_server_sema);

static struct net_device *remotedev_netdev; /* interface if it is there */

static int remotedev_netdev_notifier_event(struct notifier_block *notifier,
					   unsigned long event, void *ptr);
static int remotedev_netrecv(struct sk_buff *skb, struct net_device *netdev,
			     struct packet_type *pt,
			     struct net_device *orig_dev);
static char g_remote_device[128];

static struct notifier_block remotedev_netdev_notifier = {
	.notifier_call = remotedev_netdev_notifier_event
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void remotedev_server_get(struct _remotedev_server *prdevs,
					const char *text)
{
	atomic_add(1, &prdevs->link);
	if (atomic_read(&prdevs->link) < 4) {
		RDEV_LOCK_TRC("%s serv=0x%x link=%d: %s\n", __func__,
			      prdevs->eth_p, atomic_read(&prdevs->link), text);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void remotedev_server_put(struct _remotedev_server *prdevs,
					const char *text)
{
	if (atomic_read(&prdevs->link) == 0) {
		printk(KERN_ERR "%s serv=0x%x error link already zero %s\n",
		       __func__, prdevs->eth_p, text);
		return;
	}
	if (atomic_dec_and_test(&prdevs->link)) {
		RDEV_LOCK_TRC("%s serv=0x%x free! %s\n", __func__,
			      prdevs->eth_p, text);
		kfree(prdevs);
		return;
	}
	if (atomic_read(&prdevs->link) < 3) {
		RDEV_LOCK_TRC("%s serv=0x%x link=%d: %s\n", __func__,
			      prdevs->eth_p, atomic_read(&prdevs->link), text);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void __remotedev_conn_get(struct _remotedev_server_conn *conn,
					int count,
					const char *text)
{
	atomic_add(count, &conn->link);
	if (atomic_read(&conn->link) < 4) {
		RDEV_LOCK_TRC("%s conn=%p conindex=%d link=%d: %s\n", __func__,
			      conn, conn->conindex, atomic_read(&conn->link),
			      text);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void remotedev_conn_get(struct _remotedev_server_conn *conn,
				      const char *text)
{
	__remotedev_conn_get(conn, 1, text);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void remotedev_conn_put(struct _remotedev_server_conn *conn,
			       const char *text)
{
	if (atomic_read(&conn->link) == 0) {
		printk(KERN_ERR "%s conn=%p conindex=%d error on link=%d: %s\n",
		       __func__, conn, conn->conindex, atomic_read(&conn->link),
		       text);
		return;
	}
	if (atomic_dec_and_test(&conn->link)) {
		RDEV_LOCK_TRC("%s conn=%p conindex=%d free! %s\n", __func__,
			      conn, conn->conindex, text);
		spin_lock_bh(&conn->prdevs->list_lock);
		list_del(&conn->list);
		spin_unlock_bh(&conn->prdevs->list_lock);
		skb_queue_purge(&conn->recvqueue);
		if (conn->recombine_buffer) {
			kfree(conn->recombine_buffer);
		}
		kfree(conn);
		return;
	}
	if (atomic_read(&conn->link) < 3) {
		RDEV_LOCK_TRC("%s conn=%p conindex=%d link=%d: %s\n", __func__,
			      conn, conn->conindex, atomic_read(&conn->link),
			      text);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _remotedev_server *get_remotedev_server(unsigned short h_proto)
{
	struct _remotedev_server *prdevs = NULL;
	struct list_head *lp;

	spin_lock_bh(&remotedev_server_list_lock);
	list_for_each(lp, &remotedev_server_list) {
		prdevs = list_entry(lp, struct _remotedev_server, list);
		if (prdevs->eth_p == h_proto) {
			remotedev_server_get(prdevs, "get_remotedev_server");
			spin_unlock_bh(&remotedev_server_list_lock);
			return prdevs;
		}
	}
	spin_unlock_bh(&remotedev_server_list_lock);
	return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _remotedev_server_conn *
get_remotedev_conn_by_conn_id(struct _remotedev_server *prdevs, u8 conn_id)
{
	struct _remotedev_server_conn *conn = NULL;
	struct list_head *lp;

	spin_lock_bh(&prdevs->list_lock);
	list_for_each(lp, &prdevs->conn_list) {
		conn = list_entry(lp, struct _remotedev_server_conn, list);
		if (conn->conindex == conn_id) {
			remotedev_conn_get(conn,
					   "get_remotedev_conn_by_conn_id()");
			spin_unlock_bh(&prdevs->list_lock);
			return conn;
		}
	}
	spin_unlock_bh(&prdevs->list_lock);
	return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _remotedev_server_conn *
get_remotedev_conn_by_rhandle(struct _remotedev_server *prdevs, void *rhandle)
{
	struct _remotedev_server_conn *conn = NULL;
	struct list_head *lp;

	spin_lock_bh(&prdevs->list_lock);
	list_for_each(lp, &prdevs->conn_list) {
		conn = list_entry(lp, struct _remotedev_server_conn, list);
		if (conn->rhandle == rhandle) {
			remotedev_conn_get(conn,
					   "get_remotedev_conn_by_rhandle()");
			spin_unlock_bh(&prdevs->list_lock);
			return conn;
		}
	}
	spin_unlock_bh(&prdevs->list_lock);
	return NULL;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static inline struct net_device *remotedev_device(void)
{
	if (remotedev_netdev) {
		return remotedev_netdev;
	}
	remotedev_netdev = dev_get_by_name(&init_net, g_remote_device);
	if (remotedev_netdev) {
		printk(KERN_ERR "%s: device '%s' now there.\n", __func__,
		       remotedev_netdev->name);
	}
	return remotedev_netdev;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void remove_connection(struct _remotedev_server *prdevs,
			      struct _remotedev_server_conn *conn,
			      int internal_error)
{
	RDEV_LOCK_TRC("remotedev_remove_conn for %p connindex %d link %d\n",
		      conn, conn->conindex, atomic_read(&conn->link));
	if (conn->rhandle) {
		prdevs->remotedev_close(conn->rhandle, internal_error);
		conn->rhandle = NULL;
	}
	remotedev_conn_put(conn, __func__);
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct sk_buff *
lremotedev_allocskb(unsigned size, unsigned char type, unsigned short h_proto,
		    unsigned char connindex, unsigned int seqnr,
		    struct net_device *netdev, unsigned char **dataptr,
		    int priority)
{
	struct sk_buff *skb;
	struct ethhdr *ethh;
	struct remotedev_hdr *rhdr;
	u8 *data;

	skb = alloc_skb(REMOTEDEV_RESERVE + REMOTEDEV_HEADROOM + size,
			priority);
	if (unlikely(!skb)) {
		return NULL;
	}
	skb_reserve(skb, REMOTEDEV_RESERVE); /* reserve headroom for cpmac */
	(void)skb_put(skb, REMOTEDEV_HEADROOM + size); /* set length */
	ethh = (struct ethhdr *)skb->data;
	memset(ethh->h_dest, 0xff, ETH_ALEN);
	memcpy(ethh->h_source, netdev->dev_addr, ETH_ALEN);
	ethh->h_proto = htons(h_proto);
	rhdr = (struct remotedev_hdr *)(ethh + 1);
	rhdr->type = type;
	rhdr->conindex = connindex;
	rhdr->seqnr = htons(seqnr);
	rhdr->len = htons(size);
	data = (u8 *)(rhdr + 1);
	skb->dev = netdev;
	skb->protocol = htons(h_proto);
	skb->pkt_type = PACKET_OUTGOING;
	skb_reset_network_header(skb);

	if (dataptr) {
		*dataptr = data;
	}
	return skb;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int append_to_recombine(struct _remotedev_server_conn *conn,
			       unsigned char *src, unsigned int len)
{
	unsigned char *newbuf;
	if (unlikely(len == 0)) {
		return 0;
	}
	if (unlikely(conn->recombine_buffer == NULL)) {
		conn->max_recombine_len = len * 2;
		conn->recombine_buffer =
			kmalloc(conn->max_recombine_len, GFP_KERNEL);
		if (conn->recombine_buffer == NULL) {
			return -ENOMEM;
		}
		memcpy(conn->recombine_buffer, src, len);
		conn->recombine_len = len;
		return len;
	}
	if ((len + conn->recombine_len) < conn->max_recombine_len) {
		memcpy(&conn->recombine_buffer[conn->recombine_len], src, len);
		conn->recombine_len += len;
		return len;
	}
	newbuf = kmalloc(conn->max_recombine_len + (len * 2), GFP_KERNEL);
	if (unlikely(newbuf == NULL)) {
		kfree(conn->recombine_buffer);
		conn->recombine_buffer = NULL;
		conn->recombine_len = 0;
		conn->max_recombine_len = 0;
		return -ENOMEM;
	}
	conn->max_recombine_len += (len * 2);
	memcpy(newbuf, conn->recombine_buffer, conn->recombine_len);
	kfree(conn->recombine_buffer);
	conn->recombine_buffer = newbuf;
	memcpy(&conn->recombine_buffer[conn->recombine_len], src, len);
	conn->recombine_len += len;
	return len;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int lremotedev_open(struct _remotedev_server_conn *conn)
{
	struct net_device *netdev;

	DBG_INFO("%s: call open(%x)\n", __func__, conn->prdevs->eth_p);
	conn->rhandle = conn->prdevs->remotedev_open(conn->prdevs->eth_p);
	if (conn->rhandle == NULL) {
		struct sk_buff *bye_skb;

		DBG_INFO("%s: open(%x) denied\n", __func__,
			 conn->prdevs->eth_p);
		netdev = remotedev_device();
		if (netdev) {
			bye_skb = lremotedev_allocskb(
				0, REMOTEDEV_TYPE_APPL_BYE, conn->prdevs->eth_p,
				conn->conindex, conn->send_seqnr, netdev, NULL,
				GFP_USER);
			if (bye_skb) {
				dev_queue_xmit(
					bye_skb); /* queue paket for transmitting */
			}
		}
		remove_connection(conn->prdevs, conn, 0);
		return -EPERM;
	}
	return 0;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void lremotedev_wrkq_rcv(struct work_struct *work)
{
	struct _remotedev_server_conn *conn =
		container_of(work, struct _remotedev_server_conn, rx_work);
	struct remotedev_hdr *rhdr;
	unsigned char *buf = NULL;
	unsigned int pkt_len;
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&conn->recvqueue))) {
		/*--- printk(KERN_DEBUG "%s: skb %p\n", __func__, skb); ---*/
		rhdr = (struct remotedev_hdr *)skb->data;
		pkt_len = ntohs(rhdr->len);
		switch (REMOTE_TYP(rhdr->type)) {
		case REMOTEDEV_TYPE_APPL_BYE:
			/*--- somit ist sichergestellt, dass close erst nach allen rcv-Daten erfolgt ---*/
			DBG_INFO("%s: close by bye %p\n", __func__,
				 conn->rhandle);
			remove_connection(conn->prdevs, conn, 0);
			remotedev_conn_put(conn,
					   "lremotedev_wrkq_rcv: bye(#1)");
			kfree_skb(skb);
			return;
		case REMOTEDEV_TYPE_APPL_DEMAND:
			if (conn->rhandle == NULL) {
				lremotedev_open(conn);
			}
			if (conn->rhandle && conn->prdevs->remotedev_demand) {
				conn->prdevs->remotedev_demand(conn->rhandle,
							       pkt_len);
			}
			kfree_skb(skb);
			remotedev_conn_put(conn,
					   "lremotedev_wrkq_rcv: demand done");
			return;
		}
		skb_pull(skb, sizeof(struct remotedev_hdr)); /* strip headers */
		if ((rhdr->type & REMOTEDEV_TYPE_START) ==
		    REMOTEDEV_TYPE_START) {
			if (conn->recombine_len) {
				printk(KERN_DEBUG
				       "%s: illegal recombine_len=%d\n",
				       __func__, conn->recombine_len);
				conn->recombine_len = 0;
				conn->ignore_packet = 0;
			}
		}
		if (skb->len < pkt_len) {
			printk("%s: skb_len=%d < rhdr->len=%d reduce len\n",
			       __func__, skb->len, pkt_len);
			pkt_len = min(skb->len, pkt_len);
		}
		if ((rhdr->type & REMOTEDEV_TYPE_END) == REMOTEDEV_TYPE_END) {
			if (unlikely(conn->ignore_packet)) {
				pkt_len = 0;
			} else if (conn->recombine_len == 0) {
				buf = skb->data; /*--- only one segment ---*/
			} else {
				/*--- append last segment ... ---*/
				if (append_to_recombine(conn, skb->data,
							pkt_len) >= 0) {
					buf = conn->recombine_buffer;
					pkt_len = conn->recombine_len;
				} else {
					printk(KERN_DEBUG
					       "%s: can't alloc recombine_buffer -> ignore\n",
					       __func__);
					pkt_len = 0;
				}
			}
			if (conn->rhandle == NULL) {
				if (lremotedev_open(conn)) {
					remotedev_conn_put(
						conn,
						"lremotedev_wrkq_rcv: bye(#2)");
					kfree_skb(skb);
					return;
				}
				DBG_INFO("%s: open(%x) done - rhandle=%p\n",
					 __func__, conn->prdevs->eth_p,
					 conn->rhandle);
			}
			if (pkt_len) {
				/*--- ... and call rcv-callback ---*/
				DBG_TRC("%s: rcv(%p, %p, %u)\n", __func__,
					conn->rhandle, buf, pkt_len);
				conn->prdevs->remotedev_rcv(conn->rhandle, buf,
							    pkt_len);
			}
			conn->recombine_len = 0;
			conn->ignore_packet = 0;
			kfree_skb(skb);
			continue;
		}
		if (append_to_recombine(conn, skb->data, pkt_len) < 0) {
			printk(KERN_DEBUG
			       "%s: can't alloc recombine_buffer -> ignore(#1)\n",
			       __func__);
			conn->ignore_packet = 1;
		}
		kfree_skb(skb);
	}
	remotedev_conn_put(conn, "lremotedev_wrkq_rcv() done");
	return;
}
/*--------------------------------------------------------------------------------*\
 * alloc: if not exist: add connection
\*--------------------------------------------------------------------------------*/
static struct _remotedev_server_conn *
get_connection(struct _remotedev_server *prdevs, int conn_index, int alloc)
{
	struct _remotedev_server_conn *conn = NULL;

	if ((conn = get_remotedev_conn_by_conn_id(prdevs, conn_index))) {
		return conn;
	}
	if (alloc == 0) {
		return NULL;
	}
	conn = kzalloc(sizeof(struct _remotedev_server_conn), GFP_ATOMIC);
	if (conn == NULL) {
		printk(KERN_ERR "remotedev: %s: cannot allocate conn!\n",
		       __func__);
		return NULL;
	}
	atomic_set(&conn->link, 0);
	conn->conindex = conn_index;
	conn->prdevs = prdevs;
	__remotedev_conn_get(conn, 2, "alloc+first_get");
	skb_queue_head_init(&conn->recvqueue);
	INIT_WORK(&conn->rx_work, lremotedev_wrkq_rcv);
	spin_lock_bh(&prdevs->list_lock);
	list_add(&conn->list, &prdevs->conn_list);
	spin_unlock_bh(&prdevs->list_lock);
	return conn;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void send_pong(struct _remotedev_server *prdevs)
{
	struct sk_buff *pong_skb;
	int rc;
	struct net_device *netdev;

	netdev = remotedev_device();
	if (unlikely(!netdev)) {
		printk(KERN_WARNING
		       "remotedev: %s: got ping but have no netdev!\n",
		       __func__);
		return;
	}
	pong_skb = lremotedev_allocskb(0, REMOTEDEV_TYPE_PING, prdevs->eth_p, 0,
				       0, netdev, NULL, GFP_ATOMIC);
	if (unlikely(!pong_skb)) {
		printk(KERN_ERR "remotedev: %s: cannot allocate skb!\n",
		       __func__);
		return;
	}
	rc = dev_queue_xmit(pong_skb); /* queue paket for transmitting */

	if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
		printk(KERN_ERR "remotedev: %s: dev_queue_xmit()=%d\n",
		       __func__, rc);
	}
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static int remotedev_netrecv(struct sk_buff *skb, struct net_device *netdev,
			     struct packet_type *pt,
			     struct net_device *orig_dev)
{
	struct _remotedev_server *prdevs;
	int ret = NET_RX_SUCCESS;
	struct timeval tv;
	struct remotedev_hdr *rhdr;
	struct _remotedev_server_conn *conn = NULL;

	if (netdev != remotedev_device()) {
		printk(KERN_ERR "%s: no netdev\n", __func__);
		kfree_skb(skb);
		return NET_RX_DROP;
	}
#ifndef this_checks_are_not_really_needed_in_our_environment
	if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL) {
		printk(KERN_ERR "%s: recv: skb_share_check failed\n", __func__);
		kfree_skb(skb);
		return NET_RX_DROP;
	}
	if (skb_is_nonlinear(skb)) {
		if (skb_linearize(skb) != 0) {
			printk(KERN_ERR "%s: recv: skb_linearize failed\n",
			       __func__);
			kfree_skb(skb);
			return NET_RX_DROP;
		}
	}
	if (skb_cloned(skb) && !skb->sk) {
		struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);

		if (!nskb) {
			printk(KERN_ERR "%s: recv: skb_copy failed\n",
			       __func__);
			kfree_skb(skb);
			return NET_RX_DROP;
		}
		kfree_skb(skb);
		skb = nskb;
	}
#endif
	if (unlikely(skb->len < sizeof(struct remotedev_hdr))) {
		printk(KERN_ERR "%s: recv: packet too small\n", __func__);
		kfree_skb(skb);
		return NET_RX_DROP;
	}
	prdevs = get_remotedev_server(ntohs(pt->type));
	if (prdevs == NULL) {
		printk(KERN_ERR "%s: no entry for 0x%x\n", __func__,
		       ntohs(pt->type));
		kfree_skb(skb);
		return NET_RX_DROP;
	}
	rhdr = (struct remotedev_hdr *)skb->data;
	/*--- printk(KERN_DEBUG "%s:%x conn_index=%x seqnr=%d len=%d\n", __func__, rhdr->type, rhdr->conindex, ntohs(rhdr->seqnr), ntohs(rhdr->len)); ---*/

	switch (REMOTE_TYP(rhdr->type)) {
	case REMOTEDEV_TYPE_APPL:
		conn = get_connection(prdevs, rhdr->conindex, 1);
		if (conn == NULL) {
			ret = NET_RX_DROP;
			break;
		}
		if (ntohs(rhdr->seqnr) != conn->recv_seqnr) {
			printk(KERN_WARNING
			       "%s: recv: conn %d lost packet(s) seq-nr %d != %d(cdev) !\n",
			       __func__, conn->conindex, ntohs(rhdr->seqnr),
			       conn->recv_seqnr);
		}
		conn->recv_seqnr = ntohs(rhdr->seqnr) + 1;
		skb_get_timestamp(skb, &tv);
		if (tv.tv_sec == 0) {
			__net_timestamp(skb);
		}
		skb_queue_tail(&conn->recvqueue,
			       skb); /* add to receive queue */
		skb = NULL;
		if (likely((rhdr->type & REMOTEDEV_TYPE_END) ==
			   REMOTEDEV_TYPE_END)) {
			if (queue_work(prdevs->rcv_workqueue, &conn->rx_work) ==
			    0) {
				remotedev_conn_put(
					conn,
					"remotedev_netrecv() pending wkq");
			}
		}
		break;
	case REMOTEDEV_TYPE_APPL_DEMAND:
	case REMOTEDEV_TYPE_APPL_BYE:
		conn = get_connection(prdevs, rhdr->conindex,
				      (REMOTE_TYP(rhdr->type) ==
				       REMOTEDEV_TYPE_APPL_BYE) ?
					      0 :
					      1);
		if (conn == NULL) {
			printk(KERN_ERR
			       "%s:%x conn_index=%x seqnr=%d len=%d dropped\n",
			       __func__, rhdr->type, rhdr->conindex,
			       ntohs(rhdr->seqnr), ntohs(rhdr->len));
			ret = NET_RX_DROP;
			break;
		}
		skb_queue_tail(&conn->recvqueue,
			       skb); /* add to receive queue */
		skb = NULL;
		/*--- das close darf aber erst NACH den letzten Paketen erfolgen, deshalb hier auch in skb_receive_queue ---*/
		RDEV_LOCK_TRC(KERN_DEBUG "%s: %s received conindex=%d\n",
			      __func__,
			      rhdr->type == REMOTEDEV_TYPE_APPL_BYE ? "bye" :
								      "demand",
			      conn->conindex);
		if (queue_work(prdevs->rcv_workqueue, &conn->rx_work) == 0) {
			remotedev_conn_put(
				conn, "remotedev_netrecv() pending wkq(#1)");
		}
		break;
	case REMOTEDEV_TYPE_PING:
		DBG_INFO("%s: ping received\n", __func__);
		send_pong(prdevs);
		break;
	default:
		printk(KERN_ERR "%s: recv: type %x not implemented\n", __func__,
		       REMOTE_TYP(rhdr->type));
		break;
	}
	if (skb) {
		kfree_skb(skb);
	}
	remotedev_server_put(prdevs, "lremotedev_wrkq_rcv done");
	return ret;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static int remotedev_netdev_notifier_event(struct notifier_block *notifier,
					   unsigned long event, void *ptr)
{
	struct net_device *netdev = (struct net_device *)ptr;

	switch (event) {
	case NETDEV_UNREGISTER:
		if (netdev == remotedev_netdev) {
			printk(KERN_ERR "%s: device %s gone.\n", __func__,
			       netdev->name);
			remotedev_netdev = NULL;
		}
		break;

	case NETDEV_REGISTER:
		DBG_INFO("%s: device %s NETDEV_REGISTER\n", __func__,
			 netdev->name);
		remotedev_device();
		break;
	}
	return NOTIFY_DONE;
}
/*--------------------------------------------------------------------------------*\
 * initial auszufuehren um net_notifier zu installieren
\*--------------------------------------------------------------------------------*/
void remotedev_net_init(char *remote_device)
{
	DBG_INFO("%s(%s)\n", __func__, remote_device);
	down(&remotedev_server_sema);
	if (g_remote_device[0]) {
		printk(KERN_ERR "%s: double netdevice notifier -> ignore\n",
		       __func__);
		up(&remotedev_server_sema);
		return;
	}
	snprintf(g_remote_device, sizeof(g_remote_device), "%s", remote_device);
	register_netdevice_notifier(&remotedev_netdev_notifier);
	up(&remotedev_server_sema);
}
EXPORT_SYMBOL(remotedev_net_init);
/*--------------------------------------------------------------------------------*\
 * erst wenn alle server beendet kann netdev unregistriert werden !
\*--------------------------------------------------------------------------------*/
void remotedev_net_exit(void)
{
	DBG_INFO("%s %s\n", __func__, g_remote_device);
	down(&remotedev_server_sema);
	if (!list_empty(&remotedev_server_list)) {
		printk(KERN_ERR
		       "%s error: can't unregister because server exist\n",
		       __func__);
		up(&remotedev_server_sema);
		return;
	}
	if (g_remote_device[0]) {
		unregister_netdevice_notifier(&remotedev_netdev_notifier);
		remotedev_netdev = NULL;
		g_remote_device[0] = 0;
	}
	up(&remotedev_server_sema);
}
EXPORT_SYMBOL(remotedev_net_exit);
/*--------------------------------------------------------------------------------*\
 * senden von Daten - koennen auf Remote-Seite dann per read aus Device geholt werden
 * rhandle: Handle vom open
 * ret: < 0 Fehler bzw. gesendete Byte (immer komplett)
 *
 * Kontext: nur Kernelthread/Workqueue erlaubt
\*--------------------------------------------------------------------------------*/
int remotedev_write(void *rhandle, unsigned short h_proto, unsigned char *buf,
		    unsigned int len)
{
	struct sk_buff *skb;
	struct _remotedev_server *prdevs;
	struct _remotedev_server_conn *conn;
	struct net_device *netdev;
	int rc;
	unsigned char *data;
	unsigned int type = REMOTEDEV_TYPE_APPL | REMOTEDEV_TYPE_START;
	int ret = (int)len;

	DBG_TRC("%s(0x%x len=%u)\n", __func__, h_proto, len);
	netdev = remotedev_device();
	if (unlikely(netdev == NULL)) {
		printk(KERN_ERR "%s: netdev=NULL\n", __func__);
		return -EPIPE;
	}
	if (down_trylock(&remotedev_server_sema)) {
		/*--- nicht blockieren, falls aus rcv-workqueue aufgerufen und dabei remotedev_server_exit() erfolgt ---*/
		printk(KERN_ERR "%s: h_proto=%x down_trylock\n", __func__,
		       h_proto);
		return -EAGAIN;
	}
	prdevs = get_remotedev_server(h_proto);
	if (prdevs == NULL) {
		printk(KERN_ERR "%s: no entry for 0x%x\n", __func__, h_proto);
		up(&remotedev_server_sema);
		return -ENODEV;
	}
	conn = get_remotedev_conn_by_rhandle(prdevs, rhandle);
	if (conn == NULL) {
		printk(KERN_ERR "%s: no connection for 0x%p\n", __func__,
		       rhandle);
		remotedev_server_put(prdevs, "remotedev_write - no connection");
		up(&remotedev_server_sema);
		return -ENODEV;
	}
	while (len) {
		unsigned int tail = min(
			len, (REMOTEDEV_MAX_FRAME_SIZE - REMOTEDEV_HEADROOM));
		if (tail == len) {
			type |= REMOTEDEV_TYPE_END;
		}
		skb = lremotedev_allocskb(tail, type, prdevs->eth_p,
					  conn->conindex, conn->send_seqnr,
					  netdev, &data, GFP_KERNEL);
		if (unlikely(!skb)) {
			ret = -EPIPE;
			break;
		}
		memcpy(data, buf, tail);
		rc = dev_queue_xmit(skb); /* queue paket for transmitting */
		if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
			printk(KERN_ERR "%s: conindex=%d dev_queue_xmit()=%d\n",
			       __func__, conn->conindex, rc);
			ret = -EPIPE;
			break;
		}
		conn->send_seqnr++;
		len -= tail, buf += tail;
		type = REMOTEDEV_TYPE_APPL;
	}
	remotedev_conn_put(conn, "remotedev_write done");
	remotedev_server_put(prdevs, "remotedev_write done");
	up(&remotedev_server_sema);
	return ret;
}
EXPORT_SYMBOL(remotedev_write);
/*-------------------------------------------------------------------------------------*\
 * Remote-Device-Server mit ETH-Protocol h_proto erzeugen
 * wenn auf der Remote-Seite ein open gemacht wird -> callback remotedev_open() wird getriggert
							  h_proto: ETH-Protokoll
							  ret: NULL Verbindung ablehnen, sonst rhandle
 * nachfolgend kommen alle Daten des remotedev per remotedev_rcv() herein
 * wenn Remote-Seite Device schliesst so erfolgt remotedev_close()
 *
 * Der Kontext der Callbacks ist fein saeuberlich serialisert ein rx-worker-thread
\*-------------------------------------------------------------------------------------*/
int remotedev_server_create(
	unsigned short h_proto,
	/*--------------------------------------------------------------------------------*\
			Callback Remotseite macht open()
			conn:    Connection-Handle
			h_proto: ETH-Protokoll
			ret: NULL Verbindung ablehnen, sonst rhandle
			\*--------------------------------------------------------------------------------*/
	void *(*remotedev_open)(unsigned short h_proto),
	/*--------------------------------------------------------------------------------*\
			* Callback RemoteSeite macht write()
			* rhandle: Handle von remotedev_open()
			* buf, len: die Daten
			\*--------------------------------------------------------------------------------*/
	void (*remotedev_rcv)(void *rhandle, unsigned char *buf,
			      unsigned int len),
	/*--------------------------------------------------------------------------------*\
			* optional: Callback RemoteSeite macht read mit angeforderter Laenge
			* rhandle: Handle von remotedev_open()
			* len: die angeforderte Laenge
			\*--------------------------------------------------------------------------------*/
	void (*remotedev_demand)(void *rhandle, unsigned int len),
	/*--------------------------------------------------------------------------------*\
			* Callback RemoteSeite: macht close()
			* internal_error: 0 von Gegenseite, sonst internes Problem
			\*--------------------------------------------------------------------------------*/
	void (*remotedev_close)(void *rhandle, int internal_error))
{
	struct _remotedev_server *prdevs;
	DBG_INFO("%s(0x%x)\n", __func__, h_proto);
	if ((remotedev_open == NULL) || (remotedev_close == NULL) ||
	    (remotedev_rcv == NULL)) {
		return -EINVAL;
	}
	down(&remotedev_server_sema);
	if (g_remote_device[0] == 0) {
		printk(KERN_ERR
		       "%s: error: no netdevice notifier -> call remote_dev_net_init() first\n",
		       __func__);
		up(&remotedev_server_sema);
		return -ENODEV;
	}
	if ((prdevs = get_remotedev_server(h_proto))) {
		printk(KERN_ERR
		       "%s: error: double create with the h_proto=0x%x\n",
		       __func__, h_proto);
		remotedev_server_put(prdevs, "double remotedev_server_create");
		up(&remotedev_server_sema);
		return -EEXIST;
	}
	prdevs = kzalloc(sizeof(struct _remotedev_server), GFP_KERNEL);
	if (prdevs == NULL) {
		up(&remotedev_server_sema);
		return -ENOMEM;
	}
	atomic_set(&prdevs->link, 0);
	spin_lock_init(&prdevs->list_lock);
	INIT_LIST_HEAD(&prdevs->conn_list);
	prdevs->rcv_workqueue = create_workqueue("remotedev_rcv");
	if (prdevs->rcv_workqueue == NULL) {
		up(&remotedev_server_sema);
		kfree(prdevs);
		return -ENOMEM;
	}
	prdevs->remotedev_open   = remotedev_open;
	prdevs->remotedev_rcv    = remotedev_rcv;
	prdevs->remotedev_demand = remotedev_demand;
	prdevs->remotedev_close  = remotedev_close;

	prdevs->eth_p			  = h_proto;
	prdevs->rpacket_type.func = remotedev_netrecv;
	prdevs->rpacket_type.type = htons(h_proto);

	remotedev_server_get(prdevs, "remotedev_server_create");
	spin_lock_bh(&remotedev_server_list_lock);
	list_add(
		&prdevs->list,
		&remotedev_server_list); /*--- remote-server in globale Liste ---*/
	spin_unlock_bh(&remotedev_server_list_lock);

	dev_add_pack(&prdevs->rpacket_type);
	up(&remotedev_server_sema);
	return 0;
}
EXPORT_SYMBOL(remotedev_server_create);
/*-------------------------------------------------------------------------------------*\
 * Remote-Device-Server mit ETH-Protocol h_proto abraeumen
\*-------------------------------------------------------------------------------------*/
void remotedev_server_delete(unsigned short h_proto)
{
	struct net_device *netdev;
	struct _remotedev_server *prdevs;
	struct list_head *lp, *lp2;

	DBG_INFO("%s(0x%x)\n", __func__, h_proto);
	down(&remotedev_server_sema);
	prdevs = get_remotedev_server(h_proto);
	if (prdevs == NULL) {
		printk(KERN_ERR "%s: no entry for 0x%x\n", __func__, h_proto);
		up(&remotedev_server_sema);
		return;
	}
	dev_remove_pack(&prdevs->rpacket_type);
	destroy_workqueue(prdevs->rcv_workqueue);

	netdev = remotedev_device();
	/*--- 	erst nach abraeumen der workqueue und des netdevice aufraeumen: ---*/
	list_for_each_safe(lp, lp2, &prdevs->conn_list) {
		struct _remotedev_server_conn *conn =
			list_entry(lp, struct _remotedev_server_conn, list);
		struct sk_buff *bye_skb;

		DBG_INFO("%s: close(%p) by server_delete\n", __func__,
			 conn->rhandle);
		if (netdev) {
			bye_skb = lremotedev_allocskb(
				0, REMOTEDEV_TYPE_APPL_BYE, prdevs->eth_p,
				conn->conindex, conn->send_seqnr, netdev, NULL,
				GFP_USER);
			if (bye_skb) {
				dev_queue_xmit(
					bye_skb); /* queue paket for transmitting */
			}
		}
		remove_connection(prdevs, conn, -ENODEV);
	}
	spin_lock_bh(&remotedev_server_list_lock);
	list_del(&prdevs->list); /*--- remote-server aus globale Liste ---*/
	spin_unlock_bh(&remotedev_server_list_lock);

	remotedev_server_put(prdevs, __func__);
	up(&remotedev_server_sema);
}
EXPORT_SYMBOL(remotedev_server_delete);

#if defined(REMOTEDEV_SERVER_TEST)
static struct _priv_test {
	short used;
	unsigned short h_proto;
	unsigned int send_count;
	unsigned int rcv_count;
	unsigned int send_sum;
	unsigned int rcv_sum;
} test[2];

static char testbuf[3 * 1024];
static const char *teststring =
	"Dies ist ein Test ob angeforderte Daten richtig ruebergehen\n";

/*--------------------------------------------------------------------------------*\
 * Callback Remotseite macht open()
 * conn:    Connection-Handle
 * h_proto: ETH-Protokoll
 * ret: NULL Verbindung ablehnen, sonst rhandle
\*--------------------------------------------------------------------------------*/
static void *testremotedev_open(unsigned short h_proto)
{
	unsigned int i, ii, len = strlen(teststring);

	printk(KERN_INFO "%s: %x\n", __func__, h_proto);
	for (i = 0, ii = 0; i < sizeof(testbuf); i++, ii++) {
		if (ii >= len) {
			ii = 0;
		}
		testbuf[i] = teststring[ii];
	}
	for (i = 0; i < ARRAY_SIZE(test); i++) {
		if (test[i].used == 0) {
			memset(&test[i], 0, sizeof(test[i]));
			test[i].used = 1;
			test[i].h_proto = h_proto;
			printk(KERN_INFO "%s: %x -> h=%p\n", __func__, h_proto,
			       &test[i]);
			return &test[i];
		}
	}
	printk(KERN_INFO "%s: %x no further instance allowed\n", __func__,
	       h_proto);
	return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void testremotedev_write(struct _priv_test *ptest, unsigned char *buf,
				unsigned int len)
{
	int result;
	unsigned int i;

	/*--- 	printk(KERN_CONT"\n\n\n"); ---*/
	if ((result = remotedev_write(ptest, ptest->h_proto, buf, len)) < 0) {
		printk(KERN_ERR "%s: %p by write %d\n", __func__, ptest,
		       result);
		return;
	}
	ptest->send_count += len;
	for (i = 0; i < len; i++) {
		ptest->send_sum += buf[i];
		/*--- printk(KERN_CONT"%c", buf[i]); ---*/
	}
}
/*--------------------------------------------------------------------------------*\
* Callback RemoteSeite macht write()
* rhandle: Handle von remotedev_open()
* buf, len: die Daten
\*--------------------------------------------------------------------------------*/
static void testremotedev_rcv(void *rhandle, unsigned char *buf,
			      unsigned int len)
{
	struct _priv_test *ptest = (struct _priv_test *)rhandle;
	unsigned int i;
	printk(KERN_DEBUG "%s: %p echo len=%d\n", __func__, rhandle, len);

	ptest->rcv_count += len;
	for (i = 0; i < len; i++) {
		ptest->rcv_sum += buf[i];
		/*--- printk(KERN_CONT"%c", buf[i]); ---*/
	}
	/*--- 	printk(KERN_CONT"\n\n\n"); ---*/
	testremotedev_write(ptest, buf, len);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void testremotedev_demand(void *rhandle, unsigned int len)
{
	printk(KERN_DEBUG "%s: %p demand-len=%d\n", __func__, rhandle, len);
	testremotedev_write(rhandle, testbuf, min(sizeof(testbuf), len));
}
/*--------------------------------------------------------------------------------*\
* Callback RemoteSeite: macht close()
* internal_error: 0 von Gegenseite, sonst getriggert aus remotedev_server_delete()
\*--------------------------------------------------------------------------------*/
static void testremotedev_close(void *rhandle, int internal_error)
{
	unsigned int i;
	printk(KERN_INFO "%s: %p result=%d\n", __func__, rhandle,
	       internal_error);
	for (i = 0; i < ARRAY_SIZE(test); i++) {
		if (&test[i] == rhandle) {
			test[i].used = 0;
			printk(KERN_ERR
			       "%s: %p send: %d bytes check=%x received: %d bytes check=%x freed",
			       __func__, rhandle, test[i].send_count,
			       test[i].send_sum, test[i].rcv_count,
			       test[i].rcv_sum);
			return;
		}
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int remotedev_servertest(int mode, unsigned short h_proto, char *net)
{
	int result;
	switch (mode) {
	case 0:
		remotedev_net_init(net);
		break;
	case 1:
		if (h_proto) {
			result = remotedev_server_create(
				h_proto, testremotedev_open, testremotedev_rcv,
				testremotedev_demand, testremotedev_close);
			if (result) {
				printk(KERN_ERR
				       "%s: cant' create remotedev_server %d\n",
				       __func__, result);
			}
		}
		break;
	case 2:
		remotedev_server_delete(h_proto);
		break;
	case 3:
		remotedev_net_exit();
		break;
	default:
		break;
	}
	return 0;
}
#endif
