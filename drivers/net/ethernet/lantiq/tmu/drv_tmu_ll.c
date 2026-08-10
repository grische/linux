// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012 Lantiq Deutschland GmbH
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * tmu/drv_tmu_ll.c and tmu/drv_tmu_reg.h.
 *
 * Traffic Management Unit low-level driver: egress port and queue setup
 * through the TMU's two-step command protocol.
 */

#include <linux/types.h>
#include <linux/io.h>
#include <linux/atomic.h>
#include <linux/printk.h>
#include <linux/jiffies.h>
#include <linux/build_bug.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>

#include "../cqm/grx500/cbm.h"
#include "drv_tmu_ll.h"

atomic_t tmu_init_done = ATOMIC_INIT(0);

/*
 * Linux-port of AVM tmu_spin_lock. The AVM body wraps spin_lock_irqsave
 * inside an indirection layer; the 6.18 port uses the kernel primitive
 * directly. Held during every indirect-access write to serialize the
 * write+busy-wait pair against concurrent callers.
 */
static DEFINE_SPINLOCK(tmu_lock);

/*
 * Optional CONFIG_INTEL_XRX500_GSWIP_API_DEBUG mock-MMIO glue.
 *
 * The gate is compile-time so production builds carry zero runtime cost.
 *
 * Limited to the SIX control-register offsets that the seven sub-init
 * routines and tmu_egress_port_enable poll on (EPMTC, SBITC, SBOTC,
 * TBSTC, PPTC, CFGCMD, QMTC).
 */
#ifdef CONFIG_INTEL_XRX500_GSWIP_API_DEBUG
static bool tmu_mock_mmio_active;

static inline u32 tmu_mock_val_for_offset(u32 off)
{
	switch (off) {
	case TMU_EPMTC_OFFSET:
		return TMU_EPMTC_EMV | TMU_EPMTC_EOV |
		       TMU_EPMTC_ETV | TMU_EPMTC_EDV;
	case TMU_SBITC_OFFSET:
		return TMU_SBITC_VAL;
	case TMU_SBOTC_OFFSET:
		return TMU_SBOTC_VAL;
	case TMU_TBSTC_OFFSET:
		return TMU_TBSTC_VAL;
	case TMU_PPTC_OFFSET:
		return TMU_PPTC_VAL;
	case TMU_CFGCMD_OFFSET:
		return TMU_CFGCMD_VAL;
	case TMU_QMTC_OFFSET:
		return TMU_QMTC_QEV | TMU_QMTC_QSV | TMU_QMTC_QTV |
		       TMU_QMTC_QOV | TMU_QMTC_QDV | TMU_QMTC_QFV;
	default:
		return 0;
	}
}
#endif

/*
 * MMIO helpers
 *
 * Argument order for tmu_w32 (value, offset) matches AVM
 * drv_tmu_api.h:103-109 reg_w32 form so AVM-sourced sequences port over
 * verbatim.
 */
static inline u32 tmu_r32(u32 off)
{
	return __raw_readl(g_cbm_tmu_base + off);
}

static inline void tmu_w32(u32 val, u32 off)
{
#ifdef CONFIG_INTEL_XRX500_GSWIP_API_DEBUG
	if (tmu_mock_mmio_active)
		val |= tmu_mock_val_for_offset(off);
#endif
	__raw_writel(val, g_cbm_tmu_base + off);
}

/* tmu_enable - flip the CTRL.ACT bit. */
static void tmu_enable(bool act)
{
	/* AVM drv_tmu_ll.c:183 — tmu_w32_mask(TMU_CTRL_ACT_EN, act ? ... : 0, ctrl) */
	u32 v = tmu_r32(TMU_CTRL_OFFSET);

	if (act)
		v |= TMU_CTRL_ACT_EN;
	else
		v &= ~TMU_CTRL_ACT_EN;
	tmu_w32(v, TMU_CTRL_OFFSET);
}

/*
 * Static indirect-access write helpers. AVM ports the queue / egress-port
 * / scheduler-block tables behind a write-then-poll-VAL protocol; the
 * helpers below preserve that protocol with offset-based register access.
 *
 * EXPORT_SYMBOL on the AVM originals is DROPPED — every caller lives in
 * the same composite.
 */

/* AVM drv_tmu_ll.c:1535-1549 — tmu_cfgcmd_write. */
static void tmu_cfgcmd_write(u32 cfgcmd)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(cfgcmd, TMU_CFGCMD_OFFSET);
	while ((tmu_r32(TMU_CFGCMD_OFFSET) & TMU_CFGCMD_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:454-470 — tmu_qemt_write. */
static void tmu_qemt_write(u32 qid, u32 epn)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(epn, TMU_QEMT_OFFSET);
	tmu_w32(TMU_QMTC_QEW | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QEV) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:490-506 — tmu_qsmt_write. */
static void tmu_qsmt_write(u32 qid, u32 sbin)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(sbin, TMU_QSMT_OFFSET);
	tmu_w32(TMU_QMTC_QSW | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QSV) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:526-550 — tmu_qtht_write. */
static void tmu_qtht_write(u32 qid, const u32 *qtht)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(qtht[0], TMU_QTHT0_OFFSET);
	tmu_w32(qtht[1], TMU_QTHT1_OFFSET);
	tmu_w32(qtht[2], TMU_QTHT2_OFFSET);
	tmu_w32(qtht[3], TMU_QTHT3_OFFSET);
	tmu_w32(qtht[4], TMU_QTHT4_OFFSET);
	tmu_w32(TMU_QMTC_QTW | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QTV) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:599-624 — tmu_qoct_write. */
static void tmu_qoct_write(u32 qid, u32 wq, u32 qrth, u32 qocc, u32 qavg)
{
	u32 tmp = ((wq << TMU_QOCT0_WQ_OFFSET) & TMU_QOCT0_WQ_MASK) |
		  (qrth & TMU_QOCT0_QRTH_MASK);
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(tmp, TMU_QOCT0_OFFSET);
	tmu_w32(qocc & TMU_QOCT1_QOCC_MASK, TMU_QOCT1_OFFSET);
	tmu_w32(qavg & TMU_QOCT2_QAVG_MASK, TMU_QOCT2_OFFSET);
	tmu_w32(TMU_QMTC_QOW | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QOV) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:654-676 — tmu_qdct_write. */
static void tmu_qdct_write(u32 qid, const u32 *qdc)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(qdc[0], TMU_QDCT0_OFFSET);
	tmu_w32(qdc[1], TMU_QDCT1_OFFSET);
	tmu_w32(qdc[2], TMU_QDCT2_OFFSET);
	tmu_w32(qdc[3], TMU_QDCT3_OFFSET);
	tmu_w32(TMU_QMTC_QDW | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QDV) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:702-722 — tmu_qfmt_write. */
static void tmu_qfmt_write(u32 qid, const u32 *qfm)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(qfm[0], TMU_QFMT0_OFFSET);
	tmu_w32(qfm[1], TMU_QFMT1_OFFSET);
	tmu_w32(qfm[2], TMU_QFMT2_OFFSET);
	tmu_w32(TMU_QMTC_QFW | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QFV) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

int tmu_qfmt_read(u32 qid, u32 *qfm)
{
	unsigned long flags;
	int spins = 0;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(TMU_QMTC_QFR | qid, TMU_QMTC_OFFSET);
	while ((tmu_r32(TMU_QMTC_OFFSET) & TMU_QMTC_QFV) == 0) {
		if (++spins > 1000000) {
			spin_unlock_irqrestore(&tmu_lock, flags);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
	qfm[0] = tmu_r32(TMU_QFMT0_OFFSET);
	qfm[1] = tmu_r32(TMU_QFMT1_OFFSET);
	qfm[2] = tmu_r32(TMU_QFMT2_OFFSET);
	spin_unlock_irqrestore(&tmu_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(tmu_qfmt_read);

int tmu_ppt_read(u32 pos, u32 *ppt)
{
	unsigned long flags;
	int spins = 0;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(pos, TMU_PPTC_OFFSET);   /* RW bit clear => read; index = pos */
	while ((tmu_r32(TMU_PPTC_OFFSET) & TMU_PPTC_VAL) == 0) {
		if (++spins > 1000000) {
			spin_unlock_irqrestore(&tmu_lock, flags);
			return -ETIMEDOUT;
		}
		cpu_relax();
	}
	ppt[0] = tmu_r32(TMU_PPT0_OFFSET);
	ppt[1] = tmu_r32(TMU_PPT1_OFFSET);
	ppt[2] = tmu_r32(TMU_PPT2_OFFSET);
	ppt[3] = tmu_r32(TMU_PPT3_OFFSET);
	spin_unlock_irqrestore(&tmu_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(tmu_ppt_read);

/* AVM drv_tmu_ll.c:842-864 — tmu_epmt_write. */
static void tmu_epmt_write(u32 epn, u32 epe, u32 sbid)
{
	u32 tmp = sbid & TMU_EPMT_SBID_MASK;
	unsigned long flags;

	if (epe)
		tmp |= TMU_EPMT_EPE;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(tmp, TMU_EPMT_OFFSET);
	tmu_w32(TMU_EPMTC_EMW | epn, TMU_EPMTC_OFFSET);
	while ((tmu_r32(TMU_EPMTC_OFFSET) & TMU_EPMTC_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:866-884 — tmu_epmt_read (reduced — used only by
 * tmu_egress_port_enable's read-modify-write).
 */
static void tmu_epmt_read(u32 epn, u32 *epe, u32 *sbid)
{
	u32 tmp;
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(TMU_EPMTC_EMR | epn, TMU_EPMTC_OFFSET);
	while ((tmu_r32(TMU_EPMTC_OFFSET) & TMU_EPMTC_VAL) == 0)
		continue;
	tmp = tmu_r32(TMU_EPMT_OFFSET);
	spin_unlock_irqrestore(&tmu_lock, flags);

	*sbid = tmp & TMU_EPMT_SBID_MASK;
	*epe = (tmp & TMU_EPMT_EPE) ? 1 : 0;
}

/* AVM drv_tmu_ll.c:886-904 — tmu_epot_write. */
static void tmu_epot_write(u32 epn, const u32 *epoc)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(epoc[0], TMU_EPOT0_OFFSET);
	tmu_w32(epoc[1], TMU_EPOT1_OFFSET);
	tmu_w32(TMU_EPMTC_EOW | epn, TMU_EPMTC_OFFSET);
	while ((tmu_r32(TMU_EPMTC_OFFSET) & TMU_EPMTC_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:925-943 — tmu_eptt_write. */
static void tmu_eptt_write(u32 epn, const u32 *ept)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(ept[0], TMU_EPTT0_OFFSET);
	tmu_w32(ept[1], TMU_EPTT1_OFFSET);
	tmu_w32(TMU_EPMTC_ETW | epn, TMU_EPMTC_OFFSET);
	while ((tmu_r32(TMU_EPMTC_OFFSET) & TMU_EPMTC_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:965-987 — tmu_epdt_write. */
static void tmu_epdt_write(u32 epn, const u32 *epd)
{
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(epd[0], TMU_EPDT0_OFFSET);
	tmu_w32(epd[1], TMU_EPDT1_OFFSET);
	tmu_w32(epd[2], TMU_EPDT2_OFFSET);
	tmu_w32(epd[3], TMU_EPDT3_OFFSET);
	tmu_w32(TMU_EPMTC_EDW | epn, TMU_EPMTC_OFFSET);
	while ((tmu_r32(TMU_EPMTC_OFFSET) & TMU_EPMTC_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

/*
 * The seven AVM sub-init routines.
 *
 * Per plan Step 1, AVM struct-overlay `tmu->ctrl = v` access is replaced
 * with `tmu_w32(v, TMU_CTRL_OFFSET)`. Per plan Steps 3a/3b/3c, the seven
 * shaper-touch wrappers (tmu_relog_sequential, ...) inside tmu_basic_init
 * are INLINED as direct tmu_w32 calls — they are NOT ported as separate
 * static helpers. KEEP_GPON_ALL / CONFIG_LTQ_TMU_DDR_SIMULATE_REG /
 * CONFIG_LTQ_TMU_CHIPTEST / CONFIG_LTQ_TMU_EXT branches removed entirely.
 */

/* AVM drv_tmu_ll.c:349-452 — tmu_basic_init. */
static void tmu_basic_init(void)
{
	u32 i;
	u32 v;

	/* state machine de-activated */
	tmu_enable(false);

	/* FPL settings are covered in PacketPointerTableInit as they belong
	 * functionally to PPT issues.
	 */
	tmu_w32(PACKET_POINTER_TABLE_INDEX_MAX, TMU_FPCR_OFFSET);
	/* free pointer threshold: 0 = all pointers can be used */
	tmu_w32(0, TMU_FPTHR_OFFSET);
	/* elapsed time since last token accumulation */
	tmu_w32(0, TMU_TIMER_OFFSET);
	/* random number */
	tmu_w32(1, TMU_LFSR_OFFSET);
	/* WRED crawler period: TMU_SOC_REAL_BOARD => 0x10 (1024 clocks) */
	tmu_w32(0x10, TMU_CPR_OFFSET);
	/* current value WRED crawler counter, last queue id served */
	tmu_w32(0, TMU_CSR_OFFSET);
	/* global fill level (segments) */
	tmu_w32(0, TMU_GOCCR_OFFSET);
	/* all IRN irq acknowledged */
	tmu_w32(0, TMU_IRNCR_OFFSET);
	/* all IRN irq not set */
	tmu_w32(0, TMU_IRNICR_OFFSET);
	/* all IRN irq disabled */
	tmu_w32(0, TMU_IRNEN_OFFSET);

	for (i = 0; i < 4; i++) {
		/* global occupancy threshold n (color n discard) */
		tmu_w32(TMU_GOTH_DEFAULT, TMU_GOTHR0_OFFSET + (i * 4));
		/* global PDU discard counter register */
		tmu_w32(0, TMU_GPDCR0_OFFSET + (i * 4));
		/* Low Power Idle Configuration / Timer Status — not used on GRX500 */
		tmu_w32(0, TMU_LPIC0_OFFSET + (i * 4));
		tmu_w32(0, TMU_LPIT0_OFFSET + (i * 4));
	}

	/* queue fill status for queues 0..255, 8 entries of 32 bits */
	for (i = 0; i < 8; i++)
		tmu_w32(0, TMU_QFILL_BASE_OFFSET + (i * 4));

	/* egress port fill status for queues 0..71, 3 entries of 32 bits */
	for (i = 0; i < 3; i++)
		tmu_w32(0, TMU_EPFR_BASE_OFFSET + (i * 4));

	/* Token Bucket ID Capture Register */
	tmu_w32(0, TMU_TBIDCR_OFFSET);

	/* QID 254 (OMCI upstream) only — write QOCT0 QRTH bits.
	 * AVM drv_tmu_ll.c:430: tmu_w32_mask(TMU_QOCT0_QRTH_MASK, 0x100, qoct0);
	 */
	v = tmu_r32(TMU_QOCT0_OFFSET);
	v = (v & ~TMU_QOCT0_QRTH_MASK) | (0x100 & TMU_QOCT0_QRTH_MASK);
	tmu_w32(v, TMU_QOCT0_OFFSET);

	/* AVM drv_tmu_ll.c:432 tmu_relog_sequential(true)
	 * AVM drv_tmu_ll.c:2507-2510 — set CTRL.RPS bit. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_CTRL_OFFSET);
	v |= TMU_CTRL_RPS_RPS1;
	tmu_w32(v, TMU_CTRL_OFFSET);

	/* AVM drv_tmu_ll.c:436 tmu_token_accumulation_disable(false)
	 * AVM drv_tmu_ll.c:2495-2498 — clear CTRL.DTA bit. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_CTRL_OFFSET);
	v &= ~TMU_CTRL_DTA_DTA1;
	tmu_w32(v, TMU_CTRL_OFFSET);

	/* AVM drv_tmu_ll.c:438 tmu_max_token_bucket_set(TOKEN_BUCKET_MAX)
	 * AVM drv_tmu_ll.c:2517-2523 — set CTRL.MAXTB field. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_CTRL_OFFSET);
	v = (v & ~TMU_CTRL_MAXTB_MASK) |
	    ((TOKEN_BUCKET_MAX << TMU_CTRL_MAXTB_OFFSET) & TMU_CTRL_MAXTB_MASK);
	tmu_w32(v, TMU_CTRL_OFFSET);

	/* AVM drv_tmu_ll.c:440 tmu_random_number_set(0x0815)
	 * AVM drv_tmu_ll.c:2588-2594 — set LFSR.RN field. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_LFSR_OFFSET);
	v = (v & ~TMU_LFSR_RN_MASK) |
	    ((0x0815 << TMU_LFSR_RN_OFFSET) & TMU_LFSR_RN_MASK);
	tmu_w32(v, TMU_LFSR_OFFSET);

	/* AVM drv_tmu_ll.c:447 tmu_crawler_period_set(TMU_WRED_CRAWLER_PERIOD_DEFAULT)
	 * AVM drv_tmu_ll.c:2571-2577 — set CPR.CP field. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_CPR_OFFSET);
	v = (v & ~TMU_CPR_CP_MASK) |
	    ((TMU_WRED_CRAWLER_PERIOD_DEFAULT << TMU_CPR_CP_OFFSET) & TMU_CPR_CP_MASK);
	tmu_w32(v, TMU_CPR_OFFSET);

	/* AVM drv_tmu_ll.c:449 tmu_enqueue_delay_set(TMU_ENQUEUE_REQUEST_DELAY_DEFAULT)
	 * AVM drv_tmu_ll.c:2605-2611 — set ERDR.ERD field. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_ERDR_OFFSET);
	v = (v & ~TMU_ERDR_ERD_MASK) |
	    ((TMU_ENQUEUE_REQUEST_DELAY_DEFAULT << TMU_ERDR_ERD_OFFSET) & TMU_ERDR_ERD_MASK);
	tmu_w32(v, TMU_ERDR_OFFSET);

	/* AVM drv_tmu_ll.c:451 tmu_tacc_period_set(TMU_TOKEN_ACC_PERIOD_DEFAULT)
	 * AVM drv_tmu_ll.c:2622-2628 — set TACPER.TACP field. INLINED per plan Step 3a.
	 */
	v = tmu_r32(TMU_TACPER_OFFSET);
	v = (v & ~TMU_TACPER_TACP_MASK) |
	    ((TMU_TOKEN_ACC_PERIOD_DEFAULT << TMU_TACPER_TACP_OFFSET) & TMU_TACPER_TACP_MASK);
	tmu_w32(v, TMU_TACPER_OFFSET);
}

/* AVM drv_tmu_ll.c:762-840 — tmu_egress_queue_table_init.
 *
 * Egress Queue Table (EQT) zero-fill init covers QEMT / QSMT / QTHT /
 * QOCT / QDCT / QFMT for all EGRESS_QUEUE_ID_MAX=256 queues. KEEP_GPON_ALL
 * removed; CONFIG_LTQ_TMU_DDR_SIMULATE_REG removed.
 */
static void tmu_egress_queue_table_init(void)
{
	u32 i;
	u32 qtht[5];
	const u32 qdc[4] = { 0, 0, 0, 0 };
	const u32 qfm[3] = { 0, 0x3FFF3FFF, 0 };

	/* qtht[0]: queue disabled (QE=0), WRED dropping mode, color->threshold map */
	qtht[0] = 0x00011320;
	/* qtht[1]: MITH defaults — minimum threshold for WRED curve 0/1 */
	qtht[1] = (TMU_QTHT1_DEFAULT << 16) | TMU_QTHT1_DEFAULT;
	/* qtht[2]: MATH defaults — maximum threshold for WRED curve 0/1 */
	qtht[2] = (TMU_QTHT2_DEFAULT << 16) | TMU_QTHT2_DEFAULT;
	/* qtht[3]: SLOPE defaults — slope of WRED curve 0/1 */
	qtht[3] = (TMU_QTHT3_WRED_DEFAULT << 16) | TMU_QTHT3_WRED_DEFAULT;
	/* qtht[4]: tail-drop threshold 0/1 — defaults for red & unassigned */
	qtht[4] = (TMU_QTHT4_1_DEFAULT << 16) | TMU_QTHT4_0_DEFAULT;

	for (i = 0; i < EGRESS_QUEUE_ID_MAX; i++) {
		tmu_qemt_write(i, EPNNULL_EGRESS_PORT_ID);
		tmu_qsmt_write(i, NULL_SCHEDULER_INPUT_ID);
		tmu_qtht_write(i, &qtht[0]);
		/* TMU_SOC_REAL_BOARD = on, AVM: tmu_qoct_write(i, 10, 0, 0, 0) */
		tmu_qoct_write(i, 10, 0, 0, 0);
		tmu_qdct_write(i, &qdc[0]);
		tmu_qfmt_write(i, &qfm[0]);
	}
}

/*
 * tmu_egress_port_table_init - M0 milestone — EPOC zero-init at probe before
 * any traffic admit ().
 *
 * AVM drv_tmu_ll.c:1025-1056 (port-faithful). The EPOC zero-init loop
 * is the proof artifact for M0: every per-EP occupancy slot
 * is zero before the TMU state machine is activated, eliminating any
 * stale-counter race the AVM 4.9 boot path was vulnerable to.
 */
static void tmu_egress_port_table_init(void)
{
	u32 i;
	u32 epoc[2] = { 0, 0 };
	u32 ept[2] = {
		(TMU_EPTT0_DEFAULT << 16) | TMU_EPTT0_DEFAULT,
		(TMU_EPTT0_DEFAULT << 16) | TMU_EPTT0_DEFAULT
	};
	u32 epd[4] = { 0, 0, 0, 0 };

	for (i = 0; i < EGRESS_PORT_ID_MAX; i++) {
		/* egress port mapping table — all ports disabled,
		 * scheduler block ID = NULL_SCHEDULER_BLOCK_ID
		 */
		tmu_epmt_write(i, 0, NULL_SCHEDULER_BLOCK_ID);
		/* egress port fill level for color 0/1/2/3, initially empty */
		tmu_epot_write(i, &epoc[0]);
		/* egress port discard threshold for color 0/1/2/3 */
		tmu_eptt_write(i, &ept[0]);
		/* number of discarded PDUs for color 0/1/2/3 set to 0 */
		tmu_epdt_write(i, &epd[0]);
	}
}

/* AVM drv_tmu_ll.c:1065-1097 — tmu_sched_blk_in_table_init.
 * Scheduler Block Input Table (SBIT) — all inputs disabled, no shapers,
 * reserved queue 255 attached. KEEP_GPON_ALL / sim removed.
 */
static void tmu_sched_blk_in_table_init(void)
{
	u32 i;
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);

	for (i = 0; i < SCHEDULER_BLOCK_INPUT_ID_MAX; i++) {
		/* input disabled, queue type = queue */
		tmu_w32(0xFF, TMU_SBITR0_OFFSET);
		/* token bucket disabled, no shaping */
		tmu_w32(255, TMU_SBITR1_OFFSET);
		tmu_w32(0, TMU_SBITR2_OFFSET);
		tmu_w32(0, TMU_SBITR3_OFFSET);
		tmu_w32(TMU_SBITC_RW_W | TMU_SBITC_SEL | i, TMU_SBITC_OFFSET);
		while ((tmu_r32(TMU_SBITC_OFFSET) & TMU_SBITC_VAL) == 0)
			continue;
	}

	spin_unlock_irqrestore(&tmu_lock, flags);
}

/* AVM drv_tmu_ll.c:1104-1135 — tmu_sched_blk_out_table_init.
 * Scheduler Block Output Table (SBOT) — all 128 SBs unused, all outputs
 * disabled, every output connected to reserved egress port 72.
 */
static void tmu_sched_blk_out_table_init(void)
{
	u32 i;
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);

	for (i = 0; i < SCHEDULER_BLOCK_ID_MAX; i++) {
		/* output disabled, hierarchy level 0, connected to reserved
		 * egress port 72 (EPNNULL_EGRESS_PORT_ID).
		 */
		tmu_w32(EPNNULL_EGRESS_PORT_ID, TMU_SBOTR0_OFFSET);
		/* output initially not filled, default winner leaf, NIL winner QID */
		tmu_w32(0xFF, TMU_SBOTR1_OFFSET);
		tmu_w32(TMU_SBOTC_RW | TMU_SBOTC_SEL | i, TMU_SBOTC_OFFSET);
		while ((tmu_r32(TMU_SBOTC_OFFSET) & TMU_SBOTC_VAL) == 0)
			continue;
	}

	spin_unlock_irqrestore(&tmu_lock, flags);
}

/*
 * AVM drv_tmu_ll.c:1143-1195 — tmu_token_bucket_shaper_table_init. Token
 * Bucket Shaper Table (TBST) — all TBs disabled, color-blind, NIL scheduler
 * block input ID. KEEP_GPON_ALL / sim removed.
 */
static void tmu_token_bucket_shaper_table_init(void)
{
	u32 i;
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);

	for (i = 0; i <= TOKEN_BUCKET_MAX; i++) {
		/* color-blind TB attached to reserved scheduler input 1023 */
		tmu_w32(NULL_SCHEDULER_INPUT_ID, TMU_TBSTR0_OFFSET);
		/* all buckets disabled, 64-byte max wait, 1 byte per SRC elapse */
		tmu_w32(0xFFFF, TMU_TBSTR1_OFFSET);
		tmu_w32(0xFFFF, TMU_TBSTR2_OFFSET);
		/* bucket 0/1 max size — 0 blocks (lowest rate) */
		tmu_w32(0, TMU_TBSTR3_OFFSET);
		tmu_w32(0, TMU_TBSTR4_OFFSET);
		/* status values */
		tmu_w32(0, TMU_TBSTR5_OFFSET);
		tmu_w32(0, TMU_TBSTR6_OFFSET);
		tmu_w32(0, TMU_TBSTR7_OFFSET);
		tmu_w32(0, TMU_TBSTR8_OFFSET);
		tmu_w32(0, TMU_TBSTR9_OFFSET);
		tmu_w32(0, TMU_TBSTR10_OFFSET);
		tmu_w32(TMU_TBSTC_RW | TMU_TBSTC_SEL | i, TMU_TBSTC_OFFSET);
		while ((tmu_r32(TMU_TBSTC_OFFSET) & TMU_TBSTC_VAL) == 0)
			continue;
	}

	spin_unlock_irqrestore(&tmu_lock, flags);
}

/*
 * AVM drv_tmu_ll.c:1201-1232 — tmu_packet_pointer_table_init. PPT linked-list
 * set-up. KEEP_GPON_ALL / sim removed.
 */
static void tmu_packet_pointer_table_init(void)
{
	u32 i, tmp;
	unsigned long flags;

	spin_lock_irqsave(&tmu_lock, flags);

	for (i = 0; i < PACKET_POINTER_TABLE_INDEX_MAX; i++) {
		tmp = (i + 1) % PACKET_POINTER_TABLE_INDEX_MAX;
		tmu_w32((tmp << TMU_PPT0_PNEXT_OFFSET) & TMU_PPT0_PNEXT_MASK,
			TMU_PPT0_OFFSET);
		tmu_w32(0, TMU_PPT1_OFFSET);
		tmu_w32(0, TMU_PPT2_OFFSET);
		tmu_w32(0, TMU_PPT3_OFFSET);
		tmu_w32(TMU_PPTC_RW | i, TMU_PPTC_OFFSET);
		while ((tmu_r32(TMU_PPTC_OFFSET) & TMU_PPTC_VAL) == 0)
			continue;
	}

	spin_unlock_irqrestore(&tmu_lock, flags);

	/* Seed FPL.TFPP with the last linked-list entry. */
	tmu_w32(((PACKET_POINTER_TABLE_INDEX_MAX - 1) << TMU_FPL_TFPP_OFFSET) &
		TMU_FPL_TFPP_MASK, TMU_FPL_OFFSET);
}

/*
 * __tmu_ll_init — port of AVM drv_tmu_ll.c:224-293 (tmu_ll_init body).
 *
 * (c) spinlock initialization moved to file-scope DEFINE_SPINLOCK above. (d)
 * tmu_ll_stack_init / KEEP_GPON_ALL / CONFIG_LTQ_TMU_CHIPTEST /
 * tmu_max_print_num / proc_*_id init: all dropped. (e) Sub-init call order
 * matches AVM verbatim. (f) tmu_enable(true) caps the bring-up. (g) NOT
 * static — tmu_init.c calls via file-scope extern. The forward declaration
 * silences -Wmissing-prototypes.
 */
int __tmu_ll_init(void);

int __tmu_ll_init(void)
{
	tmu_basic_init();
	tmu_egress_queue_table_init();
	tmu_egress_port_table_init();
	tmu_sched_blk_in_table_init();
	tmu_sched_blk_out_table_init();
	tmu_token_bucket_shaper_table_init();
	tmu_packet_pointer_table_init();
	tmu_enable(true);
	return 0;
}

/*
 * tmu_egress_port_enable - flip the EPE bit on EPMT[epn] then commit via the
 * CFGCMD reconfiguration channel.
 */
void tmu_egress_port_enable(uint32_t epn, bool ena)
{
	u32 cfgcmd = 0;
	u32 old_epe, sbid;

	if (!atomic_read(&tmu_init_done)) {
		WARN_ONCE(1, "%s called before tmu_init\n", __func__);
		return;
	}

	cfgcmd |= ena ? TMU_CFGCMD_CMD_EP_ON : TMU_CFGCMD_CMD_EP_OFF;
	cfgcmd |= (epn << TMU_CFGEPN_EPN_OFFSET) & TMU_CFGEPN_EPN_MASK;
	tmu_epmt_read(epn, &old_epe, &sbid);
	tmu_epmt_write(epn, ena ? 1 : 0, sbid);
	tmu_cfgcmd_write(cfgcmd);
}

/*
 * Ports AVM drv_tmu_ll.c:2819-2847 (tmu_create_flat_egress_path) plus the
 * three high-level link helpers it calls — tmu_sched_blk_create
 * (drv_tmu_ll.c:2725-2751), tmu_egress_queue_create (2754-2782) and
 * tmu_equeue_enable (1234-1247). The AVM helpers do read-modify-write through
 * tmu_sbot_read_cfg / tmu_sbit_read / tmu_qtht_read. Because this builder runs
 * once per LAN port immediately after tmu_init has zero-initialised every
 * SBOT/SBIT/QEMT/QSMT/QTHT entry (table_init above), the RMW collapses to an
 * absolute write of the freshly-computed link state — so these ports compose
 * the same low-level indirect-table writes already present (tmu_qemt_write,
 * tmu_qsmt_write, tmu_qtht_write, tmu_epmt_write) plus inline SBOT/SBIT
 * indirect writes that mirror tmu_sched_blk_*_table_init's idiom. The
 * resulting register state is bit-for-bit identical to the AVM RMW path on a
 * fresh boot.
 */

/* AVM drv_tmu_ll.c:2230-2252 tmu_sched_blk_out_link_set, fresh-state form:
 * SBOTR0 = SOE | (V?V_SBIN:0) | lvl | omid. v==0 also links the egress port
 * back to this SB via EPMT[omid].SBID = sbid (tmu_egress_port_link_set).
 */
static void tmu_ll_sched_blk_create(uint32_t sbid, uint8_t lvl, uint32_t omid,
				    uint8_t v, uint16_t weight)
{
	unsigned long flags;
	u32 sbot0 = TMU_SBOTR0_SOE_EN;
	u32 epe, ep_sbid;

	sbot0 |= (lvl << TMU_SBOTR0_LVL_OFFSET) & TMU_SBOTR0_LVL_MASK;
	sbot0 |= (omid << TMU_SBOTR0_OMID_OFFSET) & TMU_SBOTR0_OMID_MASK;
	if (v)
		sbot0 |= TMU_SBOTR0_V_SBIN;

	/* SBOT[sbid] config write (AVM tmu_sbot_write_cfg idiom). */
	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(sbot0, TMU_SBOTR0_OFFSET);
	tmu_w32(TMU_SBOTC_RW_W | sbid, TMU_SBOTC_OFFSET);
	while ((tmu_r32(TMU_SBOTC_OFFSET) & TMU_SBOTC_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);

	if (v == 0) {
		/* tmu_egress_port_link_set: EPMT[omid].SBID = sbid (keep EPE). */
		tmu_epmt_read(omid, &epe, &ep_sbid);
		tmu_epmt_write(omid, epe, sbid);
	} else {
		/* Higher-level SB input link (unused for flat LAN path). */
		u32 sbit0 = TMU_SBITR0_SIE_EN | TMU_SBITR0_SIT_SBID;

		sbit0 |= (weight << TMU_SBITR0_IWGT_OFFSET) &
			 TMU_SBITR0_IWGT_MASK;
		sbit0 |= (sbid << TMU_SBITR0_QSID_OFFSET) & TMU_SBITR0_QSID_MASK;
		spin_lock_irqsave(&tmu_lock, flags);
		tmu_w32(sbit0, TMU_SBITR0_OFFSET);
		tmu_w32(255, TMU_SBITR1_OFFSET);
		tmu_w32(0, TMU_SBITR2_OFFSET);
		tmu_w32(0, TMU_SBITR3_OFFSET);
		tmu_w32(TMU_SBITC_RW_W | TMU_SBITC_SEL | omid, TMU_SBITC_OFFSET);
		while ((tmu_r32(TMU_SBITC_OFFSET) & TMU_SBITC_VAL) == 0)
			continue;
		spin_unlock_irqrestore(&tmu_lock, flags);
	}
}

/* AVM drv_tmu_ll.c:2758-2782 tmu_egress_queue_create, fresh-state form:
 *   QEMT[qid].EPN = epn; QSMT[qid].SBIN = sbin; QTHT[qid].QE = 1;
 *   SBIT[sbin] input link -> this queue (SIE=1, SIT=0=queue), token bucket off.
 */
static void tmu_ll_egress_queue_create(uint32_t qid, uint32_t sbin, uint32_t epn)
{
	unsigned long flags;
	u32 qtht[5];
	u32 sbit0;

	/* QEMT / QSMT absolute write (tables are zeroed at init). */
	tmu_qemt_write(qid, (epn << TMU_QEMT_EPN_OFFSET) & TMU_QEMT_EPN_MASK);
	tmu_qsmt_write(qid, (sbin << TMU_QSMT_SBIN_OFFSET) & TMU_QSMT_SBIN_MASK);

	/* tmu_equeue_enable(qid, 1) + tmu_reset_queue_threshold(qid): set the
	 * AVM default WRED/tail-drop thresholds with QE=1.
	 */
	qtht[0] = 0x00011320 | TMU_QTHT0_QE_EN;
	qtht[1] = (TMU_QTHT1_DEFAULT << 16) | TMU_QTHT1_DEFAULT;
	qtht[2] = (TMU_QTHT2_DEFAULT << 16) | TMU_QTHT2_DEFAULT;
	qtht[3] = (TMU_QTHT3_WRED_DEFAULT << 16) | TMU_QTHT3_WRED_DEFAULT;
	qtht[4] = (TMU_QTHT4_1_DEFAULT << 16) | TMU_QTHT4_0_DEFAULT;
	tmu_qtht_write(qid, qtht);

	tmu_cfgcmd_write(TMU_CFGCMD_CMD_SB_INPUT_ON |
			 ((sbin << TMU_CFGSBIN_SBIN_OFFSET) &
			  TMU_CFGSBIN_SBIN_MASK));

	/* SBIT[sbin] input link -> qid (AVM tmu_sched_blk_in_link_set fresh):
	 * SIE=1, SIT=0 (qsid is a queue id), IWGT=0, token bucket disabled.
	 */
	sbit0 = TMU_SBITR0_SIE_EN;
	sbit0 |= (qid << TMU_SBITR0_QSID_OFFSET) & TMU_SBITR0_QSID_MASK;
	spin_lock_irqsave(&tmu_lock, flags);
	tmu_w32(sbit0, TMU_SBITR0_OFFSET);
	tmu_w32(255, TMU_SBITR1_OFFSET);  /* TBE=0, TBID=255 (no shaper) */
	tmu_w32(0, TMU_SBITR2_OFFSET);
	tmu_w32(0, TMU_SBITR3_OFFSET);
	tmu_w32(TMU_SBITC_RW_W | TMU_SBITC_SEL | sbin, TMU_SBITC_OFFSET);
	while ((tmu_r32(TMU_SBITC_OFFSET) & TMU_SBITC_VAL) == 0)
		continue;
	spin_unlock_irqrestore(&tmu_lock, flags);
}

void tmu_create_flat_egress_path(uint16_t num_ports, uint16_t base_epn,
				 uint16_t base_sbid, uint16_t base_qid,
				 uint16_t qid_per_sb)
{
	uint16_t epn;
	uint16_t qid;

	if (!atomic_read(&tmu_init_done)) {
		WARN_ONCE(1, "%s called before tmu_init\n", __func__);
		return;
	}

	if (qid_per_sb > SCHEDULE_MAX_LEVEL) {
		pr_warn("tmu: %s: qid_per_sb(%u) > %u, clamping\n", __func__,
			qid_per_sb, SCHEDULE_MAX_LEVEL);
		qid_per_sb = SCHEDULE_MAX_LEVEL;
	}

	for (epn = 0; epn < num_ports; epn++) {
		/* SB output -> egress port (V=0, level 0). */
		tmu_ll_sched_blk_create(base_sbid + epn, 0, base_epn + epn, 0, 0);
		/* EPMT[epn].EPE = 1 (commit via CFGCMD). */
		tmu_egress_port_enable(base_epn + epn, true);

		for (qid = epn * qid_per_sb;
		     qid < epn * qid_per_sb + qid_per_sb; qid++) {
			tmu_ll_egress_queue_create(base_qid + qid,
						   ((base_sbid + epn) << 3) +
							   (qid % 8),
						   base_epn + epn);
		}

		pr_info("tmu: egress path built: epn=%u sbid=%u qid=%u (EPMT.EPE=1)\n",
			base_epn + epn, base_sbid + epn, base_qid + epn);
	}
}
EXPORT_SYMBOL(tmu_create_flat_egress_path);

/* Per-queue counter-ring zero helpers. */

/* AVM cqm/grx500/cbm.c:4813-4817 — reset_enq_counter (XRX500). */
static void reset_enq_counter(uint32_t qid)
{
	if (!g_cbm_qeqcnt_base) {
		WARN_ONCE(1, "%s: g_cbm_qeqcnt_base NULL\n", __func__);
		return;
	}
	__raw_writel(0, (u32 __force *)g_cbm_qeqcnt_base + qid);
}

/* AVM cqm/grx500/cbm.c:4819-4823 — reset_deq_counter (XRX500). */
static void reset_deq_counter(uint32_t qid)
{
	if (!g_cbm_qdqcnt_base) {
		WARN_ONCE(1, "%s: g_cbm_qdqcnt_base NULL\n", __func__);
		return;
	}
	__raw_writel(0, (u32 __force *)g_cbm_qdqcnt_base + qid);
}

/* tmu_reset_mib_all - zero every per-queue MIB counter. */
int tmu_reset_mib_all(void)
{
	uint32_t qdc[4] = { 0, 0, 0, 0 };
	uint32_t i;

	if (!atomic_read(&tmu_init_done)) {
		WARN_ONCE(1, "%s called before tmu_init\n", __func__);
		return -EAGAIN;
	}
	if (!g_cbm_qeqcnt_base) {
		WARN_ONCE(1, "%s: g_cbm_qeqcnt_base NULL\n", __func__);
		return -EINVAL;
	}
	if (!g_cbm_qdqcnt_base) {
		WARN_ONCE(1, "%s: g_cbm_qdqcnt_base NULL\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < EGRESS_QUEUE_ID_MAX; i++) {
		reset_enq_counter(i);
		reset_deq_counter(i);
		tmu_qdct_write(i, qdc);
	}
	return 0;
}

/*
 * Production builds carry zero overhead — the entire block compiles to
 * nothing when CONFIG_INTEL_XRX500_GSWIP_API_DEBUG is unset.
 *
 * Mock-buffer details: - 4 KiB is large enough to cover the entire TMU
 * register window through TMU_CFGCMD_OFFSET=0x720. The plan's original 1 KiB
 * sizing is recorded in build-manifest notes — 1 KiB is insufficient. The
 * mock_mmio_active flag arms the tmu_w32 shim so the indirect- access
 * busy-waits exit on first read against plain memory. Each test
 * saves/restores g_cbm_tmu_base, tmu_mock_mmio_active, and tmu_init_done so
 * subsequent tests start from a clean state.
 *
 * Reordering breaks the dependency invariants between the tests.
 */
#ifdef CONFIG_INTEL_XRX500_GSWIP_API_DEBUG

#define TMU_MOCK_BUFSZ 4096

static int tmu_epoc_zero_init_mock_check(void)
{
	void *mock_buf, *saved;
	void __iomem *saved_base;
	u32 epot0_final, epot1_final;
	int ret = 0;

	mock_buf = kmalloc(TMU_MOCK_BUFSZ, GFP_KERNEL);
	saved = kmalloc(TMU_MOCK_BUFSZ, GFP_KERNEL);
	if (!mock_buf || !saved) {
		ret = -ENOMEM;
		goto out;
	}

	saved_base = g_cbm_tmu_base;

	/* Round 1 — capture deterministic post-init state. */
	memset(mock_buf, 0xAA, TMU_MOCK_BUFSZ);
	g_cbm_tmu_base = (void __iomem *)mock_buf;
	tmu_mock_mmio_active = true;

	ret = __tmu_ll_init();
	if (ret) {
		pr_err("tmu: mock __tmu_ll_init round1 failed: %d\n", ret);
		goto restore;
	}

	epot0_final = tmu_r32(TMU_EPOT0_OFFSET);
	epot1_final = tmu_r32(TMU_EPOT1_OFFSET);
	if (epot0_final != 0 || epot1_final != 0) {
		pr_err("tmu: EPOC zero-init read-back nonzero (EPOT0=%#x EPOT1=%#x)\n",
		       epot0_final, epot1_final);
		ret = -EIO;
		goto restore;
	}

	memcpy(saved, mock_buf, TMU_MOCK_BUFSZ);

	/* Round 2 — same prefill, must produce byte-for-byte identical state. */
	memset(mock_buf, 0xAA, TMU_MOCK_BUFSZ);
	ret = __tmu_ll_init();
	if (ret) {
		pr_err("tmu: mock __tmu_ll_init round2 failed: %d\n", ret);
		goto restore;
	}

	if (memcmp(saved, mock_buf, TMU_MOCK_BUFSZ) != 0) {
		pr_err("tmu: determinism failure — mock buffer diverged across runs\n");
		ret = -EIO;
		goto restore;
	}

	pr_info("tmu: EPOC zero-init determinism check PASS\n");

restore:
	tmu_mock_mmio_active = false;
	g_cbm_tmu_base = saved_base;
out:
	kfree(mock_buf);
	kfree(saved);
	return ret;
}

static int tmu_as013_guard_check(void)
{
	void *mock_buf;
	void __iomem *saved_base;
	const u8 sentinel = 0xAA;
	int ret = 0;
	size_t i;

	mock_buf = kmalloc(TMU_MOCK_BUFSZ, GFP_KERNEL);
	if (!mock_buf)
		return -ENOMEM;

	saved_base = g_cbm_tmu_base;

	/* Confirm guard state — tmu_init_done MUST be 0 going in. */
	if (atomic_read(&tmu_init_done) != 0) {
		pr_err("tmu: setup invariant violated (tmu_init_done!=0)\n");
		ret = -EINVAL;
		goto out;
	}

	memset(mock_buf, sentinel, TMU_MOCK_BUFSZ);
	g_cbm_tmu_base = (void __iomem *)mock_buf;
	tmu_mock_mmio_active = true;

	/* Pre-init call — must WARN_ONCE and return without any MMIO. */
	tmu_egress_port_enable(0, true);

	/* Assert every byte still equals the prefill — no MMIO touched. */
	for (i = 0; i < TMU_MOCK_BUFSZ; i++) {
		u8 v = ((const u8 *)mock_buf)[i];

		if (v != sentinel) {
			pr_err("tmu: guard breach at off=%zu (got %#x, expected %#x)\n",
			       i, v, sentinel);
			ret = -EIO;
			goto restore;
		}
	}

	pr_info("tmu: EPOC ordering guard check PASS\n");

restore:
	tmu_mock_mmio_active = false;
	g_cbm_tmu_base = saved_base;
out:
	kfree(mock_buf);
	return ret;
}

static int tmu_egress_port_enable_protocol_check(void)
{
	void *mock_buf;
	void __iomem *saved_base;
	u32 cfgcmd_v, epmt_v;
	const u32 test_epn = 3;
	int ret = 0;

	mock_buf = kmalloc(TMU_MOCK_BUFSZ, GFP_KERNEL);
	if (!mock_buf)
		return -ENOMEM;

	saved_base = g_cbm_tmu_base;

	memset(mock_buf, 0, TMU_MOCK_BUFSZ);
	g_cbm_tmu_base = (void __iomem *)mock_buf;
	tmu_mock_mmio_active = true;
	atomic_set(&tmu_init_done, 1);

	tmu_egress_port_enable(test_epn, true);

	/*
	 * Inspect the CFGCMD slot. The two-step protocol's commit-write packs
	 * TMU_CFGCMD_CMD_EP_ON into the opcode field and the EPN into the EPN
	 * field.
	 */
	cfgcmd_v = tmu_r32(TMU_CFGCMD_OFFSET);
	if ((cfgcmd_v & TMU_CFGCMD_CMD_MASK) != TMU_CFGCMD_CMD_EP_ON) {
		pr_err("tmu: CFGCMD opcode mismatch (got %#x, want %#x)\n",
		       cfgcmd_v & TMU_CFGCMD_CMD_MASK,
		       (u32)TMU_CFGCMD_CMD_EP_ON);
		ret = -EIO;
		goto restore;
	}
	if ((cfgcmd_v & TMU_CFGEPN_EPN_MASK) != test_epn) {
		pr_err("tmu: CFGCMD EPN mismatch (got %#x, want %u)\n",
		       cfgcmd_v & TMU_CFGEPN_EPN_MASK, test_epn);
		ret = -EIO;
		goto restore;
	}

	/* Step-1 proof — EPMT EPE bit set by the read-modify-write. */
	epmt_v = tmu_r32(TMU_EPMT_OFFSET);
	if (!(epmt_v & TMU_EPMT_EPE)) {
		pr_err("tmu: EPMT EPE bit not set after EPE enable (got %#x)\n",
		       epmt_v);
		ret = -EIO;
		goto restore;
	}

	pr_info("tmu: egress-port-enable protocol check PASS\n");

restore:
	/*
	 * Reset tmu_init_done so the next debug-self-test pass (or any
	 * production caller) observes the initial guard state.
	 */
	atomic_set(&tmu_init_done, 0);
	tmu_mock_mmio_active = false;
	g_cbm_tmu_base = saved_base;
	kfree(mock_buf);
	return ret;
}

static int tmu_reset_mib_all_check(void)
{
	void *qeqcnt_buf, *qdqcnt_buf, *tmu_buf;
	void __iomem *saved_qeq, *saved_qdq, *saved_tmu;
	const u8 sentinel = 0xAA;
	const size_t cnt_sz = EGRESS_QUEUE_ID_MAX * sizeof(u32);
	int ret = 0;
	size_t i;

	qeqcnt_buf = kmalloc(cnt_sz, GFP_KERNEL);
	qdqcnt_buf = kmalloc(cnt_sz, GFP_KERNEL);
	tmu_buf = kmalloc(TMU_MOCK_BUFSZ, GFP_KERNEL);
	if (!qeqcnt_buf || !qdqcnt_buf || !tmu_buf) {
		ret = -ENOMEM;
		goto out;
	}

	saved_qeq = g_cbm_qeqcnt_base;
	saved_qdq = g_cbm_qdqcnt_base;
	saved_tmu = g_cbm_tmu_base;

	/* Confirm guard state — tmu_init_done MUST be 0 going in. */
	if (atomic_read(&tmu_init_done) != 0) {
		pr_err("tmu: reset-mib setup invariant violated (tmu_init_done!=0)\n");
		ret = -EINVAL;
		goto restore;
	}

	memset(qeqcnt_buf, sentinel, cnt_sz);
	memset(qdqcnt_buf, sentinel, cnt_sz);
	memset(tmu_buf, 0, TMU_MOCK_BUFSZ);
	g_cbm_qeqcnt_base = (void __iomem *)qeqcnt_buf;
	g_cbm_qdqcnt_base = (void __iomem *)qdqcnt_buf;
	g_cbm_tmu_base = (void __iomem *)tmu_buf;
	tmu_mock_mmio_active = true;

	ret = tmu_reset_mib_all();
	if (ret != -EAGAIN) {
		pr_err("tmu: guard return mismatch (got %d, want -EAGAIN)\n",
		       ret);
		ret = -EIO;
		goto restore;
	}
	for (i = 0; i < cnt_sz; i++) {
		if (((const u8 *)qeqcnt_buf)[i] != sentinel) {
			pr_err("tmu: QEQCNTR breach at byte %zu (got %#x)\n",
			       i, ((const u8 *)qeqcnt_buf)[i]);
			ret = -EIO;
			goto restore;
		}
		if (((const u8 *)qdqcnt_buf)[i] != sentinel) {
			pr_err("tmu: QDQCNTR breach at byte %zu (got %#x)\n",
			       i, ((const u8 *)qdqcnt_buf)[i]);
			ret = -EIO;
			goto restore;
		}
	}
	pr_info("tmu: reset-mib ordering guard check PASS\n");

	atomic_set(&tmu_init_done, 1);
	ret = tmu_reset_mib_all();
	if (ret != 0) {
		pr_err("tmu: reset call returned %d (want 0)\n", ret);
		ret = -EIO;
		goto restore;
	}
	for (i = 0; i < EGRESS_QUEUE_ID_MAX; i++) {
		u32 v;

		v = ((const u32 *)qeqcnt_buf)[i];
		if (v != 0) {
			pr_err("tmu: QEQCNTR[%zu]=%#x (want 0)\n", i, v);
			ret = -EIO;
			goto restore;
		}
		v = ((const u32 *)qdqcnt_buf)[i];
		if (v != 0) {
			pr_err("tmu: QDQCNTR[%zu]=%#x (want 0)\n", i, v);
			ret = -EIO;
			goto restore;
		}
	}
	ret = 0;
	pr_info("tmu: mock-counter zero invariant PASS\n");

restore:
	/* Reset tmu_init_done so subsequent tests / production code observe
	 * the initial guard state.
	 */
	atomic_set(&tmu_init_done, 0);
	tmu_mock_mmio_active = false;
	g_cbm_qeqcnt_base = saved_qeq;
	g_cbm_qdqcnt_base = saved_qdq;
	g_cbm_tmu_base = saved_tmu;
out:
	kfree(qeqcnt_buf);
	kfree(qdqcnt_buf);
	kfree(tmu_buf);
	return ret;
}

/*
 * tmu_debug_self_tests - late_initcall dispatcher for the four
 * CONFIG_INTEL_XRX500_GSWIP_API_DEBUG self-tests.
 *
 * Wired via late_initcall so it runs deterministically AFTER all
 * subsystem init but BEFORE any user-space process touches the TMU.
 * Each test's failure return aborts the dispatcher; the kernel boots
 * regardless (the dispatcher returns the error, but late_initcall
 * does not propagate errors out of initcall ordering).
 *
 * Test ordering is significant — each test relies on tmu_init_done == 0 on
 * entry and resets it back to 0 at restore.
 */
static int __init tmu_debug_self_tests(void)
{
	int ret;

	pr_info("tmu: CONFIG_INTEL_XRX500_GSWIP_API_DEBUG self-tests starting\n");

	ret = tmu_epoc_zero_init_mock_check();
	if (ret) {
		pr_err("tmu: determinism check FAILED (%d)\n", ret);
		return ret;
	}

	ret = tmu_as013_guard_check();
	if (ret) {
		pr_err("tmu: ordering-guard check FAILED (%d)\n", ret);
		return ret;
	}

	ret = tmu_egress_port_enable_protocol_check();
	if (ret) {
		pr_err("tmu: protocol check FAILED (%d)\n", ret);
		return ret;
	}

	ret = tmu_reset_mib_all_check();
	if (ret) {
		pr_err("tmu: reset-mib-all check FAILED (%d)\n", ret);
		return ret;
	}

	pr_info("tmu: all CONFIG_INTEL_XRX500_GSWIP_API_DEBUG self-tests PASS\n");
	return 0;
}
late_initcall(tmu_debug_self_tests);

#endif /* CONFIG_INTEL_XRX500_GSWIP_API_DEBUG */
