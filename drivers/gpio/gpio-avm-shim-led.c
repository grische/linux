/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm/mach_avm.h>
#include "../leds/leds.h"

#include "gpio-avm-shim.h"

struct avm_led_shim {
	struct avm_gpio_shim shim;
	struct led_trigger trigger;
	int val;
};

static int shim_led_out_bit(struct avm_gpio_shim *shim, int val)
{
	struct avm_led_shim *led_shim =
		container_of(shim, struct avm_led_shim, shim);
	struct led_classdev *led_cdev;
	unsigned long flags;

	led_shim->val = val;

	read_lock_irqsave(&led_shim->trigger.leddev_list_lock, flags);

	list_for_each_entry(led_cdev, &led_shim->trigger.led_cdevs, trig_list)
		led_set_brightness_nosleep(led_cdev, val);

	read_unlock_irqrestore(&led_shim->trigger.leddev_list_lock, flags);

	return 0;
}

static const struct avm_gpio_shim_ops avm_led_shim_ops = {
	.out_bit = shim_led_out_bit,
};

static void shim_led_led_activate(struct led_classdev *led_cdev)
{
	struct avm_led_shim *led_shim =
		container_of(led_cdev->trigger, struct avm_led_shim, trigger);

	led_set_brightness_nosleep(led_cdev, led_shim->val);
}

static int avm_gpio_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct avm_led_shim *led_shim;
	const char *name;
	int ret;

	led_shim = devm_kzalloc(dev, sizeof(*led_shim), GFP_KERNEL);
	if (!led_shim)
		return -ENOMEM;

	led_shim->shim.ops = &avm_led_shim_ops;

	if (of_property_read_string(dev->of_node, "name", &name))
		return -ENODEV;

	led_shim->shim.name = name;
	led_shim->trigger.name = name;
	led_shim->trigger.activate = shim_led_led_activate;

	ret = devm_led_trigger_register(dev, &led_shim->trigger);
	if (ret)
		return ret;

	return avm_gpio_shim_register_shim(&led_shim->shim);
}

static const struct of_device_id avm_gpio_led_match[] = {
	{ .compatible = "avm,avm_gpio_led" },
	{},
};
MODULE_DEVICE_TABLE(of, avm_gpio_led_match);

static struct platform_driver avm_gpio_led_driver = {
	.probe = avm_gpio_led_probe,
	.driver = {
			.name = "avm,avm_gpio_led",
			.owner = THIS_MODULE,
			.of_match_table = avm_gpio_led_match,
		},
};
builtin_platform_driver(avm_gpio_led_driver);
