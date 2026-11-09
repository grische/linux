// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <asm/atomic.h>
#endif /*--- #if defined(__KERNEL__) ---*/

/*--- #include <linux/avm_event.h> ---*/
#include <avm/event/event.h>
#include "internal.h"

#if defined(__KERNEL__)
#define LOCAL_LOCK() spin_lock_irqsave(&avm_event_lock, flags)
#define LOCAL_UNLOCK() spin_unlock_irqrestore(&avm_event_lock, flags)
#endif /*--- #if defined(__KERNEL__) ---*/
static struct _avm_event_item *Items;
static struct _avm_event_data *Datas;
static unsigned int max_Items, max_Datas;

volatile struct _avm_event_item *avm_event_first_Item;
volatile struct _avm_event_data *avm_event_first_Data;

/**
 */
struct _avm_event_queue {
	unsigned int rx, tx, size, count;
	void **data;
};

struct _avm_event_queue free_Items;
struct _avm_event_queue free_Datas;

/**
 */
int avm_event_deinit2(void)
{
	if (free_Items.data) {
		/*--- check if all Items are in free Queue ---*/
		if (free_Items.count != (free_Items.size - 1)) {
			pr_err("[avm_event] ERROR: not all Item(s) freeed %u missing\n",
			       (free_Items.size - 1) - free_Items.count);
			return 1;
		}
	}
	if (free_Datas.data) {
		/*--- check if all Datas are in free Queue ---*/
		if (free_Datas.count != (free_Datas.size - 1)) {
			pr_err("[avm_event] ERROR: not all Data(s) freeed %u missing\n",
			       (free_Datas.size - 1) - free_Datas.count);
			return 1;
		}
	}

	avm_event_first_Item = NULL;
	avm_event_first_Data = NULL;

	kfree(Items);
	kfree(Datas);
	kfree(free_Items.data);
	kfree(free_Datas.data);

	return 0;
}

/**
 */
int avm_event_init2(unsigned int max_items, unsigned int max_datas)
{
	unsigned int i;

	Items = (struct _avm_event_item *)kmalloc(
		sizeof(struct _avm_event_item) * max_items, GFP_ATOMIC);
	if (Items == NULL) {
		return -ENOMEM;
	}
	max_Items = max_items;

	free_Items.data = kmalloc(sizeof(void *) * (max_items + 1), GFP_ATOMIC);
	if (free_Items.data == NULL) {
		kfree(Items);
		return -ENOMEM;
	}
	avm_event_first_Item = (struct _avm_event_item *)&(free_Items.data[0]);
	for (i = 0; i < max_items; i++) {
		Items[i].next = (struct _avm_event_item *)((unsigned int)-1);
		Items[i].data = NULL;
		free_Items.data[i] = &Items[i];
		if (i < max_items - 1)
			Items[i].debug =
				(struct _avm_event_data *)&(Items[i + 1]);
		else
			Items[i].debug = NULL;
	}

	free_Items.rx = 0;
	free_Items.tx = max_items;
	free_Items.count = max_items;
	free_Items.size = max_items + 1;

	Datas = kmalloc(sizeof(struct _avm_event_data) * max_datas, GFP_ATOMIC);
	if (Datas == NULL) {
		kfree(Items);
		kfree(free_Items.data);
		return -ENOMEM;
	}
	max_Datas = max_datas;

	free_Datas.data = kmalloc(sizeof(void *) * (max_datas + 1), GFP_ATOMIC);
	if (free_Datas.data == NULL) {
		kfree(Items);
		kfree(Datas);
		kfree(free_Items.data);
		return -ENOMEM;
	}

	avm_event_first_Data = (struct _avm_event_data *)&(free_Datas.data[0]);
	for (i = 0; i < max_datas; i++) {
		Datas[i].data = NULL;
		Datas[i].data_length = 0;
		free_Datas.data[i] = &Datas[i];
		if (i < max_datas - 1)
			Datas[i].debug = &(Datas[i + 1]);
		else
			Datas[i].debug = NULL;
	}

	free_Datas.rx = 0;
	free_Datas.tx = max_datas;
	free_Datas.count = max_datas;
	free_Datas.size = max_datas + 1;

	return 0;
}


/**
 */
struct _avm_event_data *avm_event_alloc_data(void)
{
	struct _avm_event_data *D;
	unsigned long flags;

	LOCAL_LOCK();

	if (free_Datas.rx == free_Datas.tx) {
		LOCAL_UNLOCK();
		return NULL;
	}

	/*--- aus der free queue holen ---*/
	D = free_Datas.data[free_Datas.rx++];
	if (free_Datas.rx >= free_Datas.size)
		free_Datas.rx = 0;
	free_Datas.count--;

	LOCAL_UNLOCK();

	atomic_set(&D->link_count, 0);
	D->data = NULL;
	D->data_length = 0;
	return D;
}

/**
 */
void avm_event_free_data(struct _avm_event_data *D)
{
	unsigned long flags;

	if (atomic_dec_and_test(&D->link_count) == 0) {
		return;
	}
	kfree(D->data);
	D->data = NULL;

	LOCAL_LOCK();

	/*--- es sind auf jeden fall genügend elemente in der Queue ---*/
	free_Datas.data[free_Datas.tx++] = D;
	if (free_Datas.tx >= free_Datas.size)
		free_Datas.tx = 0;
	free_Datas.count++;

	LOCAL_UNLOCK();
}

/**
 */
struct _avm_event_item *avm_event_alloc_item(void)
{
	struct _avm_event_item *I;
	unsigned long flags;

	LOCAL_LOCK();

	if (free_Items.rx == free_Items.tx) {
		LOCAL_UNLOCK();
		return NULL;
	}
	I = free_Items.data[free_Items.rx++];
	if (free_Items.rx >= free_Items.size)
		free_Items.rx = 0;
	free_Items.count--;

	LOCAL_UNLOCK();
	I->next = NULL;
	I->data = NULL;
	return I;
}

/**
 */
void avm_event_free_item(struct _avm_event_item *I)
{
	unsigned long flags;

	if (I->data) {
		avm_event_free_data((struct _avm_event_data *)I->data);
	}
	I->next = (struct _avm_event_item *)((unsigned int)-1);
	I->data = NULL;

	LOCAL_LOCK();

	/*--- es sind auf jeden fall genügend elemente in der Queue ---*/
	free_Items.data[free_Items.tx++] = I;
	if (free_Items.tx >= free_Items.size)
		free_Items.tx = 0;
	free_Items.count++;

	LOCAL_UNLOCK();
}
