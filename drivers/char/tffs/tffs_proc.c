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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <asm/mach_avm.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/semaphore.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include <linux/zlib.h>
#include <avm/tffs/tffs.h>
#include <linux/env.h>
#include "tffs_local.h"

#ifdef CONFIG_SYSCTL

#if !defined(CONFIG_TFFS_ENV)
#error "no CONFIG_TFFS_ENV defined"
#endif /*--- #if !defined(CONFIG_TFFS_ENV) ---*/

#define DEV_ADAM2	    6
#define DEV_ADAM2_ENV	    1

#define ADAM2_ENV_STR_SIZE  FLASH_ENV_ENTRY_SIZE        /*--- 256 Bytes ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_urlader_getenv_proc(struct ctl_table     *ctl,
                                   int                  write,
                                   void                 *buffer,
                                   size_t               *lenp,
                                   loff_t               *ppos)
{

    char tmp_buf[ADAM2_ENV_STR_SIZE];
    char *val, len;
    char *default_val = NULL;
    int free_val;

    /*--- pr_info("[avm_urlader_getenv_proc]: ctl->ctl_name=%u, write=%u *lenp=%u *ppos=%u\n", ---*/
    /*--- (int)ctl->ctl_name, (int)write, (int)*lenp, (int)*ppos); ---*/

    if(write){
        pr_err("write not supported\n");
        return -EFAULT;
    }
    if(!*lenp || *ppos){
        *lenp = 0;
        return 0;
    }

    switch((int) ctl->extra1){
    case FLASH_FS_ANNEX:
        default_val = "B";
        break;
    case FLASH_FS_FIRMWARE_VERSION:
        default_val = "";
        break;
    default:
        pr_err("%s: id %x not supported\n", __func__, (int ) ctl->extra1);
        return -EFAULT;
    }

    free_val = 1;
    val = avm_urlader_env_get_value_by_id((int) ctl->extra1);

    if(val == NULL){
        if(default_val){
            val = default_val;
        }else{
            val = "";
        }
        free_val = 0;
    }

    /* Scan thru the flash, looking for each possible variable index */
    len = strnlen(val, ADAM2_ENV_STR_SIZE - 1);
    /*--- pr_info("[avm_urlader_getenv_proc] val=%s len=%u\n", val, len); ---*/

    memcpy(tmp_buf, val, len + 1);

    if(free_val)
        kfree(val);

    if(len > *lenp)
        len = *lenp;

    if(len){
        if(copy_to_user(buffer, tmp_buf, len)){
            return -EFAULT;
        }
    }

    *lenp = len;
    *ppos += len;

    /*--- pr_info("[avm_urlader_getenv_proc] *lenp=%u *ppos\n", *lenp, (int)*ppos); ---*/

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_GEN3) || defined(CONFIG_MACH_PUMA7)
static int avm_urlader_get_config(struct ctl_table  *ctl,
                                   int              write,
                                   void             *buffer,
                                   size_t           *lenp,
                                   loff_t           *ppos)
{

    char tmp_buf[ADAM2_ENV_STR_SIZE];
    int ret;

    if(!*lenp || (*ppos && !write)){
        *lenp = 0;
        return 0;
    }

    if(write){
        size_t len = *lenp;
        if (len >= ADAM2_ENV_STR_SIZE)
            return -EOVERFLOW;
        if (copy_from_user(tmp_buf, buffer, len))
            return -EFAULT;
        tmp_buf[len] = '\0';

        ret = copy_wlan_dect_config2user(tmp_buf, len + 1);
        if (ret)
            return ret;

        *ppos = len;
        *lenp = len; /*--- lenp darf nicht kleiner sein als bei Uebergabe, sonst werden endlos aufgerufen ---*/

        return 0;
    }else{
        char *mybuffer = kmalloc(*lenp, GFP_KERNEL);
        if(mybuffer){
            if (!test_wlan_dect_config(mybuffer, lenp)){
                if(copy_to_user(buffer, mybuffer, *lenp)){
                    kfree(mybuffer);
                    return -EFAULT;
                }
                *ppos = *lenp;
                kfree(mybuffer);
                return 0;
            }
            kfree(mybuffer);
        }else{
            printk(KERN_ERR "[%s] kmalloc failed\n", __FUNCTION__);
        }
    }

    return -EFAULT;
}
#endif /*--- #if defined(CONFIG_ARCH_PUMA5)  || defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_GEN3)---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_urlader_setenv_sysctl(struct ctl_table   *ctl,
                                     int                write,
                                     void               *buffer,
                                     size_t             *lenp,
                                     loff_t             *ppos)
{
    char *var, *val, *ptr;
    char tmp_buf[2 * ADAM2_ENV_STR_SIZE];
    int i, result;
    size_t len, total;

    result = 0;
    if(!*lenp || (*ppos && !write)){
        *lenp = 0;
        goto err_out;
    }

    if(ctl->extra1 != (void *) DEV_ADAM2_ENV){
        result = -ENODEV;
        goto err_out;
    }

    if(write){
        if(*lenp >= ADAM2_ENV_STR_SIZE){
            result = -EINVAL;
            goto err_out;
        }

        if (copy_from_user(tmp_buf, buffer, *lenp)){
            result =-EACCES;
            goto err_out;
        }

        tmp_buf[*lenp] = '\0';
        if(tmp_buf[*lenp - 1] == '\n'){
            tmp_buf[*lenp - 1] = '\0';
        }

        *ppos = *lenp;

        /*--- printk(KERN_ERR "{%s}\n", __func__); ---*/
        /*--- dump_stack(); ---*/

        ptr = strpbrk(tmp_buf, " \t");
        if(!ptr){
            /* We have no way to distinguish between unsetting a
             * variable and setting a non-value variable.
             *
             * Consequently, we will treat any variable
             * without a value as an unset request.
             */
            avm_urlader_env_unset_variable(tmp_buf);
        }else{
            /* Set the variable-value pair in flash */
            *ptr++ = '\0';

            result = avm_urlader_env_set_variable(tmp_buf, ptr);
            if(result != 0){
                pr_notice("Defragging the environment variable region.\n");

                result = avm_urlader_env_defrag();
                if(result != 0){
                    pr_warning("Defragging the environment variable region failed.\n");
                    goto err_out;
                }

                result = avm_urlader_env_set_variable(tmp_buf, ptr);
                if(result != 0){
                    pr_warning("Failed to write %s to environment variable region.\n", tmp_buf);
                    goto err_out;
                }
            }
        }
    }else{
        total = 0;
        /* Scan through the flash, looking for each possible variable index */
        for(i = 0; i < MAX_ENV_ENTRY; i++){
            var = avm_urlader_env_get_variable(i);
            if(var == NULL){
                continue;
            }

            val = avm_urlader_env_get_value(var);
            if(val == NULL){
                kfree(var);
                continue;
            }

            len = snprintf(tmp_buf, sizeof(tmp_buf), "%s\t%s\n", var, val);
            kfree(var);
            kfree(val);

            /* suppress variable if output would be truncated */
            if(len >= sizeof(tmp_buf)){
                pr_warn("[%s] Variable truncated while printing: %s\n", __func__, tmp_buf);
                continue;
            }

            if((len + total) > *lenp){
                len = *lenp - total;
            }

            if(len){
                result = copy_to_user(buffer + total, tmp_buf, len);
                if(result != 0){
                    goto err_out;
                }
            }

            total += len;
        }

        *lenp = total;
        *ppos += total;
    }

err_out:
    return result;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_urlader_reset_status_proc(struct ctl_table   *ctl,
                                         int                write,
                                         void               *buffer,
                                         size_t             *lenp,
                                         loff_t             *ppos)
{
    char tmp_buf[ADAM2_ENV_STR_SIZE] __maybe_unused;
    enum _avm_reset_status status;
    unsigned char *reason;
    int result;

    result = 0;
    if(!*lenp || (*ppos && !write)){
        *lenp = 0;
        goto err_out;
    }

    if(write) {
#if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MACH_FUSIV)
        result = -EFAULT;
#else
        if(*lenp >= ADAM2_ENV_STR_SIZE){
            result = -EFAULT;
            goto err_out;
        }

        if(copy_from_user(tmp_buf, buffer, *lenp)){
            result = -EFAULT;
            goto err_out;
        }

        tmp_buf[*lenp] = '\0';
        if(tmp_buf[*lenp - 1] == '\n'){
            tmp_buf[*lenp - 1] = '\0';
        }

        *ppos = *lenp;

        if ( ! strcmp(tmp_buf, "FW-Update")) {
            avm_set_reset_status(RS_FIRMWAREUPDATE);
        } else if ( ! strcmp(tmp_buf, "reboot")) {
            avm_set_reset_status(RS_REBOOT);
        } else if ( ! strcmp(tmp_buf, "temperature")) {
            avm_set_reset_status(RS_TEMP_REBOOT);
        } else if ( ! strcmp(tmp_buf, "reboot-for-update")) {
            avm_set_reset_status(RS_REBOOT_FOR_UPDATE);
        } else if ( ! strcmp(tmp_buf, "docsis-local")) {
            avm_set_reset_status(RS_DOCSIS_LOCAL);
        } else if ( ! strcmp(tmp_buf, "docsis-operator")) {
            avm_set_reset_status(RS_DOCSIS_OPERATOR);
        } else {
            result = -EINVAL;
        }
#endif
    } else {
        status = avm_reset_status();
        switch (status){
            case RS_POWERON:
                reason = "poweron";
                break;
            case RS_SOFTWATCHDOG:
                reason = "software";
                break;
            case RS_NMIWATCHDOG:
                reason = "watchdog";
                break;
            case RS_REBOOT:
                reason = "reboot";
                break;
            case RS_NMI_WA:
                reason = "nmi_workaround";
                break;
            case RS_FIRMWAREUPDATE:
                reason = "firmware_update";
                break;
            case RS_SHORTREBOOT:
                reason = "short-reboot";
                break;
            case RS_BUSERROR:
                reason = "buserror-reboot";
                break;
            case RS_TEMP_REBOOT:
                reason = "temperature-reboot";
                break;
            case RS_REBOOT_FOR_UPDATE:
                reason = "reboot-for-update";
                break;
            case RS_DOCSIS_LOCAL:
                reason = "docsis-local";
                break;
            case RS_DOCSIS_OPERATOR:
                reason = "docsis-operator";
                break;
            default:
                reason = "unbekannt";
                break;
        }

        *lenp = strlen(reason);
        *ppos = *lenp;

        if(copy_to_user(buffer, reason, *lenp)){
            result = -EINVAL;
        }
    }

err_out:
    return result;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct ctl_table avm_urlader_env_table3[] = {
    {
        .extra1         = (void *) DEV_ADAM2_ENV,
        .procname       = "environment", 
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE, 
        .mode           = 0644, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_setenv_sysctl 
    }, {
        .extra1         = (void *) FLASH_FS_FIRMWARE_VERSION,
        .procname       = "firmware_version", 
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE, 
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_getenv_proc
    }, {
        .extra1         = (void *) FLASH_FS_ANNEX,
        .procname       = "annex",            
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE, 
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_getenv_proc
    },
    {
        .procname       = "reboot_status",            
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE,
        .mode           = 0644, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_reset_status_proc
    },
#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_GEN3) || defined(CONFIG_MACH_PUMA7)
    {
        .procname       = "config",
        .data           = NULL,
        .maxlen         = 0,
        .mode           = 0644,
        .child          = NULL,
        .proc_handler   = &avm_urlader_get_config
    },
#endif
    {
    }
};

struct ctl_table avm_urlader_root_table3[] = {
    {
        .extra1         = (void *) DEV_ADAM2_ENV,
        .procname       = "urlader",
        .data           = NULL, 
        .maxlen         = 0, 
        .mode           = 0555, 
        .child          = avm_urlader_env_table3
    },
    {
    }
};

static struct ctl_table_header *avm_urlader_sysctl_header;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_urlader_env_sysctl_register(void)
{
    static int initialized = 0;

    if(initialized != 0){
        return;
    }

    avm_urlader_sysctl_header = register_sysctl_table(avm_urlader_root_table3);
    //avm_urlader_root_table->child->de->owner = THIS_MODULE; // PORT_ERROR_32: who or what is ->de->owner

    initialized = 1;
}

#if defined(MODULE)
static void avm_urlader_env_sysctl_unregister(void)
{
    unregister_sysctl_table(avm_urlader_sysctl_header);
}
#endif /*--- #if defined(MODULE) ---*/

int avm_urlader_env_init(void)
{
    avm_urlader_env_sysctl_register();
    pr_info("Adam2 environment variables API installed.\n");
    return 0;
}

#if defined(MODULE)
void avm_urlader_env_exit(void)
{
    avm_urlader_env_sysctl_unregister();
}
#endif /*--- #if defined(MODULE) ---*/

#endif /* CONFIG_SYSCTL */

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#ifdef CONFIG_PROC_FS
static struct tffs_cdev *tffsptr = NULL;
static struct proc_dir_entry *tffs_proc = NULL;
unsigned int tffs_info_value;

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int tffsproc_write(struct file *file, const char *buffer, size_t count, loff_t *ppos __attribute__((unused)))
{
    char page[80];
    char *p;
    int request = -1;
    unsigned int id;
    struct tffs_core_handle *handle;
    int result;

    handle = NULL;
    result = 0;

    if(IS_ERR_OR_NULL(tffsptr)){
        result = -ENODEV;
        goto err_out;
    }

    if(count > 0 && count <= sizeof(page)){
        result = copy_from_user(&page[0], buffer, count);
        page[count - 1] = '\0';
    }else{
        result = -EINVAL;
    }

    if(result != 0){
        goto err_out;
    }

    p = skip_spaces(&page[0]);
    if(strstarts(p, "info")){
        request = _TFFS_INFO;
    }else if(strstarts(p, "index")){
        request = _TFFS_REINDEX;
    }else if(strstarts(p, "cleanup")){
        request = _TFFS_CLEANUP;
    }else if(strstarts(p, "clear_id")){
        request = _TFFS_CLEAR_ID;
    }else if(strstarts(p, "werkseinstellung")){
        request = _TFFS_WERKSEINSTELLUNG;
    }else{
        pr_err("/proc/tffs: request not recognized\n");
        result = -EINVAL;
        goto err_out;
    }


    switch(request){
    case _TFFS_INFO:
    case _TFFS_REINDEX:
    case _TFFS_CLEANUP:
    case _TFFS_WERKSEINSTELLUNG:
        id = 0;
        break;
    case _TFFS_CLEAR_ID:
        p += strlen("clear_id");
        p = skip_spaces(p);
        result = kstrtouint(p, 0, &id);
        if(result != 0 || id == 0 || id > FLASH_FS_ID_LAST){
            pr_err("/proc/tffs: clear id request: invalid id %s\n", p);
            result = -EINVAL;
        }
        break;
    default:
        pr_err("/proc/tffs: command not recognized\n");
        result = -EINVAL;
    }

    if(result != 0){
        goto err_out;
    }

    handle = TFFS3_Open(id, tffs3_mode_write);
    if(IS_ERR_OR_NULL(handle)){
        result = (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
        goto err_out;
    }

    switch(request){
    case _TFFS_INFO:
        result = TFFS3_Info(handle, &tffs_info_value);
        pr_info("/proc/tffs: info request: %s\n", result ? "failed" : "success");
        break;
    case _TFFS_REINDEX:
        result = TFFS3_Create_Index();
        pr_info("/proc/tffs: index request: %s\n", result ? "failed" : "success");
        break;
    case _TFFS_CLEANUP:
        result = TFFS3_Cleanup(handle);
        pr_info("/proc/tffs: cleanup request: %s\n", result ? "failed" : "success");
        break;
    case _TFFS_WERKSEINSTELLUNG:
        result = TFFS3_Werkseinstellungen(handle);
        pr_info("/proc/tffs: werkseinstellungen request: %s\n", result ? "failed" : "success");
        break;
    case _TFFS_CLEAR_ID:
        result = TFFS3_Clear(handle);
        pr_info("/proc/tffs: clear id 0x%x request: %s\n", id, result ? "failed" : "success");
        break;
    }

err_out:
    if(!IS_ERR_OR_NULL(handle)){
        TFFS3_Close(handle);
    }

    return result == 0 ? count : result;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int tffsproc_show(struct seq_file *seq, void *data __attribute__((unused)))
{
    seq_printf(seq, "TFFS \n");

#if defined(CONFIG_TFFS2) || defined(CONFIG_TFFS_DEV_LEGACY)
    seq_printf(seq, "mount=mtd%u\n", tffs_mtd[0]);
    seq_printf(seq, "mount=mtd%u\n", tffs_mtd[1]);
#endif

    seq_printf(seq, "request=%u\n", tffsptr->request_count);

    if(tffs_info_value){
        seq_printf(seq, "fill=%u\n", tffs_info_value);
    }

    if(tffsptr->pending_events){
        seq_printf(seq, "event panding\n");
    }

#if defined(CONFIG_TFFS_EXCLUSIVE)
    seq_printf(seq, "mode: read/write: exclusive\n");
#else /*--- #if defined(CONFIG_TFFS_EXCLUSIVE) ---*/
    seq_printf(seq, "mode: read/write: shared\n");
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_EXCLUSIVE) ---*/

    seq_printf(seq, "thread state=%s\n",
               tffsptr->thread_state == tffs3_thread_state_off ?     "off" :
               tffsptr->thread_state == tffs3_thread_state_init ?    "init" :
               tffsptr->thread_state == tffs3_thread_state_idle ?    "idle" :
               tffsptr->thread_state == tffs3_thread_state_process ? "process" :
               tffsptr->thread_state == tffs3_thread_state_down ?    "down" : "unknown");

    return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int tffsproc_open(struct inode *inode __attribute__((unused)), struct file *file)
{
    int result;

    if(IS_ERR_OR_NULL(tffsptr)){
        result = -ENODEV;
    } else {
        result = single_open(file, tffsproc_show, NULL);
    }

    return result;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const struct file_operations tffsproc_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        .owner = THIS_MODULE,
#endif
        .open = tffsproc_open,
        .read = seq_read,
        .write = tffsproc_write,
        .llseek = seq_lseek,
        .release = single_release,
};

void tffs_proc_init(struct tffs_cdev *tffs)
{
    tffsptr = tffs;

    if(!IS_ERR_OR_NULL(tffsptr)){
        tffs_proc = proc_create("tffs", 0, 0, &tffsproc_fops);
    }
}

void tffs_proc_remove(struct tffs_cdev *tffs)
{
    if(!IS_ERR_OR_NULL(tffs_proc)){
        remove_proc_entry("tffs", 0);
    }
}

#endif /*--- #ifdef CONFIG_PROC_FS ---*/

