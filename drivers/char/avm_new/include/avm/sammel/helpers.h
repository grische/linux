/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_HELPERS_H_
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/ktime.h>
#include <linux/fs.h>

#define LINUX_STABLE_VERSION_HAS_SUBLEVEL(a, b, c) \
	(LINUX_VERSION_CODE < KERNEL_VERSION(a, (b) + 1, 0) && \
	 LINUX_VERSION_CODE >= KERNEL_VERSION(a, b, c))

/* Return:
 *	< 0: Error (-EIO, -EPERM, -EINVAL)
 *	= 0: Empty file
 *	> 0: Number of bytes read
 */
int avm_read_from_file(const char *filename, char *str, int maxlen);

/* Return:
 *	< 0: Error (-EIO, -EPERM, -EINVAL)
 *	= 0: Success
 */
int avm_write_to_file(const char *filename, const char *str, int len);

/*---  CPU-depend use of request_irq ---*/
int request_irq_on(int cpu, unsigned int irq, irq_handler_t handler,
		   unsigned long irqflags, const char *devname, void *dev_id);

/**
 * register_simple_dma_device - Register a simple (dummy) device for DMA
 * @dev:  The device to register
 * @name: Device name
 *
 * Register a simple device that can be used for DMA.  AVM drivers that do not
 * follow the Linux driver model but need to do DMA can hereby acquire a
 * dummy device that satisfies the requirements of the DMA API.
 *
 * There is no parent device or bus that the device is attached to.  The
 * dma_mask of the device ist unlimited.
 *
 * All fields in @dev that have no explicit value must be initialized by the
 * caller to 0.
 */
int register_simple_dma_device(struct device *dev, const char *name);

void unregister_simple_dma_device(struct device *dev);

ssize_t time_to_ascii(time64_t local_time, char *buf, int len);

#define _AVM_HELPERS_H_
#endif /* _AVM_HELPERS_H */
