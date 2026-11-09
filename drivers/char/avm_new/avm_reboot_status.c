// SPDX-License-Identifier: GPL-2.0+

/** @file avm_reboot_status.c
 *
 * mbahr:
 * 1.) hold and get reboot-status after Soft/NMI-Reboot
 * 2.) handle die-notifier
 * 3.) handle panic-notifier
 */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmsg_dump.h>
#include <linux/pm.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/kdebug.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/nmi.h>
#include <linux/proc_fs.h>
#include <linux/env.h>
#include <linux/rtc.h>
#if __has_include(<avm/tffs/tffs.h>)
#include <avm/tffs/tffs.h>
#else
#include <linux/tffs.h>
#endif
#pragma GCC pop_options
#include <linux/avm_hw_config.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#include <uapi/avm/enh/fw_info.h>
#endif
#include <avm/enh/reboot_status.h>
#include <avm/sammel/simple_proc.h>
#include <asm/mach_avm.h>


#if defined(CONFIG_AVM_FASTIRQ)
#if defined(CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON)
#include <asm/avm_enh/avm_fiq_os.h>
#else
#include <mach/avm_fiq_os.h>
#endif
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/
#include "avm_sammel.h"

#define MAILBOX_SIZE 512U

#if defined(CONFIG_SOC_GRX500)
#include "arch_avm_reboot_status_grx.h"
#endif /*--- #if defined(CONFIG_SOC_GRX500) ---*/

#if defined(CONFIG_VR9) || defined(CONFIG_AR10)
#include "arch_avm_reboot_status_ifx.h"
#endif /*--- #if defined(CONFIG_VR9) || defined(CONFIG_AR10) ---*/

#if defined(CONFIG_MACH_PUMA6)
#include "arch_avm_reboot_status_puma6a.h"
#elif defined(CONFIG_X86) && !defined(CONFIG_X86_PUMA7)
#include "arch_avm_reboot_status_puma6x.h"
#elif defined(CONFIG_MACH_PUMA7)

#if defined(CONFIG_X86_PUMA7)
#include "arch_avm_reboot_status_puma7x.h"
#else
#include "arch_avm_reboot_status_puma7.h"
#endif

#endif

#if defined(CONFIG_MACH_BCM963138) || defined(CONFIG_BCM963178)
#include "arch_avm_reboot_status_brcma.h"
#endif /*--- #if defined(CONFIG_MACH_BCM963138) || defined(CONFIG_BCM963178) ---*/

#if defined(CONFIG_ARCH_IPQ40XX)
#include "arch_avm_reboot_status_ipq40xx.h"
#endif /*--- #if defined(CONFIG_ARCH_IPQ40XX)  ---*/

#if defined(CONFIG_SOC_AR724X) || defined(CONFIG_SOC_AR934X) ||                \
	defined(CONFIG_SOC_QCA955X) || defined(CONFIG_SOC_QCA953X) ||          \
	defined(CONFIG_SOC_QCA956X)
#include "arch_avm_reboot_status_scrpn.h"
#endif /*--- #if defined(CONFIG_SOC_AR724X) || defined(CONFIG_SOC_AR934X) || defined(CONFIG_SOC_QCA955X) || defined(CONFIG_SOC_QCA953X) || defined(CONFIG_SOC_QCA956X)  ---*/

#include <avm/enh/fw_info.h>

#define DEBUG_MAILBOX 0

struct _avm_reboot_info {
	enum _avm_reset_status status;
	enum _avm_reset_status next_reboot_status;
	unsigned long uptime;
	unsigned long utc;
	char fw_version[2][64];	// index: 0 prepared before crash for mailbox-write, index: 1 read from mailbox
	char hw_version[2][32];
	char hw_subversion[2][32];
	char bootloaderVersion[2][32];
};

static struct _avm_reboot_info avm_reboot_info;

#define MB_SUM_PREFIX		"SUM("
#define MB_UPTIME_PREFIX	"UP("
#define MB_UTC_PREFIX		"UTC("
#define MB_FW_PREFIX		"FW("
#define MB_HW_PREFIX		"HW("
#define MB_HWS_PREFIX		"HWS("
#define MB_BV_PREFIX		"BV("

#define FW_STRING(strname)  .str[0] = avm_reboot_info.strname[0], .str[1] = avm_reboot_info.strname[1], .str_len = sizeof(avm_reboot_info.strname[0])
static const struct _prepare_fw_info {
	char *env_name;
	const char *mb_name;  // mailbox-name
	char *str[2];
	unsigned int str_len;
} prepare_fw_info[] = {
	{ .env_name = "firmware_info",     .mb_name = MB_FW_PREFIX,  FW_STRING(fw_version)		},
	{ .env_name = "HWRevision",        .mb_name = MB_HW_PREFIX,  FW_STRING(hw_version)		},
	{ .env_name = "HWSubRevision",     .mb_name = MB_HWS_PREFIX, FW_STRING(hw_subversion)		},
	{ .env_name = "bootloaderVersion", .mb_name = MB_BV_PREFIX,  FW_STRING(bootloaderVersion)	},
};

/**
 * \brief Deliver last reboot-status
 */
enum _avm_reset_status avm_reset_status(void)
{
	enum _avm_reset_status status;

	switch (avm_reboot_info.status) {
	case RS_PANIC:
	case RS_OOM:
	case RS_OOPS:
		status = RS_REBOOT;
		break;
	default:
		status = avm_reboot_info.status;
	}
	return status;
}
EXPORT_SYMBOL(avm_reset_status);

enum _avm_reset_status avm_next_reset_status(void)
{
	return avm_reboot_info.next_reboot_status;
}
EXPORT_SYMBOL(avm_next_reset_status);

struct _reboot_info {
	const enum _avm_reset_status status;
	const char *matchtext;
	int matchlen;
	const char *printouttext;
	const char *shortprintouttext;
	unsigned int reboot_count;
};

#define REBOOT_INFO_ENTRY(_status, user_txt, mbox_txt, mbox_counter) {	\
	.status = (_status),						\
	.matchtext = (mbox_txt),					\
	.matchlen  = (mbox_txt) ? sizeof(mbox_txt) : 0,			\
	.printouttext = (user_txt),					\
	.shortprintouttext = (mbox_counter),				\
}

// clang-format off
static struct _reboot_info reboot_info[] = {
	REBOOT_INFO_ENTRY(RS_SOFTWATCHDOG,      "Softwatchdog-Reboot",      SOFTWATCHDOG_REBOOT_STATUS_TEXT,         "WD"),
	REBOOT_INFO_ENTRY(RS_NMIWATCHDOG,       "NMI-Watchdog-Reset",       NMI_REBOOT_STATUS_TEXT,                  "NMI"),
	REBOOT_INFO_ENTRY(RS_FIRMWAREUPDATE,    "Fw-Update",                UPDATE_REBOOT_STATUS_TEXT,               NULL),
	REBOOT_INFO_ENTRY(RS_SHORTREBOOT,       "Short-PowerOff-Reboot",    POWERON_REBOOT_STATUS_TEXT,              "SHORTPOWERCUT"),
	REBOOT_INFO_ENTRY(RS_TEMP_REBOOT,       "Temperature-Reboot",       TEMP_REBOOT_STATUS_TEXT,                 "TEMPERATURE"),
	REBOOT_INFO_ENTRY(RS_REBOOT_FOR_UPDATE,	"Update-Reboot",            SOFT_REBOOT_STATUS_TEXT_UPDATE,          "UPDATE"),
	REBOOT_INFO_ENTRY(RS_PANIC,             "Soft-Reboot",              SOFT_REBOOT_STATUS_TEXT_PANIC,           "PANIC"),
	REBOOT_INFO_ENTRY(RS_OOM,               "Soft-Reboot",              SOFT_REBOOT_STATUS_TEXT_OOM,             "OOM"),
	REBOOT_INFO_ENTRY(RS_OOPS,              "Soft-Reboot",              SOFT_REBOOT_STATUS_TEXT_OOPS,            "KCRASH"),
	REBOOT_INFO_ENTRY(RS_DOCSIS_LOCAL,      "Soft-Reboot",              SOFT_REBOOT_STATUS_TEXT_DOCSIS_LOCAL,    "DOCSIS_LOCAL"),
	REBOOT_INFO_ENTRY(RS_DOCSIS_OPERATOR,   "Soft-Reboot",              SOFT_REBOOT_STATUS_TEXT_DOCSIS_OPERATOR, "DOCSIS_OPERATOR"),
	/*--- als vorletzter Eintrag, da dieser Untermenge von RS_PANIC/RS_OOM/RS_OOPS ---*/
	REBOOT_INFO_ENTRY(RS_REBOOT,            "Soft-Reboot",              SOFT_REBOOT_STATUS_TEXT,                 NULL),
	/*--- definition: have to be last entry: ---*/
	REBOOT_INFO_ENTRY(RS_POWERON,           "Power-On",                 NULL,                                    NULL),
};
// clang-format on

/**
 */
static void read_reboot_counters(char *txt)
{
	char *p;
	unsigned int i, control, sum = 0;
	unsigned int counter[ARRAY_SIZE(reboot_info)];

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		counter[i] = 0;
		if (reboot_info[i].shortprintouttext &&
		    (p = strstr(txt, reboot_info[i].shortprintouttext))) {
			p += strlen(reboot_info[i].shortprintouttext) +
			     1; /*--- inklusive ( ---*/
			sscanf(p, "%u", &counter[i]);
			sum += counter[i];
		}
	}
	p = strstr(txt, MB_SUM_PREFIX);
	if (p) {
		p += sizeof(MB_SUM_PREFIX) - 1;
		sscanf(p, "%u", &control);
		if (control == sum) {
			for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
				reboot_info[i].reboot_count = counter[i];
			}
		}
	}
}

/**
 * zero-terminate dest on ')'
 */
static void copy_info_string(char *dest, const char *src, unsigned int len)
{
	strlcpy(dest, src, len);
	while (*dest) {
		if (*dest == ')') {
			*dest = 0;
			return;
		}
		dest++;
	}
}

/**
 * read uptime and fw-info from mailbox
 * format UP(<time_sec>)UTC(<time>)FW(<version>)HW(<version>)HWS(<subversion>)BV(<version>)
 * seperate to avm_reset_info-stringfields -> index=1
 */
static void read_firmware_info(char *txt)
{
	unsigned int i;
	char *p;

	p = strstr(txt, MB_UPTIME_PREFIX);
	if (p) {
		p += sizeof(MB_UPTIME_PREFIX) - 1;
		sscanf(p, "%lu", &avm_reboot_info.uptime);
	}
	p = strstr(txt, MB_UTC_PREFIX);
	if (p) {
		p += sizeof(MB_UTC_PREFIX) - 1;
		sscanf(p, "%lu", &avm_reboot_info.utc);
	}
	/*--- read out from mailbox: ---*/
	for (i = 0; i < ARRAY_SIZE(prepare_fw_info); i++) {
		const char *pmb = prepare_fw_info[i].mb_name;

		p = strstr(txt, pmb);
		if (p) {
			p += strlen(prepare_fw_info[i].mb_name);
			copy_info_string(prepare_fw_info[i].str[1], p, prepare_fw_info[i].str_len);
		}
	}
}

/**
 * read out from prom_getenv() for inescapably future crash
 * seperate to avm_reset_info-stringfields -> index=0
 */
static void prepare_firmware_info(void)
{
	unsigned int i;

	for (i = 0; (i < ARRAY_SIZE(prepare_fw_info)); i++) {
		const char *p = prom_getenv(prepare_fw_info[i].env_name);

		/*--- f**cking special case ---*/
		if (strcmp(prepare_fw_info[i].mb_name, MB_FW_PREFIX) == 0) {
			p = avm_fw_version();
		}

		if (p) {
			strlcpy(prepare_fw_info[i].str[0], p, prepare_fw_info[i].str_len);
		}
	}
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

/**
 */
static unsigned int sum_reboot_counters(void)
{
	unsigned int i, sum = 0;

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		if (reboot_info[i].shortprintouttext) {
			sum += reboot_info[i].reboot_count;
		}
	}
	return sum;
}


static size_t write_reboot_counters_array(char *str, size_t size)
{
	struct _reboot_info *rinfo;
	size_t i, len;
	int ret;

	len = 0;
	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		rinfo = &reboot_info[i];
		if (!rinfo->shortprintouttext || !rinfo->reboot_count)
			continue;
		ret = snprintf(&str[len], size, "%s(%u) ",
				rinfo->shortprintouttext,
				rinfo->reboot_count);
		if (ret < 0)
			return len;
		len += ret;
		if (len >= size)
			return len;
	}
	if (len)
		str[--len] = '\0'; /* Strip final space */

	return len;
}

/**
 */
static unsigned long get_act_time(void)
{
	struct timeval now;

	do_gettimeofday(&now);
	return (unsigned long)(now.tv_sec);
}

static size_t write_reboot_counters(char *txt, int maxlen)
{
	struct timespec uptime;
	unsigned int sum;
	size_t len;

	len = write_reboot_counters_array(txt, maxlen);
	if (len >= (size_t)maxlen)
		return len;

	sum = sum_reboot_counters();
	if (sum)
		len += snprintf(&txt[len], maxlen - len, "SUM(%u)", sum);

	get_monotonic_boottime(&uptime);
	len += snprintf(&txt[len], maxlen - len, "%s%lu)%s%lu)%s%s)%s%s)%s%s)%s%s)",
			MB_UPTIME_PREFIX,
			(unsigned long)uptime.tv_sec,
			MB_UTC_PREFIX,
			get_act_time(),
			MB_FW_PREFIX,
			avm_reboot_info.fw_version[0],
			MB_HW_PREFIX,
			avm_reboot_info.hw_version[0],
			MB_HWS_PREFIX,
			avm_reboot_info.hw_subversion[0],
			MB_BV_PREFIX,
			avm_reboot_info.bootloaderVersion[0]);

	return len;
}

/**
 */
static int get_fieldindex_by_status(enum _avm_reset_status status)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		if (reboot_info[i].status == status) {
			return i;
		}
	}
	return -1;
}

/**
 */
void avm_rebootcounter_reset(void)
{
	clear_reboot_counters();
}

/**
 */
char *avm_rebootcounter_string(char *txt, unsigned int maxlen)
{
	unsigned int len, sum;

	txt[0] = 0;

	sum = sum_reboot_counters();
	if (sum == 0)
		return txt;

	len = snprintf(txt, maxlen, "Irregular Reboots: SUM(%u) - ", sum);
	if (len >= maxlen)
		return txt;

	write_reboot_counters_array(&txt[len], maxlen - len);

	return txt;
}

static void debug_mailbox_dump(const char *prefix, const char *mbox, size_t len)
{
	char __mb[MAILBOX_SIZE] = "";
	size_t size, pos = 0;

	if (!DEBUG_MAILBOX)
		return;

	if (len == 0)
		goto dump;

	size = len;
	if (mbox[size - 1]) {
		/* No terminating NUL, need to append one */
		++size;
	} else {
		/*
		 * While the mailbox data may contain embedded NUL chars,
		 * we merge trailing consecutive NUL chars to one.
		 */
		while (size > 1 && !mbox[size - 2])
			--size;
	}
	size = min(sizeof(__mb), size);

	/*
	 * Copy strings, mapping intermediate NUL delimiters to an
	 * alternate, printable character.
	 */
	while (true) {
		pos += strlcpy(&__mb[pos], &mbox[pos], size - pos);
		if (pos + 1 >= size)
			break;
		__mb[pos++] = '+';
	}

dump:
	pr_info("%s: %s\n", prefix, __mb);
}

static DEFINE_SEMAPHORE(mailbox_lock);

#define avm_rte_down_trylock	__BUILD_AVM_CONTEXT_FUNC(down_trylock)
#define avm_rte_up		__BUILD_AVM_CONTEXT_FUNC(up)

static char *get_mailbox(void)
{
	int ret;

	ret = avm_rte_down_trylock(&mailbox_lock);
	if (ret)
		return ERR_PTR(-EAGAIN);

	return arch_get_mailbox();
}

static void put_mailbox(void)
{
	avm_rte_up(&mailbox_lock);
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

/**
 */
static void set_reboot_status(int nofix, enum _avm_reset_status status)
{
	int idx, ret;
	const char *text = "-----";
	unsigned int len = strlen(text) + 1;
	static const char *reboot_cause_written;
	char *mailbox;
	char *reboot_counters_str;

	/*--- only if not valid set before: ---*/
	if (reboot_cause_written != NULL) {
		return;
	}

	mailbox = get_mailbox();
	if (IS_ERR(mailbox)) {
		pr_err("Error saving reboot status to mailbox: %ld\n",
			PTR_ERR(mailbox));
		return;
	}
	if (!mailbox && !IGNORE_MISSING_MAILBOX)
		goto out;

	if ((status == RS_REBOOT) && oops_in_progress) {
		status = RS_OOPS;
	}
	pr_debug("Setting reboot status: %d\n", status);
	idx = get_fieldindex_by_status(status);
	if (idx < 0) {
		goto out;
	}

	if (reboot_info[idx].matchtext) {
		text = reboot_info[idx].matchtext;
		len = reboot_info[idx].matchlen;
	}
	if (nofix == 0) {
		reboot_cause_written = text;
		/**
		 * Reboot-Counter updaten/loeschen
		 */
		switch (status) {
		case RS_REBOOT:
			clear_reboot_counters();
			break;
		case RS_FIRMWAREUPDATE:
			clear_reboot_counters();
			break;
		default:
			if (reboot_info[idx].shortprintouttext) {
				reboot_info[idx].reboot_count++;
			}
			break;
		}
	}

	avm_reboot_info.next_reboot_status = status;

	/* Without a mailbox, that's all we can do */
	if (!mailbox)
		goto out;

	memset(mailbox, 0, MAILBOX_SIZE);
	memcpy(mailbox, text, len);

	/* Append reboot counter string AFTER terminating zero */
	reboot_counters_str = &mailbox[len];
	len += write_reboot_counters(reboot_counters_str, MAILBOX_SIZE - len);
	if (len > MAILBOX_SIZE) {
		pr_err("Mailbox overflow when saving reboot status, data truncated\n");
		len = MAILBOX_SIZE;
	}

	ret = arch_flush_mailbox(mailbox, len);
	if (ret)
		pr_err("Error flushing mailbox: %d\n", ret);

	debug_mailbox_dump("Writing mailbox: ", mailbox, len);

	if (nofix == 0) {
		pr_err("%s: %s%s%s%s%s%s\n", __func__,
		       reboot_info[idx].printouttext,
		       reboot_info[idx].shortprintouttext ? "(" : "",
		       reboot_info[idx].shortprintouttext ?
			       reboot_info[idx].shortprintouttext :
			       "",
		       reboot_info[idx].shortprintouttext ? ") " : " ",
		       *reboot_counters_str ? " - " : "",
		       reboot_counters_str);
	}
out:
	put_mailbox();
}
/**
 * \brief set actual reboot-status once - following settings will be ignored.
 * special case: status = RS_POWERON - it happens as a cold-start
 * (PCMCR-workarround for GRX)
 * \param status reboot-status
 */
void avm_set_reset_status(enum _avm_reset_status status)
{
	if (status == RS_POWERON) {
		set_reboot_status(1, RS_POWERON);
		return;
	}
	set_reboot_status(0, status);
}
EXPORT_SYMBOL(avm_set_reset_status);

/**
 */
static int avm_reboot_handler(struct notifier_block *nb, unsigned long event,
			      void *buf);
static struct notifier_block avm_reboot_notifier = {
	.notifier_call = avm_reboot_handler, /* callback   */
	.priority = INT_MAX
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
	printk_avm_console_bend(0); /* force serial-output */
	avm_set_reset_status(RS_REBOOT);
	avm_stack_check(NULL);
	return arch_reboot_handler(nb, event, buf);
}

#if defined(CONFIG_AVM_FASTIRQ)
#ifdef CONFIG_ARCH_QCOM
void avm_set_crashed_cpu(int cpu)
{
	SET_CRASHED_CPU(cpu);
}
int avm_get_crashed_cpu(void)
{
	return GET_CRASHED_CPU();
}
#endif /*--- #ifdef CONFIG_ARCH_QCOM ---*/
#endif /*--- #ifdef CONFIG_AVM_FASTIRQ ---*/
static int panic_handler(struct notifier_block *notifier_block,
			 unsigned long event, void *cause_string);
static struct notifier_block panic_notifier = {
	.notifier_call	= panic_handler,
	.next = NULL,
	.priority = 0
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
	avm_oom_show_memstat(0x1); /* show memory depend on situation */
	printk_avm_console_bend(0); /*--- force serial output ---*/
#if defined(arch_panic_notifier)
	return arch_panic_notifier(notifier_block, event, cause_string);
#else /*--- #if #defined(arch_panic_notifier) ---*/
	return NOTIFY_DONE;
#endif /*--- #else ---*/ /*--- #if #defined(arch_panic_notifier) ---*/
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
	console_verbose();
	avm_set_reset_status(RS_OOPS);
	avm_stack_check(NULL);
#if defined(arch_die_notifier)
	return arch_die_notifier(self, cmd, ptr);
#else
	return NOTIFY_OK;
#endif /*--- #if #defined(arch_die_notifier) ---*/
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
static __init int get_reboot_status(void)
{
	unsigned int i, len;
	char Buffer[MAILBOX_SIZE] = {0};
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
	Buffer[MAILBOX_SIZE - 1] = '\0';
	debug_mailbox_dump("Found mailbox: ", Buffer, MAILBOX_SIZE);

	for (i = 0; i < ARRAY_SIZE(reboot_info); i++) {
		if (reboot_info[i].matchtext == NULL) {
			break;
		}
		len = reboot_info[i].matchlen;
		BUG_ON(len > MAILBOX_SIZE - 1);
		if (memcmp(Buffer, reboot_info[i].matchtext, len) == 0) {
			read_firmware_info(&Buffer[len]);
			read_reboot_counters(&Buffer[len]);
#if defined(ATH_NO_NMI)
			/*--- workarround - should be a nmi/freezer  ---*/
			if (reboot_info[i].status == RS_SHORTREBOOT) {
				int idx = get_fieldindex_by_status(RS_NMIWATCHDOG);

				if (idx >= 0) {
					char txt[128];
					unsigned long error_epc =
						read_c0_errorepc();
					unsigned int sticky_reg = ath_reg_rd(
						ATH_RESET_BASE +
						ATH_RESET_REG_STICKY);
					i = idx;
					reboot_info[i].reboot_count++;
					snprintf(
						txt, sizeof(txt),
						"Maybe watchdog without log\nerror-epc=%pS\nsticky-reg=0x%08x\n",
						(void *)error_epc, sticky_reg);
					TFFS3_panic_dummy_log(txt);
				}
			}
#endif /*--- #if defined(ATH_NO_NMI) ---*/
			if (reboot_info[i].status == RS_SHORTREBOOT) {
				reboot_info[i].reboot_count++;
			}
			break;
		}
	}
	avm_reboot_info.status = reboot_info[i].status;
	pr_err("Reboot Status is: %s%s%s%s%s\n",
	       reboot_info[i].printouttext,
	       reboot_info[i].shortprintouttext ? "(" : "",
	       reboot_info[i].shortprintouttext ?
		       reboot_info[i].shortprintouttext :
		       "",
	       reboot_info[i].shortprintouttext ? ") " : " ",
	       avm_rebootcounter_string(Buffer, sizeof(Buffer)));
#if defined(ATH_NO_NMI)
	write_c0_errorepc(0x0);
#endif
no_mailbox:
	prepare_firmware_info();
	set_reboot_status(1, RS_SHORTREBOOT);
	register_die_notifier(&die_nb);
	atomic_notifier_chain_register(&panic_notifier_list, &panic_notifier);
	register_reboot_notifier(&avm_reboot_notifier);
	return 0;
}
#if defined(CONFIG_X86_PUMA7)
fs_initcall(get_reboot_status);
#else
arch_initcall(get_reboot_status);
#endif

static struct proc_dir_entry *rebootprocdir;
#define PROC_REBOOTDIR "avm/reboot"

/**
 */
static char *time_to_ascii(unsigned long local_time, char *buf, int len, char *prefix)
{
	static const char * const month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
					      "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char * const day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	struct rtc_time tm;

	rtc_time_to_tm(local_time, &tm);
	snprintf(buf, len, "%s %s %02d %02d:%02d:%02d %02d UTC %s",
		 day[tm.tm_wday], month[tm.tm_mon],
		 tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900, prefix);
	return buf;
}

/**
 * maxsize of buf: 14 + 10 + 2 + 2 + 2  = 30
 */
static char *human_readable_time(char *buf, unsigned long buf_size, unsigned long time)
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

#define HEADER_FORMAT_A	"UPTIME: %lu\n"

#define HEADER_FORMAT_B	"(%s - panic on %s)\n"

#define HEADER_FORMAT_C "%s %s\n"\
			"HW: %s.%s\n"\
			"FW: %s%s\n"\
			"Bootloader: %s\n"\
			"PANIC LOG VERSION 3.0\n"

#define HEADER_FORMAT       HEADER_FORMAT_A""HEADER_FORMAT_B""HEADER_FORMAT_C

#define avm_reboot_info_str(a) (avm_reboot_info.a[1][0] ?  avm_reboot_info.a[1] : avm_reboot_info.a[0])
#define avm_reboot_info_fwversion_from_mailbox() (avm_reboot_info.fw_version[1][0] ?  1 : 0)

/**
 */
static void proc_reboot_info(struct seq_file *seq, void *priv __maybe_unused)
{
	char reboot_txt[128], *reboot_post_txt = "";
	char huptime[128];
	char htime[32];

	avm_rebootcounter_string(reboot_txt, sizeof(reboot_txt));
	if (reboot_txt[0]) {
		reboot_post_txt = "(since last regular reboot/power-cut)";
	}
	seq_printf(seq, HEADER_FORMAT, avm_reboot_info.uptime,
		   human_readable_time(huptime, sizeof(huptime), (unsigned long)avm_reboot_info.uptime),
		   time_to_ascii(avm_reboot_info.utc, htime, sizeof(htime), ""),
		   reboot_txt, reboot_post_txt,
		   avm_reboot_info_str(hw_version), avm_reboot_info_str(hw_subversion),
		   avm_reboot_info_str(fw_version),
		   avm_reboot_info_fwversion_from_mailbox() ? "" : " (not from mailbox)",
		   avm_reboot_info_str(bootloaderVersion));
}

/**
 */
static __init int proc_reboot_status(void)
{
	rebootprocdir = proc_mkdir(PROC_REBOOTDIR, NULL);
	if (rebootprocdir == NULL) {
		return 0;
	}
	return add_simple_proc_file("avm/reboot/info", NULL, proc_reboot_info,
				    NULL);
}
late_initcall(proc_reboot_status);

/* vim: set ts=4 sw=4 noet: */
