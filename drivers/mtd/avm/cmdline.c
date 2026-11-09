#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/mtd/avm_mtdram.h>

#include <asm/setup.h>

extern struct resource avm_mtd_ram_resource[];

struct mtdram_check_overlap_query {
	phys_addr_t base;
	phys_addr_t size;

	bool overlap_mtdram;
};

static int __init mtdram1_setup_or_query(char *p,
					 struct mtdram_check_overlap_query *query)
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

	if (query) {
		if (!(start > query->base + query->size ||
		      end < query->base))
			query->overlap_mtdram = true;
		return 0;
	}

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

static int __init mtdram1_setup(char *p)
{
	return mtdram1_setup_or_query(p, NULL);
}

__setup("mtdram1=", mtdram1_setup);

static int __init mtdram_check_overlap(char *param, char *val, const char *doing,
				       void *query)
{
	if (!strcmp(param, "mtdram1"))
		return mtdram1_setup_or_query(val, query);
	else
		return 0;
}

bool __init avm_mtdram_check_overlap(const char *cmdline, phys_addr_t base,
				     phys_addr_t size)
{
	static char static_cmdline[COMMAND_LINE_SIZE];
	static DEFINE_SPINLOCK(static_cmdline_lock);
	struct mtdram_check_overlap_query query = {
		.base = base,
		.size = size,

		.overlap_mtdram = false,
	};

	spin_lock(&static_cmdline_lock);
	strlcpy(static_cmdline, cmdline, sizeof(static_cmdline));
	parse_args("avm mtdram options", static_cmdline, NULL, 0, 0, 0, &query,
		   mtdram_check_overlap);
	spin_unlock(&static_cmdline_lock);

	return query.overlap_mtdram;
}
