// SPDX-License-Identifier: GPL-2.0

#ifndef __AVM_PROM_CONFIG_H__
#define __AVM_PROM_CONFIG_H__

#include <linux/module.h>

struct mtd_info;
struct block_device;

enum avm_prom_config_type {
	WLAN = 0,
	DECT = 1,
	WLAN2 = 2,
	ZERTIFIKATE = 3,
	DOCSIS = 4,
	DSL = 5,
	PROLIFIC = 6,
	WLAN_ZIP = 7,
	WLAN2_ZIP = 8,
	ZERTIFIKATE2 = 9,
	ZERTIFIKATE3 = 10,
	ZERTIFIKATE4 = 11,
	FINAL = 12,
	PRODCERT1 = 13,
	PRODCERT2 = 14,
	PRODCERT3 = 15,
	PRODCERT4 = 16,
	PRODENV = 17,
	WLAN3_ZIP = 18,
	WLAN3 = 19,
	AVMZERTIFIKATE = 20,
	LRWPAN = 21,
	AVM_PROM_NUM_TYPES
};

extern const char *const avm_prom_config_type_name[AVM_PROM_NUM_TYPES];

struct avm_prom_config_device;
struct avm_prom_mtd_device;
struct avm_prom_block_device;

/**
 * Set the prom config storage device.
 *
 * @param prom_dev	The device
 * @return 0 on success
 */
extern int avm_prom_config_add_device(struct avm_prom_config_device *prom_dev);

/**
 * Set an mtd device as the prom config storage device.
 *
 * @param prom_mtd	The prom mtd device
 * @return 0 on success
 */
extern int avm_prom_config_add_mtd_device(struct avm_prom_mtd_device *prom_mtd);

/**
 * Set a block device as the prom config storage device.
 *
 * @param prom_bdev	The prom block device
 * @return 0 on success
 */
extern int avm_prom_config_add_blkdev(struct avm_prom_block_device *prom_bdev);

struct avm_prom_config_device_ops {
	ssize_t (*read)(struct avm_prom_config_device *prom_dev,
			loff_t offset, void *buf, size_t len);
	loff_t (*lba_to_ofs)(struct avm_prom_config_device *prom_dev, u32 lba);
};

struct avm_prom_config_loc {
	u32	offset;
	u32	len;
	u32	align_at;
};

struct avm_prom_config_device {
	const struct avm_prom_config_device_ops *ops;
	struct avm_prom_config_loc loc;
};

struct avm_prom_mtd_device {
	struct avm_prom_config_device prom_dev;
	struct mtd_info *mtd;
};

struct avm_prom_block_device {
	struct avm_prom_config_device prom_dev;
	struct block_device *bdev;
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
 * Load one config entry header at the specified offset.
 *
 * @note This only loads meta data and creates a procfs file for the entry.
 * @param offset Offset into the mtd/block device
 * @return 0 on success
 */
extern int avm_prom_load_config_entry(loff_t offset);

/**
 * Load all config entries.
 *
 * @note This only loads meta data and creates procfs files for the entries.
 * @return 0 on success
 */
extern int avm_prom_load_config_entries(void);

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
