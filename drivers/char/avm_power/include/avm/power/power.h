/*
 *
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __linux_avm_power_h__
#define __linux_avm_power_h__

#if defined(CONFIG_AVM_EVENT_20)
#include <avm/event/event.h>
#endif /*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <asm/mach_avm.h>

#define LOAD_CONTROL_REDUCEMASK  0xF
#define LOAD_CONTROL_REDUCE(a)  ((a) & LOAD_CONTROL_REDUCEMASK)
#define LOAD_CONTROL_VOIPCALL   (0x1 << 8)
#define LOAD_CONTROL_MULTICAST  (0x1 << 9)

/*!
 * \brief
 * Callback für Load-Control
 * \param[in] load_reduce: 0 - 10 (0 keine Lastreduzierung, 10 max. Lastreduzierung)
 *			   verodert:  LOAD_CONTROL_VOIPCALL
 *				      LOAD_CONTROL_MULTICAST
 * \param[in] context:     context
 */
typedef void (*load_control_callback_t)(int load_reduce, void *context);

/*!
 * \brief
 * Load-Control-Callback-Schnittstelle
 * \param[in]	name des Treibers/Instanz
 * \param[in]	load_control_callback Callback
 * \param[in]	context
 *
 * \return Handle der Registrierung
 *
 * \retval handle NULL: Fehler
 *
 */
void *avm_powermanager_load_control_register(char *name, load_control_callback_t load_control_callback, void *context) __attribute__ ((weak));

/*! \brief
 * Load-Control-Callback abmelden
 * \param[in] handle von avm_powermanager_load_control_register
 */
void avm_powermanager_load_control_release(void *handle) __attribute__ ((weak));

#if defined(CONFIG_AVM_POWER)
/*!
 * \brief
 * Im power-Treiber sind 'vordefinierte Powermode-Skripts' (siehe avm_power_if.c) vermerkt, die es ermöglichen
 * komplexe (Powermanagment)-Abläufe in der korrekten Reihenfolge auszuführen, und bei Bedarf abzubrechen.
 * Ein Beispiel ist z.B. das Wechseln des Bitfiles, da hier piglet-Treiber, pcmlink und isdn-Treiber in
 * der korrekten Reihenfolge bestimmte Aktionen ausführen müssen.
 *
 * Die jeweiligen Hooks werden mit *dieser* Funktion angemeldet.
 *
 * Melde eine Powermanagment-Callback des Kontextes 'client_name' an.
 * Die client-names sind in den Powermode-Skripts vordefiniert.
 * \see avm_power_if.c
 *
 * \param[in] client_name
 * \param[in] CallBackPowerManagmentControl Callback für Powermanagment-Event
 *
 * \return Handle der Registrierung
 *
 * \retval handle NULL: Fehler (kein Speicher oder doppelt definierter client_name)
 *
 * \note
 * Der State des Callback ist abhaengig vom client_name. Nachfolgend eine Liste der zum jeweilgen clients_name
 * dazugehoerigen States.
 * Ein Returnwert 0 der Callback bedeutet ok. Ansonsten haengt es vom Skript ab,
 * ob das Skript weiter ausgeführt wird (AVM_PM_CB_FAILED).
 *
 * #### client_name: "adsl"
 *  * 0	Power Off
 *  * 1	Low Power (kein DSL)
 *  * 20 nur Request (kein State aendern!): IsATA  Returnwert 0: ja sonst anderer Mode
 *  * 10 Full Speed (jetziger Stand)
 *
 * #### client_name: "piglet"
 *  * 0 normal clocks
 *  * 1 high clocks
 *  * 2 low  clocks
 *  * 0x10 Abfrage ob Piglet unabhängig von Systemfrequenz liefere 1 sonst 0
 *  * verodert mit 0x80: Moeglichkeit testen, wenn möglich, so werden die Codecs allerdings auch gleich gestoppt!

 *  * 0x200: Abfrage "2 Bitfile-Mode"
 *  * 0x201: Abfrage ob TE aktiv (0x81 : verodert mit 0x20, falls tefile geladen, 0x40 falls Auto-Mode aus)
 *  * 0x202: Abfrage ob POTS-File (0 - ja)
 *  * 0x203: Abfrage ob TE-File (0 - ja)
 *  * 0x204: Auto-Mode an
 *  * 0x205: Auto-Mode aus
 *  * 0x210: POTS-File laden
 *  * 0x211: TE-File laden
 *
 *  * 0x400:  Dectsniffer aus
 *  * 0x401:  Dectsniffer an
 *
 *  * 0x1000: Piglet entladen
 *
 * #### client_name: "speedup"
 *  * 0 normal clocks
 *  * 1 high clocks
 *  * 2 low  clocks
 *  * 0x8x setze clock (0,1,2) - ab jetzt kein idleabhängiges Speedup-Control
 *  * 0x10x Setzen der unterstuetzten Modi: (1 Fast, 2 Slow (verodern))
 *  * 0x18x Setzen der unterstuezten Modi - allerdings nur manuelle Switching (idleunabhaengig)
 *  * 0x201 kein speed-down mehr - nur noch speedup (Telefonapplikation)
 *  * 0x200 speed-down entlocken
 *  * 0x40x Abfrage ob diese entsprechende Speedup erlaubt
 *  * 0x801 kein speedup/down       (USB-Treiber)
 *  * 0x800 speed-down entlocken
 *  * 0x1001 kein speedup/down       (ATM-Treiber)
 *  * 0x1000 speed-down entlocken
 *
 *  Initial sind Änderungen an der Clock nicht freigeschaltet
 *
 * #### client_name: "ethernet"
 *  * siehe union  _powermanagment_ethernet_state
 *
 * #### client_name: "isdn"
 *  * 0x10        SLIC 1 aus
 *  * 0x11        SLIC 1 an
 *  * 0x20        SLIC 2 aus
 *  * 0x21        SLIC 2 an
 *  * 0x100       entladen
 *
 *  #### client_name: "pcmlink"
 *  * 0x0:  PCM-Bus deaktivieren (nur Ur8 - für Bitfilechange)
 *  * 0x1:  PCM-Bus  aktivieren   (nur Ur8 - für Bitfilechange)
 *  * 0x100 pcmlink PCM-Bus aus, evtl. DSP's reseten
 *
 *
 * #### client_name: "protrace"
 *   * 0x400:  Protrace aus
 *   * 0x401:  Protrace an
 */
void *PowerManagmentRegister(const char *client_name, int (*CallBackPowerManagmentControl)(int state));

/*!
 * \brief
 * Kontext abmelden
 * \param[in] Handle von PowerManagmentRegister
 */
void PowerManagmentRelease(void *Handle);

#define POWERMANAGEMENT_THROTTLE_ETH

union  _powermanagment_ethernet_state {
	unsigned int Register;
	struct {
		unsigned int port:8;        ///< auszuwaehlender port
		unsigned int status:2;      ///< 0: aus, 1: power_save (+throttle), 2: normal 3: power_save (aber kein throttle)
#if defined(POWERMANAGEMENT_THROTTLE_ETH)
		unsigned int throttle_eth:1; ///< reduziere Speed
		unsigned int reserved:21;
#else/*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
		unsigned int reserved:22;
#endif/*--- #else ---*//*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
	} Bits;
};

/*!
 * \brief
 * Kernelinterface um ein vordefiniertes PowerMode-Script auszuführen (siehe avm_power_if.c)
 * \param[in] powermodename
 *
 * \return ReturnStatus
 * \retval 0: ok sonst Abbruch mit Fehler
 * \note
 * Vom Userland erfolgt das Triggern mittels
 * echo MODE="name"  >/dev/avm_power.
 * \note
 * Nicht alle Skripte sind aus dem Userland triggerbar (PM_ACCESS_DRIVER, PM_ACCESS_APPL).
 *
 * #### definierte PowerMode-Skripte:
 * * "te_active"
 * * "dsl"
 * * "LTE"
 * * "ata"
 * * "vdsl"
 * * "update"
 * * "dect_trace_on"
 * * "dect_trace_off"
 * * "telefon_profile_on"
 * * "telefon_profile_off"
 * * "multicast_profile_on"
 * * "multicast_profile_off"
 * * "voip_profile_on"
 * * "voip_profile_off"
 * * "usb_current_req"
 * * "pcmlink_bus_off"
 * * "pcmlink_bus_on"
 * * "pots_load"
 * * "te_load"
 * * "te_reload"
 * * "pots_reload"
 * * "tepots_switchauto"
 * * "tepots_switchmanu"
 * * "lock_governor"
 * * "unlock_governor"
 * * "e1_active"
 * * "e1_inactive"
 */
int PowerManagmentActivatePowerMode(const char *powermodename);
#else /*--- CONFIG_AVM_POWER ---*/
static inline int PowerManagmentActivatePowerMode(const char *powermodename)
{
	return -ENODEV;
}
static inline void *PowerManagmentRegister(const char *client_name __maybe_unused,
					   int (*CallBackPowerManagmentControl)(int state) __maybe_unused)
{
	return NULL;
}
static inline void PowerManagmentRelease(void *Handle __maybe_unused) {}
#endif /*--- CONFIG_AVM_POWER ---*/

/*! \def PM_WLAN_PARAM
 *  \brief
 *	powerdevice_wlan
 */
#define FREQUENZ_TO_PERCENT(freq, ref_freq)     ((freq) / ((ref_freq) / 100))
#define PM_RATE_MASK                      0xFFFF
#define PM_GET_RATE(param)                (((param)) & PM_RATE_MASK)
#define PM_WLAN_PARAM(eco, devices, rate) (((eco) ? (1 << 31) : 0) | ((devices & 0x7F) << 24) | ((rate) & PM_RATE_MASK))
#define PM_WLAN_GET_ECO(param)            ((param) & (1 << 31) ? 1 : 0)
#define PM_WLAN_GET_DEVICES(param)        (((param) >> 24) & 0x7F)
#define PM_WLAN_TRANSMITPOWER_TO_RATE(rate) ((rate) == 0 ? 0 : (100 >> ((rate) - 1)))

/*! \def PM_USBHOST_DEVICE_1
 *  \brief
 *	powerdevice_usb_host
 */
#define PM_USBHOST_DEVICE_1                   (0x0 << 30)
#define PM_USBHOST_DEVICE_2                   (0x1 << 30)
#define PM_USBHOST_DEVICE_3                   (0x2 << 30)
#define PM_USBHOST_DEVICE_4                   (0x3 << 30)
#define PM_USBHOST_DEVICE_MASK                (0x3 << 30)
#define PM_USBHOST_GET_DEVICES(param)         (param >> 30)

/*! \def PM_DECT_STATUS
 *  \brief
 *	powerdevice_dect
 */
/*--- powerdevice_dect ---*/
#define PM_DECT_STATUS(eco, rate)             (((eco) ? (1 << 31) : 0) | ((rate) & PM_RATE_MASK))
#define PM_DECT_GET_ECO(param)                ((param) & (1 << 31) ? 1 : 0)

/*! \def PM_E1STATUS
 *  \brief
 *	powerdevice_isdnnt /powerdevice_isdnte.
 *	Controller zählt ab 1 !!
 */
#define PM_E1STATUS(Controller)               (1 << (16 + ((Controller) - 1)))
#define PM_ISDN_SET_E1STATUS(Controller)      ((1 << 31) | PM_E1STATUS(Controller))
#define PM_ISDN_RESET_E1STATUS(Controller)    ((0 << 31) | PM_E1STATUS(Controller))

/*! \def PM_E3STATUS
 *  \brief
 *	powerdevice_isdnnt /powerdevice_isdnte.
 *	Controller zählt ab 1 !!
 */
#define PM_E3STATUS(Controller)               (1 << (24 + ((Controller) - 1)))
#define PM_ISDN_SET_E3STATUS(Controller)      ((1 << 31) | PM_E3STATUS(Controller))
#define PM_ISDN_RESET_E3STATUS(Controller)    ((0 << 31) | PM_E3STATUS(Controller))

/*! \def PM_ETHERNET_PARAM
 *  \brief
 *	powerdevice_ethernet
 */
#define PM_ETHERNET_PARAM(devicemask, rate) ((((devicemask) & 0xFF) << 24) | (rate))
#define PM_ETHERNET_GET_DEVICEMASK(param)   (((param) >> 24) & 0xFF)

#if defined(CONFIG_AVM_POWERMETER)
/*!
 * \brief
 * entsprechendes Device liefert geänderte power_rate
 * \param[in] device \see enum _powermanagment_device
 * \param[in] power_rate abhaengig vom enum
 *
 * \return ReturnStatus
 * \retval 0: ok sonst -errno
 *
 * \note
 * Die Auswertung der power_rate ist device-abhängig.
 * \see PM_WLAN_PARAM u.a. defines
 *
 */
int PowerManagmentRessourceInfo(enum _powermanagment_device device, int power_rate);
#else

static inline int PowerManagmentRessourceInfo(int device __maybe_unused,
					      int power_rate __maybe_unused)
{
	return -ENODEV;
}
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/

#ifdef CONFIG_AVM_POWER
/*!
 * \brief
 * Hook für Kernel: vor Idle-Start.
 * Ermöglicht es, die CPU-Last auf Basis von Idle/Performance-Countern zu ermitteln
 */
void avm_cpu_wait_start(void);

/*!
 * \brief
 * depreceated
 */
void avm_cpu_wait_info(void);

/*!
 * \brief
 * Hook für Kernel: nach Aufwachen aus idle-mode (idle_irq_off-Mode) UND im Irq-Kontext.
 * Triggere u.U. PowerManagmentRessourceInfo(powerdevice_loadrate, 100)
 * und die informative system-load-Ausgabe. (Deshalb Irq-Kontext notwendig).
 */
int avm_cpu_wait_end(void);

struct _avm_pagefault {
	atomic_t faultlink;
	int faultlink_max;
};
/**
 * \private
 */
extern DEFINE_PER_CPU(struct _avm_pagefault, avm_pagefault);

/*!
 * \brief
 * Hook für Kernel: start of mm-fault-handler
 * Sammmle Informationen für system-load-Ausgabe
 */
static inline void avm_page_statistic_fault_get(void)
{
	unsigned int cpu = get_cpu();
	struct _avm_pagefault *pgf = per_cpu_ptr(&avm_pagefault, cpu);

	atomic_inc(&pgf->faultlink);
	put_cpu();
}
/*!
 * \brief
 * Hook für Kernel: end of mm-fault-handler
 * Sammmle Informationen für system-load-Ausgabe
 */
static inline void avm_page_statistic_fault_put(void)
{
	unsigned int cpu = get_cpu();
	struct _avm_pagefault *pgf = per_cpu_ptr(&avm_pagefault, cpu);
	int max = atomic_dec_return(&pgf->faultlink) + 1;

	if (max > 0) {
		if (unlikely(max > pgf->faultlink_max)) {
			pgf->faultlink_max = max;
		}
	}
	put_cpu();
}

/*!
 * \brief
 * Callback für Temperatursensor
 * \param[in]  handle
 * \param[in]  context
 * \param[out] value Temperaturwert in 1/10 °C
 *
 * \retval Returnstatus 0: ok
 *         < 0: entspricht -errno (-EAGAIN: es wird der letzte gecachte Temperaturwert genommen)
 *
 */
typedef int (*TemperaturSensorReadTemperatureCallback_t)(void *handle, void *context, int *value);

/*!
 * \brief
 * Temperatur-Sensor anmelden
 * \param[in]	name des Sensors
 * \param[in]	callback Callback
 * \param[in]	context
 *
 * \return Handle der Registrierung
 *
 * \retval handle NULL: Fehler
 *
 */
void *TemperaturSensorRegister(const char *name, TemperaturSensorReadTemperatureCallback_t callback, void *context);

/*!
 * \brief
 * Temperatur-Sensor abmelden
 * \param[in]  handle
 */
void TemperaturSensorDeregister(void *handle);

/*!
 * \brief
 * Liefere Temperatur entsprechend angemeldeten Sensor
 *
 * \param[in] index: z.Z. auf 0 setzten
 * \param[out] name:  liefert einen Zeiger auf den Namen.
 * \param[out] value: Temperaturwert in 1/10°C
 * \retval Returnstatus 0: ok sonst -errno
 */
int TemperatureSensorGetCurrentTemp(int index, const char **name, int *value);

#else /* CONFIG_AVM_POWER */
static inline void avm_cpu_wait_start(void) { }
static inline void avm_cpu_wait_info(void) { }
static inline int avm_cpu_wait_end(void)
{
	return -ENODEV;
}
static inline void avm_page_statistic_fault_get(void) { }
static inline void avm_page_statistic_fault_put(void) { }
static inline void *TemperaturSensorRegister(const char *name __maybe_unused,
					     void *callback __maybe_unused,
					     void *context __maybe_unused)
{
	return NULL;
}

static inline void TemperaturSensorDeregister(void *handle) { }
static inline int TemperatureSensorGetCurrentTemp(int index __maybe_unused, const char **name __maybe_unused,
						  int *value __maybe_unused)
{
	return -ENODEV;
}
#endif /* CONFIG_AVM_POWER */

#if defined(CONFIG_AVM_SOFTIRQ_STATISTIC)
/**
 * \private
 * some lifetime tasklet-statistic
 */
struct _runtime_stat {
	unsigned long start_time;
	unsigned long count;
	unsigned long max_runtime;
	unsigned long long sum_runtime;
	unsigned short vec_nr;
	unsigned short pending;
	void *caller;
};
#define MAX_TASKLET_TRACE_FUNCS		10
/**
 * \private
 * some lifetime tasklet-statistic
 */
struct _softirq_stat {
	unsigned long reset_req_on_jiffies;
	unsigned long measure_ts_start_jiffies;
	struct _runtime_stat sum_softirq;
	struct _runtime_stat softirq[NR_SOFTIRQS];
	struct _runtime_stat func[MAX_TASKLET_TRACE_FUNCS];
	unsigned int func_array_full;
	seqcount_t seq; /* the last element - do not clear with memset */
};
/**
 * \private
 */
extern DEFINE_PER_CPU(struct _softirq_stat, avm_softirqstat);

/**
 * \private
 */
static void __avm_softirq_set_runtime(struct _runtime_stat *pts)
{
	unsigned long diff;

	diff = avm_get_cycles() - pts->start_time;
	pts->count++;
	pts->sum_runtime += diff;
	if (diff > pts->max_runtime) {
		pts->max_runtime = diff;
	}
}

/**
 * \brief
 * Hook for kernel/softirq.c
 */
static inline void avm_sum_softirq_enter(void)
{
	struct _softirq_stat *pss = this_cpu_ptr(&avm_softirqstat);

	if (unlikely(pss->reset_req_on_jiffies)) {
		unsigned long flags;

		/* protect against system-load-analysis */
		local_irq_save(flags);
		write_seqcount_begin(&pss->seq);
		memset(pss, 0, offsetof(struct _softirq_stat, seq));
		pss->measure_ts_start_jiffies = jiffies;
		write_seqcount_end(&pss->seq);
		local_irq_restore(flags);
	}
	pss->sum_softirq.start_time = avm_get_cycles();
}

/**
 * \brief
 * Hook for kernel/softirq.c
 */
static inline void avm_sum_softirq_leave(void)
{
	struct _softirq_stat *pss = this_cpu_ptr(&avm_softirqstat);

	__avm_softirq_set_runtime(&pss->sum_softirq);
}

/**
 * \brief
 * Hook for kernel/softirq.c
 *
 * Enter softirq
 *
 * \param[in]  vec_nr
 * \retval pts for avm_softirq_leave()
 */
static inline struct _runtime_stat *avm_softirq_enter(unsigned int vec_nr)
{
	struct _runtime_stat *pts = &(this_cpu_ptr(&avm_softirqstat)->softirq[vec_nr]);

	pts->start_time = avm_get_cycles();
	pts->pending    = 1;
	return pts;
}

/**
 * \brief
 * Hook for softirq.c
 *
 * Leave softirq
 * \param[in]  pts  from avm_softirq_enter()
 *
 */
static inline void avm_softirq_leave(struct _runtime_stat *pts)
{
	__avm_softirq_set_runtime(pts);
	pts->pending    = 0;
}

/**
 * \private
 */
static struct _runtime_stat *__avm_get_tasklet_func(struct _softirq_stat *pss, void *caller, unsigned short vec_nr)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pss->func); i++) {
		struct _runtime_stat *pts = &pss->func[i];

		if (pts->caller == NULL) {
			pts->caller = caller; /* new entry */
			pts->vec_nr = vec_nr;
			return pts;
		}
		if (pts->caller == caller) {
			return pts;
		}
	}
	return NULL;
}

/**
 * \brief
 * Hook for kernel/softirq.c
 *
 * Enter tasklet or hi_tasklet-function
 * \param[in]  caller
 * \param[in]  vec_nr
 *
 * \retval pts for avm_taskletfunc_leave()
 */
static inline struct _runtime_stat *avm_taskletfunc_enter(void *caller, int vec_nr)
{
	struct _softirq_stat *pss = this_cpu_ptr(&avm_softirqstat);
	struct _runtime_stat *pfunc_stat = __avm_get_tasklet_func(pss, caller, (unsigned short)vec_nr);

	if (pfunc_stat) {
		pfunc_stat->start_time = avm_get_cycles();
		pfunc_stat->pending = 1;
	} else {
		pss->func_array_full = 1;
	}
	return pfunc_stat;
}

/**
 * \brief
 * Hook for kernel/softirq.c
 *
 * Leave tasklet or hi_tasklet-function
 * \param[in]  pfunc_stat from avm_taskletfunc_enter()
 *
 */
static inline void avm_taskletfunc_leave(struct _runtime_stat *pfunc_stat)
{
	if (pfunc_stat) {
		__avm_softirq_set_runtime(pfunc_stat);
		pfunc_stat->pending = 0;
	}
}
#else/* CONFIG_AVM_SOFTIRQ_STATISTIC */
struct _runtime_stat;

static inline void avm_sum_softirq_enter(void) {}
static inline void avm_sum_softirq_leave(void) {}

static inline struct _runtime_stat *avm_softirq_enter(unsigned int vec_nr __maybe_unused) { return NULL; }
static inline void avm_softirq_leave(struct _runtime_stat *pts __maybe_unused) {}

static inline struct _runtime_stat *avm_taskletfunc_enter(void *caller __maybe_unused, int vec_nr __maybe_unused) { return NULL; }
static inline void avm_taskletfunc_leave(struct _runtime_stat *pfunc_stat __maybe_unused) {}

#endif /* CONFIG_AVM_SOFTIRQ_STATISTIC */

#endif/*--- #ifndef __linux_avm_power_h__ ---*/
