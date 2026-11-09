// SPDX-License-Identifier: GPL-2.0+

#include <asm/io.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <soc/qcom/socinfo.h>
#include <avm/sammel/kpi.h>


static u32 chip_values[8];

static u32 dakota_reg_entry(void *value)
{
	return *((u32 *)value);
}

static int die_notifier(struct notifier_block *self, unsigned long cmd,
			void *ptr)
{
	pr_err("Chip specific values: 0:[0x%x] 1:[0x%x] 2:[0x%x] 3:[0x%x] 4[0x%x] 5[0x%x] 6[0x%x] 7[0x%x]\n",
					chip_values[0], chip_values[1], chip_values[2], chip_values[3],
					chip_values[4], chip_values[5], chip_values[6], chip_values[7]);

	return NOTIFY_OK;
}
static struct notifier_block die_nb = {
	.notifier_call = die_notifier,
	.priority = 0,
};

static int __init arch_dakota_kpi_init(void)
{
	struct kpi_node *kpi_chip_node;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "qcom,ipq4019");
	if (node) {
		int i;
		void __iomem *io_mem;

		kpi_chip_node = kpi_create_dict("chip",
						kpi_get_section("system"));

		io_mem = ioremap(0x58000, sizeof(u32) * 4);
		for (i = 0; i < 4; i++) {
			chip_values[i] = ioread32(io_mem);
			io_mem += sizeof(u32);
		}
		iounmap(io_mem);

		io_mem = ioremap(0x583F0, sizeof(u32) * 4);
		for (i = 0; i < 4; i++) {
			chip_values[4 + i] = ioread32(io_mem);
			io_mem += sizeof(u32);
		}
		iounmap(io_mem);

		// QFPROM_RAW_JTAG_ID
		kpi_add_attr_u32("value0", kpi_chip_node, dakota_reg_entry, &chip_values[0]);
		// unknown
		kpi_add_attr_u32("value1", kpi_chip_node, dakota_reg_entry, &chip_values[1]);
		// QFPROM_RAW_SERIAL_NUM
		kpi_add_attr_u32("value2", kpi_chip_node, dakota_reg_entry, &chip_values[2]);
		// factory information
		kpi_add_attr_u32("value3", kpi_chip_node, dakota_reg_entry, &chip_values[3]);
		// unknown
		kpi_add_attr_u32("value4", kpi_chip_node, dakota_reg_entry, &chip_values[4]);
		// unknown
		kpi_add_attr_u32("value5", kpi_chip_node, dakota_reg_entry, &chip_values[5]);
		// unknown
		kpi_add_attr_u32("value6", kpi_chip_node, dakota_reg_entry, &chip_values[6]);
		// unknown
		kpi_add_attr_u32("value7", kpi_chip_node, dakota_reg_entry, &chip_values[7]);

		register_die_notifier(&die_nb);
	}

	return 0;
}
late_initcall(arch_dakota_kpi_init);


