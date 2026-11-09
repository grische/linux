#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <avm/power/power.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>

#include "avm_power.h"
#if defined(CONFIG_AVM_PA)
#include <avm/pa/avm_pa.h>
#endif /*--- defined(CONFIG_AVM_PA) ---*/


#if defined(DECTSYNC_PATCH)
#include "dectsync.h"

/**
 */
static struct resource dectsync_gpioressource = {
	.name  = "dectsync",
	.flags = IORESOURCE_IO,
	.start = DECT_SYNCGPIO,
	.end   = DECT_SYNCGPIO,
};
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/

/**
 */
static struct _telefonieprofile {
	void *handle;
	void *powerhandle;
	unsigned int on;
	unsigned int dectsyncmode;
} telefonevent;
#if IS_ENABLED(CONFIG_AVM_EVENT)

/**
 */
static void avmevent_telefonprofile_notify(void *context, enum _avm_event_id id)
{
	struct _telefonieprofile *ptp = (struct _telefonieprofile *)context;
	struct _avm_event_telefonprofile *event;
	int handled;

	if (id != avm_event_id_telefonprofile) {
		pr_warn("[avm_power]unknown event: %d\n", id);
		return;
	}
	event = kmalloc(sizeof(struct _avm_event_telefonprofile), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("[avm_power]can't alloc event: %d\n", id);
		return;
	}
	event->event_header.id = id;
	event->on              = ptp->on;
	handled = avm_event_source_trigger(ptp->handle, id, sizeof(struct _avm_event_telefonprofile), event);
	if (handled == 0) {
		/*--- pr_warn("[avm_power]event: %d not handled\n", id); ---*/
	}
}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
static int avm_power_telefonprofile_Callback(int state)
{
	if (state & LOAD_CONTROL_VOIPCALL) {
		avm_powermanager_load_control_setflags(state & 0x1 ? LOAD_CONTROL_VOIPCALL : 0,
							LOAD_CONTROL_VOIPCALL);
		return 0;
	}
	if (state & LOAD_CONTROL_MULTICAST) {
		avm_powermanager_load_control_setflags(state & 0x1 ? LOAD_CONTROL_MULTICAST : 0,
							LOAD_CONTROL_MULTICAST);
		return 0;
	}
	if (telefonevent.on != (unsigned int)state) {
		telefonevent.on = state;
#if defined(DECTSYNC_PATCH)
		if (telefonevent.dectsyncmode) {
			state = 0;
		}
		start_dectsync(0, state);
#endif/*--- #if defined(DECTSYNC_PATCH)  ---*/
#if IS_ENABLED(CONFIG_AVM_EVENT)
		avmevent_telefonprofile_notify(&telefonevent, avm_event_id_telefonprofile);
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
#if defined(CONFIG_AVM_PA)
		avm_pa_telefon_state(telefonevent.on);
#endif /*--- defined(CONFIG_AVM_PA) ---*/
	}
	return 0;
}

/**
 */
void avm_power_dect_syncmode(unsigned int mode)
{
	telefonevent.dectsyncmode = mode;    /*--- im Fall der Telefonie: nicht/doch auf Run anderer achten! ---*/
}

/**
 */
int avm_power_telefon_init(void)
{
	struct _avm_event_id_mask id_mask;
	int ret = 0;
#if defined(DECTSYNC_PATCH)
	if (request_resource(&gpio_resource, &dectsync_gpioressource)) {
		panic("[avmpower]bye bye - can't initialize avmpower-interface!\n");
	}
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/
#if IS_ENABLED(CONFIG_AVM_EVENT)
	telefonevent.handle =  avm_event_source_register("telefonprofile",
						   avm_event_build_id_mask(&id_mask, 1, avm_event_id_telefonprofile),
						   avmevent_telefonprofile_notify,
						   &telefonevent
						   );
	if (telefonevent.handle == NULL) {
		ret = -1;
		pr_err("[avm_power] %s register failed !\n", __func__);
	}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	telefonevent.powerhandle = PowerManagmentRegister("telefon_profile", avm_power_telefonprofile_Callback);
	if (telefonevent.handle == NULL) {
		ret = -1;
		pr_warn("[avm_power] %s PowerManagmentRegister failed !\n", __func__);
	}
	return ret;
}
