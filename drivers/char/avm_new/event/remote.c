// SPDX-License-Identifier: GPL-2.0+

#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <avm/event/event.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>

#if defined(CONFIG_ARM)
#include <mach/system.h>
#endif

#endif /*--- #if defined(__KERNEL__) ---*/

#include "remote.h"

/*--- #define DBG_INFO(args...)    pr_info("[avm_event_remote]" args) ---*/
#define DBG_INFO(args...) no_printk(args)

/**
 */
int avm_event_remote_source_register(struct _avm_event_source *source_handle,
				     char *name,
				     struct _avm_event_id_mask *id_mask)
{
	DBG_INFO("[%s]: event_source=%p %llx\n", __func__, source_handle,
		 id_mask->mask[0]);
	return node_source_register(source_handle, name, id_mask);
}

/**
 */
int avm_event_remote_source_release(struct _avm_event_source *source_handle,
				    struct _avm_event_id_mask *id_mask)
{
	void *node_handle;
	int ret = -ENODEV;

	DBG_INFO("[%s]: event_source=%p %llx\n", __func__, source_handle,
		 id_mask->mask[0]);
	if (source_handle == NULL) {
		return ret;
	}
	node_handle = node_findhandle_by_source(source_handle);
	if (node_handle == NULL) {
		return ret;
	}
	return node_source_unregister(node_handle, id_mask);
}

/**
 * sende Event zur anderen CPU
 * ret: 1   falls Remote-Senke vorhanden (like avm_event_local_source_trigger)
 *      < 0 bei Fehler
 */
int avm_event_remote_source_trigger(struct _avm_event_source *event_source,
				    enum _avm_event_id id,
				    unsigned int data_length, void *data)
{
	int ret;
	void *node_handle;

	node_handle = node_findhandle_by_source(event_source);
	DBG_INFO("%s: event_source=%p node_handle=%p, id=%d, data_length=%u\n",
		 __func__, event_source, node_handle, id, data_length);
	if (node_handle == NULL) {
		return -ENODEV;
	}
	ret = node_source_send(node_handle, id, data_length, data);
	return (ret == 0) ? 1 : -ENODEV;
}

/**
 * empfange notify-trigger: umsetzen auf dieser CPU
 */
void avm_event_remote_notifier_request(void *node_handle,
				       enum _avm_event_id id)
{
	DBG_INFO("%s: id=%d\n", __func__, id);
	avm_event_source_notify(id);
}

/**
 * wird nur getriggert wenn sich auf dieser CPU eine Source von der anderen CPU anmeldet
 * parallel wird aber schon ein Notifier versendet -> ignoriere
 */
void avm_event_remote_notifier_to_send(void *source_handle,
				       enum _avm_event_id id)
{
	DBG_INFO("%s: event_source=%p id=%u ->ignore\n", __func__,
		 source_handle, id);
}

/**
 * Notifier auf anderen CPU triggern
 */
int avm_event_remote_notifier(struct _avm_event_open_data *open_data,
			      struct _avm_event_cmd_param_trigger *data)
{
	void *node_handle;

	node_handle = node_findhandle_by_source(open_data->event_source_handle);
	DBG_INFO("%s: event_source=%p node_handle=%p, id=%d\n", __func__,
		 open_data->event_source_handle, node_handle, data->id);
	if (node_handle == NULL) {
		return -ENODEV;
	}
	return node_source_notifier(node_handle, data->id);
}

/**
 * erst wenn Remote-Eventschnittstelle running: Callback aufrufen
 */
int avm_event_node_established(
	void (*event_established_cb)(void *private, unsigned int param1,
				     unsigned int param2),
	void *private, unsigned int param1, unsigned int param2)
{
	return node_event_established_callback(event_established_cb, private,
					       param1, param2);
}
EXPORT_SYMBOL(avm_event_node_established);

#if !defined(CONFIG_AVM_CRASHPANIC_LOG_SINK)
static void *event_log_handle;
static struct _avm_event_log *panic_event;

/**
 */
static void avm_event_log_notify(void *context, enum _avm_event_id id)
{
}

/**
 */
static void
_avm_event_log_register_source(void *private,
			       unsigned int param1,
			       unsigned int param2)
{
	struct _avm_event_id_mask id_mask;

	if (event_log_handle) {
		return;
	}
	event_log_handle = avm_event_source_register(
		"event_log",
		avm_event_build_id_mask(&id_mask, 1, avm_event_id_log),
		avm_event_log_notify, NULL);
	if (event_log_handle == NULL) {
		pr_err("%s not registered\n", __func__);
		return;
	}
	panic_event = kzalloc(sizeof(struct _avm_event_log), GFP_ATOMIC);
	pr_err("%s registered\n", __func__);
}

/**
 */
int avm_event_log_register_source(void)
{
	return avm_event_node_established(_avm_event_log_register_source, NULL,
					  0, 0);
}

late_initcall(avm_event_log_register_source);

/**
 */
static unsigned int log_cpy(unsigned char *dst, const unsigned char *buffer,
			    unsigned int len)
{
	unsigned int csum = 0, count = len;

	while (count--) {
		unsigned char val = *buffer++;

		if (val == 0) {
			val = ' ';
		}
		*dst++ = val;
		csum += val;
	}
	return csum ^ (len << 16);
}

/**
 * dient zum versenden von panic (und crashlogs) ueber remote
 * flag:    FLAG_REMOTELOG_REBOOT  reboot triggern
 * verodert:
 * flag: FLAG_REMOTELOG_APPEND:         append-Mode
 * flag: FLAG_REMOTELOG_APPEND_FINISHED append-Mode abschliessen
 */
void avm_event_send_log(enum _avm_logtype type, unsigned int flag,
			const char *buffer, unsigned int len)
{
	static unsigned char logbuffer[1 << CONFIG_LOG_BUF_SHIFT];
	static unsigned int log_idx;
	struct _avm_event_log *event;
	unsigned int offset = 0;

	if (event_log_handle == NULL) {
		pr_warn("%s (remote): no event_handle %p\n", __func__,
			event_log_handle);
		return;
	}
	if ((flag & FLAG_REMOTELOG_APPEND) == FLAG_REMOTELOG_APPEND) {
		if (buffer) {
			unsigned int copy =
				min(len, sizeof(logbuffer) - log_idx);

			memcpy(&logbuffer[log_idx], buffer, copy);
			log_idx += copy;
		}
		if ((flag & FLAG_REMOTELOG_APPEND_FINISHED) !=
		    FLAG_REMOTELOG_APPEND_FINISHED) {
			return;
		}
		buffer = logbuffer;
		len = log_idx;
		log_idx = 0;
	}
	if (type == remote_panic) {
		event = panic_event;
		panic_event = NULL;
	} else {
		event = kzalloc(sizeof(struct _avm_event_log), GFP_ATOMIC);
	}
	if (event == NULL) {
		pr_warn("%s (remote): can't alloc event\n", __func__);
		return;
	}
	if (len > sizeof(logbuffer)) {
		offset = len - sizeof(logbuffer);
		len = sizeof(logbuffer);
	}
	event->event_header.id = avm_event_id_log;
	event->logtype = type;
	event->loglen = len;
	event->logpointer = virt_to_phys(logbuffer);
	event->checksum = log_cpy(logbuffer, &buffer[offset], len);
	event->rebootflag = flag & FLAG_REMOTELOG_REBOOT;
#if defined(CONFIG_X86)
	clflush_cache_range(logbuffer, len);
#else
	consistent_sync(logbuffer, len, DMA_BIDIRECTIONAL);
#endif
	/*--- printk(KERN_INFO"%s: %p %p %x\n", __FUNCTION__, logbuffer, event->logpointer, event->checksum); ---*/
	avm_event_source_trigger(event_log_handle, avm_event_id_log,
				 sizeof(struct _avm_event_log), event);
}
EXPORT_SYMBOL(avm_event_send_log);
#endif /*--- #if !defined(CONFIG_AVM_CRASHPANIC_LOG_SINK) ---*/
