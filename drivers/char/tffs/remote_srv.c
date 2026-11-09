// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
/*
 * tffs_remote_srv.c
 *
 *  Created on: 16 Oct 2014
 *      Author: tklaassen
 */

#include <linux/kernel.h>
#include <avm/tffs/tffs.h>

#define MAX_SUBPAGE_NUM 1

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <avm/event/event.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/reboot.h>

#include <avm/enh/reboot_status.h>
#include "local.h"
#include "remote.h"

#if defined(CONFIG_MACH_PUMA6)
#include <arch-avalanche/generic/pal.h>
#endif

#if defined(TFFS_ARM_PUMA7)
#include <asm-arm/arch-avalanche/generic/pal.h>
#endif

#if defined(CONFIG_X86)
#define REMOTE_MEM_OFFSET ARM_MEM_OFFSET
#else
#define REMOTE_MEM_OFFSET (-ARM_MEM_OFFSET)
#endif

#define CONN_TIMEOUT (5 * 60 * HZ) // 5 minutes
#define CLOSE_WAIT   (30 * HZ) // 30 seconds

#ifdef DEBUG
static int remote_paniclog_wait_secs;
module_param(remote_paniclog_wait_secs, int, 0600);
#else
#define remote_paniclog_wait_secs 0
#endif

enum tffs_srv_state {
	tffs_srv_none,
	tffs_srv_setup,
	tffs_srv_wait_node,
	tffs_srv_running,
	tffs_srv_shutdown,
	tffs_srv_error,
};

struct tffs_srv_ctx {
	unsigned int node_id;
	struct semaphore lock;
	unsigned int in_panic_mode;
	wait_queue_head_t wait_queue;
	enum tffs_srv_state state;
	void *node_priv;
	unsigned long events;
	spinlock_t req_lock;
	struct task_struct *kthread;
	struct list_head req_list;
	struct list_head clnt_list;
	struct avm_event_tffs send_msg;
	atomic_t handle_seq;
};

struct tffs_remote_clnt {
	struct list_head clnt_list;
	struct list_head conn_list;
	uint32_t clnt_id;
	uint64_t clnt_handle;
	int64_t mem_offset;
	struct avm_event_tffs send_msg;
};

struct tffs_client_conn {
	enum _tffs_id id;
	struct kref refcnt;
	struct list_head conn_list;
	struct list_head msg_list;
	spinlock_t msg_lock;
	struct semaphore lock;
	struct tffs_remote_clnt *clnt;
	uint64_t clnt_handle;
	uint64_t srv_handle;
	uint32_t seq_nr;
	uint32_t ack;
	unsigned long timeout;
	enum tffs_conn_state state;
	enum avm_event_tffs_open_mode mode;
	struct tffs_core_handle *loc_handle;
	struct avm_event_tffs send_msg;
	unsigned long events;
	unsigned char *last_read;
	size_t last_read_size;
	size_t last_read_len;
	size_t last_write_len;
	size_t last_write_result;
};

struct node_id_table {
	uint32_t node_id;
	enum _tffs_id paniclog_id;
};

struct node_id_table log_id_map[] = {
		{ AVM_EVENT_TFFS_NODE_PRIMARY,   FLASH_FS_ID_PANIC_LOG, },
		{ AVM_EVENT_TFFS_NODE_SECONDARY, FLASH_FS_ID_PANIC2_LOG, },
};

#define NODE_PERM_READ  (1 << 0)
#define NODE_PERM_WRITE (1 << 1)

struct node_permissions {
	uint32_t node_id;
	enum _tffs_id id_start;
	enum _tffs_id id_end;
	uint32_t permission;
};

/* set up access permissions for remote nodes */
struct node_permissions node_perm_tbl[] = {
	/* read/write on special ID 0, only valid for housekeeping tasks */
	{AVM_EVENT_TFFS_NODE_ATOM, 0, 0, NODE_PERM_READ | NODE_PERM_WRITE },
	{AVM_EVENT_TFFS_NODE_ARM,  0, 0, NODE_PERM_READ | NODE_PERM_WRITE },

	/* read/write on bootloader environment variables */
	{AVM_EVENT_TFFS_NODE_ATOM, FLASH_FS_ID_FIRMWARE_CONFIG_LAST, FLASH_FS_URLADER_VERSION, NODE_PERM_READ | NODE_PERM_WRITE },
	{AVM_EVENT_TFFS_NODE_ARM,  FLASH_FS_ID_FIRMWARE_CONFIG_LAST, FLASH_FS_URLADER_VERSION, NODE_PERM_READ | NODE_PERM_WRITE },

	/* read/write on node's crash and panic logs */
	{AVM_EVENT_TFFS_NODE_SECONDARY, FLASH_FS_ID_CRASH2_LOG, FLASH_FS_ID_PANIC2_LOG, NODE_PERM_READ | NODE_PERM_WRITE },
	{AVM_EVENT_TFFS_NODE_SECONDARY, FLASH_FS_ID_USER2_LOG, FLASH_FS_ID_USER2_LOG, NODE_PERM_READ | NODE_PERM_WRITE },
	{AVM_EVENT_TFFS_NODE_PRIMARY,  FLASH_FS_ID_CRASH_LOG,  FLASH_FS_ID_PANIC_LOG,  NODE_PERM_READ | NODE_PERM_WRITE },
	{AVM_EVENT_TFFS_NODE_PRIMARY, FLASH_FS_ID_USER_LOG, FLASH_FS_ID_USER_LOG, NODE_PERM_READ | NODE_PERM_WRITE },

#if defined(CONFIG_TFFS_DEV_SERVER_RESTRICTED)
	/* read-only on everything else */
	{AVM_EVENT_TFFS_NODE_ATOM, FLASH_FS_ID_SEGMENT + 1, FLASH_FS_ID_LAST, NODE_PERM_READ },
	{AVM_EVENT_TFFS_NODE_ARM,  FLASH_FS_ID_SEGMENT + 1, FLASH_FS_ID_LAST, NODE_PERM_READ },
#else
	/* read/write everything, except segment headers */
	{AVM_EVENT_TFFS_NODE_ATOM, FLASH_FS_ID_SEGMENT + 1, FLASH_FS_ID_LAST, NODE_PERM_READ | NODE_PERM_WRITE },
	{AVM_EVENT_TFFS_NODE_ARM,  FLASH_FS_ID_SEGMENT + 1, FLASH_FS_ID_LAST, NODE_PERM_READ | NODE_PERM_WRITE },
#endif
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
	pr_debug("[dump] 0x%8p (%u bytes)\n", p, len);
	for(i = 0 ; i < len ; i += 16, p += 16) {
		pr_debug("\t0x%8p: ", p);
		for(ii = 0 ; ii < 16 && (i + ii) < len ; ii++)
			pr_cont("0x%02x ", p[ii]);
		for( ; ii < 16 ; ii++ )
			pr_cont("     ");
		pr_cont(" : ");
		for(ii = 0 ; ii < 16 && (i + ii) < len ; ii++)
			pr_cont("%c ", p[ii] > ' ' ? p[ii] : '.');
		pr_cont("\n");
	}
}

static void dump_buffer(char *text, unsigned int len, unsigned char *buffer) {
	int i;
#define dump_buffer_block_size 128
	for(i = 0 ; i < len ; i += dump_buffer_block_size) {
		pr_debug("%s(%u bytes): 0x%x: % *B\n",
			text,
			len,
			len,
			len - i > dump_buffer_block_size ? dump_buffer_block_size : len - i,
			buffer + i);
	}
}
#endif

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
		consistent_sync(addr, len, DMA_FROM_DEVICE);
		break;
	case flush_write:
		consistent_sync(addr, len, DMA_TO_DEVICE);
		break;
	case flush_read_write:
		consistent_sync(addr, len, DMA_BIDIRECTIONAL);
		break;
	}
#endif /*--- #else ---*/ /*--- #if IS_X86 ---*/
	mb();
}

static void event_established_callback(void *priv, unsigned int param1, unsigned int param2)
{
	struct tffs_srv_ctx *ctx;

	//    pr_err("[%s] Called\n", __func__);

	ctx = (struct tffs_srv_ctx *)priv;

	if (ctx->state == tffs_srv_setup || ctx->state == tffs_srv_wait_node) {
		ctx->state = tffs_srv_running;
	}

	wake_up_interruptible_sync(&ctx->wait_queue);
}

static int add_client(struct tffs_srv_ctx *ctx, struct tffs_remote_clnt *clnt)
{
	int result;

	BUG_ON(ctx == NULL || clnt == NULL);

	//    pr_err("[%s] Called for client id 0x%x\n", __func__, clnt->clnt_id);

	result = 0;
	list_add_tail(&clnt->clnt_list, &ctx->clnt_list);

	return result;
}

static struct tffs_remote_clnt *get_client(struct tffs_srv_ctx *ctx, uint32_t clnt_id)
{
	struct tffs_remote_clnt *clnt, *tmp_clnt;

	//    pr_err("[%s] Called\n", __func__);

	clnt = NULL;
	list_for_each_entry(tmp_clnt, &ctx->clnt_list, clnt_list) {
		if (tmp_clnt->clnt_id == clnt_id) {
			clnt = tmp_clnt;
			break;
		}
	}

	return clnt;
}

static struct tffs_client_conn *get_connection(struct tffs_srv_ctx *ctx, uint32_t clnt_id,
					       uint64_t clnt_handle)
{
	struct tffs_remote_clnt *clnt;
	struct tffs_client_conn *conn, *tmp_conn;

	//    pr_err("[%s] Called\n", __func__);
	//    pr_err("[%s] client_id: 0x%x, clnt_handle: 0x%llx\n", __func__, clnt_id, clnt_handle);

	clnt = get_client(ctx, clnt_id);
	if (clnt == NULL) {
		return NULL;
	}

	//    pr_err("[%s] clnt: %p\n", __func__, clnt);

	conn = NULL;
	list_for_each_entry(tmp_conn, &clnt->conn_list, conn_list) {
		//        pr_err("[%s] conn->clnt_handle: 0x%llx\n", __func__, conn->clnt_handle);
		if (tmp_conn->clnt_handle == clnt_handle) {
			conn = tmp_conn;
			break;
		}
	}

	//    pr_err("[%s] conn: %p\n", __func__, conn);

	return conn;
}

static void free_conn(struct tffs_srv_ctx *ctx, struct tffs_client_conn *conn)
{
	//    pr_err("[%s] Called\n", __func__);

	// FIXME: implement delayed freeing so re-sending the request won't result
	// in an error
	list_del_init(&conn->conn_list);
	if (conn->last_read != NULL) {
		kfree(conn->last_read);
	}
	kfree(conn);
}

static int check_permission(uint32_t node_id, enum _tffs_id entry_id, enum tffs3_handle_mode mode)
{
	int result;
	uint32_t perm;
	unsigned int i;

	/* default is deny */
	result = -EPERM;

	switch (mode) {
	case tffs3_mode_read:
		perm = NODE_PERM_READ;
		break;
	case tffs3_mode_write:
		perm = NODE_PERM_WRITE;
		break;
	case tffs3_mode_panic:
		perm = NODE_PERM_WRITE;
		break;
	default:
		pr_err("[%s]: invalid mode: 0x%x\n", __func__, mode);
		goto err_out;
		break;
	}

	/* Search for matching matching rule in table.
	 * Search is terminated after first match.
	 */
	for (i = 0; i < ARRAY_SIZE(node_perm_tbl); ++i) {
		if (node_perm_tbl[i].node_id == node_id && node_perm_tbl[i].id_start <= entry_id &&
		    node_perm_tbl[i].id_end >= entry_id &&
		    (node_perm_tbl[i].permission & perm) == perm) {
			result = 0;
			break;
		}
	}

err_out:
	return result;
}

static void handle_open(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	struct tffs_remote_clnt *clnt;
	struct tffs_client_conn *conn;
	enum tffs3_handle_mode mode;
	struct tffs_core_handle *loc_handle;
	struct avm_event_tffs *snd_msg;
	unsigned int max_seg_size;
	uint64_t srv_handle;
	int result;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_open);

	//    pr_err("[%s] request: src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx\n",
	//            __func__, msg->src_id, msg->dst_id, msg->clt_handle, msg->srv_handle);

	conn	     = NULL;
	result	     = 0;
	max_seg_size = 0;
	srv_handle   = 0;
	snd_msg	     = &ctx->send_msg;

	clnt = get_client(ctx, msg->src_id);
	if (clnt == NULL) {
		pr_err("[%s] received open call from unknown client id 0x%x\n", __func__,
		       msg->src_id);
		result = -ENODEV;
		goto send_err;
	}

	switch (msg->call.open.mode) {
	case avm_event_tffs_mode_read:
		mode = tffs3_mode_read;
		break;
	case avm_event_tffs_mode_write:
		mode = tffs3_mode_write;
		break;
	case avm_event_tffs_mode_panic:
		mode = tffs3_mode_panic;
		// FIXME: use special one-shot panic message instead
		break;
	default:
		pr_err("[%s] client 0x%x called open with invalid mode 0x%x\n", __func__,
		       msg->src_id, msg->call.open.mode);
		result = -EINVAL;
		goto send_err;
	}

	result = check_permission(msg->src_id, msg->call.open.id, mode);
	if (result != 0) {
		goto send_err;
	}

	// if our reply to an open request gets lost, client will resend it. Make sure we
	// don't generate duplicate connection entries
	conn = get_connection(ctx, clnt->clnt_id, msg->clt_handle);
	if (conn != NULL) {
		if (conn->state == tffs_conn_closed) {
			// client is re-using handle of recently closed connection
			free_conn(ctx, conn);
			conn = NULL;
		} else if (conn->mode != msg->call.open.mode) {
			// client sent open request with known handle but different mode?!
			result = -EEXIST;
			goto send_err;
		}
	}

	// create new connection
	if (conn == NULL) {
		conn = kzalloc(sizeof(*conn), GFP_KERNEL);
		if (conn == NULL) {
			pr_err("[%s] Out of memory during open call for client 0x%x\n", __func__,
			       msg->src_id);
			result = -ENOMEM;
			goto send_err;
		}

		INIT_LIST_HEAD(&conn->conn_list);
		INIT_LIST_HEAD(&conn->msg_list);
		kref_init(&(conn->refcnt)); // sets refcnt to 1
		spin_lock_init(&conn->msg_lock);
		sema_init(&(conn->lock), 1);
		conn->clnt	  = clnt;
		conn->clnt_handle = msg->clt_handle;
		conn->seq_nr	  = msg->seq_nr;
		conn->id	  = msg->call.open.id;
		conn->mode	  = msg->call.open.mode;
		conn->timeout	  = jiffies + CONN_TIMEOUT;
		conn->state	  = tffs_conn_open;
		conn->srv_handle  = atomic_inc_return(&ctx->handle_seq);

		loc_handle	 = TFFS3_Open(conn->id, mode);
		conn->loc_handle = loc_handle;

		if (!IS_ERR_OR_NULL(loc_handle)) {
			list_add_tail(&conn->conn_list, &clnt->conn_list);
			max_seg_size = loc_handle->max_segment_size;
		} else {
			pr_err("[%s] TFFS3_Open failed for client 0x%x\n", __func__, msg->src_id);
			kfree(conn);
			result = -EIO;
			goto send_err;
		}
	}

	srv_handle = conn->srv_handle;

	// send reply message
send_err:
	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg			    = *msg;
	snd_msg->src_id			    = ctx->node_id;
	snd_msg->dst_id			    = msg->src_id;
	snd_msg->result			    = result;
	snd_msg->srv_handle		    = srv_handle;
	snd_msg->call.open.max_segment_size = max_seg_size;
	result				    = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. client will retry...
		pr_err("[%s] Error sending open-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

static void handle_close(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	int result;
	struct tffs_client_conn *conn;
	struct avm_event_tffs *snd_msg;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_close);

	//    pr_err("[%s] request: src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx\n",
	//            __func__, msg->src_id, msg->dst_id, msg->clt_handle, msg->srv_handle);

	result	= 0;
	snd_msg = &ctx->send_msg;

	conn = get_connection(ctx, msg->src_id, msg->clt_handle);
	if (conn != NULL) {
		if (conn->state != tffs_conn_closed) {
			result = TFFS3_Close(conn->loc_handle);
			if (result == 0) {
				conn->state   = tffs_conn_closed;
				conn->timeout = jiffies + CLOSE_WAIT;
				free_conn(ctx, conn);
			} else {
				pr_err("[%s] TFFS3_Close failed for client 0x%x\n", __func__,
				       msg->src_id);
				// FIXME: how do we handle this? Should the client retry or do we just
				// drop the connection and let the TFFS hanging with its broken handle?
				// Can we act on different return codes?
			}
		}
	} else {
		result = -EBADF;
	}

	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg	= *msg;
	snd_msg->src_id = ctx->node_id;
	snd_msg->dst_id = msg->src_id;
	snd_msg->result = result;

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending open-reply to unknown client 0x%x\n", __func__,
		       msg->src_id);
	}

	return;
}

static void handle_read(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	struct resource *res;
	int result;
	struct tffs_client_conn *conn;
	struct avm_event_tffs *snd_msg;
	uintptr_t io_addr;
	unsigned char *buff;
	size_t read_len;
	uint32_t crc;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_read);

	//    pr_err("[%s] request: src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx\n",
	//            __func__, msg->src_id, msg->dst_id, msg->clt_handle, msg->srv_handle);

	//    pr_err("[%s] request: seq_nr: 0x%x id: 0x%x buff_addr: 0x%llx len: 0x%llx\n",
	//            __func__, msg->seq_nr, msg->call.read.id, msg->call.read.buff_addr, msg->call.read.len);

	snd_msg	 = &ctx->send_msg;
	io_addr	 = (uintptr_t)msg->call.read.buff_addr;
	read_len = msg->call.read.len;
	crc	 = 0;
	res	 = NULL;

	conn = get_connection(ctx, msg->src_id, msg->clt_handle);
	if (conn == NULL) {
		result = -EBADF;
		goto send_err;
	}

	if (conn->state != tffs_conn_open) {
		result = -EIO;
		goto send_err;
	}

	if (msg->seq_nr < conn->seq_nr) {
		pr_err("[%s] at seq-nr. 0x%x, ignoring old request with seq-nr 0x%x\n", __func__,
		       conn->seq_nr, msg->seq_nr);
		return;
	}

	if (msg->seq_nr > (conn->seq_nr + 1)) {
		pr_err("[%s] out of order request received. local seq-nr: 0x%x, request seq-nr: 0x%x\n",
		       __func__, conn->seq_nr, msg->seq_nr);
		result	    = -EIO;
		conn->state = tffs_conn_err;
		goto send_err;
	}

	if (msg->seq_nr == conn->seq_nr) {
		pr_err("[%s] re-request of last read with seq-nr 0x%x\n", __func__, msg->seq_nr);

		if (conn->last_read == NULL || conn->last_read_size != read_len) {
			pr_err("[%s] read request with same seq-nr 0x%x but different size!\n",
			       __func__, msg->seq_nr);
			result = -EIO;
			goto send_err;
		}

		// we can use the buffered result
		result	 = 0;
		read_len = conn->last_read_len;
	} else {
		//try reusing last read buffer
		if (conn->last_read != NULL && conn->last_read_size < read_len) {
			kfree(conn->last_read);
			conn->last_read = NULL;
		}

		if (conn->last_read == NULL) {
			conn->last_read = kzalloc(read_len, GFP_KERNEL);
		}

		if (conn->last_read == NULL) {
			result = -ENOMEM;
			goto send_err;
		}

		// remember size of last read request before it gets overwritten
		conn->last_read_size = read_len;

		result = TFFS3_Read(conn->loc_handle, conn->last_read, &read_len);

		if (result != 0) {
			kfree(conn->last_read);
			conn->last_read	     = NULL;
			conn->last_read_size = 0;
			goto send_err;
		}

		conn->seq_nr	    = msg->seq_nr;
		conn->last_read_len = read_len;
	}

	if (read_len > 0) {
#if !defined TFFS_X86_PUMA7
		res = request_mem_region((resource_size_t)io_addr - REMOTE_MEM_OFFSET,
					 msg->call.read.len, "TFFSSrv");
		if (res == NULL) {
			pr_err("[%s] requesting region at 0x%08lx failed!\n", __func__,
			       io_addr - REMOTE_MEM_OFFSET);
			result = -EIO;
			goto send_err;
		}
#endif
		buff = (unsigned char *)ioremap(io_addr - REMOTE_MEM_OFFSET,
							msg->call.read.len);
		if (buff == NULL) {
			pr_err("[%s] mapping remote buffer at 0x%08lx failed!\n", __func__,
			       io_addr - REMOTE_MEM_OFFSET);
#if !defined TFFS_X86_PUMA7
			release_mem_region(io_addr - REMOTE_MEM_OFFSET, msg->call.read.len);

#endif
			result = -EIO;
			goto send_err;
		}

		//        pr_err("[%s] buffer mapped to %p\n", __func__, buff);

		//        flush_cache(buff, msg->call.read.len, flush_read);

		crc = crc32_be(0, conn->last_read, read_len);

		memcpy_toio(buff, conn->last_read, read_len);
		flush_cache(buff, msg->call.read.len, flush_write);
#if 0
		if(((msg->call.read.id >= 0x100 && msg->call.read.id <= 0x1ff) || msg->call.read.id ==  209)
				&& result == 0 && read_len > 0)
		{
			int hashsize;
			pr_err("[%s] buffer: %p local: %p read_len: 0x%x\n", __func__, io_addr, buff, read_len);
			hashsize = plaintext_to_sha1(conn->last_read, read_len, hashbuf, sizeof(hashbuf));
			print_hex_dump(KERN_ERR, "hash: ", DUMP_PREFIX_OFFSET, hashsize, 1, hashbuf, hashsize, 0);
			print_hex_dump(KERN_ERR, "dump: ", DUMP_PREFIX_OFFSET, 32, 1, buff, read_len, 0);
		}
#endif
		iounmap(buff);
#if !defined TFFS_X86_PUMA7
		release_mem_region(io_addr - REMOTE_MEM_OFFSET, msg->call.read.len);
#endif
	}

send_err:
	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg	       = *msg;
	snd_msg->src_id	       = ctx->node_id;
	snd_msg->dst_id	       = msg->src_id;
	snd_msg->result	       = result;
	snd_msg->call.read.len = read_len;
	snd_msg->call.read.crc = crc;

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending read-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

static void handle_write(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	int result;
	struct tffs_client_conn *conn;
	struct avm_event_tffs *snd_msg;
	uintptr_t io_addr;
	unsigned char *buff;
	size_t write_len;
	unsigned int final;
	uint32_t crc;

	// pr_err("[%s] Called\n", __func__);

	// pr_err("[%s] request: src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx\n",
	//         __func__, msg->src_id, msg->dst_id, msg->clt_handle, msg->srv_handle);

	BUG_ON(msg->type != avm_event_tffs_call_write);

	snd_msg	  = &ctx->send_msg;
	io_addr	  = (uintptr_t)msg->call.read.buff_addr;
	write_len = msg->call.write.len;
	final	  = msg->call.write.final != 0 ? 1 : 0;

	if (io_addr == 0 && write_len != 0) {
		result = -EFAULT;
		goto send_err;
	}

	conn = get_connection(ctx, msg->src_id, msg->clt_handle);
	if (conn == NULL) {
		result = -EBADF;
		goto send_err;
	}

	if (conn->state != tffs_conn_open) {
		result = -EIO;
		goto send_err;
	}

	// if this is a dupe of the last request, just re-send the last reply
	if (msg->seq_nr == conn->seq_nr && write_len == conn->last_write_len) {
		result = conn->last_write_result;
		goto conn_err;
	}

	// abort if this is a dupe of an earlier request or with skipped sequence numbers
	if (msg->seq_nr <= conn->seq_nr || msg->seq_nr > (conn->seq_nr + 1)) {
		result = -EIO;
		goto conn_err;
	}

	buff = NULL;
	if (io_addr != 0) {
		// map foreign buffer
		buff = (unsigned char *)ioremap(io_addr - REMOTE_MEM_OFFSET, write_len);
		if (buff == NULL) {
			pr_err("[%s] mapping remote buffer failed!\n", __func__);
			result = -EIO;
			goto conn_err;
		}

		flush_cache(buff, write_len, flush_read);
		// FIXME: should use extra buffer and memcpy_fromio
		crc = crc32_be(0, buff, write_len);
	} else {
		// ignore crc on zero sized buffers
		crc = msg->call.write.crc;
	}

	if (crc == msg->call.write.crc) {
		result = TFFS3_Write(conn->loc_handle, buff, write_len, final);
	} else {
		pr_warn("[%s] CRC error.\n", __func__);
		result = -EIO;
	}

	if (buff != NULL) {
		iounmap(buff);
	}

conn_err:
	if (result == 0) {
		// segment was successfully written. Update sequence number and store write status
		conn->seq_nr		= msg->seq_nr;
		conn->last_write_len	= write_len;
		conn->last_write_result = result;
	} else {
		// something went wrong...
		conn->last_write_len = 0;
		conn->state	     = tffs_conn_err;
	}

send_err:
	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg		= *msg;
	snd_msg->src_id		= ctx->node_id;
	snd_msg->dst_id		= msg->src_id;
	snd_msg->result		= result;
	snd_msg->call.write.len = write_len;

	// pr_err("[%s] reply: src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx result: %d\n",
	//        __func__, snd_msg->src_id, snd_msg->dst_id, snd_msg->clt_handle, snd_msg->srv_handle, snd_msg->result);

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_debug("[%s] Error sending write-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

static void handle_init(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	int result;
	struct tffs_remote_clnt *clnt;
	struct avm_event_tffs *snd_msg;
	struct tffs_core_handle *loc_handle;
	uint32_t max_seg_size;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_init);

	//    pr_err("[%s] request: src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx\n",
	//            __func__, msg->src_id, msg->dst_id, msg->clt_handle, msg->srv_handle);

	snd_msg	     = &ctx->send_msg;
	result	     = 0;
	max_seg_size = 0;

	clnt = get_client(ctx, msg->src_id);
	if (clnt == NULL) {
		clnt = kzalloc(sizeof(*clnt), GFP_KERNEL);

		if (clnt == NULL) {
			pr_err("[%s] unable to allocate memory for new client struct\n", __func__);
			result = -ENOMEM;
			goto send_err;
		}

		INIT_LIST_HEAD(&clnt->clnt_list);
		INIT_LIST_HEAD(&clnt->conn_list);
		clnt->clnt_id	  = msg->src_id;
		clnt->mem_offset  = msg->call.init.mem_offset;
		clnt->clnt_handle = msg->clt_handle;

		result = add_client(ctx, clnt);
		if (result != 0) {
			pr_err("[%s] adding client 0x%x to list failed with code %d\n", __func__,
			       clnt->clnt_id, result);
			kfree(clnt);
			clnt = NULL;
			goto send_err;
		}
	} else {
		if (clnt->clnt_handle != msg->clt_handle ||
		    clnt->mem_offset != msg->call.init.mem_offset) {
			pr_err("[%s] received init msg for known client 0x%x with differing parameters\n",
			       __func__, msg->src_id);
			result = -EEXIST;
			goto send_err;
		}
	}

	loc_handle = TFFS3_Open(FLASH_FS_ID_PANIC_LOG, tffs3_mode_write);
	if (IS_ERR_OR_NULL(loc_handle)) {
#if defined TFFS_X86_PUMA7
		pr_err("[tffs_remote_srv:%s()] TFFS3_Open() failed, skipping call for testing initialisation of remote tffs\n",
		       __func__);
#else
		result = PTR_ERR(loc_handle);
#endif
		goto send_err;
	}

	max_seg_size = loc_handle->max_segment_size;
	TFFS3_Close(loc_handle);

send_err:
	if (result != 0) {
		clnt = NULL;
	}

	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg			= *msg;
	snd_msg->src_id			= ctx->node_id;
	snd_msg->dst_id			= msg->src_id;
	snd_msg->srv_handle		= atomic_inc_return(&ctx->handle_seq);
	snd_msg->call.init.mem_offset	= REMOTE_MEM_OFFSET;
	snd_msg->call.init.max_seg_size = max_seg_size;
	snd_msg->result			= result;

	//    pr_err("[%s] reply: result: %d src_id: 0x%x dst_id: 0x%x clnt_handle: 0x%llx srv_handle: 0x%llx\n",
	//            __func__, snd_msg->result, snd_msg->src_id, snd_msg->dst_id,
	//            snd_msg->clt_handle, snd_msg->srv_handle);

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending init-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

static void handle_cleanup(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	int result;
	struct tffs_client_conn *conn;
	struct avm_event_tffs *snd_msg;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_cleanup);

	snd_msg = &ctx->send_msg;

	conn = get_connection(ctx, msg->src_id, msg->clt_handle);
	if (conn == NULL) {
		result = -ENODEV;
		goto send_err;
	}

	result = TFFS3_Cleanup(conn->loc_handle);

send_err:
	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg	= *msg;
	snd_msg->src_id = ctx->node_id;
	snd_msg->dst_id = msg->src_id;
	snd_msg->result = result;

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending cleanup-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

static void handle_reindex(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	int result;
	struct avm_event_tffs *snd_msg;
	struct tffs_remote_clnt *clnt;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_reindex);

	snd_msg = &ctx->send_msg;
	clnt	= get_client(ctx, msg->src_id);
	if (clnt == NULL) {
		result = -ENODEV;
		goto send_err;
	}

	result = TFFS3_Create_Index();

send_err:
	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg	= *msg;
	snd_msg->src_id = ctx->node_id;
	snd_msg->dst_id = msg->src_id;
	snd_msg->result = result;

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending reindex-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

static void handle_info(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	int result;
	struct tffs_client_conn *conn;
	struct avm_event_tffs *snd_msg;
	unsigned int fill = 0;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_info);

	snd_msg = &ctx->send_msg;

	conn = get_connection(ctx, msg->src_id, msg->clt_handle);
	if (conn == NULL) {
		result = -ENODEV;
		goto send_err;
	}

	result = TFFS3_Info(conn->loc_handle, &fill);

send_err:
	memset(snd_msg, 0x0, sizeof(*snd_msg));
	*snd_msg		      = *msg;
	snd_msg->src_id		      = ctx->node_id;
	snd_msg->dst_id		      = msg->src_id;
	snd_msg->result		      = result;
	snd_msg->call.info.fill_level = fill;

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending init-reply to client 0x%x\n", __func__, msg->src_id);
	}

	return;
}

#if IS_ENABLED(CONFIG_AVM_TFFS_REMOTE_RESET_STATUS_HACK)
static void avm_set_reset_status_on_remote_panic(const struct avm_event_tffs_paniclog *pl)
{
	avm_set_reset_status(pl->reboot_status);
}
#else
static void avm_set_reset_status_on_remote_panic(const struct avm_event_tffs_paniclog *pl)
{
}
#endif

static void handle_paniclog(struct tffs_srv_ctx *ctx, struct avm_event_tffs *msg)
{
	struct tffs_core_handle *loc_handle;
	struct tffs_remote_clnt *clnt;
	unsigned int i;
	unsigned char *buff;
	size_t buff_len, written, partlen;
	uintptr_t io_addr;
	enum _tffs_id log_id;
	int result, final;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(msg->type != avm_event_tffs_call_paniclog);

	console_verbose();
	/*
	 * Output warning, starting a new line (which separates our message
	 * from an existing, incomplete line, and if terminal output is
	 * timestamped, results in a correct timestamp).
	 */
	pr_crit("\nReceiving remote panic log!\n");

	avm_set_reset_status_on_remote_panic(&msg->call.paniclog);

	result	= 0;
	io_addr = (uintptr_t)msg->call.read.buff_addr;

	clnt = get_client(ctx, msg->src_id);
	if (clnt == NULL) {
		pr_err("[%s] received panic log from unknown client id 0x%x\n", __func__,
		       msg->src_id);
		goto err_out;
	}

	/*
	 * If the client is panicking, attempts to talk to it may run into
	 * long timeouts in the puma7 mailbox driver if the mbox channel to
	 * the remote cpu is busy.
	 *
	 * This is bad as the system is about to go down, abruptly.  In
	 * particular, trying to notify clients of changes can delay the
	 * server enough to prevent the log data from hitting storage.
	 *
	 * Enter the error state to sidestep this.
	 */
	ctx->state = tffs_srv_error;

	log_id = FLASH_FS_ID_SKIP;
	for (i = 0; i < ARRAY_SIZE(log_id_map); ++i) {
		if (msg->src_id == log_id_map[i].node_id) {
			log_id = log_id_map[i].paniclog_id;
			break;
		}
	}

	if (log_id == FLASH_FS_ID_SKIP) {
		pr_err("[%s] can't match paniclog from source 0x%x to TFFS-ID\n", __func__,
		       msg->src_id);
		goto err_out;
	}

	if (remote_paniclog_wait_secs > 0)
		ssleep(remote_paniclog_wait_secs);

	loc_handle = TFFS3_Open(log_id, tffs3_mode_write);

	if (IS_ERR_OR_NULL(loc_handle)) {
		pr_err("[%s] TFFS3_Open failed for TFFS-ID 0x%x\n", __func__, log_id);
		goto err_out;
	}

	pr_devel("[%s] Received paniclog from source 0x%x with len=%llu\n",
		__func__, msg->src_id, msg->call.paniclog.len);

	// map foreign buffer
	buff_len = msg->call.paniclog.len;
	buff = ioremap(io_addr - REMOTE_MEM_OFFSET, buff_len);

	if (buff == NULL) {
		pr_err("[%s] mapping remote buffer failed!\n", __func__);
		goto err_out;
	}

	flush_cache(buff, buff_len, flush_read);
	// FIXME: should use extra buffer and memcpy_fromio

	/* The received panic log might be larger than one segment, so we write it
	 * one segment at a time */
	written = 0;
	do {
		partlen = min(buff_len - written, loc_handle->max_segment_size);
		final   = written + partlen >= buff_len;
		result  = TFFS3_Write(loc_handle, &buff[written], partlen, final);

		written += partlen;
	} while (!result && buff_len > written);

	pr_notice("Remote paniclog written\n");

	if (result) {
		pr_err("[%s] Writing paniclog from source 0x%x failed with error %d\n",
			__func__, msg->src_id, result);
	}
	iounmap(buff);
err_out:
	return;
}

static int TFFS3_SERVER_callback(void *priv, struct avm_event_tffs *tffs_msg)
{
	struct tffs_srv_ctx *ctx;
	struct tffs_remote_message *request;
	unsigned long flags;

	//    pr_err("[%s] Called\n", __func__);

	BUG_ON(priv == NULL);
	ctx = (struct tffs_srv_ctx *)priv;

	request = kzalloc(sizeof(*request), GFP_KERNEL);
	if (request == NULL) {
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&request->msg_list);
	memcpy(&request->msg, tffs_msg, sizeof(*tffs_msg));

	spin_lock_irqsave(&ctx->req_lock, flags);
	list_add_tail(&request->msg_list, &ctx->req_list);
	spin_unlock_irqrestore(&ctx->req_lock, flags);

	set_bit(TFFS_EVENT_BIT_TRIGGER, &ctx->events);
	wake_up_interruptible_sync(&ctx->wait_queue);

	return 0;
}

static void handle_requests(struct tffs_srv_ctx *ctx)
{
	struct tffs_remote_message *request;
	unsigned long flags, done;

	//    pr_err("[%s] Called\n", __func__);

	done = 0;
	do {
		request = NULL;
		spin_lock_irqsave(&ctx->req_lock, flags);
		if (!list_empty(&ctx->req_list)) {
			request = list_first_entry(&ctx->req_list, struct tffs_remote_message,
						   msg_list);
			list_del_init(&request->msg_list);
		} else {
			done = 1;
		}
		spin_unlock_irqrestore(&ctx->req_lock, flags);

		if (request != NULL) {
			switch (request->msg.type) {
			case avm_event_tffs_call_open:
				handle_open(ctx, &request->msg);
				break;
			case avm_event_tffs_call_close:
				handle_close(ctx, &request->msg);
				break;
			case avm_event_tffs_call_read:
				handle_read(ctx, &request->msg);
				break;
			case avm_event_tffs_call_write:
				handle_write(ctx, &request->msg);
				break;
			case avm_event_tffs_call_cleanup:
				handle_cleanup(ctx, &request->msg);
				break;
			case avm_event_tffs_call_reindex:
				handle_reindex(ctx, &request->msg);
				break;
			case avm_event_tffs_call_info:
				handle_info(ctx, &request->msg);
				break;
			case avm_event_tffs_call_init:
				handle_init(ctx, &request->msg);
				break;
			case avm_event_tffs_call_deinit:
				break;
			case avm_event_tffs_call_notify:
				// should not happen
				break;
			case avm_event_tffs_call_paniclog:
				handle_paniclog(ctx, &request->msg);
				// should not happen
				break;
			default:
				pr_err("[%s] unknown request type 0x%x\n", __func__,
				       request->msg.type);
			}
			kfree(request);
		}
	} while (done == 0);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_SERVER_thread(void *data)
{
	struct tffs_srv_ctx *ctx;
	int result;
	struct tffs_core_handle *handle;

	ctx = (struct tffs_srv_ctx *)data;

	result = avm_event_node_established(event_established_callback, ctx, 0, 0);
	if (result != 0) {
		pr_err("[%s] installing event node callback failed\n", __func__);
		goto err_out;
	}

	do {
		pr_err("[%s] waiting for remote node connection to be established\n", __func__);
		if (wait_event_interruptible(ctx->wait_queue, ctx->state != tffs_srv_wait_node)) {
			pr_err("[%s] interrupted while waiting for remote node, exiting\n",
			       __func__);
			result = -EINTR;
			break;
		}
	} while (ctx->state == tffs_srv_wait_node);

	if (result != 0) {
		goto err_out;
	}

	ctx->node_priv = avm_event_register_tffs(ctx->node_id, TFFS3_SERVER_callback, ctx);
	if (IS_ERR_OR_NULL(ctx->node_priv)) {
		result = (ctx->node_priv == NULL) ? -ENOMEM : PTR_ERR(ctx->node_priv);
		goto err_out;
	}

	handle = TFFS3_Open(0, tffs3_mode_write);
	BUG_ON(handle == NULL);

	result = 0;
	do {
		handle_requests(ctx);

		//        pr_err("[%s] waiting for events\n", __func__);
		if (wait_event_interruptible(ctx->wait_queue,
					     test_and_clear_bit(TFFS_EVENT_BIT_TRIGGER,
								&ctx->events))) {
			pr_err("[%s] interrupted while waiting for events, exiting\n", __func__);
			result = -EINTR;
			break;
		}

	} while (!kthread_should_stop());

	TFFS3_Close(handle);

err_out:
	pr_info("[%s] event thread dead\n", __func__);

	return result;
}

static void TFFS3_SERVER_Notify(void *priv, unsigned int id, enum tffs3_notify_event event)
{
	struct tffs_srv_ctx *ctx;
	struct avm_event_tffs *snd_msg;
	int result;

	ctx = (struct tffs_srv_ctx *)priv;
	BUG_ON(ctx == NULL);

	pr_debug("[%s] Called for ID 0x%x event %s\n", __func__, id,
		 event == tffs3_notify_clear  ? "clear" :
		 event == tffs3_notify_update ? "update" :
		 event == tffs3_notify_reinit ? "reinit" :
						      "unknown");

	if (ctx->state != tffs_srv_running || ctx->node_priv == NULL) {
		pr_err("[%s] Server not running (state=%u), dropping notification\n",
		       __func__, ctx->state);
		return;
	}

	// FIXME: add locking for snd_msg? add acknowledge from clients
	snd_msg = &ctx->send_msg;

	memset(snd_msg, 0x0, sizeof(*snd_msg));
	snd_msg->src_id		= ctx->node_id;
	snd_msg->dst_id		= AVM_EVENT_TFFS_NODE_ANY;
	snd_msg->srv_handle	= atomic_inc_return(&ctx->handle_seq);
	snd_msg->type		= avm_event_tffs_call_notify;
	snd_msg->call.notify.id = id;

	switch (event) {
	case tffs3_notify_clear:
		snd_msg->call.notify.event = avm_event_tffs_notify_clear;
		break;
	case tffs3_notify_reinit:
		snd_msg->call.notify.event = avm_event_tffs_notify_reinit;
		break;
	case tffs3_notify_update:
	default:
		snd_msg->call.notify.event = avm_event_tffs_notify_update;
		break;
	}

	result = avm_event_tffs_call(ctx->node_priv, snd_msg);
	if (result != 0) {
		// sending reply failed. nothing to do, client will retry...
		pr_err("[%s] Error sending notification to clients\n", __func__);
	}
}
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_SERVER_Setup(struct tffs_server *this)
{
	struct tffs_srv_ctx *ctx;
	int result;

	pr_info("[TFFS3-REMOTE] Remote storage server for TFFS 3.x\n");

	result = 0;
	ctx    = (struct tffs_srv_ctx *)this->priv;
	BUG_ON(ctx == NULL);

	ctx->state = tffs_srv_setup;

	ctx->kthread = kthread_run(TFFS3_SERVER_thread, (void *)ctx, "tffs_server");
	BUG_ON(IS_ERR_OR_NULL(ctx->kthread));

	return result;
}
EXPORT_SYMBOL(TFFS3_SERVER_Setup);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_SERVER_Configure(struct tffs_server *this, unsigned int node_id)
{
	struct tffs_srv_ctx *ctx;
	int result;

	result = 0;
	ctx    = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		pr_err("[TFFS3-SERVER] Out of memory during configuration\n");
		result = -ENOMEM;
		goto err_out;
	}

	sema_init(&(ctx->lock), 1);
	init_waitqueue_head(&(ctx->wait_queue));
	INIT_LIST_HEAD(&ctx->req_list);
	INIT_LIST_HEAD(&ctx->clnt_list);
	spin_lock_init(&ctx->req_lock);
	atomic_set(&ctx->handle_seq, 1);
	ctx->node_id = node_id;
	ctx->state   = tffs_srv_none;

	this->setup  = TFFS3_SERVER_Setup;
	this->priv   = ctx;
	this->notify = TFFS3_SERVER_Notify;

err_out:
	return result;
}
EXPORT_SYMBOL(TFFS3_SERVER_Configure);
