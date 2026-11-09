#include <linux/kernel.h>
/*------------------------------------------------------------------------------------------*\
 * Kernel-Schnittstelle für das neue LED-Modul
\*------------------------------------------------------------------------------------------*/
enum _led_event { /* DUMMY DEFINITION */ LastEvent = 0 };
int (*led_event_action)(int, enum _led_event, unsigned int) = NULL;
EXPORT_SYMBOL(led_event_action);
