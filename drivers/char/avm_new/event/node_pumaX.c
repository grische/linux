// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2014-2019 AVM GmbH <fritzbox_info@avm.de> */

#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <asm/cacheflush.h>
#include <linux/printk.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/slab.h>

#include <avm/event/event.h>
#include "internal.h"
#include "struct/endian.h"
#include "struct/avm_event_gen_types.h"
#include "remote.h"

#if defined(CONFIG_X86)
#include <linux/ce_mailbox.h>
#elif defined(REMOTE_EVENT_PUMA7_ARM)
#include <asm-arm/arch-avalanche/generic/arm_atom_mbx.h>
#elif defined(REMOTE_EVENT_PUMA6_ARM)
#include <arch-avalanche/puma6/arm_atom_mbx.h>
#endif

#include <avm/event/event.h>
#include <asm/atomic.h>
#endif /*--- #if defined(__KERNEL__) ---*/

/*--- #include <linux/avm_event.h> ---*/
//#undef DEBUG_SAMMEL
/*--- #define AVM_EVENT_DEBUG ---*/
// clang-format off
#define EVENT_MSG_SIZE      512
#define EVENT_QUEUE_LEN     64

#if 0
#define EVENT_TYPE_PING             0x01
#define EVENT_TYPE_SRC_REGISTER     0x02
#define EVENT_TYPE_SRC_UNREGISTER   0x03
#define EVENT_TYPE_SRC_TRIGGER      0x04
#define EVENT_TYPE_SNK_REGISTER     0x05
#define EVENT_TYPE_SNK_UNREGISTER   0x06
#define EVENT_TYPE_SNK_TRIGGER      0x07
#define EVENT_TYPE_MASK             0xFFFF
#define EVENT_TYPE(x)               ((x) & EVENT_TYPE_MASK)
#endif

#define EVENT_REPLY_NONE            0x00
#define EVENT_REPLY_SUCCESS         0x01
#define EVENT_REPLY_FAIL            0x02
#define EVENT_REPLY_ERROR           0x03

#define SYNC_TIMEOUT                (5 * HZ)

#define EVENT_WORK_BIT_RUN          0
#define EVENT_WORK_RUN              (1 << EVENT_WORK_BIT_RUN)
// clang-format on

/**
 * fuer node_event_established_callback()
 */
static void node_event_established_callback_progress(struct work_struct *work);
static DEFINE_SPINLOCK(cb_lock);

DECLARE_WORK(cb_nodes_work, node_event_established_callback_progress);

struct _avm_event_node *avm_event_nodes[MAX_AVM_EVENT_NODES];

/**
 */
struct event_queue {
	volatile __be32 queue_len;
	volatile __be32 write_curr;
	volatile __be32 read_curr;
	volatile struct avm_event_message buff[EVENT_QUEUE_LEN];
} __attribute__((packed));

/**
 */
struct node_queue_data {
	unsigned int queue_len;
	struct resource *io_region;
	dma_addr_t phys_base;
	spinlock_t lock;
	volatile struct event_queue *queue;
};

/**
 */
struct sync_result {
	struct list_head list;
	unsigned int nonce;
	struct avm_event_message *msg;
	volatile int result;
	volatile int completed;
};

/**
 */
enum node_state {
	NODE_ERROR = 0,
	NODE_INIT,
	NODE_ADDR_SENT,
	NODE_ADDR_RCVD,
	NODE_PING_SENT,
	NODE_PING_RCVD,
	NODE_RUNNING,
	NODE_STALLED,
	NODE_SHUTDOWN,
};

/**
 */
struct _node_establish_cb {
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2);
	void *context;
	unsigned int param1;
	unsigned int param2;
	struct _node_establish_cb *next;
};

/**
 */
struct node_data {
	enum node_state state;
	atomic_t nonce;
	struct list_head wait_list;
	struct _node_establish_cb *first_cb;
	wait_queue_head_t sync_queue;
	spinlock_t list_lock;
};

/**
 */
static struct _event_node_ctrl {
	struct node_data node;
	struct node_queue_data send_queue, recv_queue;
	struct task_struct *kthread;
	wait_queue_head_t wait_queue;
	volatile unsigned long trigger;
	volatile unsigned long last_ping;
	volatile unsigned long last_pong;
	uint32_t last_nonce;
	struct event_node_stats stats;
	struct event_tffs_handle tffs_handles[MAX_TFFS_HANDLES];
	spinlock_t tffs_lock;
} event_node_ctrl;

static void event_node_trigger(struct _event_node_ctrl *pctrl);

#define DBG_ERR(args...) pr_err("[avm_event_node]" args)
/*--- #define AVM_EVENT_NODE_DEBUG ---*/
#if defined(AVM_EVENT_NODE_DEBUG)
#define DBG_WARN(args...) pr_warn("[avm_event_node]" args)
#define DBG_NOTE(args...) pr_notice("[avm_event_node]" args)
#define DBG_INFO(args...) pr_info("[avm_event_node]" args)
#define DBG_TRACE(args...) pr_info("[avm_event_node]" args)
#else /*--- #if defined(AVM_EVENT_NODE_DEBUG) ---*/
#define DBG_WARN(args...)
#define DBG_NOTE(args...)
#define DBG_INFO(args...)
#define DBG_TRACE(args...)
#endif /*--- #else ---*/ /*--- #if defined(AVM_EVENT_NODE_DEBUG) ---*/

/**
 */
static void dump_msg(const char *prefix, struct avm_event_message *msg)
{
	unsigned int len = msg->length;
	unsigned char *data = (unsigned char *)&msg->message;

	DBG_ERR("[%s] Message Dump:\n", prefix);
	DBG_ERR("[%s] magic:    0x%08x\n", prefix, msg->magic);
	DBG_ERR("[%s] nonce:    0x%08x\n", prefix, msg->nonce);
	DBG_ERR("[%s] flags:    0x%08x\n", prefix, msg->flags);
	DBG_ERR("[%s] result:   0x%08x\n", prefix, msg->result);
	DBG_ERR("[%s] length:   0x%08x\n", prefix, msg->length);
	if (len > sizeof(struct avm_event_message)) {
		len = be32_to_cpu(len);
		if (len > sizeof(struct avm_event_message)) {
			DBG_ERR("[%s] error Msg-Len %x\n", prefix, len);
			return;
		}
	}
	DBG_ERR("[%s] Msg-Len %d:\n", prefix, len);
	while (len--) {
		DBG_ERR(KERN_CONT "%02x,", *data++);
	}
	DBG_ERR(KERN_ERR "\n");
}

#if defined(__KERNEL__)

/**
 */
static void flush_cache(void *addr, size_t len)
{
#if defined(CONFIG_X86)
	clflush_cache_range(addr, len);
#else
	consistent_sync(addr, len, DMA_BIDIRECTIONAL);
#endif
}

/**
 */
static int setup_sendqueue(struct node_queue_data *psend_queue)
{
	psend_queue->queue =
		dma_alloc_coherent(NULL, sizeof(struct event_queue),
				   &(psend_queue->phys_base), GFP_KERNEL);

	if (psend_queue->queue == NULL) {
		DBG_ERR("[%s] Unable to allocate send queue.\n", __func__);

		return -ENOMEM;
	}

	DBG_ERR("[%s] virt: %p phys: %08lx\n", __func__, psend_queue->queue,
		(unsigned long)psend_queue->phys_base);
	DBG_ERR("[%s] queue: %p queue.buff[0]: %p\n", __func__,
		psend_queue->queue, &(psend_queue->queue->buff[0]));

	spin_lock_init(&psend_queue->lock);
	psend_queue->queue->queue_len = cpu_to_be32(EVENT_QUEUE_LEN);
	psend_queue->queue->write_curr = 0;
	psend_queue->queue->read_curr = 0;
	mb();
	flush_cache((void *)psend_queue->queue, sizeof(struct event_queue));

#if 0
	DBG_ERR("[%s] sizeof(struct avm_event_message): 0x%x\n", __func__, sizeof(struct avm_event_message));
	DBG_ERR("[%s] sizeof(foo): 0x%x\n", __func__, sizeof(foo));
	DBG_ERR("[%s] offset(foo.type): 0x%x\n", __func__, __builtin_offsetof(struct avm_event_message, type));

	DBG_ERR("[%s] sizeof(foo.type): 0x%x\n", __func__, sizeof(foo.type));
	DBG_ERR("[%s] offset(foo.message): 0x%x\n", __func__, __builtin_offsetof(struct avm_event_message, message));

	DBG_ERR("[%s] sizeof(foo.message): 0x%x\n", __func__, sizeof(foo.message));
	DBG_ERR("[%s] offset(foo.message.sink_trigger.data): 0x%x\n", __func__, __builtin_offsetof(struct avm_event_sink_trigger, data));
	DBG_ERR("[%s] offset(foo.message.sink_trigger.data.data): 0x%x\n", __func__, __builtin_offsetof(struct avm_event_data, data));

	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.pm_info_stat): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.pm_info_stat));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.cpmac): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.cpmac));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.cpu_idle): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.cpu_idle));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.led_info): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.led_info));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.led_status): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.led_status));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.pm_info_stat): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.pm_info_stat));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.powermanagement_status): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.powermanagment_status));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.push_button): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.push_button));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.telefonprofile): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.telefonprofile));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.temperature): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.temperature));
	DBG_ERR("[%s] sizeof(foo.message.sink_trigger.data.data.unserialised): 0x%x\n", __func__, sizeof(foo.message.sink_trigger.data.data.unserialised));

	DBG_ERR("[%s] dma_get_cache_alignment(): 0x%x\n", __func__, dma_get_cache_alignment());
#endif

	return 0;
}

/**
 */
static void free_sendqueue(struct node_queue_data *psend_queue)
{
	if (psend_queue->queue != NULL) {
		dma_free_coherent(NULL, sizeof(struct event_queue),
				  (void *)psend_queue->queue,
				  psend_queue->phys_base);

		psend_queue->queue = NULL;
	}
}

#if defined(REMOTE_EVENT_PUMA6_X86)
extern struct resource *puma6_get_arm_ram(void);
#endif
static int setup_recvqueue(uint32_t queue_addr,
			   struct _event_node_ctrl *pctrl)
{
	struct node_queue_data *precv_queue;
	resource_size_t size;

	precv_queue = &pctrl->recv_queue;
	size = sizeof(struct event_queue);

#if defined(REMOTE_EVENT_PUMA7_ARM) || defined(REMOTE_EVENT_PUMA6_ARM)
	precv_queue->io_region = request_mem_region((resource_size_t)queue_addr,
						    size, "EventNodeQueue");
#elif defined(REMOTE_EVENT_PUMA6_X86)
	precv_queue->io_region = puma6_get_arm_ram();
#elif defined(REMOTE_EVENT_PUMA7_X86)
	precv_queue->io_region =
		NULL; //request_mem_region((resource_size_t) queue_addr, size, "ARM-RAM");      /*--- todo ? ---*/
#else
	precv_queue->io_region = NULL;
#endif

	if (precv_queue->io_region == NULL) {
		DBG_ERR("[%s] Unable to request_mem_region at Addr. 0x%x\n",
			__func__, queue_addr);
#if !defined(REMOTE_EVENT_PUMA7_X86)
		return -EFAULT;
#endif /*--- #if !defined(REMOTE_EVENT_PUMA7_X86)  ---*/
	}
	precv_queue->phys_base = (dma_addr_t)queue_addr;
	precv_queue->queue =
		(struct event_queue *)ioremap_nocache(queue_addr, size);

	DBG_INFO("[%s] virt: %p phys: %08x\n", __func__, precv_queue->queue,
		 queue_addr);
	DBG_INFO("[%s] queue: %p queue.buff[0]: %p\n", __func__,
		 precv_queue->queue, &(precv_queue->queue->buff[0]));

	if (precv_queue->queue == NULL) {
		DBG_ERR("[%s] Unable to set up receive queue.\n", __func__);
		return -EINVAL;
	}

	flush_cache((void *)precv_queue->queue, sizeof(struct event_queue));
	mb();

	if (EVENT_QUEUE_LEN != be32_to_cpu(precv_queue->queue->queue_len)) {
		DBG_ERR("[%s] Queue size mismatch. Expected: 0x%x, found: 0x%x\n",
			__func__, EVENT_QUEUE_LEN,
			be32_to_cpu(precv_queue->queue->queue_len));
		return -EINVAL;
	}

	spin_lock_init(&precv_queue->lock);
	precv_queue->queue_len = EVENT_QUEUE_LEN;
	pctrl->stats.recv_len = precv_queue->queue_len;
	pctrl->stats.recv_min_free = precv_queue->queue_len;
	return 0;
}

/**
 */
static void free_recvqueue(struct node_queue_data *precv_queue)
{
	if (precv_queue->queue != NULL) {
		if (precv_queue->io_region) {
			iounmap(precv_queue->io_region);
			release_mem_region(
				(unsigned long)precv_queue->phys_base,
				sizeof(struct event_queue));
		}
		precv_queue->queue = NULL;
	}
}

/**
 */
static inline void send_irq(struct _event_node_ctrl *pctrl)
{
	trigger_remotecpuirq(AVM_EVENT_REMOTECPU_IRQ);
	set_bit(EVENT_WORK_BIT_RUN, &pctrl->trigger);
	wake_up_interruptible_sync(&pctrl->wait_queue);
}

/**
 */
static int send_data(struct avm_event_message *msg,
		     struct _event_node_ctrl *pctrl)
{
	uint32_t write_curr, read_curr, queue_len;
	unsigned long flags;
	struct node_queue_data *psend_queue = &pctrl->send_queue;
	struct node_queue_data *precv_queue = &pctrl->recv_queue;
	unsigned int free;

	if (pctrl->node.state < NODE_ADDR_RCVD) {
		return -ENODEV;
	}

	if (msg == NULL) {
		return -EINVAL;
	}

	if (unlikely(oops_in_progress)) {
		if (!spin_trylock_irqsave(&psend_queue->lock, flags)) {
			return -EBUSY;
		}
	} else {
		spin_lock_irqsave(&psend_queue->lock, flags);
	}

	mb();

#if 0
	DBG_ERR("[%s] pre:\n", __func__);
	DBG_ERR("[%s] sendq: read: 0x%x write: 0x%x\n", __func__, be32_to_cpu(psend_queue->queue->read_curr), be32_to_cpu(psend_queue->queue->write_curr));
	DBG_ERR("[%s] recvq: read: 0x%x write: 0x%x\n", __func__, be32_to_cpu(precv_queue->queue->read_curr), be32_to_cpu(precv_queue->queue->write_curr));
#endif

	flush_cache((void *)psend_queue->queue, sizeof(struct event_queue));
	flush_cache((void *)precv_queue->queue, sizeof(struct event_queue));

	queue_len = be32_to_cpu(psend_queue->queue->queue_len);
	read_curr = be32_to_cpu(precv_queue->queue->read_curr) % queue_len;
	write_curr = be32_to_cpu(psend_queue->queue->write_curr) % queue_len;

	free = (queue_len - 1 - write_curr + read_curr) % queue_len;
	if (free < pctrl->stats.send_min_free){
		 pctrl->stats.send_min_free = free;
	}

	if (free > 0) {
		++write_curr;
		write_curr %= queue_len;
		/**
		 *                 DEST                              SOURCE       LEN
		 */
		memcpy_toio(&(psend_queue->queue->buff[write_curr]), msg,
			    sizeof(struct avm_event_message));
		mb();

		flush_cache((void *)&(psend_queue->queue->buff[write_curr]),
			    sizeof(struct avm_event_message));

		psend_queue->queue->write_curr = cpu_to_be32(write_curr);
		mb();

		flush_cache((void *)&(psend_queue->queue->write_curr),
			    sizeof(psend_queue->queue->write_curr));
		mb();
	} else {
		DBG_ERR("[%s] Queue full, dropping message\n", __func__);
		pctrl->stats.dropped++;
        }

	spin_unlock_irqrestore(&psend_queue->lock, flags);

	send_irq(pctrl);

#if 0
	DBG_ERR("[%s] slot 0x%x, nonce 0x%x flags: 0x%08x\n", __func__, write_curr, be32_to_cpu(msg->nonce), be32_to_cpu(msg->flags));

	DBG_ERR("[%s] post:\n", __func__);
	DBG_ERR("[%s] sendq: read: 0x%x write: 0x%x\n", __func__, be32_to_cpu(psend_queue->queue->read_curr), be32_to_cpu(psend_queue->queue->write_curr));
	DBG_ERR("[%s] recvq: read: 0x%x write: 0x%x\n", __func__, be32_to_cpu(precv_queue->queue->read_curr), be32_to_cpu(precv_queue->queue->write_curr));
#endif

	return 0;
}

/**
 */
static int receive_data(struct avm_event_message *msg,
			struct _event_node_ctrl *pctrl)
{
	uint32_t write_curr, read_curr, queue_len;
	unsigned long flags;
	int result;
	struct node_queue_data *psend_queue = &pctrl->send_queue;
	struct node_queue_data *precv_queue = &pctrl->recv_queue;
	unsigned int free;

	if (pctrl->node.state < NODE_ADDR_RCVD) {
		return -ENODEV;
	}

	if (msg == NULL) {
		return -EINVAL;
	}

	mb();

	spin_lock_irqsave(&psend_queue->lock, flags);

	flush_cache((void *)psend_queue->queue, sizeof(struct event_queue));
	flush_cache((void *)precv_queue->queue, sizeof(struct event_queue));

	queue_len = be32_to_cpu(precv_queue->queue->queue_len);
	read_curr = be32_to_cpu(psend_queue->queue->read_curr) % queue_len;
	write_curr = be32_to_cpu(precv_queue->queue->write_curr) % queue_len;

	free = (queue_len - 1 - write_curr + read_curr) % queue_len;
	if (free < pctrl->stats.recv_min_free){
		 pctrl->stats.recv_min_free = free;
	}

	if (read_curr != write_curr) {
		++read_curr;
		read_curr %= queue_len;

		memcpy_fromio(msg, &(precv_queue->queue->buff[read_curr]),
			      sizeof(struct avm_event_message));
		mb();

		psend_queue->queue->read_curr = cpu_to_be32(read_curr);
		mb();

		flush_cache((void *)&(psend_queue->queue->read_curr),
			    sizeof(psend_queue->queue->read_curr));
		mb();

		result = 0;
	} else {
		result = -EAGAIN;
	}

	spin_unlock_irqrestore(&psend_queue->lock, flags);

	return result;
}

/**
 */
static int send_msg(struct avm_event_message *msg,
		    struct _event_node_ctrl *pctrl)
{
	struct avm_event_message tmp_buff;
	int result;
	int length;

	msg->magic = 42;
	msg->length = sizeof(struct avm_event_message);

	/*--- dump_msg("pre_send_msg", msg); ---*/
	length = convert_fromMachine_toBigEndian(
		sizeof(struct avm_event_message),
		convert_message_struct_avm_event_message, (unsigned char *)msg,
		(unsigned char *)&tmp_buff, 0);
	/*--- dump_msg("post_send_msg", &tmp_buff); ---*/
	if (length > 0) {
		tmp_buff.length = cpu_to_be32(length);
		result = send_data(&tmp_buff, pctrl);
		return result;
	}
	DBG_ERR("%s: failed %d\n", __func__, length);
	return -EINVAL;
}

/**
 */
static int send_msg_sync(struct avm_event_message *msg,
			 struct _event_node_ctrl *pctrl)
{
	struct sync_result *sync_data;
	int result;
	unsigned long flags;
	struct node_data *pnode = &pctrl->node;

	might_sleep();

	sync_data = kzalloc(sizeof(struct sync_result), GFP_KERNEL);
	if (sync_data == NULL) {
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&(sync_data->list));
	sync_data->nonce = msg->nonce;
	sync_data->msg = msg;

	spin_lock_irqsave(&pnode->list_lock, flags);
	list_add(&(sync_data->list), &pnode->wait_list);
	spin_unlock_irqrestore(&pnode->list_lock, flags);

	result = send_msg(msg, pctrl);
	if (result != 0) {
		spin_lock_irqsave(&pnode->list_lock, flags);
		list_del(&(sync_data->list));
		spin_unlock_irqrestore(&pnode->list_lock, flags);
		kfree(sync_data);
		return result;
	}
	wmb();

	/*--- DBG_ERR("%s: wait_event_interruptible_timeout\n", __func__); ---*/
	result = wait_event_interruptible_timeout(
		pnode->sync_queue, sync_data->completed, SYNC_TIMEOUT);
	/*--- DBG_ERR("%s: wait_event_interruptible_timeout done result=%u\n", __func__, result); ---*/
	if (result == 0) {
		/*--- timeout ---*/
		DBG_ERR("[%s] Sync call timed out -> retrigger\n", __func__);
		send_irq(pctrl);
		result = wait_event_interruptible_timeout(pnode->sync_queue,
							  sync_data->completed,
							  SYNC_TIMEOUT / 2);
	}
	if (result <= 0) {
		// interrupted by signal or timeout
		DBG_ERR("[%s] Sync call %s\n", __func__,
			result ? "interrupted" : "timed out");
		result = -EINTR;
	} else {
		DBG_NOTE("[%s] Sync call returned\n", __func__);
		result = sync_data->result;
	}

	spin_lock_irqsave(&pnode->list_lock, flags);
	list_del(&(sync_data->list));
	spin_unlock_irqrestore(&pnode->list_lock, flags);
	kfree(sync_data);

	return result;
}

/**
 */
struct avm_event_message *recv_msg(struct avm_event_message *new_buff, struct _event_node_ctrl *pctrl)
{
	struct avm_event_message tmp_msg;
	int result;
	unsigned int length;

	result = receive_data(&tmp_msg, pctrl);
	if (result != 0) {
		if (result != -EAGAIN) {
			DBG_TRACE("[%s] receive_data() returned %d\n", __func__,
				  result);
		}
		return NULL;
	}
	if (tmp_msg.magic != 42 && tmp_msg.magic != be32_to_cpu(42)) {
		DBG_ERR("[%s] invalid message struct received. magic: 0x%x\n",
			__func__, tmp_msg.magic);
		dump_msg(__func__, &tmp_msg);
		return NULL;
	}

	memset(new_buff, 0x0, sizeof(*new_buff));

	length = __be32_to_cpu(tmp_msg.length);
	result = convert_fromBigEndian_toMachine(
		length, convert_message_struct_avm_event_message,
		(unsigned char *)&tmp_msg, (unsigned char *)new_buff, 0);

	if (result < 0) {
		DBG_ERR("[%s] invalid message struct received. magic: 0x%x\n",
			__func__, tmp_msg.magic);
		dump_msg(__func__, &tmp_msg);
		return NULL;
	}

	return new_buff;
}

/**
 */
static int handle_reply(struct avm_event_message *msg, struct node_data *pnode)
{
	unsigned long flags;
	struct sync_result *sync_data;

	if (msg->flags == EVENT_REPLY_NONE) {
		DBG_ERR("[%s] Message is not a reply!\n", __func__);
		dump_msg(__func__, msg);
		return -EIO;
	}

	spin_lock_irqsave(&pnode->list_lock, flags);
	list_for_each_entry(sync_data, &(pnode->wait_list), list) {
		if (sync_data->nonce == msg->nonce) {
			break;
		}
	}
	spin_unlock_irqrestore(&pnode->list_lock, flags);

	if (&(sync_data->list) == &(pnode->wait_list)) {
		/*--- DBG_ERR("[%s] no message waiting for reply - drop it (nonce = 0x%x)!\n", __func__, msg->nonce); ---*/
		return 0;
	}
	DBG_INFO("[%s] Got reply for nonce 0x%x\n", __func__, msg->nonce);

	if (msg->type != sync_data->msg->type) {
		DBG_ERR("[%s] Type of request and reply do not match.\n",
			__func__);
		dump_msg(__func__, msg);
		msg->flags = EVENT_REPLY_ERROR;
	}

	if (msg->flags == EVENT_REPLY_ERROR) {
		sync_data->result = -EIO;
	} else {
		sync_data->result = (int32_t)msg->result;
	}
	sync_data->completed = 1;

	wmb();
	wake_up(&pnode->sync_queue);

	return 0;
}
#endif /*--- #if defined(__KERNEL__) ---*/

/**
 */
struct _avm_event_node *
node_findhandle_by_source(struct _avm_event_source *source_handle)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(avm_event_nodes); ++i) {
		if (avm_event_nodes[i] &&
		    (avm_event_nodes[i]->source_handle == source_handle)) {
			return avm_event_nodes[i];
		}
	}
	return NULL;
}

/**
 */
int check_node_valid(struct _avm_event_node *node)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(avm_event_nodes); ++i) {
		if (avm_event_nodes[i] && (avm_event_nodes[i] == node)) {
			return 1;
		}
	}
	return 0;
}

/**
 */
static struct _avm_event_node *alloc_node(void)
{
	unsigned int i;
	struct _avm_event_node *new;

	new = kzalloc(sizeof(struct _avm_event_node), GFP_KERNEL);
	if (new == NULL) {
		return new;
	}
	for (i = 0; i < ARRAY_SIZE(avm_event_nodes); i++) {
		if (atomic_test_and_set((unsigned long *)&avm_event_nodes[i],
					(unsigned long)new)) {
			return avm_event_nodes[i];
		}
	}
	kfree(new);
	DBG_ERR("[%s] failed", __func__);
	return NULL;
}

/**
 */
static void free_node(struct _avm_event_node *tofree)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(avm_event_nodes); i++) {
		if (avm_event_nodes[i] == tofree) {
			avm_event_nodes[i] = NULL;
			kfree(tofree);
			return;
		}
	}
}

/**
 */
static void swap_handles(struct avm_event_message *msg)
{
	uint32_t temp;

	temp = msg->receiver_handle;
	msg->receiver_handle = msg->transmitter_handle;
	msg->transmitter_handle = temp;
}

/**
 * es kann sein, dass die Eventschnittstelle zum Startzeitpunkt noch nicht established ist,
 * da wir noch auf die Gegenseite warten
 * Hier kann man eine Callback installieren um das Register auf passenden Zeitpunkt zu verschieben
 */
int node_event_established_callback(
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2),
	void *context, unsigned int param1, unsigned int param2)
{
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	struct node_data *pnode = &pctrl->node;
	struct _node_establish_cb *ecb, *list, *last_el = NULL;

	if (event_established_cb == NULL) {
		return -EINVAL;
	}
	if (pnode->state == NODE_RUNNING) {
		/*--- node ready: direkter Aufruf der Callback ---*/
		event_established_cb(context, param1, param2);
		return 0;
	}
	ecb = kzalloc(sizeof(struct _node_establish_cb), GFP_KERNEL);
	if (ecb == NULL) {
		return -ENOMEM;
	}
	ecb->event_established_cb = event_established_cb;
	ecb->context = context;
	ecb->param1 = param1;
	ecb->param2 = param2;

	spin_lock(&cb_lock);
	list = pnode->first_cb;
	while (list) {
		last_el = list;
		list = list->next;
	}
	if (last_el == NULL) {
		pnode->first_cb = ecb;
	} else {
		last_el->next = ecb;
	}
	spin_unlock(&cb_lock);
	return 0;
}

/**
 */
static void _node_event_established_callback_progress(struct node_data *pnode,
						      int no_cb)
{
	struct _node_establish_cb *el = NULL;

	for (;;) {
		spin_lock(&cb_lock);
		el = pnode->first_cb;
		if (el) {
			pnode->first_cb = el->next;
		}
		spin_unlock(&cb_lock);
		if (el == NULL) {
			break;
		}
		if (no_cb == 0) {
			DBG_NOTE("[%s] call %pS(%p, %d, %d)\n", __func__,
				 el->event_established_cb, el->context,
				 el->param1, el->param2);
			el->event_established_cb(el->context, el->param1,
						 el->param2);
		}
		kfree(el);
	}
}

/**
 */
static void node_event_established_callback_progress(struct work_struct *work)
{
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	struct node_data *pnode = &pctrl->node;

	_node_event_established_callback_progress(pnode, 0);
}

/**
 */
void node_event_free_callback(struct node_data *pnode)
{
	_node_event_established_callback_progress(pnode, 1);
}

/**
 * wird von avm_event_remote_source_register aufgerufen, um Node anzulegen
 */
int node_source_register(struct _avm_event_source *source_handle, char *name,
			 struct _avm_event_id_mask *id_mask)
{
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	struct _avm_event_node *new_node;
	struct avm_event_message msg;
	int status;

	new_node = alloc_node();
	if (new_node == NULL) {
		DBG_ERR("%s: failed (no ressource)\n", __func__);
		return -ENOMEM;
	}
	new_node->source_handle = source_handle;

	DBG_INFO("%s: name=%s id_mask[0]=%llx new_node=%p\n", __func__, name,
		 id_mask->mask[0], new_node);

	memset(&msg, 0, sizeof(msg));
	msg.nonce = atomic_inc_return(&pctrl->node.nonce);
	msg.type = avm_event_source_register_type;
	memcpy(&msg.message.source_register.id_mask, id_mask,
	       sizeof(msg.message.source_register.id_mask));
	msg.transmitter_handle = (uint32_t)new_node;
	msg.receiver_handle = 0;
	strncpy(msg.message.source_register.name, name,
		sizeof(msg.message.source_register.name) - 1);

	status = send_msg_sync(&msg, pctrl);

	return status;
}

/**
 * wird von avm_event_remote_source_release aufgerufen, um Node abzumelden
 */
int node_source_unregister(void *node_handle,
			   struct _avm_event_id_mask *id_mask)
{
	struct _avm_event_node *node = (struct _avm_event_node *)node_handle;
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	struct avm_event_message msg;
	int result;

	DBG_INFO("%s node=%p\n", __func__, node_handle);
	if (node == NULL) {
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));
	msg.nonce = atomic_inc_return(&pctrl->node.nonce);
	msg.type = avm_event_source_unregister_type;
	memcpy(&msg.message.source_unregister.id_mask, id_mask,
	       sizeof(msg.message.source_unregister.id_mask));
	msg.message.source_unregister.name[0] = '\0';
	msg.transmitter_handle = (uint32_t)node;
	msg.receiver_handle = (uint32_t)node->remote_node_handle;

	result = send_msg_sync(&msg, pctrl);

	free_node(node);

	return result;
}

/**
 * sende Event zur anderen CPU
 */
int node_source_send(void *node_handle,
		     enum _avm_event_id event_id,
		     unsigned int data_len, void *data)
{
	struct _avm_event_node *node = (struct _avm_event_node *)node_handle;
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	struct avm_event_message msg;
	int result;

	if (node == NULL) {
		return -EINVAL;
	}
	DBG_INFO("%s node=%p %x\n", __func__, node_handle,
		 node->remote_node_handle);
	if (data_len > sizeof(msg.message)) {
		pr_err("%s: id=%s data_len(%u) exceed sizeof(struct avm_event_data)(%d) -> no remote-transfer\n",
		       __func__, get_enum__avm_event_id_name(event_id), data_len, sizeof(msg.message));
		return -EINVAL;
	}
	memset(&msg, 0, sizeof(msg));
	msg.nonce = atomic_inc_return(&pctrl->node.nonce);
	msg.type = avm_event_remote_source_trigger_request_type;
	msg.transmitter_handle = (uint32_t)node_handle;
	msg.receiver_handle = (uint32_t)node->remote_node_handle;
	msg.length = data_len;
	memcpy(&(msg.message.remote_source_trigger_request.data), data,
	       data_len);

	result = send_msg(&msg, pctrl);

	return result;
}

/**
 * sende Notifier zu anderen CPU
 */
int node_source_notifier(void *node_handle, enum _avm_event_id event_id)
{
	struct _avm_event_node *node = (struct _avm_event_node *)node_handle;
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	struct avm_event_message msg;
	int result;

	if (node == NULL) {
		return -EINVAL;
	}

	memset(&msg, 0x0, sizeof(msg));
	msg.nonce = atomic_inc_return(&pctrl->node.nonce);
	msg.type = avm_event_source_notifier_type;
	msg.transmitter_handle = (uint32_t)node;
	msg.receiver_handle = (uint32_t)node->remote_node_handle;
	msg.message.source_notifier.id = event_id;

	result = send_msg(&msg, pctrl);

	return result;
}

/**
 */
static uint8_t *clone_event_data(void *event_data, uint32_t data_len)
{
	uint8_t *data;

	if (event_data == NULL) {
		DBG_WARN("[%s] Called with msg == NULL\n", __func__);
		return NULL;
	}

	data = kzalloc(data_len, GFP_KERNEL);
	if (data == NULL) {
		DBG_WARN("[%s] Out of memory\n", __func__);
		return NULL;
	}
	memcpy(data, event_data, data_len);

	return data;
}

/**
 */
static void
handle_remote_source_register_request(struct avm_event_message *msg,
				      struct _event_node_ctrl *pctrl)
{
	struct _avm_event_node *new_node;
	int result = 0;

	new_node = alloc_node();
	if (new_node == NULL) {
		return;
	}

	DBG_INFO("%s new_node=%p\n", __func__, new_node);

	new_node->source_handle = avm_event_local_source_register(
		msg->message.source_register.name,
		&msg->message.source_register.id_mask,
		avm_event_remote_notifier_to_send, new_node);

	if (new_node->source_handle == NULL) {
		DBG_ERR("%s can't avm_event_local_source_register()\n",
			__func__);
		free_node(new_node);
		result = -ENODEV;
	} else {
		msg->receiver_handle = (uint32_t)new_node;
		DBG_INFO("%s remote: remark handle %x in node %p\n", __func__,
			 msg->transmitter_handle, new_node);
		new_node->remote_node_handle = msg->transmitter_handle;
	}

	msg->result = result;
	if (result == 0) {
		msg->flags |= EVENT_REPLY_SUCCESS;
	} else {
		msg->flags |= EVENT_REPLY_FAIL;
	}

	swap_handles(msg);

	result = send_msg(msg, pctrl);

	if (result != 0) {
		DBG_WARN("[%s] Error sending reply message\n", __func__);
	}
}

/**
 */
static void
handle_remote_source_unregister_request(struct avm_event_message *msg,
					struct _event_node_ctrl *pctrl)
{
	int result;
	struct _avm_event_node *node;

	DBG_INFO("%s rh=%x th=%x\n", __func__, msg->receiver_handle,
		 msg->transmitter_handle);

	node = (struct _avm_event_node *)msg->receiver_handle;
	if (check_node_valid(node) == 0) {
		DBG_ERR("%s invalid node in receiver_handle %p\n", __func__,
			node);
		return;
	}

	result = avm_event_local_source_release(node->source_handle);

	msg->result = result;
	if (result == 0) {
		msg->flags |= EVENT_REPLY_SUCCESS;
	} else {
		msg->flags |= EVENT_REPLY_FAIL;
	}

	swap_handles(msg);

	result = send_msg(msg, pctrl);

	if (result != 0) {
		DBG_WARN("[%s] Error sending reply message\n", __func__);
	}

	free_node(node);
}

/**
 * Event von der anderen CPU empfangen
 */
static void handle_remote_source_trigger_request(struct avm_event_message *msg,
						 struct _event_node_ctrl *pctrl)
{
	int result = -ENOMEM;
	uint8_t *evnt_data;
	unsigned int data_len;
	struct _avm_event_node *node;

	DBG_INFO("%s rh=%x th=%x\n", __func__, msg->receiver_handle,
		 msg->transmitter_handle);

	node = (struct _avm_event_node *)msg->receiver_handle;
	if (check_node_valid(node) == 0) {
		DBG_ERR("%s invalid node in receiver_handle %p\n", __func__,
			node);
		return;
	}
	data_len = msg->length;

	evnt_data = clone_event_data(
		&(msg->message.remote_source_trigger_request.data), data_len);

	if (evnt_data != NULL) {
		result = avm_event_local_source_trigger(
			node->source_handle,
			msg->message.remote_source_trigger_request.data.id,
			data_len -
				__builtin_offsetof(struct avm_event_message
						   __attribute__((packed)),
						   message) -
				__builtin_offsetof(
					struct avm_event_remote_source_trigger_request
					__attribute__((packed)),
					data),
			evnt_data);
	}
	msg->result = result;
	msg->flags &= ~(EVENT_REPLY_SUCCESS | EVENT_REPLY_FAIL);
	if (result >= 0) {
		msg->flags |= EVENT_REPLY_SUCCESS;
	} else {
		msg->flags |= EVENT_REPLY_FAIL;
	}
	swap_handles(msg);
	result = send_msg(msg, pctrl);

	if (result != 0) {
		DBG_WARN("[%s] Error sending reply message\n", __func__);
	}
}

/**
 * Notifier von anderen CPU empfangen
 */
static void
handle_remote_source_notifier_request(struct avm_event_message *msg,
				      struct _event_node_ctrl *pctrl)
{
	int result = 0;
	struct _avm_event_node *node;

	DBG_INFO("%s rh=%x th=%x\n", __func__, msg->receiver_handle,
		 msg->transmitter_handle);
	node = (struct _avm_event_node *)msg->receiver_handle;
	if (check_node_valid(node) == 0) {
		DBG_ERR("%s invalid node in receiver_handle %p\n", __func__,
			node);
		return;
	}
	avm_event_remote_notifier_request(node,
					  msg->message.source_notifier.id);

	msg->result = result;
	msg->flags &= ~(EVENT_REPLY_SUCCESS | EVENT_REPLY_FAIL);
	if (result == 0) {
		msg->flags |= EVENT_REPLY_SUCCESS;
	} else {
		msg->flags |= EVENT_REPLY_FAIL;
	}
	swap_handles(msg);
	result = send_msg(msg, pctrl);
}

/**
 */
static void handle_ping(struct avm_event_message *msg,
			struct _event_node_ctrl *pctrl)
{
	int result = 0;

	if (pctrl->node.state == NODE_PING_SENT) {
		pctrl->node.state = NODE_PING_RCVD;
	}

	msg->result = 0;
	msg->flags &= ~(EVENT_REPLY_SUCCESS | EVENT_REPLY_FAIL);
	msg->flags |= EVENT_REPLY_SUCCESS;

	swap_handles(msg);
	result = send_msg(msg, pctrl);

	if (result != 0) {
		DBG_WARN("[%s] Error sending reply message\n", __func__);
	}
}

/**
 */
static int send_ping(uint32_t seq, struct _event_node_ctrl *pctrl)
{
	int result = 0;
	struct avm_event_message msg;

	memset(&msg, 0, sizeof(msg));

	msg.nonce = atomic_inc_return(&pctrl->node.nonce);
	msg.type = avm_event_ping_type;
	msg.message.ping.seq = seq;

	result = send_msg(&msg, pctrl);

	if (result != 0) {
		DBG_ERR("[%s] Error sending ping message\n", __func__);
	}

	return result;
}

/**
 */
void avm_event_node_release_all(void)
{
	unsigned int i;
	int result;

	for (i = 0; i < ARRAY_SIZE(avm_event_nodes); ++i) {
		if (avm_event_nodes[i] && (avm_event_nodes[i]->source_handle)) {
			result = node_source_unregister(
				avm_event_nodes[i],
				&avm_event_nodes[i]->source_handle->event_mask);
			if (result != 0) {
				DBG_ERR("[%s] error releasing remote source %d",
					__func__, result);
			}
		}
	}
}

static struct event_tffs_handle *get_tffs_handle(uint32_t src_id,
						 struct _event_node_ctrl *pctrl)
{
	unsigned int i;
	struct event_tffs_handle *handle = NULL;

	for (i = 0; i < ARRAY_SIZE(pctrl->tffs_handles); ++i) {
		if (pctrl->tffs_handles[i].callback != NULL &&
		    pctrl->tffs_handles[i].id == src_id) {
			handle = &(pctrl->tffs_handles[i]);
		}
	}

	return handle;
}

/**
 */
void *avm_event_register_tffs(uint32_t id, avm_event_tffs_cb callback,
			      void *cb_data)
{
	unsigned int i;
	struct _event_node_ctrl *pctrl;
	struct event_tffs_handle *handle;
	unsigned long flags;

	pctrl = &event_node_ctrl;

	if (id == AVM_EVENT_TFFS_NODE_ANY || id == AVM_EVENT_TFFS_NODE_NONE) {
		DBG_TRACE("[%s] refusing to register tffs node with id 0x%x\n",
			  __func__, id);
		handle = ERR_PTR(-EINVAL);
		goto err_out;
	}

	spin_lock_irqsave(&pctrl->tffs_lock, flags);

	// check if id already registered
	handle = get_tffs_handle(id, pctrl);
	if (handle != NULL) {
		handle = ERR_PTR(-EEXIST);
		goto err_out;
	}
	// id not registered. Find empty slot and insert callback
	for (i = 0; i < ARRAY_SIZE(pctrl->tffs_handles); ++i) {
		if (pctrl->tffs_handles[i].callback == NULL) {
			handle = &(pctrl->tffs_handles[i]);
			handle->pctrl = pctrl;
			handle->id = id;
			handle->callback = callback;
			handle->cb_data = cb_data;
			break;
		}
	}

	spin_unlock_irqrestore(&pctrl->tffs_lock, flags);

err_out:
	return (void *)handle;
}
EXPORT_SYMBOL(avm_event_register_tffs);

int avm_event_tffs_call(void *priv, struct avm_event_tffs *call)
{
	int result;
	struct _event_node_ctrl *pctrl;
	struct event_tffs_handle *handle;

	BUG_ON(priv == NULL);
	handle = (struct event_tffs_handle *)priv;
	pctrl = handle->pctrl;

	result = 0;

	if (handle->id != call->src_id) {
		DBG_ERR("[%s] client 0x%x tried sending call with false id 0x%x\n",
			__func__, handle->id, call->src_id);
		result = -EINVAL;
		goto err_out;
	}

	memset(&handle->send_msg, 0x0, sizeof(handle->send_msg));
	memcpy(&handle->send_msg.message.tffs, call, sizeof(*call));
	handle->send_msg.type = avm_event_tffs_type;
	handle->send_msg.nonce = atomic_inc_return(&pctrl->node.nonce);

	result = send_msg(&handle->send_msg, pctrl);

err_out:
	return result;
}
EXPORT_SYMBOL(avm_event_tffs_call);

/**
 */
static void handle_tffs_call(struct avm_event_message *msg,
			     struct _event_node_ctrl *pctrl)
{
	int result;
	struct avm_event_tffs *call;
	struct event_tffs_handle *handle;
	unsigned int i;

	BUG_ON(msg->type != avm_event_tffs_type);

	call = &msg->message.tffs;

	// TODO: prevent removing of handles while looping over callbacks
	for (i = 0; i < ARRAY_SIZE(pctrl->tffs_handles); ++i) {
		handle = &(pctrl->tffs_handles[i]);
		if (handle->callback == NULL) {
			continue;
		}

		DBG_TRACE("[%s] src_id: 0x%x dst_id: 0x%x handle_id: 0x%x\n",
			  __func__, call->src_id, call->dst_id, handle->id);
		if (call->dst_id == handle->id ||
		    call->dst_id == AVM_EVENT_TFFS_NODE_ANY) {
			result = handle->callback(handle->cb_data, call);
			if (result == 0 || call->dst_id == handle->id) {
				break;
			}
		}
	}
}

/**
 */
#if defined(__KERNEL__)
static
#endif /*--- #if defined(__KERNEL__) ---*/
int msg_poll(struct _event_node_ctrl *pctrl)
{
	struct avm_event_message msg_buf, *msg;
	int result = 0;
	unsigned int quota = EVENT_QUEUE_LEN;

	while (quota--) {
		msg = recv_msg(&msg_buf, pctrl);
		if (msg == NULL) {
			break;
		}

		if (msg->flags == EVENT_REPLY_NONE) {
			switch (msg->type) {
			case avm_event_ping_type:
				handle_ping(msg, pctrl);
				break;
			case avm_event_source_register_type:
				handle_remote_source_register_request(msg,
								      pctrl);
				break;
			case avm_event_source_unregister_type:
				handle_remote_source_unregister_request(msg,
									pctrl);
				break;
			case avm_event_source_notifier_type:
				handle_remote_source_notifier_request(msg,
								      pctrl);
				break;
			case avm_event_remote_source_trigger_request_type:
				handle_remote_source_trigger_request(msg,
								     pctrl);
				break;
			case avm_event_tffs_type:
				handle_tffs_call(msg, pctrl);
				break;
			default:
				DBG_ERR("[%s] Received unknown event type 0x%x.\n",
					__func__, msg->type);
				dump_msg(__func__, msg);
				break;
			}
		} else {
			if (msg->flags & EVENT_REPLY_SUCCESS) {
				struct _avm_event_node *node;

				switch (msg->type) {
				case avm_event_ping_type:
					DBG_TRACE("[%s] ping reply received\n",
						  __func__);
					pctrl->last_pong = jiffies;
					break;
				case avm_event_source_register_type:
					/*--- reply register: remark the remote-handle for next operation ---*/
					node = (struct _avm_event_node *)
						       msg->receiver_handle;
					if (check_node_valid(node) == 0) {
						DBG_ERR("%s invalid node in receiver_handle %p\n",
							__func__, node);
						break;
					}
					DBG_INFO(
						"%s host: remark handle %x in node %p\n",
						__func__,
						msg->transmitter_handle, node);
					node->remote_node_handle =
						msg->transmitter_handle;
					break;
				default:
					break;
				}
			}
			result = handle_reply(msg, &pctrl->node);
		}

		if (result != 0) {
			break;
		}
	}

	return result;
}

/**
 */
static inline char *state_to_str(enum node_state state)
{
	return state == NODE_ERROR ?  "ERROR" :
		state == NODE_INIT ?  "INIT" :
		state == NODE_ADDR_SENT ?  "ADDR_SENT" :
		state == NODE_ADDR_RCVD ?  "ADDR_RCVD" :
		state == NODE_PING_SENT ?  "PING_SENT" :
		state == NODE_PING_RCVD ?  "PING_RCVD" :
		state == NODE_RUNNING   ?  "RUNNING" :
		state == NODE_STALLED   ? "STALLED" : "UNKNOWN";
}

#if defined(__KERNEL__)
/**
 */
void change_state(struct node_data *my_node, enum node_state state)
{
	DBG_NOTE("[%s] %s -> %s\n", __func__, state_to_str(my_node->state),
		 state_to_str(state));
	my_node->state = state;
}

/**
 */
enum node_state node_action(struct _event_node_ctrl *pctrl)
{
	int result;
	uint32_t addr;

	DBG_NOTE("[%s] %s\n", __func__, state_to_str(pctrl->node.state));
	switch (pctrl->node.state) {
	case NODE_INIT:
		addr = cpu_to_be32(pctrl->send_queue.phys_base);
#if defined(CONFIG_X86)
		result = npcpu_appcpu_mbx_send_notification(
			APPCPU_EVENT_AVMEVENT, &addr);
#elif defined(REMOTE_EVENT_PUMA7_ARM)
		result = arm_atom_mbx_send_notification_over_hw_mbox(
			ARM11_EVENT_AVMEVENT, &addr);
#elif defined(REMOTE_EVENT_PUMA6_ARM)
		result = arm_atom_mbx_send_notification(ARM11_EVENT_AVMEVENT,
							&addr);
#else
#warning REMOTE_... not defined
#endif
		if (result == 0) {
			change_state(&pctrl->node, NODE_ADDR_SENT);
		} else {
#if defined(CONFIG_MACH_PUMA7)
			/* Wait for other CPU or initialisation of CE_Mailbox.
			 * @result is -1 for (hot) reboot (see JZ-69958),
			 * otherwise EBUSY, -EBUSY or -ENODEV.
			 */
			if (result == -1 ||
			    result == -EBUSY || result == EBUSY ||
			    result == -ENODEV)
				break;
#endif
			change_state(&pctrl->node, NODE_ERROR);
		}
		break;
	case NODE_ADDR_SENT:
#if defined(CONFIG_X86)
		result = npcpu_appcpu_mbx_check_event_notification(
			NPCPU_EVENT_AVMEVENT, &addr);
		addr -= ARM_MEM_OFFSET;
#elif defined(REMOTE_EVENT_PUMA6_ARM)
		result = arm_atom_mbx_receive_event_notification_noblock(
			ATOM_EVENT_AVMEVENT, &addr);
		addr += ARM_MEM_OFFSET;
#elif defined(REMOTE_EVENT_PUMA7_ARM)
		result = arm_atom_mbx_get_Last_Message_of_Event(
			ATOM_EVENT_AVMEVENT, &addr);
		addr += ARM_MEM_OFFSET;
#endif
		if (result == 0) {
			result = setup_recvqueue((resource_size_t)addr,
						 pctrl);
			if (result == 0) {
				change_state(&pctrl->node, NODE_ADDR_RCVD);
			}
		} else if (result != -EAGAIN) {
			DBG_ERR("[%s] unable to use receive queue!\n",
				__func__);
			change_state(&pctrl->node, NODE_ERROR);
		}
		break;
	case NODE_ADDR_RCVD:
		result = send_ping(atomic_read(&pctrl->node.nonce), pctrl);
		if (result == 0) {
			change_state(&pctrl->node, NODE_PING_SENT);
			pctrl->last_ping = jiffies;
			pctrl->last_pong = jiffies;
		} else {
			DBG_ERR("[%s] Can't send ping!\n", __func__);
			change_state(&pctrl->node, NODE_ERROR);
		}
		break;
	case NODE_PING_RCVD:
		change_state(&pctrl->node, NODE_RUNNING);
		DBG_ERR("[%s] event-node interface established\n", __func__);
		schedule_work(
			&cb_nodes_work); /*--- established - schedule callbacks ---*/
		/* fall-through */
	case NODE_PING_SENT:
	case NODE_RUNNING:
#if defined(REMOTE_EVENT_PUMA7_X86)
#if defined(                                                                   \
	CONFIG_AVM_WATCHDOG) // Funktion bisher nur im watchdog-feature-kernel enthalten. TODO kann sonst raus
		//Event Node stoppen wenn HWMailbox down
		if (!npcpu_appcpu_mbx_ready()) {
			change_state(&pctrl->node, NODE_ERROR);
			break;
		}
#endif
#endif
		result = msg_poll(pctrl);
		if (result != 0) {
			change_state(&pctrl->node, NODE_ERROR);
			break;
		}

		if (time_is_before_jiffies(pctrl->last_ping + 10 * HZ)) {
			if (time_after(pctrl->last_ping, pctrl->last_pong)) {
				DBG_ERR("Did not receive reply for last ping, link may be dead.\n");
			}
			result = send_ping(atomic_read(&pctrl->node.nonce),
					   pctrl);
			if (result != 0) {
				change_state(&pctrl->node, NODE_ERROR);
			}
			pctrl->last_ping = jiffies;
		}
		break;
	default:
		DBG_ERR("[%s] Invalid node state: 0x%x\n", __func__,
			pctrl->node.state);
		change_state(&pctrl->node, NODE_ERROR);
	}
	return pctrl->node.state;
}

/**
 */
static int event_node_thread(void *data)
{
	struct _event_node_ctrl *pctrl = (struct _event_node_ctrl *)data;
	enum node_state state;
	int timeout;

	while (!kthread_should_stop()) {
		state = node_action(pctrl);
		if (state == NODE_ERROR)
			panic("AVM remote event node stopped in error state\n");

		timeout = wait_event_interruptible_timeout(
			pctrl->wait_queue,
			test_and_clear_bit(EVENT_WORK_BIT_RUN, &pctrl->trigger),
			HZ);
		if (timeout == -ERESTARTSYS) {
			/* interrupted by signal -> exit */
			break;
		}
	}

	DBG_INFO("[%s] done\n", __func__);
	pctrl->kthread = NULL;
	return 0;
}

/**
 */
static void event_node_trigger(struct _event_node_ctrl *pctrl)
{
	if (pctrl->kthread) {
		set_bit(EVENT_WORK_BIT_RUN, &pctrl->trigger);
		wake_up_interruptible_sync(&pctrl->wait_queue);
	}
}

/**
 * remote-cpu queued event -> wakeup with event_node_thread
 */
int avm_event_node_trigger(int irq, void *ref)
{
	struct _event_node_ctrl *pctrl = (struct _event_node_ctrl *)ref;

	event_node_trigger(pctrl);
	return 0;
}

/**
 */
static void __exit event_node_thread_exit(struct _event_node_ctrl *pctrl)
{
	if (pctrl->kthread) {
		kthread_stop(pctrl->kthread);
	}
}

/**
 */
static void __init event_node_thread_init(struct _event_node_ctrl *pctrl)
{
	DBG_NOTE("[%s] Establish event-link\n", __func__);
	init_waitqueue_head(&pctrl->wait_queue);
	pctrl->kthread =
		kthread_run(event_node_thread, pctrl, "avm_event_node");
	BUG_ON((pctrl->kthread == NULL) || IS_ERR(pctrl->kthread));
}

/**
 */
static void event_node_init(struct _event_node_ctrl *pctrl)
{
	struct node_data *pnode = &pctrl->node;

	spin_lock_init(&pctrl->tffs_lock);
	memset(&pctrl->tffs_handles[0], 0x0, sizeof(pctrl->tffs_handles));

	init_waitqueue_head(&pnode->sync_queue);
	spin_lock_init(&pnode->list_lock);
	INIT_LIST_HEAD(&pnode->wait_list);
	atomic_set(&pnode->nonce, 0);

#if defined(CONFIG_X86)
	// debugging, make it easier to distinguish nodes
	atomic_set(&pnode->nonce, 42);
#endif
}

const struct event_node_stats *event_node_get_stats(void)
{
	return &event_node_ctrl.stats;
}

/**
 */
static void __exit event_node_exit(struct node_data *pnode)
{
	if (pnode->state == NODE_RUNNING) {
		avm_event_node_release_all();
	}
	pnode->state = NODE_SHUTDOWN;
	node_event_free_callback(pnode);
}

/**
 * remove module
 */
static void __exit node_exit(void)
{
	struct _event_node_ctrl *pctrl = &event_node_ctrl;

	free_remotecpu_irqhandler(AVM_EVENT_REMOTECPU_IRQ, pctrl);
	DBG_NOTE("%s()\n", __func__);
	event_node_thread_exit(pctrl);
	event_node_exit(&pctrl->node);
	free_sendqueue(&pctrl->send_queue);
	free_recvqueue(&pctrl->recv_queue);
}

/**
 */
static int __init node_init(void)
{
	struct _event_node_ctrl *pctrl = &event_node_ctrl;
	int result;

	DBG_NOTE("%s()\n", __func__);

	result = request_remotecpu_irqhandler(AVM_EVENT_REMOTECPU_IRQ,
					      avm_event_node_trigger, pctrl);
	if (result < 0) {
		return result;
	}
	change_state(&pctrl->node, NODE_INIT);
	result = setup_sendqueue(&pctrl->send_queue);
	if (result) {
		return result;
	}
	pctrl->stats.send_len = EVENT_QUEUE_LEN;
	pctrl->stats.send_min_free = EVENT_QUEUE_LEN;
	event_node_init(pctrl);
	event_node_thread_init(pctrl);
	DBG_NOTE("%s() success\n", __func__);
	return 0;
}

#if defined(CONFIG_AVM_EVENTNODE_PUMA7)
subsys_initcall_sync(node_init);
#else /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA7) ---*/
module_init(node_init);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA7) ---*/
module_exit(node_exit);

#endif /*--- #if defined(__KERNEL__) ---*/
