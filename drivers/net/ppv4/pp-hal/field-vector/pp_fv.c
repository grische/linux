/*
 * Description: PP field vector APIs
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018-2019 Intel Corporation
 */

#define pr_fmt(fmt) "[PP_FV]: %s:%d: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/if_ether.h>
#include <linux/ppp_defs.h>
#include <linux/pp_api.h>
#include <linux/learning_layer_api.h>

#include "pp_fv.h"

/**
 * @brief Helper function to convert learning field vector l2
 *        structure to PP field vector l2 structure
 */
static void __pp_learn_fv_l2_to_pp_fv_l2(struct fv_l2 *learn,
					 struct pp_fv_l2 *pp,
					 u32 bmap)
{
	memcpy(pp->h_dst, learn->h_dest, ETH_ALEN);
	memcpy(pp->h_src, learn->h_source, ETH_ALEN);
	pp->ext_vlan = learn->ext_vlan;
	pp->int_vlan = learn->int_vlan;
	pp->h_prot = learn->h_proto;
	pp->pppoe_id = learn->pppoe_id;
}

/**
 * @brief Helper function to convert learning field vector ipv4
 *        structure to PP field vector ipv4 structure
 */
static void __pp_learn_fv_ipv4_to_pp_fv_ipv4(struct fv_ipv4 *learn,
					     struct pp_fv_ipv4 *pp)
{
	pp->tos = learn->tos;
	pp->protocol = learn->protocol;
	pp->saddr = learn->saddr;
	pp->daddr = learn->daddr;
}

/**
 * @brief Helper function to convert learning field vector ipv6
 *        structure to PP field vector ipv6 structure
 */
static void __pp_learn_fv_ipv6_to_pp_fv_ipv6(struct fv_ipv6 *learn,
					     struct pp_fv_ipv6 *pp)
{
	pp->priority = learn->priority;
	pp->version = learn->version;
	memcpy(pp->flow_lbl, learn->flow_lbl, sizeof(learn->flow_lbl));
	memcpy(&pp->saddr, &learn->saddr, sizeof(pp->saddr));
	memcpy(&pp->daddr, &learn->daddr, sizeof(pp->daddr));
}

/**
 * @brief Helper function to convert learning field vector l3
 *        structure to PP field vector l3 structure
 */
static void __pp_learn_fv_l3_to_pp_fv_l3(union fv_l3 *learn,
					 union pp_fv_l3 *pp,
					 u32 bmap)
{
	if (bmap & FV_L3_IPV4)
		__pp_learn_fv_ipv4_to_pp_fv_ipv4(&learn->v4, &pp->v4);
	else if (bmap & FV_L3_IPV6)
		__pp_learn_fv_ipv6_to_pp_fv_ipv6(&learn->v6, &pp->v6);
}

/**
 * @brief Helper function to convert learning field vector l4
 *        structure to PP field vector l4 structure
 */
static void __pp_learn_fv_l4_to_pp_fv_l4(union fv_l4 *learn,
					 union pp_fv_l4 *pp,
					 u32 bmap)
{
	if (bmap & (FV_L4_TCP | FV_L4_UDP | FV_L4_SCTP)) {
		pp->tcp.src = learn->tcp.source;
		pp->tcp.dst = learn->tcp.dest;
	} else if (bmap & (FV_L4_ICMP4 | FV_L4_ICMP6)) {
		pp->icmp.code = learn->icmp.id;
		pp->icmp.type = learn->icmp.type;
	} else if (bmap & (FV_L4_ESP)) {
		pp->esp.spi = learn->esp.spi;
	} else if (bmap & (FV_L4_L2TPIP)) {
		pp->l2tpoip.sess_id = learn->l2tpoip.session_id;
	}
}

/**
 * @brief Helper function to convert learning field vector headers
 *        structure to PP field vector headers structure
 */
static void __pp_learn_fv_hdrs_to_pp_fv_hdrs(struct fv_headers *learn,
					     struct pp_fv_headers *pp,
					     u32 bmap)
{
	__pp_learn_fv_l2_to_pp_fv_l2(&learn->l2, &pp->l2, bmap);
	__pp_learn_fv_l3_to_pp_fv_l3(&learn->l3, &pp->l3, bmap);
	__pp_learn_fv_l4_to_pp_fv_l4(&learn->l4, &pp->l4, bmap);
}

/**
 * @brief Helper function to convert learning field vector tunnel
 *        structure to PP field vector tunnel structure
 */
static void __pp_learn_fv_tunn_to_pp_fv_tunn(union fv_tunnel *learn,
					     union pp_fv_tunnel *pp,
					     u32 bmap)
{
	if (bmap & FV_TUNNEL_VXLAN) {
		pp->vxlan.vni = learn->vxlan.vni;
	} else if (bmap & FV_TUNNEL_GNV) {
		memcpy(&pp->geneve.vni, &learn->geneve.vni,
		       sizeof(pp->geneve.vni));
	} else if (bmap & FV_TUNNEL_L2TPV2_UDP) {
		pp->l2tpoudp.sess_id = learn->l2tpoudp.session_id;
		pp->l2tpoudp.tunn_id = learn->l2tpoudp.tunnel_id;
	} else if (bmap & FV_TUNNEL_L2TPV3_UDP) {
		pp->l2tpv3oudp.sess_id = learn->l2tpv3oudp.session_id;
	}
}

s32 pp_learn_pkt_to_pp_fv(struct fv_pkt_info *pkt, struct pp_fv *pp)
{
	if (unlikely(!(!ptr_is_null(pkt) &&
		       !ptr_is_null(pp))))
		return -EINVAL;

	__pp_learn_fv_hdrs_to_pp_fv_hdrs(&pkt->fv.outer, &pp->first,
					 pkt->fv_outer_bitmap);
	__pp_learn_fv_tunn_to_pp_fv_tunn(&pkt->fv.tunnel, &pp->tunn,
					 pkt->fv_tunnel_bitmap);
	__pp_learn_fv_hdrs_to_pp_fv_hdrs(&pkt->fv.inner, &pp->second,
					 pkt->fv_inner_bitmap);

	return 0;
}

u8 pp_learn_pkt_to_pp_fv_proto(u32 pkt_bmap, u32 tunn_bmap)
{
	u8 proto = PP_FV_UNKNOWN_L3;
	u32 other_l4, tunn_other_l4;

	/* set other l4 protocols and tunnels */
	other_l4 = FV_L4_SCTP |
		   FV_L4_ESP |
		   FV_L4_ICMP4 |
		   FV_L4_ICMP6;
	tunn_other_l4 = FV_TUNNEL_L2TPV3_IP |
			FV_TUNNEL_GRE_ETH |
			FV_TUNNEL_GRE_IP |
			FV_TUNNEL_IPSEC;

	if (pkt_bmap & FV_L3_IPV4) {
		if (pkt_bmap & FV_L4_TCP)
			proto = PP_FV_IPV4_TCP;
		else if (pkt_bmap & FV_L4_UDP)
			proto = PP_FV_IPV4_UDP;
		else if (pkt_bmap & other_l4 || tunn_bmap & tunn_other_l4)
			proto = PP_FV_IPV4_OTHER_L4;
		else
			proto = PP_FV_IPV4_UNKNOWN_L4;
	} else if (pkt_bmap & FV_L3_IPV6) {
		if (pkt_bmap & FV_L4_TCP)
			proto = PP_FV_IPV6_TCP;
		else if (pkt_bmap & FV_L4_UDP)
			proto = PP_FV_IPV6_UDP;
		else if (pkt_bmap & other_l4 ||
			 tunn_bmap & tunn_other_l4)
			proto = PP_FV_IPV6_OTHER_L4;
		else
			proto = PP_FV_IPV6_UNKNOWN_L4;
	} else {
		if (pkt_bmap & FV_L4_TCP)
			proto = PP_FV_OTHER_L3_TCP;
		else if (pkt_bmap & FV_L4_UDP)
			proto = PP_FV_OTHER_L3_UDP;
		else if (pkt_bmap & other_l4 ||
			 tunn_bmap & tunn_other_l4)
			proto = PP_FV_OTHER_L3_OTHER_L4;
		else
			proto = PP_FV_UNKNOWN_L3;
	}

	return proto;
}