/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2021 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/kernel.h>

#if IS_BUILTIN(CONFIG_SHM_RNG_SEED)
extern int early_init_shm_rng_seed(void);
#else
static inline int early_init_shm_rng_seed(void) { return 0; }
#endif
