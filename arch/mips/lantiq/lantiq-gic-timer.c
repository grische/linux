/**
 * enhanced avm-features needed by profiler
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/irqchip/mips-gic.h>
#include <linux/interrupt.h>

#include <asm/ltq_gic_timer.h>

static unsigned int gic_timer_irq = -1;
static unsigned int gic_frequency;

/**
 */
int gic_timer_get_irq(unsigned int *irq_nr)
{
	if (gic_timer_irq == -1) {
		return -1;
	}

	if (irq_nr)
		*irq_nr = gic_timer_irq;

	pr_err("%s: irq %d\n", __func__, gic_timer_irq);

	return 0;
}
/*--- EXPORT_SYMBOL(gic_timer_get_irq); ---*/

/**
 */
static __init int gic_timer_probe(struct device_node *node)
{
	struct clk *clk;

	pr_err("%s\n", __func__);
	if (IS_ERR_OR_NULL(node) || !gic_present || !node->parent ||
	    !of_device_is_compatible(node->parent, "mti,gic")) {
		pr_warn("No DT definition for the mips gic driver");
		return -ENXIO;
	}

	clk = of_clk_get(node, 0);
	if (!IS_ERR(clk)) {
		if (clk_prepare_enable(clk) < 0) {
			pr_err("GIC failed to enable clock\n");
			clk_put(clk);
			return PTR_ERR(clk);
		}

		gic_frequency = clk_get_rate(clk);
	} else if (of_property_read_u32(node, "clock-frequency",
					&gic_frequency)) {
		pr_err("GIC frequency not specified.\n");
		return -EINVAL;
	}
	gic_timer_irq = irq_of_parse_and_map(node, 0);
	if (!gic_timer_irq) {
		pr_err("GIC timer IRQ not specified.\n");
		return -EINVAL;
	}
	pr_err("%s: irq=%u freq=%u\n", __func__, gic_timer_irq, gic_frequency);
	return 0;
}

int __init gic_timer_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mti,gic-timer");
	return gic_timer_probe(node);
}
arch_initcall(gic_timer_init);
