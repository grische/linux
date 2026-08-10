// SPDX-License-Identifier: GPL-2.0-only
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.c.
 *
 * Free Segment Queue Manager: brings up one FSQM instance and seeds its
 * linked-list table so the hardware owns a pool of buffer segments.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/io.h>
#include <linux/bug.h>
#include <linux/types.h>

#include "cbm.h"

/*
 *   AVM fsqm.h:564 OFSQ_HEAD_POS    = 0
 *   AVM fsqm.h:566 OFSQ_HEAD_MASK   = 0x7FFFu
 *   AVM fsqm.h:569 OFSQ_TAIL_POS    = 16
 *   AVM fsqm.h:571 OFSQ_TAIL_MASK   = 0x7FFF0000u
 *   AVM fsqm.h:587 OFSC_FSC_POS     = 0
 *   AVM fsqm.h:589 OFSC_FSC_MASK    = 0x7FFFu
 *   AVM fsqm.h:695 LSARNG_MINLSA_POS  = 0
 *   AVM fsqm.h:697 LSARNG_MINLSA_MASK = 0x7FFFu
 *   AVM fsqm.h:700 LSARNG_MAXLSA_POS  = 16
 *   AVM fsqm.h:702 LSARNG_MAXLSA_MASK = 0x7FFF0000u
 */
#define LSARNG_MINLSA_POS  0
#define LSARNG_MINLSA_MASK 0x00007FFFu
#define LSARNG_MAXLSA_POS  16
#define LSARNG_MAXLSA_MASK 0x7FFF0000u
#define OFSQ_HEAD_POS      0
#define OFSQ_HEAD_MASK     0x00007FFFu
#define OFSQ_TAIL_POS      16
#define OFSQ_TAIL_MASK     0x7FFF0000u
#define OFSC_FSC_POS       0
#define OFSC_FSC_MASK      0x00007FFFu

/* FSQM end-of-list sentinel. AVM cqm/grx500/cbm.c:1106 / 1110-1112:
 *   the last linked-list slot carries the literal 0x7FFF (NOT 0xFFFF
 *   and NOT 0x7FFFFFFF — the HW only consumes the low 15 bits and a
 *   wider literal would alias into the reserved bits 15..31).
 */
#define FSQM_LLT_END_MARKER 0x7FFFu

/*
 * init_fsqm_buf_std — seed the FSQM Linked-List Table RAM.
 *
 * Per AVM cqm/grx500/cbm.c:1103-1116, the loop bound is INCLUSIVE
 * (i = 1..frm_num), and the i==frm_num-1 iteration emits the
 * end-marker 0x7FFF into slot (frm_num-2). All other iterations
 * write (i % frm_num) — so the i=frm_num iteration writes slot
 * (frm_num-1) with value 0 (since frm_num % frm_num = 0).
 *
 * Worked example (frm_num=8):
 *   i=1 slot 0 -> 1     i=2 slot 1 -> 2     i=3 slot 2 -> 3
 *   i=4 slot 3 -> 4     i=5 slot 4 -> 5     i=6 slot 5 -> 6
 *   i=7 slot 6 -> 0x7FFF (END-MARKER)
 *   i=8 slot 7 -> 0 (8 % 8 = 0)
 *
 * MUST NOT REWRITE the loop to i=0..frm_num-1: doing so writes slot 0
 * with (0 % frm_num) = 0 instead of 1, and slot frm_num-1 is left
 * uninitialised — the FSQM deadlocks on first buffer return.
 *
 * After seeding, a sampled WARN_ONCE self-check verifies the seed
 * survived the writes. The check samples at most 8 random slots
 * (avoids log-spam at frm_num=2048).
 */
static void init_fsqm_buf_std(int idx, u32 frm_num)
{
	u32 i;
	u32 samples;
	u32 sample_step;

	/* AVM cqm/grx500/cbm.c:1103-1116 — i = 1..frm_num INCLUSIVE.
	 * MUST NOT REWRITE to i=0..frm_num-1 (silent deadlock).
	 *
	 * Per-slot RCNT[i-1] = 1 is written ALONGSIDE the LLT seed inside
	 * this loop — AVM cbm.c:1112-1115 (std, #else branch) and AVM
	 * cbm.c:1156-1157 (jbo, unconditional). Pulling the RCNT write out
	 * of the loop (as the attempt-1 plan-step did) leaves slots
	 * RCNT[1..frm_num-1] at the silicon-reset value (0) and risks
	 * FSQM deadlock on first descriptor return. MUST NOT MOVE the
	 * RCNT write back out of this loop.
	 */
	for (i = 1; i <= frm_num; i++) {
		u32 slot = i - 1;
		u32 val  = (i == frm_num - 1) ? FSQM_LLT_END_MARKER
					      : (i % frm_num);

		fsqm_w32(idx, RAM  + (slot << 2), val);
		fsqm_w32(idx, RCNT + (slot << 2), 1);
	}

	/*
	 * Sampled readback self-check — at most 8 slots, evenly spaced. Uses
	 * dynamic expected = ((i+1) == frm_num-1 ? 0x7FFF : (i+1) % frm_num)
	 * so any future frm_num bump stays correct.
	 *
	 * Force-check those two slots after the sweep — they are the slots a
	 * regression to the loop bound or end-marker conditional would
	 * silently corrupt. Total readback ≤ 10 slots, well below log-spam
	 * threshold.
	 */
	samples = (frm_num < 8) ? frm_num : 8;
	sample_step = frm_num / samples;
	if (!sample_step)
		sample_step = 1;
	for (i = 0; i < samples; i++) {
		u32 slot = i * sample_step;
		u32 expected;
		u32 read_val;

		if (slot >= frm_num)
			break;

		/* Loop wrote slot S with value V where (S = j-1, V depends
		 * on j) for j=1..frm_num. So expected[S] is:
		 *   if (S == frm_num - 2)  -> 0x7FFF (end marker; j=frm_num-1)
		 *   else                   -> (S + 1) % frm_num
		 */
		if (slot == frm_num - 2)
			expected = FSQM_LLT_END_MARKER;
		else
			expected = (slot + 1) % frm_num;

		read_val = fsqm_r32(idx, RAM + (slot << 2));
		WARN_ONCE(read_val != expected,
			  "cbm: FSQM LLT_RAM seed mismatch idx=%d slot=%u got=0x%08x expected=0x%08x\n",
			  idx, slot, read_val, expected);
	}

	/* Boundary slot coverage: end-marker slot (frm_num-2) MUST hold
	 * 0x7FFF and wrap-terminator slot (frm_num-1) MUST hold 0
	 * (frm_num % frm_num = 0). These are the two slots whose values
	 * are non-trivial under the AVM seed contract; any mutation to the
	 * loop bound or end-marker conditional would corrupt them.
	 * Guarded by frm_num >= 2 (already enforced at init_fsqm entry).
	 */
	if (frm_num >= 2) {
		u32 em_slot = frm_num - 2;
		u32 wrap_slot = frm_num - 1;
		u32 em_val = fsqm_r32(idx, RAM + (em_slot << 2));
		u32 wrap_val = fsqm_r32(idx, RAM + (wrap_slot << 2));

		WARN_ONCE(em_val != FSQM_LLT_END_MARKER,
			  "cbm: FSQM LLT_RAM end-marker missing idx=%d slot=%u got=0x%08x expected=0x%08x\n",
			  idx, em_slot, em_val, FSQM_LLT_END_MARKER);
		WARN_ONCE(wrap_val != 0,
			  "cbm: FSQM LLT_RAM wrap-terminator non-zero idx=%d slot=%u got=0x%08x expected=0\n",
			  idx, wrap_slot, wrap_val);
	}
}

/*
 * init_fsqm_by_idx - register-sequence init for one FSQM instance.
 *
 * AVM cqm/grx500/cbm.c:1103-1136 + 1086-1091 source ordering
 * (preserving the bit-packed LSARNG/OFSQ/OFSC operands computed by the
 * caller). The write sequence is:
 *
 * FSQM_CTRL is written LAST so the engine cannot consume a partial config
 * snapshot.
 */
static void init_fsqm_by_idx(int idx, u32 frm_num, u32 lsarng,
			     u32 ofsq, u32 ofsc)
{
	u32 fsqt0_expected;
	u32 fsqt1_expected;
	u32 fsqt2_expected;
	u32 fsqt3_expected;
	u32 fsqt4_expected;


	/*
	 * The seed loop in init_fsqm_buf_std also writes RCNT[i-1]=1 per slot
	 * (AVM cbm.c:1112-1115 std / 1156-1157 jbo), so no separate RCNT
	 * write is needed here. MUST NOT re-add a single fsqm_w32(idx, RCNT,
	 * 1) — that would touch only slot 0 and leave the per-slot writes
	 * untouched (functionally a no-op overwrite, but cosmetically
	 * misleading).
	 */

	/* (4) (5) (6) LSARNG / OFSQ / OFSC bit-packed values, computed by
	 *     init_fsqm() per AVM cbm.c:1121-1128.
	 */
	fsqm_w32(idx, LSARNG, lsarng);
	fsqm_w32(idx, OFSQ,   ofsq);
	fsqm_w32(idx, OFSC,   ofsc);

	fsqm_w32(idx, FSQM_IRNEN, FSQM_IRNEN_DEFAULT);

	/* (8) FSQT0..FSQT4 watermarks.
	 *     AVM cbm.c:1130-1134 uses left-to-right integer division:
	 *     (frm_num / 6) * N  — NOT (frm_num * N) / 6 — which yields a
	 *     different value at frm_num=2048.
	 *     MUST NOT REWRITE to (frm_num*N)/6: at frm_num=2048 it gives
	 *     1706/1365/1024/682/341 instead of 1705/1364/1023/682/341.
	 */
	fsqt0_expected = frm_num / 6 * 5;
	fsqt1_expected = frm_num / 6 * 4;
	fsqt2_expected = frm_num / 6 * 3;
	fsqt3_expected = frm_num / 6 * 2;
	fsqt4_expected = frm_num / 6;
	fsqm_w32(idx, FSQT0, fsqt0_expected);
	fsqm_w32(idx, FSQT1, fsqt1_expected);
	fsqm_w32(idx, FSQT2, fsqt2_expected);
	fsqm_w32(idx, FSQT3, fsqt3_expected);
	fsqm_w32(idx, FSQT4, fsqt4_expected);

	/* (9) IO_BUF drain + FSQM_CTRL enable — the AVM init_fsqm_by_idx
	 *     triplet (cbm.c:1086-1091), byte-exact and LAST so the engine
	 *     starts on a consistent snapshot of all preceding regs with a
	 *     freshly cleared HW-master command window.
	 */
	fsqm_w32(idx, IO_BUF_RD, 0);
	fsqm_w32(idx, IO_BUF_WR, 0);
	fsqm_w32(idx, FSQM_CTRL, 1);

	/*
	 * Dynamic formula matches the write (NOT hardcoded values) — robust
	 * to any future frm_num bump.
	 */
	WARN_ON(fsqm_r32(idx, FSQT0) != fsqt0_expected);
	WARN_ON(fsqm_r32(idx, FSQT1) != fsqt1_expected);
	WARN_ON(fsqm_r32(idx, FSQT2) != fsqt2_expected);
	WARN_ON(fsqm_r32(idx, FSQT3) != fsqt3_expected);
	WARN_ON(fsqm_r32(idx, FSQT4) != fsqt4_expected);
	WARN_ON(fsqm_r32(idx, FSQM_IRNEN) != FSQM_IRNEN_DEFAULT);
	WARN_ON(fsqm_r32(idx, LSARNG) != lsarng);
	WARN_ON(fsqm_r32(idx, OFSQ)   != ofsq);
	WARN_ON(fsqm_r32(idx, OFSC)   != ofsc);
}

/*
 * init_fsqm - public entry point.
 *
 * @idx: 0 = std pool FSQM, 1 = jbo pool FSQM (per AVM
 *       std_fsqm_idx/jbo_fsqm_idx convention).
 *
 * AVM equivalence map:
 *   init_fsqm()              (AVM cbm.c:1181-1202) — the no-arg outer
 *                            init that bound to specific pools.
 *   init_fsqm_buf_std()      (AVM cbm.c:1095-1139) — the std-pool body.
 *   init_fsqm_buf_jumbo()    (AVM cbm.c:1141-1179) — the jbo-pool body.
 */
void init_fsqm(int idx)
{
	u32 frm_num;
	u32 maxlsa;
	u32 minlsa;
	u32 lsarng;
	u32 ofsq;
	u32 ofsc;
	u32 ofsc_readback;

	if (idx < 0 || idx > 1) {
		pr_err("cbm: init_fsqm: invalid idx %d (must be 0 or 1)\n",
		       idx);
		return;
	}
	if (!g_cbm_fsqm_base[idx]) {
		pr_err("cbm: init_fsqm: g_cbm_fsqm_base[%d] not ioremapped\n",
		       idx);
		return;
	}

	frm_num = (idx == 0) ? g_cbm_buff.std_frm_num
			     : g_cbm_buff.jbo_frm_num;

	if (frm_num < 2) {
		pr_err("cbm: init_fsqm: idx=%d frm_num=%u too small (need >=2)\n",
		       idx, frm_num);
		return;
	}

	/* (b) LLT_RAM seed first — the linked-list must exist before the
	 *     register sequence below brings the engine online.
	 */
	init_fsqm_buf_std(idx, frm_num);

	/* (c) Compute LSARNG / OFSQ / OFSC pack values.
	 *     Verbatim from AVM cqm/grx500/cbm.c:1086-1101 / 1121-1128.
	 *     minlsa = 0; maxlsa = frm_num - 2  (equivalent to AVM's
	 *     (size / frm_size) - 2 because frm_num = size / frm_size).
	 *     The "-2" is the AVM-source's "last item is invalid inside
	 *     the FSQM" rule (cbm.c:1119, cbm.c:1162).
	 *
	 *     LSARNG: HEAD(maxlsa) at bit 16, TAIL(minlsa) at bit 0
	 *     OFSQ:   TAIL(maxlsa) at bit 16, HEAD(minlsa) at bit 0
	 *     OFSC:   FSC = (maxlsa - minlsa + 1) at bit 0
	 *
	 *     For std@frm_num=2048: maxlsa=2046, OFSC=0x000007FF
	 *     For jbo@frm_num=256:  maxlsa=254,  OFSC=0x000000FF
	 */
	minlsa = 0;
	maxlsa = frm_num - 2;
	lsarng = ((maxlsa << LSARNG_MAXLSA_POS) & LSARNG_MAXLSA_MASK) |
		 ((minlsa << LSARNG_MINLSA_POS) & LSARNG_MINLSA_MASK);
	ofsq   = ((maxlsa << OFSQ_TAIL_POS) & OFSQ_TAIL_MASK) |
		 ((minlsa << OFSQ_HEAD_POS) & OFSQ_HEAD_MASK);
	ofsc   = ((maxlsa - minlsa + 1) << OFSC_FSC_POS) & OFSC_FSC_MASK;

	/* (d) Run the 10-step register sequence (IO_BUF, RCNT, LSARNG,
	 *     OFSQ, OFSC, IRNEN, FSQTn, CTRL).
	 */
	init_fsqm_by_idx(idx, frm_num, lsarng, ofsq, ofsc);

	ofsc_readback = fsqm_r32(idx, OFSC);
	WARN_ON(ofsc_readback != ofsc);

	pr_info("cbm: %sinit_fsqm idx=%d frm_num=%u OFSC=0x%08x (lsarng=0x%08x ofsq=0x%08x)\n",
		g_cbm_buff.placeholder ? "PLACEHOLDER " : "",
		idx, frm_num, ofsc_readback, lsarng, ofsq);
}
EXPORT_SYMBOL_GPL(init_fsqm);
