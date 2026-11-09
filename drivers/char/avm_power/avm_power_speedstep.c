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
#include <linux/file.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#pragma GCC pop_options

#include <avm/power/power.h>
#if __has_include(<avm/sammel/helpers.h>)
#include <avm/sammel/helpers.h>
#else
#include <linux/avm_helpers.h>
#endif
#include "avm_power.h"

/*--- #define DBG_TRC(args...)   printk(KERN_INFO args) ---*/
#define DBG_TRC(args...)
/**
 */
struct _speedstep {
	void *handle;
} speedstep;

#if defined(CONFIG_SOC_GRX500)
#define GOVERNOR_KICK      "/sys/devices/system/cpu/cpufreq/policy0/kick"
#define GOVERNOR_PAUSE     "lock"
#define GOVERNOR_RESTART   "unlock"
#endif/*--- #if defined(CONFIG_SOC_GRX500) ---*/

/**
 * Aufruf nur aus Prozesskontext (AVM_PM_ASYNC) !
 * state: 0 do nothing
 *        1 Kick
 */
static int avm_power_speedstep_Callback(int state __maybe_unused)
{
	DBG_TRC("%s: state=0x%x\n", __func__,  state);
#if defined(CONFIG_SOC_GRX500)
	if (state == 1) {
		avm_write_to_file(GOVERNOR_KICK, GOVERNOR_PAUSE, sizeof(GOVERNOR_PAUSE) - 1);
	} else if (state == 2) {
		avm_write_to_file(GOVERNOR_KICK, GOVERNOR_RESTART, sizeof(GOVERNOR_RESTART) - 1);
	} else if (state == 0x80) {
		/*--- JZ-33133 ---*/
		avm_write_to_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "200000", sizeof("200000") - 1);
	} else if (state == 0x81) {
		/*--- JZ-33133 ---*/
		avm_write_to_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", "333333", sizeof("333333") - 1);
	}
#endif/*--- #if defined(CONFIG_SOC_GRX500) ---*/
	return 0;
}

/**
 */
int avm_power_speedstep_init(void)
{
	speedstep.handle = PowerManagmentRegister("speedstep", avm_power_speedstep_Callback);
	return speedstep.handle ? 0 : -1;
}
#ifdef CONFIG_AVM_POWER_MODULE

/**
 */
void avm_power_speedstep_exit(void)
{
	if (speedstep.handle) {
		PowerManagmentRelease(speedstep.handle);
		speedstep.handle = NULL;
	}
}
#endif/*--- #ifdef CONFIG_AVM_POWER_MODULE ---*/
