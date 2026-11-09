/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _CAPI_SOCKET_IF_H_
#define _CAPI_SOCKET_IF_H_

/**
 */
void capi_oslib_socket_init(char *remote_device, unsigned short h_proto);
void capi_oslib_socket_release(void);

#endif
