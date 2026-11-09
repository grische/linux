// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2019-2022 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <avm/tffs/tffs.h>
#include <linux/efi.h>
#include <linux/env.h>

#include "local.h"

struct tffs_sync_ctx {
	struct tffs_module *backend;
	void *notify_priv;
	tffs3_notify_fn notify_cb;
};

struct efi_sync_work {
	struct work_struct work;
	unsigned int id;
	enum tffs3_notify_event event;
};

static void tffs_efi_sync_work_func(struct work_struct *work)
{
	char val[URLADER_ENV_VALUE_MAX] = "";
	struct efi_sync_work *es_work;
	const char *var;
	size_t len;
	int result;

	result	= 0;
	len     = 0;
	var	= NULL;
	es_work = container_of(work, struct efi_sync_work, work);

	pr_debug("[%s] Called for ID 0x%x event %s\n", __func__, es_work->id,
		 es_work->event == tffs3_notify_clear  ? "clear" :
		 es_work->event == tffs3_notify_update ? "update" :
		 es_work->event == tffs3_notify_reinit ? "reinit" :
							       "unknown");

	/* Lookup idx in nametable first. */
	var = avm_urlader_getenv_name(es_work->id);
	pr_debug("[%s] env_get_variable=%s\n", __func__, var ? var : "(NULL)");
	if (var == NULL) {
		goto err_out;
	}

	if (es_work->event == tffs3_notify_update) {
		/* We don't know what was written/updated here so we need to read it
		 * from TFFS again.
		 */
		result = avm_urlader_getenv_by_id(es_work->id, val, sizeof(val));
		if (result)
			goto err_out;
		len = strlen(val) + 1;
	}

	if (es_work->event == tffs3_notify_update || es_work->event == tffs3_notify_clear) {
		pr_debug("[%s] sync id %x, var=%s, val=%s\n", __func__, es_work->id, var, val);

		result = avm_efivar_set_one(var, val, len);
		if (result != 0) {
			pr_err("[%s] EFI sync failed for \"%s\"=\"%s\"; result=%d\n", __func__, var,
			       val, result);
		}
	}

err_out:
	kfree(es_work);
}

static void TFFS3_EFI_SYNC_Notify(void *priv, unsigned int id, enum tffs3_notify_event event)
{
	struct tffs_sync_ctx *ctx;
	struct efi_sync_work *es_work;

	ctx = (struct tffs_sync_ctx *)priv;
	BUG_ON(ctx == NULL);

	pr_debug("[%s] Called for ID 0x%x event %s\n", __func__, id,
		 event == tffs3_notify_clear  ? "clear" :
		 event == tffs3_notify_update ? "update" :
		 event == tffs3_notify_reinit ? "reinit" :
						      "unknown");

	if (id <= FLASH_FS_ID_FIRMWARE_CONFIG_LAST || id > FLASH_FS_NAME_TABLE) {
		goto err_out;
	}

	es_work = kzalloc(sizeof(*es_work), GFP_KERNEL);
	if (es_work == NULL) {
		pr_err("[%s] Out of memory allocating work struct.\n", __func__);
		goto err_out;
	}

	/* Do this later to avoid possible recursion/locking problems
	 * when we need to call into TFFS again.
	 */
	es_work->id    = id;
	es_work->event = event;
	INIT_WORK(&es_work->work, tffs_efi_sync_work_func);
	schedule_work(&es_work->work);

err_out:
	pr_debug("[%s] sending notifications\n", __func__);

	/* Send notification upwards. */
	if (ctx->notify_cb != NULL) {
		pr_debug("[%s] sending notification upwards\n", __func__);
		ctx->notify_cb(ctx->notify_priv, id, event);
	}
	pr_debug("[%s] done\n", __func__);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int TFFS3_EFI_SYNC_Register_Notify(struct tffs_module *this, void *notify_priv,
					  tffs3_notify_fn notify_cb)
{
	struct tffs_sync_ctx *ctx;
	int result;

	ctx = (struct tffs_sync_ctx *)this->priv;

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
static int TFFS3_EFI_SYNC_Remove_Notify(struct tffs_module *this, void *notify_priv,
					tffs3_notify_fn notify_cb)
{
	struct tffs_sync_ctx *ctx;
	int result;

	ctx = (struct tffs_sync_ctx *)this->priv;

	result = -EINVAL;
	if (ctx->notify_priv == notify_priv && ctx->notify_cb == notify_cb) {
		ctx->notify_cb	 = NULL;
		ctx->notify_priv = NULL;
		result		 = 0;
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
 * Wrapper functions that call the real back end's interface.
\*-----------------------------------------------------------------------------------------------*/
static void *TFFS3_EFI_SYNC_Open(struct tffs_module *this, struct tffs_core_handle *handle)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return NULL;
	}

	return ctx->backend->open(ctx->backend, handle);
}

static int TFFS3_EFI_SYNC_Close(struct tffs_module *this, void *handle)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return -ENODEV;
	}

	return ctx->backend->close(ctx->backend, handle);
}

static int TFFS3_EFI_SYNC_Read(struct tffs_module *this, void *handle, uint8_t *read_buffer,
			       size_t *read_length)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return -ENODEV;
	}

	return ctx->backend->read(ctx->backend, handle, read_buffer, read_length);
}

static int TFFS3_EFI_SYNC_Write(struct tffs_module *this, void *handle, const uint8_t *data_buf,
				size_t data_len, size_t *retlen, unsigned int final)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return -ENODEV;
	}

	return ctx->backend->write(ctx->backend, handle, data_buf, data_len, retlen, final);
}

static int TFFS3_EFI_SYNC_Cleanup(struct tffs_module *this, void *handle)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return -ENODEV;
	}

	return ctx->backend->cleanup(ctx->backend, handle);
}

static int TFFS3_EFI_SYNC_Reindex(struct tffs_module *this)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return -ENODEV;
	}

	return ctx->backend->reindex(ctx->backend);
}

static int TFFS3_EFI_SYNC_Info(struct tffs_module *this, unsigned int *Fill)
{
	struct tffs_sync_ctx *ctx;

	pr_debug("[%s] Called\n", __func__);

	ctx = (struct tffs_sync_ctx *)this->priv;
	if (ctx == NULL) {
		pr_debug("[%s] Context not initialised\n", __func__);
		return -ENODEV;
	}

	return ctx->backend->info(ctx->backend, Fill);
}

static int TFFS3_EFI_SYNC_Setup(struct tffs_module *this)
{
	struct tffs_sync_ctx *ctx;
	int result;

	pr_info("[TFFS3-EFI-SYNC] Sync TFFS to EFI Vars\n");
	BUG_ON(this == NULL);

	result = -EINVAL;

	ctx = (struct tffs_sync_ctx *)this->priv;
	BUG_ON(ctx == NULL);

	if (ctx->backend == NULL) {
		pr_emerg("[%s] No backend configured!\n", __func__);
		result = -ENODEV;
		goto err_out;
	}

	result = ctx->backend->setup(ctx->backend);
	if (result != 0) {
		pr_err("[TFFS3-EFI-SYNC] Setup of backend failed\n");
		goto err_out;
	}

	/* Register notification CB _after_ setup, so we don't get swamped *\
	\* during the initial scan.                                        */

	result = ctx->backend->register_notify(ctx->backend, ctx, TFFS3_EFI_SYNC_Notify);
	if (result != 0) {
		pr_err("[TFFS3-EFI-SYNC] Registering notification call-back failed\n");
		goto err_out;
	}

	return 0;

err_out:
	if (result != 0 && ctx != NULL) {
		kfree(ctx);
	}

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_EFI_SYNC_Configure(struct tffs_module *this)
{
	struct tffs_module *backend;
	struct tffs_sync_ctx *ctx;
	unsigned int dummy;
	int result;

	pr_debug("[%s] Called\n", __func__);

	BUG_ON(this == NULL);

	result	= -EINVAL;
	backend = NULL;
	ctx	= NULL;

	if (!efi_enabled(EFI_BOOT)) {
		pr_err("[%s] EFI services not available\n", __func__);
		result = -ENODEV;
		goto err_out;
	}

	/* Check for TFFS marker set by bootloader. If this is found, the *\
	\* eMMC TFFS is handled there directly, so no sync is needed.     */
	result = avm_efivar_get_one("AVM_EVA_TFFS", (uint8_t *)&dummy, sizeof(dummy));
	if (result >= 0) {
		pr_info("[%s] Eva TFFS marker found, EFI sync not necessary.\n", __func__);
		result = 0;
		goto err_out;
	}

	/* Create a copy of the backend module`s interface */
	backend = kmemdup(this, sizeof(*backend), GFP_KERNEL);
	if (backend == NULL) {
		pr_err("[%s] Out of memory error\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL) {
		pr_err("[%s] Out of memory error\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	if (backend->register_notify == NULL) {
		pr_warn("[%s] Backend %s does not support notifications, sync disabled\n", __func__,
			backend->name);
		result = -EINVAL;
		goto err_out;
	}

	/* If sync is go, replace the back end functions with our wrappers. */
	pr_info("[%s] Configuring EFI sync for backend %s\n", __func__, backend->name);
	ctx->backend = backend;
	this->priv   = ctx;

	this->name	      = "efi_sync";
	this->open	      = TFFS3_EFI_SYNC_Open;
	this->close	      = TFFS3_EFI_SYNC_Close;
	this->read	      = TFFS3_EFI_SYNC_Read;
	this->write	      = TFFS3_EFI_SYNC_Write;
	this->cleanup	      = TFFS3_EFI_SYNC_Cleanup;
	this->reindex	      = TFFS3_EFI_SYNC_Reindex;
	this->info	      = TFFS3_EFI_SYNC_Info;
	this->setup	      = TFFS3_EFI_SYNC_Setup;
	this->register_notify = TFFS3_EFI_SYNC_Register_Notify;
	this->remove_notify   = TFFS3_EFI_SYNC_Remove_Notify;

	result = 0;

err_out:
	if (result != 0) {
		if (backend != NULL) {
			kfree(backend);
		}

		if (ctx != NULL) {
			kfree(ctx);
		}
	}

	return result;
}
