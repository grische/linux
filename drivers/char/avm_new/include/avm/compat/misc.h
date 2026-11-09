/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _AVM_COMPAT_MISC_H_
#define _AVM_COMPAT_MISC_H_
#include <linux/device.h>
#include <linux/rtc.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/mtd/mtd.h>
#include <asm/siginfo.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

/*
 * Sorted by developer interest: items for newer kernels/interfaces first,
 * since are likely to be used the most.  Fixes to old interfaces are less
 * relevant, so are put last.
 */

/*
 * Linux < v6.4 has class_create() defined as a wrapper macro with an owner
 * argument.
 * Linux v6.4 uses a function, but without the owner parameter.
 */
#ifndef class_create
#define class_create(owner, name) class_create(name)
#endif

/* Linux >= v6.4 requires passing the semaphore count explicitly */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
#define DEFINE_SEMAPHORE_1(name) DEFINE_SEMAPHORE(name)
#else
#define DEFINE_SEMAPHORE_1(name) DEFINE_SEMAPHORE(name, 1)
#endif

#ifndef fallthrough /* Linux << v5.4 */
#if __has_attribute(__fallthrough__)
# define fallthrough                    __attribute__((__fallthrough__))
#else
# define fallthrough                    do {} while (0)  /* fallthrough */
#endif
#endif /* fallthrough */

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
#define totalram_pages() totalram_pages
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
/*
 * Upstream has re-ordered the arguments of kernel_read and kernel_write, and
 * made the loff_t arg a pointer.  Provide avm_* wrappers that use the new
 * signature.
 */
#define avm_kernel_read(file, buf, count, poff)  kernel_read(file, *(poff), buf, count)
#define avm_kernel_write(file, buf, count, poff) kernel_write(file, buf, count, *(poff))
#else
#define avm_kernel_read kernel_read
#define avm_kernel_write kernel_write
#endif

#ifndef __ro_after_init /* Linux << v4.6 */
#define __ro_after_init
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define rtc_time64_to_tm(a, b) rtc_time_to_tm(a, b)
#endif

struct mtd_erase_waiter {
	wait_queue_head_t wait_q;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
void mtd_prepare_erase_compat(struct erase_info *instr,
				     struct mtd_info *mtd,
				     struct mtd_erase_waiter *waiter);

int mtd_erase_compat(struct mtd_info *mtd,
		     struct erase_info *instr,
		     struct mtd_erase_waiter *waiter);

#else /* Linux >= v4.17 */

static inline void mtd_prepare_erase_compat(struct erase_info *instr,
					    struct mtd_info *mtd,
					    struct mtd_erase_waiter *waiter)
{ }

static inline int mtd_erase_compat(struct mtd_info *mtd,
				   struct erase_info *instr,
				   struct mtd_erase_waiter *waiter)
{
	return mtd_erase(mtd, instr);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
#define get_task_state(t) READ_ONCE((t)->__state)
#else
#define get_task_state(t) ((t)->state)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#define SEND_SIGINFO_PID PIDTYPE_PID
#else
#define SEND_SIGINFO_PID false
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0)
typedef siginfo_t kernel_siginfo_t;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
static inline void *kvmalloc(size_t size, gfp_t gfp)
{
	unsigned int *p;

	p = kmalloc(size, GFP_KERNEL);

	if (!p && size >= PAGE_SIZE)
		p = vmalloc(size);

	return p;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
#define memblock_phys_free memblock_free
#endif

#endif /* _AVM_COMPAT_MISC_H_ */
