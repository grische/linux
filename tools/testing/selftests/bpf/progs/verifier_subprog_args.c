// SPDX-License-Identifier: GPL-2.0

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

/*
 * BPF-to-BPF calls pass all five argument registers and the return value
 * with their full 64 bits. The subprograms below only use some of them, so
 * that JITs are free to use the other argument registers for something else.
 */

__naked __noinline __used
static unsigned long ret_r1(void)
{
	asm volatile (
	"r0 = r1;"
	"exit;"
	);
}

__naked __noinline __used
static unsigned long ret_const(void)
{
	asm volatile (
	"r0 = 0xa1a2a3a4b1b2b3b4 ll;"
	"exit;"
	);
}

__naked __noinline __used
static unsigned long ret_r5(void)
{
	asm volatile (
	"r0 = r5;"
	"exit;"
	);
}

__naked __noinline __used
static unsigned long sum_r1_r5(void)
{
	asm volatile (
	"r0 = r1;"
	"r0 += r2;"
	"r0 += r3;"
	"r0 += r4;"
	"r0 += r5;"
	"exit;"
	);
}

/* Passes its arguments on without touching them */
__naked __noinline __used
static unsigned long pass_r1_r5(void)
{
	asm volatile (
	"r6 = 3;"
	"r6 *= 3;"
	"call sum_r1_r5;"
	"r0 += r6;"
	"exit;"
	);
}

SEC("socket")
__description("subprog: 64-bit first argument")
__success __retval(0)
__naked void subprog_arg1_64bit(void)
{
	asm volatile (
	"r1 = 0x0102030405060708 ll;"
	"call ret_r1;"
	"r1 = 0x0102030405060708 ll;"
	"r0 ^= r1;"
	"r1 = r0;"
	"r1 >>= 32;"
	"r0 |= r1;"
	"exit;"
	);
}

SEC("socket")
__description("subprog: 64-bit return value")
__success __retval(0)
__naked void subprog_ret_64bit(void)
{
	asm volatile (
	"call ret_const;"
	"r1 = 0xa1a2a3a4b1b2b3b4 ll;"
	"r0 ^= r1;"
	"r1 = r0;"
	"r1 >>= 32;"
	"r0 |= r1;"
	"exit;"
	);
}

SEC("socket")
__description("subprog: fifth argument")
__success __retval(0)
__naked void subprog_arg5(void)
{
	asm volatile (
	"r5 = 0x2122232425262728 ll;"
	"call ret_r5;"
	"r1 = 0x2122232425262728 ll;"
	"r0 ^= r1;"
	"r1 = r0;"
	"r1 >>= 32;"
	"r0 |= r1;"
	"exit;"
	);
}

SEC("socket")
__description("subprog: five arguments")
__success __retval(0)
__naked void subprog_args_all(void)
{
	asm volatile (
	"r1 = 0x0102030405060708 ll;"
	"r2 = 0x1020304050607080 ll;"
	"r3 = 0x0a0b0c0d0e0f1011 ll;"
	"r4 = 0x1112131415161718 ll;"
	"r5 = 0x2122232425262728 ll;"
	"call sum_r1_r5;"
	"r1 = 0x4d6175899db1c5d9 ll;"
	"r0 ^= r1;"
	"r1 = r0;"
	"r1 >>= 32;"
	"r0 |= r1;"
	"exit;"
	);
}

SEC("socket")
__description("subprog: arguments passed on")
__success __retval(0)
__naked void subprog_args_passed_on(void)
{
	asm volatile (
	"r1 = 0x0102030405060708 ll;"
	"r2 = 0x1020304050607080 ll;"
	"r3 = 0x0a0b0c0d0e0f1011 ll;"
	"r4 = 0x1112131415161718 ll;"
	"r5 = 0x2122232425262728 ll;"
	"call pass_r1_r5;"
	"r1 = 0x4d6175899db1c5e2 ll;"
	"r0 ^= r1;"
	"r1 = r0;"
	"r1 >>= 32;"
	"r0 |= r1;"
	"exit;"
	);
}

char _license[] SEC("license") = "GPL";
