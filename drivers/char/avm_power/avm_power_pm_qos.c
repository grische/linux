/**
 * Stellt sicher, dass nur cpuidle-States verwendet werden, deren wakeup-latency nicht
 * den pcmlink-irq behindern
 */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#if KERNEL_VERSION(3, 2, 0) <= LINUX_VERSION_CODE
#include <linux/pm_qos.h>
#else
#include <linux/pm_qos_params.h>
#define pm_qos_request pm_qos_request_list
#endif
#pragma GCC pop_options

#include <avm/power/power.h>
#include "avm_power.h"

/*--- #define DBG_TRC(args...)   pr_info(args) ---*/
#define DBG_TRC(args...) no_printk(args)
/**
 */
static struct _avm_pm_qos {
	struct pm_qos_request latency_pm_qos_req_initial;
	struct pm_qos_request latency_pm_qos_req;
	struct mutex lock;
	void *handle;
} avm_pm_qos = {
	.lock = __MUTEX_INITIALIZER(avm_pm_qos.lock),
};
#if defined(CONFIG_UBIK2_MSEC_PER_IRQ)
/*--- die latency wirkt sehr restriktiv, aber es zeigt sich, dass cpuidle-state3 immer noch zu trigger-too-lates fuehrt ---*/
#define INITIAL_LATENCY_REQ_USEC       ((CONFIG_UBIK2_MSEC_PER_IRQ * 1000) / 25)
#define TELEFON_LATENCY_REQ_USEC       (20)
#else
#define INITIAL_LATENCY_REQ_USEC       ((100 * 1000) / 2)
#define TELEFON_LATENCY_REQ_USEC       ((10 * 1000) / 2)
#endif
/**
 * latency_req: 0 austragen
 *              1 latency entprechend CONFIG_UBIK2_MSEC_PER_IRQ
 *
 * Achtung - process-Kontext notwendig! (AVM_PM_ASYNC)
 */
static int avm_power_pm_qos_latency(int latency_req)
{
	mutex_lock(&avm_pm_qos.lock);
	if (!avm_pm_qos.handle)
		goto out;
	if (pm_qos_request_active(&avm_pm_qos.latency_pm_qos_req)) {
		pm_qos_remove_request(&avm_pm_qos.latency_pm_qos_req);
	}
	if (latency_req) {
		int usec_req = TELEFON_LATENCY_REQ_USEC;

		DBG_TRC("[avm_power]%s: set latency to %u us\n", __func__, usec_req);
		pm_qos_add_request(&avm_pm_qos.latency_pm_qos_req, PM_QOS_CPU_DMA_LATENCY, usec_req);
	} else {
		DBG_TRC("[avm_power]%s: reset latency\n", __func__);
	}
out:
	mutex_unlock(&avm_pm_qos.lock);
	return 0;
}
/**
 */
int avm_power_pm_qos_init(void)
{
	int usec_req = INITIAL_LATENCY_REQ_USEC;

	mutex_lock(&avm_pm_qos.lock);
	pm_qos_add_request(&avm_pm_qos.latency_pm_qos_req_initial,
			PM_QOS_CPU_DMA_LATENCY, usec_req);
	DBG_TRC("[avm_power]%s: set initial latency to %u us\n", __func__, usec_req);
	avm_pm_qos.handle = PowerManagmentRegister("pm_qos_latency", avm_power_pm_qos_latency);
	mutex_unlock(&avm_pm_qos.lock);
	return avm_pm_qos.handle ? 0 : -1;
}
/**
 */
void avm_power_pm_qos_exit(void)
{
	mutex_lock(&avm_pm_qos.lock);
	if (avm_pm_qos.handle) {
		PowerManagmentRelease(avm_pm_qos.handle);
		avm_pm_qos.handle =  NULL;
	}
	if (pm_qos_request_active(&avm_pm_qos.latency_pm_qos_req_initial)) {
		pm_qos_remove_request(&avm_pm_qos.latency_pm_qos_req_initial);
	}
	if (pm_qos_request_active(&avm_pm_qos.latency_pm_qos_req)) {
		pm_qos_remove_request(&avm_pm_qos.latency_pm_qos_req);
	}
	mutex_unlock(&avm_pm_qos.lock);
}
