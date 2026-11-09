// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>

#include <avm/enh/fw_info.h>

/*
 * This file contains a fallback to the legacy avm_kernel_* api if available.
 *
 * To easy the migration to fw_info this can be used.
 */

/*
 * Declare the external symbols needed. Don't use a header here to avoid
 * introducing a dependency on the actual kernel.
 */
extern struct _avm_kernel_module_memory_config *avm_kernel_module_memory_config;
extern struct _avm_kernel_urlader_env *avm_kernel_urlader_env;

/*
 * Define these symbols as weak and initialize them with NULL.
 *
 * Some kernels did not support avm_kernel_* at all (like puma6) but won't use
 * the fw_info mechanism. So define weak symbols to provide an empty api.
 */
__weak struct _avm_kernel_module_memory_config *avm_kernel_module_memory_config;
__weak struct _avm_kernel_urlader_env *avm_kernel_urlader_env;
__weak struct avm_firmware_build_info *avm_fw_build_info;

__init void avm_fw_info_init(void)
{
}

struct _avm_kernel_module_memory_config *avm_fw_module_sizes(void)
{
	return avm_kernel_module_memory_config;
}

struct _avm_kernel_urlader_env *avm_fw_urlader_env(void)
{
	return avm_kernel_urlader_env;
}

static __init int avm_no_fw_info_init(void)
{
	avm_fw_info_print_banner();
	return 0;
}
postcore_initcall(avm_no_fw_info_init);
