/* SPDX-License-Identifier: GPL-2.0+ */

/**
 * fuer Verwendung im NonLinux-Kontext (Yield/FastIrq)
 */
#ifndef __simple_mempool_h__
#define __simple_mempool_h__

#if defined(CONFIG_AVM_IPI_YIELD) || defined(CONFIG_AVM_FASTIRQ)
void *simplemempool_alloc_init(char *pool_name, unsigned int pool_size);
void simplemempool_alloc_exit(void *pmp);

void *simplemempool_alloc(void *_pmp, unsigned int size, int zero,
			  void *caller);
void simplemempool_free(void *memhandle, const void *ptr, void *caller);
int simplemempool_size(void *_pmp, const void *ptr);

/*
 * @brief get simplemempool-area if addr in range
 * @param  flag 1 freed, 2 range is mempool-ctrl, 3 range is outside demand-size
 * @return start (zero if not exist)
 */
unsigned long get_simplemempool_area(unsigned long addr, unsigned long *caller,
				     char *mempool_name,
				     unsigned int mempool_name_size,
				     unsigned long *size, int *flag);
/*--- #define MY_TESTMEMPOOL ---*/
#if defined(MY_TESTMEMPOOL)
/**
 */
void *my_kmalloc(unsigned int size, int flag, unsigned long caller);
int my_kfree(const void *ptr, unsigned long caller);
int my_ksize(const void *ptr);
void *my_krealloc(const void *p, size_t new_size, gfp_t flags,
		  unsigned long caller);
#endif /*--- #if defined(MY_TESTMEMPOOL) ---*/

#endif /*--- #if defined(CONFIG_AVM_IPI_YIELD) || defined(CONFIG_AVM_FASTIRQ) ---*/

#endif /*--- #ifndef __simple_mempool_h__ ---*/
