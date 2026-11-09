// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "[fw-info] " fmt

#include <avm/enh/fw_info.h>
#ifdef CONFIG_AVM_PROM_ENVIRONMENT
#include <linux/env.h>
#else
#define prom_getenv(var) NULL
#endif

void avm_get_firmware_info(struct avm_firmware_info *fwinfo, unsigned int fwinfo_version)
{
	const char *s;

	*fwinfo = (struct avm_firmware_info) {
		.fwinfo_version = fwinfo_version,
		.src = AVM_FWINFO_SOURCE_CURRENTFW,
	};

	if (fwinfo_version < AVM_FWINFO_VERSION(2, 1))
		return;

	s = avm_fw_version() ?: prom_getenv("firmware_info") ?: "";
	strlcpy(fwinfo->fw_version, s, sizeof(fwinfo->fw_version));

	s = prom_getenv("HWRevision") ?: "?";
	strlcpy(fwinfo->hw_revision, s, sizeof(fwinfo->hw_revision));

	s = prom_getenv("HWSubRevision") ?: "0";
	strlcpy(fwinfo->hw_subrevision, s, sizeof(fwinfo->hw_subrevision));

	s = prom_getenv("bootloaderVersion") ?: "0";
	strlcpy(fwinfo->bootloader_version, s, sizeof(fwinfo->bootloader_version));

	fwinfo->build = avm_fw_build_info;
}

__init int avm_fw_info_print_banner(void)
{
	const char *version = avm_fw_version();

	if (!version)
		version = "unknown";

	/*
	 * Note, Hochsee parses the fw_info string, so any formatting changes
	 * need be orchestrated.
	 */
	if (avm_fw_buildnumber() == NULL)
		pr_err("Version %s (no buildnumber found)\n", version);
	else
		pr_err("Version %s buildnr=%s buildtype=%d%s%s\n",
			version, avm_fw_buildnumber(),
			avm_fw_buildtype(),
			avm_fw_is_internal() ? " internal" : "",
			avm_fw_is_modified() ? " modified" : "");
	return 0;
}

static size_t size_avail(const char *pos, const char *end)
{
	if (pos > end)
		return 0;
	return end - pos;
}

static char *firmware_info_remove_appendix(char *dest, size_t size, const char *src)
{
	strlcpy(dest, src, size);
	return strsep(&dest, "-");
}

size_t avm_snprint_firmware_info(const struct avm_firmware_info *fwinfo,
				 char *str, size_t size)
{
	char fw_short[128];
	const struct avm_firmware_build_info *bi = fwinfo->build;
	char *s, *end;

	if (!size)
		return 0;

	s = str;
	end = str + size;

	s += snprintf(s, size, "Log-Version: %u.%u\n",
			fwinfo->fwinfo_version >> 8,
			fwinfo->fwinfo_version & 0xff);
	size = size_avail(s, end);

	if (bi)
		s += snprintf(s, size,
				"%s: %s\n%s: %s\n%s: %u\n%s: %u\n",
				"Buildnumber", bi->buildnumber,
				"Firmware", firmware_info_remove_appendix(fw_short,
									  sizeof(fw_short),
									  bi->firmwarestring),
				"Buildtype", bi->buildtype,
				"Builddirty", bi->builddirty);

	return s - str;
}

size_t avm_snprint_firmware_info_kernel(const struct avm_firmware_info *fwinfo,
					char *str, size_t size)
{
	unsigned int fwinfo_version = fwinfo->fwinfo_version;
	char *s, *end;

	if (!size)
		return 0;

	s = str;
	end = str + size;

	if (fwinfo_version >= AVM_FWINFO_VERSION(2, 1)) {
		const char *fw_version_note = "";

		if (fwinfo_version >= AVM_FWINFO_VERSION(3, 0) &&
				fwinfo->src == AVM_FWINFO_SOURCE_CURRENTFW)
			fw_version_note = " (not from mailbox)";

		s += snprintf(s, size,
				"HW: %s.%s\n"
				"FW: %s%s\n"
				"Bootloader: %s\n",
				 fwinfo->hw_revision, fwinfo->hw_subrevision,
				 fwinfo->fw_version, fw_version_note,
				 fwinfo->bootloader_version);
		size = size_avail(s, end);
	}

	s += avm_snprint_firmware_info(fwinfo, s, size);

	return s - str;
}
