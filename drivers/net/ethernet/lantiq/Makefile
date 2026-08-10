# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the Intel xRX500 ethernet driver suite.
#
# switch-api/, datapath/ and cqm/grx500/ all contribute objects to one
# composite module, so the object list lives here rather than in the
# subdirectory Makefiles: symbol resolution between them stays
# intra-module and needs no exports.

obj-$(CONFIG_INTEL_XRX500_GSWIP_API) += intel-xrx500-gswip.o

intel-xrx500-gswip-y := switch-api/gsw_init.o switch-api/gsw_flow_core.o switch-api/gsw_flow_pce.o

#
# The GPHY firmware is proprietary and lives in the rootfs, so the loader
# is kept out of the kernel image and gets a module of its own.
#
obj-$(CONFIG_INTEL_XRX500_MDIO) += intel-xrx500-mdio.o

intel-xrx500-mdio-y := xrx500_phy_fw.o
