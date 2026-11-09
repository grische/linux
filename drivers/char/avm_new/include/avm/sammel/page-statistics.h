/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __avm_page_statistics_h__
#define __avm_page_statistics_h__

#if IS_ENABLED(CONFIG_AVM_PAGE_STATISTICS)
void show_avm_page_statistic(bool complete);
#else
static void show_avm_page_statistic(bool complete) { (void)complete; }
#endif

#endif
