/**
 */
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#pragma GCC pop_options
#include <avm/power/power.h>
#include "avm_power.h"

/**
 */
static struct _powermanagment_status_event {
	void *handle;
	void *powerhandle;
	int dsl_status;
} powermanagment_status_event;

#if IS_ENABLED(CONFIG_AVM_EVENT)

/**
 */
static void avm_event_powermanagment_status_notify(void *context, enum _avm_event_id id)
{
	struct _powermanagment_status_event *ppm = (struct _powermanagment_status_event *)context;
	struct _avm_event_powermanagment_status *event;
	int handled;

	if (id != avm_event_id_powermanagment_status) {
		pr_warn("[avm_power]unknown event: %d\n", id);
		return;
	}
	event = kmalloc(sizeof(struct _avm_event_powermanagment_status), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("[avm_power]can't alloc event: %d\n", id);
		return;
	}
	event->event_header.id = id;
	event->substatus = dsl_status;
	event->param.dsl_status = ppm->dsl_status;
	handled = avm_event_source_trigger(ppm->handle, id, sizeof(struct _avm_event_powermanagment_status), event);
	if (handled == 0) {
		/*--- pr_warn("[avm_power]event: %d not handled\n", id); ---*/
	}
}
#endif /*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
static int avm_power_adsl_event_Callback(int state)
{
	if (powermanagment_status_event.dsl_status == state) {
		/*--- keine gleichen Werte triggern ---*/
		return 1;
	}
	powermanagment_status_event.dsl_status = state;
	/*--- pr_warn("avm_power_adsl_event_Callback: %d\n", state); ---*/
#if IS_ENABLED(CONFIG_AVM_EVENT)
	avm_event_powermanagment_status_notify(&powermanagment_status_event, avm_event_id_powermanagment_status);
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	if (pm_ressourceinfo.kthread) {
		/*--- triggere thread  ---*/
		pm_ressourceinfo.Changes++;
		wake_up_interruptible(&pm_ressourceinfo.wait_queue);
	}
	return 0;
}

/**
 */
int avm_power_dsl_init(void)
{
	struct _avm_event_id_mask id_mask;
	int ret = 0;
#if IS_ENABLED(CONFIG_AVM_EVENT)
	powermanagment_status_event.handle = avm_event_source_register("powermanagment_status",
									avm_event_build_id_mask(&id_mask, 1, avm_event_id_powermanagment_status),
									avm_event_powermanagment_status_notify,
									&powermanagment_status_event
									);
	if (powermanagment_status_event.handle == NULL) {
		pr_err("[avm_power] %s source register failed !\n", __func__);
		ret = -1;
	}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	powermanagment_status_event.powerhandle = PowerManagmentRegister("adsl_event", avm_power_adsl_event_Callback);        /*--- nun wird zusaetzlich ein Event erzeugt ---*/
	if (powermanagment_status_event.powerhandle == NULL) {
		pr_err("[avm_power] %s PowerManagmentRegister failed !\n", __func__);
		ret = -1;
	}
	return ret;
}
