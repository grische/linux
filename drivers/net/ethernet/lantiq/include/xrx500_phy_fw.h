/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 John Crispin <blogic@openwrt.org>
 * Copyright (C) 2016 Intel Corporation
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/xrx500_phy_fw.c and xrx500_phy_fw.h.
 *
 * Interface between the GPHY firmware loader and the MDIO bus driver,
 * which ship in one module.
 */

#ifndef _LANTIQ_XRX500_PHY_FW_H_
#define _LANTIQ_XRX500_PHY_FW_H_

#include <linux/platform_device.h>
#include <linux/types.h>

/**
 * xrx500_gphy_fw_is_loaded() - report GPHY firmware load status.
 *
 * Returns true once the xrx500_phy_fw platform_driver's probe path has
 * successfully completed (firmware fetched via request_firmware, copied
 * into a 16 KiB-aligned dma_alloc_coherent buffer, programmed into the
 * GSW-L GPHY*_LBADR/MBADR register windows, and all five reset lines
 * deasserted). Returns false before that point or after a probe failure.
 */
bool xrx500_gphy_fw_is_loaded(void);

/*
 * Registered from the loader's module init, because a module defines
 * init_module once and both drivers live in this one.
 */
extern struct platform_driver ltq_gswip_mdio_driver;

#endif /* _LANTIQ_XRX500_PHY_FW_H_ */
