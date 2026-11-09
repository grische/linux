// SPDX-License-Identifier: GPL-2.0-only

#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/version.h>

#include <avm/mac_address.h>

struct device_node *
avm_find_mac_address_node(struct device_node *from, u32 type)
{
	for (from = of_find_node_with_property(from, "avm,mac-name"); from;
	     from = of_find_node_with_property(from, "avm,mac-name")) {
		u32 dt_type;

		if (type == AVM_MAC_ADDR_TYPE_ANY)
			return from;
		if (of_property_read_u32(from, "avm,mac-type", &dt_type))
			continue;
		if (type == dt_type)
			return from;
	}

	return NULL;
}
EXPORT_SYMBOL(avm_find_mac_address_node);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
#define compat_of_get_mac_address of_get_mac_address
#else
static inline int
compat_of_get_mac_address(struct device_node *np, u8 *mac)
{
	const void *mac_dt = of_get_mac_address(np);

	if (IS_ERR(mac_dt))
		return PTR_ERR(mac_dt);

	memcpy(mac, mac_dt, ETH_ALEN);
	return 0;
}
#endif

int
avm_get_mac_address(const char *name, u8 buf[ETH_ALEN])
{
	struct device_node *np;

	avm_for_each_mac_address_node (np, AVM_MAC_ADDR_TYPE_ANY) {
		const char *prop;

		prop = of_get_property(np, "avm,mac-name", NULL);
		if (!prop)
			continue;

		if (!strcmp(prop, name))
			return compat_of_get_mac_address(np, buf);
	}

	return -ENODEV;
}
EXPORT_SYMBOL(avm_get_mac_address);
