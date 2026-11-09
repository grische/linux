/**
 * enhanced avm-tdm-features needed by pcmlink
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/interrupt.h>

#include <asm/ltq_tdm.h>

static struct clk *tdm_clk;
static struct resource tdm_irqres[2];

/**
 * idx: 0  frame-irq
 *      1  pcm-irq
 */
int tdm_get_irqdata(unsigned int idx,
		    unsigned int *irq_nr, unsigned int *irq_type)
{
	if (idx >= ARRAY_SIZE(tdm_irqres)) {
		if (irq_type)
			*irq_type = -1;
		if (irq_nr)
			*irq_nr = -1;
		return -EINVAL;
	}
	if (irq_type)
		*irq_type = tdm_irqres[idx].flags & IRQF_TRIGGER_MASK;
	if (irq_nr)
		*irq_nr   = tdm_irqres[idx].start;

	pr_err("%s(%d): irq %d flags %ld\n", __func__, idx, tdm_irqres[idx].start, tdm_irqres[idx].flags);
	return 0;
}
EXPORT_SYMBOL(tdm_get_irqdata);

/**
 */
int tdm_clk_enable(unsigned int enable)
{
	int ret = 0;

	if (!tdm_clk || IS_ERR(tdm_clk)) {
		pr_err("%s: can't get clk to enable pmu %p\n", __func__, tdm_clk);
		return -EINVAL;
	}
	if (enable) {
		ret = clk_prepare_enable(tdm_clk);
	} else {
		clk_disable_unprepare(tdm_clk);
	}
	return ret;
}
EXPORT_SYMBOL(tdm_clk_enable);

/**
 */
static int tdm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

	tdm_clk = of_clk_get(node, 0);

	if (of_irq_to_resource_table(node, tdm_irqres, 2) < 2) {
		pr_err("%s: irqs not found\n", __func__);
	}
	return 0;
}

static const struct of_device_id tdm_match[] = {
	{ .compatible = "avm,ltq_tdm" },
	{},
};

MODULE_DEVICE_TABLE(of, tdm_match);

static struct platform_driver tdm_driver = {
	.probe = tdm_probe,
	.driver = {
		.name = "avm,ltq_tdm",
		.of_match_table = tdm_match,
		.owner = THIS_MODULE,
	},
};
builtin_platform_driver(tdm_driver);
