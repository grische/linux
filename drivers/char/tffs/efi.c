// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
#ifdef CONFIG_SMP
#define __SMP__
#endif /*--- #ifdef CONFIG_SMP ---*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/mtd/mtd.h>
#include <avm/tffs/tffs.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/
#include <avm/tffs/tffs.h>
#include <linux/efi.h>
#include "local.h"
#include "efi.h"

#define MAX_SEGMENT_SIZE (32 * 1024)

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void send_notification(struct tffs_efi_ctx *ctx, enum _tffs_id Id,
			      enum tffs3_notify_event evnt)
{
	//    pr_err("[%s-%d] Called.\n", __func__, __LINE__);
	if (ctx->notify_cb != NULL) {
		ctx->notify_cb(ctx->notify_priv, Id, evnt);
	}
}

static noinline int TFFS_get_name(struct tffs_efi_ctx *ctx, struct TFFS_EFI_State *state)
{
	const struct _TFFS_Name_Table *nametable;
	const char *name;
	int result;

	result = -ENOENT;
	name   = NULL;
	switch (state->id) {
	case FLASH_FS_NAME_TABLE:
		name = "tffs_nametable";
		break;
	case 0:
		name = "dummy";
		break;
	default:
		nametable = avm_urlader_get_nametable();
		while (nametable != NULL && nametable->id != 0) {
			if (nametable->id == state->id && nametable->Name[0]) {
				name = nametable->Name;
				break;
			}
			++nametable;
		}

		break;
	}
	if (name) {
		avm_efivar_resource_attach_var(state->res, name);
		result = 0;
	} else {
		pr_debug("[%s-%d] avm_urlader_env_get_variable() failed.\n", __func__, __LINE__);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void *TFFS3_EFI_Open(struct tffs_module *this, struct tffs_core_handle *core_handle)
{
	struct tffs_efi_ctx *ctx;
	struct TFFS_EFI_State *state;
	int result;

	ctx			      = (struct tffs_efi_ctx *)this->priv;
	core_handle->max_segment_size = EFIVARS_DATA_SIZE_MAX;

	// when opened in panic mode, use static state struct and if there is a special panic
	// mtd_info set up, use that one
	state  = NULL;
	result = 0;

	if (core_handle->id != 0 && (core_handle->id <= FLASH_FS_ID_FIRMWARE_CONFIG_LAST ||
				     core_handle->id >= FLASH_FS_ID_LAST)) {
		result = -EINVAL;
		goto err_out;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	state->offset = 0;
	state->id     = core_handle->id;
	state->res    = avm_efivar_resource_create();
	if (IS_ERR(state->res)) {
		result	   = PTR_ERR(state->res);
		state->res = NULL;
		goto err_out;
	}

	result = TFFS_get_name(ctx, state);
	if (result != 0) {
		pr_debug("[%s] EFI name translation failed for ID 0x%x\n", __func__, state->id);
		goto err_out;
	}

err_out:
	if (result != 0) {
		pr_debug("[%s] error: %d", __func__, result);
		if (!IS_ERR_OR_NULL(state)) {
			avm_efivar_resource_destroy(state->res);
			kfree(state);
		}
		state = ERR_PTR(result);
	}

	return state;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Close(struct tffs_module *this, void *handle)
{
	struct tffs_efi_ctx *ctx;
	struct TFFS_EFI_State *state;

	//    pr_err("[%s-%d] Called.\n", __func__, __LINE__);
	ctx   = (struct tffs_efi_ctx *)this->priv;
	state = (struct TFFS_EFI_State *)handle;

	avm_efivar_resource_destroy(state->res);
	kfree(state);

	return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Write(struct tffs_module *this, void *handle, const uint8_t *write_buffer,
			   size_t write_length, size_t *rlen, unsigned int final)
{
#if 1
	struct tffs_efi_ctx *ctx;
	struct TFFS_EFI_State *state;
	int result;

	state = (struct TFFS_EFI_State *)handle;
	if (state->id >= FLASH_FS_ID_LAST) {
		pr_debug("[%s] invalid tffs_id: 0x%x\n", __func__, state->id);
		return -ENOENT;
	}

	// erasing an entry is always  final
	if (write_length == 0 && write_buffer == NULL) {
		final = 1;
	}

	if (final == 0) {
		result = -ENOSPC;
		goto err_out;
	}

	ctx = (struct tffs_efi_ctx *)this->priv;
	if (ctx == NULL) {
		result = -EBADF;
		goto err_out;
	}

	if (write_buffer == NULL) {
		write_length = 0;
	}

	result = avm_efivar_set(state->res, NULL, (uint8_t *)write_buffer, write_length);

	/*
	 * Entry was written. Send notification for ID
	 */
	if (result == 0) {
		send_notification(ctx, state->id,
				  write_buffer ? tffs3_notify_update : tffs3_notify_clear);
	}
#endif

	*rlen = write_length;

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Read(struct tffs_module *this, void *handle, uint8_t *read_buffer,
			  size_t *read_length)
{
	struct tffs_efi_ctx *ctx;
	struct TFFS_EFI_State *state;
	unsigned int result;
	size_t read_len;
	uint8_t *efidata;

	result = 0;

	ctx = (struct tffs_efi_ctx *)this->priv;
	if (ctx == NULL) {
		result = -EBADF;
		goto err_out;
	}

	state = (struct TFFS_EFI_State *)handle;

	/* set up handle state information and fill read buffer on first read */
	if (state->offset == 0) {
		efidata = avm_efivar_get(state->res, NULL, &state->len);
		if (IS_ERR(efidata)) {
			result = PTR_ERR(efidata);
			goto err_out;
		}

		state->efidata = efidata;
		tffs_write_statistic(state->id, state->len, 0, 0);
	}

	if (state->len > state->offset) {
		read_len = min((state->len - state->offset), *read_length);

		memcpy(read_buffer, &state->efidata[state->offset], read_len);
		*read_length = read_len;
		state->offset += read_len;
	} else {
		*read_length = 0;
	}

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Reindex(struct tffs_module *this)
{
	struct tffs_efi_ctx *ctx;
	int result;

	result = 0;
	ctx    = (struct tffs_efi_ctx *)this->priv;
	if (ctx == NULL) {
		result = -EBADF;
		goto err_out;
	}

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Cleanup(struct tffs_module *this, void *handle)
{
	struct tffs_efi_ctx *ctx;
	int result;

	result = 0;
	ctx    = (struct tffs_efi_ctx *)this->priv;
	if (ctx == NULL) {
		pr_err("[%s] No context given.\n", __func__);
		result = -EBADF;
		goto err_out;
	}

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Info(struct tffs_module *this, unsigned int *Fill)
{
	struct tffs_efi_ctx *ctx;

	ctx = (struct tffs_efi_ctx *)this->priv;

	tffs_write_statistic(FLASH_FS_ID_SKIP, sizeof(struct _TFFS_Entry), 0,
			     0); /*--- internal read ---*/

	*Fill = 50;

	return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Register_Notify(struct tffs_module *this, void *notify_priv,
				     tffs3_notify_fn notify_cb)
{
	struct tffs_efi_ctx *ctx;
	int result;

	ctx = (struct tffs_efi_ctx *)this->priv;

	result = 0;
	if (ctx->notify_cb == NULL) {
		ctx->notify_priv = notify_priv;
		ctx->notify_cb	 = notify_cb;
	} else {
		result = -EEXIST;
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Remove_Notify(struct tffs_module *this, void *notify_priv,
				   tffs3_notify_fn notify_cb)
{
	struct tffs_efi_ctx *ctx;
	int result;

	ctx = (struct tffs_efi_ctx *)this->priv;

	result = -EINVAL;
	if (ctx->notify_priv == notify_priv && ctx->notify_cb == notify_cb) {
		ctx->notify_cb	 = NULL;
		ctx->notify_priv = NULL;
		result		 = 0;
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_Setup(struct tffs_module *this)
{
	struct tffs_efi_ctx *ctx;
	int result;

	result = 0;
	if (this == NULL || this->priv == NULL) {
		pr_err("[%s] Module or context information missing!\n", __func__);
		result = -EINVAL;
		goto err_out;
	}
	ctx = (struct tffs_efi_ctx *)this->priv;

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_EFI_Configure(struct tffs_module *this)
{
	struct tffs_efi_ctx *ctx;
	int result;

	result = -EINVAL;
	ctx    = NULL;

	if (!efi_enabled(EFI_BOOT)) {
		pr_err("[%s] EFI services not available\n", __func__);
		result = -ENODEV;
		goto err_out;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		pr_err("[%s] Out of memory error\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	this->name	      = "efi";
	this->setup	      = TFFS3_EFI_Setup;
	this->open	      = TFFS3_EFI_Open;
	this->close	      = TFFS3_EFI_Close;
	this->read	      = TFFS3_EFI_Read;
	this->write	      = TFFS3_EFI_Write;
	this->cleanup	      = TFFS3_EFI_Cleanup;
	this->reindex	      = TFFS3_EFI_Reindex;
	this->info	      = TFFS3_EFI_Info;
	this->register_notify = TFFS3_EFI_Register_Notify;
	this->remove_notify   = TFFS3_EFI_Remove_Notify;

	this->priv = ctx;

	smp_mb();

	result = 0;

err_out:
	if (result != 0 && !IS_ERR_OR_NULL(ctx)) {
		kfree(ctx);
	}

	return result;
}
EXPORT_SYMBOL(TFFS3_EFI_Configure);
