// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2023 AVM GmbH <fritzbox_info@avm.de> */

#define pr_fmt(fmt) "avm_of_overlay_cmdline: " fmt

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/string.h>
#include <avm/compat/misc.h>
#include <avm/sammel/avm_of_overlay_cmdline.h>

#if IS_ENABLED(CONFIG_AVM_OF_OVERLAY_EXPORT)
#define __dtbo_initdata
#else
#define __dtbo_initdata __initdata
#endif /* AVM_OF_CMDLINE_EXPORT */

#define MAX_DT_OVERLAYS CONFIG_AVM_OF_OVERLAY_CMDLINE_MAX
static struct dt_overlay_mem dt_overlay_mem[MAX_DT_OVERLAYS] __dtbo_initdata;
static size_t dt_overlay_mem_len __dtbo_initdata;

static int __init avm_of_overlay_cmdline_reserve(char *opt)
{
	unsigned long long base, size;
	char *cur = opt;

	pr_debug("Reserving the DT overlay from the parameter %s\n", opt);

	base = memparse(cur, &cur);
	if (*cur != ',') {
		pr_err("Could not parse %s: Not a number\n", opt);
		return -EINVAL;
	}

	cur++;

	size = memparse(cur, &cur);
	if (*cur != '\0') {
		pr_err("Could not parse %s: Not a number\n", opt);
		return -EINVAL;
	}

	if (base > PHYS_ADDR_MAX || size > PHYS_ADDR_MAX ||
	    size > PHYS_ADDR_MAX - base) {
		pr_err("Could not parse %s as physical address: The value is too large\n", opt);
		return -EINVAL;
	}

	if (dt_overlay_mem_len == MAX_DT_OVERLAYS) {
		pr_err("Could not load the DT overlay %s: Maximum number of overlays reached\n", opt);
		return -EINVAL;
	}

	if (memblock_reserve(base, size) < 0) {
		pr_err("Could not load the DT overlay: Could not reserve [%llx,%llx)\n", base,
		       base + size);
		return -ENOMEM;
	}

	pr_debug("Reserved the DT overlay #%zu at [%llx,%llx)\n", dt_overlay_mem_len, base, size);

	dt_overlay_mem[dt_overlay_mem_len].base = base;
	dt_overlay_mem[dt_overlay_mem_len].size = size;
	dt_overlay_mem_len++;

	return 0;
}

static __init int avm_of_overlay_cmdline_apply_one(struct dt_overlay_mem *mem)
{
	int err, ovcs_id;
	phys_addr_t mem_end = mem->base + mem->size;

	pr_debug("Considering the DT overlay at [%pa,%pa)\n", &mem->base, &mem_end);

	mem->dtbo = memremap(mem->base, mem->size, MEMREMAP_WB);
	if (!mem->dtbo) {
		pr_err("Could not map the DT overlay at [%pa,%pa)\n",
		       &mem->base, &mem_end);
		return -ENOMEM;
	}

	err = of_overlay_fdt_apply(mem->dtbo, mem->size, &ovcs_id);

	/* The mapping is needed to export the dtbo's later on.
	 * Which means cleaup would need to happen way later, after boot.
	 * For now we can only cleanup if dtbo export is not enabled.
	 */
	if (!IS_ENABLED(CONFIG_AVM_OF_OVERLAY_EXPORT))
		memunmap(mem->dtbo);

	if (err < 0) {
		pr_err("Could not apply the DT overlay at [%pa,%pa): %d\n",
		       &mem->base, &mem_end, err);
		return err;
	}

	pr_info("Successfully applied the DT overlay at [%pa,%pa)\n", &mem->base, &mem_end);

	return 0;
}

static __init int avm_of_overlay_cmdline_apply(void)
{
	size_t i;
	int err, ret = 0;

	pr_debug("Applying %zu overlays\n", dt_overlay_mem_len);

	for (i = 0; i < dt_overlay_mem_len; ++i) {
		struct dt_overlay_mem *mem = &dt_overlay_mem[i];

		err = avm_of_overlay_cmdline_apply_one(mem);
		if (!ret)
			ret = err;
		/* The overlay memory is needed to export the dtbo's later on.
		 * Which means cleaup would need to happen way later, after boot.
		 * For now we can only cleanup if dtbo export is not enabled.
		 */
		if (!IS_ENABLED(CONFIG_AVM_OF_OVERLAY_EXPORT))
			memblock_phys_free(mem->base, mem->size);
	}

	return ret;
}

early_param("avm,fdt-overlay", avm_of_overlay_cmdline_reserve);

/* This needs to be before device_initcall (6), so the devices initialize
 * with the overlay applied.
 */
core_initcall(avm_of_overlay_cmdline_apply);

const struct dt_overlay_mem *avm_of_overlay_get_dtbo_mem(int dtbo_idx)
{
	if (!IS_ENABLED(CONFIG_AVM_OF_OVERLAY_EXPORT) || dtbo_idx >= dt_overlay_mem_len)
		return NULL;
	return &dt_overlay_mem[dtbo_idx];
}
