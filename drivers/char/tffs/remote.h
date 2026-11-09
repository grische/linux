/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de> */
/*
 * remote.h
 *
 *  Created on: 16 Oct 2014
 *      Author: tklaassen
 */
#ifndef _TFFS_REMOTE_H_
#define _TFFS_REMOTE_H_

enum tffs_conn_state {
	tffs_conn_init,
	tffs_conn_wait_open,
	tffs_conn_intr_open,
	tffs_conn_open,
	tffs_conn_wait_data,
	tffs_conn_wait_close,
	tffs_conn_intr_close,
	tffs_conn_closed,
	tffs_conn_err,
};

struct tffs_remote_message {
	struct list_head msg_list;
	struct avm_event_tffs msg;
};

#endif /* _TFFS_REMOTE_H_ */
