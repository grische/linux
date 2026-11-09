/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
#ifndef _tffs_local_h_
#define _tffs_local_h_

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/zlib.h>
#include <avm/tffs/tffs.h>
#include <linux/version.h>
#include <linux/completion.h>
#include <linux/wait.h>

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#define MODULE_NAME   "tffs"

#ifdef CONFIG_TFFS_AUTO_INIT
#  define __tffs_init __init
#else
#  define __tffs_init
#endif

#if defined(CONFIG_X86_PUMA7)
#  define TFFS_X86_PUMA7
#endif
#if defined(CONFIG_ARCH_AVALANCHE) && defined(CONFIG_MACH_PUMA7)
#  define TFFS_ARM_PUMA7
#endif
#if defined(CONFIG_X86) && defined(CONFIG_ARCH_GEN3)
#  define TFFS_X86_PUMA6
#endif
#if defined(CONFIG_ARM) && defined(CONFIG_MACH_PUMA6)
#  define TFFS_ARM_PUMA6
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define TFFS_EVENT_BIT_TRIGGER (BITS_PER_LONG - 1)
#define TFFS_EVENT_BIT_CLEANUP 0
#define TFFS_EVENT_BIT_PANIC   1
#define TFFS_EVENT_TRIGGER     (1UL << TFFS_EVENT_BIT_TRIGGER)
#define TFFS_EVENT_CLEANUP     (1UL << TFFS_EVENT_BIT_CLEANUP)
#define TFFS_EVENT_PANIC       (1UL << TFFS_EVENT_BIT_PANIC)

enum tffs3_types {
	tffs3_type_legacy,
	tffs3_type_mtdnor,
	tffs3_type_bdev,
	tffs3_type_mtdnand,
	tffs3_type_cache,
	tffs3_type_remote,
	tffs3_type_last,
};

enum tffs3_handle_mode {
	tffs3_mode_read,
	tffs3_mode_write,
	tffs3_mode_panic,
};

struct tffs_core_handle {
	void *core_priv;
	unsigned int id;
	enum tffs3_handle_mode mode;
	size_t max_segment_size;
};

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
	struct class                *osclass;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern void tffs_send_event(unsigned long event);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_urlader_setenv_by_id(int id, const char *val);
int avm_urlader_unsetenv_by_id(unsigned int id);
int avm_urlader_getenv_by_id(unsigned int id, char *buf, size_t size);
const char *avm_urlader_getenv_name(unsigned int id);
void *avm_urlader_build_name_table(size_t *size);

int avm_urlader_setenv_begin(void);
void avm_urlader_setenv_end(void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern struct tffs_core_handle *TFFS3_Open(enum _tffs_id, enum tffs3_handle_mode);
extern int TFFS3_Close(struct tffs_core_handle *);
extern int TFFS3_Read(struct tffs_core_handle *, unsigned char *, size_t *);
extern int TFFS3_Write(struct tffs_core_handle *, const uint8_t *, size_t, unsigned int);

/* Simple helpers */
int tffs_write_entry(int tffs_id, const void *buf, size_t len);
int tffs_clear_entry(int tffs_id);
ssize_t tffs_read_entry(int tffs_id, void *buf, size_t len);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern const struct file_operations tffs_fops;

struct tffs_fops_handle;
ssize_t tffs_write_kern(struct tffs_fops_handle *handle, const char *write_buffer,
			size_t write_length, loff_t *offp);
ssize_t tffs_read_kern(struct tffs_fops_handle *handle, char *read_buffer, size_t max_read_length,
		       loff_t *offp);
struct tffs_fops_handle *tffs_open_kern(unsigned int id, unsigned int wr_mode);
struct tffs_fops_handle *tffs_open_panic(void);
int tffs_release_kern(struct tffs_fops_handle *handle);

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
void tffs_write_statistic(unsigned int id, unsigned int len, unsigned int mode,
			  unsigned int cached);
#else
#define tffs_write_statistic(x, y, z)
#endif /*--- #ifdef CONFIG_PROC_FS ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_urlader_env_init(void);
#ifdef CONFIG_SYSCTL
int avm_urlader_sysctl_init(void);
#else
static inline int avm_urlader_sysctl_init(void) { }
#endif


#include <linux/mtd/mtd.h>


enum tffs3_notify_event {
	tffs3_notify_clear,
	tffs3_notify_update,
	tffs3_notify_reinit
};

typedef void (*tffs3_notify_fn)(void *priv, unsigned int id, enum tffs3_notify_event event);

extern struct blocking_notifier_head tffs_state_notifier;

struct tffs_module {
	void *priv;
	char *name;
	int (*setup)(struct tffs_module *this);
	int (*read)(struct tffs_module *this, void *handle, uint8_t *read_buffer,
		    size_t *read_length);
	int (*write)(struct tffs_module *this, void *handle, const uint8_t *write_buffer,
		     size_t write_length, size_t *retlen, unsigned int level);
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

extern int tffs_init(void);
extern int TFFS3_Init(void);
extern void TFFS3_Deinit(void);
extern int TFFS3_Werkseinstellungen(struct tffs_core_handle *);
extern int TFFS3_Clear(struct tffs_core_handle *);
extern int TFFS3_Cleanup(struct tffs_core_handle *);
extern int TFFS3_Create_Index(void);
extern int TFFS3_Info(struct tffs_core_handle *, unsigned int *);
extern void TFFS3_Panic_Lock(void);
extern void tffs_send_event(unsigned long event);

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

#if defined(CONFIG_TFFS_DEV_CLIENT)
extern int TFFS3_REMOTE_Configure(struct tffs_module *this, unsigned int node_id);
#endif
#if defined(CONFIG_TFFS_DEV_SERVER)
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

#if defined(CONFIG_TFFS_DEV_SERVER) || defined(CONFIG_TFFS_DEV_CLIENT) /* Puma only */
#  define AVM_EVENT_TFFS_NODE_PRIMARY   AVM_EVENT_TFFS_NODE_ATOM
#  define AVM_EVENT_TFFS_NODE_SECONDARY AVM_EVENT_TFFS_NODE_ARM
#  if defined(TFFS_X86_PUMA7) || defined(TFFS_X86_PUMA6)
#    define system_is_primary_FritzOS 1
#  elif defined(TFFS_ARM_PUMA7) || defined(TFFS_ARM_PUMA6)
#    define system_is_primary_FritzOS 0
#  else
#    error Unknown target architecture
#  endif
#else
#  define system_is_primary_FritzOS 1
#endif

#if system_is_primary_FritzOS
#  define _FLASH_FS_ID_PANIC_LOG FLASH_FS_ID_PANIC_LOG
#  define _FLASH_FS_ID_CRASH_LOG FLASH_FS_ID_CRASH_LOG
#  define _FLASH_FS_ID_USER_LOG FLASH_FS_ID_USER_LOG
#else
#  define _FLASH_FS_ID_PANIC_LOG FLASH_FS_ID_PANIC2_LOG
#  define _FLASH_FS_ID_CRASH_LOG FLASH_FS_ID_CRASH2_LOG
#  define _FLASH_FS_ID_USER_LOG FLASH_FS_ID_USER2_LOG
#endif

static inline void smp_mb__before_clear_bit(void)
{
	smp_mb__before_atomic();
}

static inline void smp_mb__after_clear_bit(void)
{
	smp_mb__after_atomic();
}

#endif /*--- #ifndef _local.h_ ---*/
