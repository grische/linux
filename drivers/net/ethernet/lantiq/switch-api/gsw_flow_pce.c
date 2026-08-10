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
 * Ported from AVM gsw_tbl_rw.c:465-535. Caller is responsible for serialising
 * concurrent invocations via pethdev->lock_pce (helpers themselves do not
 * touch the spinlock - matches AVM convention).
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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);
	/*
	 * Re-seed ctrlval from PCE_TBL_CTRL post-poll. Without this read,
	 * gsw_field_w32(ctrlval, ...) would consume an uninitialized auto
	 * variable - which both -Wuninitialized would flag fatal and would
	 * write stack garbage into the PCE_TBL_CTRL register on the
	 * subsequent gsw_w32_raw(..., ctrlval).
	 */
	gsw_r32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, &ctrlval);

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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);

	gsw_w32_raw(cdev, PCE_TBL_CTRL_ADDR_OFFSET, 0);

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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);
	/* See gsw_pce_table_write for the rationale for this re-seed. */
	gsw_r32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, &ctrlval);

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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);
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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);
	/* See gsw_pce_table_write for the rationale for this re-seed. */
	gsw_r32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, &ctrlval);

	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_ADDR_SHIFT,
				PCE_TBL_CTRL_ADDR_SIZE, ptdata->table);

	/*KEY REG*/
	j = gswdev->pce_tbl_info[ptdata->table].num_key;

	for (i = 0; i < j; i++) {
		gsw_w32_raw(cdev, gswdev->pce_tbl_reg.key[i], ptdata->key[i]);
	}

	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_OPMOD_SHIFT,
				PCE_TBL_CTRL_OPMOD_SIZE, PCE_OP_MODE_KSRD);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_KEYFORM_SHIFT,
				PCE_TBL_CTRL_KEYFORM_SIZE, 0);
	ctrlval = gsw_field_w32(ctrlval, PCE_TBL_CTRL_BAS_SHIFT,
				PCE_TBL_CTRL_BAS_SIZE, 1);
	gsw_w32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, ctrlval);

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);
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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);
	/* See gsw_pce_table_write for the rationale for this re-seed. */
	gsw_r32_raw(cdev, PCE_TBL_CTRL_BAS_OFFSET, &ctrlval);

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

	CHECK_BUSY(PCE_TBL_CTRL_BAS_OFFSET, PCE_TBL_CTRL_BAS_SHIFT,
		   PCE_TBL_CTRL_BAS_SIZE, RETURN_FROM_FUNCTION);

	gsw_w32_raw(cdev, PCE_TBL_CTRL_ADDR_OFFSET, 0);

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
 */
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
	}

	/*
	 * GSWIP-3.0 selects the 2-bit GL_MOD width (BM_QUEUE_GCTRL_GL_MOD_SIZE2,
	 * gsw_reg.h:663). AVM's gipver branch at lines 1604-1609 collapses to
	 * a single assignment under the GSWIP-3.0-only architectural constraint.
	 */
	Gl_Mod_Size = BM_QUEUE_GCTRL_GL_MOD_SIZE2;
	gsw_w32(cdev, BM_QUEUE_GCTRL_GL_MOD_OFFSET, BM_QUEUE_GCTRL_GL_MOD_SHIFT,
		Gl_Mod_Size, 0);


	return GSW_statusOk;
}
