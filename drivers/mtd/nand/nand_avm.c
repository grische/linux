#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>

#define SOFT_ECC_FIXUP 1

struct avm_ids {
	struct nand_id id;
	unsigned int options;
	unsigned int avm_flags;
} chips[] = {
	{{{0x98, 0xf1, 0x80, 0x15, 0xf2}, 5}, NAND_NO_SUBPAGE_WRITE, 0}, /* tc58bvg0s3hta */
	{{{0x98, 0xdc, 0x90, 0x26, 0xf6}, 5}, 0, SOFT_ECC_FIXUP}, /* tc58bvg2s0hta */
	{{{0xc2, 0xf1, 0x80, 0x95, 0x82}, 5}, 0, SOFT_ECC_FIXUP}, /* mx30fl1ge8ab */
	{{{0xc2, 0xdc, 0x90, 0x95, 0xd6}, 5}, NAND_NO_SUBPAGE_WRITE, 0} /* mx30fl4ge8ab */
};

static void try_avm_ecc_fixup(struct nand_chip *chip)
{
	pr_err("%s: AVM ecc fixup is not yet implemented - skipping check\n", __func__);
	// TODO: fixup
}

int nand_avm_init_fixup(struct nand_chip *chip)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(chips); i++) {
		if (!memcmp(chips[i].id.data, chip->id.data, min(chips[i].id.len, chip->id.len))) {
			if (chips[i].options) {
				pr_info("%s: applying nand options fixup\n", __func__);
				chip->options |= chips[i].options;
			}
			if (chips[i].avm_flags & SOFT_ECC_FIXUP) {
				try_avm_ecc_fixup(chip);
			}
		}
	}

	return 0;
}
