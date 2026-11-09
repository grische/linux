/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de>
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
#ifndef _tffs_h_
#define _tffs_h_

#include <uapi/avm/tffs/tffs.h>

#if defined(CONFIG_X86_PUMA7)
#  define TFFS_X86_PUMA7
#endif
#if defined(CONFIG_ARCH_AVALANCHE) && defined(CONFIG_MACH_PUMA7)
#  define TFFS_ARM_PUMA7
#endif
#if defined(CONFIG_X86) && defined(CONFIG_MACH_PUMA6)
#  define TFFS_X86_PUMA6
#endif
#if defined(CONFIG_ARM) && defined(CONFIG_MACH_PUMA6)
#  define TFFS_ARM_PUMA6
#endif

/*-----------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------*/
#if defined(__KERNEL__) || defined(URLADER) || defined(FLASH_TFFS_IMAGE)
struct _TFFS_Entry {
    unsigned short ID;
    unsigned short Length;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union _tffs_segment_entry {
    struct _TFFS_Entry Entry;
    unsigned char Buffer[sizeof(struct _TFFS_Entry) + sizeof(unsigned int)];
};

#define TFFS_GET_SEGMENT_VALUE(E)  ~( \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 3]) <<  0) | \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 2]) <<  8) | \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 1]) << 16) | \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 0]) << 24)   \
                                    )

#define TFFS_SET_SEGMENT_VALUE(E, value)   ( \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 3] = ((~(value) >>  0) & 0xFF), \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 2] = ((~(value) >>  8) & 0xFF), \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 1] = ((~(value) >> 16) & 0xFF), \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 0] = ((~(value) >> 24) & 0xFF)  \
                                    )

#endif /*--- #if defined(__KERNEL__) || defined(URLADER) ---*/



/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#if defined(__KERNEL__)

#if !defined(URLADER)
#include <linux/types.h>
#endif

#if defined(CONFIG_TFFS)

struct mtd_info;
extern unsigned int TFFS_Init(unsigned int, unsigned int);
extern void TFFS_Deinit(void);
extern void *TFFS_Open(void);
extern void TFFS_Close(void *);
extern unsigned int TFFS_Werkseinstellungen(void *);
extern unsigned int TFFS_Minor(void *handle);
extern unsigned int TFFS_Clear(void *, enum _tffs_id);
extern unsigned int TFFS_Write(void *, enum _tffs_id, unsigned char *, unsigned int, unsigned int);
extern unsigned int TFFS_Read(void *, enum _tffs_id, unsigned char *, unsigned int *);
extern unsigned int TFFS_Create_Index(void);
extern unsigned int TFFS_Cleanup(void *);
extern unsigned int TFFS_Info(void *, unsigned int *);
extern unsigned char *TFFS_Read_Buffer(void *, unsigned int **);
extern unsigned char *TFFS_Write_Buffer(void *, unsigned int **);
extern unsigned int tffs_panic_log_suppress;
void tffs_panic_log_register_spi(void);
extern void tffs_send_event(unsigned int event);


struct mtd_info;
struct mtd_oob_ops;

enum tffs3_handle_mode {
    tffs3_mode_read,
    tffs3_mode_write,
    tffs3_mode_panic,
};

enum tffs3_types {
    tffs3_type_legacy,
    tffs3_type_mtdnor,
    tffs3_type_bdev,
    tffs3_type_mtdnand,
    tffs3_type_cache,
    tffs3_type_remote,
    tffs3_type_last,
};

struct tffs_core_handle {
    void *core_priv;
    unsigned int id;
    enum tffs3_handle_mode mode;
    size_t max_segment_size;
};

typedef struct mtd_info *(*panic_setup_cb)(struct mtd_info *mtd);

extern void TFFS3_Backend_Ready(void);
extern int TFFS3_Register_NAND(struct mtd_info *mtd);
extern int TFFS3_Register_REMOTE(unsigned int node_id);
extern int TFFS3_Register_SERVER(unsigned int node_id);
extern int TFFS3_Register_Panic_CB(struct mtd_info *mtd, panic_setup_cb panic_setup_fn);
extern struct tffs_core_handle *TFFS3_Open(enum _tffs_id, enum tffs3_handle_mode);
extern int TFFS3_Close(struct tffs_core_handle *);
extern int TFFS3_Read(struct tffs_core_handle *, unsigned char *, unsigned int *);
extern int TFFS3_Write(struct tffs_core_handle *, unsigned char *, unsigned int, unsigned int);
#endif /*--- #if defined(CONFIG_TFFS) ---*/

#if defined(CONFIG_TFFS) && defined(CONFIG_TFFS_PANIC_LOG)
void TFFS3_panic_dummy_log(const char *txt);
#else
static inline void TFFS3_panic_dummy_log(const char *txt __maybe_unused) {}
#endif

#define TFFS_EVENT_BIT_TRIGGER  (BITS_PER_LONG - 1)
#define TFFS_EVENT_BIT_CLEANUP  0
#define TFFS_EVENT_BIT_PANIC    1
#define TFFS_EVENT_TRIGGER      (1 << TFFS_EVENT_BIT_TRIGGER)
#define TFFS_EVENT_CLEANUP      (1 << TFFS_EVENT_BIT_CLEANUP)
#define TFFS_EVENT_PANIC        (1 << TFFS_EVENT_BIT_PANIC)

#define TFFS_VERSION(major, minor) ((((major) & 0xffff) << 16) + ((minor) & 0xffff))
#define TFFS3_VERSION           TFFS_VERSION(3, 1)
#define TFFS3_TYPE_MTDNOR       0x1
#define TFFS3_TYPE_MTDNAND      0x2
#define TFFS3_TYPE_BDEV         0x3
#define TFFS3_TYPE_CACHE        0x4
#define TFFS3_TYPE_REMOTE       0x5
#define TFFS3_TYPE_LEGACY       0x6

#endif /*--- #if defined(__KERNEL__) ---*/

#endif /*--- #ifndef _tffs_h_ ---*/
