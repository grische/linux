/*------------------------------------------------------------------------------------------*\
 *
 *   Copyright (C) 2011 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#if defined (CONFIG_AVM_NET_TRACE)
#include <linux/version.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/skbuff.h>
#include <linux/usb.h>

#include <linux/usb/hcd.h>

#include <avm/net_trace/net_trace.h>
#include <avm/net_trace/ioctl.h>

#if defined (AVM_USB_TRACE)
/*--------------------------------------------------------*\
\*--------------------------------------------------------*/
#define SETUP_PACKET_LEN 8
#define MAX_DATA_TRACE_LEN (2048 - 128) // More than a TCP packet, less than 2 K

#define PCAP_ENCAP_USB_189 189
#define PCAP_ENCAP_USB_220 220

/*--------------------------------------------------------*\
\*--------------------------------------------------------*/
struct pcap_usb_hdr_189 {
	u64 id;			/* URB ID - from submission to callback */
	unsigned char type;	/* Same as in text API; extensible. */
	unsigned char xfer_type;	/* ISO, Intr, Control, Bulk */
	unsigned char epnum;	/* Endpoint number and transfer direction */
	unsigned char devnum;	/* Device address */
	unsigned short busnum;	/* Bus number */
	char flag_setup;
	char flag_data;
	s64 ts_sec;		/* gettimeofday */
	s32 ts_usec;		/* gettimeofday */
	int status;
	unsigned int len_urb;	/* Length of data (submitted or actual) */
	unsigned int len_cap;	/* Delivered length */
	unsigned char setup[SETUP_PACKET_LEN];	/* Only for Control S-type */
};

/*--------------------------------------------------------*\
\*--------------------------------------------------------*/
void __avm_usb_trace (char type, struct usb_hcd *hcd, struct urb *urb, int status) {
	struct pcap_usb_hdr_189 *usb_hdr;
	struct sk_buff *skb;
	struct timeval tv;
	int ant_dir;

	if (urb->setup_packet && (USB_TYPE_VENDOR == (USB_TYPE_MASK & urb->setup_packet[0]))) {
		return;
	}

	skb = alloc_skb (sizeof(*usb_hdr)+MAX_DATA_TRACE_LEN, GFP_ATOMIC);
	if (skb == NULL) {
		return;
	}

	do_gettimeofday (&tv);
	skb->tstamp = ktime_set (tv.tv_sec, tv.tv_usec * NSEC_PER_USEC);
	usb_hdr = (struct pcap_usb_hdr_189 *) skb_put(skb, sizeof(*usb_hdr));

	usb_hdr->id = (unsigned long)urb;
	usb_hdr->type = type;
	usb_hdr->xfer_type = usb_pipetype(urb->pipe);
	usb_hdr->epnum = usb_pipeendpoint(urb->pipe) | usb_pipein(urb->pipe);
	usb_hdr->devnum = usb_pipedevice(urb->pipe);
	usb_hdr->busnum = hcd->self.busnum;
	usb_hdr->flag_setup = '-';
	usb_hdr->flag_data = usb_urb_dir_in(urb)? '<': '>';
	usb_hdr->ts_sec = tv.tv_sec;
	usb_hdr->ts_usec = tv.tv_usec;
	usb_hdr->status = status;

	memset(&usb_hdr->setup, 0, sizeof (usb_hdr->setup));
	if (type == 'S') {
		ant_dir = AVM_NET_TRACE_DIRECTION_OUTGOING;
		usb_hdr->len_urb = urb->transfer_buffer_length;
		if (usb_urb_dir_in(urb)) {
			usb_hdr->len_cap = 0;
		} else {
			usb_hdr->len_cap = usb_hdr->len_urb;
		}
		if (urb->setup_packet) {
			memcpy(&usb_hdr->setup, urb->setup_packet, sizeof (usb_hdr->setup));
			usb_hdr->flag_setup = 0;
		}
	} else {
		ant_dir = AVM_NET_TRACE_DIRECTION_HOST;
		usb_hdr->len_urb = urb->actual_length;
		usb_hdr->len_cap = 0;
		if (usb_urb_dir_in(urb) && (type == 'C')) {
			usb_hdr->len_cap = usb_hdr->len_urb;
		}
	}

	if (usb_hdr->len_cap) {
		unsigned char * data;
		if (usb_hdr->len_cap > MAX_DATA_TRACE_LEN) {
			usb_hdr->len_cap = MAX_DATA_TRACE_LEN;
		}
		if (urb->num_sgs == 0) {
			if (urb->transfer_buffer == NULL) {
				usb_hdr->len_cap = 0;
			} else {
				data = skb_put(skb, usb_hdr->len_cap);
				memcpy(data, urb->transfer_buffer, usb_hdr->len_cap);
				usb_hdr->flag_data = 0;
			}
		} else {
			data = skb_put(skb, usb_hdr->len_cap);
			usb_hdr->len_cap = 
				sg_copy_to_buffer(urb->sg, urb->num_sgs, data, usb_hdr->len_cap);
			usb_hdr->flag_data = 0;
		}
	}

	if (avm_net_trace_func (hcd->avm_ntd, skb, AVM_NET_TRACE_USE_SKB, ant_dir) < 0)
		kfree_skb(skb);
}

/*--------------------------------------------------------*\
\*--------------------------------------------------------*/
int avm_usb_register_trace_device (struct usb_hcd *hcd) {

	struct avm_net_trace_device *ntd = hcd->avm_ntd;
	if (ntd != NULL) {
		dev_err(hcd->self.controller, "Net trace device (%s:%s) already registered.\n", ntd->name, hcd->self.bus_name);
		return -EEXIST;
	}

	ntd = (struct avm_net_trace_device*) kzalloc (sizeof (struct avm_net_trace_device), GFP_KERNEL);

	if (ntd == NULL) {
		return -ENOMEM;
	}

	ntd->minor = AVM_NET_TRACE_ID_BASE_USB + hcd->self.busnum;
	ntd->iface = 0;
	ntd->type  = AVM_NET_TRACE_TYPE_USB;
	ntd->pcap_encap = PCAP_ENCAP_USB_189;
	ntd->pcap_protocol = PCAP_ENCAP_USB_189;
	snprintf (ntd->name, AVM_NET_TRACE_IFNAMSIZ, "usb%u", hcd->self.busnum);

	if (ntd->minor > AVM_NET_TRACE_ID_MAX_USB) {
		dev_err(hcd->self.controller,
			"Register net trace device '(%s:%s)' failed! Minor ID %d is greater than ID_MAX_USB %d.\n",
			ntd->name,
			hcd->self.bus_name,
			ntd->minor,
			AVM_NET_TRACE_ID_MAX_USB);
		kfree(ntd);
		return -EINVAL;
	}

	if (register_avm_net_trace_device (ntd) < 0) {
		dev_err(hcd->self.controller, "Register net trace device '(%s:%s)' failed!\n", ntd->name, hcd->self.bus_name);
		kfree(ntd);
		return -EPERM;
	}

	hcd->avm_ntd = ntd;

	return 0;
}

/*--------------------------------------------------------*\
\*--------------------------------------------------------*/
void avm_usb_deregister_trace_device (struct usb_hcd *hcd) {

	if (hcd->avm_ntd != NULL) {
		deregister_avm_net_trace_device (hcd->avm_ntd);
		kfree (hcd->avm_ntd);
		hcd->avm_ntd = NULL;
	}
}

#endif /* defined (AVM_USB_TRACE) */
#endif /* CONFIG_AVM_NET_TRACE */
