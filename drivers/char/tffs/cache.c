// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
/*
 * tffs_cache.c
 *
 *  Created on: 10 Nov 2014
 *      Author: tklaassen
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <avm/tffs/tffs.h>
#include "local.h"
#include "cache.h"

#define PANIC_USE_BIT	 0
#define PANIC_USE	 (1 << PANIC_USE_BIT)
#define RAM_MAX_SEG_SIZE (0x1000UL) // 4kB

static void put_entry(struct tffs_cache_ctx *ctx, struct tffs_cache_entry *entry);

static void release_entry(struct kref *refcnt)
{
	struct tffs_cache_entry *entry;
	struct tffs_cache_segment *segment, *tmp;

	entry = container_of(refcnt, struct tffs_cache_entry, refcnt);

	// pr_err("[%s] id: 0x%x ptr: %p\n", __func__, entry->id, entry);

	list_for_each_entry_safe(segment, tmp, &entry->segment_list, segment_list) {
		list_del_init(&segment->segment_list);
		if (segment->data != NULL) {
			kfree(segment->data);
		}
		kfree(segment);
	}

	kfree(entry);
}

static struct tffs_cache_entry *fetch_entry(struct tffs_cache_ctx *ctx, enum _tffs_id id)
{
	struct tffs_cache_entry *entry, *tmp_entry;
	struct tffs_cache_segment *segment;
	struct tffs_core_handle core_handle;
	void *backend_handle;
	uint8_t *buff;
	size_t read_len;
	long result;
	unsigned long flags;

	backend_handle = NULL;
	entry	       = NULL;
	buff	       = NULL;
	result	       = 0;

	/* if we get here in RAM-only mode, the entry does not exist */
	if (ctx->ram_only == true) {
		result = -ENOENT;
		goto err_out;
	}

	memset(&core_handle, 0x0, sizeof(core_handle));
	core_handle.id	 = id;
	core_handle.mode = tffs3_mode_read;

	backend_handle = ctx->backend->open(ctx->backend, &core_handle);
	if (IS_ERR_OR_NULL(backend_handle)) {
		result = (backend_handle == NULL) ? -ENOMEM : PTR_ERR(backend_handle);
		goto err_out;
	}

	if (ctx->buf != NULL && ksize(ctx->buf) < core_handle.max_segment_size) {
		pr_warn("[TFFS_Cache] cached segment buffer is to small (needed=%d, provided=%d). Reallocating\n",
			core_handle.max_segment_size, ksize(ctx->buf));
		kfree(ctx->buf);
		ctx->buf = NULL;
	}

	if (ctx->buf == NULL) {
		pr_info("[TFFS_Cache] Allocate segement buffer cache (size=%d)\n",
			core_handle.max_segment_size);
		ctx->buf = kmalloc(core_handle.max_segment_size, GFP_KERNEL);
	}

	buff = ctx->buf;
	if (buff == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	INIT_LIST_HEAD(&entry->entry_list);
	INIT_LIST_HEAD(&entry->segment_list);
	kref_init(&entry->refcnt); // refcnt == 1 -> held by handle
	entry->id	    = id;
	entry->max_seg_size = core_handle.max_segment_size;

	do {
		read_len = core_handle.max_segment_size;
		result	 = ctx->backend->read(ctx->backend, backend_handle, buff, &read_len);
		if (result == -ENOENT) {
			/* special case, cleared entry. We can not return -ENOENT now,
			 * because traditionally this is only returned on first read.
			 * Stupid backward compatibility cruft... */
			result = 0;
			break;
		}

		if (result != 0) {
			goto err_out;
		}

		segment = kzalloc(sizeof(*segment), GFP_KERNEL);
		if (segment == NULL) {
			result = -ENOMEM;
			goto err_out;
		}

		if (read_len > 0) {
			segment->data = kmalloc(read_len, GFP_KERNEL);
			if (segment->data == NULL) {
				kfree(segment);
				result = -ENOMEM;
				goto err_out;
			}
			memcpy(segment->data, buff, read_len);
		}

		INIT_LIST_HEAD(&segment->segment_list);
		segment->len = read_len;
		list_add_tail(&segment->segment_list, &entry->segment_list);

	} while (read_len > 0);

	if (!list_empty(&entry->segment_list)) {
		/* if entry is to be cached, get another reference and put it on the list */
#if defined(CONFIG_TFFS_DEV_CACHE_ENV_ONLY) && !defined(CONFIG_TFFS_DEV_CACHE_RAMONLY)
		if (id > FLASH_FS_ID_FIRMWARE_CONFIG_LAST && id <= FLASH_FS_NAME_TABLE)
#endif
		{
			/* find list_head before which the new entry will be added. May point
			 * to the list_head in ctx, so dereferencing tmp_entry is only safe
			 * inside the loop  */
			spin_lock_irqsave(&ctx->list_lock, flags);

			kref_get(&entry->refcnt); // refcnt == 2 -> held by cache and handle

			tmp_entry =
				list_entry(&ctx->entry_list, struct tffs_cache_entry, entry_list);
			list_for_each_entry(tmp_entry, &ctx->entry_list, entry_list) {
				if (tmp_entry->id > entry->id) {
					break;
				}
			}
			list_add_tail(&entry->entry_list, &tmp_entry->entry_list);

			spin_unlock_irqrestore(&ctx->list_lock, flags);
		}
	}

err_out:
	if (!IS_ERR_OR_NULL(backend_handle)) {
		(void)ctx->backend->close(ctx->backend, backend_handle);
	}

	if (result != 0) {
		if (!IS_ERR_OR_NULL(entry)) {
			put_entry(ctx, entry);
		}
		entry = ERR_PTR(result);
	}

	return entry;
}

static struct tffs_cache_entry *get_entry(struct tffs_cache_ctx *ctx, enum _tffs_id id)
{
	struct tffs_cache_entry *entry, *tmp_entry;
	unsigned long flags;

	entry = NULL;
	spin_lock_irqsave(&ctx->list_lock, flags);
	list_for_each_entry(tmp_entry, &ctx->entry_list, entry_list) {
		if (tmp_entry->id == id) {
			entry = tmp_entry;
			kref_get(&entry->refcnt); // handle holds a reference now
			break;
		}
	}
	spin_unlock_irqrestore(&ctx->list_lock, flags);

	if (entry == NULL) {
		// fetch_entry tries to create a new cache entry. Its refcnt will be
		// either 1 (entry belongs only to handle) or 2 (entry is also in the
		// list of cached entries)
		entry = fetch_entry(ctx, id);
	}

	return entry;
}

static void put_entry(struct tffs_cache_ctx *ctx, struct tffs_cache_entry *entry)
{
	kref_put(&entry->refcnt, release_entry);
}

static void *TFFS3_CACHE_Open(struct tffs_module *this, struct tffs_core_handle *core_handle)
{
	struct tffs_cache_ctx *ctx;
	struct tffs_cache_entry *entry;
	struct tffs_cache_handle *handle;
	void *backend_handle;
	int result;

	ctx = (struct tffs_cache_ctx *)this->priv;
	BUG_ON(ctx == NULL);

	//    pr_err("[%s] Called\n", __func__);

	backend_handle = NULL;
	handle	       = NULL;
	entry	       = NULL;
	result	       = 0;

#if 0
	pr_err("[%s-%d] Called. mode: %s\n", __func__, __LINE__,
			core_handle->mode == tffs3_mode_read  ? "read"  :
			core_handle->mode == tffs3_mode_write ? "write" :
			core_handle->mode == tffs3_mode_panic ? "panic" :
													"unknown");
#endif

	// for now pass through for non-read operations
	if (ctx->ram_only == false && core_handle->mode != tffs3_mode_read) {
		backend_handle = ctx->backend->open(ctx->backend, core_handle);
		if (IS_ERR_OR_NULL(backend_handle)) {
			// preserve error code returned by backend
			result = (backend_handle == NULL) ? -ENOMEM : PTR_ERR(backend_handle);
			goto err_out;
		}

		if (core_handle->mode == tffs3_mode_panic) {
			/* We are in panic mode, try to grab the special static handle */
			if (test_and_set_bit(PANIC_USE_BIT, &(ctx->panic_mode)) == 0) {
				handle = &(ctx->panic_handle);
				memset(handle, 0x0, sizeof(*handle));
			}
		} else {
			handle = kzalloc(sizeof(*handle), GFP_KERNEL);
		}
		if (handle == NULL) {
			result = -ENOMEM;
			goto err_out;
		}

		handle->ctx	       = ctx;
		handle->id	       = core_handle->id;
		handle->backend_handle = backend_handle;
		handle->mode	       = core_handle->mode;
		handle->entry	       = NULL;

		return handle;
	}

	if (core_handle->mode == tffs3_mode_read) {
		// prepare handle for cached read operations
		entry = get_entry(ctx, core_handle->id);
		if (IS_ERR_OR_NULL(entry)) {
			result = (entry == NULL) ? -ENOMEM : PTR_ERR(entry);
			goto err_out;
		}
	} else {
		/* this will only be used in RAM-only mode */
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (entry == NULL) {
			result = -ENOMEM;
			goto err_out;
		}

		INIT_LIST_HEAD(&entry->entry_list);
		INIT_LIST_HEAD(&entry->segment_list);
		kref_init(&entry->refcnt); // refcnt == 1 -> held by handle
		entry->max_seg_size = RAM_MAX_SEG_SIZE;
		entry->id	    = core_handle->id;
	}

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (handle == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	//    pr_err("[%s] max_seg_size: 0x%x\n", __func__, entry->max_seg_size);
	core_handle->max_segment_size = entry->max_seg_size;
	handle->ctx		      = ctx;
	handle->backend_handle	      = backend_handle;
	handle->id		      = core_handle->id;
	handle->mode		      = core_handle->mode;
	handle->entry		      = entry;
	handle->seg_offset	      = 0;
	handle->total_offset	      = 0;

	/* Segment list is empty for non-existing entries */
	if (!list_empty(&entry->segment_list)) {
		handle->segment = list_first_entry(&entry->segment_list, struct tffs_cache_segment,
						   segment_list);
	}

	return handle;

err_out:
	if (!IS_ERR_OR_NULL(backend_handle)) {
		// FIXME: handle error on close
		ctx->backend->close(ctx->backend, backend_handle);
	}

	if (!IS_ERR_OR_NULL(handle)) {
		kfree(handle);
	}

	if (!IS_ERR_OR_NULL(entry)) {
		put_entry(ctx, entry);
	}

	return ERR_PTR(result);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_CACHE_Close(struct tffs_module *this, void *handle)
{
	struct tffs_cache_ctx *ctx;
	struct tffs_cache_handle *my_handle;
	int result;

	//    pr_err("[%s] Called\n", __func__);

	ctx = (struct tffs_cache_ctx *)this->priv;

	my_handle = (struct tffs_cache_handle *)handle;

	result = 0;

	if (ctx->ram_only == false && my_handle->backend_handle != NULL) {
		result = ctx->backend->close(ctx->backend, my_handle->backend_handle);
		if (result != 0) {
			// we can't destroy the handle now because upper layer may retry later
			goto err_out;
		}
	}

	if (my_handle->entry != NULL) {
		put_entry(ctx, my_handle->entry);
	}

	if (my_handle == &(ctx->panic_handle)) {
		smp_mb__before_clear_bit();
		clear_bit(PANIC_USE_BIT, &(ctx->panic_mode));
		smp_mb__after_clear_bit();
	} else {
		kfree(my_handle);
	}

err_out:
	return result;
}

static int TFFS3_CACHE_Write(struct tffs_module *this, void *handle, const uint8_t *write_buffer,
			     size_t write_length, size_t *rlen, unsigned int final)
{
	struct tffs_cache_ctx *ctx;
	struct tffs_cache_handle *my_handle;
	struct tffs_cache_entry *entry, *tmp_entry;
	struct tffs_cache_segment *segment;
	unsigned long flags;
	int result;

	ctx	  = (struct tffs_cache_ctx *)this->priv;
	my_handle = (struct tffs_cache_handle *)handle;
	BUG_ON(ctx == NULL || my_handle == NULL);

	segment = NULL;
	result	= 0;

	if (my_handle->mode != tffs3_mode_write && my_handle->mode != tffs3_mode_panic) {
		result = -EBADF;
		goto err_out;
	}

	if (ctx->ram_only == false) {
		result = ctx->backend->write(ctx->backend, my_handle->backend_handle, write_buffer,
					     write_length, rlen, final);
		if (result == 0)
			tffs_write_statistic(my_handle->id, write_length, 1, 0);
	} else {
		/* calling write with NULL write_buffer is legal to remove an entry */
		if (write_buffer != NULL) {
			segment = kzalloc(sizeof(*segment), GFP_KERNEL);
			if (segment == NULL) {
				result = -ENOMEM;
				goto err_out;
			}

			INIT_LIST_HEAD(&segment->segment_list);

			segment->data = NULL;

			if (write_length > 0) {
				segment->data = kmalloc(write_length, GFP_KERNEL);
				if (segment->data == NULL) {
					result = -ENOMEM;
					goto err_out;
				}
				memcpy(segment->data, write_buffer, write_length);
			}

			segment->len   = write_length;
			segment->entry = my_handle->entry;
			list_add_tail(&segment->segment_list, &(my_handle->entry->segment_list));
		}

		if (final) {
			spin_lock_irqsave(&ctx->list_lock, flags);

			/* If older entry exists, erase it from cache. List is ordered by
			 * ID, so we can stop searching when we find an ID bigger than this
			 * one. */
			list_for_each_entry_safe(entry, tmp_entry, &ctx->entry_list, entry_list) {
				if (entry->id > my_handle->id) {
					/* found place to prepend new entry */
					break;
				}
				/* erase old entry. There should only be at most one, but we
				 * keep releasing until all entries with our ID are gone   */
				if (entry->id == my_handle->id) {
					list_del_init(&(entry->entry_list));
					put_entry(ctx, entry);
				}
			}

			/* if entry has at least one segment, add it to the index. Prepend
			 * it to the element found before. If no element was found, this
			 * will put it on the end of the list  */
			if (!list_empty(&(my_handle->entry->segment_list))) {
				kref_get(&(
					my_handle->entry
						->refcnt)); // refcnt == 2 -> held by cache and handle
				list_add_tail(&(my_handle->entry->entry_list), &entry->entry_list);
				segment = list_first_entry(&my_handle->entry->segment_list,
							   struct tffs_cache_segment, segment_list);
			}

			spin_unlock_irqrestore(&ctx->list_lock, flags);

			// send notification upwards
			if (ctx->notify_cb != NULL) {
				pr_debug("[%s] sending notification upwards\n", __func__);
				ctx->notify_cb(ctx->notify_priv, my_handle->entry->id,
					       list_empty(&(my_handle->entry->segment_list)) ?
						       tffs3_notify_clear :
							     tffs3_notify_update);
			}
		}

		*rlen = write_length;
		tffs_write_statistic(my_handle->entry->id, write_length, 1, 1);
	}

err_out:
	if (result != 0 && segment != NULL) {
		if (segment->data != NULL) {
			kfree(segment->data);
			segment->data = NULL;
		}

		kfree(segment);
		segment = NULL;
	}

	return result;
}

static int TFFS3_CACHE_Read(struct tffs_module *this, void *handle, uint8_t *read_buffer,
			    size_t *rlen)
{
	struct tffs_cache_ctx *ctx;
	struct tffs_cache_handle *my_handle;
	size_t read_len, total_read;
	int result;

	ctx	  = (struct tffs_cache_ctx *)this->priv;
	my_handle = (struct tffs_cache_handle *)handle;
	BUG_ON(ctx == NULL || my_handle == NULL);

	if (ctx->ram_only == false && my_handle->mode == tffs3_mode_panic) {
		return ctx->backend->read(ctx->backend, my_handle->backend_handle, read_buffer,
					  rlen);
	}

	if (my_handle->entry == NULL) {
		return -EBADF;
	}

	if (my_handle->mode != tffs3_mode_read) {
		return -EBADF;
	}

	/* Finally return error if entry does not exist. Stupid backward
	 * compatibility cruft... */
	if (my_handle->segment == NULL) {
		return -ENOENT;
	}

	total_read = 0;
	result	   = 0;
	while (total_read < *rlen && my_handle->segment != NULL) {
		// sanity check
		if (my_handle->seg_offset > my_handle->segment->len) {
			pr_err("[%s] invalid handle state, segment offset 0x%x > segment size 0x%x\n",
			       __func__, my_handle->seg_offset, my_handle->segment->len);
			result = -EIO;
			break;
		}

		if (my_handle->seg_offset == my_handle->segment->len) {
			// end of current segment reached
			if (!list_is_last(&my_handle->segment->segment_list,
					  &my_handle->entry->segment_list)) {
				my_handle->segment =
					list_first_entry(&my_handle->segment->segment_list,
							 struct tffs_cache_segment, segment_list);
				my_handle->seg_offset = 0;
			} else {
				// EOF reached
				break;
			}
		} else {
			read_len = min((*rlen - total_read),
				       (my_handle->segment->len - my_handle->seg_offset));
			memcpy(read_buffer + total_read,
			       my_handle->segment->data + my_handle->seg_offset, read_len);
			total_read += read_len;
			my_handle->seg_offset += read_len;
			my_handle->total_offset += read_len;
		}
	}

	tffs_write_statistic(my_handle->entry->id, total_read, 0, 1);

	*rlen = total_read;

	return result;
}

static void TFFS3_CACHE_Notify(void *priv, unsigned int id, enum tffs3_notify_event event)
{
	struct tffs_cache_ctx *ctx;
	struct tffs_cache_entry *entry, *tmp;
	struct list_head del_list;
	unsigned long flags;

	ctx = (struct tffs_cache_ctx *)priv;

	BUG_ON(ctx == NULL);

	pr_debug("[%s] Called for ID 0x%x event %s\n", __func__, id,
		 event == tffs3_notify_clear  ? "clear" :
		 event == tffs3_notify_update ? "update" :
		 event == tffs3_notify_reinit ? "reinit" :
						      "unknown");
	INIT_LIST_HEAD(&del_list);
	spin_lock_irqsave(&ctx->list_lock, flags);
	list_for_each_entry_safe(entry, tmp, &ctx->entry_list, entry_list) {
		if (event == tffs3_notify_reinit || entry->id == id) {
			list_del_init(&entry->entry_list);
			put_entry(ctx, entry);

			// unless the whole cache is invalidated, we can stop now
			if (event != tffs3_notify_reinit) {
				break;
			}
		}
	}
	spin_unlock_irqrestore(&ctx->list_lock, flags);

	pr_debug("[%s] sending notifications\n", __func__);
	// send notification upwards
	if (ctx->notify_cb != NULL) {
		pr_debug("[%s] sending notification upwards\n", __func__);
		ctx->notify_cb(ctx->notify_priv, id, event);
	}
	pr_debug("[%s] done\n", __func__);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_CACHE_Register_Notify(struct tffs_module *this, void *notify_priv,
				       tffs3_notify_fn notify_cb)
{
	struct tffs_cache_ctx *ctx;
	int result;

	ctx = (struct tffs_cache_ctx *)this->priv;

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
static int TFFS3_CACHE_Remove_Notify(struct tffs_module *this, void *notify_priv,
				     tffs3_notify_fn notify_cb)
{
	struct tffs_cache_ctx *ctx;
	int result;

	ctx = (struct tffs_cache_ctx *)this->priv;

	result = -EINVAL;
	if (ctx->notify_priv == notify_priv && ctx->notify_cb == notify_cb) {
		ctx->notify_cb	 = NULL;
		ctx->notify_priv = NULL;
		result		 = 0;
	}

	return result;
}

static int TFFS3_CACHE_Reindex(struct tffs_module *this)
{
	struct tffs_cache_ctx *ctx;
	int result;

	ctx = (struct tffs_cache_ctx *)this->priv;

	if (ctx->ram_only == false) {
		result = ctx->backend->reindex(ctx->backend);
	} else {
		result = 0;
	}

	return result;
}

static int TFFS3_CACHE_Cleanup(struct tffs_module *this, void *handle)
{
	struct tffs_cache_ctx *ctx;
	struct tffs_cache_handle *my_handle;
	int result;

	ctx	  = (struct tffs_cache_ctx *)this->priv;
	my_handle = (struct tffs_cache_handle *)handle;
	BUG_ON(ctx == NULL || my_handle == NULL);

	if (my_handle->mode != tffs3_mode_write) {
		return -EBADF;
	}

	if (ctx->ram_only == false) {
		result = ctx->backend->cleanup(ctx->backend, my_handle->backend_handle);
	} else {
		result = 0;
	}

	return result;
}

static int TFFS3_CACHE_Info(struct tffs_module *this, unsigned int *fill_lvl)
{
	struct tffs_cache_ctx *ctx;
	int result;

	BUG_ON(this == NULL);
	ctx = (struct tffs_cache_ctx *)this->priv;
	BUG_ON(ctx == NULL);

#if 0
	{
		struct tffs_cache_entry *entry;
		struct tffs_cache_segment *segment;
		size_t entry_size;
		unsigned long flags;

		spin_lock_irqsave(&ctx->list_lock, flags);
		list_for_each_entry(entry, &ctx->entry_list, entry_list){
			entry_size = 0;
			list_for_each_entry(segment, &entry->segment_list, segment_list){
				entry_size += segment->len;
			}
			pr_err("[%s] id: 0x%x size: 0x%x%s refcnt: 0x%x\n",
					__func__, entry->id, entry_size,
					list_empty(&entry->segment_list) ? "(erased)":"",
					entry->refcnt.refcount.counter);
		}
		spin_unlock_irqrestore(&ctx->list_lock, flags);
	}
#endif

	if (ctx->ram_only == false) {
		result = ctx->backend->info(ctx->backend, fill_lvl);
	} else {
		*fill_lvl = 50;
		result	  = 0;
	}

	return result;
}

static int TFFS3_CACHE_Setup(struct tffs_module *this)
{
	struct tffs_cache_ctx *ctx;
	int result;

	pr_info("[TFFS3-CACHE] Caching module for TFFS 3.x\n");
	BUG_ON(this == NULL);

	result = -EINVAL;

	ctx = (struct tffs_cache_ctx *)this->priv;
	BUG_ON(ctx == NULL);

	if (ctx->ram_only == false) {
		if (ctx->backend == NULL) {
			pr_emerg("[%s] No backend configured!\n", __func__);
			result = -ENODEV;
			goto err_out;
		}

		result = ctx->backend->setup(ctx->backend);
		if (result != 0) {
			pr_err("[TFFS3-CACHE] Setup of backend failed\n");
			goto err_out;
		}

		/* register notification cb _after_ setup, so we don't get swamped during
		 * initial scan
		 */

		result = ctx->backend->register_notify(ctx->backend, ctx, TFFS3_CACHE_Notify);
		if (result != 0) {
			pr_err("[TFFS3-CACHE] Registering notification call-back failed\n");
			goto err_out;
		}
	}

	return 0;

err_out:
	if (ctx != NULL) {
		kfree(ctx);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_CACHE_Configure(struct tffs_module *this, struct tffs_module *backend)
{
	struct tffs_cache_ctx *ctx;
	int result;

	// pr_err("[%s] Called\n", __func__);

	BUG_ON(this == NULL || backend == NULL);

	result = -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		pr_err("[%s] Out of memory error\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	INIT_LIST_HEAD(&ctx->entry_list);
	INIT_LIST_HEAD(&ctx->clear_list);
	spin_lock_init(&ctx->list_lock);
	spin_lock_init(&ctx->clear_lock);
	ctx->ram_only = false;

	this->priv = ctx;

#if defined(CONFIG_TFFS_DEV_CACHE_RAMONLY)
	if (backend->setup == NULL) {
		pr_info("[%s] No backend module registered, switching to RAM-only mode.\n",
			__func__);
		ctx->ram_only = true;
	}
#endif

	if (ctx->ram_only == false && backend->register_notify == NULL) {
		pr_warn("[%s] Backend %s does not support cache notification, caching disabled\n",
			__func__, backend->name);
		this->name	      = "cache_passthrough";
		this->setup	      = backend->setup;
		this->open	      = backend->open;
		this->close	      = backend->close;
		this->read	      = backend->read;
		this->write	      = backend->write;
		this->cleanup	      = backend->cleanup;
		this->reindex	      = backend->reindex;
		this->info	      = backend->info;
		this->register_notify = backend->register_notify;
		this->remove_notify   = backend->remove_notify;

	} else {
		if (ctx->ram_only == false) {
			pr_info("[%s] Setting up caching for backend %s\n", __func__,
				backend->name);
			ctx->backend = backend;
		}

		this->name	      = "cache";
		this->setup	      = TFFS3_CACHE_Setup;
		this->open	      = TFFS3_CACHE_Open;
		this->close	      = TFFS3_CACHE_Close;
		this->read	      = TFFS3_CACHE_Read;
		this->write	      = TFFS3_CACHE_Write;
		this->cleanup	      = TFFS3_CACHE_Cleanup;
		this->reindex	      = TFFS3_CACHE_Reindex;
		this->info	      = TFFS3_CACHE_Info;
		this->register_notify = TFFS3_CACHE_Register_Notify;
		this->remove_notify   = TFFS3_CACHE_Remove_Notify;
	}

	result = 0;

err_out:
	return result;
}
EXPORT_SYMBOL(TFFS3_CACHE_Configure);
