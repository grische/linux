// SPDX-License-Identifier: GPL-2.0+

/** @file avm_reboot_status.c
 *
 * mbahr:
 * 1.) hold and get reboot-status after Soft/NMI-Reboot
 * 2.) handle die-notifier
 * 3.) handle panic-notifier
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmsg_dump.h>
#include <linux/pm.h>
#include <linux/notifier.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
#include <linux/panic_notifier.h>
#endif
#include <linux/reboot.h>
#include <linux/kdebug.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/nmi.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#ifdef CONFIG_AVM_QTI_REBOOT_INFO
#include <linux/qcom_scm.h>
#endif
#include <avm/tffs/tffs.h>
#include <uapi/avm/enh/fw_info.h>
#include <avm/enh/reboot_status.h>
#include <avm/sammel/simple_proc.h>
#include <avm/sammel/helpers.h>
#include <avm/compat/timer.h>
#include <avm/compat/misc.h>


#if defined(CONFIG_AVM_FASTIRQ)
#if defined(CONFIG_AVM_GENERIC_RTE)
#include <avm/rte/rte.h>
#elif defined(CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON)
#include <asm/avm_enh/avm_fiq_os.h>
#else
#include <mach/avm_fiq_os.h>
#endif
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/
#include "avm_sammel.h"

#define MAILBOX_SIZE 512U

#if defined(CONFIG_SOC_GRX500)
#include "platform_grx.h"
#endif /*--- #if defined(CONFIG_SOC_GRX500) ---*/

#if defined(CONFIG_MACH_PUMA7)
#if defined(CONFIG_X86_PUMA7)
#include "platform_puma7x.h"
#else
#include "platform_puma7.h"
#endif

#endif

#if defined(CONFIG_MACH_BCM963138) || defined(CONFIG_BCM963178) || defined(CONFIG_BCM96764) || defined(CONFIG_BCM96766)
#include "platform_brcma.h"
#endif /*--- #if defined(CONFIG_MACH_BCM963138) || defined(CONFIG_BCM963178) ---*/

#if defined(CONFIG_ARCH_QCOM)
#include "platform_ipq.h"
#endif

#if defined(CONFIG_ARCH_ECONET)
#include "platform_airoha01.h"
#endif

#if defined(CONFIG_SOC_AR724X) || defined(CONFIG_SOC_AR934X) ||                \
	defined(CONFIG_SOC_QCA955X) || defined(CONFIG_SOC_QCA953X) ||          \
	defined(CONFIG_SOC_QCA956X)
#include "platform_scrpn.h"
#endif /*--- #if defined(CONFIG_SOC_AR724X) || defined(CONFIG_SOC_AR934X) || defined(CONFIG_SOC_QCA955X) || defined(CONFIG_SOC_QCA953X) || defined(CONFIG_SOC_QCA956X)  ---*/

#include <avm/enh/fw_info.h>

#define DEBUG_MAILBOX 0

static struct avm_reboot_info current_reboot_info = {
	.reboot_status = RS_POWERON,
};
static struct avm_reboot_info next_reboot_info;
static struct avm_firmware_build_info reboot_buildinfo; /* parsed from mailbox */

/**
 * \brief Deliver last reboot-status
 */
enum _avm_reset_status avm_reset_status(void)
{
	return current_reboot_info.reboot_status;
}
EXPORT_SYMBOL(avm_reset_status);

enum _avm_reset_status avm_next_reset_status(void)
{
	return next_reboot_info.reboot_status;
}
EXPORT_SYMBOL(avm_next_reset_status);

/*
 * Mailbox string helpers
 */
#define __repeat3x(s) s s s
/*
 * "Simple" mailbox strings are zero-terminated. The AVM Urlader knows about
 * these, so we cannot change these strings.
 */
#define __mbx_string_simple(s) __repeat3x(s)
/*
 * "Extended" mailbox strings extend beyond the first NUL character.
 * The Urlader stops parsing at the first NUL; so, everything after that is
 * internal to the kernel.
 */
#define __mbx_string_ext(s, x) \
	__mbx_string_simple(s) "\0(" x ")"

struct mailbox_strings {
	const u8 *str;
	size_t strlen;
	const char *cntr;
};
#define _mbx_str_init(s, c) { s, sizeof(s), c }

/* Header file for target box must define MAILBOX_STRING_YEAR */
#ifndef MAILBOX_STRING_YEAR
#error MAILBOX_STRING_YEAR not defined for target box!
#endif

#define mbx_str_init(_mbs, _mbcntr) _mbx_str_init(						\
	__mbx_string_simple("(c) AVM " MAILBOX_STRING_YEAR ", Reboot Status is: " _mbs),	\
	_mbcntr)
#define mbx_str_init_ext(_mbs, _mbs_ext, _mbcntr) _mbx_str_init(				\
	__mbx_string_ext("(c) AVM " MAILBOX_STRING_YEAR ", Reboot Status is: " _mbs, _mbs_ext),	\
	_mbcntr)

struct _reboot_info {
	const enum _avm_reset_status status;
	const char *status_name;
	const struct mailbox_strings mbs;
	/* Historical sysctl status names for reads and writes can differ. */
	const char *sysctl_name_read;
	const char *sysctl_name_write;	/* NULL if status is r/o */
	unsigned int reboot_count;
	unsigned int live_count;	/* for other events, reset on each boot */
#define RI_PROVISIONAL		BIT(0)
#define RI_PANICKING		BIT(1)
#define RI_CLEARALL_ON_BOOTUP	BIT(2)
#define RI_CNT_ON_BOOTUP	BIT(3)
#define RI_OTHER_EVENT		BIT(4)
#define RI_RESET_ON_BOOTUP	BIT(5)
	unsigned int flags;
};

enum reboot_array_format {
	mailbox_format,
	irregularreboot_format,	/* format for "Irregular-Reboots" */
	line_format		/* format for "Line by Line", the keyword is sysctl_name_read */
};

#define REBOOT_INFO(_status, _status_name, _sysctl_rd, _sysctl_wr, \
		    _mbs, _mbcntr, _flags) {	\
	.status = _status,			\
	.status_name = _status_name,		\
	.sysctl_name_read = _sysctl_rd,		\
	.sysctl_name_write = _sysctl_wr,	\
	.mbs = mbx_str_init(_mbs, _mbcntr),	\
	.flags = (_flags),						\
}

#define REBOOT_INFO_EXT(_status, _status_name, _sysctl_rd, _sysctl_wr,	\
			_mbs, _mbs_ext, _mbcntr, _flags) {		\
	.status = _status,						\
	.status_name = _status_name,					\
	.sysctl_name_read = _sysctl_rd,					\
	.sysctl_name_write = _sysctl_wr,				\
	.mbs = mbx_str_init_ext(_mbs, _mbs_ext, _mbcntr),		\
	.flags = (_flags),						\
}

#define REBOOT_INFO_NOMBX(_status, _status_name, _sysctl_rd, _sysctl_wr, _flags) {	\
	.status = _status,								\
	.status_name = _status_name,							\
	.sysctl_name_read = _sysctl_rd,							\
	.sysctl_name_write = _sysctl_wr,						\
	.flags = (_flags),								\
}

// clang-format off
static struct _reboot_info reboot_info[] = {
	/*              status                status_name              sysctl_name_read,     sysctl_name_write,  mbx_status_name,     mbx_status_ext,      mbx_cntr,           flags */
	REBOOT_INFO_EXT(RS_PANIC,	      "Soft-Reboot",           "panic",              NULL,               "Software-Reboot",    "PANIC",             "PANIC",           RI_PANICKING),
	REBOOT_INFO_EXT(RS_OOM,               "Soft-Reboot",           "oom",                NULL,               "Software-Reboot",    "OOM",               "OOM",             RI_PANICKING),
	REBOOT_INFO_EXT(RS_OOPS,              "Soft-Reboot",           "oops",               NULL,               "Software-Reboot",    "OOPS",              "KCRASH",          RI_PANICKING),
	REBOOT_INFO_EXT(RS_BUSERROR,          "Soft-Reboot",           "buserror-reboot",    "buserror",         "Software-Reboot",    "BE",                "BE",              0),
	REBOOT_INFO_EXT(RS_DOCSIS_LOCAL,      "Soft-Reboot",           "docsis-local",       "docsis-local",     "Software-Reboot",    "DOCSIS_LOCAL",      "DOCSIS_LOCAL",    0),
	REBOOT_INFO_EXT(RS_DOCSIS_OPERATOR,   "Soft-Reboot",           "docsis-operator",    "docsis-operator",  "Software-Reboot",    "DOCSIS_OPERATOR",   "DOCSIS_OPERATOR", 0),
	REBOOT_INFO_EXT(RS_BOXCHANGE,         "Soft-Reboot",           "boxchange",          "boxchange",        "Software-Reboot",    "BOXCHANGE",         "BOXCHANGE",       0),
	REBOOT_INFO_EXT(RS_OPERATOR,          "Soft-Reboot",           "operator",           "operator",         "Software-Reboot",    "OPERATOR",          "OPERATOR",        0),
	/*              status                status_name              sysctl_name_read,     sysctl_name_write,  mbx_status_name,      mbx_cntr,            flags */
	REBOOT_INFO    (RS_SOFTWATCHDOG,      "Softwatchdog-Reboot",   "software",           NULL,               "Software-Watchdog",  "WD",                RI_PANICKING),
	REBOOT_INFO    (RS_NMIWATCHDOG,       "NMI-Watchdog-Reset",    "watchdog",           NULL,               "NMI-Watchdog",       "NMI",               RI_PANICKING),
	REBOOT_INFO    (RS_SHORTREBOOT,       "Short-PowerOff-Reboot", "short-reboot",       NULL,               "Power-On-Reboot",    "SHORTPOWERCUT",     RI_PROVISIONAL | RI_CNT_ON_BOOTUP),
	REBOOT_INFO    (RS_TEMP_REBOOT,       "Temperature-Reboot",    "temperature-reboot", "temperature",      "Temperature-Reboot", "TEMPERATURE",       0),
	REBOOT_INFO    (RS_FIRMWAREUPDATE,    "Fw-Update",             "firmware_update",    "FW-Update",        "Firmware-Update",    NULL,                RI_CLEARALL_ON_BOOTUP),
	/*              status                status_name              sysctl_name_read,     sysctl_name_write,  mbx_status_name,      mbx_cntr,	    flags */
	REBOOT_INFO    (RS_DYING_GASP,        "Soft-Reboot",           "dying-gasp",         NULL,               "Software-Reboot",    "DG",		    RI_PROVISIONAL | RI_OTHER_EVENT | RI_RESET_ON_BOOTUP),
	/*--- als vorletzter Eintrag, da dieser Untermenge von RS_PANIC/RS_OOM/RS_OOPS ---*/
	REBOOT_INFO    (RS_REBOOT,            "Soft-Reboot",           "reboot",             "reboot",           "Software-Reboot",    NULL,                RI_CLEARALL_ON_BOOTUP),
	/*                status              status_name              sysctl_name_read,     sysctl_name_write,  flags */
	/*--- definition: have to be last entry: ---*/
	REBOOT_INFO_NOMBX(RS_POWERON,         "Power-On",              "poweron",            NULL,		 RI_PROVISIONAL),
};
// clang-format on

static struct _reboot_info *get_reboot_info_struct(enum _avm_reset_status status)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++)
		if (reboot_info[i].status == status)
			return &reboot_info[i];
	return NULL;
}

int avm_sysctl_set_reset_status(const char *status)
{
	const struct _reboot_info *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(reboot_info); ++i) {
		ri = &reboot_info[i];

		if (!ri->sysctl_name_write)
			continue;
		if (!strcmp(status, ri->sysctl_name_write))
			goto found;
	}
	return -EINVAL;
found:
	avm_set_reset_status(ri->status);
	return 0;
}

const char *avm_sysctl_reset_status(void)
{
	const struct _reboot_info *ri = get_reboot_info_struct(avm_reset_status());

	BUG_ON(!ri);
	return ri->sysctl_name_read;
}

static int mailbox_get_value(const char *mbox, const char *mbkey,
			     char *buf, size_t bufsiz)
{
	const char *s, *e;
	size_t len;

	if (!bufsiz)
		return 0;

	s = mbox;
	len = strlen(mbkey);
	while ((s = strstr(s, mbkey))) {
		bool match;

		/* Only match complete words */
		match = (s == mbox || s[-1] == ')') && s[len] == '(';
		s += len+1;
		if (match)
			goto foundkey;
	}
	return -ENOENT;
foundkey:
	e = strnchr(s, bufsiz, ')');
	if (!e)
		return -EILSEQ;

	strlcpy(buf, s, e-s+1);

	return 0;
}

/**
 */
static void read_reboot_counters(const char *mbox)
{
	unsigned int i;
	char val[32];
	int err;

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		unsigned int counter = 0;
		const char *mbkey;

		mbkey = reboot_info[i].mbs.cntr;
		if (!mbkey)
			continue;

		err = mailbox_get_value(mbox, mbkey, val, sizeof(val));
		if (err)
			continue;

		sscanf(val, "%u", &counter);
		reboot_info[i].reboot_count = counter;

		if ((reboot_info[i].flags & RI_OTHER_EVENT) &&
		    (reboot_info[i].flags & RI_RESET_ON_BOOTUP) == 0)
			reboot_info[i].live_count = counter;
	}

}

/**
 * read uptime and fw-info from mailbox
 * format UP(<time_sec>)UTC(<time>)FW(<version>)HW(<version>)HWS(<subversion>)BV(<version>)BN(<buildnumber>)FS(<firmwarestring>)BT(<buildtype>)BD(<builddirty)
 */
static void mailbox_get_reboot_info(const char *mbox, struct avm_reboot_info *info)
{
	struct avm_firmware_build_info *bi;
	char val[128];
	int err;

	info->fw.fwinfo_version = AVM_FWINFO_VERSION(3, 0);
	info->fw.src = AVM_FWINFO_SOURCE_MAILBOX;

	err = mailbox_get_value(mbox, "UP", val, sizeof(val));
	if (!err)
		sscanf(val, "%lu", &info->tm.uptime);

	err = mailbox_get_value(mbox, "UTC", val, sizeof(val));
	if (!err)
		sscanf(val, "%lld", (long long *)&info->tm.time);

	err = mailbox_get_value(mbox, "FW", val, sizeof(val));
	if (!err)
		strlcpy(info->fw.fw_version, val, sizeof(info->fw.fw_version));

	err = mailbox_get_value(mbox, "HW", val, sizeof(val));
	if (!err)
		strlcpy(info->fw.hw_revision, val, sizeof(info->fw.hw_revision));

	err = mailbox_get_value(mbox, "HWS", val, sizeof(val));
	if (!err)
		strlcpy(info->fw.hw_subrevision, val, sizeof(info->fw.hw_subrevision));

	bi = info->fw.build;
	if (bi == NULL)
		return;

	err = mailbox_get_value(mbox, "BN", val, sizeof(val));
	if (!err)
		strlcpy(bi->buildnumber, val, sizeof(bi->buildnumber));

	err = mailbox_get_value(mbox, "FS", val, sizeof(val));
	if (!err)
		strlcpy(bi->firmwarestring, val, sizeof(bi->firmwarestring));

	err = mailbox_get_value(mbox, "BT", val, sizeof(val));
	if (!err)
		sscanf(val, "%d", &bi->buildtype);
	err = mailbox_get_value(mbox, "BD", val, sizeof(val));
	if (!err)
		sscanf(val, "%d", &bi->builddirty);
}

static bool strempty(const char *str)
{
	return str[0] == 0;
}

static void fixup_fwinfo_from_mailbox(struct avm_firmware_info *cur,
				      const struct avm_firmware_info *next)
{
	if (strempty(cur->fw_version)) {
		cur->src = AVM_FWINFO_SOURCE_CURRENTFW;
		strcpy(cur->fw_version, next->fw_version);
	}

	if (strempty(cur->hw_revision))
		strcpy(cur->hw_revision, next->hw_revision);

	if (strempty(cur->hw_subrevision))
		strcpy(cur->hw_subrevision, next->hw_subrevision);

	if (strempty(cur->bootloader_version))
		strcpy(cur->bootloader_version, next->bootloader_version);
}

/**
 */
static void clear_reboot_counters(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		reboot_info[i].reboot_count = 0;
	}
}

static size_t write_reboot_counters_array(char *str, size_t size, enum reboot_array_format mbs_format)
{
	struct _reboot_info *rinfo;
	size_t i, len;
	unsigned int count;

	len = 0;
	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		int ret = 0;

		rinfo = &reboot_info[i];

		if (!rinfo->mbs.cntr)
			continue;

		switch (mbs_format) {
		case mailbox_format:
			/* parsable reboot-mailbox-format */
			count = (rinfo->flags & RI_OTHER_EVENT) ? rinfo->live_count : rinfo->reboot_count;
			if (!count)
				break;
			ret = snprintf(&str[len], size, "%s(%u)",
				       rinfo->mbs.cntr,
				       count);
			break;
		case irregularreboot_format:
			/* Irregular-Reboots: <format> (/proc/avm/reboot/info) */
			if ((rinfo->flags & RI_OTHER_EVENT) || !rinfo->reboot_count)
				break;
			ret = snprintf(&str[len], size, "%s(%u) ",
				       rinfo->mbs.cntr,
				       rinfo->reboot_count);
			break;
		case line_format:
			/* line by line format (/proc/avm/reboot/info) */
			if (!rinfo->reboot_count)
				break;
			ret = snprintf(&str[len], size, "Reboots: %s %u\n",
				       rinfo->sysctl_name_read,
				       rinfo->reboot_count);
			break;
		}
		if (ret < 0)
			return len;
		len += ret;
		if (len >= size)
			return len;
	}
	if (len && str[len-1] == ' ')
		str[--len] = '\0'; /* Strip final space */
	else if (!len && size)
		str[len] = '\0';

	return len;
}

void avm_get_firmware_uptime(struct avm_firmware_uptime *tm)
{
	/* Use lockless API */
	tm->uptime = ktime_get_seconds();
	tm->time = __ktime_get_real_seconds();
}

static size_t write_reboot_counters(char *mbox, int maxlen)
{
	struct avm_firmware_build_info *bi;
	struct avm_reboot_info *info;
	int len, avail;

	len = write_reboot_counters_array(mbox, maxlen, mailbox_format);

	info = &next_reboot_info;
	avm_get_firmware_uptime(&info->tm);

	avail = max(0, maxlen - len);
	len += snprintf(&mbox[len], avail, "%s(%lu)%s(%lld)%s(%s)%s(%s)%s(%s)%s(%s)",
			"UP",  info->tm.uptime,
			"UTC", info->tm.time,
			"FW",  info->fw.fw_version,
			"HW",  info->fw.hw_revision,
			"HWS", info->fw.hw_subrevision,
			"BV",  info->fw.bootloader_version);

	bi = info->fw.build;
	if (bi == NULL)
		return len;

	avail = max(0, maxlen - len);
	len += snprintf(&mbox[len], avail, "%s(%s)%s(%s)%s(%d)%s(%d)",
			"BN",  bi->buildnumber,
			"FS",  bi->firmwarestring,
			"BT",  bi->buildtype,
			"BD",  bi->builddirty);

	return len;
}

static void debug_mailbox_dump(const char *prefix, const char *mbox, size_t len)
{
	if (DEBUG_MAILBOX)
		print_hex_dump(KERN_DEBUG, prefix, DUMP_PREFIX_OFFSET, 16, 1,
			       mbox, len, true);
}

static DEFINE_SEMAPHORE_1(mailbox_lock);

static char *get_mailbox(void)
{
	int ret;

	ret = rte_down_trylock(&mailbox_lock);
	if (ret)
		return ERR_PTR(-EAGAIN);

	return arch_get_mailbox();
}

static void put_mailbox(void)
{
	rte_up(&mailbox_lock);
}

#ifndef arch_read_mailbox
#define arch_read_mailbox __default_read_mailbox
#else
static int __default_read_mailbox(char *mbox, size_t size) __maybe_unused;
#endif
static int __default_read_mailbox(char *mbox, size_t size)
{
	char *arch_mailbox;

	arch_mailbox = arch_get_mailbox();
	if (!arch_mailbox)
		return -ENOSYS;
	memcpy(mbox, arch_mailbox, size);

	return 0;
}

static __init int read_mailbox(char *mbox, size_t size)
{
	int ret;

	down(&mailbox_lock);
	ret = arch_read_mailbox(mbox, size);
	up(&mailbox_lock);

	return ret;
}

/* On armp7, we allow using reboot_status w/o a mailbox.
 *
 * Doing this on other boxes hasn't been evaluated when adding this; it is
 * confined to armp7 in order not to silently change behavior on other
 * platforms.
 */
#if defined(CONFIG_MACH_PUMA7) && defined(CONFIG_ARCH_AVALANCHE)
#  define IGNORE_MISSING_MAILBOX 1
#else
#  define IGNORE_MISSING_MAILBOX 0
#endif

static void inc_reboot_counter(struct _reboot_info *ri)
{
	if (ri->flags & RI_OTHER_EVENT)
		ri->live_count++;
	else if ((ri->flags & RI_CNT_ON_BOOTUP) == 0 &&
		 ri->mbs.cntr)
		ri->reboot_count++;
}

static void write_mailbox(struct _reboot_info *ri, char *mailbox,
			  char **reboot_counters_str)
{
	const u8 *mbstr;
	unsigned int len;
	char *rcs;
	int ret;

	if (!mailbox)
		return;

	if (ri->mbs.str) {
		mbstr = ri->mbs.str;
		len = ri->mbs.strlen;
	} else {
		mbstr = "-----";
		len = strlen(mbstr) + 1;
	}

	memset(mailbox, 0, MAILBOX_SIZE);
	memcpy(mailbox, mbstr, len);

	/* Append reboot counter string AFTER terminating NUL */
	*reboot_counters_str = rcs = mailbox + len;
	len += write_reboot_counters(rcs, MAILBOX_SIZE - len);
	if (len >= MAILBOX_SIZE) {
		pr_err("Mailbox overflow when saving reboot status, data truncated\n");
		len = MAILBOX_SIZE;
	} else {
		++len; /* Include NUL */
	}

	debug_mailbox_dump("Writing mailbox: ", mailbox, len);

	ret = arch_flush_mailbox(mailbox, len);
	if (ret)
		pr_err("Error flushing mailbox: %d\n", ret);
}

#ifdef CONFIG_AVM_PLATFORM_RESET_STATUS
#define __avm_reset_init
#else
#define __avm_reset_init __init
#endif

void __weak avm_pltfm_reboot_status_notify(enum _avm_reset_status status) { }

static bool accept_new_reboot_status(const struct _reboot_info *new_ri)
{
	enum _avm_reset_status status;
	const struct _reboot_info *ri;

	status = avm_next_reset_status();
	if (status == RS_UNSPECIFIED)
		return true;

	ri = get_reboot_info_struct(status);
	if (ri->flags & RI_PROVISIONAL)
		return true;
	if (ri->flags & RI_PANICKING)
		return false;

	return new_ri->flags & RI_PANICKING;
}

/**
 * \brief Set reboot-status that applies after the next reboot.
 * \param status reboot-status
 */
void avm_set_reset_status(enum _avm_reset_status status)
{
	char *reboot_counters_str = "";
	struct _reboot_info *ri;
	char *mailbox;

	if (status == RS_REBOOT && oops_in_progress)
		status = RS_OOPS;

	if (WARN_ON(status == RS_UNSPECIFIED))
		return;

	ri = get_reboot_info_struct(status);
	if (!ri)
		return;

	if (!accept_new_reboot_status(ri))
		return;

	if (status == avm_next_reset_status()) {
		pr_notice("%s: Ignoring request to set status %d which is already set\n",
			  __func__, (int)status);
		return;
	}

	mailbox = get_mailbox();
	if (IS_ERR(mailbox)) {
		pr_err("Error saving reboot status to mailbox: %ld\n",
			PTR_ERR(mailbox));
		return;
	}

	pr_debug("Setting reboot status: %d\n", status);

	/**
	 * Reboot-Counter updaten/loeschen
	 */
	if (ri->flags & RI_CLEARALL_ON_BOOTUP)
		clear_reboot_counters();

	inc_reboot_counter(ri);

	write_mailbox(ri, mailbox, &reboot_counters_str);

	if ((ri->flags & RI_OTHER_EVENT) == 0)
		next_reboot_info.reboot_status = status;
	if ((ri->flags & RI_PROVISIONAL) == 0) {
		/* Note, Hochsee parses this and expects __func__ */
		pr_emerg("%s: %s%s%s%s%s%s\n", __func__,
		       ri->status_name,
		       ri->mbs.cntr ? "(" : "",
		       ri->mbs.cntr ?
			       ri->mbs.cntr :
			       "",
		       ri->mbs.cntr ? ") " : " ",
		       *reboot_counters_str ? " - " : "",
		       reboot_counters_str);
	}
	put_mailbox();

	avm_pltfm_reboot_status_notify(status);
}
EXPORT_SYMBOL(avm_set_reset_status);

bool rebooting_after_panic(void)
{
	const struct _reboot_info *ri = get_reboot_info_struct(avm_reset_status());

	BUG_ON(!ri);
	return ri->flags & RI_PANICKING;
}

bool avm_reset_status_is_provisional(enum _avm_reset_status status)
{
	const struct _reboot_info *ri = get_reboot_info_struct(status);

	return ri && (ri->flags & RI_PROVISIONAL);
}

/**
 */
static int avm_reboot_handler(struct notifier_block *nb, unsigned long event,
			      void *buf);
static struct notifier_block avm_reboot_notifier = {
	.notifier_call = avm_reboot_handler, /* callback   */
	.priority = 0
};

/**
 */
#ifndef arch_reboot_handler
#  define arch_reboot_handler(nb, ev, data) NOTIFY_DONE
#endif
static int avm_reboot_handler(struct notifier_block *nb,
			      unsigned long event,
			      void *buf)
{
	console_loglevel = CONSOLE_LOGLEVEL_DEFAULT;
	avm_set_reset_status(RS_REBOOT);
	avm_stack_check(NULL);
	return arch_reboot_handler(nb, event, buf);
}

#if defined(CONFIG_AVM_FASTIRQ)
#if defined(CONFIG_ARCH_QCOM) || defined(CONFIG_AVM_FASTIRQ_OPTEE)
void avm_set_crashed_cpu(int cpu)
{
	SET_CRASHED_CPU(cpu);
}
int _avm_get_crashed_cpu(void)
{
	return atomic_read(&crashed_cpu);
}
int avm_get_crashed_cpu(void)
{
	return GET_CRASHED_CPU();
}
#endif /*--- #ifdef CONFIG_ARCH_QCOM ---*/
#endif /*--- #ifdef CONFIG_AVM_FASTIRQ ---*/
static int panic_handler(struct notifier_block *notifier_block,
			 unsigned long event, void *cause_string);
static struct notifier_block panic_notifier[] = {

	{.notifier_call	= panic_handler, .next = NULL, .priority = 1 },
#if defined(arch_panic_notifier)
	{.notifier_call	= arch_panic_notifier, .next = NULL, .priority = arch_panic_notifier_priority },
#endif
};

/**
 */
static int panic_handler(struct notifier_block *notifier_block,
			 unsigned long event,
			 void *cause_string)
{
	console_verbose();
	avm_set_reset_status(RS_PANIC);
	avm_stack_check(NULL);
	avm_oom_show_memstat(AVM_OOM_MEMSTAT_ONCE);
	return NOTIFY_DONE;
}

/**
 */
static int die_notifier(struct notifier_block *self, unsigned long cmd,
			void *ptr);
static struct notifier_block die_nb = {
	.notifier_call = die_notifier,
	.priority = 0,
};

/**
 */
static int die_notifier(struct notifier_block *self, unsigned long cmd,
			void *ptr)
{
	if (cmd != DIE_OOPS)
		return NOTIFY_DONE;

	console_verbose();
	avm_set_reset_status(RS_OOPS);
	avm_stack_check(NULL);
#if defined(arch_die_notifier)
	return arch_die_notifier(self, cmd, ptr);
#else
	return NOTIFY_OK;
#endif /*--- #if #defined(arch_die_notifier) ---*/
}

static struct _reboot_info *fixup_reboot_status(struct _reboot_info *ri,
						const struct avm_pltfm_reset_status *prs)
{
	struct _reboot_info *pri;

	if (!prs || prs->status == RS_UNSPECIFIED)
		return ri;
	if (ri->status != RS_SHORTREBOOT && ri->status != RS_POWERON)
		return ri;

	pr_notice("Platform override for reboot_status: (%d -> %d) %s\n",
		  ri->status, prs->status, prs->message ?: "(no description provided)");

	if (prs->message)
		tffs_write_pltfm_paniclog(prs->message);

	pri = get_reboot_info_struct(prs->status);
	inc_reboot_counter(pri);

	return pri;
}

/**
 * \brief initial function called over arch_initcall-chain of linux-os
 * get the last reboot-status from architecture-depend reboot-safe
 * further functions:
 *  - print out last reboot-status
 *  - initialize actual reboot-status to overwritable short-power-on
 *  - register die-notifier
 *  - register panic-notifier
 */
static __avm_reset_init int get_reboot_status(const struct avm_pltfm_reset_status *prs)
{
	unsigned int i, len;
	char Buffer[MAILBOX_SIZE] = {0};
	struct _reboot_info *ri;
	int ret;

	ret = read_mailbox(Buffer, MAILBOX_SIZE);
	if (ret == -ENOSYS) {
		if (IGNORE_MISSING_MAILBOX)
			goto no_mailbox;
		return 0;
	}
	if (ret) {
		pr_err("Error reading reboot status: %d\n", ret);
		return ret;
	}
	current_reboot_info.fw.build = &reboot_buildinfo;
	Buffer[MAILBOX_SIZE - 1] = '\0';
	debug_mailbox_dump("Found mailbox: ", Buffer, MAILBOX_SIZE);

	for (ri = &reboot_info[0];; ++ri) {
		if (!ri->mbs.str)
			goto rs_poweron;
		if (ri->flags & RI_OTHER_EVENT)
			continue;
		len = ri->mbs.strlen;
		BUG_ON(len > MAILBOX_SIZE - 1);
		if (memcmp(Buffer, ri->mbs.str, len) == 0)
			break;
	}

	mailbox_get_reboot_info(&Buffer[len], &current_reboot_info);
	read_reboot_counters(&Buffer[len]);

rs_poweron:
	ri = fixup_reboot_status(ri, prs);

	if (ri->flags & RI_CNT_ON_BOOTUP)
		ri->reboot_count++;

	current_reboot_info.reboot_status = ri->status;
	write_reboot_counters_array(Buffer, sizeof(Buffer), mailbox_format);
	pr_err("Reboot Status is: %s%s%s%s%s\n",
	       ri->status_name,
	       ri->mbs.cntr ? "(" : "",
	       ri->mbs.cntr ?
		       ri->mbs.cntr :
		       "",
	       ri->mbs.cntr ? ") " : " ",
	       Buffer);
no_mailbox:
	avm_get_firmware_info(&next_reboot_info.fw, AVM_FWINFO_VERSION(3, 0));
	fixup_fwinfo_from_mailbox(&current_reboot_info.fw, &next_reboot_info.fw);
	avm_set_reset_status(RS_SHORTREBOOT);
	register_die_notifier(&die_nb);

	for (i = 0; i < ARRAY_SIZE(panic_notifier); i++)
		atomic_notifier_chain_register(&panic_notifier_list, &panic_notifier[i]);

	register_reboot_notifier(&avm_reboot_notifier);
	return 0;
}

#ifdef CONFIG_AVM_QTI_REBOOT_INFO
static size_t write_qca_reset_reason(char *buf, size_t buf_len)
{
	char qca_reset_reason_msg[100];
	int qca_reset_reason;

	qca_reset_reason = get_avm_qti_reboot_info_data(qca_reset_reason_msg, sizeof(qca_reset_reason_msg));

	if (qca_reset_reason > 0)
		return scnprintf(buf, buf_len, "QCA Reset Reason: %s[%#x]\n", qca_reset_reason_msg, qca_reset_reason);

	if (qca_reset_reason == -E2BIG)
		return scnprintf(buf, buf_len, "QCA Reset Reason: %s [Message too long]\n", qca_reset_reason_msg);

	return 0;
}
#endif

#ifdef CONFIG_AVM_PLATFORM_RESET_STATUS
void avm_pltfm_set_reset_status(const struct avm_pltfm_reset_status *prs)
{
	get_reboot_status(prs);
}
#else
static int __init init_reboot_status(void)
{
	get_reboot_status(NULL);
	return 0;
}
arch_initcall(init_reboot_status);
#endif

/**
 * maxsize of buf: 14 + 10 + 2 + 2 + 2  = 30
 */
static char *human_readable_time(unsigned long time, char *buf, unsigned long buf_size)
{
	unsigned long seconds, minutes, hours, days;

	seconds = time % 60;
	time /= 60;
	minutes = time % 60;
	time /= 60;
	hours = time % 24;
	time /= 24;
	days = time;
	snprintf(buf, buf_size, "%lu d %lu h %lu min %lu s", days, hours, minutes, seconds);
	return buf;
}

size_t avm_snprint_reboot_info(const struct avm_reboot_info *reboot_info,
			       char *buf, size_t bufsiz)
{
	const struct avm_firmware_info *fwinfo = &reboot_info->fw;
	const struct avm_firmware_uptime *tm = &reboot_info->tm;
	char reboot_counter_str[256] = "";
	size_t len = 0;

	if (!bufsiz)
		return 0;

	if (!fwinfo->fwinfo_version) {
		buf[0] = 0;
		return 0;
	}

	len += scnprintf(&buf[len], bufsiz - len, "Uptime: %lu\n", tm->uptime);
	if (tm->uptime) {
		char htime[32], huptime[128];

		human_readable_time(tm->uptime, huptime, sizeof(huptime));
		time_to_ascii(tm->time, htime, sizeof(htime));

		len += scnprintf(&buf[len], bufsiz - len,
				 "Uptime-String: %s - panic on %s\n", huptime, htime);
	}

	if (fwinfo->fwinfo_version >= AVM_FWINFO_VERSION(2, 1)) {
		/* compat: we need counter BE(x) for WLAN: JZ-111763 */
		write_reboot_counters_array(reboot_counter_str, sizeof(reboot_counter_str), irregularreboot_format);
		if (!strempty(reboot_counter_str)) {
			len += scnprintf(&buf[len], bufsiz - len,
					"%s: %s %s\n",
					"Irregular-Reboots",
					reboot_counter_str,
					"(since last regular reboot/power-cut)");
		}
		/* display counter line by line */
		len += write_reboot_counters_array(&buf[len], bufsiz - len, line_format);
	}

	len += avm_snprint_firmware_info_kernel(fwinfo, &buf[len], bufsiz - len);

#ifdef CONFIG_AVM_QTI_REBOOT_INFO
	len += write_qca_reset_reason(&buf[len], bufsiz - len);
#endif

	return len;
}

static struct proc_dir_entry *rebootprocdir;
#define PROC_REBOOTDIR "avm/reboot"

static void proc_reboot_info_read(struct seq_file *seq, void *priv)
{
	char buf[AVM_REBOOT_INFO_SIZE];

	avm_snprint_reboot_info(&current_reboot_info, buf, sizeof(buf));
	seq_puts(seq, buf);
	seq_puts(seq, "\n");
}

static int proc_reboot_info_write(char *buf, void *priv)
{
	if (strstr(buf, "clear_rebootcounter")) {
		clear_reboot_counters();
	} else {
		pr_err("/proc/avm/reboot/info: invalid input (\"clear_rebootcounter\" resets counters)\n");
		return -EINVAL;
	}

	return 0;
}

/**
 */
static __init int proc_reboot_status(void)
{
	rebootprocdir = proc_mkdir(PROC_REBOOTDIR, NULL);
	if (rebootprocdir == NULL) {
		return 0;
	}
	return add_simple_proc_file("avm/reboot/info",
		proc_reboot_info_write, proc_reboot_info_read, NULL);
}
late_initcall(proc_reboot_status);
