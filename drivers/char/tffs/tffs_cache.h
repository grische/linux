/*
 * tffs_cache.h
 *
 *  Created on: 10 Nov 2014
 *      Author: tklaassen
 */

#ifndef DRIVERS_CHAR_TFFS_3_0_TFFS_CACHE_H_
#define DRIVERS_CHAR_TFFS_3_0_TFFS_CACHE_H_

#include "tffs_local.h"

struct tffs_cache_entry;

struct tffs_cache_segment {
    struct list_head segment_list;
    struct tffs_cache_entry *entry;
    size_t len;
    char *data;
};

struct tffs_cache_entry {
    struct list_head entry_list;
    struct list_head segment_list;
    struct kref refcnt;
    enum _tffs_id id;
    uint32_t cache_seq;
    size_t max_seg_size;
};

struct tffs_cache_ctx;
struct tffs_cache_handle {
    struct tffs_cache_ctx *ctx;
    enum _tffs_id id;
    struct tffs_cache_entry *entry;
    struct tffs_cache_segment *segment;
    void *backend_handle;
    enum tffs3_handle_mode mode;
    size_t seg_offset;
    size_t total_offset;
};

struct tffs_cache_ctx {
    struct tffs_module *backend;
    struct list_head entry_list;
    struct list_head clear_list;
    spinlock_t list_lock;
    spinlock_t clear_lock;
    bool ram_only;
    void *notify_priv;
    tffs3_notify_fn notify_cb;
    volatile unsigned long panic_mode;
    struct tffs_cache_handle panic_handle;
    void *buf;
};


#endif /* DRIVERS_CHAR_TFFS_3_0_TFFS_CACHE_H_ */
