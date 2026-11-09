/**
 * \file    avm_net_trace_test.c
 * \date    07/17/2015
 * \author  AVM/RST
 * \brief   Console test tool for AVM Net Trace interface.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h> 
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include "avm_net_trace_ioctl.h"

/* undef stupid and useless defines */
#undef u_buf
#undef u_dev

static const size_t AVM_NET_TRACE_LEN_MAX  = 30;              /* max. trace device name */
static const char * AVM_NET_TRACE_NAME     = "avm_net_trace"; /* Trace device name without id */
static const size_t DEVICE_LIST_BUFFER_MAX = 2048;            /* Buffer size for device list */

static struct ioctl_ant_device_list _device_list;             /* Globel device list. */


/**************************************************************************//**
 * \brief     Close device
 * \param[inout] device_fd  Pointer to device handle. Will be set to NULL after
 *                          completion.
 */
static void _ant_device_close(int * device_fd)
{
	if (*device_fd >= 0) {
		close (*device_fd);
		*device_fd = -1;
	}
} /* _ant_device_close */


/**************************************************************************//**
 * \brief      Open the given device.
 * \param[in]  device_id  The device id.
 * \param[out] device_fd  File descriptor on success. Otherwise NULL.
 * \return      0: Open success.<br>
 *             -1: Open failed.
 */
static int _ant_device_open(int device_id, int * device_fd)
{
	char device[AVM_NET_TRACE_LEN_MAX];

	_ant_device_close(device_fd);

	(void)snprintf(device, AVM_NET_TRACE_LEN_MAX, "/dev/%s%d", AVM_NET_TRACE_NAME, device_id);

	*device_fd = open(device, O_RDONLY | O_NONBLOCK);
	if (*device_fd < 0) {
		printf ("ERR: Failed to open device '%s': %s\n", device, strerror(errno));
		return -1;
	}

	return 0;
} /* _ant_device_open */


/**************************************************************************//**
 * \brief     Valide device id.
 * \param[in] device_id  Device id to validate.
 * \reutrn    0/1: Device id is not/is valid.
 */
static int _ant_device_id_valid(int device_id)
{
	struct ioctl_ant_device * device          = _device_list.u.u_dev;
	size_t                    device_list_len = _device_list.buf_len;

	while (device_list_len >= sizeof(*device)) {
		if (device_id == device->minor) {
			return 0;
		}
		device++;
		device_list_len -= sizeof(*device);
	}

	return -1;
} /* _ant_device_id_valid */


/**************************************************************************//**
 * \brief     Set buffer size for a permanent device interface.
 * \param[in] device_id    The device id.
 * \param[in] buffer_size  Buffer size in number of packets.
 * \return     0: Set buffer succeeded.<br>
 *            -1: Set buffer failed.
 */
static int _ant_device_perm_buffer_size_set(int device_id, int buffer_size)
{
	int ctrl_dev_fd = -1;
	int ret         = 0;

	ret = _ant_device_open(device_id, &ctrl_dev_fd);
	if (ret < 0) {
		return -1;
	}

	ret = ioctl(ctrl_dev_fd, ANT_IOCTL_BUFFER_SIZE_SET, &buffer_size);
	if (ret < 0) {
		printf("ERR: IOCTL 0x%08x failed: %s\n", ANT_IOCTL_GET_DEVICES, strerror(errno));
	}

	_ant_device_close(&ctrl_dev_fd);

	return ret;
} /* _ant_device_perm_buffer_size_set */


/**************************************************************************//**
 * \brief     Get buffer size for a permanent device interface.
 * \param[in] device_id    The device id.
 * \return     0: Get buffer succeeded.<br>
 *            -1: Get buffer failed.
 */
static int _ant_device_perm_buffer_size_get(int device_id)
{
	int ctrl_dev_fd = -1;
	int ret         = 0;
	int buffer_size = -1;

	ret = _ant_device_open(device_id, &ctrl_dev_fd);
	if (ret < 0) {
		return -1;
	}

	ret = ioctl(ctrl_dev_fd, ANT_IOCTL_BUFFER_SIZE_GET, &buffer_size);
	if (ret < 0) {
		printf("ERR: IOCTL 0x%08x failed: %s\n", ANT_IOCTL_GET_DEVICES, strerror(errno));
	} else {
		printf("Buffer size for trace interface %d is %d\n", device_id, buffer_size);
	}

	_ant_device_close(&ctrl_dev_fd);

	return ret;
} /* _ant_device_perm_buffer_size_get */


/**************************************************************************//**
 * \brief  Update device list informations.
 * \return  0: Update successfully.<br>
 *         -1: Update failed.
 */
static int _ant_device_update(void)
{
	int                       ctrl_dev_fd     = -1;
	int                       ret             = 0;
	struct ioctl_ant_device * device          = NULL;
	size_t                    device_list_len = 0;

	ret = _ant_device_open(0, &ctrl_dev_fd);
	if (ret < 0) {
		return -1;
	}

	_device_list.buf_len = DEVICE_LIST_BUFFER_MAX;
	(void)memset(_device_list.u.u_buf, 0, DEVICE_LIST_BUFFER_MAX);

	ret = ioctl(ctrl_dev_fd, ANT_IOCTL_GET_DEVICES, &_device_list);
	if (ret < 0) {
		printf("ERR: IOCTL 0x%08x failed: %s\n", ANT_IOCTL_GET_DEVICES, strerror(errno));
		goto quit;
	}

quit:
	_ant_device_close(&ctrl_dev_fd);

	return 0;
} /* _ant_device_update */


/**************************************************************************//**
 * \brief  Show a list of net trace devices.
 * \return  0: Dump list succeeded.<br>
 *         -1: Dump list failed.
 */
static int _ant_device_list(void)
{
	int                       ret             = 0;
	struct ioctl_ant_device * device          = NULL;
	size_t                    device_list_len = 0;

	device = _device_list.u.u_dev;
	device_list_len = _device_list.buf_len;
	while (device_list_len >= sizeof(*device)) {
		printf("Device :\n");
		printf("  name    = '%s'\n", device->name   );
		printf("  minor   = %d\n"  , device->minor  );
		printf("  iface   = %d\n"  , device->iface  );
		printf("  type    = %d\n"  , device->type   );
		printf("  is_open = %d\n"  , device->is_open);
		printf("\n");

		device++;
		device_list_len -= sizeof(*device);
	}

	return 0;
} /* _ant_device_list */


/**************************************************************************//**
 * \brief  Do some global initializations.
 * \return  0: Init succeeded.<br>
 *         -1: Init failed.
 */
static int _ant_init(void)
{

	(void)memset(&_device_list, 0, sizeof(_device_list));

	_device_list.buf_len = DEVICE_LIST_BUFFER_MAX;
	_device_list.u.u_buf = (char *)malloc(DEVICE_LIST_BUFFER_MAX);
	if (NULL == _device_list.u.u_buf) {
		printf("ERR: Alloc device list buffer failed.\n");
		return -1;
	}
	(void)memset(_device_list.u.u_buf, 0, DEVICE_LIST_BUFFER_MAX);

	return 0;
} /* _ant_init */


/**************************************************************************//**
 * \brief  Global deinit.
 */
static void _ant_deinit(void)
{
	_device_list.buf_len = DEVICE_LIST_BUFFER_MAX;
	if (NULL != _device_list.u.u_buf) {
		free(_device_list.u.u_buf);
	}
} /* _ant_deinit */


/**************************************************************************//**
 * \brief  Show program usage.
 */
static int _ant_test_usage(char option)
{
	if ('\0' != option) {
		printf("ERR: Invalid option '%c'.\n\n", option);
	}

	printf("usage: avm_net_trace_test [-h] [-l] [-d device-id] [-g] [-b buffer-size]\n"
	       "  -b  Set buffer size for permanent interface. The buffer size is"
	            " defined in number of packets. Needs '-d'.\n"
	       "  -g  Get buffer size for permanent interface. Needs '-d'.\n"
	       "  -d  Set the device id. the device id is the minor number in the"
	            " device list.\n"
	       "  -h  Show this help.\n"
	       "  -l  Dump interface list.\n"
	       "\n");

	return -1;
} /* _ant_test_usage */


/**************************************************************************//**
 * \brief  Entry function.
 */
int main(int argc, char * argv[])
{
	int device_id = -1;
	int ret       = 0;

	if (1 == argc) {
		ret = _ant_test_usage('\0');
		return 0;
	}

	if (_ant_init() < 0) {
		return -1;
	}

	if (_ant_device_update() < 0) {
		return -1;
	}

	while (0 == ret) {
		char option = getopt(argc, argv, "hlgb:d:");
		if (option < 0) {
			break;
		}

		switch (option) {
			case 'h': {
				ret = _ant_test_usage('\0');
				break;
			}
			case 'l': {
				ret = _ant_device_list();
				break;
			}
			case 'b':
			case 'g': {
				if (device_id < 0) {
					printf ("ERR: Device id is not set\n");
					ret = -1;
					break;
				}
				if (NULL == optarg) {
					ret = _ant_device_perm_buffer_size_get(device_id);
				} else {
					int buffer_size = atoi(optarg);
					ret = _ant_device_perm_buffer_size_set(device_id, buffer_size);
				}
				break;
			}
			case 'd': {
				device_id = atoi(optarg);
				ret = _ant_device_id_valid(device_id);
				if (ret < 0) {
					printf ("ERR: Invalid device id '%s'\n", optarg);
					break;
				}
				break;
			}
			default: {
				ret = _ant_test_usage(option);
				break;
			}
		}
	}

quit:
	_ant_deinit();

	return 0;
} /* main */
