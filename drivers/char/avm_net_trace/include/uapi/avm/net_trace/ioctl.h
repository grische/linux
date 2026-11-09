#ifndef _AVM_NET_TRACE_IOCTL_H
#define _AVM_NET_TRACE_IOCTL_H

#include <linux/if.h>

#define AVM_NET_TRACE_DYNAMIC_MINOR	-1

#define AVM_NET_TRACE_ID_WLAN_MGMT_TRACE 128 // hard coded for support data script
#define AVM_NET_TRACE_ID_BASE_WLAN       129
#define AVM_NET_TRACE_ID_MAX_WLAN        199
#define AVM_NET_TRACE_ID_BASE_USB        200 // has to be divisible by 10 for better readability
#define AVM_NET_TRACE_ID_MAX_USB         209

#define AVM_NET_TRACE_TYPE_IFACE    1 /* ifconfig ifaces*/
#define AVM_NET_TRACE_TYPE_PVC      2
#define AVM_NET_TRACE_TYPE_DSLIFACE 3 /* internet, voip, ... */
#define AVM_NET_TRACE_TYPE_WLAN     4
#define AVM_NET_TRACE_TYPE_USB      5
#define AVM_NET_TRACE_TYPE_DSLFW    6

#define AVM_NET_TRACE_IFNAMSIZ     32

#define MAX_AVM_NET_TRACE_DEVICES 255 

struct ioctl_ant_device {
	char  name[AVM_NET_TRACE_IFNAMSIZ];
	int   minor;
	int   iface;
	int   type;
	int   is_open;
};

struct ioctl_ant_device_list {
	int buf_len; /* size of buffer */
	union {
		char *u_buf;
		struct ioctl_ant_device *u_dev;
	}u;
#define u_buf u.u_buf /* address of buffer */
#define u_dev u.u_dev /* array of structures */
};

#define ANT_IOCTL_GET_DEVICES      _IOR('C', 0x77, struct ioctl_ant_device_list)
#define ANT_IOCTL_BUFFER_SIZE_SET  _IOW('C', 0x01, int                         )
#define ANT_IOCTL_BUFFER_SIZE_GET  _IOW('C', 0x02, int                         )

#endif /* _AVM_NET_TRACE_IOCTL_H */
