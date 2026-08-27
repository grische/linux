// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012, 2014, 2015 Lantiq Deutschland GmbH
 * Copyright (c) 2016, 2017 Intel Corporation
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/switch-api/gsw_tbl_rw.c and gsw_flow_core.c.
 *
 * GSWIP-3.0 packet classification engine: microcode load, sub-table access,
 * and the PMAC and port configuration the CPU datapath needs.
 */

#include <linux/bug.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "gsw_init.h"
#include "pce_microcode_table.h"


/*
 * PCE table register-offset arrays - verbatim from AVM gsw_flow_core.c:47-78.
 * The three counts (22 / 4 / 26) match the GSWIP-3.x PCE table-rw register
 * window in gsw_reg.h (PCE_TBL_KEY_0..21 / PCE_TBL_MASK_0..3 / PCE_TBL_VAL_0..25).
 */
static const u32 gsw_pce_tbl_reg_key[] = {
	PCE_TBL_KEY_0_KEY0_OFFSET, PCE_TBL_KEY_1_KEY1_OFFSET,
	PCE_TBL_KEY_2_KEY2_OFFSET, PCE_TBL_KEY_3_KEY3_OFFSET,
	PCE_TBL_KEY_4_KEY4_OFFSET, PCE_TBL_KEY_5_KEY5_OFFSET,
	PCE_TBL_KEY_6_KEY6_OFFSET, PCE_TBL_KEY_7_KEY7_OFFSET,
	PCE_TBL_KEY_8_KEY8_OFFSET, PCE_TBL_KEY_9_KEY9_OFFSET,
	PCE_TBL_KEY_10_KEY10_OFFSET, PCE_TBL_KEY_11_KEY11_OFFSET,
	PCE_TBL_KEY_12_KEY12_OFFSET, PCE_TBL_KEY_13_KEY13_OFFSET,
	PCE_TBL_KEY_14_KEY14_OFFSET, PCE_TBL_KEY_15_KEY15_OFFSET,
	PCE_TBL_KEY_16_KEY16_OFFSET, PCE_TBL_KEY_17_KEY17_OFFSET,
	PCE_TBL_KEY_18_KEY18_OFFSET, PCE_TBL_KEY_19_KEY19_OFFSET,
	PCE_TBL_KEY_20_KEY20_OFFSET, PCE_TBL_KEY_21_KEY21_OFFSET
};
static const u32 gsw_pce_tbl_reg_mask[] = {
	PCE_TBL_MASK_0_MASK0_OFFSET, PCE_TBL_MASK_1_MASK1_OFFSET,
	PCE_TBL_MASK_2_MASK2_OFFSET, PCE_TBL_MASK_3_MASK3_OFFSET
};
static const u32 gsw_pce_tbl_reg_value[] = {
	PCE_TBL_VAL_0_VAL0_OFFSET, PCE_TBL_VAL_1_VAL1_OFFSET,
	PCE_TBL_VAL_2_VAL2_OFFSET, PCE_TBL_VAL_3_VAL3_OFFSET,
	PCE_TBL_VAL_4_VAL4_OFFSET, PCE_TBL_VAL_5_VAL5_OFFSET,
	PCE_TBL_VAL_6_VAL6_OFFSET, PCE_TBL_VAL_7_VAL7_OFFSET,
	PCE_TBL_VAL_8_VAL8_OFFSET, PCE_TBL_VAL_9_VAL9_OFFSET,
	PCE_TBL_VAL_10_VAL10_OFFSET, PCE_TBL_VAL_11_VAL11_OFFSET,
	PCE_TBL_VAL_12_VAL12_OFFSET, PCE_TBL_VAL_13_VAL13_OFFSET,
	PCE_TBL_VAL_14_VAL14_OFFSET, PCE_TBL_VAL_15_VAL15_OFFSET,
	PCE_TBL_VAL_16_VAL16_OFFSET, PCE_TBL_VAL_17_VAL17_OFFSET,
	PCE_TBL_VAL_18_VAL18_OFFSET, PCE_TBL_VAL_19_VAL19_OFFSET,
	PCE_TBL_VAL_20_VAL20_OFFSET, PCE_TBL_VAL_21_VAL21_OFFSET,
	PCE_TBL_VAL_22_VAL22_OFFSET, PCE_TBL_VAL_23_VAL23_OFFSET,
	PCE_TBL_VAL_24_VAL24_OFFSET, PCE_TBL_VAL_25_VAL25_OFFSET
};

/*
 * GSWIP-3.0 PCE table-shape descriptor - verbatim from AVM gsw_flow_core.c:110-142.
 * Indexed by ptdata->table (the GSWIP PCE ADDR field, ptbl_cmds_t enum at
 * gsw_flow_core.h:453-486). Each triplet {num_key, num_mask, num_val} tells the
 * four helpers below how many KEY / MASK / VAL registers to push or pull for
 * the selected PCE sub-table.
 *
 * 31 entries (indices 0..30) cover the entire GSWIP-3.0 PCE ADDR range. Index
 * 31 (PCE_PMAP_INDEX = 0x1F) and above are GSWIP-3.1-only and are guarded by
 * each helper's `ptdata->table > 30` check.
 */
static const gsw_pce_tbl_info_t gsw_pce_tbl_info_30[] = {
	{ 0, 0, 4 },
	{ 1, 1, 0 },
	{ 0, 0, 3 },
	{ 1, 0, 0 },
	{ 1, 1, 0 },
	{ 1, 1, 0 },
	{ 4, 4, 0 },
	{ 4, 4, 0 },
	{ 1, 1, 0 },
	{ 0, 0, 1 },
	{ 0, 0, 1 },
	{ 4, 0, 2 },
	{ 0, 0, 0 },
	{ 3, 0, 2 },
	{ 2, 0, 5 },
	{ 16, 0, 10 },
	{ 0, 0, 0 },
	{ 0, 0, 1 },
	{ 1, 1, 1 },
	{ 1, 1, 1 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 3, 1, 0 },
	{ 3, 1, 0 },
	{ 1, 1, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 1 },
	{ 0, 0, 1 },
	{ 0, 0, 2 },
	{ 1, 1, 0 },
	{ 0, 0, 2 }
};

/**
 * gsw_pce_attach_tbl_30 - wire the GSWIP-3.0 PCE table-shape +
 * register-offset arrays into a per-instance ethsw_api_dev_t.
 */
void gsw_pce_attach_tbl_30(ethsw_api_dev_t *pethdev)
{
	pethdev->pce_tbl_reg.key = gsw_pce_tbl_reg_key;
	pethdev->pce_tbl_reg.mask = gsw_pce_tbl_reg_mask;
	pethdev->pce_tbl_reg.value = gsw_pce_tbl_reg_value;
	pethdev->pce_tbl_info = gsw_pce_tbl_info_30;
	pethdev->num_of_pce_tbl = ARRAY_SIZE(gsw_pce_tbl_info_30);
}

/**
 * gsw_pce_table_write - program one PCE sub-table row by address.
 * @cdev: ethsw_api_dev_t * cast to void * (see GSW_PDATA_GET).
 * @ptdata: PCE table programming descriptor; ptdata->table selects the
 *          sub-table, ptdata->pcindex the row, ptdata->key/mask/val carry
 *          the payload to write. Must be non-NULL.
 *
 * Returns -EINVAL if ptdata->table > 30 (GSWIP-3.1-only ADDR), GSW_statusErr
 * if the BAS busy-bit fails to clear within MAX_BUSY_RETRY iterations on
 * either the pre-write or post-write poll, GSW_statusOk on success.
 *
 * Ported from AVM gsw_tbl_rw.c:465-535. Takes gswdev->lock_pce across the
 * indirect-table transaction itself, so callers do not have to serialise
 * (matches AVM 08.25, which calls the same lock lock_pce_tbl). The lock is
 * NOT held across the entry busy-poll - see the note at the CHECK_BUSY below.
 * Callers must therefore not hold lock_pce when they call in.
 */
int gsw_pce_table_write(void *cdev, pctbl_prog_t *ptdata)
{
	u32 ctrlval;
	u16 i, j;
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;

	if (gswdev == NULL)
		return GSW_statusErr;

	if (ptdata->table > 30) {
		WARN_ONCE(1,
			  "gsw_pce_table_write: GSWIP-3.0 PCE ADDR must be <=30, got %u (PCE_PMAP_INDEX=0x1F and above are GSWIP-3.1-only)\n",
			  ptdata->table);
		return -EINVAL;
	}

	/*
	 * Entry poll stays outside the lock, as it does in AVM 08.25: waiting
	 * for a transaction someone else started is not something to do with
	 * the lock held, or a slow transaction turns into a livelock.
	 */
	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);

	spin_lock(&gswdev->lock_pce);

	/*
	 * Compose the control word from zero, so every field this sequence
	 * does not set is written as zero rather than inherited from whatever
	 * the previous PCE transaction left in the register.
	 *
	 * AVM's 07.30 drop has no explicit seed: its do/while busy-poll leaves
	 * ctrlval holding the register value it last read, and the
	 * gsw_field_w32 calls below merge onto that. CHECK_BUSY keeps its
	 * read-back in a macro-local we cannot see, so this used to be an
	 * explicit re-seeding read, on the reading that composing onto chip
	 * state was deliberate - notably for the EXTOP bit, which AVM never
	 * writes explicitly. The 08.25 drop refutes that reading: it inserts
	 * ctrlval = 0 before the composition in all four accessors
	 * (gsw_tbl_rw.c:512, :589, :674, :735). Follow it.
	 */
	ctrlval = 0;

	gsw_w32_raw(cdev, PCE_TBL_ADDR_ADDR_OFFSET, ptdata->pcindex);
	/*TABLE ADDRESS*/
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_ADDR_SHIFT,
				PCE_TBL_CTRL_ADDR_SIZE, ptdata->table);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_OPMOD_SHIFT,
				PCE_TBL_CTRL_OPMOD_SIZE, PCE_OP_MODE_ADWR);
	/*KEY REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_key;

	if (ptdata->kformat)
		j *= 4;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.key[i], ptdata->key[i]);
	}

	/*MASK REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_mask;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.mask[i], ptdata->mask[i]);
	}

	/*VAL REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_val;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.value[i], ptdata->val[i]);
	}

	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_KEYFORM_SHIFT,
				PCE_TBL_CTRL_KEYFORM_SIZE, ptdata->kformat);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_TYPE_SHIFT,
				PCE_TBL_CTRL_TYPE_SIZE, ptdata->type);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_VLD_SHIFT,
				PCE_TBL_CTRL_VLD_SIZE, ptdata->valid);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_GMAP_SHIFT,
				PCE_TBL_CTRL_GMAP_SIZE, ptdata->group);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_BAS_SHIFT,
				PCE_TBL_CTRL_BAS_SIZE, 1);
	gsw_w32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, ctrlval);

	if (CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		       PCE_TBL_CTRL_BAS_SIZE, RETURN_ERROR_CODE)) {
		spin_unlock(&gswdev->lock_pce);
		return GSW_statusErr;
	}

	gsw_w32_raw(cdev, PCE_TBL_CTRL_ADDR_OFFSET, 0);

	spin_unlock(&gswdev->lock_pce);

	return GSW_statusOk;
}

/**
 * gsw_pce_table_read - read one PCE sub-table row by address.
 *
 * @cdev: ethsw_api_dev_t * cast to void *.
 *
 * @ptdata: PCE table programming descriptor; ptdata->table + ptdata->pcindex
 *          select the row to read; ptdata->key/mask/val + ptdata->type +
 *          ptdata->valid + ptdata->group receive the read-back data.
 */
int gsw_pce_table_read(void *cdev, pctbl_prog_t *ptdata)
{
	u32 ctrlval, value;
	u16 i, j;
	/* See gsw_pce_table_write for the cdev-accessor rationale. */
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;

	if (gswdev == NULL)
		return GSW_statusErr;

	if (ptdata->table > 30) {
		WARN_ONCE(1,
			  "gsw_pce_table_read: GSWIP-3.0 PCE ADDR must be <=30, got %u (PCE_PMAP_INDEX=0x1F and above are GSWIP-3.1-only)\n",
			  ptdata->table);
		return -EINVAL;
	}

	/* Entry poll outside the lock - see gsw_pce_table_write. */
	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);

	spin_lock(&gswdev->lock_pce);

	/* See gsw_pce_table_write for why the control word starts at zero. */
	ctrlval = 0;

	gsw_w32_raw(cdev, PCE_TBL_ADDR_ADDR_OFFSET, ptdata->pcindex);
	/*TABLE ADDRESS*/
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_ADDR_SHIFT,
				PCE_TBL_CTRL_ADDR_SIZE, ptdata->table);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_OPMOD_SHIFT,
				PCE_TBL_CTRL_OPMOD_SIZE, PCE_OP_MODE_ADRD);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_KEYFORM_SHIFT,
				PCE_TBL_CTRL_KEYFORM_SIZE, ptdata->kformat);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_BAS_SHIFT,
				PCE_TBL_CTRL_BAS_SIZE, 1);
	gsw_w32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, ctrlval);

	/* Tail poll inside the lock - see gsw_pce_table_write. */
	if (CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		       PCE_TBL_CTRL_BAS_SIZE, RETURN_ERROR_CODE)) {
		spin_unlock(&gswdev->lock_pce);
		return GSW_statusErr;
	}
	/*
	 * Re-seed ctrlval after the post-read poll: the AVM do/while left
	 * ctrlval holding the read-back PCE_TBL_CTRL register state, which
	 * the gsw_field_r32 extractions for ptdata->type / valid / group
	 * (below) depend on.
	 */
	gsw_r32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, &ctrlval);

	/*KEY REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_key;

	if (ptdata->kformat)
		j *= 4;

	for (i = 0; i < j; i++) {
		gsw_r32_raw(cdev, gswdev->pce_tbl_reg.key[i], &value);
		ptdata->key[i] = value;
	}

	/*MASK REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_mask;

	for (i = 0; i < j; i++) {
		gsw_r32_raw(cdev, gswdev->pce_tbl_reg.mask[i], &value);
		ptdata->mask[i] = value;
	}

	/*VAL REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_val;

	for (i = 0; i < j; i++) {
		gsw_r32_raw(cdev, gswdev->pce_tbl_reg.value[i], &value);
		ptdata->val[i] = value;
	}

	ptdata->type = gsw_field_r32(ctrlval, PCE_TBL_CTRL_TYPE_SHIFT,
				     PCE_TBL_CTRL_TYPE_SIZE);
	ptdata->valid = gsw_field_r32(ctrlval, PCE_TBL_CTRL_VLD_SHIFT,
				      PCE_TBL_CTRL_VLD_SIZE);
	ptdata->group = gsw_field_r32(ctrlval, PCE_TBL_CTRL_GMAP_SHIFT,
				      PCE_TBL_CTRL_GMAP_SIZE);
	gsw_w32_raw(cdev, PCE_TBL_CTRL_ADDR_OFFSET, 0);

	spin_unlock(&gswdev->lock_pce);

	return GSW_statusOk;
}

/**
 * gsw_pce_table_key_read - look up a PCE sub-table row by key, then read.
 *
 * @cdev: ethsw_api_dev_t * cast to void *.
 *
 * @ptdata: PCE table programming descriptor; ptdata->table + ptdata->key[]
 *          identify the row; ptdata->val/type/valid/group + ptdata->pcindex
 *          receive the read-back data.
 */
int gsw_pce_table_key_read(void *cdev, pctbl_prog_t *ptdata)
{
	u32 ctrlval, value;
	u16 i, j;
	/* See gsw_pce_table_write for the cdev-accessor rationale. */
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;

	if (gswdev == NULL)
		return GSW_statusErr;

	if (ptdata->table > 30) {
		WARN_ONCE(1,
			  "gsw_pce_table_key_read: GSWIP-3.0 PCE ADDR must be <=30, got %u (PCE_PMAP_INDEX=0x1F and above are GSWIP-3.1-only)\n",
			  ptdata->table);
		return -EINVAL;
	}

	/* Entry poll outside the lock - see gsw_pce_table_write. */
	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);

	spin_lock(&gswdev->lock_pce);

	/*KEY REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_key;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.key[i], ptdata->key[i]);
	}

	/* See gsw_pce_table_write for why the control word starts at zero. */
	ctrlval = 0;

	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_ADDR_SHIFT,
				PCE_TBL_CTRL_ADDR_SIZE, ptdata->table);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_OPMOD_SHIFT,
				PCE_TBL_CTRL_OPMOD_SIZE, PCE_OP_MODE_KSRD);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_KEYFORM_SHIFT,
				PCE_TBL_CTRL_KEYFORM_SIZE, 0);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_BAS_SHIFT,
				PCE_TBL_CTRL_BAS_SIZE, 1);
	gsw_w32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, ctrlval);

	/* Tail poll inside the lock - see gsw_pce_table_write. */
	if (CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		       PCE_TBL_CTRL_BAS_SIZE, RETURN_ERROR_CODE)) {
		spin_unlock(&gswdev->lock_pce);
		return GSW_statusErr;
	}
	/*
	 * Re-seed ctrlval after the post-read poll: the gsw_field_r32
	 * extractions for ptdata->type / valid / group below depend on the
	 * read-back PCE_TBL_CTRL register state.
	 */
	gsw_r32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, &ctrlval);

	/*VAL REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_val;

	for (i = 0; i < j; i++) {
		gsw_r32_raw(cdev, gswdev->pce_tbl_reg.value[i], &value);
		ptdata->val[i] = value;
	}

	ptdata->type = gsw_field_r32(ctrlval, PCE_TBL_CTRL_TYPE_SHIFT,
				     PCE_TBL_CTRL_TYPE_SIZE);
	ptdata->valid = gsw_field_r32(ctrlval, PCE_TBL_CTRL_VLD_SHIFT,
				      PCE_TBL_CTRL_VLD_SIZE);
	ptdata->group = gsw_field_r32(ctrlval, PCE_TBL_CTRL_GMAP_SHIFT,
				      PCE_TBL_CTRL_GMAP_SIZE);
	gsw_r32_raw(cdev, PCE_TBL_ADDR_ADDR_OFFSET, &value);
	ptdata->pcindex = value;
	gsw_w32_raw(cdev, PCE_TBL_CTRL_ADDR_OFFSET, 0);

	spin_unlock(&gswdev->lock_pce);

	return GSW_statusOk;
}

/**
 * gsw_pce_table_key_write - program a PCE sub-table row keyed by
 * ptdata->key[].
 *
 * @cdev: ethsw_api_dev_t * cast to void *.
 *
 * @ptdata: PCE table programming descriptor; key/mask/val all push to HW.
 */
int gsw_pce_table_key_write(void *cdev, pctbl_prog_t *ptdata)
{
	u32 ctrlval;
	u16 i, j;
	/* See gsw_pce_table_write for the cdev-accessor rationale. */
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;

	if (gswdev == NULL)
		return GSW_statusErr;

	if (ptdata->table > 30) {
		WARN_ONCE(1,
			  "gsw_pce_table_key_write: GSWIP-3.0 PCE ADDR must be <=30, got %u (PCE_PMAP_INDEX=0x1F and above are GSWIP-3.1-only)\n",
			  ptdata->table);
		return -EINVAL;
	}

	/* Entry poll outside the lock - see gsw_pce_table_write. */
	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);

	spin_lock(&gswdev->lock_pce);

	/* See gsw_pce_table_write for why the control word starts at zero. */
	ctrlval = 0;

	/*TABLE ADDRESS*/
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_ADDR_SHIFT,
				PCE_TBL_CTRL_ADDR_SIZE, ptdata->table);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_OPMOD_SHIFT,
				PCE_TBL_CTRL_OPMOD_SIZE, PCE_OP_MODE_KSWR);

	/*KEY REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_key;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.key[i], ptdata->key[i]);
	}

	/*MASK REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_mask;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.mask[i], ptdata->mask[i]);
	}

	/*VAL REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_val;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.value[i], ptdata->val[i]);
	}

	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_KEYFORM_SHIFT,
				PCE_TBL_CTRL_KEYFORM_SIZE, 0);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_TYPE_SHIFT,
				PCE_TBL_CTRL_TYPE_SIZE, ptdata->type);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_VLD_SHIFT,
				PCE_TBL_CTRL_VLD_SIZE, ptdata->valid);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_GMAP_SHIFT,
				PCE_TBL_CTRL_GMAP_SIZE, ptdata->group);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_BAS_SHIFT,
				PCE_TBL_CTRL_BAS_SIZE, 1);
	gsw_w32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, ctrlval);

	/* Tail poll inside the lock - see gsw_pce_table_write. */
	if (CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		       PCE_TBL_CTRL_BAS_SIZE, RETURN_ERROR_CODE)) {
		spin_unlock(&gswdev->lock_pce);
		return GSW_statusErr;
	}

	gsw_w32_raw(cdev, PCE_TBL_CTRL_ADDR_OFFSET, 0);

	spin_unlock(&gswdev->lock_pce);

	return GSW_statusOk;
}

/**
 * gsw_pmicro_code_init - load the GSWIP-3.0 PCE parser microcode at probe time.
 *
 * @cdev: ethsw_api_dev_t * cast to void * (see GSW_PDATA_GET).
 *
 * Returns GSW_statusOk on success, GSW_statusErr on NULL gswdev or row-write
 * failure.
 *
 * Ported from AVM 4.9 gsw_flow_pce.c:1488-1616 (GSWIP-3.0 branch only;
 * the GSWIP-2.2 and GSWIP-3.1 branches at AVM lines 1538-1581 are NOT ported
 * per the architectural constraint that this driver is GSWIP-3.0 only).
 *
 * ordering (silicon arbiter contract): (1) NULL-check; (2) compute no_ports =
 * gswdev->pnum; (3) FDMA/SDMA disable loop with GSW-R hole-skip; (4)
 * GSWT_GCTRL_SE = 1 (FDMA arbiter activation gate, MUST precede MC_VALID
 * writes - reversing (4) and (5) re-introduces the v5.10
 * silicon-FDMA-arbiter-not-armed bug retracted in); (5) PCE_GCTRL_0.MC_VALID
 * = 0; (6) 256-row microcode load via gsw_pce_table_write (PCE_PARS_INDEX);
 * (7) PCE_GCTRL_0.MC_VALID = 1; (8) RMON enable pair-of-loops (BM_PCFG_CNTEN
 * + BM_RMON_CTRL_BCAST_CNT) with GSW-R hole-skip, gated on gipver ==
 * LTQ_GSWIP_3_0; (9) BM_QUEUE_GCTRL.GL_MOD = 0 (using the 2-bit _SIZE2
 * width).
 *
 * AVM's commented-out per-row pr_err at line 1536 is dead code and is not
 * ported. AVM's silent ignore of gsw_pce_table_write's return value (AVM line
 * 1535) is strengthened here: a non-zero return aborts the load loop with
 * pr_err + GSW_statusErr so the upstream probe path can unwind cleanly rather
 * than leaving MC_VALID asserted over a partial microcode.
 *
 * The PMAC config subsystem was entirely unported; without it the PMAC drops
 * the DMA2TX-delivered frame instead of forwarding it to GPHY5 (iteration-8
 * finding: the CBM path delivers the frame to DMA2TX ch5, but it never
 * egresses). 3.0-only (no Freeze, no 3.1 branches), PMAC0 only (pmac_addr_off
 * is identity for pmacId=0). Register word-offsets/shifts are verbatim from
 * gsw_reg.h.
 */
#define PMAC_REG_CTRL_0   0xD03
#define PMAC_REG_CTRL_2   0xD05
#define PMAC_REG_CTRL_3   0xD06
#define PMAC_REG_CTRL_4   0xD07
#define PMAC_REG_TBL_VAL4 0xD40
#define PMAC_REG_TBL_VAL3 0xD41
#define PMAC_REG_TBL_VAL2 0xD42
#define PMAC_REG_TBL_VAL1 0xD43
#define PMAC_REG_TBL_VAL0 0xD44
#define PMAC_REG_TBL_ADDR 0xD45
#define PMAC_REG_TBL_CTRL 0xD46  /* [2:0]=ptcaddr [5]=opmod [15]=BAS */
#define PMAC_TBL_IGCFG_IDX 1
#define PMAC_TBL_EGCFG_IDX 2

/* xwayflow_pmac_table_write (3.0): poll BAS, write ADDR/CTRL/OPMOD/VAL4..0,
 * set BAS, poll BAS clear. */
static void gsw_pmac_tbl_write(void *cdev, u16 ptaddr, u8 ptcaddr,
			       const u16 val[5])
{
	u32 bas;
	int i;

	for (i = 0; i < 1000; i++) {
		gsw_r32_raw(cdev, PMAC_REG_TBL_CTRL, &bas);
		if (!(bas & (1u << 15)))
			break;
	}
	gsw_w32(cdev, PMAC_REG_TBL_ADDR, 0, 12, ptaddr);
	gsw_w32(cdev, PMAC_REG_TBL_CTRL, 0, 3, ptcaddr);
	gsw_w32(cdev, PMAC_REG_TBL_CTRL, 5, 1, 1);   /* OPMOD = write */
	gsw_w32(cdev, PMAC_REG_TBL_VAL4, 0, 16, val[4]);
	gsw_w32(cdev, PMAC_REG_TBL_VAL3, 0, 16, val[3]);
	gsw_w32(cdev, PMAC_REG_TBL_VAL2, 0, 16, val[2]);
	gsw_w32(cdev, PMAC_REG_TBL_VAL1, 0, 16, val[1]);
	gsw_w32(cdev, PMAC_REG_TBL_VAL0, 0, 16, val[0]);
	gsw_w32(cdev, PMAC_REG_TBL_CTRL, 15, 1, 1);  /* BAS = trigger */
	for (i = 0; i < 1000; i++) {
		gsw_r32_raw(cdev, PMAC_REG_TBL_CTRL, &bas);
		if (!(bas & (1u << 15)))
			break;
	}
}

/*
 * The vendor's PMAC global-configuration helper is only reached from a
 * GSWIP-3.1 branch, so on GSWIP-3.0 PMAC_CTRL_0/2/3/4 stay at their silicon
 * reset values. Only the two fields the CPU datapath depends on are written
 * here: FLAGEN = 0, which GSW_PMAC_EG_CfgSet requires, and MLEN = 0, which
 * the vendor's own legacy core init writes for GSW-L.
 */
static void gsw_pmac_glbl_cfg(void *cdev)
{
	gsw_w32(cdev, PMAC_REG_CTRL_4,  0, 1, 0);
	gsw_w32(cdev, PMAC_REG_CTRL_2,  3, 1, 0);  /* MLEN=0 */
}

/* defPmacHdr packs into val[0..3] as val[n] = (hdr[2n]<<8)|hdr[2n+1]. */
static void gsw_pmac_ig_cfg(void *cdev)
{
	int i;

	for (i = 0; i <= 15; i++) {
		u16 val[5] = { 0, 0, 0, 0, 0 };

		/* val[1] = defPmacHdr[2]<<8 | defPmacHdr[3] */
		val[1] = (u16)((((((i & 8) >> 3) * 2) + 1) << 8) | 0x90);
		/* val[3] = defPmacHdr[6]<<8 | defPmacHdr[7]; [6]=0, [7]=1<<(i&7) */
		val[3] = (u16)(1u << (i & 0x7));
		/* all "default/enable" flags set */
		val[4] = 0x00FF;

		gsw_pmac_tbl_write(cdev, (u16)(i & 0x0F), PMAC_TBL_IGCFG_IDX, val);
	}
}

static void gsw_pmac_eg_cfg(void *cdev)
{
	int tc, j;

	for (tc = 0; tc <= 15; tc++) {
		for (j = 0; j <= 3; j++) {
			u16 val[5] = { 0, 0, 0, 0, 0 };
			u16 ptaddr = (u16)(((tc & 0x0F) << 4) |
					   ((j & 0x3) << 8)); /* dest = 0 */

			val[1] = 0;                         /* nRxDmaChanId=0 (SINGLE_RX_CH0_ONLY) */
			val[2] = 1u << 0;                   /* bPmacEna=1 */
			gsw_pmac_tbl_write(cdev, ptaddr,
					   PMAC_TBL_EGCFG_IDX, val);
		}
	}
}

static void gsw_pmac_ig_cfg_r(void *cdev)
{
	int i;

	for (i = 0; i <= 15; i++) {
		u16 val[5] = { 0, 0, 0, 0, 0 };
		int pmap_def = ((i < 4) || (i == 15)) ? 1 : 0;

		if (i == 15) {
			val[1] = 0x0080;  /* defPmacHdr[2]=0, [3]=0x80 */
			val[3] = 0x8000;  /* defPmacHdr[6]=0x80, [7]=0x00 */
		} else {
			val[1] = (u16)((i & 0x0F) << 12); /* hdr[2]=i<<4, hdr[3]=0 */
			val[3] = 0xFFFF;                  /* hdr[6]=hdr[7]=0xFF */
		}
		val[4] = (u16)(0x01                    /* b0 bPmacPresent */
			    | ((i == 15) ? 0x02 : 0)   /* b1 bSpIdDefault */
			    | (pmap_def ? 0x10 : 0)    /* b4 bPmapEna */
			    | (pmap_def ? 0x40 : 0)    /* b6 bPmapDefault */
			    | 0x80);                   /* b7 bErrPktsDisc */

		gsw_pmac_tbl_write(cdev, (u16)(i & 0x0F), PMAC_TBL_IGCFG_IDX, val);
	}
}

/*
 * AVM GSW-R PMAC egress (ltq_eth_drv_xrx500.c:1009-1066): destPort k=0..15,
 * traffic class i=0..15, flowMsb j=0..3; TC-addressed (FLAGEN=0).
 *   ptaddr = (k & 0xF) | ((i & 0xF) << 4) | ((j & 0x3) << 8)
 *   nRxDmaChanId = (k <= 13) ? 0 : 5  (GRX350-B / soc_rev==2 path; differs from
 *     the else-branch only for k=14/15, outside the CPU<->GPHY5 datapath)
 *   bPmacEna=1 always; bFcsEna=(k==13); bRemL2Hdr + numBytesRem=1 for k==14.
 * val[2]: b0 bPmacEna, b1 bFcsEna, b2 bRemL2Hdr, [15:8] numBytesRem
 * (gsw_flow_core.c:16311-16327).
 */
static void gsw_pmac_eg_cfg_r(void *cdev)
{
	int k, i, j;

	for (k = 0; k <= 15; k++) {
		for (i = 0; i <= 15; i++) {
			for (j = 0; j <= 3; j++) {
				u16 val[5] = { 0, 0, 0, 0, 0 };
				u16 ptaddr = (u16)((k & 0x0F)
						   | ((i & 0x0F) << 4)
						   | ((j & 0x03) << 8));

				val[1] = (u16)((k <= 13) ? 0 : 5); /* nRxDmaChanId */
				val[2] |= (1u << 0);               /* bPmacEna=1 */
				if (k == 13)
					val[2] |= (1u << 1);       /* bFcsEna */
				if (k == 14) {
					val[2] |= (1u << 2);       /* bRemL2Hdr */
					val[2] |= (1u << 8);       /* numBytesRem=1 */
				}
				gsw_pmac_tbl_write(cdev, ptaddr,
						   PMAC_TBL_EGCFG_IDX, val);
			}
		}
	}
}

static void gsw_cpu_special_tag_ingress(void *cdev)
{
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;
	u8 pidx;

	if (gswdev == NULL)
		return;

	if (gswdev->sdev == LTQ_FLOW_DEV_INT) {
		/*
		 * GSW-L: AVM enables Special-Tag-Ingress on the single CPU/PMAC
		 * port (ltq_eth_drv_xrx500.c:958 nPortId=0 ->
		 * GSW_3X_SOC_CPU_PORT). pidx=0 -> offset 0x480 + 0xa*0 = 0x480.
		 */
		pidx = (u8)GSW_3X_SOC_CPU_PORT;
		gsw_w32(cdev, (PCE_PCTRL_0_IGSTEN_OFFSET + (0xa * pidx)),
			PCE_PCTRL_0_IGSTEN_SHIFT,
			PCE_PCTRL_0_IGSTEN_SIZE, 1);
		{
			u8 p;

			/* SPFDIS (bit14) on the CPU port -- faithful to AVM. */
			gsw_w32(cdev, (PCE_PCTRL_0_SPFDIS_OFFSET + (0xa * pidx)),
				PCE_PCTRL_0_SPFDIS_SHIFT,
				PCE_PCTRL_0_SPFDIS_SIZE, 1);

			/*
			 * PSTATE=FORWARDING on GSW-L ports 0..6 (live CPU-TX path
			 * is p0 ingress -> p5 egress; golden RMON shows p0..p6
			 * carrying traffic). Belt-and-suspenders vs the reset
			 * default.
			 */
			for (p = 0; p <= 6; p++)
				gsw_w32(cdev,
					(PCE_PCTRL_0_PSTATE_OFFSET + (0xa * p)),
					PCE_PCTRL_0_PSTATE_SHIFT,
					PCE_PCTRL_0_PSTATE_SIZE,
					PORT_STATE_FORWARDING);

		}

		/*
		 * Clear the PMAC short-frame length check: with it set the
		 * PMAC drops the CPU-TX frame before the switch ever sees it.
		 * The vendor clears it on both instances.
		 */
		{
			gsw_w32(cdev, PMAC_REG_CTRL_2, 0, 2, 0); /* LCHKS = SHORT_LEN_DIS */
		}

		/*
		 * These are shared FDMA/SDMA registers whose reset value is
		 * 0. A warm start can leave them non-zero, and a stale parser
		 * select or special-tag type on the CPU-port ingress path
		 * silently mis-parses the tagged CPU-TX frame, so they are
		 * written explicitly.
		 */
		{
			u8 cp = (u8)GSW_3X_SOC_CPU_PORT;   /* GSW-L CPU port = 0 */


			/* FDMA CPU/MPE parser-select = NIL(0) -- single reg 0xa47 */
			gsw_w32(cdev, FDMA_PASR_CPU_OFFSET, FDMA_PASR_CPU_SHIFT,
				FDMA_PASR_CPU_SIZE, 0);
			gsw_w32(cdev, FDMA_PASR_MPE1_OFFSET, FDMA_PASR_MPE1_SHIFT,
				FDMA_PASR_MPE1_SIZE, 0);
			gsw_w32(cdev, FDMA_PASR_MPE2_OFFSET, FDMA_PASR_MPE2_SHIFT,
				FDMA_PASR_MPE2_SIZE, 0);
			gsw_w32(cdev, FDMA_PASR_MPE3_OFFSET, FDMA_PASR_MPE3_SHIFT,
				FDMA_PASR_MPE3_SIZE, 0);
			/* FDMA per-port special-tag egress + type = 0 */
			gsw_w32(cdev, (FDMA_PCTRL_STEN_OFFSET + (0x6 * cp)),
				FDMA_PCTRL_STEN_SHIFT, FDMA_PCTRL_STEN_SIZE, 0);
			gsw_w32(cdev, (FDMA_PCTRL_ST_TYPE_OFFSET + (0x6 * cp)),
				FDMA_PCTRL_ST_TYPE_SHIFT, FDMA_PCTRL_ST_TYPE_SIZE, 0);
			/* SDMA per-port FCS-ignore = bFcsCheck = 0 */
			gsw_w32(cdev, (SDMA_PCTRL_FCSIGN_OFFSET + (0x6 * cp)),
				SDMA_PCTRL_FCSIGN_SHIFT, SDMA_PCTRL_FCSIGN_SIZE, 0);

		}

		/*
		 * Flood maps: 0x1 floods to the CPU port only. It is the
		 * silicon reset default, which the vendor leaves untouched on
		 * GSW-L and writes explicitly on GSW-R
		 * (ltq_eth_drv_xrx500.c:1131-1133).
		 */
		{

			gsw_w32(cdev, PCE_PMAP_3_UUCMAP_OFFSET,
				PCE_PMAP_3_UUCMAP_SHIFT,
				PCE_PMAP_3_UUCMAP_SIZE, 0x1);
			gsw_w32(cdev, PCE_PMAP_2_DMCPMAP_OFFSET,
				PCE_PMAP_2_DMCPMAP_SHIFT,
				PCE_PMAP_2_DMCPMAP_SIZE, 0x1);

		}
	} else if (gswdev->sdev == LTQ_FLOW_DEV_INT_R) {
		/*
		 * GSW-R: AVM enables Special-Tag-Ingress on ports 0..14
		 * (ltq_eth_drv_xrx500.c:1098 "for (k = 0; k < 15; k++)").
		 */
		for (pidx = 0; pidx < 15; pidx++) {
			gsw_w32(cdev, (PCE_PCTRL_0_IGSTEN_OFFSET + (0xa * pidx)),
				PCE_PCTRL_0_IGSTEN_SHIFT,
				PCE_PCTRL_0_IGSTEN_SIZE, 1);
		}

		{
			gsw_w32(cdev, PMAC_REG_CTRL_2, 0, 2, 0); /* LCHKS = SHORT_LEN_DIS */
		}

		gsw_w32(cdev, PCE_PMAP_2_DMCPMAP_OFFSET,
			PCE_PMAP_2_DMCPMAP_SHIFT, PCE_PMAP_2_DMCPMAP_SIZE, 0x1);
		gsw_w32(cdev, PCE_PMAP_3_UUCMAP_OFFSET,
			PCE_PMAP_3_UUCMAP_SHIFT, PCE_PMAP_3_UUCMAP_SIZE, 0x1);
	}
}

int gsw_pmicro_code_init(void *cdev)
{
	/* See gsw_pce_table_write for the cdev-accessor rationale. */
	ethsw_api_dev_t *gswdev = (ethsw_api_dev_t *)cdev;
	pctbl_prog_t tbl_entry;
	u16 i, j;
	u8 Gl_Mod_Size = 0;
	u32 no_ports = 0;
	int ret;

	if (gswdev == NULL)
		return GSW_statusErr;

	/*
	 * GSWIP-3.0-only port: the AVM IS_VRSN_31 branch that would otherwise
	 * pick gswdev->tpnum is dropped (architectural constraint, GSWIP-3.0).
	 */
	no_ports = gswdev->pnum;

	/*
	 * On GSW-R (sdev=LTQ_FLOW_DEV_INT_R) the AVM hole-skip pattern jumps
	 * from j=1 to j=15 -- outside the pnum=8 loop bound -- so GSW-R
	 * disables FDMA/SDMA only on port 0 here.
	 *
	 * Disable all physical port
	 */
	for (j = 0; j < no_ports; j++) {
		if (gswdev->sdev == LTQ_FLOW_DEV_INT_R && j == 1)
			j = 15;
		gsw_w32(cdev, (FDMA_PCTRL_EN_OFFSET + (j * 0x6)),
			FDMA_PCTRL_EN_SHIFT, FDMA_PCTRL_EN_SIZE, 0);
		gsw_w32(cdev, (SDMA_PCTRL_PEN_OFFSET + (j * 0x6)),
			SDMA_PCTRL_PEN_SHIFT, SDMA_PCTRL_PEN_SIZE, 0);
	}

	/*
	 * FDMA arbiter activation gate. MUST come BEFORE the MC_VALID=0 write
	 * below; reversing the two re-introduces the v5.10 silicon bug class
	 * where the FDMA arbiter is not armed when the parser is taken out of
	 * reset.
	 */
	gsw_w32(cdev, (GSWT_GCTRL_SE_OFFSET + GSW30_TOP_OFFSET),
		GSWT_GCTRL_SE_SHIFT, GSWT_GCTRL_SE_SIZE, 1);

	/* Micro code set invalid */
	gsw_w32(cdev, PCE_GCTRL_0_MC_VALID_OFFSET,
		PCE_GCTRL_0_MC_VALID_SHIFT, PCE_GCTRL_0_MC_VALID_SIZE, 0);

	/* Download the microcode */
	for (i = 0; i < 256 /* PCE_MICRO_TABLE_SIZE */; i++) {
		memset(&tbl_entry, 0, sizeof(pctbl_prog_t));
		tbl_entry.val[3] = pce_microcode_table[i].val_3;
		tbl_entry.val[2] = pce_microcode_table[i].val_2;
		tbl_entry.val[1] = pce_microcode_table[i].val_1;
		tbl_entry.val[0] = pce_microcode_table[i].val_0;
		tbl_entry.pcindex = i;
		tbl_entry.table = PCE_PARS_INDEX;
		ret = gsw_pce_table_write(cdev, &tbl_entry);
		if (ret) {
			pr_err("gsw_pmicro_code_init: row %u write failed: %d\n",
			       i, ret);
			return GSW_statusErr;
		}
	}

	/* Micro code set valid */
	gsw_w32(cdev, PCE_GCTRL_0_MC_VALID_OFFSET,
		PCE_GCTRL_0_MC_VALID_SHIFT, PCE_GCTRL_0_MC_VALID_SIZE, 1);

	/* Enable RMON Counter for all ports */
	if (gswdev->gipver == LTQ_GSWIP_3_0) {
		for (j = 0; j < gswdev->pnum; j++) {
			if (gswdev->sdev == LTQ_FLOW_DEV_INT_R && j == 1)
				j = 15;
			gsw_w32(cdev, (BM_PCFG_CNTEN_OFFSET + (j * 0x2)),
				BM_PCFG_CNTEN_SHIFT, BM_PCFG_CNTEN_SIZE, 1);
		}

		for (j = 0; j < gswdev->pnum; j++) {
			if (gswdev->sdev == LTQ_FLOW_DEV_INT_R && j == 1)
				j = 15;
			gsw_w32(cdev, (BM_RMON_CTRL_BCAST_CNT_OFFSET + (j * 0x2)),
				BM_RMON_CTRL_BCAST_CNT_SHIFT,
				BM_RMON_CTRL_BCAST_CNT_SIZE, 1);
		}

		if (gswdev->sdev == LTQ_FLOW_DEV_INT_R)
			gsw_w32(cdev, PCE_TFCR_NUM_NUM_OFFSET,
				PCE_TFCR_NUM_NUM_SHIFT,
				PCE_TFCR_NUM_NUM_SIZE, 0x80);
	}

	/*
	 * GSWIP-3.0 selects the 2-bit GL_MOD width (BM_QUEUE_GCTRL_GL_MOD_SIZE2,
	 * gsw_reg.h:663). AVM's gipver branch at lines 1604-1609 collapses to
	 * a single assignment under the GSWIP-3.0-only architectural constraint.
	 */
	Gl_Mod_Size = BM_QUEUE_GCTRL_GL_MOD_SIZE2;
	gsw_w32(cdev, BM_QUEUE_GCTRL_GL_MOD_OFFSET, BM_QUEUE_GCTRL_GL_MOD_SHIFT,
		Gl_Mod_Size, 0);

	/*
	 * SDMA port-enable is set for every port that can receive, p6
	 * included.
	 */
	for (j = 0; j < no_ports; j++) {
		if (gswdev->sdev == LTQ_FLOW_DEV_INT_R && j == 1)
			j = 15;
		gsw_w32(cdev, (FDMA_PCTRL_EN_OFFSET + (j * 0x6)),
			FDMA_PCTRL_EN_SHIFT, FDMA_PCTRL_EN_SIZE, 1);
	}

	/*
	 * The FDMA loop above carries the GSW-R j==1->15 skip, which EXCLUDES
	 * LAN ports 2-5 from FDMA_PCTRL.EN on GSW-R -- so GSW-R LAN ingress
	 * (the GSW-L->GSW-R bridge direction) is left disabled. Arm them
	 * explicitly, mirroring the SDMA LAN-port loop below, so FDMA + SDMA
	 * are both live for the LAN egress/bridge path.
	 */
	{
		static const u16 fdma_lan_ports[] = { 2, 3, 4, 5 };
		u16 k;

		for (k = 0; k < ARRAY_SIZE(fdma_lan_ports); k++) {
			u16 p = fdma_lan_ports[k];

			gsw_w32(cdev, (FDMA_PCTRL_EN_OFFSET + (p * 0x6)),
				FDMA_PCTRL_EN_SHIFT, FDMA_PCTRL_EN_SIZE, 1);
		}
	}

	/*
	 * SDMA: arm the egress ports EXPLICITLY (ports 0,1 + LAN ports
	 * 2,3,4,5) on BOTH instances. Iterate the LAN ports directly so p5 is
	 * armed.
	 */
	{
		/*
		 * SDMA port-enable is set for every port that can receive, p6
		 * included.
		 */
		static const u16 sdma_ports[] = { 0, 1, 2, 3, 4, 5, 6 };
		u16 k;

		for (k = 0; k < ARRAY_SIZE(sdma_ports); k++) {
			u16 p = sdma_ports[k];

			gsw_w32(cdev, (SDMA_PCTRL_PEN_OFFSET + (p * 0x6)),
				SDMA_PCTRL_PEN_SHIFT, SDMA_PCTRL_PEN_SIZE, 1);
		}
	}

	/*
	 * SDMA port-enable is set for every port that can receive, p6
	 * included.
	 */
	{
		u16 lim = (gswdev->sdev == LTQ_FLOW_DEV_INT_R) ? 16 : no_ports;
		u16 p;

		for (p = 0; p < lim; p++) {
			gsw_w32(cdev, (FDMA_PCTRL_EN_OFFSET + (p * 0x6)),
				FDMA_PCTRL_EN_SHIFT, FDMA_PCTRL_EN_SIZE, 1);
			gsw_w32(cdev, (SDMA_PCTRL_PEN_OFFSET + (p * 0x6)),
				SDMA_PCTRL_PEN_SHIFT, SDMA_PCTRL_PEN_SIZE, 1);
			gsw_w32(cdev, (BM_PCFG_CNTEN_OFFSET + (p * 2)),
				BM_PCFG_CNTEN_SHIFT, BM_PCFG_CNTEN_SIZE, 1);
		}
	}

	for (j = 0; j + 1 < no_ports; j++)
		gsw_w32(cdev, (short)(GSWT_ANEG_EEE_1_CLK_STOP_CAPABLE_OFFSET +
				      (j * 4) + GSW30_TOP_OFFSET),
			GSWT_ANEG_EEE_1_CLK_STOP_CAPABLE_SHIFT,
			GSWT_ANEG_EEE_1_CLK_STOP_CAPABLE_SIZE, 0x3);

	gsw_w32(cdev, MAC_PFSA_0_PFAD_OFFSET, MAC_PFSA_0_PFAD_SHIFT,
		MAC_PFSA_0_PFAD_SIZE, 0x0000);
	gsw_w32(cdev, MAC_PFSA_1_PFAD_OFFSET, MAC_PFSA_1_PFAD_SHIFT,
		MAC_PFSA_1_PFAD_SIZE, 0x9600);
	gsw_w32(cdev, MAC_PFSA_2_PFAD_OFFSET, MAC_PFSA_2_PFAD_SHIFT,
		MAC_PFSA_2_PFAD_SIZE, 0xAC9A);

	if (gswdev->sdev != LTQ_FLOW_DEV_INT_R) {
		/* "discard jumbo frames on the GSWIP-L" (AVM :2823-2830) */
		gsw_w32(cdev, MAC_FLEN_LEN_OFFSET, MAC_FLEN_LEN_SHIFT,
			MAC_FLEN_LEN_SIZE, 1518);
	} else {
		gsw_w32(cdev, MAC_FLEN_LEN_OFFSET, MAC_FLEN_LEN_SHIFT,
			MAC_FLEN_LEN_SIZE, 1612);
		gsw_w32(cdev, PMAC_CTRL_2_MLEN_OFFSET, PMAC_CTRL_2_MLEN_SHIFT,
			PMAC_CTRL_2_MLEN_SIZE, 1);
		gsw_w32(cdev, MAC_CTRL_2_MLEN_OFFSET, MAC_CTRL_2_MLEN_SHIFT,
			MAC_CTRL_2_MLEN_SIZE, 1);
	}

	if (gswdev->sdev == LTQ_FLOW_DEV_INT) {
		gsw_pmac_glbl_cfg(cdev);
		gsw_pmac_ig_cfg(cdev);
		gsw_pmac_eg_cfg(cdev);
	} else if (gswdev->sdev == LTQ_FLOW_DEV_INT_R) {
		gsw_w32(cdev, PMAC_CTRL_0_PADEN_OFFSET, PMAC_CTRL_0_PADEN_SHIFT,
			PMAC_CTRL_0_PADEN_SIZE, 1);
		gsw_pmac_ig_cfg_r(cdev);
		gsw_pmac_eg_cfg_r(cdev);
	}

	gsw_cpu_special_tag_ingress(cdev);

	return GSW_statusOk;
}

int gsw_hw_reinit(void *cdev)
{
	u32 j = 1;
	int spins = 0;

	gsw_w32(cdev, ETHSW_SWRES_R0_OFFSET, ETHSW_SWRES_R0_SHIFT,
		ETHSW_SWRES_R0_SIZE, 1);
	do {
		u32 raw = 0;

		gsw_r32_raw(cdev, ETHSW_SWRES_R0_OFFSET, &raw);
		j = raw & BIT(ETHSW_SWRES_R0_SHIFT);
	} while (j && ++spins < 1000000);

	pr_info("gsw: SWRES self-cleared after %d polls; re-running full init\n",
		spins);

	return gsw_pmicro_code_init(cdev);
}

int gsw_hw_reinit_gswl(void)
{
	static bool done;
	struct core_ops *ops = gsw_get_swcore_ops(0);

	if (!ops)
		return -ENODEV;

	if (done) {
		pr_info("gsw: GSW-L full re-init already done; skipping (whole-core SWRES must not re-run under live links)\n");
		return -EALREADY;
	}
	done = true;

	return gsw_hw_reinit(GSW_PDATA_GET(ops));
}
EXPORT_SYMBOL_GPL(gsw_hw_reinit_gswl);
