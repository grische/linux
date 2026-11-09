/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __avm_reboot_status_h__
#define __avm_reboot_status_h__
#include <avm/enh/fw_info.h>

/**
 */
enum _avm_reset_status {
	RS_UNSPECIFIED,
	RS_POWERON,
	RS_SOFTWATCHDOG,	/*--- Userland-Watchdog because Application do not trigger ---*/
	RS_NMIWATCHDOG,		/*--- Watchdog over NMI ---*/
	RS_REBOOT,
	RS_FIRMWAREUPDATE,
	RS_SHORTREBOOT,		/*--- short powercut or devil reboot without nmi-handling ---*/
	RS_BUSERROR,		/*--- special case for atheros ---*/
	RS_TEMP_REBOOT,		/*--- temperature - cpu to hot! ---*/
	RS_PANIC,		/*--- panic occurred  ---*/
	RS_OOM,			/*--- oom occurred  ---*/
	RS_OOPS,		/*--- oops occurred  ---*/
	RS_DYING_GASP,		/*    Voltage drop detected, maybe warm reset */
	RS_DOCSIS_LOCAL,	/*    Reboot requested by local DOCSIS software  */
	RS_DOCSIS_OPERATOR,	/*    Reboot requested by DOCSIS operator (provider) */
	RS_BOXCHANGE,           /*    Reboot after user requested boxchange */
	RS_OPERATOR,            /*    Reboot requested by (generic) operator */
};

struct avm_firmware_uptime {
	unsigned long uptime;
	time64_t      time;
};

struct avm_reboot_info {
	enum _avm_reset_status	    reboot_status;
	struct avm_firmware_uptime  tm;
	struct avm_firmware_info    fw;
};

struct avm_pltfm_reset_status {
	enum _avm_reset_status status;
	const char *message;
};

#if IS_ENABLED(CONFIG_AVM_REBOOT_STATUS)
extern enum _avm_reset_status avm_reset_status(void);
extern enum _avm_reset_status avm_next_reset_status(void);
/**
 * ein zweites Setzen des Status wird ignoriert !
 */
extern void avm_set_reset_status(enum _avm_reset_status status);

#ifdef CONFIG_AVM_PLATFORM_RESET_STATUS
extern void avm_pltfm_set_reset_status(const struct avm_pltfm_reset_status *prs);
#endif

extern void avm_pltfm_reboot_status_notify(enum _avm_reset_status status);

extern bool rebooting_after_panic(void);
extern bool avm_reset_status_is_provisional(enum _avm_reset_status status);

/* sysctl access */
extern int avm_sysctl_set_reset_status(const char *status);
extern const char *avm_sysctl_reset_status(void);

extern void avm_get_firmware_uptime(struct avm_firmware_uptime *tm);
#define AVM_REBOOT_INFO_SIZE   400
extern size_t avm_snprint_reboot_info(const struct avm_reboot_info *info,
				      char *buf, size_t bufsiz);

#else
static inline enum _avm_reset_status avm_reset_status(void) { return RS_REBOOT; }
static inline enum _avm_reset_status avm_next_reset_status(void) { return RS_REBOOT; }
static inline void avm_set_reset_status(enum _avm_reset_status status) {}

static inline bool rebooting_after_panic(void) { return false; }
static inline bool avm_reset_status_is_provisional(enum _avm_reset_status status) { return false; }

static inline int avm_sysctl_set_reset_status(const char *status) { return 0; }
static inline const char *avm_sysctl_reset_status(void) { return ""; }

static inline void avm_get_firmware_uptime(struct avm_firmware_uptime *tm) {};
static inline size_t avm_snprint_reboot_info(const struct avm_reboot_info *info,
					     char *buf, size_t bufsiz) { return 0; }
#endif

#endif /*--- #ifndef __avm_reboot_status_h__ ---*/
