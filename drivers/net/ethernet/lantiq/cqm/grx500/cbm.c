// SPDX-License-Identifier: GPL-2.0-only
/*
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.c.
 *
 * Central Buffer Manager (CBM) core: probe, register-window mapping and the
 * hardware bring-up sequence for the enqueue (EQM) and dequeue (DQM)
 * managers, the free-segment queue manager and the load spreader.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <linux/clk.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/bits.h>
#include <asm/io.h>
#include <asm/page.h>

#include "cbm.h"
#include "../../tmu/drv_tmu_ll.h"
#include "../../dma/lantiq_dmax.h"

#define CBM_SBA_0  0x200  /* Std Buf Addr 0 (physical) */
#define CBM_SBA_1  0x204  /* Std Buf Addr 1 (IOCU alias) */
#define CBM_JBA_0  0x208  /* Jumbo Buf Addr 0 (physical) */
#define CBM_JBA_1  0x20C  /* Jumbo Buf Addr 1 (IOCU alias) */

#define CBM_PORT_F_STANDARD_BUF  0x1U
#define CBM_PORT_F_JUMBO_BUF     0x2U
#define CBM_DMADESC_PHYS_BASE    0x1e500000U /* DT cbm dma_desc window (reg idx 11) */

extern void cbm_set_val(void __iomem *addr, u32 val, u32 mask, u32 shift);
extern int cbm_buf_init_pools(struct device *dev);
extern phys_addr_t cbm_buf_pool_phys_base(int which);

/* Pool index constants — mirror cbm_buf.c's CBM_POOL_STD / CBM_POOL_JBO. */
#define CBM_POOL_STD  0
#define CBM_POOL_JBO  1

irqreturn_t cbm_isr_0(int irq, void *dev_id);
irqreturn_t cbm_isr_4(int irq, void *dev_id);
irqreturn_t cbm_isr_5(int irq, void *dev_id);
irqreturn_t cbm_isr_6(int irq, void *dev_id);
irqreturn_t cbm_isr_7(int irq, void *dev_id);


/*
 * MMIO base pointers — populated by cbm_xrx500_probe from the 12 reg tuples
 * in the DT node.
 */
void __iomem *g_cbm_tmu_base;
void __iomem *g_cbm_base;
void __iomem *g_cbm_qidt_base;
void __iomem *g_cbm_sbim_base;
void __iomem *g_cbm_qeqcnt_base;
void __iomem *g_cbm_qdqcnt_base;
void __iomem *g_cbm_ls_base;
void __iomem *g_cbm_eqm_base;
void __iomem *g_cbm_dqm_base;
void __iomem *g_cbm_fsqm_base[2];
void __iomem *g_cbm_dma_desc_base;

EXPORT_SYMBOL_GPL(g_cbm_tmu_base);
EXPORT_SYMBOL_GPL(g_cbm_base);
EXPORT_SYMBOL_GPL(g_cbm_qidt_base);
EXPORT_SYMBOL_GPL(g_cbm_sbim_base);
EXPORT_SYMBOL_GPL(g_cbm_qeqcnt_base);
EXPORT_SYMBOL_GPL(g_cbm_qdqcnt_base);
EXPORT_SYMBOL_GPL(g_cbm_ls_base);
EXPORT_SYMBOL_GPL(g_cbm_eqm_base);
EXPORT_SYMBOL_GPL(g_cbm_dqm_base);
EXPORT_SYMBOL_GPL(g_cbm_fsqm_base);
EXPORT_SYMBOL_GPL(g_cbm_dma_desc_base);

/*
 * Probe re-entry guard.
 *
 * The kernel driver-model contract should never double-probe a single
 * platform_device, but AVM's source pattern uses a global g_cbm_ctrl which
 * would silently corrupt if a second probe landed (e.g. due to a DT-overlay
 * race). atomic_inc_return + dev_warn makes the failure mode loud and
 * observable.
 */
static atomic_t cbm_probe_count = ATOMIC_INIT(0);

static struct platform_device *cbm_pdev_cache;
static int cbm_irqs[5];
static struct clk *cbm_clk;

/*
 * Slot order MUST match the DT reg-names order EXACTLY (tmu first, dma_desc
 * last); a mismatch would silently route every subsequent MMIO write to the
 * wrong window.
 */
struct cbm_ioremap_slot {
	const char *name;
	void __iomem **store;
};

static const struct cbm_ioremap_slot cbm_ioremap_table[] = {
	{ "tmu",      &g_cbm_tmu_base },      /* idx 0 - TMU       */
	{ "cbm",      &g_cbm_base },          /* idx 1 - CBM       */
	{ "qidt",     &g_cbm_qidt_base },     /* idx 2 - QIDT      */
	{ "sbim",     &g_cbm_sbim_base },     /* idx 3 - SBIM      */
	{ "qeqcnt",   &g_cbm_qeqcnt_base },   /* idx 4 - QEQCNTR   */
	{ "qdqcnt",   &g_cbm_qdqcnt_base },   /* idx 5 - QDQCNTR   */
	{ "ls",       &g_cbm_ls_base },       /* idx 6 - LS        */
	{ "eqm",      &g_cbm_eqm_base },      /* idx 7 - CBM EQM   */
	{ "dqm",      &g_cbm_dqm_base },      /* idx 8 - CBM DQM   */
	{ "fsqm0",    &g_cbm_fsqm_base[0] },  /* idx 9 - FSQM0     */
	{ "fsqm1",    &g_cbm_fsqm_base[1] },  /* idx 10 - FSQM1    */
	{ "dma_desc", &g_cbm_dma_desc_base }, /* idx 11 - CBM DMA  */
};

static u32 cbm_p2p_setup_done;
static u32 cbm_p2p_turned_on;

static void setup_DMA_p2p(void)
{
	/*do only p2p here.commented out other dma channel setup*/
	int chan1 = DMA1TX_LAN_SWITCH_CLASS0;
	int chan2 = DMA2RX_GSWIP_R_CLASS0;
#if !defined(SINGLE_RX_CH0_ONLY) || !SINGLE_RX_CH0_ONLY
	int chan1_1 = DMA1TX_LAN_SWITCH_CLASS1;
	int chan2_1 = DMA2RX_GSWIP_R_CLASS1;
	int chan1_2 = DMA1TX_LAN_SWITCH_CLASS2;
	int chan2_2 = DMA2RX_GSWIP_R_CLASS2;
	int chan1_3 = DMA1TX_LAN_SWITCH_CLASS3;
	int chan2_3 = DMA2RX_GSWIP_R_CLASS3;
#endif

	if ((ltq_request_dma(chan1, "dma1 tx lan")) < 0)
		pr_err(" %s failed to open chan for chan1\r\n", __func__);

	if ((ltq_request_dma(chan2, "dma2 rx gswpr")) < 0)
		pr_err(" %s failed to open chan for chan2\r\n", __func__);

	/* rx decriptor setup */
	if (ltq_dma_chan_desc_alloc(chan2, 32) == 0)
		ltq_dma_chan_data_buf_alloc(chan2);

#if !defined(SINGLE_RX_CH0_ONLY) || !SINGLE_RX_CH0_ONLY
	if ((ltq_request_dma(chan1_1, "dma1 tx lan")) < 0)
		pr_err(" %s failed to open chan for chan1_1\r\n", __func__);

	if ((ltq_request_dma(chan2_1, "dma2 rx gswpr")) < 0)
		pr_err(" %s failed to open chan for chan2_1\r\n", __func__);

	/* rx decriptor setup */
	if (ltq_dma_chan_desc_alloc(chan2_1, 32) == 0)
		ltq_dma_chan_data_buf_alloc(chan2_1);

	if ((ltq_request_dma(chan1_2, "dma1 tx lan")) < 0)
		pr_err(" %s failed to open chan for chan1_2\r\n", __func__);

	if ((ltq_request_dma(chan2_2, "dma2 rx gswpr")) < 0)
		pr_err(" %s failed to open chan for chan2_2\r\n", __func__);

	/* rx decriptor setup */
	if (ltq_dma_chan_desc_alloc(chan2_2, 32) == 0)
		ltq_dma_chan_data_buf_alloc(chan2_2);

	if ((ltq_request_dma(chan1_3, "dma1 tx lan")) < 0)
		pr_err(" %s failed to open chan for chan1_3\r\n", __func__);

	if ((ltq_request_dma(chan2_3, "dma2 rx gswpr")) < 0)
		pr_err(" %s failed to open chan for chan2_3\r\n", __func__);

	/* rx decriptor setup */
	if (ltq_dma_chan_desc_alloc(chan2_3, 32) == 0)
		ltq_dma_chan_data_buf_alloc(chan2_3);
#endif

	/* Any other configuration ???? */
	ltq_dma_p2p_cfg(chan2, chan1);
#if !defined(SINGLE_RX_CH0_ONLY) || !SINGLE_RX_CH0_ONLY
	ltq_dma_p2p_cfg(chan2_3, chan1_3);
	ltq_dma_p2p_cfg(chan2_1, chan1_1);
	ltq_dma_p2p_cfg(chan2_2, chan1_2);
#endif
	ltq_dma_chan_irq_disable(chan2);
	ltq_dma_chan_irq_disable(chan1);
#if !defined(SINGLE_RX_CH0_ONLY) || !SINGLE_RX_CH0_ONLY
	ltq_dma_chan_irq_disable(chan2_3);
	ltq_dma_chan_irq_disable(chan1_3);
	ltq_dma_chan_irq_disable(chan2_1);
	ltq_dma_chan_irq_disable(chan1_1);
	ltq_dma_chan_irq_disable(chan2_2);
	ltq_dma_chan_irq_disable(chan1_2);
#endif
	cbm_p2p_setup_done = 1;
	pr_info("%s executed\n", __func__);
}

int turn_on_DMA_p2p(void)
{
	int chan1 = DMA1TX_LAN_SWITCH_CLASS0;
	int chan2 = DMA2RX_GSWIP_R_CLASS0;
#if !defined(SINGLE_RX_CH0_ONLY) || !SINGLE_RX_CH0_ONLY
	int chan1_1 = DMA1TX_LAN_SWITCH_CLASS1;
	int chan2_1 = DMA2RX_GSWIP_R_CLASS1;
	int chan1_2 = DMA1TX_LAN_SWITCH_CLASS2;
	int chan2_2 = DMA2RX_GSWIP_R_CLASS2;
	int chan1_3 = DMA1TX_LAN_SWITCH_CLASS3;
	int chan2_3 = DMA2RX_GSWIP_R_CLASS3;
#endif

	if (!cbm_p2p_setup_done || cbm_p2p_turned_on)
		return -EPERM;

	if (!cbm_p2p_turned_on) {
		ltq_dma_chan_on(chan1);
		ltq_dma_chan_on(chan2);
#if !defined(SINGLE_RX_CH0_ONLY) || !SINGLE_RX_CH0_ONLY
		ltq_dma_chan_on(chan1_1);
		ltq_dma_chan_on(chan2_1);

		ltq_dma_chan_on(chan1_2);
		ltq_dma_chan_on(chan2_2);

		ltq_dma_chan_on(chan1_3);
		ltq_dma_chan_on(chan2_3);
#endif
		cbm_p2p_turned_on = 1;
	}
	return 0;
}

static int cbm_eqm_rx_chan_open(int chan, int cbm_port, u32 buf_type)
{
	dma_addr_t desc;
	int ret;

	ret = ltq_request_dma((u32)chan, "dma2 rx eqm");
	if (ret < 0) {
		pr_err("cbm: EQM RX ltq_request_dma(chan=%d) = %d\n",
		       chan, ret);
		return ret;
	}

	desc = (dma_addr_t)(CBM_DMADESC_PHYS_BASE +
			    CBM_EQM_DMA_DESC(cbm_port, 0, (buf_type >> 1)));
	ret = ltq_dma_chan_desc_cfg((u32)chan, desc, 2);
	if (ret < 0) {
		pr_err("cbm: EQM RX desc_cfg(chan=%d) = %d\n", chan, ret);
		return ret;
	}

	ltq_dma_chan_irq_disable((u32)chan);

	ret = ltq_dma_chan_on((u32)chan);
	if (ret < 0) {
		pr_err("cbm: EQM RX chan_on(chan=%d) = %d\n", chan, ret);
		return ret;
	}

	pr_info("cbm: EQM RX chan %d opened (cbm_port %d, %s)\n",
		chan, cbm_port,
		(buf_type & CBM_PORT_F_STANDARD_BUF) ? "std" : "jumbo");
	return 0;
}

int cbm_hw_init(struct platform_device *pdev)
{
	int ret;

	/*
	 * The datapath initiators must be strapped big-endian before any CBM
	 * or GSW reset: the straps are sampled at block reset, so the value
	 * has to be held across bring-up. SPI_DEBUG_EN (IFMUX_CFG bit 18) is
	 * cleared as the vendor's own chiptop setup does.
	 */
	{
		void __iomem *ct = ioremap(0x16080000, 0x200);

		if (ct) {
			u32 pre;

			__raw_writel(0x0000c221, ct + 0x4C);
			pre = __raw_readl(ct + 0x120);
			__raw_writel(pre & ~(BIT(22) | BIT(18)), ct + 0x120);
			iounmap(ct);
		}
		pr_info("cbm: chiptop parity applied (ipt_endian, ifmux)\n");
	}

	(void)pdev;

	init_fsqm(0);
	init_fsqm(1);

	ret = init_cbm_basic();
	if (ret) {
		pr_err("cbm: init_cbm_basic failed: %d\n", ret);
		return ret;
	}

	/*
	 * (d) post-init markers. The "cbm: " prefix is required so the
	 * hardware tester's `dmesg | grep '^cbm:'` finds these lines.
	 */
	pr_info("cbm: post-init FSQM0 OFSC=0x%08x OFSQ=0x%08x\n",
		fsqm_r32(0, OFSC),
		fsqm_r32(0, OFSQ));
	pr_info("cbm: post-init FSQM1 OFSC=0x%08x OFSQ=0x%08x\n",
		fsqm_r32(1, OFSC),
		fsqm_r32(1, OFSQ));
	pr_info("cbm: post-init SBA_0=0x%08x JBA_0=0x%08x CBM_CTRL=0x%08x\n",
		(u32)cbm_buf_pool_phys_base(CBM_POOL_STD),
		(u32)cbm_buf_pool_phys_base(CBM_POOL_JBO),
		xrx500_cbm_r32(CBM_CTRL));

	{
		int p;

		for (p = 0; p < 4; p++)
			init_cbm_eqm_cpu_port(p);
	}

	{
		int p;

		for (p = 0; p < 4; p++)
			init_cbm_eqm_dma_port(p, CBM_PORT_F_STANDARD_BUF);
		for (p = 0; p < 4; p++)
			init_cbm_eqm_dma_port(p, CBM_PORT_F_JUMBO_BUF);
		pr_info("cbm: EQM DMA ports 5-8 seeded (std + jumbo)\n");
	}

	{
		int sret = init_cbm_eqm_dma_port(4, CBM_PORT_F_STANDARD_BUF);

		if (sret)
			pr_err("cbm: init IGP9/TOE port failed: %d\n", sret);
		sret = init_cbm_eqm_ldma_port();
		if (sret)
			pr_err("cbm: init_cbm_eqm_ldma_port failed: %d\n",
			       sret);
	}

	/*
	 * SINGLE_RX_CH0_ONLY: only the CLASS0 channel pair is brought up,
	 * matching the vendor's own definition in
	 * include/net/lantiq_cbm_api.h.
	 */
	if (!cbm_p2p_setup_done)
		setup_DMA_p2p();

	return 0;
}

bool g_cbm_egress_preconfig[CBM_MAX_DP_PORTS];

/*
 * cbm_dt_dp_ports - collect the datapath port ids this board's ethernet node
 * declares, ascending, into @out (capacity @max).
 *
 * If no ethernet node is present, no netdev will exist either; fall back to
 * the four LAN ports so a DT without an ethernet node behaves exactly as it
 * did before this became DT-driven.
 */
static int cbm_dt_dp_ports(u32 *out, int max)
{
	static const u32 lan_only[] = { 2, 3, 4, 5 };
	struct device_node *eth, *port;
	int n = 0, i, j;

	eth = of_find_compatible_node(NULL, NULL, "intel,xrx500-net");
	if (eth) {
		for_each_available_child_of_node(eth, port) {
			u32 dp;

			if (n >= max)
				continue;
			if (of_property_read_u32(port, "intel,dp-port-id", &dp))
				continue;
			/* insertion sort; the lists are 4-5 entries long */
			for (i = 0; i < n && out[i] < dp; i++)
				;
			if (i < n && out[i] == dp)
				continue;	/* duplicate DT entry */
			for (j = n; j > i; j--)
				out[j] = out[j - 1];
			out[i] = dp;
			n++;
		}
		of_node_put(eth);
	}

	if (n)
		return n;

	for (i = 0; i < (int)ARRAY_SIZE(lan_only) && i < max; i++)
		out[i] = lan_only[i];
	return i;
}

extern int hdma_port_enable(int port_id);
extern int hdma_ig192_toe_dma3_reset(void); /* TOE/DMA3 reset parity */

#define CBM_LS_GLBL_CTRL    0x900u
#define CBM_LS_SPR_CTRL     0x904u
#define CBM_LS_IRNEN        0x918u
#define CBM_LS_PORT_CTRL(i) ((u32)(i) * 0x100u + 0x10u)
#define CBM_LS_PORT_NUM     4
#define CBM_LS_PORT_ACTIVE   0x2000070Fu  /* 0xF | (7<<8) | (0x2000<<16) */
#define CBM_LS_PORT_INACTIVE 0x2000070Du  /* 0xD | (7<<8) | (0x2000<<16) */

static void cbm_init_load_spreader(void)
{
	u32 spr = 0;          /* SPR_SEL = SPREAD_WRR(0) in bit0 */
	int i;

	if (!g_cbm_ls_base) {
		pr_warn("cbm: LS base not mapped; skipping load-spreader init\n");
		return;
	}

	/* Per-port control: only port 0 active on this nosmp / no
	 * CONFIG_CBM_LS_ENABLE build (AVM init_cbm_ls_port: active iff idx==0). */
	for (i = 0; i < CBM_LS_PORT_NUM; i++)
		__raw_writel((i == 0) ? CBM_LS_PORT_ACTIVE : CBM_LS_PORT_INACTIVE,
			     g_cbm_ls_base + CBM_LS_PORT_CTRL(i));

	/* Spread alg = WRR, per-port weight 2 (WP[n] = bits [17+2n:16+2n]). */
	for (i = 0; i < CBM_LS_PORT_NUM; i++)
		spr |= (2u & 0x3u) << (16 + 2 * i);
	__raw_writel(spr, g_cbm_ls_base + CBM_LS_SPR_CTRL);

	__raw_writel(0xFF0000u, g_cbm_ls_base + CBM_LS_IRNEN);
	__raw_writel(0x1u, g_cbm_ls_base + CBM_LS_GLBL_CTRL);   /* EN */
	wmb();
	pr_info("cbm: load spreader init (SPR_CTRL=0x%08x GLBL=0x%08x)\n",
		__raw_readl(g_cbm_ls_base + CBM_LS_SPR_CTRL),
		__raw_readl(g_cbm_ls_base + CBM_LS_GLBL_CTRL));
}

void cbm_program_cpu_qidt(u8 qid_val)
{
	unsigned int fl, cls, mpe1, mpe2, enc, dec;

	if (!g_cbm_qidt_base) {
		pr_err("cbm: cpu_qidt: g_cbm_qidt_base not mapped\n");
		return;
	}

	for (fl = 0; fl < 2; fl++)     /* flowid_low dont-care sweep 0..1 */
	    for (cls = 0; cls < 16; cls++)
		for (mpe1 = 0; mpe1 < 2; mpe1++)
			for (mpe2 = 0; mpe2 < 2; mpe2++)
				for (enc = 0; enc < 2; enc++)
					for (dec = 0; dec < 2; dec++) {
						u32 qidt = ((fl << 12) & 0x1000U) |
							   ((dec << 11) & 0x800U) |
							   ((enc << 10) & 0x400U) |
							   ((mpe2 << 9) & 0x200U) |
							   ((mpe1 << 8) & 0x100U) |
							   (cls & 0xFU); /* ep=0, flowidh=0 */
						u32 idx = qidt >> 2;
						u32 off = (qidt % 4) << 3;
						u32 mask = 0xFFU << off;
						u32 v;

						v = __raw_readl(g_cbm_qidt_base + idx * 4);
						v = (v & ~mask) |
						    (((u32)qid_val & 0xFFU) << off);
						__raw_writel(v, g_cbm_qidt_base + idx * 4);
					}
	wmb();
	pr_info("cbm: CPU ingress QIDT ep0->q%u programmed (512 slots, flowidl 0..1)\n",
		qid_val);
}

static void cbm_program_ep_qidt(u8 ep, u8 qid_val)
{
	unsigned int fh, fl, cls, mpe1, mpe2, enc, dec;

	if (!g_cbm_qidt_base) {
		pr_err("cbm: ep_qidt: g_cbm_qidt_base not mapped\n");
		return;
	}

	for (fh = 0; fh < 2; fh++)
	 for (fl = 0; fl < 2; fl++)
	  for (cls = 0; cls < 16; cls++)
	   for (mpe1 = 0; mpe1 < 2; mpe1++)
	    for (mpe2 = 0; mpe2 < 2; mpe2++)
	     for (enc = 0; enc < 2; enc++)
	      for (dec = 0; dec < 2; dec++) {
			u32 qidt = ((fh << 13) & 0x2000U) |
				   ((fl << 12) & 0x1000U) |
				   ((dec << 11) & 0x800U) |
				   ((enc << 10) & 0x400U) |
				   ((mpe2 << 9) & 0x200U) |
				   ((mpe1 << 8) & 0x100U) |
				   (((u32)ep << 4) & 0xF0U) |
				   (cls & 0xFU);
			u32 idx = qidt >> 2;
			u32 off = (qidt % 4) << 3;
			u32 mask = 0xFFU << off;
			u32 v;

			v = __raw_readl(g_cbm_qidt_base + idx * 4);
			v = (v & ~mask) | (((u32)qid_val & 0xFFU) << off);
			__raw_writel(v, g_cbm_qidt_base + idx * 4);
	      }
	wmb();
	pr_info("cbm: QIDT ep%u->q%u programmed (1024 slots, all dims dontcare)\n",
		ep, qid_val);
}

/*
 * Split out of cbm_hw_init so probe can interpose the CPU-TX egress-port +
 * TMU-egress-path config in between (AVM's configure_ports-before-CTRL=0x11
 * ordering, cbm.c:5605/5614/5621/5657). 0x11 = EN(bit0) | QEN(bit4), MSEL=0;
 * direct write (not RMW), matching AVM.
 */
static void cbm_enable_controllers(void)
{
	cbm_dqm_w32(CBM_DQM_CTRL, 0x11);  /* DQM_EN | DQM_QEN */

	cbm_init_load_spreader();

	cbm_eqm_w32(CBM_EQM_CTRL, 0x11);  /* EQM_EN | EQM_QEN */
	wmb();
	pr_info("cbm: controllers enabled (EQM_CTRL=0x%08x DQM_CTRL=0x%08x)\n",
		cbm_eqm_r32(CBM_EQM_CTRL), cbm_dqm_r32(CBM_DQM_CTRL));

	{
		u32 v = cbm_eqm_r32(CBM_EQM_CTRL);
		int i;

		v |= BIT(8) | BIT(6) | (3u << 9);
		cbm_eqm_w32(CBM_EQM_CTRL, v);
		for (i = 0; i <= 15; i++)
			cbm_eqm_w32(CBM_EQM_CPU_PORT(i, dcntr), 0x10u & 0x3fu);
		wmb();
		pr_info("cbm: EQM enqueue-delay enabled (EQM_CTRL=0x%08x want 0x751; DCNTR[0]=0x%08x want 0x10)\n",
			cbm_eqm_r32(CBM_EQM_CTRL),
			cbm_eqm_r32(CBM_EQM_CPU_PORT(0, dcntr)));
	}
}

/* cbm_xrx500_probe - DT-bind entry point. */
static int cbm_xrx500_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	void __iomem *base;
	int irqs[5];
	int i;
	int irq;
	int ret;

	/*
	 * (a) Re-entry guard. atomic_inc_return returns the new value so the
	 * first probe sees 1 (passes), the second sees 2 (rejected).
	 */
	if (atomic_inc_return(&cbm_probe_count) > 1) {
		dev_warn(dev, "cbm: duplicate probe rejected\n");
		return -EBUSY;
	}

	/* (b) Loop ioremap the 12 MMIO windows in fixed DT-reg order. */
	for (i = 0; i < (int)ARRAY_SIZE(cbm_ioremap_table); i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(dev,
				"cbm: missing reg resource %s (idx %d)\n",
				cbm_ioremap_table[i].name, i);
			ret = -ENOENT;
			goto err;
		}
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			dev_err(dev,
				"cbm: ioremap %s (idx %d) failed: %ld\n",
				cbm_ioremap_table[i].name, i,
				PTR_ERR(base));
			ret = PTR_ERR(base);
			goto err;
		}
		*cbm_ioremap_table[i].store = base;
	}

	/* (c) Collect 5 IRQs. */
	for (i = 0; i < 5; i++) {
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			dev_err(dev,
				"cbm: platform_get_irq %d failed: %d\n",
				i, irq);
			ret = -EINVAL;
			goto err;
		}
		irqs[i] = irq;
		cbm_irqs[i] = irq;
	}

	/* (d) Optional clock. */
	cbm_clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(cbm_clk)) {
		ret = PTR_ERR(cbm_clk);
		dev_err(dev, "cbm: clk_get failed: %d\n", ret);
		cbm_clk = NULL;
		goto err;
	}
	if (cbm_clk) {
		ret = clk_prepare_enable(cbm_clk);
		if (ret) {
			dev_err(dev,
				"cbm: clk_prepare_enable failed: %d\n",
				ret);
			cbm_clk = NULL;
			goto err;
		}
	}

	ret = cbm_buf_init_pools(dev);
	if (ret) {
		dev_err(dev, "cbm: cbm_buf_init_pools failed: %d\n", ret);
		goto err_clk;
	}

	cbm_pdev_cache = pdev;

	hdma_ig192_toe_dma3_reset();

	/* (g) Bring up FSQM + CBM basic. */
	ret = cbm_hw_init(pdev);
	if (ret) {
		dev_err(dev, "cbm: cbm_hw_init failed: %d\n", ret);
		goto err_clk;
	}

	dev_dbg(dev, "cbm: IRQs collected [%d,%d,%d,%d,%d]\n",
		irqs[0], irqs[1], irqs[2], irqs[3], irqs[4]);

	/*
	 * Failure here unwinds through err_clk because the carve allocator's
	 * pool storage is devm-managed and releases automatically when the
	 * device unbinds.
	 */
	ret = tmu_init();
	if (ret) {
		dev_err(dev, "cbm: tmu_init failed: %d\n", ret);
		goto err_clk;
	}

	if (g_cbm_qidt_base) {
		u32 qi;

		for (qi = 0; qi < 0x1000; qi++)
			__raw_writel(0xFFFFFFFFU, g_cbm_qidt_base + qi * 4);
		wmb();
		pr_info("cbm: QIDT whole-table drop-fill done (0x1000 words = qid 255)\n");
	} else {
		pr_err("cbm: QIDT drop-fill skipped: g_cbm_qidt_base not mapped\n");
	}

	if (cbm_configure_dqm_cpu_ports() != CBM_SUCCESS)
		pr_err("cbm: DQM-CPU configure_ports walk FAILED\n");

	cbm_program_ep_qidt(7, 35);

	cbm_rx_engine_init();

	/*
	 * AVM's configure_ports() walks all 17 compiled-in rows at probe
	 * (cbm_config.c), so every DMA egress port is preconfigured before
	 * the controllers come up; dp 2..4 were falling through to the
	 * ndo_open path. The per-port numbers come from
	 * cbm_dp_egress_res_get() (the ported epg_lookup_table + cbm_config.c
	 * rows) rather than the dp+5 / dp+15 arithmetic they used to be
	 * computed with — that arithmetic is a LAN coincidence and does not
	 * extend.
	 *
	 * Which ports to walk is a board fact, taken from the ethernet node's
	 * own port children rather than a literal 2..5: a board with a WAN port
	 * needs dp 15 preconfigured on the same before-enable side of this
	 * boundary as its LAN ports, and a board without one must not have
	 * egress state written for a port it does not have.
	 */
	{
		u32 dp_ports[CBM_MAX_DP_PORTS];
		int n = cbm_dt_dp_ports(dp_ports, ARRAY_SIZE(dp_ports));
		int i;

		for (i = 0; i < n; i++) {
			struct cbm_dp_egress_res res;
			u32 dp = dp_ports[i];
			u16 deq, qid, sbid;

			if (cbm_dp_egress_res_get(dp, &res)) {
				pr_err("cbm: dp%u has no egress resources; skipping preconfig\n",
				       dp);
				continue;
			}
			deq  = (u16)res.deq_port;
			qid  = (u16)res.tmu_queue;
			sbid = (u16)(res.tmu_queue - SBID_START);

			init_cbm_dqm_dma_port((int)deq);
			tmu_create_flat_egress_path(1, deq, sbid, qid, 1);
			g_cbm_egress_preconfig[dp] = true;
			pr_info("cbm: dp%u egress preconfigured at probe (deq%u/q%u/sbid%u) before controller enable\n",
				dp, deq, qid, sbid);
		}
	}

	cbm_enable_controllers();

	{
		u32 dp_ports[CBM_MAX_DP_PORTS];
		int n = cbm_dt_dp_ports(dp_ports, ARRAY_SIZE(dp_ports));
		int i;

		for (i = 0; i < n; i++) {
			struct cbm_dp_egress_res res;
			int hret;

			if (cbm_dp_egress_res_get(dp_ports[i], &res))
				continue;

			hret = hdma_port_enable((int)res.deq_port);
			if (hret)
				dev_warn(dev, "cbm: probe-time hdma_port_enable(%u) = %d (ndo_open will retry)\n",
					 res.deq_port, hret);
			else
				pr_info("cbm: DMA%uTX ch%u opened at probe-end for deq %u (AVM order)\n",
					res.dma_ctrl, res.dma_chan, res.deq_port);
		}
	}

	cbm_eqm_rx_chan_open(DMA2RX_CBMP5_CLASS14, 5, CBM_PORT_F_STANDARD_BUF);
	cbm_eqm_rx_chan_open(DMA2RX_CBMP6_CLASS15, 6, CBM_PORT_F_STANDARD_BUF);
	cbm_eqm_rx_chan_open(DMA2RX_CBMP5_CLASS30_JUMBO, 5, CBM_PORT_F_JUMBO_BUF);
	cbm_eqm_rx_chan_open(DMA2RX_CBMP6_CLASS31_JUMBO, 6, CBM_PORT_F_JUMBO_BUF);

	cbm_eqm_rx_chan_open(DMA1RX_TMU_CLASS0, 7, CBM_PORT_F_STANDARD_BUF);
	cbm_eqm_rx_chan_open(DMA1RX_CBM_P8_CLASS5, 8, CBM_PORT_F_STANDARD_BUF);

	cbm_eqm_rx_chan_open(DMA1RX_CBM_P7_CLASS6_JUMBO, 7, CBM_PORT_F_JUMBO_BUF);
	cbm_eqm_rx_chan_open(DMA1RX_CBM_P8_CLASS11_JUMBO, 8, CBM_PORT_F_JUMBO_BUF);

	dev_info(dev,
		 "%s: CBM XRX500 ready (reserved-memory pools 18MiB std / 8MiB jbo)\n",
		 dev_name(dev));
	return 0;

err_clk:
	if (cbm_clk) {
		clk_disable_unprepare(cbm_clk);
		cbm_clk = NULL;
	}
err:
	cbm_pdev_cache = NULL;
	atomic_dec(&cbm_probe_count);
	return ret;
}

/*
 * cbm_xrx500_remove - tear down in reverse.
 *
 * This function only needs to undo what is NOT devm-tracked: -
 * clk_disable_unprepare (devm undoes _get, not _enable) - atomic_dec the
 * probe guard - clear the g_cbm_buff buffer pointers so a re-bind sees them
 * empty
 */
static void cbm_xrx500_remove(struct platform_device *pdev)
{
	(void)pdev;

	g_cbm_buff.jbo_buf_base = NULL;
	g_cbm_buff.jbo_buf_addr = NULL;
	g_cbm_buff.std_buf_base = NULL;
	g_cbm_buff.std_buf_addr = NULL;

	if (cbm_clk) {
		clk_disable_unprepare(cbm_clk);
		cbm_clk = NULL;
	}
	cbm_pdev_cache = NULL;
	atomic_dec(&cbm_probe_count);
}

/* of_match_table — AVM cqm/grx500/cbm.c:5480-5483. */
static const struct of_device_id cbm_xrx500_match[] = {
	{ .compatible = "lantiq,cbm-xrx500" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, cbm_xrx500_match);

static struct platform_driver cbm_xrx500_driver = {
	.probe  = cbm_xrx500_probe,
	.remove = cbm_xrx500_remove,
	.driver = {
		.name           = "lantiq-cbm-xrx500",
		.of_match_table = cbm_xrx500_match,
	},
};
module_platform_driver(cbm_xrx500_driver);

/*
 * Port of the vendor's cbm_intr_mapping_init (cbm.c:1208-1225). The
 * ENABLE_LL_DEBUG and CONFIG_CBM_LS_ENABLE variants are not built by default
 * and are not ported.
 */
int cbm_intr_mapping_init(void)
{
	/* egp_irnen-zero target lines per 0, 4, 5, 6. */
	static const u8 egp_zero_lines[] = { 0, 4, 5, 6 };
	int n;
	size_t i;
	u32 expected;
	u32 actual;

	if (!g_cbm_base) {
		pr_err("cbm: cbm_intr_mapping_init: g_cbm_base NULL\n");
		return -ENODEV;
	}

	/* (a) cbm_irnen writes — AVM cbm.c:1208/1212/1215/1219. */
	for (n = 4; n <= 7; n++) {
		expected = 0x100U << (n - 4);
		__raw_writel(expected, g_cbm_base + CBM_INT_LINE(n, cbm_irnen));
	}

	for (i = 0; i < ARRAY_SIZE(egp_zero_lines); i++)
		__raw_writel(0U, g_cbm_base + CBM_INT_LINE(egp_zero_lines[i],
							    egp_irnen));

	for (n = 4; n <= 7; n++) {
		expected = 0x100U << (n - 4);
		actual = __raw_readl(g_cbm_base + CBM_INT_LINE(n, cbm_irnen));
		WARN_ONCE(actual != expected,
			  "cbm: CBM_INT_LINE(%d, cbm_irnen) readback 0x%08x != expected 0x%08x\n",
			  n, actual, expected);
	}
	for (i = 0; i < ARRAY_SIZE(egp_zero_lines); i++) {
		actual = __raw_readl(g_cbm_base +
				     CBM_INT_LINE(egp_zero_lines[i], egp_irnen));
		WARN_ONCE(actual != 0U,
			  "cbm: CBM_INT_LINE(%u, egp_irnen) readback 0x%08x != expected 0x00000000\n",
			  (unsigned int)egp_zero_lines[i], actual);
	}

	return 0;
}

/*
 * cbm_interrupt_init — port of AVM cqm/grx500/cbm.c:2810-2870
 *                      adapted for linux-6.18.y devm semantics.
 *
 * Affinity: AVM sets per-VPE irq_set_affinity (cpumask 0x1 / 0x2 / 0x4 / 0x8)
 * on lines 1..4. Documented here so the omission is intentional, not an
 * oversight.
 *
 * Handler names: AVM uses "cbm_eqm" (line 0) and "cbm_dqm" (lines 1..4).
 *
 * Failure path: devm_request_irq returns the linux errno.
 *
 * Returns 0 on full success; negative errno (propagated from
 * devm_request_irq) on the first failing line.
 */
int cbm_interrupt_init(struct platform_device *pdev, int *irqs)
{
	static const irq_handler_t cbm_isr_table[5] = {
		cbm_isr_0,
		cbm_isr_4,
		cbm_isr_5,
		cbm_isr_6,
		cbm_isr_7,
	};
	static const char * const cbm_isr_names[5] = {
		"cbm_isr_0", "cbm_isr_4", "cbm_isr_5", "cbm_isr_6", "cbm_isr_7",
	};
	int n;
	int ret;

	if (!pdev || !irqs) {
		pr_err("cbm: cbm_interrupt_init: pdev=%p irqs=%p\n",
		       pdev, irqs);
		return -EINVAL;
	}

	for (n = 0; n < 5; n++) {
		ret = devm_request_irq(&pdev->dev, irqs[n], cbm_isr_table[n],
				       IRQF_SHARED, cbm_isr_names[n], pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"cbm: devm_request_irq %s (irq=%d) failed: %d\n",
				cbm_isr_names[n], irqs[n], ret);
			return ret;
		}
	}

	return 0;
}

/*
 * init_cbm_basic — port of AVM cqm/grx500/cbm.c:1231-1256.
 *
 *   (1) SBA_0 = __pa(std_buf_addr)                    // physical
 *   (2) JBA_0 = __pa(jbo_buf_addr)                    // physical
 *   (3) SBA_1 = CBM_IOCU_ADDR(__pa(std_buf_addr))     // KSEG1 IOCU alias
 *   (4) JBA_1 = CBM_IOCU_ADDR(__pa(jbo_buf_addr))     // KSEG1 IOCU alias
 *   (5) jsel = (jbo_frm_size == 0x2000) ? 0 : 1       // AVM cbm.c:1242
 *   (6) RMW CBM_CTRL[17] = jsel                        // preserve other bits
 *   (7) QEQCNT 256 u32 = 0                             // cbm_dw_memset 0x400 B
 *   (8) QDQCNT 256 u32 = 0                             // cbm_dw_memset 0x400 B
 *   (9) cbm_intr_mapping_init()                        // per-line masks
 *  (10) cbm_interrupt_init(pdev, irqs)                 // 5x devm_request_irq
 *
 * Step (7)/(8) cbm_dw_memset target: QEQCNT and QDQCNT live in their OWN
 * ioremap'd windows (g_cbm_qeqcnt_base / g_cbm_qdqcnt_base) — NOT within
 * g_cbm_base. The 'BASE' suffix in this file refers to offset-zero within
 * those windows.").
 *
 * Step (10) chains via the file-static cbm_pdev_cache / cbm_irqs[]
 * cookies populated by cbm_xrx500_probe. init_cbm_basic keeps the AVM
 * `void` signature (cbm.h declares it as `int init_cbm_basic(void)`),
 * matching AVM cbm.c:1231 where init_cbm_basic also reaches the IRQ
 * array via file-static g_cbm_irq[].
 *
 * Returns 0 on success, negative errno on init failure (propagated
 * from cbm_interrupt_init).
 */
int init_cbm_basic(void)
{
	phys_addr_t std_pa;
	phys_addr_t jbo_pa;
	int jsel;
	int ret;

	if (!g_cbm_base) {
		pr_err("cbm: init_cbm_basic: g_cbm_base NULL\n");
		return -ENODEV;
	}
	if (!g_cbm_qeqcnt_base || !g_cbm_qdqcnt_base) {
		pr_err("cbm: init_cbm_basic: QEQCNT/QDQCNT base NULL\n");
		return -ENODEV;
	}
	if (!g_cbm_buff.std_buf_addr || !g_cbm_buff.jbo_buf_addr) {
		pr_err("cbm: init_cbm_basic: std/jbo buffer pool not allocated\n");
		return -ENODEV;
	}
	if (!cbm_pdev_cache) {
		pr_err("cbm: init_cbm_basic: pdev cache empty (probe order bug)\n");
		return -ENODEV;
	}

	std_pa = cbm_buf_pool_phys_base(CBM_POOL_STD);
	jbo_pa = cbm_buf_pool_phys_base(CBM_POOL_JBO);

	if (!std_pa || !jbo_pa) {
		pr_err("cbm: init_cbm_basic: pool phys base zero (std=%pa jbo=%pa)\n",
		       &std_pa, &jbo_pa);
		return -ENODEV;
	}

	/* (a) AVM cbm.c:1236-1237 pr_info markers (entry trace). */
	pr_info("cbm: init_cbm_basic: PHY ADDR STD 0x%llx\n",
		(unsigned long long)std_pa);
	pr_info("cbm: init_cbm_basic: PHY ADDR JBO 0x%llx\n",
		(unsigned long long)jbo_pa);

	/* (1)-(4) Buffer pool base addresses. ORDER IS LOAD-BEARING:
	 * SBA_0 / JBA_0 first (physical), then SBA_1 / JBA_1 (IOCU
	 * alias). cbm_dp_enable and the silicon descriptor engine read
	 * all four during DMA setup; any reorder against the jsel write
	 * below would expose a window where the engine could observe
	 * inconsistent (SBA_*, JSEL) state.
	 */
	__raw_writel((u32)std_pa, g_cbm_base + CBM_SBA_0);
	__raw_writel((u32)jbo_pa, g_cbm_base + CBM_JBA_0);
	__raw_writel((u32)CBM_IOCU_ADDR(std_pa),
		     g_cbm_base + CBM_SBA_1);
	__raw_writel((u32)CBM_IOCU_ADDR(jbo_pa),
		     g_cbm_base + CBM_JBA_1);

	/* (5) jsel — AVM cbm.c:1242 verbatim. */
	jsel = (g_cbm_buff.jbo_frm_size == 0x2000) ? 0 : 1;

	/* (6) RMW CBM_CTRL[17] = jsel — cbm_set_val preserves the other
	 * 31 bits of the control register (read-modify-write semantics
	 * verbatim from AVM cqm/cqm_common.h:203-210).
	 */
	cbm_set_val(g_cbm_base + CBM_CTRL, (u32)jsel, JSEL_MASK, JSEL_POS);

	/* (7)/(8) Zero the QEQCNT/QDQCNT counter rings (256 u32 = 0x400
	 * bytes each). The (u32 __force *) cast bridges void __iomem *
	 * to cbm_dw_memset's u32 * param — cbm_dw_memset uses
	 * __raw_writel internally so MMIO semantics are preserved.
	 */
	cbm_dw_memset((u32 __force *)g_cbm_qeqcnt_base, 0, CBM_QEQCNT_SIZE);
	cbm_dw_memset((u32 __force *)g_cbm_qdqcnt_base, 0, CBM_QDQCNT_SIZE);

	/* (9) Per-line cbm_irnen / egp_irnen masks (AVM cbm.c:1208-1225). */
	ret = cbm_intr_mapping_init();
	if (ret) {
		pr_err("cbm: cbm_intr_mapping_init failed: %d\n", ret);
		return ret;
	}

	ret = cbm_interrupt_init(cbm_pdev_cache, cbm_irqs);
	if (ret) {
		pr_err("cbm: cbm_interrupt_init failed: %d\n", ret);
		return ret;
	}

	pr_info("cbm: init basic CBM successfully (jsel=%d)\n", jsel);
	return 0;
}

u8 get_lookup_qid_via_index(u32 lookup_idx)
{
	(void)lookup_idx;
	return 0;
}
EXPORT_SYMBOL_GPL(get_lookup_qid_via_index);
