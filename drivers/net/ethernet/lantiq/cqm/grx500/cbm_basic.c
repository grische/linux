// SPDX-License-Identifier: GPL-2.0-only
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.c and cqm/cqm_common.h.
 *
 * CBM low-level register helpers: read-modify-write of an MMIO bit-field,
 * block memset and the EQM/DQM counter-mode selector.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/module.h>

#include "cbm.h"

/* Forward declaration matching the extern in cbm.c. */
void cbm_set_val(void __iomem *addr, u32 val, u32 mask, u32 shift);

/* cbm_dw_memset - 32-bit-aligned wordwise memset over MMIO. */
void cbm_dw_memset(u32 *base, int val, u32 size)
{
	u32 i;

	if (!base)
		return;
	for (i = 0; i < size / sizeof(u32); i++)
		__raw_writel((u32)val, &base[i]);
}
EXPORT_SYMBOL_GPL(cbm_dw_memset);

/* cbm_set_val - read-modify-write of an MMIO bit-field. */
void cbm_set_val(void __iomem *addr, u32 val, u32 mask, u32 shift)
{
	u32 reg;

	if (!addr || !mask)
		return;

	reg  = __raw_readl(addr);
	reg &= ~mask;
	reg |= (val << shift) & mask;
	__raw_writel(reg, addr);
}
EXPORT_SYMBOL_GPL(cbm_set_val);
