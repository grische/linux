/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2021 AVM GmbH <fritzbox_info@avm.de> */

#ifndef _FW_AVM_DT_URLADER_ENV_H_
#define _FW_AVM_DT_URLADER_ENV_H_

#include <linux/kconfig.h>
#include <linux/of.h>

#include <stdbool.h>

#if IS_ENABLED(CONFIG_AVM_DT_URLADER_ENV)
static inline bool avm_dt_urlader_env_is_enabled(void)
{
	struct device_node *np;
	bool ret;

	np = of_find_compatible_node(NULL, NULL, "avm,dt-urlader-env");
	ret = np;
	of_node_put(np);

	return ret;
}
#else
static inline bool avm_dt_urlader_env_is_enabled(void)
{
	return false;
}
#endif

#endif
