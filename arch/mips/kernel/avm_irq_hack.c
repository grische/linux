#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

#define NUM_HACK_IRQS 16
static struct resource irqres[NUM_HACK_IRQS];
static int num_irqs;

int avm_get_hack_irq(uint32_t idx)
{
	if (idx >= num_irqs) {
		return -EINVAL;
	}

	return irqres[idx].start;
}
EXPORT_SYMBOL(avm_get_hack_irq);

static int irq_hack_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	num_irqs = of_irq_to_resource_table(node, irqres, NUM_HACK_IRQS);
	if (!num_irqs) {
		pr_err("%s: interrupts not found", __func__);
		return -ENODEV;
	}
	return 0;
}

static const struct of_device_id irq_hack_match[] = {
	{ .compatible = "avm,irq_hack" },
	{},
};
MODULE_DEVICE_TABLE(of, irq_hack_match);

static struct platform_driver irq_hack_driver = {
	.probe = irq_hack_probe,
	.driver = {
		.name = "avm,irq_hack",
		.of_match_table = irq_hack_match,
		.owner = THIS_MODULE,
	},
};
builtin_platform_driver(irq_hack_driver);
