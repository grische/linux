// SPDX-License-Identifier: GPL-2.0+

/*--------------------------------------------------------------------------------*\
gcc test.c avm_event_remote.c avm_event_main.c ./avm_dist_event/avm_event_gen_debug.c avm_event_alloc.c avm_event_node_puma6.c -m32 -o eventsim -DAVM_EVENT_DEBUG  2>&1 | grep "error\|undefined"; ./eventsim
\*--------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "intern.h"
#include <avm/event/event.h>

pid_t getpid(void);
int pipefd[2];
unsigned int count;

#if !defined(CONFIG_AVM_EVENTNODE_PUMA6)
#define msg_poll(a)
#endif /*--- #if !defined(CONFIG_AVM_EVENTNODE_PUMA6) ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int send_msg_sync(struct avm_event_message *msg, struct _event_node_ctrl *pctrl)
{
	unsigned char *p = (unsigned char *)msg;
	msg->magic = (getpid() << 16) | count++;
	write(pipefd[1], msg, sizeof(struct avm_event_message));
	printf("####[%u]send_msg %x: '%s' flags=%x rh=%x th=%x\n", getpid(),
	       msg->magic, get_enum_avm_event_msg_type_name(msg->type),
	       msg->flags, msg->receiver_handle, msg->transmitter_handle);
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int send_msg(struct avm_event_message *msg, struct _event_node_ctrl *pctrl)
{
	return send_msg_sync(msg, pctrl);
}
struct node_data;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int handle_reply(struct avm_event_message *msg, struct node_data *pnode)
{
	if (msg->flags == 0) {
		printf("#####[%u] Message is not a reply!\n", getpid());
		return -EIO;
	}
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct avm_event_message *recv_msg(struct _event_node_ctrl *pctrl)
{
	static struct avm_event_message *msg;
	struct avm_event_message *pmsg;
	unsigned int len;

	if (msg == NULL) {
		msg = calloc(1, sizeof(struct avm_event_message));
	}
	len = read(pipefd[0], msg, sizeof(struct avm_event_message));
	if (len == sizeof(struct avm_event_message)) {
		printf("####[%u]rcv msg %x: '%s' flags=%x rh=%x th=%x pos=%d\n",
		       getpid(), msg->magic,
		       get_enum_avm_event_msg_type_name(msg->type), msg->flags,
		       msg->receiver_handle, msg->transmitter_handle, len);
		pmsg = msg;
		msg = NULL;
		return pmsg;
	}
	return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void prepare_all(char *exec, int child)
{
	int pid, result;
	if (child == 0) {
		result = mkfifo("event_fifo_1", S_IRUSR | S_IWUSR);
		if (result < 0) {
			perror("mkfifo");
		}
		result = mkfifo("event_fifo_2", S_IRUSR | S_IWUSR);
		if (result < 0) {
			perror("mkfifo");
		}
		exec = strdup(exec);
		/*--- spawn process ---*/
		pid = fork();
		if (pid == 0) {
			char *argvlist[3] = { exec, "CHILD", NULL };
			/*--- now we are in the Child-Process  ---*/
			execv(exec, argvlist);
			perror("can't start exec");
			_exit(127);
		}
	}
	pipefd[0] = open(child ? "event_fifo_2" : "event_fifo_1",
			 O_RDONLY | O_NONBLOCK);
	if (pipefd[0] == -1) {
		perror("open");
		exit(2);
	}
	pipefd[1] = open(child ? "event_fifo_1" : "event_fifo_2", O_WRONLY);
	if (pipefd[1] == -1) {
		perror("open");
		exit(2);
	}
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void event_1_notify(void *context, enum _avm_event_id id)
{
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void *duplicate_event(void *p, int len)
{
	void *pp = malloc(len);
	memcpy(pp, p, len);
	return pp;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void event_received(void *context, unsigned char *buf, unsigned int len)
{
	struct _avm_event_telefonprofile *pt =
		(struct _avm_event_telefonprofile *)buf;
	printf("####[%u]%s: %p (len=%d): id=%x on=%x\n", getpid(), __func__, pt,
	       len, pt->event_header.id, pt->on);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	unsigned int state = 0;
	unsigned int end;
	void *sink_handle;
	void *event_handle = NULL;
	struct _avm_event_telefonprofile telefon;

	telefon.event_header.id = avm_event_id_telefon_up;
	telefon.on = 0xabcd;

	avm_event_init2(512 /* max_items */, 512 /* max_datas */);
	prepare_all(argv[0], argc > 1 ? 1 : 0);
	if (argc == 1) {
		printf("####[%u]Source started\n", getpid());
		event_handle = avm_event_source_register(
			"event_1", 1ULL << avm_event_id_telefon_up,
			event_1_notify, NULL);
	} else {
		printf("####[%u]Remote Sink started\n", getpid());
	}
	sink_handle = avm_event_sink_register(
		"sink", 1ULL << avm_event_id_telefon_up, event_received, NULL);
	for (;;) {
		msg_poll(NULL);
		sleep(1);
		switch (state) {
		case 4:
			if (event_handle) {
				avm_event_source_trigger(
					event_handle, avm_event_id_telefon_up,
					sizeof(telefon),
					duplicate_event(&telefon,
							sizeof(telefon)));
			}
			break;
		case 5:
			if (event_handle) {
				telefon.on = 0;
				avm_event_source_trigger(
					event_handle, avm_event_id_telefon_up,
					sizeof(telefon),
					duplicate_event(&telefon,
							sizeof(telefon)));
			}
			break;
		case 7:
			if (event_handle) {
				avm_event_source_release(event_handle);
			}
			break;
		case 10:
			avm_event_sink_release(sink_handle);
			exit(0);
		}
		state++;
	}
	return 0;
}
