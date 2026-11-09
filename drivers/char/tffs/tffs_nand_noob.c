/*
 * tffs_nand.c
 *
 *  Created on: 09.04.2014
 *      Author: tklaassen
 */
/*------------------------------------------------------------------------------------------*\
 *
 *   Copyright (C) 2004-2014 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <avm/tffs/tffs.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <avm/tffs/tffs.h>
#include "tffs_local.h"
#include "tffs_nand_noob.h"

#define TFFS_CRC_INIT_VAL -1

//#define DEBUG_CHAINS

#define MAX_SUBPAGE_NUM 1

enum defect_type {
    defect_hdr,
    defect_tag,
    defect_new,
};

static struct TFFS_Entry_Index *get_index(struct tffs_nand_ctx *ctx, enum _tffs_id Id);
static int page_is_bad(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t offset);
#if 0
static int mark_page_bad(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off);
#endif
static int add_entry_to_index(struct tffs_nand_ctx *ctx, struct TFFS_Entry_Index *idx, struct TFFS_NAND_Entry *entry, unsigned int prunable);
static inline void invalidate_segment_chain(struct TFFS_NAND_SegChain *chain);
static void validate_segment_chain(struct tffs_nand_ctx *ctx, struct TFFS_NAND_SegChain *chain);
static int block_mark_page_bad(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off, enum defect_type type);
static void release_segment_chain(struct kref *refcnt);
static int format_block(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *block, uint32_t blkseq_nr);
static int clean_block(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk);
static int recycle_blocks(struct tffs_nand_ctx *ctx, unsigned int async);
static void free_entry(struct TFFS_NAND_Entry *entry);


static inline int put_chain(struct TFFS_NAND_SegChain *chain)
{
    // pr_err("[%s] chain: %p\n", __func__, chain);
    return kref_put(&(chain->refcnt), release_segment_chain);
}

static inline void get_chain(struct TFFS_NAND_SegChain *chain)
{
    // pr_err("[%s] chain: %p\n", __func__, chain);
    kref_get(&(chain->refcnt));
}

static inline size_t padded_entry_len(size_t data_len, size_t hdr_len, struct tffs_nand_ctx *ctx)
{
    return DIV_ROUND_UP((data_len + hdr_len), ctx->sector_size) * ctx->sector_size;
}

static inline size_t max_block_space(struct tffs_nand_ctx *ctx)
{
    return ctx->mtd->erasesize - ((TFFS3_MAX_BADPAGES + 1) << ctx->buffer_sft);
}

static void handle_corrupted_block(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk)
{
    struct TFFS_NAND_Entry *entry, *tmp;
    struct list_head *curr, *next;
    pr_debug("[%s] Called\n", __func__);

    /* this is tricky...
     * Since we are called from the low-level read/write_oob functions, we can not simply
     * free all entries in the block, but we do have to invalidate all chains containing
     * entries from it. Invalidation can trigger freeing all entries from that chain and a
     * block may contain multiple entries from a single chain.
     * There is no (easy) telling in which order the chain's entries appear in the block's
     * entry list or how many entries there are. So even a list_for_each_safe will not
     * save us from having the rug pulled from beneath our feet.
     * To prevent this, we have to manually adjust the next ptr to point to the first entry
     * from a different chain.
     *
     * 1. get current entry
     * 2. move next ptr forward until we are at an entry from a different chain or at the
     *    end of the list
     * 3. call invalidate() on the current entry
     * 4. let list_for_each_safe continue from adjusted next ptr
     */
    list_for_each_safe(curr, next, &(blk->blk_entry_list)){
        entry = list_entry(curr, struct TFFS_NAND_Entry, blk_entry_list);
        if(entry->chain != NULL){
            while(next != &(blk->blk_entry_list)){
                tmp = list_entry(next, struct TFFS_NAND_Entry, blk_entry_list);
                if(tmp->chain == NULL || tmp->chain != entry->chain){
                    break;
                }
                next = next->next;
            }
            invalidate_segment_chain(entry->chain);
        }
    }
}

#if defined(CONFIG_TFFS_NAND_V31)
static int write_noob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off, size_t *write_len, char *write_buf)
{
    int result, blk_bad;
    size_t retlen;
    struct mtd_ecc_stats stats;

    if(page_is_bad(ctx, blk, page_off)){
        pr_debug("[%s] page_is_bad for Addr 0x%llx\n", __func__, blk->blk_addr + page_off);
        return -EIO;
    }

    stats = ctx->mtd->ecc_stats;
    result = mtd_write(ctx->mtd, blk->blk_addr + page_off, *write_len, &retlen, write_buf);
    *write_len = retlen;
    pr_debug("[%s] Writing page @offs 0x%llx; len 0x%x; result: %d\n", __func__, blk->blk_addr + page_off, *write_len, result);

    // skip block and index management in panic mode
    if(ctx->in_panic_mode){
        goto err_out;
    }

    if(result == 0 && stats.corrected < ctx->mtd->ecc_stats.corrected){
        pr_warning("[TFFS3-NAND] Block at address 0x%llx needs to be rewritten\n", blk->blk_addr);
        ++blk->needs_rewrite;
    }

    if(result == -EIO){
        // Toshiba BeNAND seems to have internal bad block management.
        // Check if it has been triggered
        blk_bad = mtd_block_isbad(ctx->mtd, blk->blk_addr);
        blk_bad = blk_bad != 0 ? -EIO : 0;
        pr_debug("[%s] mtd->block_isbad(0x%llx): %d\n", __func__, blk->blk_addr, blk_bad);

        // update the block's bad page list. Returns -EIO if limit has been crossed
        if(blk_bad == 0){
            blk_bad = block_mark_page_bad(ctx, blk, page_off, defect_new);
            pr_debug("[%s] block_mark_page_bad(0x%llx): %d\n", __func__, page_off, blk_bad);
        }

#if 0
        // we still have not reached the bad page limit. Try to write marker to page oob
        if(blk_bad == 0){
            blk_bad = mark_page_bad(ctx, blk, page_off);
            pr_debug("[%s] mark_page_bad(0x%llx): %d\n", __func__, page_off, blk_bad);
        }
#endif
        // if the block is unusable, write marker and mark all chains containing entries from
        // this block as corrupted.
        if(blk_bad == -EIO){
            mtd_block_markbad(ctx->mtd, blk->blk_addr);
            handle_corrupted_block(ctx, blk);
            blk->state = tffs_blk_bad;
        }
    }

err_out:
    return result;
}
#endif

static int write_oob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off, struct mtd_oob_ops *oob_ops)
{
    int result, blk_bad;
    struct mtd_ecc_stats stats;

    if(page_is_bad(ctx, blk, page_off)){
        return -EIO;
    }

    stats = ctx->mtd->ecc_stats;
    result = mtd_write_oob(ctx->mtd, blk->blk_addr + page_off, oob_ops);

    // skip block and index management in panic mode
    if(ctx->in_panic_mode){
        goto err_out;
    }

    if(result == 0 && stats.corrected < ctx->mtd->ecc_stats.corrected){
        pr_warning("[TFFS3-NAND] Block at address 0x%llx needs to be rewritten\n", blk->blk_addr);
        ++blk->needs_rewrite;
    }

    if(result == -EIO){
        // Toshiba BeNAND seems to have internal bad block management.
        // Check if it has been triggered
        blk_bad = mtd_block_isbad(ctx->mtd, blk->blk_addr);
        blk_bad = blk_bad != 0 ? -EIO : 0;
        pr_debug("[%s] mtd->block_isbad(0x%llx): %d\n", __func__, blk->blk_addr, blk_bad);

        // update the block's bad page list. Returns -EIO if limit has been crossed
        if(blk_bad == 0){
            blk_bad = block_mark_page_bad(ctx, blk, page_off, defect_new);
            pr_debug("[%s] block_mark_page_bad(0x%llx): %d\n", __func__, page_off, blk_bad);
        }

#if 0
        // we still have not reached the bad page limit. Try to write marker to page oob
        if(blk_bad == 0){
            blk_bad = mark_page_bad(ctx, blk, page_off);
            pr_debug("[%s] mark_page_bad(0x%llx): %d\n", __func__, page_off, blk_bad);
        }
#endif
        // if the block is unusable, write marker and mark all chains containing entries from
        // this block as corrupted.
        if(blk_bad == -EIO){
            mtd_block_markbad(ctx->mtd, blk->blk_addr);
            handle_corrupted_block(ctx, blk);
            blk->state = tffs_blk_bad;
        }
    }

err_out:
    return result;
}

#if defined(CONFIG_TFFS_NAND_V31)
static int read_noob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off, size_t *read_len, char *read_buf)
{
    int result, blk_bad;
    size_t retlen;
    struct mtd_ecc_stats stats;

    if(page_is_bad(ctx, blk, page_off)){
        pr_debug("[%s] page_is_bad at addr 0x%llx\n", __func__, (long long)(blk->blk_addr + page_off));
        return -EIO;
    }

    stats = ctx->mtd->ecc_stats;
    result = mtd_read(ctx->mtd, blk->blk_addr + page_off, *read_len, &retlen, read_buf);
    *read_len = retlen;

    // skip block and index management in panic mode
    if(ctx->in_panic_mode){
        goto err_out;
    }

#if 0
    /* DEBUG */
    if(result || *read_len != retlen){
        pr_err("[%s] mtd_read at addr 0x%llx failed: %d (retlen: %d/%d)\n",
                __func__, (long long)(blk->blk_addr + page_off),
                result, (int)retlen, (int)*read_len);
    }
#endif

    if(result == 0 && stats.corrected < ctx->mtd->ecc_stats.corrected){
        pr_debug("[%s] Block at address 0x%llx needs to be rewritten\n", __func__, blk->blk_addr);
        ++blk->needs_rewrite;
    }

    if(result == -EIO){
        // Toshiba BeNAND seems to have internal bad block management.
        // Check if it has been triggered
        blk_bad = mtd_block_isbad(ctx->mtd, blk->blk_addr);
        blk_bad = blk_bad != 0 ? -EIO : 0;
        pr_debug("[%s] mtd->block_isbad(0x%llx): %d\n", __func__, blk->blk_addr, blk_bad);

        // update the block's bad page list. Returns -EIO if limit has been crossed
        if(blk_bad == 0){
            blk_bad = block_mark_page_bad(ctx, blk, page_off, defect_new);
            pr_debug("[%s] block_mark_page_bad(0x%llx): %d\n", __func__, page_off, blk_bad);
        }

        // we can not mark the page bad, as it probably lies before the block's last
        // write position.

        // if the block is unusable, write marker and mark all chains containing entries from
        // this block as corrupted.
        if(blk_bad == -EIO){
            mtd_block_markbad(ctx->mtd, blk->blk_addr);
            handle_corrupted_block(ctx, blk);
            blk->state = tffs_blk_bad;
        }
    }

err_out:
    return result;
}
#endif

static int read_oob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off, struct mtd_oob_ops *oob_ops)
{
    int result, blk_bad;
    struct mtd_ecc_stats stats;

    if(page_is_bad(ctx, blk, page_off)){
        return -EIO;
    }

    stats = ctx->mtd->ecc_stats;
    result = mtd_read_oob(ctx->mtd, blk->blk_addr + page_off, oob_ops);

    // skip block and index management in panic mode
    if(ctx->in_panic_mode){
        goto err_out;
    }

    if(result == 0 && stats.corrected < ctx->mtd->ecc_stats.corrected){
        pr_debug("[%s] Block at address 0x%llx needs to be rewritten\n", __func__, blk->blk_addr);
        ++blk->needs_rewrite;
    }

    if(result == -EIO){
        // Toshiba BeNAND seems to have internal bad block management.
        // Check if it has been triggered
        blk_bad = mtd_block_isbad(ctx->mtd, blk->blk_addr);
        blk_bad = blk_bad != 0 ? -EIO : 0;
        pr_debug("[%s] mtd->block_isbad(0x%llx): %d\n", __func__, blk->blk_addr, blk_bad);

        // update the block's bad page list. Returns -EIO if limit has been crossed
        if(blk_bad == 0){
            blk_bad = block_mark_page_bad(ctx, blk, page_off, defect_new);
            pr_debug("[%s] block_mark_page_bad(0x%llx): %d\n", __func__, page_off, blk_bad);
        }

        // we can not mark the page bad, as it probably lies before the block's last
        // write position.

        // if the block is unusable, write marker and mark all chains containing entries from
        // this block as corrupted.
        if(blk_bad == -EIO){
            mtd_block_markbad(ctx->mtd, blk->blk_addr);
            handle_corrupted_block(ctx, blk);
            blk->state = tffs_blk_bad;
        }
    }

err_out:
    return result;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void* TFFS3_NAND_Open(struct tffs_module *this, struct tffs_core_handle *handle)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_State *state;

    //pr_err("[%s] called for id: 0x%x; mode: 0x%x\n", __func__, handle->id, handle->mode);

    state = NULL;
    ctx = (struct tffs_nand_ctx *) this->priv;
    if(ctx == NULL){
        pr_err("[TFFS3-NAND] TFFS device not initialised\n");
        goto err_out;
    }

    // when opened in panic mode, use static state struct
    if(handle->mode == tffs3_mode_panic){
        if(ctx->in_panic_mode == 0){
            state = &(ctx->panic_state);
            ctx->in_panic_mode = 1;
        }
    } else {
        state = kzalloc(sizeof(*state), GFP_KERNEL);
    }

    if(state == NULL) {
        pr_debug("[%s] malloc(%u) failed\n", __func__, (int)sizeof(struct TFFS_NAND_State));
        goto err_out;
    }

    memset(state, 0x0, sizeof(*state));
    state->id = handle->id;

    // As soon as we use TFFS3.1 blocks, we shrink the maximum segment size to fit TFFS3.1 entries.
    handle->max_segment_size = ctx->sector_size - (ctx->flags.got_tffs31_blocks ? sizeof(struct _TFFS_NAND_Entry_31) : sizeof(struct _TFFS_NAND_Entry)); //max_block_space(ctx) - sizeof(struct _TFFS_NAND_Entry);
//    open_data->max_segment_size = max_block_space(ctx) - sizeof(struct _TFFS_NAND_Entry);

#if 0
    pr_err("[%s] sizeof(*state): 0x%x sizeof *state: 0x%x, sizeof(struct TFFS_NAND_State): 0x%x\n", __func__, sizeof(*state), sizeof *state, sizeof(struct TFFS_NAND_State));
    pr_err("[%s] state: curr_entry: %p, offset: 0x%llx, revision: 0x%x, segment: 0x%x, next_segment: 0x%x segment_size: 0x%x\n",
            __func__,
            state->curr_entry,
            state->offset,
            state->revision,
            state->segment,
            state->next_segment,
            state->segment_size);
    pr_err("[%s] max_segment_size: 0x%x\n", __func__, (int)handle->max_segment_size);
#endif

err_out:
    return state;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Close(struct tffs_module *this, void *handle)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_State *state;
    int result;

    // pr_err("[%s] called\n", __func__);

    result = 0;
    ctx = (struct tffs_nand_ctx *) this->priv;
    if(ctx == NULL){
        pr_err("[TFFS3-NAND] TFFS device not initialised\n");
        result = -ENODEV;
        goto err_out;
    }

    state = (struct TFFS_NAND_State *) handle;
    if(state == NULL){
        result = -ENODEV;
        goto err_out;;
    }

    if(ctx->in_panic_mode == 0){
        if(state->chain != NULL){
            put_chain(state->chain);
        }
        state->curr_entry = NULL;
        state->chain = NULL;
        kfree(state);
    } else {
        ctx->in_panic_mode = 0;
    }
    mtd_sync(ctx->mtd);

err_out:
    return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void entry_to_hdr(struct TFFS_NAND_Entry *entry, struct _TFFS_NAND_Entry *hdr)
{
    hdr->ID = cpu_to_be32(entry->ID);
    hdr->Length = cpu_to_be32(entry->Length);
    hdr->segment_nr = cpu_to_be32(entry->segment_nr);
    hdr->next_segment = cpu_to_be32(entry->next_segment);
    hdr->revision_nr = cpu_to_be32(entry->revision);
    hdr->timestamp = cpu_to_be32(entry->timestamp);
}

#if defined(CONFIG_TFFS_NAND_V31)
static void entry_to_hdr_31(struct TFFS_NAND_Entry *entry, struct _TFFS_NAND_Entry_31 *hdr)
{
    hdr->ID = cpu_to_be32(entry->ID);
    hdr->Length = cpu_to_be32(entry->Length);
    hdr->segment_nr = cpu_to_be32(entry->segment_nr);
    hdr->next_segment = cpu_to_be32(entry->next_segment);
    hdr->revision_nr = cpu_to_be32(entry->revision);
    hdr->timestamp = cpu_to_be32(entry->timestamp);
    hdr->magic = cpu_to_be64(TFFS_ENTRY_MAGIC);
    hdr->crc = 0; // Calculate crc later with crc set to 0 here.
}
#endif

#if 0
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void hdr_to_entry(struct _TFFS_NAND_Entry *hdr, struct TFFS_NAND_Entry *entry)
{
    entry->ID = be32_to_cpu(hdr->ID);
    entry->Length = be32_to_cpu(hdr->Length);
    entry->segment_nr = be32_to_cpu(hdr->segment_nr);
    entry->next_segment = be32_to_cpu(hdr->next_segment);
    entry->revision = be32_to_cpu(hdr->revision_nr);
    entry->timestamp = be32_to_cpu(hdr->timestamp);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void entry_to_oob(struct TFFS_NAND_Entry *entry, struct _TFFS_NAND_OOB *oob)
{
    oob->ID = cpu_to_be32(entry->ID);
    oob->Length = cpu_to_be32(entry->Length);
    oob->Revision = cpu_to_be32(entry->revision);
}
#endif

static struct TFFS_NAND_Block *find_free_block(struct tffs_nand_ctx *ctx, size_t data_len, uint32_t not_seq, enum tffs_srch_param srch_param)
{
    struct TFFS_NAND_Block *blk, *best_blk;
    size_t padded_len;
    unsigned int better;

    best_blk = NULL;
    list_for_each_entry(blk, &(ctx->blk_list), blk_list){
        padded_len = padded_entry_len(data_len, blk->hdr_size, ctx);
        if(blk->state == tffs_blk_active && blk->free_space >= padded_len && blk->blkseq_nr != not_seq){
            if(best_blk == NULL){
                best_blk = blk;
            } else {
                better = 0;
                switch(srch_param){
                case  tffs_srch_min_spc:
                    if(blk->free_space < best_blk->free_space){
                        better = 1;
                    }
                    break;
                case  tffs_srch_max_spc:
                    if(blk->free_space > best_blk->free_space){
                        better = 1;
                    }
                    break;
                case  tffs_srch_min_seq:
                    if(blk->blkseq_nr < best_blk->blkseq_nr){
                        better = 1;
                    }
                    break;
                case  tffs_srch_max_seq:
                    if(blk->blkseq_nr > best_blk->blkseq_nr){
                        better = 1;
                    }
                    break;
                case  tffs_srch_min_erase:
                    if(blk->erase_cnt < best_blk->erase_cnt){
                        better = 1;
                    }
                    break;
                case  tffs_srch_max_erase:
                    if(blk->erase_cnt > best_blk->erase_cnt){
                        better = 1;
                    }
                    break;
                }
                if(better && blk->needs_rewrite < best_blk->needs_rewrite){
                    best_blk = blk;
                }
            }
        }
    }

    return best_blk;
}

int num_free_blocks(struct tffs_nand_ctx *ctx)
{
    struct TFFS_NAND_Block *blk;
    int result;

    result = 0;
    list_for_each_entry(blk, &(ctx->blk_list), blk_list){
        // pr_err("[%s] addr: 0x%llx state: 0x%x free_space: 0x%x used_space: 0x%x max_block_space: 0x%x\n", __func__, blk->blk_addr, blk->state, blk->free_space, blk->used_space, max_block_space(ctx));
        if(blk->state == tffs_blk_active && blk->free_space >= max_block_space(ctx)){
            ++result;
        }
    }

    // pr_err("[%s] free blocks found: %d\n", __func__, result);
    return result;
}

#if defined(CONFIG_TFFS_NAND_V31)
#if defined(CONFIG_TFFS_VERIFY_WRITE)
static struct TFFS_NAND_Entry *parse_entry_noob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t start_page, loff_t first_sect);
#endif

static int do_write_noob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Entry *entry,
        struct TFFS_NAND_Block *blk, uint8_t *data_buf, size_t data_len, size_t *retlen)
{
    struct _TFFS_NAND_Entry_31 entry_hdr;
    uint8_t *write_buf, *write_ptr, *data_ptr;
    loff_t page_off, entry_start;
    uint32_t start_sect, end_sect, num_sect;
    size_t to_write, to_copy, buffer_len, data_written, tmp_retlen;
    int result;
    unsigned int i;
#if defined(CONFIG_TFFS_VERIFY_WRITE)
    loff_t page_start, sect_start;
    struct TFFS_NAND_Entry *tmp_entry;
#endif

    *retlen = 0;

    write_buf = ctx->rw_buffer;
    if(write_buf == NULL){
        result = -ENOMEM;
        goto err_out;
    }

    if(blk->free_space < (data_len + sizeof(entry_hdr))){
        result = -ENOSPC;
        goto err_out;
    }

    entry_to_hdr_31(entry, &entry_hdr);
    i = crc32_be(TFFS_CRC_INIT_VAL, (const unsigned char *)&entry_hdr, sizeof(entry_hdr));
    i = crc32_be(i, data_buf, data_len);
    entry_hdr.crc = cpu_to_be32(i);
#if 0
    /* DEBUG */
    j = crc32_be(TFFS_CRC_INIT_VAL, data_buf, data_len);
    pr_info("[%s] crc32: 0x%08x (hdr_len=0x%x + data_len=0x%x; data_crc: 0x%08x)\n", __func__, i,
            (int)sizeof(entry_hdr), data_len, j);
#endif
#if defined(CONFIG_TFFS_VERIFY_WRITE)
    page_start = blk->free_start & ~ctx->buffer_msk;
    sect_start = (blk->free_start - page_start) >> ctx->sector_sft;
#endif

    data_ptr = data_buf;
    data_written = 0;
    entry_start = 0;
    do{
        // failed write might have changed block state. Check and bail if block turned bad
        if(blk->state == tffs_blk_bad){
            result = -EIO;
            goto err_out;
        }

        /**
         * get address of page containing start of free space and calculate
         * index of start sector
         */
        page_off = blk->free_start & ~ctx->buffer_msk;
        start_sect = (blk->free_start - page_off) >> ctx->sector_sft;

        buffer_len = ctx->buffer_size;
        write_ptr = write_buf + (start_sect * ctx->sector_size);
        buffer_len -= (start_sect * ctx->sector_size);
        to_write = 0;

        // prepare buffers
        memset(write_buf, 0xff, ctx->buffer_size);

        // put header before actual data
        // it should be safe to assume that the entry header is smaller than one whole sector
        if(data_written == 0){
            memcpy(write_ptr, &entry_hdr, sizeof(entry_hdr));
            write_ptr += sizeof(entry_hdr);
            buffer_len -= sizeof(entry_hdr);
            to_write = sizeof(entry_hdr);

            entry_start = blk->free_start;
        }

        // copy data to the right place in the write buffer
        to_copy = min(buffer_len, data_len - data_written);
        to_write += to_copy;
        if(to_copy > 0 && data_ptr != NULL){
            memcpy(write_ptr, data_ptr, to_copy);
        }

        num_sect = DIV_ROUND_UP(to_write, ctx->sector_size);
        end_sect = min(start_sect + num_sect, ctx->sectors_per_page) - 1;

        // pr_err("[%s] to_write: 0x%x start_sect: 0x%x end_sect: 0x%x, num_sect: 0x%x\n", __func__, to_write, start_sect, end_sect, num_sect);

        tmp_retlen = num_sect * ctx->sector_size;
        result = write_noob(ctx, blk, page_off + (start_sect * ctx->sector_size), &tmp_retlen, write_buf + (start_sect * ctx->sector_size));

        // pr_err("[%s] result: %d data_written: 0x%x retlen: 0x%x oobretlen: 0x%x\n", __func__, result, data_written, oob_ops.retlen, oob_ops.oobretlen);
        if(result == 0 && tmp_retlen == num_sect * ctx->sector_size){
            /* successful write.
             * Adjust block accounting info, move write data pointer forward and update
             * the returned written length
             */
            blk->free_start += num_sect * ctx->sector_size;
            blk->free_space -= num_sect * ctx->sector_size;
            data_ptr += to_copy;
            data_written += to_copy;
            *retlen = data_written;
        } else {
            /* write_noob() already updated the bad page info for this block. Set free_start
             * to start of next page.
             * The data will be written again during the next loop iteration, unless the whole
             * block turned bad.
             */
            blk->free_start = page_off + ctx->buffer_size;
            result = -EIO;

            /* skip complex error handling in panic mode. Just report back the write error
             * and let the upper layers sort out how to handle it.
             */
            if(ctx->in_panic_mode != 0){
                *retlen = 0;
                goto err_out;
            }
        }
    }while(result != -EIO && data_written < data_len);

    // write successful, update entry data and block accounting info
    result = 0;
    entry->block_ptr = blk;
    entry->blk_offset = entry_start;
    entry->flash_len = blk->free_start - entry_start;

    pr_debug("[%s] wrote ID: 0x%x Rev: 0x%x Len: 0x%x(0x%x/0x%x) at block 0x%llx offset 0x%llx\n",
            __func__, entry->ID, entry->revision, entry->Length, entry->padded_len, entry->flash_len, entry->block_ptr->blk_addr, entry->blk_offset);

#if defined(CONFIG_TFFS_VERIFY_WRITE)
    tmp_entry = parse_entry_noob(ctx, blk, page_start, sect_start);
    if(tmp_entry){
        kfree(tmp_entry);
    } else {
        pr_err("Parsing written entry failed!");
        result = -EIO;
    }
#endif

err_out:
    return result;
}
#endif

static int do_write(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Entry *entry,
        struct TFFS_NAND_Block *blk, uint8_t *data_buf, size_t data_len, size_t *retlen)
{
    struct _TFFS_NAND_OOB oob_hdrs[MAX_SUBPAGE_NUM];
    struct _TFFS_NAND_Entry entry_hdr;
    struct mtd_oob_ops oob_ops;
    uint8_t *write_buf, *write_ptr, *data_ptr;
    loff_t page_off, entry_start;
    uint32_t start_sect, end_sect, num_sect;
    size_t to_write, to_copy, buffer_len, data_written;
    int result;
    unsigned int i;

    *retlen = 0;

    write_buf = ctx->rw_buffer;
    if(write_buf == NULL){
        result = -ENOMEM;
        goto err_out;
    }

    if(blk->free_space < (data_len + sizeof(entry_hdr))){
        result = -ENOSPC;
        goto err_out;
    }

    entry_to_hdr(entry, &entry_hdr);

    data_ptr = data_buf;
    data_written = 0;
    entry_start = 0;
    do{
        // failed write might have changed block state. Check and bail if block turned bad
        if(blk->state == tffs_blk_bad){
            result = -EIO;
            goto err_out;
        }

        /**
         * get address of page containing start of free space and calculate
         * index of start sector
         */
        page_off = blk->free_start - mtd_mod_by_ws(blk->free_start, ctx->mtd);
        start_sect = (blk->free_start - page_off) >> ctx->sector_sft;

        buffer_len = ctx->buffer_size;
        write_ptr = write_buf + (start_sect * ctx->sector_size);
        buffer_len -= (start_sect * ctx->sector_size);
        to_write = 0;

        // prepare buffers
        memset(&(oob_hdrs[0]), 0xff, sizeof(oob_hdrs));
        memset(write_buf, 0xff, ctx->buffer_size);

        // put header before actual data
        // it should be safe to assume that the entry header is smaller than one whole sector
        if(data_written == 0){
            memcpy(write_ptr, &entry_hdr, sizeof(entry_hdr));
            write_ptr += sizeof(entry_hdr);
            buffer_len -= sizeof(entry_hdr);
            to_write = sizeof(entry_hdr);

            entry_start = blk->free_start;
        }

        // copy data to the right place in the write buffer
        to_copy = min(buffer_len, data_len - data_written);
        to_write += to_copy;
        if(to_copy > 0 && data_ptr != NULL){
            memcpy(write_ptr, data_ptr, to_copy);
        }

        num_sect = DIV_ROUND_UP(to_write, ctx->sector_size);
        end_sect = min(start_sect + num_sect, ctx->sectors_per_page) - 1;

        // pr_err("[%s] to_write: 0x%x start_sect: 0x%x end_sect: 0x%x, num_sect: 0x%x\n", __func__, to_write, start_sect, end_sect, num_sect);

        // add generic OOB marker to occupied sectors
        for(i = start_sect; i <= end_sect; ++i){
            oob_hdrs[i].ID = cpu_to_be32(FLASH_FS_ID_SKIP);
            oob_hdrs[i].Length = cpu_to_be32(0);
            oob_hdrs[i].Revision = cpu_to_be32(0);
        }

        // mark very last sector with entry-specific marker
        if((to_write + data_written) >= data_len){
            oob_hdrs[end_sect].ID = cpu_to_be32(entry->ID);
            oob_hdrs[end_sect].Length = cpu_to_be32(entry->Length);
            oob_hdrs[end_sect].Revision = cpu_to_be32(entry->revision);
        }


        oob_ops.datbuf = write_buf + (start_sect * ctx->sector_size);
        oob_ops.len = num_sect * ctx->sector_size;
        oob_ops.mode = MTD_OPS_AUTO_OOB;
        oob_ops.oobbuf = (uint8_t *)&(oob_hdrs[start_sect]);
        oob_ops.ooblen = num_sect * sizeof(oob_hdrs[0]);
        oob_ops.ooboffs = start_sect * sizeof(oob_hdrs[0]);

        // TODO: test write error handling!
        result = write_oob(ctx, blk, page_off + (start_sect * ctx->sector_size), &oob_ops);
        // pr_err("[%s] result: %d data_written: 0x%x retlen: 0x%x oobretlen: 0x%x\n", __func__, result, data_written, oob_ops.retlen, oob_ops.oobretlen);
        if(result == 0){
            /* successful write.
             * Adjust block accounting info, move write data pointer forward and update
             * the returned written length
             */
            blk->free_start += num_sect * ctx->sector_size;
            blk->free_space -= num_sect * ctx->sector_size;
            data_ptr += to_copy;
            data_written += to_copy;
            *retlen = data_written;
        } else {
            /* write_oob() already updated the bad page info for this block. Set free_start
             * to start of next page.
             * The data will be written again during the next loop iteration, unless the whole
             * block turned bad.
             */
            blk->free_start = page_off + ctx->buffer_size;
            result = -EIO;

            /* skip complex error handling in panic mode. Just report back the write error
             * and let the upper layers sort out how to handle it.
             */
            if(ctx->in_panic_mode != 0){
                *retlen = 0;
                goto err_out;
            }
        }
    }while(result != -EIO && data_written < data_len);

    // write successful, update entry data and block accounting info
    result = 0;
    entry->block_ptr = blk;
    entry->blk_offset = entry_start;
    entry->flash_len = blk->free_start - entry_start;

    pr_debug("[%s] wrote ID: 0x%x Rev: 0x%x Len: 0x%x(0x%x/0x%x) at block 0x%llx offset 0x%llx\n",
            __func__, entry->ID, entry->revision, entry->Length, entry->padded_len, entry->flash_len, entry->block_ptr->blk_addr, entry->blk_offset);

err_out:
    return result;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Write(struct tffs_module *this, void *handle,
                            uint8_t *data_buf, size_t data_len, size_t *retlen, unsigned int final)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_Block *blk;
    struct TFFS_Entry_Index *idx;
    struct TFFS_NAND_Entry *entry;
    struct TFFS_NAND_State *state;
    size_t padded_len;
    int result;
    struct timespec timestamp;

    result = 0;
    *retlen = 0;
    entry = NULL;
    state = (struct TFFS_NAND_State *) handle;
    ctx = (struct tffs_nand_ctx *)this->priv;

#if 0
    pr_err("[%s] handle: %p Id. 0x%x data_buf: %p data_len: %d bytes\n",
           __func__, handle, state->id, data_buf, data_len);
#endif


    if(ctx == NULL) {
        return -EBADF;
    }

    if(state->finished){
        result = -ESPIPE; // illegal seek
        goto err_out;
    }

    /* get index for ID. If we start a new chain or are in panic mode, we get the global
     * index for this ID. Otherwise we use the one we stored during previous writes.
     * This takes care of the situation when the index gets rebuild while we are writing
     */
    idx = NULL;
    if(state->chain == NULL || ctx->in_panic_mode){
        idx = get_index(ctx, state->id);
    } else {
        // no point in going on if previous parts of the chain were lost to a bad block
        if(state->chain->corrupt_cnt == state->corrupt_cnt){
            idx = state->chain->idx;
        }
    }
    if(idx == NULL){
        result = -EIO;
        goto err_out;
    }

    /*
     * in panic mode we can not allocate new entry data structures. Instead we take two
     * static structs within our context and use them alternatingly between writes.
     * This is no problem as long as these don't get added to the index or a block entry list.
     */
    if(ctx->in_panic_mode){
        entry = (state->curr_entry != &(ctx->panic_entries[0])) ?
                    &(ctx->panic_entries[0]) : &(ctx->panic_entries[1]);
    } else {
        entry = kmalloc(sizeof(*entry), GFP_KERNEL);
        if(entry == NULL){
            result = -ENOMEM;
            goto err_out;
        }
    }

    memset(entry, 0x0, sizeof(*entry));

    getnstimeofday(&timestamp);
    INIT_LIST_HEAD(&(entry->entry_list));
    INIT_LIST_HEAD(&(entry->blk_entry_list));
    INIT_LIST_HEAD(&(entry->segment_list));
    entry->ID = state->id;
    entry->Length = data_len;
    entry->timestamp = timestamp.tv_sec;

    if(state->curr_entry == NULL){
        /* since it is impossible to remove an entry by simply zeroing out its
         * ID (we can not re-write to an earlier address as we did with NOR flash),
         * we instead add a single segment entry with its segment number set to a
         * special value
         */
        if(data_len == 0 && data_buf == NULL){
            entry->segment_nr = TFFS_SEG_CLEARED;
            final = 1;
        } else {
            entry->segment_nr = 0;
        }
        entry->revision = ++(idx->max_rev);
    } else {
        entry->segment_nr = state->curr_entry->next_segment;
        entry->revision = state->curr_entry->revision;
    }

    if(final){
        entry->next_segment = 0;
        state->finished = 1;
    }else {
        entry->next_segment = entry->segment_nr + 1;
    }

    // try to keep related segments in same erase block
    blk = NULL;
    if(state->curr_entry){
        padded_len = padded_entry_len(entry->Length, state->curr_entry->block_ptr->hdr_size, ctx);
        if(state->curr_entry->block_ptr->free_space >= padded_len
           && state->curr_entry->block_ptr->needs_rewrite == 0)
       {
           blk = state->curr_entry->block_ptr;
       }
    }

    if(blk == NULL){
        blk = find_free_block(ctx, entry->Length, ANY_BLK_SEQ, tffs_srch_min_erase);
    }

    if(blk == NULL){
        pr_err("[%s] No free blk found.\n", __func__);
        result = -ENOSPC;
        goto err_out;
    }

    entry->padded_len = padded_entry_len(entry->Length, blk->hdr_size, ctx);
#if 0
    pr_info("[%s] entry: %p revision: 0x%x segment_nr: 0x%x, next_segment: 0x%x Length: 0x%x, padded_len: 0x%x\n",
            __func__,
            entry,
            entry->revision,
            entry->segment_nr,
            entry->next_segment,
            entry->Length,
            entry->padded_len);
#endif

    result = blk->write_entry(ctx, entry, blk, data_buf, data_len, retlen);
    if(result != 0){
        goto err_out;
    }

    blk->used_space += entry->padded_len;

    /*
     * if we are not in panic mode, add new entry to block's entry list and to the
     * global index. Also increase its chain's refcount if this is a new chain
     */
    if(ctx->in_panic_mode == 0){
        list_add_tail(&(entry->blk_entry_list), &(blk->blk_entry_list));
        result = add_entry_to_index(ctx, idx, entry, 0);

        if(result == 0){
            if(state->chain == NULL){
                /* first segment entry was successfully added to index. Get a reference to
                 * its chain.
                 */
                //get_chain(entry->chain); // implicit get on kref_init
                state->chain = entry->chain;
                state->corrupt_cnt = state->chain->corrupt_cnt;
            }
        }

        if(result != 0){
            state->finished = 1;
            entry = NULL;
            goto err_out;
        }
        mtd_sync(ctx->mtd);
    }

    state->curr_entry = entry;

    // entry belongs to index now
    entry = NULL;

    if(ctx->in_panic_mode == 0){
        // check that there are enough empty erase blocks left
        if(num_free_blocks(ctx) < MIN_FREE_BLOCKS){
            // we are below the absolute minimum, trigger synchronous recycle now
            recycle_blocks(ctx, 0);
        } else if(blk->needs_rewrite){
            // block is degrading, trigger asynchronous cleanup
            tffs_send_event(TFFS_EVENT_CLEANUP);
        }
    }

err_out:
    if(ctx->in_panic_mode == 0 && entry != NULL){
        kfree(entry);
    }

    return result;
}

#if 0
static int compare_flash_hdrs(struct _TFFS_NAND_Entry *entry_hdr, struct _TFFS_NAND_OOB *oob_hdr)
{
    if(entry_hdr->ID != oob_hdr->ID || entry_hdr->Length != oob_hdr->Length
       || entry_hdr->revision_nr != oob_hdr->Revision)
    {
        return 1;
    }

    return 0;
}
#endif

static inline int check_flash_hdr(struct TFFS_NAND_Entry *entry, struct _TFFS_NAND_Entry *entry_hdr)
{
    if(   be32_to_cpu(entry_hdr->ID) != entry->ID
       || be32_to_cpu(entry_hdr->Length) != entry->Length
       || be32_to_cpu(entry_hdr->revision_nr) != entry->revision
       || be32_to_cpu(entry_hdr->revision_nr) != entry->revision
       || be32_to_cpu(entry_hdr->segment_nr) != entry->segment_nr
       || be32_to_cpu(entry_hdr->next_segment) != entry->next_segment
       )
    {
        return 1;
    }

    return 0;
}

static inline int check_flash_hdr_31(struct TFFS_NAND_Entry *entry, struct _TFFS_NAND_Entry_31 *entry_hdr)
{
    if(   be64_to_cpu(entry_hdr->magic) != TFFS_ENTRY_MAGIC
       || be32_to_cpu(entry_hdr->ID) != entry->ID
       || be32_to_cpu(entry_hdr->Length) != entry->Length
       || be32_to_cpu(entry_hdr->revision_nr) != entry->revision
       /*--- || be32_to_cpu(entry_hdr->revision_nr) != entry->revision ---*/
       || be32_to_cpu(entry_hdr->segment_nr) != entry->segment_nr
       || be32_to_cpu(entry_hdr->next_segment) != entry->next_segment
       )
    {
        return 1;
    }

    return 0;
}

#if defined(CONFIG_TFFS_NAND_V31)
static int do_read_noob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Entry *entry, loff_t offset, uint8_t *buffer, size_t *len)
{
    struct TFFS_NAND_Block *blk;
    struct _TFFS_NAND_Entry_31 entry_hdr;
    uint8_t *read_buf, *data_ptr, *dst_ptr;
    loff_t page_off, read_off, data_off, total_skip, skip;
    uint32_t end_sect;
    size_t bytes_read, bytes_left, buffer_left, read_len, to_read, to_copy, retlen;
    int result;

    result = 0;
    read_buf = NULL;

    if(ctx == NULL) {
        pr_debug("[%s] ctx == NULL\n", __func__);
        return -ENODEV;
    }

    read_buf = ctx->rw_buffer;
    if(read_buf == NULL){
        result = -ENOMEM;
        goto err_out;
    }

    if(offset != 0 && offset >= entry->Length){
        pr_warning("[TFFS3-NAND] seek offset 0x%llx beyond entry end\n", offset);
        result = -EINVAL;
        goto err_out;
    }

    blk = entry->block_ptr;
    read_len = min(*len, entry->Length - ((size_t) offset));
    buffer_left = read_len;
    bytes_left = read_len + sizeof(entry_hdr);
    total_skip = offset + sizeof(entry_hdr);
    bytes_read = 0;
    read_off = entry->blk_offset;
    dst_ptr = buffer;

    // shut up gcc...
    entry_hdr.ID = 0;
    entry_hdr.Length = 0;
    entry_hdr.revision_nr = 0;


    while(bytes_left > 0){
        to_read = min(bytes_left, ctx->buffer_size);
        data_off = read_off & ctx->buffer_msk;
        page_off = read_off & ~ctx->buffer_msk;
        end_sect = (data_off + to_read - 1) >> ctx->sector_sft;
        retlen = ctx->buffer_size;

        result = read_noob(ctx, blk, page_off, &retlen, read_buf);
        if(result != 0){
            pr_err("[TFFS3-NAND] [%s] Flash read at address 0x%llx failed\n", __func__, blk->blk_addr + page_off);
            goto err_out;
        }

        data_ptr = read_buf + data_off;

        // we always read the first sector to get the entry header
        if(bytes_read == 0){
            if(retlen >= (sizeof(entry_hdr) + data_off)){
                memcpy(&entry_hdr, data_ptr, sizeof(entry_hdr));
                bytes_read += sizeof(entry_hdr);
                bytes_left -= sizeof(entry_hdr);
                data_ptr += sizeof(entry_hdr);
                retlen -= sizeof(entry_hdr);

                // TODO: To be *really* sure we would have to also do crc here.
                if(check_flash_hdr_31(entry, &entry_hdr)){
                    pr_debug("[%s] Entry header at address 0x%llx and index are inconsistent!\n",
                           __func__, blk->blk_addr + read_off);
                    result = -EIO;
                    goto err_out;
                }
            } else {
                pr_debug("[%s] Aborting on incomplete flash header read at address 0x%llx (read 0x%x; calc: 0x%x; expected 0x%x)\n",
                       __func__, blk->blk_addr + read_off, (int)retlen, (int)(ctx->buffer_size - page_off), (int)(sizeof(entry_hdr) + data_off));
                result = -EIO;
                goto err_out;
            }
        }

        if(retlen > 0 && buffer_left > 0){
            // ignore data before to requested offset
            if(bytes_read < total_skip){
                skip = min(retlen, ((size_t)total_skip) - bytes_read);
                bytes_read += skip;
                retlen -= skip;
                data_ptr += skip;
            }

            if(bytes_read >= total_skip){
                to_copy = min(retlen, buffer_left);
                memcpy(dst_ptr, data_ptr, to_copy);
                bytes_read += to_copy;
                bytes_left -= to_copy;
                buffer_left -= to_copy;
                dst_ptr += to_copy;
            }
        }

        if(bytes_left > 0){
            /**
             *  next read (if any) will always start at page boundary.
             */
            do{
                page_off += ctx->buffer_size;

                /**
                 *  Old bootloader might still write entries with invalid
                 *  pages enclosed.
                 */
                if(page_is_bad(ctx, blk, page_off)){
                    continue;
                }

                // skip reading data before requested offset
                if(bytes_read + ctx->buffer_size <= total_skip){
                    bytes_read += ctx->buffer_size;
                } else {
                    break;
                }
            } while(page_off < ctx->mtd->erasesize);

            if(page_off >= ctx->mtd->erasesize){
                pr_debug("[%s] Entry at address 0x%llx crosses block boundary\n",
                       __func__, blk->blk_addr + entry->blk_offset);
                result = -EIO;
                goto err_out;
            }

            read_off = page_off;
        }
    }

    *len = read_len;

err_out:
    return result;
}
#endif

static int do_read(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Entry *entry, loff_t offset, uint8_t *buffer, size_t *len)
{
    struct TFFS_NAND_Block *blk;
    struct _TFFS_NAND_OOB oob_hdrs[MAX_SUBPAGE_NUM];
    struct _TFFS_NAND_Entry entry_hdr;
    struct mtd_oob_ops oob_ops;
    uint8_t *read_buf, *data_ptr, *dst_ptr;
    loff_t page_off, read_off, data_off, total_skip, skip;
    uint32_t end_sect;
    size_t bytes_read, bytes_left, buffer_left, read_len, to_read, to_copy;
    int result;

    result = 0;
    read_buf = NULL;

    if(ctx == NULL) {
        pr_debug("[%s] ctx == NULL\n", __func__);
        return -ENODEV;
    }

    read_buf = ctx->rw_buffer;
    if(read_buf == NULL){
        result = -ENOMEM;
        goto err_out;
    }

    if(offset != 0 && offset >= entry->Length){
        pr_warning("[TFFS3-NAND] seek offset 0x%llx beyond entry end\n", offset);
        result = -EINVAL;
        goto err_out;
    }

    blk = entry->block_ptr;
    read_len = min(*len, entry->Length - ((size_t) offset));
    buffer_left = read_len;
    bytes_left = read_len + sizeof(entry_hdr);
    total_skip = offset + sizeof(entry_hdr);
    bytes_read = 0;
    read_off = entry->blk_offset;
    dst_ptr = buffer;

    // shut up gcc...
    entry_hdr.ID = 0;
    entry_hdr.Length = 0;
    entry_hdr.revision_nr = 0;


    while(bytes_left > 0){
        to_read = min(bytes_left, ctx->buffer_size);
        data_off = mtd_mod_by_ws(read_off, ctx->mtd);
        page_off = read_off - data_off;
        end_sect = (data_off + to_read - 1) >> ctx->sector_sft;


        oob_ops.datbuf = read_buf;
        oob_ops.len = ctx->buffer_size;
        oob_ops.mode = MTD_OPS_AUTO_OOB;
        oob_ops.oobbuf = (uint8_t *)&(oob_hdrs[0]);
        oob_ops.ooblen = ctx->sectors_per_page * sizeof(oob_hdrs[0]);
        oob_ops.ooboffs = 0;

        result = read_oob(ctx, blk, page_off, &oob_ops);
        if(result != 0){
            pr_err("[TFFS3-NAND] [%s] Flash read at address 0x%llx failed\n", __func__, blk->blk_addr + page_off);
            goto err_out;
        }

        data_ptr = read_buf + data_off;

        // we always read the first sector to get the entry header
        if(bytes_read == 0){
            if(oob_ops.retlen >= (sizeof(entry_hdr) + data_off)){
                memcpy(&entry_hdr, data_ptr, sizeof(entry_hdr));
                bytes_read += sizeof(entry_hdr);
                bytes_left -= sizeof(entry_hdr);
                data_ptr += sizeof(entry_hdr);
                oob_ops.retlen -= sizeof(entry_hdr);

                if(check_flash_hdr(entry, &entry_hdr)){
                    pr_debug("[%s] Entry header at address 0x%llx and index are inconsistent!\n",
                           __func__, blk->blk_addr + read_off);
                    result = -EIO;
                    goto err_out;
                }
            } else {
                pr_debug("[%s] Aborting on incomplete flash header read at address 0x%llx\n",
                       __func__, blk->blk_addr + read_off);
                result = -EIO;
                goto err_out;
            }
        }

        if(oob_ops.retlen > 0 && buffer_left > 0){
            // ignore data before to requested offset
            if(bytes_read < total_skip){
                skip = min(oob_ops.retlen, ((size_t)total_skip) - bytes_read);
                bytes_read += skip;
                oob_ops.retlen -= skip;
                data_ptr += skip;
            }

            if(bytes_read >= total_skip){
                to_copy = min(oob_ops.retlen, buffer_left);
                memcpy(dst_ptr, data_ptr, to_copy);
                bytes_read += to_copy;
                bytes_left -= to_copy;
                buffer_left -= to_copy;
                dst_ptr += to_copy;
            }
        }

        if(bytes_left > 0){
            /**
             *  next read (if any) will always start at page boundary.
             */
            do{
                page_off += ctx->buffer_size;

                /**
                 *  Old bootloader might still write entries with invalid
                 *  pages enclosed.
                 */
                if(page_is_bad(ctx, blk, page_off)){
                    continue;
                }

                // skip reading data before requested offset
                if(bytes_read + ctx->buffer_size <= total_skip){
                    bytes_read += ctx->buffer_size;
                } else {
                    break;
                }
            } while(page_off < ctx->mtd->erasesize);

            if(page_off >= ctx->mtd->erasesize){
                pr_debug("[%s] Entry at address 0x%llx crosses block boundary\n",
                       __func__, blk->blk_addr + entry->blk_offset);
                result = -EIO;
                goto err_out;
            }

            read_off = page_off;
        }
    }

    *len = read_len;

err_out:
    return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Read(struct tffs_module *this, void *handle,
                           uint8_t *read_buffer, size_t *read_length)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_State *state;
    struct TFFS_Entry_Index *idx;
    int result;

    result = 0;

    ctx = (struct tffs_nand_ctx *)this->priv;

    if(ctx == NULL) {
        pr_debug("[%s] ctx == NULL\n", __func__);
        return -ENODEV;
    }

    state = (struct TFFS_NAND_State *) handle;
    if(state == NULL){
        pr_debug("[%s] state == NULL\n", __func__);
        goto err_out;
    }

    if(state->chain == NULL){
        idx = get_index(ctx, state->id);
        if(idx == NULL || idx->chain_ptr == NULL || idx->chain_ptr->complete == 0){
            pr_debug("[%s] idx=%p; idx->chain_ptr=%p; idx->chain_ptr->complete=%d\n",
                    __func__, idx, idx?idx->chain_ptr:NULL, idx->chain_ptr?idx->chain_ptr->complete:0);
            result = -ENOENT;
            goto err_out;
        }

        state->curr_entry = list_first_entry(&(idx->chain_ptr->segment_list), struct TFFS_NAND_Entry, segment_list);
        state->offset = 0;

        // special case of cleared entry
        if(state->curr_entry->segment_nr == TFFS_SEG_CLEARED){
            pr_debug("[%s] SEG_CLEARED\n", __func__);
            state->curr_entry = NULL;
            result = -ENOENT;
            goto err_out;
        }

        state->chain = state->curr_entry->chain;
        state->corrupt_cnt = state->chain->corrupt_cnt;
        get_chain(state->curr_entry->chain);
    } else {
        // abort if some block with chain entries turned bad during read
        if(state->chain->corrupt_cnt != state->corrupt_cnt){
#if defined(DEBUG_CHAINS)
            pr_err("[%s] chain ID: 0x%x Revision: 0x%x corrupted while reading!\n", __func__, state->chain->ID, state->chain->revision);
            {
                struct TFFS_Entry_Index *idx;
                struct TFFS_NAND_SegChain *chain;

                idx = state->chain->idx;
                list_for_each_entry(chain, &(idx->rev_list), rev_list){
                    pr_err("[%s] ID: 0x%x Revision: 0x%x Complete: 0x%x, CorruptCnt: 0x%x, Prunable: 0x%x, RefCnt: 0x%x\n", __func__, chain->ID, chain->revision, chain->complete, chain->corrupt_cnt, chain->prune, chain->refcnt.refcount.counter);
                }
            }
#endif
            result = -EIO;
            goto err_out;
        }

        if(state->offset >= state->curr_entry->Length){
            if(state->curr_entry->next_segment == 0){
                *read_length = 0;
                result = 0;
                goto err_out;
            } else {
                state->curr_entry = list_first_entry(&(state->curr_entry->segment_list), struct TFFS_NAND_Entry, segment_list);
                state->offset = 0;
            }
        }
    }

    result = state->curr_entry->block_ptr->read_entry(ctx, state->curr_entry, state->offset, read_buffer, read_length);
    if(result == 0){
        state->offset += *read_length;
    }

    if(ctx->in_panic_mode == 0){
        if(result == -EIO){
            invalidate_segment_chain(state->chain);
        }

        /* note: dereferencing curr_entry still safe because we are holding a reference to
         * its chain
         */
        if(state->curr_entry->block_ptr->needs_rewrite){
            // block is degrading, trigger asynchronous cleanup
            tffs_send_event(TFFS_EVENT_CLEANUP);
        }
    }

#if defined(DEBUG_CHAINS)
    // test chain invalidation
    if(state->curr_entry->ID == 240 && (state->curr_entry->revision % 20) == 0 && state->curr_entry->next_segment != 0 ){
        {
            struct TFFS_Entry_Index *idx;
            struct TFFS_NAND_SegChain *chain;

            idx = state->chain->idx;
            list_for_each_entry(chain, &(idx->rev_list), rev_list){
                pr_err("[%s] ID: 0x%x Revision: 0x%x Complete: 0x%x, CorruptCnt: 0x%x, Prunable: 0x%x, RefCnt: 0x%x\n", __func__, chain->ID, chain->revision, chain->complete, chain->corrupt_cnt, chain->prune, chain->refcnt.refcount.counter);
            }
        }
        free_entry(state->curr_entry);
    }
#endif

err_out:
    return result;
}

static int move_entry(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Entry *entry, struct TFFS_NAND_Block *trgt_blk)
{
    int result;
    uint8_t *data_buf;
    size_t data_len, trgt_len, src_len, written;
    struct TFFS_NAND_Block *src_blk;

    BUG_ON(entry->block_ptr == NULL);

    result = 0;
    data_buf = NULL;
    data_len = entry->Length;
    trgt_len = padded_entry_len(data_len, trgt_blk->hdr_size, ctx);
    src_blk = entry->block_ptr;
    src_len = padded_entry_len(data_len, src_blk->hdr_size, ctx);

    BUG_ON(src_blk->used_space < src_len);

    if(trgt_blk->free_space < trgt_len){
        result = -ENOSPC;
        goto err_out;
    }

    data_buf = NULL;
    if(data_len > 0){
        data_buf = kzalloc(data_len, GFP_KERNEL);
        if(data_buf == NULL){
            result = -ENOMEM;
            goto err_out;
        }

        result = src_blk->read_entry(ctx, entry, 0, data_buf, &data_len);
        if(result != 0){
            goto err_out;
        }
        if(data_len != entry->Length){
            result = -EIO;
            goto err_out;
        }
    }

    result = trgt_blk->write_entry(ctx, entry, trgt_blk, data_buf, data_len, &written);
    if(result == 0){
        if(!list_empty(&(entry->blk_entry_list))){
            list_del_init(&(entry->blk_entry_list));
        }

        src_blk->used_space -= src_len;

        list_add_tail(&(entry->blk_entry_list), &(trgt_blk->blk_entry_list));
        trgt_blk->used_space += trgt_len;
        entry->padded_len = trgt_len;
    }

err_out:
    if(data_buf != NULL){
        kfree(data_buf);
    }

    return result;
}

static int clean_block(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk)
{
    struct TFFS_NAND_Block *replace_blk;
    struct TFFS_NAND_Entry *entry, *tmp;
    unsigned int cnt;
    int result;

    result = 0;
    entry = NULL;
    cnt = 0;

    while(!list_empty(&(blk->blk_entry_list))){
        /* Try to find a block with enough free space to hold all of the
         * source block's entries. If the target block's header size is bigger
         * than the source block's, we still might end up moving the entries
         * to multiple blocks. */
        /* FIXME: find_free_block() adds the block's hdr_size internally */
        /* XXX: The above SHOULD not be a problem since we never want to have
         *        blocks with different formats in one TFFS.
         */
        replace_blk = find_free_block(ctx, blk->used_space, blk->blkseq_nr, tffs_srch_min_erase);
        if(replace_blk == NULL){
            pr_debug("[%s] no suitable replacement block found for block 0x%x\n", __func__, blk->blkseq_nr);
            result = -ENOSPC;
            goto err_out;
        }

        // pr_err("[%s] moving entries from block at 0x%llx to block at 0x%llx\n", __func__, blk->blk_addr, replace_blk->blk_addr);

        list_for_each_entry_safe(entry, tmp, &(blk->blk_entry_list), blk_entry_list){
            result = move_entry(ctx, entry, replace_blk);
            if(result != 0){
                if(result == -ENOSPC){
                    /* we ran out of space on the target block. Try to continue
                     * on another block */
                    break;
                } else {
                    goto err_out;
                }
            }
            ++cnt;
        }
    }

    if(list_empty(&(blk->blk_entry_list))){
        result = format_block(ctx, blk, ++(ctx->max_block_seq));
        // pr_err("[%s] addr: 0x%llx used: 0x%x free: 0x%x\n", __func__, blk->blk_addr, blk->used_space, blk->free_space);
    } else {
        pr_debug("[%s] block at address 0x%llx not empty after moving entries.\n", __func__, blk->blk_addr);
        result = -EIO;
        goto err_out;
    }

err_out:
    // pr_err("[%s] 0x%x entries moved, format result: %d\n", __func__, cnt, result);
    return result;
}

static int recycle_blocks(struct tffs_nand_ctx *ctx, unsigned int async)
{
    struct TFFS_NAND_Block *blk, *best_blk;
    uint32_t highest_ratio, curr_ratio;
    size_t dead_space, live_space;
    int result, num_before_recycle;

#if 0
    pr_debug("[%s] free_block = %d, min_block = %d, optimal_blocks = %d\n",
             __func__, num_free_blocks(ctx), MIN_FREE_BLOCKS, OPT_FREE_BLOCKS);
#endif

    result = 0;
    if(num_free_blocks(ctx) >= OPT_FREE_BLOCKS){
        goto err_out;
    }

    num_before_recycle = num_free_blocks(ctx);

    while(num_free_blocks(ctx) < MIN_FREE_BLOCKS || async != 0){
        curr_ratio = 0;
        highest_ratio = 0;
        best_blk = NULL;
        list_for_each_entry(blk, &(ctx->blk_list), blk_list){
            // pr_err("[%s] addr: 0x%llx state: 0x%x used: 0x%x free: 0x%x\n", __func__, blk->blk_addr, blk->state, blk->used_space, blk->free_space);
            if(blk->state == tffs_blk_active){
                dead_space = max_block_space(ctx);
                live_space = blk->used_space + blk->free_space;

                if(dead_space >= live_space){
                    dead_space -= blk->used_space + blk->free_space;
                } else {
                    pr_debug("[%s] block at address 0x%llx has more data than block space!\n", __func__, blk->blk_addr);
                    dead_space = 0;
                }

                if(dead_space > 0 || live_space > 0){
                    curr_ratio = (dead_space * 3) / (dead_space + live_space); // 25% steps
                } else {
                    pr_debug("[%s] Bogus accounting for block 0x%x at address 0x%llx found!\n",
                           __func__, blk->blkseq_nr, blk->blk_addr);
                    curr_ratio = 0;
                }

                if(   best_blk == NULL
                   || curr_ratio > highest_ratio
                   || (curr_ratio == highest_ratio && best_blk->erase_cnt > blk->erase_cnt)
                   )
                {
                    best_blk = blk;
                    highest_ratio = curr_ratio;
                }
            }
        }

        if(best_blk == NULL){
            pr_debug("[%s] No block for cleanup found!\n", __func__);
            result = -ENOSPC;
            goto err_out;
        }

        // pr_err("[%s] block at address 0x%llx: highest_ratio: 0x%x used: 0x%x free: 0x%x\n", __func__, best_blk->blk_addr, highest_ratio, best_blk->used_space, best_blk->free_space);
        result = clean_block(ctx, best_blk);
        if(result != 0){
            pr_debug("[%s] cleaning block at address 0x%llx failed\n", __func__, best_blk->blk_addr);
            goto err_out;
        }

        // if we are above MIN_FREE_BLOCKS, free at most one block per call in async mode
        if(num_free_blocks(ctx) > MIN_FREE_BLOCKS){
            async = 0;
        }
    }

    // if we are still below optimal free block count, trigger async recycle (again)
    // but only if we made progress...
    if((num_free_blocks(ctx) < OPT_FREE_BLOCKS) && (num_free_blocks(ctx) > num_before_recycle)){
        tffs_send_event(TFFS_EVENT_CLEANUP);
    }

err_out:
    return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Cleanup(struct tffs_module *this, void *handle)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_State *state;
    struct TFFS_NAND_Block *blk;
    int result;

    result = 0;
    state = (struct TFFS_NAND_State *) handle;
    ctx = (struct tffs_nand_ctx *)this->priv;

    if(ctx == NULL) {
        return -ENODEV;
    }

    if(ctx->in_panic_mode){
        return -EBUSY;
    }

    // trigger an asynchronous block recycle
    result = recycle_blocks(ctx, 1);
    if(result != 0){
        goto err_out;
    }

    list_for_each_entry(blk, &(ctx->blk_list), blk_list){
        if(blk->needs_rewrite != 0){
            result = clean_block(ctx, blk);
            if(result != 0){
                pr_debug("[%s] cleaning block at address 0x%llx failed\n", __func__, blk->blk_addr);
                goto err_out;
            }
        }
    }
    mtd_sync(ctx->mtd);

err_out:
    return result;
}


static int page_is_bad(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t offset)
{
    unsigned int i;

    for(i = 0; i < blk->num_bad_pages; ++i){
        if(offset >= blk->bad_pages[i] && offset < (blk->bad_pages[i] + ctx->buffer_size)){
            return 1;
        }
    }

    return 0;
}

static int scan_page(struct tffs_nand_ctx *ctx, loff_t addr, unsigned int *blk_bad,
                     unsigned int *page_bad, struct _TFFS_NAND_OOB oob_hdrs[])
{
    int result = 0;
    struct mtd_oob_ops oob_ops;

    // No use in scanning here for no_oob devices. We never know if the page is really bad.
    if (ctx->flags.no_oob)
        return 0;

    oob_ops.mode = MTD_OPS_AUTO_OOB;
    oob_ops.datbuf = NULL;
    oob_ops.len = 0;
    oob_ops.oobbuf = (uint8_t *) &oob_hdrs[0];
    oob_ops.ooboffs = 0;
    oob_ops.ooblen = ctx->sectors_per_page * sizeof(oob_hdrs[0]);

    result = mtd_read_oob(ctx->mtd, addr, &oob_ops);

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void format_callback(struct erase_info *instr)
{
    switch(instr->state) {
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

/**
 * Sort list of known bad pages in block. Use simple bubble sort because
 * list is really small.
 */
static void sort_bad_pages(struct TFFS_NAND_Block *blk)
{
    unsigned int i, swapped;
    loff_t tmp;

    do{
        swapped = 0;
        for(i = 1; i < blk->num_bad_pages; ++i){
            if(blk->bad_pages[i] < blk->bad_pages[i - 1]){
                tmp = blk->bad_pages[i];
                blk->bad_pages[i] = blk->bad_pages[i - 1];
                blk->bad_pages[i - 1] = tmp;
                swapped = 1;
            }
        }
    }while(swapped);
}

static int block_mark_page_bad(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t page_off, enum defect_type type)
{
    unsigned int i;
    int result;

    result = 0;
    page_off &= ~ctx->buffer_msk;

    pr_err("[%s] adding bad page at offset 0x%llx to list. Defect type: %s\n", __func__, page_off,
            type == defect_hdr ? "header list ":
            type == defect_tag ? "page tag" :
            type == defect_new ? "new" : "unknown");
    /**
     * Trigger a rewrite for the block if we found a new defect or a legacy
     * bad page tag
     */
    if(page_off < ctx->mtd->erasesize || type != defect_hdr){
        ++blk->needs_rewrite;
    }

    /**
     * Adjust offset if it does not come from a bad page tag. We need
     * to preserve the real offset from those because multi-page entries
     * may include defect pages that must not be read.
     * On newer kernels this can not happen, but we may have to deal with
     * this when upgrading the firmware or if the bootloader creates such
     * an entry.
     */
    if(type != defect_tag && page_off < ctx->mtd->erasesize){
        page_off += ctx->mtd->erasesize;
    }

    /**
     * Declare block bad if the same page produced an error before
     */
    if(type == defect_new
       && (   page_is_bad(ctx, blk, page_off)
           || page_is_bad(ctx, blk, page_off + ctx->mtd->erasesize)))
    {
        result = -EIO;
        goto err_out;
    }

    /**
     * Bad pages can be added twice during block scanning. Once from
     * the header's bad page list and a second time when scanning the
     * individual pages. In this case we have to keep only the non-shifted
     * offset so we can skip the marked page(s) when reading.
     */
    for(i = 0; i < blk->num_bad_pages; ++i){
        if(   type != defect_tag
           && blk->bad_pages[i] + ctx->mtd->erasesize == page_off)
        {
            // bad page already known from tag. Ignore
            result = 0;
            goto err_out;
        }

        if(blk->bad_pages[i] == page_off + ctx->mtd->erasesize){
            // bad page tag found after entry has been added by header. Replace
            blk->bad_pages[i] = page_off;
            break;
        }
    }

    /**
     * Page was not in list, append if we are still below threshold
     */
    if(i >= blk->num_bad_pages){
        if(blk->num_bad_pages < TFFS3_MAX_BADPAGES){
            blk->bad_pages[blk->num_bad_pages] = page_off;
            ++blk->num_bad_pages;
        } else {
            result = -EIO;
            goto err_out;
        }
    }

    /**
     * An entry was replaced or appended to list. Sort it again.
     */
    sort_bad_pages(blk);

err_out:
    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int format_block(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *block,
                        uint32_t blkseq_nr)
{
    struct _TFFS_Block_Hdr *new_hdr;
    int result;
    size_t retlen;
    unsigned int i;
    struct erase_info *erase;
    DECLARE_WAITQUEUE(wait, current);
    wait_queue_head_t wait_q;
    uint8_t *write_buff;

    pr_debug("[%s] called\n", __func__);

    write_buff = NULL;
    result = 0;

    if(block->state == tffs_blk_bad){
        pr_err("[TFFS3-NAND] Refusing to format bad block at address 0x%llx\n", block->blk_addr);
        result = -EIO;
        goto err_out;
    }

    if(!list_empty(&(block->blk_entry_list))){
        pr_err("[TFFS3_NAND] Block at address 0x%llx still in use, aborting\n", block->blk_addr);
        result = -EBUSY;
        goto err_out;
    }

    if(block->num_bad_pages >= TFFS3_MAX_BADPAGES){
        pr_debug("[%s] Too many bad pages\n", __func__);
        result = -EIO;
        goto err_out;
    }

    block->state = tffs_blk_raw;

    // erase block
    init_waitqueue_head(&wait_q);

    erase = (struct erase_info *)kzalloc(sizeof(struct erase_info), GFP_KERNEL);
    if(erase == NULL) {
        result = -ENOMEM;
        goto err_out;
    }

    erase->mtd      = ctx->mtd;
    erase->addr     = block->blk_addr;
    erase->len      = ctx->mtd->erasesize;
    erase->callback = format_callback;
    erase->priv     = (u_long)&wait_q;
    erase->next     = NULL;

    pr_debug("[%s] erasing block at address 0x%llx\n", __func__, block->blk_addr);
    result = mtd_erase(ctx->mtd, erase);
    if(result == 0){
        set_current_state(TASK_UNINTERRUPTIBLE);
        add_wait_queue(&wait_q, &wait);
        if(erase->state != MTD_ERASE_DONE && erase->state != MTD_ERASE_FAILED) {
            schedule();
        }

        remove_wait_queue(&wait_q, &wait);
        set_current_state(TASK_RUNNING);

        result = (erase->state == MTD_ERASE_FAILED) ? -EIO : 0;
    } else {
        result = -EIO;
    }
    kfree(erase);

    if(result != 0){
        pr_warning("[TFFS3-NAND] erase operation of block at address 0x%llx failed\n", block->blk_addr);
        goto err_out;
    }

    ++(block->erase_cnt);
    block->blkseq_nr = blkseq_nr;

    write_buff = kmalloc(ctx->buffer_size, GFP_KERNEL);
    if(write_buff == NULL){
        pr_err("[TFFS3-NAND] unable to allocate memory for write buffer\n");
        result = -ENOMEM;
        goto err_out;
    }

    memset(write_buff, 0xff, ctx->buffer_size);
    new_hdr = (struct _TFFS_Block_Hdr *)write_buff;
    new_hdr->magic = cpu_to_be64(TFFS3_HDR_MAGIC);
    new_hdr->version = cpu_to_be32(TFFS3_VERSION);
    new_hdr->type = cpu_to_be32(TFFS3_TYPE_MTDNAND);
    new_hdr->mtdnand.blkseq_nr = cpu_to_be32(block->blkseq_nr);
    new_hdr->mtdnand.sect_per_pg = cpu_to_be32(ctx->sectors_per_page);

    /**
     * if bad page was originally marked by bootloader, shift its address
     * out of the block's address range so it will not be marked by
     * bootloader again
     */
    for(i = 0; i < block->num_bad_pages; ++i){
        if(block->bad_pages[i] < ctx->mtd->erasesize){
            block->bad_pages[i] += ctx->mtd->erasesize;
        }
    }

    // list needs to be sorted again if page addresses have been adjusted
    sort_bad_pages(block);

    // transfer bad page list to header
    for(i = 0; i < block->num_bad_pages; ++i){
        new_hdr->mtdnand.bad_pages[i] = cpu_to_be64(block->bad_pages[i]);
    }
    new_hdr->mtdnand.num_bad_pages = cpu_to_be32(block->num_bad_pages);

    // remember the erase counter
    new_hdr->mtdnand.erase_cnt = cpu_to_be32(block->erase_cnt);

    // write TFFS header
    result = mtd_write(ctx->mtd, block->blk_addr, ctx->buffer_size,
                             &retlen, (const u_char *) new_hdr);

    // reset management data and block state
    if(result == 0){
        block->used_space = 0;
        block->free_space = max_block_space(ctx);
        block->free_start = ctx->buffer_size;
        block->needs_rewrite = 0;
        block->state = tffs_blk_active;
        pr_debug("[TFFS3-NAND] wrote TFFS header to address 0x%llx. SeqNr: 0x%x EraseCnt: 0x%x; free_start: 0x%x; free_space: 0x%x \n",
                (long long)block->blk_addr, (int)block->blkseq_nr, (int)block->erase_cnt, (int)block->free_start, (int)block->free_space);
    }else{
        pr_err("[TFFS3-NAND] Failed to write block.\n");
    }

err_out:
    if(write_buff != NULL){
        kfree(write_buff);
    }

    return result;
}

#if defined(CONFIG_TFFS_NAND_V31)
/*
 * parse a TFFS entry at given offset via CRC32
 *
 * start_page is in byte but must be page aligned.
 * first_sect must be in range 0..ctx->sectors_per_page
 */
static struct TFFS_NAND_Entry *parse_entry_noob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t start_page, loff_t first_sect)
{
    struct _TFFS_NAND_Entry_31 entry_hdr, *entry_hdr_p;
    struct TFFS_NAND_Entry *new_entry;
    int result;
    loff_t entry_addr, entry_offs, end_offs, offset;
    uint32_t tmp_crc, hdr_crc;
    unsigned int bad_page_i;
    size_t retlen, read_len;

    new_entry = NULL;

    if (blk->hdr_size != sizeof(entry_hdr)){
        pr_err("[%s] entry hdr size mismatch at address 0x%llx (is %x; should be %x)\n",
                __func__, blk->blk_addr, (int)blk->hdr_size, (unsigned int)sizeof(entry_hdr));
        goto err_out;
    }
    entry_offs = start_page + first_sect * ctx->sector_size;

    // Skip bad pages in front of the entry. Since the block is scanned from start
    // to end, we can assume that the bad page table is sorted in ascending order
    for(bad_page_i = 0; bad_page_i < blk->num_bad_pages && blk->bad_pages[bad_page_i] <= start_page; ++bad_page_i){

        if(blk->bad_pages[bad_page_i] < start_page)
            continue;

        // This can only happen if first_sect is 0.
        entry_offs += ctx->buffer_size;
        start_page += ctx->buffer_size;
    }

    // sanity check. Start address can never be greater than erasesize.
    if(entry_offs >= ctx->mtd->erasesize){
        pr_debug("[%s] bogus oob entry at address 0x%llx\n", __func__, blk->blk_addr + entry_offs);
        goto err_out;
    }
    
    entry_addr = entry_offs + blk->blk_addr;
    result = mtd_read(ctx->mtd, entry_addr, ctx->buffer_size, &retlen, ctx->rw_buffer);
    if(result != 0 || retlen != ctx->buffer_size){
        pr_debug("[%s] error reading entry header at address 0x%llx\n", __func__, entry_addr);
        goto err_out;
    }
    memcpy(&entry_hdr, ctx->rw_buffer, sizeof(entry_hdr));

    if(be32_to_cpu(entry_hdr.ID) == ~(FLASH_FS_ID_SKIP) || be32_to_cpu(entry_hdr.ID) == FLASH_FS_ID_SKIP){
        pr_debug("[%s] empty entry header found at address 0x%llx\n", __func__, entry_addr);
        goto err_out;
    }

    // Check if this is really a valid entry.
    if(be64_to_cpu(entry_hdr.magic) != TFFS_ENTRY_MAGIC){
        /* DEBUG should be err */
        pr_debug("[%s] Corrupt entry found at address 0x%llx (magic = 0x%llx)\n", __func__, entry_addr, (long long)be64_to_cpu(entry_hdr.magic));
        goto err_out;
    }

#if 0
    pr_info("[%s] entry_hdr: ID: 0x%x, Length: 0x%x, timestamp: 0x%x revision_nr: 0x%x segment_nr: 0x%x next_segment: 0x%x\n",
            __func__, be32_to_cpu(entry_hdr.ID), be32_to_cpu(entry_hdr.Length),
            be32_to_cpu(entry_hdr.timestamp), be32_to_cpu(entry_hdr.revision_nr),
            be32_to_cpu(entry_hdr.segment_nr), be32_to_cpu(entry_hdr.next_segment));
#endif

    // CRC check. Ignore pages that are marked bad. Don't crc any padding.
    end_offs = entry_offs + be32_to_cpu(entry_hdr.Length) + sizeof(entry_hdr);
    // Sanity check: We never read more than a block here.
    if(end_offs > ctx->mtd->erasesize){
        pr_err("[%s] Corrupt entry found at address 0x%llx (length = 0x%llx)\n", __func__, entry_addr,
                (long long)(end_offs - entry_offs));
        goto err_out;
    }
    // crc of header with crc field set to 0.
    hdr_crc = be32_to_cpu(entry_hdr.crc);
    entry_hdr_p = (struct _TFFS_NAND_Entry_31 *)ctx->rw_buffer;
    entry_hdr_p->crc = 0;
    read_len = min(ctx->buffer_size, (size_t)(end_offs - entry_offs));
    tmp_crc = crc32_be(TFFS_CRC_INIT_VAL, ctx->rw_buffer, read_len);
    offset = entry_offs + read_len;
    // Always read to page boundaries.
    while(offset < end_offs) {
        if(bad_page_i < blk->num_bad_pages && offset == blk->bad_pages[bad_page_i]){
            offset += read_len;
            end_offs += ctx->buffer_size;
            read_len = min(ctx->buffer_size, (size_t)(end_offs - offset));
            bad_page_i++;
            continue;
        }
        entry_addr = offset + blk->blk_addr;
        pr_debug("[%s] read addr=0x%llx, len=%x\n", __func__, (long long)entry_addr, (int)read_len);
        result = mtd_read(ctx->mtd, entry_addr, ctx->buffer_size, &retlen, ctx->rw_buffer);
        if(result || retlen != ctx->buffer_size){
            pr_debug("[%s] Read failed: result=%x; retlen=%x\n", __func__, result, (int)retlen);
            break;
        }
        offset += read_len;
        tmp_crc = crc32_be(tmp_crc, ctx->rw_buffer, read_len);
        read_len = min(ctx->buffer_size, (size_t)(end_offs - offset));
    };

    if (result || tmp_crc != hdr_crc){
        pr_debug("[%s] Corrupt entry found at address 0x%llx (crc read 0x%x, calc 0x%x)\n", __func__, entry_offs + blk->blk_addr, (int)hdr_crc, (int)tmp_crc);
        goto err_out;
    }

    new_entry = kzalloc(sizeof(*new_entry), ctx->in_panic_mode ? GFP_ATOMIC : GFP_KERNEL);
    if(new_entry == NULL){
        pr_err("[%s] out of memory\n", __func__);
        goto err_out;
    }

    INIT_LIST_HEAD(&(new_entry->entry_list));
    INIT_LIST_HEAD(&(new_entry->blk_entry_list));
    INIT_LIST_HEAD(&(new_entry->segment_list));
    new_entry->ID = be32_to_cpu(entry_hdr.ID);
    new_entry->Length = be32_to_cpu(entry_hdr.Length);
    /* align end_offs to sector size, round up */
    end_offs = (end_offs + ctx->sector_msk) & ~ctx->sector_msk;
    new_entry->flash_len = end_offs - entry_offs;
    new_entry->padded_len = padded_entry_len(new_entry->Length, sizeof(entry_hdr), ctx);
    new_entry->revision = be32_to_cpu(entry_hdr.revision_nr);
    new_entry->segment_nr = be32_to_cpu(entry_hdr.segment_nr);
    new_entry->next_segment = be32_to_cpu(entry_hdr.next_segment);
    new_entry->timestamp = be32_to_cpu(entry_hdr.timestamp);
    new_entry->blk_offset = entry_offs;
    new_entry->block_ptr = blk;

err_out:
    return new_entry;
}
#endif

/*
 * parse a TFFS entry by checking the tail marker in OOB at given page offset and sector number
 */
static struct TFFS_NAND_Entry *parse_entry_oob(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk, loff_t end_page, unsigned int end_sector)
{
    struct _TFFS_NAND_Entry entry_hdr;
    struct _TFFS_NAND_OOB oob_hdr;
    struct TFFS_NAND_Entry *new_entry;
    struct mtd_oob_ops oob_ops;
    int result;
    loff_t entry_addr, first_sect, last_sect, start_page;
    uint32_t entry_len;
    unsigned int i, entry_sects;
    size_t retlen;

    /* AMY:
     * Parameters from call in scan_block:
     * end_sector = i (0 .. sectors_per_page)
     * end_page = offset (0 .. mtd->erasesize; in steps of buffer_size)
     *   end_page %= mtd->erasesize
     */
    new_entry = NULL;
    // TODO: Remove this? end_page is ALWAYS < mtd->erasesize here.
    end_page = mtd_mod_by_eb(end_page, ctx->mtd);

    oob_ops.mode = MTD_OPS_AUTO_OOB;
    oob_ops.datbuf = NULL;
    oob_ops.len = 0;
    oob_ops.oobbuf = (uint8_t *) &oob_hdr;
    oob_ops.ooboffs = end_sector * sizeof(oob_hdr);
    oob_ops.ooblen = sizeof(oob_hdr);

    result = mtd_read_oob(ctx->mtd, blk->blk_addr + end_page, &oob_ops);
    if(result != 0){
        pr_err("[%s] error reading oob entry at address 0x%llx\n", __func__, blk->blk_addr + end_page);
        goto err_out;
    }

    entry_len = padded_entry_len(be32_to_cpu(oob_hdr.Length), blk->hdr_size, ctx);
    entry_sects = DIV_ROUND_UP(entry_len, ctx->sector_size);
    last_sect = (end_page >> ctx->sector_sft) + end_sector;
    first_sect = last_sect - entry_sects + 1;
    start_page = (first_sect * ctx->sector_size) & ~ctx->buffer_msk;

    // check if there are bad pages within the entry. Since the block is scanned from start
    // to end, we can assume that the bad page table is sorted in ascending order
    // Note: We could have skipped multiple pages before reaching the end of an entry!
    //       This skips back even further to the real start address if we have bad pages in this entry.
    for(i = blk->num_bad_pages; i > 0; --i){
        if(start_page <= blk->bad_pages[i - 1] && end_page > blk->bad_pages[i - 1]){
            first_sect -= ctx->sectors_per_page;
            start_page -= ctx->buffer_size;
        }
    }

    entry_addr = blk->blk_addr + (first_sect * ctx->sector_size);

    // sanity check. Start address can never be smaller than start of second page. First
    // page is occupied by block header
    if(start_page < ctx->buffer_size){
        pr_debug("[%s] bogus oob entry at address 0x%llx\n", __func__, blk->blk_addr + end_page);
        goto err_out;
    }

    result = mtd_read(ctx->mtd, entry_addr, sizeof(entry_hdr), &retlen, (uint8_t *) &entry_hdr);
    if(result != 0){
        pr_debug("[%s] error reading entry header at address 0x%llx\n", __func__, entry_addr);
        goto err_out;
    }

#if 0
    pr_err("[%s] entry_hdr: ID: 0x%x, Length: 0x%x, timestamp: 0x%x revision_nr: 0x%x segment_nr: 0x%x next_segment: 0x%x\n",
            __func__, be32_to_cpu(entry_hdr.ID), be32_to_cpu(entry_hdr.Length),
            be32_to_cpu(entry_hdr.timestamp), be32_to_cpu(entry_hdr.revision_nr),
            be32_to_cpu(entry_hdr.segment_nr), be32_to_cpu(entry_hdr.next_segment));
#endif

    if(be32_to_cpu(entry_hdr.ID) == ~(FLASH_FS_ID_SKIP)){
        pr_err("[%s] empty entry header found at address 0x%llx\n", __func__, entry_addr);
        goto err_out;
    }

    if(entry_hdr.ID != oob_hdr.ID || be32_to_cpu(entry_hdr.Length) != be32_to_cpu(oob_hdr.Length)){
        pr_err("[%s] mismatch between data and oob headers at address 0x%llx\n", __func__, entry_addr);
        pr_err("[%s] e_hdr.ID: 0x%x o_hdr.ID: 0x%x e_hdr.Len: 0x%x, o_hdr.Len: 0x%x \n", __func__, entry_hdr.ID, oob_hdr.ID, entry_hdr.Length, oob_hdr.Length);
        goto err_out;
    }

    new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
    if(new_entry == NULL){
        pr_debug("[%s] out of memory\n", __func__);
        goto err_out;
    }

    INIT_LIST_HEAD(&(new_entry->entry_list));
    INIT_LIST_HEAD(&(new_entry->blk_entry_list));
    INIT_LIST_HEAD(&(new_entry->segment_list));
    new_entry->ID = be32_to_cpu(entry_hdr.ID);
    new_entry->Length = be32_to_cpu(entry_hdr.Length);
    new_entry->flash_len = (last_sect - first_sect + 1) * ctx->sector_size;
    new_entry->padded_len = padded_entry_len(new_entry->Length, blk->hdr_size, ctx);
    new_entry->revision = be32_to_cpu(entry_hdr.revision_nr);
    new_entry->segment_nr = be32_to_cpu(entry_hdr.segment_nr);
    new_entry->next_segment = be32_to_cpu(entry_hdr.next_segment);
    new_entry->timestamp = be32_to_cpu(entry_hdr.timestamp);
    new_entry->blk_offset = first_sect * ctx->sector_size;
    new_entry->block_ptr = blk;

err_out:
    return new_entry;
}

static void free_entry(struct TFFS_NAND_Entry *entry)
{
    BUG_ON(entry->block_ptr->used_space < entry->padded_len);

    entry->block_ptr->used_space -= entry->padded_len;

    /* first, remove entry from all lists.
     * Otherwise free_entry() will be called again on this entry if chain invalidation
     * triggers its removal
     */
    list_del_init(&(entry->blk_entry_list));
    list_del_init(&(entry->entry_list));
    list_del_init(&(entry->segment_list));

    /* if entry is member of a segment chain, invalidate it
     */
    if(entry->chain != NULL){
        invalidate_segment_chain(entry->chain);
    }

    kfree(entry);
}

static struct TFFS_Entry_Index *get_index(struct tffs_nand_ctx *ctx, enum _tffs_id Id)
{
    struct TFFS_Entry_Index *index, *tmp, *new_idx;

    list_for_each_entry(index, &(ctx->index_list), index_list){
        if(index->ID == Id){
            return index;
        }
    }

    new_idx = kzalloc(sizeof(*new_idx), GFP_KERNEL);
    if(new_idx != NULL){
        INIT_LIST_HEAD(&(new_idx->index_list));
        INIT_LIST_HEAD(&(new_idx->rev_list));
        new_idx->ID = Id;

        // find first element with ID bigger than new one and add new index before it
        list_for_each_entry_safe(index, tmp, &(ctx->index_list), index_list){
            if(index->ID > Id){
                list_add(&(new_idx->index_list), &(index->index_list));
                break;
            }
        }

        // no element bigger than new one found, add to tail of list
        if(list_empty(&(new_idx->index_list))){
            list_add_tail(&(new_idx->index_list), &(ctx->index_list));
        }
    }

    return new_idx;
}


/*
 * add new flash entry info to block info.
 */
static void add_block_entry(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Entry *entry)
{
    struct TFFS_NAND_Block *blk;

    blk = entry->block_ptr;
    if(entry->padded_len > blk->free_space){
        pr_debug("[%s] bogus entry for ID 0x%x at address 0x%llx: Len 0x%x > block free space 0x%x\n",
               __func__, entry->ID, blk->blk_addr + entry->blk_offset, entry->Length, blk->free_space);

        list_del(&(entry->blk_entry_list));
        list_del(&(entry->entry_list));
        list_del(&(entry->segment_list));
        kfree(entry);
        return;
    }

    list_add_tail(&(entry->blk_entry_list), &(blk->blk_entry_list));
    blk->free_start += entry->flash_len;
    blk->free_space -= entry->padded_len;
    blk->used_space += entry->padded_len;
}

static inline int cmp_entries(struct TFFS_NAND_Entry * first, struct TFFS_NAND_Entry *second)
{
    if(unlikely(
            first->ID == second->ID
         && first->Length == second->Length
         && first->revision == second->revision
         && first->blk_offset == second->blk_offset
         && first->flash_len == second->flash_len))
    {
        return 0;
    }

    return 1;
}
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int scan_block(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *block, unsigned int rescan)
{
    int result;
    struct _TFFS_Block_Hdr blk_hdr;
    struct _TFFS_NAND_OOB oob_hdrs[MAX_SUBPAGE_NUM];
    struct TFFS_NAND_Entry *entry, *old_entry, *tmp_entry;
    loff_t offset, first_free;
    size_t retlen, free_len;
    uint32_t bad_seen;
    unsigned int i, blk_bad, page_bad;
    LIST_HEAD(old_entries);

    // scan for bad pages and look for valid block header and entries
    pr_debug("[%s] %sscanning block at address 0x%llx\n", __func__, rescan ? "re-" : "", block->blk_addr);

    if(rescan && block->state == tffs_blk_active){
        block->state = tffs_blk_rescan;
        list_replace_init(&(block->blk_entry_list), &old_entries);
    } else {
        block->state = tffs_blk_raw;
        INIT_LIST_HEAD(&(block->blk_entry_list));
    }

    result = 0;
    offset = 0;

    block->blkseq_nr = 0;
    block->num_bad_pages = 0;
    block->used_space = 0;
    // we reserve TFFS3_MAX_BADPAGES pages for defects and 1 page for the header
    block->free_space = max_block_space(ctx);
    block->free_start = 0;
    bad_seen = 0; // bad blocks encountered so far
    first_free = 0;
    blk_bad = 0;
    page_bad = 0;

    if(mtd_block_isbad(ctx->mtd, block->blk_addr)){
        block->state = tffs_blk_bad;
        result = -EIO;
        goto err_out;
    }

    do{
        result = scan_page(ctx, block->blk_addr + offset, &blk_bad, &page_bad, &oob_hdrs[0]);

        // check for bad block marker only at first page
        // FIXME: Remove this block. blk_bad is never set in scan_page.
        if(offset == 0 && blk_bad){
            pr_warning("[TFFS3-NAND] block at address 0x%llx is marked as bad\n", block->blk_addr);
            block->state = tffs_blk_bad;
            result = -EIO;
            goto err_out;
        }

        // FIXME: Remove this block. page_bad is never set in scan_page.
        if(page_bad){
            ++bad_seen;
            pr_warning("[TFFS3-NAND] bad page at offset 0x%llx\n", offset);
            if(page_is_bad(ctx, block, offset) == 0){
                result = block_mark_page_bad(ctx, block, offset, defect_tag);

                if(result != 0){
                    pr_warning("[TFFS3-NAND] Block at address 0x%llx contains too many bad pages.\n", block->blk_addr);
                    block->state = tffs_blk_bad;
                    mtd_block_markbad(ctx->mtd, block->blk_addr);

                    result = -EIO;
                    goto err_out;
                }
            }

            // if free_start points to this page, move it to next page. We only have to check
            // if it points to start of page. If it points to a sector inside, this means that
            // there was a valid end of entry found at an earlier sector inside this page and
            // it therefore can not be bad.
            if(first_free == offset){
                first_free += ctx->buffer_size;
            }
        } else {
            switch(block->state){
            case tffs_blk_raw:
            case tffs_blk_rescan:
                // look for TFFS3 header
                result = mtd_read(ctx->mtd, block->blk_addr + offset, sizeof(blk_hdr), &retlen, (u_char *)&blk_hdr);
                if(result == 0){
                    if(be64_to_cpu(blk_hdr.magic) == TFFS3_HDR_MAGIC){
                        pr_debug("[TFFS3-NAND] found TFFS header magic at address 0x%llx, version 0x%x\n", block->blk_addr + offset, be32_to_cpu(blk_hdr.version));
                        block->entry_ver = be32_to_cpu(blk_hdr.version);
                        block->blkseq_nr = be32_to_cpu(blk_hdr.mtdnand.blkseq_nr);
                        block->erase_cnt = be32_to_cpu(blk_hdr.mtdnand.erase_cnt);
                        block->sect_per_pg = be32_to_cpu(blk_hdr.mtdnand.sect_per_pg);
                        first_free = offset + ctx->buffer_size;
                        ctx->max_block_seq = max(ctx->max_block_seq, block->blkseq_nr);
#if 0
                        pr_info("[%s] entry_ver: 0x%x blk_addr: 0x%llx blkseq_nr: 0x%x sect_per_pg: 0x%x num_bad_pages: 0x%x erase_cnt: 0x%x\n",
                                __func__,
                                block->entry_ver,
                                block->blk_addr,
                                block->blkseq_nr,
                                block->sect_per_pg,
                                block->num_bad_pages,
                                block->erase_cnt);
#endif
                        // TFFS version 3.0 ALWAYS uses oob. 3.1 (and up) does NOT.
                        if(block->entry_ver == TFFS_VERSION(3, 0)){
                            if(ctx->flags.no_oob){
                                pr_err("[TFFS3-NAND] Need NAND with OOB to handle version 3.0 block at address 0x%llx\n.", block->blk_addr);
                                block->state = tffs_blk_bad;
                                result = -EINVAL;
                                goto err_out;
                            }
                            pr_debug("[%s] Got TFFS3.0 block at addr 0x%llx\n", __func__, (long long)block->blk_addr);
                            block->read_entry = do_read;
                            block->write_entry = do_write;
                            block->hdr_size = sizeof(struct _TFFS_NAND_Entry);
#if defined(CONFIG_TFFS_NAND_V31)
                            // If we find TFFS3.0 blocks mixed with 3.1 ones, the old ones need to be updated ASAP.
                            if(ctx->flags.got_tffs31_blocks){
                                ++block->needs_rewrite;
                                pr_debug("[%s] Set is to rewrite\n", __func__);
                            }
                        } else if(block->entry_ver == TFFS_VERSION(3, 1)){
                            block->read_entry = do_read_noob;
                            block->write_entry = do_write_noob;
                            block->hdr_size = sizeof(struct _TFFS_NAND_Entry_31);
                            ctx->flags.got_tffs31_blocks = 1;
#endif
                        } else {
                            pr_err("[%s] Unable to handle entry format in block at address 0x%llx\n", __func__,block->blk_addr);
                            block->state = tffs_blk_bad;
                            result = -EINVAL;
                            goto err_out;
                        }

                        if(block->sect_per_pg != ctx->sectors_per_page){
                            pr_err("[TFFS3-NAND] incompatible sector layout found in block at address 0x%llx\n", block->blk_addr);
                            block->state = tffs_blk_raw;
                            result = 0;
                            goto err_out;
                        }

                        // get known bad pages in block
                        for(i = 0; i < be32_to_cpu(blk_hdr.mtdnand.num_bad_pages); ++i){
                            result = block_mark_page_bad(ctx, block, be64_to_cpu(blk_hdr.mtdnand.bad_pages[i]), defect_hdr);
                            if(result != 0){
                                pr_debug("[TFFS3-NAND] Adding known bad page for block at address 0x%llx failed.\n", block->blk_addr);
                                block->state = tffs_blk_bad;
                                goto err_out;
                            }
                        }
                        block->state = tffs_blk_init;
                    } else {
                        /**
                         * first page not marked as bad but contains no TFFS3 header.
                         * No error, but block needs to be formatted
                         */
                        result = 0;
                        goto err_out;
                    }
                }
                break;
            case tffs_blk_init:
                // TFFS3 header found, scan page for entries
                for(i = 0; i < ctx->sectors_per_page; ++i){
#if defined(CONFIG_TFFS_NAND_V31)
                    if(ctx->flags.no_oob){
                        entry = parse_entry_noob(ctx, block, offset, i);
                        if(entry != NULL){
                            // Update offsets here to skip entry in block parsing.
                            // FIXME: Ugly. Ideas for improvement?
                            // Add entry length to offset. This is sector aligned.
                            offset += entry->flash_len;
                            first_free = offset;
                            // Align offset to page
                            offset &= ~ctx->buffer_msk;
                            if(offset == first_free)
                                // Did this round down?
                                // If not, sub one page so offset ends up on next page after entry in next loop iteration.
                                offset -= ctx->buffer_size;
                            // Exit for loop.
                            i = ctx->sectors_per_page;
                        }
                    } else {
#endif
                        // if this sector's OOB marker has been written to, free area can earliest
                        // start at next sector
                        if(be32_to_cpu(oob_hdrs[i].ID) != ~(FLASH_FS_ID_SKIP)
                                || be32_to_cpu(oob_hdrs[i].Length) != (uint32_t) -1)
                        {
                            first_free = offset + ((i + 1) * ctx->sector_size);
                        }

                        if(   be32_to_cpu(oob_hdrs[i].ID) == FLASH_FS_ID_SKIP
                                || be32_to_cpu(oob_hdrs[i].ID) == ~(FLASH_FS_ID_SKIP))
                        {
                            /**
                             * workaround for buggy boot loader. If there is an entry
                             * with empty id but non-empty length, mark this block
                             * as unclean so we can fix it later
                             */
                            if(be32_to_cpu(oob_hdrs[i].Length) != (uint32_t) -1){
                                ++block->needs_rewrite;
                            }

                            continue;
                        }

                        entry = parse_entry_oob(ctx, block, offset, i);
#if defined(CONFIG_TFFS_NAND_V31)
                    }
#endif
                    if(entry != NULL){
                        if(likely(rescan)){ // optimize for runtime rescan
                            list_for_each_entry_safe(old_entry, tmp_entry, &old_entries, blk_entry_list){
                                if(!cmp_entries(old_entry, entry)){
                                    pr_debug("[%s] using existing struct for entry ID: 0x%x Len: 0x%x rev: 0x%x seg: 0x%x next: 0x%x at 0x%llx\n", __func__, entry->ID, entry->Length, entry->revision, entry->segment_nr, entry->next_segment, entry->blk_offset);
                                    kfree(entry);
                                    entry = old_entry;
                                    list_del_init(&(old_entry->blk_entry_list));
                                    break;

                                }
                            }
                        }

                        add_block_entry(ctx, entry);
                    }
                }
                break;
            default:
                pr_warn("[TFFS3-NAND] unknown state: 0x%x\n", block->state);
                block->state = tffs_blk_bad;
                result = -EIO;
                goto err_out;
                break;
            }
        }
        offset += ctx->buffer_size;
    } while(result == 0 && offset < ctx->mtd->erasesize);

    // adjust free_start and free_space
    block->free_start = first_free;
    free_len = ctx->mtd->erasesize - first_free;
    if(free_len < ((TFFS3_MAX_BADPAGES - bad_seen) << ctx->buffer_sft)){
        free_len = 0;
    } else {
        free_len -= (TFFS3_MAX_BADPAGES - bad_seen) << ctx->buffer_sft;
    }
    block->free_space = free_len;
    block->state = tffs_blk_active;

#if 0
    pr_err("[%s] blk_addr: 0x%llx blkseq_nr: 0x%x sect_per_pg: 0x%x num_bad_pages: 0x%x bad_seen: 0x%x free_start: 0x%llx free_space: 0x%x used_space: 0x%x, erase_cnt: 0x%x\n",
            __func__,
            block->blk_addr,
            block->blkseq_nr,
            block->sect_per_pg,
            block->num_bad_pages,
            bad_seen,
            block->free_start,
            block->free_space,
            block->used_space,
            block->erase_cnt);
#endif
err_out:
    // release entries if block turned out to be bad
    if(block->state == tffs_blk_bad && !list_empty(&(block->blk_entry_list))){
        while(!list_empty(&(block->blk_entry_list))){
            entry = list_first_entry(&(block->blk_entry_list), struct TFFS_NAND_Entry, blk_entry_list);
            free_entry(entry);
        }
    }

    // any old entries left after rescan? Release them after setting used space info
    // to zero, so they will not fsck up the block accounting
    while(!list_empty(&old_entries)){
        old_entry = list_first_entry(&old_entries, struct TFFS_NAND_Entry, blk_entry_list);
        pr_debug("[%s] freeing stale entry ID: 0x%x Len: 0x%x rev: 0x%x at 0x%llx\n", __func__, old_entry->ID, old_entry->Length, old_entry->revision, old_entry->blk_offset);
        old_entry->padded_len = 0;
        old_entry->flash_len = 0;
        free_entry(old_entry);
    }

    return result;
}

static int add_entry_to_index(struct tffs_nand_ctx *ctx, struct TFFS_Entry_Index *idx, struct TFFS_NAND_Entry *entry, unsigned int prunable)
{
    int result;
    struct TFFS_NAND_Entry *chain_entry;
    struct TFFS_NAND_SegChain *chain, *new_chain;

    result = 0;

    // remember max. revision number we have seen for this ID, even if invalid
    idx->max_rev = max(idx->max_rev, entry->revision);

    // we can not risk memory allocations in panic mode
    if(ctx->in_panic_mode){
        pr_emerg("[%s] Called in panic mode!\n", __func__);
        dump_stack();
        result = -EBUSY;
        goto err_out;
    }

#if 0
    /* no need to keep the entry data cached if we know that there is a valid
     * segment chain with equal or higher revision number available
     */
    if(entry->revision <= idx->valid_rev){
        result = -EEXIST;
        free_entry(entry);
        goto err_out;
    }
#endif

    // TODO: during normal operation numbers will always go up, so reverse sort
    //       (or search) order

    /* we keep the list sorted in ascending order. Walk through list
     * until we find an element with matching or higher revision number
     */
    list_for_each_entry(chain, &(idx->rev_list), rev_list){
        if(chain->revision >= entry->revision){
            break;
        }
    }

    /* we may need to insert a new chain element in the revision list.
     * If we reached the end of the list (ended up at the list head again),
     * this could mean:
     * 1. list was empty
     * 2. new revision is higher than all entries in list
     *
     * if the search stopped somewhere inside the list, we may have found
     * a higer revision entry.
     *
     * In any case, we need to insert the new revision element before the head we reached
     *
     */
    if(   &(chain->rev_list) == &(idx->rev_list)
       || chain->revision > entry->revision)
    {
        new_chain = kzalloc(sizeof(*new_chain), GFP_KERNEL);
        if(new_chain == NULL){
            result = -ENOMEM;
            free_entry(entry);

            goto err_out;
        }

        INIT_LIST_HEAD(&(new_chain->rev_list));
        INIT_LIST_HEAD(&(new_chain->segment_list));
        kref_init(&(new_chain->refcnt));
        new_chain->idx = idx;
        new_chain->ID = entry->ID;
        new_chain->revision = entry->revision;
        new_chain->complete = 0;
        if(prunable){
            new_chain->prune = 1;
        }
        list_add_tail(&(new_chain->rev_list), &(chain->rev_list));

        chain = new_chain;
    }

    /* we have either found or created a segment chain head for this entry. Insert the
     * new entry into the chain. Find first element with equal or higer segment number
     * (or end/start of list)
     */
    list_for_each_entry(chain_entry, &(chain->segment_list), segment_list){
        if(chain_entry->segment_nr >= entry->segment_nr){
            break;
        }
    }

    /* if we have a power loss during cleanup, there might exist duplicate entries.
     * We keep the one from the block with the higher sequence number.
     */
    if(   &(chain_entry->segment_list) != &(chain->segment_list)
       && chain_entry->segment_nr == entry->segment_nr)
    {
        if(chain_entry == entry){
            pr_err("[TFFS3-NAND] Double add to index detected!\n");
            result = -EEXIST;
            goto err_out;
        }

        // lower block sequence number? Just drop the new entry.
        if(chain_entry->block_ptr->blkseq_nr > entry->block_ptr->blkseq_nr){
            free_entry(entry);
            result = -EEXIST;
        } else {
            // new entry is from more recent block. Replace and drop the older entry.
            entry->chain = chain;
            list_replace_init(&(chain_entry->segment_list), &(entry->segment_list));

            // clear pointer to chain so free_entry() will not trigger its invalidation
            chain_entry->chain = NULL;
            free_entry(chain_entry);
            result = 0;
        }
        goto err_out;
    }
    entry->chain = chain;

    /* chain_entry points to either a list item with bigger segment number or to
     * to the list head. We need to insert the entry before it in either case.
     */
    list_add_tail(&(entry->segment_list), &(chain_entry->segment_list));

    /* if this is the last entry of a segment chain, we can check if it is
     * complete. This will mark the chain  as valid as soon as possible in case of normal
     * write operations. During initial scan segments may come in any order, so after the
     * block scan there will have to be an explicit check for all chains
     */
    if(entry->next_segment == 0){
        validate_segment_chain(ctx, chain);
    }

err_out:
    return result;
}

static int add_block_to_index(struct tffs_nand_ctx *ctx, struct TFFS_NAND_Block *blk)
{
    struct TFFS_Entry_Index *idx;
    struct TFFS_NAND_Entry *entry, *tmp;
    int result;

    result = 0;
    list_for_each_entry_safe(entry, tmp, &(blk->blk_entry_list), blk_entry_list){
        // do not add old entries that where rediscovered during block rescan
        if(entry->chain != NULL){
            continue;
        }

        idx = get_index(ctx, entry->ID);
        if(idx == NULL){
            pr_err("[TFFS3-NAND] error fetching index object for ID 0x%x\n", entry->ID);
            result = -ENOMEM;
            goto err_out;
        }

        result = add_entry_to_index(ctx, idx, entry, 1);
        if(result != 0){
            if(result == -EEXIST){
                // not really an error, there might already be a valid segment chain of higher rev.
                pr_debug("[%s] adding stale entry to index failed\n", __func__);
            } else {
                pr_err("[TFFS3-NAND] adding entry to index failed with code %d\n", result);
                goto err_out;
            }
        }
    }

    result = 0;

err_out:
    return result;
}

static int check_segment_chain(struct tffs_nand_ctx *ctx, struct TFFS_NAND_SegChain *chain)
{
    struct TFFS_NAND_Entry *entry;
    uint32_t next_nr;
    int result;

    // No point in checking if segment list is empty
    if(list_empty(&(chain->segment_list))){
        return -1;
    }

    result = -1;

    // special case of cleared entry. Single segment with segment_nr set to TFFS_SEG_CLEARED
    if(list_is_singular(&(chain->segment_list))){
        entry = list_first_entry(&(chain->segment_list), struct TFFS_NAND_Entry, segment_list);
        if(entry->segment_nr == TFFS_SEG_CLEARED && entry->next_segment == 0){
            result = 0;
            goto out;
        }
    }

    // chains always start with segment 0. Traverse chain until an unexpected segment is found
    // or the end is reached.
    next_nr = 0;
    list_for_each_entry(entry, &(chain->segment_list), segment_list){
        if(entry->segment_nr != next_nr){
            result = -1;
            goto out;
        }
        next_nr = entry->next_segment;
    }

    // next_segment for last segment must be 0
    if(next_nr == 0){
        result = 0;
    }

out:
    return result;
}

static void free_segment_chain(struct TFFS_NAND_SegChain *chain)
{
    struct TFFS_NAND_Entry *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &(chain->segment_list), segment_list){
        free_entry(entry);
    }

    list_del(&(chain->rev_list));
    kfree(chain);
}

static void validate_segment_chain(struct tffs_nand_ctx *ctx, struct TFFS_NAND_SegChain *chain)
{
    struct TFFS_NAND_SegChain *prev_chain;

    if(check_segment_chain(ctx, chain) == 0){
        chain->complete = 1;
        if(chain->revision > chain->idx->valid_rev){
            prev_chain = chain->idx->chain_ptr;

            chain->idx->valid_rev = chain->revision;
            chain->idx->chain_ptr = chain;
            // chain is new active chain, increase refcount
            get_chain(chain);

            // if there was a previously active chain, decrease its refcount
            if(prev_chain != NULL){
                put_chain(prev_chain);

                // if notification call back is registered, inform it about update
                if(ctx->notify_cb != NULL && ctx->in_panic_mode == 0){
                    ctx->notify_cb(ctx->notify_priv, chain->ID, tffs3_notify_update);
                }
            }
        }
    }
}

static inline void invalidate_segment_chain(struct TFFS_NAND_SegChain *chain)
{
    // increase corruption counter and mark chain as incomplete
    ++chain->corrupt_cnt;
    chain->complete = 0;

    /* if this chain is the active revision in index, invalidate the index and drop
     * its held reference. Highly unlikely to be called on active chain, so optimise
     * for nothing to do
     */
    if(likely(chain->idx != NULL) && unlikely(chain->idx->chain_ptr == chain)){
        chain->idx->chain_ptr = NULL;
        chain->idx->valid_rev = 0;
        put_chain(chain);
    }
}

static void release_segment_chain(struct kref *refcnt)
{
    struct TFFS_NAND_SegChain *chain;

    chain = container_of(refcnt, struct TFFS_NAND_SegChain, refcnt);

    if(chain->idx == NULL || chain->revision != chain->idx->valid_rev){
        free_segment_chain(chain);
    } else {
        pr_err("[TFFS3-NAND] Not freeing active segment chain for ID 0x%x, revision 0x%x!\n", chain->ID, chain->revision);
        get_chain(chain);
    }
}


static int prune_index(struct tffs_nand_ctx *ctx)
{
    struct TFFS_Entry_Index *idx;
    struct TFFS_NAND_SegChain *chain, *tmp_chain;
    int result;

    pr_debug("[%s] called\n", __func__);

    result = 0;
    list_for_each_entry(idx, &(ctx->index_list), index_list){
        list_for_each_entry_safe_reverse(chain, tmp_chain, &(idx->rev_list), rev_list){
            idx->max_rev = max(chain->revision, idx->max_rev);

            // drop one reference on all chains created by scanning the MTD
            if(chain->prune != 0){
                chain->prune = 0;
                validate_segment_chain(ctx, chain);
                put_chain(chain);
            }
        }
    }

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int scan_mtd(struct tffs_nand_ctx *ctx)
{
    uint32_t blocks;
    loff_t blk_addr;
    unsigned int subpages;
    struct mtd_info *mtd;
    int result;
    struct TFFS_NAND_Block *nand_blk, *tmp_blk;
    uint32_t blkseq_nr;

    // pr_debug("[%s] Called\n", __func__);

    mtd = ctx->mtd;

    pr_debug("[%s] name: %s size: 0x%llx erasesize: 0x%x writesize: 0x%x oobsize: 0x%x oobavail: 0x%x subpages: 0x%x\n",
            __func__, mtd->name, mtd->size, mtd->erasesize, ctx->buffer_size, mtd->oobsize, mtd->oobavail,
            (1 << mtd->subpage_sft));

    blocks = mtd_div_by_eb(mtd->size, mtd);
    if(ctx->flags.no_oob){
        subpages = 1;
    } else {
        subpages = min_t(uint32_t, (1UL << mtd->subpage_sft), (mtd->oobavail / sizeof(struct _TFFS_NAND_OOB)));
        subpages = min_t(uint32_t, subpages, MAX_SUBPAGE_NUM);
    }

    if(subpages == 0){
#if !defined(CONFIG_TFFS_NAND_V31)
        pr_err("[TFFS3-NAND] invalid OOB configuration, aborting!\n");
        return -EINVAL;
#else
        pr_debug("[%s] subpage_sft: 0x%x, erasesize_shift: 0x%x, buffer_shift: 0x%x, writesize_shift: 0x%x blocks: 0x%x\n", __func__, mtd->subpage_sft, mtd->erasesize_shift, ctx->buffer_sft, mtd->writesize_shift, blocks);
        subpages = 1;
        ctx->flags.no_oob = 1;
#endif
    }
    subpages = rounddown_pow_of_two(subpages);

    ctx->block_cnt = blocks;
    ctx->pages_per_block = mtd_div_by_ws(mtd->erasesize, ctx->mtd);
    ctx->pages_per_block = mtd->erasesize >> ctx->buffer_sft;
    ctx->sectors_per_page = subpages;
    ctx->sector_size = ctx->buffer_size >> ilog2(subpages);
    ctx->sector_sft = ilog2(ctx->sector_size);
    ctx->sector_msk = (1UL << ctx->sector_sft) - 1;

    pr_debug("[%s] sector_size: 0x%x sectors_per_page: 0x%x buffer_sft: 0x%x sector_sft: 0x%x sector_msk: 0x%x; pages_per_block: 0x%x\n", __func__, ctx->sector_size, ctx->sectors_per_page, ctx->buffer_sft, ctx->sector_sft, ctx->sector_msk, ctx->pages_per_block);

#if 0 // force defect for testing
    {
        struct _TFFS_NAND_OOB test_hdr;
        struct mtd_oob_ops oob_ops;
        pr_err("[%s] destroying oob at page 0x1c800\n", __func__);

        memset(&test_hdr, 0x0, sizeof(test_hdr));

        oob_ops.mode = MTD_OOB_AUTO;
        oob_ops.datbuf = NULL;
        oob_ops.len = 0;
        oob_ops.oobbuf = (uint8_t *)&test_hdr;
        oob_ops.ooboffs = 0;
        oob_ops.ooblen = sizeof(test_hdr);

        result = write_oob(ctx, 0x1c800, &oob_ops);
        pr_err("[%s] result: %d oobretlen: 0x%x\n", __func__, result, oob_ops.oobretlen);
    }
#endif
    result = 0;
    blk_addr = 0;
    blkseq_nr = 0;
    while(blk_addr < (blocks * ctx->mtd->erasesize)){

        nand_blk = (struct TFFS_NAND_Block *)kzalloc(sizeof(*nand_blk), GFP_KERNEL);
        if(nand_blk == NULL){
            pr_err("[TFFS3-NAND] out of memory during block initialisation, aborting\n");
            result = -ENOMEM;
            goto err_out;
        }

        nand_blk->blk_addr = blk_addr;

        pr_debug("[TFFS3-NAND] scanning block at address 0x%llx\n", blk_addr);

        result = scan_block(ctx, nand_blk, 0);
        if(result == 0){
            list_add_tail(&(nand_blk->blk_list), &(ctx->blk_list));
        } else {
            pr_warn("[TFFS3-NAND] Error scanning block at address 0x%llx\n", blk_addr);
            kfree(nand_blk);
        }

        blk_addr += ctx->mtd->erasesize;
    }

    list_for_each_entry_safe(nand_blk, tmp_blk, &(ctx->blk_list), blk_list){
        if(nand_blk->state == tffs_blk_raw){
            pr_debug("[TFFS3-NAND] Formatting block at address 0x%llx with revision no. 0x%x\n", nand_blk->blk_addr, ctx->max_block_seq + 1);
            result = format_block(ctx, nand_blk, ++(ctx->max_block_seq));
            if(result != 0){
                pr_err("[TFFS3-NAND] formatting block at address 0x%llx failed, ignoring it.\n", nand_blk->blk_addr);
                list_del(&(nand_blk->blk_list));
                kfree(nand_blk);
                continue;
            }

            result = scan_block(ctx, nand_blk, 0);
            if(result != 0){
                pr_err("[TFFS3-NAND] Error re-scanning block at address 0x%llx failed, aborting!\n", nand_blk->blk_addr);
                list_del(&(nand_blk->blk_list));
                kfree(nand_blk);
                continue;
            }
        }

        // add entries in block to index
        result = add_block_to_index(ctx, nand_blk);
        if(result != 0){
            pr_err("[TFFS3-NAND] adding block number 0x%x to index failed, aborting!.\n", nand_blk->blkseq_nr);
            goto err_out;
        }
    }

    result = prune_index(ctx);
    // Make sure we have an index entry for these.
    get_index(ctx, FLASH_FS_ID_CRASH2_LOG);
    get_index(ctx, FLASH_FS_ID_PANIC2_LOG);
    get_index(ctx, FLASH_FS_ID_CRASH_LOG);
    get_index(ctx, FLASH_FS_ID_PANIC_LOG);

    // handle blocks with unclean entries
    list_for_each_entry(nand_blk, &(ctx->blk_list), blk_list){
        if(nand_blk->needs_rewrite != 0){
            pr_debug("[%s] non-fatal error detected on block number 0x%x, running clean\n",
                     __func__, nand_blk->blkseq_nr);

            result = clean_block(ctx, nand_blk);
            if(result != 0){
                pr_debug("[%s] cleaning block at address 0x%llx failed\n", __func__, nand_blk->blk_addr);
                goto err_out;
            }
        }
    }

    // print some statistics
    pr_debug("[%s] after prune_index:\n", __func__);
    list_for_each_entry(nand_blk, &(ctx->blk_list), blk_list){
        if(nand_blk->state == tffs_blk_active){
            pr_debug("[%s] block number 0x%x, blk_addr: 0x%llx, free_start: 0x%llx, used: 0x%x\n",
                   __func__, nand_blk->blkseq_nr, nand_blk->blk_addr, nand_blk->free_start, nand_blk->used_space);
        }
    }


    return result;

err_out:
    list_for_each_entry_safe(nand_blk, tmp_blk, &(ctx->blk_list), blk_list){
        list_del(&(nand_blk->blk_list));
        kfree(nand_blk);
    }

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Reindex(struct tffs_module *this)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_Block *blk, *blk_tmp;
    struct TFFS_NAND_Entry *entry, *entry_tmp;

    int result;

    ctx = (struct tffs_nand_ctx *) this->priv;
    if(ctx == NULL){
        pr_debug("[%s] Context not initialised\n", __func__);
        return -ENODEV;
    }

    if(ctx->in_panic_mode){
        return -EBUSY;
    }

#if defined(DEBUG_CHAINS)
    {
        struct TFFS_Entry_Index *idx;
        struct TFFS_NAND_SegChain *chain;
        list_for_each_entry(idx, &(ctx->index_list), index_list){
            list_for_each_entry(chain, &(idx->rev_list), rev_list){
                pr_err("[%s] ID: 0x%x Revision: 0x%x Complete: 0x%x, CorruptCnt: 0x%x, Prunable: 0x%x, RefCnt: 0x%x\n", __func__, chain->ID, chain->revision, chain->complete, chain->corrupt_cnt, chain->prune, chain->refcnt.refcount.counter);
            }
        }
    }
#endif

    result = 0;

    list_for_each_entry_safe(blk, blk_tmp, &(ctx->blk_list), blk_list){
        result = scan_block(ctx, blk, 1);

        if(result == 0 && blk->state == tffs_blk_raw){
            pr_debug("[%s] Formatting block at address 0x%llx with revision no. 0x%x\n", __func__, blk->blk_addr, ctx->max_block_seq + 1);
            result = format_block(ctx, blk, ++(ctx->max_block_seq));
            if(result == 0){
                result = scan_block(ctx, blk, 0);
            } else {
                pr_err("[TFFS3-NAND] formatting block at address 0x%llx failed.\n", blk->blk_addr);
            }
        }

        if(result != 0){
            pr_err("[TFFS3-NAND] Error: re-scanning block at address 0x%llx failed\n", blk->blk_addr);
            list_del(&(blk->blk_list));
            kfree(blk);
            continue;
        }

        result = add_block_to_index(ctx, blk);
        if(result != 0){
            // clean up if adding (some) entries to index failed
            pr_warning("[TFFS3-NAND] Error on block at 0x%llx while rebuilding TFFS index!\n", blk->blk_addr);

            // free all entries in this block that have not been picked up by the index
            list_for_each_entry_safe(entry, entry_tmp, &(blk->blk_entry_list), blk_entry_list){
                if(entry->chain == NULL){
                    pr_debug("[%s] Block at 0x%llx: dropping entry ID: 0x%x Revision: 0x%x , Segment: 0x%x\n", __func__, blk->blk_addr, entry->ID, entry->revision, entry->segment_nr);
                    free_entry(entry);
                }
            }
        }
    }

#if defined(DEBUG_CHAINS)
    {
        struct TFFS_Entry_Index *idx;
        struct TFFS_NAND_SegChain *chain;
        list_for_each_entry(idx, &(ctx->index_list), index_list){
            list_for_each_entry(chain, &(idx->rev_list), rev_list){
                pr_err("[%s] ID: 0x%x Revision: 0x%x Complete: 0x%x, CorruptCnt: 0x%x, Prunable: 0x%x, RefCnt: 0x%x\n", __func__, chain->ID, chain->revision, chain->complete, chain->corrupt_cnt, chain->prune, chain->refcnt.refcount.counter);
            }
        }
    }
#endif

    result = prune_index(ctx);

#if defined(DEBUG_CHAINS)
    {
        struct TFFS_Entry_Index *idx;
        struct TFFS_NAND_SegChain *chain;
        list_for_each_entry(idx, &(ctx->index_list), index_list){
            list_for_each_entry(chain, &(idx->rev_list), rev_list){
                pr_err("[%s] ID: 0x%x Revision: 0x%x Complete: 0x%x, CorruptCnt: 0x%x, Prunable: 0x%x, RefCnt: 0x%x\n", __func__, chain->ID, chain->revision, chain->complete, chain->corrupt_cnt, chain->prune, chain->refcnt.refcount.counter);
            }
        }
    }
#endif

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Info(struct tffs_module *this, unsigned int *Fill)
{
    struct tffs_nand_ctx *ctx;
    struct TFFS_NAND_Block *blk;
    unsigned int blk_cnt;
    size_t blksize;

    pr_debug("[%s] Called\n", __func__);

    ctx = (struct tffs_nand_ctx *) this->priv;
    if(ctx == NULL){
        pr_debug("[%s] Context not initialised\n", __func__);
        return -ENODEV;
    }

    if(ctx->in_panic_mode){
        return -EBUSY;
    }

    *Fill = 100;
    blk_cnt = 0;
    blksize = max_block_space(ctx);
    list_for_each_entry(blk, &(ctx->blk_list), blk_list){
        if(blk->state == tffs_blk_active){
            ++blk_cnt;
            *Fill = DIV_ROUND_CLOSEST(((*Fill * (blk_cnt - 1)) + DIV_ROUND_CLOSEST((blk->used_space * 100), blksize)), blk_cnt);
        }
    }

    *Fill = (*Fill > 100) ? 100 : *Fill;

    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Register_Notify(struct tffs_module *this, void *notify_priv, tffs3_notify_fn notify_cb)
{
    struct tffs_nand_ctx *ctx;
    int result;

    ctx = (struct tffs_nand_ctx *) this->priv;

    result = 0;
    if(ctx->notify_cb == NULL){
        ctx->notify_priv = notify_priv;
        ctx->notify_cb = notify_cb;
    } else {
        result = -EEXIST;
    }


    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Remove_Notify(struct tffs_module *this, void *notify_priv, tffs3_notify_fn notify_cb)
{
    struct tffs_nand_ctx *ctx;
    int result;

    ctx = (struct tffs_nand_ctx *) this->priv;

    result = -EINVAL;
    if(ctx->notify_priv == notify_priv && ctx->notify_cb == notify_cb){
        ctx->notify_cb = NULL;
        ctx->notify_priv = NULL;
        result = 0;
    }

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_NAND_Setup(struct tffs_module *this)
{
    struct tffs_nand_ctx *ctx;
    struct mtd_info *mtd;
    int result;
    struct TFFS_NAND_Block *blk;
    unsigned int blk_act, blk_cnt, fill;
    unsigned int i, j, buf_sft;

    pr_info("[TFFS3-NAND] NAND storage backend for TFFS 3.x\n");

    result = -EINVAL;

    ctx = (struct tffs_nand_ctx *) this->priv;
    BUG_ON(ctx == NULL);

    mtd = get_mtd_device(ctx->mtd, -1);
    if(IS_ERR(mtd)){
        pr_err("[TFFS3-NAND] Unable to get MTD device %s\n", ctx->mtd->name);
        result = -ENODEV;
        goto err_out;
    }

    // BLOCK2MTD is of MTD_RAM type.
#if defined(CONFIG_TFFS_NAND_V31)
    if(mtd->type != MTD_NANDFLASH && mtd->type != MTD_RAM){
        pr_err("[TFFS3-NAND] MTD %s not NAND or RAM type\n", mtd->name);
        result = -EINVAL;
        goto err_out;
    }

    // Assume this is block2mtd if 1.
    if (ctx->mtd->writesize == 1){
        ctx->buffer_size = MIN_WRITESIZE;
        ctx->flags.no_oob = 1;
        if(ctx->buffer_size >= ctx->mtd->erasesize || ctx->mtd->erasesize % ctx->buffer_size != 0){
            // erasesize needs to be multiple of our writesize.
            pr_err("[TFFS3-NAND] MTD %s has invalid erasesize: 0x%x\n", mtd->name, mtd->erasesize);
            result = -EINVAL;
            goto err_out;
        }
        ctx->buffer_msk = MIN_WRITESIZE - 1;
        ctx->buffer_sft = ilog2(MIN_WRITESIZE);
    } else {
        ctx->buffer_size = ctx->mtd->writesize;
        ctx->buffer_msk = ctx->mtd->writesize_mask;
        ctx->buffer_sft = ctx->mtd->writesize_shift;
    }
#else
    if(mtd->type != MTD_NANDFLASH){
        pr_err("[TFFS3-NAND] MTD %s not NAND type\n", mtd->name);
        result = -EINVAL;
        goto err_out;
    }

    ctx->buffer_size = ctx->mtd->writesize;
    ctx->buffer_msk = ctx->mtd->writesize_mask;
    ctx->buffer_sft = ctx->mtd->writesize_shift;
#endif

    i = 0;
    j = ctx->buffer_size;
    buf_sft = 0;
    while(j){
        if((j & 1) && (i++)){
            pr_err("[TFFS3-NAND] MTD writesize needs to be power of 2 (is %x)",
                    (unsigned int)ctx->mtd->writesize);
            result = -EINVAL;
            goto err_out;
        }
        j >>= 1;
        buf_sft++;
    }

    if(ctx->buffer_msk == 0){
        ctx->buffer_msk = ctx->buffer_size - 1;
    }
    if(ctx->buffer_sft == 0){
        ctx->buffer_sft = buf_sft;
    }

    if(ctx->mtd->erasesize < ((TFFS3_MAX_BADPAGES + 1) << ctx->buffer_sft)){
        pr_err("[TFFS3-NAND] MTD erasesize 0x%x is too small. Need at least 0x%x for a minimum writesize of 0x%x\n",
                (int)ctx->mtd->erasesize,
                (int)((TFFS3_MAX_BADPAGES + 1) << ctx->buffer_sft),
                (int)ctx->buffer_size);
        result = -EINVAL;
        goto err_out;
    }

    if(ctx->mtd->erasesize % ctx->buffer_size){
        pr_err("[TFFS3-NAND] Erasesize (0x%x) must be multiple of page size (0x%x).\n",
                (int)ctx->mtd->erasesize,
                (int)ctx->buffer_size);
        result = -EINVAL;
        goto err_out;
    }

    ctx->rw_buffer = kmalloc(ctx->buffer_size, GFP_KERNEL);
    if(ctx->rw_buffer == NULL){
        result = -ENOMEM;
        goto err_out;
    }

    down(&(ctx->lock));

    INIT_LIST_HEAD(&(ctx->blk_list));
    INIT_LIST_HEAD(&(ctx->entry_list));
    INIT_LIST_HEAD(&(ctx->index_list));
    result = scan_mtd(ctx);

    up(&(ctx->lock));

    if(result == 0){
        TFFS3_NAND_Info(this, &fill);

        blk_cnt = 0;
        blk_act = 0;
        list_for_each_entry(blk, &(ctx->blk_list), blk_list){
            ++blk_cnt;
            if(blk->state == tffs_blk_active){
                ++blk_act;
            }
        }

        pr_info("[TFFS3-NAND] Initialisation successful, %d/%d/%lld NAND blocks active, fill rate %d%%\n", blk_act, blk_cnt, ctx->block_cnt, fill);
    }

err_out:
	if(result != 0){
        pr_info("[TFFS3-NAND] Initialisation failed!\n");

        if(ctx != NULL){
	        this->priv = NULL;
	        kfree(ctx);
	    }
	    if(mtd != NULL && !IS_ERR(mtd)){
	        put_mtd_device(mtd);
	    }
	}
    
    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_NAND_Configure(struct tffs_module *this, struct mtd_info *mtd)
{
    struct tffs_nand_ctx *ctx;
    int result;

    pr_debug("[%s] Called\n", __func__);

    result = -EINVAL;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if(ctx == NULL){
        pr_err("[TFFS3-NAND] Out of memory during configuration\n");
        result = -ENOMEM;
        goto err_out;
    }

    if(IS_ERR_OR_NULL(mtd)){
        pr_err("[TFFS3-NAND] Invalid pointer to mtd: %p\n", mtd);
        result = -ENODEV;
        goto err_out;
    }

    ctx->mtd = mtd;

//    ctx->mtd_num = mtd_num;
    this->priv = ctx;
    sema_init(&(ctx->lock), 1);

    this->name = "nand";
    this->setup = TFFS3_NAND_Setup;
    this->open = TFFS3_NAND_Open;
    this->close = TFFS3_NAND_Close;
    this->read = TFFS3_NAND_Read;
    this->write = TFFS3_NAND_Write;
    this->cleanup = TFFS3_NAND_Cleanup;
    this->reindex = TFFS3_NAND_Reindex;
    this->info = TFFS3_NAND_Info;
    this->register_notify = TFFS3_NAND_Register_Notify;
    this->remove_notify = TFFS3_NAND_Remove_Notify;

    result = 0;

err_out:
    return result;
}
EXPORT_SYMBOL(TFFS3_NAND_Configure);

/* vim: set sw=4 ts=4 et ai cc=80 : */
