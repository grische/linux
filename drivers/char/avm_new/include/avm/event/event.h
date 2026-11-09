/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _avm_linux_event_h_
#define _avm_linux_event_h_

#include <uapi/avm/event/event.h>

struct seq_file;
extern void dump_event_node_stats(struct seq_file *s);
typedef void (*avm_event_established_cb)(void *context,
				     unsigned int param1,
				     unsigned int param2);

#if defined(CONFIG_AVM_EVENTNODE_PUMA7)

#define ARM_MEM_OFFSET 0x0

int avm_event_node_established(avm_event_established_cb callback,
			       void *context,
			       unsigned int param1,
			       unsigned int param2);
#define FLAG_REMOTELOG_REBOOT 0x1
#define FLAG_REMOTELOG_APPEND 0x2
#define FLAG_REMOTELOG_APPEND_FINISHED (0x4 | FLAG_REMOTELOG_APPEND)
void avm_event_send_log(enum _avm_logtype type, unsigned int flag,
			const char *buffer, unsigned int len);

/**
 * ist in den arm2atom/atom2arm-irq einzuklinken:
 */
unsigned int handle_remotecpuirq(unsigned int cause);

typedef int (*avm_event_tffs_cb)(void *priv,
				 struct avm_event_tffs *tffs_data);
void *avm_event_register_tffs(uint32_t id,
			      avm_event_tffs_cb callback,
			      void *cb_data);
int avm_event_tffs_call(void *priv, struct avm_event_tffs *call);

void avm_event_node_emergency_shutdown(void);

#else /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA7) ---*/
/**
 */
static inline
int avm_event_node_established(avm_event_established_cb callback,
			       void *context,
			       unsigned int param1,
			       unsigned int param2)
{
	if (callback)
		callback(context, param1, param2);
	return 0;
}
#endif
#endif
