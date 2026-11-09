#include <linux/pm.h>
#include <linux/acpi.h>

struct usb_hub_descriptor;
struct usb_dev_state;

#ifdef CONFIG_AVM_USB_SUSPEND

#ifdef CONFIG_PM
#error CONFIG_PM must not be set while CONFIG_AVM_USB_SUSPEND is active
#endif

//#define AVM_USB_SUSPEND_DEBUG
#define AVM_USB_SUSPEND_DELAY 60
#define AVM_USB_SUSPEND_MAX_BLACKLIST_SIZE 8
#define AVM_USB_SUSPEND_SPINDOWN_DELAY (630*1000) // 10.5 min in ms
#define AVM_USB_SUSPEND_FLASH_DELAY    (20*1000)  // 20 s in ms
#define AVM_USB_SUSPEND_MIN_DELAY      2000       // 2 s in ms

struct avm_usb_suspend_blacklist {
	unsigned int idVendor;
	unsigned int idProduct;
	unsigned int bcdDevice;
};
#endif

/* Functions local to drivers/usb/core/ */

extern int usb_create_sysfs_dev_files(struct usb_device *dev);
extern void usb_remove_sysfs_dev_files(struct usb_device *dev);
extern void usb_create_sysfs_intf_files(struct usb_interface *intf);
extern void usb_remove_sysfs_intf_files(struct usb_interface *intf);
extern int usb_create_ep_devs(struct device *parent,
				struct usb_host_endpoint *endpoint,
				struct usb_device *udev);
extern void usb_remove_ep_devs(struct usb_host_endpoint *endpoint);

extern void usb_enable_endpoint(struct usb_device *dev,
		struct usb_host_endpoint *ep, bool reset_toggle);
extern void usb_enable_interface(struct usb_device *dev,
		struct usb_interface *intf, bool reset_toggles);
extern void usb_disable_endpoint(struct usb_device *dev, unsigned int epaddr,
		bool reset_hardware);
extern void usb_disable_interface(struct usb_device *dev,
		struct usb_interface *intf, bool reset_hardware);
extern void usb_release_interface_cache(struct kref *ref);
extern void usb_disable_device(struct usb_device *dev, int skip_ep0);
extern int usb_deauthorize_device(struct usb_device *);
extern int usb_authorize_device(struct usb_device *);
extern void usb_deauthorize_interface(struct usb_interface *);
extern void usb_authorize_interface(struct usb_interface *);
extern void usb_detect_quirks(struct usb_device *udev);
extern void usb_detect_interface_quirks(struct usb_device *udev);
extern int usb_remove_device(struct usb_device *udev);

extern int usb_get_device_descriptor(struct usb_device *dev,
		unsigned int size);
extern int usb_get_bos_descriptor(struct usb_device *dev);
extern void usb_release_bos_descriptor(struct usb_device *dev);
extern char *usb_cache_string(struct usb_device *udev, int index);
extern int usb_set_configuration(struct usb_device *dev, int configuration);
extern int usb_choose_configuration(struct usb_device *udev);

static inline unsigned usb_get_max_power(struct usb_device *udev,
		struct usb_host_config *c)
{
	/* SuperSpeed power is in 8 mA units; others are in 2 mA units */
	unsigned mul = (udev->speed >= USB_SPEED_SUPER ? 8 : 2);

	return c->desc.bMaxPower * mul;
}

extern void usb_kick_hub_wq(struct usb_device *dev);
extern int usb_match_one_id_intf(struct usb_device *dev,
				 struct usb_host_interface *intf,
				 const struct usb_device_id *id);
extern int usb_match_device(struct usb_device *dev,
			    const struct usb_device_id *id);
extern void usb_forced_unbind_intf(struct usb_interface *intf);
extern void usb_unbind_and_rebind_marked_interfaces(struct usb_device *udev);

extern void usb_hub_release_all_ports(struct usb_device *hdev,
		struct usb_dev_state *owner);
extern bool usb_device_is_owned(struct usb_device *udev);

extern int  usb_hub_init(void);
extern void usb_hub_cleanup(void);
extern int usb_major_init(void);
extern void usb_major_cleanup(void);
extern int usb_device_supports_lpm(struct usb_device *udev);

#ifdef	CONFIG_PM

extern int usb_suspend(struct device *dev, pm_message_t msg);
extern int usb_resume(struct device *dev, pm_message_t msg);
extern int usb_resume_complete(struct device *dev);

#endif

#if defined (CONFIG_PM) || defined (CONFIG_AVM_USB_SUSPEND)

extern int usb_port_suspend(struct usb_device *dev, pm_message_t msg);
extern int usb_port_resume(struct usb_device *dev, pm_message_t msg);

#else

static inline int usb_port_suspend(struct usb_device *udev, pm_message_t msg)
{
	return 0;
}

static inline int usb_port_resume(struct usb_device *udev, pm_message_t msg)
{
	return 0;
}

#endif

#ifdef CONFIG_AVM_USB_SUSPEND
extern int suspend_error_count;

extern void avm_usb_suspend_init(struct usb_device *udev);
extern void avm_usb_suspend_stop(struct usb_device *udev);
extern void avm_usb_suspend_schedule(struct usb_device *udev, unsigned int delay);
extern void avm_usb_suspend_set_delay(struct usb_device *udev, unsigned msesc);
extern int avm_usb_suspend_is_blacklisted(struct usb_device *udev);
extern void avm_usb_suspend_add_to_blacklist(struct usb_device *udev);

#define avm_usb_suspend_get(udev_p) __avm_usb_suspend_get_func(udev_p, __func__)
#define avm_usb_suspend_put(udev_p) __avm_usb_suspend_put_func(udev_p, __func__)
#define avm_usb_suspend_get_locked(udev_p) __avm_usb_suspend_get_locked_func(udev_p, __func__)
#define avm_usb_suspend_put_locked(udev_p) __avm_usb_suspend_put_locked_func(udev_p, __func__)

extern int	__avm_usb_suspend_start_timer(struct usb_device *udev);
extern int	__avm_usb_suspend_do_resume(struct usb_device *udev);
extern void	__avm_usb_suspend_do_suspend(struct usb_device *udev);

extern int	__avm_usb_suspend_get(struct usb_device *udev);
extern void	__avm_usb_suspend_put(struct usb_device *udev);

static inline bool avm_usb_suspend_is_idle(struct usb_device *udev)
{
	return (atomic_read(&udev->avm_use_count) == 0);
}

static inline int __avm_usb_suspend_get_locked_func(struct usb_device *udev, const char *func_name)
{
	int ret;
#ifdef AVM_USB_SUSPEND_DEBUG
	WARN_ON(!mutex_is_locked(&udev->dev.mutex));
#endif
	ret = __avm_usb_suspend_get(udev);
#ifdef AVM_USB_SUSPEND_DEBUG
	printk(KERN_ERR "%s(): avm_usb_suspend_get ret = %d, avm_use_count = %d\n",
						func_name, ret, atomic_read(&udev->avm_use_count));
#endif
	return ret;
}

static inline void __avm_usb_suspend_put_locked_func(struct usb_device *udev, const char *func_name)
{
#ifdef AVM_USB_SUSPEND_DEBUG
	WARN_ON(!mutex_is_locked(&udev->dev.mutex));
#endif
	__avm_usb_suspend_put(udev);
#ifdef AVM_USB_SUSPEND_DEBUG
	printk(KERN_ERR "%s(): avm_usb_suspend_put avm_use_count = %d\n",
						func_name, atomic_read(&udev->avm_use_count));
#endif
}

#ifdef AVM_USB_SUSPEND_DEBUG
static inline void __avm_usb_suspend_wait_until_unlocked(struct usb_device *udev)
{
	int count;

	for (count = 0; count < 100; count ++) {
		if(!mutex_is_locked(&udev->dev.mutex))
			break;
		schedule();
	}
}
#endif

static inline int __avm_usb_suspend_get_func(struct usb_device *udev, const char *func_name)
{
	int ret;
#ifdef AVM_USB_SUSPEND_DEBUG
	__avm_usb_suspend_wait_until_unlocked(udev);
	WARN_ON(mutex_is_locked(&udev->dev.mutex));
#endif
	usb_lock_device(udev);
	ret = __avm_usb_suspend_get_locked_func(udev, func_name);
	usb_unlock_device(udev);
	return ret;
}

static inline void __avm_usb_suspend_put_func(struct usb_device *udev, const char *func_name)
{
#ifdef AVM_USB_SUSPEND_DEBUG
	__avm_usb_suspend_wait_until_unlocked(udev);
	WARN_ON(mutex_is_locked(&udev->dev.mutex));
#endif
	usb_lock_device(udev);
	__avm_usb_suspend_put_locked_func(udev, func_name);
	usb_unlock_device(udev);
}

static inline void avm_usb_suspend_min_delay(struct usb_device *udev, unsigned msecs)
{
#ifdef AVM_USB_SUSPEND_DEBUG
	__avm_usb_suspend_wait_until_unlocked(udev);
	WARN_ON(mutex_is_locked(&udev->dev.mutex));
#endif
	if (udev->avm_usb_suspend_enable &&
		(udev->avm_usb_suspend_delay > 0) && (udev->avm_usb_suspend_delay < msecs_to_jiffies(msecs))) {
		avm_usb_suspend_set_delay(udev, msecs);
	}
}

static inline void avm_usb_suspend_max_delay(struct usb_device *udev, unsigned msecs)
{
#ifdef AVM_USB_SUSPEND_DEBUG
	__avm_usb_suspend_wait_until_unlocked(udev);
	WARN_ON(mutex_is_locked(&udev->dev.mutex));
#endif
	if (udev->avm_usb_suspend_enable && (udev->avm_usb_suspend_delay > msecs_to_jiffies(msecs))) {
		avm_usb_suspend_set_delay(udev, msecs);
	}
}

static inline unsigned int avm_usb_suspend_get_idle_ms(struct usb_device *udev)
{
	if (atomic_read(&udev->avm_use_count) > 0) {
		return 0;
	}

	return jiffies_to_msecs(jiffies - udev->avm_usb_suspend_last_busy);
}

#else

#define avm_usb_suspend_get(udev_p) do {} while (0)
#define avm_usb_suspend_put(udev_p) do {} while (0)
#define avm_usb_suspend_get_locked(udev_p) do {} while (0)
#define avm_usb_suspend_put_locked(udev_p) do {} while (0)
#define avm_usb_suspend_schedule(udev_p,delay) do {} while (0)
#define avm_usb_suspend_set_delay(udev_p,delay) do {} while (0)
#define avm_usb_suspend_min_delay(udev_p,delay) do {} while (0)
#define avm_usb_suspend_max_delay(udev_p,delay) do {} while (0)
#define avm_usb_suspend_get_idle_ms(udev) (0)

#endif

#ifdef CONFIG_PM

extern void usb_autosuspend_device(struct usb_device *udev);
extern int usb_autoresume_device(struct usb_device *udev);
extern int usb_remote_wakeup(struct usb_device *dev);
extern int usb_runtime_suspend(struct device *dev);
extern int usb_runtime_resume(struct device *dev);
extern int usb_runtime_idle(struct device *dev);
extern int usb_enable_usb2_hardware_lpm(struct usb_device *udev);
extern int usb_disable_usb2_hardware_lpm(struct usb_device *udev);

#else

#ifdef CONFIG_AVM_USB_SUSPEND

static inline void usb_autosuspend_device(struct usb_device *udev)
{
	avm_usb_suspend_put_locked(udev);
}

static inline int usb_autoresume_device(struct usb_device *udev)
{
	return avm_usb_suspend_get_locked(udev);
}

#else

#define usb_autosuspend_device(udev)		do {} while (0)
static inline int usb_autoresume_device(struct usb_device *udev)
{
	return 0;
}

#endif /* CONFIG_AVM_USB_SUSPEND */

static inline int usb_enable_usb2_hardware_lpm(struct usb_device *udev)
{
	return 0;
}

static inline int usb_disable_usb2_hardware_lpm(struct usb_device *udev)
{
	return 0;
}

#endif

extern struct bus_type usb_bus_type;
extern struct mutex usb_port_peer_mutex;
extern struct device_type usb_device_type;
extern struct device_type usb_if_device_type;
extern struct device_type usb_ep_device_type;
extern struct device_type usb_port_device_type;
extern struct usb_device_driver usb_generic_driver;

static inline int is_usb_device(const struct device *dev)
{
	return dev->type == &usb_device_type;
}

static inline int is_usb_interface(const struct device *dev)
{
	return dev->type == &usb_if_device_type;
}

static inline int is_usb_endpoint(const struct device *dev)
{
	return dev->type == &usb_ep_device_type;
}

static inline int is_usb_port(const struct device *dev)
{
	return dev->type == &usb_port_device_type;
}

/* Do the same for device drivers and interface drivers. */

static inline int is_usb_device_driver(struct device_driver *drv)
{
	return container_of(drv, struct usbdrv_wrap, driver)->
			for_devices;
}

/* for labeling diagnostics */
extern const char *usbcore_name;

/* sysfs stuff */
extern const struct attribute_group *usb_device_groups[];
extern const struct attribute_group *usb_interface_groups[];

/* usbfs stuff */
extern struct mutex usbfs_mutex;
extern struct usb_driver usbfs_driver;
extern const struct file_operations usbfs_devices_fops;
extern const struct file_operations usbdev_file_operations;
extern void usbfs_conn_disc_event(void);

extern int usb_devio_init(void);
extern void usb_devio_cleanup(void);

/*
 * Firmware specific cookie identifying a port's location. '0' == no location
 * data available
 */
typedef u32 usb_port_location_t;

/* internal notify stuff */
extern void usb_notify_add_device(struct usb_device *udev);
extern void usb_notify_remove_device(struct usb_device *udev);
extern void usb_notify_add_bus(struct usb_bus *ubus);
extern void usb_notify_remove_bus(struct usb_bus *ubus);
extern void usb_hub_adjust_deviceremovable(struct usb_device *hdev,
		struct usb_hub_descriptor *desc);

#ifdef CONFIG_ACPI
extern int usb_acpi_register(void);
extern void usb_acpi_unregister(void);
extern acpi_handle usb_get_hub_port_acpi_handle(struct usb_device *hdev,
	int port1);
#else
static inline int usb_acpi_register(void) { return 0; };
static inline void usb_acpi_unregister(void) { };
#endif
