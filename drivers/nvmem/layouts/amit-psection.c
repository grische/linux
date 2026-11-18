// SPDX-License-Identifier: GPL-2.0-only
/*
 * NVMEM layout for the AMIT P-section
 *
 * Devices built by AMIT, e.g. the D-Link AQUILA PRO AI M60, keep their
 * factory data in a "P-section". All fields are little endian:
 *
 *   0x00  section tag (16 bytes)
 *           u8  cmark          u8  section_id     u16 magic (0x2b24)
 *           u32 time_stamp     u32 image_length
 *           u16 image_checksum u16 tag_checksum
 *   0x10  CSXF header (16 bytes)
 *           u16 magic (0x5343) u16 checksum       u32 body_length
 *           u8  body_encoding  u8  reserved       u16 seed
 *           u32 raw_length
 *   0x20  body: records, terminated by csid 0
 *           u32 csid           s16 size           u8 data[max(size, 0)]
 *
 * The offset of a record depends on the sizes of all records before it,
 * so a value has to be looked up by its CSID.
 */
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/sizes.h>
#include <linux/slab.h>

#define AMIT_PSEC_TAG_MAGIC	0x2b24
#define AMIT_PSEC_CSXF_MAGIC	0x5343
#define AMIT_PSEC_ENC_PLAIN	0
/* headers and body; the OEM firmware reads only the first 16 KiB */
#define AMIT_PSEC_MAX_SIZE	SZ_16K
#define AMIT_PSEC_CSID_BEID	0x80004230

struct amit_psec_tag {
	u8 cmark;
	u8 section_id;
	__le16 magic;
	__le32 time_stamp;
	__le32 image_length;
	__le16 image_checksum;
	__le16 tag_checksum;
} __packed;

struct amit_psec_csxf {
	__le16 magic;
	__le16 checksum;
	__le32 body_length;
	u8 body_encoding;
	u8 reserved;
	__le16 seed;
	__le32 raw_length;
} __packed;

struct amit_psec_header {
	struct amit_psec_tag tag;
	struct amit_psec_csxf csxf;
} __packed;

struct amit_psec_record {
	__le32 csid;
	__le16 size;
	u8 data[];
} __packed;

static_assert(sizeof(struct amit_psec_tag) == 16);
static_assert(sizeof(struct amit_psec_csxf) == 16);
static_assert(sizeof(struct amit_psec_record) == 6);

/*
 * Return the offset of the data of the first record with the given CSID,
 * or a negative error code. The record must hold len bytes of data.
 */
static int amit_psec_find(struct nvmem_device *nvmem, u32 csid, size_t len)
{
	const struct amit_psec_record *rec;
	struct amit_psec_header hdr;
	size_t body_len, pos, size;
	u8 *body;
	u32 id;
	int ret;

	ret = nvmem_device_read(nvmem, 0, sizeof(hdr), &hdr);
	if (ret < 0)
		return ret;
	if (ret != sizeof(hdr))
		return -EIO;

	/* Like the OEM firmware's Linux reader, ignore the checksums. */
	if (le16_to_cpu(hdr.tag.magic) != AMIT_PSEC_TAG_MAGIC ||
	    le16_to_cpu(hdr.csxf.magic) != AMIT_PSEC_CSXF_MAGIC ||
	    hdr.csxf.body_encoding != AMIT_PSEC_ENC_PLAIN)
		return -EINVAL;

	body_len = le32_to_cpu(hdr.csxf.body_length);
	if (body_len > AMIT_PSEC_MAX_SIZE - sizeof(hdr) ||
	    sizeof(hdr) + body_len > nvmem_dev_size(nvmem))
		return -EINVAL;
	if (!body_len)
		return -ENOENT;

	body = kmalloc(body_len, GFP_KERNEL);
	if (!body)
		return -ENOMEM;

	ret = nvmem_device_read(nvmem, sizeof(hdr), body_len, body);
	if (ret < 0)
		goto out;
	if (ret != body_len) {
		ret = -EIO;
		goto out;
	}

	ret = -ENOENT;
	pos = 0;
	while (body_len - pos >= sizeof(*rec)) {
		rec = (const struct amit_psec_record *)(body + pos);
		id = le32_to_cpu(rec->csid);
		if (!id)
			break;

		size = max_t(int, (s16)le16_to_cpu(rec->size), 0);
		pos += sizeof(*rec);
		if (size > body_len - pos) {
			ret = -EINVAL;
			break;
		}

		if (id == csid) {
			ret = size == len ? sizeof(hdr) + pos : -EINVAL;
			break;
		}
		pos += size;
	}

out:
	kfree(body);
	return ret;
}

static int amit_psec_mac_pp(void *priv, const char *id, int index,
			    unsigned int offset, void *buf, size_t bytes)
{
	if (bytes != ETH_ALEN || index < 0 || !is_valid_ether_addr(buf))
		return -EINVAL;

	eth_addr_add(buf, index);

	return 0;
}

static int amit_psec_add_cells(struct nvmem_layout *layout)
{
	struct nvmem_cell_info info = {
		.name = "base-mac-address",
		.bytes = ETH_ALEN,
		.read_post_process = amit_psec_mac_pp,
	};
	int ret;

	ret = amit_psec_find(layout->nvmem, AMIT_PSEC_CSID_BEID, ETH_ALEN);
	if (ret < 0) {
		dev_err(&layout->dev, "no base MAC address found: %d\n", ret);
		/*
		 * Bind without the cell. of_nvmem_cell_get() then returns
		 * -ENOENT, and the consumers fall back to another address.
		 */
		return 0;
	}

	info.offset = ret;
	info.np = of_get_child_by_name(layout->dev.of_node, info.name);
	ret = nvmem_add_one_cell(layout->nvmem, &info);
	if (ret)
		of_node_put(info.np);

	return ret;
}

static int amit_psec_probe(struct nvmem_layout *layout)
{
	layout->add_cells = amit_psec_add_cells;

	return nvmem_layout_register(layout);
}

static void amit_psec_remove(struct nvmem_layout *layout)
{
	nvmem_layout_unregister(layout);
}

static const struct of_device_id amit_psec_of_match_table[] = {
	{ .compatible = "amit,psection" },
	{},
};
MODULE_DEVICE_TABLE(of, amit_psec_of_match_table);

static struct nvmem_layout_driver amit_psec_layout = {
	.driver = {
		.name = "amit-psection-layout",
		.of_match_table = amit_psec_of_match_table,
	},
	.probe = amit_psec_probe,
	.remove = amit_psec_remove,
};
module_nvmem_layout_driver(amit_psec_layout);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NVMEM layout driver for the AMIT P-section");
