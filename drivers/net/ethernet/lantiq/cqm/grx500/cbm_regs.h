/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/reg/ and cqm/falconmx/reg/cbm_ls.h.
 *
 * CBM register-window offsets and bit-field definitions.
 */

#ifndef __XRX500_CBM_REGS_H
#define __XRX500_CBM_REGS_H

#include <linux/types.h>
#include <linux/bits.h>
#include <linux/skbuff.h>
#include <linux/build_bug.h>

/*
 * AVM cqm/cqm_common.h:18-19 — verbatim. TCP-lite/LRO require 128-byte
 * DMA data offset.
 */
#define CBM_GRX550_DMA_DATA_OFFSET 128

/*
 * CBM control window (per CBM_BASE / g_cbm_base):
 *   AVM cqm/grx500/reg/cbm.h:2617 — CBM_CTRL relative offset = 0x210.
 *   AVM cqm/grx500/reg/cbm.h:2658-2660 — JSEL bit-field.
 *
 * The plan's "0x0000" was a draft-stage placeholder.
 */
#define CBM_CTRL              0x210
#define JSEL_POS              17
#define JSEL_MASK             BIT(17)

/*
 * QEQCNT / QDQCNT counter rings live inside their own ioremap'd windows
 * (g_cbm_qeqcnt_base / g_cbm_qdqcnt_base). The "BASE" suffix in this
 * file refers to offset-zero within those windows. The 0x400 byte size
 * corresponds to 256 u32 counters — same as AVM CBM_QEQCNTR_SIZE /
 * CBM_QDQCNTR_SIZE (cqm/grx500/cbm.h:175-176).
 */
#define CBM_QEQCNT_BASE       0x0000
#define CBM_QDQCNT_BASE       0x0000
#define CBM_QEQCNT_SIZE       0x400
#define CBM_QDQCNT_SIZE       0x400

/*
 * EQM / DQM port-window base offsets — verbatim from AVM:
 *   cqm/grx500/reg/cbm_eqm.h:2941 — CFG_CPU_IGP_0 = 0x10000
 *   cqm/grx500/reg/cbm_eqm.h:9541 — CFG_DMA_IGP_5 = 0x15000
 *   cqm/grx500/reg/cbm_eqm.h:14707 — SDESC0_0_IGP_15 = 0x1F100
 *   cqm/grx500/reg/cbm_dqm.h:155 — CFG_CPU_EGP_0 = 0x10000
 *   cqm/grx500/reg/cbm_dqm.h:4839 — CFG_DMA_EGP_6 = 0x16000
 *   cqm/grx500/reg/cbm_desc64b.h:25 — SDESC0_0_IGP_5 = 0x0
 *   cqm/grx500/reg/cbm_desc64b.h:1465 — DESC0_0_EGP_5 = 0x40000
 *   cqm/grx500/reg/cbm_ls.h:25 — LS_DESC_DW0_PORT0 = 0x0
 *
 * NOTE: CFG_CPU_IGP_0 / CFG_CPU_EGP_0 both live at offset 0x10000 in
 * their respective windows (EQM / DQM ioremap regions) — they are
 * not aliasing the same physical address.
 */
#define CFG_CPU_IGP_0         0x10000
#define CFG_DMA_IGP_5         0x15000
#define SDESC0_0_IGP_15       0x1F100
#define CFG_DMA_IGP_15        0x1F000
#define WM_DMA_IGP_15         0x1F004
#define POCC_DMA_IGP_15       0x1F008
#define CFG_CPU_EGP_0         0x10000
#define CFG_DMA_EGP_6         0x16000
#define SDESC0_0_IGP_5        0x0
#define DESC0_0_EGP_5         0x40000
#define LS_DESC_DW0_PORT0     0x0

/*
 * CPU DQM egress-port EPMAP bit-field — verbatim from AVM
 * cqm/grx500/reg/cbm_dqm.h:199 (POS) and :201 (MASK). The dequeue engine's
 * egress-port-map tag; identical in shape to the DMA sibling's
 * CFG_DMA_EGP_6_EPMAP_POS/MASK (reg/cbm_dqm.h:4865/4867, mirrored locally in
 * cbm_dma.c as CBM_DQM_EGP_EPMAP_POS/MASK). Consumed by init_cbm_dqm_cpu_port
 * to tag the CPU DQM ingress port that feeds the Load Spreader.
 */
#define CFG_CPU_EGP_0_EPMAP_POS   16
#define CFG_CPU_EGP_0_EPMAP_MASK  0x7F0000U

/*
 * CPU_DQM_PORT_NUM — number of CPU DQM (dequeue) egress ports. Verbatim from
 * AVM cqm/grx500/cbm.h:199. Bounds the idx guard in init_cbm_dqm_cpu_port,
 * mirroring the literal-4 range init_cbm_eqm_cpu_port enforces on the EQM side.
 */
#define CPU_DQM_PORT_NUM          4

/* CBM_EQM_CTRL - AVM cqm/grx500/reg/cbm_eqm.h:25. */
#define CBM_EQM_CTRL          0x0
#define EQM_EN_POS            0
#define EQM_EN_MASK           BIT(0)
#define EQM_FRZ_POS           1
#define EQM_FRZ_MASK          BIT(1)
#define EQM_QEN_POS           4
#define EQM_QEN_MASK          BIT(4)
#define EQM_MSEL_POS          5
#define EQM_MSEL_MASK         BIT(5)

/*
 * CBM_DQM_CTRL — AVM cqm/grx500/reg/cbm_dqm.h:25. Field positions match
 * the EQM block by silicon convention; DQQCEN replaces EQQCEN as the QEN
 * gate (AVM cqm/grx500/cbm.c:83-87 cbm_cntr_func[1] wiring).
 */
#define CBM_DQM_CTRL          0x0
#define DQM_EN_POS            0
#define DQM_EN_MASK           BIT(0)
#define DQM_FRZ_POS           1
#define DQM_FRZ_MASK          BIT(1)
#define DQM_QEN_POS           4
#define DQM_QEN_MASK          BIT(4)
#define DQM_MSEL_POS          5
#define DQM_MSEL_MASK         BIT(5)

/*
 * FSQM register window offsets — verbatim from AVM cqm/grx500/reg/fsqm.h:
 *   line  27 — FSQM_CTRL    0x0
 *   line 256 — IO_BUF_RD    0x8
 *   line 274 — IO_BUF_WR    0xC
 *   line 444 — FSQM_IRNEN   0x18
 *   line 556 — OFSQ         0x24
 *   line 579 — OFSC         0x2C
 *   line 597 — FSQT0        0x30
 *   line 615 — FSQT1        0x70
 *   line 633 — FSQT2        0xB0
 *   line 651 — FSQT3        0xF0
 *   line 669 — FSQT4        0x130
 *   line 687 — LSARNG       0x180
 *   line 733 — RCNT         0x80000
 *   line 769 — RAM          0xC0000
 */
#define FSQM_CTRL             0x0
#define IO_BUF_RD             0x8
#define IO_BUF_WR             0xC
#define FSQM_IRNEN            0x18
#define OFSQ                  0x24
#define OFSC                  0x2C
#define FSQT0                 0x30
#define FSQT1                 0x70
#define FSQT2                 0xB0
#define FSQT3                 0xF0
#define FSQT4                 0x130
#define LSARNG                0x180
#define RCNT                  0x80000
#define RAM                   0xC0000

/* IRNCR_LS — Load Spreader IRN Capture Register. */
#define IRNCR_LS              0x910

/*
 * All values are byte-distances within the LS ioremap'd window
 * (g_cbm_ls_base), NOT absolute MIPS physical addresses.
 *
 *   IRNICR_LS (0x914) / IRNEN_LS (0x918) — the LS IRN interrupt
 *     capture-ICR / enable registers, immediately after IRNCR_LS (0x910):
 *     the standard IRN 0x10/0x14/0x18 triplet at the LS aggregator base
 *     0x900. AVM cqm/grx500/reg/cbm_ls.h.
 *
 * IRNEN_LS MUST equal the pre-existing literal CBM_LS_IRNEN (0x918u) in
 * cbm.c:543 (used at cbm.c:570: __raw_writel(..., g_cbm_ls_base +
 * CBM_LS_IRNEN)).
 *
 *   CBM_LS_PORT_STRIDE / CBM_LS_PORT_STATUS / CBM_LS_PORT_DESC — per-LS
 *     -output-port window geometry, from AVM cqm/cqm_common.h:28-63
 *     (struct cbm_ls_reg + CBM_LS_PORT macro): each port is a 0x100 stride
 *     from LS_DESC_DW0_PORT0 (0x0); within a port the four descriptor words
 *     desc0..desc3 sit at +0x0/+0x4/+0x8/+0xC and the status register at
 *     +0x14 (desc[4 words]=0x10, ctrl@0x10, status@0x14).
 *
 *   CBM_LS_QUEUE_LEN_POS / _MASK — AVM cbm_ls.h LS_STATUS_PORT0_QUEUE_LEN_*:
 *     bits 7..10 (0x780) carry the 4-bit queued-descriptor count, read as
 *     (status >> 7) & 0xF.
 *   CBM_LS_QUEUE_EMPTY — status BIT(13), the LS-ring-drained flag the
 *     do_cbm_tasklet re-arm gate tests (AVM cbm.c:2650).
 */
#define IRNICR_LS             0x914
#define IRNEN_LS              0x918
#define CBM_LS_PORT_STRIDE    0x100
#define CBM_LS_PORT_STATUS(i)  ((i) * CBM_LS_PORT_STRIDE + 0x14)
#define CBM_LS_PORT_DESC(i, n) ((i) * CBM_LS_PORT_STRIDE + (n) * 4)
#define CBM_LS_QUEUE_LEN_POS  7
#define CBM_LS_QUEUE_LEN_MASK 0x780
#define CBM_LS_QUEUE_EMPTY    BIT(13)

/*
 * FSQM_IRNEN bit-field decomposition — derived from AVM
 * cqm/grx500/reg/fsqm.h:451-530. The AVM register-doc naming
 * (T0U..T4U / ACCMEM / FRMEM / FROVLO / ALLNIL) is preserved as the
 * AVM-side comment; the planner's mnemonic names (EVT0..EVT4 /
 * RAM_VIOL / CMD_RAM_VIOL / CMD_OVERFLOW / ALLOC_NIL) are aliased so
 * the FSQM_IRNEN_DEFAULT composition reads naturally.
 *
 * Bit positions (single source of truth: AVM fsqm.h):
 *   T0U_POS    =  0   "Threshold 0 Underflow"
 *   T1U_POS    =  1   "Threshold 1 Underflow"
 *   T2U_POS    =  2   "Threshold 2 Underflow"
 *   T3U_POS    =  3   "Threshold 3 Underflow"
 *   T4U_POS    =  4   "Threshold 4 Underflow"
 *   ACCMEM_POS = 12   "RAM Access Violation"
 *   FRMEM_POS  = 16   "Free Command RAM Access Violation"
 *   FROVLO_POS = 20   "Free Command Overflow on OFSQ"
 *   ALLNIL_POS = 24   "Alloc Command NIL Response"
 */
#define FSQM_IRNEN_EVT0           BIT(0)
#define FSQM_IRNEN_EVT1           BIT(1)
#define FSQM_IRNEN_EVT2           BIT(2)
#define FSQM_IRNEN_EVT3           BIT(3)
#define FSQM_IRNEN_EVT4           BIT(4)
#define FSQM_IRNEN_RAM_VIOL       BIT(12)
#define FSQM_IRNEN_CMD_RAM_VIOL   BIT(16)
#define FSQM_IRNEN_CMD_OVERFLOW   BIT(20)
#define FSQM_IRNEN_ALLOC_NIL      BIT(24)

#define FSQM_IRNEN_DEFAULT                                          \
	(FSQM_IRNEN_EVT0 | FSQM_IRNEN_EVT1 | FSQM_IRNEN_EVT2 |      \
	 FSQM_IRNEN_EVT3 | FSQM_IRNEN_EVT4 | FSQM_IRNEN_RAM_VIOL |  \
	 FSQM_IRNEN_CMD_RAM_VIOL | FSQM_IRNEN_CMD_OVERFLOW |        \
	 FSQM_IRNEN_ALLOC_NIL)

/* AVM cqm/grx500/cbm.h:189 — fixed DMA RX skb headroom. */
#define CBM_FIXED_RX_OFFSET \
	(CBM_GRX550_DMA_DATA_OFFSET + NET_IP_ALIGN + NET_SKB_PAD)

#endif /* __XRX500_CBM_REGS_H */
