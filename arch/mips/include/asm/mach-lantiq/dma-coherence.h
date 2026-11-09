/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006, 07  Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2014, 10  Chuanhua Lei <chuanhua.lei@lantiq.com>
 *
 */
#ifndef __ASM_MACH_LANTIQ_DMA_COHERENCE_H
#define __ASM_MACH_LANTIQ_DMA_COHERENCE_H

#if defined(CONFIG_SOC_GRX500) || defined(CONFIG_SOC_TYPE_GRX500_TEP)

#if defined(CONFIG_SOC_GRX500_BOOTCORE)
#define MIPS_MACH_LANTIQ_DMA_OFFSET 0x20000000
#elif defined(CONFIG_SOC_PRX300_BOOTCORE)
#define MIPS_MACH_LANTIQ_DMA_OFFSET 0x30000000
#else
#define MIPS_MACH_LANTIQ_DMA_OFFSET 0x0
#endif

#define GRX500_IO_COHERENT_START 0xA0000000
struct device;

static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr,
					  size_t size)
{
#if defined(CONFIG_DMA_NONCOHERENT) || defined(CONFIG_DMA_MAYBE_COHERENT)
	return virt_to_phys(addr) + MIPS_MACH_LANTIQ_DMA_OFFSET;
#else
	return virt_to_phys(addr) + GRX500_IO_COHERENT_START;
#endif /* CONFIG_DMA_NONCOHERENT || CONFIG_DMA_MAYBE_COHERENT */
}

static inline dma_addr_t plat_map_dma_mem_page(struct device *dev,
					       struct page *page)
{
#if defined(CONFIG_DMA_NONCOHERENT) || defined(CONFIG_DMA_MAYBE_COHERENT)
	return page_to_phys(page) + MIPS_MACH_LANTIQ_DMA_OFFSET;
#else
	return page_to_phys(page) + GRX500_IO_COHERENT_START;
#endif /* CONFIG_DMA_NONCOHERENT || CONFIG_DMA_MAYBE_COHERENT */
}

static inline unsigned long plat_dma_addr_to_phys(struct device *dev,
						  dma_addr_t dma_addr)
{
	return dma_addr - MIPS_MACH_LANTIQ_DMA_OFFSET;
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr,
				      size_t size,
				      enum dma_data_direction direction)
{
}

static inline int plat_dma_supported(struct device *dev, u64 mask)
{
	/*
	 * we fall back to GFP_DMA when the mask isn't all 1s,
	 * so we can't guarantee allocations that must be
	 * within a tighter range than GFP_DMA..
	 */
	if (mask < DMA_BIT_MASK(24))
		return 0;

	return 1;
}

static inline int plat_device_is_coherent(const struct device *dev)
{
#ifdef CONFIG_DMA_COHERENT
	return 1;
#else
	switch (coherentio) {
	default:
	case IO_COHERENCE_DEFAULT:
		return hw_coherentio;
	case IO_COHERENCE_ENABLED:
		return 1;
	case IO_COHERENCE_DISABLED:
		return 0;
	}
#endif /* CONFIG_DMA_COHERENT */
}

static inline void plat_post_dma_flush(struct device *dev)
{
}
#else
#include <asm/mach-generic/dma-coherence.h>
#endif /* CONFIG_SOC_GRX500 */

#endif /* __ASM_MACH_LANTIQ_DMA_COHERENCE_H */
