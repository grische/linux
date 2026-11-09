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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/file.h>
#include <avm/power/power.h>
#include <avm/event/event.h>
#include <avm/compat/misc.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <asm/errno.h>
#include <linux/cdev.h>

#include "avm_power.h"

/*--- #define DEBUG_TRACE_POWERTAB ---*/
#if defined(DEBUG_TRACE_POWERTAB)
#define DEB_TRC_PT      DEB_ERR
#else/*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/
#define DEB_TRC_PT(args...)
#endif/*--- #else ---*//*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/

/**
 */
struct _avm_power {
	struct class *osclass;
	dev_t         device;
	struct cdev  *cdev;
};

/**
 */
struct _avm_power_open_data {
	unsigned int reserved;
};

/**
 */
#define SKIP_SPACES(p) {while ((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) (p)++; }
#define CMD_MODE     "MODE"
#define WE_CMD_MODE  "WE_MODE"
#define PMINFO_MODE  "PMINFO_MODE"
#define PMINFO_SET   "PMINFO_SET"
#define ETH_MODE     "ETH_MODE"
#define IDLE_MODE    "IDLE_MODE"
#define LOAD_MODE    "LOAD_MODE"

unsigned int avm_power_disp_loadrate;

/**
 */
static int avm_power_open(struct inode *, struct file *);
static int avm_power_close(struct inode *, struct file *);
static ssize_t avm_power_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos);

/**
 */
struct _avm_power avm_power;

/**
 */
const struct file_operations avm_power_fops = {
	.owner		= THIS_MODULE,
	.open		= avm_power_open,
	.release	= avm_power_close,
	/*--- .read	= avm_power_read, ---*/
	.write		= avm_power_write,
	/*--- .ioctl = avm_power_ioctl, ---*/
	/*--- .fasync = avm_power_fasync, ---*/
	/*--- .poll = avm_power_poll, ---*/
};

/**
 */
int __init avm_power_file_init(void)
{
	int reason;

	reason = alloc_chrdev_region(&avm_power.device, 0, 1, MODULE_NAME);
	if (reason) {
		DEB_ERR("[%s] register_chrdev_region failed: reason %d!\n", MODULE_NAME, reason);
		return -ERESTARTSYS;
	}
	avm_power.cdev = cdev_alloc();
	if (!avm_power.cdev) {
		unregister_chrdev_region(avm_power.device, 1);
		DEB_ERR("[%s] cdev_alloc failed!\n", MODULE_NAME);
		return -ERESTARTSYS;
	}
	avm_power.cdev->owner = avm_power_fops.owner;
	avm_power.cdev->ops = &avm_power_fops;
	kobject_set_name(&(avm_power.cdev->kobj), MODULE_NAME);

	if (cdev_add(avm_power.cdev, avm_power.device, 1)) {
		kobject_put(&avm_power.cdev->kobj);
		unregister_chrdev_region(avm_power.device, 1);
		DEB_ERR("[%s] cdev_add failed!\n", MODULE_NAME);
		return -ERESTARTSYS;
	}
	/*--- Geraetedatei anlegen: ---*/
	avm_power.osclass = class_create(THIS_MODULE, MODULE_NAME);
	device_create(avm_power.osclass, NULL, avm_power.device, NULL, "avm_power");
	return 0;
}

/**
 */
static int avm_power_open(struct inode *inode __attribute__((unused)), struct file *filp)
{
	struct _avm_power_open_data *open_data;

	DEB_INFO("[%s]: %s:\n", MODULE_NAME, __func__);

	open_data = kmalloc(sizeof(struct _avm_power_open_data), GFP_KERNEL);
	if (!open_data) {
		DEB_ERR("%s: %s: open malloc failed\n", MODULE_NAME, __func__);
		return -EFAULT;
	}
	memset(open_data, 0, sizeof(*open_data));
	filp->private_data = (void *)open_data;

	DEB_INFO("[%s]: %s: open success flags=0x%x\n", MODULE_NAME, __func__, filp->f_flags);
	return 0;
}

/**
 */
static int avm_power_close(struct inode *inode __attribute__((unused)), struct file *filp)
{

	DEB_INFO("[%s]: %s:\n", MODULE_NAME, __func__);

	kfree(filp->private_data);
	filp->private_data = NULL;
	return 0;
}

/**
 */
static ssize_t avm_power_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos)
{
	char Buffer[256], *pm;
	const char *p;
#if defined(AVM_POWER_DEBUG)
	unsigned int org_write_length;
#endif
	const struct _power_managment_dest_entry *powermodetab __attribute__((unused)) = NULL;

	if (write_pos != NULL) {
		DEB_INFO("[%s]: %s write_length = %zu *write_pos = 0x%llx\n", MODULE_NAME, __func__, write_length, *write_pos);
	}
#if defined(AVM_POWER_DEBUG)
	org_write_length = write_length;
#endif

	if (write_length >= sizeof(Buffer)) {
		write_length = sizeof(Buffer) - 1;
		DEB_NOTE("[%s] %s: long line reduce to %zu bytes\n", MODULE_NAME, __func__, write_length);
	}
	if (filp == NULL) {
		memcpy(Buffer, write_buffer, write_length);
	} else {
		if (copy_from_user(Buffer, write_buffer, write_length)) {
			DEB_ERR("[%s]: %s: copy_from_user failed\n", MODULE_NAME, __func__);
			return -EFAULT;
		}
	}
	/**
	 */
	Buffer[write_length] = '\0';
#if defined(AVM_POWER_DEBUG)
	DEB_NOTE("[%s] %s: org_len=%u len %u = '%s'\n", MODULE_NAME, __func__, org_write_length, write_length, Buffer);
#endif
	pm = strchr(Buffer, 0x0A);
	if (pm) {
		*pm = '\0';
		write_length = strlen(Buffer) + 1;
		DEB_NOTE("[%s] %s: multi line reduce to %zu bytes\n", MODULE_NAME, __func__, write_length);
	}
	p = Buffer;

	/**
	 * cmd extrahieren
	 */
	SKIP_SPACES(p);
	DEB_NOTE("[%s]: %s process input \"%s\"\n", MODULE_NAME, __func__, p);

	if (0) {
#if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
	} else if (!strncmp(CMD_MODE, p, sizeof(CMD_MODE) - 1)) {
		p += sizeof(CMD_MODE) - 1;
		if (avm_power_write_find_special_char(&p, '=')) {
			avm_power_writeformaterror("[avm_power] format error: \""CMD_MODE" = <powermode>\"");
			/*--- return -EPERM; ---*/
			return write_length;
		}
		SKIP_SPACES(p);
		powermodetab = find_powermode(p, PM_ACCESS_APPL);
#if defined(CONFIG_AVM_POWERMETER)
	} else if (!strncmp(PMINFO_MODE, p, sizeof(PMINFO_MODE) - 1)) {
		p += sizeof(PMINFO_MODE) - 1;
		if (avm_power_write_find_special_char(&p, '=')) {
			DEB_ERR("[%s] format error: '%s'\n", MODULE_NAME, p);
			DEB_ERR("[%s] use: \""PMINFO_MODE" = device, norm_rate, multiplier, divider, offset\"\n", MODULE_NAME);
			/*--- return -EPERM; ---*/
			return write_length;
		}
		pm_ressourceinfo_scriptparse(p);
		return write_length;
	} else if (!strncmp(PMINFO_SET, p, sizeof(PMINFO_SET) - 1)) {
		p += sizeof(PMINFO_SET) - 1;
		if (avm_power_write_find_special_char(&p, '=')) {
			DEB_ERR("[%s] format error: '%s'\n", MODULE_NAME, p);
			DEB_ERR("[%s] use: \""PMINFO_SET" = device, power_rate\n", MODULE_NAME);
			return write_length;
		}
		pm_ressourceinfo_parse(p);
		return write_length;
	} else if (!strncmp(ETH_MODE, p, sizeof(ETH_MODE) - 1)) {
		union  _powermanagment_ethernet_state eth;
		unsigned int tmp, throttle = ETH_NORMAL_DEMAND_CON;

		eth.Register = 0;
		p += sizeof(ETH_MODE) - 1;
		if (avm_power_write_find_special_char(&p, '=')) {
			DEB_ERR("[%s] format error: '%s'\n", MODULE_NAME, p);
			DEB_ERR("[%s] use: \""ETH_MODE" = port, state\"\n", MODULE_NAME);
			/*--- return -EPERM; ---*/
			return write_length;
		}
		SKIP_SPACES(p);
		sscanf(p, "%u", &tmp);
		if (tmp > 255) {
			DEB_ERR("[%s] : unknown port %u:\n", MODULE_NAME, tmp);
			return write_length;
		}
		eth.Bits.port = tmp;
		tmp = 4;
		if (avm_power_write_find_special_char(&p, ',') == 0) {
			SKIP_SPACES(p);
			sscanf(p, "%u", &tmp);
		}
		switch (tmp) {
		default:
			DEB_ERR("[%s] : unknown status - set status to powered(2)\n", MODULE_NAME);
			tmp = 2;
			fallthrough;
		case 0:
		case 2:
			eth.Bits.status = tmp;
			break;
		case 1:
			throttle = ETH_THROTTLE_DEMAND_CON;
			eth.Bits.status = tmp;
			break;
		case 3:
			eth.Bits.status = 1; /*--- powersave ohne throttle ---*/
			break;
		}
		if (eth.Bits.port < AVMPOWER_MAX_ETHERNETPORTS) {
			pm_ressourceinfo.eth_status[eth.Bits.port] = ETH_MASK_BITS(pm_ressourceinfo.eth_status[eth.Bits.port], 0x3 | ETH_THROTTLE_DEMAND_CON | ETH_NORMAL_DEMAND_CON, throttle) | eth.Bits.status;
			tmp = pm_ressourceinfo_limit_set_eth(&pm_ressourceinfo, eth.Bits.port, 1);
		} else {
			tmp = 1;
		}
		if (tmp == 2) {
			DEB_ERR("[%s] : ethernet not registered\n", MODULE_NAME);
		} else if (tmp) {
			DEB_ERR("[%s] : ethernet switch failed\n", MODULE_NAME);
		}
		return write_length;
#endif/*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
	} else if (!strncmp(LOAD_MODE, p, sizeof(LOAD_MODE) - 1)) {
		unsigned int val, scale = 1;
		p += sizeof(LOAD_MODE) - 1;
		if (avm_power_write_find_special_char(&p, '=')) {
			DEB_ERR("[%s] format error: '%s'\n", MODULE_NAME, p);
			DEB_ERR("[%s] use: \""LOAD_MODE" = mode (0 auto, 1 off, > 1 Level\"\n", MODULE_NAME);
			/*--- return -EPERM; ---*/
			return write_length;
		}
		SKIP_SPACES(p);
		sscanf(p, "%u", &val);
		if (avm_power_write_find_special_char(&p, ',') == 0) {
			SKIP_SPACES(p);
			sscanf(p, "%u", &scale);
		}
		avm_powermanager_load_control_set(val, scale);
#endif/*--- #if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE) ---*/
	} else if (!strncmp(IDLE_MODE, p, sizeof(IDLE_MODE) - 1)) {
		p += sizeof(IDLE_MODE) - 1;
		if (avm_power_write_find_special_char(&p, '=')) {
			DEB_ERR("[%s] format error: '%s'\n", MODULE_NAME, p);
			DEB_ERR("[%s] use: \""IDLE_MODE" = mode\"\n", MODULE_NAME);
			/*--- return -EPERM; ---*/
			return write_length;
		}
		SKIP_SPACES(p);
		sscanf(p, "%x", &avm_power_disp_loadrate);
		pr_err("mode=0x%x\n", avm_power_disp_loadrate);
		return write_length;
#if defined(CONFIG_AVM_POWER_REMOTE_SINK) && 0
	} else if (!strncmp("sinkinit", p, sizeof("sinkinit") - 1)) {
		avm_power_remote_sink_init();
	} else if (!strncmp("sinkexit", p, sizeof("sinkexit") - 1)) {
		avm_power_remote_sink_exit();
#endif/*--- #ifdef CONFIG_AVM_POWER_REMOTE_SINK ---*/
	} else {
#if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
		avm_power_writeformaterror("[avm_power] format error: \""CMD_MODE" = <powermode>\"");
#endif/*--- #if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE) ---*/
		/*--- return -EPERM; ---*/
		return write_length;
	}
#if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE)
	if (powermode_action(powermodetab, 0)) {
		/*--- pr_err("[avm_power] error on activate powermode"); ---*/
		/*--- return -EPERM; ---*/
		return write_length;
	}
#endif/*--- #if !defined(CONFIG_AVM_POWER_REMOTE_SOURCE) ---*/
	return write_length;
}

/**
 * aktuell max. 4 CPU's
 */
char *display_per_cpurate(char *txt, int txt_len, unsigned int cpuspowerrate)
{
	unsigned int cpu, len, run;
	char idle_txt[64], *pidle_txt = idle_txt;
	char run_txt[64], *prun_txt   = run_txt;
	unsigned int idle_txt_len = sizeof(idle_txt);
	unsigned int run_txt_len  = sizeof(run_txt);

	idle_txt[0] = 0;
	run_txt[0]  = 0;
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		if (cpu_online(cpu)) {
			run = cpuspowerrate & 0xFF;
			if (idle_txt_len) {
				len = snprintf(pidle_txt, idle_txt_len, "%s%u", idle_txt[0] ? "/" : "", 100 - run);
				len = min(idle_txt_len, len);
				idle_txt_len -= len, pidle_txt += len;
			}
			if (run_txt_len) {
				len = snprintf(prun_txt, run_txt_len, "%s%u", run_txt[0] ? "/" : "", run);
				len = min(run_txt_len, len);
				run_txt_len -= len, prun_txt += len;
			}
		}
		cpuspowerrate >>= 8;
	}
	snprintf(txt, txt_len, "idle: %s %% (%s %%)", idle_txt, run_txt);
	return txt;
}
