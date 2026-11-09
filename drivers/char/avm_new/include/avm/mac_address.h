/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AVM_NEW_INCLUDE_AVM_MAC_ADDRESS_H
#define AVM_NEW_INCLUDE_AVM_MAC_ADDRESS_H

#include <linux/kernel.h>

#include <avm/dt-bindings/mac_address.h>

#define AVM_MAC_ADDR_TYPE_ANY 0

struct device_node *
avm_find_mac_address_node(struct device_node *from, u32 type);

#define avm_for_each_mac_address_node(dn, type) \
	for (dn = avm_find_mac_address_node(NULL, type); dn; \
	     dn = avm_find_mac_address_node(dn, type))

int
avm_get_mac_address(const char *name, u8 buf[ETH_ALEN]);

#endif
