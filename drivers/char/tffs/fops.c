// SPDX-License-Identifier: GPL-2.0+
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <avm/tffs/tffs.h>
#include <linux/zlib.h>
#include <linux/vmalloc.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include <asm/pgtable.h>
#include <linux/gfp.h>
#include <linux/sizes.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include "local.h"

static struct tffs_fops_handle panic_handle;
static volatile unsigned long panic_handle_use = 0;
#define PANIC_HANDLE_USE_BIT	 0
#define ZBUFFER_START_LEN	 (4 * 1024)
#define STATIC_ZBUFFER_LEN	 ((32 * 1024) + 16)
#define STATIC_ZBUFFER_LEN_CRYPT (STATIC_ZBUFFER_LEN + 4)
#define MAX_IOCTL_DATA_SIZE	 SZ_64K

static unsigned char static_z_Buffer[STATIC_ZBUFFER_LEN_CRYPT];

#define WINDOW_BITS 12
#define MEM_LEVEL 4

#define PANIC_LOG_WRKSPC_SIZE 268000
static char panic_log_workspace[PANIC_LOG_WRKSPC_SIZE];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct tffs_fops_handle {
	struct tffs_core_handle *core_handle;
	unsigned int init_flag;
	unsigned char *z_Buffer;
	unsigned int z_length;
	z_stream stream;
};

static int write_segment(struct tffs_fops_handle *handle, size_t to_write, unsigned int final)
{
	int result;

	to_write = min((size_t)(handle->z_length - handle->stream.avail_out), to_write);

	result = TFFS3_Write(handle->core_handle, handle->z_Buffer, to_write, final);

	if (result == 0) {
		memmove(handle->z_Buffer, handle->z_Buffer + to_write, handle->z_length - to_write);
		handle->stream.next_out -= to_write;
		handle->stream.avail_out += to_write;
	}

	return result;
}

static int grow_zbuffer(struct tffs_fops_handle *handle)
{
	unsigned char *new_buf;
	loff_t offset;
	unsigned int new_len;
	int result;

	/**
	 * Try increasing zbuffer size, unless handle is panic handle or
	 * zbuffer is already beyond maximum segment size
	 */
	result = -ENOMEM;
	if (handle->z_length <= handle->core_handle->max_segment_size &&
	    handle->z_Buffer != static_z_Buffer) {
		offset = handle->stream.next_out - handle->z_Buffer;

		new_len = handle->z_length + ZBUFFER_START_LEN;

		new_buf = krealloc(handle->z_Buffer, new_len, GFP_KERNEL);
		if (new_buf != NULL) {
			pr_debug("[%s] id: %02x %p/%d -> %p/%d\n", __func__, handle->core_handle->id,
			       handle->z_Buffer, handle->z_length, new_buf, new_len);
			handle->stream.next_out = new_buf + offset;
			handle->stream.avail_out += (new_len - handle->z_length);
			handle->z_length = new_len;
			handle->z_Buffer = new_buf;

			result = 0;
		}
	}

	return result;
}

static inline size_t flush_threshold(struct tffs_fops_handle *handle)
{
	size_t flush_thrsh;

	/* in panic mode we need to flush as soon as the static z_buffer
	 * gets exhausted. In normal mode we only flush if we are sure
	 * that there will be another segment to write
	 */
	if (handle->z_Buffer == static_z_Buffer) {
		flush_thrsh = STATIC_ZBUFFER_LEN;
	} else {
		flush_thrsh = handle->core_handle->max_segment_size + 1;
	}

	return flush_thrsh;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
#define __vmalloc(size, flags) __vmalloc((size), (flags), PAGE_KERNEL)
#endif

struct tffs_fops_handle *tffs_open_kern(unsigned int id, unsigned int wr_mode)
{
	struct tffs_fops_handle *handle;
	int result, status;
	int workspacesize;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (handle == NULL) {
		pr_err("[%s:%s] Out of memory error.\n", MODULE_NAME, __func__);
		result = -ENOMEM;
		goto err_out;
	}

	handle->core_handle = TFFS3_Open(id, wr_mode ? tffs3_mode_write : tffs3_mode_read);
	if (IS_ERR_OR_NULL(handle->core_handle)) {
		pr_debug("[%s] TFFS3_Open failed\n", __func__);
		result = (handle->core_handle == NULL) ? -ENOMEM : PTR_ERR(handle->core_handle);
		goto err_out;
	}

	/*--------------------------------------------------------------------------------------*\
	\*--------------------------------------------------------------------------------------*/

	result			 = 0;
	handle->z_Buffer	 = NULL;
	handle->stream.workspace = NULL;

	/*
	 * minor 0 is only for ioctl, no read or write support
	 */
	if (id != 0) {
		/* we start out with a small zbuffer for (de)compression and grow
		 * it if necessary.  */
		handle->z_length =
			min_t(size_t, ZBUFFER_START_LEN, handle->core_handle->max_segment_size);

		// z_Buffer must be DMA-able
		handle->z_Buffer = kmalloc(handle->z_length, GFP_KERNEL);
		if (handle->z_Buffer == NULL) {
			pr_err("[%s] no memory for z_buffer, requested 0x%zx bytes\n", __func__,
			       handle->core_handle->max_segment_size);
			result = -ENOMEM;
			goto err_out;
		}

		/*----------------------------------------------------------------------------------*\
		\*----------------------------------------------------------------------------------*/
		memset(&(handle->stream), 0x0, sizeof(handle->stream));
		handle->stream.data_type = Z_ASCII;
		if (wr_mode) {
			workspacesize = max(zlib_deflate_workspacesize(WINDOW_BITS, MEM_LEVEL),
					    zlib_inflate_workspacesize());
			handle->stream.next_out	 = handle->z_Buffer;
			handle->stream.avail_out = handle->z_length;
			handle->stream.total_out = 0;
		} else {
			workspacesize		= zlib_inflate_workspacesize();
			handle->stream.next_in	= handle->z_Buffer;
			handle->stream.avail_in = 0;
			handle->stream.total_in = 0;
		}
		handle->stream.workspace = __vmalloc(workspacesize, GFP_KERNEL | __GFP_HIGHMEM
							| __GFP_NORETRY);
		if (handle->stream.workspace == NULL) {
			pr_err("[%s] no memory for %s workspace\n", __func__,
			       wr_mode ? "write" : "read");
			result = -ENOMEM;
			goto err_out;
		}

		if (wr_mode) {
			status = zlib_deflateInit2(&handle->stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, WINDOW_BITS,
						    MEM_LEVEL, Z_DEFAULT_STRATEGY);
		} else {
			status = zlib_inflateInit(&handle->stream);
		}
		if (status != Z_OK) {
			pr_err("[%s] zlib_deflateInit failed, status = %d\n", __func__, status);
			result = -EIO;
			goto err_out;
		}

		handle->init_flag = 1;
		/*----------------------------------------------------------------------------------*\
		\*----------------------------------------------------------------------------------*/
	}

	pr_debug("[%s] TFFS3_Open success wr_mode=0x%x\n", __func__, wr_mode);
	return handle;

err_out:
	if (handle != NULL) {
		if (handle->z_Buffer) {
			kfree(handle->z_Buffer);
		}

		if (handle->stream.workspace) {
			vfree(handle->stream.workspace);
		}

		if (!IS_ERR_OR_NULL(handle->core_handle)) {
			TFFS3_Close(handle->core_handle);
		}

		kfree(handle);
	}

	return ERR_PTR(result);
}

struct tffs_fops_handle *tffs_open_panic(void)
{
	struct tffs_fops_handle *handle;
	int status;
	int workspacesize;

	// pr_err("[%s] Called. panic_handle_use: %lu\n", __func__, panic_handle_use);

	handle = NULL;

	if (test_and_set_bit(PANIC_HANDLE_USE_BIT, &panic_handle_use)) {
		pr_err("[%s] panic handle already used\n", __func__);
		goto err_out;
	}

	handle		    = &panic_handle;
	handle->core_handle = TFFS3_Open(_FLASH_FS_ID_PANIC_LOG, tffs3_mode_panic);
	if (IS_ERR_OR_NULL(handle->core_handle)) {
		pr_debug("[%s]TFFS3_Open failed\n", __func__);
		goto err_out;
	}

	/*--------------------------------------------------------------------------------------*\
	\*--------------------------------------------------------------------------------------*/
	handle->stream.data_type = Z_ASCII;
	handle->stream.workspace = NULL;
	handle->z_length	 = STATIC_ZBUFFER_LEN;
	handle->z_Buffer	 = static_z_Buffer;

	pr_debug("[%s] z_length: 0x%x\n", __func__, handle->z_length);

	memset(&(handle->stream), 0x0, sizeof(handle->stream));
	workspacesize = zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL);
	if (workspacesize > PANIC_LOG_WRKSPC_SIZE) {
		pr_err("[%s] workspace buffer too small (0x%x) for zlib workspace (0x%x)\n",
		       __func__, PANIC_LOG_WRKSPC_SIZE, workspacesize);
		goto err_out;
	}

	handle->stream.next_out	 = handle->z_Buffer;
	handle->stream.avail_out = handle->z_length;
	handle->stream.total_out = 0;
	handle->stream.workspace = panic_log_workspace;

	status = zlib_deflateInit(&handle->stream, Z_DEFAULT_COMPRESSION);
	if (status != Z_OK) {
		pr_err("[%s] zlib_deflateInit failed, status = %d\n", __func__, status);
		goto err_out;
	}

	handle->init_flag = 1;

	return handle;

err_out:
	if (handle != NULL) {
		if (!IS_ERR_OR_NULL(handle->core_handle)) {
			TFFS3_Close(handle->core_handle);
		}
		smp_mb__before_clear_bit();
		clear_bit(PANIC_HANDLE_USE_BIT, &panic_handle_use);
		smp_mb__after_clear_bit();
	}

	return NULL;
}

static int tffs_open(struct inode *inode, struct file *filp)
{
	struct tffs_fops_handle *handle;
	int minor = MINOR(inode->i_rdev);

	pr_debug("[%s] MAJOR %u MINOR %u\n", __func__, MAJOR(inode->i_rdev), MINOR(inode->i_rdev));

	if (filp->private_data) {
		TFFS3_Close(filp->private_data);
		filp->private_data = NULL;
	}

	if (filp->f_flags & O_APPEND) {
		pr_debug("[%s]: TFFS3_Open O_APPEND not supported\n", __func__);
		return -EINVAL;
	}

	if (filp->f_flags & O_RDWR) {
		pr_debug("[%s] opening O_RDONLY and O_WRONLY simultaneously not supported\n",
			 __func__);
		return -EINVAL;
	}

	handle = tffs_open_kern(minor, (filp->f_flags & O_WRONLY));

	if (IS_ERR_OR_NULL(handle)) {
		pr_debug("[%s] TFFS3_Open failed\n", __func__);
		return (handle == NULL) ? -ENOMEM : PTR_ERR(handle);
	}

	filp->private_data = handle;
	return nonseekable_open(inode, filp);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int tffs_flush(struct file *filp, fl_owner_t id)
{
	int status, result;
	struct tffs_fops_handle *handle;
	size_t to_write;

	status = 0;
	result = 0;
	handle = (struct tffs_fops_handle *)filp->private_data;
	if (!handle) {
		return -EINVAL;
	}

	if (handle->core_handle->id && handle->core_handle->mode != tffs3_mode_read) {
		pr_debug(
		     "{%s} Z_SYNC_FLUSH: zlib_deflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u): status = %d\n",
		     __func__, (int)handle->stream.total_in, handle->stream.next_in,
		     (int)handle->stream.avail_in, (int)handle->stream.total_out,
		     handle->stream.next_out, (int)handle->stream.avail_out, status);

		/* try to process all uncompressed input */
		do {
			status = zlib_deflate(&handle->stream, Z_SYNC_FLUSH);

			if (status == Z_BUF_ERROR) {
				/* Z_BUF_ERROR could mean no more input available or no
				 * space left to put compressed output. Or both.
				 * If we run out of output space, first check if we can write
				 * out a full segment worth of data. */
				to_write = handle->z_length - handle->stream.avail_out;
				if (to_write >= flush_threshold(handle)) {
					result = write_segment(
						handle, handle->core_handle->max_segment_size, 0);

					if (result != 0) {
						goto err_out;
					}
				}

				/* if there is still no space in output buffer, try growing
				 * it  */
				if (handle->stream.avail_out == 0) {
					result = grow_zbuffer(handle);

					if (result != 0) {
						goto err_out;
					}
				}
			}
		} while (status == Z_BUF_ERROR && handle->stream.avail_in > 0);

		if (status != Z_OK && status != Z_BUF_ERROR) {
			result = -ENOSPC;
		}
	}

err_out:
	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int tffs_release_kern(struct tffs_fops_handle *handle)
{
	int status, result;
	size_t to_write;

	if (IS_ERR_OR_NULL(handle) || IS_ERR_OR_NULL(handle->core_handle)) {
		return -EBADF;
	}

	status = 0;
	result = 0;
	if (handle->z_Buffer != NULL) {
		if (handle->core_handle->mode != tffs3_mode_read) {
			handle->stream.next_in	= NULL;
			handle->stream.avail_in = 0;
			do {
				status = zlib_deflate(&(handle->stream), Z_FINISH);

				to_write = handle->z_length - handle->stream.avail_out;
				while (to_write >= flush_threshold(handle)) {
					/* zbuffer holds enough data for writing a non-final
					 * entry segment */
					result = write_segment(
						handle, handle->core_handle->max_segment_size, 0);
					if (result != 0) {
						goto err_out;
					}

					to_write = handle->z_length - handle->stream.avail_out;
				}

				/* if buffer is still full, it was too small to hold a complete
				 * entry segment. Try growing it */
				if (status == Z_BUF_ERROR && handle->stream.avail_out == 0) {
					result = grow_zbuffer(handle);
					if (result != 0) {
						goto err_out;
					}
				}
			} while (status == Z_OK || status == Z_BUF_ERROR);

			if (status == Z_STREAM_END) {
				/* all input data has been compressed and put into the output
				 * buffer. Write final segment */
				zlib_deflateEnd(&handle->stream);
				result = write_segment(
					handle, handle->z_length - handle->stream.avail_out, 1);
			} else {
				/* some error occured during compression. */
				result = -EIO;
			}
		} else {
			zlib_inflateEnd(&handle->stream);
		}
	}

err_out:
	if (result != 0) {
		pr_debug("[%s] Close error %d on ID 0x%x\n", __func__, result,
			 handle->core_handle->id);
	}

	if (handle->core_handle->mode != tffs3_mode_panic) {
		if (handle->stream.workspace) {
			vfree(handle->stream.workspace);
			handle->stream.workspace = NULL;
		}

		if (handle->z_Buffer) {
			kfree(handle->z_Buffer);
			handle->z_Buffer = NULL;
		}
	}

	status		    = TFFS3_Close(handle->core_handle);
	handle->core_handle = NULL;

	if (handle == &panic_handle) {
		smp_mb__before_clear_bit();
		clear_bit(PANIC_HANDLE_USE_BIT, &panic_handle_use);
		smp_mb__after_clear_bit();
	} else {
		kfree(handle);
	}

	// give preference to zlib errors
	return (result != 0) ? result : status;
}

static int tffs_release(struct inode *inode, struct file *filp)
{
	struct tffs_fops_handle *handle;
	int result;

	handle		   = (struct tffs_fops_handle *)filp->private_data;
	filp->private_data = NULL;

	if (IS_ERR_OR_NULL(handle)) {
		return -EBADF;
	}

	result = tffs_release_kern(handle);

	return result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
ssize_t tffs_read_kern(struct tffs_fops_handle *handle, char *read_buffer, size_t max_read_length,
		       loff_t *offp)
{
	size_t this_read, total_read;
	int result;
	unsigned int status;

	result	   = 0;
	total_read = 0;

	if (IS_ERR_OR_NULL(handle) || IS_ERR_OR_NULL(handle->core_handle) ||
	    handle->core_handle->mode != tffs3_mode_read) {
		result = -EBADF;
		goto err_out;
	}

	if (handle->core_handle->id == 0) {
		pr_debug("%s: no read supported on minor 0\n", __func__);
		result = -EOPNOTSUPP;
		goto err_out;
	}

	handle->stream.next_out	 = read_buffer;
	handle->stream.avail_out = max_read_length;
	do {
		if (handle->stream.avail_in == 0) {
			this_read = handle->z_length;
			result	  = TFFS3_Read(handle->core_handle, handle->z_Buffer, &this_read);
			if (result == 0) {
				handle->stream.avail_in = this_read;
				handle->stream.next_in	= handle->z_Buffer;
			} else {
				// if there is no data entry for this ID in flash, we pretend it was empty
				if (result == -ENOENT) {
					result = 0;
				}
				goto err_out;
			}
		}

		status = zlib_inflate(&handle->stream, Z_SYNC_FLUSH);
		switch (status) {
		case Z_STREAM_END:
			zlib_inflateEnd(&handle->stream);
			// no break
		case Z_OK:
		case Z_BUF_ERROR:
			break;
		default:
			if (handle->stream.msg) {
				pr_err("[%s] id 0x%x msg = %s\n", __func__, handle->core_handle->id,
				       handle->stream.msg);
			}
			result = -EIO;
			goto err_out;
			break;
		}

	} while (status == Z_OK && handle->stream.avail_out > 0);

	total_read = max_read_length - handle->stream.avail_out;
	*offp += total_read;

err_out:
	return (result == 0) ? total_read : result;
}
static ssize_t tffs_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *filp = iocb->ki_filp;
	size_t max_read_length = iov_iter_count(to);
	loff_t *offp = &iocb->ki_pos;
	ssize_t total_read;
	int result;
	struct tffs_fops_handle *handle;
	char *local_buff;

	result	   = 0;
	total_read = 0;
	handle	   = (struct tffs_fops_handle *)filp->private_data;
	local_buff = NULL;

	if (IS_ERR_OR_NULL(handle) || IS_ERR_OR_NULL(handle->core_handle) ||
	    handle->core_handle->mode != tffs3_mode_read) {
		result = -EBADF;
		goto err_out;
	}

	local_buff = vmalloc(max_read_length);
	if (local_buff == NULL) {
		pr_debug("%s: vmalloc(%zu) failed\n", __func__, max_read_length);
		result = -ENOMEM;
		goto err_out;
	}

	total_read = tffs_read_kern(handle, local_buff, max_read_length, offp);
	if (total_read < 0) {
		result = total_read;
		goto err_out;
	}

	if (total_read == 0)
		goto err_out;

	if (!copy_to_iter(local_buff, total_read, to)) {
		result = -EFAULT;
		goto err_out;
	}

err_out:
	if (local_buff != NULL) {
		vfree(local_buff);
	}

	return (result == 0) ? total_read : result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
ssize_t tffs_write_kern(struct tffs_fops_handle *handle, const char *write_buffer,
			size_t write_length, loff_t *offp)
{
	unsigned int status;
	int result;
	ssize_t written;
	size_t to_write;

	pr_debug("%s: %s(%zu bytes)\n", MODULE_NAME, __func__, write_length);

	result	= 0;
	written = 0;

	if (IS_ERR_OR_NULL(handle) || IS_ERR_OR_NULL(handle->core_handle) ||
	    (handle->core_handle->mode != tffs3_mode_write &&
	     handle->core_handle->mode != tffs3_mode_panic)) {
		result = -EBADF;
		goto err_out;
	}

	if (write_length == 0) {
		result = -EINVAL;
		goto err_out;
	}

	if (handle->core_handle->id == 0) {
		pr_debug("%s: no write supported on minor 0\n", __func__);
		result = -EOPNOTSUPP;
		goto err_out;
	}

	handle->stream.next_in	= write_buffer;
	handle->stream.avail_in = write_length;
	written			= 0;
	do {
		status = zlib_deflate(&(handle->stream), Z_NO_FLUSH);
		if (status == Z_OK || status == Z_BUF_ERROR) {
			written	 = write_length - handle->stream.avail_in;
			to_write = handle->z_length - handle->stream.avail_out;

			/* The final segment will be written out on release of the file
			 * handle. To make sure that the final segment is not empty, we
			 * only write out data if we have buffered more than
			 * flush_threshold bytes */
			if (to_write >= flush_threshold(handle)) {
				result = write_segment(handle,
						       handle->core_handle->max_segment_size, 0);

				/* data has been removed from zbuffer, so there is free
				 * space again */
				status = Z_OK;
			}

			/* zbuffer is full but not enough data to fill whole segment.
			 * Try growing buffer */
			if (status == Z_BUF_ERROR) {
				result = grow_zbuffer(handle);
			}
		} else {
			if (handle->stream.msg) {
				pr_err("[%s] msg = %s\n", __func__, handle->stream.msg);
			}
			result = -EIO;
		}
	} while (result == 0 && handle->stream.avail_in > 0);

err_out:
	*offp += written;

	return (result == 0) ? written : result;
}

static ssize_t tffs_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *filp = iocb->ki_filp;
	size_t write_length = iov_iter_count(from);
	loff_t *offp = &iocb->ki_pos;
	int result;
	ssize_t written;
	struct tffs_fops_handle *handle = (struct tffs_fops_handle *)filp->private_data;
	char *local_write_buffer;

	pr_debug("%s: %s(%zu bytes)\n", MODULE_NAME, __func__, write_length);

	result		   = 0;
	written		   = 0;
	local_write_buffer = NULL;

	if (write_length == 0) {
		return 0;
	}

	local_write_buffer = vmalloc(write_length);
	if (local_write_buffer == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	if (!copy_from_iter(local_write_buffer, write_length, from)) {
		result = -EFAULT;
		goto err_out;
	}

	written = tffs_write_kern(handle, local_write_buffer, write_length, offp);

err_out:
	if (local_write_buffer != NULL) {
		vfree(local_write_buffer);
	}

	return (result == 0) ? written : result;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int tffs_ioctl(struct inode *inode, struct file *filp, unsigned int ioctl_param,
		      unsigned long argv)
{
	struct _tffs_cmd tffs_cmd;
	unsigned char *buffer;
	unsigned int ioc_nr;
	struct tffs_fops_handle *fops_handle;
	struct tffs_core_handle *core_handle, *tmp_handle;
	int result;

	ioc_nr	    = _IOC_NR(ioctl_param);
	fops_handle = (struct tffs_fops_handle *)filp->private_data;

	pr_debug("%s: %s: type %u number %u size %u\n", MODULE_NAME, __func__,
	     _IOC_TYPE(ioctl_param), ioc_nr, _IOC_SIZE(ioctl_param));

	if (IS_ERR_OR_NULL(fops_handle) || IS_ERR_OR_NULL(fops_handle->core_handle)) {
		return -EBADF;
	}

	if (fops_handle->core_handle->id != 0) {
		no_printk(KERN_WARNING "%s: %s: ioctl not supported by minor != 0\n", MODULE_NAME, __func__);
		return -EOPNOTSUPP;
	}

	if (argv == 0) {
		pr_debug("%s: %s: no data pointer for cmd number %u\n", MODULE_NAME, __func__,
			 ioc_nr);
		return -EINVAL;
	}

	if (copy_from_user(&tffs_cmd, (const void *)argv, sizeof(tffs_cmd))) {
		pr_err("%s: %s: copy_from_user failed\n", MODULE_NAME, __func__);
		return -EFAULT;
	}

	buffer		= NULL;
	tmp_handle	= NULL;
	core_handle     = fops_handle->core_handle;
	tffs_cmd.status = 0;
	result		= 0;

	switch (ioc_nr) {
	case _TFFS_READ_ID:
		pr_debug("%s: %s: read id\n", MODULE_NAME, __func__);
		if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
			pr_err("%s: %s: read failed: flags=O_WRONLY\n", MODULE_NAME, __func__);
			result = -EINVAL;
			goto err_out;
		}

		if (tffs_cmd.size > MAX_IOCTL_DATA_SIZE) {
			result = -EINVAL;
			goto err_out;
		}

		buffer = kmalloc(tffs_cmd.size, GFP_KERNEL);
		if (buffer == NULL) {
			pr_err("[%s]: %s: alloc(%zu) failed\n", MODULE_NAME, __func__,
			       tffs_cmd.size);
			result = -ENOMEM;
			goto err_out;
		}

		pr_debug("%s: %s: read \n", MODULE_NAME, __func__);

		tmp_handle = TFFS3_Open(tffs_cmd.id, tffs3_mode_read);
		if (IS_ERR_OR_NULL(tmp_handle)) {
			pr_err("[%s] _TFFS_READ_ID: TFFS3_Open() failed\n", __func__);
			result = (tmp_handle == NULL) ? -ENOMEM : PTR_ERR(tmp_handle);
			goto err_out;
		}

		tffs_cmd.status = TFFS3_Read(tmp_handle, buffer, &tffs_cmd.size);
		if (tffs_cmd.status != 0) {
			result = tffs_cmd.status;
			goto err_out;
		}

		if (copy_to_user((void *)tffs_cmd.buffer, buffer, (unsigned long)tffs_cmd.size)) {
			pr_err("%s: %s: copy_to_user failed\n", MODULE_NAME, __func__);
			result = -EFAULT;
			goto err_out;
		}
		break;

	case _TFFS_WRITE_ID:
		pr_debug("%s: %s: write id\n", MODULE_NAME, __func__);
		if ((filp->f_flags & O_ACCMODE) == O_RDONLY) {
			pr_err("%s: %s: write failed: flags=O_RDONLY\n", MODULE_NAME, __func__);
			result = -EINVAL;
			goto err_out;
		}

		if (tffs_cmd.size > MAX_IOCTL_DATA_SIZE) {
			result = -EINVAL;
			goto err_out;
		}

		tmp_handle = TFFS3_Open(tffs_cmd.id, tffs3_mode_write);
		if (IS_ERR_OR_NULL(tmp_handle)) {
			pr_err("[%s] _TFFS_WRITE_ID: TFFS3_Open() failed\n", __func__);
			result = (tmp_handle == NULL) ? -ENOMEM : PTR_ERR(tmp_handle);
			goto err_out;
		}

		buffer = kmalloc(tffs_cmd.size, GFP_KERNEL);
		if (!buffer) {
			pr_err("%s: %s: alloc(%zu) failed\n", MODULE_NAME, __func__, tffs_cmd.size);
			result = -ENOMEM;
			goto err_out;
		}

		if (copy_from_user(buffer, (void *)tffs_cmd.buffer, (unsigned long)tffs_cmd.size)) {
			pr_err("%s: %s: copy_from_user failed\n", MODULE_NAME, __func__);
			result = -EFAULT;
			goto err_out;
		}
		tffs_cmd.status = TFFS3_Write(tmp_handle, buffer, tffs_cmd.size, 1);

		break;
	case _TFFS_WERKSEINSTELLUNG:
		pr_debug("%s: %s: format\n", MODULE_NAME, __func__);
		tffs_cmd.status = TFFS3_Werkseinstellungen(core_handle);

		break;
	case _TFFS_CLEAR_ID:
		pr_debug("%s: %s: clear id\n", MODULE_NAME, __func__);

		tmp_handle = TFFS3_Open(tffs_cmd.id, tffs3_mode_write);
		if (IS_ERR_OR_NULL(tmp_handle)) {
			pr_err("[%s] _TFFS_CLEAR_ID: TFFS3_Open() failed\n", __func__);
			result = (tmp_handle == NULL) ? -ENOMEM : PTR_ERR(tmp_handle);
			goto err_out;
		}

		tffs_cmd.status = TFFS3_Clear(tmp_handle);

		break;
	case _TFFS_CLEANUP:
		pr_debug("%s: %s: cleanup\n", MODULE_NAME, __func__);
		tffs_cmd.status = TFFS3_Cleanup(core_handle);

		break;
	case _TFFS_REINDEX:
		pr_debug("%s: %s: reindex\n", MODULE_NAME, __func__);
		tffs_cmd.status = TFFS3_Create_Index();

		break;
	case _TFFS_INFO:
		pr_debug("%s: %s: info\n", MODULE_NAME, __func__);
		tffs_cmd.status = TFFS3_Info(core_handle, &tffs_cmd.id);

		break;
	default:
		pr_err("%s: %s: unknwon\n", MODULE_NAME, __func__);
		result = -EINVAL;
		goto err_out;
	}

	if (copy_to_user((void *)argv, &tffs_cmd, sizeof(tffs_cmd))) {
		pr_err("%s: %s: copy_to_user failed\n", MODULE_NAME, __func__);
		tffs_cmd.status = -EFAULT;
	}
	result = tffs_cmd.status;

err_out:
	if (!IS_ERR_OR_NULL(tmp_handle)) {
		TFFS3_Close(tmp_handle);
	}

	if (buffer != NULL) {
		kfree(buffer);
	}

	return result;
}

static long tffs_unlocked_ioctl(struct file *filp, unsigned int ioctl_param,
				unsigned long argv)
{
	return tffs_ioctl(filp->f_path.dentry->d_inode, filp, ioctl_param, argv);
}

const struct file_operations tffs_fops = {
	.owner   = THIS_MODULE,
	.open    = tffs_open,
	.flush   = tffs_flush,
	.release = tffs_release,
	.read_iter = tffs_read_iter,
	.write_iter = tffs_write_iter,
	.unlocked_ioctl = tffs_unlocked_ioctl,
};
