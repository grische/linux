// SPDX-License-Identifier: GPL-2.0+

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <avm/sammel/simple_proc.h>
#include "avm_sammel.h"
#include <avm/event/event.h>
#include "event/remote.h"

LIST_HEAD(source_list);
DEFINE_SEMAPHORE(source_sema);

#define NODE_NAME_LEN 32

struct checkpoint_node {
	struct list_head list;
	uint32_t node_id;
	uint64_t checkpoints;
	char name[NODE_NAME_LEN];
};

struct node_name {
	uint32_t node_id;
	char *name;
};

static struct node_name node_names[] = {
	{ AVM_EVENT_CHKPNT_ID_ARM, "NODE_ARM" },
	{ AVM_EVENT_CHKPNT_ID_ATOM, "NODE_ATOM" },
	{ 0, NULL },
};

static struct checkpoint_node local_node;
static void *remote_event_source_handle;
static void *remote_event_sink_handle;

static void avm_event_remote_notify(void *context,
				    enum _avm_event_id id)
{
}

/**
 */
static void avm_event_checkpoint_remote_sink(void *private,
					     unsigned char *buf,
					     unsigned int len)
{
	struct _avm_event_checkpoint event;
	struct checkpoint_node *node;
	struct node_name *name;

	if (len < sizeof(event)) {
		pr_err("%s: incompatible event len=%u sizeof=%u\n", __func__,
		       len, sizeof(event));
		return;
	}

	memcpy(&event, buf, min(len, sizeof(event)));
	if (event.event_header.id != avm_event_id_checkpoint) {
		pr_err("[%s]: incompatible event (id=%u)\n", __func__,
		       event.event_header.id);
		return;
	}

	/* search list of known nodes for matching ID */
	list_for_each_entry(node, &source_list, list) {
		if (node->node_id == event.node_id) {
			break;
		}
	}

	/* if no node with matching ID is found, allocate new node and append it
	 * to the list.
	 * We do not have to worry about concurrency here because
	 *   a) new nodes will only be created from remote event calls
	 *   b) remote events are delivered by a single kthread
	 */
	if (&node->list == &source_list) {
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (node != NULL) {
			INIT_LIST_HEAD(&node->list);
			node->node_id = event.node_id;

			name = &node_names[0];
			while (name->name != NULL &&
			       name->node_id != event.node_id) {
				++name;
			}

			if (name->name != NULL) {
				strlcpy(node->name, name->name,
					sizeof(node->name));
			} else {
				scnprintf(node->name, sizeof(node->name),
					  "NODE_%u", event.node_id);
			}

			list_add_tail_rcu(&node->list, &source_list);
		}
	}

	/* add new checkpoints to node */
	if (node != NULL) {
		node->checkpoints |= event.checkpoints;
		smp_wmb();
	}
}

static void checkpoint_read(struct seq_file *seq, void *priv)
{
	struct checkpoint_node *node;
	unsigned int i;

	rcu_read_lock();
	list_for_each_entry_rcu (node, &source_list, list) {
		for (i = 0; i < (sizeof(node->checkpoints) * 8); ++i) {
			if (node->checkpoints & (0x1ULL << i)) {
				seq_printf(seq, "%s CHECKPOINT_%02d\n",
					   node->name, i);
			}
		}
	}
	rcu_read_unlock();

	return;
}

/**
 * set new checkpoint via procfs and propagate it to remote nodes
 */
static int checkpoint_write(char *buffer, void *priv)
{
	struct _avm_event_checkpoint *event;
	char parsbuf[256];
	unsigned int chkpnt;
	int result;

	/* copy and parse string written to procfs */
	strlcpy(parsbuf, buffer, sizeof(parsbuf));

	result = sscanf(parsbuf, "CHECKPOINT_%d", &chkpnt);
	if (result != 1 || chkpnt >= (sizeof(event->checkpoints) * 8)) {
		result = -EINVAL;
		goto err_out;
	}

	/* update local node checkpoints */
	result = down_interruptible(&source_sema);
	if (result != 0) {
		goto err_out;
	}

	smp_rmb();
	local_node.checkpoints |= (0x1ULL << chkpnt);
	smp_wmb();

	up(&source_sema);

	/* generate checkpoint event for remote nodes */
	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (event == NULL) {
		pr_err("[%s] unable to alloc event\n", __func__);
		result = -ENOMEM;
		goto err_out;
	}

	event->event_header.id = avm_event_id_checkpoint;
	event->node_id = local_node.node_id;
	event->checkpoints = local_node.checkpoints;
	result = avm_event_remote_source_trigger(remote_event_source_handle,
						 avm_event_id_checkpoint,
						 sizeof(*event), event);
	if (result >= 0) {
		result = 0;
	}

	kfree(event);

err_out:
	return result;
}

/**
 * erst wenn Remote-Event-Node oben, Event-Schnittstellen anmelden
 */
static void checkpoint_node_established(void *private,
					unsigned int param1,
					unsigned int param2)
{
	struct _avm_event_id_mask id_mask;

	remote_event_source_handle = avm_event_source_register(
		"avm_event_checkpoint",
		avm_event_build_id_mask(&id_mask, 1, avm_event_id_checkpoint),
		avm_event_remote_notify, NULL);

	if (remote_event_source_handle == NULL) {
		pr_err("%s not registered\n", __func__);
		goto err_out;
	}

	remote_event_sink_handle = avm_event_sink_register(
		"avm_event_checkpoint_sink",
		avm_event_build_id_mask(&id_mask, 1, avm_event_id_checkpoint),
		avm_event_checkpoint_remote_sink, NULL);
	if (remote_event_sink_handle == NULL) {
		pr_err("%s not registered\n", __func__);
	}

err_out:
	return;
}

void avm_checkpoint_init(void)
{
	int result;

	pr_info("Checkpoint Proc init\n");

	result = 0;

	memset(&local_node, 0x0, sizeof(local_node));

#if defined(CONFIG_MACH_PUMA6) || defined(CONFIG_ARCH_AVALANCHE)
	local_node.node_id = AVM_EVENT_CHKPNT_ID_ARM;
	strlcpy(local_node.name, "NODE_ARM", sizeof(local_node.name));
#else
	local_node.node_id = AVM_EVENT_CHKPNT_ID_ATOM;
	strlcpy(local_node.name, "NODE_ATOM", sizeof(local_node.name));
#endif
	INIT_LIST_HEAD(&local_node.list);
	list_add_tail(&local_node.list, &source_list);

	result = add_simple_proc_file("avm/checkpoints", checkpoint_write,
				      checkpoint_read, NULL);
	if (result != 0) {
		pr_err("[%s] add_simple_proc_file() failed\n", __func__);
		goto err_out;
	}

	avm_event_node_established(checkpoint_node_established, NULL, 0, 0);

err_out:
	return;
}

MODULE_LICENSE("GPL");
