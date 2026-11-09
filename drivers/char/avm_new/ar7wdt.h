/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AR7WDT_H_
#define _AR7WDT_H_

#include <linux/version.h>
#include "avm_sammel.h"

/**
 */
int __init ar7wdt_init(void);
void ar7wdt_cleanup(void);

#endif /*--- #ifndef _AR7WDT_H_ ---*/
