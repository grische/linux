/*
 * Copyright (C) 2017 Free Electrons
 * Copyright (C) 2017 NextThing Co
 *
 * Author: Boris Brezillon <boris.brezillon@free-electrons.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mtd/nand.h>

/* Bit for detecting internal ECC */
#define MACRONIX_NAND_ID4_IS_INTERNAL_ECC	BIT(7)

#define MACRONIX_NAND_STATUS_ECC_MASK		(BIT(3)|BIT(4))
#define MACRONIX_NAND_STATUS_ECC_01BIT		0
#define MACRONIX_NAND_STATUS_ECC_2BIT		BIT(4)
#define MACRONIX_NAND_STATUS_ECC_3BIT		BIT(3)
#define MACRONIX_NAND_STATUS_ECC_4BIT		(BIT(3)|BIT(4))

static int nand_ooblayout_ecc_mx30lf4ge8ab(struct mtd_info *mtd, int section,
		struct mtd_oob_region *oobregion) {

	if (section > 3)
		return -ERANGE;

	oobregion->offset = 16 * section + 8;
	oobregion->length = 8;

	return 0;
};

static int nand_ooblayout_free_mx30lf4ge8ab(struct mtd_info *mtd, int section,
		struct mtd_oob_region *oobregion) {

	if (section > 3)
		return -ERANGE;

	oobregion->offset = !section ? 2 : 16 * section;
	oobregion->length = !section ? 6 : 8;

	return 0;
};

const struct mtd_ooblayout_ops nand_ooblayout_mx30lf4ge8ab_ops = {
	.ecc = nand_ooblayout_ecc_mx30lf4ge8ab,
	.free = nand_ooblayout_free_mx30lf4ge8ab,
};

static int macronix_nand_ecc_status(struct mtd_info *mtd,
					 struct nand_chip *chip)
{
	int ret;
	unsigned int max_bitflips = 0;
	u8 status;

	/* Check Status */
	ret = nand_status_op(chip, &status);

	if (ret)
		return ret;

	if (status & NAND_STATUS_FAIL) {
		/* uncorrected */
		mtd->ecc_stats.failed++;
	} else {
		switch (status & MACRONIX_NAND_STATUS_ECC_MASK) {
		case MACRONIX_NAND_STATUS_ECC_01BIT:
			/* 0 or 1 bit errors */
			max_bitflips = 0;
			break;
		case MACRONIX_NAND_STATUS_ECC_2BIT:
			max_bitflips = 2;
			break;
		case MACRONIX_NAND_STATUS_ECC_3BIT:
			max_bitflips = 3;
			break;
		case MACRONIX_NAND_STATUS_ECC_4BIT:
			max_bitflips = 4;
			break;
		}
		mtd->ecc_stats.corrected += max_bitflips;
	}

	return max_bitflips;
}

static int
macronix_nand_ecc_read_page(struct mtd_info *mtd,
			      struct nand_chip *chip, uint8_t *buf,
			      int oob_required, int page)
{
	int ret;

	ret = nand_read_page_raw(mtd, chip, buf, oob_required, page);
	if (ret)
		return ret;

	return macronix_nand_ecc_status(mtd, chip);
}

static int
macronix_nand_ecc_read_subpage(struct mtd_info *mtd,
				 struct nand_chip *chip, uint32_t data_offs,
				 uint32_t readlen, uint8_t *bufpoi, int page)
{
	int ret;

	ret = nand_read_page_op(chip, page, data_offs,
				bufpoi + data_offs, readlen);
	if (ret)
		return ret;

	return macronix_nand_ecc_status(mtd, chip);
}

static int macronix_nand_ecc_init(struct nand_chip *chip)
{
	struct mtd_info *mtd = nand_to_mtd(chip);
	int hidden_ecc = chip->id.data[1] != 0xdc;

	/*
	 * On MX30LF1GE8AB and MX30LF2GE8AB the ECC bytes are hidden. On
	 * MX30LF4GE8AB they are visible.
	 */

	pr_err("%s: enabling ECC (hidden_ecc: %d):\n", __func__, hidden_ecc);

	chip->ecc.bytes = hidden_ecc ? 0 : 8;
	chip->ecc.size = 512;
	chip->ecc.strength = 4;
	chip->ecc.algo = NAND_ECC_BCH;
	chip->ecc.read_page = macronix_nand_ecc_read_page;
	chip->ecc.read_subpage = macronix_nand_ecc_read_subpage;
	chip->ecc.write_page = nand_write_page_raw;
	chip->ecc.read_page_raw = nand_read_page_raw_notsupp;
	chip->ecc.write_page_raw = nand_write_page_raw_notsupp;

	chip->options |= NAND_SUBPAGE_READ;

	mtd_set_ooblayout(mtd, hidden_ecc
			? &nand_ooblayout_lp_ops
			: &nand_ooblayout_mx30lf4ge8ab_ops);

	return 0;
}

static int macronix_nand_init(struct nand_chip *chip)
{
	if (nand_is_slc(chip))
		chip->bbt_options |= NAND_BBT_SCAN2NDPAGE;

	/* Check that chip has internal ECC and ECC mode is on-die */
	if (nand_is_slc(chip) && chip->ecc.mode == NAND_ECC_ON_DIE &&
			chip->id.len >= 5 &&
			(chip->id.data[4] & MACRONIX_NAND_ID4_IS_INTERNAL_ECC))
		return macronix_nand_ecc_init(chip);

	return 0;
}

const struct nand_manufacturer_ops macronix_nand_manuf_ops = {
	.init = macronix_nand_init,
};
