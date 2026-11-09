// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2016-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/rtc.h>
#include <linux/rtc-avm.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>

/**
 */
static void dev_avm_dma_release(struct device *dev)
{
}

/**
 */
static struct platform_device avm_dma_platform_device = {
	.name = "avm_dma",
	.id = 0,
	.num_resources = 0,
	.dev = {
	  .coherent_dma_mask = DMA_BIT_MASK(32),
	  .release = dev_avm_dma_release,
	}
};

struct device *avm_dma_device = &avm_dma_platform_device.dev;
EXPORT_SYMBOL(avm_dma_device);

/**
 */
static int __init avm_device_init(void)
{
	if (platform_device_register(&avm_dma_platform_device)) {
		return -ENODEV;
	}
	return 0;
}

late_initcall(avm_device_init);
