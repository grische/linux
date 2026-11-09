// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2025 AVM GmbH <fritzbox_info@avm.de> */

#define pr_fmt(fmt) "[brcm_wdt] " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/types.h>

#include "ar7wdt_private.h"
#include "hw_fiq_generic.h"


struct WDTimer {
	uint32_t        WatchDogDefCount;
	/* Write 0xff00 0x00ff to Start timer
	* Write 0xee00 0x00ee to Stop and re-load default count
	* Read from this register returns current watch dog count
	* */
	uint32_t        WatchDogCtl;

	/* Number of 50-MHz ticks for WD Reset pulse to last */
	uint32_t        WDResetCount;

#define SOFT_RESET              0x00000001
	uint32_t        WDTimerCtl;

	uint32_t        WDAccessCtl;
};

static struct WDTimer *WDTIMER0;

void nonsec_wdt_enable(bool enable)
{
	if (!ar7wdt_hw_initialized())
		return;

	if (enable) {
		writel(0xFF00, &WDTIMER0->WatchDogCtl);
		writel(0x00FF, &WDTIMER0->WatchDogCtl);
	} else {
		writel(0xEE00, &WDTIMER0->WatchDogCtl);
		writel(0x00EE, &WDTIMER0->WatchDogCtl);
	}
}

void nonsec_wdt_reset(void)
{
	if (ar7wdt_hw_initialized()) {
		writel(0xFF00, &WDTIMER0->WatchDogCtl);
		writel(0x00FF, &WDTIMER0->WatchDogCtl);
	}
}

void nonsec_wdt_set_timeout(unsigned int timeout)
{
	if (ar7wdt_hw_initialized()) {
		writel(timeout, &WDTIMER0->WatchDogDefCount);
	}
}

/* used by brcm code */
int bcmbca_wd_start(unsigned int timeout)
{
	return 0;
}
EXPORT_SYMBOL(bcmbca_wd_start);

/* MODULE INFRASTRUCTURE */
static const struct of_device_id wdt_brcm_match_table[] = {
	{ .compatible = "brcm,bcm96xxx-wdt" },
	{}
};
MODULE_DEVICE_TABLE(of, wdt_brcm_match_table);

static int wdt_brcm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *id;
	struct device_node *np = NULL;

	id = of_match_device(wdt_brcm_match_table, dev);
	if (!id)
		goto err_out;


	np = of_find_compatible_node(NULL, NULL,
				     id->compatible);

	if (!np) {
		pr_err("[%s] ERROR dt-entry\n", __func__);
		goto err_out;
	}

	WDTIMER0 = of_iomap(np, 0);
	if (!WDTIMER0) {
		pr_err("Failed to remap watchdog regs");
		goto err_out;
	}


	return generic_wdt_probe(np);

err_out:
	return -ENODEV;
}

void ar7wdt_hw_init(void)
{
	generic_ar7wdt_hw_init();
}

void unregister_early_restart_nb(void)
{
}

static int wdt_brcm_remove(struct platform_device *pdev)
{
	generic_wdt_remove();

	return 0;
}

static struct platform_driver wdt_brcm_driver = {
	.probe = wdt_brcm_probe,
	.remove = wdt_brcm_remove,
	.driver = {
		.name = "qcom,kpss-wdt-ipq",
		.owner = THIS_MODULE,
		.of_match_table = wdt_brcm_match_table,
	},
};

static int __init wdt_brcm_init(void)
{
	return platform_driver_register(&wdt_brcm_driver);
}
core_initcall(wdt_brcm_init);

static void __exit wdt_brcm_exit(void)
{
	platform_driver_unregister(&wdt_brcm_driver);
}
module_exit(wdt_brcm_exit);
