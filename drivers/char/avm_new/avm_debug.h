/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_DEBUG_H_
#define _AVM_DEBUG_H_

#include "avm_sammel.h"

/*
 * only for debug: check remotedev-server
 */
/*--- #define REMOTEDEV_SERVER_TEST ---*/
#if defined(REMOTEDEV_SERVER_TEST)
extern int remotedev_servertest(int mode, unsigned short h_proto, char *net);
#endif /*--- #if defined(REMOTEDEV_SERVER_TEST) ---*/

/**
 */
int __init avm_debug_init(void);
void avm_debug_cleanup(void);

#endif /*--- #ifndef _AVM_DEBUG_H_ ---*/
