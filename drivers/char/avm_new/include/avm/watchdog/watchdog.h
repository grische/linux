/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _linux_ar7wdt_h_
#define _linux_ar7wdt_h_

struct notifier_block;

#if IS_ENABLED(CONFIG_AVM_WATCHDOG)
extern void AVM_WATCHDOG_emergency_retrigger(void);
/* An atomic notifier chain, called when the watchdog is about
 * to trigger a reboot */
extern int AVM_WATCHDOG_register_reboot_notifier(struct notifier_block *nb);
extern int AVM_WATCHDOG_unregister_reboot_notifier(struct notifier_block *nb);

/**
 * \brief
 * Wird im TFFS aufgerufen, wenn die current-Applikation einen Crash-Log schreibt
 * \retval 0 current in wdt-List gefunden
 *
 * \note
 * siehe https://wiki.avm.de/display/BA/Crashreport+Sendestrategien
 */
extern int AVM_WATCHDOG_Crashlog_notify(void);

/**
 * \brief
 * Wird im TFFS-Treiber beim Zugriff auf /proc/avm/log_cr/crash aufgerufen.
 * Checkt ob ein Crashlog erzeugt und die entsprechende Applikation anschliessend
 * noch keinen Watchdog getriggert hat. Dann ist sie vermutlich abgestuerzt.
 * Dies soll verhindern, dass ein Crash-Log abgeschickt wird, obwohl
 * eigentlich noch ein panic-Log wegen Watchdogueberwachung folgt (verhindere aufgetrennte CRV's).
 * \retval: 1 - sperre Zugriff auf das Log
 *
 * \note
 * siehe https://wiki.avm.de/display/BA/Crashreport+Sendestrategien
 */
int AVM_WATCHDOG_Crashlog_read_forbidden(void);

/**
 * \brief
 * Abschalten des Watchdogs auf der lokalen CPU
 */
void AVM_WATCHDOG_local_stop(void);

/**
 */
extern int ar7wdt_no_reboot;

#if defined(CONFIG_AVM_FASTIRQ)
/**
 * der (pseudo-)nmi-handler ueber fastirq
 */
int register_nmi_notifier(struct notifier_block *nb);
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/
#define ATH_GET_NMI_MODE
int ath_get_nmi_mode(void);

#else /* #if IS_ENABLED(CONFIG_AVM_WATCHDOG) */


static inline void AVM_WATCHDOG_local_stop(void) {}
static inline void AVM_WATCHDOG_emergency_retrigger(void) {}
static inline int AVM_WATCHDOG_Crashlog_notify(void) { return -ENODEV; }
static inline int AVM_WATCHDOG_Crashlog_read_forbidden(void) { return 0; }
static inline int AVM_WATCHDOG_register_reboot_notifier(struct notifier_block *nb) { return 0; }
static inline int AVM_WATCHDOG_unregister_reboot_notifier(struct notifier_block *nb) { return 0; }

#define ar7wdt_no_reboot 2

#if defined(CONFIG_AVM_FASTIRQ)
static inline int register_nmi_notifier(struct notifier_block *nb) { return -ENODEV; }
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

#define ATH_GET_NMI_MODE
static inline int ath_get_nmi_mode(void) { return 0; }

#endif /* #if IS_ENABLED(CONFIG_AVM_WATCHDOG) */
#endif /*--- #ifndef _linux_ar7wdt_h_ ---*/
