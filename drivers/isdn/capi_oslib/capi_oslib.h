/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _capi_oslib_h_
#define _capi_oslib_h_

#include "consts.h"
#include "capi_pipe.h"
#include "appl.h"

/**
 */
struct _adr_map {
	unsigned int size;  /* 4K Blocks */
	unsigned int len;   /* bytes of memory area */
	void *user;         /* user space base adr */
	struct _adr_map_pages {
		enum _entry_use InUse;
		struct page *P;
		void *user;
		void *kernel;
		unsigned int len;
	} pages[1];
};

/**
 */
struct _capi_oslib_open_data {
	struct _capi_oslib_open_data *next;
	struct _capi_oslib_open_data *prev;
	wait_queue_head_t wait_queue;
	struct fown_struct *pf_owner;
	struct fasync_struct *fasync;
	unsigned char put_message_buffer[MAX_CAPI_MESSAGE_SIZE];
	unsigned char get_message_buffer[MAX_CAPI_MESSAGE_SIZE + (sizeof(struct __attribute__ ((packed)) _capi_message)) + 8];
	struct _adr_map *data_buffer;
	struct _adr_b3_ind_data *b3_data; /* max window size * max nccis */

	unsigned int ApplId;
	unsigned int last_error;
	struct capi_pipe *read_pipe;
	enum _capi_source mode;
	/**
	 */
	unsigned int B3BlockSize;
	unsigned int AllocB3BlockSize;
	unsigned int B3WindowSize;
	unsigned int MaxNCCIs;
	unsigned int MessageBufferSize;
};

/**
 */
struct _capi_oslib {
	dev_t         device;
	struct cdev  *cdev;
	struct class *osclass;
	unsigned int  activated;
};

/**
 */
void capi_oslib_scheduler_work_init(void (*scheduler_work_func)(struct work_struct *work));
void capi_oslib_scheduler_work_deinit(void);
int capi_oslib_install_card(struct _stack_init_params *card);
void capi_oslib_remove_card(struct _stack_init_params *card);

#if 0
unsigned long EnterCritical(void);
void LeaveCritical(unsigned long flags);
#else
void EnterCritical(void);
void LeaveCritical(void);
#endif/*--- #else ---*/

/**
 */
int capi_oslib_register_user_space_blocks(struct _capi_oslib_open_data *open_data, void *user, unsigned int len, unsigned int MaxNCCIs, unsigned int WindowSize, unsigned int B3BlockSize);
void capi_oslib_map_addr(struct _capi_oslib_open_data *open_data, void *msg);
unsigned int capi_oslib_get_data_b3_ind_buffer_size(unsigned int MaxNCCIs, unsigned int B3BlockSize, unsigned int WindowSize);

/**
 */
void os_enable_scheduler(void);
void os_disable_scheduler(void);


#endif /*--- #ifndef _capi_oslib_h_ ---*/
