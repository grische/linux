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

/*
 * Returns a string describing the firmware version.
 *
 * May return NULL if the version can not be determined
 */
extern const char *avm_fw_version(void);

/*
 * Returns true when this FW is internal and can contain
 * sensitive debug functionality
 */
extern bool avm_fw_is_internal(void);

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

#endif
