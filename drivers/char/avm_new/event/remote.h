/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __avm_event_remote_h__
#define __avm_event_remote_h__

#if defined(CONFIG_X86_PUMA7) && defined(CONFIG_AVM_EVENTNODE_PUMA7)
#define REMOTE_EVENT_PUMA7_X86
#endif

#if defined(CONFIG_X86) && defined(CONFIG_AVM_EVENTNODE_PUMA6)
#define REMOTE_EVENT_PUMA6_X86
#endif

#if defined(CONFIG_ARM) && defined(CONFIG_AVM_EVENTNODE_PUMA7)
#define REMOTE_EVENT_PUMA7_ARM
#endif

#if defined(CONFIG_ARM) && defined(CONFIG_AVM_EVENTNODE_PUMA6)
#define REMOTE_EVENT_PUMA6_ARM
#endif

#include "internal.h"
#if defined(CONFIG_AVM_EVENTNODE_PUMA6) || defined(CONFIG_AVM_EVENTNODE_PUMA7)
/**
 * An/Abmelden  an der andern CPU
 */
int avm_event_remote_source_register(struct _avm_event_source *source_handle,
				     char *name,
				     struct _avm_event_id_mask *id_mask);
int avm_event_remote_source_release(struct _avm_event_source *source_handle,
				    struct _avm_event_id_mask *id_mask);
/**
 * sende Event zur anderen CPU
 */
int avm_event_remote_source_trigger(struct _avm_event_source *event_source,
				    enum _avm_event_id id,
				    unsigned int data_length, void *data);

/**
 * Notifier auf anderen CPU triggern
 */
int avm_event_remote_notifier(struct _avm_event_open_data *open_data,
			      struct _avm_event_cmd_param_trigger *data);

#else /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA6) ---*/

#define avm_event_remote_source_register(source_handle, name, id_mask)       (0)
#define avm_event_remote_source_release(source_handle, id_mask)              (0)
#define avm_event_remote_source_trigger(event_source, id, data_length, data) (0)
#define avm_event_remote_notifier(open_data, data)                           (0)
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_EVENTNODE_PUMA6) ---*/

/**
 * wird von der Remoteschnittstelle aufgerufen in handle_remote_source_register_request()
 */
void *
avm_event_local_source_register(char *name, struct _avm_event_id_mask *id_mask,
				void (*notify)(void *, enum _avm_event_id),
				void *Context);

/**
 * wird von der Remoteschnittstelle aufgerufen installiert mit handle_remote_source_register_request()
 */
void avm_event_remote_notifier_to_send(void *handle, enum _avm_event_id id);

void avm_event_remote_notifier_request(void *handle, enum _avm_event_id id);

/**
 * wird von der Remoteschnittstelle aufgerufen mit handle_remote_source_release_request()
 */
int avm_event_local_source_release(void *handle);

/**
 */
int avm_event_local_source_trigger(void *handle, enum _avm_event_id id,
				   unsigned int data_length, void *data);

typedef int (*avm_event_tffs_cb)(void *priv, struct avm_event_tffs *tffs_data);

struct event_tffs_handle {
	struct _event_node_ctrl *pctrl;
	struct avm_event_message send_msg;
	uint32_t id;
	avm_event_tffs_cb callback;
	void *cb_data;
};

#define MAX_TFFS_HANDLES 4

#endif /*--- #ifndef __avm_event_remote_h__ ---*/
