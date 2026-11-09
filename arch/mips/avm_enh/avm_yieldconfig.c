/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/processor.h>
#include <asm/watch.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include <asm/yield_context.h>

struct _yield_config {
	const enum _yield_signal_id id;
	const char *name;
	unsigned int
		tc_prio; /*--- value between 0 (lowest) until 3 (highest) ---*/
};
#define YIELD_CONFIG_ENTRY(a, prio)                                            \
	{                                                                      \
	id:                                                                    \
		a, name : #a, tc_prio : prio                                   \
	}
#if defined(CONFIG_SOC_GRX500)

struct _yield_config yield_avm_config[] = {
	YIELD_CONFIG_ENTRY(YIELD_PCMFRAME_ID, YIELD_TC_SCHED_PRIORITY_MAX),
	YIELD_CONFIG_ENTRY(YIELD_PCMLINK_ID, YIELD_TC_SCHED_PRIORITY_MED),
	YIELD_CONFIG_ENTRY(YIELD_MONITOR_IPI0_ID, YIELD_TC_SCHED_PRIORITY_LOW),
	YIELD_CONFIG_ENTRY(YIELD_MONITOR_IPI1_ID, YIELD_TC_SCHED_PRIORITY_LOW),
	YIELD_CONFIG_ENTRY(YIELD_PROFILE_IPI0_ID, YIELD_TC_SCHED_PRIORITY_LOW),
	YIELD_CONFIG_ENTRY(YIELD_PROFILE_IPI1_ID, YIELD_TC_SCHED_PRIORITY_LOW),
	YIELD_CONFIG_ENTRY(YIELD_GPTC_ID, YIELD_TC_SCHED_PRIORITY_MAX),
};
#else
enum _yield_config yield_avm_config[] = {
	YIELD_CONFIG_ENTRY(YIELD_INVAL_ID, YIELD_TC_SCHED_PRIORITY_MIN),
};
#endif /*--- #if defined(CONFIG_SOC_GRX500) ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void avm_yield_setup(void)
{
	unsigned int tc, cpu, i, signal_mask[YIELD_MAX_TC], prio[YIELD_MAX_TC];
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		memset(signal_mask, 0, sizeof(signal_mask));
		memset(prio, 0, sizeof(prio));
		for (i = 0; i < ARRAY_SIZE(yield_avm_config); i++) {
			enum _yield_signal_id id = yield_avm_config[i].id;
			const char *name = yield_avm_config[i].name;
			unsigned int prio_val = yield_avm_config[i].tc_prio;
			tc = YIELD_TC_BY_ID(id);
			if (tc >= YIELD_MAX_TC + YIELD_FIRST_TC) {
				panic("%s: false configuration: %s: cpu=%u signal=%u tc=%u not supported\nn",
				      __func__, name, cpu,
				      YIELD_SIGNAL_BY_ID(id), tc);
			}
			tc -= YIELD_FIRST_TC;
			if (YIELD_CPU_BY_ID(id) == cpu) {
				if (YIELD_SIGNALMASK_BY_ID(id) &
				    signal_mask[tc]) {
					panic("%s: false configuration: %s: cpu=%u signal=%u overlapped with existing signal_mask=0x%x\n",
					      __func__, name, cpu,
					      YIELD_SIGNAL_BY_ID(id),
					      signal_mask[tc]);
				}
				printk(KERN_ERR
				       "%s: %-24s: id=0x%08x: cpu=%u tc=%u prio=%u signal=%2d\n",
				       __func__, name, id, cpu,
				       tc + YIELD_FIRST_TC, prio_val,
				       YIELD_SIGNAL_BY_ID(id));
				signal_mask[tc] |= YIELD_SIGNALMASK_BY_ID(id);
				if (prio_val > prio[tc]) {
					prio[tc] = prio_val;
				}
			}
		}
		for (tc = 0; tc < YIELD_MAX_TC; tc++) {
			if (signal_mask[tc] == 0) {
				continue;
			}
			yield_context_init_on(cpu, tc + YIELD_FIRST_TC,
					      signal_mask[tc], prio[tc]);
		}
	}
}
