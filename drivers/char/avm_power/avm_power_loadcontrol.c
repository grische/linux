/*
 *
 *   Copyright (C) 2011 AVM GmbH <fritzbox_info@avm.de>
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

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <avm/power/power.h>
#include <linux/slab.h>

#pragma GCC pop_options

#include "avm_power.h"

/**
 * sammelt LoadControl-Modules
 */
struct _power_managment_loadcontrol_modules {
	struct _power_managment_loadcontrol_modules *next;
	char *module_name;
	void *context;
	load_control_callback_t load_control_callback;
};

/**
 */
static struct _power_managment_loadcontrol {
	volatile struct _power_managment_loadcontrol_modules *anchor;
	enum _load_control_set loadcontrol_mode;
	int countdown;
	int load_control_val;
	unsigned int load_control_flags;
	int load_control_status;
	int load_control_scale;
} PwLoadControl;

static DEFINE_SPINLOCK(lock);

/**
 */
static inline void lavm_powermanager_load_control_setmoduleval(struct _power_managment_loadcontrol *pwlc, int val)
{
	struct _power_managment_loadcontrol_modules *module;


	/*--- pr_err("[%s] %x\n", __func__, val); ---*/
	module = (struct _power_managment_loadcontrol_modules *)pwlc->anchor;
	while (module) {
		module->load_control_callback(val, module->context);
		module = module->next;
	}
}

/**
 * mode: 0 auto
 * mode: 1 - x manu
 */
void avm_powermanager_load_control_set(enum _load_control_set mode, int scale)
{
	struct _power_managment_loadcontrol *pwlc = &PwLoadControl;
	unsigned long flags;
	int erg;

	if (mode >= load_control_off) {
		erg = min(10, (int)(mode - load_control_off));
		erg |= scale;   /*--- hier als scale = flag ---*/
		pr_info("[loadcontrol] set level to %x\n", erg);
	} else {
		pwlc->load_control_scale = min(8, max(0, scale));
		pr_info("[loadcontrol] set auto - scale=%d\n", pwlc->load_control_scale);
		erg = 0;
	}
	spin_lock_irqsave(&lock, flags);
	lavm_powermanager_load_control_setmoduleval(pwlc, erg);
	pwlc->loadcontrol_mode = mode;
	spin_unlock_irqrestore(&lock, flags);
}

/**
 */
int avm_powermanager_load_control_handler(int run)
{
	struct _power_managment_loadcontrol *pwlc = &PwLoadControl;
	unsigned long flags;
	int load_control_val;

	if (pwlc->loadcontrol_mode != load_control_auto) {
		return 0;
	}
	if (run == 100) {
		pwlc->countdown = min(pwlc->countdown + 1, 10  << pwlc->load_control_scale);
	} else {
		pwlc->countdown = max(pwlc->countdown - ((100 - run) >> 1), 0);
	}
	load_control_val = pwlc->countdown >> pwlc->load_control_scale;
	/*--- pr_info("[%s] run=%d countdown=%d val=%d\n", __func__, run, pwlc->countdown, load_control_val); ---*/
	spin_lock_irqsave(&lock, flags);
	if (pwlc->load_control_val != load_control_val) {
		pwlc->load_control_val = load_control_val;
		lavm_powermanager_load_control_setmoduleval(pwlc, load_control_val | pwlc->load_control_flags);
	}
	spin_unlock_irqrestore(&lock, flags);
	return load_control_val | pwlc->load_control_flags;
}

/**
 * Callback registrieren
 * name: Name des Treibers
 * load_control_callback_t: Callback (s.o.)
 * context: Parameter fuer Callback
 */
void *avm_powermanager_load_control_register(char *name, load_control_callback_t load_control_callback, void *context)
{
	struct _power_managment_loadcontrol *pwlc = &PwLoadControl;
	struct _power_managment_loadcontrol_modules *new;
	unsigned long flags;

	if (name == NULL)
		name = "?";
	if (load_control_callback == NULL) {
		return NULL;
	}
	new = kmalloc(sizeof(struct _power_managment_loadcontrol_modules) + strlen(name) + 1, GFP_KERNEL);
	if (new == NULL) {
		pr_err("[loadcontrol]module %s register failed\n", name);
		return NULL;
	}
	new->module_name = (char *)new + sizeof(struct _power_managment_loadcontrol_modules);
	new->context     = context;
	strcpy(new->module_name, name);
	new->load_control_callback = load_control_callback;
	new->next = NULL;
	spin_lock_irqsave(&lock, flags);
	new->next     = (struct _power_managment_loadcontrol_modules *)pwlc->anchor;
	pwlc->anchor = new;
	spin_unlock_irqrestore(&lock, flags);
	pr_info("[loadcontrol]module %s registered\n", name);
	return new;
}
EXPORT_SYMBOL(avm_powermanager_load_control_register);

/**
 * Load-Control-Callback abmelden
 */
void avm_powermanager_load_control_release(void *handle)
{
	struct _power_managment_loadcontrol *pwlc = &PwLoadControl;
	struct _power_managment_loadcontrol_modules *prevmodule = NULL;
	struct _power_managment_loadcontrol_modules *module;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	module = (struct _power_managment_loadcontrol_modules *)pwlc->anchor;
	while (module) {
		if (module == handle) {
			if (prevmodule == NULL) {
				/*--- erste Element ---*/
				pwlc->anchor = module->next;
			} else {
				prevmodule->next = module->next;
			}
			spin_unlock_irqrestore(&lock, flags);
			pr_info("[loadcontrol]module %s released\n", module->module_name);
			kfree(module);
			return;
		}
		prevmodule = module;
		module     = module->next;
	}
	pr_err("[loadcontrol]module %p release failed\n", handle);
	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(avm_powermanager_load_control_release);

/**
 * setze load_reduce-flags (eingeschraenkt durch mask)
 * Achtung! die unteren 4 Bit sind grundsetzlich nicht aenderbar (load_reduce-Wert) !
 */
void avm_powermanager_load_control_setflags(unsigned int value, unsigned int mask)
{
	struct _power_managment_loadcontrol *pwlc = &PwLoadControl;
	unsigned long flags;
	unsigned int load_control_flags;

	/*--- pr_info("[%s] %d\n", __func__, value); ---*/
	if (pwlc->loadcontrol_mode != load_control_auto) {
		return;
	}
	spin_lock_irqsave(&lock, flags);
	load_control_flags  = pwlc->load_control_flags & ~(mask | LOAD_CONTROL_REDUCEMASK);
	load_control_flags |= value & (mask & ~LOAD_CONTROL_REDUCEMASK);

	if (load_control_flags != pwlc->load_control_flags) {
		pwlc->load_control_flags = load_control_flags;
		lavm_powermanager_load_control_setmoduleval(pwlc, pwlc->load_control_val | load_control_flags);
	}
	spin_unlock_irqrestore(&lock, flags);
}
