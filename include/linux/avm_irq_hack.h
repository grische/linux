/* SPDX-License-Identifier: GPL-2.0 */
#if defined(CONFIG_AVM_IRQ_HACK)
int avm_get_hack_irq(uint32_t idx);
#else
static inline int avm_get_hack_irq(uint32_t idx)
{
	return -ENOTSUP;
}
#endif
