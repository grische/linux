/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _uapi_avm_linux_event_h_
#define _uapi_avm_linux_event_h_

#ifndef __KERNEL__	/* Fallback for undefined IFNAMSIZ */
/* nsc@avm: userspace headers net/if.h and linux/if.h bite each other.
 * To allow continuing fast-forward development we assume IFNAMSIZ to
 * stay 16; further we add a warning if neither <net/if.h> nor
 * <linux/if.h> is included.
 *
 * Iff one of these _is_ included before avm_event.h, we check whether
 * our IFNAMSIZ-stays-16 assumption is still valid.
 */

#define _avm_event_ifnamsiz 16

#ifndef IFNAMSIZ
# warning linux/avm_event.h requires pre-inclusion of <net/if.h> _or_ <linux/if.h>!
# define IFNAMSIZ _avm_event_ifnamsiz
# define _avm_event_supplied_ifnamsiz_ 1
#elif _avm_event_ifnamsiz != IFNAMSIZ
# error IFNAMSIZ constrained failed: IFNAMSIZ changed its value (was: 16 Byte)!
#endif
#endif /* __KERNEL__ -- Fallback for undefined IFNAMSIZ */

#include <avm/event/gen-types.h>

#if defined(__KERNEL__)
#include <asm/ioctl.h>
#include <linux/version.h>
#endif /*--- #if defined(__KERNEL__) ---*/

#define MAX_EVENT_CLIENT_NAME_LEN 32
#define MAX_EVENT_SOURCE_NAME_LEN 32
#define MAX_EVENT_NODE_NAME_LEN 32
#define MAX_AVM_EVENT_SOURCES 32
#define MAX_AVM_EVENT_NODES 32

/* This define identifies that the power manager can deliver cpu idle and temperature information
 * See the structures _avm_event_cpu_idle and _avm_event_temperature below.
 */
#define AVM_EVENT_HAVE_CPU_TEMPERATURE 1
/**
 * Aufteilung der Event typen (vorlaeufig in 8er Gruppen):
 *
 *  0 -  7:   WLAN
 *  8 - 15:   DSL
 * 16 - 23:
 * 24 - 31:
 * 32 - 39:   Ethernet
 * 40 - 47:
 * 48 - 55:
 * 56 - 63:
 */
#define AVM_EVENT_TYPE_WLAN_CLIENT_CHANGE 0
#define AVM_EVENT_TYPE_WLAN_ERROR         1

#if defined(__KERNEL__)
/**
 *   Kernel Schnittstelle des avm_event Treibers
 */

/*--- avm_event_mask_fieldentry mask[(avm_event_last - 1) / (sizeof(avm_event_mask_fieldentry) * 8) + 1]; ---*/

/**
 * Baut die Event-Mask zusammen
 * num_ids: Anzahl der folgenden num_ids
 */
struct _avm_event_id_mask *
avm_event_build_id_mask(struct _avm_event_id_mask *id_mask, int num_ids, ...);

/**
 *   Jede Event/Informations Quelle muss sich einmalig registrieren.
 *
 *   Parameter:   name      ein frei zu waehlender Name
 *                id_mask   Maskenstruktur, die mit  avm_event_build_id_mask() erzeugt wird fuer
 *                          alle Ids zu der der Registrierende Informationen liefern will
 *                notify    Diese Funktion wird durch den Event Treiber immer dann
 *                          aufgerufen, wenn sich a) ein Empfaenger, der Informationen von
 *                          diesem Treiber haben will, registriert hat oder b) wenn ein
 *                          schon registrierter Empfaenger eine Aktualisierung der Daten
 *                          wuenscht (avm_event_trigger())
 *                          Parameter dieser Funktion sind: 1) der beim Registrieren
 *                          uebergebene Kontext, und 2) die Id zu der Informationen gewuenscht
 *                          sind.
 *                Context   (siehe notify)
 *
 *   Returnwert:            NULL im Fehlerfall, andernfalls ein gueltiges Handle
 */
extern void *
avm_event_source_register(char *name, struct _avm_event_id_mask *id_mask,
			  void (*notify)(void *, enum _avm_event_id),
			  void *Context);

/**
 *   Deregistrieren, anschliessend ist der Handle ungueltig
 */
extern void avm_event_source_release(void *handle);

/**
 *   Immer wenn Informationen an den Event Treiber uebergeben werden sollen, muss diese
 *   Funktion aufgerufen werden.
 *
 *   Parameter:   Handle   Handle von avm_event_source_register()
 *
 *                Id       Id des Events/Information
 *                data_length   Laenge der Daten/Informationen
 *                data     Pointer auf die Daten. Dieser MUSS mittels kmalloc alloziiert
 *                         worden sein.
 *   Returnwert:  Anzahl der Empfaenger die sich fuer die Daten interessierten (0 == keiner)
 */
extern int avm_event_source_trigger(void *handle, enum _avm_event_id id,
				    unsigned int data_length, void *data);

/**
 * Ermoeglicht es die Nutzung eines Buttons extern an/abzuschalten und entsprechend zu
 * konfigurieren
 * Dies ist z.B. notwendig, wenn es sich bei dem Button um kein GPIO, sondern einem FPGA-Register
 * handelt, der erst nach laden des FPGA's zur Verfügung steht.
 * name: Suchname der Schalters
 * enable:             0 aus, 1 an: Übernahme der Werte nur bei enable = 1
 *                     2 keine Übernahme aber enable
 * gpio:               GPIO/Register-Bit
 * register_address:   != NULL Register-Adresse anstelle GPIO
 * button_type:        0 = Taster, 1 = Schalter
 * Returnwert: 0: ok und gesetzt 1 Schalter nicht gefunden
 */
extern int avm_event_push_button_ctrl(char *name, unsigned int enable,
				      unsigned int gpio,
				      volatile unsigned int *register_addr,
				      unsigned int button_type);

/**
 * Eventsenke aus dem Kernelkontext
 *   Parameter:   name              ein frei zu waehlender Name
 *                id_mask			Maskenstruktur, die mit  avm_event_build_id_mask() erzeugt wird fuer
 *								    alle Ids dessen Events empfangen werden sollen
 *                event_received    Diese Funktion wird durch den Event Treiber immer dann
 *                                  aufgerufen, wenn ein Event entsprechend id_mask
 *                                  existiert
 *                                  Der Kontext dieser Callback ist eine Workqueue,
 *                                  Parameter: context -Kontext
 *                                             buf     -Zeiger auf event-struct (aligned)
 *                                             len     -Laenge der event-struct
 *
 *                context           (siehe event_received)
 *  Reurnwert:    Handle
 */
void *avm_event_sink_register(char *name, struct _avm_event_id_mask *id_mask,
			      void (*event_received)(void *context,
						     unsigned char *buf,
						     unsigned int len),
			      void *context);

/**
 *   Deregistrieren, anschliessend ist der Handle ungueltig
 */
extern void avm_event_sink_release(void *handle);

/**
 *   Pruefen ob sich jemand fuer die Daten interessiert
 */
extern int avm_event_source_check_id(void *handle, enum _avm_event_id id);

#endif /*--- #if defined(__KERNEL__) ---*/

#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)

#if defined(CONFIG_AVM_EVENTNODE_PUMA6)
#define ARM_MEM_OFFSET 0x40000000
#else
#define ARM_MEM_OFFSET 0x0
#endif
int avm_event_node_established(
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2),
	void *context, unsigned int param1, unsigned int param2);
#define FLAG_REMOTELOG_REBOOT 0x1
#define FLAG_REMOTELOG_APPEND 0x2
#define FLAG_REMOTELOG_APPEND_FINISHED (0x4 | FLAG_REMOTELOG_APPEND)
void avm_event_send_log(enum _avm_logtype type, unsigned int flag,
			const char *buffer, unsigned int len);

/**
 * initial auszufuehren um net_notifier zu installieren
 */
void remotedev_net_init(char *remote_device);
/**
 * erst wenn alle server beendet kann netdev unregistriert werden !
 */
void remotedev_net_exit(void);

/**
 * Remote-Device-Server mit ETH-Protocol h_proto erzeugen
 * wenn auf der Remote-Seite ein open gemacht wird -> callback remotedev_open() wird getriggert
 * h_proto: ETH-Protokoll
 * ret: NULL Verbindung ablehnen, sonst rhandle
 * nachfolgend kommen alle Daten des remotedev per remotedev_rcv() herein
 * wenn Remote-Seite Device schliesst so erfolgt remotedev_close()
 *
 * Der Kontext der Callbacks ist fein saeuberlich serialisert ein rx-worker-thread
 */
int remotedev_server_create(
	unsigned short h_proto,
	/**
			 * Callback Remotseite macht open()
			 * conn:    Connection-Handle
			 * h_proto: ETH-Protokoll
			 * ret: NULL Verbindung ablehnen, sonst rhandle
			 */
	void *(*remotedev_open)(unsigned short h_proto),
	/**
			 * Callback RemoteSeite macht write()
			 * rhandle: Handle von remotedev_open()
			 * buf, len: die Daten
			 */
	void (*remotedev_rcv)(void *rhandle, unsigned char *buf,
			      unsigned int len),
	/**
			 * optional: Callback RemoteSeite macht read mit angeforderter Laenge
			 * rhandle: Handle von remotedev_open()
			 * len: die angeforderte Laenge
			 */
	void (*remotedev_demand)(void *rhandle, unsigned int len),
	/**
			 * Callback RemoteSeite: macht close()
			 * internal_error: 0 von Gegenseite, sonst internes Problem
			 */
	void (*remotedev_close)(void *rhandle, int internal_error));

/**
 * senden von Daten - koennen auf Remote-Seite dann per read aus Device geholt werden
 * rhandle: Handle vom open
 * ret: < 0 Fehler bzw. gesendete Byte (immer komplett)
 *
 * Kontext: nur Kernelthread/Workqueue erlaubt
 */
int remotedev_write(void *rhandle, unsigned short h_proto, unsigned char *buf,
		    unsigned int len);

/**
 * Remote-Device-Server mit ETH-Protocol h_proto abraeumen
 */
void remotedev_server_delete(unsigned short h_proto);

/**
 * ist in den arm2atom/atom2arm-irq einzuklinken:
 */
unsigned int handle_remotecpuirq(unsigned int cause);

/**
 * Schnittstelle um Remote-Trigger-Irqs (bis zu 16) generisch nutzen zu koennen
 * allerdings ist IRQ 0 fuer REBOOT-Signalisierung reserviert
 * Returncode vom Handler wird NICHT ausgewertet (dient nur zur Komaptibilität mit request_irq())
 * -> gleicher Handler verwendbar
 */
#define AVM_EVENT_REMOTECPU_IRQ   0x1
#define AVM_CPUCONN_REMOTECPU_IRQ 0x2
#define AVM_PCMLINK_REMOTECPU_IRQ 0x3
/**
 * like request_irq ...
 */
int request_remotecpu_irqhandler(int irq,
				 int (*remotecpu_handler)(int irq, void *ref),
				 void *ref);
int free_remotecpu_irqhandler(int irq, void *ref);
void enable_remotecpu_irqhandler(int irq);
void disable_remotecpu_irqhandler(int irq);

/**
 * Remote-CPU triggern
 */
void trigger_remotecpuirq(unsigned int irq);
#else /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA6) ---*/
/**
 */
static inline int avm_event_node_established(
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2),
	void *context, unsigned int param1, unsigned int param2)
{
	if (event_established_cb)
		event_established_cb(context, param1, param2);
	return 0;
}
#endif
/**
 * user event structures
 */
#if defined(__KERNEL__)
void avm_event_push_button_deinit(void);
int avm_event_push_button_init(void);
#endif /*--- #if defined(__KERNEL__) ---*/
#endif /*--- #ifndef _uapi_avm_linux_event_h_ ---*/
