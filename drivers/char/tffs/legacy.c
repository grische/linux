// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
#ifdef CONFIG_SMP
#define __SMP__
#endif /*--- #ifdef CONFIG_SMP ---*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/mtd/mtd.h>
#include <avm/tffs/tffs.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/bitops.h>

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/
#include <avm/tffs/tffs.h>
#include "local.h"
#include "legacy.h"

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
//#define FORCE_DEBUG_DEFECT // force error on Entry.ID == 0xf2

#define RESCAN_CHECK  0
#define RESCAN_DIAG   1
#define RESCAN_UPDATE 2

#define MAX_SEGMENT_SIZE    ((32 * 1024) + 16) // historic value
#define CLEANUP_BUFFER_SIZE PAGE_SIZE /* Something small and reasonable. */

#define TFFS_GET_SEGMENT_VALUE(E)                                               \
	~(((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 3]) << 0) |  \
	  ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 2]) << 8) |  \
	  ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 1]) << 16) | \
	  ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 0]) << 24))

#define TFFS_SET_SEGMENT_VALUE(E, value)                                          \
	((E)->Buffer[sizeof(struct _TFFS_Entry) + 3] = ((~(value) >> 0) & 0xFF),  \
	 (E)->Buffer[sizeof(struct _TFFS_Entry) + 2] = ((~(value) >> 8) & 0xFF),  \
	 (E)->Buffer[sizeof(struct _TFFS_Entry) + 1] = ((~(value) >> 16) & 0xFF), \
	 (E)->Buffer[sizeof(struct _TFFS_Entry) + 0] = ((~(value) >> 24) & 0xFF))

union _tffs_segment_entry {
	struct _TFFS_Entry Entry;
	unsigned char Buffer[sizeof(struct _TFFS_Entry) + sizeof(unsigned int)];
};

// TODO: get rid of sparsely populated array, use something like a hashmap
static loff_t TFFS_Global_Index[FLASH_FS_ID_LAST];

int tffs_mtd[2] = { CONFIG_TFFS_MTD_DEVICE_0, CONFIG_TFFS_MTD_DEVICE_1 };

//#define FORCE_DEBUG_DEFECT
#if defined(FORCE_DEBUG_DEFECT)
static unsigned int force_test = 0;
#endif

static int TFFS3_LGCY_Reindex(struct tffs_module *this);
static int TFFS3_LGCY_Cleanup(struct tffs_module *this, void *handle);
static int do_cleanup(struct tffs_module *this);
static int rescan_segment(struct tffs_lgcy_ctx *ctx, struct tffs_mtd *tffs_mtd, unsigned int mode,
			  unsigned int *alive_cnt, unsigned int *alive_size);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int MTD_READ(struct tffs_mtd *local_TFFS, loff_t from, size_t len, size_t *retlen,
			   u_char *buf)
{
	if (local_TFFS == NULL) {
		pr_err("[%s] mtd_info/mtd_info->read is NULL\n", __func__);
		return -EFAULT;
	}

	return mtd_read(local_TFFS->mtd, from, len, retlen, buf);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int MTD_WRITE(struct tffs_mtd *local_TFFS, loff_t from, size_t len, size_t *retlen,
			    const u_char *buf)
{
	if (local_TFFS == NULL) {
		pr_err("[%s] mtd_info/mtd_info->write is NULL\n", __func__);
		return -EFAULT;
	}

	return mtd_write(local_TFFS->mtd, from, len, retlen, buf);
}

static inline int MTD_READ_HDR(struct tffs_mtd *local_TFFS, loff_t from, struct _TFFS_Entry *E)
{
	int result;
	struct _TFFS_Entry Entry;
	size_t retlen;

	result = MTD_READ(local_TFFS, from, sizeof(Entry), &retlen, (u_char *)&Entry);

	if (result == 0 && retlen == sizeof(Entry)) {
		E->ID	  = be16_to_cpu(Entry.ID);
		E->Length = be16_to_cpu(Entry.Length);
	} else {
		result = -EIO;
	}

	return result;
}

static inline int MTD_WRITE_HDR(struct tffs_mtd *local_TFFS, loff_t from, struct _TFFS_Entry *E)
{
	int result;
	struct _TFFS_Entry Entry;
	size_t retlen;

	Entry.ID     = cpu_to_be16(E->ID);
	Entry.Length = cpu_to_be16(E->Length);
	result	     = MTD_WRITE(local_TFFS, from, sizeof(Entry), &retlen, (u_char *)&Entry);
	if (result != 0 || retlen != sizeof(Entry)) {
		result = -EIO;
	}

	return result;
}

static inline int MTD_READ_SEGMENT(struct tffs_mtd *local_TFFS, loff_t from,
				   union _tffs_segment_entry *seg)
{
	int result;
	size_t retlen;

	result = MTD_READ(local_TFFS, from, sizeof(seg->Buffer), &retlen, seg->Buffer);

	if (result == 0 && retlen == sizeof(seg->Buffer)) {
		seg->Entry.ID	  = be16_to_cpu(seg->Entry.ID);
		seg->Entry.Length = be16_to_cpu(seg->Entry.Length);
	} else {
		result = -EIO;
	}

	return result;
}

static inline int MTD_WRITE_SEGMENT(struct tffs_mtd *local_TFFS, loff_t to,
				    union _tffs_segment_entry *seg)
{
	int result;
	union _tffs_segment_entry fixed_seg;
	size_t retlen;

	memcpy(&fixed_seg, seg, sizeof(*seg));
	fixed_seg.Entry.ID     = cpu_to_be16(seg->Entry.ID);
	fixed_seg.Entry.Length = cpu_to_be16(seg->Entry.Length);

	result = MTD_WRITE(local_TFFS, to, sizeof(fixed_seg.Buffer), &retlen, fixed_seg.Buffer);
	if (result != 0 || retlen != sizeof(fixed_seg.Buffer)) {
		result = -EIO;
	}

	return result;
}

static inline unsigned int align_len(unsigned int len)
{
	return (len + 3) & ~(0x3UL);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int get_mtd_device_wrapped(struct tffs_mtd *tffs_mtd, unsigned int mtd_idx)
{
	int result;

	result		    = 0;
	tffs_mtd->start_bad = -1;

	tffs_mtd->mtd = get_mtd_device(NULL, mtd_idx);
	if (IS_ERR_OR_NULL(tffs_mtd->mtd)) {
		result = -ENODEV;
	} else {
		tffs_mtd->mtd_idx = mtd_idx;
		tffs_mtd->size	  = tffs_mtd->mtd->size;
	}

	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void put_mtd_device_wrapped(struct tffs_mtd *tffs_mtd)
{
	put_mtd_device(tffs_mtd->mtd);
}

static void send_notification(struct tffs_lgcy_ctx *ctx, enum _tffs_id Id,
			      enum tffs3_notify_event evnt)
{
	if (ctx->notify_cb != NULL && ctx->panic_mode == 0) {
		ctx->notify_cb(ctx->notify_priv, Id, evnt);
	}
}

#if 0
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void TFFS_Deinit(void)
{
	DBG((KERN_INFO "TFFS_Deinit()\n"));
	if(!TFFS_mtd.use_bdev)
		put_mtd_device_wrapped(&TFFS_mtd);
	TFFS_mtd_number = (unsigned int) -1;
	TFFS_mtd.tffs.mtd = NULL;
}
#endif

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void *TFFS3_LGCY_Open(struct tffs_module *this, struct tffs_core_handle *core_handle)
{
	struct tffs_lgcy_ctx *ctx;
	struct TFFS_LGCY_State *state;
	int result;

	ctx			      = (struct tffs_lgcy_ctx *)this->priv;
	core_handle->max_segment_size = MAX_SEGMENT_SIZE;

	// when opened in panic mode, use static state struct and if there is a special panic
	// mtd_info set up, use that one
	state  = NULL;
	result = 0;
	if (core_handle->mode == tffs3_mode_panic) {
		if (!test_and_set_bit(PANIC_MODE_BIT, &(ctx->panic_mode))) {
			state = &(ctx->panic_state);
		}
	} else {
		if (ctx->idx_created == 0) {
			result = TFFS3_LGCY_Reindex(this);
			if (result != 0) {
				goto err_out;
			}
		}

		state = kzalloc(sizeof(*state), GFP_KERNEL);
	}

	if (state == NULL) {
		pr_debug("[%s] malloc(%u) failed\n", __func__, sizeof(*state));
		result = -ENOMEM;
	} else {
		state->offset  = 0;
		state->id      = core_handle->id;
		state->readbuf = NULL;
	}

err_out:
	if (result != 0) {
		state = ERR_PTR(result);
	}

	return state;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Close(struct tffs_module *this, void *handle)
{
	struct tffs_lgcy_ctx *ctx;
	struct TFFS_LGCY_State *state;

	ctx = (struct tffs_lgcy_ctx *)this->priv;

	if (ctx && ctx->active_mtd) {
		mtd_sync(ctx->active_mtd->mtd);
	}

	state = (struct TFFS_LGCY_State *)handle;
	if (state == &(ctx->panic_state)) {
		smp_mb__before_clear_bit();
		clear_bit(PANIC_MODE_BIT, &(ctx->panic_mode));
		smp_mb__after_clear_bit();
	} else {
		if (!IS_ERR_OR_NULL(state->readbuf)) {
			kfree(state->readbuf);
		}

		kfree(state);
	}

	return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void TFFS3_LGCY_Format_Callback(struct erase_info *instr)
{
	switch (instr->state) {
	case MTD_ERASE_PENDING:
		break;
	case MTD_ERASING:
		break;
	case MTD_ERASE_SUSPEND:
		break;
	case MTD_ERASE_FAILED:
	case MTD_ERASE_DONE:
		wake_up((wait_queue_head_t *)instr->priv);
		break;
	}

	return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Format(struct tffs_mtd *tffs_mtd)
{
	struct erase_info *erase;
	int result;
	wait_queue_head_t wait_q;

	init_waitqueue_head(&wait_q);

	erase = (struct erase_info *)kzalloc(sizeof(struct erase_info), GFP_KERNEL);
	if (erase == NULL) {
		pr_err("[%s] kzalloc failed\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	erase->mtd	= tffs_mtd->mtd;
	erase->addr	= 0;
	erase->len	= tffs_mtd->mtd->size;
	erase->callback = TFFS3_LGCY_Format_Callback;
	erase->priv	= (u_long)&wait_q;
	erase->next	= NULL;

	//pr_err("[%s] erase: addr %llx len %llx\n", __func__, erase->addr, erase->len);

	result = mtd_erase(tffs_mtd->mtd, erase);

	if (result != 0) {
		pr_err("[%s] Failed to erase mtd, region [0x%llx, 0x%llx]\n", __func__, erase->addr,
		       erase->len);
		goto err_out;
	}

	/* reset bad entry marker */
	tffs_mtd->start_bad = -1;
	wait_event(wait_q, (erase->state == MTD_ERASE_DONE) || (erase->state == MTD_ERASE_FAILED));

	if (erase->state == MTD_ERASE_FAILED) {
		pr_err("[%s] Failed (callback) to erase mtd, region [0x%llx, 0x%llx]\n", __func__,
		       erase->addr, erase->len);
		result = -EIO;
	}

err_out:
	if (erase != NULL) {
		kfree(erase);
	}

	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _tffs_memcmp { tffs_memcmp_equal, tffs_memcmp_writeable, tffs_memcmp_clear_required };

static enum _tffs_memcmp TFFS3_Memcmp(struct tffs_lgcy_ctx *ctx, struct _TFFS_Entry *Entry,
				      loff_t offset, const uint8_t *wr_buf, bool verify)
{
	uint8_t *rd_buf, *my_buf;
	int result;
	size_t retlen;
	enum _tffs_memcmp writable;

	rd_buf = NULL;

	// NULL pointer means entry is to be cleared
	if (wr_buf == NULL) {
		pr_debug("[%s] explicit clearing requested\n", __func__);
		writable = tffs_memcmp_clear_required;
		goto err_out;
	}

	// no need to compare zero length buffers ;)
	if (Entry->Length == 0) {
		pr_debug("[%s] zero length entry\n", __func__);
		writable = tffs_memcmp_equal;
		goto err_out;
	}

	writable = tffs_memcmp_clear_required;

	/* skip actual reading and comparing when in panic mode */
	if (ctx->panic_mode & PANIC_MODE) {
		/* pretend everything is okay */
		if (verify) {
			writable = tffs_memcmp_equal;
		}
		goto err_out;
	}

	/* read old entry */
	rd_buf = kmalloc(Entry->Length, GFP_KERNEL);
	if (rd_buf == NULL) {
		pr_debug("[%s] allocating compare buffer failed, skipping test\n", __func__);
		goto err_out;
	}

	result = MTD_READ(ctx->active_mtd, offset + sizeof(*Entry), Entry->Length, &retlen, rd_buf);
	if (result != 0 || retlen != Entry->Length) {
		pr_err("[%s] reading old entry for compare failed\n", __func__);
		goto err_out;
	}

	tffs_write_statistic(Entry->ID, retlen, 0, 0);

	result = memcmp(rd_buf, wr_buf, Entry->Length);
	if (result == 0) {
		pr_debug("[%s] entries equal\n", __func__);
		writable = tffs_memcmp_equal;
		goto err_out;
	}

	/* dump the first kb of write and read buffers when in verify only mode */
	if (verify) {
		pr_debug("Verify for entry 0x%x with len 0x%x at 0x%llx failed.\n", Entry->ID,
			 Entry->Length, offset);
		pr_debug("Write buffer:\n");
		print_hex_dump(KERN_DEBUG, "orig: ", DUMP_PREFIX_OFFSET, 16, 1, wr_buf,
			       min_t(unsigned short, Entry->Length, 1024), false);
		pr_debug("Read buffer:\n");
		print_hex_dump(KERN_DEBUG, "read: ", DUMP_PREFIX_OFFSET, 16, 1, rd_buf,
			       min_t(unsigned short, Entry->Length, 1024), false);
	}

	/**
	 * enries are differing. Check if the old entry may be overwritten by new
	 * one. Since this is NOR flash, we can only turn ones into zeros. Do a
	 * bitwise 'AND' of the two buffers and check if the result equals the
	 * write buffer.
	 */
	writable = tffs_memcmp_writeable;
	my_buf	 = rd_buf;
	while (retlen--) {
		if ((*my_buf & *wr_buf) != *wr_buf) {
			writable = tffs_memcmp_clear_required;
			break;
		}

		++my_buf;
		++wr_buf;
	}

	if (writable == tffs_memcmp_writeable) {
		pr_debug("[%s] old entry writable\n", __func__);
	}

err_out:
	if (rd_buf != NULL) {
		kfree(rd_buf);
	}

	return writable;
}

static int do_write(struct tffs_lgcy_ctx *ctx, enum _tffs_id Id, const uint8_t *write_buffer,
		    size_t write_length, size_t *rlen, unsigned int *fill_lvl)
{
	int result, retlen;
	struct _TFFS_Entry Entry;
	loff_t offset, clear_offset, write_offset;
	size_t clear_size, needed_size;
	unsigned int Len;

	*rlen	     = write_length;
	needed_size  = sizeof(struct _TFFS_Entry) + align_len(write_length);
	result	     = 0;
	Len	     = 0;
	offset	     = 0;
	write_offset = -1;
	clear_offset = -1;
	clear_size   = 0;

	// if entry is to be cleared, invalidate it in global index
	if (write_buffer == NULL) {
		TFFS_Global_Index[Id] = -1;
	}

	do {
		result = MTD_READ_HDR(ctx->active_mtd, offset, &Entry);
		if (result != 0) {
			goto err_out;
		}

		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(Entry), 0,
				     0); /*--- internal read ---*/

		/*---------------------------------------------------------------------------------------*\
		 * ID found
		 \*---------------------------------------------------------------------------------------*/
		if (Entry.ID == (unsigned short)Id &&
		    Id < FLASH_FS_DROPABLE_DATA) { /*---  old entry found ----*/
			pr_debug("[%s] found old entry with ID=%x ", __func__, Id);

			/*-----------------------------------------------------------------------------------*\
			 * if we haven't found a place to write the data to yet, check if we can re-use
			 * this old entry. Skip test when in panic mode because of the kmalloc needed
			\*-----------------------------------------------------------------------------------*/
			if (write_offset < 0 && (ctx->panic_mode & PANIC_MODE) == 0 &&
			    Entry.Length == write_length) {
				switch (TFFS3_Memcmp(ctx, &Entry, offset, write_buffer, false)) {
				case tffs_memcmp_equal:
					pr_debug("[%s] old and new entry identical\n", __func__);
					/* identical entry found, we can abort search here. There is a corner case
					 * where we found an entry that has been written earlier but somehow the
					 * previous entry for that one has not been cleared.
					 * Update global index and make sure an earlier entry gets cleared now.
					 */
					TFFS_Global_Index[Entry.ID] = offset;
					result			    = 0;
					goto clear_old;
					break;
				case tffs_memcmp_writeable:
					pr_debug("[%s] old entry can be overwritten by new one\n",
						 __func__);
					write_offset = offset;
					break;
				default:
				case tffs_memcmp_clear_required:
					break;
				}
			}
			/*
			 * remember entry for clearing later, unless we are going to overwrite it
			 */
			if (write_offset != offset) {
				if (clear_offset < 0) {
					/* first entry found for this id. This will have to be cleared when a
					 * new valid entry has been written
					 */
					clear_offset = offset;
					clear_size   = Entry.Length;

				} else {
					/* only the first entry for a given ID is valid. If we find a second
					 * (third, fourth...) entry for this ID, it must be a remnant of a failed
					 * write and can therefore safely be marked as erased
					 */
					Entry.ID =
						FLASH_FS_ID_SKIP; // safe to change, can't be FLASH_FS_ID_FREE checked for later
					result = MTD_WRITE_HDR(ctx->active_mtd, offset, &Entry);

					if (result != 0) {
						pr_warn("[%s] erasing duplicate entry for ID 0x%x at 0x%llx failed!\n",
							__func__, Id, offset);
						goto err_out;
					}
				}
			}
		}

		/*---------------------------------------------------------------------------------------*\
		 * found start of free space. Append new entry here unless we already found an earlier
		 * entry we can recycle
		\*---------------------------------------------------------------------------------------*/
		if (Entry.ID == FLASH_FS_ID_FREE) {
			pr_debug("[%s] found start of free area at offset 0x%llx\n", __func__,
				 offset);
			if (write_offset < 0) {
				write_offset = offset;
			}
			break;
		}

		Len = align_len(Entry.Length);
		offset += (sizeof(struct _TFFS_Entry) + Len);
	} while ((offset + needed_size) <= ctx->active_mtd->size);

	/*
	 * if there is data to be written, write it to the place we have found above
	 */
	if (write_buffer != NULL) {
		if (write_offset >= 0 && (write_offset + needed_size) < ctx->active_mtd->size) {
			Entry.ID     = (unsigned short)Id;
			Entry.Length = (unsigned short)write_length;

#if defined(FORCE_DEBUG_DEFECT)
			if (Entry.ID == 0xf2) {
				Entry.ID   = 0x4042;
				force_test = 1;
			}
#endif // defined(FORCE_DEBUG_DEFECT)

			result = MTD_WRITE_HDR(ctx->active_mtd, write_offset, &Entry);
			if (result != 0) {
				pr_err("[%s] writing entry header for id 0x%x at offset 0x%llx failed, reason %d\n",
				       __func__, Id, write_offset, result);
				goto err_out;
			}

#if defined(FORCE_DEBUG_DEFECT)
			if (Entry.ID == 0x4042) {
				Entry.ID = 0xf2;
			}
#endif // defined(FORCE_DEBUG_DEFECT)

			tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 1,
					     0); /*--- internal write ---*/

			pr_debug("[%s] header with id %x written at offset 0x%llx\n", __func__, Id,
				 write_offset);

			if (Entry.Length > 0) {
				result = MTD_WRITE(ctx->active_mtd,
						   write_offset + sizeof(struct _TFFS_Entry),
						   Entry.Length, &retlen, write_buffer);

				if (result != 0 || (retlen != Entry.Length)) {
					pr_err("[%s] writing entry data for id 0x%x at offset 0x%llx failed, reason %d\n",
					       __func__, Id,
					       write_offset + sizeof(struct _TFFS_Entry), result);
					result = result ? result : -EIO;
					goto err_out;
				}

				tffs_write_statistic(Entry.ID, retlen, 1, 0);
			}

			// write succeeded, update global index
			TFFS_Global_Index[Entry.ID] = write_offset;
			if (write_offset == offset) {
				offset += needed_size;
			}

			pr_debug("[%s] %d bytes data written\n", __func__, write_length);
		} else {
			/* if we found nowhere to put the data, we must have run out of space.
			 * Return file table overflow error, so wrapper func may trigger a cleanup and retry.
			 */
			result = -ENFILE;
			goto err_out;
		}
	}

clear_old:
	/*
	 * if an entry already existed, clear it out after the new one has
	 * been written
	 */
	if (clear_offset >= 0) {
		Entry.ID     = FLASH_FS_ID_SKIP;
		Entry.Length = clear_size;

		result = MTD_WRITE_HDR(ctx->active_mtd, clear_offset, &Entry);
		if (result != 0) {
			pr_err("[%s] clearing entry for ID 0x%x at offset 0x%llx failed.\n",
			       __func__, Id, clear_offset);
			goto err_out;
		}

		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 1,
				     0); /*--- internal write ---*/

		pr_debug("[%s] cleared ID 0x%x at offset 0x%llx, len=%u\n", __func__, Id,
			 clear_offset, clear_size);
	}

	/*
	 *   compute filesystem fill level
	 */
	if (fill_lvl != NULL) {
		*fill_lvl = offset;
		*fill_lvl *= 100;
		*fill_lvl /= (unsigned int)ctx->active_mtd->size;
	}
err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Write(struct tffs_module *this, void *handle, const uint8_t *write_buffer,
			    size_t write_length, size_t *rlen, unsigned int final)
{
	struct tffs_lgcy_ctx *ctx;
	struct TFFS_LGCY_State *state;
	unsigned int fill_lvl;
	bool trigger_cleanup;
	int result, clean_res;
#if defined(CONFIG_TFFS_VERIFY_WRITE)
	struct _TFFS_Entry Entry __maybe_unused;
	loff_t old_offset __maybe_unused;
	size_t old_len __maybe_unused;
	enum _tffs_memcmp cmp_result __maybe_unused;
#endif // defined(CONFIG_TFFS_VERIFY_WRITE)

	state = (struct TFFS_LGCY_State *)handle;
	if (state->id >= FLASH_FS_ID_LAST) {
		pr_debug("[%s] invalid tffs_id: 0x%x\n", __func__, state->id);
		return -ENOENT;
	}

	// erasing an entry is always  final
	if (write_length == 0 && write_buffer == NULL) {
		final = 1;
	}

	if (final == 0 || write_length > MAX_SEGMENT_SIZE) {
		return -ENOSPC;
	}

	ctx = (struct tffs_lgcy_ctx *)this->priv;
	if (ctx == NULL) {
		return -EBADF;
	}

#if defined(CONFIG_TFFS_VERIFY_WRITE)
	old_offset = TFFS_Global_Index[state->id];
	old_len	   = 0;

	if (old_offset >= 0) {
		result = MTD_READ_HDR(ctx->active_mtd, old_offset, &Entry);
		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(Entry), 0,
				     0); /*--- internal read ---*/
		if (result != 0) {
			goto err_out;
		}

		if (Entry.ID != state->id) {
			pr_err("[%s] Entry for ID 0x%x at offset 0x%llx is wrong: ID 0x%x Len 0x%x\n",
			       __func__, state->id, old_offset, Entry.ID, Entry.Length);
		}
		old_len = Entry.Length;
	}
#endif // defined(CONFIG_TFFS_VERIFY_WRITE)

	result = do_write(ctx, state->id, write_buffer, write_length, rlen, &fill_lvl);

	// handle write error and high fill levels
	if (ctx->panic_mode == 0) {
		if (result == -ENFILE) {
			/* write failed because there was no space left.    *
			 * Do emergency cleanup and try again               */
			clean_res = do_cleanup(this);
			if (clean_res == 0) {
#if defined(CONFIG_TFFS_VERIFY_WRITE)
				old_offset = TFFS_Global_Index[state->id];
#endif // defined(CONFIG_TFFS_VERIFY_WRITE)
				result = do_write(ctx, state->id, write_buffer, write_length, rlen,
						  &fill_lvl);
			}
		}

		if (result != 0) {
			goto err_out;
		}

		trigger_cleanup = false;

#if defined(CONFIG_TFFS_VERIFY_WRITE)
		if (old_offset >= 0 && old_offset != TFFS_Global_Index[state->id]) {
			result = MTD_READ_HDR(ctx->active_mtd, old_offset, &Entry);
			tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(Entry), 0,
					     0); /*--- internal read ---*/
			if (result != 0) {
				goto err_out;
			}

			if (Entry.ID != FLASH_FS_ID_SKIP || Entry.Length != old_len) {
				pr_err("[%s] Clearing old entry for ID 0x%x Length 0x%x at 0x%llx failed. "
				       "Found ID 0x%x Length 0x%x\n",
				       __func__, state->id, old_len, old_offset, Entry.ID,
				       Entry.Length);
			}
		}

		if (TFFS_Global_Index[state->id] >= 0) {
			result =
				MTD_READ_HDR(ctx->active_mtd, TFFS_Global_Index[state->id], &Entry);
			tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(Entry), 0,
					     0); /*--- internal read ---*/
			if (result != 0) {
				goto err_out;
			}

			if (Entry.ID != state->id || Entry.Length != write_length) {
				pr_err("[%s] Wrong header written for ID 0x%x Length 0x%x at 0x%llx. "
				       "Found ID 0x%x Length 0x%x\n",
				       __func__, state->id, write_length,
				       TFFS_Global_Index[state->id], Entry.ID, Entry.Length);

				ctx->active_mtd->start_bad = TFFS_Global_Index[state->id];
			} else {
				cmp_result = TFFS3_Memcmp(ctx, &Entry, TFFS_Global_Index[state->id],
							  write_buffer, true);
				if (cmp_result != tffs_memcmp_equal) {
					pr_err("[%s] Wrong data written for ID 0x%x Length 0x%x at 0x%llx.\n",
					       __func__, state->id, write_length,
					       TFFS_Global_Index[state->id]);

					ctx->active_mtd->start_bad = TFFS_Global_Index[state->id];
				}
			}

			if (ctx->active_mtd->start_bad != -1) {
				pr_err("[%s] calling do_cleanup()\n", __func__);
				do_cleanup(this);
				result = -EIO;
				goto err_out;
			}
		}
#endif // defined(CONFIG_TFFS_VERIFY_WRITE)

		if (fill_lvl > 90) {
			/* fill level is dangerously high. Do a synchronous cleanup */
			pr_info("TFFS: fill level %u%% > 90%% ... doing emergency Cleanup\n",
				fill_lvl);

			trigger_cleanup = false;
			clean_res	= do_cleanup(this);
			if (clean_res != 0) {
				pr_warn("TFFS: emergency cleanup failed.\n");
			}
		} else if (fill_lvl > 75) {
			/* filesystem is getting full. Trigger an asynchronous cleanup */
			pr_info("TFFS: fill level %u%% > 75%% ... trigger Cleanup\n", fill_lvl);
			trigger_cleanup = true;
		}

		if (trigger_cleanup != 0) {
			tffs_send_event(TFFS_EVENT_CLEANUP);
		}
	}

	/*
	 * Entry was written. Send notification for ID
	 */
	if (result == 0) {
		send_notification(ctx, state->id,
				  write_buffer ? tffs3_notify_update : tffs3_notify_clear);
	}

err_out:
	if (result == -ENFILE) {
		result = -ENOSPC;
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Read(struct tffs_module *this, void *handle, uint8_t *read_buffer,
			   size_t *read_length)
{
	struct tffs_lgcy_ctx *ctx;
	struct TFFS_LGCY_State *state;
	unsigned int retlen, result;
	struct _TFFS_Entry Entry;
	loff_t read_pos;
	size_t read_len;

	result = 0;

	ctx = (struct tffs_lgcy_ctx *)this->priv;
	if (ctx == NULL) {
		result = -EBADF;
		goto err_out;
	}

	state = (struct TFFS_LGCY_State *)handle;

	if (state->id >= FLASH_FS_ID_LAST || TFFS_Global_Index[state->id] < 0) {
		pr_debug("[%s] tffs id not found: 0x%x\n", __func__, state->id);
		result = -ENOENT;
		goto err_out;
	}

	/* set up handle state information and fill read buffer on first read */
	if (IS_ERR_OR_NULL(state->readbuf)) {
		pr_debug("[%s] found index entry for ID %x\n", __func__, state->id);
		result = MTD_READ_HDR(ctx->active_mtd, TFFS_Global_Index[state->id], &Entry);
		if (result != 0) {
			goto err_out;
		}

		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 0,
				     0); /*--- internal read ---*/

		state->start   = TFFS_Global_Index[state->id];
		state->len     = Entry.Length;
		state->readbuf = kmalloc(state->len, GFP_KERNEL);
		if (IS_ERR_OR_NULL(state->readbuf)) {
			result = -ENOMEM;
			goto err_out;
		}

		read_len = Entry.Length;
		read_pos = 0;
		while (read_len > 0) {
			result = MTD_READ(ctx->active_mtd,
					  state->start + sizeof(struct _TFFS_Entry) + read_pos,
					  read_len, &retlen, state->readbuf + read_pos);

			if (result != 0) {
				goto err_out;
			}

			tffs_write_statistic(state->id, retlen, 0, 0);

			read_len -= retlen;
			read_pos += retlen;
		}
	}

	if (state->len > state->offset) {
		read_len = min((state->len - state->offset), *read_length);

		memcpy(read_buffer, state->readbuf + state->offset, read_len);
		*read_length = read_len;
		state->offset += read_len;
	} else {
		*read_length = 0;
	}

err_out:
	return result;
}

static int rescan_segment(struct tffs_lgcy_ctx *ctx, struct tffs_mtd *tffs_mtd, unsigned int mode,
			  unsigned int *alive_cnt, unsigned int *alive_size)
{
	unsigned int result;
	struct _TFFS_Entry Entry;
	union _tffs_segment_entry segment;
	loff_t offset;
	unsigned int flash_len;
	unsigned int i, skip_cnt, skip_size;

	pr_debug("[%s] Called with mode=%u\n", __func__, mode);

	result	    = 0;
	skip_cnt    = 0;
	skip_size   = 0;
	*alive_cnt  = 0;
	*alive_size = 0;
	flash_len   = 0;

	// reset global index
	if (mode == RESCAN_UPDATE) {
		pr_debug("TFFS: Clearing index for update\n");
		for (i = 0; i < ARRAY_SIZE(TFFS_Global_Index); ++i) {
			TFFS_Global_Index[i] = -1;
		}

		// index has been initialised.
		ctx->idx_created = 1;
	}

	result = MTD_READ_SEGMENT(tffs_mtd, 0, &segment);
	tffs_write_statistic(segment.Entry.ID, sizeof(segment), 0, 0);
	if (result != 0) {
		pr_err("[%s] MTD read failed, could not read a complete _tffs_segment_entry from %s\n",
		       __func__, tffs_mtd->mtd->name);
		goto err_out;
	} else {
		if (segment.Entry.ID == FLASH_FS_ID_SEGMENT) {
			pr_debug("[%s] mtd \"%s\": segment value %u\n", __func__,
				 tffs_mtd->mtd->name, TFFS_GET_SEGMENT_VALUE(&segment));
		} else {
			pr_err("[TFFS] mtd \"%s\": no SEGMENT VALUE (0x%x)\n", tffs_mtd->mtd->name,
			       segment.Entry.ID);

			result = -ENFILE;
			goto err_out;
		}
	}

	for (offset = 0; offset + sizeof(struct _TFFS_Entry) < tffs_mtd->size;
	     offset += flash_len) {
		result = MTD_READ_HDR(tffs_mtd, offset, &Entry);
		if (result != 0) {
			goto err_out;
		}

		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 0, 0);

		// found start of free area
		if (Entry.ID == (unsigned short)FLASH_FS_ID_FREE) {
			pr_debug("[%s] end found at 0x%llx: 0x%x\n", __func__, offset, Entry.ID);
			break;
		}

		// dump found entries in diagnosis mode
		if (mode == RESCAN_DIAG) {
			pr_err("[TFFS] <0x%x> %u bytes at 0x%llx\n", Entry.ID, Entry.Length,
			       offset);
		} else {
			pr_debug("[%s] <0x%x> %u bytes at 0x%llx\n", __func__, Entry.ID,
				 Entry.Length, offset);
		}

		// illegal entry ID found
		if (Entry.ID >= (unsigned short)FLASH_FS_ID_LAST) {
			pr_err("[TFFS] illegal ID 0x%x found at offset 0x%llx\n", Entry.ID, offset);

			result = -ENFILE;
			goto err_out;
		}

		// calculate offset to next entry header
		flash_len = align_len(Entry.Length) + sizeof(struct _TFFS_Entry);

		if (Entry.ID == FLASH_FS_ID_SKIP) {
			++skip_cnt;
			skip_size += flash_len;
		} else {
			++(*alive_cnt);
			*alive_size += flash_len;
		}

		// check if length makes sense
		if (Entry.Length > MAX_SEGMENT_SIZE || (offset + flash_len) >= tffs_mtd->size) {
			pr_err("[TFFS] Entry with id 0x%x at 0x%llx has illegal length: 0x%x\n",
			       Entry.ID, offset, Entry.Length);

			result = -ENFILE;
			goto err_out;
		}

		// do not touch the global index unless we are in update mode
		if (mode != RESCAN_UPDATE) {
			continue;
		}

		/**
		 * detect sticky global index condition and cause an indirect panic.
		 */
		if (offset < sizeof(struct _TFFS_Entry) &&
		    TFFS_Global_Index[FLASH_FS_ID_SEGMENT] >= 0LL) {
			pr_err("TFFS_Global_Index not cleared!\n");
			ctx->idx_created = 0;
			TFFS3_Panic_Lock();
			tffs_send_event(TFFS_EVENT_PANIC);
			goto err_out;
		}

		// duplicate entry found, keep only the first
		if ((Entry.ID != FLASH_FS_ID_SKIP) && (TFFS_Global_Index[Entry.ID] >= 0LL)) {
			pr_info("[TFFS] clearing duplicate entry for id <0x%x> with %u bytes at 0x%llx\n",
				Entry.ID, Entry.Length, offset);

			Entry.ID = FLASH_FS_ID_SKIP;
			result	 = MTD_WRITE_HDR(tffs_mtd, offset, &Entry);
			if (result != 0) {
				goto err_out;
			}

			tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 1,
					     0); /*--- internal write ---*/
		} else {
			TFFS_Global_Index[Entry.ID] = offset;
		}
	}

	if (offset + sizeof(struct _TFFS_Entry) >= tffs_mtd->size) {
		pr_warn("[TFFS] filesystem full\n");
	}

err_out:
	if (mode == RESCAN_DIAG) {
		pr_err("[TFFS] Diagnosis summary: result: %d "
		       "skip_cnt: 0x%x skip_len: 0x%x alive_cnt: 0x%x alive_len: 0x%x\n",
		       result, skip_cnt, skip_size, *alive_cnt, *alive_size);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Reindex(struct tffs_module *this)
{
	unsigned int result, entry_cnt, entry_len;
	struct tffs_lgcy_ctx *ctx;

	ctx = (struct tffs_lgcy_ctx *)this->priv;
	if (ctx == NULL) {
		result = -EBADF;
		goto err_out;
	}

	result = rescan_segment(ctx, ctx->active_mtd, RESCAN_UPDATE, &entry_cnt, &entry_len);

	// send re-index notification to listeners
	if (result == 0) {
		send_notification(ctx, 0, tffs3_notify_reinit);
	} else if (result == -ENFILE) {
		// we discovered inconsistent entries. Try to fix them
		result = do_cleanup(this);
	}

err_out:
	return result;
}

static int kill_segment(struct tffs_mtd *mtd, uint32_t seg_nr)
{
	int result;
	union _tffs_segment_entry hdr;

	result = TFFS3_LGCY_Format(mtd);
	if (result != 0) {
		pr_err("[%s] format of mtd \"%s\" failed\n", __func__, mtd->mtd->name);
		goto err_out;
	}

	hdr.Entry.ID	 = FLASH_FS_ID_SEGMENT;
	hdr.Entry.Length = sizeof(uint32_t);
	TFFS_SET_SEGMENT_VALUE(&hdr, seg_nr);

	pr_debug("[%s] writing segment header to mtd \"%s\"\n", __func__, mtd->mtd->name);
	result = MTD_WRITE_SEGMENT(mtd, 0, &hdr);
	if (result != 0) {
		goto err_out;
	}

	tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(hdr), 1, 0); /*--- internal write ---*/

err_out:
	return result;
}

static int switch_active_segment(struct tffs_lgcy_ctx *ctx, struct tffs_mtd *new)
{
	int result;
	unsigned int entry_cnt, entry_len;

	pr_debug("[%s] activating segment on \"%s\"\n", __func__, new->mtd->name);
	ctx->idx_created = 0;

	result = rescan_segment(ctx, new, RESCAN_UPDATE, &entry_cnt, &entry_len);
	if (result != 0) {
		pr_err("[TFFS] Rescan of new segment on mtd \"%s\" failed.\n", new->mtd->name);
		goto err_out;
	}

	/*------------------------------------------------------------------------------------------*\
	 * new segment is ready for use. activate it
	\*------------------------------------------------------------------------------------------*/
#if defined(FORCE_DEBUG_DEFECT)
	if (force_test != 0) {
		result	   = -ENFILE;
		force_test = 0;
		goto err_out;
	}
#endif

	ctx->active_mtd = new;

err_out:
	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum cleanup_mode {
	cleanup_normal,
	cleanup_failed,
	cleanup_fallback,
	cleanup_hard,
};

static int do_cleanup(struct tffs_module *this)
{
	unsigned int retlen, result;
	unsigned int skip_cnt, orig_entry_len, orig_entry_cnt, new_entry_cnt, new_entry_len;
	enum cleanup_mode mode;
	struct tffs_mtd *curr_mtd, *prev_mtd;
	struct tffs_lgcy_ctx *ctx;
	union _tffs_segment_entry hdr;
	struct _TFFS_Entry Entry;
	loff_t offset, dest_offset, end;
	unsigned int flash_len;
	uint32_t current_number;
	unsigned char *cleanup_buf = NULL;

	result	       = 0;
	skip_cnt       = 0;
	orig_entry_len = 0;
	mode	       = cleanup_normal;

	ctx = (struct tffs_lgcy_ctx *)this->priv;
	if (ctx == NULL) {
		pr_err("[%s] No context given\n", __func__);
		result = -EBADF;
		goto err_out;
	}

	cleanup_buf = kmalloc(CLEANUP_BUFFER_SIZE, GFP_KERNEL);
	if (cleanup_buf == NULL) {
		pr_debug("[%s] malloc(%u) failed\n", __func__, (unsigned int)CLEANUP_BUFFER_SIZE);
		result = -ENOMEM;
		goto err_out;
	}

	curr_mtd = ctx->active_mtd;
	prev_mtd = (curr_mtd == &(ctx->avail_mtd[0])) ? &(ctx->avail_mtd[1]) : &(ctx->avail_mtd[0]);

	end = ctx->active_mtd->size;
	if (ctx->active_mtd->start_bad >= 0) {
		pr_err("[%s] Ignoring entries after offset 0x%llx\n", __func__,
		       ctx->active_mtd->start_bad);
		end = min(end, ctx->active_mtd->start_bad);
	}

	/*------------------------------------------------------------------------------------------*\
	 * collect valid entries 
	\*------------------------------------------------------------------------------------------*/
	/* Save old segment entry so we can write it back later. */
	result = MTD_READ_SEGMENT(ctx->active_mtd, 0, &hdr);
	if (result != 0) {
		pr_err("[TFFS]: cannot read segment for mtd \"%s\".\n", curr_mtd->mtd->name);
		goto err_out;
	}
	tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(hdr), 0, 0); /*--- internal read ---*/

	if (hdr.Entry.ID != FLASH_FS_ID_SEGMENT) {
		pr_err("[TFFS]: First entry invalid for mtd \"%s\".\n", curr_mtd->mtd->name);
		result = -ENFILE;
	} else {
		current_number = TFFS_GET_SEGMENT_VALUE(&hdr);
		pr_debug("[%s] current segment number %u\n", __func__, current_number);
		/**
		 * make sure sequence value is 0 (really 0xFFFFFFFF) so we can update it
		 * after the buffer has been written
		 */
		TFFS_SET_SEGMENT_VALUE(&hdr, 0);
		orig_entry_cnt = 1; /* Just parsed first entry. Start from there. */

		for (offset = align_len(hdr.Entry.Length) + sizeof(struct _TFFS_Entry);
		     offset + sizeof(struct _TFFS_Entry) < end; offset += flash_len) {
			result = MTD_READ_HDR(ctx->active_mtd, offset, &Entry);
			if (result != 0)
				goto err_out;

			tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 0,
					     0); /*--- internal read ---*/

			if (Entry.ID == FLASH_FS_ID_FREE) {
				pr_info("[%s] free area found at offset 0x%llx \n", __func__,
					offset);
				break;
			}

			// calculate offset to next entry header
			flash_len = align_len(Entry.Length) + sizeof(struct _TFFS_Entry);

			// check if length makes sense
			if (Entry.Length > MAX_SEGMENT_SIZE ||
			    (offset + flash_len) >= ctx->active_mtd->size) {
				pr_err("[TFFS] Entry with id 0x%x at 0x%llx has illegal length: 0x%x\n",
				       Entry.ID, offset, Entry.Length);

				result = -ENFILE;
				break;
			}

			// Catch entries with bogus IDs.
			if (Entry.ID >= (unsigned short)FLASH_FS_ID_LAST) {
				pr_warn("[%s] invalid entry id 0x%x with length 0x%x found at offset 0x%llx\n",
					__func__, Entry.ID, Entry.Length, offset);

				result = -ENFILE;
				break;
			}

			// ignore erased entries
			if (Entry.ID == FLASH_FS_ID_SKIP) {
				++skip_cnt;
			} else {
				++orig_entry_cnt;
			}
		}

		/**
		 * the current segment contains no erased or faulty entries.
		 * Copying would be pointless
		 */
		if (skip_cnt == 0 && result == 0 && curr_mtd->start_bad < 0) {
			pr_debug("[TFFS] no IDs skipped, leaving it as it is\n");
			goto err_out;
		}

		pr_info("[%s] %u data records skipped\n", __func__, skip_cnt);
	}

	if (result == -ENFILE || curr_mtd->start_bad > 0) {
		pr_err("[TFFS] Cleanup found inconsistent data\n");

		/*
		 * do a rescan in diagnosis mode of the current segment to dump info
		 * for the panic log
		 */
		(void)rescan_segment(ctx, curr_mtd, RESCAN_DIAG, &new_entry_cnt, &new_entry_len);
	}

	if (result == -ENFILE) {
		pr_err("[TFFS] Current segment unreadable, trying fall back to previous segment\n");

		mode = cleanup_fallback;
	}

	pr_warn("[TFFS] close old mtd \"%s\", open new mtd \"%s\"\n", curr_mtd->mtd->name,
		prev_mtd->mtd->name);

	if (get_mtd_device_wrapped(prev_mtd, prev_mtd->mtd_idx)) {
		pr_err("[TFFS] Can't get mtd \"%s\"\n", prev_mtd->mtd->name);
		result = -ENXIO;
		goto err_out;
	}

	if (mode == cleanup_normal) {
		pr_warn("[TFFS] Format and populate new mtd \"%s\"\n", prev_mtd->mtd->name);
		result = TFFS3_LGCY_Format(prev_mtd);
		if (result != 0) {
			pr_err("[%s] format of mtd \"%s\" failed\n", __func__, prev_mtd->mtd->name);
			goto fail;
		}

		/* Write back segment entry with id set to 0 first. */
		result = MTD_WRITE_SEGMENT(prev_mtd, 0, &hdr);
		if (result != 0) {
			goto fail;
		}

		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 1,
				     0); /*--- internal write ---*/

		/*------------------------------------------------------------------------------------------*\
		 * collect valid entries and copy them one by one to the other segment/mtd
		 \*------------------------------------------------------------------------------------------*/
		/* Start past first entry (segment) */
		dest_offset = align_len(hdr.Entry.Length) + sizeof(struct _TFFS_Entry);
		for (offset = dest_offset; offset + sizeof(struct _TFFS_Entry) < end;
		     offset += flash_len) {
			unsigned char *buf_p;
			unsigned int buf_len;

			result = MTD_READ(ctx->active_mtd, offset, sizeof(Entry), &retlen,
					  cleanup_buf);
			if (result != 0 || retlen != sizeof(Entry)) {
				break;
			}

			tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 0,
					     0); /*--- internal read ---*/
			memcpy(&Entry, cleanup_buf, sizeof(Entry));

			if (Entry.ID == cpu_to_be16(FLASH_FS_ID_FREE)) {
				pr_debug("[%s] free area found at offset 0x%llx \n", __func__,
					 offset);
				break;
			}

			// calculate offset to next entry header starting AFTER current header.
			flash_len = align_len(be16_to_cpu(Entry.Length));
			offset += sizeof(Entry);
			/* Skip sanity checks here. Already did that above. */

			// ignore erased entries
			if (Entry.ID == cpu_to_be16(FLASH_FS_ID_SKIP)) {
				continue;
			}

			/* Don't read the header twice. */
			buf_p	= cleanup_buf + sizeof(Entry);
			buf_len = sizeof(Entry);

			/* Copy this entry over to second mtd. */
			do {
				unsigned int write_retlen;
				unsigned int read_len;

				read_len =
					min(flash_len, (unsigned int)CLEANUP_BUFFER_SIZE - buf_len);
				result =
					MTD_READ(ctx->active_mtd, offset, read_len, &retlen, buf_p);
				if (result != 0) {
					break;
				}

				buf_len += retlen;
				flash_len -= retlen;
				offset += retlen;
				tffs_write_statistic(FLASH_FS_ID_SKIP, retlen, 0,
						     0); /*--- internal read ---*/
				buf_p = cleanup_buf;

				do {
					result = MTD_WRITE(prev_mtd, dest_offset, buf_len,
							   &write_retlen, buf_p);
					if (result != 0) {
						flash_len = 0;
						break;
					}

					tffs_write_statistic(FLASH_FS_ID_SKIP, write_retlen, 1,
							     0); /*--- internal write ---*/
					buf_len -= write_retlen;
					buf_p += write_retlen;
					dest_offset += write_retlen;
				} while (buf_len);

				buf_p = cleanup_buf;
			} while (flash_len);

			if (result != 0) {
				break;
			}
		}

		if (result != 0) {
			goto fail;
		}

		orig_entry_len = dest_offset;

		/**
		 * check consistency of freshly written segment
		 */
		result =
			rescan_segment(ctx, prev_mtd, RESCAN_CHECK, &new_entry_cnt, &new_entry_len);
		if (result != 0) {
			pr_err("[TFFS] Created cleanup segment with inconsistent entries. Aborting.\n");
			goto fail;
		}

		if (new_entry_cnt != orig_entry_cnt || new_entry_len != orig_entry_len) {
			pr_err("[TFFS] Written segment has different count/size: old entries/size: %u/%u new %u/%u\n",
			       orig_entry_cnt, orig_entry_len, new_entry_cnt, new_entry_len);
			result = -ENFILE;
			goto fail;
		}

		/*------------------------------------------------------------------------------------------*\
		 * data successfully written, set segment sequence number
		 \*------------------------------------------------------------------------------------------*/
		++current_number;
		TFFS_SET_SEGMENT_VALUE(&hdr, current_number);

		pr_debug("[%s] setting segment number of mtd \"%s\" to %u\n", __func__,
			 prev_mtd->mtd->name, current_number);
		result = MTD_WRITE_SEGMENT(prev_mtd, 0, &hdr);
		if (result != 0) {
			goto fail;
		}

		tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(hdr), 1,
				     0); /*--- internal write ---*/

		pr_info("[TFFS] Activating new segment\n");
		result = switch_active_segment(ctx, prev_mtd);
		/**
		 * there was either an error while writing the segment or the new
		 * segment was somehow corrupted. Kill it so it won't be picked up
		 * later by mistake.
		 */
		if (result != 0) {
			pr_err("[TFFS] Switching to new segment failed.\n");
		fail:
			pr_err("[TFFS] killing new but corrupted mtd \"%s\", "
			       "triggering panic reboot\n",
			       prev_mtd->mtd->name);
			(void)kill_segment(prev_mtd, 0);

			mode = cleanup_failed;

			/**
			 * we need to rebuild the index for current segment.
			 */
			result = rescan_segment(ctx, curr_mtd, RESCAN_UPDATE, &new_entry_cnt,
						&new_entry_len);
			if (result != 0) {
				pr_err("[TFFS] Re-building index of current segment failed, switching to hard cleanup\n");
				/**
				 * so we have killed the new segment and the current segment
				 * is now also corrupted. Something is seriously wrong here.
				 * Our only chance now is to go berserk and do a hard cleanup.
				 */

				mode = cleanup_hard;
			}
		}
	}

	if (mode == cleanup_fallback) {
		result = kill_segment(curr_mtd, 0);
		if (result != 0) {
			pr_err("[TFFS] Error while killing damaged segment! Have to keep going...\n");
		}

		pr_err("[TFFS] Activating previous segment\n");
		result = switch_active_segment(ctx, prev_mtd);

		if (result != 0) {
			pr_err("[TFFS] Previous segment corrupted, hard re-init needed\n");
			mode = cleanup_hard;
		}
	}

	if (mode == cleanup_hard) {
		/**
		 * Both segments are corrupted. Last chance to get out of this
		 * situation halfway gracefully is a complete reset.
		 */
		result = kill_segment(curr_mtd, 1);
		result |= kill_segment(prev_mtd, 0);
		result |= rescan_segment(ctx, curr_mtd, RESCAN_UPDATE, &new_entry_cnt,
					 &new_entry_len);
		if (result != 0) {
			/**
			 * no feasible way to salvage the situation. Both segments are
			 * unusable. Wipe them completely and let boot loader re-initialise
			 * them
			 */
			pr_err("[TFFS] Error during hard cleanup?! Going postal...\n");
			(void)TFFS3_LGCY_Format(prev_mtd);
			(void)TFFS3_LGCY_Format(curr_mtd);

			/*
			 * Trigger panic from here so TFFS is still locked. Prevents
			 * paniclog from writing to unusable segments
			 */
			panic("[TFFS] Uncorrectable state, forcing reboot\n");
			while (1)
				;
		}
	}

	if (mode != cleanup_normal) {
		/**
		 * we went through an abnormal cleanup and possibly even wiped all
		 * entries. Bootloader will sulk if there is a valid tffs segment
		 * with no environment name table present.
		 */
		if (TFFS_Global_Index[FLASH_FS_NAME_TABLE] < 0) {
			/* This needs FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY = 64k of buffer.
			 * TODO: May likely fail on boxes with very little RAM. Need to
			 *       re-write the build_name_table stuff if this proves to be a
			 *       real problem.
			 */
			kfree(cleanup_buf);
			cleanup_buf = kmalloc(FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY, GFP_KERNEL);
			result	    = -ENOMEM;
			if (cleanup_buf != NULL) {
				orig_entry_len = avm_urlader_build_name_table(
					cleanup_buf, FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY);
				if (orig_entry_len > 0) {
					result = do_write(ctx, FLASH_FS_NAME_TABLE, cleanup_buf,
							  orig_entry_len, &retlen, NULL);
				}
			}

			if (result != 0) {
				/**
				 * Oh ffs, really?!
				 */
				pr_err("[TFFS] Writing name table failed, wiping both segments\n");
				(void)TFFS3_LGCY_Format(prev_mtd);
				(void)TFFS3_LGCY_Format(curr_mtd);

				/*
				 * Trigger panic from here so TFFS is still locked. Prevents
				 * paniclog from writing to unusable segments
				 */
				panic("[TFFS] Uncorrectable state, forcing reboot\n");
				while (1)
					;
			}
		}
	}

	/**
	 * release unused mtd
	 */
	curr_mtd = ctx->active_mtd;
	prev_mtd = (curr_mtd == &(ctx->avail_mtd[0])) ? &(ctx->avail_mtd[1]) : &(ctx->avail_mtd[0]);
	put_mtd_device_wrapped(prev_mtd);

err_out:
	kfree(cleanup_buf);
	/**
	 * trigger asynchronous panic. We can't do it directly because we are
	 * still holding the TFFS semaphore and would therefore block the panic
	 * log.
	 */
	if (mode != cleanup_normal) {
		TFFS3_Panic_Lock();
		tffs_send_event(TFFS_EVENT_PANIC);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Cleanup(struct tffs_module *this, void *handle)
{
	struct tffs_lgcy_ctx *ctx;
	struct TFFS_LGCY_State *state;
	int result;

	result = 0;
	ctx    = (struct tffs_lgcy_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] No context given.\n", __func__);
		result = -EBADF;
		goto err_out;
	}

	state = (struct TFFS_LGCY_State *)handle;
	if (state->id != 0) {
		pr_err("[%s] Called from non-management handle-id: 0x%x, aborting\n", __func__,
		       state->id);
		result = -EINVAL;
		goto err_out;
	}

	pr_info("[%s] Starting Cleanup\n", __func__);
	result = do_cleanup(this);
	pr_info("[%s] Cleanup %s.\n", __func__, (result == 0) ? "finished" : "failed");

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Info(struct tffs_module *this, unsigned int *Fill)
{
	struct tffs_lgcy_ctx *ctx;
	unsigned int ret;
	struct _TFFS_Entry Entry;
	loff_t offset, max_offset;
	unsigned int Count;

	ctx = (struct tffs_lgcy_ctx *)this->priv;

	offset	   = 0;
	max_offset = 0;
	for (Count = 0; Count < FLASH_FS_ID_LAST; ++Count) {
		offset = TFFS_Global_Index[Count];
		if (offset >= 0) {
			max_offset = max(offset, max_offset);
		}
	}
	if (max_offset == 0) {
		*Fill = 0;
		return 0;
	}

	ret = MTD_READ_HDR(ctx->active_mtd, max_offset, &Entry);
	if (ret) {
		return ret;
	}

	tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 0,
			     0); /*--- internal read ---*/

	max_offset += align_len(Entry.Length);

	*Fill = ((int)max_offset * 100) / (unsigned int)(ctx->active_mtd->size);

	return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_LGCY_Register_Notify(struct tffs_module *this, void *notify_priv,
				      tffs3_notify_fn notify_cb)
{
	struct tffs_lgcy_ctx *ctx;
	int result;

	ctx = (struct tffs_lgcy_ctx *)this->priv;

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
static int TFFS3_LGCY_Remove_Notify(struct tffs_module *this, void *notify_priv,
				    tffs3_notify_fn notify_cb)
{
	struct tffs_lgcy_ctx *ctx;
	int result;

	ctx = (struct tffs_lgcy_ctx *)this->priv;

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
static int TFFS3_LGCY_Setup(struct tffs_module *this)
{
	struct tffs_lgcy_ctx *ctx;
	union _tffs_segment_entry u;
	unsigned int i;
	int result;

	pr_debug("[%s] Called\n", __func__);

	if (this == NULL || this->priv == NULL) {
		pr_err("[%s] Module or context information missing!\n", __func__);
		result = -EINVAL;
		goto err_out;
	}
	ctx = (struct tffs_lgcy_ctx *)this->priv;

	memset(&u, 0x0, sizeof(u));
	result = 0;

	result = get_mtd_device_wrapped(&(ctx->avail_mtd[0]), ctx->mtd_num[0]);
	if (result != 0) {
		pr_err("[%s] Unable to get mtd%u\n", __func__, ctx->mtd_num[0]);
		goto err_out;
	}

	result = get_mtd_device_wrapped(&(ctx->avail_mtd[1]), ctx->mtd_num[1]);
	if (result != 0) {
		pr_err("[%s] Unable to get mtd%u\n", __func__, ctx->mtd_num[1]);
		goto err_out;
	}

	pr_info("[%s] using mtd%u(%s), mtd%u(%s)\n", __func__, ctx->mtd_num[0],
		ctx->avail_mtd[0].mtd->name, ctx->mtd_num[1], ctx->avail_mtd[1].mtd->name);

	/*------------------------------------------------------------------------------------------*\
	 * pruefen welcher buffer der "richtige" ist
	 \*------------------------------------------------------------------------------------------*/

	result = MTD_READ_SEGMENT(&(ctx->avail_mtd[0]), 0, &u);
	if (result != 0) {
		pr_err("[%s] MTD read failed, could not read a complete _tffs_segment_entry from %s\n",
		       __func__, ctx->avail_mtd[0].mtd->name);
	} else {
		if (u.Entry.ID == FLASH_FS_ID_SEGMENT) {
			ctx->avail_mtd[0].segment_id = TFFS_GET_SEGMENT_VALUE(&u);
			pr_info("[%s] mtd \"%s\": segment value %u\n", __func__,
				ctx->avail_mtd[0].mtd->name, ctx->avail_mtd[0].segment_id);
		} else {
			ctx->avail_mtd[0].segment_id = 0;
			pr_info("[%s] mtd \"%s\": no SEGMENT VALUE (0x%x)\n", __func__,
				ctx->avail_mtd[0].mtd->name, u.Entry.ID);
		}
	}
	tffs_write_statistic(u.Entry.ID, sizeof(struct _TFFS_Entry), 0, 0);

	result = MTD_READ_SEGMENT(&(ctx->avail_mtd[1]), 0, &u);
	if (result != 0) {
		pr_err("[%s] MTD read failed, could not read a complete _tffs_segment_entry from %s\n",
		       __func__, ctx->avail_mtd[1].mtd->name);
	} else {
		if (u.Entry.ID == FLASH_FS_ID_SEGMENT) {
			ctx->avail_mtd[1].segment_id = TFFS_GET_SEGMENT_VALUE(&u);
			pr_info("[%s] mtd \"%s\": segment value %u\n", __func__,
				ctx->avail_mtd[1].mtd->name, ctx->avail_mtd[1].segment_id);
		} else {
			ctx->avail_mtd[1].segment_id = 0;
			pr_info("[%s] mtd \"%s\": no SEGMENT VALUE (0x%x)\n", __func__,
				ctx->avail_mtd[1].mtd->name, u.Entry.ID);
		}
	}
	tffs_write_statistic(u.Entry.ID, sizeof(struct _TFFS_Entry), 0, 0);

	if (ctx->avail_mtd[0].segment_id == 0 && ctx->avail_mtd[1].segment_id == 0) {
		panic("[%s] no valid filesystem on mtds \"%s\" and \"%s\"!\n", __func__,
		      ctx->avail_mtd[0].mtd->name, ctx->avail_mtd[1].mtd->name);
	}

	if (ctx->avail_mtd[0].segment_id > ctx->avail_mtd[1].segment_id) {
		put_mtd_device_wrapped(&(ctx->avail_mtd[1]));
		ctx->active_mtd = &(ctx->avail_mtd[0]);
	} else {
		put_mtd_device_wrapped(&(ctx->avail_mtd[0]));
		ctx->active_mtd = &(ctx->avail_mtd[1]);
	}

	pr_info("[%s] Using segment %u (avail: %u + %u)\n", __func__, ctx->active_mtd->segment_id,
		ctx->avail_mtd[0].segment_id, ctx->avail_mtd[1].segment_id);

	pr_info("[%s] mtd%u size=0x%llx\n", __func__, ctx->active_mtd->mtd_idx,
		ctx->active_mtd->mtd->size);

	// clear global index and make sure it gets initialised when first file
	// handle gets opened
	for (i = 0; i < ARRAY_SIZE(TFFS_Global_Index); ++i) {
		TFFS_Global_Index[i] = -1;
	}
	ctx->idx_created = 0;

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_LGCY_Configure(struct tffs_module *this, int mtd_num0, int mtd_num1)
{
	struct tffs_lgcy_ctx *ctx;
	int result;

	pr_debug("[%s] Called\n", __func__);

	result = -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		pr_err("[%s] Out of memory error\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	this->name	      = "legacy";
	this->setup	      = TFFS3_LGCY_Setup;
	this->open	      = TFFS3_LGCY_Open;
	this->close	      = TFFS3_LGCY_Close;
	this->read	      = TFFS3_LGCY_Read;
	this->write	      = TFFS3_LGCY_Write;
	this->cleanup	      = TFFS3_LGCY_Cleanup;
	this->reindex	      = TFFS3_LGCY_Reindex;
	this->info	      = TFFS3_LGCY_Info;
	this->register_notify = TFFS3_LGCY_Register_Notify;
	this->remove_notify   = TFFS3_LGCY_Remove_Notify;

	ctx->mtd_num[0] = mtd_num0;
	ctx->mtd_num[1] = mtd_num1;
	ctx->panic_mode = 0;

	this->priv = ctx;

	result = 0;

err_out:
	return result;
}
EXPORT_SYMBOL(TFFS3_LGCY_Configure);
