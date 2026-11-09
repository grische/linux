# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2019 Intel Corporation.
obj-$(CONFIG_LANTIQ_ETH_FRAMEWORK) += lantiq_eth_framework.o
obj-$(CONFIG_INTEL_XPCS) += xpcs/
obj-$(CONFIG_LTQ_ETHSW_API) += switch-api/
obj-$(CONFIG_LTQ_DATAPATH) += ../../datapath/dpm/
obj-$(CONFIG_INTEL_UMT) += ../../datapath/umt/
obj-$(CONFIG_LTQ_TMU) += tmu/
obj-$(CONFIG_LTQ_CBM) += cqm/
obj-$(CONFIG_LTQ_ETH_XRX500) += ltq_eth_drv_xrx500.o
obj-$(CONFIG_INTEL_PON_HGU_VUNI) += intel_pon_hgu_vuni.o
obj-$(CONFIG_INTEL_GINT_ETH) += intel_gint_eth_drv.o
obj-$(CONFIG_XRX500_PHY_FW) += xrx500_phy_fw.o
obj-$(CONFIG_PPA) += ppa/
ifneq ($(CONFIG_PPA),y)
obj-$(CONFIG_QOS_MGR) += qos_mgr/
endif
#obj-$(CONFIG_INTEL_UMT_CQEM_MODE) += umt/umt.o
obj-$(CONFIG_LTQ_TOE_DRIVER) += ltq_toe_drv.o
obj-$(CONFIG_INTEL_MIB_COUNTERS) += ../../datapath/mib_counters/
obj-$(CONFIG_PON_QOS) += ../../datapath/pon_qos/
