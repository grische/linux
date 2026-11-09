/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm/mach_avm.h>

#include "gpio-avm-shim.h"

#define PINCTRL_STATE_START 1
#define NUM_PINCTRL_STATES 3
struct avm_gpiod_shim {
	struct avm_gpio_shim shim;
	struct device *dev;
	struct gpio_desc *desc;
	struct pinctrl *pinctrl;
	struct pinctrl_state *states[NUM_PINCTRL_STATES];
};

static const char * const state_names[] = {
	"mux1",
	"mux2",
	"mux3"
};

static struct avm_gpiod_shim *shim_to_gpiod_shim(struct avm_gpio_shim *shim)
{
	return container_of(shim, struct avm_gpiod_shim, shim);
}

static int shim_gpiod_out_bit(struct avm_gpio_shim *shim, int val)
{
	struct gpio_desc *desc = shim_to_gpiod_shim(shim)->desc;

	if (!desc)
		return -EIO;

	gpiod_set_value(desc, !!val);

	return 0;
}

static int shim_gpiod_in_bit(struct avm_gpio_shim *shim)
{
	struct gpio_desc *desc = shim_to_gpiod_shim(shim)->desc;

	if (!desc)
		return -EIO;

	return gpiod_get_value(desc);
}

static int shim_gpiod_set_pinctrl_state(struct avm_gpiod_shim *gpiod_shim,
					enum _hw_gpio_function pin_mode)
{
	struct pinctrl_state *state;
	unsigned int stateidx = pin_mode - PINCTRL_STATE_START;

	if (stateidx >= NUM_PINCTRL_STATES)
		return -EINVAL;

	state = gpiod_shim->states[stateidx];
	if (!gpiod_shim->pinctrl || !state)
		return -EINVAL;

	return pinctrl_select_state(gpiod_shim->pinctrl, state);
}

static int shim_gpiod_ctrl(struct avm_gpio_shim *shim,
			   enum _hw_gpio_function pin_mode,
			   enum _hw_gpio_direction pin_dir)
{
	struct avm_gpiod_shim *gpiod_shim = shim_to_gpiod_shim(shim);
	struct gpio_desc *desc = gpiod_shim->desc;

	switch (pin_mode) {
	case GPIO_PIN:
		if (!desc) {
			desc = devm_gpiod_get(gpiod_shim->dev, NULL,
					GPIOD_ASIS);

			if (IS_ERR(desc))
				return PTR_ERR(desc);

			gpiod_shim->desc = desc;
		}
		/* fall-through */

	case FUNCTION_PIN_NOCHANGE:
		if (!desc)
			return -EIO;

		switch (pin_dir) {
		case GPIO_INPUT_PIN:
			return gpiod_direction_input(desc);

		case GPIO_OUTPUT_PIN:
			return gpiod_direction_output(desc, 0);

		default:
			return -EINVAL;
		}
		break;

	case FUNCTION_PINMUX1:
	case FUNCTION_PINMUX2:
	case FUNCTION_PINMUX3:
		gpiod_shim->desc = NULL;

		if (desc)
			devm_gpiod_put(gpiod_shim->dev, desc);

		return shim_gpiod_set_pinctrl_state(gpiod_shim, pin_mode);

	default:
		return -EINVAL;
	}
}

static void __iomem *shim_gpiod_get_inputreg_and_bit(struct avm_gpio_shim *shim,
						     int *bit)
{
	struct gpio_desc *desc = shim_to_gpiod_shim(shim)->desc;

	if (!desc)
		return NULL;

	return gpiod_avm_get_inputreg_and_bit(desc, bit);
}

static const struct avm_gpio_shim_ops avm_gpiod_shim_ops = {
	.out_bit             = shim_gpiod_out_bit,
	.in_bit              = shim_gpiod_in_bit,
	.ctrl                = shim_gpiod_ctrl,
	.get_inputreg_and_bit = shim_gpiod_get_inputreg_and_bit,
};

static int avm_gpio_generic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct avm_gpiod_shim *gpiod_shim;
	struct pinctrl *pinctrl;
	enum _hw_gpio_direction dir;
	int input, output;
	int i, ret;

	gpiod_shim = devm_kzalloc(dev, sizeof(*gpiod_shim), GFP_KERNEL);
	if (!gpiod_shim)
		return -ENOMEM;

	gpiod_shim->dev      = dev;
	gpiod_shim->shim.ops = &avm_gpiod_shim_ops;

	if (of_property_read_string(node, "name", &gpiod_shim->shim.name))
		return -ENODEV;

	pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(pinctrl)) {
		for (i = 0; i < NUM_PINCTRL_STATES; i++) {
			struct pinctrl_state *state =
				pinctrl_lookup_state(pinctrl, state_names[i]);

			if (!IS_ERR(state))
				gpiod_shim->states[i] = state;
		}

		gpiod_shim->pinctrl = pinctrl;
	}

	input = of_property_read_bool(node, "avm,input");
	output = of_property_read_bool(node, "avm,output");
	if (input && !output) {
		dir = GPIO_INPUT_PIN;
	} else if (!input && output) {
		dir = GPIO_OUTPUT_PIN;
	} else {
		pr_err("avm_gpio %s: direction not specified\n", gpiod_shim->shim.name);
		return -ENODEV;
	}

	ret = shim_gpiod_ctrl(&gpiod_shim->shim, GPIO_PIN, dir);
	if (ret)
		return -EPROBE_DEFER;

	return avm_gpio_shim_register_shim(&gpiod_shim->shim);
}

static const struct of_device_id avm_gpio_generic_match[] = {
	{ .compatible = "avm,avm_gpio_generic" },
	{},
};
MODULE_DEVICE_TABLE(of, avm_gpio_generic_match);

static struct platform_driver avm_gpio_generic_driver = {
	.probe = avm_gpio_generic_probe,
	.driver = {
			.name = "avm,avm_gpio_generic",
			.owner = THIS_MODULE,
			.of_match_table = avm_gpio_generic_match,
		},
};
builtin_platform_driver(avm_gpio_generic_driver);
