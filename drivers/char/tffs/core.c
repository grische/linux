// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004-2014 AVM GmbH <fritzbox_info@avm.de> */
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
#include <linux/list.h>
#include <linux/kconfig.h>
#include <linux/env.h>

#if defined(CONFIG_TFFS_DEV_SERVER) || defined(CONFIG_TFFS_DEV_CLIENT)
#include <avm/event/event.h>
#endif

#include "local.h"

struct tffs3_cfg_funcs cfg_funcs = {
#if defined(CONFIG_TFFS_DEV_LEGACY)
	.legacy = TFFS3_LGCY_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_MTDNAND)
	.mtdnand = TFFS3_NAND_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_CACHE)
	.cache = TFFS3_CACHE_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_EFI)
	.efi = TFFS3_EFI_Configure,
#endif
#if defined(CONFIG_TFFS_EFI_SYNC)
	.efi_sync = TFFS3_EFI_SYNC_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_CLIENT)
	.remote = TFFS3_REMOTE_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_SERVER)
	.server = TFFS3_SERVER_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_MTDNOR)
	.mtdnor = TFFS3_NOR_Configure,
#endif
#if defined(CONFIG_TFFS_DEV_BDEV)
	.bdev = TFFS3_BDEV_Configure,
#endif
};

struct tffs_panic_cb {
	struct mtd_info *mtd;
	panic_setup_cb panic_setup_fn;
	unsigned int setup_done;
	struct list_head panic_cb_list;
};

static struct _tffs_device TFFS_device;

#define PANIC_LOCK_BIT 0
#define PANIC_USE_BIT  1
#define PANIC_LOCK     (1 << PANIC_LOCK_BIT)
#define PANIC_USE      (1 << PANIC_USE_BIT)

static int lock_device(struct _tffs_device *device, enum tffs3_handle_mode mode)
{
	int result;

	if (mode != tffs3_mode_panic) {
		/* normal mode operations need to acquire the outer lock.
		 * This lock implements the waiting queue for non-panic-mode operations
		 */
		result = down_interruptible(&(device->outer_lock));
		if (result != 0)
			goto err_out;

		if (device->panic_mode & PANIC_LOCK) {
			result = -EBUSY;
			goto err_unlock_outer;
		}
	}

	/* the inner lock controls access to the back end.
	 * This one is used to coordinate normal and panic mode accesses,
	 * hence only a down_trylock()
	 */
	result = down_trylock(&(device->inner_lock));
	if (result != 0) {
		result = -EBUSY;
		goto err_unlock_outer;
	}

	if (device->backend_state != tffs3_module_running) {
		result = -ENODEV;
		goto err_unlock_inner;
	}

	return 0;

err_unlock_inner:
	up(&device->inner_lock);
err_unlock_outer:
	if (mode != tffs3_mode_panic)
		up(&device->outer_lock);
err_out:
	return result;
}

static void unlock_device(struct _tffs_device *device, enum tffs3_handle_mode mode)
{
	up(&(device->inner_lock));
	if (mode != tffs3_mode_panic)
		up(&(device->outer_lock));
}

static void __maybe_unused notify_cb(void *priv __maybe_unused, unsigned int id,
				     enum tffs3_notify_event event)
{
	pr_debug("[%s] Called for ID 0x%x event %s\n", __func__, id,
		 event == tffs3_notify_clear  ? "clear" :
		 event == tffs3_notify_update ? "update" :
		 event == tffs3_notify_reinit ? "reinit" :
						      "unknown");
#if IS_ENABLED(CONFIG_TFFS_DEV_SERVER)
	if (TFFS_device.server_state == tffs3_module_running && TFFS_device.server.notify != NULL) {
		TFFS_device.server.notify(TFFS_device.server.priv, id, event);
	}
#endif
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int __init TFFS3_Early_Init(void)
{
	sema_init(&TFFS_device.outer_lock, 1);
	sema_init(&TFFS_device.inner_lock, 1);
	INIT_LIST_HEAD(&TFFS_device.panic_cb_list);
	TFFS_device.panic_mode = 0;

	TFFS_device.initialised = 1;

	pr_debug("[%s] called\n", __func__);

	return 0;
}
arch_initcall(TFFS3_Early_Init);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Register_NAND(struct mtd_info *mtd)
{
	int result;

	pr_debug("[%s] Called\n", __func__);

	if (!TFFS_device.initialised) {
		pr_err("[%s] TFFS_device not initialised\n", __func__);
		return -EAGAIN;
	}

	result = -ENODEV;
	down(&TFFS_device.inner_lock);
	if (TFFS_device.backend_state == tffs3_module_init) {
		if (cfg_funcs.mtdnand) {
			result = cfg_funcs.mtdnand(&TFFS_device.backend, mtd);
			if (result == 0) {
				TFFS_device.backend_state = tffs3_module_configured;
			}
		} else {
			pr_err("[%s] No config function registered for NAND\n", __func__);
		}
	} else {
		pr_err("[%s] Backing device already registered.\n", __func__);
		result = -EINVAL;
	}
	up(&TFFS_device.inner_lock);

	tffs_init();

	return result;
}
EXPORT_SYMBOL(TFFS3_Register_NAND);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Register_LGCY(unsigned int mtd0, unsigned int mtd1)
{
	int result;

	pr_debug("[%s] Called\n", __func__);

	if (!TFFS_device.initialised) {
		pr_err("[%s] TFFS_device not initialised\n", __func__);
		return -EAGAIN;
	}

	result = -ENODEV;
	down(&TFFS_device.inner_lock);
	if (TFFS_device.backend_state == tffs3_module_init) {
		if (cfg_funcs.legacy) {
			result = cfg_funcs.legacy(&TFFS_device.backend, mtd0, mtd1);
			if (result == 0) {
				TFFS_device.backend_state = tffs3_module_configured;
			}
		} else {
			pr_err("[%s] No config function registered for LGCY\n", __func__);
		}
	} else {
		pr_err("[%s] Backing device already registered.\n", __func__);
		result = -EINVAL;
	}
	up(&TFFS_device.inner_lock);

	return result;
}
EXPORT_SYMBOL(TFFS3_Register_LGCY);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Register_EFI(void)
{
	int result;

	pr_debug("[%s] Called\n", __func__);

	if (!TFFS_device.initialised) {
		pr_err("[%s] TFFS_device not initialised\n", __func__);
		return -EAGAIN;
	}

	result = -ENODEV;
	down(&TFFS_device.inner_lock);
	if (TFFS_device.backend_state == tffs3_module_init) {
		if (cfg_funcs.efi) {
			result = cfg_funcs.efi(&TFFS_device.backend);
			if (result == 0) {
				TFFS_device.backend_state = tffs3_module_configured;
			}
		} else {
			pr_err("[%s] No config function registered for EFI\n", __func__);
		}
	} else {
		pr_err("[%s] Backing device already registered.\n", __func__);
		result = -EINVAL;
	}
	up(&TFFS_device.inner_lock);

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Register_REMOTE(unsigned int node_id)
{
	int result;

	pr_debug("[%s] Called\n", __func__);

	if (!TFFS_device.initialised) {
		pr_err("[%s] TFFS_device not initialised\n", __func__);
		return -EAGAIN;
	}

	result = -ENODEV;
	down(&TFFS_device.inner_lock);
	if (TFFS_device.backend_state == tffs3_module_init) {
		if (cfg_funcs.remote) {
			result = cfg_funcs.remote(&TFFS_device.backend, node_id);
			if (result == 0) {
				TFFS_device.backend_state = tffs3_module_configured;
			}
		} else {
			pr_err("[%s] No config function registered for remote access\n", __func__);
		}
	} else {
		pr_err("[%s] Backing device already registered.\n", __func__);
		result = -EINVAL;
	}
	up(&TFFS_device.inner_lock);

	return result;
}
EXPORT_SYMBOL(TFFS3_Register_REMOTE);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Register_SERVER(unsigned int node_id)
{
	int result;

	pr_err("[%s] Called\n", __func__);
	result = 0;

	if (!TFFS_device.initialised) {
		pr_err("[%s] TFFS_device not initialised\n", __func__);
		return -EAGAIN;
	}

	result = -ENODEV;
	down(&TFFS_device.inner_lock);
	if (TFFS_device.server_state == tffs3_module_init) {
		if (cfg_funcs.server) {
			result = cfg_funcs.server(&TFFS_device.server, node_id);
			if (result == 0) {
				TFFS_device.server_state = tffs3_module_configured;
			}
		} else {
			pr_err("[%s] No config function registered for remote server\n", __func__);
		}
	} else {
		pr_err("[%s] Server already registered.\n", __func__);
		result = -EINVAL;
	}
	up(&TFFS_device.inner_lock);

	return result;
}
EXPORT_SYMBOL(TFFS3_Register_SERVER);

int TFFS3_Register_Panic_CB(struct mtd_info *mtd, panic_setup_cb panic_setup_fn)
{
	struct tffs_panic_cb *panic_cb;
	int result = 0;

	pr_info("[%s] registering panic callback for mtd %s\n", __func__, mtd ? mtd->name : "NULL");
	panic_cb = kzalloc(sizeof(*panic_cb), GFP_KERNEL);
	if (panic_cb == NULL) {
		pr_err("[%s] Unable to allocate memory.\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	INIT_LIST_HEAD(&panic_cb->panic_cb_list);
	panic_cb->mtd		 = mtd;
	panic_cb->panic_setup_fn = panic_setup_fn;
	panic_cb->setup_done	 = 0;

	list_add_tail(&(panic_cb->panic_cb_list), &TFFS_device.panic_cb_list);

err_out:
	return result;
}

BLOCKING_NOTIFIER_HEAD(tffs_state_notifier);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int __tffs_init TFFS3_Init(void)
{
	int result;

	result = -ENODEV;

	pr_info("[%s] Called.\n", __func__);

#if defined(CONFIG_TFFS_DEV_LEGACY)
	if (TFFS_device.backend_state == tffs3_module_init) {
		pr_err("[%s] No storage module registered, trying legacy fallback\n", __func__);
		result = TFFS3_Register_LGCY(tffs_mtd[0], tffs_mtd[1]);
	}
#endif

#if defined(CONFIG_TFFS_DEV_EFI)
	if (TFFS_device.backend_state == tffs3_module_init) {
		pr_err("[%s] No storage module registered, trying EFI fallback\n", __func__);
		result = TFFS3_Register_EFI();
	}
#endif

#ifdef CONFIG_TFFS_DEV_SERVER
	TFFS3_Register_SERVER(AVM_EVENT_TFFS_NODE_ATOM);
#endif
#ifdef CONFIG_TFFS_DEV_CLIENT
	TFFS3_Register_REMOTE(AVM_EVENT_TFFS_NODE_ARM);
#endif
	down(&TFFS_device.inner_lock);

#if defined(CONFIG_TFFS_DEV_CACHE_RAMONLY)
	if (TFFS_device.backend_state == tffs3_module_init) {
		TFFS_device.backend_state = tffs3_module_configured;
	}
#endif

	if (TFFS_device.backend_state == tffs3_module_configured) {
#if defined(CONFIG_TFFS_EFI_SYNC)
		result = TFFS3_EFI_SYNC_Configure(&TFFS_device.backend);
		if (result != 0) {
			pr_err("[%s] Configuration of EFI sync module failed with status %d\n",
			       __func__, result);
			goto err_out;
		}
#endif

#if defined(CONFIG_TFFS_DEV_CACHE)
		result = TFFS3_CACHE_Configure(&TFFS_device.cache, &TFFS_device.backend);
		if (result != 0) {
			pr_err("[%s] Configuration of caching module failed with status %d\n",
			       __func__, result);
			goto err_out;
		}
		TFFS_device.cache_state = tffs3_module_configured;
#else
		// caching not enabled, use backend directly instead
		TFFS_device.cache = TFFS_device.backend;
#endif // defined(CONFIG_TFFS_DEV_CACHE)

		if (TFFS_device.cache.setup == NULL) {
			pr_err("[%s] No cache/backend setup function registered.\n", __func__);
			result = -ENODEV;
			goto err_out;
		}

		result = TFFS_device.cache.setup(&TFFS_device.cache);
		if (result != 0) {
			TFFS_device.backend_state = tffs3_module_error;
			TFFS_device.cache_state	  = tffs3_module_error;
			goto err_out;
		}

		TFFS_device.backend_state = tffs3_module_running;
		TFFS_device.cache_state	  = tffs3_module_running;

#if IS_ENABLED(CONFIG_TFFS_DEV_SERVER)
		// backend device is up. Try starting server if configured
		if (TFFS_device.server_state == tffs3_module_configured) {
			if (TFFS_device.server.setup) {
				result = TFFS_device.server.setup(&TFFS_device.server);
				if (result == 0) {
					TFFS_device.server_state = tffs3_module_running;
					if (TFFS_device.server.notify) {
						TFFS_device.cache.register_notify(
							&TFFS_device.cache, NULL, notify_cb);
					}
				} else {
					TFFS_device.server_state = tffs3_module_error;
				}
			} else {
				pr_err("[%s] No server setup function registered.\n", __func__);
			}

			// don't abort if server setup fails
			result = 0;
		}
#endif // CONFIG_TFFS_DEV_SERVER
	} else {
		pr_err("[%s] Backing device either not configured or already set up.\n", __func__);
		result = -EINVAL;
	}

err_out:
	up(&TFFS_device.inner_lock);

	blocking_notifier_call_chain(&tffs_state_notifier, tffs3_module_running, &TFFS_device);

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void TFFS3_Deinit(void)
{
	pr_debug("[%s] called\n", __func__);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
struct tffs_core_handle *TFFS3_Open(enum _tffs_id id, enum tffs3_handle_mode mode)
{
	struct tffs_core_handle *handle;
	int result;

	handle = NULL;

	if (mode == tffs3_mode_panic)
		TFFS3_Panic_Lock();

	result = lock_device(&TFFS_device, mode);
	if (result) {
		if (result == -EINTR)
			result = -ERESTARTSYS;
		goto err_out;
	}

	if (mode == tffs3_mode_panic) {
		/* We are in panic mode, try to grab the special static handle */
		if (test_and_set_bit(PANIC_USE_BIT, &TFFS_device.panic_mode) == 0)
			handle = &TFFS_device.panic_handle;
	} else {
		handle = kmalloc(sizeof(*handle), GFP_KERNEL);
	}
	if (handle == NULL) {
		pr_debug("[%s] getting %shandle failed\n", __func__,
			 (mode == tffs3_mode_panic) ? "panic " : "");
		result = -ENOMEM;
		goto err_unlock;
	}

	memset(handle, 0x0, sizeof(*handle));
	handle->id   = id;
	handle->mode = mode;

	/* We have our handle, now use it to open a handle on the back end device */
	handle->core_priv = TFFS_device.cache.open(&TFFS_device.cache, handle);
	if (IS_ERR_OR_NULL(handle->core_priv)) {
		result = handle->core_priv == NULL ? -ENOMEM : PTR_ERR(handle->core_priv);
		handle->core_priv = NULL;
		goto err_free_handle;
	}

	unlock_device(&TFFS_device, mode);

	return handle;

err_free_handle:
	if (handle == &TFFS_device.panic_handle) {
		smp_mb__before_clear_bit();
		clear_bit(PANIC_USE_BIT, &TFFS_device.panic_mode);
		smp_mb__after_clear_bit();
	} else {
		kfree(handle);
	}

err_unlock:
	unlock_device(&TFFS_device, mode);
err_out:
	return ERR_PTR(result);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Close(struct tffs_core_handle *handle)
{
	int result;

	if (handle == NULL) {
		result = -EBADF;
		goto err_out;
	}

	result = lock_device(&TFFS_device, handle->mode);
	if (result != 0) {
		goto err_out;
	}

	result = TFFS_device.cache.close(&TFFS_device.cache, handle->core_priv);

	unlock_device(&TFFS_device, handle->mode);

	if (result == 0) {
		/* only release handle if backend close was successful. Upper layer
		 * might retry closing.
		 */
		if (handle == &TFFS_device.panic_handle) {
			smp_mb__before_clear_bit();
			clear_bit(PANIC_USE_BIT, &TFFS_device.panic_mode);
			smp_mb__after_clear_bit();
		} else {
			handle->core_priv = NULL;
			kfree(handle);
		}
	}

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Clear(struct tffs_core_handle *handle)
{
	return TFFS3_Write(handle, NULL, 0, 1);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int TFFS3_Werkseinstellungen(struct tffs_core_handle *handle)
{
	struct tffs_core_handle *tmp_handle;
	enum _tffs_id id;
	unsigned int count;
	int result;

	smp_mb();
	if (TFFS_device.panic_mode & PANIC_LOCK) {
		return -EBUSY;
	}

	pr_debug("TFFS3_Werkseinstellungen(%p)\n", handle);
	result = 0;
	count  = 0;
	for (id = FLASH_FS_ID_USER2_LOG; id <= FLASH_FS_ID_FIRMWARE_CONFIG_LAST; id++) {
		tmp_handle = TFFS3_Open(id, tffs3_mode_write);
		if (IS_ERR_OR_NULL(tmp_handle)) {
			result = (tmp_handle == NULL) ? -ENOMEM : PTR_ERR(tmp_handle);
			goto err_out;
		}

		result = TFFS3_Clear(tmp_handle);
		TFFS3_Close(tmp_handle);

		if (result) {
			pr_debug(
				"TFFS3_Werkseinstellungen(%p): clear id 0x%x failed (%u cleared)\n",
				handle, id, count);
			goto err_out;
		}

		++count;
	}

	pr_debug("TFFS3_Werkseinstellungen(%p): success (%u cleared)\n", handle, count);

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Write(struct tffs_core_handle *handle, const uint8_t *write_buffer,
		size_t write_length, unsigned int final)
{
	size_t written;
	int result;

	if (write_buffer == NULL && write_length > 0) {
		result = -EINVAL;
		goto err_out;
	}

	result = lock_device(&TFFS_device, handle->mode);
	if (result != 0) {
		goto err_out;
	}

	result = TFFS_device.cache.write(&TFFS_device.cache, handle->core_priv, write_buffer,
					 write_length, &written, final);

	unlock_device(&TFFS_device, handle->mode);

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Read(struct tffs_core_handle *handle, uint8_t *read_buffer, size_t *read_length)
{
	int result;

	if (read_buffer == NULL || read_length == NULL) {
		result = -EFAULT;
		goto err_out;
	}

	if (*read_length == 0) {
		result = 0;
		goto err_out;
	}

	result = lock_device(&TFFS_device, handle->mode);
	if (result != 0) {
		goto err_out;
	}

	result = TFFS_device.cache.read(&TFFS_device.cache, handle->core_priv, read_buffer,
					read_length);

	unlock_device(&TFFS_device, handle->mode);

err_out:
	return result;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int TFFS3_Cleanup(struct tffs_core_handle *handle)
{
	int result;

	// pr_err("[%s] called\n", __func__);

	result = lock_device(&TFFS_device, handle->mode);
	if (result != 0) {
		goto err_out;
	}

	result = TFFS_device.cache.cleanup(&TFFS_device.cache, handle->core_priv);

	unlock_device(&TFFS_device, handle->mode);

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Create_Index(void)
{
	int result;
	struct tffs_core_handle dummy;

	dummy.core_priv = NULL;
	dummy.id	= 42; // does not matter
	dummy.mode	= tffs3_mode_write;

	result = lock_device(&TFFS_device, dummy.mode);
	if (result != 0) {
		goto err_out;
	}

	result = TFFS_device.cache.reindex(&TFFS_device.cache);

	unlock_device(&TFFS_device, dummy.mode);

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int TFFS3_Info(struct tffs_core_handle *handle, unsigned int *Fill)
{
	int result;

	result = lock_device(&TFFS_device, handle->mode);
	if (result != 0) {
		goto err_out;
	}

	result = TFFS_device.cache.info(&TFFS_device.cache, Fill);

	unlock_device(&TFFS_device, handle->mode);

err_out:
	return result;
}

void TFFS3_Panic_Lock(void)
{
	struct tffs_panic_cb *panic_cb;

	pr_debug("[%s] Called.\n", __func__);

	if (test_and_set_bit(PANIC_LOCK_BIT, &TFFS_device.panic_mode) == 0) {
		list_for_each_entry(panic_cb, &TFFS_device.panic_cb_list, panic_cb_list) {
			if (panic_cb->setup_done == 0) {
				panic_cb->panic_setup_fn(panic_cb->mtd);
				panic_cb->setup_done = 1;
			}
		}
	}
}
