// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2014 Lei Chuanhua <Chuanhua.lei@lantiq.com>
 * Copyright (C) 2016 Intel Corporation.
 */
#include <linux/export.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/dma-map-ops.h>
#include <linux/printk.h>
#include <asm/bootinfo.h>
#include <asm/mips-cps.h>
#include <asm/mipsregs.h>
#include <asm/prom.h>
#include <asm/smp-ops.h>

#define IOPORT_RESOURCE_START   0x10000000
#define IOMEM_RESOURCE_START    0x10000000

const char *get_system_type(void)
{
	return "Intel MIPS interAptiv SoC";
}

void prom_free_prom_memory(void)
{
}

static void __init prom_init_cmdline(void)
{
	int i;
	int argc;
	char **argv;

	/*
	 * If u-boot pass parameters, it is ok, however, if without u-boot
	 * JTAG or other tool has to reset all register value before it goes
	 * emulation most likely belongs to this category
	 */
	if (fw_arg0 == 0 || fw_arg1 == 0)
		return;

	/*
	 * a0: fw_arg0 - the number of string in init cmdline
	 * a1: fw_arg1 - the address of string in init cmdline
	 *
	 * In accordance with the MIPS UHI specification, the bootloader can
	 * pass the following arguments to the kernel: - $a0: -2. $a1: KSEG0
	 * address of the flattened device-tree blob.
	 */
	if (fw_arg0 == -2)
		return;

	argc = fw_arg0;
	argv = (char **)KSEG1ADDR(fw_arg1);

	arcs_cmdline[0] = '\0';

	for (i = 0; i < argc; i++) {
		char *p = (char *)KSEG1ADDR(argv[i]);

		if (argv[i] && *p) {
			strlcat(arcs_cmdline, p, sizeof(arcs_cmdline));
			strlcat(arcs_cmdline, " ", sizeof(arcs_cmdline));
		}
	}
}

static int __init plat_enable_iocoherency(void)
{
	if (!mips_cps_numiocu(0))
		return 0;

	/* Nothing special needs to be done to enable coherency */
	pr_info("Coherence Manager IOCU detected\n");
	/* Second IOCU for MPE or other master access register */
	write_gcr_reg0_base(0xa0000000);
	write_gcr_reg0_mask(0xf8000000 | CM_GCR_REGn_MASK_CMTGT_IOCU1);
	return 1;
}

static void __init plat_setup_iocoherency(void)
{
	/*
	 * Software coherency: the datapath masters are programmed with
	 * direct-DDR bus addresses, which the Coherence Manager does not
	 * snoop, so cache maintenance must run on every DMA.
	 */
	plat_enable_iocoherency();
	dma_default_coherent = false;
	pr_info("software DMA cache coherency enforced\n");
}

static void free_init_pages_eva_intel(void *begin, void *end)
{
	free_init_pages("unused kernel", __pa_symbol((unsigned long *)begin),
			__pa_symbol((unsigned long *)end));
}

static void plat_early_init_devtree(void)
{
	void *dtb;

	/*
	 * The boot loader passes no device tree; get_fdt() returns the blob
	 * appended to vmlinux.bin.
	 */
	dtb = get_fdt();
	if (!dtb)
		panic("no dtb found");

	__dt_setup_arch(dtb);
}

void __init plat_mem_setup(void)
{
	ioport_resource.start = IOPORT_RESOURCE_START;
	ioport_resource.end = ~0UL; /* No limit */
	iomem_resource.start = IOMEM_RESOURCE_START;
	iomem_resource.end = ~0UL; /* No limit */

	set_io_port_base((unsigned long)KSEG1);

	strscpy(arcs_cmdline, boot_command_line, COMMAND_LINE_SIZE);

	plat_early_init_devtree();
	plat_setup_iocoherency();

	if (IS_ENABLED(CONFIG_EVA))
		free_init_pages_eva = free_init_pages_eva_intel;
	else
		free_init_pages_eva = 0;
}

void __init device_tree_init(void)
{
	unflatten_and_copy_device_tree();
}

void __init prom_init(void)
{
	prom_init_cmdline();

	/*
	 * The boot firmware enters Linux with the core already in the CM
	 * coherence domain and a coherent CCA selected, so nothing here may
	 * join it again: the writes that would do so change the fabric
	 * configuration of a system that is already running on it.
	 */

	/*
	 * mips_cpc_probe() is skipped for the same reason: it writes
	 * GCR_CPC_BASE, and re-pointing the CPC window while the other core
	 * is running under the boot firmware's configuration is not safe
	 * here.
	 */

	if (!register_cps_smp_ops())
		return;

	if (!register_vsmp_smp_ops())
		return;
}
