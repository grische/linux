/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_avm_reboot_status_puma6a_h__
#define __arch_avm_reboot_status_puma6a_h__

#define UPDATE_REBOOT_STATUS_TEXT                                              \
	"(c) AVM 2016, Reboot Status is: Firmware-Update"                      \
	"(c) AVM 2016, Reboot Status is: Firmware-Update"                      \
	"(c) AVM 2016, Reboot Status is: Firmware-Update"
#define NMI_REBOOT_STATUS_TEXT                                                 \
	"(c) AVM 2016, Reboot Status is: NMI-Watchdog"                         \
	"(c) AVM 2016, Reboot Status is: NMI-Watchdog"                         \
	"(c) AVM 2016, Reboot Status is: NMI-Watchdog"
#define SOFTWATCHDOG_REBOOT_STATUS_TEXT                                        \
	"(c) AVM 2016, Reboot Status is: Software-Watchdog"                    \
	"(c) AVM 2016, Reboot Status is: Software-Watchdog"                    \
	"(c) AVM 2016, Reboot Status is: Software-Watchdog"
#define POWERON_REBOOT_STATUS_TEXT                                             \
	"(c) AVM 2016, Reboot Status is: Power-On-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Power-On-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Power-On-Reboot"
#define TEMP_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2016, Reboot Status is: Temperature-Reboot"                   \
	"(c) AVM 2016, Reboot Status is: Temperature-Reboot"                   \
	"(c) AVM 2016, Reboot Status is: Temperature-Reboot"
#define SOFT_REBOOT_STATUS_TEXT_PANIC                                          \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot\0(PANIC)"
#define SOFT_REBOOT_STATUS_TEXT_OOM                                            \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot\0(OOM)"
#define SOFT_REBOOT_STATUS_TEXT_OOPS                                           \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot\0(OOPS)"
#define SOFT_REBOOT_STATUS_TEXT_UPDATE                                         \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot\0(RESET-FOR-UPDATE)"
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_LOCAL                                   \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot\0(DOCSIS_LOCAL)"
#define SOFT_REBOOT_STATUS_TEXT_DOCSIS_OPERATOR                                \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot\0(DOCSIS_OPERATOR)"
/*--- Achtung! Untermenge von obigen Eintraegen: ---*/
#define SOFT_REBOOT_STATUS_TEXT                                                \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"                      \
	"(c) AVM 2016, Reboot Status is: Software-Reboot"

/**
 */
static int arch_flush_mailbox(char *mbox, size_t len)
{
	return 0;
}

static char *arch_get_mailbox(void)
{
	return NULL;
}
#endif /*--- #ifndef __arch_avm_reboot_status_puma6a_h__ ---*/
