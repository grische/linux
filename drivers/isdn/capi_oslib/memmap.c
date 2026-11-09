// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/hardirq.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#pragma GCC pop_options

#include "consts.h"
#include "debug.h"
#include <avm/capi_oslib/oslib.h>
#include <avm/capi_oslib/new.h>
#include "appl.h"
#include "capi_oslib.h"
#include "local_capi.h"
#include "ca.h"
#include "zugriff.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0) && \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 168)
#	define get_user_pages(start, len, gup_flags, pages) \
		get_user_pages(current, current->mm, start, len, \
				gup_flags, pages, NULL)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
#	define get_user_pages(start, len, gup_flags, pages) \
		get_user_pages(current, current->mm, start, len, \
				!!(gup_flags & FOLL_WRITE), \
				!!(gup_flags & FOLL_FORCE), pages, NULL)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
#	define get_user_pages(start, len, gup_flags, pages) \
		get_user_pages(start, len, \
				!!(gup_flags & FOLL_WRITE), \
				!!(gup_flags & FOLL_FORCE), pages, NULL)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0)
#	define get_user_pages(start, len, gup_flags, pages) \
		get_user_pages(start, len, gup_flags, pages, NULL)
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 208)
/**
 */
static inline void mmap_read_lock(struct mm_struct *mm)
{
	down_read(&mm->mmap_sem);
}

/**
 */
static inline void mmap_read_unlock(struct mm_struct *mm)
{
	up_read(&mm->mmap_sem);
}
#endif

/**
 */
enum _user_space_block_action {
	_do_nothing_ = 0,
	_do_map_     = 1,
	_do_unmap_   = 2
};

/**
 */
static void *capi_oslib_map_data_b3_req(void *user_ptr, unsigned int len, enum _user_space_block_action map)
{
	unsigned int kaddress = 0, offset;
	unsigned int uaddress = (unsigned int) (unsigned long)user_ptr;
	struct page *page;
	/**
	 *  page align
	 */
	offset = uaddress & ((1 << PAGE_SHIFT) - 1);
	uaddress &= ~((1 << PAGE_SHIFT) - 1);

	/**
	 * Seite mappen
	 */
	page = follow_page((void *)current->mm, uaddress, 0 /* nur read */);
	if (page == NULL) {
		DEB_ERR("memory page no present, while %s !\n",
				map == _do_nothing_ ? "_do_nothing_" :
				map == _do_map_ ? "_do_map_" :
				map == _do_unmap_ ? "_do_unmap_" : "unknown");
		return 0;
	}
	if (len + offset > (1 << PAGE_SHIFT)) {
		struct page *next_page;

		DEB_INFO("[data_b3_req] check: buffer not in one page\n");
		next_page = follow_page((void *)current->mm, uaddress + (1 << PAGE_SHIFT), 0 /* nur read */);
		if (page + 1 != next_page) {
			DEB_ERR("[data_b3_req] ALLOC Error: buffer not in one page\n");
		}
	}

	/**
	 * Seite mappen
	 */
	kaddress = (unsigned int)(unsigned long)lowmem_page_address(page) + offset;
	return (void *)(unsigned long)kaddress;
}

/**
 * map den durch das register übergebene user memory block in den Kernel space
 */
static int capi_oslib_map_register_block(struct _adr_map *map)
{
	unsigned int size  = map->size;
	unsigned int len   = map->len;
	unsigned char *user_address = (unsigned char *)map->user;
	struct page **P;
	int ret;

	P = kmalloc(sizeof(struct page *) * size, GFP_KERNEL);
	BUG_ON(in_softirq());
	mmap_read_lock(current->mm);
	ret = get_user_pages(
				(unsigned long)user_address, /*--- unsigned long start, ---*/
				size, /*--- int len, ---*/
				FOLL_WRITE, /*--- unsigned int gup_flags ---*/
				P /*--- struct page **pages, ---*/
				);
	mmap_read_unlock(current->mm);

	if (ret > 0) {
		int i;

		for (i = 0 ; i < ret ; i++) {
			map->pages[i].InUse  = _entry_in_use_;
			map->pages[i].user   = user_address + i * (1 << PAGE_SHIFT);
			map->pages[i].kernel = lowmem_page_address(P[i]);
			map->pages[i].len    = (1 << PAGE_SHIFT);
			map->pages[i].P      = P[i];
		}
		kfree(P);
		DEB_INFO("[%s] page array %u entries, %u used\n", __func__, map->size, ret);
		return 0;
	}
	kfree(P);
	DEB_ERR("[%s] could not map 0x%p (len %u, %u pages) to kernel memory\n",
		__func__, user_address, len, size);
	return ret;
}


/**
 */
void capi_oslib_release_b3_buffer(void *context)
{
	DEB_ERR("[%s] callback context=0x%p\n", __func__, context);
}

/**
 */
int capi_oslib_register_user_space_blocks(struct _capi_oslib_open_data *open_data,
					  void *user, unsigned int len, unsigned int MaxNCCIs,
					  unsigned int WindowSize, unsigned int B3BlockSize)
{
	int anzahl = (len + ((1 << PAGE_SHIFT) - 1)) >> PAGE_SHIFT;
	int ret, ncci, window, index, b3;
	unsigned char *p, *u;

	open_data->data_buffer = (struct _adr_map *)CA_MALLOC(sizeof(struct _adr_map_pages) * (anzahl - 1) + sizeof(struct _adr_map));

	if (open_data->data_buffer == NULL) {
		DEB_ERR("[register_user_space_blocks] no memory !\n");
		return -EFAULT;
	}
	open_data->data_buffer->user = user;
	open_data->data_buffer->size = anzahl;
	open_data->data_buffer->len  = len;

	ret = capi_oslib_map_register_block(open_data->data_buffer);
	if (ret)
		return ret;

	/*
	 * Speicher fuer die mapping tabelle
	 */
	open_data->b3_data = (struct _adr_b3_ind_data *)CA_MALLOC(sizeof(struct _adr_b3_ind_data) * open_data->data_buffer->size);
	if (open_data->b3_data == NULL) {
		DEB_ERR("[] out of memory\n");
		return -ENOMEM;
	}

	/*
	 * Mapping Tabelle fuellen
	 */
	for (ncci = 0, index = 0, b3 = 0, u = open_data->data_buffer->user, p = NULL ; ncci < (int)MaxNCCIs ; ncci++) {
		for (window = 0 ; window < (int)WindowSize ; window++) {
			if (p == NULL)
				p = lowmem_page_address(open_data->data_buffer->pages[index].P);
			if (p == NULL) {
				DEB_ERR("[%s] ERROR: page not mapable\n", __func__);
				return -EFAULT;
			}

			open_data->b3_data[b3].InUse         = _entry_in_use_;
			open_data->b3_data[b3].Kernel_Buffer = p;
			open_data->b3_data[b3].User_Buffer   = u;
			DEB_INFO("[%s] ncci %u window %u buffer %u page %u  kernel 0x%p user 0x%p\n",
				 __func__, ncci, window, b3, index, p, u);

			/* prüfen ob noch ein den Buffer in den block passt */
			if ((((unsigned long)p & ((1 << PAGE_SHIFT) - 1)) + B3BlockSize) < (1 << PAGE_SHIFT)) {
				DEB_INFO("same page\n");
				p += B3BlockSize;
				u += B3BlockSize;
			} else {
				DEB_INFO("next page\n");
				index++, p = NULL;
				if (index >= (int)open_data->data_buffer->size) {
					DEB_ERR("[%s] ERROR: %u b3 blocks %u pages avail (too little)\n", __func__,
						b3, open_data->data_buffer->size);
					return -EFAULT;
				}
				u += (1 << PAGE_SHIFT) - 1;
				u = (unsigned char *)((unsigned long)u & ~((1UL << PAGE_SHIFT) - 1));  /* align auf naechsten 4k Block */
			}
			b3++;
		}
	}
	DEB_INFO("[%s] %u b3 blocks %u pages used %u pages avail\n",
		 __func__,
		 b3, index, open_data->data_buffer->size);

	return LOCAL_CAPI_REGISTER_B3_BUFFER(SOURCE_PTR_CAPI, open_data->ApplId,
					     open_data->b3_data, b3,
					     capi_oslib_release_b3_buffer, open_data);
}

/**
 */
int capi_oslib_release_user_space_blocks(struct _capi_oslib_open_data *open_data)
{
	CA_FREE(open_data->data_buffer);
	open_data->data_buffer = NULL;
	return 0;
}

/**
 */
static void *capi_oslib_map_kernel_space_block(struct _capi_oslib_open_data *open_data, void *kernel_ptr)
{
	unsigned int index;

	for (index = 0 ; index < open_data->data_buffer->size ; index++) {
		if (open_data->b3_data[index].Kernel_Buffer == kernel_ptr)
			return open_data->b3_data[index].User_Buffer;
	}
	return NULL;
}

/**
 */
#if defined(NEED_PAGE_LOCK)
static void capi_oslib_lock_memory(void *kernel_ptr, unsigned int DataLen)
{
	unsigned long pfn   = (unsigned int)kernel_ptr >> PAGE_SHIFT;
	struct page *page   = pfn_to_page(pfn);
	unsigned int offset = (unsigned int)kernel_ptr & ((1 << PAGE_SHIFT) - 1);

	get_page(page);
	/*--- DEB_INFO("lock(0x%p)\n", page); ---*/
	if (offset + DataLen > (1 << PAGE_SHIFT)) {
		page++;
		get_page(page);
		/*--- DEB_INFO("lock(0x%p)\n", page); ---*/
	}
}
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/

/**
 */
#if defined(NEED_PAGE_LOCK)
static void capi_oslib_unlock_memory(void *kernel_ptr, unsigned int DataLen)
{
	unsigned long pfn   = (unsigned int)kernel_ptr >> PAGE_SHIFT;
	struct page *page   = pfn_to_page(pfn);
	unsigned int offset = (unsigned int)kernel_ptr & ((1 << PAGE_SHIFT) - 1);

	put_page(page);
	/*--- DEB_INFO("unlock(0x%p)\n", page); ---*/
	if (offset + DataLen > (1 << PAGE_SHIFT)) {
		page++;
		put_page(page);
		/*--- DEB_INFO("unlock(0x%p)\n", page); ---*/
	}
}
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/

/**
 */
#if defined(NEED_PAGE_LOCK)
static void capi_oslib_register_handle(struct _capi_oslib_open_data *open_data,
				       unsigned int dir, unsigned int handle,
				       void *user, void *kernel, unsigned int len)
{
	int i;

	for (i = 0 ; i < 8 ; i++) {
		if (open_data->data_buffer[dir][i].InUse == _entry_not_used_) {
			open_data->data_buffer[dir][i].InUse = _entry_in_use_;
			open_data->data_buffer[dir][i].handle = handle;
			open_data->data_buffer[dir][i].user   = user;
			open_data->data_buffer[dir][i].kernel = kernel;
			open_data->data_buffer[dir][i].len    = len;
			return;
		}
	}
	DEB_ERR("too manny handles\n");
}
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/

/**
 */
#if defined(NEED_PAGE_LOCK)
static unsigned int capi_oslib_release_handle(struct _capi_oslib_open_data *open_data, unsigned int dir,
					      unsigned int handle, void **user, void **kernel)
{
	int i, len;

	for (i = 0 ; i < 8 ; i++) {
		if ((open_data->data_buffer[dir][i].InUse == _entry_in_use_) && (open_data->data_buffer[dir][i].handle == handle)) {
			*user   = open_data->data_buffer[dir][i].user;
			*kernel = open_data->data_buffer[dir][i].kernel;
			len     = open_data->data_buffer[dir][i].len;
			open_data->data_buffer[dir][i].InUse = _entry_not_used_;
			return len;
		}
	}
	DEB_ERR("[%s]handle not found\n", __func__);
}
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/

/**
 */
void capi_oslib_map_addr(struct _capi_oslib_open_data *open_data, void *msg)
{
	struct __attribute__ ((packed))_capi_message *C = (struct __attribute__ ((packed))_capi_message *)msg;
	void *kernel_ptr, *user_ptr;

	switch (C->capi_message_header.SubCommand) {
	/*
	 * map from USER to KERNEL
	 */
	case CAPI_REQ:
		switch (C->capi_message_header.Command) {
		case CAPI_DATA_B3: /* DATA_B3_REQ */
			user_ptr = get_data_b3_pointer(C->capi_message_part.data_b3_req);
			kernel_ptr = capi_oslib_map_data_b3_req(user_ptr, copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen), _do_nothing_);
			/*--- DEB_INFO("[b3_req]: hdl=%u msg=0x%p data=0x%p\n", copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.Handle), C, user_ptr); ---*/
			DEB_INFO("[%u] DATA_B3_REQ: NCCI 0x%x Data 0x%p/0x%p Len %u Handle 0x%x\n",
				 copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.ApplId),
				 copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.NCCI),
				 user_ptr, kernel_ptr,
				 copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen),
				 copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.Handle));
				 set_data_b3_pointer(C->capi_message_part.data_b3_req, kernel_ptr);
#if defined(NEED_PAGE_LOCK)
			capi_oslib_lock_memory(kernel_ptr, copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen));
			capi_oslib_register_handle(open_data, USER_TO_KERNEL,
						   copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.Handle),
						   user_ptr, kernel_ptr,
						   copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen));
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/
			break;
		default:
			break;
		}
		break;
	/**
	 * map from USER to KERNEL
	 */
	case CAPI_RESP:
		switch (C->capi_message_header.Command) {
		case CAPI_DATA_B3: /* DATA_B3_RESP */
			DEB_INFO("[%u] DATA_B3_RESP: NCCI 0x%x Handle 0x%x\n",
					copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.ApplId),
					copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_resp.NCCI),
					copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_resp.Handle));
#if defined(NEED_PAGE_LOCK)
			DataLen = capi_oslib_release_handle(open_data, KERNEL_TO_USER,
							    copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_resp.Handle),
							    &user_ptr, &kernel_ptr);
			capi_oslib_unlock_memory(kernel_ptr, DataLen);
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/
			break;
		default:
			break;
		}
		break;

	/**
	 * map from KERNEL to USER
	 */
	case CAPI_IND:
		switch (C->capi_message_header.Command) {
		case CAPI_DATA_B3: /* DATA_B3_IND */
			kernel_ptr = get_data_b3_pointer(C->capi_message_part.data_b3_ind);
			user_ptr   = capi_oslib_map_kernel_space_block(open_data, kernel_ptr);
			DEB_INFO("[%u] DATA_B3_IND: NCCI 0x%x Data 0x%p/0x%p Len %u Handle 0x%x\n",
					copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.ApplId),
					copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.NCCI),
					kernel_ptr, user_ptr,
					copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen),
					copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.Handle));
			if (user_ptr) {
				set_data_b3_pointer(C->capi_message_part.data_b3_ind, user_ptr);
#if defined(NEED_PAGE_LOCK)
				capi_oslib_lock_memory(kernel_ptr, copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen));
				capi_oslib_register_handle(open_data, KERNEL_TO_USER,
							   copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.Handle),
							   user_ptr, kernel_ptr,
							   copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen));
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/
			}
			break;
		default:
			break;
		}
		break;

	/**
	 * map from KERNEL to USER
	 */
	case CAPI_CONF:
		switch (C->capi_message_header.Command) {
		case CAPI_DATA_B3: /* DATA_B3_CONF */
			DEB_INFO("[%u] DATA_B3_CONF: NCCI 0x%x Handle 0x%x\n",
				 copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.ApplId),
				 copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_conf.NCCI),
				 copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_conf.Handle));
#if defined(NEED_PAGE_LOCK)
			DataLen = capi_oslib_release_handle(open_data, USER_TO_KERNEL,
							    copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_conf.Handle),
							    &user_ptr, &kernel_ptr);
			capi_oslib_unlock_memmory(kernel_ptr, DataLen);
#endif /*--- #if defined(NEED_PAGE_LOCK) ---*/
			break;
		default:
			break;
		}
		break;
	}
}

/**
 */
unsigned int capi_oslib_get_data_b3_ind_buffer_size(unsigned int MaxNCCIs, unsigned int B3BlockSize,
						    unsigned int WindowSize)
{
	unsigned int Size = 0;
	unsigned int ncci, block;

	for (ncci = 0; ncci < MaxNCCIs ; ncci++) {
		for (block = 0 ; block < WindowSize ; block++) {
			if ((Size >> PAGE_SHIFT) != ((Size + B3BlockSize) >> PAGE_SHIFT)) {
				Size +=   (1 << PAGE_SHIFT) - 1;
				Size &= ~((1 << PAGE_SHIFT) - 1);
			}
			Size += B3BlockSize;
		}
	}
	return Size;
}
