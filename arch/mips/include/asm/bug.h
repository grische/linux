#ifndef __ASM_BUG_H
#define __ASM_BUG_H

#include <linux/compiler.h>
#include <asm/sgidefs.h>

#ifdef CONFIG_BUG

#include <asm/break.h>

#ifdef CONFIG_BUG_EXTRA_INFO
#define BUG() \
( { \
	__asm__ __volatile__("1:  \n" \
			"    break %0\n" \
			"   .section __bug_debug_table, \"a\"\n" \
			"   .word 1b\n" \
			"   .word %1\n" \
			"   .word %2\n" \
			"   .word %3\n" \
			"   .word 0\n" \
			"   .previous\n" \
			: \
			: "i"(BRK_BUG), "i"(__FILE__), "i"(__LINE__), "i"(__FUNCTION__)); \
	unreachable(); \
} )
#else
static inline void __noreturn BUG(void)
{
	__asm__ __volatile__("break %0" : : "i" (BRK_BUG));
	unreachable();
}
#endif

#define HAVE_ARCH_BUG

#if (_MIPS_ISA > _MIPS_ISA_MIPS1)

#ifdef CONFIG_BUG_EXTRA_INFO
#define  __BUG_ON(condition) \
( { \
	if (__builtin_constant_p(condition)) { \
		if (condition) { \
			BUG(); \
		} \
	} else { \
		__asm__ __volatile__("1:  \n" \
				"   tne $0, %0, %1\n" \
				"   .section __bug_debug_table, \"a\"\n" \
				"   .word 1b\n" \
				"   .word %2\n" \
				"   .word %3\n" \
				"   .word %4\n" \
				"   .word 0\n" \
				"   .previous\n" \
				: \
				: "r"(condition), "i"(BRK_BUG), "i"(__FILE__), "i"(__LINE__), "i"(__FUNCTION__)); \
	} \
} )
#else
static inline void  __BUG_ON(unsigned long condition)
{
	if (__builtin_constant_p(condition)) {
		if (condition)
			BUG();
		else
			return;
	}
	__asm__ __volatile__("tne $0, %0, %1"
			     : : "r" (condition), "i" (BRK_BUG));
}
#endif

#define BUG_ON(C) __BUG_ON((unsigned long)(C))

#define HAVE_ARCH_BUG_ON

#endif /* _MIPS_ISA > _MIPS_ISA_MIPS1 */

#endif

#include <asm-generic/bug.h>

#endif /* __ASM_BUG_H */
