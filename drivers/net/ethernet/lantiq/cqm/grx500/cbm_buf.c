// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * CBM buffer pools carved out of reserved memory: resolve, map, align and
 * free-list each pool, and hand chunks to the CPU-TX path.
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sizes.h>

#include "cbm.h"

#define DEFAULT_STD_FRM_SIZE               2048U
#define DEFAULT_JBO_FRM_SIZE               8192U

/* Number of pool kinds carried by this TU. */
#define CBM_POOL_STD  0
#define CBM_POOL_JBO  1
#define CBM_POOL_NUM  2

/*
 * Public function prototypes for the cross-TU callers in cbm.c.
 *
 * The kernel build's -Werror=missing-prototypes wants the defining TU to also
 * see a prototype before the definition; placing them at file scope satisfies
 * the toolchain without widening cbm.h.
 */
int cbm_buf_init_pools(struct device *dev);
phys_addr_t cbm_buf_pool_phys_base(int which);

/*
 * struct cbm_pool — one CBM reserved-memory carve pool.
 *
 * @name:        human-readable label ("cbm-std-pool" / "cbm-jbo-pool")
 *               used in pr_info / pr_err substrings the hardware tester
 *               greps for. Set from of_reserved_mem_lookup()->name so the
 *               DT name is reflected verbatim.
 *
 * @phys_base:   physical address of the reserved region (aligned UP via
 *               buf_addr_adjust to @frm_size — defensive; the 5.10
 *               reference addresses are already 0x1000-aligned).
 *
 * @size:        usable region size after @phys_base alignment.
 *
 * @virt_base:   memremap() result — kernel virtual base, used for
 *               free-list traversal. devm_memremap()-managed so unbind
 *               releases it without explicit munmap.
 *
 * @frm_size:    per-chunk size (2048 for std, 8192 for jbo). The carve
 *               allocator returns chunks of exactly @frm_size.
 *
 * @frm_num:     number of chunks the carve allocator manages (floor of
 *               @size / @frm_size — any tail bytes are unreachable).
 *
 * @lock: protects @free_head.
 *
 * @free_head:   head of the embedded singly-linked free-list. Each free
 *               chunk's first sizeof(void *) bytes hold the kernel-virt
 *               pointer to the next free chunk. NULL means the pool is
 *               drained.
 *
 * Invariants: - @virt_base must remain valid for the lifetime of the bound
 * device. Every pointer reachable from @free_head lies in [virt_base,
 * virt_base + frm_num * frm_size). @free_head transitions only under @lock. A
 * chunk handed out by cbm_buf_alloc must NOT have its first bytes touched
 * until the consumer overwrites them — the consumer is then free to use the
 * whole chunk including byte 0; cbm_buf_free re-establishes the embedded next
 * pointer when the chunk is returned.
 */
struct cbm_pool {
	const char *name;
	phys_addr_t phys_base;
	size_t size;
	void *virt_base;
	u32 frm_size;
	u32 frm_num;
	spinlock_t lock;
	void *free_head;
};

static struct cbm_pool g_cbm_pools[CBM_POOL_NUM] = {
	[CBM_POOL_STD] = { .name = "cbm-std-pool" },
	[CBM_POOL_JBO] = { .name = "cbm-jbo-pool" },
};

/* Global pool tracker. */
struct cbm_buff g_cbm_buff = {
	.std_frm_size = DEFAULT_STD_FRM_SIZE,
	.jbo_frm_size = DEFAULT_JBO_FRM_SIZE,
};
EXPORT_SYMBOL_GPL(g_cbm_buff);

/*
 * buf_addr_adjust - round a raw allocator return up to the frame-size
 * alignment.
 *
 * The contract is unchanged — only the dead-code marker dropped.
 */
static void *buf_addr_adjust(void *raw, u32 frm_size)
{
	uintptr_t a = (uintptr_t)raw;
	uintptr_t fs;

	if (!frm_size)
		return raw;

	fs = (uintptr_t)frm_size;
	if ((a % fs) == 0)
		return raw;

	/* Otherwise align UP to the next frm_size multiple. Generic
	 * arithmetic (no power-of-two mask shortcut) keeps the carve
	 * allocator correct even if frm_size is not a power of two.
	 */
	a = a + (fs - (a % fs));
	return (void *)a;
}

/*
 * carve_pool_build_freelist — populate @pool->free_head with frm_num
 * frm_size-aligned chunks reachable via embedded next pointers.
 *
 * Walks the pool from the highest chunk to the lowest, pushing each onto
 * the head. The resulting free-list iterates chunks lowest-to-highest
 * (pop returns lowest first) — a stable-traversal order that makes early
 * boot dmesg easier to read; not a functional requirement.
 *
 * @pool->virt_base, @pool->frm_size, and @pool->frm_num must all be set
 * before this is called.
 */
static void carve_pool_build_freelist(struct cbm_pool *pool)
{
	u32 i;
	void *chunk;

	pool->free_head = NULL;
	for (i = pool->frm_num; i-- > 0; ) {
		chunk = (u8 *)pool->virt_base + (size_t)i * pool->frm_size;
		*(void **)chunk = pool->free_head;
		pool->free_head = chunk;
	}
}

/*
 * carve_pool_init_one - resolve, map, align, and freelist-seed a single pool
 * by name.
 *
 * Resolve the phandle by name via of_property_match_string +
 * of_parse_phandle. 2. of_reserved_mem_lookup() to pull phys_base + size out
 * of the reservation tracker (set up by
 * early_init_dt_alloc_reserved_memory_arch at boot from the fdt). 3. Reject
 * regions smaller than 2 * frm_size — anything smaller can hand out at most
 * one chunk and is a configuration error. 4. devm_memremap(MEMREMAP_WB) the
 * region. 5. buf_addr_adjust() the kernel-virt base up to frm_size alignment.
 * 6. Re-derive phys_base by applying the same alignment shift, so
 * cbm_buf_pool_phys_base() reflects the carved start (not the raw
 * reserved-region start). 7. Compute frm_num = floor(size / frm_size). 8.
 * Build the embedded free-list. 9. Substring contract: "cbm: reserved-memory
 * cbm-std-pool resolved @ phys 0x23800000 size 0x1200000" (the jbo line uses
 * the same shape with its own address/size). These substrings are unique
 * enough that a hardware tester can grep for "reserved-memory cbm-std-pool
 * resolved" without false positives.
 */
static int carve_pool_init_one(struct device *dev,
			       struct cbm_pool *pool,
			       u32 frm_size)
{
	struct device_node *np = dev->of_node;
	struct device_node *rmem_np;
	struct reserved_mem *rmem;
	void *virt;
	void *aligned_virt;
	phys_addr_t aligned_phys;
	int idx;

	if (!np) {
		dev_err(dev, "cbm: %s: parent of_node NULL\n", pool->name);
		return -ENODEV;
	}

	/* (1) Resolve phandle by name. */
	idx = of_property_match_string(np, "memory-region-names",
				       pool->name);
	if (idx < 0) {
		dev_err(dev,
			"cbm: %s: memory-region-names lookup failed: %d\n",
			pool->name, idx);
		return -ENODEV;
	}

	rmem_np = of_parse_phandle(np, "memory-region", idx);
	if (!rmem_np) {
		dev_err(dev,
			"cbm: %s: of_parse_phandle(memory-region[%d]) returned NULL\n",
			pool->name, idx);
		return -ENODEV;
	}

	/* (2) Look up the reserved-memory entry. */
	rmem = of_reserved_mem_lookup(rmem_np);
	of_node_put(rmem_np);
	if (!rmem) {
		dev_err(dev,
			"cbm: %s: of_reserved_mem_lookup returned NULL\n",
			pool->name);
		return -ENODEV;
	}

	/* (3) Sanity-check the size — must fit at least 2 chunks. */
	if (rmem->size < (phys_addr_t)frm_size * 2) {
		dev_err(dev,
			"cbm: %s: region size %pa < 2 * frm_size (%u)\n",
			pool->name, &rmem->size, frm_size);
		return -EINVAL;
	}

	/* (4) Establish a kernel-virtual mapping. devm-managed. */
	virt = devm_memremap(dev, rmem->base, rmem->size, MEMREMAP_WB);
	if (!virt) {
		dev_err(dev,
			"cbm: %s: devm_memremap(phys=%pa size=%pa) failed\n",
			pool->name, &rmem->base, &rmem->size);
		return -ENOMEM;
	}

	/*
	 * Compute the same shift on both the kernel-virt pointer and the
	 * physical address so the two stay in lockstep.
	 */
	aligned_virt = buf_addr_adjust(virt, frm_size);
	aligned_phys = rmem->base +
		       ((phys_addr_t)((u8 *)aligned_virt - (u8 *)virt));

	/* (7) Frame count after alignment. */
	pool->phys_base = aligned_phys;
	pool->size      = rmem->size -
			  (size_t)((u8 *)aligned_virt - (u8 *)virt);
	pool->virt_base = aligned_virt;
	pool->frm_size  = frm_size;
	pool->frm_num   = pool->size / frm_size;

	if (pool->frm_num < 2) {
		dev_err(dev,
			"cbm: %s: after alignment frm_num=%u < 2 (size=%zu frm_size=%u)\n",
			pool->name, pool->frm_num, pool->size, frm_size);
		return -EINVAL;
	}

	spin_lock_init(&pool->lock);

	/* (8) Seed the embedded free-list. */
	carve_pool_build_freelist(pool);

	pr_info("cbm: reserved-memory %s resolved @ phys 0x%llx size 0x%zx (frm_size=%u frm_num=%u)\n",
		pool->name,
		(unsigned long long)pool->phys_base,
		pool->size,
		pool->frm_size,
		pool->frm_num);

	return 0;
}

/*
 * cbm_buf_init_pools - public entry point called from cbm_xrx500_probe().
 *
 * Partial success is intentionally NOT supported: a single-pool CBM is
 * meaningless because the SBA_0/JBA_0 hardware registers must both be
 * programmed before init_cbm_basic completes.
 *
 * devm-attached resources are released automatically on driver unbind;
 * the carve allocator's free-list lives in the memremap'd memory, so
 * losing the mapping is equivalent to losing the free-list — no separate
 * teardown is required.
 */
int cbm_buf_init_pools(struct device *dev)
{
	int ret;

	if (!dev) {
		pr_err("cbm: cbm_buf_init_pools: NULL dev\n");
		return -EINVAL;
	}

	/* std pool — 2048 byte chunks. */
	ret = carve_pool_init_one(dev, &g_cbm_pools[CBM_POOL_STD],
				  DEFAULT_STD_FRM_SIZE);
	if (ret)
		return ret;

	/* jbo pool — 8192 byte chunks. */
	ret = carve_pool_init_one(dev, &g_cbm_pools[CBM_POOL_JBO],
				  DEFAULT_JBO_FRM_SIZE);
	if (ret)
		return ret;

	/* Mirror dimensions into g_cbm_buff for the legacy readers. */
	g_cbm_buff.std_frm_size  = g_cbm_pools[CBM_POOL_STD].frm_size;
	g_cbm_buff.jbo_frm_size  = g_cbm_pools[CBM_POOL_JBO].frm_size;
	g_cbm_buff.std_pool_size = g_cbm_pools[CBM_POOL_STD].size;
	g_cbm_buff.jbo_pool_size = g_cbm_pools[CBM_POOL_JBO].size;
	g_cbm_buff.std_frm_num   = g_cbm_pools[CBM_POOL_STD].frm_num;
	g_cbm_buff.jbo_frm_num   = g_cbm_pools[CBM_POOL_JBO].frm_num;
	g_cbm_buff.std_buf_base  = g_cbm_pools[CBM_POOL_STD].virt_base;
	g_cbm_buff.std_buf_addr  = g_cbm_pools[CBM_POOL_STD].virt_base;
	g_cbm_buff.jbo_buf_base  = g_cbm_pools[CBM_POOL_JBO].virt_base;
	g_cbm_buff.jbo_buf_addr  = g_cbm_pools[CBM_POOL_JBO].virt_base;
	g_cbm_buff.placeholder   = false;

	return 0;
}
EXPORT_SYMBOL_GPL(cbm_buf_init_pools);

/*
 * cbm_buf_pool_phys_base - accessor used by init_cbm_basic (cbm.c) to program
 * SBA_0 / JBA_0 / SBA_1 / JBA_1 with the physical-address bases of the carve
 * pools.
 *
 * @which: CBM_POOL_STD (0) or CBM_POOL_JBO (1). Any other value returns
 *         0, which init_cbm_basic interprets as an error condition via
 *         the surrounding NULL-pointer checks (SBA_0=0 would surface as
 *         a fatal silicon error long before frame admit).
 */
phys_addr_t cbm_buf_pool_phys_base(int which)
{
	if (which < 0 || which >= CBM_POOL_NUM)
		return 0;
	return g_cbm_pools[which].phys_base;
}
EXPORT_SYMBOL_GPL(cbm_buf_pool_phys_base);

/*
 * cbm_buf_alloc - request a chunk from one of the carve pools.
 *
 * @pool_phys: optional out — physical address of the returned chunk. The
 *             carve allocator computes phys = pool->phys_base +
 *             (chunk - pool->virt_base) so callers can populate DMA
 *             descriptors directly.
 */
void *cbm_buf_alloc(u32 size, u32 *pool_phys, u32 flags)
{
	struct cbm_pool *pool;
	void *chunk;
	unsigned long irq_flags;

	(void)flags;  /* dispatch is by SIZE, @flags unused. */

	if (size <= DEFAULT_STD_FRM_SIZE) {
		pool = &g_cbm_pools[CBM_POOL_STD];
	} else if (size <= DEFAULT_JBO_FRM_SIZE) {
		pool = &g_cbm_pools[CBM_POOL_JBO];
	} else {
		pr_err("cbm: cbm_buf_alloc(size=%u): exceeds jbo_frm_size cap %u — no pool can service\n",
		       size, DEFAULT_JBO_FRM_SIZE);
		return NULL;
	}

	if (!pool->virt_base) {
		pr_err("cbm: cbm_buf_alloc(%s): pool not initialised (probe path missed of_reserved_mem_lookup?)\n",
		       pool->name);
		return NULL;
	}

	spin_lock_irqsave(&pool->lock, irq_flags);
	chunk = pool->free_head;
	if (chunk)
		pool->free_head = *(void **)chunk;
	spin_unlock_irqrestore(&pool->lock, irq_flags);

	if (!chunk) {
		pr_err_ratelimited("cbm: cbm_buf_alloc(%s): free-list empty (frm_num=%u)\n",
				   pool->name, pool->frm_num);
		return NULL;
	}

	if (pool_phys)
		*pool_phys = (u32)(pool->phys_base +
				   ((phys_addr_t)((u8 *)chunk -
						  (u8 *)pool->virt_base)));

	return chunk;
}
EXPORT_SYMBOL_GPL(cbm_buf_alloc);

/*
 * cbm_buf_phys_to_virt - map a CBM pool PHYSICAL address back to its
 * kernel-virtual address within the memremap'd carve pool.
 */
void *cbm_buf_phys_to_virt(u32 phys)
{
	int i;

	for (i = 0; i < CBM_POOL_NUM; i++) {
		struct cbm_pool *pool = &g_cbm_pools[i];
		phys_addr_t lo, hi;

		if (!pool->virt_base)
			continue;
		lo = pool->phys_base;
		hi = lo + (phys_addr_t)pool->frm_num * pool->frm_size;
		if ((phys_addr_t)phys >= lo && (phys_addr_t)phys < hi)
			return (u8 *)pool->virt_base +
			       ((phys_addr_t)phys - lo);
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(cbm_buf_phys_to_virt);

/*
 * cbm_buf_free - return a chunk previously handed out by cbm_buf_alloc.
 *
 * @buf:  kernel-virtual address as returned by cbm_buf_alloc.
 *
 * @size: byte size originally requested.
 */
int cbm_buf_free(void *buf, u32 size)
{
	struct cbm_pool *pool;
	uintptr_t addr;
	uintptr_t lo;
	uintptr_t hi;
	unsigned long irq_flags;

	if (!buf)
		return -EINVAL;

	/* Pick the pool the same way alloc did. */
	if (size <= DEFAULT_STD_FRM_SIZE) {
		pool = &g_cbm_pools[CBM_POOL_STD];
	} else if (size <= DEFAULT_JBO_FRM_SIZE) {
		pool = &g_cbm_pools[CBM_POOL_JBO];
	} else {
		pr_err("cbm: cbm_buf_free(size=%u): exceeds jbo_frm_size cap %u\n",
		       size, DEFAULT_JBO_FRM_SIZE);
		return -EINVAL;
	}

	if (!pool->virt_base)
		return -EINVAL;

	/* Range check — the buf must point inside the chosen pool. */
	addr = (uintptr_t)buf;
	lo   = (uintptr_t)pool->virt_base;
	hi   = lo + pool->frm_num * pool->frm_size;
	if (addr < lo || addr >= hi) {
		pr_err_ratelimited("cbm: cbm_buf_free(%s): buf %p outside pool [%p, +0x%zx)\n",
				   pool->name, buf, pool->virt_base,
				   (size_t)(pool->frm_num * pool->frm_size));
		return -EINVAL;
	}

	spin_lock_irqsave(&pool->lock, irq_flags);
	*(void **)buf = pool->free_head;
	pool->free_head = buf;
	spin_unlock_irqrestore(&pool->lock, irq_flags);

	return 0;
}
EXPORT_SYMBOL_GPL(cbm_buf_free);
