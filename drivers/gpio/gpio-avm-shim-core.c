/* SPDX-License-Identifier: GPL-2.0-only */

#define pr_fmt(fmt) "[avm-gpio-shim] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <asm/mach_avm.h>
#include <linux/avm_hw_config.h>

#include "gpio-avm-shim.h"

static LIST_HEAD(shim_list);
static DEFINE_SPINLOCK(shim_list_lock);
static int next_free_pin;

#define FIND_SHIM_BY(prop, type, cond)					\
	static struct avm_gpio_shim *find_shim_by_ ## prop(type prop)	\
	{								\
		struct avm_gpio_shim *shim = NULL, *pos;		\
		unsigned long flags;					\
									\
		spin_lock_irqsave(&shim_list_lock, flags);		\
		list_for_each_entry(pos, &shim_list, list) {		\
			if (cond) {					\
				shim = pos;				\
				break;					\
			}						\
		}							\
		spin_unlock_irqrestore(&shim_list_lock, flags);		\
		return shim;						\
	}
FIND_SHIM_BY(name, const char *, !strcmp(pos->name, name))
FIND_SHIM_BY(pin, unsigned int, pos->pin == pin)
#undef FIND_SHIM_BY

int avm_get_hw_config(unsigned int version, const char *name,
		      int *p_value, enum _avm_hw_param *p_param)
{
	struct avm_gpio_shim *shim;

	if (version != AVM_HW_CONFIG_VERSION)
		return -EINVAL;

	if (!name)
		return -EINVAL;

	shim = find_shim_by_name(name);
	if (!shim)
		return -ENODEV;

	if (p_value)
		*p_value = shim->pin;

	if (p_param)
		pr_err("%s: ignoring p_param pointer!\n", __func__);

	return 0;
}
EXPORT_SYMBOL(avm_get_hw_config);

int avm_gpio_shim_register_shim(struct avm_gpio_shim *shim)
{
	unsigned long flags;

	if (!shim || !shim->name || !shim->ops)
		return -EINVAL;

	spin_lock_irqsave(&shim_list_lock, flags);

	shim->pin = next_free_pin++;
	list_add_tail(&shim->list, &shim_list);

	spin_unlock_irqrestore(&shim_list_lock, flags);

	return 0;
}

int avm_gpio_shim_out_bit(unsigned int gpio_pin, int val)
{

	struct avm_gpio_shim *shim = find_shim_by_pin(gpio_pin);

	if (!shim)
		return -ENODEV;

	if (!shim->ops->out_bit)
		return -EOPNOTSUPP;

	shim->ops->out_bit(shim, val);

	return 0;
}
EXPORT_SYMBOL(avm_gpio_shim_out_bit);

int avm_gpio_shim_in_bit(unsigned int gpio_pin)
{
	struct avm_gpio_shim *shim = find_shim_by_pin(gpio_pin);

	if (!shim)
		return -ENODEV;

	if (!shim->ops->in_bit)
		return -EOPNOTSUPP;

	return shim->ops->in_bit(shim);
}
EXPORT_SYMBOL(avm_gpio_shim_in_bit);

int avm_gpio_shim_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode,
		       enum _hw_gpio_direction pin_dir)
{
	struct avm_gpio_shim *shim = find_shim_by_pin(gpio_pin);

	if (!shim)
		return -ENODEV;

	if (!shim->ops->ctrl)
		return -EOPNOTSUPP;

	return shim->ops->ctrl(shim, pin_mode, pin_dir);
}
EXPORT_SYMBOL(avm_gpio_shim_ctrl);

void __iomem *avm_gpio_shim_get_inputreg_and_bit(unsigned int gpio_pin,
						 int *bit)
{
	struct avm_gpio_shim *shim = find_shim_by_pin(gpio_pin);

	if (!shim)
		return ERR_PTR(-ENODEV);

	if (!shim->ops->get_inputreg_and_bit)
		return ERR_PTR(-EOPNOTSUPP);

	return shim->ops->get_inputreg_and_bit(shim, bit);
}
EXPORT_SYMBOL(avm_gpio_shim_get_inputreg_and_bit);
