// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2013-2019 AVM GmbH <fritzbox_info@avm.de> */
/*
 *   author: cbuettner@avm.de
 *   description: generic avm_event source for ethernet device drivers
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/env.h>
#include <linux/of.h>

#if defined(CONFIG_AVM_POWER)
#include <avm/power/power.h>
#endif

#if defined(CONFIG_AVM_EVENT)
#include <avm/event/event.h>
#endif

#if defined(CONFIG_AVM_KERNEL)
#include <asm/mach_avm.h>
#include <linux/avm_hw_config.h>
#endif

#if defined(CONFIG_AVM_LED_EVENTS)
#include <avm/event/led.h>
#if ((LED_EVENT_VERSION <= 2) && (LED_EVENT_SUBVERSION < 12))
#error we dont support legacy led events
#endif
#endif

struct map_avmport_netdev {
	char avmdev_name[IFNAMSIZ];
	char netdev_name[IFNAMSIZ];
	bool wanport;
};

/* TODO: Das wird schnell unübersichtlich!
 * Kriegt man die Infos auch woanders her?
 * Anders unterbringen/in HW_CONFIG?
 * AMY
 *
 * .. Siehe JZ-50409 ..
 * Als Nächstes: Aufräumen!
 */

/* Die Reihenfolge der Einträge hier ist wichtig! */
struct map_avmport_netdev mapHW227[] = {
	{ "lan1", "eth0", false },
	{ "lan2", "eth1", false },
	{ "lan3", "eth2", false },
	{ "lan4", "eth3", false },
	{ "wan", "wan", true }
};

struct map_avmport_netdev mapHW229[] = {
	{ "lan1", "eth0", false },
};
struct map_avmport_netdev mapHW232[] = {
	{ "lan1", "eth0", false },
};
struct map_avmport_netdev map7581[] = {
	{ "lan1", "eth1", false },
	{ "lan2", "eth2", false },
	{ "lan3", "eth3", false },
	{ "lan4", "eth4", false },
	{ "wan", "eth0", true }
};

struct _avmnet_linkstate_cache {
	size_t avm_portnum; /* 0...7 */
	bool link_state;
	char avmdev_name[IFNAMSIZ];
	char netdev_name[IFNAMSIZ];
	struct net_device *netdev;
#if defined(CONFIG_AVM_LED_EVENTS)
	bool led_event_pending;
#endif
#if defined(CONFIG_AVM_POWER)
	uint32_t power_percent;
#endif
#if defined(CONFIG_AVM_EVENT)
	struct cpmac_port cpmac_port_state;
#endif
};

#if defined(CONFIG_AVM_EVENT)
static void *avm_event_handle;
#endif
static struct _avmnet_linkstate_cache *avmnet_linkstate_cache;
static size_t avmnet_linkstate_cache_size;

#if defined(CONFIG_AVM_LED_EVENTS)
struct _led_event_map {
	const char avmdev_name[IFNAMSIZ];
	enum _led_event active_event_code;
	enum _led_event inactive_event_code;
};

struct _led_event_map led_event_map[] = {
	{ "lan1", event_lan1_active, event_lan1_inactive },
	{ "lan2", event_lan2_active, event_lan2_inactive },
	{ "lan3", event_lan3_active, event_lan3_inactive },
	{ "lan4", event_lan4_active, event_lan4_inactive },
	{ "lan5", event_lan5_active, event_lan5_inactive },
	{ "wan", event_wan_active, event_wan_inactive },
};

static enum _led_event lookup_led_event(const char *avmdev_name,
					bool link_active)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(led_event_map); i++) {
		if (strncmp(avmdev_name, led_event_map[i].avmdev_name,
			    IFNAMSIZ) == 0) {
			return link_active ?
				       led_event_map[i].active_event_code :
				       led_event_map[i].inactive_event_code;
		}
	}
	BUG();
}
#endif

#if defined(CONFIG_AVM_EVENT)
static void avm_event_trigger(void *event_handle)
{
	struct cpmac_event_struct *event;
	size_t i;

	BUG_ON(avmnet_linkstate_cache_size > AVM_EVENT_ETH_MAXPORTS);

	if (avmnet_linkstate_cache_size == 0) {
		pr_debug("[%s] no cached devices\n", __func__);
		return;
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		pr_debug("[%s] oom\n", __func__);
		return;
	}

	event->event_header.id = avm_event_id_ethernet_connect_status;
	event->ports = avmnet_linkstate_cache_size;
	for (i = 0; i < event->ports; i++) {
		memcpy(&event->port[i],
		       &avmnet_linkstate_cache[i].cpmac_port_state,
		       sizeof(struct cpmac_port));
	}
	avm_event_source_trigger(event_handle,
				 avm_event_id_ethernet_connect_status,
				 sizeof(*event), (void *)event);
}

static void avm_event_handler(void *context, enum _avm_event_id id)
{
	void *event_handle = avm_event_handle;

	if (id != avm_event_id_ethernet_connect_status)
		return;
	if (!avm_event_source_check_id(event_handle,
				       avm_event_id_ethernet_connect_status)) {
		return;
	}
	pr_debug("connection status requested\n");
	avm_event_trigger(event_handle);
}
#endif

#if defined(CONFIG_AVM_POWER)
static int avm_pwrmgmnt_set_phy_state(int state)
{
	union _powermanagment_ethernet_state pstate;
	uint8_t port;
	struct ethtool_cmd cmd;
	struct _avmnet_linkstate_cache *cache_port;

	pstate.Register = state;

	pr_debug("port=%d throttle=%s\n", pstate.Bits.port,
		 pstate.Bits.throttle_eth ? "on" : "off");

	port = pstate.Bits.port;

	if (port >= avmnet_linkstate_cache_size) {
		pr_debug("port=%d, setup_ports=%zd\n", port,
			 avmnet_linkstate_cache_size);
		return -ENXIO;
	}

	cache_port = &avmnet_linkstate_cache[port];

	if (!cache_port->netdev) {
		pr_debug("port=%d, no netdev\n", port);
		return -ENXIO;
	}
	if (!cache_port->netdev->ethtool_ops) {
		pr_debug("port=%d, no ethtool_ops\n", port);
		return -ENXIO;
	}
	if (!cache_port->netdev->ethtool_ops->set_settings) {
		pr_debug("port=%d, no set_settings", port);
		return -ENXIO;
	}
	if (!cache_port->netdev->ethtool_ops->get_settings) {
		pr_debug("port=%d, no get_settings\n", port);
		return -ENXIO;
	}

	rtnl_lock();
	cache_port->netdev->ethtool_ops->get_settings(cache_port->netdev, &cmd);
	cmd.advertising = cmd.supported;
	if (pstate.Bits.throttle_eth) {
		cmd.advertising &= ~(ADVERTISED_1000baseT_Half |
				     ADVERTISED_1000baseT_Full);
	}
	cache_port->netdev->ethtool_ops->set_settings(cache_port->netdev, &cmd);
	rtnl_unlock();

	return 0;
}
#endif

static const char *ethtool_speed_string(const struct ethtool_cmd *ep)
{
	u32 speed = ethtool_cmd_speed(ep);

	switch (speed) {
	case SPEED_10:
		return "SPEED_10";
	case SPEED_100:
		return "SPEED_100";
	case SPEED_1000:
		return "SPEED_1000";
	case SPEED_2500:
		return "SPEED_2500";
	case SPEED_5000:
		return "SPEED_5000";
	case SPEED_10000:
		return "SPEED_10000";
	}
	return "Unknown";
}

static struct _avmnet_linkstate_cache *lookup_cache(struct net_device *netdev)
{
	size_t i;

	for (i = 0; i < avmnet_linkstate_cache_size; i++) {
		if (avmnet_linkstate_cache[i].netdev == netdev) {
			return &avmnet_linkstate_cache[i];
		}
	}
	for (i = 0; i < avmnet_linkstate_cache_size; i++) {
		if (strncmp(avmnet_linkstate_cache[i].netdev_name, netdev->name,
			    IFNAMSIZ) == 0) {
			pr_debug("setup cache for netdev \"%s\" on %zu\n",
				 netdev->name, i);
			avmnet_linkstate_cache[i].netdev = netdev;
			return &avmnet_linkstate_cache[i];
		}
	}

	return NULL;
}

#if defined(CONFIG_AVM_LED_EVENTS)
static void notify_led(struct _avmnet_linkstate_cache *cache_entry)
{
	if (!led_event_action) {
		pr_debug("[led_event_action] is not available\n");
		cache_entry->led_event_pending = true;
		return;
	}

	(*led_event_action)(LEDCTL_VERSION,
			    lookup_led_event(cache_entry->avmdev_name,
					     cache_entry->link_state),
			    1);
	cache_entry->led_event_pending = false;
}
#endif

static void notify_avm_listeners(struct _avmnet_linkstate_cache *cache_entry)
{
	pr_debug("[%s] %s/%s\n", __func__, cache_entry->avmdev_name,
		 cache_entry->netdev_name);

#if defined(CONFIG_AVM_EVENT)
	if (avm_event_handle)
		avm_event_trigger(avm_event_handle);
#endif

#if defined(CONFIG_AVM_LED_EVENTS)
	notify_led(cache_entry);
#endif

#if defined(CONFIG_AVM_POWER)
	pr_debug("reporting %u%% for port %zu\n", cache_entry->power_percent,
		 cache_entry->avm_portnum);
	PowerManagmentRessourceInfo(
		powerdevice_ethernet,
		PM_ETHERNET_PARAM((1 << cache_entry->avm_portnum),
				  cache_entry->power_percent));
#endif
}

static struct _avmnet_linkstate_cache *
update_cache_for_dev(struct net_device *dev)
{
	struct ethtool_cmd ecmd;
	struct _avmnet_linkstate_cache *cache_entry;

	if (!dev)
		return NULL;

	if (!dev->ethtool_ops)
		return NULL;

	if (!dev->ethtool_ops->get_settings)
		return NULL;

	if (!dev->ethtool_ops->get_link)
		return NULL;

	cache_entry = lookup_cache(dev);
	if (!cache_entry)
		return NULL;

	if (dev->ethtool_ops->begin && dev->ethtool_ops->begin(dev) < 0)
		return NULL;

	dev->ethtool_ops->get_settings(dev, &ecmd);
	cache_entry->link_state = netif_running(dev) &&
				  dev->ethtool_ops->get_link(dev);
	if (dev->ethtool_ops->complete)
		dev->ethtool_ops->complete(dev);

#if defined(CONFIG_AVM_EVENT)
	/*
	 * keep linkstate redundant for AVM_EVENT messages
	 */
	cache_entry->cpmac_port_state.link = cache_entry->link_state;

	if (ecmd.supported & SUPPORTED_1000baseT_Full) {
		cache_entry->cpmac_port_state.maxspeed =
			avm_event_ethernet_speed_1G;
	} else {
		cache_entry->cpmac_port_state.maxspeed =
			avm_event_ethernet_speed_100M;
	}
	switch (ecmd.speed) {
	case SPEED_10000:
		cache_entry->cpmac_port_state.speed =
			avm_event_ethernet_speed_10G;
		break;
	case SPEED_5000:
		cache_entry->cpmac_port_state.speed =
			avm_event_ethernet_speed_5G;
		break;
	case SPEED_2500:
		cache_entry->cpmac_port_state.speed =
			avm_event_ethernet_speed_2_5G;
		break;
	case SPEED_1000:
		cache_entry->cpmac_port_state.speed =
			avm_event_ethernet_speed_1G;
		break;
	case SPEED_100:
		cache_entry->cpmac_port_state.speed =
			avm_event_ethernet_speed_100M;
		break;
	case SPEED_10:
		cache_entry->cpmac_port_state.speed =
			avm_event_ethernet_speed_10M;
		break;
	}
	cache_entry->cpmac_port_state.fullduplex = ecmd.duplex;
#endif

#if defined(CONFIG_AVM_POWER)
	/*
	 * TODO: power consumption should be platform dependent !?!
	 */
	cache_entry->power_percent = 0;

	if (cache_entry->link_state) {
		switch (ecmd.speed) {
		case SPEED_10000: /* TODO: measure consumption on a future product */
		case SPEED_5000: /* TODO: measure consumption on a future product */
		case SPEED_2500: /* power consumption for 1G and 2.5G are equal on a 5550 */
		case SPEED_1000:
			cache_entry->power_percent = (550 * 100) / 550;
			break;
		case SPEED_100:
			cache_entry->power_percent = (100 * 100) / 550;
			break;
		case SPEED_10:
			cache_entry->power_percent = (50 * 100) / 550;
			break;
		}
	}
#endif

	pr_debug("netdev_event: up/changing %s: [link %d, speed=%s]\n",
		 dev->name, cache_entry->link_state,
		 ethtool_speed_string(&ecmd));

	return cache_entry;
}

static int delete_dev_from_cache(const struct net_device *netdev)
{
	unsigned int i;

	if (!netdev)
		return -ENODEV;

	for (i = 0; i < avmnet_linkstate_cache_size; i++) {
		if (avmnet_linkstate_cache[i].netdev == netdev) {
			avmnet_linkstate_cache[i].netdev = NULL;
			return 0;
		}
	}

	return -ENODEV;
}

static int
avmnet_netdev_event_handler(struct notifier_block *ev_block,
			    unsigned long event, void *ptr)
{
	const struct netdev_notifier_info *info = ptr;
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	if (!dev)
		return NOTIFY_DONE;

	pr_debug("[%s] called; event=0x%lx, dev: 0x%p; dev->name: \"%s\"\n",
		 __func__, event, dev, dev->name);

	switch (event) {
	case NETDEV_UP:
	case NETDEV_REGISTER:
	case NETDEV_CHANGE: {
		struct _avmnet_linkstate_cache *cache_entry =
			update_cache_for_dev(dev);

		if (cache_entry) {
			notify_avm_listeners(cache_entry);
		}
		break;
	}
	case NETDEV_UNREGISTER:
		/* Don't care if this fails - can do no harm either. */
		delete_dev_from_cache(dev);
		break;
	}

	return NOTIFY_DONE;
}

static int plc_reset_gpio = -1;
static int
avmnet_netdev_plc_event_handler(struct notifier_block *ev_block,
				unsigned long event, void *ptr)
{
	const struct netdev_notifier_info *info = ptr;
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	if (!dev)
		return NOTIFY_DONE;

	if (strcmp(dev->name, "plc"))
		return NOTIFY_DONE;

	pr_debug("[%s] called; event=0x%lx, dev: 0x%p; dev->name: \"%s\"\n",
		 __func__, event, dev, dev->name);

	if (plc_reset_gpio == -1) {
		pr_err("[%s] Cannot reset plc as plc_reset_gpio is not set\n",
		       __func__);
		return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_UP:
		avm_gpio_out_bit(plc_reset_gpio, 1);
		break;
	case NETDEV_DOWN:
		avm_gpio_out_bit(plc_reset_gpio, 0);
		break;
	}

	return NOTIFY_DONE;
}

#if defined(CONFIG_AVM_LED_EVENTS)
static int
avmnet_module_event_handler(struct notifier_block *self,
			    unsigned long val, void *data)
{
	struct module *mod = data;
	size_t i;

	pr_debug("[%s] called; val=0x%lx\n", __func__, val);
	if (val == MODULE_STATE_LIVE &&
	    strstr(mod->name, "led_modul_Fritz_Box_")) {
		pr_debug("%s is up and running\n", mod->name);

		/*
		 * send pending LED messages
		 * messages have to be delayed till
		 * led module is up.
		 */
		for (i = 0; i < avmnet_linkstate_cache_size; i++) {
			if (avmnet_linkstate_cache[i].led_event_pending) {
				notify_led(&avmnet_linkstate_cache[i]);
			}
		}
	}
	return 0;
}

static struct notifier_block avmnet_module_event_notifier = {
	.notifier_call = avmnet_module_event_handler,
};
#endif

static struct notifier_block avmnet_net_event_notifier = {
	.notifier_call = avmnet_netdev_event_handler,
};

static struct notifier_block avmnet_plc_event_notifier = {
	.notifier_call = avmnet_netdev_plc_event_handler,
};

static void setup_device_mapping(struct map_avmport_netdev map[], size_t size)
{
	size_t i;

	avmnet_linkstate_cache_size = size;
	pr_debug("[%s]\n", __func__);
	avmnet_linkstate_cache = kzalloc(
		size * sizeof(struct _avmnet_linkstate_cache), GFP_KERNEL);
	if (!avmnet_linkstate_cache)
		BUG();

	for (i = 0; i < size; i++) {
		strncpy(avmnet_linkstate_cache[i].avmdev_name,
			map[i].avmdev_name, IFNAMSIZ);
		strncpy(avmnet_linkstate_cache[i].netdev_name,
			map[i].netdev_name, IFNAMSIZ);
		pr_debug("avmdev=%s, netdev=%s, wan=%d\n", map[i].avmdev_name,
			 map[i].netdev_name, map[i].wanport);
		avmnet_linkstate_cache[i].avm_portnum = i;
	}
};

/**
 * Load map_avmport_netdev mapping from device tree.
 *
 * Allocates and populates a map_avmport_netdev array from a "avm,port_netdev"
 * compatible node in device tree. The avmdev_name is derived from the child
 * node names. The netdev property is used to set the netdev_name.
 * wanport is a boolean property to set wanport variable.
 * Put "plcport" property below compatible to trigger installation of plc
 * reset handlers.
 * On success, caller is responsible for freeing the pointer returned in @map.
 *
 * Example node (taken from dakota Fritz_Box_HW227-0.dts):
 *		avm_netdev_map {
 *			compatible = "avm,port_netdev";
 *
 *			lan1 {
 *				netdev = "eth0";
 *			};
 *			lan2 {
 *				netdev = "eth1";
 *			};
 *			lan3 {
 *				netdev = "eth2";
 *			};
 *			lan4 {
 *				netdev = "eth3";
 *			};
 *			wan {
 *				netdev = "wan";
 *				wanport;
 *			};
 *		};
 *
 * Note that order of nodes matters!
 *
 * @map: Pointer to fill with array of map_avmport_netdev structs.
 *
 * Returns positive number of valid array entries and valid pointer in @map or
 *         negative error code and @map set to NULL.
 */
static int map_avmport_netdev_from_dt(struct map_avmport_netdev **map)
{
#if defined(CONFIG_OF)
	struct device_node *node = NULL, *child;
	int result = 0, i;
	const char *of_compatible = "avm,port_netdev";

	*map = NULL;
	if (!of_have_populated_dt()) {
		pr_debug(
			"[%s] Device Tree not populated, cannot read map_avmport_netdev\n",
			__func__);
		result = -ENOMSG;
		goto err_out;
	}

	node = of_find_compatible_node(node, NULL, of_compatible);
	if (node) {
		result = of_get_child_count(node);
	}
	if (result == 0) {
		pr_debug("[%s] Compatible node %s not found.\n", __func__,
			 of_compatible);
		result = -ENOENT;
		goto err_out;
	}

	*map = kmalloc_array(result, sizeof(**map), GFP_KERNEL);
	if (*map == NULL) {
		result = -ENOMEM;
		goto err_out;
	}

	i = 0;
	for_each_child_of_node(node, child) {
		const char *tmpstr;

		result = of_property_read_string(child, "netdev", &tmpstr);
		if (child->name == NULL || result != 0) {
			pr_warn("[%s] Error %d reading name or netdev property for child %d. Skip.\n",
				__func__, result, i);
			continue;
		}
		strncpy((*map)[i].netdev_name, tmpstr, IFNAMSIZ - 1);
		strncpy((*map)[i].avmdev_name, child->name, IFNAMSIZ - 1);
		(*map)[i].wanport = of_property_read_bool(child, "wanport");

		pr_debug(
			"[%s] DT entry %d: netdev: %s; avmdev: %s; wanport: %d\n",
			__func__, i, (*map)[i].netdev_name,
			(*map)[i].avmdev_name, (*map)[i].wanport);
		++i;
	}

	if (i == 0)
		result = -ENOENT;
	else
		result = i;

	if (result > 0 && of_property_read_bool(node, "plcport")) {
		int ret;

		pr_debug("[%s] Got plcport property.\n", __func__);
		ret = register_netdevice_notifier(&avmnet_plc_event_notifier);
		if (ret < 0) {
			pr_warn("[%s] Failed to register plc netdevice notifier\n",
				__func__);
			result = -EAGAIN;
			goto err_out;
		}
		ret = avm_get_hw_config(AVM_HW_CONFIG_VERSION,
					"gpio_avm_reset_plc", &plc_reset_gpio,
					NULL);
		if (ret < 0) {
			pr_warn("[%s] Problem getting gpio_avm_reset_plc pin.",
				__func__);
			result = ret;
			goto err_out;
		}
	}

err_out:
	of_node_put(node);

	if (result < 0 && *map) {
		kfree(*map);
		*map = NULL;
	}
	return result;
#else
	*map = NULL;

	return -ENOENT;
#endif
}

int __init avmnet_event_init(void)
{
	int ret;
	char *hwid = NULL;
	struct map_avmport_netdev *map;

	ret = map_avmport_netdev_from_dt(&map);
	if (ret > 0) {
		setup_device_mapping(map, ret);
		kfree(map);
	} else {
		if (ret && ret != -ENOENT)
			pr_warn("[%s] Error %d reading avmnet mappings from DT. Trying fallback.\n",
				__func__, ret);
		/* Fallback: Get local mappings. */
		hwid = prom_getenv("HWRevision");
		if (!hwid)
			return -ENODEV;

		switch (simple_strtol(hwid, NULL, 0)) {
		case 227:
			setup_device_mapping(mapHW227, ARRAY_SIZE(mapHW227));
			break;
		case 232:
			setup_device_mapping(mapHW232, ARRAY_SIZE(mapHW232));
			break;
		case 229:
			setup_device_mapping(mapHW229, ARRAY_SIZE(mapHW229));
			/* FALLTHRU */
		case 235:
			ret = register_netdevice_notifier(
				&avmnet_plc_event_notifier);
			if (ret < 0) {
				pr_warn("[%s] Failed to register plc netdevice notifier\n",
					__func__);
				return -EAGAIN;
			}
			ret = avm_get_hw_config(AVM_HW_CONFIG_VERSION,
						"gpio_avm_reset_plc",
						&plc_reset_gpio, NULL);
			if (ret < 0) {
				pr_warn("[%s] Problem getting gpio_avm_reset_plc pin.",
					__func__);
				return ret;
			}
			break;
		case 244:
			setup_device_mapping(mapHW229, ARRAY_SIZE(mapHW229));
			break;
		case 224:
		case 228:
			setup_device_mapping(map7581, ARRAY_SIZE(map7581));
			break;
		default:
			pr_err("no support for hwid %s\n", hwid);
			return -ENODEV;
		}
	}

	ret = register_netdevice_notifier(&avmnet_net_event_notifier);
	if (ret < 0) {
		pr_warn("Failed to register netdevice notifier\n");
		return -EAGAIN;
	}
#if defined(CONFIG_AVM_LED_EVENTS)
	ret = register_module_notifier(&avmnet_module_event_notifier);
	if (ret < 0) {
		pr_warn("Failed to register module notifier\n");
		return -EAGAIN;
	}
#endif

#if defined(CONFIG_AVM_EVENT)
	{
		struct _avm_event_id_mask id_mask;

		avm_event_handle = avm_event_source_register(
			"Ethernet status",
			avm_event_build_id_mask(
				&id_mask, 1,
				avm_event_id_ethernet_connect_status),
			avm_event_handler, NULL);
	}
#endif

#if defined(CONFIG_AVM_POWER)
	PowerManagmentRegister("ethernet", avm_pwrmgmnt_set_phy_state);
#endif

	return 0;
}

arch_initcall(avmnet_event_init);
