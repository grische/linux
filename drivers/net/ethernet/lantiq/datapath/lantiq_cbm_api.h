/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * include/net/lantiq_cbm_api.h.
 *
 * CBM constants and the dequeue-resource lookup shared inside the composite
 * module.
 */

#ifndef __CBM_API_H
#define __CBM_API_H

#include <linux/types.h>
#include <linux/bits.h>

#ifndef SINGLE_RX_CH0_ONLY
#define SINGLE_RX_CH0_ONLY 1
#endif

/*
 * CBM return / error codes (AVM lantiq_cbm_api.h:78-82).
 */
#define CBM_OK 0
#define CBM_ERROR (-1)

/*
 * Flag bits for struct cbm_dp_en_data and dp_alloc_complete (AVM
 * lantiq_cbm_api.h around the CBM_PORT_* macro block). These two bits
 * gate the cbm_dp_enable HW-interaction path inside dp_register_subif:
 * if neither is set, the call short-circuits.
 */
#define CBM_PORT_DP_SET BIT(0)
#define CBM_PORT_DQ_SET BIT(1)
#define CBM_PORT_DMA_CHAN_SET BIT(2)
#define CBM_PORT_PKT_CRDT_SET BIT(3)
#define CBM_PORT_BYTE_CRDT_SET BIT(4)
#define CBM_PORT_RING_ADDR_SET BIT(5)
#define CBM_PORT_RING_SIZE_SET BIT(6)
#define CBM_PORT_RING_OFFSET_SET BIT(7)

/*
 * Maximum CPU ports used by the CBM CPU-port enumeration. Ported as-is
 * from AVM because struct cbm_cpu_port_data carries an array of this
 * length.
 */
#define CQM_MAX_CPU 4

/* CBM DP allocate completion data block - returned by cbm_dp_port_alloc. */
struct cbm_dp_alloc_data {
	int dp_inst;
	int cbm_inst;
	u32 flags;
	u32 dp_port;
	u32 deq_port_num;
	u32 deq_port;
	u32 dma_chan;
	u32 tx_pkt_credit;
	u32 tx_b_credit;
	u32 tx_ring_addr;
	u32 tx_ring_size;
	u32 tx_ring_offset;
	u32 num_dma_chan;
};

typedef struct cbm_dp_alloc_data cbm_dp_alloc_complete_t;

/*
 * The .flags field is the one tested via (flags & (CBM_PORT_DP_SET |
 * CBM_PORT_DQ_SET)) inside dp_register_subif_private.
 */
struct cbm_dp_en_data {
	int dp_inst;
	int cbm_inst;
	u32 deq_port;
	u32 dma_chnl_init;
	u32 num_dma_chan;
};

#ifndef DP_F_DEQ_CPU
#define DP_F_DEQ_CPU 0x2
#define DP_F_DEQ_CPU1 0x3
#define DP_F_DEQ_MPE 0x4
#define DP_F_DEQ_DL 0x5
#endif

#ifndef DP_F_FAST_ETH_LAN
#define DP_F_FAST_ETH_LAN	BIT(1)
#define DP_F_FAST_ETH_WAN	BIT(2)
#define DP_F_FAST_WLAN		BIT(3)
#define DP_F_FAST_DSL		BIT(4)
#define DP_F_DIRECT		BIT(5)
#define DP_F_LOOPBACK		BIT(6)
#define DP_F_DIRECTLINK		BIT(7)
#define DP_F_MPE_ACCEL		BIT(25)
#define DP_F_CHECKSUM		BIT(26)
#define DP_F_DIRECTPATH_RX	BIT(27)
#define DP_F_DONTCARE		BIT(28)
#define DP_F_LRO		BIT(29)
#define DP_F_FAST_DSL_DOWNSTREAM BIT(30)
#define DP_F_PORT_TUNNEL_DECAP	DP_F_LOOPBACK
#endif

/*
 * TMU resource structure for a TMU port — AVM include/net/
 * lantiq_cbm_api.h:475-480 verbatim.
 */
typedef struct cbm_tmu_res {
	uint32_t tmu_port; /*!< TMU port number, -1 if no TMU port allocated */
	uint32_t cbm_deq_port; /*!< CBM Dequeue port number, -1 if no CBM port allocated */
	int32_t tmu_sched; /*!< TMU scheduler Id attached to TMU port, -1 if unassigned */
	int32_t tmu_q; /*!< TMU Queue Id attached to TMU scheduler on this TMU port, -1 if unassigned */
} cbm_tmu_res_t;

s32 cbm_dp_port_resources_get(u32 *dp_port, u32 *num_tmu_ports,
			      cbm_tmu_res_t **res_pp, u32 flags);

#endif /* __CBM_API_H */
