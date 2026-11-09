/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _linux_avm_led_event_h_
#define _linux_avm_led_event_h_

#include <uapi/avm/event/led.h>
#include <linux/errno.h>

/* event-driven LEDs */
extern int (*led_event_action)(int version, enum _led_event event,
			       unsigned int value);
void led_event_disable_timer(void) __weak;
char *avm_get_led_event_name(enum _led_event event);

/* externally driven LEDs */
enum _led_action {
	led_ext_enable,
	led_ext_disable,
	led_ext_on,
	led_ext_off,
	led_ext_error
};

enum _led_groups {
	led_cpu_gpio_leds = 0,
	led_cpu_shift_leds = 1,
	led_wlan_leds = 2,
	led_lan_leds = 3,
	led_dsl_leds = 4,
	led_dect_leds = 5,
	led_usb_leds = 6,
};

typedef void (*led_callback_t)(enum _led_groups group_id, unsigned int led_id,
			       enum _led_action action);
typedef int (*led_action_t)(unsigned int led_id, enum _led_action action,
			    led_callback_t callback_func);

int __led_register_external(enum _led_groups group_id,
			    led_action_t action_func) __weak;
void __led_release_external(enum _led_groups group_id) __weak;

/*
 * Registriert zur group_id eine entsprechende Action-Funktion und gibt 0 bei Erfolg zurück
 * Die Action-Funktion muss im Interrupt-Kontext aufrufbar sein und sollte größere Aktionen
 * asynchron ausführen.
 * Parameter der Action Funktion sind:
 *   - led_id        => 0..99
 *   - action        => led_ext_enable/led_ext_disable ermöglichen eine De/Initialisierung der
 *                      LED-Ansteuerung; led_ext_error braucht hier nicht behandelt werden
 *   - callback_func => falls die Aktion asychron ausgeführt wird, soll callback_func()
 *                      anschließen daufgerufen werden (im Fehlerfall mit action = led_ext_error)
 *                      callback_func kann auch ein NULL-Pointer sein
 *   Erwartete Rückgabewerte der Funktion sind
 *     0       Aktion wurde gleich ausgeführt
 *     -EBUSY  Aktion wird asynchron ausgeführt und später per Aufruf von callback_func quittiert
 *     -EIO    Aktion ist fehlgeschlagen (z.B. ungültige led_id)
 *
 */
static inline int led_register_external(enum _led_groups group_id,
					led_action_t action_func)
{
	if (&__led_register_external)
		return __led_register_external(group_id, action_func);

	return -ENODEV;
}

static inline void led_release_external(enum _led_groups group_id)
{
	if (&__led_release_external)
		__led_release_external(group_id);
}

#endif
