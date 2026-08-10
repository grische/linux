/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014 ~ 2015 Lei Chuanhua <chuanhua.lei@lantiq.com>
 * Copyright (C) 2016 ~ 2017 Intel Corporation.
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/dma/intel/hdma.c.
 *
 * xRX500 DMA controller register offsets and field encodings.
 */
#ifndef __XRX500_HDMA_REGS_H
#define __XRX500_HDMA_REGS_H

#include <linux/bits.h>
#include <linux/types.h>

/* Field shift/mask helpers — AVM hdma.c:28-29 verbatim. */
#define MS(_v, _f) (((_v) & (_f)) >> _f##_S)
#define SM(_v, _f) (((_v) << _f##_S) & (_f))

#define DMA_CLC 0x0000

#define DMA_ID 0x0008
#define DMA_ID_REV 0xFFu
#define DMA_ID_REV_S 0
#define DMA_ID_ID 0xFF00u
#define DMA_ID_ID_S 8
#define DMA_ID_PRTNR 0xF0000u
#define DMA_ID_PRTNR_S 16
#define DMA_ID_CHNR 0x7F00000u
#define DMA_ID_CHNR_S 20

#define DMA_CTRL 0x0010
#define DMA_CTRL_RST BIT(0)
#define DMA_CTRL_DSRAM_PATH BIT(1)
#define DMA_CTRL_CH_FL BIT(6)
#define DMA_CTRL_DS_FOD BIT(7)
#define DMA_CTRL_DRB BIT(8)
#define DMA_CTRL_ENBE BIT(9)
#define DMA_CTRL_PRELOAD_INT_S 10
#define DMA_CTRL_PRELOAD_INT 0x0C00u
#define DMA_CTRL_PRELOAD_EN BIT(12)
#define DMA_CTRL_MBRST_CNT_S 16
#define DMA_CTRL_MBRST_CNT 0x3FF0000u
#define DMA_CTRL_MBRSTARB BIT(30)
#define DMA_CTRL_PKTARB BIT(31)

#define DMA_CPOLL 0x0014
/*
 * CPOLL has no standalone enable bit below bit 31: CPOLL_CNT occupies bits
 * 4-15, so a poll count of 8 reads back as 0x80 (hdma.c:58-61).
 */
#define DMA_CPOLL_CNT_S 4
#define DMA_CPOLL_CNT 0xFFF0u
#define DMA_CPOLL_EN BIT(31)

#define DMA_CGBL 0x0030
#define DMA_CGBL_GBL_S 0
#define DMA_CGBL_GBL 0xFFFFu

#define DMA_CS 0x0018
#define DMA_CS_MASK 0x3Fu

#define DMA_CCTRL 0x001C
#define DMA_CCTRL_ON BIT(0)
#define DMA_CCTRL_RST BIT(1)
#define DMA_CCTRL_CH_POLL_EN BIT(2)	/* DMA V3 */
#define DMA_CCTRL_DIR_TX BIT(8)
#define DMA_CCTRL_CLASS_S 9
#define DMA_CCTRL_CLASS 0xE00u
#define DMA_CCTRL_PRTNR_S 12
#define DMA_CCTRL_PRTNR 0xF000u
#define DMA_CCTRL_TXWGT_S 16
#define DMA_CCTRL_TXWGT 0x30000u
#define DMA_CCTRL_CLASSH_S 18
#define DMA_CCTRL_CLASSH 0xC0000u
#define DMA_CCTRL_TXWGT2 BIT(20)	/* DMA V3 */
#define DMA_CCTRL_PDEN BIT(23)
#define DMA_CCTRL_P2PCPY BIT(24)
#define DMA_CCTRL_LBEN BIT(25)
#define DMA_CCTRL_LBCHNR_S 26
#define DMA_CCTRL_LBCHNR 0xFC000000u
#define DMA_MAX_CLASS 31 /* 5 bits */

#define DMA_CDBA 0x0020

#define DMA_CDLEN 0x0024
#define DMA_CDLEN_CDLEN_S 0
#define DMA_CDLEN_CDLEN 0xFFFu

#define DMA_CIS 0x0028
#define DMA_CIE 0x002C

#define DMA_CI_EOP BIT(1)
#define DMA_CI_DUR BIT(2)
#define DMA_CI_DESCPT BIT(3)
#define DMA_CI_CHOFF BIT(4)
#define DMA_CI_RDERR BIT(5)
#define DMA_CI_ALL (DMA_CI_EOP | DMA_CI_DUR | DMA_CI_DESCPT | \
		    DMA_CI_CHOFF | DMA_CI_RDERR)

#define DMA_CI_DEFAULT (DMA_CI_EOP | DMA_CI_DESCPT)

#define DMA_CDPTNR 0x0034
#define DMA_PS 0x0040
#define DMA_PS_PS_S 0
#define DMA_PS_PS 0xFu

#define DMA_PCTRL 0x0044
#define DMA_PCTRL_RXBL16 BIT(0)
#define DMA_PCTRL_TXBL16 BIT(1)
#define DMA_PCTRL_RXBL_S 2
#define DMA_PCTRL_RXBL 0xCu
#define DMA_PCTRL_TXBL_S 4
#define DMA_PCTRL_TXBL 0x30u
#define DMA_PCTRL_PDEN BIT(6)
#define DMA_PCTRL_RXBL32 BIT(7)
#define DMA_PCTRL_PDEN_S 6
#define DMA_PCTRL_RXENDI 0x300u
#define DMA_PCTRL_RXENDI_S 8
#define DMA_PCTRL_TXENDI 0xC00u
#define DMA_PCTRL_TXENDI_S 10
#define DMA_PCTRL_TXWGT 0x7000u
#define DMA_PCTRL_TXWGT_S 12
#define DMA_PCTRL_TXBL32 BIT(15)
#define DMA_PCTRL_MEM_FLUSH BIT(16)
/*
 * Bit 29 of the per-channel control word is DMA_DBG_P2D_RME in DMA_PCTRL
 * (hdma.c:140), not a writable DMA_CTRL bit.
 */
#define DMA_DBG_P2D_CLS 0x1E0000u
#define DMA_DBG_P2D_CLS_S 17
#define DMA_DBG_P2D_ACK BIT(21)
#define DMA_DBG_D2P_CLS 0x3C00000u
#define DMA_DBG_D2P_CLS_S 22
#define DMA_DBG_D2P_ACK BIT(26)
#define DMA_DBG_P2D_REQ BIT(27)
#define DMA_DBG_D2P_REQ BIT(28)
#define DMA_DBG_P2D_RME BIT(29)
#define DMA_DBG_D2P_XME BIT(30)
#define DMA_DBG_P2D_JMB BIT(31)

#define DMA_CPDCNT 0x0080

#define DMA_IRNEN 0x00F4
#define DMA_IRNCR 0x00F8
#define DMA_IRNICR 0x00FC
#define DMA_IRNEN1 0x00E8
#define DMA_IRNCR1 0x00EC
#define DMA_IRNICR1 0x00F0

/* DMA V3.0 new registers — AVM hdma.c:154-209 verbatim. */
#define DMA_C_DP_TICK 0x100

#define DMA_C_DP_TICK_TIKNARB_S 0
#define DMA_C_DP_TICK_TIKNARB 0xFFFFu
#define DMA_C_DP_TICK_TIKARB_S 16
#define DMA_C_DP_TICK_TIKARB 0xFFFF0000u

#define DMA_C_HDRM 0x110

#define DMA_C_HDRM_HDR_LEN_S 0
#define DMA_C_HDRM_HDR_LEN 0xFFu
#define DMA_C_HDRM_EN BIT(31)

#define DMA_C_BOFF 0x120

#define DMA_C_BOFF_BOF_LEN_S 0
#define DMA_C_BOFF_BOF_LEN 0xFFu
#define DMA_C_BOFF_EN BIT(31)

#define DMA_C_INTCO 0x140
#define DMA_C_INTCO_COAL_LEN_S 0
#define DMA_C_INTCO_COAL_LEN 0xFFu
#define DMA_C_INTCO_TO_LEN 0xFF00u
#define DMA_C_INTCO_TO_LEN_S 8
#define DMA_C_INTCO_EN BIT(31)
#define DMA_C_INTCO_TO_EN BIT(30)
#define DMA_C_INTCO_TO_ST BIT(29)
#define DMA_C_INTCO_TO 0x1FFF0000
#define DMA_C_INTCO_TO_S 16

#define DMA_C_INTCO_PEND 0x0000FF00
#define DMA_C_INTCO_PEND_S 8

#define DMA_C_SWPOLL 0x150
#define DMA_C_SWPOLL_MUX BIT(30)
#define DMA_C_SWPOLL_EN BIT(31)

#define DMA_ORRC 0x190
#define DMA_ORRC_ORRCNT_S 4

#define DMA_ORRC_ORRCNT 0x1F0u
#define DMA_ORRC_EN BIT(31)

#define DMA_LOG_CH 0x194
#define DMA_LOG_CH_NR_S 0
#define DMA_LOG_CH_NR 0x3Fu

#define DMA_C_ENDIAN 0x200

#define DMA_C_END_DATAENDI_S 0
#define DMA_C_END_DATAENDI 0x3u
#define DMA_C_END_DE_EN BIT(7)

#define DMA_C_END_DESENDI_S 8
#define DMA_C_END_DESENDI 0x300u
#define DMA_C_END_DES_EN BIT(16)

#endif /* __XRX500_HDMA_REGS_H */
