// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "[module-mem] " fmt

#include <linux/module.h>
#include <linux/moduleloader.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/ioport.h>
#include <linux/env.h>
#include <linux/version.h>
#include <linux/kmemleak.h>
#include <linux/random.h>

#include <asm-generic/sections.h>
#include <asm/pgtable.h>

#if IS_ENABLED(CONFIG_ARM)
#include <asm/mach/map.h>
#endif

/*
 *  Choose an early bootmem allocator to use.
 *
 *  In theory, all allocators being available on a system should work.  This
 *  is true for systems where nobootmem is available, where both the boomem
 *  and the memblock API should work.  It's also true for kernels >= 4.20,
 *  where only memblock is available, and on legacy bootmem-only platforms.
 *
 *  For archs where both proper memblock and proper bootmem are available, the
 *  arch setup code has to keep the two in sync.  One arch falling into this
 *  category is MIPS, before it was converted to nobootmen.
 *
 *  Looking at the mainline history of this, I think we better do not trust
 *  the arch code on such platforms, and use memblock in the Sammeltreiber
 *  only if nobootmem is available, or with kernels >= 4.20 where bootmem has
 *  been removed entirely.
 */
#define HAVE_BOOTMEM \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0))
#define USE_MEMBLOCK \
	(!HAVE_BOOTMEM || IS_ENABLED(CONFIG_NO_BOOTMEM))

#if HAVE_BOOTMEM
#  include <linux/bootmem.h>
#endif

#include <avm/sammel/simple_proc.h>

#include <avm/enh/fw_info.h>
#include <avm/enh/module_mem.h>

/*
 * Module mem basically consists of five main parts.
 *
 * 1) The early memory reservation code.
 *
 *    It tries to steal the required memory from the system
 *    in an very early boot phase. This mostly works on phys addresses.
 *
 * 2) The module_mem-Allocator
 *
 *    This is a simple sbrk-style allocator which does not support freeing.
 *    It is used to allocate space reserved at boot into module_spaces
 *
 * 3) The module_space-Allocator
 *
 *    The module_space allocator divides the module memory into spaces for
 *    each module. It allows freeing and reusing with some restrictions.
 *    The main restriction is that a reused space can not grow.
 *
 * 4) The module_mem_proc-Interface
 *
 *    The module_mem_proc interface provides a simple proc interface to read
 *    out various information stored in avm_module_mem.
 *
 * 5) The avm_module_mem-API
 *
 *    This is the public api exposed to the rest of the system. It will access
 *    the other three as needed and provides an consistent view to the outside.
 *
 */

static struct resource mem_res;

static void *module_mem_free_next;
static size_t module_mem_free_size;

struct module_space {
	atomic_t alloc;
	/*
	 * This flag indicates that the following fields are valid and will not
	 * change.
	 *
	 * This flag will only be set once upon the first allocation of this
	 * space. It will never be cleared.
	 *
	 * This way this struct can be read safely without locks when valid=1.
	 */
	atomic_t valid;

	size_t size;
	void *base;
	char name[64];
	enum avm_module_mem_type type;
};

static struct module_space *module_spaces;
static unsigned int module_spaces_count;
static int module_alloc_waste;

/*
 * Use this to serialize alloc/free operations, not needed for read-only access
 */
static DEFINE_SPINLOCK(module_mem_lock);

/*
 * Strict Mode
 *
 * To catch bugs early module_mem supports a strict mode to fail on problems
 * that otherwise would be hard to detect.
 *
 * Mode   req > reserved      req > free
 *   0        warn          warn, fallback
 *   1        warn             fail
 *   2        fail             fail
 *
 * The default mode will be 0 except when a whitelist is used, where it will be set to 1.
 */
static int strict_mode;
module_param(strict_mode, int, 0644);

static bool module_mem_has_whitelist(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "avm,module-mem");

	if (!node)
		return false;

	return of_property_count_strings(node, "whitelist") > 0;
}

bool avm_module_is_allowed(const char *name)
{
	struct device_node *node;
	int count_whitelist, count_blacklist, i;

	node = of_find_compatible_node(NULL, NULL, "avm,module-mem");

	/*
	 * No module mem configuration, so default to putting all
	 * modules into the module mem
	 */
	if (!node)
		return true;

	count_whitelist = of_property_count_strings(node, "whitelist");
	count_blacklist = of_property_count_strings(node, "blacklist");

	if (count_whitelist > 0 && count_blacklist > 0) {
		pr_warn("Whitelist and blacklist specified. Ignoring whitelist.\n");
		count_whitelist = 0;
	}

	for (i = 0; i < count_blacklist; i++) {
		const char *other;

		if (of_property_read_string_index(node, "blacklist", i,
						  &other)) {
			pr_warn("Could not read blacklist entry %i\n", i);
			continue;
		}

		/* Found, so deny  */
		if (strcmp(name, other) == 0)
			return false;
	}

	for (i = 0; i < count_whitelist; i++) {
		const char *other;

		if (of_property_read_string_index(node, "whitelist", i,
						  &other)) {
			pr_warn("Could not read whitelist entry %i\n", i);
			continue;
		}

		/* Found, so allow */
		if (strcmp(name, other) == 0)
			return true;
	}

	/*
	 * When using a blacklist, default to true
	 */
	return count_blacklist > 0;
}

static size_t
reserved_module_size(struct _avm_kernel_module_memory_config *CMod)
{
	if (IS_ENABLED(CONFIG_KALLSYMS_ALL))
		return ALIGN(CMod->core_size + CMod->symbol_size, PAGE_SIZE);
	else if (IS_ENABLED(CONFIG_KALLSYMS))
		return ALIGN(CMod->core_size + CMod->symbol_text_size,
			     PAGE_SIZE);

	return ALIGN(CMod->core_size, PAGE_SIZE);
}

static unsigned long module_reserved_size(const char *name)
{
	struct _avm_kernel_module_memory_config *mod = avm_fw_module_sizes();

	if (!mod)
		return 0;

	for (; mod->name; mod++) {
		if (!strcmp(mod->name, name)) {
			return reserved_module_size(mod);
		}
	}

	return 0;
}

static bool avm_module_mem_available(void)
{
	return mem_res.flags & IORESOURCE_BUSY;
}

/*
 * Module Mem-Allocator
 *
 * This is a very simple sbrk-style alloc-only allocator.
 */
static void *module_mem_alloc(size_t size)
{
	void *ptr;

	if (size > module_mem_free_size)
		return NULL;

	ptr = module_mem_free_next;
	module_mem_free_next += size;
	module_mem_free_size -= size;

	return ptr;
}

/*
 * The early memory reservation code.
 *
 * This code tries to steal memory during early boot from the linux system
 */

#if IS_ENABLED(CONFIG_ARM)

#define START_ALIGN PMD_SIZE
#define END_ALIGN PAGE_SIZE

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
static __init pmd_t *pmd_off_k(unsigned long virt)
{
	return pmd_offset(pud_offset(pgd_offset_k(virt), virt), virt);
}
#endif

static __init void mark_space_executable(void *_start, unsigned long size)
{
	struct map_desc map;
	unsigned long addr, start = (unsigned long)_start;
	unsigned long remap_start, remap_end;

	/*
	 * Memory will be mapped as SECTION and will span multiple PTE, so we
	 * need to redo the whole PMD.
	 */
	remap_start = round_down(start, PMD_SIZE);
	remap_end = round_up(start + size, PMD_SIZE);

	/* We refuse to remap before the space, as this may be kernel code */
	BUG_ON(remap_start != start);

	/*
	 * Preparation
	 *
	 * The kernel already mapped the ram pages, so we need to unmap them
	 * first to redo them.
	 *
	 * This is safe because:
	 *  1) the first part of memory is owned by us, so we can do what we
	 *     want
	 *  2) the remainder is not used by us, and we restore the mapping
	 *     right away
	 *
	 * There is no need to flush the TLB, as the mapping is restored as is.
	 */
	for (addr = remap_start; addr < remap_end; addr += PMD_SIZE)
		pmd_clear(pmd_off_k(addr));

	/*
	 * Part 1
	 *
	 * First map the space we need to be executable.
	 */
	map.virtual = start;
	map.pfn = virt_to_pfn(start);
	map.length = size;
	map.type = MT_MEMORY_RWX;

	iotable_init(&map, 1);

	/*
	 * Part 2
	 *
	 * Map the remaining space, if any to restore a normal non-executable
	 * mapping.
	 */

	map.virtual = start + size;
	map.pfn = virt_to_pfn(map.virtual);
	map.length = remap_end - map.virtual;
	map.type = MT_MEMORY_RW;

	if (map.length > 0)
		iotable_init(&map, 1);
}

#else

/*
 * Other platforms don't have special alignment requirements and can just
 * execute out of the normal RAM.
 */

#define START_ALIGN PAGE_SIZE
#define END_ALIGN PAGE_SIZE

static inline void __init mark_space_executable(void *start, unsigned long size)
{
}

#endif

#if USE_MEMBLOCK

static __init phys_addr_t find_low_mem(phys_addr_t size)
{
	phys_addr_t start;
	phys_addr_t this_start, this_end, cand;
	phys_addr_t end = memblock.current_limit;
	u64 i;

	start = __pa_symbol(_end) + PAGE_SIZE;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
	for_each_free_mem_range(i, NUMA_NO_NODE, 0, &this_start, &this_end,
#else
	for_each_free_mem_range(i, NUMA_NO_NODE, &this_start, &this_end,
#endif
				NULL) {
		this_start = clamp(this_start, start, end);
		this_end = clamp(this_end, start, end);

		cand = round_up(this_start, START_ALIGN);
		if (cand < this_end && this_end - cand >= size)
			return cand;
	}

	return 0;
}

static __init unsigned long alloc_space(unsigned long size)
{
	unsigned long base;
	int err;

	/*
	 * TODO: Low memory is not actually required
	 *
	 * Traditionally the module memory is placed as close behind the kernel
	 * as possible. The main reason for this is, that relocation entries
	 * have a limited reach and must be able to reach the kernel. Choosing a
	 * location close to the kernel, increases the likely hood that the
	 * relocations will succeed.
	 *
	 * To further improve this, the allocator should consider the maximum
	 * address offset possible during relocations and choose a appropriate
	 * memory location based on that.
	 */
	base = find_low_mem(size);

	if (!base)
		return 0L;

	err = memblock_reserve(base, size);
	BUG_ON(err); /* We've found a free range, so cant fail. */

	return base;
}

static __init void free_space(unsigned long base, unsigned long size)
{
	memblock_free(base, size);
}

#else

#define MODULE_MEM_ASLR_MAX_TRIES 10

#if IS_ENABLED(CONFIG_RANDOMIZE_BASE) && \
	IS_ENABLED(CONFIG_MIPS) && CONFIG_RANDOMIZE_BASE_MIN_ADDR
static __init unsigned long alloc_space(unsigned long size)
{
	unsigned int i;
	unsigned long min_addr, req_base, base = 0;

	min_addr = CONFIG_RANDOMIZE_BASE_MIN_ADDR;
	for (i = 0; i < MODULE_MEM_ASLR_MAX_TRIES; ++i) {
		if (base)
			free_bootmem(base, size);

		req_base = get_random_long();

		req_base &= CONFIG_RANDOMIZE_BASE_MAX_OFFSET - 1;
		req_base += min_addr;
		req_base = PAGE_ALIGN(req_base);
		req_base = CPHYSADDR(req_base);

		base = CPHYSADDR(__alloc_bootmem_nopanic(size, PAGE_SIZE, req_base));
		if (base == req_base)
			return base;
	}

	pr_warn("%s randomized allocation failed, returning a predictable address\n",
		__func__);
	return base;
}
#else
static __init unsigned long alloc_space(unsigned long size)
{
	return CPHYSADDR(alloc_bootmem_pages(size));
}
#endif

static __init void free_space(unsigned long base, unsigned long size)
{
	free_bootmem(base, size);
}

#endif

static __init unsigned long calc_space_size_prom(void)
{
	const char *arg;
	unsigned long size;

	arg = prom_getenv("modulemem");

	if (!arg)
		return 0L;

	if (kstrtoul(arg, 0, &size)) {
		pr_err("Invalid modulemem param: %s\n", arg);
		return 0L;
	}

	return ALIGN(size, END_ALIGN);
}

static __init unsigned long calc_space_size(unsigned int *_module_count)
{
	struct _avm_kernel_module_memory_config *mod = avm_fw_module_sizes();
	unsigned long size = 0;
	unsigned int module_count = 0;

	/*
	 * Try fallback to prom specified size
	 */
	if (!mod) {
		size = calc_space_size_prom();
		*_module_count = size ? 50 : 0;
		return size;
	}
	for (; mod->name; mod++) {
		if (!avm_module_is_allowed(mod->name))
			continue;

		size += reserved_module_size(mod);
		module_count++;
	}
	*_module_count = module_count;
	/*
	 * For luck
	 *
	 * Under circumstanced not fully understood we sometimes have one page
	 * less than needed. This is a WAR to quickly fix this problem.
	 */
	size += PAGE_SIZE;

	return ALIGN(size, END_ALIGN);
}

void __init avm_module_mem_init(void)
{
	unsigned long base, size;
	unsigned int array_size;

	size = calc_space_size(&module_spaces_count);
	if (!size) {
		pr_info("No module information available\n");
		return;
	}
	array_size =
		PAGE_ALIGN(module_spaces_count * sizeof(struct module_space));
	size += array_size; /* include management-array */
	base = alloc_space(size);
	if (!base) {
		/*
		 * We don't BUG_ON here as modules can then still be loaded
		 * into vmalloc'd space to allow an update.
		 */
		pr_err("Cannot allocate module memory space\n");
		return;
	}

	mem_res.start = base;
	mem_res.end = base + size - 1; // Inclusive
	mem_res.name = "AVM Module Mem";
	mem_res.flags = IORESOURCE_MEM | IORESOURCE_BUSY;

	if (insert_resource(&iomem_resource, &mem_res)) {
		pr_err("Could not claim resource\n");
		goto out_fail;
	}

	mark_space_executable(phys_to_virt(base), size);

	module_mem_free_next = phys_to_virt(base);
	module_mem_free_size = size;

	pr_info("Use 0x%08lx-0x%08lx (mapped at %p-%p) for %u modules\n", base,
		base + size - 1, module_mem_free_next,
		module_mem_free_next + module_mem_free_size - 1,
		module_spaces_count);

	// Allocate mem for module_spaces
	module_spaces = module_mem_alloc(array_size);

	// No more space in module memory
	if (!module_spaces) {
		pr_err("module memory exhausted.\n");
		goto out_fail;
	}
	memset(module_spaces, 0, array_size);

	/*
	 * When using a whitelist, modules in there usually use avm_rte which
	 * requires the use of module_mem. Otherwise hard to debug crashes will
	 * occur later on.
	 *
	 * Therefore simply panic the kernel, to fail early when the can not be used.
	 */
	if (module_mem_has_whitelist()) {
		strict_mode = 2;
		pr_info("Using whitelist, set default strict_mode=%d\n", strict_mode);
	}

	return;

out_fail:
	free_space(base, size);
	mem_res.flags &= ~IORESOURCE_BUSY;
}

static __init int test_reserved(void)
{
	unsigned long pfn_start, pfn_end, i, unreserved = 0;

	if (!avm_module_mem_available())
		return 0;

	/*
	 * When using the legacy bootmem allocator pages will not be marked
	 * reserved. This is okay, because the bootmem allocator will simply not
	 * free the used pages to the buddy allocator.
	 *
	 * Sadly there seems to be no easy way to actually verify this.
	 */
	if (!USE_MEMBLOCK)
		return 0;

	/*
	 * We've allocated memory during early boot using memblock.
	 *
	 * As this requires hooks into the early setup of the arch, we check
	 * that the allocated memory is actually marked reserved in the page
	 * allocator. This is to ensure that the hook is early enough and not
	 * after the actual linux mm has been enabled.
	 *
	 * All pages must be reserved in order to be sure we own the memory. If
	 * none are reserved the allocation obviously failed. If only some are
	 * reserved the allocation failed also, and the allocated pages can be
	 * attributed to the buddy allocator.
	 */
	pfn_start = __phys_to_pfn(mem_res.start);
	pfn_end = __phys_to_pfn(mem_res.end);

	for (i = pfn_start; i <= pfn_end; i++) {
		struct page *page = pfn_to_page(i);

		if (!PageReserved(page)) {
			phys_addr_t pa = page_to_phys(page);
			unreserved++;
			pr_err("Page %lu: %pa is not reserved: %08lx\n", i,
			       &pa, page->flags);
		}
	}

	return unreserved;
}

static int __init avm_module_mem_test(void)
{
	/* Only run tests on internal FWs  */
	if (!avm_fw_is_internal())
		return 0;

	BUG_ON(test_reserved() > 0);

	return 0;
}
late_initcall(avm_module_mem_test);

/*
 * Module Spaces
 *
 * The available module mem is divided into module spaces which correspond to a
 * single module.
 */

#define for_each_module_space(i, space)                             \
	for (i = 0, space = module_spaces; i < module_spaces_count; \
	     i++, space++)

static struct module_space *module_space_by_addr(const void *ptr)
{
	int i;
	struct module_space *space;

	for_each_module_space(i, space) {
		if (!atomic_read(&space->valid))
			continue;

		if (space->base <= ptr && ptr < space->base + space->size)
			return space;
	}

	return NULL;
}

static struct module_space *module_space_by_name(const char *name,
						 enum avm_module_mem_type type)
{
	int i;
	struct module_space *space;

	for_each_module_space(i, space) {
		if (!atomic_read(&space->valid))
			continue;

		if (strcmp(space->name, name) == 0 && space->type == type)
			return space;
	}

	return NULL;
}

static struct module_space *next_empty_module_space(void)
{
	unsigned int i;
	struct module_space *space;

	for_each_module_space(i, space)
		if (!atomic_read(&space->valid))
			return space;

	return NULL;
}

static int module_space_free(const void *ptr)
{
	struct module_space *space;

	spin_lock_bh(&module_mem_lock);
	space = module_space_by_addr(ptr);

	if (!space) {
		pr_warn("Pointer %p is not in module_mem\n", ptr);
		goto out_err;
	}

	if (!atomic_read(&space->alloc)) {
		pr_warn("Pointer %p is not allocated\n", ptr);
		goto out_err;
	}

	// We refuse freeing when the provided address is not exactly the start
	if (space->base != ptr) {
		pr_warn("Pointer %p not at the start %p of %s\n", ptr,
			 space->base, space->name);
		goto out_err;
	}

	pr_debug(
		"module=%s pointer %p found in kernel-module-list -freed for re-use\n",
		space->name, ptr);
	// TODO: should this use the kernel poisoning?
	memset(space->base, 0xCC, space->size);
	atomic_set(&space->alloc, 0);
	spin_unlock_bh(&module_mem_lock);

	kmemleak_free(ptr);
	return 0;

out_err:
	spin_unlock_bh(&module_mem_lock);
	return -EFAULT;
}

/*
 * Allocates or reuses a module_space for a given name and type.
 *
 * @returns a valid ptr if the allocation succeeded
 * @returns an ERR_PTR if the allocation failed
 * @returns NULL when the normal allocation path should be used
 */
static void *module_space_alloc(size_t size, char *name,
				enum avm_module_mem_type type)
{
	struct module_space *space;
	void *ret;

	if (!avm_module_mem_available())
		return NULL;

	if (!avm_module_is_allowed(name))
		return NULL;

	size = ALIGN(size, PAGE_SIZE);

	spin_lock_bh(&module_mem_lock);
	space = module_space_by_name(name, type);

	if (!space)
		space = next_empty_module_space();

	if (!space) {
		pr_warn("No more module spaces available\n");
		ret = ERR_PTR(-ENOMEM);
		goto out;
	}

	if (atomic_read(&space->alloc)) {
		pr_warn("Module '%s' already allocated\n", space->name);
		ret = ERR_PTR(-EBUSY);
		goto out;
	}

	// Space is empty, so allocate some memory
	if (!atomic_read(&space->valid)) {
		size_t reserved_size = module_reserved_size(name);

		if (strict_mode >= 2 && size > reserved_size) {
			panic("module '%s' requires more memory than reserved (%zu > %zu)", name, size, reserved_size);
		} else if (reserved_size > size) {
			module_alloc_waste += reserved_size - size;
			pr_warn("module '%s' reserved size %zu is to great for demand size %zu - waste %zu (module_alloc_waste=%d)\n",
				name, reserved_size, size, reserved_size - size,
				module_alloc_waste);
		} else if (reserved_size < size) {
			module_alloc_waste -= size - reserved_size;
			pr_err("module '%s' reserved size %zu too small for demand size %zu - need %zu more (module_alloc_waste=%d)\n",
			       name, reserved_size, size, size - reserved_size,
			       module_alloc_waste);
		}

		space->base = module_mem_alloc(size);
		space->size = size;

		// No more space in module memory
		if (!space->base) {
			pr_err("module memory exhausted.\n");
			ret = ERR_PTR(-ENOMEM);
			goto out;
		}

		strlcpy(space->name, name, sizeof(space->name));

		pr_info("give 0x%zx bytes at %p to module '%s' (0x%zx bytes left)\n",
			space->size, space->base, space->name,
			module_mem_free_size);
		atomic_set(&space->valid, 1);
	}
	// The space we have is less that what is needed
	else if (space->size < size) {
		pr_err("invalid size change 0x%zx bytes < 0x%zx bytes (module '%s')\n",
		       space->size, size, space->name);

		ret = ERR_PTR(-EINVAL);
		goto out;
	}

	atomic_set(&space->alloc, 1);
	ret = space->base;
out:
	spin_unlock_bh(&module_mem_lock);

	if (!IS_ERR_OR_NULL(ret))
		kmemleak_alloc(ret, size, 1, GFP_KERNEL);

	return ret;
}


/**
 */
static void module_alloc_proc_allocated(struct seq_file *file, void *ctx)
{
	const struct module_space *space;
	unsigned long allocated = 0;
	unsigned int i;

	seq_printf(file, "%30s %15s %15s\n", "Module", "Allocated", "Reserved");

	for_each_module_space(i, space) {
		unsigned long reserved;

		if (!atomic_read(&space->valid))
			continue;

		allocated += space->size;

		seq_printf(file, "%30s %15zd", space->name, space->size);

		reserved = module_reserved_size(space->name);

		if (reserved == 0) {
			seq_printf(file, " %15s", "-");
		} else {
			seq_printf(file, " %15ld", reserved);
		}

		seq_puts(file, "\n");
	}

	seq_puts(file, "\n");
	seq_printf(file, "  Modules: %u/%u\n", i, module_spaces_count);
	seq_printf(file, "    Total: %8ld\n", module_mem_free_size + allocated);
	seq_printf(file, "Allocated: %8ld\n", allocated);
	seq_printf(file, " Reserved: %8zd\n", module_mem_free_size);
}

#define MODULE_MEM_PROC_SIZES_RESERVED (void *)0
#define MODULE_MEM_PROC_SIZES_ALL (void *)1

/**
 */
static void module_alloc_proc_sizes(struct seq_file *file, void *ctx)
{
	struct _avm_kernel_module_memory_config *mod;

	seq_printf(file, "%30s %15s %15s %15s\n", "Module", "Size", "CoreSize",
		   "SymbolSize");

	if (!avm_fw_module_sizes())
		return;

	for (mod = avm_fw_module_sizes(); mod->name; mod++) {
		unsigned int symbol_size;

		if (!avm_module_is_allowed(mod->name) &&
		    ctx == MODULE_MEM_PROC_SIZES_RESERVED)
			continue;

		if (IS_ENABLED(CONFIG_KALLSYMS_ALL))
			symbol_size = mod->symbol_size;
		else if (IS_ENABLED(CONFIG_KALLSYMS))
			symbol_size = mod->symbol_text_size;
		else
			symbol_size = 0;

		seq_printf(file, "%30s %15zd %15d %15d\n", mod->name,
			   reserved_module_size(mod), mod->core_size,
			   symbol_size);
	}
}

/**
 */
static __init int module_alloc_proc_init(void)
{
	proc_mkdir("avm/module", NULL);

	add_simple_proc_file("avm/module/allocated", NULL,
			     module_alloc_proc_allocated, NULL);
	add_simple_proc_file("avm/module/reserved", NULL,
			     module_alloc_proc_sizes,
			     MODULE_MEM_PROC_SIZES_RESERVED);
	add_simple_proc_file("avm/module/sizes", NULL, module_alloc_proc_sizes,
			     MODULE_MEM_PROC_SIZES_ALL);

	return 0;
}
late_initcall(module_alloc_proc_init);

/*
 * Public API
 */
void *avm_module_mem_alloc(struct module *mod, unsigned long size,
			   enum avm_module_mem_type type)
{
	void *ptr = NULL;

	/*
	 * We only allocate on the core type.
	 *
	 * TODO: the code also handles type_page which seems to be used nowhere.
	 */
	if (type == avm_module_mem_type_core)
		ptr = module_space_alloc(size, mod->name, type);

	// We've got an actual error, so do not fallback
	if (IS_ERR(ptr) && PTR_ERR(ptr) != -ENOMEM)
		return NULL;

	// There is no more memory available but we should allocate from there,
	// abort if strict_mode >= 1
	if (strict_mode >= 1 && IS_ERR(ptr))
		panic("Could not allocate memory for module %s", mod->name);

	if (IS_ERR_OR_NULL(ptr))
		ptr = module_alloc(size);

	return ptr;
}

void avm_module_mem_free(void *ptr)
{
	if (avm_module_mem_contains(ptr))
		WARN_ON(module_space_free(ptr));
	else
		module_memfree(ptr);
}

int avm_module_mem_contains(void *ptr)
{
	phys_addr_t phys_addr = virt_to_phys(ptr);

	return (phys_addr >= mem_res.start) && (phys_addr < mem_res.end);
}

/*
 * Public legacy api
 */

char *module_alloc_find_module_name(char *buff, char *end, unsigned long addr)
{
	struct module_space *space;
	unsigned int len;

	space = module_space_by_addr((void *)addr);

	if (!space)
		goto not_found;

	if (!atomic_read(&space->alloc))
		goto not_found;

	len = snprintf(buff, end - buff, "0x%08lx (%s + 0x%lx)   [%s]", addr,
		       space->name, addr - (unsigned long)space->base,
		       space->name);

out:
	return buff + len;

not_found:
	len = snprintf(buff, end - buff, "0x%08lx", addr);
	goto out;
}

/**
 */
#if defined(CONFIG_CHECK_TIMER_ON_FREED_MODULE)
int module_alloc_check_pointer(unsigned long addr, char **name)
{
	struct module_space *space = module_space_by_addr((void *)addr);

	if (!space)
		return 1;

	if (atomic_read(&space->alloc))
		return 0;

	*name = space->name;
	return -1;
}
#endif /*--- #if defined(CONFIG_CHECK_TIMER_ON_FREED_MODULE) ---*/

/**
 * @brief get module-alloc-area-infos if addr in range
 * @return start (zero if out of range)
 */
unsigned long get_modulealloc_area(unsigned long addr, char **module_name,
				   unsigned int *allocated, unsigned long *size)
{
	char *name = "";
	unsigned int _allocated = 0;
	unsigned long _size = 0;
	unsigned long _addr = 0;
	const struct module_space *space;

	space = module_space_by_addr((void *)addr);
	if (space) {
		name = (char *)space->name;
		_size = space->size;
		_allocated = atomic_read(&space->alloc);
		_addr = (unsigned long)space->base;

		pr_debug(
			"%s: 0x%0lx - is [%s] in range %0lx - %0lx allocated=%u\n",
			__func__, addr, name, _addr, _addr + _size, _allocated);
	} else if (avm_module_mem_contains((void *)addr)) {
		/*--- reserved but now never allocated ---*/
		_size = module_mem_free_size;
		_addr = (unsigned long)module_mem_free_next;
	}
	if (module_name)
		*module_name = name;
	if (size)
		*size = _size;
	if (allocated)
		*allocated = _allocated;
	return _addr;
}
