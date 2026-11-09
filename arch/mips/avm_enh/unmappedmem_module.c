// SPDX-License-Identifier: GPL-2.0+

#include <linux/moduleloader.h>
#include <linux/elf.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/zugriff.h>
#include <linux/spinlock.h>
#include <linux/jump_label.h>
#include <linux/avm_kernel_config.h>
#include <linux/ioport.h>
#include <linux/bootmem.h>
#include <linux/env.h>
#include <linux/simple_proc.h>
#include <linux/proc_fs.h>

#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/avm_enh/avm_enh.h>
#include <linux/avm_kernel_config.h>

static unsigned long module_alloc_size_list_base;
static unsigned int module_alloc_size_list_size;

static DEFINE_SPINLOCK(unmapped_module_lock);

#if !IS_ENABLED(CONFIG_AVM_KERNEL_CONFIG)
static struct _avm_kernel_module_memory_config *avm_kernel_module_memory_config;
#endif


/*--- #define DBG_TRC(args...) pr_err( args) ---*/
#define DBG_TRC(args...)	no_printk(args)
/**
 */
static struct _module_alloc_size_list {
	atomic_t alloc;
	unsigned long size;
	unsigned long addr;
	char name[64];
	enum _module_alloc_type_ type;
} module_alloc_size_list[90];

static unsigned int module_alloc_size_list_elements;
/**
 */
static char *module_load_black_list[] = {
	/*--- "avm_coredump", "flash_update", "kspeedtest", ---*/
	NULL
};

/**
 */
static int module_is_blacklisted(char *name)
{
	char **p;

	p = module_load_black_list;
	while (*p) {
		if (!strcmp(*p, name)) {
			return 1;
		}
		p++;
	}
	return 0;
}
static struct resource module_param;

/**
 */
int is_kernel_module_addr(unsigned long addr)
{
	return (addr >= (unsigned long)__va(module_param.start)) &&
	       (addr < (unsigned long)__va(module_param.end));
}
/**
 */
static unsigned int size_special_page_aligned(unsigned int size)
{
	return ((size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
}

/**
 */
static unsigned int reserved_module_size(struct _avm_kernel_module_memory_config *CMod)
{
#if defined(CONFIG_KALLSYMS)
#if defined(CONFIG_KALLSYMS_ALL)
	return size_special_page_aligned(CMod->core_size + CMod->symbol_size);
#else /*--- #if defined(CONFIG_KALLSYMS_ALL) ---*/
	return size_special_page_aligned(CMod->core_size +
					 CMod->symbol_text_size);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_KALLSYMS_ALL) ---*/
#else /*--- #if defined(CONFIG_KALLSYMS) ---*/
	return size_special_page_aligned(CMod->core_size);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_KALLSYMS) ---*/
}
/*
 * Initiale Vorbereitungen fuer unmapped-memory-bootmem
 * es wird ein extra module_memory reserviert, so dass Treiber direkt in unmapped Speicher
 * geladen werden koennen.
 * Dies wird z.B. unbedingt benoetigt, falls Module die Yield-Schnittstelle verwenden
 * will (es duerfen im Yield-Kontext keine TLB-Exception auftreten!)
 */
void __init module_alloc_bootmem_init(struct resource **res)
{
	unsigned int i, module_size;
	char *arg;

	if (res)
		*res = NULL;

	if (avm_kernel_module_memory_config) {
		struct _avm_kernel_module_memory_config *CMod = avm_kernel_module_memory_config;

		module_alloc_size_list_size = 0;
		while (CMod->name) {
			if (!module_is_blacklisted(CMod->name)) {
				module_size = reserved_module_size(CMod);
				module_alloc_size_list_size += module_size;
				DBG_TRC("%s: module '%s' size=%u\n", __func__,
					CMod->name, module_size);
				module_alloc_size_list_elements++;
			}
			CMod++;
		}
		if (module_alloc_size_list_elements > ARRAY_SIZE(module_alloc_size_list)) {
			panic("FATAL ERROR: reserved modules(%d) exceed module_alloc_size_list-array(%u)\n",
			       module_alloc_size_list_elements, ARRAY_SIZE(module_alloc_size_list));
		}
	} else {
		arg = prom_getenv("modulemem");
		if (arg  != NULL) {
			module_alloc_size_list_size = simple_strtoul(arg, NULL, 0) + (ARRAY_SIZE(module_alloc_size_list) * PAGE_SIZE);
		pr_err("[module-alloc-by-name] warning 'modulemem' used - depreciated\n");
		}
	}
	if (module_alloc_size_list_size == 0UL) {
		pr_err("[module-alloc-by-name] 'modulemem' not set, "
		       "function disabled\n");
		return;
	}

	module_alloc_size_list_base = (unsigned long)alloc_bootmem_pages(module_alloc_size_list_size + (1 << PAGE_SHIFT));
	if (module_alloc_size_list_base == 0L) {
		pr_err("[module-alloc] reserve memory for module-load failed (start 0x%x end 0x%x)\n",
		       module_param.start, module_param.end);
		module_alloc_size_list_size = 0;
		return;
	}
	module_param.start = CPHYSADDR(module_alloc_size_list_base);
	module_param.end = CPHYSADDR(module_alloc_size_list_base +
				     module_alloc_size_list_size);
	module_param.name = "module memory";
	module_param.flags = IORESOURCE_MEM | IORESOURCE_BUSY;

	for (i = 0; i < ARRAY_SIZE(module_alloc_size_list); i++) {
		atomic_set(&module_alloc_size_list[i].alloc, 0);
	}
	if (res)
		*res = &module_param;
}

/**
 */
static int find_module_area_by_addr(unsigned long addr)
{
	unsigned int i;

	if (addr >= module_alloc_size_list_base) {
		/*--- fast decision ---*/
		return -1;
	}
	for (i = 0; i < ARRAY_SIZE(module_alloc_size_list); i++) {

		if (addr < module_alloc_size_list[i].addr) {
			break;
		}
		if (addr >= module_alloc_size_list[i].addr + module_alloc_size_list[i].size) {
			continue;
		}
		return i;
	}
	return -1;
}

/**
 * ret: 0 addr-range freed for re-use
 */
static int module_alloc_size_list_free(unsigned long addr)
{
	int idx;

	spin_lock_bh(&unmapped_module_lock);

	idx = find_module_area_by_addr(addr);
	if ((idx >= 0) && (module_alloc_size_list[idx].addr == addr)) {
		if (atomic_read(&module_alloc_size_list[idx].alloc) == 1) {
			DBG_TRC("[%s] module=%s pointer 0x%lx found in unmapped-module-list -freed for re-use\n",
				__func__,
				module_alloc_size_list[idx].name, addr);
			memset((void *)module_alloc_size_list[idx].addr,
			       0xCC,
			       module_alloc_size_list[idx].size); /*--- destroy contents  ---*/
			atomic_set(&module_alloc_size_list[idx].alloc, 0);
			spin_unlock_bh(&unmapped_module_lock);
			return 0;
		}
	}
	DBG_TRC("[%s] pointer 0x%pS not found in unmapped-memory-module-list\n",
		__func__, (void *)addr);
	spin_unlock_bh(&unmapped_module_lock);
	return -EFAULT;
}

/**
 * @brief get module-alloc-area-infos if addr in range
 * @return start (zero if out of range)
 */
unsigned long get_modulealloc_area(unsigned long addr,
				   char **module_name,
				   unsigned int *allocated,
				   unsigned long *size)
{
	int idx;
	char *name = "";
	unsigned int _allocated = 0;
	unsigned long _size = 0;
	unsigned long _addr = 0;

	idx = find_module_area_by_addr(addr);
	if (idx >= 0) {
		name       = module_alloc_size_list[idx].name;
		_size	   = module_alloc_size_list[idx].size;
		_allocated = atomic_read(&module_alloc_size_list[idx].alloc);
		_addr	   = module_alloc_size_list[idx].addr;

		DBG_TRC("%s: 0x%0lx - is idx=%d [%s] in range %0lx - %0lx allocated=%u\n",
			__func__, addr, idx, name, _addr, _addr + _size, _allocated);
	} else if ((addr >= module_alloc_size_list_base) &&
		   (addr < module_alloc_size_list_base + module_alloc_size_list_size)) {
		/*--- reserved but now never allocated ---*/
		_size	   = module_alloc_size_list_size;
		_addr	   = module_alloc_size_list_base;
	}
	if (module_name)
		*module_name = name;
	if (size)
		*size = _size;
	if (allocated)
		*allocated = _allocated;
	return _addr;
}

static unsigned long module_alloc_get_reserved_size_by_name(const char *name)
{
	// Work around for odd led module naming
	if (strncmp(name, "led", strlen("led")) == 0) {
		name = "led_module";
	}

	if (avm_kernel_module_memory_config) {
		struct _avm_kernel_module_memory_config *CMod = avm_kernel_module_memory_config;

		while (CMod->name) {
			if (!strcmp(CMod->name, name)) {
				return reserved_module_size(CMod);
			}
			CMod++;
		}
	}
	return 0;
}
static int module_alloc_waste;
/**
 */
unsigned long module_alloc_size_list_alloc(unsigned long size, char *name,
					   enum _module_alloc_type_ type)
{
	unsigned long aligned_module_size;
	unsigned int i;

	if ((module_alloc_size_list_size == 0) ||
	    (module_alloc_size_list_base == 0))
		return 0L;

	if (module_is_blacklisted(name)) {
		pr_err("[module-alloc-by-name] module '%s' on black list, use normal alloc\n",
		       name);
		return 0UL;
	}
	DBG_TRC("[%s] next base is 0x%lx, rest size is 0x%x\n", __func__,
		module_alloc_size_list_base, module_alloc_size_list_size);
	size = size_special_page_aligned(size);

	spin_lock_bh(&unmapped_module_lock);
	for (i = 0; i < ARRAY_SIZE(module_alloc_size_list); i++) {
		if (!strcmp(module_alloc_size_list[i].name, name)) { /*--- name gefunden ---*/
			if (module_alloc_size_list[i].type == type) {
				DBG_TRC("[%s] module '%s' addr %lx found\n",
					__func__, name,
					module_alloc_size_list[i].addr);
				if (atomic_read(&module_alloc_size_list[i].alloc)) {
					DBG_TRC("[module-alloc-by-name] module '%s' already allocated\n",
						name);
					spin_unlock_bh(&unmapped_module_lock);
					return 0UL;
				}
				break;
			}
		}
		if (module_alloc_size_list[i].name[0] == '\0') {
			snprintf(module_alloc_size_list[i].name,
				 sizeof(module_alloc_size_list[i].name), "%s",
				 name);
			module_alloc_size_list[i].type = type;
			DBG_TRC("[%s] new module '%s' will use entry %d\n",
				__func__, name, i);
			break;
		}
	}
	if (i == ARRAY_SIZE(module_alloc_size_list)) {
		pr_err("[module-alloc-by-name] module alloc table full\n");
		spin_unlock_bh(&unmapped_module_lock);
		return 0UL;
	}
	/*--- wenn noch keine addresse, dann festlegen ---*/
	if (module_alloc_size_list[i].addr == 0) {
		if (size > module_alloc_size_list_size) {
			module_alloc_size_list[i].name[0] = '\0';
			pr_err("[module-alloc-by-name] no unmapped-space for module '%s' (0x%lx bytes) -> use ksseg\n",
			       name, size);
			spin_unlock_bh(&unmapped_module_lock);
			return 0UL;
		}
		module_alloc_size_list[i].size = size;
		module_alloc_size_list[i].addr = module_alloc_size_list_base;

		module_alloc_size_list_size -= size;
		module_alloc_size_list_base += size;

		aligned_module_size = module_alloc_get_reserved_size_by_name(module_alloc_size_list[i].name);

		pr_err("[module-alloc-by-name] give 0x%lx bytes at 0x%lx to module '%s' (0x%x total bytes left)\n",
		       module_alloc_size_list[i].size,
		       module_alloc_size_list[i].addr,
		       module_alloc_size_list[i].name,
		       module_alloc_size_list_size);
		if (aligned_module_size > size) {
			module_alloc_waste += aligned_module_size - size;
			pr_err("%s: error: module '%s' reserved size %lu is to great for demand size %lu - waste %lu"
			       " (module_alloc_waste=%d)\n",
			       __func__, module_alloc_size_list[i].name,
			       aligned_module_size, size,
			       aligned_module_size - size, module_alloc_waste);
		} else if (size > aligned_module_size) {
			module_alloc_waste -= size - aligned_module_size;
			pr_err("%s: error: module '%s' reserved size %lu too small for demand size %lu - need %lu more"
			       " (module_alloc_waste=%d)\n",
			       __func__, module_alloc_size_list[i].name,
			       aligned_module_size, size,
			       size - aligned_module_size, module_alloc_waste);
		}
	}
	/*--- segment groesse überprüfen ---*/
	if (size > module_alloc_size_list[i].size) {
		pr_err("[module-alloc-by-name] invalid size change 0x%lx bytes < 0x%lx bytes (module '%s')\n",
		       module_alloc_size_list[i].size, size,
		       module_alloc_size_list[i].name);
		spin_unlock_bh(&unmapped_module_lock);
		return 0UL;
	}
	atomic_set(&module_alloc_size_list[i].alloc, 1);
	spin_unlock_bh(&unmapped_module_lock);
	return module_alloc_size_list[i].addr;
}

#if IS_ENABLED(CONFIG_AVM_SAMMEL)
static void module_alloc_proc_allocated(struct seq_file *file, void *ctx)
{
	int i;
	unsigned long allocated = 0;

	seq_printf(file, "%30s %15s %15s\n", "Module", "Allocated", "Reserved");

	for (i = 0; i < ARRAY_SIZE(module_alloc_size_list); i++) {
		struct _module_alloc_size_list *mod = module_alloc_size_list + i;
		unsigned long reserved;

		// End of allocated list
		if (!*mod->name) {
			break;
		}

		allocated += mod->size;

		seq_printf(file, "%30s %15ld", mod->name, mod->size);

		reserved = module_alloc_get_reserved_size_by_name(mod->name);

		if (reserved == 0) {
			seq_printf(file, " %15s", "-");
		} else {
			seq_printf(file, " %15ld", reserved);
		}

		seq_puts(file, "\n");
	}

	seq_printf(file, "  Modules: %2u from %2u\n    Total: %8ld\nAllocated: %8ld\n Reserved: %8d\n",
		   i, module_alloc_size_list_elements,
		   module_alloc_size_list_size + allocated, allocated,
		   module_alloc_size_list_size);
}

static void module_alloc_proc_reserved(struct seq_file *file, void *ctx)
{
	struct _avm_kernel_module_memory_config *mod;

	if (!avm_kernel_module_memory_config) {
		seq_puts(file, "No module memory config found!\n");
		return;
	}

	seq_printf(file, "%30s %15s %15s %15s\n", "Module", "Reserved",
		   "CoreSize", "SymbolSize");

	for (mod = avm_kernel_module_memory_config; mod->name; mod++) {
		seq_printf(file, "%30s %15d %15d %15d\n", mod->name,
			   reserved_module_size(mod), mod->core_size,
#if defined(CONFIG_KALLSYMS_ALL)
			   mod->symbol_size
#else /*--- #if defined(CONFIG_KALLSYMS_ALL) ---*/
			   mod->symbol_text_size
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_KALLSYMS_ALL) ---*/
		);
	}
}

/* this replaces the default module_memfree which is marked as weak
 * Also called for init-memory and blacklisted drivers, so fallback to default-vfree have to exist!
 * context: also soft-irq (rcu_process_callbacks) !!
 */
void module_memfree(void *module_region)
{
	if (module_alloc_size_list_free((unsigned long)module_region)) {
		if (is_vmalloc_addr(module_region))
			vfree(module_region);
	}
}

static __init int module_alloc_proc_init(void)
{
	proc_mkdir("avm/module", NULL);

	add_simple_proc_file("avm/module/allocated", NULL,
			     module_alloc_proc_allocated, NULL);
	add_simple_proc_file("avm/module/reserved", NULL,
			     module_alloc_proc_reserved, NULL);

	return 0;
}
late_initcall(module_alloc_proc_init);
#endif
