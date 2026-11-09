#ifndef _AVM_HW_CONFIG_DEF_H_
#define _AVM_HW_CONFIG_DEF_H_
//enums from avm_hw_config.h are stored here as defines for the device tree generation
#define AVM_DEF_HW_PARAM_NO_PARAM 0
#define AVM_DEF_HW_PARAM_GPIO_OUT_ACTIVE_LOW 10
#define AVM_DEF_HW_PARAM_GPIO_OUT_ACTIVE_HIGH 11
#define AVM_DEF_HW_PARAM_GPIO_IN_ACTIVE_LOW 12
#define AVM_DEF_HW_PARAM_GPIO_IN_ACTIVE_HIGH 13
#define AVM_DEF_HW_PARAM_S17_OUT_ACTIVE_LOW 14
#define AVM_DEF_HW_PARAM_S17_OUT_ACTIVE_HIGH 15

//enums from mach_avm.h are stored here as defines for the device tree generation
#define AVM_DEF_HW_FUNCTION_GPIO_PIN 0x0
#define AVM_DEF_HW_FUNCTION_PINMUX1 0x1
#define AVM_DEF_HW_FUNCTION_PINMUX2 0x2
#define AVM_DEF_HW_FUNCTION_PINMUX3 0x3
#define AVM_DEF_HW_FUNCTION_PIN_NOCHANGE 0x4

#endif
