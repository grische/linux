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

#ifndef _AVMUSBTRACE_H_
#define _AVMUSBTRACE_H_

#if defined (CONFIG_AVM_NET_TRACE)

#include <avm/net_trace/net_trace.h>
#undef AVM_USB_TRACE

#if defined (AVM_NET_TRACE_TYPE_USB)
#define AVM_USB_TRACE
#endif

#ifdef AVM_USB_TRACE

struct usb_hcd;
struct urb;

void __avm_usb_trace (char type, struct usb_hcd *hcd, struct urb *urb, int status);

#define avm_usb_trace_submit(hcd, urb) do {	\
	if ((hcd)->avm_ntd && __avm_net_trace_in_progress((hcd)->avm_ntd))	\
		__avm_usb_trace ('S', hcd, urb, urb->status);	\
} while (0)

#define avm_usb_trace_complete(hcd, urb, status) do {		\
	if ((hcd)->avm_ntd && __avm_net_trace_in_progress((hcd)->avm_ntd))	\
		__avm_usb_trace ('C', hcd, urb, status);	\
} while (0)

#define avm_usb_trace_error(hcd, urb, status) do {		\
	if ((hcd)->avm_ntd && __avm_net_trace_in_progress((hcd)->avm_ntd))	\
		__avm_usb_trace ('E', hcd, urb, status);	\
} while (0)

int avm_usb_register_trace_device (struct usb_hcd *hcd);
void avm_usb_deregister_trace_device (struct usb_hcd *hcd);

#endif /* AVM_USB_TRACE */
#endif /* CONFIG_AVM_NET_TRACE */

#endif /* _AVMUSBTRACE_H_ */

