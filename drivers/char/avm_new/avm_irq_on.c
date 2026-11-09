// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2010-2019 AVM GmbH <fritzbox_info@avm.de> */
/*
 *   CPU-depend use of request_irq, free_irq, enable_irq, disable_irq
 */

#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/version.h>

#if !defined(CONFIG_AR9) && !defined(CONFIG_VR9) && !defined(CONFIG_AR10)
/**
 * fuer Lantiq bis VR9 gibt es eine entsprechende Loesung in ifx_mips_interrupt_on.c
 * (mit einigen weiteren Feinheiten)
 */

/*--- #define DBG_TRC(args...) printk(KERN_ERR args) ---*/
#define DBG_TRC(args...)

#if defined(CONFIG_SMP)
/**
 */
struct _generic_irq_on {
	enum {
		t_request_irq_on,
		t_free_irq_on,
		t_enable_irq_on,
		t_disable_irq_on
	} type;

	union {
		struct _param_request_irq_on {
			unsigned int irq;
			irq_handler_t handler;
			unsigned long irqflags;
			const char *devname;
			void *dev_id;
		} param_request_irq_on;

		struct _param_free_irq_on {
			unsigned int irq;
			void *dev_id;
		} param_free_irq_on;

		struct _param_enable_irq_on {
			unsigned int irq;
		} param_enable_irq_on;

		struct _param_disable_irq_on {
			unsigned int irq;
		} param_disable_irq_on;
	} param;
};

/**
 * Workerthread Kontext auf der ausgewaehlten CPU
 */
static long irq_on_startup(void *arg)
{
	struct cpumask tmask;
	struct _generic_irq_on *pirq_on = (struct _generic_irq_on *)arg;
	int retval = 0;
	int cpu = get_cpu();

	switch (pirq_on->type) {
	case t_request_irq_on:
		DBG_TRC("[%s request on cpu%d irq: %d %s %p]\n", __func__, cpu,
			pirq_on->param.param_request_irq_on.irq,
			pirq_on->param.param_request_irq_on.devname,
			pirq_on->param.param_request_irq_on.dev_id);
		retval = request_threaded_irq(
			pirq_on->param.param_request_irq_on.irq,
			pirq_on->param.param_request_irq_on.handler, NULL,
			pirq_on->param.param_request_irq_on.irqflags,
			pirq_on->param.param_request_irq_on.devname,
			pirq_on->param.param_request_irq_on.dev_id);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
		tmask = cpumask_of_cpu(cpu);
#else
		cpumask_copy(&tmask, cpumask_of(cpu));
#endif
		irq_set_affinity(pirq_on->param.param_request_irq_on.irq,
				 (const struct cpumask *)&tmask);
		break;
	case t_free_irq_on:
		DBG_TRC("[%s free on cpu%d irq: %d %p]\n", __func__, cpu,
			pirq_on->param.param_free_irq_on.irq,
			pirq_on->param.param_free_irq_on.dev_id);
		free_irq(pirq_on->param.param_free_irq_on.irq,
			 pirq_on->param.param_free_irq_on.dev_id);
		break;
	case t_enable_irq_on:
		DBG_TRC("[%s enable on cpu%d irq: %d]\n", __func__, cpu,
			pirq_on->param.param_enable_irq_on.irq);
		enable_irq(pirq_on->param.param_enable_irq_on.irq);
		break;
	case t_disable_irq_on:
		DBG_TRC("[%s disable on cpu%d irq: %d]\n", __func__, cpu,
			pirq_on->param.param_disable_irq_on.irq);
		disable_irq(pirq_on->param.param_disable_irq_on.irq);
		break;
	}
	put_cpu();
	return retval;
}
#endif /*--- #if defined(CONFIG_SMP) ---*/

/**
 */
int request_irq_on(int cpu, unsigned int irq,
		   irq_handler_t handler, unsigned long irqflags,
		   const char *devname, void *dev_id)
{
#if defined(CONFIG_SMP)
	struct cpumask tmask;
	int this_cpu = get_cpu();

	DBG_TRC("[%s(%d %s): %d -> %d %p]\n", __func__, irq, devname, this_cpu,
		cpu, dev_id);
	put_cpu();
	if (cpu != this_cpu) {
		struct _generic_irq_on irq_on;

		irq_on.type = t_request_irq_on;
		irq_on.param.param_request_irq_on.irq = irq;
		irq_on.param.param_request_irq_on.handler = handler;
		irq_on.param.param_request_irq_on.irqflags = irqflags;
		irq_on.param.param_request_irq_on.devname = devname;
		irq_on.param.param_request_irq_on.dev_id = dev_id;

		return work_on_cpu(cpu, irq_on_startup, &irq_on);
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
	tmask = cpumask_of_cpu(cpu);
#else
	cpumask_copy(&tmask, cpumask_of(cpu));
#endif
	irq_set_affinity(irq, (const struct cpumask *)&tmask);

#endif /*--- #if defined(CONFIG_SMP) ---*/
	return request_irq(irq, handler, irqflags, devname, dev_id);
}
EXPORT_SYMBOL(request_irq_on);

/**
 */
int free_irq_on(int cpu, unsigned int irq, void *dev_id)
{
#if defined(CONFIG_SMP)
	int this_cpu = get_cpu();

	put_cpu();
	if (cpu != (int)this_cpu) {
		struct _generic_irq_on irq_on;

		DBG_TRC("[%s(%d): %d -> %d]\n", __func__, irq, this_cpu, cpu);
		irq_on.type = t_free_irq_on;
		irq_on.param.param_free_irq_on.irq = irq;
		irq_on.param.param_free_irq_on.dev_id = dev_id;

		return work_on_cpu(cpu, irq_on_startup, &irq_on);
	}
#endif /*--- #if defined(CONFIG_SMP) ---*/
	free_irq(irq, dev_id);
	return 0;
}
EXPORT_SYMBOL(free_irq_on);

/**
 */
void enable_irq_on(int cpu, unsigned int irq)
{
#if defined(CONFIG_SMP)
	int this_cpu = get_cpu();

	put_cpu();
	if (cpu != (int)this_cpu) {
		struct _generic_irq_on irq_on;

		DBG_TRC("[%s(%d): %d -> %d]\n", __func__, irq, this_cpu, cpu);
		irq_on.type = t_enable_irq_on;
		irq_on.param.param_enable_irq_on.irq = irq;

		work_on_cpu(cpu, irq_on_startup, &irq_on);
		return;
	}
#endif /*--- #if defined(CONFIG_SMP) ---*/
	enable_irq(irq);
}
EXPORT_SYMBOL(enable_irq_on);

/**
 */
void disable_irq_on(int cpu, unsigned int irq)
{
#if defined(CONFIG_SMP)
	int this_cpu = get_cpu();

	put_cpu();
	if (cpu != (int)this_cpu) {
		struct _generic_irq_on irq_on;

		DBG_TRC("[%s(%d): %d -> %d]\n", __func__, irq, this_cpu, cpu);
		irq_on.type = t_disable_irq_on;
		irq_on.param.param_disable_irq_on.irq = irq;

		work_on_cpu(cpu, irq_on_startup, &irq_on);
		return;
	}
#endif /*--- #if defined(CONFIG_SMP) ---*/
	disable_irq(irq);
}
EXPORT_SYMBOL(disable_irq_on);

#endif /*--- #if !defined(CONFIG_AR9) && !defined(CONFIG_VR9) && !defined(CONFIG_AR10) ---*/
