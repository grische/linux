/* SPDX-License-Identifier: GPL-2.0+ */
#if defined(CONFIG_GRX5_AVM_FRITZ_BOX)
#define WDT_DEFAULT_TIME 1
#elif defined(CONFIG_MACH_PUMA7) || defined(CONFIG_PRX_AVM_FRITZ_BOX)
#define WDT_DEFAULT_TIME 2
#elif defined(CONFIG_LANTIQ) || \
      defined(CONFIG_ATH79) || \
      defined(CONFIG_ARCH_QCOM) || \
      defined(CONFIG_ARCH_ECONET)
#define WDT_DEFAULT_TIME 5
#elif defined(CONFIG_MACH_PUMA7) ||              \
	defined(CONFIG_X86) || defined(CONFIG_MACH_BCM963138) ||               \
	defined(CONFIG_BCM963178) || defined(CONFIG_BCM96764) ||               \
	defined(CONFIG_BCM96766) || defined(CONFIG_ARCH_AVALANCHE)
#define WDT_DEFAULT_TIME 10
#else
#define WDT_DEFAULT_TIME 20
#endif

extern int AVM_WATCHDOG_register(int handle, char *name, int len);
extern int AVM_WATCHDOG_announce(int handle, char *name, int len);
extern int AVM_WATCHDOG_release(int handle, char *name, int len);
extern int AVM_WATCHDOG_set_timeout(int handle, char *name, int len);
extern int AVM_WATCHDOG_trigger(int handle, char *name, int len);
extern int AVM_WATCHDOG_disable(int handle, char *name, int len);
extern int AVM_WATCHDOG_read(int handle, char *name, int len);
extern int AVM_WATCHDOG_init_start(int handle, char *name, int len);
extern int AVM_WATCHDOG_init_done(int handle, char *name, int len);
extern int AVM_WATCHDOG_poll(int handle);
extern void AVM_WATCHDOG_ungraceful_release(int handle);

extern int AVM_WATCHDOG_init(void);
extern void AVM_WATCHDOG_deinit(void);
extern struct fasync_struct **AVM_WATCHDOG_get_fasync_ptr(int handle);
extern wait_queue_head_t *AVM_WATCHDOG_get_wait_queue(int handle);
extern int AVM_WATCHDOG_wait_event_interruptible(int handle);

/**
 */
void ar7wdt_hw_trigger(void);
void ar7wdt_hw_deinit(void);
void ar7wdt_hw_init(void);
int ar7wdt_hw_initialized(void);

#ifdef CONFIG_AVM_WATCHDOG_STARTED_BY_FW
void ar7wdt_hw_disable_noinit(void);
#else
static inline void ar7wdt_hw_disable_noinit(void) { }
#endif
