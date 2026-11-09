// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "[fw-info] " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <avm/enh/fw_info.h>

/* copied from module.c */
#ifdef CONFIG_DEBUG_SET_MODULE_RONX
# define debug_align(X) ALIGN(X, PAGE_SIZE)
#else
# define debug_align(X) (X)
#endif


static const char *fw_version;
static bool fw_is_internal;
static struct _avm_kernel_module_memory_config *fw_module_sizes;

#if IS_ENABLED(CONFIG_AVM_FW_INFO_EMBED)
static inline void *fw_info_embed_get(enum _avm_kernel_config_tags tag);
static inline void fw_info_embed_load(void);
#else
static inline void *fw_info_embed_get(enum _avm_kernel_config_tags tag)
{
	return NULL;
}
static inline void fw_info_embed_load(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AVM_FW_INFO_DT)
static inline void fw_info_dt_load(void);
#else
static inline void fw_info_dt_load(void)
{
}
#endif

const char *avm_fw_version(void)
{
	return fw_version;
}
EXPORT_SYMBOL(avm_fw_version);

bool avm_fw_is_internal(void)
{
	return fw_is_internal;
}
EXPORT_SYMBOL(avm_fw_is_internal);

struct _avm_kernel_module_memory_config *avm_fw_module_sizes(void)
{
	return fw_module_sizes;
}

struct _avm_kernel_urlader_env *avm_fw_urlader_env(void)
{
	return fw_info_embed_get(avm_kernel_config_tags_urlader_env);
}

static __init int fw_info_print_banner(void)
{
	const char *version = avm_fw_version();

	if (!version)
		version = "unknown";

	if (avm_fw_is_internal())
		pr_err("Version %s (internal only)\n", version);
	else
		pr_err("Version %s\n", version);

	return 0;
}

__init void avm_fw_info_init(void)
{
	// First load embedded info to allow DT to override values
	fw_info_embed_load();
	fw_info_dt_load();
	fw_info_print_banner();
}

#if IS_ENABLED(CONFIG_AVM_FW_INFO_EMBED_DTB)
void *avm_fw_embedded_dtb(void)
{
	return fw_info_embed_get(avm_kernel_config_tags_device_tree_subrev_0);
}
#endif

#if IS_ENABLED(CONFIG_AVM_FW_INFO_EMBED)
extern struct _avm_kernel_config *__avm_kernel_config_start;

static inline void *fw_info_embed_get(enum _avm_kernel_config_tags tag)
{
	struct _avm_kernel_config *p;

	p = __avm_kernel_config_start;

	if (!p)
		return NULL;

	for (; p->tag < avm_kernel_config_tags_last && p->config; p++) {
		if (p->tag == tag)
			return p->config;
	}

	return NULL;
}

static inline void fw_info_embed_load(void)
{
	struct _avm_kernel_version_info *version;

	version = fw_info_embed_get(avm_kernel_config_tags_version_info);
	if (version) {
		fw_version = version->firmwarestring;

		// 'M' means modified as in an private internal build
		if (fw_version[strlen(fw_version) - 1] == 'M')
			fw_is_internal = true;
	}

	fw_module_sizes =
		fw_info_embed_get(avm_kernel_config_tags_module_memory);
}
#endif

#if IS_ENABLED(CONFIG_AVM_FW_INFO_DT)
static void *early_kzalloc(u64 size)
{
	void *ptr;

	ptr = early_init_dt_alloc_memory_arch(size, 4);
	if (!ptr)
		return NULL;

	memset(ptr, 0, size);

	return ptr;
}

static inline void fw_info_dt_load(void)
{
	struct device_node *node, *module_node, *np;
	int module_count, i;

	node = of_find_compatible_node(NULL, NULL, "avm,fw-info");

	if (!node)
		return;

	of_property_read_string(node, "version", &fw_version);

	if (of_property_read_bool(node, "is-internal"))
		fw_is_internal = true;

	module_node = of_get_child_by_name(node, "module-sizes");
	if (!module_node)
		return;

	module_count = of_get_child_count(module_node);
	if (module_count <= 0)
		return;

	fw_module_sizes =
		early_kzalloc((module_count + 1) *
			      sizeof(struct _avm_kernel_module_memory_config));
	if (!fw_module_sizes)
		panic("[fw-info] Could not allocate memory for module sizes\n");

	i = 0;
	for_each_child_of_node(module_node, np) {
		u32 size, num;
		struct property *prop;
		const __be32 *p;
		u32 core_size;

		fw_module_sizes[i].name = np->name;

		of_property_for_each_u32(np, "core-size", prop, p, core_size) {
			fw_module_sizes[i].core_size = debug_align(fw_module_sizes[i].core_size + core_size);

		}

		of_property_read_u32_index(np, "core-symbols", 0, &num);
		of_property_read_u32_index(np, "core-symbols", 1, &size);
		fw_module_sizes[i].symbol_text_size =
			size + num * sizeof(Elf_Sym);

		of_property_read_u32_index(np, "all-symbols", 0, &num);
		of_property_read_u32_index(np, "all-symbols", 1, &size);
		fw_module_sizes[i].symbol_size = size + num * sizeof(Elf_Sym);

		i++;
	}
}
#endif
