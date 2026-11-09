// SPDX-License-Identifier: GPL-2.0+
/**
 */
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>
#include <asm/mach_avm.h>
#include <asm/performance.h>

#include "avm_profile.h"
#include "arch_profile.h"

/*--- List taken from Cortex -A7 MPCore Revision: r0p5 ARM DDI 0464F (ID051113) ---*/
/*--- http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0464f/index.html ---*/
#if defined(PROFILING_IN_FIQ)
// clang-format off
const struct _perfcount_options performance_counter_options[PM_EVENT_LAST] = {
	[PM_EVENT_SW_INCR]                = { .name = "software increment",                                   .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_L1I_CACHE_REFILL]       = { .name = "Level 1 instruction cache refill",                     .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L1I_TLB_REFILL]         = { .name = "Level 1 instruction TLB refill",                       .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE_REFILL]       = { .name = "Level 1 data cache refill",                            .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE]              = { .name = "Level 1 data cache access",                            .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_TLB_REFILL]         = { .name = "Level 1 data TLB refill",                              .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_LD_RETIRED]             = { .name = "Data read  architecturally executed",                  .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_ST_RETIRED]             = { .name = "Data write architecturally executed",                  .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_INST_RETIRED]           = { .name = "Instruction architecturally executed",                 .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_EXC_TAKEN]              = { .name = "Exception taken",                                      .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXC_RETURN]             = { .name = "Exception return architecturally executed",            .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_CID_WRITE_RETIRED]      = { .name = "Change to ContextID retired",                          .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_PC_WRITE_RETIRED]       = { .name = "Software change of PC",                                .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_BR_IMMED_RETIRED]       = { .name = "Immediate branch architecturally executed",            .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_BR_RETURN_RETIRED]      = { .name = "Procedure return architecturally executed",            .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_UNALIGNED_LDST_RETIRED] = { .name = "Unaligned load-store",                                 .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_BR_MIS_PRED]            = { .name = "Branch mispredicted/not predicted",                    .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_CPU_CYCLES]             = { .name = "Cycle Counter",                                        .norm_factor = NORMED_BY_FREQUENCY   }, /* Only valid for cycle counter, not for normal performance counters */
	[PM_EVENT_BR_PRED]                = { .name = "Predictable branch speculatively executed",            .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_MEM_ACCESS]             = { .name = "Data memory access",                                   .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L1I_CACHE]              = { .name = "Level 1 instruction cache access",                     .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L1D_CACHE_WB]           = { .name = "Level 1 data cache write-back",                        .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L2D_CACHE]              = { .name = "Level 2 data cache access",                            .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L2D_CACHE_REFILL]       = { .name = "Level 2 data cache refill",                            .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_L2D_CACHE_WB]           = { .name = "Level 2 data cache write-back",                        .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_BUS_ACCESS]             = { .name = "Bus access",                                           .norm_factor = NORMED_BY_FREQUENCY   },
	[PM_EVENT_BUS_CYCLES]             = { .name = "Bus cycle",                                            .norm_factor = NORMED_BY_FREQUENCY   },
	[PM_EVENT_BUS_ACCESS_LD]          = { .name = "Bus access, read",                                     .norm_factor = NORMED_BY_FREQUENCY   },
	[PM_EVENT_BUS_ACCESS_ST]          = { .name = "Bus access, write",                                    .norm_factor = NORMED_BY_FREQUENCY   },
	[PM_EVENT_EXC_IRQ]                = { .name = "Exception taken, IRQ",                                 .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXC_FIQ]                = { .name = "Exception taken, FIQ",                                 .norm_factor = NORMED_BY_INSTRUCTION },
	[PM_EVENT_EXT_MEM_REQ]            = { .name = "External memory request",                              .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_NON_CACHE_EXT_MEM_REQ]  = { .name = "Non-cacheable external memory request",                .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_LINEFILL_PREFETCH]      = { .name = "Linefill because of prefetch",                         .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_PREFETCH_LINEFILL_DROP] = { .name = "Prefetch linefill dropped",                            .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_ENTER_READ_ALLOC]       = { .name = "Entering read allocate mode",                          .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_READ_ALLOC_MODE]        = { .name = "Read allocate mode.",                                  .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_ETM_EXT_OUT0]           = { .name = "ETM Ext Out[0].",                                      .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_ETM_EXT_OUT1]           = { .name = "ETM Ext Out[1].",                                      .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_DATA_WRITE_STALL]       = { .name = "Data Write stalls pipeline because store buffer full", .norm_factor = NORMED_BY_CYCLE       },
	[PM_EVENT_DATA_SNOOPED]           = { .name = "Data snooped from other processor",                    .norm_factor = NORMED_BY_CYCLE       },
};
// clang-format on

// clang-format off
const struct _roundrobin_perf_ctrlstat roundrobin_performance[] =  {
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_L1I_CACHE_REFILL,  .perf_ctrl[1] = PM_EVENT_L1I_TLB_REFILL,         .perf_ctrl[2] = PM_EVENT_L1D_CACHE_REFILL         }, /*--- Stall Cycles, Cycles ---*/
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_L1D_CACHE,         .perf_ctrl[1] = PM_EVENT_L1D_TLB_REFILL,         .perf_ctrl[2] = PM_EVENT_LD_RETIRED               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_ST_RETIRED,        .perf_ctrl[1] = PM_EVENT_BUS_CYCLES,             .perf_ctrl[2] = PM_EVENT_EXC_TAKEN                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_EXC_RETURN,        .perf_ctrl[1] = PM_EVENT_CID_WRITE_RETIRED,      .perf_ctrl[2] = PM_EVENT_PC_WRITE_RETIRED         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_BR_IMMED_RETIRED,  .perf_ctrl[1] = PM_EVENT_BR_RETURN_RETIRED,      .perf_ctrl[2] = PM_EVENT_UNALIGNED_LDST_RETIRED   },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_BR_MIS_PRED,       .perf_ctrl[1] = PM_EVENT_BR_PRED,                .perf_ctrl[2] = PM_EVENT_MEM_ACCESS               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_L1I_CACHE,         .perf_ctrl[1] = PM_EVENT_L1D_CACHE_WB,           .perf_ctrl[2] = PM_EVENT_L2D_CACHE                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_L2D_CACHE_REFILL,  .perf_ctrl[1] = PM_EVENT_L2D_CACHE_WB,           .perf_ctrl[2] = PM_EVENT_BUS_ACCESS               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_BUS_ACCESS_LD,     .perf_ctrl[1] = PM_EVENT_BUS_ACCESS_ST,          .perf_ctrl[2] = PM_EVENT_EXC_IRQ                  },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_EXC_FIQ,           .perf_ctrl[1] = PM_EVENT_EXT_MEM_REQ,            .perf_ctrl[2] = PM_EVENT_NON_CACHE_EXT_MEM_REQ    },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_LINEFILL_PREFETCH, .perf_ctrl[1] = PM_EVENT_PREFETCH_LINEFILL_DROP, .perf_ctrl[2] = PM_EVENT_ENTER_READ_ALLOC         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_READ_ALLOC_MODE,   .perf_ctrl[1] = PM_EVENT_ETM_EXT_OUT0,           .perf_ctrl[2] = PM_EVENT_ETM_EXT_OUT1             },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[0] = PM_EVENT_DATA_WRITE_STALL,  .perf_ctrl[1] = PM_EVENT_DATA_SNOOPED,           .perf_ctrl[2] = PM_EVENT_SW_INCR                  },
	/*--- um alle Performance-Counter in Profile-Mode zu tracen (Reg0 und Reg1 reserviert): Reg0 -> Reg2 ---*/
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_L1I_CACHE_REFILL,  .perf_ctrl[0] = PM_EVENT_L1I_TLB_REFILL,         .perf_ctrl[1] = PM_EVENT_L1D_CACHE_REFILL         }, /*--- Stall Cycles, Cycles ---*/
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_L1D_CACHE,         .perf_ctrl[0] = PM_EVENT_L1D_TLB_REFILL,         .perf_ctrl[1] = PM_EVENT_LD_RETIRED               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_ST_RETIRED,        .perf_ctrl[0] = PM_EVENT_BUS_CYCLES,             .perf_ctrl[1] = PM_EVENT_EXC_TAKEN                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_EXC_RETURN,        .perf_ctrl[0] = PM_EVENT_CID_WRITE_RETIRED,      .perf_ctrl[1] = PM_EVENT_PC_WRITE_RETIRED         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_BR_IMMED_RETIRED,  .perf_ctrl[0] = PM_EVENT_BR_RETURN_RETIRED,      .perf_ctrl[1] = PM_EVENT_UNALIGNED_LDST_RETIRED   },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_BR_MIS_PRED,       .perf_ctrl[0] = PM_EVENT_BR_PRED,                .perf_ctrl[1] = PM_EVENT_MEM_ACCESS               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_L1I_CACHE,         .perf_ctrl[0] = PM_EVENT_L1D_CACHE_WB,           .perf_ctrl[1] = PM_EVENT_L2D_CACHE                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_L2D_CACHE_REFILL,  .perf_ctrl[0] = PM_EVENT_L2D_CACHE_WB,           .perf_ctrl[1] = PM_EVENT_BUS_ACCESS               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_BUS_ACCESS_LD,     .perf_ctrl[0] = PM_EVENT_BUS_ACCESS_ST,          .perf_ctrl[1] = PM_EVENT_EXC_IRQ                  },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_EXC_FIQ,           .perf_ctrl[0] = PM_EVENT_EXT_MEM_REQ,            .perf_ctrl[1] = PM_EVENT_NON_CACHE_EXT_MEM_REQ    },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_LINEFILL_PREFETCH, .perf_ctrl[0] = PM_EVENT_PREFETCH_LINEFILL_DROP, .perf_ctrl[1] = PM_EVENT_ENTER_READ_ALLOC         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_READ_ALLOC_MODE,   .perf_ctrl[0] = PM_EVENT_ETM_EXT_OUT0,           .perf_ctrl[1] = PM_EVENT_ETM_EXT_OUT1             },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[2] = PM_EVENT_DATA_WRITE_STALL,  .perf_ctrl[0] = PM_EVENT_DATA_SNOOPED,           .perf_ctrl[1] = PM_EVENT_SW_INCR                  },
	/*--- um alle Performance-Counter in Profile-Mode zu tracen (Reg0 und Reg1 reserviert): Reg1 -> Reg2 ---*/
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_L1I_CACHE_REFILL,  .perf_ctrl[2] = PM_EVENT_L1I_TLB_REFILL,         .perf_ctrl[0] = PM_EVENT_L1D_CACHE_REFILL         }, /*--- Stall Cycles, Cycles ---*/
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_L1D_CACHE,         .perf_ctrl[2] = PM_EVENT_L1D_TLB_REFILL,         .perf_ctrl[0] = PM_EVENT_LD_RETIRED               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_ST_RETIRED,        .perf_ctrl[2] = PM_EVENT_BUS_CYCLES,             .perf_ctrl[0] = PM_EVENT_EXC_TAKEN                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_EXC_RETURN,        .perf_ctrl[2] = PM_EVENT_CID_WRITE_RETIRED,      .perf_ctrl[0] = PM_EVENT_PC_WRITE_RETIRED         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_BR_IMMED_RETIRED,  .perf_ctrl[2] = PM_EVENT_BR_RETURN_RETIRED,      .perf_ctrl[0] = PM_EVENT_UNALIGNED_LDST_RETIRED   },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_BR_MIS_PRED,       .perf_ctrl[2] = PM_EVENT_BR_PRED,                .perf_ctrl[0] = PM_EVENT_MEM_ACCESS               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_L1I_CACHE,         .perf_ctrl[2] = PM_EVENT_L1D_CACHE_WB,           .perf_ctrl[0] = PM_EVENT_L2D_CACHE                },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_L2D_CACHE_REFILL,  .perf_ctrl[2] = PM_EVENT_L2D_CACHE_WB,           .perf_ctrl[0] = PM_EVENT_BUS_ACCESS               },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_BUS_ACCESS_LD,     .perf_ctrl[2] = PM_EVENT_BUS_ACCESS_ST,          .perf_ctrl[0] = PM_EVENT_EXC_IRQ                  },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_EXC_FIQ,           .perf_ctrl[2] = PM_EVENT_EXT_MEM_REQ,            .perf_ctrl[0] = PM_EVENT_NON_CACHE_EXT_MEM_REQ    },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_LINEFILL_PREFETCH, .perf_ctrl[2] = PM_EVENT_PREFETCH_LINEFILL_DROP, .perf_ctrl[0] = PM_EVENT_ENTER_READ_ALLOC         },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_READ_ALLOC_MODE,   .perf_ctrl[2] = PM_EVENT_ETM_EXT_OUT0,           .perf_ctrl[0] = PM_EVENT_ETM_EXT_OUT1             },
	{.prefix = "", .perf_ctrl[PROFILING_CYCLE_REGISTER] = PM_EVENT_CPU_CYCLES, .perf_ctrl[3] = PM_EVENT_INST_RETIRED, .perf_ctrl[1] = PM_EVENT_DATA_WRITE_STALL,  .perf_ctrl[2] = PM_EVENT_DATA_SNOOPED,           .perf_ctrl[0] = PM_EVENT_SW_INCR                  },
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
