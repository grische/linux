#ifndef _AVM_HW_CONFIG_H_
#define _AVM_HW_CONFIG_H_

#define AVM_HW_CONFIG_VERSION 1

#include <linux/avm_hw_config_def.h>

enum _avm_hw_param {
	avm_hw_param_no_param = AVM_DEF_HW_PARAM_NO_PARAM,

	avm_hw_param_gpio_out_active_low = AVM_DEF_HW_PARAM_GPIO_OUT_ACTIVE_LOW,
	avm_hw_param_gpio_out_active_high = AVM_DEF_HW_PARAM_GPIO_OUT_ACTIVE_HIGH,
	avm_hw_param_gpio_in_active_low = AVM_DEF_HW_PARAM_GPIO_IN_ACTIVE_LOW,
	avm_hw_param_gpio_in_active_high = AVM_DEF_HW_PARAM_GPIO_IN_ACTIVE_HIGH,
	avm_hw_param_s17_out_active_low = AVM_DEF_HW_PARAM_S17_OUT_ACTIVE_LOW,
	avm_hw_param_s17_out_active_high = AVM_DEF_HW_PARAM_S17_OUT_ACTIVE_HIGH,

	avm_hw_param_gpio_out_rgb = AVM_DEF_HW_PARAM_GPIO_OUT_RGB,
	avm_hw_param_gpio_out_rgb_active_low = AVM_DEF_HW_PARAM_GPIO_OUT_RGB_ACTIVE_LOW,

	avm_hw_param_last_param
};

struct _avm_hw_config {
	char *name;
	int value;
	enum _avm_hw_param param;
};

struct _avm_hw_config_table {
	unsigned int hwrev;
	unsigned int hwsubrev;
	struct _avm_hw_config *table;
};

/*
 * Config-Wert name nicht gefunden:
 *  return -EINVAL, p_value & p_param nicht gültig
 * version != AVM_HW_CONFIG_VERSION:
 *  return -EINVAL, p_value & p_param nicht gültig
 * sonst:
 *  return 0
 */
int avm_get_hw_config(unsigned int version, const char *name, int *p_value,
		enum _avm_hw_param *p_param);

#endif /* _AVM_HW_CONFIG_H_ */
