/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _AVM_COMPAT_TIMER_H_
#define _AVM_COMPAT_TIMER_H_
#include <linux/timer.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
static inline time64_t __ktime_get_real_seconds(void)
{
	return get_seconds();
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
typedef void (*timer_func_t)(unsigned long);
#ifndef from_timer
#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)
#endif

#ifndef timer_setup
/*
 * Unfortunately, some AVM kernel sub-projects (telephony...) enable
 * -Wextra, which implies -Wcast-function-type for gcc >= v8
 */
#pragma GCC push_options
#if __GNUC__ >= 8
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#define timer_setup timer_setup
static inline void timer_setup(struct timer_list *timer,
			       void (*func)(struct timer_list *),
			       unsigned int flags)
{
	__setup_timer(timer, (timer_func_t)func, (unsigned long)timer, flags);
}
#pragma GCC pop_options
#endif /* timer_setup */

#else  /* KERNEL_VERSION */
typedef void (*timer_func_t)(struct timer_list *);
#endif

#endif /* _AVM_COMPAT_TIMER_H_ */
