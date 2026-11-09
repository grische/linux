// SPDX-License-Identifier: GPL-2.0+

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/dma-map-ops.h>
#else
#include <linux/dma-mapping.h>
#endif
#include <avm/sammel/helpers.h>
#include <avm/compat/misc.h>

int avm_read_from_file(const char *filename, char *str, int maxlen)
{
	struct file *fp;
	loff_t pos = 0;
	int bytesRead;

	if (WARN_ON(!filename || !str || maxlen == 0))
		return -EINVAL;

	fp = filp_open(filename, O_RDONLY, 00);
	if (IS_ERR(fp)) {
		str[0] = 0;
		return PTR_ERR(fp);
	}
	bytesRead = avm_kernel_read(fp, str, maxlen - 1, &pos);
	filp_close(fp, NULL);

	if (bytesRead < 0)
		str[0] = 0;
	else
		str[bytesRead] = 0;

	return bytesRead;
}
EXPORT_SYMBOL(avm_read_from_file);

int avm_write_to_file(const char *filename, const char *str, int len)
{
	struct file *fp;
	loff_t pos = 0;
	int ret;

	if (WARN_ON(!filename || !str || len == 0))
		return -EINVAL;

	pr_debug("%s: write %s to %s\n", __func__, str, filename);
	fp = filp_open(filename, O_WRONLY, 00);

	if (IS_ERR(fp) || (fp->f_op == 0))
		return -EIO;

	/* Schreibzugriff auf File(system) erlaubt? */
	if (fp->f_op->write == NULL) {
		pr_err("[avm_power]speedstep failed: write %s\n", filename);
		filp_close(fp, NULL);
		return -EPERM;
	}
	ret = avm_kernel_write(fp, str, len, &pos);
	if (ret > 0)
		ret = 0;

	filp_close(fp, NULL); /* Close the file */
	return ret;
}
EXPORT_SYMBOL(avm_write_to_file);

/*
 * Sometimes avm modules need symbols that are not exported by the kernel.
 *
 * To avoid the need to patch every kernel there is, we just export them
 * here. Use the GPL variant unless the exported symbol is known to be non-GPL.
 */
EXPORT_SYMBOL_GPL(func_ptr_is_kernel_text);

ssize_t time_to_ascii(time64_t local_time, char *buf, int len)
{
	static const char * const month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char * const day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	struct rtc_time tm;

	rtc_time64_to_tm(local_time, &tm);

	return scnprintf(buf, len, "%s %s %02d %02d:%02d:%02d %02d UTC",
		 day[tm.tm_wday], month[tm.tm_mon],
		 tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);
}

int request_irq_on(int cpu, unsigned int irq,
		   irq_handler_t handler, unsigned long irqflags,
		   const char *devname, void *dev_id)
{
	const struct cpumask *mask = cpumask_of(cpu);

	irq_set_affinity(irq, mask);
	return request_irq(irq, handler, irqflags, devname, dev_id);
}
EXPORT_SYMBOL(request_irq_on);

int register_simple_dma_device(struct device *dev, const char *name)
{
	int ret;

	ret = dev_set_name(dev, name);
	if (ret)
		return ret;

	ret = device_register(dev);
	if (ret) {
		put_device(dev);
		return ret;
	}

	ret = dma_coerce_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		device_unregister(dev);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(register_simple_dma_device);

void unregister_simple_dma_device(struct device *dev)
{
	device_unregister(dev);
}
EXPORT_SYMBOL(unregister_simple_dma_device);
