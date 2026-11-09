// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2021 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/kdebug.h>

struct ctx {
	struct device *dev;

	struct gpio_desc *gpio_in;
	struct gpio_desc *gpio_out;

	unsigned int enabled;
	u32 sync;

	struct notifier_block panic_notifier;
};

static int companion_other_panic_maybe(struct ctx *ctx)
{
	int enabled, state;

	enabled = READ_ONCE(ctx->enabled);
	if (!enabled)
		return 0;

	state = gpiod_get_value(ctx->gpio_in);
	if (state < 0)
		return state;

	BUG_ON(state);

	return 0;
}

static irqreturn_t companion_other_panic_irq_handler(int irq, void *ctx)
{
	WARN_ON(companion_other_panic_maybe(ctx) < 0);
	return IRQ_HANDLED;
}

static int companion_self_panic_notify(struct notifier_block *nb,
				       unsigned long event,
				       void *unused)
{
	struct ctx *ctx = container_of(nb, struct ctx, panic_notifier);
	u32 i;

	gpiod_set_value(ctx->gpio_out, 1);

	if (ctx->sync) {
		/* wait for the companion to panic until we restart and the
		 * GPIO goes back to 0
		 */
		for (i = 0; i < ctx->sync && !gpiod_get_value(ctx->gpio_in); ++i)
			udelay(1000);
		WARN_ON(i == ctx->sync);
	}

	return NOTIFY_DONE;
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct ctx *ctx = platform_get_drvdata(pdev);
	int enabled;

	enabled = READ_ONCE(ctx->enabled);
	return sprintf(buf, "%d [0 1]\n", enabled);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct ctx *ctx = platform_get_drvdata(pdev);
	long enabled;
	int rv;

	rv = kstrtol(buf, 0, &enabled);
	if (rv != 0 || (enabled & (~1)))
		return -EINVAL;

	WRITE_ONCE(ctx->enabled, enabled);
	if (enabled)
		companion_other_panic_maybe(ctx);
	return len;
}

static DEVICE_ATTR_RW(enabled);

static int companion_panic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ctx *ctx;
	int irq, rv;

	ctx = devm_kcalloc(dev, 1, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->dev = dev;
	if (of_property_read_u32(dev->of_node, "avm,sync", &ctx->sync))
		ctx->sync = 0;
	ctx->enabled = !of_property_read_bool(dev->of_node,
					      "avm,default-disabled");
	platform_set_drvdata(pdev, ctx);

	ctx->gpio_out = devm_gpiod_get(dev, "out", 0);
	if (IS_ERR(ctx->gpio_out))
		return PTR_ERR(ctx->gpio_out);

	rv = gpiod_direction_output(ctx->gpio_out, 0);
	if (rv < 0)
		return rv;

	ctx->panic_notifier = (struct notifier_block) {
		.notifier_call = companion_self_panic_notify,
		.priority = 256,
	};
	atomic_notifier_chain_register(&panic_notifier_list,
				       &ctx->panic_notifier);

	ctx->gpio_in = devm_gpiod_get(dev, "in", 0);
	if (IS_ERR(ctx->gpio_in))
		return PTR_ERR(ctx->gpio_in);

	rv = sysfs_create_file(&pdev->dev.kobj, &dev_attr_enabled.attr);
	if (rv < 0)
		return rv;

	rv = gpiod_direction_input(ctx->gpio_in);
	if (rv < 0)
		return rv;

	irq = gpiod_to_irq(ctx->gpio_in);
	if (irq < 0)
		return rv;

	rv = devm_request_irq(dev, irq, companion_other_panic_irq_handler,
			      IRQF_TRIGGER_RISING | IRQF_SHARED, pdev->name,
			      ctx);
	if (rv < 0)
		return rv;

	rv = companion_other_panic_maybe(ctx);
	if (rv < 0)
		return rv;

	return 0;
}

static const struct of_device_id of_match_tbl[] = {
	{ .compatible = "avm,companion-panic" },
	{ },
};
MODULE_DEVICE_TABLE(of, of_match_tbl);

static struct platform_driver plat_driver = {
	.probe = companion_panic_probe,
	.driver = {
		.name = "avm-companion-panic",
		.of_match_table = of_match_tbl,
	},
};
module_platform_driver(plat_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Nixdorf <j.nixdorf@avm.de>");
MODULE_DESCRIPTION("Synchroize panics between companion CPUs via GPIOs.");
