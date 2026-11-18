/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 MediaTek Inc.
 */

#ifndef __LINUX_PHY_MTK_XFI_TPHY_H
#define __LINUX_PHY_MTK_XFI_TPHY_H

#include <linux/clk.h>
#include <linux/types.h>

struct device;
struct reset_control;

#define MTK_XFI_TPHY_NUM_CLOCKS		2

/**
 * struct mtk_xfi_tphy - run-time data of the XFI phy instance
 * @base: IO memory area to access phy registers.
 * @dev: Kernel device used to output prefixed debug info.
 * @reset: Reset control corresponding to the phy instance.
 * @clocks: All clocks required for the phy to operate.
 * @da_war: Enables work-around for 10GBase-R mode.
 */
struct mtk_xfi_tphy {
	void __iomem		*base;
	struct device		*dev;
	struct reset_control	*reset;
	struct clk_bulk_data	clocks[MTK_XFI_TPHY_NUM_CLOCKS];
	bool			da_war;
};

#endif /* __LINUX_PHY_MTK_XFI_TPHY_H */
