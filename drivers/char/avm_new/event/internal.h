/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __avm_event_intern_h__
#define __avm_event_intern_h__

#if !defined(__KERNEL__)
#include "test_event.h"
#endif /*--- #if !defined(__KERNEL__) ---*/

/**
 * helper-functions
 */
extern spinlock_t avm_event_lock;

/**
 */
extern struct semaphore avm_event_sema;

/**
 */
static inline int atomic_test_and_set(unsigned long *ptr, unsigned long value)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&avm_event_lock, flags);
	if (*ptr == 0) {
		*ptr = value;
		ret = 1;
	}
	spin_unlock_irqrestore(&avm_event_lock, flags);
	return ret;
}

/**
 */
static inline void atomic_add_value(unsigned int *addr, signed int value)
{
	unsigned long flags;

	spin_lock_irqsave(&avm_event_lock, flags);
	*addr += value;
	spin_unlock_irqrestore(&avm_event_lock, flags);
}

#if defined(__KERNEL__)

/**
 */
struct _avm_event {
	dev_t device;
	struct cdev *cdev;
	struct class *osclass;
};
#endif /*--- #if defined(__KERNEL__) ---*/

/**
 */
struct _avm_event_open_data {
	volatile struct _avm_event_open_data *next;
	volatile struct _avm_event_open_data *prev;
	unsigned int registered;
	struct _avm_event_id_mask event_mask_registered;
	volatile struct _avm_event_item *item;
	char Name[MAX_EVENT_CLIENT_NAME_LEN + 1];
	wait_queue_head_t wait_queue;
#if defined(__KERNEL__)
	struct fown_struct *pf_owner;
	struct fasync_struct *fasync;
#endif /*--- #if defined(__KERNEL__) ---*/
	void *event_source_handle;
	unsigned int receive_count;
	unsigned int queue_count;
	/**
	 * Senke aus dem Kernelkontext
	 */
	void (*event_received)(void *h, unsigned char *buf, unsigned int len);
	void *context;
	atomic_t busy;
	volatile struct workqueue_struct *pwq_sink_handle;
	struct work_struct wq_sink;
};

struct _avm_event_data {
	struct _avm_event_data *debug; /*--- verkettung zu debug zwecken ---*/
	atomic_t link_count;
	void *data;
	unsigned int data_length;
};

struct _avm_event_item {
	struct _avm_event_data *debug; /*--- verkettung zu debug zwecken ---*/
	volatile struct _avm_event_item
		*next; /*--- -1 wenn objekt in free queue ---*/
	struct _avm_event_data *data;
};

extern volatile struct _avm_event_item
	*avm_event_first_Item; /*--- verkettung zu debug zwecken ---*/
extern volatile struct _avm_event_data
	*avm_event_first_Data; /*--- verkettung zu debug zwecken ---*/

#define AVM_EVENT_SIGNATUR (unsigned long long)(0x544E56455F4D5641ULL)
struct _avm_event_source {
	unsigned long long signatur;
	struct _avm_event_id_mask event_mask;
	struct _avm_event_id_mask event_mask_registered;
	char Name[MAX_EVENT_SOURCE_NAME_LEN + 1];
	void (*notify)(void *h, enum _avm_event_id);
	void *Context;
	unsigned int send_count;
};

struct _avm_event_node {
	struct _avm_event_source *source_handle; /*--- lokales event-Handle ---*/
	uint32_t remote_node_handle; /*--- remote-Node-Handle (beim reply uebermittelt) ---*/
	char name[MAX_EVENT_NODE_NAME_LEN];
	unsigned int synced;
};

struct event_node_stats {
	unsigned int recv_len;
	unsigned int recv_min_free;
	unsigned int send_len;
	unsigned int send_min_free;
	unsigned int dropped;
};

#define AVM_EVENT_MASK_BITS (sizeof(avm_event_mask_fieldentry) * 8)
#define AVM_EVENT_MASK_BITMASK (AVM_EVENT_MASK_BITS - 1)

#define AVM_EVENT_MASK_IDX(id) ((id) / AVM_EVENT_MASK_BITS)
#define AVM_EVENT_MASK_BIT(id) ((id)&AVM_EVENT_MASK_BITMASK)
/**
 */
static inline int check_id_mask_with_id(struct _avm_event_id_mask *id_mask,
					enum _avm_event_id id)
{
	if (id >= avm_event_last) {
		pr_err("%s:fatal error: illegal event_id: %d\n", __func__, id);
		return 0;
	}
	return (id_mask->mask[AVM_EVENT_MASK_IDX(id)] &
		((avm_event_mask_fieldentry)1 << AVM_EVENT_MASK_BIT(id))) ?
		       1 :
		       0;
}

/**
 * avm_event_node_puma6.c
 */
int node_source_register(struct _avm_event_source *source_handle, char *name,
			 struct _avm_event_id_mask *id_mask);
int node_source_unregister(void *node_handle,
			   struct _avm_event_id_mask *id_mask);
int node_source_send(void *node_handle, enum _avm_event_id event_id,
		     unsigned int data_len, void *data);
int node_source_notifier(void *node_handle, enum _avm_event_id event_id);
struct _avm_event_node *
node_findhandle_by_source(struct _avm_event_source *source_handle);
int node_event_established_callback(
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2),
	void *context, unsigned int param1, unsigned int param2);

#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)
const struct event_node_stats *event_node_get_stats(void);
#else
static inline const struct event_node_stats *event_node_get_stats(void)
{
	return NULL;
}
#endif
/**
 */
extern int avm_event_register(struct _avm_event_open_data *open_data,
			      struct _avm_event_cmd_param_register *data);
extern int avm_event_release(struct _avm_event_open_data *open_data,
			     struct _avm_event_cmd_param_release *data);
extern int avm_event_trigger(struct _avm_event_open_data *open_data,
			     struct _avm_event_cmd_param_trigger *data);
extern int avm_event_get(struct _avm_event_open_data *open_data,
			 unsigned char **rx_buffer,
			 unsigned int *rx_buffer_length,
			 unsigned int *event_pos);
extern void avm_event_commit(struct _avm_event_open_data *open_data,
			     unsigned int event_pos);
extern int avm_event_source_trigger_one(struct _avm_event_open_data *open_data,
					struct _avm_event_data *D);
void avm_event_source_notify(enum _avm_event_id id);

int avm_event_init(void);
int avm_event_init2(unsigned int max_items, unsigned int max_datas);
int avm_event_deinit2(void);
struct _avm_event_data *avm_event_alloc_data(void);
void avm_event_free_data(struct _avm_event_data *D);
struct _avm_event_item *avm_event_alloc_item(void);
void avm_event_free_item(struct _avm_event_item *I);
void avm_event_proc_init(void);
void avm_event_proc_exit(void);
#endif /*--- #ifndef __avm_event_intern_h__ ---*/
