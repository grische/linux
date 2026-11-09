// SPDX-License-Identifier: GPL-2.0+
#define spinlock_t int
#define ctl_table int
#define DEFINE_SPINLOCK(a)
#define spin_lock_irqsave(a, b)
#define spin_unlock_irqrestore(a, b)
#define spin_lock(a)
#define spin_unlock(a)
#define printk(args...) printf("[%d]", getpid()), printf(args)
#define KERN_ERR ""
#define KERN_CONT ""
#define KERN_INFO ""
#define KERN_WARNING ""
#define KERN_NOTICE ""
#define be32_to_cpu(a) (a)
#define LOCAL_LOCK()
#define LOCAL_UNLOCK()

#define CONFIG_AVM_EVENTNODE_PUMA6

#define __init
#define kmalloc(a, b) malloc(a)
#define kzalloc(a, b) calloc(1, a)
#define vmalloc(a) malloc(a)
#define kfree(a) free(a)
#define vfree(a) free(a)
#define schedule()
#define EXPORT_SYMBOL(a)
#define GFP_KERNEL 0

typedef unsigned int dma_addr_t;

typedef unsigned int atomic_t;
#define atomic_inc_return(a) (*(a) += 1)
struct _event_node_ctrl;
/*--- dummy workqueue-stuff ---*/
typedef unsigned int wait_queue_head_t;

struct work_struct {
	void (*work_queue_cb)(struct work_struct *work);
};
struct workqueue_struct {
	unsigned int dummy;
};
#define WQ_MEM_RECLAIM 0
#define alloc_workqueue(a, b, c) (void *)1
#define destroy_workqueue(a)
#define queue_work(a, b)                                                       \
	do {                                                                   \
		if ((b)->work_queue_cb)                                        \
			(b)->work_queue_cb(b);                                 \
	} while (0)
#define INIT_WORK(a, b)                                                        \
	((a)->work_queue_cb = (b))

#define wake_up(a)
#define LINUX_VERSION_CODE ((2 << 16) + (6 << 8) + 39)
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
struct completion {
	unsigned int dummy;
};
struct list_head {
	unsigned int dummy;
};
struct resource {
	unsigned int dummy;
};

static struct _dummy_current {
	char *comm;
} _current;
static struct _dummy_current *current = &_current;

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member)                                        \
	({                                                                     \
		const typeof(((type *)0)->member) *__mptr = (ptr);             \
		(type *)((char *)__mptr - offsetof(type, member));             \
	})
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avm/event/event.h>
struct node_data;
extern int send_msg_sync(struct avm_event_message *msg,
			 struct _event_node_ctrl *pctrl);
extern int send_msg(struct avm_event_message *msg,
		    struct _event_node_ctrl *pctrl);
extern int handle_reply(struct avm_event_message *msg, struct node_data *pnode);
