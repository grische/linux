// SPDX-License-Identifier: GPL-2.0+

#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>
#include <asm/mach_avm.h>
#include <asm/performance.h>

#include "avm_profile.h"
#include "arch_profile.h"

#if defined(PROFILING_IN_FIQ)
/*--- Liste aus Cortex-A9 Technical Reference Manual, ARM DDI0388I_cortex_a9_r4p1_trm.pdf ---*/
// clang-format off
const struct _perfcount_options performance_counter_options[PM_EVENT_LAST] = {
	[PM_EVENT_SW_INCR]                = { .name = "software increment",                                   .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_L1I_CACHE_REFILL]       = { .name = "Level 1 instruction cache refill",                     .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_L1I_TLB_REFILL]         = { .name = "Level 1 instruction TLB refill",                       .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE_REFILL]       = { .name = "Level 1 data cache refill",                            .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE]              = { .name = "Level 1 data cache access",                            .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_TLB_REFILL]         = { .name = "Level 1 data TLB refill",                              .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_LD_RETIRED]             = { .name = "Data read  architecturally executed",                  .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_ST_RETIRED]             = { .name = "Data write architecturally executed",                  .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXC_TAKEN]              = { .name = "Exception taken",                                      .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXC_RETURN]             = { .name = "Exception return architecturally executed",            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_CID_WRITE_RETIRED]      = { .name = "Change to ContextID retired",                          .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_PC_WRITE_RETIRED]       = { .name = "Software change of PC",                                .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_BR_IMMED_RETIRED]       = { .name = "Immediate branch architecturally executed",            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_UNALIGNED_LDST_RETIRED] = { .name = "Unaligned load-store",                                 .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_BR_MIS_PRED]            = { .name = "Branch mispredicted/not predicted",                    .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_CPU_CYCLES]             = { .name = "Cycle Counter",                                        .norm_factor =  NORMED_BY_FREQUENCY   }, // Only valid for cycle counter, not for normal performance counters
	[PM_EVENT_BR_PRED]                = { .name = "Predictable branch speculatively executed",            .norm_factor =  NORMED_BY_INSTRUCTION },

	[PM_EVENT_COH_LINEFILL_MISS]      = { .name = "Coherent linefill miss",                               .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_COH_LINEFILL_HIT]       = { .name = "Coherent linefill hit",                                .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_L1I_DEP_STALL]          = { .name = "Level 1 instruction cache dependent stall",            .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_DEP_STALL]          = { .name = "Level 1 data cache dependent stall",                   .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_MAIN_TLB_STALL]         = { .name = "Main TLB miss stall",                                  .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_STREX_PASS_PREC]        = { .name = "STREX architecturally executed and passed",            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_STREX_FAIL_PREC]        = { .name = "STREX architecturally executed and failed",            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_L1D_EVICTION]           = { .name = "Data eviction",                                        .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_ISSUE_STAGE_STALL]      = { .name = "Issue stage stall",                                    .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_ISSUE_STAGE_EMPTY]      = { .name = "Issue stage empty",                                    .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_INST_RETIRED]           = { .name = "Instructions going through register renaming stage",   .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXT_LINEFILL_REQ]       = { .name = "External linefill requests",                           .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PREFETCH_REFILL]        = { .name = "Prefetch line refill requests",                        .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PREFETCH_HIT]           = { .name = "Prefetch cache hits",                                  .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_RETURN_EXEC_PRED_VAL]   = { .name = "Predictable function returns",                         .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_MAIN_EXEC_UNIT_INST]    = { .name = "Main execution unit instructions",                     .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_SECOND_EXEC_UNIT_INST]  = { .name = "Second execution unit instructions",                   .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_LDST_SPEC]              = { .name = "Load/store unit instructions",                         .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_FP_EXEC_INST]           = { .name = "Floating point instructions",                          .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_NEON_EXEC_INST]         = { .name = "NEON instructions",                                    .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_PLD_STALL]              = { .name = "PLD stall",                                            .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_EXT_MEM_WRITE_STALL]    = { .name = "External memory write stall",                          .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_MAIN_TLB_INST_STALL]    = { .name = "Instruction side main TLB miss",                       .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_MAIN_TLB_DATA_STALL]    = { .name = "Data side main TLB miss",                              .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_MICRO_TLB_INST_STALL]   = { .name = "Instruction side micro TLB miss",                      .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_MICRO_TLB_DATA_STALL]   = { .name = "Data side micro TLB miss",                             .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_DMB_STALL_SPEC]         = { .name = "DMB stall",                                            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_INTEGER_CLOCK_EN]       = { .name = "Integer core clock enabled",                           .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_DATA_ENGINE_CLOCK_EN]   = { .name = "Data engine clock enabled",                            .norm_factor =  NORMED_BY_CYCLE       },
	[PM_NEON_SIMD_CLOCK_EN]           = { .name = "NEON SIMD clock enabled",                              .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_TLB_INST_ALLOC]         = { .name = "Instruction TLB allocation",                           .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_TLB_DATA_ALLOC]         = { .name = "Data TLB allocation",                                  .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_ISB_PREC]               = { .name = "ISB instructions architecturally executed",            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_DSB_PREC]               = { .name = "DSB instructions architecturally executed",            .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_DMB_SPEC]               = { .name = "DMB instructions speculatively executed",              .norm_factor =  NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXT_IRQ]                = { .name = "External interrupts",                                  .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PLE_CACHE_REQ_COMPL]    = { .name = "PLE cache line requests completed",                    .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PLE_CACHE_REQ_SKIP]     = { .name = "PLE cache line requests skipped",                      .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PLE_FIFO_FLUSH]         = { .name = "PLE Fifo flush",                                       .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PLE_REQ_COMPL]          = { .name = "PLE request completed",                                .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PLE_FIFO_OVERFL]        = { .name = "PLE Fifo overflow",                                    .norm_factor =  NORMED_BY_CYCLE       },
	[PM_EVENT_PLE_REQ_PROG]           = { .name = "PLE request programmed",                               .norm_factor =  NORMED_BY_CYCLE       },
};
// clang-format on

/**
 */
// clang-format off
const struct _roundrobin_perf_ctrlstat roundrobin_performance[] =  {
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_SW_INCR,           .perf_ctrl[1] = PM_EVENT_L1I_CACHE_REFILL,       .perf_ctrl[0] = PM_EVENT_L1I_TLB_REFILL           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_L1D_CACHE_REFILL,  .perf_ctrl[1] = PM_EVENT_L1D_CACHE,              .perf_ctrl[0] = PM_EVENT_L1D_TLB_REFILL           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_LD_RETIRED,        .perf_ctrl[1] = PM_EVENT_ST_RETIRED,             .perf_ctrl[0] = PM_EVENT_EXC_TAKEN                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_EXC_RETURN,        .perf_ctrl[1] = PM_EVENT_CID_WRITE_RETIRED,      .perf_ctrl[0] = PM_EVENT_PC_WRITE_RETIRED         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_BR_IMMED_RETIRED,  .perf_ctrl[1] = PM_EVENT_UNALIGNED_LDST_RETIRED, .perf_ctrl[0] = PM_EVENT_BR_MIS_PRED              },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_BR_PRED,           .perf_ctrl[1] = PM_EVENT_COH_LINEFILL_MISS,      .perf_ctrl[0] = PM_EVENT_COH_LINEFILL_HIT         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_L1I_DEP_STALL,     .perf_ctrl[1] = PM_EVENT_L1D_DEP_STALL,          .perf_ctrl[0] = PM_EVENT_MAIN_TLB_STALL           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_STREX_PASS_PREC,   .perf_ctrl[1] = PM_EVENT_STREX_FAIL_PREC,        .perf_ctrl[0] = PM_EVENT_L1D_EVICTION             },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_ISSUE_STAGE_STALL, .perf_ctrl[1] = PM_EVENT_ISSUE_STAGE_EMPTY,      .perf_ctrl[0] = PM_EVENT_EXT_LINEFILL_REQ         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_PREFETCH_REFILL,   .perf_ctrl[1] = PM_EVENT_PREFETCH_HIT,           .perf_ctrl[0] = PM_EVENT_RETURN_EXEC_PRED_VAL     },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_MAIN_EXEC_UNIT_INST, .perf_ctrl[1] = PM_EVENT_SECOND_EXEC_UNIT_INST, .perf_ctrl[0] = PM_EVENT_LDST_SPEC               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_FP_EXEC_INST,      .perf_ctrl[1] = PM_EVENT_NEON_EXEC_INST,         .perf_ctrl[0] = PM_EVENT_PLD_STALL                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_EXT_MEM_WRITE_STALL, .perf_ctrl[1] = PM_EVENT_MAIN_TLB_INST_STALL,  .perf_ctrl[0] = PM_EVENT_MAIN_TLB_DATA_STALL      },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_MICRO_TLB_INST_STALL, .perf_ctrl[1] = PM_EVENT_MICRO_TLB_DATA_STALL, .perf_ctrl[0] = PM_EVENT_DMB_STALL_SPEC          },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_INTEGER_CLOCK_EN,  .perf_ctrl[1] = PM_EVENT_DATA_ENGINE_CLOCK_EN,   .perf_ctrl[0] = PM_NEON_SIMD_CLOCK_EN             },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_TLB_INST_ALLOC,    .perf_ctrl[1] = PM_EVENT_TLB_DATA_ALLOC,         .perf_ctrl[0] = PM_EVENT_ISB_PREC                 },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_DSB_PREC,          .perf_ctrl[1] = PM_EVENT_DMB_SPEC,               .perf_ctrl[0] = PM_EVENT_EXT_IRQ                  },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_PLE_CACHE_REQ_COMPL, .perf_ctrl[1] = PM_EVENT_PLE_CACHE_REQ_SKIP,   .perf_ctrl[0] = PM_EVENT_PLE_FIFO_FLUSH           },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_PLE_REQ_COMPL,     .perf_ctrl[1] = PM_EVENT_PLE_FIFO_OVERFL,        .perf_ctrl[0] = PM_EVENT_PLE_REQ_PROG             },

};
// clang-format on

unsigned int array_size_roundrobin_performance(void)
{
	return ARRAY_SIZE(roundrobin_performance);
}

#else /*--- #if defined(PROFILING_IN_FIQ) ---*/
const struct _perfcount_options performance_counter_options[PM_EVENT_LAST] = {
	[0] = { .name = NULL, .norm_factor = NORMED_MAX }
};
#endif /*--- #if defined(PROFILING_IN_FIQ) ---*/
