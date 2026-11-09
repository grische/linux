// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "[fw-info] " fmt

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

#include <avm/enh/fw_info.h>

/* copied from module.c */
# define debug_align(X) ALIGN(X, PAGE_SIZE)

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
#include <linux/bootmem.h>
static void *early_init_dt_alloc_memory_arch(u64 size, u64 align)
{
	return memblock_virt_alloc(size, align);
}
#endif

struct avm_firmware_build_info *avm_fw_build_info;
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

	if (avm_fw_buildnumber() == NULL)
		pr_err("error: no buildnumber exists - Version %s\n", version);
	else
		pr_info("Version %s buildnr=%s buildtype=%d%s%s\n",
			version, avm_fw_buildnumber(),
			avm_fw_buildtype(),
			avm_fw_is_internal() ? " internal" : "",
			avm_fw_is_modified() ? " modified" : "");
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
	avm_fw_build_info = fw_info_embed_get(avm_kernel_config_tags_version_info);

	fw_module_sizes =
		fw_info_embed_get(avm_kernel_config_tags_module_memory);
}

#define RELOCATED(x, offset) ((void *)((long)x + offset))

static void relocate_module_memory(struct _avm_kernel_module_memory_config *config,
				   unsigned long offset)
{
	for (; config->name; config++)
		config->name = RELOCATED(config->name, offset);
}

__init void avm_fw_info_embed_relocate(unsigned long offset)
{

	struct _avm_kernel_config *p_orig, *p_new;
	void **root_ptr_new;

	p_orig = __avm_kernel_config_start;

	if (!p_orig)
		return;

	p_new = RELOCATED(p_orig, offset);
	root_ptr_new = RELOCATED(&__avm_kernel_config_start, offset);
	*root_ptr_new = p_new;

	for (; p_orig->tag < avm_kernel_config_tags_last && p_orig->config; p_orig++, p_new++) {
		p_new->config = RELOCATED(p_orig->config, offset);

		switch(p_new->tag) {
		case avm_kernel_config_tags_module_memory:
			relocate_module_memory(p_new->config, offset);
			break;
		default:
			break;
		}
	}
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
	static struct avm_firmware_build_info bi;
	const char *str = NULL;
	unsigned int tmp;
	struct device_node *node, *module_node, *np;
	int module_count, i;

	node = of_find_compatible_node(NULL, NULL, "avm,fw-info");

	if (!node)
		return;

	if (of_property_read_string(node, "version", &str) == 0)
		strlcpy(bi.firmwarestring, str,
			sizeof(avm_fw_build_info->firmwarestring));

	if (of_property_read_string(node, "buildnumber", &str) == 0)
		strlcpy(bi.buildnumber, str,
			sizeof(bi.buildnumber));

	if (of_property_read_u32(node, "buildtype",  &tmp) == 0)
		bi.buildtype = tmp;

	if (of_property_read_bool(node, "builddirty"))
		bi.builddirty = true;

	avm_fw_build_info = &bi;

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
