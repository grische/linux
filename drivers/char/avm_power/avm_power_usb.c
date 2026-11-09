#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#pragma GCC pop_options

#include <avm/power/power.h>
#include "avm_power.h"

static void *usbpower_handle;

/**
 * state: 0x2   liefere aktuellen USB-Stromverbrauch  (alle Hosts)
 */
static int avm_power_usb_Callback(int state)
{
	/*--- printk("avm_power_usb_Callback: %d\n", state); ---*/
	if (state >= 2) {
		/*--- liefere SUM(mA) zurueck ---*/
		return pm_ressourceinfo.deviceinfo[powerdevice_usb_host].power_rate +
			   pm_ressourceinfo.deviceinfo[powerdevice_usb_host2].power_rate +
			   pm_ressourceinfo.deviceinfo[powerdevice_usb_host3].power_rate;
	}
	return 0;
}

/**
 */
int avm_power_usb_init(void)
{
	usbpower_handle = PowerManagmentRegister("usbpower", avm_power_usb_Callback);
	return usbpower_handle ? 0 : -1;
}
