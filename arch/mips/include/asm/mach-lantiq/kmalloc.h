#ifndef __ASM_MACH_LANTIQ_KMALLOC_H
#define __ASM_MACH_LANTIQ_KMALLOC_H


#ifndef CONFIG_DMA_COHERENT
/*
 * Internal bus-system is based on 64 bye
 */
#define ARCH_DMA_MINALIGN	64
#endif

#endif /* __ASM_MACH_LANTIQ_KMALLOC_H */
