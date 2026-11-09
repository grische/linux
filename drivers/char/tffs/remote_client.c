// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
/*
 * tffs_remote.c
 *
 *  Created on: 16 Oct 2014
 *      Author: tklaassen
 */

#include <linux/kernel.h>
#include <avm/tffs/tffs.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <avm/event/event.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/list.h>
#include <linux/crc32.h>
#include "local.h"
#include "remote.h"

#if defined(CONFIG_MACH_PUMA6)
#include <arch-avalanche/generic/pal.h>
#endif

#if defined(TFFS_ARM_PUMA7)
#include <asm-arm/arch-avalanche/generic/pal.h>
#endif

#define MAX_MESSAGE_TRIES 3
#define MESSAGE_TIMEOUT	  (5 * HZ)
#define PANIC_BUFF_SIZE   (1 << CONFIG_LOG_BUF_SHIFT)

enum tffs_remote_state {
	tffs_remote_none,
	tffs_remote_setup,
	tffs_remote_wait_node,
	tffs_remote_wait_server,
	tffs_remote_running,
	tffs_remote_shutdown,
	tffs_remote_error,
};

struct tffs_server_conn {
	struct tffs_remote_ctx *ctx;
	enum _tffs_id id;
	struct kref refcnt;
	struct list_head conn_list;
	struct list_head msg_list;
	spinlock_t msg_lock;
	struct semaphore lock;
	uint64_t clnt_handle;
	uint64_t srv_handle;
	uint32_t seq_nr;
	uint32_t ack;
	uint32_t max_seg_size;
	enum avm_event_tffs_open_mode mode;
	struct tffs_core_handle *loc_handle;
	enum tffs_conn_state state;
	struct avm_event_tffs send_msg;
	unsigned long events;
	unsigned long timeout;
	unsigned int tries;
};

struct tffs_remote_ctx {
	unsigned int node_id;
	unsigned int in_panic_mode;
	struct tffs_server_conn panic_conn;
	wait_queue_head_t wait_queue;
	enum tffs_remote_state state;
	unsigned long events;
	uint32_t srv_id;
	uint64_t srv_handle;
	int64_t mem_offset;
	uint32_t max_seg_size;
	uint8_t *panic_buff;
	void *node_priv;
	spinlock_t conn_lock;
	struct list_head conn_list;
	atomic_t handle_seq;
	void *notify_priv;
	tffs3_notify_fn notify_cb;
};

#if 0
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>

static char hashbuf[64];
static int plaintext_to_sha1(uint8_t *plaintext, unsigned int plen, uint8_t *hash, unsigned int hlen)
{
  struct crypto_hash *tfm;
  struct hash_desc desc;
  struct scatterlist sg;
  unsigned int hashsize;

  tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);
  if (tfm == NULL) {
	pr_err("Failed to load transform for SHA1\n");
	return -EINVAL;
  }

  desc.tfm = tfm;
  desc.flags = 0;

  hashsize = crypto_hash_digestsize(tfm);
  if (hashsize > hlen){
	  pr_err("[%s] hashbuf too small\n", __func__);
	  return -EINVAL;
  }

  sg_init_one(&sg, plaintext, plen);
  crypto_hash_init(&desc);
  crypto_hash_update(&desc, &sg, plen);
  crypto_hash_final(&desc, hash);
  crypto_free_hash(tfm);

  return hashsize;
}

static void tffs_dump_block(unsigned char *p, unsigned int len) {
	int i, ii;
	printk("[dump] 0x%8p (%u bytes)\n", p, len);
	for(i = 0 ; i < len ; i += 16, p += 16) {
		printk("\t0x%8p: ", p);
		for(ii = 0 ; ii < 16 && (i + ii) < len ; ii++)
			printk("0x%02x ", p[ii]);
		for( ; ii < 16 ; ii++ )
			printk("     ");
		printk(" : ");
		for(ii = 0 ; ii < 16 && (i + ii) < len ; ii++)
			printk("%c ", p[ii] > ' ' ? p[ii] : '.');
		printk("\n");
	}
}

static void dump_buffer(char *text, unsigned int len, unsigned char *buffer) {
	int i;
#define dump_buffer_block_size 128
	for(i = 0 ; i < len ; i += dump_buffer_block_size) {
		printk("%s(%u bytes): 0x%x: % *B\n",
			text,
			len,
			len,
			len - i > dump_buffer_block_size ? dump_buffer_block_size : len - i,
			buffer + i);
	}
}
#endif /*--- #if defined(TFFS_DEBUG) ---*/

static void release_connection(struct kref *refcnt)
{
	struct tffs_server_conn *conn;
	struct tffs_remote_message *msg, *tmp;
	unsigned long flags;

	//    pr_err("[%s] Called\n", __func__);

	conn = container_of(refcnt, struct tffs_server_conn, refcnt);

	BUG_ON(!list_empty(&conn->conn_list));

	spin_lock_irqsave(&conn->msg_lock, flags);
	list_for_each_entry_safe(msg, tmp, &conn->msg_list, msg_list) {
		list_del(&msg->msg_list);
		kfree(msg);
	}
	spin_unlock_irqrestore(&conn->msg_lock, flags);

	kfree(conn);
}

static void add_connection(struct tffs_server_conn *conn)
{
	unsigned long flags;

	//    pr_err("[%s] Called\n", __func__);

	spin_lock_irqsave(&(conn->ctx->conn_lock), flags);
	list_add_tail(&conn->conn_list, &(conn->ctx->conn_list));
	spin_unlock_irqrestore(&(conn->ctx->conn_lock), flags);
}

static void del_connection(struct tffs_server_conn *conn)
{
	unsigned long flags;

	//    pr_err("[%s] Called\n", __func__);

	spin_lock_irqsave(&(conn->ctx->conn_lock), flags);
	list_del_init(&conn->conn_list);
	spin_unlock_irqrestore(&(conn->ctx->conn_lock), flags);
}

/*-----------------------------------------------------------------------------------------------*\
 * check that the connection handle given in clnt_handle points to a known connection struct.
 * This way we can avoid de-referencing connections that have been deleted already
\*-----------------------------------------------------------------------------------------------*/
static struct tffs_server_conn *get_connection(struct tffs_remote_ctx *ctx, uint64_t clnt_handle)
{
	struct tffs_server_conn *conn, *tmp_conn;
	unsigned long flags;

	//    pr_err("[%s] Called\n", __func__);
	//    pr_err("[%s] clnt_handle: 0x%llx\n", __func__, clnt_handle);

	spin_lock_irqsave(&ctx->conn_lock, flags);

	conn = NULL;
	list_for_each_entry(tmp_conn, &ctx->conn_list, conn_list) {
		//        pr_err("[%s] conn_handle: 0x%llx\n", __func__, conn->clnt_handle);
		if (tmp_conn->clnt_handle == clnt_handle) {
			conn = tmp_conn;
			kref_get(&conn->refcnt);
			break;
		}
	}
	spin_unlock_irqrestore(&ctx->conn_lock, flags);

	return conn;
}

static int put_connection(struct tffs_server_conn *conn)
{
	// pr_err("[%s] chain: %p\n", __func__, chain);
	return kref_put(&(conn->refcnt), release_connection);
}

/* handle_orphans(struct tffs_module *this)
 * Cleanly shut down connections that have been orphaned when the
 * open/close call has been interrupted by a signal to the owner
 * task                                                           */
static void handle_orphans(struct tffs_module *this)
{
	struct tffs_server_conn *conn, *tmp_conn;
	unsigned int work;
	int result;
	struct tffs_remote_ctx *ctx;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_message *rcv_msg;
	unsigned long flags;

	ctx = (struct tffs_remote_ctx *)this->priv;

	work = 5;
	do {
		--work;
		conn = NULL;

		/* find connections in interrupted state that have either received
		 * a message from the server or timed out waiting for a reply      */
		spin_lock_irqsave(&ctx->conn_lock, flags);
		list_for_each_entry(tmp_conn, &ctx->conn_list, conn_list) {
			if ((tmp_conn->state == tffs_conn_intr_open ||
			     tmp_conn->state == tffs_conn_intr_close) &&
			    (!list_empty(&(tmp_conn->msg_list)) ||
			     time_after_eq(jiffies, tmp_conn->timeout))) {
				conn = tmp_conn;
				break;
			}
		}
		spin_unlock_irqrestore(&ctx->conn_lock, flags);

		/* no connection found, abort */
		if (conn == NULL) {
			continue;
		}

		/* handle received message */
		rcv_msg = NULL;
		spin_lock_irqsave(&conn->msg_lock, flags);
		if (!list_empty(&conn->msg_list)) {
			rcv_msg = list_first_entry(&conn->msg_list, struct tffs_remote_message,
						   msg_list);
			list_del_init(&(rcv_msg->msg_list));
		}

		if (rcv_msg != NULL) {
			if (rcv_msg->msg.result != 0) {
				conn->state = tffs_conn_err;
			} else if (conn->state == tffs_conn_intr_open &&
				   rcv_msg->msg.type == avm_event_tffs_call_open) {
				/* open handshake for connection complete, now close it down */
				conn->state = tffs_conn_intr_close;
				conn->seq_nr++;
				conn->tries = 0;
				conn->timeout =
					jiffies; // make sure close message gets sent immediately
			} else if (conn->state == tffs_conn_intr_close &&
				   rcv_msg->msg.type == avm_event_tffs_call_close) {
				conn->state = tffs_conn_closed;
			}

			kfree(rcv_msg);
		}
		spin_unlock_irqrestore(&conn->msg_lock, flags);

		/* conn still in interrupted state, we have to (re-)send requests
		 * to the server */
		if (conn->state == tffs_conn_intr_open || conn->state == tffs_conn_intr_close) {
			if (conn->tries >= MAX_MESSAGE_TRIES) {
				conn->state = tffs_conn_err;
			} else {
				snd_msg = &conn->send_msg;
				memset(snd_msg, 0x0, sizeof(*snd_msg));
				snd_msg->clt_handle = conn->clnt_handle;
				snd_msg->seq_nr	    = conn->seq_nr;
				snd_msg->ack	    = conn->ack;
				snd_msg->dst_id	    = ctx->srv_id;
				snd_msg->src_id	    = ctx->node_id;
				snd_msg->result	    = 0;

				switch (conn->state) {
				case tffs_conn_intr_open:
					/* no reply to the initial open call received. Either our
					 * message to the server or the server's reply was lost.
					 * In both cases we need to re-send the request until
					 * the connection is established.
					 */
					snd_msg->type		= avm_event_tffs_call_open;
					snd_msg->call.open.id	= conn->id;
					snd_msg->call.open.mode = conn->mode;
					break;
				case tffs_conn_intr_close:
					/* (re-) send close request */
					snd_msg->type = avm_event_tffs_call_close;
					break;
				default:
					break;
				}

				conn->tries++;
				conn->timeout = jiffies + MESSAGE_TIMEOUT;

				result = avm_event_tffs_call(ctx->node_priv, snd_msg);
				if (result != 0) {
					conn->state = tffs_conn_err;
				}
			}
		}

		/* release connection if we are done with it */
		if (conn->state == tffs_conn_err || conn->state == tffs_conn_closed) {
			del_connection(conn);
			put_connection(conn);
		}

	} while (conn != NULL && work > 0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

enum flush_type {
	flush_read,
	flush_write,
	flush_read_write,
};

static void flush_cache(void *addr, size_t len, enum flush_type type)
{
	mb();
#if defined(CONFIG_X86)
	clflush_cache_range(addr, len);
#else /*--- #if IS_X86 ---*/
	switch (type) {
	case flush_read:
		PAL_sysCacheInvalidate(PAL_OSMEM_ADDR_DAT, addr, len);
		break;
	case flush_write:
		PAL_sysCacheFlush(PAL_OSMEM_ADDR_DAT, addr, len);
		break;
	case flush_read_write:
		PAL_sysCacheFlushAndInvalidate(PAL_OSMEM_ADDR_DAT, addr, len);
		break;
	}
#endif /*--- #else ---*/ /*--- #if IS_X86 ---*/
	mb();
}

static struct tffs_remote_message *wait_message(struct tffs_server_conn *conn,
						enum avm_event_tffs_call_type type)
{
	struct tffs_remote_message *rcv_msg;
	unsigned long flags, timeout;
	int result;

	//    pr_err("[%s] Called for type 0x%x\n", __func__, type);

	timeout = MESSAGE_TIMEOUT;
	do {
		result = wait_event_interruptible_timeout(
			conn->ctx->wait_queue,
			test_and_clear_bit(TFFS_EVENT_BIT_TRIGGER, &conn->events), timeout);

		if (result < 0) {
			pr_err("[%s] interrupted while waiting for message\n", __func__);
			rcv_msg = ERR_PTR(-ERESTARTSYS);
			break;
		}

		if (result == 0) {
			rcv_msg = ERR_PTR(-ETIMEDOUT);
			break;
		}

		timeout = result;
		rcv_msg = NULL;

		spin_lock_irqsave(&conn->msg_lock, flags);
		if (!list_empty(&conn->msg_list)) {
			rcv_msg = list_first_entry(&conn->msg_list, struct tffs_remote_message,
						   msg_list);
			list_del_init(&rcv_msg->msg_list);
		}
		spin_unlock_irqrestore(&conn->msg_lock, flags);

		if (rcv_msg != NULL && rcv_msg->msg.type != type) {
			pr_err("[%s] ignoring unexpected reply of type 0x%x\n", __func__,
			       rcv_msg->msg.type);
			kfree(rcv_msg);
			rcv_msg = NULL;
		}

	} while (rcv_msg == NULL && timeout > 0);

	if (rcv_msg == NULL) {
		rcv_msg = ERR_PTR(-ETIMEDOUT);
	}

	return rcv_msg;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void *TFFS3_REMOTE_Open(struct tffs_module *this, struct tffs_core_handle *handle)
{
	struct tffs_remote_ctx *ctx;
	struct tffs_server_conn *conn;
	enum avm_event_tffs_open_mode mode;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_message *rcv_msg;
	unsigned int tries;
	long result;

	//    pr_err("[%s] called\n", __func__);

	conn   = NULL;
	result = 0;
	ctx    = (struct tffs_remote_ctx *)this->priv;
	if (ctx == NULL) {
		pr_err("[TFFS3-REMOTE] TFFS device not initialised\n");
		result = -ENODEV;
		goto err_out;
	}

	switch (handle->mode) {
	case tffs3_mode_read:
		mode = avm_event_tffs_mode_read;
		break;
	case tffs3_mode_write:
		mode = avm_event_tffs_mode_write;
		break;
	case tffs3_mode_panic:
		mode = avm_event_tffs_mode_panic;
		break;
	default:
		pr_err("[%s] unknown open mode 0x%x\n", __func__, handle->mode);
		result = -EINVAL;
		goto err_out;
	}

	/* normal open handshake won't work during a kernel panic, so we just
	 * pretend it succeeded and send a special one-shot paniclog message
	 * later
	 */
	conn = NULL;
	if (mode == avm_event_tffs_mode_panic) {
		if (ctx->in_panic_mode == 0) {
			conn			 = &ctx->panic_conn;
			ctx->in_panic_mode	 = 1;
			handle->max_segment_size = ctx->max_seg_size;
		}
	} else {
		/* handle interrupted connections */
		handle_orphans(this);

		conn = kzalloc(sizeof(*conn), GFP_KERNEL);
	}

	if (conn == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	INIT_LIST_HEAD(&conn->conn_list);
	INIT_LIST_HEAD(&conn->msg_list);
	kref_init(&(conn->refcnt)); // sets refcnt to 1
	spin_lock_init(&conn->msg_lock);
	sema_init(&(conn->lock), 1);

	conn->ctx	  = ctx;
	conn->mode	  = mode;
	conn->id	  = handle->id;
	conn->clnt_handle = atomic_inc_return(&ctx->handle_seq);
	conn->state	  = tffs_conn_wait_open;
	conn->seq_nr	  = 1;

	if (mode == avm_event_tffs_mode_panic) {
		conn->state = tffs_conn_open;
	} else {
		conn->state = tffs_conn_wait_open;

		add_connection(conn);

		snd_msg = &conn->send_msg;

		tries = 0;
		do {
			memset(snd_msg, 0x0, sizeof(*snd_msg));
			snd_msg->type		= avm_event_tffs_call_open;
			snd_msg->call.open.id	= conn->id;
			snd_msg->call.open.mode = mode;
			snd_msg->clt_handle	= conn->clnt_handle;
			snd_msg->seq_nr		= conn->seq_nr;
			snd_msg->ack		= conn->ack;
			snd_msg->dst_id		= ctx->srv_id;
			snd_msg->src_id		= ctx->node_id;
			snd_msg->result		= 0;

			result = avm_event_tffs_call(ctx->node_priv, snd_msg);

			if (result != 0) {
				goto err_out;
			}

			rcv_msg = wait_message(conn, avm_event_tffs_call_open);

			if (IS_ERR(rcv_msg)) {
				result = PTR_ERR(rcv_msg);
				if (result == -ERESTARTSYS) {
					/* interrupted while waiting for reply. We can not just
					 * drop the connection because that would leave the server
					 * with a dangling client connection. Put the connection
					 * into a special interrupted state and use an async
					 * handlerclose it down once the reply arrives */
					conn->state   = tffs_conn_intr_open;
					conn->timeout = jiffies + MESSAGE_TIMEOUT;
					conn->tries   = 0;
					goto err_out;
				}
			} else {
				if (rcv_msg->msg.result == 0) {
					conn->state	 = tffs_conn_open;
					conn->srv_handle = rcv_msg->msg.srv_handle;
					conn->max_seg_size =
						rcv_msg->msg.call.open.max_segment_size;
				} else {
					conn->state = tffs_conn_err;
					result	    = rcv_msg->msg.result;
				}

				kfree(rcv_msg);
			}

			++tries;
		} while (conn->state == tffs_conn_wait_open && tries < MAX_MESSAGE_TRIES);

		if (conn->state != tffs_conn_open) {
			conn->state = tffs_conn_err;
			result	    = (result == 0) ? -ETIMEDOUT : result;
		}

		if (result != 0) {
			goto err_out;
		}

		handle->max_segment_size = conn->max_seg_size;
		conn->seq_nr++;
	}

	return conn;

err_out:
	if (conn != NULL && conn->state != tffs_conn_intr_open) {
		del_connection(conn);
		put_connection(conn);
	}

	return ERR_PTR(result);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Close(struct tffs_module *this, void *handle)
{
	struct tffs_remote_ctx *ctx;
	struct tffs_server_conn *conn;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_message *rcv_msg;
	unsigned int tries;
	long result;

	//    pr_err("[%s] called\n", __func__);

	ctx = (struct tffs_remote_ctx *)this->priv;
	if (ctx == NULL) {
		pr_err("[TFFS3-REMOTE] TFFS device not initialised\n");
		result = -ENODEV;
		goto err_out;
	}

	conn = (struct tffs_server_conn *)handle;

	// nothing to do for fake panic log connection
	if (conn == &ctx->panic_conn) {
		ctx->in_panic_mode = 0;
		result		   = 0;
		goto err_out;
	}

	/* handle orphaned connections */
	handle_orphans(this);

	snd_msg = &conn->send_msg;

	tries = 0;
	do {
		memset(snd_msg, 0x0, sizeof(*snd_msg));
		snd_msg->type	    = avm_event_tffs_call_close;
		snd_msg->clt_handle = conn->clnt_handle;
		snd_msg->srv_handle = conn->srv_handle;
		snd_msg->seq_nr	    = conn->seq_nr++;
		snd_msg->ack	    = conn->ack;
		snd_msg->dst_id	    = ctx->srv_id;
		snd_msg->src_id	    = ctx->node_id;

		result = avm_event_tffs_call(ctx->node_priv, snd_msg);

		if (result != 0) {
			goto err_out;
		}

		rcv_msg = wait_message(conn, avm_event_tffs_call_close);

		if (IS_ERR(rcv_msg)) {
			// interrupted while waiting for reply
			result = PTR_ERR(rcv_msg);
			if (result == -EINTR) {
				conn->state   = tffs_conn_intr_close;
				conn->timeout = jiffies + MESSAGE_TIMEOUT;
				conn->tries   = 0;
				goto err_out;
			}
		} else {
			result = rcv_msg->msg.result;
			if (result == 0 || (result == -EBADF && tries > 0)) {
				// server successfully closed connection or did not know this
				// connection when we re-sent the request.
				conn->state = tffs_conn_closed;
				result	    = 0;
			} else {
				conn->state = tffs_conn_err;
			}

			kfree(rcv_msg);
		}

		++tries;
	} while (conn->state == tffs_conn_wait_close && tries < MAX_MESSAGE_TRIES);

	if (conn->state == tffs_conn_closed) {
		del_connection(conn);
		put_connection(conn);
	}

err_out:
	return result;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int write_paniclog(struct tffs_remote_ctx *ctx, const uint8_t *data_buf, size_t data_len)
{
	struct tffs_server_conn *conn;
	struct avm_event_tffs *snd_msg;
	dma_addr_t dma_addr;
	int result;

	result	 = 0;
	dma_addr = 0;
	conn	 = &ctx->panic_conn;
	snd_msg	 = &conn->send_msg;

	if (ctx->panic_buff == NULL || data_len > PANIC_BUFF_SIZE) {
		pr_err("[%s] panic log exceeds size limit and cannot be written\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	memcpy(ctx->panic_buff, data_buf, data_len);

	flush_cache(ctx->panic_buff, data_len, flush_write);
	dma_addr = dma_map_single(NULL, ctx->panic_buff, data_len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(NULL, dma_addr))) {
		pr_err("[%s] mapping data buffer failed!\n", __func__);
		result = -EIO;
		goto err_out;
	}

	memset(snd_msg, 0x0, sizeof(*snd_msg));
	snd_msg->type			 = avm_event_tffs_call_paniclog;
	snd_msg->clt_handle		 = conn->clnt_handle;
	snd_msg->srv_handle		 = conn->srv_handle;
	snd_msg->seq_nr			 = conn->seq_nr;
	snd_msg->ack			 = conn->ack;
	snd_msg->dst_id			 = ctx->srv_id;
	snd_msg->src_id			 = ctx->node_id;
	snd_msg->call.paniclog.buff_addr = dma_addr;
	snd_msg->call.paniclog.len	 = data_len;

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);

	if (result == 0) {
		conn->seq_nr++;
	}

err_out:
	/* we do not know when the buffer will be read on the other side, so we can not
	 * unmap it. The system is going down anyway...
	 */

	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Write(struct tffs_module *this, void *handle, const uint8_t *data_buf,
			      size_t data_len, size_t *retlen, unsigned int final)
{
	struct tffs_remote_ctx *ctx;
	struct tffs_server_conn *conn;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_message *rcv_msg;
	dma_addr_t dma_addr;
	unsigned int tries;
	int result;
	__be32 crc;

	// pr_err("[%s] Called\n", __func__);

	result	 = 0;
	*retlen	 = 0;
	dma_addr = 0;
	final	 = (final != 0) ? 1 : 0;

	ctx = (struct tffs_remote_ctx *)this->priv;

	if (ctx == NULL) {
		pr_err("[%s] ctx == NULL\n", __func__);
		return -ENODEV;
	}

	if (retlen == NULL || (data_buf == NULL && data_len > 0)) {
		return -EINVAL;
	}

	conn = (struct tffs_server_conn *)handle;
	if (conn == &ctx->panic_conn) {
		pr_debug("[%s] in_panic_mode: 0x%x final: 0x%x data_len: 0x%x\n", __func__,
			 ctx->in_panic_mode, final, data_len);
		if (ctx->in_panic_mode != 0 && final != 0) {
			result	= write_paniclog(ctx, data_buf, data_len);
			*retlen = data_len;
		} else {
			result = -EBADF;
		}

		return result;
	}

	/* handle orphaned connections */
	handle_orphans(this);

	conn = get_connection(ctx, conn->clnt_handle);
	if (conn == NULL) {
		return -EBADF;
	}
	// need to get ref-counting right after this point

	down(&conn->lock);

	if (conn->state != tffs_conn_open) {
		result = -EBADF;
		goto err_out;
	}

	if (conn->mode != avm_event_tffs_mode_write) {
		result = -EINVAL;
		goto err_out;
	}

	snd_msg = &conn->send_msg;
	crc	= 0;
	if (data_buf != NULL && data_len > 0) {
		void *dma_buf;

		crc = crc32_be(0, data_buf, data_len);

		dma_buf = (void *)data_buf;
		flush_cache(dma_buf, data_len, flush_write);
		dma_addr = dma_map_single(NULL, dma_buf, data_len, DMA_TO_DEVICE);
		if (unlikely(dma_mapping_error(NULL, dma_addr))) {
			pr_err("[%s] mapping data buffer failed!\n", __func__);
			dma_addr = 0;
			result	 = -EIO;
			goto err_out;
		}
	}

	tries = 0;
	do {
		memset(snd_msg, 0x0, sizeof(*snd_msg));
		snd_msg->type		      = avm_event_tffs_call_write;
		snd_msg->clt_handle	      = conn->clnt_handle;
		snd_msg->srv_handle	      = conn->srv_handle;
		snd_msg->seq_nr		      = conn->seq_nr;
		snd_msg->ack		      = conn->ack;
		snd_msg->dst_id		      = ctx->srv_id;
		snd_msg->src_id		      = ctx->node_id;
		snd_msg->call.write.id	      = conn->id;
		snd_msg->call.write.buff_addr = dma_addr;
		snd_msg->call.write.len	      = data_len;
		snd_msg->call.write.final     = final;
		snd_msg->call.write.crc	      = crc;

		result = avm_event_tffs_call(ctx->node_priv, snd_msg);

		if (result != 0) {
			break;
		}

		rcv_msg = wait_message(conn, avm_event_tffs_call_write);

		if (IS_ERR(rcv_msg)) {
			result = PTR_ERR(rcv_msg);
		} else {
			*retlen = rcv_msg->msg.call.write.len;
			result	= rcv_msg->msg.result;
			tffs_write_statistic(rcv_msg->msg.call.write.id, *retlen, 1, 0);
			kfree(rcv_msg);
		}

		++tries;
	} while (result == -ETIMEDOUT && tries < MAX_MESSAGE_TRIES);

	if (result == 0) {
		conn->seq_nr++;
	}

err_out:
	if (dma_addr != 0) {
		dma_unmap_single(NULL, dma_addr, data_len, DMA_TO_DEVICE);
	}

	up(&conn->lock);
	put_connection(conn);

	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Read(struct tffs_module *this, void *handle, uint8_t *read_buffer,
			     size_t *read_length)
{
	struct tffs_remote_ctx *ctx;
	struct tffs_server_conn *conn;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_message *rcv_msg;
	dma_addr_t dma_addr;
	unsigned int tries;
	size_t buffer_len;
	int result;
	uint32_t crc;

	//    pr_err("[%s] Called\n", __func__);

	result	 = 0;
	crc	 = 0;
	dma_addr = 0;
	conn	 = NULL;

	ctx = (struct tffs_remote_ctx *)this->priv;

	if (ctx == NULL) {
		pr_err("[%s] ctx == NULL\n", __func__);
		result = -ENODEV;
		goto err_out;
	}

	/* handle cleanup of orphaned connections */
	handle_orphans(this);

	if (read_buffer == NULL || read_length == NULL) {
		result = -EINVAL;
		goto err_out;
	}

	/* shortcut if read length is zero */
	if (*read_length == 0) {
		result = 0;
		goto err_out;
	}

	conn = (struct tffs_server_conn *)handle;
	conn = get_connection(ctx, conn->clnt_handle);
	if (conn == NULL) {
		result = -EBADF;
		goto err_out;
	}

	// need to get ref-counting right after this point
	down(&conn->lock);

	if (conn->state != tffs_conn_open) {
		result = -EBADF;
		goto err_out;
	}

	if (conn->mode != avm_event_tffs_mode_read) {
		result = -EINVAL;
		goto err_out;
	}

	buffer_len = *read_length;
	flush_cache(read_buffer, buffer_len, flush_write);
	dma_addr = dma_map_single(NULL, read_buffer, buffer_len, DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(NULL, dma_addr))) {
		pr_err("[%s] mapping data buffer failed!\n", __func__);
		dma_addr = 0;
		result	 = -EIO;
		goto err_out;
	}

	snd_msg = &conn->send_msg;
	tries	= 0;
	do {
		memset(snd_msg, 0x0, sizeof(*snd_msg));
		snd_msg->type		     = avm_event_tffs_call_read;
		snd_msg->clt_handle	     = conn->clnt_handle;
		snd_msg->srv_handle	     = conn->srv_handle;
		snd_msg->seq_nr		     = conn->seq_nr;
		snd_msg->ack		     = conn->ack;
		snd_msg->dst_id		     = ctx->srv_id;
		snd_msg->src_id		     = ctx->node_id;
		snd_msg->call.read.id	     = conn->id;
		snd_msg->call.read.buff_addr = dma_addr;
		snd_msg->call.read.len	     = buffer_len;

		//        pr_err("[%s] request: seq_nr: 0x%x id: 0x%x buff_addr: 0x%llx len: 0x%llx\n",
		//                __func__, snd_msg->seq_nr, snd_msg->call.read.id, snd_msg->call.read.buff_addr, snd_msg->call.read.len);
		result = avm_event_tffs_call(ctx->node_priv, snd_msg);

		if (result != 0) {
			break;
		}

		rcv_msg = wait_message(conn, avm_event_tffs_call_read);

		if (IS_ERR(rcv_msg)) {
			result = PTR_ERR(rcv_msg);
		} else {
			*read_length = rcv_msg->msg.call.read.len;
			crc	     = rcv_msg->msg.call.read.crc;
			result	     = rcv_msg->msg.result;
			kfree(rcv_msg);
			//            pr_err("[%s] read reply: result: %d length: 0x%x\n", __func__, result, *read_length);
		}

		++tries;
	} while (result == -ETIMEDOUT && tries < MAX_MESSAGE_TRIES);

	//    pr_err("[%s] result: %d ID: 0x%0x len: 0x%x\n", __func__, result, Id, *read_length);

	if (result == 0) {
		tffs_write_statistic(conn->id, *read_length, 0, 0);
		conn->seq_nr++;
	}

err_out:
#if 0
	if(((Id >= 0x100 && Id <= 0x1ff) || Id ==  209)
			&& result == 0 && *read_length > 0)
	{
		int hashsize;
		pr_err("[%s] buffer: %p phys: 0x%x read_length: 0x%x\n", __func__, read_buffer, dma_addr, *read_length);
		hashsize = plaintext_to_sha1(read_buffer, *read_length, hashbuf, sizeof(hashbuf));
		print_hex_dump(KERN_ERR, "hash: ", DUMP_PREFIX_OFFSET, hashsize, 1, hashbuf, hashsize, 0);
		print_hex_dump(KERN_ERR, "dump: ", DUMP_PREFIX_OFFSET, 32, 1, read_buffer, *read_length, 0);
	}
#endif
	if (conn != NULL) {
		up(&conn->lock);
		put_connection(conn);
	}

	if (dma_addr != 0) {
		dma_unmap_single(NULL, dma_addr, buffer_len, DMA_FROM_DEVICE);
		flush_cache(read_buffer, buffer_len, flush_read);
	}

	/* do a final CRC check on the received data */
	if (result == 0) {
		if (crc != crc32_be(0, read_buffer, *read_length)) {
			pr_warn("[%s] CRC error.\n", __func__);
			result = -EIO;
		}
	}

	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Cleanup(struct tffs_module *this, void *handle)
{
	struct tffs_remote_ctx *ctx;
	struct tffs_server_conn *conn;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_message *rcv_msg;
	unsigned int tries;
	int result;

	//    pr_err("[%s] Called\n", __func__);

	result = 0;
	conn   = NULL;
	ctx    = (struct tffs_remote_ctx *)this->priv;

	if (ctx == NULL) {
		result = -ENODEV;
		goto err_out;
	}

	if (ctx->in_panic_mode) {
		result = -EBUSY;
		goto err_out;
	}

	/* handle orphaned connections */
	handle_orphans(this);

	conn = (struct tffs_server_conn *)handle;
	conn = get_connection(ctx, conn->clnt_handle);
	if (conn == NULL) {
		result = -EBADF;
		goto err_out;
	}

	if (conn->id != 0) {
		result = -EBADF;
		goto err_out;
	}

	snd_msg = &conn->send_msg;
	tries	= 0;
	do {
		memset(snd_msg, 0x0, sizeof(*snd_msg));
		snd_msg->type	    = avm_event_tffs_call_cleanup;
		snd_msg->clt_handle = conn->clnt_handle;
		snd_msg->srv_handle = conn->srv_handle;
		snd_msg->seq_nr	    = conn->seq_nr;
		snd_msg->ack	    = conn->ack;
		snd_msg->dst_id	    = ctx->srv_id;
		snd_msg->src_id	    = ctx->node_id;

		result = avm_event_tffs_call(ctx->node_priv, snd_msg);

		if (result != 0) {
			break;
		}

		rcv_msg = wait_message(conn, avm_event_tffs_call_cleanup);

		if (IS_ERR(rcv_msg)) {
			result = PTR_ERR(rcv_msg);
		} else {
			result = rcv_msg->msg.result;
			kfree(rcv_msg);
		}

		++tries;
	} while (result == -ETIMEDOUT && tries < MAX_MESSAGE_TRIES);

	if (result == 0) {
		conn->seq_nr++;
	}

err_out:
	if (conn != NULL) {
		put_connection(conn);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Reindex(struct tffs_module *this)
{
	int result = 0;

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Info(struct tffs_module *this, unsigned int *Fill)
{
	//    pr_err("[%s] Called\n", __func__);

	*Fill = 50;

	return 0;
}

static void event_established_cb(void *priv, unsigned int param1, unsigned int param2)
{
	struct tffs_remote_ctx *ctx;

	//    pr_err("[%s] Called\n", __func__);

	ctx = (struct tffs_remote_ctx *)priv;

	if (ctx->state == tffs_remote_setup || ctx->state == tffs_remote_wait_node) {
		ctx->state = tffs_remote_wait_server;
	}

	wake_up_interruptible_sync(&ctx->wait_queue);
}

static int TFFS3_REMOTE_callback(void *priv, struct avm_event_tffs *tffs_msg)
{
	struct tffs_remote_ctx *ctx;
	struct tffs_server_conn *conn;
	struct tffs_remote_message *msg;
	unsigned long flags;
	int result;

	BUG_ON(priv == NULL);
	ctx = (struct tffs_remote_ctx *)priv;

	//    pr_err("[%s] called with message type 0x%x\n", __func__, tffs_msg->type);

	result = 0;
	conn   = NULL;
	switch (tffs_msg->type) {
	case avm_event_tffs_call_init:
		if (tffs_msg->result == 0) {
			ctx->srv_id	  = tffs_msg->src_id;
			ctx->srv_handle	  = tffs_msg->srv_handle;
			ctx->mem_offset	  = tffs_msg->call.init.mem_offset;
			ctx->max_seg_size = tffs_msg->call.init.max_seg_size;
			ctx->state	  = tffs_remote_running;
			set_bit(TFFS_EVENT_BIT_TRIGGER, &ctx->events);
			wake_up_interruptible_sync(&ctx->wait_queue);
		}
		break;
	case avm_event_tffs_call_notify:
		if (ctx->srv_id == tffs_msg->src_id && ctx->state == tffs_remote_running) {
			if (ctx->notify_cb) {
				ctx->notify_cb(ctx->notify_priv, tffs_msg->call.notify.id,
					       tffs_msg->call.notify.event);
			}
		}
		break;
	default:
		conn = get_connection(ctx, tffs_msg->clt_handle);
		if (conn == NULL) {
			result = -EBADF;
			goto err_out;
		}

		//      pr_err("[%s] conn: 0x%08x sizeof(*msg): 0x%x\n", __func__, conn, sizeof(*msg));
		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		if (msg == NULL) {
			result = -ENOMEM;
			goto err_out;
		}

		//      pr_err("[%s] msg: 0x%08x \n", __func__, msg);

		INIT_LIST_HEAD(&msg->msg_list);
		memcpy(&msg->msg, tffs_msg, sizeof(msg->msg));
		spin_lock_irqsave(&conn->msg_lock, flags);
		list_add_tail(&msg->msg_list, &conn->msg_list);
		spin_unlock_irqrestore(&conn->msg_lock, flags);

		//      pr_err("[%s] waking connection\n", __func__);
		set_bit(TFFS_EVENT_BIT_TRIGGER, &conn->events);

		wake_up_interruptible_sync(&ctx->wait_queue);
		break;
	}

err_out:
	//    pr_err("[%s] err_out result: %d\n", __func__, result);
	if (conn != NULL) {
		put_connection(conn);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Register_Notify(struct tffs_module *this, void *notify_priv,
					tffs3_notify_fn notify_cb)
{
	struct tffs_remote_ctx *ctx;
	int result;

	ctx = (struct tffs_remote_ctx *)this->priv;

	result = 0;
	if (ctx->notify_cb == NULL) {
		ctx->notify_priv = notify_priv;
		ctx->notify_cb	 = notify_cb;
	} else {
		result = -EEXIST;
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Remove_Notify(struct tffs_module *this, void *notify_priv,
				      tffs3_notify_fn notify_cb)
{
	struct tffs_remote_ctx *ctx;
	int result;

	ctx = (struct tffs_remote_ctx *)this->priv;

	result = -EINVAL;
	if (ctx->notify_priv == notify_priv && ctx->notify_cb == notify_cb) {
		ctx->notify_cb	 = NULL;
		ctx->notify_priv = NULL;
		result		 = 0;
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_REMOTE_Setup(struct tffs_module *this)
{
	struct tffs_remote_ctx *ctx;
	unsigned int fill;
	int result;
	struct avm_event_tffs disc_msg;

	pr_info("[TFFS3-REMOTE] Remote storage backend for TFFS 3.x\n");

	result = -EINVAL;

	ctx = (struct tffs_remote_ctx *)this->priv;
	BUG_ON(ctx == NULL);

	ctx->state = tffs_remote_wait_node;
	result	   = avm_event_node_established(event_established_cb, ctx, 0, 0);
	if (result != 0) {
		pr_err("[%s] installing event node callback failed\n", __func__);
		goto err_out;
	}

	do {
		pr_err("[%s] waiting for remote node connection to be established\n", __func__);
		if (wait_event_interruptible(ctx->wait_queue,
					     ctx->state != tffs_remote_wait_node)) {
			pr_err("[%s] interrupted while waiting for remote node, exiting\n",
			       __func__);
			result = -EINTR;
			goto err_out;
		}
	} while (ctx->state == tffs_remote_wait_node);

	pr_err("[%s] registering TFFS3_REMOTE_callback\n", __func__);
	ctx->node_priv = avm_event_register_tffs(ctx->node_id, TFFS3_REMOTE_callback, ctx);
	if (IS_ERR_OR_NULL(ctx->node_priv)) {
		result = (ctx->node_priv == NULL) ? -ENOMEM : PTR_ERR(ctx->node_priv);
		pr_err("[%s] registering callback failed with return code %d\n", __func__, result);
		goto err_out;
	}

	memset(&disc_msg, 0x0, sizeof(disc_msg));
	disc_msg.src_id		      = ctx->node_id;
	disc_msg.dst_id		      = AVM_EVENT_TFFS_NODE_ANY;
	disc_msg.type		      = avm_event_tffs_call_init;
	disc_msg.call.init.mem_offset = 42;
	disc_msg.clt_handle	      = (uintptr_t)ctx;
	do {
		pr_err("[%s] waiting for remote server connection to be established\n", __func__);
		result = avm_event_tffs_call(ctx->node_priv, &disc_msg);
		if (result != 0) {
			break;
		}

		result = wait_event_interruptible_timeout(
			ctx->wait_queue, ctx->state != tffs_remote_wait_server, 5 * HZ);
		if (result < 0) {
			pr_err("[%s] interrupted while waiting for remote server, exiting\n",
			       __func__);
			result = -EINTR;
			break;
		}

		result = 0;
	} while (ctx->state == tffs_remote_wait_server);

	if (result == 0) {
		ctx->panic_buff = kmalloc(PANIC_BUFF_SIZE, GFP_KERNEL);
		if (ctx->panic_buff == NULL) {
			pr_err("[%s] Unable to allocate panic log buffer\n", __func__);
			result = -ENOMEM;
			goto err_out;
		}

		TFFS3_REMOTE_Info(this, &fill);
		pr_info("[TFFS3-REMOTE] Initialisation successful, fill rate %d%%\n", fill);
	}

err_out:
	if (result != 0) {
		pr_info("[TFFS3-REMOTE] Initialisation failed!\n");

		if (ctx != NULL) {
			if (ctx->panic_buff != NULL) {
				kfree(ctx->panic_buff);
			}
			this->priv = NULL;
			kfree(ctx);
		}
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_REMOTE_Configure(struct tffs_module *this, unsigned int node_id)
{
	struct tffs_remote_ctx *ctx;
	int result;

	// pr_err("[%s] Called\n", __func__);

	result = -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		pr_err("[TFFS3-REMOTE] Out of memory during configuration\n");
		result = -ENOMEM;
		goto err_out;
	}

	this->priv = ctx;
	INIT_LIST_HEAD(&ctx->conn_list);
	init_waitqueue_head(&(ctx->wait_queue));
	spin_lock_init(&ctx->conn_lock);
	atomic_set(&ctx->handle_seq, 1);
	ctx->node_id = node_id;

	this->name	      = "remote";
	this->setup	      = TFFS3_REMOTE_Setup;
	this->open	      = TFFS3_REMOTE_Open;
	this->close	      = TFFS3_REMOTE_Close;
	this->read	      = TFFS3_REMOTE_Read;
	this->write	      = TFFS3_REMOTE_Write;
	this->cleanup	      = TFFS3_REMOTE_Cleanup;
	this->reindex	      = TFFS3_REMOTE_Reindex;
	this->info	      = TFFS3_REMOTE_Info;
	this->register_notify = TFFS3_REMOTE_Register_Notify;
	this->remove_notify   = TFFS3_REMOTE_Remove_Notify;

	result = 0;

err_out:
	return result;
}
EXPORT_SYMBOL(TFFS3_REMOTE_Configure);
