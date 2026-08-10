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

/* TCP-lite/LRO require 128-byte DMA data offset. */
#define CBM_GRX550_DMA_DATA_OFFSET 128

/*
 * CBM control window (per CBM_BASE / g_cbm_base):
 *   CBM_CTRL relative offset = 0x210.
 *   JSEL bit-field.
 */
#define CBM_CTRL              0x210
#define JSEL_POS              17
#define JSEL_MASK             BIT(17)

/*
 * QEQCNT / QDQCNT counter rings live inside their own ioremap'd windows
 * (g_cbm_qeqcnt_base / g_cbm_qdqcnt_base). The "BASE" suffix in this
 * file refers to offset-zero within those windows. The 0x400 byte size
 * corresponds to 256 u32 counters.
 */
#define CBM_QEQCNT_BASE       0x0000
#define CBM_QDQCNT_BASE       0x0000
#define CBM_QEQCNT_SIZE       0x400
#define CBM_QDQCNT_SIZE       0x400

/*
 * EQM / DQM port-window base offsets:
 *   CFG_CPU_IGP_0 = 0x10000
 *   CFG_DMA_IGP_5 = 0x15000
 *   SDESC0_0_IGP_15 = 0x1F100
 *   CFG_CPU_EGP_0 = 0x10000
 *   CFG_DMA_EGP_6 = 0x16000
 *   SDESC0_0_IGP_5 = 0x0
 *   DESC0_0_EGP_5 = 0x40000
 *   LS_DESC_DW0_PORT0 = 0x0
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
 * CPU DQM egress-port EPMAP bit-field. The dequeue engine's egress-port-map
 * tag; identical in shape to the DMA sibling's CFG_DMA_EGP_6_EPMAP_POS/MASK
 * (mirrored locally in cbm_dma.c as CBM_DQM_EGP_EPMAP_POS/MASK). Consumed by
 * init_cbm_dqm_cpu_port.
 */
#define CFG_CPU_EGP_0_EPMAP_POS   16
#define CFG_CPU_EGP_0_EPMAP_MASK  0x7F0000U

/*
 * CPU_DQM_PORT_NUM — number of CPU DQM (dequeue) egress ports. Bounds the
 * idx guard in init_cbm_dqm_cpu_port,
 * mirroring the literal-4 range init_cbm_eqm_cpu_port enforces on the EQM side.
 */
#define CPU_DQM_PORT_NUM          4

/*
 * CPU_EQM_PORT_NUM — number of CPU EQM (enqueue) ingress ports. Bounds the
 * eqm_intr_ctrl / dqm_intr_ctrl sweeps, which both walk this count.
 */
#define CPU_EQM_PORT_NUM          4

/* CBM_EQM_CTRL. */
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
 * CBM_DQM_CTRL. Field positions match the EQM block by silicon convention;
 * DQQCEN replaces EQQCEN as the QEN gate.
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
 * FSQM register window offsets:
 *   FSQM_CTRL    0x0
 *   IO_BUF_RD    0x8
 *   IO_BUF_WR    0xC
 *   FSQM_IRNEN   0x18
 *   OFSQ         0x24
 *   OFSC         0x2C
 *   FSQT0        0x30
 *   FSQT1        0x70
 *   FSQT2        0xB0
 *   FSQT3        0xF0
 *   FSQT4        0x130
 *   LSARNG       0x180
 *   RCNT         0x80000
 *   RAM          0xC0000
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

/*
 * FSQM_IRNEN bit-field decomposition. The register's own field names
 * (T0U..T4U / ACCMEM / FRMEM / FROVLO / ALLNIL) are listed below; the
 * mnemonic names (EVT0..EVT4 / RAM_VIOL / CMD_RAM_VIOL / CMD_OVERFLOW /
 * ALLOC_NIL) are aliased so the FSQM_IRNEN_DEFAULT composition reads
 * naturally.
 *
 * Bit positions:
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

/* Fixed DMA RX skb headroom. */
#define CBM_FIXED_RX_OFFSET \
	(CBM_GRX550_DMA_DATA_OFFSET + NET_IP_ALIGN + NET_SKB_PAD)

#endif /* __XRX500_CBM_REGS_H */
