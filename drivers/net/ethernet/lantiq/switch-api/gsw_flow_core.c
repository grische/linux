// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012, 2014, 2015 Lantiq Deutschland GmbH
 * Copyright (c) 2016, 2017 Intel Corporation
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/switch-api/gsw_flow_core.c.
 *
 * GSWIP-3.0 register accessors and the per-instance device slot table.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "gsw_init.h"
#include "gsw_flow_core.h"

/*
 * Slot table for per-devid device pointers. Writers (gsw_devs_register and
 * gsw_devs_unregister) take gsw_devs_lock; readers (gsw_get_swcore_ops and
 * the register accessors) operate lock-free on the slot pointer because
 * pointer load/store is atomic on every architecture this driver targets
 * and the slot table outlives all consumers (driver unload would tear down
 * consumers before the slot pointers are cleared).
 */
static ethsw_api_dev_t *gsw_devs[LTQ_GSW_DEV_MAX];
static DEFINE_MUTEX(gsw_devs_lock);

/**
 * gsw_devs_register - install per-instance ethsw_api_dev_t in the slot table.
 *
 * @devid: the lantiq,gswip-instance value (must be < LTQ_GSW_DEV_MAX).
 *
 * @dev:   the devm-allocated per-instance state.
 */
int gsw_devs_register(u32 devid, ethsw_api_dev_t *dev)
{
	if (devid >= LTQ_GSW_DEV_MAX)
		return -EINVAL;

	if (!dev)
		return -EINVAL;

	mutex_lock(&gsw_devs_lock);
	if (gsw_devs[devid]) {
		pr_warn("gsw_devs_register: slot devid=%u already occupied\n",
			devid);
		mutex_unlock(&gsw_devs_lock);
		return -EEXIST;
	}
	gsw_devs[devid] = dev;
	mutex_unlock(&gsw_devs_lock);

	return 0;
}

/**
 * gsw_devs_unregister - clear the slot-table entry for @devid.
 *
 * @devid: the lantiq,gswip-instance value.
 */
void gsw_devs_unregister(u32 devid)
{
	if (devid >= LTQ_GSW_DEV_MAX)
		return;

	mutex_lock(&gsw_devs_lock);
	gsw_devs[devid] = NULL;
	mutex_unlock(&gsw_devs_lock);
}

/**
 * gsw_get_swcore_ops - return the core_ops vtable for @devid.
 *
 * @devid: the lantiq,gswip-instance value.
 */
struct core_ops *gsw_get_swcore_ops(u32 devid)
{
	ethsw_api_dev_t *dev;

	if (devid >= LTQ_GSW_DEV_MAX)
		return NULL;

	dev = gsw_devs[devid];
	if (!dev)
		return NULL;

	return &dev->ops;
}
EXPORT_SYMBOL_NS_GPL(gsw_get_swcore_ops, "GSWIP_INTERNAL");

/**
 * gsw_r32_raw - read a 32-bit register at word-offset @offset.
 *
 * @cdev:   ethsw_api_dev_t pointer carrying the ioremap'd gsw_base.
 *
 * @offset: word-offset into the per-instance register window (NOT byte).
 *
 * @value:  out-parameter for the read value.
 */
void gsw_r32_raw(void *cdev, short offset, u32 *value)
{
	ethsw_api_dev_t *pethdev = (ethsw_api_dev_t *)cdev;

	if (!pethdev || !pethdev->gsw_base) {
		pr_err_once("gsw_r32_raw: NULL device or unmapped gsw_base (offset=0x%x)\n",
			    (unsigned int)(unsigned short)offset);
		return;
	}

	*value = readl((void __iomem *)pethdev->gsw_base + (offset * 4));
}
EXPORT_SYMBOL_NS_GPL(gsw_r32_raw, "GSWIP_INTERNAL");

/**
 * gsw_w32_raw - write a 32-bit register at word-offset @offset.
 *
 * @cdev:   ethsw_api_dev_t pointer carrying the ioremap'd gsw_base.
 *
 * @offset: word-offset into the per-instance register window.
 *
 * @value:  value to write.
 */
void gsw_w32_raw(void *cdev, short offset, u32 value)
{
	ethsw_api_dev_t *pethdev = (ethsw_api_dev_t *)cdev;

	if (!pethdev || !pethdev->gsw_base) {
		pr_err_once("gsw_w32_raw: NULL device or unmapped gsw_base (offset=0x%x)\n",
			    (unsigned int)(unsigned short)offset);
		return;
	}

	writel(value, (void __iomem *)pethdev->gsw_base + (offset * 4));
}
EXPORT_SYMBOL_NS_GPL(gsw_w32_raw, "GSWIP_INTERNAL");

/**
 * gsw_r32 - read a bitfield from a 32-bit register.
 * @cdev:   ethsw_api_dev_t pointer.
 * @offset: word-offset.
 * @shift:  bitfield LSB shift inside the 32-bit word.
 * @size:   bitfield width in bits.
 * @value:  out-parameter for the extracted field value.
 *
 * Layers field-extraction on top of gsw_r32_raw via the static-inline helper
 * gsw_field_r32 declared in gsw_init.h (verbatim from AVM gsw_init.h:193-196).
 */
void gsw_r32(void *cdev, short offset, short shift, short size, u32 *value)
{
	u32 rval = 0;

	gsw_r32_raw(cdev, offset, &rval);
	*value = gsw_field_r32(rval, shift, size);
}
EXPORT_SYMBOL_NS_GPL(gsw_r32, "GSWIP_INTERNAL");

/**
 * gsw_w32 - write a bitfield into a 32-bit register (read-modify-write).
 * @cdev:   ethsw_api_dev_t pointer.
 * @offset: word-offset.
 * @shift:  bitfield LSB shift inside the 32-bit word.
 * @size:   bitfield width in bits.
 * @value:  value to write into the field (masked to @size bits).
 *
 * Read-modify-write via gsw_r32_raw + gsw_field_w32 (verbatim from AVM
 * gsw_init.h:198-205) + gsw_w32_raw.
 */
void gsw_w32(void *cdev, short offset, short shift, short size, u32 value)
{
	u32 rval = 0;

	gsw_r32_raw(cdev, offset, &rval);
	rval = gsw_field_w32(rval, shift, size, value);
	gsw_w32_raw(cdev, offset, rval);
}
EXPORT_SYMBOL_NS_GPL(gsw_w32, "GSWIP_INTERNAL");
