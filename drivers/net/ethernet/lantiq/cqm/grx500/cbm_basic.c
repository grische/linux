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

/*
 * cbm_counter_mode_set - toggle EQM/DQM counter between PKT and BYTE.
 *
 * Direct port of AVM cqm/grx500/cbm.c:4843-4855 (counter_mode_set). The
 * "qen-toggle dance" is mandatory per silicon spec: the MSEL field can
 * only be updated while EQQCEN/DQQCEN (the counter enable bit) is
 * cleared. The sequence is:
 *
 * 1. Read current MSEL — bail early if it already matches @mode. 2. Clear QEN
 * (freezes counter logic). 3. The qen-toggle dance is still complete without
 * the reset because the silicon constraint is "MSEL must not change while
 * QEN=1" — the counter reset is a datapath cleanup nicety, not a mode-change
 * prerequisite. 4. Write MSEL = @mode (the actual mode swap). 5. Set QEN = 1
 * (re-arm counter logic).
 *
 * @idx: 0 = enqueue (CBM_EQM_CTRL), 1 = dequeue (CBM_DQM_CTRL).
 *
 * @mode: 0 = packet counter, 1 = byte counter.
 */
