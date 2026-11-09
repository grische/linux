#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <lantiq.h>

#include <asm/addrspace.h>
#include <linux/uaccess.h>

#include <avm/enh/fw_info.h>

#define IFMUX_CFG		(void *)KSEG1ADDR(0x16080120)
#define VCODEC_UART_EN_SHIFT	(12)
#define VCODEC_UART_EN_MASK	(3 << VCODEC_UART_EN_SHIFT)
#define VCODEC_UART_EN_ASC0_U	(0 << VCODEC_UART_EN_SHIFT)
#define VCODEC_UART_EN_TEP_U	(2 << VCODEC_UART_EN_SHIFT)

#define ASC_WHBSTATE		(void *)KSEG1ADDR(0x16600018)
#define ASC_WHBSTATE_CLRREN	(1 << 0)
#define ASC_WHBSTATE_SETREN	(1 << 1)

static const char *mode_to_string(uint32_t mode) {
	return (mode == VCODEC_UART_EN_ASC0_U) ? "interaptiv" : "bootcore";
}

static ssize_t console_show(struct kobject *kobj __always_unused,
		struct kobj_attribute *attr __always_unused, char *buf) {
	uint32_t mode = ltq_r32(IFMUX_CFG) & VCODEC_UART_EN_MASK;
	BUG_ON(!(mode == VCODEC_UART_EN_TEP_U || mode == VCODEC_UART_EN_ASC0_U));

	strcpy(buf, mode_to_string(mode));
	return strlen(buf);
}

static ssize_t console_store(struct kobject *kobj __always_unused,
		struct kobj_attribute *attr __always_unused, const char *buf,
		size_t count) {

	uint32_t newmode, oldmode, ifmux;

	if (count < 1)
		return -EINVAL;

	// Denies access to other modes when running Release version
	if (!avm_fw_is_internal())
		return -EPERM;

	ifmux = ltq_r32(IFMUX_CFG);

	oldmode = ifmux & VCODEC_UART_EN_MASK;

	BUG_ON(!(oldmode == VCODEC_UART_EN_TEP_U || oldmode == VCODEC_UART_EN_ASC0_U));

	ifmux &= ~VCODEC_UART_EN_MASK;

	switch (buf[0]) {
	case 'b':
	case 'B':
		newmode = VCODEC_UART_EN_TEP_U;
		break;
	case 'i':
	case 'I':
		newmode = VCODEC_UART_EN_ASC0_U;
		break;
	case '1':
	case 't':
	case 'T':
		newmode = (oldmode == VCODEC_UART_EN_ASC0_U) ? VCODEC_UART_EN_TEP_U : VCODEC_UART_EN_ASC0_U;
		break;
	default:
		return -EINVAL;
	}
	ifmux |= newmode;
	pr_warn("Switching to %s Console\n", mode_to_string(newmode));
	if (newmode == VCODEC_UART_EN_ASC0_U)
		ltq_w32(ASC_WHBSTATE_SETREN, ASC_WHBSTATE);
	else
		ltq_w32(ASC_WHBSTATE_CLRREN, ASC_WHBSTATE);
	ltq_w32(ifmux, IFMUX_CFG);

	return count;
}

static const struct kobj_attribute console_attr = {
	.attr = {
		.name = "switch_console",
		.mode = 0600,
	},
	.show = console_show,
	.store = console_store,
};

static int __init console_init(void) {
	return sysfs_create_file(kernel_kobj, &console_attr.attr);
}

late_initcall(console_init);
