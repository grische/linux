// SPDX-License-Identifier: GPL-2.0+
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/semaphore.h>
#pragma GCC pop_options
#include "capi_events.h"

/**
 */
int Capi_Set_Events(struct capi_events *E __attribute((unused)),
		    unsigned int SetEvents __attribute((unused)), int AndOr __attribute((unused)))
{
	return 0;
}

