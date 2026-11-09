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
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/file.h>
#include <avm/power/power.h>
#include <avm/event/event.h>
#include <linux/netdevice.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <asm/errno.h>
#include <linux/completion.h>
#include <linux/kthread.h>

#pragma GCC pop_options

#include "avm_power.h"

#define STD_SYSTEMCLK           120000000
#define MAX_SYSTEMCLK           120000000

#define STD_DSPCLK              360000000
#define MAX_DSPCLK              360000000

#define STD_MIPSCLK             360000000
#define MAX_MIPSCLK             360000000

MODULE_DESCRIPTION("AVM powermanagment module");
MODULE_LICENSE("GPL");

/*--- #define DEBUG_TRACE_POWERTAB ---*/
#if defined(DEBUG_TRACE_POWERTAB)
#define DEB_TRC_PT      DEB_ERR
#else/*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/
#define DEB_TRC_PT(args...)
#endif/*--- #else ---*//*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/

#define SKIP_SPACES(p)  { while ((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) (p)++; }

#ifdef CONFIG_AVM_POWERMETER

/**
 */
#if IS_ENABLED(CONFIG_AVM_EVENT)
static void pm_ressourceinfo_notify(void *context, enum _avm_event_id id)
{
	struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)context;
	struct _avm_event_pm_info_stat *event;
	int handled;

	if (id != avm_event_id_pm_ressourceinfo_status) {
		pr_warn("[avm_power]unknown event: %d\n", id);
		return;
	}
	event = kmalloc(sizeof(struct _avm_event_pm_info_stat), GFP_ATOMIC);
	if (event == NULL) {
		pr_warn("[avm_power]can't alloc event: %d\n", id);
		return;
	}
	memcpy(event, &pm_info->stat, sizeof(struct _avm_event_pm_info_stat));
	event->header.id = id;
	handled = avm_event_source_trigger(pm_info->event_handle, id, sizeof(struct _avm_event_pm_info_stat), event);
	if (handled == 0) {
		pr_warn("[avm_power]event: %d not handled\n", id);
	}
}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
static unsigned int PM_GET_MWATT(struct _power_managment_device_info *df)
{
	unsigned int ret = 0;

	if (df->divider != 0) {
		ret = (((PM_GET_RATE(df->power_rate)) * df->multiplier) / df->divider) + df->offset;
	}
	return ret;
}

/**
 */
static unsigned int PM_GET_NORM_MWATT(struct _power_managment_device_info *df)
{
	unsigned int ret = 0;

	if (df->divider != 0) {
		ret = (((PM_GET_RATE(df->norm_power_rate)) * df->multiplier) / df->divider) + df->offset;
	}
	return ret;
}

#if IS_ENABLED(CONFIG_AVM_EVENT)

#define PM_CALC_SUM(old, new, messure)  ((old) + ((new) * (messure)))
#define PM_TIME_DIFF(low1, low2) (((low1) - (low2)) < 0 ?  -((low1) - (low2)) : ((low1) - (low2)))

/**
 * kumulierte Werte berechnen
 */
static void pm_process_cum(struct _power_managment_ressource_info *pm_info)
{
	int i, intervall = 0;
	struct _power_managment_cum_info *pcum;
	long MessureSamples, MessureSampleperPeriod;
	long tail_messure;
	int rate_dspcum = 0;
	int rate_systemcum = 0;
	int rate_sumcum = 0;
	int rate_wlancum = 0;
	int rate_ethcum = 0;
	int rate_abcum = 0;
	int rate_dectcum = 0;
	int rate_battcum = 0;
	int rate_usbhostcum = 0;
	int rate_ltecum = 0;
	int rate_dvbccum = 0;
	int rate_plccum = 0;
	int rate_temp = 0;
	signed char min_temp, max_temp;

	MessureSamples = PM_TIME_DIFF((long)jiffies, (long)pm_info->LastMessureTimeStamp) / HZ;
	if (MessureSamples == 0) {
		/*--- ein Messpunkt pro Sekunde: wenn rate_.. max. 100: 40 * 10e6 Samples (kein Overflow zu berücksichtigen!) ---*/
		return;
	}
	pm_info->LastMessureTimeStamp = jiffies;
	/*--- Gesamt-Zeitfenster: PM_TIME_INTERVALL mit PM_TIME_ENTRIES Teilzeitfenstern ---*/
	MessureSampleperPeriod = PM_TIME_INTERVALL / PM_TIME_ENTRIES; /*--- die Anzahl der Sample pro Teilzeitfenster ---*/
	do {
		pcum = &pm_info->cum[pm_info->ActTimeEntry];
		tail_messure = min(MessureSampleperPeriod - pcum->messure_count, MessureSamples);
/*--- pr_info("[%ld j:%ld]Samples: %ld tail_messure: %d act_entry %ld cnt %d\n", pm_info->LastMessureTimeStamp, jiffies, MessureSamples,  tail_messure, pm_info->ActTimeEntry, pcum->messure_count); ---*/
		if (tail_messure) {
			/*--- die Werte über Restmesspunkte dieses Teilzeitfensters integrieren ---*/
			pcum->rate_sum          =  PM_CALC_SUM(pcum->rate_sum,    pm_info->stat.rate_sumact,    tail_messure);
			pcum->rate_system       =  PM_CALC_SUM(pcum->rate_system, pm_info->stat.rate_systemact, tail_messure);
			pcum->rate_dsp          =  PM_CALC_SUM(pcum->rate_dsp,    pm_info->stat.rate_dspact,    tail_messure);
			pcum->rate_wlan         =  PM_CALC_SUM(pcum->rate_wlan,   pm_info->stat.rate_wlanact,   tail_messure);
			pcum->rate_eth          =  PM_CALC_SUM(pcum->rate_eth,    pm_info->stat.rate_ethact,    tail_messure);
			pcum->rate_ab           =  PM_CALC_SUM(pcum->rate_ab,     pm_info->stat.rate_abact,     tail_messure);
			pcum->rate_dect         =  PM_CALC_SUM(pcum->rate_dect,   pm_info->stat.rate_dectact,   tail_messure);
			pcum->rate_battcharge   =  PM_CALC_SUM(pcum->rate_battcharge, pm_info->stat.rate_battchargeact, tail_messure);
			pcum->rate_usbhost      =  PM_CALC_SUM(pcum->rate_usbhost, pm_info->stat.rate_usbhostact,   tail_messure);
			pcum->rate_lte          =  PM_CALC_SUM(pcum->rate_lte,    pm_info->stat.rate_lteact,   tail_messure);
			pcum->rate_dvbc         =  PM_CALC_SUM(pcum->rate_dvbc,   pm_info->stat.rate_dvbcact,  tail_messure);
			pcum->rate_plc          =  PM_CALC_SUM(pcum->rate_plc,    pm_info->stat.rate_plcact,   tail_messure);
			pcum->rate_temp         =  PM_CALC_SUM(pcum->rate_temp,   pm_info->stat.act_temperature,   tail_messure);

			if (pcum->min_temp > pm_info->stat.act_temperature) {
				pcum->min_temp = pm_info->stat.act_temperature;
			}
			if (pcum->max_temp < pm_info->stat.act_temperature) {
				pcum->max_temp = pm_info->stat.act_temperature;
			}
		}
		MessureSamples      -= tail_messure;
		pcum->messure_count += tail_messure;
		if (MessureSamples) {
			/*--- überlauf in neues Teilzeitfenster  ---*/
			pm_info->ActTimeEntry++;
			if (pm_info->ActTimeEntry >= PM_TIME_ENTRIES) {
				pm_info->ActTimeEntry = 0;
			}
			pcum = &pm_info->cum[pm_info->ActTimeEntry];
			memset(pcum, 0, sizeof(pm_info->cum[0]));
			pcum->min_temp = pcum->max_temp = pm_info->stat.act_temperature;
		}
	} while (MessureSamples);

	min_temp = max_temp = pm_info->stat.act_temperature;
	for (i = 0; i < PM_TIME_ENTRIES; i++) {
		/*--- Mittelwert ueber komplettes TIMER_INTERVALL ---*/
		pcum = &pm_info->cum[i];
		if (pcum->messure_count) {
			/*--- Addition der einzelnen Teilzeitfenster ---*/
			rate_sumcum     += pcum->rate_sum;
			rate_systemcum  += pcum->rate_system;
			rate_dspcum     += pcum->rate_dsp;
			rate_wlancum    += pcum->rate_wlan;
			rate_ethcum     += pcum->rate_eth;
			rate_abcum      += pcum->rate_ab;
			rate_dectcum    += pcum->rate_dect;
			rate_battcum    += pcum->rate_battcharge;
			rate_usbhostcum += pcum->rate_usbhost;
			rate_ltecum     += pcum->rate_lte;
			rate_dvbccum    += pcum->rate_dvbc;
			rate_plccum     += pcum->rate_plc;
			rate_temp       += pcum->rate_temp;
			if (min_temp > pcum->min_temp) {
				min_temp = pcum->min_temp;
			}
			if (max_temp < pcum->max_temp) {
				max_temp = pcum->max_temp;
			}
			intervall += pcum->messure_count;
		}
	}
/*--- pr_info("intervall: %d\n", intervall); ---*/
	if (intervall) {
		pm_info->stat.rate_sumcum        = (unsigned char)min(100, rate_sumcum     / intervall);  /*--- kumulierter Energieverbrauch in Prozent ---*/
		pm_info->stat.rate_systemcum     = (unsigned char)min(100, rate_systemcum  / intervall);  /*--- (gewichtete) kumulierte Aktivität MIPS, System und idle in Prozent ---*/
		pm_info->stat.rate_dspcum        = (unsigned char)min(100, rate_dspcum     / intervall);  /*--- kumulierte Aktivität DSP in Prozent ---*/
		pm_info->stat.rate_wlancum       = (unsigned char)min(100, rate_wlancum    / intervall);  /*--- kumulierter Verbrauch WLAN in Prozent ---*/
		pm_info->stat.rate_ethcum        = (unsigned char)min(100, rate_ethcum     / intervall);  /*--- kumulierter Ethernet-Verbrauch ---*/
		pm_info->stat.rate_abcum         = (unsigned char)min(100, rate_abcum      / intervall);  /*--- kumulierter Nebenstellen-Verbrauch in Prozent ---*/
		pm_info->stat.rate_dectcum       = (unsigned char)min(100, rate_dectcum    / intervall);  /*--- kumulierter Verbrauch DECT in Prozent ---*/
		pm_info->stat.rate_battchargecum = (unsigned char)min(100, rate_battcum    / intervall);  /*--- kumulierter Verbrauch Battery-Charge in Prozent ---*/
		pm_info->stat.rate_usbhostcum    = (unsigned char)min(100, rate_usbhostcum / intervall);  /*--- kumulierter Verbrauch USB-Host in Prozent ---*/
		pm_info->stat.rate_ltecum        = (unsigned char)min(100, rate_ltecum     / intervall);  /*--- kumulierter Verbrauch LTE in Prozent ---*/
		pm_info->stat.rate_dvbccum       = (unsigned char)min(100, rate_dvbccum    / intervall);  /*--- kumulierter Verbrauch DVB-C in Prozent ---*/
		pm_info->stat.rate_plccum        = (unsigned char)min(100, rate_plccum     / intervall);  /*--- kumulierter Verbrauch PLC in Prozent ---*/
		pm_info->stat.avg_temperature    = rate_temp / intervall;
		pm_info->stat.min_temperature    = min_temp;
		pm_info->stat.max_temperature    = max_temp;
	}
}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

/**
 */
int pm_ressourceinfo_limit_set_eth(struct _power_managment_ressource_info *pm_info,
				   unsigned int eth_idx, unsigned int force)
{
	union  _powermanagment_ethernet_state eth;
	unsigned int eth_status = pm_info->eth_status[eth_idx];

	if (ETH_THROTTLE_DEMAND(eth_status)) {
		/*--- pr_info("[avm_power] eth: port %u throttle demand %s%s (t%d) status=%x\n", eth_idx, eth_status & ETH_THROTTLE_DEMAND_POLICY ? " POLICY" : "", eth_status & ETH_THROTTLE_DEMAND_CON    ? " CON" : "", pm_info->stat.act_temperature, eth_status); ---*/
		force   |= !ETH_THROTTLE_ACTIVE(eth_status); /*--- wenn vorher keine aktiv dann mit Sicherheit jetzt ---*/
		if (force) {
			pr_info("[avm_power] eth: port %u force throttle %s%s t%d\n", eth_idx,
					eth_status & ETH_THROTTLE_DEMAND_POLICY ? " POLICY" : "",
					eth_status & ETH_THROTTLE_DEMAND_CON    ? " CON" : "",
					pm_info->stat.act_temperature);
		}
		if (eth_status & ETH_THROTTLE_DEMAND_POLICY) {
			eth_status = ETH_MASK_BITS(eth_status, ETH_THROTTLE_DEMAND_POLICY, ETH_THROTTLE_ACTIVE_POLICY);
		}
		if (eth_status & ETH_THROTTLE_DEMAND_CON) {
			eth_status = ETH_MASK_BITS(eth_status, ETH_THROTTLE_DEMAND_CON, ETH_THROTTLE_ACTIVE_CON);
		}
		pm_info->eth_status[eth_idx] = eth_status;
	} else if (ETH_NORMAL_DEMAND(eth_status)) {
		unsigned int active = ETH_THROTTLE_ACTIVE(eth_status);
		/*--- pr_info("[avm_power] eth: port %u normal demand %s%s t%d status=%x\n", eth_idx, eth_status & ETH_NORMAL_DEMAND_POLICY ? " POLICY" : "", eth_status & ETH_NORMAL_DEMAND_CON    ? " CON" : "",  pm_info->stat.act_temperature, eth_status); ---*/
		if (eth_status & ETH_NORMAL_DEMAND_POLICY) {
			eth_status = ETH_MASK_BITS(eth_status, ETH_NORMAL_DEMAND_POLICY | ETH_THROTTLE_ACTIVE_POLICY, 0);
		}
		if (eth_status & ETH_NORMAL_DEMAND_CON) {
			eth_status = ETH_MASK_BITS(eth_status, ETH_NORMAL_DEMAND_CON | ETH_THROTTLE_ACTIVE_CON, 0);
		}
		force   |= (active != ETH_THROTTLE_ACTIVE(eth_status)) ? 1 : 0;
		if (force) {
			pr_info("[avm_power] eth: port %u force normal %s%s t%d\n", eth_idx,
					pm_info->eth_status[eth_idx] & ETH_NORMAL_DEMAND_POLICY ? " POLICY" : "",
					pm_info->eth_status[eth_idx] & ETH_NORMAL_DEMAND_CON ? " CON" : "",
					pm_info->stat.act_temperature);
		}
		pm_info->eth_status[eth_idx] = eth_status;
	}
	if (force == 0) {
		return 0;
	}
	eth.Register           = 0;
	eth.Bits.port          = eth_idx;
	eth.Bits.status        = pm_info->eth_status[eth_idx] & 0x3;
	eth.Bits.throttle_eth  = ETH_THROTTLE_ACTIVE(eth_status);
	if (avm_power_disp_loadrate & 0x10) {
		pr_info("[avm_power] eth: port %d status %d throttle %d%s%s t%d\n",
				eth.Bits.port, eth.Bits.status, eth.Bits.throttle_eth,
				eth_status & ETH_THROTTLE_ACTIVE_POLICY ? " POLICY" : "",
				eth_status & ETH_THROTTLE_ACTIVE_CON    ? " CON" : "",
				pm_info->stat.act_temperature);
	}
	return powermode_action_nolist("ethernet", eth.Register);
}

/**
 */
static int pm_ressourceinfo_thread(void *data)
{
	struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)data;
	unsigned int isdn_status =  0;
#if IS_ENABLED(CONFIG_AVM_EVENT)
	int device, mWatt, norm_p, rate, normmwatt, usbmwatt;
	int activ_ethports, i;
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

	/*--- DEB_ERR("[avm_power]%s: start\n", __func__); ---*/
#if IS_ENABLED(CONFIG_AVM_EVENT)
	pm_info->LastMessureTimeStamp = jiffies;
	pm_info->ActTimeEntry = 0;
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	while (!kthread_should_stop()) {
		if (wait_event_interruptible_timeout(pm_info->wait_queue, pm_info->Changes, 10 * HZ) < 0) {
			break;
		}
		pm_info->Changes--;
#if defined(CONFIG_AVM_EVENT_20)
		if (pm_info->cpurun_trigger) {
			pm_info->cpurun_trigger = 0;
			avmevent_cpu_run_notify(pm_info, avm_event_id_cpu_run);
			if (pm_info->Changes == 0) {
				continue;
			}
		}
#endif/*--- #if defined(CONFIG_AVM_EVENT_20) ---*/
		pm_info->Changes = 0;
		pm_info->deviceinfo[powerdevice_temperature].power_rate = avm_power_temperature();
#if IS_ENABLED(CONFIG_AVM_EVENT)
		pm_info->stat.act_temperature    = (signed char)pm_info->deviceinfo[powerdevice_temperature].power_rate;
		/*--- auf Grundlage der letzten Werte kummulierte Werte ermitteln ---*/
		pm_process_cum(pm_info);
		mWatt = 0;
		for (device = 0; device < powerdevice_maxdevices; device++) {
			mWatt += PM_GET_MWATT(&pm_info->deviceinfo[device]);
		}
		pm_info->stat.rate_sumact = (mWatt * 100) / (pm_info->NormP | 1);  /*--- aktueller Energieverbrauch in Prozent ---*/
		norm_p = ((pm_info->deviceinfo[powerdevice_cpuclock].norm_power_rate)   +
				 (pm_info->deviceinfo[powerdevice_systemclock].norm_power_rate) +
				 (pm_info->deviceinfo[powerdevice_loadrate].norm_power_rate));
		rate = 0;
		if (pm_info->deviceinfo[powerdevice_cpuclock].norm_power_rate) {
			rate += (pm_info->deviceinfo[powerdevice_cpuclock].power_rate);
		}
		if (pm_info->deviceinfo[powerdevice_systemclock].norm_power_rate) {
			rate += (pm_info->deviceinfo[powerdevice_systemclock].power_rate);
		}
		if (pm_info->deviceinfo[powerdevice_loadrate].norm_power_rate) {
			rate += (pm_info->deviceinfo[powerdevice_loadrate].power_rate);
		}
/*--- pr_info("rate: %d norm_p %d\n", rate, norm_p); ---*/
		pm_info->stat.rate_systemact = (rate * 100) / (norm_p | 1);
		pm_info->stat.system_status = 0xFF;  /*--- unbekannt ---*/
		pm_info->stat.rate_dspact   = PM_GET_RATE(pm_info->deviceinfo[powerdevice_dsl].power_rate);          /*--- aktueller Aktivität DSL in Prozent ---*/
		pm_info->stat.rate_wlanact  = PM_GET_RATE(pm_info->deviceinfo[powerdevice_wlan].power_rate);         /*--- aktueller Verbrauch WLAN in Prozent ---*/
		pm_info->stat.wlan_devices  = PM_WLAN_GET_DEVICES(pm_info->deviceinfo[powerdevice_wlan].power_rate); /*--- angemeldete WLAN-Devices ---*/
		pm_info->stat.wlan_status   = PM_WLAN_GET_ECO(pm_info->deviceinfo[powerdevice_wlan].power_rate);     /*--- WLAN-Status (1= ECO) ---*/
		pm_info->stat.rate_ethact   = (PM_GET_RATE(pm_info->deviceinfo[powerdevice_ethernet].norm_power_rate) == 0) ? 0 :
									  (PM_GET_RATE(pm_info->deviceinfo[powerdevice_ethernet].power_rate * 100) /
									   PM_GET_RATE(pm_info->deviceinfo[powerdevice_ethernet].norm_power_rate));      /*--- aktueller Ethernet-Verbrauch in Prozent ---*/
		activ_ethports              = PM_ETHERNET_GET_DEVICEMASK(pm_info->deviceinfo[powerdevice_ethernet].power_rate); /*--- Maske je Bit ein LAN-Port Bit0: Lan1 ... ---*/
		pm_info->stat.eth_status = 0;
		for (i = 0; i < AVMPOWER_MAX_ETHERNETPORTS; i++) {
			pm_ressourceinfo_limit_set_eth(pm_info, i, 0);
			pm_info->stat.eth_status |=  ((activ_ethports & (1 << i)) ? (3 << (i * 2)) : (pm_info->eth_status[i] & 0x3) << (i * 2));
		}
		pm_info->stat.isdn_status = 0;
		for (i = 1; i < 5; i++) {
			/*--- maximal 4 Controller ---*/
			pm_info->stat.isdn_status |= (PM_E1STATUS(i) & pm_info->deviceinfo[powerdevice_isdnnt].power_rate) ? (2 << ((i-1) * 4)) : 0; /*--- NT-E1 ---*/
			pm_info->stat.isdn_status |= (PM_E3STATUS(i) & pm_info->deviceinfo[powerdevice_isdnnt].power_rate) ? (4 << ((i-1) * 4)) : 0; /*--- NT E3 ---*/
			pm_info->stat.isdn_status |= (PM_E1STATUS(i) & pm_info->deviceinfo[powerdevice_isdnte].power_rate) ? (1 << ((i-1) * 4)) : 0; /*--- TE-Bit ---*/
		}
		if (!isdn_status != !pm_info->stat.isdn_status) {
			if (pm_info->stat.isdn_status) {
				/*--- JZ-33133 (GRX) 200 MHz ist zu lahm ---*/
				PowerManagmentActivatePowerMode("e1_active");
			} else {
				PowerManagmentActivatePowerMode("e1_inactive");
			}
			isdn_status = pm_info->stat.isdn_status;
		}

		pm_info->stat.rate_abact     = (PM_GET_RATE(pm_info->deviceinfo[powerdevice_analog].norm_power_rate) == 0) ? 0 :
									   (PM_GET_RATE(pm_info->deviceinfo[powerdevice_analog].power_rate) * 100) /
									   (PM_GET_RATE(pm_info->deviceinfo[powerdevice_analog].norm_power_rate)); /*--- aktueller Nebenstellen-Verbrauch in Prozent ---*/
		pm_info->stat.rate_dectact       = PM_GET_RATE(pm_info->deviceinfo[powerdevice_dect].power_rate);     /*--- aktueller Verbrauch DECT in Prozent ---*/
		pm_info->stat.dect_status        = PM_DECT_GET_ECO(pm_info->deviceinfo[powerdevice_dect].power_rate) ? 1 : 0;      /*--- DECT-ECO Status ---*/
		pm_info->stat.rate_battchargeact = (PM_GET_RATE(pm_info->deviceinfo[powerdevice_charge].norm_power_rate) == 0) ? 0 :
										   (PM_GET_RATE(pm_info->deviceinfo[powerdevice_charge].power_rate) * 100) /
										   (PM_GET_RATE(pm_info->deviceinfo[powerdevice_charge].norm_power_rate));               /*--- Batterieladeverbrauch (DECT) ---*/
		usbmwatt = PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_usb_host]) +
				   PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_usb_host2]) +
				   PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_usb_host3]) +
				   0;
		normmwatt = PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_usb_host]) +
					PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_usb_host2]) +
					PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_usb_host3]) +
					0;
		if (normmwatt) {
			usbmwatt = (usbmwatt * 100) / normmwatt;
			if (usbmwatt > 100) {
				usbmwatt = 100;
			}
		}
		pm_info->stat.rate_usbhostact   = (unsigned char) usbmwatt;
		pm_info->stat.usb_status        = PM_GET_RATE(pm_info->deviceinfo[powerdevice_usb_client].power_rate) ? 1 : 0;  /*--- USB-Client connected ---*/
		pm_info->stat.rate_dvbcact      = PM_GET_RATE(pm_info->deviceinfo[powerdevice_dvbc].power_rate);                /*--- aktueller Verbrauch DVBC Prozent ---*/
		pm_info->stat.rate_plcact       = PM_GET_RATE(pm_info->deviceinfo[powerdevice_plc].power_rate);                /*--- aktueller Verbrauch PLC Prozent ---*/

		normmwatt = PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_lte]);
		if (normmwatt) {
			int ltemwatt  = PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_lte]);

			rate = (ltemwatt * 100) / normmwatt; /*--- aktueller Verbrauch LTE in Prozent ---*/
			if (rate > 100)  {
				rate = 100;
			}
		} else {
			rate = 0;
		}
		pm_info->stat.rate_lteact = rate; /*--- aktueller Verbrauch LTE in Prozent ---*/
		if (avm_power_disp_loadrate & 0x20) {
			pr_info("SUM:%d(%d) SYST:%d(%d)-%x DSP:%d(%d) WLAN:%d(%d)-%d-%x ETH:%d(%d)-%x ISDN:%x AB:%d(%d) DECT:%d(%d) USB:%d(%d)-%x LTE:%d(%d) DVBC:%d(%d) PLC:%d(%d) TEMP(%d, %d min %d max %d) want:%d\n",
				 pm_info->stat.rate_sumact,     pm_info->stat.rate_sumcum,
				 pm_info->stat.rate_systemact,  pm_info->stat.rate_systemcum, pm_info->stat.system_status,
				 pm_info->stat.rate_dspact,     pm_info->stat.rate_dspcum,
				 pm_info->stat.rate_wlanact,    pm_info->stat.rate_wlancum, pm_info->stat.wlan_devices, pm_info->stat.wlan_status,
				 pm_info->stat.rate_ethact,     pm_info->stat.rate_ethcum, pm_info->stat.eth_status,
				 pm_info->stat.isdn_status,
				 pm_info->stat.rate_abact,      pm_info->stat.rate_abcum,
				 pm_info->stat.rate_dectact,    pm_info->stat.rate_dectcum,
				 pm_info->stat.rate_usbhostact, pm_info->stat.rate_usbhostcum, pm_info->stat.usb_status,
				 pm_info->stat.rate_lteact,     pm_info->stat.rate_ltecum,
				 pm_info->stat.rate_dvbcact,    pm_info->stat.rate_dvbccum,
				 pm_info->stat.rate_plcact,     pm_info->stat.rate_plccum,
				 pm_info->stat.act_temperature, pm_info->stat.avg_temperature, pm_info->stat.min_temperature, pm_info->stat.max_temperature,
				 avm_event_source_check_id(pm_info->event_handle, avm_event_id_pm_ressourceinfo_status)
			  );
		}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
	}
	pm_ressourceinfo.kthread = NULL;
	DEB_ERR("[avm_power]%s: exit\n", __func__);
	return 0;
}

/**
 */
int pm_ressourceinfo_init(void)
{
	struct _avm_event_id_mask id_mask;
#if IS_ENABLED(CONFIG_AVM_EVENT)
	int i;
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/

	if (pm_ressourceinfo.deviceinfo[powerdevice_dspclock].power_rate == 0) {
		pm_ressourceinfo.deviceinfo[powerdevice_dspclock].power_rate = FREQUENZ_TO_PERCENT(STD_DSPCLK, MAX_DSPCLK);
	}
	if (pm_ressourceinfo.deviceinfo[powerdevice_cpuclock].power_rate == 0) {
		pm_ressourceinfo.deviceinfo[powerdevice_cpuclock].power_rate = FREQUENZ_TO_PERCENT(STD_MIPSCLK, MAX_MIPSCLK);
	}
	if (pm_ressourceinfo.deviceinfo[powerdevice_systemclock].power_rate == 0) {
		pm_ressourceinfo.deviceinfo[powerdevice_systemclock].power_rate = FREQUENZ_TO_PERCENT(STD_SYSTEMCLK, MAX_SYSTEMCLK);
	}
	init_waitqueue_head(&pm_ressourceinfo.wait_queue);
	pm_ressourceinfo.kthread = kthread_run(pm_ressourceinfo_thread, (void *) &pm_ressourceinfo, "pm_info");
	BUG_ON((pm_ressourceinfo.kthread == NULL) || IS_ERR((void *)pm_ressourceinfo.kthread));
#if IS_ENABLED(CONFIG_AVM_EVENT)
	for (i = 0; i < AVMPOWER_MAX_ETHERNETPORTS; i++) {
		pm_ressourceinfo.eth_status[i] = 2; /*--- auf normal vorinitialisieren ---*/
	}
	pm_ressourceinfo.event_handle = avm_event_source_register("pm_info_stat",
								avm_event_build_id_mask(&id_mask, 1, avm_event_id_pm_ressourceinfo_status),
								pm_ressourceinfo_notify,
								&pm_ressourceinfo
								);
	if (pm_ressourceinfo.event_handle == NULL) {
		pr_info("[avm_power] avm event register failed !\n");
		return -1;
	}
#endif
	PowerManagmentRessourceInfo_Init();
	return 0;
}

/**
 * Zeilenaufbau: alle Werte dezimal (line steht hinter '=')
 * PMINFO= device, act_power_rate
 */
void pm_ressourceinfo_parse(const char *line)
{
	const char *p = line;
	int device, power_rate;

	SKIP_SPACES(p);
	if (sscanf(p, "%d", &device) == 0) {
		device = powerdevice_none;
	}
	if (device <= powerdevice_none || device >= powerdevice_maxdevices) {
		DEB_ERR("[avm_power] p%s: unknown_device %d: '%s'\n", __func__, device, line);
		return;
	}
	if (avm_power_write_find_special_char(&p, ',')) {
		DEB_ERR("[avm_power] %s: invalid format '%s'\n", __func__, p);
		return;
	}
	SKIP_SPACES(p);
	if (sscanf(p, "%d", &power_rate)) {
		PowerManagmentRessourceInfo(device, power_rate);
	} else {
		DEB_ERR("[avm_power] %s: invalid format '%s'\n", __func__, p);
	}
}

/**
 * Zeilenaufbau: alle Werte dezimal (line steht hinter '=')
 * PMINFO_MODE = device, norm_power_rate, multiplier, divider, offset
 */
void pm_ressourceinfo_scriptparse(const char *line)
{
	struct _power_managment_device_info *df;
	const char *p = line;
	int mWatt, device, i;

	SKIP_SPACES(p);
	if (sscanf(p, "%d", &device) == 0) {
		device = powerdevice_none;
	}
	if (device <= powerdevice_none || device >= powerdevice_maxdevices) {
		DEB_ERR("[avm_power]%s: unknown_device %d: '%s'\n", __func__, device, line);
		return;
	}
	df =  &pm_ressourceinfo.deviceinfo[device];
	if (avm_power_write_find_special_char(&p, ',')) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	SKIP_SPACES(p);
	if (sscanf(p, "%d", &df->norm_power_rate) == 0) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
	}
	if (avm_power_write_find_special_char(&p, ',')) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	SKIP_SPACES(p);
	if (sscanf(p, "%d", &df->multiplier) == 0) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	if (avm_power_write_find_special_char(&p, ',')) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	SKIP_SPACES(p);
	if (sscanf(p, "%d", &df->divider) == 0) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	if (avm_power_write_find_special_char(&p, ',')) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	SKIP_SPACES(p);
	if (sscanf(p, "%d", &df->offset) == 0) {
		DEB_ERR("[avm_power]%s: invalid format '%s'\n", __func__, p);
		return;
	}
	if (df->divider != 0) {
		int SumNorm_mWatt = 0;

		for (i = 0; i < powerdevice_maxdevices; i++) {
			struct _power_managment_device_info *df1 =  &pm_ressourceinfo.deviceinfo[i];
				SumNorm_mWatt += PM_GET_NORM_MWATT(df1);
		}
		mWatt = PM_GET_NORM_MWATT(df);
		pr_info("[avm_power]%s: %s: norm_power_rate=%d act_rate=%d mul=%d div=%d offset=%d NormP=%d mW -> SumNormP=%d mW\n",
			__func__, pm_name_device(device), df->norm_power_rate, df->power_rate,
			df->multiplier, df->divider, df->offset, mWatt, SumNorm_mWatt);
		pm_ressourceinfo.NormP = SumNorm_mWatt;
	} else {
		DEB_ERR("[avm_power]%s: warning divider is zero '%s'\n", __func__, line);
	}
	pm_ressourceinfo.Changes++;
	wake_up_interruptible(&pm_ressourceinfo.wait_queue);
}
#if IS_ENABLED(CONFIG_AVM_EVENT)

/**
 */
void display_pminfostat(const char *prefix)
{
	struct _avm_event_pm_info_stat *pstat = &pm_ressourceinfo.stat;
	unsigned int i;
	char tmp[128], *p;

	tmp[0] = 0;
	p = tmp;
	/*--- pr_info("eth_status=0x%x %x", pstat->eth_status, pm_ressourceinfo.deviceinfo[powerdevice_ethernet].power_rate); ---*/
	for (i = 0; i < AVMPOWER_MAX_ETHERNETPORTS; i++) {
		if ((((pstat->eth_status >> (i * 2)) & 0x3) == 0x3)) {
			sprintf(p, "LAN%x ", i+1);
			p += strlen(p);
		}
	}
	pr_err("%sCPUs-Activity=%d %% DSL-Activity=%d %% WLAN-Activity=%d %% WLAN-Devices=%d%s USB=%d %% %s\n",
			prefix ? prefix : "",
			pm_ressourceinfo.deviceinfo[powerdevice_loadrate].power_rate,
			pstat->rate_dspact,
			pstat->rate_wlanact,
			pstat->wlan_devices,
			pstat->wlan_status ? "ECO" : "",
			pstat->rate_usbhostact, tmp);
}
#endif/*--- #if IS_ENABLED(CONFIG_AVM_EVENT) ---*/
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
