/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2019 AVM GmbH <fritzbox_info@avm.de>
 *
 *   description: high-level yield-thread-interface mips34k
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/yield_context.h>

struct yield_data {
	int cpu, tc, signal;
	struct irq_data *irq_data;
	int (*handler)(int signal, void *dev_id);
	const char *devname;
	void *dev_id;
	struct list_head list;
};

static LIST_HEAD(yield_list);
static DEFINE_MUTEX(yield_lock);


static int yield_handler(int signal, void *dev_id)
{
	struct yield_data *yd = dev_id;

	if (yd->irq_data->chip->irq_ack)
		yd->irq_data->chip->irq_ack(yd->irq_data);
	return yd->handler(signal, yd->dev_id);
}

static struct yield_data *find_yield_data(int cpu, int tc, int signal)
{
	struct yield_data *yd;

	list_for_each_entry (yd, &yield_list, list) {
		if (yd->cpu == cpu && yd->tc == tc && yd->signal == signal) {
			return yd;
		}
	}

	return NULL;
}

int request_yield_on(int cpu, int tc, int irq, int signal,
		int (*handler)(int signal, void *dev_id),
		unsigned long irqflags, const char *devname, void *dev_id)
{
	int ret;
	struct yield_data *yd = kmalloc(sizeof(struct yield_data), GFP_KERNEL);

	if (!yd) {
		ret = -ENOMEM;
		goto err1;
	}

	mutex_lock(&yield_lock);
	ret = request_yield_handler_on(cpu, tc, signal, yield_handler, yd);
	if (ret < 0)
		goto err2;
	ret = request_irq(irq, dummy_isr, irqflags, devname, NULL);
	if (ret < 0)
		goto err3;
	ret = gic_map_setup(cpu, irq, 2, signal);
	if (ret < 0)
		goto err4;

	yd->cpu      = cpu;
	yd->tc       = tc;
	yd->signal   = signal;
	yd->irq_data = irq_get_irq_data(irq);
	yd->handler  = handler;
	yd->devname  = devname;
	yd->dev_id   = dev_id;
	list_add_tail(&yd->list, &yield_list);

	mutex_unlock(&yield_lock);

	return 0;

err4:
	free_irq(irq, dev_id);
err3:
	free_yield_handler_on(cpu, tc, signal, dev_id);
err2:
	mutex_unlock(&yield_lock);
	kfree(yd);
err1:
	return ret;
}
EXPORT_SYMBOL(request_yield_on);


int free_yield_on(int cpu, int tc, int signal)
{
	struct yield_data *yd;
	int irq, ret;

	mutex_lock(&yield_lock);
	yd  = find_yield_data(cpu, tc, signal);
	if (!yd) {
		mutex_unlock(&yield_lock);
		return -EINVAL;
	}

	list_del(&yd->list);

	irq = yd->irq_data->irq;

	gic_map_setup(cpu, irq, 0, 0);
	free_irq(irq, yd->dev_id);
	ret = free_yield_handler_on(cpu, tc, signal, yd->dev_id);
	mutex_unlock(&yield_lock);
	kfree(yd);

	return ret;
}
EXPORT_SYMBOL(free_yield_on);
