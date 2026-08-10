// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * TMU bring-up entry point.
 */

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/errno.h>

#include "../cqm/grx500/cbm.h"
#include "drv_tmu_ll.h"

/*
 * File-scope externs into drv_tmu_ll.c.
 *
 * tmu_init_done is defined in drv_tmu_ll.c with EXTERNAL LINKAGE (no static
 * qualifier).
 */
extern atomic_t tmu_init_done;
extern int __tmu_ll_init(void);

int tmu_init(void)
{
	int ret;

	if (!g_cbm_tmu_base) {
		pr_err("tmu: init failed, base=NULL\n");
		return -EIO;
	}

	ret = __tmu_ll_init();
	if (ret) {
		pr_err("tmu: __tmu_ll_init failed: %d\n", ret);
		return ret;
	}

	/*
	 * M0 milestone marker — fires after EPOC is zeroed and after
	 * tmu_enable(true). The atomic_set BELOW the pr_info preserves the
	 * milestone-proof ordering: the marker emits the timestamp BEFORE the
	 * ordering guard arms, so the dmesg evidence matches the moment EPOC
	 * was actually zeroed.
	 */
	pr_info("tmu: probe complete, EPOC zeroed at t=%lu\n", jiffies);
	atomic_set(&tmu_init_done, 1);
	return 0;
}
