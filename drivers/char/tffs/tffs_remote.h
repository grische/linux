/*
 * tffs_remote.h
 *
 *  Created on: 16 Oct 2014
 *      Author: tklaassen
 */

#ifndef _TFFS_REMOTE_H_
#define _TFFS_REMOTE_H_

//FIXME: put these in proper include files
typedef int (*avm_event_tffs_cb)(void *priv, struct avm_event_tffs *tffs_data);
extern void * avm_event_register_tffs(uint32_t id, avm_event_tffs_cb callback, void *cb_data);
extern int avm_event_tffs_call(void *priv, struct avm_event_tffs *call);
extern int avm_event_node_established(
        void (*event_established_cb)(void *private, unsigned int param1, unsigned int param2),
        void *private, unsigned int param1, unsigned int param2);

enum tffs_remote_state {
    tffs_remote_none,
    tffs_remote_setup,
    tffs_remote_wait_node,
    tffs_remote_wait_server,
    tffs_remote_running,
    tffs_remote_shutdown,
    tffs_remote_error,
};

enum tffs_srv_state {
    tffs_srv_none,
    tffs_srv_setup,
    tffs_srv_wait_node,
    tffs_srv_running,
    tffs_srv_shutdown,
    tffs_srv_error,
};

enum tffs_conn_state {
    tffs_conn_init,
    tffs_conn_wait_open,
    tffs_conn_intr_open,
    tffs_conn_open,
    tffs_conn_wait_data,
    tffs_conn_wait_close,
    tffs_conn_intr_close,
    tffs_conn_closed,
    tffs_conn_err,
};

struct tffs_remote_message {
    struct list_head msg_list;
    struct avm_event_tffs msg;
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
    struct tffs_remote_clnt* clnt;
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

struct tffs_server_conn {
    struct tffs_remote_ctx *ctx;
    enum _tffs_id id;
    struct kref refcnt;
    struct list_head conn_list;
    struct list_head msg_list;
    spinlock_t msg_lock;
    struct semaphore lock;
    struct tffs_remote_clnt* clnt;
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
    void                    *notify_priv;
    tffs3_notify_fn         notify_cb;

};

struct tffs_srv_ctx {
    unsigned int        node_id;
    struct semaphore   lock;
    unsigned int       in_panic_mode;
    wait_queue_head_t   wait_queue;
    enum tffs_srv_state state;
    void                *node_priv;
    unsigned long       events;
    spinlock_t          req_lock;
    struct task_struct  *kthread;
    struct list_head    req_list;
    struct list_head    clnt_list;
    struct avm_event_tffs send_msg;
    atomic_t            handle_seq;
};



#endif /* _TFFS_REMOTE_H_ */
