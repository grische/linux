// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2014-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <asm/cacheflush.h>
#include <linux/printk.h>
#include <linux/kthread.h>

#include "avm_sammel.h"
#include <avm/event/event.h>
#include "avm_event_intern.h"
#include "avm_dist_event/endian.h"
#include "avm_dist_event/avm_event_gen_types.h"
#include "avm_event_remote.h"

#if defined(CONFIG_AVM_EVENTNODE_PUMA7)
#include <linux/interrupt.h>
#endif
#if defined(CONFIG_X86)
#include <linux/ce_mailbox.h>
#elif defined(REMOTE_EVENT_PUMA7_ARM)
#include <asm-arm/arch-avalanche/generic/arm_atom_mbx.h>
#include <asm-arm/arch-avalanche/puma7/puma7_bootcfg_ctrl.h>
#elif defined(REMOTE_EVENT_PUMA6_ARM)
#include <arch-avalanche/puma6/arm_atom_mbx.h>
#include <arch-avalanche/puma6/puma6_bootcfg_ctrl.h>
#endif
#include <avm/event/event.h>
#include <asm/atomic.h>

/**
 * fuer PumaX & Atom:
 * Schnittstelle um Remote-Trigger-Irqs (bis zu 16) generisch nutzen zu koennen
 * allerdings ist IRQ 0 ist fuer REBOOT-Signalisierung reserviert
 */

#if defined(CONFIG_X86)
#define INT_REMOTECPU_TO_HOSTCPU_MASK BOOTCFG_REG_SW_INT1_ARM11_2_ATOM_MASK
#define INT_HOSTCPU_TO_REMOTECPU_MASK BOOTCFG_REG_SW_INT_ATOM_2_ARM11_INTC_MASK
#define VAL_ENDIAN(val)               cpu_to_be32(val)
#define VAL_ENDIAN16(val)             cpu_to_be16(val)
#else /*--- #if defined(CONFIG_X86) ---*/
#define INT_REMOTECPU_TO_HOSTCPU_MASK BOOTCFG_REG_SW_INT_ATOM_2_ARM11_INTC_MASK
#define INT_HOSTCPU_TO_REMOTECPU_MASK BOOTCFG_REG_SW_INT1_ARM11_2_ATOM_MASK
#define VAL_ENDIAN(val)               (val)
#define VAL_ENDIAN16(val)             (val)
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_X86) ---*/

static DEFINE_SEMAPHORE(remotecpuirq_sema);

/**
 */
static struct _remotecpu_irq {
	atomic_t used;
	atomic_t pending;
	atomic_t enable;
	void *ref;
	int (*remotecpu_handler)(int irq, void *ref);
} remotecpu_irq[hweight32(INT_REMOTECPU_TO_HOSTCPU_MASK)];

/**
 * Schnittstelle um Remote-Trigger-Irqs (bis zu 16) generisch nutzen zu koennen
 * allerdings ist IRQ 0 fuer REBOOT-Signalisierung reserviert
 * Returncode vom Handler wird NICHT ausgewertet (dient nur zur Komaptibilität mit request_irq())
 * -> gleicher Handler verwendbar
 */
int request_remotecpu_irqhandler(int irq,
				 int (*remotecpu_handler)(int irq, void *ref),
				 void *ref)
{
	struct _remotecpu_irq *prcirq;

	/*---	pr_err("%s: irq=%d\n", __func__, irq); ---*/
	if (((unsigned int)irq >= hweight32(INT_REMOTECPU_TO_HOSTCPU_MASK))) {
		pr_err("%s remote-irq %d exceed range\n", __func__, irq);
		return -EINVAL;
	}
	if ((remotecpu_handler == NULL) || (ref == NULL)) {
		pr_err("%s invalid handler/ref\n", __func__);
		return -EINVAL;
	}
	prcirq = &remotecpu_irq[irq];
	down(&remotecpuirq_sema);
	if (atomic_read(&prcirq->used)) {
		pr_err("%s remote-irq %d already installed\n", __func__, irq);
		up(&remotecpuirq_sema);
		return -EBUSY;
	}
	prcirq->ref = ref;
	prcirq->remotecpu_handler = remotecpu_handler;
	atomic_set(&prcirq->used, 1);
	wmb();
	up(&remotecpuirq_sema);
	atomic_set(&prcirq->enable, 1);
	return irq;
}
EXPORT_SYMBOL(request_remotecpu_irqhandler);

/**
 */
int free_remotecpu_irqhandler(int irq, void *ref)
{
	struct _remotecpu_irq *prcirq;

	if (((unsigned int)irq >= hweight32(INT_REMOTECPU_TO_HOSTCPU_MASK))) {
		pr_err("%s remote-irq %d exceed range\n", __func__, irq);
		return -EINVAL;
	}
	prcirq = &remotecpu_irq[irq];
	down(&remotecpuirq_sema);
	if (atomic_read(&prcirq->used) == 0) {
		up(&remotecpuirq_sema);
		return -ERANGE;
	}
	if (prcirq->ref == ref) {
		atomic_set(&prcirq->used, 0);
		while (atomic_read(&prcirq->pending)) {
			schedule();
		}
		prcirq->remotecpu_handler = NULL;
		prcirq->ref = NULL;
		up(&remotecpuirq_sema);
	} else {
		pr_err("%s false ref\n", __func__);
	}
	up(&remotecpuirq_sema);
	return -ERANGE;
}
EXPORT_SYMBOL(free_remotecpu_irqhandler);

/**
 */
void disable_remotecpu_irqhandler(int irq)
{
	struct _remotecpu_irq *prcirq;

	if (((unsigned int)irq >= hweight32(INT_REMOTECPU_TO_HOSTCPU_MASK))) {
		pr_err("%s remote-irq %d exceed range\n", __func__, irq);
		return;
	}
	prcirq = &remotecpu_irq[irq];
	if (atomic_sub_return(1, &prcirq->enable) < 0) {
		pr_err("%s warning unbalanced disable\n", __func__);
		dump_stack();
	}
}
EXPORT_SYMBOL(disable_remotecpu_irqhandler);

/**
 */
void enable_remotecpu_irqhandler(int irq)
{
	struct _remotecpu_irq *prcirq;

	if (((unsigned int)irq >= hweight32(INT_REMOTECPU_TO_HOSTCPU_MASK))) {
		pr_err("%s remote-irq %d exceed range\n", __func__, irq);
		return;
	}
	prcirq = &remotecpu_irq[irq];
	atomic_add(1, &prcirq->enable);
}
EXPORT_SYMBOL(enable_remotecpu_irqhandler);

/**
 * Muss in  arm2atom_interrupt/atom2arm_interrupt einegahengt werden
 * Output: handled Causes
 */
unsigned int handle_remotecpuirq(unsigned int cause)
{
	int irq = 0;
	unsigned int shift;
	unsigned int handled = 0;
	unsigned int mask = VAL_ENDIAN(INT_REMOTECPU_TO_HOSTCPU_MASK);

	cause = VAL_ENDIAN(cause);
	shift = ffs(mask) - 1;
	/*--- pr_err("%s: cause=%x mask=%x shift=%d\n", __func__, cause, INT_REMOTECPU_TO_HOSTCPU_MASK, shift); ---*/
	mask >>= shift;
	cause >>= shift;
	while (mask && cause) {
		if ((mask & 0x1)) {
			int erg = (mask & cause) & 1;

			if (erg) {
				struct _remotecpu_irq *prcirq =
					&remotecpu_irq[irq];

				atomic_set(&prcirq->pending, 1);
				if (atomic_read(&prcirq->enable) &&
				    atomic_read(&prcirq->used) &&
				    prcirq->remotecpu_handler) {
					/*--- pr_err("%s: irq=%d ref=%p\n", __func__, irq, prcirq->ref); ---*/
					(void)prcirq->remotecpu_handler(
						irq, prcirq->ref);
				}
				atomic_set(&prcirq->pending, 0);
				handled |= 1 << shift;
			}
			irq++;
		}
		mask >>= 1;
		cause >>= 1;
		shift++;
	}
	/*--- pr_err("%s: done handled=%x\n", __func__, VAL_ENDIAN(handled)); ---*/
	return VAL_ENDIAN(handled);
}
EXPORT_SYMBOL(handle_remotecpuirq);

#if !defined(CONFIG_AVM_EVENTNODE_PUMA7)
static DEFINE_SPINLOCK(trigger_lock);
#endif

#if defined(CONFIG_AVM_EVENTNODE_PUMA7)
static volatile unsigned long irq_bits;

/**
 */
static void
trigger_remotecpu_from_wq_context(struct work_struct *wq)
{
	unsigned int shift, val, irq;

	shift = ffs(INT_HOSTCPU_TO_REMOTECPU_MASK) - 1;

	for (irq = 0; irq < hweight32(INT_HOSTCPU_TO_REMOTECPU_MASK); irq++) {
		if (test_and_clear_bit(irq, &irq_bits)) {
			val = VAL_ENDIAN16(1U << (irq)) << shift;
#if defined(REMOTE_EVENT_PUMA7_X86)
			//send the irq as normal event-> hw mailbox will take care of irq
			/*--- pr_err("[%s]: sending irq=%d val=%x mask=%x shift=%d\n", __func__, irq, val, INT_HOSTCPU_TO_REMOTECPU_MASK, shift); ---*/
			npcpu_appcpu_mbx_send_notification(
				BOOTCFG_REG_SW_INT_SET, &val);
#elif defined(REMOTE_EVENT_PUMA7_ARM)
			/*--- pr_err("[%s]: sending irq=%d val=%x mask=%x shift=%d\n", __func__, irq, val, INT_HOSTCPU_TO_REMOTECPU_MASK, shift); ---*/
			arm_atom_mbx_send_notification_over_hw_mbox(
				BOOTCFG_REG_SW_INT1_SET, &val);
#endif
		}
	}
}

static DECLARE_WORK(wq_trigger_remotecpu, trigger_remotecpu_from_wq_context);
#endif /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA7) ---*/

/**
 * Remote-CPU triggern
 */
void trigger_remotecpuirq(unsigned int irq)
{
	unsigned int shift, val;

#if !defined(CONFIG_AVM_EVENTNODE_PUMA7)
	unsigned long flags;
#endif

	if (irq >= hweight32(INT_HOSTCPU_TO_REMOTECPU_MASK)) {
		pr_err("%s remote-irq %d exceed range\n", __func__, irq);
		return;
	}
	shift = ffs(INT_HOSTCPU_TO_REMOTECPU_MASK) - 1;
	val = VAL_ENDIAN16(1U << (irq)) << shift;

#if defined(CONFIG_AVM_EVENTNODE_PUMA7)
	if (in_atomic()) {
		/* im irq-context/ausgeschalteten irqs workqueue zum triggern verwenden */
		if (test_and_set_bit(irq, &irq_bits) == 0) {
			schedule_work(&wq_trigger_remotecpu);
		}
		return;
	}
#if defined(REMOTE_EVENT_PUMA7_X86)
	/*---pr_err("[%s]: sending irq=%d val=%x mask=%x shift=%d\n", __func__, irq, val, INT_HOSTCPU_TO_REMOTECPU_MASK, shift);---*/
	//send the irq as normal event-> hw mailbox will take care of irq
	npcpu_appcpu_mbx_send_notification(BOOTCFG_REG_SW_INT_SET, &val);
#elif defined(REMOTE_EVENT_PUMA7_ARM)
	arm_atom_mbx_send_notification_over_hw_mbox(BOOTCFG_REG_SW_INT1_SET,
						    &val);
#endif
#else
	spin_lock_irqsave(&trigger_lock, flags);
/*--- pr_err("%s: irq=%d val=%x mask=%x shift=%d\n", __func__, irq, val, INT_HOSTCPU_TO_REMOTECPU_MASK, shift); ---*/
#if defined(REMOTE_EVENT_PUMA6_X86)
	npcpu_bootcfg_ctrl_write_reg(BOOTCFG_REG_SW_INT_CLR, val);
	wmb();
	npcpu_bootcfg_ctrl_write_reg(BOOTCFG_REG_SW_INT_SET, val);
#elif defined(REMOTE_EVENT_PUMA6_ARM)
	PAL_sysBootCfgCtrl_WriteReg(BOOTCFG_REG_SW_INT1_CLR, val);
	wmb();
	PAL_sysBootCfgCtrl_WriteReg(BOOTCFG_REG_SW_INT1_SET, val);
#else
#warning REMOTE_... not defined
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_X86) ---*/
	spin_unlock_irqrestore(&trigger_lock, flags);
#endif
}
EXPORT_SYMBOL(trigger_remotecpuirq);
