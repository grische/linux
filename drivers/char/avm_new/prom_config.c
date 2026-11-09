// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "[avm_prom_config] " fmt

#include <linux/module.h>

#include <linux/vmalloc.h>
#include <linux/mtd/mtd.h>
#include <linux/blkdev.h>
#include <linux/zlib.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/pagemap.h>
#include <linux/blkdev.h>
#include <linux/version.h>

#include <avm/enh/prom_config.h>
#include "prom_config_internal.h"

#define AVM_PROM_DEVICEWAIT_MS 5000

struct config_data {
	loff_t offset;
	size_t size;
	size_t hdrsize;
	enum avm_prom_config_type zipped_src_type;

	// Flags
	unsigned int is_present : 1;
	unsigned int has_zipped_src : 1;
	unsigned int is_zipped : 1;
	unsigned int procfs_export : 1;
	unsigned int is_aligned : 1;
};

// This struct holds information about the zip types
//
// It will be filled by load_config. If a non-zipped config
// is found on-disk the has_zipped_src flag is cleared.
static struct config_data configs[AVM_PROM_MAX_TYPE] = {
	[AVM_PROM_WLAN]   = {
		.procfs_export   = 1,
		.has_zipped_src  = 1,
		.zipped_src_type = WLAN_ZIP
	},
	[AVM_PROM_WLAN2]  = {
		.procfs_export   = 1,
		.has_zipped_src  = 1,
		.zipped_src_type = WLAN2_ZIP
	},
	[AVM_PROM_WLAN3]  = {
		.procfs_export   = 1,
		.has_zipped_src  = 1,
		.zipped_src_type = WLAN3_ZIP
	},
	[AVM_PROM_WLAN4]  = {
		.procfs_export   = 1,
		.has_zipped_src  = 1,
		.zipped_src_type = WLAN4_ZIP
	},
	[AVM_PROM_DECT]           = { .procfs_export = 1, },
	[AVM_PROM_ZERTIFIKATE]    = { .procfs_export = 1, },
	[AVM_PROM_DOCSIS]         = { .procfs_export = 1, },
	[AVM_PROM_DSL]            = { .procfs_export = 1, },
	[AVM_PROM_FINAL]          = { .procfs_export = 1, },
	[AVM_PROM_PRODCERT1]      = { .procfs_export = 1, .is_aligned = 1 },
	[AVM_PROM_PRODCERT2]      = { .procfs_export = 1, .is_aligned = 1 },
	[AVM_PROM_PRODCERT3]      = { .procfs_export = 1, .is_aligned = 1 },
	[AVM_PROM_PRODCERT4]      = { .procfs_export = 1, .is_aligned = 1 },
	[AVM_PROM_PRODENV]        = { .procfs_export = 1, .is_aligned = 1 },
	[AVM_PROM_AVMZERTIFIKATE] = { .procfs_export = 1, },
	[AVM_PROM_LRWPAN]         = { .procfs_export = 1, },
	[AVM_PROM_FIBER]          = {
		.procfs_export   = 1,
		.has_zipped_src  = 1,
		.zipped_src_type = FIBER_ZIP
	},
};

static struct avm_prom_config_device *avm_prom_config_device;
unsigned int (*avm_prom_config_offsets)[AVM_PROM_CONFIG_LEGACY_ENTRY_COUNT] __initdata;

#define to_mtd(prom_dev) (((struct avm_prom_mtd_device *)(prom_dev))->mtd)
#define to_blockdev(prom_dev) (((struct avm_prom_block_device *)(prom_dev))->bdev)

static bool avm_prom_config_device_ready(void)
{
	return smp_load_acquire(&avm_prom_config_device) != NULL;
}

static bool avm_prom_type_is_valid(int type)
{
	return type >= 0 && type < AVM_PROM_MAX_TYPE;
}

int avm_prom_is_config_available(enum avm_prom_config_type type)
{
	struct config_data *config;

	if (type >= AVM_PROM_MAX_TYPE)
		return 0;

	config = &configs[type];

	if (config->has_zipped_src)
		return avm_prom_is_config_available(config->zipped_src_type);

	return config->is_present;
}

int get_wlan_dect_config(enum wlan_dect_type type, unsigned char *buffer,
			 unsigned int len)
{
	struct wlan_dect_config *hdr = (void *)buffer;
	ssize_t ret;

	ret = avm_prom_get_config(type, hdr + 1,
				  len - sizeof(struct wlan_dect_config));

	if (ret < 0)
		return ret;

	// We can not return the size, so abort
	if (ret >= U16_MAX)
		return -EOVERFLOW;

	// Fake the header
	hdr->Version = 1;
	hdr->Type = type;
	hdr->Len = ret;

	return 0;
}
EXPORT_SYMBOL(get_wlan_dect_config);

static ssize_t config_mtd_read(struct avm_prom_config_device *prom_dev,
			       loff_t offset,
			       void *buf,
			       size_t size)
{
	struct mtd_info *mtd = to_mtd(prom_dev);
	void *bufstart = buf;
	size_t retlen;
	int ret;
	u8 align_buf[4];

	if (!IS_ENABLED(CONFIG_AVM_PROMCONFIG_FORCE_WORDALIGNED_ACCESS)) {
		ret = mtd_read(mtd, offset, size, &retlen, buf);
		if (ret < 0)
			return ret;
		if (retlen != size)
			return -EIO;
		return retlen;
	}

	// The start is unaligned, so we align our reads to 4
	if (!IS_ALIGNED(offset, sizeof(align_buf))) {
		loff_t aligned_offset = round_down(offset, sizeof(align_buf));
		unsigned int start = offset - aligned_offset;
		unsigned int len = min(sizeof(align_buf) - start, size);

		ret = mtd_read(mtd, aligned_offset, sizeof(align_buf), &retlen,
				align_buf);

		if (ret < 0)
			return ret;
		if (retlen != sizeof(align_buf))
			return -EIO;

		// We need to copy over the last len bytes
		memcpy(buf, align_buf + start, len);
		offset = ALIGN(offset, sizeof(align_buf));
		buf += len;
		size -= len;
	}

	// Now copy aligned to 4 byte boundaries
	if (size >= sizeof(align_buf)) {
		size_t len = round_down(size, sizeof(align_buf));

		ret = mtd_read(mtd, offset, len, &retlen, buf);

		if (ret < 0)
			return ret;
		if (retlen != len)
			return -EIO;

		offset += len;
		buf += len;
		size -= len;
	}

	// Read the last remaining bytes
	if (size > 0) {
		ret = mtd_read(mtd, offset, sizeof(align_buf), &retlen, align_buf);

		if (ret < 0)
			return ret;
		if (retlen != sizeof(align_buf))
			return -EIO;

		memcpy(buf, align_buf, size);
		buf += size;
	}

	return buf - bufstart;
}

static const struct avm_prom_config_device_ops mtd_ops = {
	.read = config_mtd_read,
};

static ssize_t config_blk_read(struct avm_prom_config_device *prom_dev,
			       loff_t offset,
			       void *buf,
			       size_t size)
{
	struct block_device *blk = to_blockdev(prom_dev);
	void *bufstart = buf;
	pgoff_t index;
	off_t page_offset;

	index = offset >> PAGE_SHIFT;
	page_offset = offset & (PAGE_SIZE - 1);

	while (size > 0) {
		size_t cpylen;
		struct page *page;

		if ((page_offset + size) > PAGE_SIZE)
			cpylen = PAGE_SIZE - page_offset;
		else
			cpylen = size;

		page = read_mapping_page(
				blk->bd_inode->i_mapping,
				index,
				NULL);
		if (IS_ERR(page))
			return PTR_ERR(page);

		memcpy(buf, page_address(page) + page_offset, cpylen);

		put_page(page);

		buf += cpylen;
		size -= cpylen;
		page_offset = 0;
		index++;
	}

	return buf - bufstart;
}

static loff_t config_blk_lba_to_ofs(struct avm_prom_config_device *prom_dev,
				    u32 lba)
{
	struct block_device *blk = to_blockdev(prom_dev);

	return lba * bdev_physical_block_size(blk);
}

static const struct avm_prom_config_device_ops blk_ops = {
	.read      = config_blk_read,
	.to_loff_t = config_blk_lba_to_ofs,
};

static int avm_prom_config_add_device(struct avm_prom_config_device *prom_dev)
{
	BUG_ON(!prom_dev || !prom_dev->ops);
	WARN_ON(avm_prom_config_device);
	avm_prom_config_device = prom_dev;

	return 0;
}

static int avm_prom_config_add_mtd_device(struct avm_prom_mtd_device *prom_mtd)
{
	struct avm_prom_config_device *prom_dev;
	struct mtd_info *mtd;
	int ret;

	BUG_ON(!prom_mtd || !prom_mtd->mtd || prom_mtd->prom_dev.ops);
	prom_dev = &prom_mtd->prom_dev;
	mtd = prom_mtd->mtd;

	ret = __get_mtd_device(mtd);
	if (ret)
		return ret;

	prom_dev->ops = &mtd_ops;

	ret = avm_prom_config_add_device(prom_dev);
	if (ret) {
		__put_mtd_device(mtd);
		return ret;
	}

	pr_debug("Use config mtd \"%s\" (mtd%d)\n", mtd->name,
	       mtd->index);

	return 0;
}

static int avm_prom_config_add_blkdev(struct avm_prom_block_device *prom_bdev)
{
	struct avm_prom_config_device *prom_dev;
	struct block_device *bdev;
	char b[BDEVNAME_SIZE];
	int ret;

	BUG_ON(!prom_bdev || !prom_bdev->bdev || prom_bdev->prom_dev.ops);
	prom_dev = &prom_bdev->prom_dev;
	bdev = prom_bdev->bdev;

	/*
	 * The blkdev API is amazing. blkdev_get does not get a ref to the
	 * bdev, assuming this has already happened, while blkdev_put drops
	 * the ref.  Also blkdev_get drops the ref when it encounters an
	 * error. So, the required sequence for us here is bdgrab ->
	 * blkdev_get -> blkdev_put.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	ret = PTR_ERR_OR_ZERO(blkdev_get_by_dev(bdev->bd_dev, FMODE_READ, NULL));
#else
	bdgrab(bdev);
	ret = blkdev_get(bdev, FMODE_READ, NULL);
#endif
	if (ret)
		return ret;

	prom_dev->ops = &blk_ops;

	ret = avm_prom_config_add_device(prom_dev);
	if (ret) {
		blkdev_put(bdev, FMODE_READ);
		return ret;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	pr_debug("Use config blkdev %s (%s)\n",
	       bdev->bd_meta_info->volname, bdevname(bdev, b));
#else
	pr_debug("Use config blkdev %s (%s)\n",
	       bdev->bd_part->info->volname, bdevname(bdev, b));
#endif
	return 0;
}

static const char *avm_prom_get_config_device_name(void)
{
	const char *name;
	int ret;

	if (!of_have_populated_dt() || !of_chosen)
		goto out_default;

	ret = of_property_read_string(of_chosen, "wlan_dect_config_part",
				      &name);
	if (ret < 0)
		goto out_default;

	return name;

out_default:
	return "urlader";
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
static struct partition_meta_info *bdev_part_info(struct device *dev)
{
	struct block_device *blkdev = dev_to_bdev(dev);

	return blkdev->bd_meta_info;
}
#else
static struct partition_meta_info *bdev_part_info(struct device *dev)
{
	struct hd_struct *part = dev_to_part(dev);

	return part->info;
}
#endif

static int match_dev_by_label(struct device *dev, const void *data)
{
	const char *label = data;
	struct partition_meta_info *info = bdev_part_info(dev);

	if (!info)
		return 0;

	if (strcasecmp(label, info->volname))
		return 0;
	// TODO: we should make sure that we have an internal (non-usb) device
	return 1;
}

static struct block_device *blkdev_get_by_name(const char *name)
{
	struct block_device *bdev;
	struct device *dev;

	wait_for_device_probe();

	dev = class_find_device(&block_class, NULL, name, &match_dev_by_label);

	if (!dev)
		return ERR_PTR(-ENODEV);

	bdev = blkdev_get_by_dev(dev->devt, FMODE_READ, THIS_MODULE);
	put_device(dev);

	return bdev;
}

static int prom_device_schedule_timeout(unsigned long msecs);

static int avm_prom_detect_config_device(void)
{
	static unsigned long timeout;
	const char *name;
	struct mtd_info *mtd;
	struct block_device *blk;
	int ret;

	if (avm_prom_config_device_ready())
		return 0;

	if (!timeout)
		timeout = jiffies + msecs_to_jiffies(AVM_PROM_DEVICEWAIT_MS);
	name = avm_prom_get_config_device_name();

retry:
	// First try if a mtd exists
	mtd = get_mtd_device_nm(name);
	if (!IS_ERR(mtd)) {
		struct avm_prom_mtd_device *prom_mtd;

		prom_mtd = kzalloc(sizeof(*prom_mtd), GFP_KERNEL);
		if (!prom_mtd) {
			put_mtd_device(mtd);
			return -ENOMEM;
		}

		prom_mtd->mtd = mtd;
		ret = avm_prom_config_add_mtd_device(prom_mtd);
		if (ret)
			kfree(prom_mtd);

		put_mtd_device(mtd);

		return ret;
	}

	// Then try block device
	blk = blkdev_get_by_name(name);
	if (!IS_ERR(blk)) {
		struct avm_prom_block_device *prom_bdev;

		prom_bdev = kzalloc(sizeof(*prom_bdev), GFP_KERNEL);
		if (!prom_bdev) {
			blkdev_put(blk, FMODE_READ);
			return -ENOMEM;
		}

		prom_bdev->bdev = blk;
		ret = avm_prom_config_add_blkdev(prom_bdev);
		if (ret)
			kfree(prom_bdev);

		blkdev_put(blk, FMODE_READ);

		return ret;
	}

	if (time_is_after_jiffies(timeout)) {
		ret = prom_device_schedule_timeout(100);
		if (ret)
			return ret;
		goto retry;
	}

	pr_err("Could not find config device\n");
	return -ENODEV;
}

static int avm_prom_read(loff_t offset, void *buf, size_t size)
{
	if (!avm_prom_config_device_ready())
		return -ENODEV;

	return avm_prom_config_device->ops->read(avm_prom_config_device,
			offset, buf, size);
}

static void avm_prom_create_files(enum avm_prom_config_type type)
{
	int i;

	BUG_ON(!avm_prom_type_is_valid(type));

	for (i = 0; i < AVM_PROM_MAX_TYPE; i++) {
		const struct config_data *c = &configs[i];

		if (c->has_zipped_src && c->zipped_src_type == type) {
			if (c->procfs_export)
				avm_prom_create_config_proc(i);
			avm_prom_create_config_debugfs(i);
		}
	}

	if (configs[type].procfs_export)
		avm_prom_create_config_proc(type);
	avm_prom_create_config_debugfs(type);
}

static int avm_prom_load_config_entry(loff_t offset)
{
	union {
		struct avm_prom_config_hdr v2;
		struct avm_prom_config_hdr_v3 v3;
	} hdr;
	ssize_t len, hdrsize;
	bool is_zipped;
	struct avm_prom_config_hdr_head *head =
				(struct avm_prom_config_hdr_head *)&hdr;
	struct config_data *config;
	int ret;

	BUILD_BUG_ON(sizeof(hdr.v2) != 4 || sizeof(hdr.v3) != 8);
	ret = avm_prom_read(offset, &hdr, sizeof(hdr.v2));
	if (ret < 0)
		return ret;
	if (ret != sizeof(hdr.v2))
		return -EIO;

	switch (head->version) {
	case 1: // FALL THROUGH
	case 2:
		hdrsize = sizeof(hdr.v2);
		len = be16_to_cpu(hdr.v2.len);
		is_zipped = false;
		break;
	case 3:
		ret = avm_prom_read(offset, &hdr, sizeof(hdr.v3));
		if (ret < 0)
			return ret;
		if (ret != sizeof(hdr.v3))
			return -EIO;
		hdrsize = sizeof(hdr.v3);
		len = be32_to_cpu(hdr.v3.len);
		is_zipped = be16_to_cpu(hdr.v3.cal_options) &
						AVM_CAL_OPTION_COMPRESSED;
		break;
	case 0xFF:
		return -ENOENT;
	default:
		return -EINVAL;
	}

	if (head->type >= AVM_PROM_MAX_TYPE)
		return -EINVAL;

	config = &configs[head->type];

	// We already loaded a config with this type
	if (config->is_present)
		return -EBUSY;

	config->offset = offset + hdrsize;
	config->size = len;
	config->hdrsize = hdrsize;
	config->is_present = 1;
	config->has_zipped_src = 0;
	config->is_zipped = is_zipped;
	pr_debug("Add entry ofs=0x%llx type=%d size=0x%zx\n",
	       config->offset, head->type, config->size);

	avm_prom_create_files(head->type);

	return head->type;
}

ssize_t avm_prom_raw_read_config(loff_t offset, void *buf, size_t len)
{
	return avm_prom_read(offset, buf, len);
}

static int __avm_prom_load_config_at_offsets(struct avm_prom_config_device *promdev,
					     const unsigned int *dev_offsets, size_t noffs)
{
	int i, ret;

	for (i = 0; i < noffs; i++) {
		loff_t off;

		if (promdev->ops->to_loff_t)
			off = promdev->ops->to_loff_t(promdev, dev_offsets[i]);
		else
			off = dev_offsets[i];
		ret = avm_prom_load_config_entry(off);

		if (ret < 0 && ret != -ENOENT) {
			pr_err("Error reading entry %d: %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}

#if defined(CONFIG_OF_AVM_DT_ENV)
static void __avm_prom_load_config_devicetree(struct work_struct *work)
{
	struct avm_prom_config_device *prom_dev;
	int count, i, ret;
	const __be32 *dev_offsets_be;
	u32 dev_offsets[AVM_PROM_MAX_TYPE];

	if (!of_have_populated_dt() || !of_chosen)
		pr_err("Don't have chosen node in DT\n");

	ret = avm_prom_detect_config_device();
	if (ret)
		return;

	prom_dev = avm_prom_config_device;
	BUG_ON(!prom_dev || !prom_dev->ops);

	dev_offsets_be = of_get_property(of_chosen, "wlan_dect_configs", &count);
	if (!dev_offsets_be) {
		pr_err("No wlan_dect_configs property in chosen node\n");
		return;
	}

	count /= sizeof(__be32);
	BUG_ON(count > ARRAY_SIZE(dev_offsets));

	for (i = 0; i < count; i++)
		dev_offsets[i] = be32_to_cpu(dev_offsets_be[i]);

	__avm_prom_load_config_at_offsets(prom_dev, dev_offsets, count);
}

static DECLARE_DELAYED_WORK(dev_init_dwork, __avm_prom_load_config_devicetree);

/**
 * Automatically read config offsets from the devicetree
 */
static int avm_prom_load_config_devicetree(void)
{
	schedule_delayed_work(&dev_init_dwork, 0);
	return 0;
}
late_initcall(avm_prom_load_config_devicetree);

static int prom_device_schedule_timeout(unsigned long msecs)
{
	schedule_delayed_work(&dev_init_dwork, msecs_to_jiffies(msecs));
	return -EAGAIN;
}
#else /* if !CONFIG_OF_AVM_DT_ENV */
static int prom_device_schedule_timeout(unsigned long msecs)
{
	msleep(msecs);
	return 0;
}

/*
 * Legacy setups, no devicetree
 */
static int __init avm_prom_load_config_at_offsets(struct avm_prom_config_device *promdev,
						  unsigned int *dev_offs)
{
	unsigned int offs[AVM_PROM_CONFIG_LEGACY_ENTRY_COUNT];
	int i;

	for (i = 0; i < ARRAY_SIZE(offs); ++i)
		offs[i] = dev_offs[i] & AVM_PROM_CONFIG_LEGACY_OFFSET_MASK;

	__avm_prom_load_config_at_offsets(promdev, offs, ARRAY_SIZE(offs));

	return 0;
}

static int __init avm_prom_load_config(void)
{
	int ret;

	/* We handle only legacy setups here */
	if (!avm_prom_config_offsets) {
		pr_info("No config offsets provided, skipping legacy setup\n");
		return 0;
	}

	ret = avm_prom_detect_config_device();
	if (ret)
		return ret;

	return avm_prom_load_config_at_offsets(avm_prom_config_device,
					       *avm_prom_config_offsets);
}
late_initcall(avm_prom_load_config);

static loff_t avm_prom_align(struct avm_prom_config_packed_locator *locat,
			     loff_t offset)
{
	if (locat->align_at)
		return ALIGN(offset, locat->align_at);

	return offset;
}

/*
 * Packed config data with locator info
 */
static int avm_prom_load_config_packed(struct avm_prom_config_device *promdev,
				       struct avm_prom_config_packed_locator *locat)
{
	enum avm_prom_config_type type;
	loff_t offset, end;

	BUG_ON(!locat->len);

	offset = locat->offset;
	end = locat->offset + locat->len;
	while (offset < end) {
		type = avm_prom_load_config_entry(offset);
		if (type == -ENOENT) {
			/* Skip entry */
			offset = avm_prom_align(locat, offset + 1);
			continue;
		} else if (type < 0) {
			pr_err("Error reading entry: %d\n", type);
			return type;
		}

		offset += configs[type].hdrsize + configs[type].size;
		if (configs[type].is_aligned)
			offset = avm_prom_align(locat, offset);
	}

	return 0;
}

int avm_prom_config_load_packed(struct avm_prom_config_device *prom_dev,
				struct avm_prom_config_packed_locator *locat)
{
	int ret;

	ret = avm_prom_config_add_device(prom_dev);
	if (ret)
		return ret;
	return avm_prom_load_config_packed(prom_dev, locat);
}

int avm_prom_config_load_mtd_packed(struct avm_prom_mtd_device *prom_mtd,
				    struct avm_prom_config_packed_locator *locat)
{
	int ret;

	ret = avm_prom_config_add_mtd_device(prom_mtd);
	if (ret)
		return ret;
	return avm_prom_load_config_packed(&prom_mtd->prom_dev, locat);
}

int avm_prom_config_load_blkdev_packed(struct avm_prom_block_device *prom_bdev,
				       struct avm_prom_config_packed_locator *locat)
{
	int ret;

	ret = avm_prom_config_add_blkdev(prom_bdev);
	if (ret)
		return ret;
	return avm_prom_load_config_packed(&prom_bdev->prom_dev, locat);
}
#endif /* !CONFIG_OF_AVM_DT_ENV */

static ssize_t avm_prom_get_config_raw_alloc(enum avm_prom_config_type type,
					     void **buf, size_t max_len)
{
	bool alloced_buffer = false;
	struct config_data *config;
	size_t len, pos;
	ssize_t ret;

	if (type >= AVM_PROM_MAX_TYPE)
		return -EINVAL;

	config = &configs[type];

	// We don't handle zip here
	if (config->has_zipped_src)
		return -EINVAL;

	if (!config->is_present)
		return -ENOENT;

	if (!*buf) {
		*buf = vmalloc(config->size);

		if (!*buf)
			return -ENOMEM;

		alloced_buffer = true;
		max_len = config->size;
	}

	len = min(config->size, max_len);

	for (pos = 0; pos < len; ) {
		ret = avm_prom_read(config->offset + pos, *buf + pos,
				len - pos);
		if (ret <= 0) {
			if (alloced_buffer)
				vfree(*buf);

			return ret ?: -EIO;
		}
		pos += ret;
	}

	// Zero out remaining space
	if (max_len > len)
		memset(*buf + len, 0, max_len - len);

	return config->size;
}

static ssize_t get_inflated_size(void *zipped_buf, size_t zipped_size,
		z_stream *stream)
{
	ssize_t ret;
	char tbuf[32];

	stream->next_in = zipped_buf;
	stream->avail_in = zipped_size;

	stream->next_out = tbuf;
	stream->avail_out = sizeof(tbuf);

	// Simulate decompression
	while ((ret = zlib_inflate(stream, Z_SYNC_FLUSH)) == Z_OK) {
		stream->next_out = tbuf;
		stream->avail_out = sizeof(tbuf);
	}

	if (ret != Z_STREAM_END)
		return -EIO;

	ret = stream->total_out;
	zlib_inflateReset(stream);

	return ret;
}

static ssize_t unzip_buffer(void **buf, size_t max_len,
		void *zipped_buf, size_t zipped_size)
{
	ssize_t ret;
	bool alloced_buffer = false;
	z_stream stream;

	stream.workspace = vmalloc(zlib_inflate_workspacesize());

	if (!stream.workspace)
		return -ENOMEM;

	ret = zlib_inflateInit(&stream);
	if (ret != Z_OK) {
		ret = -EIO;
		goto out_err_zlib_init;
	}

	// No buffer supplied, so guess the size
	if (!*buf) {
		ret = get_inflated_size(zipped_buf, zipped_size, &stream);

		if (ret < 0)
			goto out_err_zlib;

		// Get out the size and alloc the buffer
		max_len = ret;
		*buf = vmalloc(max_len);
		if (!*buf) {
			ret = -ENOMEM;
			goto out_err_zlib;
		}
		alloced_buffer = true;
	}

	stream.next_in = zipped_buf;
	stream.avail_in = zipped_size;

	stream.next_out = *buf;
	stream.avail_out = max_len;

	ret = zlib_inflate(&stream, Z_FINISH);
	if (ret == Z_OK) {
		zlib_inflateReset(&stream);
		ret = get_inflated_size(zipped_buf, zipped_size, &stream);
		// Either we hold error code or total size in ret here.
		goto out_err_zlib;
	}
	if (ret != Z_STREAM_END) {
		ret = -EIO;
		goto out_err_zlib;
	}

	// Store actual size of output
	ret = stream.total_out;

	// Zero out remainder
	if ((size_t)ret < max_len)
		memset(*buf + ret, 0, max_len - ret);

out_err_zlib:
	// We error-out and have allocated the buffer.
	if (ret < 0 && alloced_buffer) {
		vfree(*buf);
		*buf = NULL;
	}

	zlib_inflateEnd(&stream);

out_err_zlib_init:
	vfree(stream.workspace);

	return ret;
}

static ssize_t avm_prom_get_config_zip_alloc(enum avm_prom_config_type type,
					     void **buf, size_t max_len)
{
	struct config_data *config;
	enum avm_prom_config_type zipped_type;
	ssize_t ret, zipped_size;
	void *zipped_buf = NULL;

	if (type >= AVM_PROM_MAX_TYPE)
		return -EINVAL;

	config = &configs[type];

	// Not a zip file, so we can just use the raw read
	if (!config->has_zipped_src && !config->is_zipped)
		return -EINVAL;

	zipped_type = config->is_zipped ? type : config->zipped_src_type;

	// Read zipped buf into temporary buffer
	zipped_size = avm_prom_get_config_raw_alloc(zipped_type,
							 &zipped_buf, 0);
	if (zipped_size < 0)
		return zipped_size;

	ret = unzip_buffer(buf, max_len, zipped_buf, zipped_size);
	vfree(zipped_buf);

	return ret;
}

static ssize_t
avm_prom_get_config_internal_alloc(enum avm_prom_config_type type, void **buf,
				   size_t max_len)
{
	struct config_data *config;
	ssize_t ret;

	if (type >= AVM_PROM_MAX_TYPE)
		return -EINVAL;

	config = &configs[type];

	if (config->has_zipped_src || config->is_zipped)
		ret = avm_prom_get_config_zip_alloc(type, buf, max_len);
	else
		ret = avm_prom_get_config_raw_alloc(type, buf, max_len);

	return ret;
}

ssize_t avm_prom_get_config(enum avm_prom_config_type type, void *buf,
			    size_t max_len)
{
	void *_buf = buf;

	return avm_prom_get_config_internal_alloc(type, &_buf, max_len);
}
EXPORT_SYMBOL(avm_prom_get_config);

ssize_t avm_prom_get_config_alloc(enum avm_prom_config_type type, void **_buf)
{
	*_buf = NULL;

	return avm_prom_get_config_internal_alloc(type, _buf, 0);
}
EXPORT_SYMBOL(avm_prom_get_config_alloc);
