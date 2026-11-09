// SPDX-License-Identifier: GPL-2.0

#ifndef __PROM_CONFIG_INTERNAL_H__
#define __PROM_CONFIG_INTERNAL_H__

#include <avm/enh/prom_config.h>

extern int avm_prom_create_config_proc(enum avm_prom_config_type type);
extern int avm_prom_create_config_debugfs(enum avm_prom_config_type type);

#endif
