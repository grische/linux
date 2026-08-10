/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014 ~ 2015 Lei Chuanhua <chuanhua.lei@lantiq.com>
 * Copyright (C) 2016 ~ 2017 Intel Corporation.
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * include/linux/dma/lantiq_dmax.h.
 *
 * DMA controller, port and channel encoding shared with the CBM and netdev
 * translation units.
 */
#ifndef LANTIQ_DMAX_H
#define LANTIQ_DMAX_H
#include <linux/types.h>
#include <linux/interrupt.h>

/*
 * DMA controller, port and channel encoding: 32 bits total as the
 * following layout:
 *  controller | port  | channel number
 *  31.......24|23...16|15...........0
 *
 * AVM lantiq_dmax.h:54-105 — verbatim. Silicon-honoured encoding;
 * never re-bias the shifts or widths or callers' channel handles break.
 */
#define _DMA_CHANBITS 16
#define _DMA_PORTBITS 8
#define _DMA_CTRLBITS 8

#define _DMA_CHANMASK ((1 << _DMA_CHANBITS) - 1)
#define _DMA_PORTMASK ((1 << _DMA_PORTBITS) - 1)
#define _DMA_CTRLMASK ((1 << _DMA_CTRLBITS) - 1)

#define _DMA_CHANSHIFT 0
#define _DMA_PORTSHIFT (_DMA_CHANSHIFT + _DMA_CHANBITS)
#define _DMA_CTRLSHIFT (_DMA_PORTSHIFT + _DMA_PORTBITS)

#define _DMA_C(controller, port, channel)	\
	(((controller) << _DMA_CTRLSHIFT) |	\
	 ((port) << _DMA_PORTSHIFT) |		\
	 ((channel) << _DMA_CHANSHIFT))

#define _DMA_CONTROLLER(nr) (((nr) >> _DMA_CTRLSHIFT) & _DMA_CTRLMASK)
#define _DMA_PORT(nr) (((nr) >> _DMA_PORTSHIFT) & _DMA_PORTMASK)
#define _DMA_CHANNEL(nr) (((nr) >> _DMA_CHANSHIFT) & _DMA_CHANMASK)

#define MAX_DMA_CHAN_PER_PORT 64
#define MAX_DMA_PORT_PER_CTRL 4

/*
 * enum dma_controller — keeps the full 7-entry numbering verbatim from AVM
 * lantiq_dmax.h:82-91 because the cid value is silicon-encoded (top 8 bits of
 * the _DMA_C() handle) and dma_name[] (hdma.c) indexes by cid.
 */
enum dma_controller {
	DMA0 = 0,
	DMA1TX,
	DMA1RX,
	DMA2TX,
	DMA2RX,
	DMA3,
	DMA4,
	DMAMAX,
};

enum dma_ctrl_port {
	DMA1TX_PORT = 0,
	DMA1RX_PORT = 0,
	DMA2TX_PORT = 0,
	DMA2RX_PORT = 0,
};

enum dma_endian {
	DMA_ENDIAN_TYPE0 = 0,
	DMA_ENDIAN_TYPE1,
	DMA_ENDIAN_TYPE2,
	DMA_ENDIAN_TYPE3,
	DMA_ENDIAN_MAX,
};

enum dma_burst {
	DMA_BURSTL_2DW = 1,
	DMA_BURSTL_4DW = 2,
	DMA_BURSTL_8DW = 3,
	DMA_BURSTL_16DW = 16,
	DMA_BURSTL_32DW = 32,
};

enum dma_pkt_drop {
	DMA_PKT_DROP_DISABLE = 0,
	DMA_PKT_DROP_ENABLE,
};

enum dma_channel {
	DMA_CHANNEL_0 = 0,
	DMA_CHANNEL_1,
	DMA_CHANNEL_2,
	DMA_CHANNEL_3,
	DMA_CHANNEL_4,
	DMA_CHANNEL_5,
	DMA_CHANNEL_6,
	DMA_CHANNEL_7,
	DMA_CHANNEL_8,
	DMA_CHANNEL_9,
	DMA_CHANNEL_10,
	DMA_CHANNEL_11,
	DMA_CHANNEL_12,
	DMA_CHANNEL_13,
	DMA_CHANNEL_14,
	DMA_CHANNEL_15,
	DMA_CHANNEL_16,
	DMA_CHANNEL_17,
	DMA_CHANNEL_18,
	DMA_CHANNEL_19,
	DMA_CHANNEL_20,
	DMA_CHANNEL_21,
	DMA_CHANNEL_22,
	DMA_CHANNEL_23,
	DMA_CHANNEL_24,
	DMA_CHANNEL_25,
	DMA_CHANNEL_26,
	DMA_CHANNEL_27,
	DMA_CHANNEL_28,
	DMA_CHANNEL_29,
	DMA_CHANNEL_30,
	DMA_CHANNEL_31,
	DMA_CHANNEL_32,
	DMA_CHANNEL_33,
	DMA_CHANNEL_34,
	DMA_CHANNEL_35,
	DMA_CHANNEL_36,
	DMA_CHANNEL_37,
	DMA_CHANNEL_38,
	DMA_CHANNEL_39,
	DMA_CHANNEL_40,
	DMA_CHANNEL_41,
	DMA_CHANNEL_42,
	DMA_CHANNEL_43,
	DMA_CHANNEL_44,
	DMA_CHANNEL_45,
	DMA_CHANNEL_46,
	DMA_CHANNEL_47,
	DMA_CHANNEL_48,
	DMA_CHANNEL_49,
	DMA_CHANNEL_50,
	DMA_CHANNEL_51,
	DMA_CHANNEL_52,
	DMA_CHANNEL_53,
	DMA_CHANNEL_54,
	DMA_CHANNEL_55,
	DMA_CHANNEL_56,
	DMA_CHANNEL_57,
	DMA_CHANNEL_58,
	DMA_CHANNEL_59,
	DMA_CHANNEL_60,
	DMA_CHANNEL_61,
	DMA_CHANNEL_62,
	DMA_CHANNEL_63,
};

/*
 * Per-controller channel handles — verbatim from AVM lantiq_dmax.h: lines
 * 240-262 (DMA1TX), 265-292 (DMA1RX), 295-310 (DMA2TX), 313-326 (DMA2RX).
 */

#ifndef SINGLE_RX_CH0_ONLY
#define SINGLE_RX_CH0_ONLY 1
#endif

/* DMA1TX */
#define DMA1TX_LAN_SWITCH_CLASS0	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_0)

#define DMA1TX_LAN_SWITCH_CLASS1	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_1)
#define DMA1TX_LAN_SWITCH_CLASS2	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_2)
#define DMA1TX_LAN_SWITCH_CLASS3	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_3)

#define DMA1TX_CHAN4_RESERV _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_4)
#define DMA1TX_LOOP_FCS_REGEN _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_5)
#define DMA1TX_CHAN6_RESERV _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_6)

#define DMA1TX_EXT_WLAN_PCIE_CLASS7	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_7)
#define DMA1TX_INTERNAL_WLAN_CLASS8	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_8)

#define DMA1TX_USB_LAN_CLASS9 _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_9)
#define DMA1TX_USB_LAN_CLASS10 _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_10)
#define DMA1TX_CHAN11_RESERV _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_11)
#define DMA1TX_USB_WAN_CLASS12 _DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_12)

#define DMA1TX_DSL_WAN_CBMP18_CLASS13	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_13)
#define DMA1TX_DMA1RX_CH14_CLASS14	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_14)
#define DMA1TX_GSWIP_R_WAN_CBMP19_CLASS15	\
	_DMA_C(DMA1TX, DMA1TX_PORT, DMA_CHANNEL_15)

/* DMA1RX */
#define DMA1RX_TMU_CLASS0 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_0)
#define DMA1RX_TMU_CLASS1 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_1)
/*
 * EQM port 8 std-buf RX-admit channel on A21 silicon (FRITZ!Box 7560). The
 * cbm_config.c row encodes the un-remapped std channel 15, but AVM sets
 * cbm_rev=2 unconditionally (cbm.c:5597) before configure_ports(), which
 * activates the A21 std-channel remap in conf_eqm_dma_port (cbm.c:5297-5302):
 * for dma_ctrl==1 (DMA1RX), `dma_chnl = (dma_chnl == 0) ? 0 : 5`. So std
 * channel 15 is rewritten to 5 — AVM arms DMA1RX channel 5 for EQM port 8,
 * not 15. Use this post-remap handle, NOT DMA1RX_CBM_P8_CLASS15.
 */
#define DMA1RX_CBM_P8_CLASS5 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_5)
#define DMA1RX_CBM_P10_CLASS9 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_9)
#define DMA1RX_CBM_P11_CLASS10 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_10)
#define DMA1RX_CBM_P12_CLASS11 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_11)
#define DMA1RX_CBM_P13_CLASS12 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_12)
#define DMA1RX_CBM_P14_CLASS13 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_13)
#define DMA1RX_DMA1TX_CH14_CLASS14	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_14)
#define DMA1RX_CBM_P8_CLASS15 _DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_15)
#define DMA1RX_CBM_P8_CLASS16_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_16)
#define DMA1RX_CBM_P10_CLASS25_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_25)
#define DMA1RX_CBM_P11_CLASS26_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_26)
#define DMA1RX_CBM_P12_CLASS27_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_27)
#define DMA1RX_CBM_P13_CLASS28_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_28)
#define DMA1RX_CBM_P14_CLASS29_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_29)
#define DMA1RX_CBM_P7_CLASS6_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_6)
#define DMA1RX_CBM_P8_CLASS11_JUMBO	\
	_DMA_C(DMA1RX, DMA1RX_PORT, DMA_CHANNEL_11)

/* DMA2TX */
#define DMA2TX_CBM_P6_CLASS0 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_0)
#define DMA2TX_CBM_P6_CLASS1 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_1)
#define DMA2TX_CBM_P7_CLASS2 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_2)
#define DMA2TX_CBM_P8_CLASS3 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_3)
#define DMA2TX_CBM_P9_CLASS4 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_4)
#define DMA2TX_CBM_P10_CLASS5 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_5)
#define DMA2TX_CBM_P11_CLASS6 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_6)
#define DMA2TX_CBM_P12_CLASS9 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_9)
#define DMA2TX_CBM_P13_CLASS10 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_10)
#define DMA2TX_CBM_P14_CLASS11 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_11)
#define DMA2TX_CBM_P15_CLASS12 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_12)
#define DMA2TX_CBM_P16_CLASS13 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_13)
#define DMA2TX_CBM_P17_CLASS14 _DMA_C(DMA2TX, DMA2TX_PORT, DMA_CHANNEL_14)

/* DMA2RX */
#define DMA2RX_GSWIP_R_CLASS0 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_0)
#define DMA2RX_GSWIP_R_CLASS1 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_1)
#define DMA2RX_GSWIP_R_CLASS2 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_2)
#define DMA2RX_GSWIP_R_CLASS3 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_3)

#define DMA2RX_GSWIP_R_CLASS4 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_4)
#define DMA2RX_GSWIP_R_CLASS5 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_5)
#define DMA2RX_GSWIP_R_CLASS6 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_6)

#define DMA2RX_CBMP5_CLASS14 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_14)
#define DMA2RX_CBMP6_CLASS15 _DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_15)
#define DMA2RX_CBMP5_CLASS30_JUMBO	\
	_DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_30)
#define DMA2RX_CBMP6_CLASS31_JUMBO	\
	_DMA_C(DMA2RX, DMA2RX_PORT, DMA_CHANNEL_31)

enum dma_pseudo_irq {
	RCV_INT = 1,
	TX_BUF_FULL_INT = 2,
	TRANSMIT_CPT_INT = 4,
};

/*
 * Buffer / IRQ callback typedefs — verbatim from AVM lantiq_dmax.h.
 */
typedef char *(*buffer_alloc_t)(int len, int *byte_offset, void **opt);
typedef int (*buffer_free_t)(char *dataptr, void *opt);
typedef int (*intr_handler_t)(u32 lnr, void *priv, int flags);

int ltq_request_dma(u32 chan, const char *device_id);
int ltq_free_dma(u32 chan);
int ltq_dma_chan_on(u32 chan);
int ltq_dma_chan_off(u32 chan);
int ltq_dma_chan_open(u32 chan);
int ltq_dma_chan_close(u32 chan);
int ltq_dma_chan_reset(u32 chan);
int ltq_dma_chan_pktsize_cfg(u32 chan, size_t pktsize);
int ltq_dma_chan_desc_alloc(u32 chan, u32 desc_num);
int ltq_dma_chan_desc_free(u32 chan);
int ltq_dma_chan_data_buf_alloc(u32 chan);
int ltq_dma_chan_data_buf_free(u32 chan);
int ltq_dma_chan_irq_enable(u32 chan);
int ltq_dma_chan_irq_disable(u32 chan);
int ltq_dma_chan_pkt_drop_cfg(u32 chan, int enable);
int ltq_dma_chan_byte_offset_cfg(u32 chan, u32 boff_len);
int ltq_dma_chan_class_cfg(u32 chan, u32 cls);
int ltq_dma_chan_pseudo_irq_handler_callback_cfg(u32 chan,
						 intr_handler_t handler,
						 void *priv);
int ltq_dma_chan_desc_cfg(u32 chan, dma_addr_t desc_base, int desc_num);
dma_addr_t ltq_dma_chan_get_curr_desc_addr(u32 chan);

/*
 * Opens the per-controller channel set CBM EQM port_id is wired to.
 *
 *   ndo_open:
 *     hdma_port_enable(port_id)
 *       -> cbm_dp_enable(... 0 ...)        // TRANSITIVE via
 *       -> dp_register_subif(...)          //    dp_register_subif_private
 *
 *   ndo_stop:
 *     dp_deregister_subif(...)             // TRANSITIVE
 *       -> cbm_dp_enable(... CBM_PORT_F_DISABLE ...)
 *     hdma_port_disable(port_id)
 *
 * The reverse-order pairing (enable: hdma -> dp; disable: dp -> hdma)
 * ensures DMA channels are still held open while the datapath unprograms
 * the CTP / BridgePort / QIDT entries that reference them.
 */
int hdma_port_enable(int port_id);

int hdma_ig192_toe_dma3_reset(void);

int ltq_dma_chan_sw_poll_cfg(u32 chan, int enable);
int hdma_swtx_init(u32 chan, int n, int buf_size);
int hdma_swtx_xmit(u32 chan, const void *data, int len);

int ltq_dma_p2p_cfg(u32 rx_chan, u32 tx_chan);

/*
 * Releases the DMA channels that hdma_port_enable(port_id) opened, in reverse
 * order: ltq_dma_chan_close -> ltq_dma_chan_desc_free -> ltq_free_dma per
 * channel.
 *
 * Returns 0 on success; non-zero on partial failure (each failing step
 * is logged via pr_err and teardown continues for the remaining
 * channels so a single stuck channel does not strand the others).
 */
int hdma_port_disable(int port_id);

#endif /* LANTIQ_DMAX_H */
