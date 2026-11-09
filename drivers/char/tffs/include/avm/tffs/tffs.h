/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
#ifndef _tffs_h_
#define _tffs_h_

#include <uapi/avm/tffs/tffs.h>
#include <uapi/avm/tffs/ondisk.h>
#include <linux/semaphore.h>

#if defined(CONFIG_TFFS)
extern unsigned int tffs_panic_log_suppress;

extern int avm_urlader_getenv(const char *var, char *value, size_t size);
extern int avm_urlader_setenv(const char *var, const char *val);
extern int avm_urlader_unsetenv(const char *var);
extern const struct _TFFS_Name_Table TFFS_Name_Table[URLADER_ENV_NAMETABLE_SIZE];
static inline const struct _TFFS_Name_Table *avm_urlader_get_nametable(void)
{
    return TFFS_Name_Table;
}

extern struct blocking_notifier_head tffs_state_notifier;

enum tffs3_module_state {
	tffs3_module_init,
	tffs3_module_configured,
	tffs3_module_running,
	tffs3_module_error,
};

struct mtd_info;
typedef struct mtd_info *(*panic_setup_cb)(struct mtd_info *mtd);

extern int TFFS3_Register_NAND(struct mtd_info *mtd);
extern int TFFS3_Register_REMOTE(unsigned int node_id);
extern int TFFS3_Register_SERVER(unsigned int node_id);
extern int TFFS3_Register_Panic_CB(struct mtd_info *mtd, panic_setup_cb panic_setup_fn);
#endif /*--- #if defined(CONFIG_TFFS) ---*/

#ifdef CONFIG_AVM_PLATFORM_RESET_STATUS
void tffs_write_pltfm_paniclog(const char *message);
#else
static inline void tffs_write_pltfm_paniclog(const char *message) { }
#endif

#if defined(CONFIG_TFFS_DEV_LEGACY)
extern int tffs_mtd[2];
#endif

#if defined(CONFIG_TFFS_REMOTE_CRASHLOG_WRITER) || \
    defined(CONFIG_TFFS_REMOTE_CRASHLOG_READER)
extern struct semaphore tffs_log_sema;
#endif

#if defined(CONFIG_TFFS_REMOTE_CRASHLOG_WRITER)
/* Provided by platform kernel (currently puma7 only) */
int avm_remote_crashlog_is_read_forbidden(void);
#else
static inline int avm_remote_crashlog_is_read_forbidden(void) { return 0; }
#endif

#if defined(CONFIG_TFFS_REMOTE_CRASHLOG_READER)
/* Provided by platform kernel (currently puma7 only) */
int avm_remote_crashlog_sem_down(void);
int avm_remote_crashlog_sem_up(void);
#else
static inline int avm_remote_crashlog_sem_down(void) { return 0; }
static inline int avm_remote_crashlog_sem_up(void) { return 0; }
#endif

#endif /*--- #ifndef _tffs_h_ ---*/
