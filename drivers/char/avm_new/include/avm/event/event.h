/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _avm_linux_event_h_
#define _avm_linux_event_h_

#include <uapi/avm/event/event.h>

struct seq_file;
extern void dump_event_node_stats(struct seq_file *s);

#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)

#if defined(CONFIG_AVM_EVENTNODE_PUMA6)
#define ARM_MEM_OFFSET 0x40000000
#else
#define ARM_MEM_OFFSET 0x0
#endif

int avm_event_node_established(
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2),
	void *context, unsigned int param1, unsigned int param2);
#define FLAG_REMOTELOG_REBOOT 0x1
#define FLAG_REMOTELOG_APPEND 0x2
#define FLAG_REMOTELOG_APPEND_FINISHED (0x4 | FLAG_REMOTELOG_APPEND)
void avm_event_send_log(enum _avm_logtype type, unsigned int flag,
			const char *buffer, unsigned int len);

/**
 * ist in den arm2atom/atom2arm-irq einzuklinken:
 */
unsigned int handle_remotecpuirq(unsigned int cause);

/**
 * Schnittstelle um Remote-Trigger-Irqs (bis zu 16) generisch nutzen zu koennen
 * allerdings ist IRQ 0 fuer REBOOT-Signalisierung reserviert
 * Returncode vom Handler wird NICHT ausgewertet (dient nur zur Komaptibilität mit request_irq())
 * -> gleicher Handler verwendbar
 */
#define AVM_EVENT_REMOTECPU_IRQ   0x1
#define AVM_CPUCONN_REMOTECPU_IRQ 0x2
#define AVM_PCMLINK_REMOTECPU_IRQ 0x3
/**
 * like request_irq ...
 */
int request_remotecpu_irqhandler(int irq,
				 int (*remotecpu_handler)(int irq, void *ref),
				 void *ref);
int free_remotecpu_irqhandler(int irq, void *ref);
void enable_remotecpu_irqhandler(int irq);
void disable_remotecpu_irqhandler(int irq);

/**
 * Remote-CPU triggern
 */
void trigger_remotecpuirq(unsigned int irq);
#else /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA6) ---*/
/**
 */
static inline int avm_event_node_established(
	void (*event_established_cb)(void *context, unsigned int param1,
				     unsigned int param2),
	void *context, unsigned int param1, unsigned int param2)
{
	if (event_established_cb)
		event_established_cb(context, param1, param2);
	return 0;
}
#endif
#endif
