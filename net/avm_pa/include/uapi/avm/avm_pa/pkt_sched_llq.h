/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */

#ifndef AVM_PA_PKT_SCHED_LLQ_H
#define AVM_PA_PKT_SCHED_LLQ_H

/* Guard against kernels that have this stuff in pkt_sched.h still. */
#ifndef TCA_LLQ_MAX

#include <linux/types.h>

/* LLQ section */

enum {
	TCA_LLQ_UNSPEC,
	TCA_LLQ_OPTIONS,
	__TCA_LLQ_MAX,
};

#define TCA_LLQ_MAX (__TCA_LLQ_MAX - 1)

struct tc_llq_copt {
	__u8 priority;
	__u8 weight;
};

struct tc_llq_cinfo {
	__u8 priority;
	__u8 weight;
	long deficit;
	long quantum;
};

struct tc_llq_qopt {
	__s32 maxq; /* biggest possible quantum in classes */
	__s32 minq; /* smallest possible quantum in classes */
	__u32 defaultclass;
};

#endif /* TCA_LLQ_MAX */

#endif /* AVM_PA_PKT_SCHED_LLQ_H */
