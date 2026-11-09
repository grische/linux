// SPDX-License-Identifier: GPL-2.0

#ifndef __AVM_PROM_CONFIG_H__
#define __AVM_PROM_CONFIG_H__

#include <linux/types.h>
#include <uapi/avm/enh/prom_uapi.h>
/* including legacy to both include old names and avm_prom_config_type */
#include <avm/enh/prom_config_legacy.h>

struct mtd_info;
struct block_device;

#define AVM_PROM_CONFIG_NONE 0xff

extern const char *const avm_prom_config_type_name[AVM_PROM_MAX_TYPE];

/* For legacy devices w/o devicetree: */
#define AVM_PROM_CONFIG_LEGACY_ENTRY_COUNT 8
#define AVM_PROM_CONFIG_LEGACY_OFFSET_MASK 0x1ffff
extern
unsigned int (*avm_prom_config_offsets)[AVM_PROM_CONFIG_LEGACY_ENTRY_COUNT] __initdata;

struct avm_prom_config_packed_locator {
	u32	offset;
	u32	len;
	u32	align_at;
};

struct avm_prom_config_device {
	const struct avm_prom_config_device_ops *ops;
};

struct avm_prom_mtd_device {
	struct avm_prom_config_device prom_dev;
	struct mtd_info *mtd;
};

struct avm_prom_block_device {
	struct avm_prom_config_device prom_dev;
	struct block_device *bdev;
};

struct avm_prom_config_device_ops {
	ssize_t (*read)(struct avm_prom_config_device *prom_dev,
			loff_t offset, void *buf, size_t len);
	loff_t (*to_loff_t)(struct avm_prom_config_device *prom_dev, u32 dev_offset);
};

/**
 * Raw reading of the config storage device.
 *
 * @param offset	The offset to read from.
 * @param buf		The buffer to read into.
 * @param len		The number of bytes to read.
 * @return		The number of bytes read if successful,
 *			else a negative error code
 */
extern ssize_t avm_prom_raw_read_config(loff_t offset, void *buf, size_t len);

/**
 * Load packed config data from a generic device at given location.
 *
 * @param prom_mtd	The prom mtd device
 * @param locat         Location info
 * @return 0 on success
 */
extern int avm_prom_config_load_packed(struct avm_prom_config_device *prom_dev,
				       struct avm_prom_config_packed_locator *locat);

/**
 * Load packed config data from mtd device at given location.
 *
 * @param prom_mtd	The prom mtd device
 * @param locat         Location info
 * @return 0 on success
 */
extern int avm_prom_config_load_mtd_packed(struct avm_prom_mtd_device *prom_mtd,
					   struct avm_prom_config_packed_locator *locat);

/**
 * Load packed config data from block device at given location.
 *
 * @param prom_bdev	The prom block device
 * @param locat         Location info
 * @return 0 on success
 */
extern int avm_prom_config_load_blkdev_packed(struct avm_prom_block_device *prom_bdev,
					      struct avm_prom_config_packed_locator *locat);

/**
 * Checks if a config with the given type is available.
 *
 * This checks if a config has been made available, but not if it can
 * be loaded correctly.
 *
 * @param type Config type to check
 * @return 1 If config is available, otherwise 0
 */
extern int avm_prom_is_config_available(enum avm_prom_config_type type);

/**
 * Retrieve the config into a presupplied buffer.
 *
 * A config that is to short will be padded with zeros and a long config will be truncated.
 *
 * @param type Config type to retrieve
 * @param buf Buffer to store the config into
 * @param max_len Size of buf
 * @return > 0 as actual config len
 * @return -ENOENT for no config
 * @return others for fatal errors
 */
extern ssize_t avm_prom_get_config(enum avm_prom_config_type type, void *buf, size_t max_len);


/**
 * Retrieve the config into a allocated buffer.
 *
 * Writes a pointer to buf with an vmalloced buffer containing the config. This buffer
 * must be released with vfree.
 *
 * @param type Config type to retrieve
 * @param buf Pointer to a void* pointer where the buffer will be writen
 * @return > 0 as actual config len
 * @return -ENOENT for no config
 * @return others for fatal errors
 */
extern ssize_t avm_prom_get_config_alloc(enum avm_prom_config_type type, void **buf);

#endif
