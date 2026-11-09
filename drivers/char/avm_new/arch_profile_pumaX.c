// SPDX-License-Identifier: GPL-2.0+

#include <linux/percpu.h>
#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>
#include <asm/mach_avm.h>
#include "avm_profile.h"
#include "arch_profile.h"

#if defined(PROFILING_PERFORMANCE_COUNTER_SUPPORT)

/*--- Liste aus ARM1176JZS - Technical Reference Manual ---*/
// clang-format off
const struct _perfcount_options performance_counter_options[PM_EVENT_LAST] = {
	[PM_EVENT_L1I_CACHE_MISS_SPEC]    = { name: "Level 1 instruction cache miss speculative",           norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1I_CACHE_STALL]        = { name: "Level 1 instruction buffer stall",                     norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_DATA_DEP_STALL]         = { name: "Data dependency stall",                                norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1I_TLB_MISS]           = { name: "Level 1 instruction TLB miss",                         norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_TLB_MISS]           = { name: "Level 1 data TLB miss",                                norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_BR_EXEC]                = { name: "Branch instruction executed",                          norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_BR_MIS_PRED]            = { name: "Branch instruction mispredicted",                      norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_INST_EXEC]              = { name: "Instructions executed",                                norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHABLE_ACCESS]    = { name: "Data cache access cachable",                           norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE_ACCESS]       = { name: "Data cache access all",                                norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE_MISS]         = { name: "Data cache miss",                                      norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE_WB]           = { name: "Data cache write back",                                norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_PC_WRITE]               = { name: "Software change of PC",                                norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_MAIN_TLB_MISS]          = { name: "Main TLB miss",                                        norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_EXT_DATA_ACCESS]        = { name: "Explicit external data access",                        norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_LDST_UNIT_STALL]        = { name: "Load Store Unit stall",                                norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_WB_DRAIN]               = { name: "Write buffer drained",                                 norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_ETM_EXT_OUT0]           = { name: "ETM Ext Out[0]",                                       norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_ETM_EXT_OUT1]           = { name: "ETM Ext Out[1]",                                       norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_ETM_EXT_OUT01]          = { name: "ETM Ext Out[0,1]",                                     norm_factor: NORMED_BY_CYCLE       },
	[PM_EVENT_CALL_EXEC]              = { name: "Procedure call executed",                              norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_RETURN_EXEC]            = { name: "Procedure return executed",                            norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_RETURN_EXEC_PRED_VAL]   = { name: "Procedure return executed with prediction",            norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_RETURN_EXEC_PRED_INVAL] = { name: "Procedure return executed with invalid prediction",    norm_factor: NORMED_BY_INSTRUCTION },
	[PM_EVENT_CPU_CYCLES]             = { name: "Cycle Counter",                                        norm_factor: NORMED_BY_FREQUENCY   }, /* Only valid for cycle counter, not for normal performance counters */
};
// clang-format on

#if defined(PROFILING_IN_FIQ)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
// clang-format off
const struct _roundrobin_perf_ctrlstat roundrobin_performance[] =  {
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1I_CACHE_MISS_SPEC    },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1I_CACHE_STALL        },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_DATA_DEP_STALL         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1I_TLB_MISS           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1D_TLB_MISS           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_BR_EXEC                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_BR_MIS_PRED            },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1D_CACHABLE_ACCESS    },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1D_CACHE_ACCESS       },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1D_CACHE_MISS         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_L1D_CACHE_WB           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_PC_WRITE               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_MAIN_TLB_MISS          },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_EXT_DATA_ACCESS        },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_LDST_UNIT_STALL        },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_WB_DRAIN               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_ETM_EXT_OUT0           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_ETM_EXT_OUT1           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_ETM_EXT_OUT01          },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_CALL_EXEC              },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_RETURN_EXEC            },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_RETURN_EXEC_PRED_VAL   },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[1] = PM_EVENT_INST_EXEC, .perf_ctrl[0] = PM_EVENT_RETURN_EXEC_PRED_INVAL },
};
// clang-format on

unsigned int array_size_roundrobin_performance(void)
{
	return ARRAY_SIZE(roundrobin_performance);
}
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/

#endif /*--- #if defined(PROFILING_PERFORMANCE_COUNTER_SUPPORT) ---*/
