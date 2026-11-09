/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __avm_reboot_status_h__
#define __avm_reboot_status_h__

/**
 */
enum _avm_reset_status {
	RS_POWERON		= 0,
	RS_SOFTWATCHDOG		= 1,  /*--- Userland-Watchdog because Application do not trigger ---*/
	RS_NMIWATCHDOG		= 2,  /*--- Watchdog over NMI ---*/
	RS_REBOOT		= 3,
	RS_NMI_WA		= 4,  /*--- only atheros: NMI-Workarround ---*/
	RS_FIRMWAREUPDATE	= 5,
	RS_SHORTREBOOT		= 6,  /*--- short powercut or devil reboot without nmi-handling ---*/
	RS_BUSERROR		= 7,  /*--- special case for atheros ---*/
	RS_TEMP_REBOOT		= 8,  /*--- temperature - cpu to hot! ---*/
	RS_PANIC		= 9,  /*--- panic occurred  ---*/
	RS_OOM			= 10, /*--- oom occurred  ---*/
	RS_OOPS			= 11, /*--- oops occurred  ---*/
	RS_REBOOT_FOR_UPDATE	= 12, /*--- reboot for a clean system - then update ---*/
	RS_DOCSIS_LOCAL		= 13, /*    Reboot requested by local DOCSIS software  */
	RS_DOCSIS_OPERATOR	= 14, /*    Reboot requested by DOCSIS operator (provider) */
};

extern enum _avm_reset_status avm_reset_status(void);
extern enum _avm_reset_status avm_next_reset_status(void);
/**
 * ein zweites Setzen des Status wird ignoriert !
 */
extern void avm_set_reset_status(enum _avm_reset_status status);
/**
 * Anzahl der (ungewollten) Reboots
 */
extern char *avm_rebootcounter_string(char *txt, unsigned int txtlen);
#define avm_rebootcounter_string avm_rebootcounter_string
void avm_rebootcounter_reset(void);

#endif /*--- #ifndef __avm_reboot_status_h__ ---*/
