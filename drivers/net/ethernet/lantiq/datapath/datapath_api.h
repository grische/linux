/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Grische <github@grische.xyz>
 *
 * Public datapath API types and entry points.
 */

#ifndef DATAPATH_API_PRIV_H
#define DATAPATH_API_PRIV_H

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/module.h>

#include "datapath_api_qos.h"
#include "datapath_api_gswip30.h"

/*
 * Bare-minimum forward declarations of types consumed by the four prototypes
 * below. Forward decls are sufficient here because every reference is by
 * pointer.
 */
struct dp_subif;
typedef struct dp_subif dp_subif_t;
struct dp_port_data;
struct dp_subif_data;
struct dp_dev_data;
struct cbm_dp_en_data;
struct dp_pmac_cfg;
typedef struct dp_pmac_cfg dp_pmac_cfg_t;

/* dp_alloc_port - allocate a datapath/PMAC port for a netdev module. */
int dp_alloc_port(struct module *owner, struct net_device *dev,
		  u32 dev_port, u32 port_id,
		  dp_pmac_cfg_t *pmac_cfg, u32 flags);

/* dp_alloc_port_ext - explicit-instance variant of dp_alloc_port. */
int dp_alloc_port_ext(int inst, struct module *owner, struct net_device *dev,
		      u32 dev_port, int port_id, dp_pmac_cfg_t *pmac_cfg,
		      struct dp_port_data *data, u32 flags);

/*
 * Frees the per-port state allocated by dp_alloc_port_private and clears
 * the dp_port_info[][port_id] slot back to PORT_FREE. Internally routes
 * through dp_alloc_port_ext with DP_F_DEREGISTER OR'd into @flags so the
 * locking and state-machine invariants match the alloc-side path exactly.
 *
 * Returns DP_SUCCESS on success, DP_FAILURE on any intermediate error
 * (same convention as dp_alloc_port_ext — NOT 0 / -errno).
 */
int dp_dealloc_port_ext(int inst, struct module *owner, struct net_device *dev,
			u32 dev_port, int port_id, dp_pmac_cfg_t *pmac_cfg,
			struct dp_port_data *data, u32 flags);

/*
 * dp_register_subif - register a sub-interface (CTP) under an allocated DP
 * port.
 */
int dp_register_subif(struct module *owner, struct net_device *dev,
		      char *subif_name, dp_subif_t *subif_id, u32 flags);

/*
 * dp_register_subif_ext - explicit-instance variant of dp_register_subif.
 */
int dp_register_subif_ext(int inst, struct module *owner,
			  struct net_device *dev, char *subif_name,
			  dp_subif_t *subif_id, struct dp_subif_data *data,
			  u32 flags);

#endif /* DATAPATH_API_PRIV_H */
