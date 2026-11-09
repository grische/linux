// SPDX-License-Identifier: GPL-2.0
#ifndef __AVM_PROM_CONFIG_LEGACY_H__
#define __AVM_PROM_CONFIG_LEGACY_H__

#include <linux/module.h>

#include <avm/enh/prom_config.h>

#define AVM_MAX_CONFIG_ENTRIES  8
#define AVM_CALIB_WLAN3
#define HAS_AVMZERTIFIKATE

#define wlan_dect_type avm_prom_config_type

// While this was the on disk format, this struct is now explicitly different from the
// on-disk format.
// The on-disk format struct can be found in prom_config.c: ondisk_config_hdr
struct wlan_dect_config {
	unsigned char        Version;    /*--- z.Z. 1 ---*/
	enum  wlan_dect_type Type;       /*--- 0 - WLAN; 1 - DECT ---*/
	unsigned short       Len;        /*--- 384 - WLAN, 128 - DECT ---*/
};

extern int get_wlan_dect_config(enum wlan_dect_type Type, unsigned char *buffer, unsigned int len);

#endif
