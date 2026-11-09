/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __AVM_PROM_UAPI_H_
#define __AVM_PROM_UAPI_H_

#include <linux/types.h>

enum avm_prom_config_type {
	AVM_PROM_WLAN 		= 0,
	AVM_PROM_DECT 		= 1,
	AVM_PROM_WLAN2 		= 2,
	AVM_PROM_ZERTIFIKATE 	= 3,
	AVM_PROM_DOCSIS 	= 4,
	AVM_PROM_DSL 		= 5,
	AVM_PROM_PROLIFIC 	= 6,
	AVM_PROM_WLAN_ZIP 	= 7,
	AVM_PROM_WLAN2_ZIP 	= 8,
	AVM_PROM_ZERTIFIKATE2 	= 9,
	AVM_PROM_ZERTIFIKATE3 	= 10,
	AVM_PROM_ZERTIFIKATE4 	= 11,
	AVM_PROM_FINAL 		= 12,
	AVM_PROM_PRODCERT1 	= 13,
	AVM_PROM_PRODCERT2 	= 14,
	AVM_PROM_PRODCERT3 	= 15,
	AVM_PROM_PRODCERT4 	= 16,
	AVM_PROM_PRODENV 	= 17,
	AVM_PROM_WLAN3_ZIP 	= 18,
	AVM_PROM_WLAN3 		= 19,
	AVM_PROM_AVMZERTIFIKATE = 20,
	AVM_PROM_LRWPAN 	= 21,
	AVM_PROM_DTB_OVERLAY 	= 22,
	AVM_PROM_FIBER 		= 23,
	AVM_PROM_FIBER_ZIP 	= 24,
	AVM_PROM_WLAN4 		= 25,
	AVM_PROM_WLAN4_ZIP 	= 26,
	AVM_PROM_MAX_TYPE,
};

/*
 * The on-disk header format has changed from v2 to v3. Represents the initial
 * fields that are common to v2 and v3.
 */
struct avm_prom_config_hdr_head {
	__u8 version;
	__u8 type;
} __packed;

/* The on-disk header format, v2. Note that Urlader and ar7-manager use this. */
struct avm_prom_config_hdr {
	struct avm_prom_config_hdr_head head;
	__be16 				len;
} __packed;

/* The on-disk header format, v3. U-boot and Sonar use this. */
struct avm_prom_config_hdr_v3 {
	struct avm_prom_config_hdr_head head;
	__be16 				cal_options;
	__be32 				len;
} __packed;
/* Values for cal_options flags */
#define AVM_CAL_OPTION_COMPRESSED 	0x1	/* Data is compressed */

#endif
