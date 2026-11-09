#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>

extern struct resource avm_mtd_ram_resource[];

static int __init prepare_ram_resource(char *p)
{
	char *np;
	unsigned long start, end;
	int result;

	pr_err("[%s] entered\n", __func__);

	if (p == NULL) {
		return 0;
	}

	pr_debug("[%s] [mtdram1] %s\n", __func__, p);

	np = strchr(p, ',');
	if (np == NULL) {
		goto err_out;
	}

	*np = '\0';
	++np;

	result = kstrtoul(p, 0, &start);
	if (result != 0) {
		goto err_out;
	}

	result = kstrtoul(np, 0, &end);
	if (result != 0) {
		goto err_out;
	}

	if (start >= end) {
		goto err_out;
	}

#ifdef CONFIG_AVM_MTDRAM_KSEG0
	start -= 0x80000000 - PHYS_OFFSET;
	end -= 0x80000000 - PHYS_OFFSET;
#endif

	avm_mtd_ram_resource[0].start = start;
	avm_mtd_ram_resource[0].end = end - 1;
	avm_mtd_ram_resource[0].flags = IORESOURCE_MEM;

	pr_info("[%s] mtdram1 0x%08x-0x%08x\n", __func__,
		avm_mtd_ram_resource[0].start, avm_mtd_ram_resource[0].end);

	return 0;

err_out:
	pr_err("[%s] error parsing setup string: %s\n", __func__, p);
	return 0;
}

__setup("mtdram1=", prepare_ram_resource);
