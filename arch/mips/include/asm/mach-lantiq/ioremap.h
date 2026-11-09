/*
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  Copyright (C) 2014 Lei Chuanhua <Chuanhua.lei@lantiq.com>
 *  Copyright (C) 2017 Intel Corporation.
 */
#ifndef __ASM_MACH_LANTIQ_IOREMAP_H
#define __ASM_MACH_LANTIQ_IOREMAP_H

#include <linux/types.h>
#include <asm/mach-lantiq/spaces.h>

extern unsigned long _page_cachable_default;

static inline phys_addr_t fixup_bigphys_addr(phys_addr_t phys_addr,
					     phys_addr_t size)
{
	return phys_addr;
}

#if defined(CONFIG_SOC_GRX500) || defined(CONFIG_SOC_GRX500_BOOTCORE) || defined(CONFIG_SOC_PRX300_BOOTCORE)

/* TOP IO Space definition for SSX7 components /PCIe/ToE/Memcpy
 * physical 0xa0000000 --> virtual 0xe0000000
 */
#ifdef CONFIG_SOC_GRX500
# define GRX500_TOP_IOREMAP_BASE 0xA0000000
# define GRX500_TOP_IOREMAP_SIZE 0x20000000
# define GRX500_TOP_IOREMAP_PHYS_VIRT_OFFSET 0x40000000
#endif

#define GRX500_RAM_IOREMAP_BASE PHYS_OFFSET
#define GRX500_CACHED_RAM_IOREMAP_SIZE (KSEG1 - KSEG0)
#define GRX500_UNCACHED_RAM_IOREMAP_SIZE 0x10000000
#define GRX500_REG_IOREMAP_BASE 0x10000000
#define GRX500_REG_IOREMAP_SIZE 0x10000000
#define GRX500_REG_IOREMAP_PHYS_VIRT_OFFSET 0xA0000000

static inline void __iomem *plat_ioremap(phys_addr_t offset, unsigned long size,
					 unsigned long flags)
{
#ifdef CONFIG_SOC_GRX500
	if ((offset >= GRX500_TOP_IOREMAP_BASE) &&
	    offset < (GRX500_TOP_IOREMAP_BASE + GRX500_TOP_IOREMAP_SIZE) &&
	    flags == _CACHE_UNCACHED)
		return (void __iomem *)(unsigned long)(offset + GRX500_TOP_IOREMAP_PHYS_VIRT_OFFSET);
#endif

	if ((offset >= GRX500_RAM_IOREMAP_BASE) &&
	    offset < (GRX500_RAM_IOREMAP_BASE + GRX500_UNCACHED_RAM_IOREMAP_SIZE) &&
	    flags == _CACHE_UNCACHED)
		return (void __iomem *)(CKSEG1ADDR(offset));

	if ((offset >= GRX500_RAM_IOREMAP_BASE) &&
	    offset < (GRX500_RAM_IOREMAP_BASE + GRX500_CACHED_RAM_IOREMAP_SIZE) &&
	    flags == _page_cachable_default)
		return (void __iomem *)(CKSEG0ADDR(offset));

	if ((offset >= GRX500_REG_IOREMAP_BASE) &&
	    offset < (GRX500_REG_IOREMAP_BASE + GRX500_REG_IOREMAP_SIZE) &&
	    flags == _CACHE_UNCACHED)
		return (void __iomem *)(unsigned long)(offset + GRX500_REG_IOREMAP_PHYS_VIRT_OFFSET);

	return NULL;
}

static inline int plat_iounmap(const volatile void __iomem *addr)
{
	return
#ifdef CONFIG_SOC_GRX500
		((unsigned long)addr >= (unsigned long)(GRX500_TOP_IOREMAP_BASE + GRX500_TOP_IOREMAP_PHYS_VIRT_OFFSET)) ||
#endif
		((unsigned long)addr >= KSEG0 && (unsigned long)addr < KSEG0 + GRX500_CACHED_RAM_IOREMAP_SIZE) ||
		((unsigned long)addr >= KSEG1 && (unsigned long)addr < KSEG1 + GRX500_UNCACHED_RAM_IOREMAP_SIZE) ||
		((unsigned long)addr >= GRX500_REG_IOREMAP_BASE + GRX500_REG_IOREMAP_PHYS_VIRT_OFFSET && (unsigned long)addr < GRX500_REG_IOREMAP_BASE + GRX500_REG_IOREMAP_SIZE + GRX500_REG_IOREMAP_PHYS_VIRT_OFFSET);
}
#else
static inline void __iomem *plat_ioremap(phys_addr_t offset, unsigned long size,
					 unsigned long flags)
{
	return NULL;
}

static inline int plat_iounmap(const volatile void __iomem *addr)
{
	return 0;
}
#endif /* CONFIG_SOC_GRX500 || CONFIG_SOC_GRX500_BOOTCORE || CONFIG_SOC_PRX300_BOOTCORE */
#endif /* __ASM_MACH_LANTIQ_IOREMAP_H */
