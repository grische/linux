// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/ratelimit.h>
#include <asm/atomic.h>
#include <linux/workqueue.h>
#endif /*--- #if defined(__KERNEL__) ---*/

#include <avm/event/event.h>
#include "remote.h"
#include <avm/event/event.h>
#include "internal.h"
#include "struct/endian.h"

#include <avm/enh/fw_info.h>

static unsigned int avm_check_event_msg;

#define DBG_ERR(args...)    pr_err("[avm_event_main]" args)
/*--- #define DEBUG_AVM_EVENT_MAIN ---*/
#if defined(DEBUG_AVM_EVENT_MAIN)
#define DBG_WARN(args...)   pr_err("[avm_event_main]" args)
#define DBG_INFO(args...)   pr_info("[avm_event_main]" args)
#define DBG_TRC(args...)    pr_info("[avm_event_main]" args)

/**
 */
static void dump_data(const char *prefix, const unsigned char *data,
		      unsigned int len)
{
	DBG_TRC(KERN_ERR "%s: data %p len=%d:", prefix, data, len);
	while (data && len--) {
		DBG_TRC(KERN_CONT "%02x,", *data++);
	}
	DBG_TRC(KERN_ERR "\n");
}
#else /*--- #if defined(DEBUG_AVM_EVENT_MAIN) ---*/
#define DBG_WARN(args...)	no_printk(args)
#define DBG_INFO(args...)	no_printk(args)
#define DBG_TRC(args...)	no_printk(args)
#define dump_data(a, b, c)
#endif

#define LOCAL_LOCK()   spin_lock_irqsave(&avm_event_lock, flags)
#define LOCAL_UNLOCK() spin_unlock_irqrestore(&avm_event_lock, flags)

struct _dump_event_stat {
	unsigned short count;
	unsigned short max_datalength;
	unsigned short max_linkcount;
};

/**
 */
static volatile struct _avm_event_open_data *first, *last;
static struct _avm_event_source *avm_event_source[MAX_AVM_EVENT_SOURCES];

/**
 * Baut die Event-Mask zusammen
 * num_ids: Anzahl der folgenden num_ids
 */
struct _avm_event_id_mask *
avm_event_build_id_mask(struct _avm_event_id_mask *id_mask, int num_ids, ...)
{
	int i;
	va_list listPointer;

	va_start(listPointer, num_ids);

	DBG_TRC("%s: num_ids=%d\n", __func__, num_ids);
	memset(id_mask, 0, sizeof(struct _avm_event_id_mask));
	if (num_ids <= 0) {
		DBG_ERR("%s: invalid num_ids %d\n", __func__, num_ids);
		return NULL;
	}
	for (i = 0; i < num_ids; i++) {
		enum _avm_event_id id = va_arg(listPointer, enum _avm_event_id);

		DBG_TRC("%s: id=%u\n", __func__, id);
		if ((unsigned int)id >= avm_event_last) {
			/*--- id out of range ---*/
			DBG_ERR("%s: id out of range\n", __func__);
			return NULL;
		}
		id_mask->mask[AVM_EVENT_MASK_IDX(id)] |=
			(avm_event_mask_fieldentry)1 << AVM_EVENT_MASK_BIT(id);
		DBG_TRC("%s: set idx=%u bit=%d (%llx)\n", __func__,
			AVM_EVENT_MASK_IDX(id), AVM_EVENT_MASK_BIT(id),
			id_mask->mask[AVM_EVENT_MASK_IDX(id)]);
	}
	va_end(listPointer);
	return id_mask;
}
EXPORT_SYMBOL(avm_event_build_id_mask);

/**
 */
static int check_id_masks(struct _avm_event_id_mask *id_mask1,
			  struct _avm_event_id_mask *id_mask2)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(id_mask1->mask); i++) {
		if (id_mask1->mask[i] & id_mask2->mask[i]) {
			/*--- DBG_ERR("%s: i=%u (mask=%llx):  (mask=%llx) may cause conflicts\n", __func__, i, id_mask1->mask[i], id_mask2->mask[i]); ---*/
			return i;
		}
	}
	return -1;
}

/**
 */
void avm_event_source_notify(enum _avm_event_id id);

/**
 * duerfen mehr als eine Source anmelden:
 */
static unsigned int id_multi_source_white_list[] = {
	avm_event_id_wlan_client_status,
	avm_event_id_wlan_event,
	avm_event_id_fax_file
};

static int avm_event_source_mask_ignore(struct _avm_event_id_mask *id_mask)
{
	unsigned int i, ignore = 0, id;

	for (id = 0; id < avm_event_last; id++) {

		if (id_mask->mask[AVM_EVENT_MASK_IDX(id)] & ((avm_event_mask_fieldentry)1 << AVM_EVENT_MASK_BIT(id))) {
			ignore = 0;
			for (i = 0; i < ARRAY_SIZE(id_multi_source_white_list); i++) {
				if (id_multi_source_white_list[i] == id) {
					DBG_TRC("%s: id=%u in whitelist\n", __func__, id);
					ignore = 1;
				}
			}
			if (ignore == 0) {
				break;
			}
		}
	}
	return ignore;
}

/**
 * Checke ob diese Eventmask bereits als Source angemeldet
 */
static int avm_event_check_source_mask(const char *prefix, char *name,
				       struct _avm_event_id_mask *id_mask)
{
	unsigned int ret = 0;
	unsigned int i;
	int idx;

	if (avm_event_source_mask_ignore(id_mask)) {
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(avm_event_source); i++) {
		if (avm_event_source[i]) {
			idx = check_id_masks(&avm_event_source[i]->event_mask,
					     id_mask);
			if (idx >= 0) {
				DBG_ERR("%s: warning: idx=%d %s(mask=%llx): double registered source: %s (mask=%llx) may cause conflicts\n",
					prefix, idx, name, id_mask->mask[idx],
					avm_event_source[i]->Name,
					avm_event_source[i]->event_mask.mask[idx]);
				ret++;
			}
		}
	}
	return ret;
}

/**
 */
static struct _avm_event_source *
alloc_local_event_source(struct _avm_event_source *new)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(avm_event_source); i++) {
		if (atomic_test_and_set((unsigned long *)&avm_event_source[i],
					(unsigned long)new)) {
			new->signatur = AVM_EVENT_SIGNATUR;
			break;
		}
	}
	if (i == ARRAY_SIZE(avm_event_source)) {
		return NULL;
	}
	return new;
}

/**
 */
static int free_local_event_source(struct _avm_event_source *pevent)
{
	unsigned int i;

	if (pevent == NULL) {
		DBG_ERR("[%s]: null-pointer\n", __func__);
		return -EFAULT;
	}
	if (pevent->signatur != AVM_EVENT_SIGNATUR) {
		DBG_ERR("[%s]: invalid pointer %p\n", __func__, pevent);
		return -EFAULT;
	}
	for (i = 0; i < ARRAY_SIZE(avm_event_source); i++) {
		if (avm_event_source[i] == pevent) {
			unsigned long flags;

			LOCAL_LOCK();
			avm_event_source[i] = NULL;
			pevent->signatur = 0;
			LOCAL_UNLOCK();
			kfree(pevent);
			return 0;
		}
	}
	DBG_ERR("[%s]: not found %p !\n", __func__, pevent);
	return -EFAULT;
}

/**
 */
static void atomic_add_to_front_of_queue(struct _avm_event_open_data *open_data)
{
	unsigned long flags;

	open_data->next = open_data->prev = NULL;

	LOCAL_LOCK();

	if (last == NULL) { /*--- aller erster Eintrag ---*/
		last = open_data;
	} else {
		first->prev = open_data;
		open_data->next = first;
	}
	first = open_data;

	LOCAL_UNLOCK();
}

/**
 */
static void atomic_rm_from_queue(struct _avm_event_open_data *open_data)
{
	unsigned long flags;

	LOCAL_LOCK();

	if (open_data->prev) {
		open_data->prev->next = open_data->next;
	} else { /*--- (first == open_data) wenn es keinen Vorgaenger gibt, muss dieser Eintrag der erste sein ---*/
		first = open_data->next;
	}
	if (open_data->next) {
		open_data->next->prev = open_data->prev;
	} else { /*--- (last == open_data) wenn es keine Nachfolger, muss dieser Eintrag der letzte sein ---*/
		last = open_data->prev;
	}
	LOCAL_UNLOCK();

	/*--- falls noch gerade noch was eingequeued/per proc zugegriffen wird  ---*/
	while (atomic_read(&open_data->busy)) {
		schedule();
	}
}

#if !defined(CONFIG_NO_PRINTK)
/**
 */
static void dump_event_stat(char *prefix,
			    struct _dump_event_stat *event_stat_tab)
{
	unsigned int i;

	for (i = 0; i < avm_event_last; i++) {
		if (event_stat_tab->count) {
			if (prefix) {
				pr_err("%s", prefix);
				prefix = NULL;
			}

			pr_err("\t\tid: %s(%u): pending entries %u  max-link-counts %u, max-len %u\n",
			       get_enum__avm_event_id_name(i), i,
			       event_stat_tab->count,
			       event_stat_tab->max_linkcount,
			       event_stat_tab->max_datalength);

			event_stat_tab->count = 0;
			event_stat_tab->max_linkcount = 0;
			event_stat_tab->max_datalength = 0;
		}
		event_stat_tab++;
	}
}

/**
 */
static void dump_pending_events(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	unsigned long flags;
	struct _avm_event_open_data *open_data;
	struct _dump_event_stat evstat[avm_event_last];
	char text[256];
	struct _avm_event_item *I;
	unsigned int count = 0, i = 0, events = 0;

	if (__ratelimit(&_rs) == 0) {
		return;
	}
	memset(&evstat, 0, sizeof(evstat));
	LOCAL_LOCK();
	open_data = (struct _avm_event_open_data *)first;
	while (open_data) {
		count++;
		I = (struct _avm_event_item *)open_data->item;

		if (I) {
			snprintf(text, sizeof(text),
				 "user %u \"%s\" pending events:\n", count,
				 open_data->Name ? open_data->Name : "unknown");
			i = 0;
		} else {
			text[0] = 0;
		}
		while (I) {
			struct _avm_event_header *event_header;
			struct _avm_event_data *data;
			enum _avm_event_id id;
			unsigned int link_count;

			i++;
			data = (struct _avm_event_data *)I->data;
			if (data == NULL) {
				pr_err("%s\t\titem %u: ERROR no DATA\n", text,
				       i);
				break;
			}
			event_header = (struct _avm_event_header *)(data->data);
			if (event_header == NULL) {
				pr_err("%s\t\titem: %u ERROR no DATAPOINTER: len %u\n",
				       text, i, data->data_length);
				break;
			}
			id = event_header->id;
			if (id >= avm_event_last) {
				pr_err("%s\t\titem %u ERROR invalid id: %u\n",
				       text, i, id);
				break;
			}

			events++;
			evstat[id].count++;

			if (evstat[id].max_datalength < data->data_length) {
				evstat[id].max_datalength = data->data_length;
			}
			link_count = atomic_read(&data->link_count);
			if (evstat[id].max_linkcount < link_count) {
				evstat[id].max_linkcount = link_count;
			}

			I = (struct _avm_event_item *)I->next;
		}
		LOCAL_UNLOCK();
		dump_event_stat(text, evstat);
		LOCAL_LOCK();
		open_data = (struct _avm_event_open_data *)open_data->next;
	}
	LOCAL_UNLOCK();
	pr_err("Summary: %u user and %u pending events\n", count, events);
}
#else
#define dump_pending_events()
#endif /*--- #if !defined(CONFIG_NO_PRINTK) ---*/
/**
 */
int avm_event_register(struct _avm_event_open_data *open_data,
		       struct _avm_event_cmd_param_register *data)
{
	enum _avm_event_id id;

	DBG_INFO("[%s]: Name=%s Mask=%llx\n", __func__, data->Name,
		 data->mask.mask[0]);
	atomic_add_to_front_of_queue(open_data);
	DBG_INFO("[%s]: open_data=0x%p first=0x%p last=0x%p\n", __func__,
		 open_data, first, last);
	strcpy(open_data->Name, data->Name);
	memcpy(&open_data->event_mask_registered, &data->mask,
	       sizeof(data->mask));
	for (id = 0; id < avm_event_last; id++) {
		if (check_id_mask_with_id(&open_data->event_mask_registered,
					  id)) {
			avm_event_source_notify(id);
		}
	}
	DBG_INFO("[%s]: success\n", __func__);
	return 0;
}

/**
 * workerthread
 */
static void do_sink_event(struct work_struct *work)
{
	struct _avm_event_open_data *open_data =
		container_of(work, struct _avm_event_open_data, wq_sink);

	/*--- Senke Kernelkontext  ---*/
	unsigned int event_pos = 0;
	unsigned int rx_buffer_length = 0;
	unsigned char *rx_buffer;

	for (;;) {
		avm_event_get(open_data, &rx_buffer, &rx_buffer_length,
			      &event_pos);
		if (rx_buffer_length == 0) {
			break;
		}
		open_data->event_received(open_data->context, rx_buffer,
					  rx_buffer_length);
		avm_event_commit(open_data, event_pos);
	}
}

/**
 * Event-Senke aus Kernelmode anmelden
 */
void *avm_event_sink_register(char *name, struct _avm_event_id_mask *id_mask,
			      void (*event_received)(void *context,
						     unsigned char *buf,
						     unsigned int len),
			      void *context)
{
	struct _avm_event_open_data *open_data;
	struct _avm_event_cmd_param_register data;

	DBG_INFO("[%s]:\n", __func__);

	if (id_mask == NULL) {
		DBG_ERR("[%s] no id_mask\n", __func__);
		return NULL;
	}
	if (event_received == NULL) {
		DBG_ERR("%s: invalid param\n", __func__);
		return NULL;
	}
	open_data = (struct _avm_event_open_data *)kzalloc(
		sizeof(struct _avm_event_open_data), GFP_KERNEL);
	if (!open_data) {
		DBG_ERR("%s: malloc failed\n", __func__);
		return NULL;
	}
	memset(&data, 0, sizeof(data));

	open_data->pwq_sink_handle =
		alloc_workqueue("eventsink_wq", WQ_MEM_RECLAIM, 1);
	if (open_data->pwq_sink_handle == NULL) {
		DBG_ERR("%s: create_workqueue failed\n", __func__);
		kfree(open_data);
		return NULL;
	}
	INIT_WORK(&open_data->wq_sink, do_sink_event);
	/*--- damit wird beim Trigger statt wake_up die Workqueue aufgerufen ---*/
	open_data->event_received = event_received;
	open_data->context = context;
	strncpy(data.Name, name, sizeof(data.Name) - 1);
	memcpy(&data.mask, id_mask, sizeof(data.mask));
	avm_event_register(open_data, &data);
	return open_data;
}
EXPORT_SYMBOL(avm_event_sink_register);

/**
 */
void avm_event_sink_release(void *handle)
{
	struct _avm_event_open_data *open_data =
		(struct _avm_event_open_data *)handle;
	struct workqueue_struct *pwq_sh;

	if (open_data == NULL) {
		DBG_ERR("%s: invalid handle\n", __func__);
		return;
	}

	avm_event_release(open_data, NULL);

	pwq_sh = (struct workqueue_struct *)open_data->pwq_sink_handle;

	if (pwq_sh) {
		destroy_workqueue(pwq_sh);
	}
	kfree(open_data);
}
EXPORT_SYMBOL(avm_event_sink_release);

/**
 */
int avm_event_release(struct _avm_event_open_data *open_data,
		      struct _avm_event_cmd_param_release *data)
{
	enum _avm_event_id id;

	DBG_INFO("[%s]: first=0x%p last=0x%p\n", __func__, first, last);

	atomic_rm_from_queue(open_data);
	for (id = 0; id < avm_event_last; id++) {
		if (check_id_mask_with_id(&open_data->event_mask_registered,
					  id)) {
			while (open_data->item) {
				avm_event_commit(open_data, id);
			}
		}
	}
	DBG_INFO("[%s]: (first=%p last=%p) success\n", __func__, first, last);
	return 0;
}

/**
 * notifier triggern
 */
int avm_event_local_notifier(struct _avm_event_open_data *open_data,
			     struct _avm_event_cmd_param_trigger *data)
{
	DBG_INFO("[%s]: Id %u\n", __func__, data->id);

	if (check_id_mask_with_id(&open_data->event_mask_registered,
				  data->id)) {
		avm_event_source_notify((enum _avm_event_id)data->id);
	} else {
		DBG_WARN("[%s]: Id %u event mask not registered\n", __func__,
			 data->id);
	}
	return 0;
}

/**
 * Den ersten Eintrag fuer den man benachrichtigt wurde holen
 */
int avm_event_get(struct _avm_event_open_data *open_data,
		  unsigned char **rx_buffer, unsigned int *rx_buffer_length,
		  unsigned int *event_pos)
{
	if (open_data->item) {
		*rx_buffer = open_data->item->data->data;
		*rx_buffer_length = open_data->item->data->data_length;
		DBG_INFO("[%s]: success\n", __func__);

		return 0;
	}
	*rx_buffer_length = 0;
	DBG_INFO("[%s]: empty\n", __func__);
	return 0;
}

/**
 * Ruecksetzen der notified Maske, herunterzaehlen des link_count und ggf. freigaben
 * der event_data
 */
void avm_event_commit(struct _avm_event_open_data *open_data,
		      unsigned int event_pos)
{
	unsigned long flags;
	struct _avm_event_item *I = (struct _avm_event_item *)open_data->item;

	DBG_INFO("[%s]: Name=%s\n", __func__, open_data->Name);
	if (I) { /*--- wenn ein event item vorhanden ---*/
		/**
		 * geschuetztes manipulieren der Liste (erstes Element endfernen)
		 */
		spin_lock_irqsave(&avm_event_lock, flags);
		open_data->item = I->next; /*--- das naechste Element ---*/
		spin_unlock_irqrestore(&avm_event_lock, flags);
		/*--- avm_event_free_data(I->data); ---*/
		avm_event_free_item(I);
	}
	/**
	 */
	{
		int count = 0;
		unsigned char iii[64];

		memset(iii, 0, sizeof(iii));
		I = (struct _avm_event_item *)open_data->item;
		while (I) {
			if (I->data) {
				struct _avm_event_header *event_header;

				event_header = (struct _avm_event_header
							*)(I->data->data);
				iii[event_header->id]++;
			}
			I = (struct _avm_event_item *)I->next;
			count++;
		}
		DBG_INFO("[%s]: Name=%s\n", __func__, open_data->Name);
		for (count = 0; count < 64; count++) {
			if (iii[count]) { /*--- BUG FIX, Events ---*/
#if 0
				if (iii[count] > 20) {
					struct _avm_event_item *II = (struct _avm_event_item *)open_data->item;
					int i;

					for (i = 0; i < 15; i++) {
						spin_lock_irqsave(&avm_event_lock, flags);
						open_data->item = II->next; /*--- das naechste Element ---*/
						spin_unlock_irqrestore(&avm_event_lock, flags);
						avm_event_free_item(II);
					}
					pr_err("WARNING: id: %d: %d events pending, 15 event dropped\n",
					       count, iii[count]);
					iii[count] -= 15;
				} else {
#endif
				DBG_INFO("\tid: %d: %d events\n", count,
					 iii[count]);
#if 0
				}
#endif
			}
		}
	}
	open_data->receive_count++;
}

/**
 * wird NUR im Kontext der sich registrierenden Applikation aufgerufen, somit ist die
 * 'avm_event_sema' gesetzt
 */
void avm_event_source_notify(enum _avm_event_id id)
{
	int i;

	for (i = 0; i < MAX_AVM_EVENT_SOURCES; i++) {
		if (avm_event_source[i] == NULL)
			continue;
		if (avm_event_source[i]->notify &&
		    check_id_mask_with_id(&avm_event_source[i]->event_mask,
					  id)) {
			(*avm_event_source[i]->notify)(
				avm_event_source[i]->Context, id);
		}
	}
}

/**
 * nur lokal registrieren
 */
void *
avm_event_local_source_register(char *name, struct _avm_event_id_mask *id_mask,
				void (*notify)(void *, enum _avm_event_id),
				void *Context)
{
	struct _avm_event_source *S;

	DBG_INFO("[%s]: name=%s mask[0]=%llx\n", __func__, name,
		 id_mask->mask[0]);

	if (strlen(name) > MAX_EVENT_SOURCE_NAME_LEN) {
		DBG_ERR("[%s]: Event name '%s' is too long!\n", __func__, name);
		return NULL;
	}
	if (avm_event_check_source_mask(__func__, name, id_mask)) {
		return NULL;
	}
	S = kzalloc(sizeof(struct _avm_event_source), GFP_KERNEL);
	if (S == NULL) {
		DBG_ERR("[%s]: out of memory\n", __func__);
		return NULL;
	}
	strlcpy(S->Name, name, sizeof(S->Name));
	S->notify = notify;
	S->Context = Context;
	memcpy(&S->event_mask, id_mask, sizeof(S->event_mask));
	S->send_count = 0;

	if (alloc_local_event_source(S) == NULL) {
		kfree(S);
		DBG_ERR("[%s]: out of resources\n", __func__);
		return NULL;
	}
	DBG_INFO("[%s]: handle=0x%X success\n", __func__, (unsigned int)S);
	return (void *)S;
}

/**
 */
int avm_event_local_source_release(void *handle)
{
	DBG_INFO("[%s]: handle=%p\n", __func__, handle);
	return free_local_event_source((struct _avm_event_source *)handle);
}

/**
 */
int avm_event_trigger(struct _avm_event_open_data *open_data,
		      struct _avm_event_cmd_param_trigger *data)
{
	int status = avm_event_remote_notifier(open_data, data);

	if (status) {
		DBG_INFO(
			"[%s]: handle=%p avm_event_remote_notifier failed %d\n",
			__func__, open_data, status);
	}
	return avm_event_local_notifier(open_data, data);
}

/**
 * lokal und remote registrieren
 */
void *avm_event_source_register(char *name, struct _avm_event_id_mask *id_mask,
				void (*notify)(void *, enum _avm_event_id),
				void *Context)
{
	int result;
	void *handle;

	if (id_mask == NULL) {
		DBG_ERR("[%s] no id_mask\n", __func__);
		return NULL;
	}
	handle = avm_event_local_source_register(name, id_mask, notify, Context);
	if (handle == NULL) {
		return handle;
	}
	result = avm_event_remote_source_register(handle, name, id_mask);
	if (result != 0) {
		DBG_ERR("[%s] avm_event_remote_source_register(%s) failed: %d\n",
			__func__, name, result);
		avm_event_local_source_release(handle);
		return NULL;
	}
	DBG_INFO("[%s] event_source=%p success\n", __func__, handle);
	return handle;
}
EXPORT_SYMBOL(avm_event_source_register);

/**
 * local und remote
 */
void avm_event_source_release(void *handle)
{
	struct _avm_event_source *S = (struct _avm_event_source *)handle;

	DBG_INFO("[%s]: handle=%p\n", __func__, handle);
	if (S == NULL) {
		DBG_ERR("[%s] %p failed.\n", __func__, S);
		return;
	}
	if (avm_event_remote_source_release(S, &S->event_mask)) {
		DBG_ERR("[%s] %p avm_event_remote_source_release() failed.\n",
			__func__, handle);
	}
	if (avm_event_local_source_release(handle)) {
		DBG_ERR("[%s] %p avm_event_local_source_release() failed.\n",
			__func__, handle);
		return;
	}
}
EXPORT_SYMBOL(avm_event_source_release);

/**
 */
int avm_event_source_check_id(void *handle, enum _avm_event_id id)
{
	struct _avm_event_open_data *open_data;
	unsigned long flags;
	int ret = 0;

	DBG_INFO("[%s]: handle=0x%x id=%u\n", __func__, (unsigned int)handle,
		 id);
	if (handle == NULL) {
		DBG_ERR("[%s]: not registered\n", __func__);
		return 0;
	}
	LOCAL_LOCK();
	open_data = (struct _avm_event_open_data *)first;
	while (open_data) { /*--- alle geoeffneten handles durchgehen ---*/
		/**
		 * jeder der auf diese id ein bit in seiner Maske gesetzt hat, wird mit Informationen
		 * versorgt und anschliessend geweckt
		 */
		if (check_id_mask_with_id(&open_data->event_mask_registered,
					  id)) {
			ret = 1;
			break;
		}
		open_data = (struct _avm_event_open_data *)open_data->next;
	}
	LOCAL_UNLOCK();
	return ret;
}

/**
 * nur 'M'-Versionen
 */
static int check_event_msg(enum _avm_event_id id, unsigned char *data,
			   unsigned int data_length)
{
	union avm_event_message_union message;
	int length, cr = 0;

	length = convert_fromMachine_toMachine(
		sizeof(struct avm_event_data),
		convert_message_struct_avm_event_data, ((unsigned char *)data),
		(unsigned char *)&message, 0);

	if (length > 0) {
		/*--- DBG_ERR("EventMsg id=%u(%s) ok (data_length=%u length=%u)\n", id, get_enum__avm_event_id_name(id), data_length, length); ---*/
		return 0;
	}
	DBG_ERR("error: Event id=0x%x(%s) invalid struct - Msg-Len %u:\n", id,
		get_enum__avm_event_id_name(id), data_length);
	while (data_length--) {
		if (cr++ == 16) {
			pr_cont("\n");
			cr = 0;
		}
		pr_cont("%02x ", *data++);
	}
	pr_cont("\n");
	return -1;
}

/**
 * wird von beiden Kontexten aufgerufen
 */
int avm_event_local_source_trigger(void *handle, enum _avm_event_id id,
				   unsigned int data_length, void *data)
{
	struct _avm_event_data *D;
	struct _avm_event_open_data *open_data;
	struct _avm_event_header *H;
	struct _avm_event_source *S = (struct _avm_event_source *)handle;
	int status;
	unsigned long flags;

	DBG_TRC("[%s]: handle=0x%x id=%u data=%p len=%u (%s)\n", __func__,
		(unsigned int)handle, id, data, data_length, current->comm);

	if (unlikely(avm_check_event_msg)) {
		if (check_event_msg(id, data, data_length)) {
			dump_stack();
			return -1;
		}
	}
	LOCAL_LOCK();
	if ((S == NULL) || (S->signatur != AVM_EVENT_SIGNATUR)) {
		LOCAL_UNLOCK();
		DBG_ERR("[%s]: invalid handle %p\n", __func__, S);
		dump_stack();
		kfree(data);
		return 0;
	}
	S->send_count++;
	LOCAL_UNLOCK();
	H = (struct _avm_event_header *)data;
	if (!H || (H->id != id)) {
		DBG_ERR("[%s]: avm_event_header incorrect id=%s(%d) data_length=%u!\n",
			__func__, get_enum__avm_event_id_name(id), id,
			data_length);
		dump_stack();
		dump_data(__func__, data, data_length);
		kfree(data);
		return 0;
	}

	D = avm_event_alloc_data();
	if (D == NULL) {
		printk_ratelimited(
			"[%s]: out of memory (data descriptors) context=%s\n",
			__func__, current->comm);
		dump_pending_events();

		kfree(data);
		return 0;
	}
	atomic_set(&D->link_count, 1);
	D->data = data;
	D->data_length = data_length;

	status = 0;
	LOCAL_LOCK();
	open_data = (struct _avm_event_open_data *)first;
	while (open_data) { /*--- alle geoeffneten handles durchgehen ---*/
		atomic_inc(&open_data->busy);
		LOCAL_UNLOCK();
		/**
		 * jeder der auf diese id ein bit in seiner Maske gesetzt hat, wird mit Informationen
		 * versorgt und anschliessend geweckt
		 */
		if (check_id_mask_with_id(&open_data->event_mask_registered,
					  id)) {
			if (avm_event_source_trigger_one(open_data, D)) {
				/* kein speicher mehr */
				LOCAL_LOCK();
				atomic_dec(&open_data->busy);
				break;
			}
			status++;
		}
		LOCAL_LOCK();
		atomic_dec(&open_data->busy);
		open_data = (struct _avm_event_open_data *)open_data->next;
	}
	LOCAL_UNLOCK();
	avm_event_free_data(D);
	DBG_TRC("[%s]: success (%u instances notified)\n", __func__,
		D ? atomic_read(&D->link_count) : 0);
	return status; /*--- anzahl der benachrichtigten empfaenger ---*/
}

/**
 */
int avm_event_source_trigger(void *handle, enum _avm_event_id id,
			     unsigned int data_length, void *data)
{
	int result;

	result = avm_event_remote_source_trigger(
		handle, id, data_length,
		data); /*--- schicke Event zur anderen CPU ---*/
	if (result < 0) {
		DBG_ERR("[%s] propagating event to remote nodes failed! (id=%u)\n",
			__func__, id);
		result = 0;
	}
	return avm_event_local_source_trigger(handle, id, data_length, data) +
	       result;
}
EXPORT_SYMBOL(avm_event_source_trigger);

/**
 * wird von beiden Kontexten aufgerufen
 */
int avm_event_source_trigger_one(struct _avm_event_open_data *O,
				 struct _avm_event_data *D)
{
	unsigned long flags;
	struct _avm_event_item *I;

	I = avm_event_alloc_item();
	if (I == NULL) {
		DBG_ERR("[%s]: out of memory (items) context=%s\n", __func__,
			current->comm);
		dump_pending_events();

		return 1;
	}

	I->data = D;
	I->next = NULL;
	atomic_inc(&D->link_count);

	/**
	 * geschuetztes verketten (einhaengen)
	 */
	LOCAL_LOCK();
	if (O->item == NULL) { /*--- erster ---*/
		O->item = I;
	} else { /*--- ende finden und anhaengen ---*/
		struct _avm_event_item *i = (struct _avm_event_item *)O->item;

		while (i->next) /*--- bis zum Ende die schlange abarbeiten ---*/
			i = (struct _avm_event_item *)i->next;
		i->next = I;
	}
	LOCAL_UNLOCK();
	/*--- dump_pending_events(); ---*/
	/**
	 */
	O->queue_count++;
	if (O->event_received) {
		/*--- sink from kernel-context: start workqueue ---*/
		if (O->pwq_sink_handle) {
			queue_work(
				(struct workqueue_struct *)O->pwq_sink_handle,
				&O->wq_sink);
		}
		return 0;
	}
	DBG_INFO("[%s]: wake up '%s'\n", __func__, O->Name);
	wake_up(&(O->wait_queue));
	return 0;
}
EXPORT_SYMBOL(avm_event_source_check_id);

/**
 */

#if defined(CONFIG_PROC_FS)
/**
 */
static void avm_event_print_function_mask_names(struct seq_file *seq,
						struct _avm_event_id_mask *mask)
{
	enum _avm_event_id id;
	char *p;

	for (id = 0; id < avm_event_last; id++) {
		if (check_id_mask_with_id(mask, id) == 0)
			continue;

		p = get_enum__avm_event_id_name(id);
		if (p) {
			if (strstr(p, "_avm_event_id_name_unknown")) {
				p = NULL;
			} else {
				seq_printf(seq, "%s ", p);
			}
		}
		if (p == NULL) {
			seq_printf(seq, "undef-%d ", id);
		}
	}
}

/**
 */
static int source_show(struct seq_file *seq, void *data)
{
	unsigned int count;
	struct _avm_event_source *S;
	unsigned long flags;

	seq_puts(seq, "[avm_event] list Event Source\n");
	LOCAL_LOCK();
	for (count = 0; count < ARRAY_SIZE(avm_event_source); count++) {
		S = avm_event_source[count];

		if (S == NULL)
			continue;

		seq_printf(seq,
			   "Source: %-*.s sent:%5d notify: %-60pF: Function: ",
			   MAX_EVENT_SOURCE_NAME_LEN, S->Name, S->send_count,
			   S->notify);

		avm_event_print_function_mask_names(seq, &S->event_mask);
		seq_puts(seq, "\n");
	}
	LOCAL_UNLOCK();
	seq_puts(seq, "--------------------------\n");

	return 0;
}

/**
 */
static int source_open(struct inode *inode, struct file *file)
{
	return single_open(file, source_show, NULL);
}

/**
 */
static int sink_show(struct seq_file *seq, void *data)
{
	struct _avm_event_open_data *open_data;
	unsigned long flags;

	seq_puts(seq, "[avm_event] list Event Sink\n");
	LOCAL_LOCK();
	open_data = (struct _avm_event_open_data *)first;
	while (open_data) {
		atomic_inc(&open_data->busy);
		LOCAL_UNLOCK();
		seq_printf(seq, "Sink: %-*.s: receive:%5d Functions: ",
			   MAX_EVENT_SOURCE_NAME_LEN, open_data->Name,
			   open_data->receive_count);
		avm_event_print_function_mask_names(
			seq, &open_data->event_mask_registered);
		seq_puts(seq, "\n");

		if (open_data->receive_count != open_data->queue_count) {
			seq_printf(seq, "\t\t%5d event still pending.\n",
				   open_data->queue_count -
					   open_data->receive_count);
		}
		LOCAL_LOCK();
		atomic_dec(&open_data->busy);
		open_data = (struct _avm_event_open_data *)open_data->next;
	}
	LOCAL_UNLOCK();
	seq_puts(seq, "--------------------------\n");

	return 0;
}

/**
 */
static int sink_open(struct inode *inode, struct file *file)
{
	return single_open(file, sink_show, NULL);
}

/**
 */
void show_event_stat(struct seq_file *seq,
		     struct _dump_event_stat *event_stat_tab)
{
	unsigned int i;

	for (i = 0; i < avm_event_last; i++) {
		if (event_stat_tab->count) {
			seq_printf(
				seq,
				"\t\tid: %s(%u): pending entries %u  max-link-counts %u, max-len %u\n",
				get_enum__avm_event_id_name(i), i,
				event_stat_tab->count,
				event_stat_tab->max_linkcount,
				event_stat_tab->max_datalength);

			event_stat_tab->count = 0;
			event_stat_tab->max_linkcount = 0;
			event_stat_tab->max_datalength = 0;
		}

		event_stat_tab++;
	}
}

/**
 */
int events_show(struct seq_file *seq, void *data)
{
	struct _avm_event_open_data *open_data;
	struct _dump_event_stat evstat[avm_event_last];
	struct _avm_event_item *I;

	/*--- struct _avm_event_data *D; ---*/
	unsigned int count = 0, i = 0, events = 0;
	unsigned long flags;

	memset(&evstat, 0, sizeof(evstat));
	LOCAL_LOCK();
	open_data = (struct _avm_event_open_data *)first;
	while (open_data) {
		atomic_inc(&open_data->busy);
		LOCAL_UNLOCK();
		count++;
		I = (struct _avm_event_item *)open_data->item;
		if (I) {
			i = 0;
			seq_printf(
				seq, "user %u \"%s\" pending events:\n", count,
				open_data->Name ? open_data->Name : "unknown");
		}
		while (I) {
			struct _avm_event_header *event_header;
			unsigned int link_count;

			i++;

			if (I->data == NULL) {
				seq_printf(seq, "\t\titem %u: ERROR no DATA\n",
					   i);
				break;
			}

			if (I->data->data == NULL) {
				seq_printf(
					seq,
					"\t\titem: %u ERROR no DATAPOINTER: len %u\n",
					i, I->data->data_length);
				break;
			}

			event_header =
				(struct _avm_event_header *)(I->data->data);
			if (event_header->id >= avm_event_last) {
				seq_printf(seq,
					   "\t\titem %u ERROR invalid id: %u\n",
					   i, event_header->id);
				break;
			}

			events++;
			evstat[event_header->id].count++;

			if (evstat[event_header->id].max_datalength <
			    I->data->data_length) {
				evstat[event_header->id].max_datalength =
					I->data->data_length;
			}
			link_count = atomic_read(&I->data->link_count);
			if (evstat[event_header->id].max_linkcount <
			    link_count) {
				evstat[event_header->id].max_linkcount =
					link_count;
			}

			I = (struct _avm_event_item *)I->next;
		}

		show_event_stat(seq, evstat);
		LOCAL_LOCK();
		atomic_dec(&open_data->busy);
		open_data = (struct _avm_event_open_data *)open_data->next;
	}
	LOCAL_UNLOCK();

	seq_printf(seq, "Summary: %u user and %u pending events\n", count,
		   events);

	return 0;
}

/**
 */
static int events_open(struct inode *inode, struct file *file)
{
	return single_open(file, events_show, NULL);
}

void dump_event_node_stats(struct seq_file *s)
{
	const struct event_node_stats *stats;
	char *prefix = s ? "\t" : KERN_EMERG;
	const char *msg = "%sRemote Queue: Len: %d  Min Free: %d    Local Queue:  Len: %d  Min Free: %d  Dropped: %d\n";

	stats = event_node_get_stats();

	if (!stats) {
		/**
		 * During BUG() or panic() output, it makes no sense to
		 * show that there is nothing to show, so print only if s
		 * is non-null.
		 */
		if (s)
			seq_puts(s, "\tN/A\n");
		return;
	}

	if (s)
		seq_printf(s, msg,
			   prefix, stats->recv_len, stats->recv_min_free,
			   stats->send_len, stats->send_min_free,
			   stats->dropped);
	else
		/* pr_*() requires plain strings as format argument */
		printk(msg,
		       prefix, stats->recv_len, stats->recv_min_free,
		       stats->send_len, stats->send_min_free, stats->dropped);
}
EXPORT_SYMBOL(dump_event_node_stats);

/**
 */
static int stats_show(struct seq_file *seq, void *data)
{
	seq_puts(seq, "Event Node Stats\n");
	dump_event_node_stats(seq);

	return 0;
}

/**
 */
static int stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, stats_show, NULL);
}

static const struct file_operations stats_fops = {
	.open = stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 */
static const struct file_operations source_fops = {
	.open = source_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations sink_fops = {
	.open = sink_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations events_fops = {
	.open = events_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 */
static struct proc_dir_entry *event_dir;
void avm_event_proc_init(void)
{
	if (IS_ENABLED(CONFIG_MIPS) && avm_fw_is_internal()) {
		avm_check_event_msg = 1; /*--- nur bei der Entwicklerversion checken ---*/
	}

#if defined(DEBUG_AVM_EVENT_MAIN)
	avm_check_event_msg = 1;
#endif /*--- #if defined(DEBUG_AVM_EVENT_MAIN) ---*/
	event_dir = proc_mkdir("avm/event", NULL);
	proc_create("source", 0, event_dir, &source_fops);
	proc_create("sink", 0, event_dir, &sink_fops);
	proc_create("events", 0, event_dir, &events_fops);
	proc_create("node_stats", 0, event_dir, &stats_fops);
}

/**
 */
void avm_event_proc_exit(void)
{
	remove_proc_entry("source", event_dir);
	remove_proc_entry("sink", event_dir);
	remove_proc_entry("events", event_dir);
	remove_proc_entry("node_stats", event_dir);
	remove_proc_entry("event", NULL);
}

#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
