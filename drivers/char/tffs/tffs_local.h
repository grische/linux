/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de>
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
#ifndef _tffs_local_h_
#define _tffs_local_h_

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/zlib.h>
#include <avm/tffs/tffs.h>
#include <linux/version.h>
#include <linux/completion.h>

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
//#define TFFS_DEBUG

#if defined(TFFS_DEBUG)
#define DBG(a)       printk a
#else /*--- #if defined(TFFS_DEBUG) ---*/
#define DBG(a)  
#endif /*--- #else ---*/ /*--- #if defined(TFFS_DEBUG) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#define MODULE_NAME             "tffs"
#define MAX_ENV_ENTRY           256
#if defined(FLASH_ENV_ENTRY_SIZE)
    #undef FLASH_ENV_ENTRY_SIZE
#endif /*--- #if defined(FLASH_ENV_ENTRY_SIZE) ---*/
#define FLASH_ENV_ENTRY_SIZE    256

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _tffs3_thread_state {
    tffs3_thread_state_off,
    tffs3_thread_state_init,
    tffs3_thread_state_idle,
    tffs3_thread_state_process,
    tffs3_thread_state_down
};

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
struct tffs_cdev {
    int                          major;
    int                          minor;
    int                          count;
    dev_t                        device;
    struct cdev                 *cdev;
    struct cdev                 *cdev_ticfg;
    wait_queue_head_t            event_wq;
    volatile unsigned long       pending_events;
    volatile struct task_struct *kthread;
    enum _tffs3_thread_state     thread_state;
    unsigned int                 request_count;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern int tffs_thread_event;
extern enum _tffs3_thread_state tffs_thread_state;
extern unsigned int tffs_request_count;
extern int tffs_mtd[2];

#define PANIC_LOG_WRKSPC_SIZE 268000
extern char panic_log_workspace[PANIC_LOG_WRKSPC_SIZE];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct tffs_fops_handle {
    struct tffs_core_handle *core_handle;
    unsigned int init_flag;
    unsigned char *z_Buffer;
    unsigned int z_length;
    z_stream stream;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_value(char *var);
int avm_urlader_env_unset_variable(char *var);
int avm_urlader_env_set_variable(char *var, char *val);
int avm_urlader_env_defrag(void);
char *avm_urlader_env_get_value_by_id(unsigned int id);
char *avm_urlader_env_get_variable(int idx);
char *avm_urlader_env_get_id_name(unsigned int id);
size_t avm_urlader_build_name_table(unsigned char *buffer, size_t max_len);
extern const struct _TFFS_Name_Table * avm_urlader_get_nametable(void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int tffs_ioctl(struct inode *inode, struct file *filp, unsigned int ioctl_param, unsigned long argv);
long tffs_unlocked_ioctl(struct file *filp, unsigned int ioctl_param, unsigned long argv);
ssize_t tffs_write_kern(struct tffs_fops_handle *handle, const char *write_buffer, size_t write_length, loff_t *offp);
ssize_t tffs_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *offp);
ssize_t tffs_read_kern(struct tffs_fops_handle *handle, char *read_buffer, size_t max_read_length, loff_t *offp);
ssize_t tffs_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *offp);
struct tffs_fops_handle *tffs_open_kern(unsigned int id, unsigned int wr_mode);
struct tffs_fops_handle *tffs_open_panic(void);
int tffs_open(struct inode *inode, struct file *filp);
int tffs_flush(struct file *filp, fl_owner_t id);
int tffs_release_kern(struct tffs_fops_handle *handle);
int tffs_release(struct inode *inode, struct file *filp);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_PROC_FS
extern void tffs_proc_init(struct tffs_cdev *tffs);
extern void tffs_proc_remove(struct tffs_cdev *tffs);

/*--------------------------------------------------------------------------------*\
 * collect statistic value
 * id:  TFFS-ID
 * len: written/read len
 * mode: 0: read 1: write
 * cached: 0: uncached 1: cached
\*--------------------------------------------------------------------------------*/
void tffs_write_statistic(unsigned int id, unsigned int len, unsigned int mode, unsigned int cached);
#else
#define tffs_write_statistic(x, y, z)
#endif /*--- #ifdef CONFIG_PROC_FS ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_SYSCTL
int avm_urlader_env_init(void);
#endif /* CONFIG_SYSCTL */

#include <linux/mtd/mtd.h>


#define TFFS3_HDR_MAGIC     0x41564d5f54464653ULL // AVM_TFFS
#define TFFS3_MAX_BADPAGES  0x4U

enum tffs3_notify_event {
    tffs3_notify_clear,
    tffs3_notify_update,
    tffs3_notify_reinit
};

enum tffs3_module_state {
    tffs3_module_init,
    tffs3_module_configured,
    tffs3_module_running,
    tffs3_module_error,
};

typedef struct _tffs_device tffs_device;
typedef void (*tffs3_notify_fn)(void *priv, unsigned int id, enum tffs3_notify_event event);

struct tffs_module {
    void *priv;
    char *name;
    int (*setup)(struct tffs_module *this);
    int (*read)(struct tffs_module *this, void *handle, uint8_t *read_buffer, size_t *read_length);
    int (*write)(struct tffs_module *this, void *handle, uint8_t *write_buffer, size_t write_length, size_t *retlen, unsigned int level);
    void *(*open)(struct tffs_module *this, struct tffs_core_handle *core_handle);
    int (*close)(struct tffs_module *this, void *handle);
    int (*cleanup)(struct tffs_module *this, void *handle);
    int (*reindex)(struct tffs_module *this);
    int (*info)(struct tffs_module *this, unsigned int *Fill);
    int (*register_notify)(struct tffs_module *this, void *priv, tffs3_notify_fn notify_cb);
    int (*remove_notify)(struct tffs_module *this, void *priv, tffs3_notify_fn notify_cb);
};

struct tffs_server {
    // server interface
    void *priv;
    int (*setup)(struct tffs_server *this);
    tffs3_notify_fn notify;
};

struct _tffs_device {
    unsigned int initialised;
    struct semaphore outer_lock;
    struct semaphore inner_lock;
    struct completion backend_ready;
    volatile unsigned long panic_mode;
    struct tffs_core_handle panic_handle;

    enum tffs3_module_state backend_state;
    struct tffs_module backend;

    enum tffs3_module_state cache_state;
    struct tffs_module cache;

    enum tffs3_module_state server_state;
    struct tffs_server server;

    struct list_head panic_cb_list;
};


struct tffs3_cfg_funcs {
    int (*legacy)(struct tffs_module *this, int mtd_num0, int mtd_num1);
    int (*mtdnor)(struct tffs_module *this, unsigned int mtd_number);
    int (*bdev)(struct tffs_module *this, char *path);
    int (*mtdnand)(struct tffs_module *this, struct mtd_info *mtd);
    int (*cache)(struct tffs_module *this, struct tffs_module *backend);
    int (*remote)(struct tffs_module *this, unsigned int node_id);
    int (*server)(struct tffs_server *this, unsigned int node_id);
    int (*efi)(struct tffs_module *this);
    int (*efi_sync)(struct tffs_module *backend);
};


struct _TFFS_Block_Hdr_NAND {
    __be32 blkseq_nr;
    __be32 erase_cnt;
    __be32 sect_per_pg;
    __be32 num_bad_pages;
    __be64 bad_pages[TFFS3_MAX_BADPAGES];
} __attribute__((packed));

struct _TFFS_Block_Hdr {
    __be64 magic;
    __be32 version;
    __be32 type;
    union {
        struct _TFFS_Block_Hdr_NAND mtdnand;
    };
} __attribute__((packed));

extern int TFFS3_Init(void);
extern void TFFS3_Wait_For_Backend(void);
extern void TFFS3_Backend_Ready(void);
extern void TFFS3_Deinit(void);
extern int TFFS3_Werkseinstellungen(struct tffs_core_handle *);
extern int TFFS3_Clear(struct tffs_core_handle *);
extern int TFFS3_Cleanup(struct tffs_core_handle *);
extern int TFFS3_Create_Index(void);
extern int TFFS3_Info(struct tffs_core_handle *, unsigned int *);
extern void TFFS3_Panic_Lock(void);
extern void tffs_send_event(unsigned int event);

#if defined(CONFIG_TFFS_DEV_MTDNOR)
extern int TFFS3_NOR_Configure(struct tffs_module *this, unsigned int mtd_num);
#endif

#if defined(CONFIG_TFFS_DEV_MTDNAND)
extern int TFFS3_NAND_Configure(struct tffs_module *this, struct mtd_info *mtd);
#endif

#if defined(CONFIG_TFFS_DEV_BDEV)
extern int TFFS3_BDEV_Configure(struct tffs_module *this, char *path);
#endif

#if defined(CONFIG_TFFS_DEV_CACHE)
extern int TFFS3_CACHE_Configure(struct tffs_module *this, struct tffs_module *backend);
#endif

#if defined(CONFIG_TFFS_DEV_REMOTE)
extern int TFFS3_REMOTE_Configure(struct tffs_module *this, unsigned int node_id);
extern int TFFS3_SERVER_Configure(struct tffs_server *this, unsigned int node_id);
#endif

#if defined(CONFIG_TFFS_DEV_LEGACY)
extern int TFFS3_LGCY_Configure(struct tffs_module *this, int mtd_num1, int mtd_num2);
#endif

#if defined(CONFIG_TFFS_DEV_EFI)
extern int TFFS3_EFI_Configure(struct tffs_module *this);
#endif

#if defined(CONFIG_TFFS_EFI_SYNC)
extern int TFFS3_EFI_SYNC_Configure(struct tffs_module *backend);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
#define mtd_read(mtd, from, len, retlen, buf) (mtd)->read((mtd), (from), (len), (retlen), (buf))
#define mtd_write(mtd, to, len, retlen, buf)  (mtd)->write((mtd), (to), (len), (retlen), (buf))
#define mtd_read_oob(mtd, from, ops)          (mtd)->read_oob((mtd), (from), (ops))
#define mtd_write_oob(mtd, to, ops)           (mtd)->write_oob((mtd), (to), (ops))
#define mtd_block_isbad(mtd, addr)            (mtd)->block_isbad((mtd), (addr))
#define mtd_block_markbad(mtd, addr)          (mtd)->block_markbad((mtd), (addr))
#define mtd_erase(mtd, erase)                 (mtd)->erase((mtd), (erase))

static inline void mtd_sync(struct mtd_info *mtd)
{
    if (mtd->sync)
        mtd->sync(mtd);
}

#endif // LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
static inline void smp_mb__before_clear_bit(void)
{
	smp_mb__before_atomic();
}

static inline void smp_mb__after_clear_bit(void)
{
	smp_mb__after_atomic();
}
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)

#endif /*--- #ifndef _tffs_local_h_ ---*/
