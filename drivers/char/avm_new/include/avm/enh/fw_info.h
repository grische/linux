/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/module.h>

#ifndef __AVM_ENH_FW_INFO_H__
#define __AVM_ENH_FW_INFO_H__

#include <uapi/avm/enh/fw_info.h>

/*
 * Functions of fw_info can be called during early boot after the dt is present.
 *
 * The only exception is avm_fw_embedded_dtb as this does not use the DT and can
 * be called before.
 */

extern __init void avm_fw_info_init(void);
extern __init int avm_fw_info_print_banner(void);

enum avm_fw_buildtype {
	avm_fw_buildtype_unknown  = -1,
	avm_fw_buildtype_Private  = 998,
	avm_fw_buildtype_Intern   = 2,
	avm_fw_buildtype_Inhaus   = 1000,
	avm_fw_buildtype_LabBETA  = 1001,
	avm_fw_buildtype_LabPHONE = 1004,
	avm_fw_buildtype_Lab      = 1005,
	avm_fw_buildtype_LabTEST  = 1006,
	avm_fw_buildtype_LabPLUS  = 1007,
	avm_fw_buildtype_Release  = 1
};
extern struct avm_firmware_build_info *avm_fw_build_info;

/**
 * Returns a string describing the firmware version.
 *
 * May return NULL if the version can not be determined
 */
static inline const char *avm_fw_version(void)
{
	if (!avm_fw_build_info)
		return NULL;

	return avm_fw_build_info->firmwarestring;
}

/*
 * Returns true when this FW is internal and can contain
 * sensitive debug functionality
 */
static inline bool avm_fw_is_internal(void)
{
	if (!avm_fw_build_info)
		return false;

	switch (avm_fw_build_info->buildtype) {
	case avm_fw_buildtype_Private:
	case avm_fw_buildtype_Intern:
		return true;
	default:
		return false;
	}
	return false;
}

 /*
  * Returns the buildnumber.
  *
  * May return NULL if the buildnumber can not be determined
  */
static inline const char *avm_fw_buildnumber(void)
{
	if (!avm_fw_build_info)
		return NULL;
	return avm_fw_build_info->buildnumber;
}
/**
 * Returns true when this FW is "modified" and the build
 * cannot be considered reproducible.
 *
 * In particular, this is the case if the GU being used to
 * build the FW had uncommitted changes, e.g. caused by a local
 * build of a firmware component.
 */
static inline bool avm_fw_is_modified(void)
{
	if (!avm_fw_build_info)
		return false;

	return avm_fw_build_info->builddirty;
}

/**
 *
 * May return < 0 -> not supported
 */
static inline int avm_fw_buildtype(void)
{
	if (!avm_fw_build_info)
		return avm_fw_buildtype_unknown;

	return avm_fw_build_info->buildtype;
}

#if IS_ENABLED(CONFIG_AVM_PROM_ENVIRONMENT)
extern void avm_get_firmware_info(struct avm_firmware_info *fwinfo,
				  unsigned int fwinfo_version);
#define AVM_FIRMWARE_INFO_SIZE	200
size_t avm_snprint_firmware_info(const struct avm_firmware_info *fwinfo,
				 char *str, size_t size);
size_t avm_snprint_firmware_info_kernel(const struct avm_firmware_info *fwinfo,
					char *str, size_t size);
#endif

extern struct _avm_kernel_module_memory_config *avm_fw_module_sizes(void);

/*
 * This is used for kernels where the environment is baked into the
 * firmware.
 *
 * Usually the environment is provided by the urlader via DT oder as a
 * plain kv-array. However one some boxes the urlader can not provide an
 * environment so this is bundled with the firmware and extracted here.
 *
 * The only FW currently using this functionality is HW238, the offload
 * scorpion on the 7490.
 */
extern struct _avm_kernel_urlader_env *avm_fw_urlader_env(void);

#if IS_ENABLED(CONFIG_AVM_FW_INFO_EMBED_DTB)
/*
 * Returns a pointer to the embedded binary device tree blob.
 *
 * May return NULL if no dtb is available.
 * NOTE: This can be called before avm_fw_info_init
 */
extern void *avm_fw_embedded_dtb(void);
#else
static inline void *avm_fw_embedded_dtb(void)
{
	return NULL;
}
#endif
#if IS_ENABLED(CONFIG_AVM_FW_INFO_EMBED)
extern void avm_fw_info_embed_relocate(unsigned long offset);
#else
static inline void avm_fw_info_embed_relocate(unsigned long offset) {}
#endif

#endif
