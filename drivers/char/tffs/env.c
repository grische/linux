// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004-2023 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/overflow.h>
#include <avm/tffs/tffs.h>
#include "local.h"

const struct _TFFS_Name_Table TFFS_Name_Table[URLADER_ENV_NAMETABLE_SIZE] =
	TFFS_NAMETABLE_INITIALIZER;

static int avm_urlader_env_defrag(void);

int tffs_write_entry(int tffs_id, const void *buf, size_t len)
{
	struct tffs_core_handle *handle;
	size_t chunksiz;
	int ret;

	handle = TFFS3_Open(tffs_id, tffs3_mode_write);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	do {
		chunksiz = min(len, handle->max_segment_size);
		ret = TFFS3_Write(handle, buf, chunksiz, chunksiz == len);
		buf += chunksiz;
		len -= chunksiz;
	} while (ret == 0 && len > 0);

	TFFS3_Close(handle);

	if (ret && ret != -ENOENT)
		pr_err("[TFFS] Error writing id=%d: %d\n", tffs_id, ret);
	return ret;
}

int tffs_clear_entry(int tffs_id)
{
	struct tffs_core_handle *handle;
	int ret;

	handle = TFFS3_Open(tffs_id, tffs3_mode_write);
	if (IS_ERR(handle))
		return PTR_ERR(handle);
	ret = TFFS3_Clear(handle);
	TFFS3_Close(handle);
	return ret;
}

ssize_t tffs_read_entry(int tffs_id, void *buf, size_t len)
{
	struct tffs_core_handle *handle;
	void *pos = buf, *end = buf + len;
	int ret;

	handle = TFFS3_Open(tffs_id, tffs3_mode_read);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	do {
		len = end - pos;
		ret = TFFS3_Read(handle, pos, &len);
		pos += len;
	} while (ret == 0 && len);

	TFFS3_Close(handle);

	if (ret == 0)
		return pos - buf;

	if (ret != -ENOENT)
		pr_err("[TFFS] Error reading id=%d: %d\n", tffs_id, ret);
	return ret;
}

static bool urlader_nametable_is_current(void)
{
	const char *tblversion = TFFS_Name_Table[0].Name;
	struct __tffs_name_table_ondisk_storage saved_ntbl;
	int ret;

	ret = tffs_read_entry(FLASH_FS_NAME_TABLE, &saved_ntbl, sizeof(saved_ntbl));
	if (ret < 0)
		return false;

	/* Handle junk safely */
	if (!memchr(saved_ntbl.name, '\0', sizeof(saved_ntbl.name)))
		return false;

	if (be32_to_cpu(saved_ntbl.id) != FLASH_FS_TABLE_VERSION ||
	    strcmp(saved_ntbl.name, tblversion)) {
		pr_notice("TFFS Name Table needs update: old %s, new %s\n",
			  saved_ntbl.name, tblversion);
		return false;
	}
	return true;
}

void *avm_urlader_build_name_table(size_t *size)
{
	unsigned int i, len;
	void *buffer;

	buffer = kmalloc_array(ARRAY_SIZE(TFFS_Name_Table),
				sizeof(struct __tffs_name_table_ondisk_storage), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	for (i = len = 0; i < ARRAY_SIZE(TFFS_Name_Table); ++i) {
		struct tffs_name_table_ondisk *saved_ntbl;
		const struct _TFFS_Name_Table *ntbl;
		size_t namesiz, entrysiz;

		ntbl = &TFFS_Name_Table[i];
		saved_ntbl = buffer + len;

		if (!ntbl->id)
			break;

		namesiz = strlen(ntbl->Name) + 1;
		entrysiz = struct_size(saved_ntbl, name, namesiz);

		saved_ntbl->id = cpu_to_be32(ntbl->id);
		strcpy(saved_ntbl->name, ntbl->Name);

		len += ALIGN(entrysiz, 4);
	}
	*size = len;
	return buffer;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int avm_urlader_getenv_id(const char *Name)
{
	const struct _TFFS_Name_Table *t;

	for (t = &TFFS_Name_Table[0]; t->id; ++t) {
		if (Name[0] < t->Name[0])
			break; /*--- hat keinen Zweck mehr ---*/
		if (!strcmp(Name, t->Name))
			return t->id;
	}
	return -1;
}

/*--------------------------------------------------------------------------*\
 * liefert den Namen für die ID der Variable
 *
 * Get the variable referenced by id
 \*-------------------------------------------------------------------------*/
const char *avm_urlader_getenv_name(unsigned int id)
{
	const struct _TFFS_Name_Table *t;

	for (t = &TFFS_Name_Table[0]; t->id; ++t)
		if (t->id == id)
			return t->Name;
	return NULL;
}

int avm_urlader_env_init(void)
{
	int result = 0;

	avm_urlader_sysctl_init();

	/* is nametable present and up to date? */
	if (!urlader_nametable_is_current()) {
		u8 *buffer;
		size_t len;

		buffer = avm_urlader_build_name_table(&len);
		if (IS_ERR(buffer))
			return PTR_ERR(buffer);
		result = tffs_write_entry(FLASH_FS_NAME_TABLE, buffer, len);
		kfree(buffer);
	}


	return result;
}

int avm_urlader_getenv_by_id(unsigned int id, char *buf, size_t size)
{
	char maxbuf[URLADER_ENV_VALUE_MAX];
	ssize_t len;

	if (!size)
		return 0;

	len = tffs_read_entry(id, maxbuf, sizeof(maxbuf)-1);
	if (len < 0)
		return len;

	maxbuf[len] = '\0';
	len = strscpy(buf, maxbuf, size);

	return len < 0 ? len : 0;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Inhalt der Variable
 *
 * Get the value associated with an environment variable
 \*-----------------------------------------------------------------------------------------------*/
int avm_urlader_getenv(const char *var, char *value, size_t size)
{
	int id;

	id = avm_urlader_getenv_id(var);
	if (id == -1)
		return -ENOENT;

	return avm_urlader_getenv_by_id(id, value, size);
}
EXPORT_SYMBOL(avm_urlader_getenv);

int avm_urlader_setenv_by_id(int id, const char *val)
{
	int result, defragged = 0;

retry:
	result = avm_urlader_setenv_begin();
	if (result)
		return result;
	if (!val)
		result = tffs_clear_entry(id);
	else
		result = tffs_write_entry(id, val, strlen(val) + 1);
	avm_urlader_setenv_end();
	if (result && val && !defragged++ && !avm_urlader_env_defrag())
		goto retry;

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
 * Set the variable to a specific value.
 *
 * NOTE: If the value is NULL, the variable will be unset. Otherwise, the
 * variable-value pair will be written to flash.
 \*-----------------------------------------------------------------------------------------------*/
int avm_urlader_setenv(const char *var, const char *val)
{
	unsigned int id;

	id = avm_urlader_getenv_id(var);
	if ((int)id < 0)
		return -ENOENT;

	return avm_urlader_setenv_by_id(id, val);

}
EXPORT_SYMBOL(avm_urlader_setenv);

/*-----------------------------------------------------------------------------------------------*\
 * Unset the variable to a specific value.
 \*-----------------------------------------------------------------------------------------------*/
int avm_urlader_unsetenv(const char *var)
{
	return avm_urlader_setenv(var, NULL);
}
EXPORT_SYMBOL(avm_urlader_unsetenv);

int avm_urlader_unsetenv_by_id(unsigned int id)
{
	return avm_urlader_setenv_by_id(id, NULL);
}

/*-----------------------------------------------------------------------------------------------*\
 * Defrag the block associated with the Adam2 environment variables.
 \*-----------------------------------------------------------------------------------------------*/
static int avm_urlader_env_defrag(void)
{
	struct tffs_core_handle *handle;
	int result;

	pr_notice("Defragging the environment variable region.\n");

	handle = TFFS3_Open(0, tffs3_mode_write);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	result = TFFS3_Cleanup(handle);
	TFFS3_Close(handle);

	if (result)
		pr_warn("Defragging the environment variable region failed: %d\n", result);
	return result;
}
