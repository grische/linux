/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012 Lantiq Deutschland GmbH
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * include/net/drv_tmu_ll.h and tmu/drv_tmu_reg.h.
 *
 * TMU register offsets and the intra-module TMU interface.
 */

#ifndef _DRV_TMU_LL_H_
#define _DRV_TMU_LL_H_

#include <linux/types.h>

/*
 * GSWIP-variant rejection guard. A misconfigured defconfig must fail at
 * preprocess time, not at link time.
 */
#if defined(CONFIG_SOC_GSWIP_3_1) || defined(CONFIG_SOC_GRX550)
#error "this TMU port supports GSWIP-3.0/GRX350 only - GRX550/GSWIP-3.1 register layouts diverge"
#endif

/*
 * GRX500-family architectural constants
 *
 * Values are ported VERBATIM from AVM include/net/drv_tmu_ll.h:29-44.
 */

/* AVM include/net/drv_tmu_ll.h:43 — maximum scheduler hierarchy depth */
#define SCHEDULE_MAX_LEVEL              8

#define PACKET_POINTER_TABLE_INDEX_MAX  6144

/* AVM include/net/drv_tmu_ll.h:33 — total egress queue IDs */
#define EGRESS_QUEUE_ID_MAX             256

#define EGRESS_PORT_ID_MAX              72

#define SCHEDULER_BLOCK_INPUT_ID_MAX    (128 * SCHEDULE_MAX_LEVEL)

/* AVM include/net/drv_tmu_ll.h:36 — total scheduler blocks */
#define SCHEDULER_BLOCK_ID_MAX          128

#define TOCKEN_BUCKET_ID                256

/* AVM include/net/drv_tmu_ll.h:41 — sentinel "no scheduler block" handle */
#define NULL_SCHEDULER_BLOCK_ID         127

/* AVM include/net/drv_tmu_ll.h:42 — sentinel "no scheduler input" handle */
#define NULL_SCHEDULER_INPUT_ID         1023

/* AVM include/net/drv_tmu_ll.h:37 — sentinel "no egress port" handle */
#define EPNNULL_EGRESS_PORT_ID          72

/* AVM include/net/drv_tmu_ll.h:84 (and drv_tmu_ll.c:1034-1038 GRX500 comment)
 * — default EP tail-drop threshold (0x120 << 3 = 2304 segments).
 */
#define TMU_EPTT0_DEFAULT               0x120

/* AVM include/net/drv_tmu_ll.h:78 — default global tail-drop threshold base
 * (9216-frame normal FSQM * 66 / 100). Exposed so the basic-init helper has
 * a compile-time constant for the GOTHR loop.
 */
#define TMU_FSQM_FRAME_MAX              9216
#define TMU_GOTH_DEFAULT                (TMU_FSQM_FRAME_MAX * 66 / 100)

/* AVM include/net/drv_tmu_ll.h:46 — alias for the highest TBID value */
#define TOKEN_BUCKET_MAX                (TOCKEN_BUCKET_ID - 1)

/*
 * Sub-init default values
 *
 * AVM include/net/drv_tmu_ll.h:55-105 — defaults consumed by tmu_basic_init
 * and tmu_egress_queue_table_init.
 */

#define TMU_WRED_CRAWLER_PERIOD_DEFAULT 10

/* AVM include/net/drv_tmu_ll.h:70 — Enqueue Request Delay default. */
#define TMU_ENQUEUE_REQUEST_DELAY_DEFAULT 0

/* AVM include/net/drv_tmu_ll.h:73 — Token Accumulation period default. */
#define TMU_TOKEN_ACC_PERIOD_DEFAULT    0

/* AVM include/net/drv_tmu_ll.h:88 — queue threshold MITH default. */
#define TMU_QTHT1_DEFAULT               (((TMU_EPTT0_DEFAULT) / 8) / 2)

/* AVM include/net/drv_tmu_ll.h:92 — queue threshold MATH default. */
#define TMU_QTHT2_DEFAULT               ((TMU_EPTT0_DEFAULT) / 8)

/* AVM include/net/drv_tmu_ll.h:95 — WRED slope default. */
#define TMU_QTHT3_WRED_DEFAULT          0x0fff

/* AVM include/net/drv_tmu_ll.h:101-103 — tail-drop QTHT4 defaults
 * (unassigned col / red col).
 */
#define TMU_QTHT4_0_DEFAULT             0x0004
#define TMU_QTHT4_1_DEFAULT             0x0000

/*
 * TMU register-window offsets
 *
 * All offsets are derived from the struct tmu_reg layout at AVM
 * drv_tmu_reg.h:55-476.
 *
 * Naming convention: TMU_<reg>_OFFSET — matches the offset suffix used
 * elsewhere in cqm/grx500/cbm_regs.h.
 */

/* Core control / status — AVM drv_tmu_reg.h:55-93 */
#define TMU_CTRL_OFFSET                 0x000
#define TMU_FPL_OFFSET                  0x004
#define TMU_FPCR_OFFSET                 0x008
#define TMU_FPTHR_OFFSET                0x00C
#define TMU_TIMER_OFFSET                0x010
#define TMU_LFSR_OFFSET                 0x014
#define TMU_CPR_OFFSET                  0x018
#define TMU_CSR_OFFSET                  0x01C
#define TMU_GOCCR_OFFSET                0x020

/* Global occupancy / discard thresholds — AVM drv_tmu_reg.h:96-103 */
#define TMU_GOTHR0_OFFSET               0x030
#define TMU_GOTHR1_OFFSET               0x034   /* (gothr[1]) */
#define TMU_GOTHR2_OFFSET               0x038   /* (gothr[2]) */
#define TMU_GOTHR3_OFFSET               0x03C   /* (gothr[3]) */
#define TMU_GPDCR0_OFFSET               0x040
#define TMU_GPDCR1_OFFSET               0x044   /* (gpdcr[1]) */
#define TMU_GPDCR2_OFFSET               0x048   /* (gpdcr[2]) */
#define TMU_GPDCR3_OFFSET               0x04C   /* (gpdcr[3]) */

/* Queue / egress-port fill status — AVM drv_tmu_reg.h:109-113 */
#define TMU_QFILL_OFFSET                0x060
#define TMU_EPFR_OFFSET                 0x080

/* Low-power-idle config — AVM drv_tmu_reg.h:116-123
 * (Indices 0..3 map to LAN0..LAN3.)
 */
#define TMU_LPIC0_OFFSET                0x0A0
#define TMU_LPIC1_OFFSET                0x0A4   /* (lpic[1]) */
#define TMU_LPIC2_OFFSET                0x0A8   /* (lpic[2]) */
#define TMU_LPIC3_OFFSET                0x0AC   /* (lpic[3]) */
#define TMU_LPIT0_OFFSET                0x0B0
#define TMU_LPIT1_OFFSET                0x0B4   /* (lpit[1]) */
#define TMU_LPIT2_OFFSET                0x0B8   /* (lpit[2]) */
#define TMU_LPIT3_OFFSET                0x0BC   /* (lpit[3]) */

/* IRN / capture registers — AVM drv_tmu_reg.h:145-165 */
#define TMU_IRNCR_OFFSET                0x100
#define TMU_IRNICR_OFFSET               0x104
#define TMU_IRNEN_OFFSET                0x108
#define TMU_TBIDCR_OFFSET               0x110

/* Queue mapping / threshold / occupancy / discard / fifo / control —
 * AVM drv_tmu_reg.h:171-249.
 */
#define TMU_QEMT_OFFSET                 0x200
#define TMU_QSMT_OFFSET                 0x210
#define TMU_QTHT0_OFFSET                0x220
#define TMU_QTHT1_OFFSET                0x224
#define TMU_QTHT2_OFFSET                0x228
#define TMU_QTHT3_OFFSET                0x22C
#define TMU_QTHT4_OFFSET                0x230
#define TMU_QOCT0_OFFSET                0x240
#define TMU_QOCT1_OFFSET                0x244
#define TMU_QOCT2_OFFSET                0x248
#define TMU_QDCT0_OFFSET                0x250
#define TMU_QDCT1_OFFSET                0x254
#define TMU_QDCT2_OFFSET                0x258
#define TMU_QDCT3_OFFSET                0x25C
#define TMU_QFMT0_OFFSET                0x260
#define TMU_QFMT1_OFFSET                0x264
#define TMU_QFMT2_OFFSET                0x268
#define TMU_QMTC_OFFSET                 0x270

/* Egress port — AVM drv_tmu_reg.h:255-297 */
#define TMU_EPMT_OFFSET                 0x300
#define TMU_EPOT0_OFFSET                0x310
#define TMU_EPOT1_OFFSET                0x314
#define TMU_EPTT0_OFFSET                0x320
#define TMU_EPTT1_OFFSET                0x324
#define TMU_EPDT0_OFFSET                0x330
#define TMU_EPDT1_OFFSET                0x334
#define TMU_EPDT2_OFFSET                0x338
#define TMU_EPDT3_OFFSET                0x33C
#define TMU_EPMTC_OFFSET                0x340

/* Scheduler-block input / output table registers — AVM drv_tmu_reg.h:303-335 */
#define TMU_SBITR0_OFFSET               0x400
#define TMU_SBITR1_OFFSET               0x404
#define TMU_SBITR2_OFFSET               0x408
#define TMU_SBITR3_OFFSET               0x40C
#define TMU_SBITC_OFFSET                0x410
#define TMU_SBOTR0_OFFSET               0x420
#define TMU_SBOTR1_OFFSET               0x424
#define TMU_SBOTC_OFFSET                0x430
/* SBOTR2 / SBOTR3 — not present in GRX500 struct layout (struct tmu_reg
 * stops at sbotr1). GSWIP-3.1 SKIP: SBOTR2, SBOTR3.
 */

/* QFILL / EPFR arrays — AVM drv_tmu_reg.h:109-113 (these are arrays of
 * uint32_t; tmu_basic_init iterates 8 / 3 entries respectively).
 */
#define TMU_QFILL_BASE_OFFSET           0x060   /* (qfill[0]) */
#define TMU_EPFR_BASE_OFFSET            0x080   /* (epfr[0]) */

/* Token-bucket shaper table — AVM drv_tmu_reg.h:341-395 */
#define TMU_TBSTR0_OFFSET               0x500
#define TMU_TBSTR1_OFFSET               0x504
#define TMU_TBSTR2_OFFSET               0x508
#define TMU_TBSTR3_OFFSET               0x510
#define TMU_TBSTR4_OFFSET               0x514
#define TMU_TBSTR5_OFFSET               0x520
#define TMU_TBSTR6_OFFSET               0x524
#define TMU_TBSTR7_OFFSET               0x530
#define TMU_TBSTR8_OFFSET               0x534
#define TMU_TBSTR9_OFFSET               0x540
#define TMU_TBSTR10_OFFSET              0x544
#define TMU_TBSTC_OFFSET                0x550

/* Token Accumulation Period Register — AVM drv_tmu_reg.h:401 */
#define TMU_TACPER_OFFSET               0x560

/* Enqueue Request Delay Register — AVM drv_tmu_reg.h:127 */
#define TMU_ERDR_OFFSET                 0x0C0

/* Packet-pointer table — AVM drv_tmu_reg.h:407-423 */
#define TMU_PPT0_OFFSET                 0x600
#define TMU_PPT1_OFFSET                 0x604
#define TMU_PPT2_OFFSET                 0x608
#define TMU_PPT3_OFFSET                 0x60C
#define TMU_PPTC_OFFSET                 0x610

/* Reconfiguration command overlays — AVM drv_tmu_reg.h:453-472 */
#define TMU_CFGEPN_OFFSET               0x71C
#define TMU_CFGCMD_OFFSET               0x720

/* Enqueue / dequeue counters — these live OUTSIDE the TMU window in
 * dedicated ioremap'd windows g_cbm_qeqcnt_base / g_cbm_qdqcnt_base
 * (cqm/grx500/cbm.h:208-209). Both windows are flat arrays of u32
 * counters indexed by EQID/QID; the base address starts at offset 0
 * within each window.
 *
 * AVM drv_tmu_ll.c:4803-4823 — read/reset accessors do
 *   xrx500_cbm_r32(CBM_QEQCNT_BASE + index * 4)
 * — base + index*4.
 */
#define TMU_QEQCNTR_OFFSET              0x000   /* (CBM_QEQCNT_BASE relative) */
#define TMU_QDQCNTR_OFFSET              0x000   /* (CBM_QDQCNT_BASE relative) */

/*
 * TMU register bit-field masks
 *
 * The masks below cover the subset reachable from the retained APIs:
 * EPMTC valid / write-select bits, EPOT EPOC color masks, EPTT thresholds,
 * SBITC / SBOTC / TBSTC indirect-access bits, CFGCMD / CFGEPN overlay
 * fields, and the EPMT mapping bits.
 */

/* EPMT mapping bits — AVM drv_tmu_reg.h:1899-1907 (approx; struct EPMT) */
#define TMU_EPMT_EPE                    0x80000000  /* AVM drv_tmu_reg.h: EPMT EPE bit */
#define TMU_EPMT_SBID_MASK              0x0000007F  /* AVM drv_tmu_reg.h: EPMT SBID field */
#define TMU_EPMT_SBID_OFFSET            0           /* AVM drv_tmu_reg.h: EPMT SBID offset */

/* EPMTC indirect-access control — AVM drv_tmu_reg.h:2034-2092 */
#define TMU_EPMTC_EDV                   0x08000000  /* discard-table read-valid */
#define TMU_EPMTC_ETV                   0x04000000  /* threshold-table read-valid */
#define TMU_EPMTC_EOV                   0x02000000  /* occupancy-table read-valid */
#define TMU_EPMTC_EMV                   0x01000000  /* mapping-table read-valid */

/*
 * TMU_EPMTC_VAL - union of all four EPMTC command-complete bits.
 *
 * Used by the four EPMT/EPOT/EPTT/EPDT write helpers' busy-wait. The GRX350
 * EPMTC hardware clears stale VAL bits on every new control- register write
 * and sets only the bit matching the active write-select (EMW/EOW/ETW/EDW),
 * so polling on the union is functionally identical to polling on the
 * specific bit.
 */
#define TMU_EPMTC_VAL                   (TMU_EPMTC_EMV | TMU_EPMTC_EOV | \
					 TMU_EPMTC_ETV | TMU_EPMTC_EDV)
#define TMU_EPMTC_EDR                   0x00080000  /* discard read-select */
#define TMU_EPMTC_ETR                   0x00040000  /* threshold read-select */
#define TMU_EPMTC_EOR                   0x00020000  /* occupancy read-select */
#define TMU_EPMTC_EMR                   0x00010000  /* mapping read-select */
#define TMU_EPMTC_EDW                   0x00000800  /* discard write-select */
#define TMU_EPMTC_ETW                   0x00000400  /* threshold write-select */
#define TMU_EPMTC_EOW                   0x00000200  /* occupancy write-select */
#define TMU_EPMTC_EMW                   0x00000100  /* mapping write-select */
#define TMU_EPMTC_EPN_MASK              0x0000007F
#define TMU_EPMTC_EPN_OFFSET            0

/* EPOT egress-port occupancy color masks — AVM drv_tmu_reg.h:1943-1968
 * Each EPOC<N>_MASK is a 32-bit value that isolates the per-color slot
 * inside the packed EPOT0 / EPOT1 registers. EPOC0/EPOC1 share EPOT0
 * (low/high halves); EPOC2/EPOC3 share EPOT1.
 */
#define TMU_EPOT0_EPOC0_MASK            0x00007FFF
#define TMU_EPOT0_EPOC0_OFFSET          0
#define TMU_EPOT0_EPOC1_MASK            0x7FFF0000
#define TMU_EPOT0_EPOC1_OFFSET          16
#define TMU_EPOT1_EPOC2_MASK            0x00007FFF
#define TMU_EPOT1_EPOC2_OFFSET          0
#define TMU_EPOT1_EPOC3_MASK            0x7FFF0000
#define TMU_EPOT1_EPOC3_OFFSET          16

/* EPTT egress-port threshold masks — AVM drv_tmu_reg.h:1974-1996 */
#define TMU_EPTT0_EPTH0_MASK            0x00000FFF
#define TMU_EPTT0_EPTH0_OFFSET          0
#define TMU_EPTT0_EPTH1_MASK            0x0FFF0000
#define TMU_EPTT0_EPTH1_OFFSET          16
#define TMU_EPTT1_EPTH2_MASK            0x00000FFF
#define TMU_EPTT1_EPTH2_OFFSET          0
#define TMU_EPTT1_EPTH3_MASK            0x0FFF0000
#define TMU_EPTT1_EPTH3_OFFSET          16

/* SBITC scheduler-block input table control — AVM drv_tmu_reg.h:2212-2232 */
#define TMU_SBITC_VAL                   0x00040000  /* command-complete */
#define TMU_SBITC_SEL                   0x00020000  /* index selector */
#define TMU_SBITC_RW_W                  0x00010000  /* write access */
#define TMU_SBITC_RW                    0x00010000  /* read/write selector bit */

/* SBOTC scheduler-block output table control — AVM drv_tmu_reg.h:2317-2336 */
#define TMU_SBOTC_VAL                   0x00040000  /* command-complete */
#define TMU_SBOTC_SEL                   0x00020000  /* index selector */
#define TMU_SBOTC_RW_W                  0x00010000  /* write access */
#define TMU_SBOTC_RW                    0x00010000  /* read/write selector bit */

/* TBSTC token-bucket-shaper table control — AVM drv_tmu_reg.h:2528-2547 */
#define TMU_TBSTC_VAL                   0x00040000  /* command-complete */
#define TMU_TBSTC_SEL                   0x00020000  /* index selector */
#define TMU_TBSTC_RW_W                  0x00010000  /* write access */
#define TMU_TBSTC_RW                    0x00010000  /* read/write selector bit */

/* CFGCMD reconfiguration-command opcodes — AVM drv_tmu_reg.h:2838-2884 */
#define TMU_CFGCMD_VAL                  0x00010000  /* command-complete */
#define TMU_CFGCMD_CMD_MASK             0xF0000000  /* opcode field */
#define TMU_CFGCMD_CMD_OFFSET           28
#define TMU_CFGCMD_CMD_EP_ON            0xB0000000  /* egress-port enable */
#define TMU_CFGCMD_CMD_EP_OFF           0xC0000000  /* egress-port disable */
#define TMU_CFGCMD_CMD_SB_INPUT_ON      0x00000000  /* SB input enable */

/* CFGSBIN reconfiguration-command overlay — AVM drv_tmu_reg.h:2763-2772 */
#define TMU_CFGSBIN_SBIN_MASK           0x000003FF
#define TMU_CFGSBIN_SBIN_OFFSET         0

/* CFGEPN reconfiguration-command overlay — AVM drv_tmu_reg.h:2828-2834 */
#define TMU_CFGEPN_VAL                  0x00010000
#define TMU_CFGEPN_EPN_MASK             0x0000007F
#define TMU_CFGEPN_EPN_OFFSET           0

#define TMU_PPT0_PNEXT_MASK             0x3FFF0000
#define TMU_PPT0_PNEXT_OFFSET           16

/* PPTC PDU-pointer-table control — AVM drv_tmu_reg.h:2653-2662 */
#define TMU_PPTC_VAL                    0x00020000
#define TMU_PPTC_RW                     0x00010000
#define TMU_PPTC_RW_W                   0x00010000

/* CTRL register bits — AVM drv_tmu_reg.h:478-524 */
#define TMU_CTRL_ACT_EN                 0x00000001
#define TMU_CTRL_DTA_DTA1               0x00000004
#define TMU_CTRL_RPS_RPS1               0x00000010
#define TMU_CTRL_MAXTB_MASK             0x0000FF00
#define TMU_CTRL_MAXTB_OFFSET           8

/* FPL register bits — AVM drv_tmu_reg.h:526-538 */
#define TMU_FPL_TFPP_MASK               0x3FFF0000
#define TMU_FPL_TFPP_OFFSET             16

/* LFSR register bits — AVM drv_tmu_reg.h:579-581 */
#define TMU_LFSR_RN_MASK                0x0000FFFF
#define TMU_LFSR_RN_OFFSET              0

/* CPR register bits — AVM drv_tmu_reg.h:593-595 */
#define TMU_CPR_CP_MASK                 0x000000FF
#define TMU_CPR_CP_OFFSET               0

/* ERDR register bits — AVM drv_tmu_reg.h:953-955 */
#define TMU_ERDR_ERD_MASK               0x0000FFFF
#define TMU_ERDR_ERD_OFFSET             0

/* TACPER register bits — AVM drv_tmu_reg.h:2560-2562 */
#define TMU_TACPER_TACP_MASK            0x000000FF
#define TMU_TACPER_TACP_OFFSET          0

/* QMTC indirect-access bits — AVM drv_tmu_reg.h:1821-1910
 * QEW/QSW/QTW/QOW/QDW/QFW = write-select bits (offset 0x100..0x4000)
 * QEV/QSV/QTV/QOV/QDV/QFV = command-complete bits (offset 0x01000000..0x40000000)
 */
#define TMU_QMTC_QEW                    0x00000100
#define TMU_QMTC_QSW                    0x00000200
#define TMU_QMTC_QTW                    0x00000400
#define TMU_QMTC_QOW                    0x00000800
#define TMU_QMTC_QDW                    0x00002000
#define TMU_QMTC_QFW                    0x00004000
#define TMU_QMTC_QFR                    0x00400000  /* (QFMT read) */
#define TMU_QMTC_QEV                    0x01000000
#define TMU_QMTC_QSV                    0x02000000
#define TMU_QMTC_QTV                    0x04000000
#define TMU_QMTC_QOV                    0x08000000
#define TMU_QMTC_QDV                    0x20000000
#define TMU_QMTC_QFV                    0x40000000

/* QEMT / QSMT field masks — AVM drv_tmu_reg.h:1530-1538 */
#define TMU_QEMT_EPN_MASK               0x0000007F
#define TMU_QSMT_SBIN_MASK              0x000003FF

/* QOCT0/1/2 field masks — AVM drv_tmu_reg.h:1690-1718 */
#define TMU_QOCT0_WQ_MASK               0x000F0000
#define TMU_QOCT0_WQ_OFFSET             16
#define TMU_QOCT0_QRTH_MASK             0x00000FFF
#define TMU_QOCT1_QOCC_MASK             0x00007FFF
#define TMU_QOCT2_QAVG_MASK             0x007FFFFF

/* QTHT0 enable bit — AVM drv_tmu_reg.h:1551
 * (Used by tmu_egress_queue_table_init — qtht[0] = 0x00011320 keeps QE = 0,
 * but the symbol exists for completeness / future use.)
 */
#define TMU_QTHT0_QE_EN                 0x80000000

/* QEMT / QSMT field offsets — AVM drv_tmu_reg.h:1532/1540 */
#define TMU_QEMT_EPN_OFFSET             0
#define TMU_QSMT_SBIN_OFFSET            0

/* SBOTR0 field masks/bits — AVM drv_tmu_reg.h:2247-2278. */
#define TMU_SBOTR0_SOE_EN               0x80000000
#define TMU_SBOTR0_LVL_MASK             0x00070000
#define TMU_SBOTR0_LVL_OFFSET           16
#define TMU_SBOTR0_V_SBIN               0x00008000
#define TMU_SBOTR0_OMID_MASK            0x000003FF
#define TMU_SBOTR0_OMID_OFFSET          0

/* SBITR0 field masks/bits — AVM drv_tmu_reg.h:2104-2132. */
#define TMU_SBITR0_SIE_EN               0x80000000
#define TMU_SBITR0_IWGT_MASK            0x03FF0000
#define TMU_SBITR0_IWGT_OFFSET          16
#define TMU_SBITR0_SIT_SBID             0x00008000
#define TMU_SBITR0_QSID_MASK            0x000000FF
#define TMU_SBITR0_QSID_OFFSET          0

/* Public API surface (exactly three functions) */

/**
 * tmu_init - Phase-3 TMU bring-up.
 *
 * Performs basic-register, EPT, SBIT, SBOT, TBST, and PPT initialization in
 * the AVM-canonical order; the EPOC zero-init step is the M0 milestone
 * artifact ( "tmu: probe complete, EPOC zeroed at t=<jiffies>"). Arms the
 * ordering guard on success.
 */
int tmu_init(void);

/**
 * tmu_egress_port_enable - flip the EPE bit on EPMT[epn] then commit via the
 * CFGCMD reconfiguration channel.
 *
 * @epn: egress port number (0..EGRESS_PORT_ID_MAX-1).
 *
 * @ena: true to enable, false to disable.
 */
void tmu_egress_port_enable(uint32_t epn, bool ena);

/**
 * tmu_qfmt_read - indirect read of the Queue Format table for @qid. Faithful
 * port of AVM drv_tmu_ll.c:724-744. Fills qfm[0..2] (QFMT0/1/2); qfm[1]
 * HQPP[13:0] is the head PDU's PPT index. Bounded spin; returns -ETIMEDOUT if
 * the QMTC valid bit never sets.
 */
int tmu_qfmt_read(u32 qid, u32 *qfm);

/**
 * tmu_ppt_read - indirect read of Packet Pointer Table entry @pos. Faithful
 * port of AVM drv_tmu_ll.c:3954-3963. Fills ppt[0..3] (PPT0/1/2/3); ppt[2]
 * BDYL[15:0] is the PDU body length, ppt[1] SEGL[9:0], ppt[0] OFFS[15:8].
 * Bounded spin; returns -ETIMEDOUT on timeout.
 */
int tmu_ppt_read(u32 pos, u32 *ppt);

/**
 * tmu_reset_mib_all - zero all egress-queue MIB counters (enq + deq + QDCT
 * rings) for every QID up to EGRESS_QUEUE_ID_MAX.
 */
int tmu_reset_mib_all(void);

/**
 * tmu_create_flat_egress_path - build a flat TMU egress path for one port.
 *
 * @num_ports:  number of consecutive egress ports to build (1 per LAN port).
 *
 * @base_epn:   first egress-port number (= tmu_egress_port for the LAN port).
 *
 * @base_sbid:  first scheduler-block id (= tmu_queue - SBID_START).
 *
 * @base_qid:   first egress-queue id (= tmu_queue).
 *
 * @qid_per_sb: queues per scheduler block (1 for a flat LAN path).
 */
void tmu_create_flat_egress_path(uint16_t num_ports, uint16_t base_epn,
				 uint16_t base_sbid, uint16_t base_qid,
				 uint16_t qid_per_sb);

#endif /* _DRV_TMU_LL_H_ */
