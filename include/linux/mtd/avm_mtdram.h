/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/kernel.h>

#if IS_ENABLED(CONFIG_AVM_KERNEL)
/* Returns true if the given address range [base, base + size) overlaps with
 * and mtdram device.
 */
extern bool avm_mtdram_check_overlap(const char *cmdline, phys_addr_t base,
				     phys_addr_t size);
#else
static inline bool avm_mtdram_check_overlap(const char *cmdline,
					    phys_addr_t base,
					    phys_addr_t size)
{
	return false;
}
#endif
