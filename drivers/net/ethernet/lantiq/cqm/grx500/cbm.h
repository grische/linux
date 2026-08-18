/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.h.
 *
 * CBM register overlays, descriptor layout and the intra-module function
 * surface shared by the cbm_*.c translation units.
 */

#ifndef __XRX500_CBM_H
#define __XRX500_CBM_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/build_bug.h>
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/bitops.h>

#include "cbm_regs.h"
#include "../../datapath/lantiq_cbm_api.h"

struct cbm_dp_en_data;
struct net_device;
struct cbm_dp_alloc_data;

/*
 * struct cbm_desc — CBM/TMU 16-byte descriptor.
 *
 * Verbatim from AVM include/net/lantiq_cbm_api.h:1460-1465.
 */
#ifndef __CBM_DESC_DEFINED
#define __CBM_DESC_DEFINED
struct cbm_desc {
	u32 desc0;
	u32 desc1;
	u32 desc2;
	u32 desc3;
};
#endif

/*
 * struct cbm_int_reg — interrupt-line MMIO overlay.
 *
 * Verbatim from AVM cbm.h:442-454. Offsets:
 *   cbm_irncr  = 0x00   cbm_irnicr = 0x04   cbm_irnen  = 0x08
 *   resv0[1]   = 0x0C
 *   igp_irncr  = 0x10   igp_irnicr = 0x14   igp_irnen  = 0x18
 *   resv1[1]   = 0x1C
 *   egp_irncr  = 0x20   egp_irnicr = 0x24   egp_irnen  = 0x28
 */
struct cbm_int_reg {
	u32 cbm_irncr;
	u32 cbm_irnicr;
	u32 cbm_irnen;
	u32 resv0[1];
	u32 igp_irncr;
	u32 igp_irnicr;
	u32 igp_irnen;
	u32 resv1[1];
	u32 egp_irncr;
	u32 egp_irnicr;
	u32 egp_irnen;
};

/*
 * struct cbm_eqm_cpu_igp_reg — per-CPU enqueue ingress port window.
 *
 * Verbatim from AVM cbm.h:356-381 (resv2[12] and resv4[27] are NOT
 * truncated — they are the planner's compiler-verified pad sizes that
 * push new_sptr to 0x80, new_jptr to 0x90, desc0 to 0x100, desc1 to
 * 0x110, and stride to 0x120). Any deviation from those offsets
 * indicates a struct miscopy.
 */
struct cbm_eqm_cpu_igp_reg {
	u32 cfg;
	u32 wm;
	u32 pocc;
	u32 eqpc;
	struct cbm_desc disc;
	u32 irncr;
	u32 irnicr;
	u32 irnen;
	u32 resv0[2];
	u32 rcnt;
	u32 dicc;
	u32 rcntc;
	u32 nsbpc;
	u32 njbpc;
	u32 resv1[1];
	u32 dcntr;
	u32 resv2[12];
	u32 new_sptr;
	u32 resv3[3];
	u32 new_jptr;
	u32 resv4[27];

	struct cbm_desc desc0;
	struct cbm_desc desc1;
};

/*
 * struct cbm_eqm_dma_igp_reg — per-DMA enqueue ingress port window.
 *
 * Verbatim from AVM cbm.h:383-396.
 */
struct cbm_eqm_dma_igp_reg {
	u32 cfg;
	u32 wm;
	u32 pocc;
	u32 eqpc;
	struct cbm_desc disc;
	u32 irncr;
	u32 irnicr;
	u32 irnen;
	u32 resv0;
	u32 dptr;
	u32 resv1;
	u32 dicc;
};

/*
 * struct cbm_dqm_cpu_egp_reg — per-CPU dequeue egress port window.
 *
 * Verbatim from AVM cbm.h:398-413.
 */
struct cbm_dqm_cpu_egp_reg {
	u32 cfg;
	u32 dqpc;
	u32 resv0[6];
	u32 irncr;
	u32 irnicr;
	u32 irnen;
	u32 resv1;
	u32 dptr;
	u32 bprc;
	u32 resv2[18];
	u32 ptr_rtn;
	u32 resv3[31];
	struct cbm_desc desc0;
	struct cbm_desc desc1;
};

/*
 * Address-calculation macros — verbatim from AVM cbm.h:167-189.
 *
 * The (idx)*0x1000 stride matches the 4 KiB per-port window the HW
 * enforces. CBM_IOCU_ADDR converts a KSEG0/KSEG1 pointer to its KSEG1
 * uncacheable alias (((addr) & 0x1FFFFFFF) | 0xC0000000).
 */
#define CBM_EQM_CPU_PORT(idx, reg) \
	(CFG_CPU_IGP_0 + (idx) * 0x1000 + offsetof(struct cbm_eqm_cpu_igp_reg, reg))
#define CBM_EQM_DMA_PORT(idx, reg) \
	(CFG_DMA_IGP_5 + (idx) * 0x1000 + offsetof(struct cbm_eqm_dma_igp_reg, reg))
#define CBM_DQM_CPU_PORT(idx, reg) \
	(CFG_CPU_EGP_0 + (idx) * 0x1000 + offsetof(struct cbm_dqm_cpu_egp_reg, reg))
#define CBM_INT_LINE(idx, reg) \
	((idx) * 0x40 + offsetof(struct cbm_int_reg, reg))
/* AVM cbm.h:181-182 — verbatim. Parens around (addr) inside the mask
 * defend against macro-expansion bugs when passed a `base + offset`
 * expression.
 */
#define CBM_IOCU_ADDR(addr) \
	(((addr) & 0x1FFFFFFF) | 0xC0000000)
/* AVM cbm.h CBM_EQM_DMA_DESC macro. The ((pid)-5) factor reflects that
 * AVM's DMA-IGP ports are numbered 5..8 but the per-port descriptor
 * staging window starts at SDESC0_0_IGP_5.
 */
#define CBM_EQM_DMA_DESC(pid, des_idx, jbo_flag) \
	(SDESC0_0_IGP_5 + ((pid) - 5) * 0x1000 + (des_idx) * 16 + (jbo_flag) * 0x800)
/* FSQM_LLT_RAM / FSQM_RCNT — verbatim from AVM cbm.h:167-170. (idx)<<2
 * is the per-segment u32 stride.
 */
#define FSQM_LLT_RAM(fsqm_base, idx) \
	((fsqm_base) + RAM + ((idx) << 2))
#define FSQM_RCNT(fsqm_base, idx) \
	((fsqm_base) + RCNT + ((idx) << 2))

/*
 * The plan's 12-base layout matches the AVM cbm_base_addr struct fields
 * (cqm/grx500/cbm.h:336-348). All bases except g_cbm_fsqm_base[] are
 * single-entry; FSQM has two physical instances (FSQM0 / FSQM1).
 */
extern void __iomem *g_cbm_tmu_base;
extern void __iomem *g_cbm_base;
extern void __iomem *g_cbm_qidt_base;
extern void __iomem *g_cbm_sbim_base;
extern void __iomem *g_cbm_qeqcnt_base;
extern void __iomem *g_cbm_qdqcnt_base;
extern void __iomem *g_cbm_ls_base;
extern void __iomem *g_cbm_eqm_base;
extern void __iomem *g_cbm_dqm_base;
extern void __iomem *g_cbm_fsqm_base[2];
extern void __iomem *g_cbm_dma_desc_base;

/*
 * Per-window MMIO accessors.
 *
 * KISS: one helper per ioremap'd window so the caller cannot accidentally
 * route an EQM offset through the DQM base. The accessor inlines wrap
 * __raw_readl/__raw_writel (matching AVM xrx500_cbm_r32/w32 at AVM cbm.c:170-171)
 * so MIPS strict byte-ordering applies to the descriptor traffic.
 */
static inline u32 xrx500_cbm_r32(u32 off)
{
	return __raw_readl(g_cbm_base + off);
}
static inline void xrx500_cbm_w32(u32 off, u32 val)
{
	__raw_writel(val, g_cbm_base + off);
}

static inline u32 cbm_eqm_r32(u32 off)
{
	return __raw_readl(g_cbm_eqm_base + off);
}
static inline void cbm_eqm_w32(u32 off, u32 val)
{
	__raw_writel(val, g_cbm_eqm_base + off);
}

static inline u32 cbm_dqm_r32(u32 off)
{
	return __raw_readl(g_cbm_dqm_base + off);
}
static inline void cbm_dqm_w32(u32 off, u32 val)
{
	__raw_writel(val, g_cbm_dqm_base + off);
}

static inline u32 fsqm_r32(int idx, u32 off)
{
	return __raw_readl(g_cbm_fsqm_base[idx] + off);
}
static inline void fsqm_w32(int idx, u32 off, u32 val)
{
	__raw_writel(val, g_cbm_fsqm_base[idx] + off);
}

struct cbm_buff {
	void *std_buf_base;
	void *std_buf_addr;
	void *jbo_buf_base;
	void *jbo_buf_addr;
	u32 std_pool_size;
	u32 jbo_pool_size;
	u32 std_frm_size;
	u32 jbo_frm_size;
	u32 std_frm_num;
	u32 jbo_frm_num;
	bool placeholder;
};

extern struct cbm_buff g_cbm_buff;

/*
 * Faithful port of the AVM 4.9 registry surface that conf_dqm_cpu_port /
 * dp_port_resources_get consume (bodies in cbm_ports.c). Provenance:
 *   - CBM_PMAC_DYNAMIC..CBM_PORT_NOT_APPL: AVM cqm/cqm_common.h:9-12
 *   - CBM_MAX_PHY_PORT_PER_EP:             AVM cqm/grx500/cbm.c:23
 *   - SBID_START / CBM_NOTFOUND / CBM_PORT_MAX: AVM cqm/grx500/cbm.h:207-210
 *     (SBID_START promoted from cbm_dp.c's ST007_SBID_START local mirror)
 *   - CBM_SUCCESS / CBM_FAILURE:           AVM cqm/cqm_common.h:22-23
 *     (cbm_intr.c:130-131 carries identical-token local mirrors; the
 *      #ifndef guards + identical replacement lists keep both legal)
 *   - enum FREE_FLAG:                      AVM cqm/grx500/cbm.h:94-103
 *   - struct cbm_egp_map:                  AVM cqm/grx500/cbm.h:490-494
 *   - struct cbm_pmac_port_map:            AVM cqm/grx500/cbm.h:521-538
 *   - cbm_dq_info_t / struct cbm_dqm_port_info: REDUCED ports, see below
 *   - get_is_bit_set:                      AVM cqm/cqm_common.h:232-235
 */
#define CBM_PMAC_DYNAMIC 1000
#define CBM_PORT_INVALID 2000
#define CBM_PMAC_NOT_APPL 3000
#define CBM_PORT_NOT_APPL 255
#define CBM_MAX_PHY_PORT_PER_EP 4
#define SBID_START 16
#define CBM_NOTFOUND 2
#define CBM_PORT_MAX 64
/*
 * Datapath port-id space on GSWIP-3.0: 0..15. The bound is silicon, not
 * convention — the TX descriptor's DW1 "ep" field is 4 bits wide
 * (datapath_api_gswip30.h), so 15 is the largest representable dp port and
 * any array indexed by dp_port_id needs exactly this many slots. Mirrors
 * MAX_DP_PORTS in datapath/datapath.h; duplicated rather than included
 * because the CQM headers deliberately do not pull in the datapath
 * internals.
 */
#define CBM_MAX_DP_PORTS 16
#ifndef CBM_SUCCESS
#define CBM_SUCCESS 0
#endif
#ifndef CBM_FAILURE
#define CBM_FAILURE (-1)
#endif

/*! PMAC port flag — AVM cbm.h:94-103 verbatim. */
enum FREE_FLAG {
	P_FREE = 0, /*! The port is free */
	/*the port is already allocated to some driver,
	 *but not registered or no need to register at all.\n
	 *for example, LRO/CAPWA, only need to allocate,
	 *but no need to register
	 */
	P_ALLOCATED,
	P_REGISTERED, /*! Registered already. */
	P_FLAG_NO_VALI /*! Not valid flag */
};

/* AVM cbm.h:490-494 verbatim — epg_lookup_table row shape. */
struct cbm_egp_map {
	u32 epg;
	u32 pmac;
	u32 port_type;
};

/**
 * struct cbm_dp_egress_res - egress resources of one Ethernet datapath port.
 *
 * @deq_port: CBM DQM dequeue port.
 *
 * @tmu_queue: TMU queue feeding @deq_port. The scheduler block id is
 *             @tmu_queue - SBID_START.
 *
 * @dma_ctrl:  DMA controller draining @deq_port, in AVM cbm_config.c's
 *             encoding: 1 = DMA1TX, 2 = DMA2TX. NOT an enum dma_controller
 *             cid — callers translate.
 *
 * @dma_chan:  channel within @dma_ctrl.
 *
 * Filled by cbm_dp_egress_res_get() from the ported vendor tables. The four
 * fields always come from one pair of vendor rows, so they cannot drift
 * apart the way four independent closed forms could.
 */
struct cbm_dp_egress_res {
	u32 deq_port;
	u32 tmu_queue;
	u32 dma_ctrl;
	u32 dma_chan;
};

int cbm_dp_egress_res_get(u32 dp_port_id, struct cbm_dp_egress_res *res);
int cbm_dp_deq_port_get(u32 dp_port_id, u32 *deq_port);

/*
 * cbm_dq_info_t — REDUCED port of AVM include/net/lantiq_cbm_api.h:486-492.
 * Member names identical to AVM.
 */
typedef struct cbm_dq_info {
	uint32_t port_no; /*! < CBM Dequeue Port No */
	int32_t dma_tx_chan; /*! PMAC DMA Tx Channel */
	uint32_t num_desc; /*!< Number of Descriptors at port base */
} cbm_dq_info_t;

/*
 * struct cbm_dqm_port_info — REDUCED port of AVM cbm.h:497-509. Member names
 * identical to AVM for every field conf_dqm_cpu_port / dp_port_resources_get
 * touch.
 */
struct cbm_dqm_port_info {
	u32 def_qid;
	u32 def_schd;
	u32 num_free_entries; /*!< Number of Free Port entries */
	cbm_dq_info_t deq_info;
	u32 egp_type;
};

/* AVM cbm.h:521-538 verbatim — registry node. */
struct cbm_pmac_port_map {
	/*! port flag */
	enum FREE_FLAG flags;
	struct module *owner;
	struct net_device *dev;
	u32 dev_port;
	u32 pmac;
	/*bit map to egp port*/
	u32 egp_port_map;
	/*queue numbers allocated to that pmac port*/
	u32 qid_num;
	u32 qids[16]; /*qid array*/
	/*e.g. DP_F_FAST_ETH_LAN/DP_F_FAST_ETH_WAN/
	 *DP_F_DIRECT/High priority/Low priority
	 */
	u32 egp_type;
	struct list_head list;
};

/* AVM cqm/cqm_common.h:232-235 verbatim — lowest-set-bit helper. */
static inline int get_is_bit_set(u32 flags)
{
	return ffs(flags) - 1;
}

struct cbm_pmac_port_map *is_cbm_allocated(s32 cbm, u32 flags);
struct cbm_pmac_port_map *is_dp_allocated(s32 pmac, u32 flags);
int cbm_configure_dqm_cpu_ports(void);
s32 dp_port_resources_get(u32 *dp_port, u32 *num_tmu_ports,
			  cbm_tmu_res_t **res_pp, u32 flags);
void cbm_program_cpu_qidt(u8 qid_val);

void init_fsqm(int idx);
int init_cbm_basic(void);
int cbm_intr_mapping_init(void);
int cbm_interrupt_init(struct platform_device *pdev, int *irqs);
int cbm_hw_init(struct platform_device *pdev);

int cbm_dp_enable(struct module *owner, u32 port_id,
		  struct cbm_dp_en_data *cbm_data, u32 flags, u32 alloc_flags);

int cbm_dp_port_alloc(struct module *owner, struct net_device *dev,
		      u32 dev_port, s32 port_id,
		      struct cbm_dp_alloc_data *data, u32 flags);
int cbm_dp_port_dealloc(struct module *owner, u32 dev_port, s32 port_id,
			struct cbm_dp_alloc_data *data, u32 flags);

void *cbm_buf_alloc(u32 size, u32 *pool_phys, u32 flags);
int cbm_buf_free(void *buf, u32 size);
void *cbm_buf_phys_to_virt(u32 phys);

void *cbm_fsqm_buf_alloc(int pid, u32 flags, u32 *buf_phys); /* flags = pool select */
int cbm_fsqm_buf_free(int pid, u32 buf_phys);

int dma_port_enable(u32 idx, int dqm_flag);
int init_cbm_eqm_dma_port(int idx, u32 flags); /* flags = std/jumbo buffer type */
int init_cbm_eqm_ldma_port(void); /* IGP15 VRX318/LDMA arming */
int init_cbm_eqm_cpu_port(int idx);
int init_cbm_dqm_dma_port(int dqp);
int init_cbm_dqm_cpu_port(int idx);
void cbm_rx_engine_init(void);
void cbm_rx_set_netdev(u32 sppid, struct net_device *dev);
int turn_on_DMA_p2p(void);
extern bool g_cbm_egress_preconfig[CBM_MAX_DP_PORTS];
int setup_eqm_dma_desc(int pid, int desc_count, u32 flags, u32 buf_offset);
u8 get_lookup_qid_via_index(u32 lookup_idx);
int cbm_counter_mode_set(int idx, int mode);

/* cbm_dw_memset - 32-bit-aligned wordwise memset over MMIO. */
void cbm_dw_memset(u32 *base, int val, u32 size);

static_assert(offsetof(struct cbm_int_reg, igp_irnen) == 0x18,
	      "cbm_int_reg.igp_irnen must be at offset 0x18 (AVM cbm.h:442-454)");
static_assert(offsetof(struct cbm_int_reg, egp_irnen) == 0x28,
	      "cbm_int_reg.egp_irnen must be at offset 0x28 (AVM cbm.h:442-454)");
static_assert(offsetof(struct cbm_eqm_cpu_igp_reg, new_sptr) == 0x80,
	      "cbm_eqm_cpu_igp_reg.new_sptr must be at offset 0x80 (AVM cbm.h:356-381)");
static_assert(offsetof(struct cbm_eqm_cpu_igp_reg, new_jptr) == 0x90,
	      "cbm_eqm_cpu_igp_reg.new_jptr must be at offset 0x90 (AVM cbm.h:356-381)");
static_assert(offsetof(struct cbm_eqm_cpu_igp_reg, desc0) == 0x100,
	      "cbm_eqm_cpu_igp_reg.desc0 must be at offset 0x100 (AVM cbm.h:356-381)");
static_assert(offsetof(struct cbm_eqm_cpu_igp_reg, desc1) == 0x110,
	      "cbm_eqm_cpu_igp_reg.desc1 must be at offset 0x110 (AVM cbm.h:356-381)");
static_assert(offsetof(struct cbm_dqm_cpu_egp_reg, irncr) == 0x20,
	      "cbm_dqm_cpu_egp_reg.irncr must be at offset 0x20 (AVM cbm.h:398-413)");
static_assert(offsetof(struct cbm_dqm_cpu_egp_reg, ptr_rtn) == 0x80,
	      "cbm_dqm_cpu_egp_reg.ptr_rtn must be at offset 0x80 (AVM cbm.h:398-413)");
static_assert(offsetof(struct cbm_dqm_cpu_egp_reg, desc0) == 0x100,
	      "cbm_dqm_cpu_egp_reg.desc0 must be at offset 0x100 (AVM cbm.h:398-413)");
static_assert(offsetof(struct cbm_eqm_dma_igp_reg, irncr) == 0x20,
	      "cbm_eqm_dma_igp_reg.irncr must be at offset 0x20 (AVM cbm.h:383-396)");
static_assert(offsetof(struct cbm_eqm_dma_igp_reg, dptr) == 0x30,
	      "cbm_eqm_dma_igp_reg.dptr must be at offset 0x30 (AVM cbm.h:383-396)");
static_assert(offsetof(struct cbm_eqm_dma_igp_reg, dicc) == 0x38,
	      "cbm_eqm_dma_igp_reg.dicc must be at offset 0x38 (AVM cbm.h:383-396)");
static_assert(CBM_GRX550_DMA_DATA_OFFSET == 128,
	      "CBM_GRX550_DMA_DATA_OFFSET must equal 128 (AVM cqm/cqm_common.h:18-19)");
static_assert(FSQM_IRNEN_DEFAULT == 0x111101F,
	      "FSQM_IRNEN_DEFAULT must equal 0x111101F (AVM cbm.c:1129)");

#endif /* __XRX500_CBM_H */
