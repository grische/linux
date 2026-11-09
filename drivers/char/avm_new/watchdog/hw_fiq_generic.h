/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2025 AVM GmbH <fritzbox_info@avm.de> */

#ifndef __HW_FIQ_GENERIC_H__
#define __HW_FIQ_GENERIC_H__

#include <linux/of.h>
#include <linux/types.h>

void nonsec_wdt_enable(bool enable);
void nonsec_wdt_reset(void);
void nonsec_wdt_set_timeout(unsigned int timeout);

void generic_ar7wdt_hw_init(void);
int generic_wdt_probe(struct device_node *np);
void generic_wdt_remove(void);
void unregister_early_restart_nb(void);

#endif
