/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2021 AVM GmbH <fritzbox_info@avm.de> */

#ifndef _SHM_RNG_SEED_H
#define _SHM_RNG_SEED_H

#ifdef CONFIG_SHM_RNG_SEED
extern int shm_rng_seed_wait_all(void);
#else
static inline int shm_rng_seed_wait_all(void) { return 0; }
#endif

#endif
