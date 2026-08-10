// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012, 2014, 2015 Lantiq Deutschland GmbH
 * Copyright (c) 2016, 2017 Intel Corporation
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/switch-api/gsw_init.c and gsw_flow_core.h.
 *
 * GSWIP-3.0 switch-API probe: maps both instances' register windows, loads
 * the PCE microcode and publishes the per-instance device.
 */

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/ethtool.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/types.h>

/*
 * gsw_init.h pulls in the full AVM-ported header chain (gsw_flow_core.h ->
 * gsw_flow_ops.h -> lantiq_gsw_api.h -> ...). This is the single canonical
 * entry point for AVM types inside switch-api/.c sources.
 */
#include "gsw_init.h"
#include "gsw_flow_core.h"
#include "../datapath/datapath.h"

/*
 * Counts probe calls; the datapath dp_init_module() fires exactly once when
 * atomic_inc_return reaches 2 (i.e. after BOTH GSW-L (devid=0) and GSW-R
 * (devid=1) have completed gsw_devs_register), because dp_platform_set's
 * sanity check at AVM datapath_misc.c:294-296 requires dp_port_prop[0].ops[0]
 * AND ops[1] both populated. The file-scope static keeps the refcount
 * per-module - there is exactly one .ko, so exactly one atomic_t
 * dp_init_refcnt declaration across the whole module.
 */
static atomic_t dp_init_refcnt = ATOMIC_INIT(0);

static int intel_xrx500_gswip_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	ethsw_api_dev_t *pethdev;
	void __iomem *base;
	u32 devid;
	int ret;

	ret = of_property_read_u32(dev->of_node, "lantiq,gswip-instance",
				   &devid);
	if (ret) {
		dev_err(dev, "missing lantiq,gswip-instance property: %d\n",
			ret);
		return ret;
	}

	if (devid >= LTQ_GSW_DEV_MAX) {
		dev_err(dev, "lantiq,gswip-instance=%u out of range (max %u)\n",
			devid, LTQ_GSW_DEV_MAX);
		return -EINVAL;
	}

	pethdev = devm_kzalloc(dev, sizeof(*pethdev), GFP_KERNEL);
	if (!pethdev)
		return -ENOMEM;

	/*
	 * Passive mapping only. The vendor probe additionally takes the
	 * switch gate clock; that gate is on from reset and no driver in this
	 * tree disables it (see the CGU gate policy), so this probe maps the
	 * register window and nothing else. It performs no soft-reset and
	 * acquires no reset line, matching the vendor probe, which does
	 * neither.
	 */
	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	/*
	 * Populate the per-devid identity fields. devid=0 is the GSW-L
	 * instance at physical 0x1c000000 (alias gswl_base); devid=1 is GSW-R
	 * at physical 0x1a000000 (alias gswr_base).
	 */
	pethdev->gsw_base = (void *)base;
	pethdev->parent_devid = devid;
	pethdev->pdev = (void *)pdev;

	if (devid == 0) {
		pethdev->sdev = LTQ_FLOW_DEV_INT;
		pethdev->gswl_base = (void *)base;
	} else if (devid == 1) {
		pethdev->sdev = LTQ_FLOW_DEV_INT_R;
		pethdev->gswr_base = (void *)base;
	} else {
		/*
		 * The DT-binding limits devid to 0/1 for the FRITZ!Box 7560, so
		 * this branch is unreachable by design and kept only
		 * defensively.
		 */
		pethdev->sdev = (gsw_devtype_t)devid;
	}

	/*
	 * These protect the various functional-area lookaside caches inside
	 * the ethsw_api_dev_t struct (PCE rule table, buffer manager, PMAC,
	 * miscellaneous, PAE, allocator free/alloc lists, IRQ link list, MDIO
	 * bus, MMD).
	 */
	spin_lock_init(&pethdev->lock_pce);
	spin_lock_init(&pethdev->lock_bm);
	spin_lock_init(&pethdev->lock_pmac);
	spin_lock_init(&pethdev->lock_misc);
	spin_lock_init(&pethdev->lock_pae);
	spin_lock_init(&pethdev->lock_alloc);
	spin_lock_init(&pethdev->lock_free);
	spin_lock_init(&pethdev->lock_irq);
	spin_lock_init(&pethdev->lock_mdio);
	spin_lock_init(&pethdev->lock_mmd);

	ret = gsw_devs_register(devid, pethdev);
	if (ret) {
		dev_err(dev, "gsw_devs_register failed: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, pethdev);

	pethdev->gipver = LTQ_GSWIP_3_0; /* GSWIP-3.0-only driver per architectural constraint; not replaced with a cap-read (AVM gsw_flow_core.h LTQ_GSWIP_3_0 = 0x030) */
	pethdev->pnum = 8;  /* TODO:replace with ETHSW_CAP_1_PPORTS read */
	pethdev->tpnum = 16; /* TODO:replace with ETHSW_CAP_1_VPORTS read */
	gsw_pce_attach_tbl_30(pethdev);

	/*
	 * Load the GSWIP-3.0 PCE parser microcode (256 rows, version "212"
	 * per MICRO_CODE_VERSION at gsw_init.h:118). Failure path: dev_err +
	 * idempotent gsw_devs_unregister (gsw_flow_core.h:997 documents the
	 * silently-tolerates-devid-out-of-range contract) + return -EIO so
	 * the platform_driver probe contract is honoured (negative errno on
	 * failure).
	 */
	ret = gsw_pmicro_code_init(pethdev);
	if (ret) {
		dev_err(dev, "gsw_pmicro_code_init failed: %d\n", ret);
		gsw_devs_unregister(devid);
		return -EIO;
	}

	if (atomic_inc_return(&dp_init_refcnt) == 2) {
		int dp_ret = dp_init_module();

		if (dp_ret)
			dev_err(&pdev->dev,
				"gsw_init: dp_init_module failed=%d\n", dp_ret);
	}

	return 0;
}

static void intel_xrx500_gswip_remove(struct platform_device *pdev)
{
	ethsw_api_dev_t *pethdev = platform_get_drvdata(pdev);

	if (pethdev)
		gsw_devs_unregister(pethdev->parent_devid);

	/* devm_* allocations are released automatically. */
}

static const struct of_device_id intel_xrx500_gswip_of_match[] = {
	{ .compatible = "intel,xrx500-gswip-3.0" },
	{ }
};
MODULE_DEVICE_TABLE(of, intel_xrx500_gswip_of_match);

static struct platform_driver intel_xrx500_gswip_driver = {
	.probe = intel_xrx500_gswip_probe,
	.remove = intel_xrx500_gswip_remove,
	.driver = {
		.name = "intel-xrx500-gswip",
		.of_match_table = intel_xrx500_gswip_of_match,
	},
};
module_platform_driver(intel_xrx500_gswip_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grische <github@grische.xyz>");
MODULE_DESCRIPTION("Intel xRX500 (GRX350) GSWIP-3.0 ethernet driver suite");
