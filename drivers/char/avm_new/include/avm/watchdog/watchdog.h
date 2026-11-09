/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _linux_ar7wdt_h_
#define _linux_ar7wdt_h_

#define MAX_WDT_NAME_LEN 64
#define MAX_WDT_APPLS    63 /*--- letzte Position ist fuer init-Ueberwachung reserviert ---*/

#if defined(CONFIG_GRX5) || defined(CONFIG_MACH_PUMA7)
#define WDT_DEFAULT_TIME 2
#elif defined(CONFIG_LANTIQ) ||                                                \
	defined(CONFIG_ATH79) || defined(CONFIG_ARCH_IPQ40XX)
#define WDT_DEFAULT_TIME 5
#elif defined(CONFIG_MACH_PUMA6) || defined(CONFIG_MACH_PUMA7) ||              \
	defined(CONFIG_X86) || defined(CONFIG_MACH_BCM963138) ||               \
	defined(CONFIG_BCM963178) || defined(CONFIG_ARCH_AVALANCHE)
#define WDT_DEFAULT_TIME 10
#else
#define WDT_DEFAULT_TIME 20
#endif

struct _ar7wdt_command_queue {
	unsigned int rd, wr;
	unsigned char cmd[8];
};

struct _ar7wdt_appl_data {
	char Name[MAX_WDT_NAME_LEN + 1];
	unsigned int default_time;
	struct timer_list Timer;
	unsigned int *pf_owner;
	struct fasync_struct *fasync;
	wait_queue_head_t wait_queue;
	unsigned int  wait_condition;
	unsigned long req_jiffies;
	unsigned long avg_trigger;
	unsigned long pagefaults;
	unsigned int crashflag;
	unsigned int crashlog_read_forbidden; /*--- falls dieses flag gesetzt, so darf der crash-log nicht lesbar sein ---*/
	unsigned int is_announce;
	pid_t        pid;
};

struct _ar7wdt_data {
	DECLARE_BITMAP(mask, MAX_WDT_APPLS);
	DECLARE_BITMAP(triggered, MAX_WDT_APPLS);
	DECLARE_BITMAP(requested, MAX_WDT_APPLS);
	DECLARE_BITMAP(states, MAX_WDT_APPLS); /*--- timer schon einmal erfolglos abgelaufen ---*/
	unsigned long last_hw_wd_trigger;
	struct _ar7wdt_appl_data appl[MAX_WDT_APPLS + 1];
};

#if IS_ENABLED(CONFIG_AVM_WATCHDOG)
extern int AVM_WATCHDOG_register(int handle, char *name, int len);
extern int AVM_WATCHDOG_announce(int handle, char *name, int len);
extern int AVM_WATCHDOG_release(int handle, char *name, int len);
extern int AVM_WATCHDOG_set_timeout(int handle, char *name, int len);
extern int AVM_WATCHDOG_trigger(int handle, char *name, int len);
extern int AVM_WATCHDOG_disable(int handle, char *name, int len);
extern int AVM_WATCHDOG_read(int handle, char *name, int len);
extern int AVM_WATCHDOG_init_start(int handle, char *name, int len);
extern int AVM_WATCHDOG_init_done(int handle, char *name, int len);
extern int AVM_WATCHDOG_reboot(int handle);
extern int AVM_WATCHDOG_poll(int handle);
extern void AVM_WATCHDOG_ungraceful_release(int handle);
extern void AVM_WATCHDOG_emergency_retrigger(void);

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
 */
extern void AVM_WATCHDOG_init(void);
extern void AVM_WATCHDOG_deinit(void);
extern void AVM_WATCHDOG_OOM_init(void);
extern struct fasync_struct **AVM_WATCHDOG_get_fasync_ptr(int handle);
extern wait_queue_head_t *AVM_WATCHDOG_get_wait_queue(int handle);
extern int AVM_WATCHDOG_wait_event_interruptible(int handle);
extern int ar7wdt_no_reboot;

/**
 */
void ar7wdt_hw_trigger(void);
void ar7wdt_hw_reboot(void);
void ar7wdt_hw_deinit(void);
void ar7wdt_hw_init(void);
int ar7wdt_hw_is_wdt_running(void);
void ar7wdt_hw_secure_wdt_disable(void);

#if defined(CONFIG_AVM_FASTIRQ)
/**
 * der (pseudo-)nmi-handler ueber fastirq
 */
int register_nmi_notifier(struct notifier_block *nb);
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

#define ATH_GET_NMI_MODE
int ath_get_nmi_mode(void);

#else /* #if IS_ENABLED(CONFIG_AVM_WATCHDOG) */

static inline int AVM_WATCHDOG_register(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_announce(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_release(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_set_timeout(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_trigger(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_disable(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_read(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_init_start(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_init_done(int handle, char *name, int len) { return -ENODEV; }
static inline int AVM_WATCHDOG_reboot(int handle) { return -ENODEV; }
static inline int AVM_WATCHDOG_poll(int handle) { return -ENODEV; }
static inline void AVM_WATCHDOG_ungraceful_release(int handle) {}
static inline void AVM_WATCHDOG_emergency_retrigger(void) {}

static inline int AVM_WATCHDOG_Crashlog_notify(void) { return -ENODEV; }

static inline int AVM_WATCHDOG_Crashlog_read_forbidden(void) { return 0; }

static inline void AVM_WATCHDOG_init(void) {}
static inline void AVM_WATCHDOG_deinit(void) {}
static inline void AVM_WATCHDOG_OOM_init(void) {}
static inline struct fasync_struct **AVM_WATCHDOG_get_fasync_ptr(int handle) { return NULL; }
static inline wait_queue_head_t *AVM_WATCHDOG_get_wait_queue(int handle) { return NULL; }
static inline int AVM_WATCHDOG_wait_event_interruptible(int handle) { return -ENODEV; }

#define ar7wdt_no_reboot 2

static inline void ar7wdt_hw_trigger(void) {}
static inline void ar7wdt_hw_reboot(void) {}
static inline void ar7wdt_hw_deinit(void) {}
static inline void ar7wdt_hw_init(void) {}
static inline int ar7wdt_hw_is_wdt_running(void) { return 0; }
static inline void ar7wdt_hw_secure_wdt_disable(void) {}

#if defined(CONFIG_AVM_FASTIRQ)
static inline int register_nmi_notifier(struct notifier_block *nb) { return -ENODEV; }
#endif /*--- #if defined(CONFIG_AVM_FASTIRQ) ---*/

#define ATH_GET_NMI_MODE
static inline int ath_get_nmi_mode(void) { return 0; }
#endif /* #if IS_ENABLED(CONFIG_AVM_WATCHDOG) */

#endif /*--- #ifndef _linux_ar7wdt_h_ ---*/
