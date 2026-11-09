/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note) OR BSD-2-Clause */

#ifndef AVM_PA_IOCTL_H
#define AVM_PA_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define AVM_PA_IOCTL_NUM 0xAA
#define AVM_PA_IOCTL_SEQ 0xC0

struct avm_pa_ioctl_perf_test {
	__u32 test_id;
	__u32 h_len;
	__u8  h_buf[248];
};

struct avm_pa_ioctl_perf_result {
	__u32 version;
	__u32 test_id;
	__u64 call_count;
};

#define AVM_PA_PERF_START         _IO(AVM_PA_IOCTL_NUM, AVM_PA_IOCTL_SEQ + 20)
#define AVM_PA_PERF_RECV        _IOWR(AVM_PA_IOCTL_NUM, AVM_PA_IOCTL_SEQ + 21, struct avm_pa_ioctl_perf_test)
#define AVM_PA_PERF_FORW        _IOWR(AVM_PA_IOCTL_NUM, AVM_PA_IOCTL_SEQ + 22, struct avm_pa_ioctl_perf_result)

#endif /* AVM_PA_IOCTL_H */
