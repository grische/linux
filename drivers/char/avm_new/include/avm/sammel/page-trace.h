/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __avm_page_trace_h__
#define __avm_page_trace_h__

struct page;

#if IS_ENABLED(CONFIG_AVM_PAGE_TRACE)
unsigned long avm_get_page_current_pc(struct page *page);
void avm_set_page_current_pc(struct page *page, unsigned long pc);
#else
static inline unsigned long avm_get_page_current_pc(struct page *page) { return 0; }
static inline void avm_set_page_current_pc(struct page *page, unsigned long pc) { }
#endif

#endif
