// SPDX-License-Identifier: GPL-2.0
#ifndef _GPIO_AVM_SHIM
#define _GPIO_AVM_SHIM 1

#include <linux/list.h>
#include <linux/avm_hw_config.h>

struct avm_gpio_shim;

struct avm_gpio_shim_ops {
	int (*out_bit)(struct avm_gpio_shim *shim, int val);
	int (*in_bit)(struct avm_gpio_shim *shim);
	int (*ctrl)(struct avm_gpio_shim *shim, enum _hw_gpio_function pin_mode,
		    enum _hw_gpio_direction pin_dir);
	void __iomem *(*get_inputreg_and_bit)(struct avm_gpio_shim *shim,
					      int *bit);
};

struct avm_gpio_shim {
	/* provided by driver */
	const char *name;
	const struct avm_gpio_shim_ops *ops;
	/* internal for core */
	int pin;
	struct list_head list;
};

int avm_gpio_shim_register_shim(struct avm_gpio_shim *shim);

#endif
