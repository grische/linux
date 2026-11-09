// SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
/*
 * AVM packet accelerator - control character device
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100:
 *
 * Copyright (c) 2011-2020 AVM GmbH <info@avm.de>
 * All rights reserved.
 */

#define pr_fmt(fmt) "avm_pa_dev: " fmt

#include <linux/atomic.h>
#include <linux/poll.h>
#include <linux/jump_label.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>

#include "avm_pa.h"
#include "avm_pa_intern.h"
#include "include/uapi/avm/avm_pa/avm_pa_ioctl.h"

enum pa_perf_method {
	PERF_METHOD_RECV,
	PERF_METHOD_FORW,
};

struct pa_perf_test {
	struct list_head list;
	int id;
	enum pa_perf_method method;
	ulong start, end;
	/* Dummy skb-under-test */
	struct sk_buff *skb;
	/* actual results */
	ulong call_count;
};


struct pa_perf_data {
	wait_queue_head_t   result_wait;
	struct avm_pa_dev_info pid_in, pid_out;
	spinlock_t lock;
	struct pa_perf_test *recv_test;
	struct tasklet_struct recv_task;
	long recv_count;

	struct list_head tests, completed;
	int test_count;
};

#define REPORT_VERSION 1
static ssize_t pa_perf_read(struct file *file, char __user *buffer,
			    size_t count, loff_t *ppos)
{
	int nb = file->f_flags & O_NONBLOCK;
	struct pa_perf_data *p = file->private_data;
	struct pa_perf_test *test;
	struct avm_pa_ioctl_perf_result out;
	int err;

	if (count == 0)
		return 0;

	err = wait_event_interruptible(p->result_wait,
				       nb || !list_empty(&p->completed));

	if (err != 0)
		return err;
	else if (list_empty(&p->completed))
		return -EAGAIN;

	spin_lock_bh(&p->lock);
	test = list_first_entry(&p->completed, struct pa_perf_test, list);
	list_del(&test->list);
	spin_unlock_bh(&p->lock);

	out.version = REPORT_VERSION;
	out.test_id = test->id;
	out.call_count = test->call_count;

	if (copy_to_user(buffer, &out, min(sizeof(out), count)))
		err = -EFAULT;

	kfree_skb(test->skb);
	kfree(test);

	return err ?: min(sizeof(out), count);
}

static unsigned int pa_perf_poll(struct file *file, poll_table *wait)
{
	struct pa_perf_data *p = file->private_data;

	poll_wait(file, &p->result_wait, wait);

	if (!list_empty(&p->completed))
		return POLLIN;

	return 0;
}

static void recv_task(unsigned long data)
{
	struct pa_perf_data *p = (struct pa_perf_data *) data;
	int budget = 64;
	int err, ret;
	struct pa_perf_test *test;

	test = p->recv_test;
	if (!test) {
		spin_lock(&p->lock);
		p->recv_test = list_first_entry(&p->tests, struct pa_perf_test, list);
		list_del(&p->recv_test->list);
		spin_unlock(&p->lock);

		test = p->recv_test;
		test->start = jiffies;
		test->end = jiffies + HZ;
	}

	test->call_count += budget;
	err = AVM_PA_RX_OK;
	while (budget--) {
		switch (test->method) {
		case PERF_METHOD_RECV:
			ret = avm_pa_dev_pid_receive(&p->pid_in, test->skb);
			break;
		case PERF_METHOD_FORW:
			ret = avm_pa_dev_pid_receive(&p->pid_in, test->skb);
			if (ret == AVM_PA_RX_ACCELERATED)
				ret = AVM_PA_RX_OK;
			else {
				/* avm_pa_dev_pid_receive() must be synchronous,
				 * as we re-use the same skb all over
				 */
				AVM_PKT_INFO(test->skb)->realtime = 1;
				avm_pa_dev_snoop_transmit(&p->pid_out, test->skb);
			}
			break;
		default:
			BUG(); /* not reached */
		}
		avm_pa_reset_skb(test->skb);
		if (ret != AVM_PA_RX_OK)
			err = ret;
	}
	WARN_ON(err != AVM_PA_RX_OK);

	if (time_before(jiffies, test->end)) {
		tasklet_schedule(&p->recv_task);
		return;
	}

	/* test end, next one? */
	p->recv_test = NULL;

	spin_lock(&p->lock);
	list_add_tail(&test->list, &p->completed);
	if (!list_empty(&p->tests))
		tasklet_schedule(&p->recv_task);
	spin_unlock(&p->lock);

	wake_up_interruptible(&p->result_wait);
}

static long pa_perf_ioctl(struct file *file,
			  unsigned int cmd,
			  unsigned long uarg)
{
	struct pa_perf_data *p = file->private_data;
	struct pa_perf_test *test;
	struct avm_pa_ioctl_perf_test th;
	struct avm_pa_ioctl_perf_test __user *uth;

	memset(&th, 0, sizeof(th));
	switch (cmd) {
	case AVM_PA_PERF_START:
		tasklet_schedule(&p->recv_task);
		break;
	case AVM_PA_PERF_RECV:
	case AVM_PA_PERF_FORW:
		uth = (void __user *) uarg;
		if (copy_from_user(&th.h_len, &uth->h_len, sizeof(u32)))
			return -EFAULT;
		if (th.h_len > sizeof(th.h_buf))
			return -EFBIG;
		if (copy_from_user(&th.h_buf, &uth->h_buf, th.h_len))
			return -EFAULT;

		/* Allocate a new test and add it to the queue */
		test = kzalloc(sizeof(*test), GFP_KERNEL);
		if (!test)
			return -ENOMEM;

		test->id = p->test_count++;
		if (cmd == AVM_PA_PERF_RECV)
			test->method = PERF_METHOD_RECV;
		else
			test->method = PERF_METHOD_FORW;

		/* Pre-allocate skb for the test */
		test->skb = alloc_skb(sizeof(th.h_len) + 64 + NET_IP_ALIGN, GFP_KERNEL);
		if (!test->skb)
			return -ENOMEM;
		skb_reserve(test->skb, 64 + NET_IP_ALIGN);
		memcpy(skb_put(test->skb, th.h_len), th.h_buf, th.h_len);

		/* Add it */
		spin_lock_bh(&p->lock);
		list_add_tail(&test->list, &p->tests);
		spin_unlock_bh(&p->lock);

		if (copy_to_user(&uth->test_id, &test->id, sizeof(test->id)))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void pa_perf_xmit(void *arg, struct sk_buff *skb)
{
}

static int register_perf_pid(struct avm_pa_dev_info *info, const char *name, int suffix)
{
	struct avm_pa_pid_cfg cfg = {0};

	cfg.framing = avm_pa_framing_ether;
	snprintf(cfg.name, sizeof(cfg.name), "%s_%d", name, suffix);
	cfg.tx_func = pa_perf_xmit;
	cfg.tx_arg = NULL;

	return avm_pa_dev_pid_register(info, &cfg);
}

static int pa_perf_open(struct inode *inode, struct file *file)
{
	struct pa_perf_data *p;
	int id = atomic_read(&inode->i_count);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	if (register_perf_pid(&p->pid_in, "perf_in", id) != 0) {
		kfree(p);
		return -ENOSPC;
	}
	if (register_perf_pid(&p->pid_out, "perf_out", id) != 0) {
		avm_pa_dev_unregister_sync(&p->pid_in);
		kfree(p);
		return -ENOSPC;
	}

	init_waitqueue_head(&p->result_wait);
	tasklet_init(&p->recv_task, recv_task, (unsigned long) p);
	spin_lock_init(&p->lock);
	INIT_LIST_HEAD(&p->tests);
	INIT_LIST_HEAD(&p->completed);

	file->private_data = p;

	return 0;
}

static int pa_perf_release(struct inode *inode, struct file *file)
{
	struct pa_perf_data *p = file->private_data;
	DECLARE_COMPLETION_ONSTACK(done);
	struct pa_perf_test *test, *tmp;
	int ret;

	tasklet_kill(&p->recv_task);

	if (p->recv_test) {
		list_add_tail(&p->recv_test->list, &p->tests);
		p->recv_test = NULL;
	}
	list_splice_tail(&p->completed, &p->tests);
	list_for_each_entry_safe(test, tmp, &p->tests, list) {
		list_del(&test->list);
		kfree_skb(test->skb);
		kfree(test);
	}

	avm_pa_dev_unregister(&p->pid_in, &done);
	avm_pa_dev_unregister(&p->pid_out, &done);

	ret = wait_for_completion_killable_timeout(&done, HZ * 10);
	if (ret > 0)
		ret = wait_for_completion_killable_timeout(&done, HZ * 10);

	/* In case of timeout, the pids are not gone yet (and likely stay
	 * forever due to a bug elsewhere).
	 */
	if (!ret)
		ret = -EIO;

	return ret > 0 ? 0 : ret;
}

static const struct file_operations pa_perf_fops = {
	.llseek         = no_llseek,
	.read           = pa_perf_read,
	.poll           = pa_perf_poll,
	.unlocked_ioctl = pa_perf_ioctl,
	.open           = pa_perf_open,
	.release        = pa_perf_release,
};

static struct miscdevice pa_perf_dev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     "avm_pa_perf",
	.fops =     &pa_perf_fops
};


int __init pa_dev_init(void)
{
	int err;

	err = misc_register(&pa_perf_dev);
	if (err)
		goto out_err;

	return 0;

out_err:
	return err;
}


void __exit pa_dev_exit(void)
{
	misc_deregister(&pa_perf_dev);
}
