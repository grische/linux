/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _avm_irq_on_h_
#define _avm_irq_on_h_

/*---  CPU-depend use of request_irq, free_irq, enable_irq, disable_irq ---*/
int request_irq_on(int cpu, unsigned int irq, irq_handler_t handler,
		   unsigned long irqflags, const char *devname, void *dev_id);
int free_irq_on(int cpu, unsigned int irq, void *dev_id);
void enable_irq_on(int cpu, unsigned int irq);
void disable_irq_on(int cpu, unsigned int irq);

#endif /*--- #ifndef _avm_irq_on_h_ ---*/
