#include <linux/slab.h>
#include <uapi/linux/magic.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/stddef.h>

static int itsmagic(uint32_t word)
{
	/* avm squashfs may be using cpu byte-order, so check both */
	if (word == SQUASHFS_MAGIC || swab32(word) == SQUASHFS_MAGIC) {
		return SQUASHFS_MAGIC;
	} else {
		return false;
	}
}

/* returns the offset of a superblock or mtd->size on error */
static size_t find_rootfs(struct mtd_info *mtd)
{
	size_t pos, readlen;
	uint32_t word;

	for (pos = 0; pos < mtd->size;
	     pos += 256) { /* TODO where is 256 coming from? */
		if (mtd_read(mtd, pos, sizeof(word), &readlen,
			     (u_char *)&word) == -EINVAL) {
			return mtd->size;
		}
		if (itsmagic(word)) {
			pr_debug(
				"[%s] found some superblock at mtd offset %d.\n",
				__func__, pos);
			break;
		}
	}

	return pos;
}

static int mtd_parser_cb(struct mtd_info *mtd, const struct mtd_partition **p_mtd_pat,
			 struct mtd_part_parser_data *parse_data)
{
	struct mtd_partition *parts = NULL;
	int parts_n = 0;

	pr_debug("[%s] entered\n", __func__);

	if (strcmp(mtd->name, "ram-filesystem") == 0) {
		unsigned long int offset;
		parts = kzalloc(sizeof(*parts) * 2, GFP_KERNEL);
		BUG_ON(!parts);

		parts[0].name = "rootfs_ram";
		parts[0].mask_flags = MTD_ROM;
		parts[1].name = "kernel_ram";
		parts[1].mask_flags = MTD_ROM;

		offset = find_rootfs(mtd);
		if (offset >= mtd->size) {
			/* something is wrong, as we couldn't find a
			 * filesystem */
			parts_n = 0;
			pr_debug("[%s] no fs found\n", __func__);
		} else if (offset == 0) {
			/* there's no data in front of the fs, so we invalidate the
			 * second partition*/
			parts[1].offset = mtd->size + 1;
			parts_n = 2;
			pr_debug("[%s] fs found at offset %lld\n", __func__,
				 parts[1].offset);
		} else {
			/* we found a fs behind something that we assume to
			 * be the kernel */
			parts[0].offset = offset;
			parts[0].size = mtd->size - offset;
			parts[1].size = offset;
			parts_n = 2;
			pr_debug("[%s] fs found at offset %lld\n", __func__,
				 parts[1].offset);
		}
	} else {
		/* nand not handled yet */
	}

	*p_mtd_pat = parts;
	return parts_n;
}

struct mtd_part_parser avm_mtd_parser __initdata = {
	.owner = THIS_MODULE,
	.parse_fn = mtd_parser_cb,
	.name = "avmpart",
};
