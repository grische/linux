#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <lantiq.h>
#include <asm/addrspace.h>

#define SRAMFLAG (void *)KSEG1ADDR(0x1f807f00)


static bool started = false;

static ssize_t iap_show(struct kobject *kobj __always_unused, struct kobj_attribute *attr __always_unused, char *buf) {
	strcpy(buf, started ? "started" : "not started");

	return strlen(buf);
}

static ssize_t iap_store(struct kobject *kobj __always_unused, struct kobj_attribute *attr __always_unused, const char *buf, size_t count) {
	if (count < 5 || strncmp("start", buf, 5) != 0) {
		pr_err("interaptiv_start: unknown input. Run 'echo start > /sys/kernel/interaptiv_start' to start the interaptiv\n");
		return -EINVAL;
	}

	if (started) {
		pr_err("interaptiv_start: interaptiv already started\n");
		return -EINVAL;
	}

	pr_warn("interaptiv_start: starting interaptiv\n");
	ltq_w32(0, SRAMFLAG);
	started = true;

	return count;
}

static const struct kobj_attribute iap_attr = {
	.attr = {
		.name = "interaptiv_start",
		.mode = 0600,
	},
	.show = iap_show,
	.store = iap_store,
};

static int __init iap_init(void) {
	return sysfs_create_file(kernel_kobj, &iap_attr.attr);
}

late_initcall(iap_init);
