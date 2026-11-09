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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <asm/errno.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <asm/mach_avm.h>

#if defined(CONFIG_TFFS_ENV)
#define TFFS_NAME_TABLE
#include <avm/tffs/tffs.h>
#include "tffs_local.h"

static int avm_urlader_init(void);

static int initialised = 0;

// #define TFFS_CONFIG
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(TFFS_CONFIG)
#define DBG_TRC(args...)  printk(KERN_ERR args)
#else /*--- #if defined(TFFS_CONFIG) ---*/
#define DBG_TRC(args...)
#endif /*--- #else ---*/ /*--- #if defined(TFFS_CONFIG) ---*/
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void avm_urlader_extract_name_table(unsigned char *buffer, int len)
{
    unsigned int index = 0;
    unsigned int name_len;
    memset(&TFFS_Name_Table[0], 0, sizeof(TFFS_Name_Table));

    DBG_TRC("avm_urlader_extract_name_table(buffer=%x, len=%x)\n", (unsigned int)buffer, len);
    while(len > 0 && index < MAX_ENV_ENTRY){
        TFFS_Name_Table[index].id = be32_to_cpu(*(unsigned int *) buffer);
        buffer   += sizeof(unsigned int);
        len      -= sizeof(unsigned int);
        name_len  = strlen(buffer) + 1;

        /* check that entry name fits into struct. */
        if(name_len > sizeof(TFFS_Name_Table[index].Name)){
            pr_warn("[%s] corrupt TFFS name table found\n", __func__);

            /* return unusable name table to force rebuild */
            TFFS_Name_Table[0].id = 0;
            memset(TFFS_Name_Table[0].Name, 0, sizeof(TFFS_Name_Table[0].Name));
            goto err_out;
        }

        memcpy(TFFS_Name_Table[index].Name, buffer, name_len);

        name_len  = (name_len + 3) & ~0x03;
        len      -= name_len;
        buffer   += name_len;
        index++;
    }

err_out:
    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
size_t avm_urlader_build_name_table(unsigned char *buffer, size_t max_len)
{
    unsigned int index = 0;
    size_t name_len, output_len = 0;

    /* point current table to default table before erasing runtime table */
    current_nametable = T_Init;
    smp_mb();

    memset(&TFFS_Name_Table[0], 0, sizeof(TFFS_Name_Table));
    memcpy(&TFFS_Name_Table[0], &T_Init[0], sizeof(T_Init));

    DBG_TRC("avm_urlader_build_name_table(buffer=%x, max_len=%x)\n", (unsigned int)buffer, max_len);
    while(index < MAX_ENV_ENTRY && TFFS_Name_Table[index].id && output_len + 64 < max_len){
        if(TFFS_Name_Table[index].Name && TFFS_Name_Table[index].Name[0]){
            *(unsigned int *) buffer = cpu_to_be32(TFFS_Name_Table[index].id);
            buffer      += sizeof(unsigned int);
            output_len  += sizeof(unsigned int);
            name_len     = strlen(TFFS_Name_Table[index].Name) + 1;
            name_len     = (name_len + 3) & ~0x03;
            memcpy(buffer, TFFS_Name_Table[index].Name, name_len);
            output_len  += name_len;
            buffer      += name_len;
        }
        index++;
    }
    return output_len;
}

const struct _TFFS_Name_Table * avm_urlader_get_nametable(void)
{
    smp_mb();
    return current_nametable;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int avm_urlader_get_name(char *Name)
{
    struct _TFFS_Name_Table *T = &TFFS_Name_Table[0];
    DBG_TRC("avm_urlader_get_name(%s)\n", Name);

    while(T->id){
        if(Name[0] == T->Name[0] && !strcmp(Name, T->Name)){
            return T->id; /*--- gefunden ---*/
        }
        if(Name[0] < T->Name[0]){
            return (unsigned int) -1; /*--- hat keine Zweck mehr ---*/
        }
        T++;
    }

    DBG_TRC("avm_urlader_get_name(%s): not found\n", Name);
    return (unsigned int) -1;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Namen für den Index der Variable
 *
 * Get the variable referenced by index
 *
 * NOTE: Caller is responsibe for freeing the memory. 
 \*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_variable(int idx)
{
    char *buffer, *result;
    int status;

    DBG_TRC("avm_urlader_env_get_variable(%u)\n", idx);

    if(idx < 0){
        pr_err("[%s-%d] avm_urlader_env_get_variable(%d) failed, invalid handle\n", __func__, __LINE__, idx);
        return NULL ;
    }

    status = 0;
    if(initialised == 0){
        status = avm_urlader_init();
    }

    if(status != 0){
        pr_err("[%s-%d] avm_urlader_env_get_variable(%u) failed, not initialised\n", __func__, __LINE__, idx);
        return NULL ;
    }

    result = NULL;
    if(idx < ARRAY_SIZE(TFFS_Name_Table) - 1){
        if(TFFS_Name_Table[idx].Name && TFFS_Name_Table[idx].Name[0]){
            buffer = kmalloc(strlen(TFFS_Name_Table[idx].Name) + 1, GFP_KERNEL);
            if(buffer == NULL ){
                pr_err("[%s-%d] avm_urlader_env_get_variable(%u) found, but no memory\n", __func__, __LINE__, idx);
                goto err_out;
            }
            strcpy(buffer, TFFS_Name_Table[idx].Name);
            result = buffer;
        }
    }

    /*--- DBG_TRC("avm_urlader_env_get_variable(%u) not found\n", idx); ---*/

err_out:
    return result;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Namen für die ID der Variable
 *
 * Get the variable referenced by id
 *
 * NOTE: Caller is responsibe for freeing the memory. 
 \*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_id_name(unsigned int id)
{
    char *buffer;
    struct _TFFS_Name_Table *T = &TFFS_Name_Table[0];
    int status;

    DBG_TRC("%s(%u)\n", __func__, idx);

    status = 0;
    if(initialised == 0){
        status = avm_urlader_init();
    }

    if(status != 0){
        pr_err("[%s-%d] %s(%u) failed, not initialised\n", __func__, __LINE__, __func__, id);
        return NULL ;
    }

    while(T->id){
        /* TODO: Can this be optimized? */
        if(T->id == id){
            buffer = kmalloc(strlen(T->Name) + 1, GFP_KERNEL);
            if(buffer == NULL ){
                pr_err("[%s-%d] %s(%u) found, but no memory\n", __func__, __LINE__, __func__, id);
                return NULL;
            }
            strcpy(buffer, T->Name);
            return buffer; /*--- gefunden ---*/
        }
        T++;
    }
    
    DBG_TRC("%s(%u) not found\n", __func__, id);
    return NULL;
}

/*-----------------------------------------------------------------------------------------------*\
 * Check if name table for environment variables is present and its version number is not older
 * then that of the table compiled into this kernel.
 * Returns:
 *      0 if table is up to date
 *      1 table is present but too old
 *     <0 on error
\*-----------------------------------------------------------------------------------------------*/
static int nametable_current(void)
{
    unsigned char *buffer;
    unsigned int need_rebuild;
    size_t ret_length, total_len;
    struct tffs_core_handle *handle;
    int result;

    buffer = NULL;
    handle = NULL;
    need_rebuild = 1;

    handle = TFFS3_Open(FLASH_FS_NAME_TABLE, tffs3_mode_read);
    if(IS_ERR_OR_NULL(handle)){
        DBG_TRC("[%s] open TFFS failed\n", __func__);
        result = PTR_ERR(handle);
        goto err_out;
    }

    buffer = kmalloc(FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY, GFP_KERNEL);
    if(buffer == NULL ){
        DBG_TRC("[%s] out of memory\n", __func__);
        result = -ENOMEM;
        goto err_out;
    }

    total_len = 0;
    do{
        ret_length = (FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY) - total_len;
        result = TFFS3_Read(handle, buffer, &ret_length);
        total_len += ret_length;
    }while(result == 0 && ret_length > 0);

    if(result != 0 && result != -ENOENT){
        goto err_out;
    }

    if(result == 0){
        DBG_TRC("[%s] read name table success (%u bytes)\n", __func__, ret_length);
        avm_urlader_extract_name_table(buffer, total_len);
        DBG_TRC("[%s] extract name table success\n", __func__);

        if(   (T_Init[0].id == TFFS_Name_Table[0].id)
           && (T_Init[0].Name[1] == TFFS_Name_Table[0].Name[1]))
        {
            pr_info("TFFS Name Table %c\n", TFFS_Name_Table[0].Name[1]);
            need_rebuild = 0;
        }else{
            pr_warn("WARNING: TFFS Name Table update ! (current %s new %s)\n", TFFS_Name_Table[0].Name, T_Init[0].Name);
        }
    }

err_out:
    if(buffer != NULL){
        kfree(buffer);
    }

    if(!IS_ERR_OR_NULL(handle)){
        TFFS3_Close(handle);
    }

    return need_rebuild;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_urlader_init(void)
{
    unsigned char *buffer;
    unsigned int need_rebuild;
    size_t ret_length, total_len, written;
    struct tffs_core_handle *handle;
    int result;

    buffer = NULL;
    handle = NULL;
    result = 0;

    if(initialised){
        return 1;
    }

    /* is nametable present and up to date? */
    need_rebuild = nametable_current();

    if(need_rebuild != 0){
        DBG_TRC("[%s] Updating nametable\n", __func__);
        handle = TFFS3_Open(FLASH_FS_NAME_TABLE, tffs3_mode_write);
        if(IS_ERR_OR_NULL(handle)){
            DBG_TRC("[%s] open TFFS failed\n", __func__);
            result = (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
            goto err_out;
        }

        buffer = kmalloc(FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY, GFP_KERNEL);
        if(buffer == NULL ){
            DBG_TRC("[%s] out of memory\n", __func__);
            result = -ENOMEM;
            goto err_out;
        }

        total_len = avm_urlader_build_name_table(buffer, FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY);
        DBG_TRC("[%s] build name table success (%u bytes)\n", __func__, total_len);

        written = 0;
        do{
            ret_length = min(total_len - written, handle->max_segment_size);
            result = TFFS3_Write(handle, buffer + written,
                                 ret_length, (written + ret_length >= total_len));
            written += ret_length;
        }while(result == 0 && total_len > written);

        if(result != 0){
            DBG_TRC("[%s] write name table failed\n", __func__);
            goto err_out;
        }

        DBG_TRC("[%s] write name table success (%u bytes)\n", __func__, ret_length);
    }

    current_nametable = &TFFS_Name_Table[0];
    initialised = 1;

err_out:
    if(buffer != NULL ){
        kfree(buffer);
    }

    if(!IS_ERR_OR_NULL(handle)){
        TFFS3_Close(handle);
    }

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Inhalt der Variable
 *
 * Get the value associated with an environment variable
 *
 * NOTE: Caller is responsibe for freeing the memory. 
 \*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_value_by_id(unsigned int id)
{
    struct tffs_core_handle *handle;
    unsigned int length, total_len;
    unsigned char *buffer;
    int result;

    DBG_TRC("avm_urlader_env_get_value_by_id(%u)\n", id);

    result = 0;
    buffer = NULL;
    handle = NULL;

    if(!initialised){
        result = avm_urlader_init();
    }
    if(result != 0){
        goto err_out;
    }

    buffer = kmalloc(FLASH_ENV_ENTRY_SIZE + 1, GFP_KERNEL);
    if(buffer == NULL ){
        goto err_out;
    }

    handle = TFFS3_Open(id, tffs3_mode_read);
    if(IS_ERR_OR_NULL(handle)){
        goto err_out;
    }

    total_len = 0;
    do{
        length = FLASH_ENV_ENTRY_SIZE - total_len;
        result = TFFS3_Read(handle, buffer + total_len, &length);
        total_len += length;
    }while(result == 0 && length > 0);

    if(result != 0){
        goto err_out;
    }

    buffer[total_len] = '\0';
    TFFS3_Close(handle);

    DBG_TRC("avm_urlader_env_get_value_by_id(%u) : '%s'\n", id, buffer);
    return buffer;

err_out:
    if(buffer != NULL ){
        kfree(buffer);
    }
    if(!IS_ERR_OR_NULL(handle)){
        TFFS3_Close(handle);
    }

    return NULL ;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Inhalt der Variable
 *
 * Get the value associated with an environment variable
 *
 * NOTE: Caller is responsibe for freeing the memory. 
 \*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_value(char *var)
{
    unsigned int id;
    int result;

    DBG_TRC("avm_urlader_env_get_value(%s)\n", var);

    result = 0;
    if(!initialised){
        result = avm_urlader_init();
    }
    if(result != 0){
        return NULL ;
    }

    id = avm_urlader_get_name(var);
    if(id == (unsigned int) -1){
        return NULL ;
    }

    DBG_TRC("avm_urlader_env_get_value(%s) id=%u\n", var, id);

    return avm_urlader_env_get_value_by_id(id);
}

/*-----------------------------------------------------------------------------------------------*\
 * Set the variable to a specific value.
 *
 * NOTE: If the value is NULL, the variable will be unset. Otherwise, the
 * variable-value pair will be written to flash.
 \*-----------------------------------------------------------------------------------------------*/
int avm_urlader_env_set_variable(char *var, char *val)
{
    struct tffs_core_handle *handle;
    unsigned int id;
    size_t written, seg_len, total_len;
    int result;

    DBG_TRC("[%s] %s=%s\n", __func__, var, val);

    result = 0;
    if(!initialised){
        result = avm_urlader_init();
    }
    if(result != 0){
        return result;
    }

    id = avm_urlader_get_name(var);

    if(id == (unsigned int) -1){
        return -ENOENT;
    }

    pr_debug("[%s] opening ID 0x%x for writing\n", __func__, id);
    handle = TFFS3_Open(id, tffs3_mode_write);
    if(IS_ERR_OR_NULL(handle)){
        pr_debug("[%s] TFFS3_Open returned %ld\n", __func__, PTR_ERR(handle));
        return (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
    }

    if(val == NULL ){
        result = TFFS3_Clear(handle);
    } else {
        total_len = strlen(val) + 1;
        written = 0;
        do{
            seg_len = min(total_len - written, handle->max_segment_size);
            result = TFFS3_Write(handle, val + written, seg_len,
                                 (written + seg_len >=total_len));
            if(result != 0){
                pr_debug("[%s] TFFS3_Write() returned %d\n", __func__, result);
            }
            written += seg_len;
        }while(result == 0 && total_len > written);
    }
    TFFS3_Close(handle);

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
 * Unset the variable to a specific value.
 \*-----------------------------------------------------------------------------------------------*/
int avm_urlader_env_unset_variable(char *var)
{
    return avm_urlader_env_set_variable(var, NULL );
}

/*-----------------------------------------------------------------------------------------------*\
 * Defrag the block associated with the Adam2 environment variables.
 \*-----------------------------------------------------------------------------------------------*/
int avm_urlader_env_defrag(void)
{
    struct tffs_core_handle *handle;
    int result;

    handle = TFFS3_Open(0, tffs3_mode_write);
    if(IS_ERR_OR_NULL(handle)){
        return (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
    }

    result = TFFS3_Cleanup(handle);

    TFFS3_Close(handle);

    return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(avm_urlader_env_get_value);
EXPORT_SYMBOL(avm_urlader_env_unset_variable);
EXPORT_SYMBOL(avm_urlader_env_set_variable);

#endif /*--- #if defined(CONFIG_TFFS_ENV) ---*/
/* vim: set sw=4 ts=4 et ai : */
