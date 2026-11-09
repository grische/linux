// SPDX-License-Identifier: GPL-2.0
#ifndef __AVM_PROM_CONFIG_LEGACY_H__
#define __AVM_PROM_CONFIG_LEGACY_H__

#include <linux/module.h>

#define AVM_MAX_CONFIG_ENTRIES  8
#define AVM_CALIB_WLAN3
#define HAS_AVMZERTIFIKATE

#define wlan_dect_type avm_prom_config_type

// Provides legacy names.
#define WLAN 			AVM_PROM_WLAN
#define DECT 			AVM_PROM_DECT
#define WLAN2 			AVM_PROM_WLAN2
#define ZERTIFIKATE 		AVM_PROM_ZERTIFIKATE
#define DOCSIS 			AVM_PROM_DOCSIS
#define DSL 			AVM_PROM_DSL
#define PROLIFIC 		AVM_PROM_PROLIFIC
#define WLAN_ZIP 		AVM_PROM_WLAN_ZIP
#define WLAN2_ZIP 		AVM_PROM_WLAN2_ZIP
#define ZERTIFIKATE2 		AVM_PROM_ZERTIFIKATE2
#define ZERTIFIKATE3 		AVM_PROM_ZERTIFIKATE3
#define ZERTIFIKATE4 		AVM_PROM_ZERTIFIKATE4
#define FINAL 			AVM_PROM_FINAL
#define PRODCERT1 		AVM_PROM_PRODCERT1
#define PRODCERT2 		AVM_PROM_PRODCERT2
#define PRODCERT3 		AVM_PROM_PRODCERT3
#define PRODCERT4 		AVM_PROM_PRODCERT4
#define PRODENV 		AVM_PROM_PRODENV
#define WLAN3_ZIP 		AVM_PROM_WLAN3_ZIP
#define WLAN3 			AVM_PROM_WLAN3
#define AVMZERTIFIKATE 		AVM_PROM_AVMZERTIFIKATE
#define LRWPAN 			AVM_PROM_LRWPAN
#define DTB_OVERLAY 		AVM_PROM_DTB_OVERLAY
#define FIBER 			AVM_PROM_FIBER
#define FIBER_ZIP 		AVM_PROM_FIBER_ZIP
#define WLAN4 			AVM_PROM_WLAN4
#define WLAN4_ZIP 		AVM_PROM_WLAN4_ZIP
#define AVM_PROM_NUM_TYPES 	AVM_PROM_MAX_TYPE

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
