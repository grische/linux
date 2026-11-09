/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2016 AVM GmbH <fritzbox_info@avm.de>
 *
 *   author: mbahr@avm.de
 *   description: yield-thread-interface mips34k
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#ifndef __yield_context_os_h__
#define __yield_context_os_h__

#if defined(CONFIG_AVM_IPI_YIELD)
#include <linux/bug.h>
#include <linux/thread_info.h>
#include <asm/mipsmtregs.h>

#define YIELD_LAST_TC 5
#define YIELD_FIRST_TC 2
#define YIELD_MAX_TC (YIELD_LAST_TC - YIELD_FIRST_TC + 1)
/*--- #define YIELD_MASK_TC               (~(0xFFFFFFFF << (YIELD_LAST_TC + 1)) & (0xFFFFFFFF << YIELD_FIRST_TC)) ---*/

/*--------------------------------------------------------------------------------*\
 * performante Ermittlung ob im Linux-Kontext
\*--------------------------------------------------------------------------------*/
static inline int yield_is_linux_context(void)
{
	/*--- 	unsigned int act_tc; ---*/
	/*--- 	act_tc = (read_c0_tcbind() & TCBIND_CURTC) >> TCBIND_CURTC_SHIFT; ---*/
	/*--- 	return !((act_tc >= YIELD_FIRST_TC) && (act_tc <= YIELD_LAST_TC));   ---*/ /*---  5 Assembler-Befehle ---*/
	/*--- 	return ((1 << act_tc) & YIELD_MASK_TC) ? 0 : 1; ---*/ /*--- 7 Assembler-Befehle ---*/
	return !test_thread_flag(
		TIF_YIELDCONTEXT); /*--- 3 Assembler-Befehle ---*/
}

/**
 */
static inline int yield_is_avm_rte(void)
{
	return test_thread_flag(TIF_YIELDCONTEXT);
}

/*--------------------------------------------------------------------------------*\
 * tell me if we in any yield instead linux context
 * ret: 0 no yield-context
 *      <any> - 1: thread-number
\*--------------------------------------------------------------------------------*/
extern int is_yield_context(void);
#else
#define yield_is_linux_context() 1
#define yield_is_avm_rte() 0
#define is_avm_rte() 0
#define is_yield_context() 0
#endif

#define BUG_ON_YIELDTHREAD_NOT_ALLOWED() BUG_ON(!yield_is_linux_context())

#endif /* __yield_context_debug_h__ */
