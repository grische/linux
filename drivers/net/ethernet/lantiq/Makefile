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
intel-xrx500-gswip-y += datapath/datapath_misc.o datapath/datapath_api.o datapath/datapath_instance.o
intel-xrx500-gswip-y += cqm/grx500/cbm.o cqm/grx500/cbm_basic.o cqm/grx500/cbm_buf.o
intel-xrx500-gswip-y += cqm/grx500/cbm_intr.o cqm/grx500/fsqm.o cqm/grx500/cbm_dma.o
intel-xrx500-gswip-y += cqm/grx500/cbm_dp.o
intel-xrx500-gswip-y += cqm/grx500/cbm_ports.o
intel-xrx500-gswip-y += dma/hdma.o
intel-xrx500-gswip-y += tmu/tmu_init.o tmu/drv_tmu_ll.o

#
# The buffer-manager conduit netdev. Built in, never a module: its transmit
# path calls MIPS cache maintenance which is not exported to modules.
#
obj-$(CONFIG_INTEL_XRX500_CBM_CONDUIT) += xrx500-cbm.o
