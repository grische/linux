/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/module.h>
#include <linux/moduleloader.h>

#ifndef __AVM_ENH_MODULE_MEM_H__
#define __AVM_ENH_MODULE_MEM_H__

enum avm_module_mem_type {
	avm_module_mem_type_core,
	avm_module_mem_type_init,
};

#if IS_ENABLED(CONFIG_AVM_MODULE_MEM)

extern __init void avm_module_mem_init(void);
extern void *avm_module_mem_alloc(struct module *mod, unsigned long size,
				  enum avm_module_mem_type type);
extern void avm_module_mem_free(void *ptr);
extern int avm_module_mem_contains(void *ptr);

extern char *module_alloc_find_module_name(char *buff, char *end,
					   unsigned long addr);

extern unsigned long get_modulealloc_area(unsigned long addr,
					  char **module_name,
					  unsigned int *allocated,
					  unsigned long *size);

extern bool avm_module_is_allowed(const char *name);
#else

static inline __init void avm_module_mem_init(void)
{
}

static inline void *avm_module_mem_alloc(struct module *mod, unsigned long size,
					 enum avm_module_mem_type type)
{
	return module_alloc(size);
}

static inline void avm_module_mem_free(void *ptr)
{
	module_memfree(ptr);
}

static inline int avm_module_mem_contains(void *ptr)
{
	return 0;
}

static inline char *module_alloc_find_module_name(char *buff, char *end,
						  unsigned long addr)
{
	return buff + snprintf(buff, end - buff, "0x%08lx", addr);
}

static inline unsigned long get_modulealloc_area(unsigned long addr,
						 char **module_name,
						 unsigned int *allocated,
						 unsigned long *size)
{
	return 0;
}

static inline bool avm_module_is_allowed(const char *name)
{
	return false;
}

#endif

#if IS_ENABLED(CONFIG_AVM_MODULE_MEM) && \
	IS_ENABLED(CONFIG_CHECK_TIMER_ON_FREED_MODULE)

extern int module_alloc_check_pointer(unsigned long addr, char **name);

#else

static inline int module_alloc_check_pointer(unsigned long addr, char **name)
{
	return 1;
}

#endif

static inline int is_kernel_module_addr(unsigned long addr)
{
	return avm_module_mem_contains((void *)addr);
}

#endif
