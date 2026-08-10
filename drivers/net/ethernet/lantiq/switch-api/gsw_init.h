/* SPDX-License-Identifier: GPL-2.0-only */
/******************************************************************************
                Copyright (c) 2016, 2017 Intel Corporation

******************************************************************************/
/*****************************************************************************
                Copyright (c) 2012, 2014, 2015
                    Lantiq Deutschland GmbH
******************************************************************************/


#ifndef _ETHSW_INIT_H_
#define _ETHSW_INIT_H_

#define SWAPI_DRV_VERSION "3.0.1"

/* Switch Features  */
#define CONFIG_LTQ_STP 1
#define CONFIG_LTQ_8021X 1
#define CONFIG_LTQ_MULTICAST 1
#define CONFIG_LTQ_QOS 1
#define CONFIG_LTQ_VLAN 1
#define CONFIG_LTQ_WOL 1
#define CONFIG_LTQ_PMAC 1
#define CONFIG_LTQ_RMON 1

#define CONFIG_MAC 1

/* User configuration options */

#define SMDIO_INTERFACE 0
#define GSW_IOCTL_SUPPORT 1

#define ENABLE_MICROCODE 0
#define PTR_TO_INT_CAST void *

//#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/interrupt.h>
//#include <asm/delay.h> */
#include <linux/delay.h>
#include <linux/slab.h>
/*
 * <linux/ethtool.h> must come before "gsw_dev.h": the ported switch-api
 * headers use struct ethtool_* types in prototypes without declaring them,
 * and GCC otherwise warns that they are declared inside the parameter list.
 */
#include <linux/ethtool.h>

#include "lantiq_gsw_api.h"

#include "gsw_dev.h"
#include <linux/netdevice.h>
#include "gsw_tbl_rw.h"

/*#include <xway/switch-api/lantiq_gsw_routing.h>*/
/*#include <xway/switch-api/gsw_types.h>*/

#define LTQ_GSW_DEV_MAX 3

/*include*/
#include "gsw_flow_core.h"
#include "gsw_ll_func.h"
#include "gsw_reg.h"
#include "gsw_reg_top.h"
#include "gsw30_reg_top.h"

#define GSWIP_GET_BITS(x, msb, lsb) \
	(((x) & ((1 << ((msb) + 1)) - 1)) >> (lsb))
#define GSWIP_SET_BITS(x, msb, lsb, value) \
	(((x) & ~(((1 << ((msb) + 1)) - 1) ^ ((1 << (lsb)) - 1))) | (((value) & ((1 << (1 + (msb) - (lsb))) - 1)) << (lsb)))

#define GSW_API_MODULE_NAME "GSW SWITCH API"
#define GSW_API_DRV_VERSION "3.0.2"
#define MICRO_CODE_VERSION "212"

typedef struct {
	void *ecdev;
	void *pdev;
	gsw_devtype_t sdev;
	void *gsw_base_addr;
} ethsw_core_init_t;

void gsw_r32(void *cdev, short offset, short shift, short size, u32 *value);
void gsw_w32(void *cdev, short offset, short shift, short size, u32 value);

void gsw_r32_raw(void *cdev, short offset, u32 *value);
void gsw_w32_raw(void *cdev, short offset, u32 value);

/*
 * Tagged with the namespaced GPL export macro (namespace "GSWIP_INTERNAL") in
 * gsw_flow_core.c.
 */
struct core_ops *gsw_get_swcore_ops(u32 devid);

static inline u32 gsw_field_r32(u32 rval, short shift, short size)
{
	return (rval >> shift) & ((1 << size) - 1);
}

static inline u32 gsw_field_w32(u32 rval, short shift, short size, u32 val)
{
	u32 mask;

	mask = ((1 << size) - 1) << shift;
	val = (val << shift) & mask;
	return (rval & ~mask) | val;
}

static inline ethsw_api_dev_t *GSW_PDATA_GET(void *pdev)
{
	struct core_ops *gsw_ops;
	ethsw_api_dev_t *pdata = NULL;

	if (pdev == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return pdata;
	}

	gsw_ops = (struct core_ops *)pdev;

	if (gsw_ops == NULL) {
		pr_err("%s:%s:%d", __FILE__, __func__, __LINE__);
		return pdata;
	}

	pdata = container_of(gsw_ops, ethsw_api_dev_t, ops);
	return pdata;
}

typedef enum {
	IRQ_REGISTER = 0,
	IRQ_UNREGISTER = 1,
	IRQ_ENABLE = 2,
	IRQ_DISABLE = 3,
} IRQ_TYPE;

#endif /* _ETHSW_INIT_H_ */
