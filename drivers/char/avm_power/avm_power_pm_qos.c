/**
 * Stellt sicher, dass nur cpuidle-States verwendet werden, deren wakeup-latency nicht
 * den pcmlink-irq behindern
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#if KERNEL_VERSION(3, 2, 0) <= LINUX_VERSION_CODE
#include <linux/pm_qos.h>
#else
#include <linux/pm_qos_params.h>
#define pm_qos_request pm_qos_request_list
#endif

#include <avm/power/power.h>
#include "avm_power.h"

#define DBG_TRC(args...)   pr_info(args)
/* #define DBG_TRC(args...) no_printk(args) */

static int avm_power_pm_qos_latency_invariable(int latency_req_ms);
static int avm_power_pm_qos_latency(int latency_req);
#define TELEFON_LATENCY_REQ_USEC    20 /* usec */
/**
 */
static struct _avm_pm_qos {
	struct mutex lock;
	struct pm_qos_request latency_pm_qos_req;
	void *handle;
	const char *name;
	int (*pm_qos_cb)(int latency_req_ms);
} avm_pm_qos[2] = {
	[0] { .lock = __MUTEX_INITIALIZER(avm_pm_qos[0].lock), .name = "pm_qos_latency_invariable", .pm_qos_cb = avm_power_pm_qos_latency_invariable }, /* depend on pcmlink (permanent) */
	[1] { .lock = __MUTEX_INITIALIZER(avm_pm_qos[1].lock), .name = "pm_qos_latency",            .pm_qos_cb = avm_power_pm_qos_latency },            /* depend on telefon-state */
};

/**
 * latency_req_usec: 0 austragen
 *                   x latency in usec
 *
 * Achtung - process-Kontext notwendig! (AVM_PM_ASYNC)
 */
static int _avm_power_pm_qos_latency(struct _avm_pm_qos *ppm_qos, int latency_req_usec)
{
	mutex_lock(&ppm_qos->lock);
	if (!ppm_qos->handle)
		goto out;
	if (pm_qos_request_active(&ppm_qos->latency_pm_qos_req))
		pm_qos_remove_request(&ppm_qos->latency_pm_qos_req);

	if (latency_req_usec) {
		DBG_TRC("[avm_power]%s: set latency to %u us\n", __func__, latency_req_usec);
		pm_qos_add_request(&ppm_qos->latency_pm_qos_req, PM_QOS_CPU_DMA_LATENCY, latency_req_usec);
	} else {
		DBG_TRC("[avm_power]%s: reset latency\n", __func__);
	}
out:
	mutex_unlock(&ppm_qos->lock);
	return 0;
}

/**
 * puma7: Solange der pcmlink läuft, ist dauerhaft eine definierte maximale Latenz notwendig.
 * Sonst wird der netss_irq verschluckt.
 * irq_period_ms: 0 austragen
 *                Irq-Period in ms
 *
 * Achtung - process-Kontext notwendig! (AVM_PM_ASYNC)
 */
static int avm_power_pm_qos_latency_invariable(int irq_period_ms __maybe_unused)
{
	int usec_req;
	struct _avm_pm_qos *ppm_qos = &avm_pm_qos[0];

	/**
	 * puma7: sehr restriktiv: selbst im PowerState-3 (200us) gehen sonst Irqs verloren
	 */
	usec_req = 199;
	return _avm_power_pm_qos_latency(ppm_qos, usec_req);
}

/**
 * puma7: wenn Telefonie in-/aktiv
 * latency_req: 0 austragen
 *              1 minimale Latenz an
 *
 * Achtung - process-Kontext notwendig! (AVM_PM_ASYNC)
 */
static int avm_power_pm_qos_latency(int latency_req)
{
	int usec_req;
	struct _avm_pm_qos *ppm_qos = &avm_pm_qos[1];

	usec_req = latency_req ? TELEFON_LATENCY_REQ_USEC : 0;
	return _avm_power_pm_qos_latency(ppm_qos, usec_req);
}

/**
 */
int avm_power_pm_qos_init(void)
{
	unsigned int i;

	DBG_TRC("[avm_power]%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(avm_pm_qos); i++) {
		struct _avm_pm_qos *ppm_qos = &avm_pm_qos[i];

		ppm_qos->handle = PowerManagmentRegister(ppm_qos->name, ppm_qos->pm_qos_cb);
	}
	return 0;
}
/**
 */
void avm_power_pm_qos_exit(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(avm_pm_qos); i++) {
		struct _avm_pm_qos *ppm_qos = &avm_pm_qos[i];

		mutex_lock(&ppm_qos->lock);
		if (ppm_qos->handle) {
			PowerManagmentRelease(ppm_qos->handle);
			ppm_qos->handle = NULL;
		}
		if (pm_qos_request_active(&ppm_qos->latency_pm_qos_req))
			pm_qos_remove_request(&ppm_qos->latency_pm_qos_req);
		mutex_unlock(&ppm_qos->lock);
	}
}
