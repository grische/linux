/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_HELPERS_H_
#include <linux/version.h>
#include <linux/ktime.h>
#include <linux/timer.h>

#define LINUX_STABLE_VERSION_HAS_SUBLEVEL(a, b, c) \
	(LINUX_VERSION_CODE < KERNEL_VERSION(a, (b) + 1, 0) && \
	 LINUX_VERSION_CODE >= KERNEL_VERSION(a, b, c))

/* Return:
 *	< 0: Error (-EIO, -EPERM, -EINVAL)
 *	= 0: Empty file
 *	> 0: Number of bytes read
 */
int avm_read_from_file(const char *filename, char *str, int maxlen);

/* Return:
 *	< 0: Error (-EIO, -EPERM, -EINVAL)
 *	= 0: Success
 */
int avm_write_to_file(const char *filename, const char *str, int len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
typedef __s64 time64_t;

static inline time64_t ktime_get_real_seconds(void)
{
	return get_seconds();
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
static inline time64_t __ktime_get_real_seconds(void)
{
	return get_seconds();
}
#endif

/* On old kernels that don't have it, we emulate ktime_get_seconds. */
time64_t ktime_get_seconds(void);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
typedef void (*timer_func_t)(unsigned long);
#ifndef from_timer
#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)
#endif

#ifndef timer_setup
#ifndef __setup_timer
   /* Ancient kernels << v3.7 (puma6...), no support for timer flags */
#  define __setup_timer(_timer, _fn, _data, _flags)	\
	do {						\
		BUILD_BUG_ON(_flags);			\
		setup_timer((_timer), (_fn), (_data));	\
	} while (0)
#endif
/*
 * Unfortunately, some AVM kernel sub-projects (telephony...) enable
 * -Wextra, which implies -Wcast-function-type for gcc >= v8
 */
#define timer_setup timer_setup
static inline void timer_setup(struct timer_list *timer,
			       void (*func)(struct timer_list *),
			       unsigned int flags)
{
	__setup_timer(timer, (timer_func_t)func, (unsigned long)timer, flags);
}
#endif /* timer_setup */

#else  /* KERNEL_VERSION */
typedef void (*timer_func_t)(struct timer_list *);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
#  define PTR_ERR_OR_ZERO PTR_RET
#endif

ssize_t time_to_ascii(time64_t local_time, char *buf, int len);

#define _AVM_HELPERS_H_
#endif /* _AVM_HELPERS_H */
