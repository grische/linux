/*
 *
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
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
 *
 *   Export-Funktionen (fuer Local)
		 PowerManagmentRessourceInfo
		 PowerManagmentActivatePowerMode
		 PowerManagmentRegister
		 PowerManagmentRelease
 */
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <avm/power/power.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/loadavg.h>
#endif
#include <avm/sammel/simple_proc.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#pragma GCC pop_options
#include <avm/rte/rte.h>

#include "avm_power.h"

static volatile struct _power_managment_clients *PwClientAnker;

/*--- #define DEBUG_TRACE_POWERTAB ---*/
#if defined(DEBUG_TRACE_POWERTAB)
#define DEB_TRC_PT      DEB_ERR
#else/*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/
#define DEB_TRC_PT(args...)
#endif/*--- #else ---*//*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/

static DEFINE_SPINLOCK(gpw_client_lock);
static DEFINE_SPINLOCK(avmpower_lock);

/**
 * Liste von registrierten Treibern, die den PowermanagmentCallback-Befehl bekommen
 */
static struct _power_managment_dest_entry dslmode_entries[] =  {
	{ "adsl_event", AVM_PM_CB_UNINSTALLED_OR_FAILED, 10 },  /*--- DSL an: erzeuge Event ---*/
	{ "adsl",       AVM_PM_CB_UNINSTALLED_OR_FAILED, 10 },  /*--- DSL an ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry atamode_entries[] =  {
	{ "adsl_event",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 1 },   /*--- DSL komplett aus: erzeuge Event ---*/
	{ "adsl",        AVM_PM_CB_IGNORE,                1 },   /*--- DSL komplett aus ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry vdslmode_entries[] =  {
	{ "adsl_event",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 10 },     /*--- VDSL oder DSL (FUSIV) an: erzeuge Event ---*/
	{ "adsl",        AVM_PM_CB_IGNORE,                 3 },     /*--- (VDSL) OHIO/UR8-DSL aus aber Spannung an ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry lteonlymode_entries[] =  {
	{ "adsl_event",  AVM_PM_CB_UNINSTALLED_OR_FAILED,  1 },     /*--- DSL komplett aus: erzeuge Event ---*/
	{ "adsl",        AVM_PM_CB_IGNORE,                 1 },     /*--- DSL komplett aus ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry updatemode_entries[] =  {
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry lock_governor[] =  {
	{ "speedstep",       AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x1 },  /*--- pause governor ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry unlock_governor[] =  {
	{ "speedstep",       AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x2 },                   /*--- restart governor ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry increase_min_freq[] =  {
	{ "speedstep",       AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x81 },  /*--- JZ-33133 (GRX): 200 MHz sind zu lahm  - minfreq anheben---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry default_min_freq[] =  {
	{ "speedstep",       AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x80 }, /*--- JZ-33133: wieder minimale CPU-Frequenz ---*/
	{ NULL, 0, 0 }
};

/**
 * bei puma7 müssen bei laufenden pcmlink unabhängig von Telefonieaktivität div. PowerStates verboten werden.
 * Ansonsten wird der netss_irq verschluckt.
 */
static struct _power_managment_dest_entry telefon_profile_4ms[] =  {
	{ "pm_qos_latency_invariable",  AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 4 },
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry telefon_profile_8ms[] =  {
	{ "pm_qos_latency_invariable",  AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 8 },
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry telefon_profile_exit[] =  {
	{ "pm_qos_latency_invariable",  AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0 },
	{ NULL, 0, 0 }
};


/**
 */
static struct _power_managment_dest_entry telefon_profile_off[] =  {
	{ "telefon_profile", AVM_PM_CB_FAILED, 0x0 },                     /*--- Eventmanager bekommt telefon-profil aus mit ---*/
	{ "speedstep",       AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x2 },      /*--- restart governor ---*/
	{ "pm_qos_latency",  AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x0 },
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry telefon_profile_on[] =  {
	{ "telefon_profile", AVM_PM_CB_FAILED, 0x1 },  /*--- Eventmanager bekommt telefon-profil an mit ---*/
	{ "speedstep",       AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x1 },  /*--- pause governor ---*/
	{ "pm_qos_latency",  AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x1 },
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry dect_trace_on[] =  {
	{ "protrace", AVM_PM_CB_IGNORE, 0x401 },  /*--- Port 51111 oeffnen ---*/
	{ "piglet",   AVM_PM_CB_IGNORE, 0x401 },  /*--- Port 51112 oeffnen ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry dect_trace_off[] =  {
	{ "protrace", AVM_PM_CB_IGNORE | AVM_PM_ASYNC,     0x400 },  /*--- Port 51111 schliessen ---*/
	{ "piglet",   AVM_PM_CB_IGNORE,                    0x400 },  /*--- Port 51112 schliessen ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry multicast_profile_on[] =  {
	{ "telefon_profile", AVM_PM_CB_IGNORE, LOAD_CONTROL_MULTICAST | 0x1 },  /*--- Loadcontrols setzt LOAD_CONTROL_MULTICAST ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry multicast_profile_off[] =  {
	{ "telefon_profile", AVM_PM_CB_IGNORE, LOAD_CONTROL_MULTICAST | 0x0 }, /*--- Loadcontrols ruecksetzen LOAD_CONTROL_MULTICAST ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry voip_profile_on[] =  {
	{ "telefon_profile", AVM_PM_CB_IGNORE, LOAD_CONTROL_VOIPCALL | 0x1 },  /*--- Loadcontrols setzt LOAD_CONTROL_MULTICAST ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry voip_profile_off[] =  {
	{ "telefon_profile", AVM_PM_CB_IGNORE, LOAD_CONTROL_VOIPCALL | 0x0 },  /*--- Loadcontrols ruecksetzen LOAD_CONTROL_MULTICAST ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry usbcurrentreq_entries[] =  {
	{ "usbpower",  AVM_PM_CB_FAILED, 0x2 },         /*--- Abfrage aktueller Stromverbrauch aller USB-Hosts ----*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry pcmlinkbus_stop_entries[] =  {
	{ "pcmlink",   AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },        /*---  PCM-Bus deaktivieren ---*/
	{ "speedstep", AVM_PM_CB_IGNORE | AVM_PM_ASYNC, 0x1 },        /*--- Kick governor ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry pcmlinkbus_start_entries[] =  {
	{ "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*---  PCM-Bus reaktivieren ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry pots_load_entries[] =  {
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x203 },        /*--- nur wenn TE aktuell Wechsel notwendig ---*/
	{ "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },         /*---  PCM-Bus deaktivieren ---*/
	{ "piglet",   AVM_PM_CB_IGNORE, 0x210 },                      /*--- lade POTS-File ---*/
	{ "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
	{ "isdn",     AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x100 },       /*--- TE-DKanal aus (inkl. Slot austragen) ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry te_load_entries[] =  {
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x202 },        /*--- nur wenn POTS aktuell Wechsel notwendig ---*/
	{ "pcmlink", AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },         /*---  PCM-Bus deaktivieren ---*/
	{ "piglet",  AVM_PM_CB_IGNORE, 0x211 },                       /*--- lade TE-File ---*/
	{ "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry te_reload_entries[] =  {
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x203 },        /*--- nur wenn TE aktuell reload notwendig ---*/
	{ "pcmlink", AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },          /*---  PCM-Bus deaktivieren ---*/
	{ "piglet",  AVM_PM_CB_IGNORE, 0x213 },                       /*--- lade TE-File (force) ---*/
	{ "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
	{ "isdn",     AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x100 },       /*--- TE-DKanal komplett aus -> triggert Reinitialisierung ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry pots_reload_entries[] =  {
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x202 },        /*--- nur wenn POTS aktuell reload notwendig ---*/
	{ "pcmlink", AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },         /*---  PCM-Bus deaktivieren ---*/
	{ "piglet",  AVM_PM_CB_IGNORE, 0x212 },                       /*--- lade POTS-File (force) ---*/
	{ "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry tepots_switchauto_entries[] =  {
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x204 },        /*--- AutoMode an ---*/
	{ NULL, 0, 0 }
};

/**
 */
static struct _power_managment_dest_entry tepots_switchmanu_entries[] =  {
	{ "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x205 },        /*----AutoMode aus ---*/
	{ NULL, 0, 0 }
};

/**
 * wird vom pcmlink-Treiber ausgewertet:
 */
static struct _power_managment_dest_entry teactive_entries[] =  {
	{ "piglet",  AVM_PM_CB_FAILED, 0x201 },
	{ NULL, 0, 0 }
};

/**
 * powermode mit nachfolgender Powermanagment-Treiber-Liste
 */
static struct _power_managment power[] = {
	{ powermode:"te_active",             dests:teactive_entries,          Access:PM_ACCESS_ALL},
	{ powermode:"dsl",                   dests:dslmode_entries,           Access:PM_ACCESS_ALL},
	{ powermode:"LTE",                   dests:lteonlymode_entries,       Access:PM_ACCESS_ALL},
	{ powermode:"ata",                   dests:atamode_entries,           Access:PM_ACCESS_ALL},
	{ powermode:"vdsl",                  dests:vdslmode_entries,          Access:PM_ACCESS_ALL},   /*--- im VDSL-Mode DSL auf ATA stellen ---*/
	{ powermode:"update",                dests:updatemode_entries,        Access:PM_ACCESS_ALL},
	{ powermode:"dect_trace_on",         dests:dect_trace_on,             Access:PM_ACCESS_ALL},   /*--- DECT-Trace Ports bereitstellen ---*/
	{ powermode:"dect_trace_off",        dests:dect_trace_off,            Access:PM_ACCESS_ALL},   /*--- DECT-Trace Ports schliessen ---*/
	{ powermode:"telefon_profile_4ms",   dests:telefon_profile_4ms,       Access:PM_ACCESS_ALL},   /*--- PM-QOS für 4 ms pcmlink-Irq ---*/
	{ powermode:"telefon_profile_8ms",   dests:telefon_profile_8ms,       Access:PM_ACCESS_ALL},   /*--- PM-QOS für 8 ms pcmlink-Irq ---*/
	{ powermode:"telefon_profile_exit",  dests:telefon_profile_exit,      Access:PM_ACCESS_ALL},   /*--- PM-QOS aus (pcmlink entladen) ---*/
	{ powermode:"telefon_profile_on",    dests:telefon_profile_on,        Access:PM_ACCESS_ALL},   /*--- Speedkick ---*/
	{ powermode:"telefon_profile_off",   dests:telefon_profile_off,       Access:PM_ACCESS_ALL},   /*--- vorhergehendes Profil ---*/
	{ powermode:"multicast_profile_on",  dests:multicast_profile_on,      Access:PM_ACCESS_ALL},   /*--- signalisiert das Multicast an ---*/
	{ powermode:"multicast_profile_off", dests:multicast_profile_off,     Access:PM_ACCESS_ALL},   /*--- signalisiert, dass Multicast aus ---*/
	{ powermode:"voip_profile_on",       dests:voip_profile_on,           Access:PM_ACCESS_ALL},   /*--- signalisiert das Voip an ---*/
	{ powermode:"voip_profile_off",      dests:voip_profile_off,          Access:PM_ACCESS_ALL},   /*--- signalisiert, dass Voip aus ---*/
	{ powermode:"usb_current_req",       dests:usbcurrentreq_entries,     Access:PM_ACCESS_ALL},
	{ powermode:"pcmlink_bus_off",       dests:pcmlinkbus_stop_entries,   Access:PM_ACCESS_DRIVER},
	{ powermode:"pcmlink_bus_on",        dests:pcmlinkbus_start_entries,  Access:PM_ACCESS_DRIVER},
	{ powermode:"pots_load",             dests:pots_load_entries,         Access:PM_ACCESS_ALL},
	{ powermode:"te_load",               dests:te_load_entries,           Access:PM_ACCESS_ALL},
	{ powermode:"te_reload",             dests:te_reload_entries,         Access:PM_ACCESS_ALL},
	{ powermode:"pots_reload",           dests:pots_reload_entries,       Access:PM_ACCESS_ALL},
	{ powermode:"tepots_switchauto",     dests:tepots_switchauto_entries, Access:PM_ACCESS_ALL},
	{ powermode:"tepots_switchmanu",     dests:tepots_switchmanu_entries, Access:PM_ACCESS_ALL},
	{ powermode:"lock_governor",         dests:lock_governor,             Access:PM_ACCESS_ALL},
	{ powermode:"unlock_governor",       dests:unlock_governor,           Access:PM_ACCESS_ALL},
	{ powermode:"e1_active",             dests:increase_min_freq,         Access:PM_ACCESS_ALL},
	{ powermode:"e1_inactive",           dests:default_min_freq,          Access:PM_ACCESS_ALL},
	{ powermode:NULL,                    dests:NULL,                      Access:0}
};

/**
 * access: Zugriff vom Treiber/Applikation PM_ACESS_DRIVER oder PM_ACESS_APPL
 */
const struct _power_managment_dest_entry *find_powermode(const char *powermode_name, unsigned int access)
{
	struct _power_managment *powertab = power;

	while (powertab->powermode) {
		if ((strcmp(powermode_name, powertab->powermode) == 0) && (powertab->Access & access)) {
			/*--- DEB_ERR("[avm_power] exec powermanagment '%s'\n", powertab->powermode); ---*/
			return powertab->dests;
		}
		powertab++;
	}
	return NULL;
}

/**
 * ret: 0 freigegeben
 */
static inline int put_pwclient(struct _power_managment_clients *client)
{
	if (atomic_read(&client->link) == 0) {
		pr_err("%s error link already zero\n", __func__);
		return 0;
	}
	if (atomic_dec_and_test(&client->link)) {
		/*--- printk(KERN_INFO"%s free\n", __func__); ---*/
		if (!is_rte_context()) {
			/*--- dirty but should not happens - possible memleak in nonlinux-context ---*/
			kfree(client);
		}
		return 0;
	}
	return 1;
}

/**
 */
struct _power_managment_clients *get_pwclient_by_name(const char *client_name)
{
	struct _power_managment_clients *client;
	unsigned long flags;

	rte_spin_lock_irqsave(&gpw_client_lock, flags);
	client = (struct _power_managment_clients *)PwClientAnker;
	while (client) {
		if (strcmp(client_name, client->client_name) == 0) {
			atomic_add(1, &client->link);
			rte_spin_unlock_irqrestore(&gpw_client_lock, flags);
			return client;
		}
		client = client->next;
	}
	rte_spin_unlock_irqrestore(&gpw_client_lock, flags);
	return NULL;
}

/**
 */
static struct _power_managment_clients *add_pwclient(const char *client_name, int (*CallBackPowerManagmentControl)(int state))
{
	struct _power_managment_clients *new;
	unsigned long flags;

	new = kmalloc(sizeof(struct _power_managment_clients) + strlen(client_name) + 1, GFP_KERNEL);
	if (new == NULL) {
		return NULL;
	}
	atomic_set(&new->link, 1);
	new->client_name = (char *)new + sizeof(struct _power_managment_clients);
	strcpy(new->client_name, client_name);
	new->CallBackPowerManagmentControl = CallBackPowerManagmentControl;

	spin_lock_irqsave(&gpw_client_lock, flags);
	new->next     = (struct _power_managment_clients *)PwClientAnker;
	PwClientAnker = new;
	spin_unlock_irqrestore(&gpw_client_lock, flags);
	return new;
}

/**
 */
static int del_pwclient(struct _power_managment_clients *delclient)
{
	unsigned long flags;
	struct _power_managment_clients *prevclient = NULL;
	struct _power_managment_clients *client;

	spin_lock_irqsave(&gpw_client_lock, flags);
	client = (struct _power_managment_clients *)PwClientAnker;
	while (client) {
		if (client == delclient) {
			if (prevclient == NULL) {
				/*--- erste Element ---*/
				PwClientAnker = client->next;
			} else {
				prevclient->next = client->next;
			}
			spin_unlock_irqrestore(&gpw_client_lock, flags);
			return put_pwclient(client);
		}
		prevclient = client;
		client     = client->next;
	}
	spin_unlock_irqrestore(&gpw_client_lock, flags);
	return 0;
}

/**
 * Powermanagment des Treibers anmelden
 * beide Parameter NULL : dying gasp
 * state: kontextbezogen - siehe (linux_)avm_power.h
 */
void *PowerManagmentRegister(const char *client_name, int (*CallBackPowerManagmentControl)(int state))
{
	struct _power_managment_clients *client;

	DEB_INFO("[avm_power]%s: \"%s\", 0x%pS\n", __func__, client_name, CallBackPowerManagmentControl);
	if (client_name == NULL || CallBackPowerManagmentControl == NULL) {
		DEB_ERR("[avm_power]%s: invalid param %p %pS\n", __func__, client_name, CallBackPowerManagmentControl);
		return NULL;
	}
	client = get_pwclient_by_name(client_name);
	if (client) {
		put_pwclient(client);
		return client;
	}
	return add_pwclient(client_name, CallBackPowerManagmentControl);
}
EXPORT_SYMBOL(PowerManagmentRegister);

/**
 * Powermanagment des Treibers abmelden
 */
void PowerManagmentRelease(void *Handle)
{
	struct _power_managment_clients *delclient = (struct _power_managment_clients *)Handle;

	pr_err("[avm_power]%s:(0x%p)\n", __func__, Handle);
	if (Handle == NULL) {
		DEB_ERR("[avm_power]%s: invalid Handle\n", __func__);
		return;
	}
	if (del_pwclient(delclient)) {
		schedule_timeout(HZ/10); /*--- er scheint noch in CallBackPowerManagmentControl zu haengen: hier warten ! ---*/
	}
}
EXPORT_SYMBOL(PowerManagmentRelease);

/**
 */
struct _async_powermodetab {
	struct delayed_work delayed_wk;
	const struct _power_managment_dest_entry *powermodetab;
};

/**
 */
static void async_powermode_action(struct work_struct *work)
{
	struct _async_powermodetab *papmt = container_of(work, struct _async_powermodetab, delayed_wk.work);

	powermode_action(papmt->powermodetab, 1);
	/*--- DEB_ERR("[avm_power]%s: free: %p\n", __func__, papmt); ---*/
	kfree(papmt);
}

/**
 */
static int powermode_action_async(const struct _power_managment_dest_entry *powermodetab)
{
	struct _async_powermodetab *papmt;

	if (is_rte_context()) {
		/*--- not allowed  ---*/
		return 1;
	}
	papmt = kzalloc(sizeof(struct _async_powermodetab), GFP_ATOMIC);
	if (papmt == NULL) {
		DEB_ERR("[avm_power]%s: no memory\n", __func__);
		return 1;
	}
	papmt->powermodetab = powermodetab;
	/*--- INIT_WORK(&papmt->work, async_powermode_action); ---*/
	INIT_DELAYED_WORK(&papmt->delayed_wk, async_powermode_action);
	/*--- DEB_ERR("[avm_power]%s: async mode: %p\n", __func__, papmt); ---*/
	/*--- schedule_work(&papmt->work); ---*/
	schedule_delayed_work(&papmt->delayed_wk, HZ / 1000);
	return 0;
}

/**
 * hier werden die Tabelleneintraege (struct _power_managment_dest_entry ...[])
 * "ausgeführt"
 * non-linux-context fest (yield/firq)
 */
int powermode_action(const struct _power_managment_dest_entry *powermodetab, unsigned int async_context)
{
	struct _power_managment_clients *registered_client;
	int ret = 0, i = 0, locked = 0;
	unsigned long flags;

	if (powermodetab == NULL) {
		return 1;
	}
	while (powermodetab[i].client_name) {
		registered_client = get_pwclient_by_name(powermodetab[i].client_name);
		if (registered_client == NULL) {
			if (powermodetab[i].mandatory & AVM_PM_CB_UNINSTALLED_OR_FAILED) {
				DEB_TRC_PT("[avm_power] '%s' not registered can't execute powermanagment ->stop\n", powermodetab[i].client_name);
				ret = 1;
			} else {
				DEB_TRC_PT("[avm_power] '%s' not registered can't execute powermanagment ->ignore\n", powermodetab[i].client_name);
			}
		} else {
			if ((async_context == 0) && (powermodetab[i].mandatory & AVM_PM_ASYNC)) {
				put_pwclient(registered_client);
				return powermode_action_async(&powermodetab[i]);
			}
			if ((powermodetab[i].mandatory & AVM_PM_LOCK) && (locked == 0)) {
				locked = 1;
				rte_spin_lock_irqsave(&avmpower_lock, flags);
			}
			ret = registered_client->CallBackPowerManagmentControl(powermodetab[i].state);
			if (ret) {
				if (powermodetab[i].mandatory & (AVM_PM_CB_UNINSTALLED_OR_FAILED | AVM_PM_CB_FAILED)) {
					DEB_TRC_PT("[avm_power] '%s'=0x%x powermanagment failed->stop ret=%x\n", powermodetab[i].client_name, powermodetab[i].state, ret);
				} else {
					DEB_TRC_PT("[avm_power] '%s'=0x%x powermanagment failed->ignore ret=%x\n", powermodetab[i].client_name, powermodetab[i].state, ret);
					ret = 0;
				}
			} else {
				/*--- DEB_TRC_PT("[avm_power] '%s'=0x%x powermanagment ok\n", powermodetab[i].client_name, powermodetab[i].state); ---*/
			}
			put_pwclient(registered_client);
		}
		if (ret) {
			break;
		}
		i++;
	}
	if (locked) {
		rte_spin_unlock_irqrestore(&avmpower_lock, flags);
	}
	return ret;
}

/**
 * Powermanagment nicht ueber Tabelle
 * ret 0: alles ok, 1: Callback-Fehler, 2: Client nicht registriert
 *
 */
int powermode_action_nolist(const char *client_name, int State)
{
	struct _power_managment_clients *registered_client;
	int ret;

	registered_client = get_pwclient_by_name(client_name);
	if (registered_client == NULL) {
		return 2;
	}
	ret = registered_client->CallBackPowerManagmentControl(State);
	put_pwclient(registered_client);
	return ret ? 1 : 0;
}

/**
 */
void avm_power_writeformaterror(const char *text)
{
	char TextBuf[512], *p;
	int writesize = sizeof(TextBuf);
	struct _power_managment *powertab = power;

	p = TextBuf;
	snprintf(p, writesize, "%s\navailable powermode:", text);
	writesize -= strlen(p);
	p         += strlen(p);
	while (powertab->powermode) {
		if (powertab->Access & PM_ACCESS_APPL) {
			snprintf(p, writesize, "%s%s", powertab->powermode, (powertab+1)->powermode ? "," : "");
			writesize -= strlen(p);
			p         += strlen(p);
		}
		powertab++;
	}
	snprintf(p, writesize, "\n");
	DEB_ERR("%s", TextBuf);
}

/**
 * vom Kernel den Powermode ändern
 * Returnwert: 0 ok sonst Abbruch mit Fehler
 */
int PowerManagmentActivatePowerMode(const char *powermode_name)
{
	const struct _power_managment_dest_entry *powermodetab = find_powermode(powermode_name, PM_ACCESS_DRIVER);
#if defined(DEBUG_TRACE_POWERTAB)
	if (powermodetab !=  teactive_entries) {
		DEB_TRC_PT("[avm_power]%s: '%s'\n", __func__, powermode_name);
	}
#endif
	return powermode_action(powermodetab, 0);
}
EXPORT_SYMBOL(PowerManagmentActivatePowerMode);
#ifdef CONFIG_AVM_POWERMETER

/**
 * die Infos werden erst hier zusammengesammelt, Status wird mit Set/Reset-Flag
 * signalisiert, Power wird hier gesetzt
 */
static void convert_isdnpowerstate(unsigned int *SaveValue, unsigned int Value)
{
	int i;

	/*--- Ebene1/3 State uebernehmen ---*/
	if (Value & (1 << 31)) { /* Set State */
		*SaveValue |= Value;
	} else {               /* Reset State */
		*SaveValue &= ~Value;
	}
	*SaveValue &= ~0xFFFF;  /*--- Powerratebereich loeschen ---*/
	for (i = 1; i < 5; i++) {
		/*--- max. 4 Controller E1 aktiv -> power++ ! ---*/
		if (*SaveValue & PM_E1STATUS(i)) {
			*SaveValue += 100;
		}
	}
	/*--- DBG_ERR(("convert_isdnpowerstate: %x\n", *SaveValue)); ---*/
}

/**
 * aktuell max. 4 CPU's
 */
static inline unsigned int cpusloadrate_to_powerrate(unsigned char cpu_table[], unsigned int cpuspowerrate)
{
	unsigned int cpu, norm = 0;
	unsigned int run = 0;

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		if (cpu_online(cpu)) {
			norm++;
			run           += cpuspowerrate & 0xFF;
			cpu_table[cpu] = cpuspowerrate & 0xFF;
		} else {
			cpu_table[cpu] = 0xFF;
		}
		cpuspowerrate >>= 8;
	}
	if (norm == 0) {
		return norm;
	}
	return run / norm;
}

/**
 * Funktion wird von Treibern aufgerufen um Infos ueber den aktuellen Power-Status zu liefern
 * Achtung! Kann aus Yield-Kontext aufgerufen werden
 */
int PowerManagmentRessourceInfo(enum _powermanagment_device device, int power_rate)
{
	int changes = 0, ready;
	unsigned int cpus_loadrate = 0;

	if ((unsigned int)device >= powerdevice_maxdevices) {
		DEB_ERR("[avm_power]%s: unknown device: %d\n", __func__, device);
		return 0;
	}
	ready =  pm_ressourceinfo.kthread ? 1 : 0;
	if (device == powerdevice_loadrate) {
		cpus_loadrate = power_rate;
		power_rate    = cpusloadrate_to_powerrate(pm_ressourceinfo.cpu_run, cpus_loadrate);
		if (pm_ressourceinfo.cpus_loadrate != (unsigned int)cpus_loadrate) {
			pm_ressourceinfo.cpus_loadrate = (unsigned int)cpus_loadrate;
			pm_ressourceinfo.cpurun_trigger = 1;
			changes = (ready && pm_ressourceinfo.NormP) ? 1 : 0; /*--- nur wenn Thread existiert und script geladen ----*/
		}
		pm_ressourceinfo.loadcntrl = avm_powermanager_load_control_handler(power_rate);
	}
	if (pm_ressourceinfo.deviceinfo[device].power_rate != power_rate) {
		if ((device == powerdevice_isdnte) || (device == powerdevice_isdnnt)) {
			convert_isdnpowerstate(&pm_ressourceinfo.deviceinfo[device].power_rate, power_rate);
		} else {
			pm_ressourceinfo.deviceinfo[device].power_rate = power_rate;
		}
		if ((avm_power_disp_loadrate & 1) && (device == powerdevice_loadrate)) {
			char txt[64];
			unsigned long avnrun[3];

			get_avenrun(avnrun, FIXED_1/200, 0);
			pr_err("%s loadavg %lu.%lu %lu.%lu %lu.%lu loadcntrl 0x%x\n",
					display_per_cpurate(txt, sizeof(txt), cpus_loadrate),
					LOAD_INT(avnrun[0]), LOAD_FRAC(avnrun[0]),
					LOAD_INT(avnrun[1]), LOAD_FRAC(avnrun[1]),
					LOAD_INT(avnrun[2]), LOAD_FRAC(avnrun[2]),
					pm_ressourceinfo.loadcntrl
					);
		}
		changes = (ready && pm_ressourceinfo.NormP) ? 1 : 0;   /*--- nur wenn Thread existiert und script geladen ----*/
		if ((avm_power_disp_loadrate & 2) && (device != powerdevice_loadrate)) {
			pr_err("[avm_power]%s: device: %s value=(0x%x)%d changes=%d\n", __func__,
											pm_name_device(device), power_rate, power_rate, changes);
		}
	}
	pm_ressourceinfo.Changes += changes;
	if (changes) {
		rte_wake_up_interruptible(&pm_ressourceinfo.wait_queue);
	}
	return 0;
}
EXPORT_SYMBOL(PowerManagmentRessourceInfo);

static struct proc_dir_entry *pmri_procdir;

#define PROC_PMRI_DIR   "avm/powermanagmentressourceinfo"

#define DBG_TRC(args...)	no_printk(args)
/*--- #define DBG_TRC(args...) pr_info(args) ---*/


/**
 */
static struct _pmri_proc_list {
#define pmri_proc_entry(type, flag, fmt)  { .device = type, .fmode = flag, .format = fmt}
	enum _powermanagment_device device;
	fmode_t fmode;        /*--- FMODE_READ / FMODE_WRITE ---*/
	const char *format;
} pmri_proc_list[] = {
	pmri_proc_entry(powerdevice_wlan,        FMODE_READ | FMODE_WRITE, "0x%x "),
	pmri_proc_entry(powerdevice_isdnnt,      FMODE_READ,               "0x%x "),
	pmri_proc_entry(powerdevice_isdnte,      FMODE_READ,               "0x%x "),
	pmri_proc_entry(powerdevice_analog,      FMODE_READ,               "%u %% "),
	pmri_proc_entry(powerdevice_dect,        FMODE_READ,               "0x%x  "),
	pmri_proc_entry(powerdevice_ethernet,    FMODE_READ,               "0x%x "),
	pmri_proc_entry(powerdevice_dsl,         FMODE_READ | FMODE_WRITE, "%u %% "),
	pmri_proc_entry(powerdevice_usb_host,    FMODE_READ,               "%u mA "),
	pmri_proc_entry(powerdevice_usb_host2,   FMODE_READ,               "%u mA "),
	pmri_proc_entry(powerdevice_usb_host3,   FMODE_READ,               "%u mA "),
	pmri_proc_entry(powerdevice_loadrate,    FMODE_READ,               "%u %% "),
	pmri_proc_entry(powerdevice_temperature, FMODE_READ,               "%d °C "),
	pmri_proc_entry(powerdevice_lte,         FMODE_READ,               "%u mW "),
	pmri_proc_entry(powerdevice_loadrate2,   FMODE_READ,               "%u %% "),
	pmri_proc_entry(powerdevice_dvbc,        FMODE_READ,               "%u %% "),
	pmri_proc_entry(powerdevice_plc,         FMODE_READ | FMODE_WRITE, "%u %% "),
};

/**
 */
static void lproc_pmri_read(struct seq_file *seq, void *priv)
{
	struct _pmri_proc_list *pproc_entry = (struct _pmri_proc_list *)priv;
	enum _powermanagment_device device = pproc_entry->device;

	if (device < powerdevice_maxdevices) {
		seq_printf(seq, pproc_entry->format, pm_ressourceinfo.deviceinfo[device].power_rate);
	}
}
#define SKIP_SPACES(p) { while ((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) p++; }

/**
 */
static int lproc_pmri_write(char *buf, void *priv)
{
	struct _pmri_proc_list *pproc_entry = (struct _pmri_proc_list *)priv;
	enum _powermanagment_device device = pproc_entry->device;
	unsigned int power_rate = 0;
	char *p = buf;

	SKIP_SPACES(p);
	if (*p == '0' && *(p+1) == 'x') {
		sscanf(p, "0x%x", &power_rate);
	} else {
		sscanf(p, "%d", (int *)&power_rate);
	}
	DBG_TRC("%s: device=%s = 0x%x\n", __func__, pm_name_device(device), power_rate);
	PowerManagmentRessourceInfo(device, power_rate);
	return 0;
}

/**
 */
int PowerManagmentRessourceInfo_Init(void)
{
	char filebuf[128];
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pmri_proc_list); i++) {
		struct _pmri_proc_list *pproc_entry = &pmri_proc_list[i];

		if (pproc_entry->fmode) {
			if (pmri_procdir == NULL) {
				pmri_procdir = proc_mkdir(PROC_PMRI_DIR, NULL);
				if (pmri_procdir == NULL) {
					pr_err("%s: can't create %s\n", __func__, PROC_PMRI_DIR);
					return -1;
				}
			}
			snprintf(filebuf, sizeof(filebuf), "%s/%s", PROC_PMRI_DIR, pm_name_device(pproc_entry->device));
			DBG_TRC("%s: %s\n", __func__, filebuf);
			add_simple_proc_file(filebuf,
					pproc_entry->fmode & FMODE_WRITE ? lproc_pmri_write : NULL,
					pproc_entry->fmode & FMODE_READ  ? lproc_pmri_read : NULL,
					&pmri_proc_list[i]);
		}
	}
	return 0;
}
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
