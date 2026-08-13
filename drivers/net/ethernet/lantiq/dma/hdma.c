// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2014 ~ 2015 Lei Chuanhua <chuanhua.lei@lantiq.com>
 * Copyright (C) 2016 ~ 2017 Intel Corporation.
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/dma/intel/hdma.c.
 *
 * xRX500 high-speed DMA controllers: controller and channel configuration,
 * descriptor rings, the interrupt chip and the per-port channel tables that
 * pair a CBM dequeue port with a DMA channel.
 */

#define pr_fmt(fmt) "hdma: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/build_bug.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/bitmap.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqdesc.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "hdma.h"
#include "hdma_regs.h"

/*
 * itoc — extract struct dma_ctrl from an irq_data via the per-irq chip
 * data. AVM hdma.c:212 verbatim. Used by the per-irq chip ops
 * (dma_disable_irq / dma_enable_irq / dma_ack_irq / dma_mask_and_ack_irq).
 * The chip data is set per-irq by dma_irq_map() (the irq_domain map
 * callback) at irq_domain_create_legacy() time.
 */
#define itoc(i) ((struct dma_ctrl *)irq_get_chip_data((i)->irq))

/*
 * dma_name[] — AVM hdma.c:215-223 verbatim. Indexed by cid (the value of enum
 * dma_controller).
 */
static const char *const dma_name[] = {
	"dma0",
	"dma1tx",
	"dma1rx",
	"dma2tx",
	"dma2rx",
	"dma3",
	"dma4",
};

/*
 * ltq_dma_controller[] — per-cid storage for the struct dma_ctrl bound by
 * ltq_dma_probe. AVM hdma.c:238 verbatim.
 */
static struct dma_ctrl ltq_dma_controller[DMAMAX];

/* Handy dma register accessors — AVM hdma.c:247-262 verbatim. */
static inline unsigned int ltq_dma_r32(struct dma_ctrl *pctrl, u32 offset)
{
	return ioread32(pctrl->membase + offset);
}

static inline void ltq_dma_w32(struct dma_ctrl *pctrl, u32 value, u32 offset)
{
	iowrite32(value, pctrl->membase + offset);
}

static inline void ltq_dma_w32_mask(struct dma_ctrl *pctrl, u32 clr, u32 set,
				    u32 offset)
{
	ltq_dma_w32(pctrl, (ltq_dma_r32(pctrl, offset) & ~clr) | set, offset);
}

/* AVM hdma.c:4093-4118 verbatim. */
static u32 burst_len_to_burst_cfg(int val)
{
	u32 burst = DMA_BURSTL_16DW;

	switch (val) {
	case 2:
		burst = DMA_BURSTL_2DW;
		break;
	case 4:
		burst = DMA_BURSTL_4DW;
		break;
	case 8:
		burst = DMA_BURSTL_8DW;
		break;
	case 16:
		burst = DMA_BURSTL_16DW;
		break;
	case 32:
		burst = DMA_BURSTL_32DW;
		break;
	default:
		burst = DMA_BURSTL_16DW;
		break;
	}
	return burst;
}

/* AVM hdma.c:242-245 verbatim. */
static char const *dma_get_name_by_cid(int cid)
{
	return dma_name[cid];
}

/* AVM hdma.c:269-274 verbatim. */
static struct dma_ctrl *dma_port_get_controller(struct dma_port *port)
{
	if (!port)
		return NULL;
	return port->controller;
}

/* AVM hdma.c:276-281 verbatim. */
static struct dma_ctrl *dma_chan_get_controller(struct dmax_chan *pch)
{
	if (!pch)
		return NULL;
	return pch->controller;
}

/* AVM hdma.c:301-306 verbatim. */
static struct dma_port *dma_chan_get_port(struct dmax_chan *pch)
{
	if (!pch)
		return NULL;
	return pch->port;
}

/* AVM hdma.c:352-357 verbatim. */
static int dma_set_port_controller_data(struct dma_port *port,
					struct dma_ctrl *pctrl)
{
	port->controller = pctrl;
	return 0;
}

/* AVM hdma.c:359-364 verbatim. */
static int dma_set_chan_controller_data(struct dmax_chan *pch,
					struct dma_ctrl *pctrl)
{
	pch->controller = pctrl;
	return 0;
}

/* AVM hdma.c:366-370 verbatim. */
static int dma_set_chan_port_data(struct dmax_chan *pch, struct dma_port *port)
{
	pch->port = port;
	return 0;
}

/* AVM hdma.c:372-374 verbatim. */
static inline bool dma_is_64bit(struct dma_ctrl *ctrl)
{
	return (ctrl->flags & DMA_CTL_64BIT) ? true : false;
}

/* AVM hdma.c:377-380 verbatim. */
static inline bool dma_chan_tx(struct dmax_chan *pch)
{
	return (pch->flags & DMA_TX_CH) ? true : false;
}

/* AVM hdma.c:382-385 verbatim. */
static inline bool dma_chan_rx(struct dmax_chan *pch)
{
	return (pch->flags & DMA_RX_CH) ? true : false;
}

/*
 * dma_ctrl_reset — AVM hdma.c:407-415 verbatim. Pulses DMA_CTRL_RST and
 * waits 1ms for the silicon self-clear. The RST bit is self-clearing per
 * the GRX350 reference manual; the udelay ensures the writeback observes
 * the cleared state before dma_ctrl_cfg starts programming.
 */
static void dma_ctrl_reset(struct dma_ctrl *pctrl)
{
	unsigned long flags;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_RST, DMA_CTRL);
	udelay(1000);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* DMA controller related configuration — AVM hdma.c:418-426 verbatim. */
static void dma_ctrl_pkt_arb_cfg(struct dma_ctrl *pctrl, int enable)
{
	if (enable) {
		ltq_dma_w32_mask(pctrl, DMA_CTRL_MBRSTARB, 0, DMA_CTRL);
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_PKTARB, DMA_CTRL);
	} else {
		ltq_dma_w32_mask(pctrl, DMA_CTRL_PKTARB, 0, DMA_CTRL);
	}
}

/* AVM hdma.c:428-451 verbatim. */
static void dma_ctrl_arb_cfg(struct dma_ctrl *pctrl)
{
	unsigned long flags;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	switch (pctrl->arb_type) {
	case DMA_ARB_BURST:
		ltq_dma_w32_mask(pctrl, DMA_CTRL_PKTARB, 0, DMA_CTRL);
		ltq_dma_w32_mask(pctrl, DMA_CTRL_MBRSTARB, 0, DMA_CTRL);
		break;
	case DMA_ARB_MUL_BURST:
		ltq_dma_w32_mask(pctrl, DMA_CTRL_PKTARB, 0, DMA_CTRL);
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_MBRSTARB, DMA_CTRL);
		ltq_dma_w32_mask(pctrl, DMA_CTRL_MBRSTARB,
				 SM(DMA_ARB_MUL_BURST_DEFAULT,
				    DMA_CTRL_MBRST_CNT),
				 DMA_CTRL);
		break;
	case DMA_ARB_PKT:
		dma_ctrl_pkt_arb_cfg(pctrl, 1);
		break;
	}
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/*
 * dma_ctrl_sram_desc_cfg — AVM hdma.c:453-466. DMA1RX / DMA2TX still silently
 * return.
 */
static void dma_ctrl_sram_desc_cfg(struct dma_ctrl *pctrl, int enable)
{
	unsigned long flags;

	if ((pctrl->cid != DMA3) && (pctrl->cid != DMA4) &&
	    (pctrl->cid != DMA1TX))
		return;
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_DSRAM_PATH, DMA_CTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CTRL_DSRAM_PATH, 0, DMA_CTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* AVM hdma.c:468-480 verbatim. */
static void dma_ctrl_chan_flow_ctl_cfg(struct dma_ctrl *pctrl, int enable)
{
	unsigned long flags;

	if ((pctrl->cid != DMA1TX) && (pctrl->cid != DMA2TX))
		return;
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_CH_FL, DMA_CTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CTRL_CH_FL, 0, DMA_CTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* AVM hdma.c:482-492 verbatim. */
static void dma_ctrl_global_polling_enable(struct dma_ctrl *pctrl)
{
	u32 reg = 0;
	unsigned long flags;

	reg |= DMA_CPOLL_EN;
	reg |= (u32)(SM(pctrl->pollcnt, DMA_CPOLL_CNT));
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32_mask(pctrl, DMA_CPOLL_CNT, reg, DMA_CPOLL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/*
 * dma_ctrl_desc_fetch_on_demand_cfg — AVM hdma.c:494-509. DMA1TX / DMA1RX /
 * DMA2TX fall through as before.
 */
static void dma_ctrl_desc_fetch_on_demand_cfg(struct dma_ctrl *pctrl,
					      int enable)
{
	unsigned long flags;

	if ((pctrl->cid == DMA0) || (pctrl->cid == DMA3) ||
	    (pctrl->cid == DMA4))
		return;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_DS_FOD, DMA_CTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CTRL_DS_FOD, 0, DMA_CTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* AVM hdma.c:511-521 verbatim. */
static void dma_ctrl_desc_read_back_cfg(struct dma_ctrl *pctrl, int enable)
{
	unsigned long flags;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_DRB, DMA_CTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CTRL_DRB, 0, DMA_CTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* AVM hdma.c:523-533 verbatim. */
static void dma_ctrl_byte_enable_cfg(struct dma_ctrl *pctrl, int enable)
{
	unsigned long flags;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CTRL_ENBE, DMA_CTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CTRL_ENBE, 0, DMA_CTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* AVM hdma.c:535-550 verbatim. DMA1TX only — silent no-op on others. */
static void dma_ctrl_labcnt_cfg(struct dma_ctrl *pctrl)
{
	u32 reg = 0;
	unsigned long flags;

	if (pctrl->cid != DMA1TX)
		return;

	if (pctrl->labcnt <= 0)
		return;
	reg |= DMA_CTRL_PRELOAD_EN;
	reg |= (u32)(SM(pctrl->labcnt, DMA_CTRL_PRELOAD_INT));
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32_mask(pctrl, DMA_CTRL_PRELOAD_INT, reg, DMA_CTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/*
 * dma_ctrl_orrc_cfg — AVM hdma.c:557-575 verbatim. ORRC is valid for
 * V3.0 + TX/memcopy controllers only; DMA1RX/DMA2RX (RX controllers)
 * silently return.
 *   orr_cnt >= 16 → 16; 4 <= orr_cnt < 16 → orr_cnt; orr_cnt < 4 → 3
 *   (silicon minimum). Out-of-range disables ORRC entirely.
 */
static void dma_ctrl_orrc_cfg(struct dma_ctrl *pctrl)
{
	u32 val = 0;

	if (pctrl->ver <= DMA_VER_22)
		return;

	/* Only valid for DMA TX and memory copy DMA. */
	if (pctrl->cid == DMA1RX || pctrl->cid == DMA2RX)
		return;

	if (pctrl->orrc <= 0 || pctrl->orrc > DMA_ORRC_MAX_CNT) {
		ltq_dma_w32_mask(pctrl, DMA_ORRC_EN | DMA_ORRC_ORRCNT, 0,
				 DMA_ORRC);
	} else {
		val = DMA_ORRC_EN | SM(pctrl->orrc, DMA_ORRC_ORRCNT);
		ltq_dma_w32(pctrl, val, DMA_ORRC);
	}
}

/* AVM hdma.c:577-617 verbatim. */
static int dma_ctrl_cfg(struct dma_ctrl *pctrl)
{
	int enable;

	if ((pctrl->flags & DMA_FLCTL))
		enable = 1;
	else
		enable = 0;
	dma_ctrl_chan_flow_ctl_cfg(pctrl, enable);

	if ((pctrl->flags & DMA_FTOD))
		enable = 1;
	else
		enable = 0;
	dma_ctrl_desc_fetch_on_demand_cfg(pctrl, enable);

	if ((pctrl->flags & DMA_DESC_IN_SRAM))
		enable = 1;
	else
		enable = 0;
	dma_ctrl_sram_desc_cfg(pctrl, enable);

	dma_ctrl_arb_cfg(pctrl);
	if ((pctrl->flags & DMA_DRB))
		enable = 1;
	else
		enable = 0;
	dma_ctrl_desc_read_back_cfg(pctrl, enable);

	if ((pctrl->flags & DMA_EN_BYTE_EN))
		enable = 1;
	else
		enable = 0;
	dma_ctrl_byte_enable_cfg(pctrl, enable);
	dma_ctrl_labcnt_cfg(pctrl);
	dma_ctrl_orrc_cfg(pctrl);
	dma_ctrl_global_polling_enable(pctrl);
	dev_dbg(pctrl->dev, "%s Controller 0x%08x configuration done\n",
		pctrl->name, ltq_dma_r32(pctrl, DMA_CTRL));
	return 0;
}

/* AVM hdma.c:619-641 verbatim. */
static int dma_chan_cctrl_cfg(struct dmax_chan *pch, u32 val)
{
	u32 reg;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);
	unsigned long flags;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	reg = ltq_dma_r32(pctrl, DMA_CCTRL);

	/* Read from hardware */
	if ((reg & DMA_CCTRL_DIR_TX))
		pch->flags |= DMA_TX_CH;
	else
		pch->flags |= DMA_RX_CH;

	/* Keep the class value unchanged */
	reg &= (DMA_CCTRL_CLASS | DMA_CCTRL_CLASSH);
	val |= reg;
	ltq_dma_w32(pctrl, val, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	return 0;
}

/* AVM hdma.c:661-683 verbatim. */
static int dma_chan_set_class(struct dmax_chan *pch, u32 val)
{
	unsigned long flags;
	u32 class_val;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	if (pctrl->ver <= DMA_VER_22)
		return -EPERM;

	if (val > DMA_MAX_CLASS)
		return -EINVAL;
	/* 3 bits low */
	class_val = SM((val & 0x7), DMA_CCTRL_CLASS);
	/* 2 bits high */
	class_val |= SM(((val >> 3) & 0x3), DMA_CCTRL_CLASSH);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32_mask(pctrl, DMA_CCTRL_CLASS | DMA_CCTRL_CLASSH,
			 class_val, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	return 0;
}

/* AVM hdma.c:2124-2163 verbatim. */
static int dma_port_cfg(struct dma_port *port)
{
	u32 reg = 0;
	unsigned long flags;
	struct dma_ctrl *pctrl;
	int cid;

	if (!port)
		return -EINVAL;
	pctrl = dma_port_get_controller(port);
	cid = pctrl->cid;
	reg |= (port->flush_memcpy ? DMA_PCTRL_MEM_FLUSH : 0);
	reg |= (port->txwgt << DMA_PCTRL_TXWGT_S);
	reg |= (port->txendi << DMA_PCTRL_TXENDI_S);
	reg |= (port->rxendi << DMA_PCTRL_RXENDI_S);
	reg |= (port->pkt_drop << DMA_PCTRL_PDEN_S);

	if (port->txbl == DMA_BURSTL_32DW)
		reg |= DMA_PCTRL_TXBL32;
	else if (port->txbl == DMA_BURSTL_16DW)
		reg |= DMA_PCTRL_TXBL16;
	else
		reg |= (port->txbl << DMA_PCTRL_TXBL_S);

	if (port->rxbl == DMA_BURSTL_32DW)
		reg |= DMA_PCTRL_RXBL32;
	else if (port->rxbl == DMA_BURSTL_16DW)
		reg |= DMA_PCTRL_RXBL16;
	else
		reg |= (port->rxbl << DMA_PCTRL_RXBL_S);
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, port->pid, DMA_PS);
	ltq_dma_w32(pctrl, reg, DMA_PCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	spin_lock_init(&port->port_lock);
	dev_dbg(pctrl->dev, "%s Port %d %s Control 0x%08x configuration done\n",
		dma_name[cid], port->pid, port->name,
		ltq_dma_r32(pctrl, DMA_PCTRL));
	return 0;
}

/*
 * dma_chan_cfg - AVM hdma.c:2179-2233 with the auto-descriptor-allocation
 * tail (AVM:2225-2228, the dma_chan_desc_alloc + dma_chan_data_buf_alloc call
 * pair guarded by !dma_chan_desc_alloc_by_device) STRIPPED.
 *
 * Writes happen inside the DMA_CS-selected window (silicon requires DMA_CS to
 * point at the channel before per-channel register writes take effect).
 */

static char *common_buffer_alloc(int len, int *byte_offset, void **opt)
{
	char *buffer = kmalloc(len, GFP_ATOMIC);

	*byte_offset = 0;
	return buffer;
}

static int common_buffer_free(char *dataptr, void *opt)
{
	kfree(dataptr);
	return 0;
}

static int dma_chan_cfg(struct dmax_chan *pch)
{
	u32 reg = 0;
	int cid;
	int pid;
	unsigned long flags;
	struct dma_ctrl *pctrl;
	struct dma_port *pport;

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);
	pport = dma_chan_get_port(pch);

	if (unlikely(!pport))
		return -EINVAL;

	cid = pctrl->cid;
	pid = pport->pid;
	reg |= (pch->lpbk_ch_nr << DMA_CCTRL_LBCHNR_S);
	reg |= (pch->lpbk_en ? DMA_CCTRL_LBEN : 0);
	reg |= (pch->p2pcpy ? DMA_CCTRL_P2PCPY : 0);
	reg |= (pch->pden ? DMA_CCTRL_PDEN : 0);

	reg |= (pch->txwgt << DMA_CCTRL_TXWGT_S);

	reg |= (pch->onoff ? DMA_CCTRL_ON : 0);
	reg |= (pch->rst ? DMA_CCTRL_RST : 0);
	dma_chan_cctrl_cfg(pch, reg);
	dma_chan_set_class(pch, pch->nr);
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	/* Clear all interrupts and disable. */
	ltq_dma_w32(pctrl, 0, DMA_CIE);
	ltq_dma_w32(pctrl, DMA_CI_ALL, DMA_CIS);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);

	/* Disable related top-level interrupts. */
	if (pch->nr < 32) {
		ltq_dma_w32_mask(pctrl, (1u << (pch->nr & 0x1f)), 0,
				 DMA_IRNEN);
		ltq_dma_w32_mask(pctrl, 0, (1u << (pch->nr & 0x1f)),
				 DMA_IRNCR);
	} else {
		ltq_dma_w32_mask(pctrl, (1u << (pch->nr & 0x1f)), 0,
				 DMA_IRNEN1);
		ltq_dma_w32_mask(pctrl, 0, (1u << (pch->nr & 0x1f)),
				 DMA_IRNCR1);
	}
	pch->alloc = &common_buffer_alloc;
	pch->free = &common_buffer_free;
	mutex_init(&pch->ch_lock);
	spin_lock_init(&pch->irq_lock);
	dev_dbg(pctrl->dev, "%s port %d chan %d done\n",
		dma_name[cid], pid, pch->nr);
	return 0;
}

/*
 * do_dma_tasklet - AVM hdma.c:2907-2957 with the DMA0 dispatch branch
 * (AVM:2921-2934, the !dma_is_64bit special-case for the 8-DW-desc
 * controller) STRIPPED.
 *
 * The per-iteration clear_bit happens BEFORE invoking the callback so a
 * re-entrant set_bit from a fast follow-up IRQ on the same channel is
 * preserved (the bit gets re-set and the next iteration picks it up).
 */
static void do_dma_tasklet(unsigned long data)
{
	int ch_nr;
	int flags;
	struct dma_port *pport;
	struct dmax_chan *pch;
	struct dma_ctrl *pctrl = (struct dma_ctrl *)data;
	int budget = pctrl->budget;

	while (!bitmap_empty(pctrl->dma_int_status, pctrl->chans)) {
		ch_nr = find_first_bit(pctrl->dma_int_status, pctrl->chans);
		if (budget-- < 0) {
			tasklet_schedule(&pctrl->dma_tasklet);
			return;
		}
		pport = &pctrl->ports[0];
		pch = &pport->chans[ch_nr];
		clear_bit(ch_nr, pctrl->dma_int_status);
		if (pch->intr_handler) {
			flags = dma_chan_tx(pch) ? TRANSMIT_CPT_INT : RCV_INT;
			pch->intr_handler(pch->lnr, pch->priv, flags);
		}
	}
	/*
	 * Sanity check: a new IRQ may have raced in during the gap between
	 * the while loop exit and the in-process clear.
	 */
	atomic_set(&pctrl->dma_in_process, 0);
	if (!bitmap_empty(pctrl->dma_int_status, pctrl->chans)) {
		atomic_set(&pctrl->dma_in_process, 1);
		tasklet_schedule(&pctrl->dma_tasklet);
	}
}

/* AVM hdma.c:2959-2962 verbatim. */
static inline int dma_irq_to_chan_nr(struct dma_ctrl *pctrl, int irq)
{
	return irq - pctrl->irq_base;
}

/* dma_chan_interrupt — AVM hdma.c:2964-2988 verbatim. */
static irqreturn_t dma_chan_interrupt(int irq, void *dev_id)
{
	unsigned long flags;
	struct dmax_chan *pch = (struct dmax_chan *)dev_id;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	/* Disable this channel interrupt. */
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32_mask(pctrl, DMA_CI_ALL, 0, DMA_CIE);
	/* Record and clear status to prevent dummy interrupts (level irq). */
	pch->cis = ltq_dma_r32(pctrl, DMA_CIS);
	ltq_dma_w32(pctrl, DMA_CI_ALL, DMA_CIS);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);

	/* Record this channel interrupt for the tasklet. */
	set_bit(dma_irq_to_chan_nr(pctrl, irq), pctrl->dma_int_status);

	/* If not in process, invoke the tasklet. */
	if (!atomic_read(&pctrl->dma_in_process)) {
		atomic_set(&pctrl->dma_in_process, 1);
		tasklet_schedule(&pctrl->dma_tasklet);
	}
	return IRQ_HANDLED;
}

/*
 * AVM's `cn > 32` is an off-by-one — it sends cn==32 to the low bank, but
 * cn==32 is bit 0 of the high bank. The dma_chan_cfg boundary at AVM:2213
 * uses the correct `nr < 32`, so the AVM source itself is inconsistent.
 */
static void dma_disable_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int reg_off;
	struct dma_ctrl *pctrl = itoc(d);
	unsigned int cn;

	if (unlikely(!pctrl))
		return;

	cn = d->irq - pctrl->irq_base;
	reg_off = (cn >= 32) ? DMA_IRNEN1 : DMA_IRNEN;
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32_mask(pctrl, BIT((cn & 0x1f)), 0, reg_off);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

static void dma_enable_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int reg_off;
	struct dma_ctrl *pctrl = itoc(d);
	unsigned int cn;

	if (unlikely(!pctrl))
		return;

	cn = d->irq - pctrl->irq_base;
	reg_off = (cn >= 32) ? DMA_IRNEN1 : DMA_IRNEN;
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32_mask(pctrl, 0, BIT((cn & 0x1f)), reg_off);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

static void dma_ack_irq(struct irq_data *d)
{
	unsigned int reg_off;
	struct dma_ctrl *pctrl = itoc(d);
	unsigned int cn;

	if (unlikely(!pctrl))
		return;

	cn = d->irq - pctrl->irq_base;
	reg_off = (cn >= 32) ? DMA_IRNCR1 : DMA_IRNCR;
	ltq_dma_w32(pctrl, BIT((cn & 0x1f)), reg_off);
}

static void dma_mask_and_ack_irq(struct irq_data *d)
{
	unsigned long flags;
	unsigned int reg_off;
	struct dma_ctrl *pctrl = itoc(d);
	unsigned int cn;

	if (unlikely(!pctrl))
		return;

	cn = d->irq - pctrl->irq_base;
	reg_off = (cn >= 32) ? DMA_IRNEN1 : DMA_IRNEN;
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32_mask(pctrl, BIT((cn & 0x1f)), 0, reg_off);
	reg_off = (cn >= 32) ? DMA_IRNCR1 : DMA_IRNCR;
	ltq_dma_w32(pctrl, BIT((cn & 0x1f)), reg_off);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/*
 * dma_irq_handler — AVM hdma.c:3098-3120 verbatim. Parent chained
 * handler: reads the top-level IRNCR (and IRNCR1 when chans > 32) and
 * dispatches to the per-channel virq via generic_handle_irq().
 */
static void dma_irq_handler(struct irq_desc *desc)
{
	int offset;
	unsigned long irncr;
	unsigned long irncr1 = 0;
	struct dma_ctrl *pctrl = irq_desc_get_handler_data(desc);

	if (unlikely(!pctrl))
		return;
	irncr = ltq_dma_r32(pctrl, DMA_IRNCR);
	if (pctrl->chans > 32)
		irncr1 = ltq_dma_r32(pctrl, DMA_IRNCR1);

	if (pctrl->chans <= 32) {
		for_each_set_bit(offset, &irncr, pctrl->chans)
			generic_handle_irq(pctrl->irq_base + offset);
	} else {
		for_each_set_bit(offset, &irncr, 32)
			generic_handle_irq(pctrl->irq_base + offset);
		for_each_set_bit(offset, &irncr1, pctrl->chans - 32)
			generic_handle_irq(pctrl->irq_base + 32 + offset);
	}
}

/* AVM hdma.c:3122-3128 verbatim. */
static struct irq_chip dma_irq_chip = {
	.name = "dma_irq",
	.irq_mask = dma_disable_irq,
	.irq_unmask = dma_enable_irq,
	.irq_ack = dma_ack_irq,
	.irq_mask_ack = dma_mask_and_ack_irq,
};

/*
 * dma_irq_map — AVM hdma.c:3130-3140 verbatim (modulo dev_dbg call
 * adapted). Sets the per-irq chip + handler + chip_data so itoc() works
 * in the disable/enable/ack callbacks.
 */
static int dma_irq_map(struct irq_domain *d, unsigned int irq,
		       irq_hw_number_t hw)
{
	struct dma_ctrl *pctrl = d->host_data;

	irq_set_chip_and_handler_name(irq, &dma_irq_chip,
				      handle_level_irq, "mux");
	irq_set_chip_data(irq, pctrl);
	dev_dbg(pctrl->dev, "dma_irq_map irq %d --> hw %ld\n", irq, hw);
	return 0;
}

/* AVM hdma.c:3142-3145 verbatim. */
static const struct irq_domain_ops dma_irq_domain_ops = {
	.xlate = irq_domain_xlate_onetwocell,
	.map = dma_irq_map,
};

/*
 * dma_irq_chip_init - AVM hdma.c:3153-3181 with v6.18 API translations A: -
 * the legacy 'irq-domain-add-legacy' helper (renamed in v5.18) is replaced
 * with irq_domain_create_legacy + explicit fwnode argument
 * (of_node_to_fwnode(node)).
 */
static int dma_irq_chip_init(struct dma_ctrl *pctrl)
{
	struct resource irqres;
	struct device_node *node = pctrl->dev->of_node;
	struct irq_domain *domain;

	if (of_irq_to_resource_table(node, &irqres, 1) != 1)
		return 0;

	pctrl->irq_base = irq_alloc_descs(-1, 0, pctrl->chans,
					  numa_node_id());
	if ((int)pctrl->irq_base < 0) {
		dev_err(pctrl->dev, "Can't allocate IRQ numbers\n");
		return -ENODEV;
	}
	pctrl->chained_irq = irqres.start;
	domain = irq_domain_create_legacy(of_node_to_fwnode(node),
					  pctrl->chans, pctrl->irq_base, 0,
					  &dma_irq_domain_ops, pctrl);
	if (!domain) {
		dev_err(pctrl->dev, "Can't create legacy IRQ domain\n");
		return -ENODEV;
	}

	/*
	 * v6.18: irq_set_chained_handler_and_data folds the AVM
	 * legacy-helper + irq_set_handler_data + irq_set_chained_handler
	 * sequence into one call.
	 */
	irq_set_chained_handler_and_data(irqres.start, dma_irq_handler,
					 pctrl);
	return 0;
}

/*
 * dma_cfg_init() - AVM hdma.c:4120-4319 with the DMA0 / DMA3 / DMA4 branches
 * stripped.
 */
static int dma_cfg_init(struct dma_ctrl *pctrl)
{
	int i;
	u32 prop;
	u32 chan_fc = 0;
	u32 desc_fod = 0;
	u32 desc_insram = 0;
	u32 dma_drb = 0;
	u32 byte_en = 0;
	u32 txendi;
	u32 rxendi;
	struct dma_port *pport;
	struct dmax_chan *pch;
	struct device_node *node = pctrl->dev->of_node;

	pctrl->flags |= DMA_CTL_64BIT;

	pctrl->burst_mask = DMA_4DW_DESC_MASK;
	pctrl->desc_size = DMA_4DW_DESC_SIZE;

	if (!of_property_read_u32(node, "lantiq,dma-pkt-arb", &prop)) {
		if (prop > DMA_ARB_MAX)
			pctrl->arb_type = DMA_ARB_PKT;
		else
			pctrl->arb_type = prop;
	} else {
		pctrl->arb_type = DMA_ARB_PKT;
	}

	if (!of_property_read_u32(node, "lantiq,dma-chan-fc", &chan_fc)) {
		if (chan_fc)
			pctrl->flags |= DMA_FLCTL;
		else
			pctrl->flags &= ~DMA_FLCTL;
	}

	if (!of_property_read_u32(node, "lantiq,dma-desc-fod", &desc_fod)) {
		if (desc_fod)
			pctrl->flags |= DMA_FTOD;
		else
			pctrl->flags &= ~DMA_FTOD;
	}

	if (!of_property_read_u32(node, "lantiq,dma-desc-in-sram",
				  &desc_insram)) {
		if (desc_insram)
			pctrl->flags |= DMA_DESC_IN_SRAM;
		else
			pctrl->flags &= ~DMA_DESC_IN_SRAM;
	}

	if (!of_property_read_u32(node, "lantiq,dma-drb", &dma_drb)) {
		if (dma_drb)
			pctrl->flags |= DMA_DRB;
		else
			pctrl->flags &= ~DMA_DRB;
	}

	if (!of_property_read_u32(node, "lantiq,dma-byte-en", &byte_en)) {
		if (byte_en)
			pctrl->flags |= DMA_EN_BYTE_EN;
		else
			pctrl->flags &= ~DMA_EN_BYTE_EN;
	}

	if (!of_property_read_u32(node, "lantiq,dma-polling-cnt", &prop))
		pctrl->pollcnt = prop;
	else
		pctrl->pollcnt = DMA_GLOBAL_POLLING_DEFAULT_INTERVAL;

	if (!of_property_read_u32(node, "lantiq,dma-lab-cnt", &prop))
		pctrl->labcnt = prop;
	else
		pctrl->labcnt = 0;

	if (!of_property_read_u32(node, "lantiq,dma-orrc", &prop))
		pctrl->orrc = prop;
	else
		pctrl->orrc = 0;

	if (!of_property_read_u32(node, "lantiq,dma-txendi", &prop))
		txendi = prop;
	else
		txendi = DMA_DEFAULT_ENDIAN;

	if (!of_property_read_u32(node, "lantiq,dma-rxendi", &prop))
		rxendi = prop;
	else
		rxendi = DMA_DEFAULT_ENDIAN;

	dev_dbg(pctrl->dev,
		"arb %d fc %d fod %d insram %d drb %d ben %d pcnt %d labcnt %d orrc %d\n",
		pctrl->arb_type, chan_fc, desc_fod, desc_insram, dma_drb,
		byte_en, pctrl->pollcnt, pctrl->labcnt, pctrl->orrc);

	if (!of_property_read_u32(node, "lantiq,budget", &prop))
		pctrl->budget = prop;
	else
		pctrl->budget = DMA_IRQ_BUDGET;

	pctrl->ports = devm_kzalloc(pctrl->dev,
				    pctrl->port_nrs * sizeof(*pport),
				    GFP_KERNEL);
	if (!pctrl->ports)
		return -ENOMEM;

	pport = &pctrl->ports[0];
	pport->chan_nrs = pctrl->chans;
	pport->pid = 0;
	pport->name = dma_name[pctrl->cid];
	pport->rxendi = rxendi;
	pport->txendi = txendi;

	if (!of_property_read_u32(node, "lantiq,dma-burst", &prop)) {
		u32 burst = burst_len_to_burst_cfg(prop);

		pport->rxbl = burst;
		pport->txbl = burst;
	} else {
		pport->rxbl = DMA_DEFAULT_BURST;
		pport->txbl = DMA_DEFAULT_BURST;
	}
	pport->txwgt = DMA_TX_PORT_DEFAULT_WEIGHT;
	pport->pkt_drop = DMA_PKT_DROP_DISABLE;
	pport->flush_memcpy = 0;

	pport->chans = devm_kzalloc(pctrl->dev,
				    pport->chan_nrs * sizeof(*pch),
				    GFP_KERNEL);
	if (!pport->chans) {
		devm_kfree(pctrl->dev, pctrl->ports);
		return -ENOMEM;
	}
	for (i = 0; i < pport->chan_nrs; i++) {
		pch = &pport->chans[i];
		pch->flags |= DEVICE_ALLOC_DESC;
		pch->nr = i;
		pch->onoff = DMA_CH_OFF;
		pch->rst = DMA_CHAN_RST;
		pch->pkt_size = DMA_PKT_SIZE_DEFAULT;
		pch->opt = NULL;
		pch->lnr = -1;
		pch->cis = 0;
		pch->desc_configured = false;
	}
	return 0;
}

/* dma_ctrl_init - full body. */
static int dma_ctrl_init(struct dma_ctrl *pctrl)
{
	u32 i, j;
	int ret;
	struct dma_port *pport = NULL;
	struct dmax_chan *pch = NULL;

	dev_dbg(pctrl->dev,
		"struct dma_ctrl size %zu port %zu chan %zu\n",
		sizeof(*pctrl), sizeof(*pport), sizeof(*pch));
	spin_lock_init(&pctrl->ctrl_lock);
	bitmap_zero(pctrl->dma_int_status, MAX_DMA_CHAN_PER_PORT);
	atomic_set(&pctrl->dma_in_process, 0);

	dma_ctrl_reset(pctrl);

	dma_ctrl_cfg(pctrl);

	/*
	 * dma_irq_chip_init MUST complete BEFORE the per-channel
	 * devm_request_irq loop runs (else the irq_domain isn't yet wired and
	 * pctrl->irq_base is undefined).
	 */
	ret = dma_irq_chip_init(pctrl);
	if (ret)
		return ret;

	/*
	 * tasklet_init BEFORE the devm_request_irq loop — a child IRQ
	 * could fire (and dma_chan_interrupt schedule the tasklet) as soon
	 * as request_irq returns.
	 */
	tasklet_init(&pctrl->dma_tasklet, do_dma_tasklet,
		     (unsigned long)pctrl);

	for (i = 0; i < pctrl->port_nrs; i++) {
		pport = &pctrl->ports[i];
		dma_set_port_controller_data(pport, pctrl);
		dma_port_cfg(pport);

		for (j = 0; j < pport->chan_nrs; j++) {
			pch = &pport->chans[j];
			dma_set_chan_controller_data(pch, pctrl);
			dma_set_chan_port_data(pch, pport);
			dma_chan_cfg(pch);
			pch->irq = pctrl->irq_base + pch->nr;
			ret = devm_request_irq(pctrl->dev, pch->irq,
					       dma_chan_interrupt,
					       IRQF_NO_THREAD |
					       IRQF_NOBALANCING,
					       dma_get_name_by_cid(pctrl->cid),
					       (void *)pch);
			if (ret) {
				dev_err(pctrl->dev,
					"Failed to register irq %d on chan %d %s\n",
					pch->irq, pch->nr, pctrl->name);
				return -ENODEV;
			}
		}
	}
	return 0;
}

/*
 * the panic-on-failure call at :4360 replaced with dev_err + return
 * PTR_ERR(membase).
 */
static int ltq_dma_probe(struct platform_device *pdev)
{
	int err;
	int irq;
	struct device_node *node = pdev->dev.of_node;
	struct resource *memres;
	struct dma_ctrl *pctrl;
	unsigned int id;
	int cidx;

	/*
	 * Layout asserts hosted here (rather than in dma_ctrl_init) to fire
	 * exactly once early in probe. Descriptor sizes are silicon-binding
	 * (16-byte 4-DW format) and MAX_DMA_CHAN_PER_PORT defines the bitmap
	 * width inside struct dma_ctrl.
	 */
	BUILD_BUG_ON(sizeof(struct dma_rx_desc) != 16);
	BUILD_BUG_ON(sizeof(struct dma_tx_desc) != 16);
	BUILD_BUG_ON(MAX_DMA_CHAN_PER_PORT != 64);

	/* DMA controller physical idx from aliases. */
	cidx = of_alias_get_id(node, "dma");
	if (cidx < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", cidx);
		return cidx;
	}

	pdev->id = cidx;

	if (pdev->id < 0 || pdev->id >= DMAMAX)
		return -EINVAL;

	pctrl = &ltq_dma_controller[pdev->id];

	memset(pctrl, 0, sizeof(*pctrl));

	pctrl->cid = pdev->id;
	pctrl->name = dma_name[pctrl->cid];
	pctrl->dev = &pdev->dev;

	memres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!memres) {
		dev_err(&pdev->dev, "failed to get dma resource\n");
		return -ENODEV;
	}

	/* remap dma register range */
	pctrl->membase = devm_ioremap_resource(&pdev->dev, memres);
	if (IS_ERR(pctrl->membase)) {
		dev_err(&pdev->dev, "failed to remap dma resource\n");
		return PTR_ERR(pctrl->membase);
	}

	err = dma_set_mask(pctrl->dev, DMA_BIT_MASK(32));
	if (err) {
		err = dma_set_coherent_mask(pctrl->dev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev,
				"No usable DMA configuration, aborting\n");
			return err;
		}
	}


	id = ltq_dma_r32(pctrl, DMA_ID);
	pctrl->chans = MS(id, DMA_ID_CHNR);
	pctrl->port_nrs = MS(id, DMA_ID_PRTNR);
	pctrl->ver = MS(id, DMA_ID_REV);

	err = dma_cfg_init(pctrl);
	if (err)
		return err;

	err = dma_ctrl_init(pctrl);
	if (err)
		return err;

	/* Link platform with driver data for retrieving */
	platform_set_drvdata(pdev, pctrl);


	irq = platform_get_irq(pdev, 0);

	pr_info("probe entry cid=%d name=%s reg=0x%lx irq=%d\n",
		cidx, pctrl->name, (unsigned long)memres->start, irq);

	return 0;
}

/* 1. */

/*
 * Returns NULL when ltq_dma_controller[cid].dev is NULL (controller never
 * bound by ltq_dma_probe).
 *
 * DMA2RX only binds when the xrx500.dtsi dma2rx node (alias dma4) is present;
 * absent that node ltq_dma_controller[DMA2RX] .dev stays NULL and this
 * function still returns NULL for DMA2RX cids.
 */
static struct dma_ctrl *dma_lookup_ctrl(int cid)
{
	if (cid <= DMA0 || cid > DMA2RX)
		return NULL;
	if (cid >= DMAMAX)
		return NULL;
	if (!ltq_dma_controller[cid].dev)
		return NULL;
	return &ltq_dma_controller[cid];
}

/*
 * Decodes the 32-bit encoded channel handle, validates the controller is
 * bound and the (pid, nid) pair resolves to a real per-channel slot.
 *
 * Returns NULL on any failure (caller logs / returns -EINVAL upstream).
 */
static struct dmax_chan *dma_chan_l2p(u32 chan)
{
	int cid = _DMA_CONTROLLER(chan);
	int pid = _DMA_PORT(chan);
	int nid = _DMA_CHANNEL(chan);
	struct dma_ctrl *pctrl;
	struct dma_port *pport;
	int i;

	pctrl = dma_lookup_ctrl(cid);
	if (!pctrl)
		return NULL;
	if (pid < 0 || pid >= (int)pctrl->port_nrs)
		return NULL;
	pport = &pctrl->ports[pid];
	if (!pport->chans)
		return NULL;
	for (i = 0; i < pport->chan_nrs; i++) {
		if (pport->chans[i].nr == nid)
			return &pport->chans[i];
	}
	return NULL;
}

/*
 * dma_chan_in_use — small inline helper used to enforce post-request
 * preconditions on the per-channel APIs. Matches AVM hdma.c:387-389.
 */
static inline bool dma_chan_in_use(struct dmax_chan *pch)
{
	return (pch->flags & CHAN_IN_USE) ? true : false;
}

int ltq_request_dma(u32 chan, const char *device_id)
{
	int cid = _DMA_CONTROLLER(chan);
	int pid = _DMA_PORT(chan);
	int nid = _DMA_CHANNEL(chan);
	struct dma_ctrl *pctrl;
	struct dma_port *pport;
	struct dmax_chan *pch = NULL;
	int i;

	pctrl = dma_lookup_ctrl(cid);
	if (!pctrl)
		return -EINVAL;

	if (pid < 0 || pid >= (int)pctrl->port_nrs)
		return -EINVAL;
	pport = &pctrl->ports[pid];
	if (!pport->chans)
		return -EINVAL;
	for (i = 0; i < pport->chan_nrs; i++) {
		if (pport->chans[i].nr == nid) {
			pch = &pport->chans[i];
			break;
		}
	}
	if (!pch)
		return -ENODEV;

	if (!device_id)
		pr_warn("%s: no device id given for %s chan %d\n", __func__,
			dma_get_name_by_cid(cid), nid);

	mutex_lock(&pch->ch_lock);
	if (dma_chan_in_use(pch)) {
		mutex_unlock(&pch->ch_lock);
		pr_err("%s chan %d already in use\n",
		       dma_get_name_by_cid(cid), nid);
		return -EBUSY;
	}
	pch->flags |= CHAN_IN_USE;
	pch->lnr = chan;
	pch->device_id = device_id;
	mutex_unlock(&pch->ch_lock);

	pr_debug("%s chan %d is allocated for %s\n", dma_get_name_by_cid(cid),
		 nid, device_id ? device_id : "(unnamed)");
	return 0;
}

/*
 * ltq_free_dma — AVM hdma.c:2268-2281 verbatim. Inverse of ltq_request_dma:
 * clears CHAN_IN_USE, resets lnr / device_id. No silicon-side teardown
 * here — callers must have already invoked ltq_dma_chan_close +
 * ltq_dma_chan_desc_free per the API contract documented in lantiq_dmax.h.
 */
int ltq_free_dma(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	mutex_lock(&pch->ch_lock);
	pch->flags &= ~CHAN_IN_USE;
	pch->lnr = -1;
	pch->device_id = NULL;
	mutex_unlock(&pch->ch_lock);
	return 0;
}

/*
 * dma_chan_on / ltq_dma_chan_on — AVM hdma.c:734-756 verbatim. The
 * desc_configured WARN_ON in the public wrapper preserves AVM's "no on
 * without descriptors" contract — callers must invoke ltq_dma_chan_desc_cfg
 * (or equivalent) before chan_on.
 */
static void dma_chan_on(struct dmax_chan *pch)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32_mask(pctrl, 0, DMA_CCTRL_ON, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	pch->onoff = DMA_CH_ON;
}

int ltq_dma_chan_on(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	/* If descriptors not configured, not allow to turn on channel. */
	if (WARN_ON(!pch || !pch->desc_configured))
		return -EINVAL;
	dma_chan_on(pch);
	return 0;
}

/*
 * dma_chan_off / ltq_dma_chan_off — AVM hdma.c:758-788 verbatim. Spins up
 * to ~10k iterations waiting for the DMA_CCTRL_ON bit to self-clear; logs
 * (but does not fail) on timeout — silicon may take longer if a long
 * descriptor is mid-burst.
 */
static void dma_chan_off(struct dmax_chan *pch)
{
	u32 reg;
	int i = 10000;
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32_mask(pctrl, DMA_CCTRL_ON, 0, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);

	/* Wait for channel off to complete. */
	while (((reg = ltq_dma_r32(pctrl, DMA_CCTRL)) & DMA_CCTRL_ON) && i--)
		;
	if (i == 0)
		dev_err(pctrl->dev, "%s chan %d off failed\n",
			pctrl->name, pch->nr);
	pch->onoff = DMA_CH_OFF;
}

int ltq_dma_chan_off(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	dma_chan_off(pch);
	return 0;
}

/*
 * dma_chan_reset / ltq_dma_chan_reset — AVM hdma.c:935-970. dma_chan_off
 * is invoked first so any in-flight burst finishes cleanly before the
 * reset pulse. Resets curr_desc / prev_desc back to 0 and re-arms the
 * silicon CDBA / CDLEN registers with the cached descriptor ring.
 *
 * Callers that need a ring re-seed must do it via the descriptor-cfg path
 * explicitly.
 */
int ltq_dma_chan_reset(u32 chan)
{
	int i = 10000;
	unsigned long flags;
	struct dmax_chan *pch = dma_chan_l2p(chan);
	struct dma_ctrl *pctrl;

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);

	dma_chan_off(pch);
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32_mask(pctrl, 0, DMA_CCTRL_RST, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	/* Wait for the RST bit to self-clear. */
	while ((ltq_dma_r32(pctrl, DMA_CCTRL) & DMA_CCTRL_RST) && i--)
		;
	if (i == 0)
		dev_err(pctrl->dev, "%s chan %d reset failed\n",
			pctrl->name, pch->nr);
	pch->rst = 1;
	/* Re-arm CDBA / CDLEN with the cached descriptor ring base. */
	if (pch->desc_phys && pch->desc_len) {
		spin_lock_irqsave(&pctrl->ctrl_lock, flags);
		ltq_dma_w32(pctrl, pch->nr, DMA_CS);
		ltq_dma_w32(pctrl, pch->desc_phys, DMA_CDBA);
		ltq_dma_w32(pctrl, pch->desc_len, DMA_CDLEN);
		spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	}
	pch->curr_desc = 0;
	pch->prev_desc = 0;
	pch->desc_configured = false;
	return 0;
}

/*
 * Writes DMA_CI_DEFAULT (= DMA_CI_EOP | DMA_CI_DESCPT) to the per-channel
 * DMA_CIE — NOT DMA_CI_ALL (DUR/CHOFF/RDERR would generate spurious IRQs at
 * high traffic). Then sets BIT(cn & 0x1f) in the top-level DMA_IRNEN (cn<=31)
 * or DMA_IRNEN1 (cn>=32) so the GIC delivers the chained IRQ.
 */
static void dma_chan_irq_enable(struct dmax_chan *pch)
{
	unsigned long flags;
	u32 reg_off;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32(pctrl, DMA_CI_DEFAULT, DMA_CIE);
	reg_off = (pch->nr >= 32) ? DMA_IRNEN1 : DMA_IRNEN;
	ltq_dma_w32_mask(pctrl, 0, BIT(pch->nr & 0x1f), reg_off);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

int ltq_dma_chan_irq_enable(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	dma_chan_irq_enable(pch);
	return 0;
}

/*
 * Do not clear the top-level DMA_IRNEN channel bit. The vendor's
 * dma_chan_irq_disable writes only DMA_CIE = 0; IRNEN is owned by the genirq
 * irqchip, which sets it for all channels at controller init. Clearing CIE
 * alone silences the channel, and leaving IRNEN set cannot raise a spurious
 * interrupt.
 */
static void dma_chan_irq_disable(struct dmax_chan *pch)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32(pctrl, 0, DMA_CIE);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

int ltq_dma_chan_irq_disable(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	dma_chan_irq_disable(pch);
	return 0;
}

/*
 * Composition: chan_on + (for RX) irq_enable. TX channels intentionally do
 * NOT enable IRQ at open time — AVM-verbatim contract preserved.
 */
static void dma_chan_open(struct dmax_chan *pch)
{
	/* chan on, then enable irq (RX only — AVM verbatim). */
	dma_chan_on(pch);
	if (dma_chan_rx(pch))
		dma_chan_irq_enable(pch);
}

int ltq_dma_chan_open(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch || !pch->desc_configured))
		return -EINVAL;
	dma_chan_open(pch);
	return 0;
}

/*
 * dma_chan_close / ltq_dma_chan_close — AVM hdma.c:1039-1054 verbatim.
 * Composition: chan_off + irq_disable (unconditional for both directions
 * — TX too, since TX may have been later promoted to IRQ-enabled via the
 * direct ltq_dma_chan_irq_enable path).
 */
static void dma_chan_close(struct dmax_chan *pch)
{
	dma_chan_off(pch);
	dma_chan_irq_disable(pch);
}

int ltq_dma_chan_close(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	dma_chan_close(pch);
	return 0;
}

/*
 * dma_chan_pkt_drop_cfg / ltq_dma_chan_pkt_drop_cfg — AVM hdma.c:1056-1085
 * verbatim. TX channels silently no-op — packet-drop is meaningful only on
 * the RX side where silicon needs a fallback when the descriptor ring
 * overflows.
 */
static void dma_chan_pkt_drop_cfg(struct dmax_chan *pch, int enable)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	if (dma_chan_tx(pch))
		return;

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CCTRL_PDEN, DMA_CCTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CCTRL_PDEN, 0, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

int ltq_dma_chan_pkt_drop_cfg(u32 chan, int enable)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	dma_chan_pkt_drop_cfg(pch, enable);
	mutex_lock(&pch->ch_lock);
	pch->pden = enable;
	mutex_unlock(&pch->ch_lock);
	return 0;
}

/*
 * ltq_dma_chan_pktsize_cfg — AVM hdma.c:1134-1147 verbatim. Stores
 * pkt_size in the per-channel slot; the silicon honours this at
 * data_buf_alloc / descriptor-seed time.
 */
int ltq_dma_chan_pktsize_cfg(u32 chan, size_t pktsize)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	if (pktsize > DMA_MAX_PKT_SIZE)
		return -EINVAL;
	mutex_lock(&pch->ch_lock);
	pch->pkt_size = (int)pktsize;
	mutex_unlock(&pch->ch_lock);
	return 0;
}

/*
 * dma_chan_byte_offset_cfg / ltq_dma_chan_byte_offset_cfg — AVM
 * hdma.c:1241-1278 verbatim. boff_len > DMA_CHAN_BOFF_MAX (255) disables
 * the feature; valid 1..255 enables and writes the length to DMA_C_BOFF.
 * V2.2 hardware doesn't have DMA_C_BOFF — returns -EPERM there.
 */
static void dma_chan_byte_offset_cfg(struct dmax_chan *pch, u32 boff_len)
{
	u32 val = 0;
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	if (boff_len == 0 || boff_len > DMA_CHAN_BOFF_MAX) {
		ltq_dma_w32_mask(pctrl, DMA_C_BOFF_EN | DMA_C_BOFF_BOF_LEN,
				 0, DMA_C_BOFF);
	} else {
		val = SM(boff_len, DMA_C_BOFF_BOF_LEN) | DMA_C_BOFF_EN;
		ltq_dma_w32(pctrl, val, DMA_C_BOFF);
	}
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

int ltq_dma_chan_byte_offset_cfg(u32 chan, u32 boff_len)
{
	struct dma_ctrl *pctrl;
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;

	pctrl = dma_chan_get_controller(pch);
	if (pctrl->ver <= DMA_VER_22)
		return -EPERM;

	dma_chan_byte_offset_cfg(pch, boff_len);

	mutex_lock(&pch->ch_lock);
	pch->boff_len = boff_len;
	mutex_unlock(&pch->ch_lock);
	return 0;
}

/*
 * Programs the 5-bit CCTRL_CLASS / CCTRL_CLASSH field used by GSWIP-3.0
 * traffic classes. V2.2 silicon does NOT have the class field — returns
 * -EPERM there. cls > 31 (5-bit max) returns -EINVAL.
 */
int ltq_dma_chan_class_cfg(u32 chan, u32 cls)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	return dma_chan_set_class(pch, cls);
}

/*
 * arch_dma_set_uncached() on MIPS is __pa(addr) + UNCAC_BASE
 * (arch/mips/mm/dma-noncoherent.c), and on this platform __pa() yields the
 * FULL physical address because PHYS_OFFSET is 0x20000000
 * (mach-intel-mips/spaces.h). So the "uncached" pointer the allocator returns
 * is 0xa0000000 + 0x2xxxxxxx = 0xC2xxxxxx — past CKSEG1 and inside the
 * TLB-mapped vmalloc/ioremap window (VMALLOC_START = CKSEG2 = 0xc0000000).
 * Note the spaces.h route cannot work: arch_dma_set_uncached ADDS UNCAC_BASE,
 * so a PHYS_OFFSET-correcting UNCAC_BASE would have to be 0x80000000 and
 * would collide with CAC_BASE.
 *
 * The GRX500 aliases DDR at physical 0, so CKSEG1ADDR(phys) is an uncached
 * window onto the very same DRAM page; the guard below accepts exactly the
 * native DDR window.
 */
int ltq_dma_chan_desc_alloc(u32 chan, u32 desc_num)
{
	struct dma_ctrl *pctrl;
	struct dmax_chan *pch = dma_chan_l2p(chan);
	dma_addr_t phys;
	void *vaddr;
	size_t bytes;

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);

	if (desc_num == 0 || desc_num > DMA_MAX_DESC_NUM) {
		dev_err(pctrl->dev,
			"%s: invalid desc_num %u (max %u)\n",
			pch->device_id ? pch->device_id : "(unnamed)",
			desc_num, DMA_MAX_DESC_NUM);
		return -EINVAL;
	}

	if (pch->desc_base) {
		dev_err(pctrl->dev,
			"%s chan %d desc already allocated\n",
			pch->device_id ? pch->device_id : "(unnamed)",
			pch->nr);
		return -EINVAL;
	}

	bytes = (size_t)desc_num * pctrl->desc_size;
	vaddr = dma_alloc_coherent(pctrl->dev, bytes, &phys, GFP_KERNEL);
	if (!vaddr)
		return -ENOMEM;

	if (phys < 0x20000000ULL || phys >= 0x28000000ULL) {
		dev_err(pctrl->dev,
			"desc_phys 0x%llx outside native DDR window, refusing the ring\n",
			(unsigned long long)phys);
		dma_free_coherent(pctrl->dev, bytes, vaddr, phys);
		return -EINVAL;
	}

	pch->desc_alloc_va = vaddr;
	pch->desc_base = (u32)CKSEG1ADDR((unsigned long)phys);
	pch->desc_phys = phys;
	pch->desc_len = desc_num;
	pch->curr_desc = 0;
	pch->prev_desc = 0;
	dev_dbg(pctrl->dev,
		"%s chan %d desc_base=0x%08x desc_phys=0x%llx desc_len=%u\n",
		dma_get_name_by_cid(pctrl->cid), pch->nr, pch->desc_base,
		(unsigned long long)pch->desc_phys, desc_num);
	return 0;
}

/*
 * Reverses ltq_dma_chan_desc_alloc with dma_free_coherent. Idempotent:
 * returns 0 if desc_base is already NULL (e.g. double-free or
 * free-before-alloc). Always clears the slot back to a zero state so a
 * subsequent desc_alloc can re-bind.
 *
 * Handing dma_free_coherent the old 0xC2xxxxxx desc_base was a latent bad
 * free: MIPS has no arch_dma_clear_uncached, so dma_direct_free falls into
 * `if (is_vmalloc_addr(cpu_addr)) vunmap(cpu_addr)` and that address IS in
 * the vmalloc range, so it would vunmap an area that was never mapped (WARN).
 * Unreachable until now only because hdma_port_disable never ran for the one
 * CBM-managed port; the deq-7..9 ports added alongside eth1-3 do tear down.
 */
int ltq_dma_chan_desc_free(u32 chan)
{
	struct dma_ctrl *pctrl;
	struct dmax_chan *pch = dma_chan_l2p(chan);
	size_t bytes;

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);

	if (!pch->desc_base)
		return 0;

	/*
	 * desc_alloc_va NULL with desc_base set means this channel does not OWN
	 * the ring, and there is exactly one way to get there: dma_p2p_cfg()
	 * copies the RX channel's desc_base/desc_phys/desc_len onto the TX
	 * channel of a peripheral-to-peripheral pair, without an allocation of
	 * its own. Freeing here would double-free the RX channel's ring, so drop
	 * the slot without freeing.
	 *
	 * The CBM-managed egress channels do NOT reach this branch: they never
	 * allocate a ring, and ltq_dma_chan_desc_cfg() (which points them at the
	 * DQM MMIO window) writes desc_phys, not desc_base, so desc_base stays 0
	 * and the idempotent guard above returns first.
	 */
	if (!pch->desc_alloc_va) {
		pch->desc_base = 0;
		pch->desc_phys = 0;
		pch->desc_len = 0;
		pch->curr_desc = 0;
		pch->prev_desc = 0;
		pch->desc_configured = false;
		return 0;
	}

	bytes = (size_t)pch->desc_len * pctrl->desc_size;
	dma_free_coherent(pctrl->dev, bytes, pch->desc_alloc_va, pch->desc_phys);
	pch->desc_alloc_va = NULL;
	pch->desc_base = 0;
	pch->desc_phys = 0;
	pch->desc_len = 0;
	pch->curr_desc = 0;
	pch->prev_desc = 0;
	pch->desc_configured = false;
	return 0;
}

/*
 * ltq_dma_chan_data_buf_alloc — AVM hdma.c:1887-1906 verbatim. Walks the
 * descriptor ring and uses pch->alloc to populate the per-desc data buffer.
 */
int ltq_dma_chan_data_buf_alloc(u32 chan)
{
	int i;
	int byte_offset;
	int ret;
	char *buffer;
	struct dma_rx_desc *rx_desc_p;
	struct dmax_chan *pch = dma_chan_l2p(chan);
	struct dma_ctrl *pctrl;

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);
	if (!pch->alloc)
		return -EINVAL;
	if (!pch->desc_base || pch->desc_len <= 0)
		return -EINVAL;

	if (!pch->opt) {
		pch->opt = devm_kzalloc(pctrl->dev,
					(size_t)pch->desc_len * sizeof(void *),
					GFP_KERNEL);
		if (!pch->opt)
			return -ENOMEM;
	}
	ret = ltq_dma_chan_desc_cfg(chan, pch->desc_phys, pch->desc_len);
	if (ret)
		return ret;

	for (i = 0; i < pch->desc_len; i++) {
		rx_desc_p = (struct dma_rx_desc *)(uintptr_t)pch->desc_base + i;
		buffer = pch->alloc(pch->pkt_size, &byte_offset, &pch->opt[i]);
		if (!buffer) {
			dev_err(pctrl->dev, "No enough memory for %s\n",
				pch->device_id ? pch->device_id : "(unnamed)");
			return -ENOMEM;
		}
		rx_desc_p->data_pointer = dma_map_single(pctrl->dev, buffer,
							 pch->pkt_size,
							 DMA_FROM_DEVICE);
		if (dma_mapping_error(pctrl->dev, rx_desc_p->data_pointer)) {
			dev_err(pctrl->dev, "%s DMA map failed\n", __func__);
			pch->free(buffer, pch->opt[i]);
			return -ENOMEM;
		}
		rx_desc_p->status.all = 0;
		rx_desc_p->status.field.sop = 1;
		rx_desc_p->status.field.eop = 1;
		rx_desc_p->status.field.c = 0;
		rx_desc_p->status.field.byte_offset = byte_offset;
		rx_desc_p->status.field.data_len = pch->pkt_size;
		/* Ensure data field ready before ownership change */
		wmb();
		rx_desc_p->status.field.own = DMA_OWN;
		/* Ensure ownership changed before moving to next step */
		wmb();
	}

	pr_info("hdma: %s ch%d rx ring seeded: %d desc x %d B, cdba=0x%08llx\n",
		dma_get_name_by_cid(pctrl->cid), pch->nr, pch->desc_len,
		pch->pkt_size, (unsigned long long)pch->desc_phys);
	return 0;
}

/* ltq_dma_chan_data_buf_free — counterpart to data_buf_alloc. */
int ltq_dma_chan_data_buf_free(u32 chan)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	if (!pch->free)
		return 0;
	return 0;
}

/*
 * ltq_dma_chan_pseudo_irq_handler_callback_cfg — AVM hdma.c:2107-2122
 * verbatim.
 */
int ltq_dma_chan_pseudo_irq_handler_callback_cfg(u32 chan,
						 intr_handler_t handler,
						 void *priv)
{
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!handler || !pch))
		return -EINVAL;

	mutex_lock(&pch->ch_lock);
	pch->intr_handler = handler;
	pch->priv = priv;
	mutex_unlock(&pch->ch_lock);
	return 0;
}

/*
 * The DMA_CS-select-before-CDBA/CDLEN-write ordering is silicon-mandatory —
 * silicon requires DMA_CS to point at the channel before per-channel register
 * writes take effect.
 */
int ltq_dma_chan_desc_cfg(u32 chan, dma_addr_t desc_base, int desc_num)
{
	unsigned long flags;
	struct dma_ctrl *pctrl;
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);

	if (desc_num <= 0 || desc_num > DMA_MAX_DESC_NUM) {
		dev_err(pctrl->dev,
			"%s: desc_num %d out of range (1..%u)\n",
			pch->device_id ? pch->device_id : "(unnamed)",
			desc_num, DMA_MAX_DESC_NUM);
		return -EINVAL;
	}

	mutex_lock(&pch->ch_lock);
	pch->flags |= DMA_HW_DESC;
	pch->desc_len = desc_num;
	pch->desc_phys = desc_base;
	mutex_unlock(&pch->ch_lock);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32(pctrl, desc_base, DMA_CDBA);
	ltq_dma_w32(pctrl, desc_num, DMA_CDLEN);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);

	pch->desc_configured = true;
	return 0;
}

/*
 * ltq_dma_chan_get_curr_desc_addr - returns the physical address of the
 * descriptor currently being processed by silicon on the selected channel.
 *
 * Returns the dma_addr_t value read from DMA_CDPTNR, or 0 on lookup
 * failure (callers must check pch validity via prior ltq_request_dma).
 */
dma_addr_t ltq_dma_chan_get_curr_desc_addr(u32 chan)
{
	unsigned long flags;
	dma_addr_t cur;
	struct dma_ctrl *pctrl;
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return 0;
	pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	cur = (dma_addr_t)ltq_dma_r32(pctrl, DMA_CDPTNR);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	return cur;
}

/*
 * The AVM-legacy "lantiq,dma-grx500" compatible is also absent (renamed
 * wholesale).
 */
static const struct of_device_id ltq_dma_match[] = {
	{ .compatible = "lantiq,dma-xrx500" },
	{},
};

static struct platform_driver ltq_dma_driver = {
	.probe = ltq_dma_probe,
	.driver = {
		.name = "dma-xrx500",
		.of_match_table = ltq_dma_match,
	},
};

static int __init ltq_dma_init(void)
{
	return platform_driver_register(&ltq_dma_driver);
}
postcore_initcall(ltq_dma_init);


/*
 *   AVM cbm_config.c:40-49 (DEQUEUE, TX-egress) — CBM tmu_port=7 row:
 *     { .type = DQM_DMA_TYPE, .tmu_port = 7, .tmu_queue = 17,
 *       .tmu_queue_nos = 1, .dma_ctrl = 2, .dma_chan = 2, ... }
 *   AVM cbm.c:4514-4539 (dequeue_dma_port_init):
 *     sprintf(dma_ctrl, "DMA%dTX", dma_hw_num);
 *     chan = _DMA_C(cqm_dma_get_controller(dma_ctrl), 0, dma_chan_num);
 *   Substituting (dma_hw_num=2, dma_chan_num=2) yields
 *     _DMA_C(DMA2TX_controller, 0, 2) == lantiq_dmax.h:303
 *     DMA2TX_CBM_P7_CLASS2 — empirical (not name-heuristic) evidence
 *     that DMA2TX channel 2 services CBM port 7 TX-egress.
 *
 * AVM cbm_config.c:289-306 (ENQUEUE, RX-admit) lists two CBM tmu_port=7
 * entries: _DMA_C(DMA1RX, 0, 0) (std buf) and _DMA_C(DMA1RX, 0, 16) (jumbo).
 * To add them later, append: { .cid = DMA1RX, .pid = DMA1RX_PORT, .nid =
 * DMA_CHANNEL_0, ... }, { .cid = DMA1RX, .pid = DMA1RX_PORT, .nid =
 * DMA_CHANNEL_16, ... }, with citation cbm_config.c:289-306 +
 * cbm.c:4459-4496.
 */
struct port7_chan_desc {
	u32 cid;
	u32 pid;
	u32 nid;
	const char *label;
	u32 cbm_deq;
};

static const struct port7_chan_desc port7_channels[] = {
	{ .cid = DMA2TX, .pid = DMA2TX_PORT, .nid = DMA_CHANNEL_2,
	  .label = "DMA2TX_CBM_P7_CLASS2", .cbm_deq = 7 },
};

static const struct port7_chan_desc port8_channels[] = {
	{ .cid = DMA2TX, .pid = DMA2TX_PORT, .nid = DMA_CHANNEL_3,
	  .label = "DMA2TX_CBM_P8_CLASS3", .cbm_deq = 8 },
};

static const struct port7_chan_desc port9_channels[] = {
	{ .cid = DMA2TX, .pid = DMA2TX_PORT, .nid = DMA_CHANNEL_4,
	  .label = "DMA2TX_CBM_P9_CLASS4", .cbm_deq = 9 },
};

static const struct port7_chan_desc port10_channels[] = {
	{ .cid = DMA2TX, .pid = DMA2TX_PORT, .nid = DMA_CHANNEL_5,
	  .label = "DMA2TX_CBM_P10_CLASS5", .cbm_deq = 10 },
};

/*
 * hdma_port_chan_tbl — the per-CBM-dequeue-port channel table, or NULL for a
 * dequeue port this driver does not serve. Shared by hdma_port_enable and
 * hdma_port_disable so the two can never disagree about which channels belong
 * to a port (they did once: disable knew only port 7, so eth0's channel 5
 * leaked across a netifd ndo_stop -> ndo_open cycle and the re-open failed
 * -EBUSY).
 */
static const struct port7_chan_desc *hdma_port_chan_tbl(int port_id, size_t *n)
{
	switch (port_id) {
	case 7:
		*n = ARRAY_SIZE(port7_channels);
		return port7_channels;
	case 8:
		*n = ARRAY_SIZE(port8_channels);
		return port8_channels;
	case 9:
		*n = ARRAY_SIZE(port9_channels);
		return port9_channels;
	case 10:
		*n = ARRAY_SIZE(port10_channels);
		return port10_channels;
	default:
		return NULL;
	}
}

/*
 * The CBM DQM descriptor window is laid out per dequeue port and is
 * controller-independent (the vendor computes it in setup_DMA_channel, two
 * descriptors per port), so the same arithmetic serves the WAN port
 * unchanged.
 */
#define CBM_DMADESC_PHYS_BASE   0x1e500000U  /* DT cbm dma_desc window */
#define CBM_DESC0_0_EGP_5_OFF   0x40000U     /* cbm_regs.h DESC0_0_EGP_5 */
#define CBM_DQM_DESC_NUM        2U           /* AVM: 2 ping-pong descriptors */
#define CBM_DQM_DESC_PHYS(deq) \
	((dma_addr_t)(CBM_DMADESC_PHYS_BASE + CBM_DESC0_0_EGP_5_OFF + \
		      ((u32)((deq) - 5) * 0x1000U)))

/*
 * Opens the DMA channel(s) the CBM dequeue port @port_id owns, per
 * hdma_port_chan_tbl().
 */
#define HDMA_PORT_DESC_NUM 32

/*
 * Once a CBM-managed DMA2TX egress channel is armed, keep it armed: the CBM
 * dequeue port owns the descriptor window, and tearing the channel down
 * leaves the port pointing at descriptors nothing will refill.
 */
#define HDMA_CBM_DEQ_FIRST 7
#define HDMA_CBM_DEQ_LAST  10

static u32 g_hdma_deq_armed;

static bool hdma_deq_armed(int port_id)
{
	if (port_id < HDMA_CBM_DEQ_FIRST || port_id > HDMA_CBM_DEQ_LAST)
		return false;
	return !!(g_hdma_deq_armed & BIT(port_id - HDMA_CBM_DEQ_FIRST));
}

static void hdma_deq_set_armed(int port_id)
{
	if (port_id < HDMA_CBM_DEQ_FIRST || port_id > HDMA_CBM_DEQ_LAST)
		return;
	g_hdma_deq_armed |= BIT(port_id - HDMA_CBM_DEQ_FIRST);
}

extern int init_cbm_dqm_dma_port(int dqp);

/*
 * AVM ltq_dma_chan_sw_poll_cfg (hdma.c:1306-1364) — per-channel SW-poll mode
 * (DMA_C_SWPOLL), which takes one channel off the controller-global CH_FL
 * flow control engine.
 */
static void dma_chan_sw_poll_cfg(struct dmax_chan *pch, int enable)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	/* MUX always on; EN (bit31) gates SW-poll on/off. */
	ltq_dma_w32_mask(pctrl, 0, DMA_C_SWPOLL_MUX, DMA_C_SWPOLL);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_C_SWPOLL_EN, DMA_C_SWPOLL);
	else
		ltq_dma_w32_mask(pctrl, DMA_C_SWPOLL_EN, 0, DMA_C_SWPOLL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

int ltq_dma_chan_sw_poll_cfg(u32 chan, int enable)
{
	struct dma_ctrl *pctrl;
	struct dmax_chan *pch = dma_chan_l2p(chan);

	if (WARN_ON(!pch))
		return -EINVAL;
	pctrl = dma_chan_get_controller(pch);
	if (pctrl->ver <= DMA_VER_22)
		return -EPERM;
	dma_chan_sw_poll_cfg(pch, enable);
	mutex_lock(&pch->ch_lock);
	pch->sw_poll = enable;
	mutex_unlock(&pch->ch_lock);
	return 0;
}
EXPORT_SYMBOL_GPL(ltq_dma_chan_sw_poll_cfg);

/*
 * Faithful port of AVM hdma.c dma_chan_p2p_cfg (:1545), dma_chan_global_buf_
 * len_cfg (:1564), dma_chan_desc_hw_cfg (:790), dma_p2p_cfg (:2291) and
 * ltq_dma_p2p_cfg (:2313). The CBM setup_DMA_p2p (cqm/grx500/cbm.c) drives
 * this to wire the GSWIP-R RX path (DMA2RX ch0..3) into the LAN-switch TX
 * path (DMA1TX ch0..3): the TX channel inherits the RX channel's descriptor
 * ring, enables P2P-copy (DMA_CCTRL_P2PCPY) and a global buffer length so the
 * silicon copies each RX-DMA'd frame straight to the TX-DMA peripheral with
 * no CPU/DRAM round trip.
 */

/* AVM hdma.c:790-802 verbatim. Programs CDBA/CDLEN + sets desc_configured. */
static void dma_chan_desc_hw_cfg(struct dmax_chan *pch, dma_addr_t desc_base,
				 int desc_num)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32(pctrl, desc_base, DMA_CDBA);
	ltq_dma_w32(pctrl, desc_num, DMA_CDLEN);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
	pch->desc_configured = true;
}

/* AVM hdma.c:1545-1562 verbatim. TX-only: P2P-copy bit in DMA_CCTRL. */
static void dma_chan_p2p_cfg(struct dmax_chan *pch, int enable)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	if (dma_chan_rx(pch)) {
		dev_err(pctrl->dev,
			"RX channel has no need to configure P2P bit\n");
		return;
	}
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	if (enable)
		ltq_dma_w32_mask(pctrl, 0, DMA_CCTRL_P2PCPY, DMA_CCTRL);
	else
		ltq_dma_w32_mask(pctrl, DMA_CCTRL_P2PCPY, 0, DMA_CCTRL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* AVM hdma.c:1564-1580 verbatim. TX-only: global buffer length in DMA_CGBL. */
static void dma_chan_global_buf_len_cfg(struct dmax_chan *pch, int data_len)
{
	unsigned long flags;
	struct dma_ctrl *pctrl = dma_chan_get_controller(pch);

	if (dma_chan_rx(pch)) {
		dev_err(pctrl->dev,
			"RX channel has no need to configure P2P bit\n");
		return;
	}
	if (data_len < 0 || data_len > DMA_MAX_PKT_SIZE)
		return;
	spin_lock_irqsave(&pctrl->ctrl_lock, flags);
	ltq_dma_w32(pctrl, pch->nr, DMA_CS);
	ltq_dma_w32(pctrl, data_len, DMA_CGBL);
	spin_unlock_irqrestore(&pctrl->ctrl_lock, flags);
}

/* dma_p2p_cfg - AVM hdma.c:2291-2311. */
static void dma_p2p_cfg(struct dmax_chan *rch, struct dmax_chan *tch)
{
	/*
	 * Assume RX and TX has been configured using
	 * DMA descriptor and data buffer functions.
	 * TX has to be released its old buffer for default cases.
	 */
	if (WARN_ON(rch->desc_len == 0))
		return;
	dma_chan_desc_hw_cfg(rch, rch->desc_phys, rch->desc_len);
	mutex_lock(&tch->ch_lock);
	tch->desc_len = rch->desc_len;
	tch->desc_base = rch->desc_base;
	tch->desc_phys = rch->desc_phys;
	tch->pkt_size = rch->pkt_size;
	tch->p2pcpy = 1;
	tch->global_buffer_len = tch->pkt_size;
	mutex_unlock(&tch->ch_lock);
	dma_chan_desc_hw_cfg(tch, tch->desc_phys, tch->desc_len);
	dma_chan_p2p_cfg(tch, 1);
	dma_chan_global_buf_len_cfg(tch, tch->global_buffer_len);
}

/* AVM hdma.c:2313-2323 verbatim. Public entry used by CBM setup_DMA_p2p. */
int ltq_dma_p2p_cfg(u32 rx_chan, u32 tx_chan)
{
	struct dmax_chan *rxch = dma_chan_l2p(rx_chan);
	struct dmax_chan *txch = dma_chan_l2p(tx_chan);

	if (WARN_ON((!rxch) || (!txch)))
		return -EINVAL;
	dma_p2p_cfg(rxch, txch);
	return 0;
}
EXPORT_SYMBOL_GPL(ltq_dma_p2p_cfg);

/*
 * hdma_ig192_toe_dma3_reset - reset-parity for the TOE DMA controller (AVM
 * cid DMA3 @ ssx0+0x300000 = phys 0x1E300000).
 *
 * Faithful replication of the AVM probe hardware effects using the same
 * static primitives, with a hand-built pctrl carrying DMA3's DT props
 * (4.9 lantiq/xrx500.dtsi:114-131): pkt-arb=2(pkt) burst=16 polling-cnt=24
 * chan-fc=0 desc-fod=0 desc-in-sram=1 drb=0 byte-en=1 txendi=3 rxendi=3.
 * The AVM dma_cfg_init DMA3/DMA4 "port_nrs--" quirk (AVM:4139) is
 * replicated; ports/chans are then configured exactly as dma_ctrl_init's
 * nested loop would. Deliberate non-hardware deltas: no devm (plain
 * kzalloc, never freed — builtin), no request_irq / tasklet (channels
 * stay DMA_CH_OFF and IRNEN bits are masked by dma_chan_cfg, so no IRQ
 * can fire; AVM's request_irq writes no device register).
 */
int hdma_ig192_toe_dma3_reset(void)
{
	struct dma_ctrl *pctrl = &ltq_dma_controller[DMA3];
	struct dma_port *pport;
	struct dmax_chan *pch;
	u32 id;
	int i, j;

	if (pctrl->membase) {
		pr_info("hdma: TOE/DMA3 already initialized, skipping\n");
		return 0;
	}

	pctrl->membase = ioremap(0x1E300000, 0x1000);
	if (!pctrl->membase) {
		pr_err("hdma: TOE/DMA3 ioremap failed\n");
		return -ENOMEM;
	}
	pctrl->cid = DMA3;
	pctrl->name = dma_name[DMA3];
	pctrl->dev = NULL;	/* no platform device — pr_* logging only */
	spin_lock_init(&pctrl->ctrl_lock);
	bitmap_zero(pctrl->dma_int_status, MAX_DMA_CHAN_PER_PORT);
	atomic_set(&pctrl->dma_in_process, 0);

	id = ltq_dma_r32(pctrl, DMA_ID);
	if (id == 0 || id == 0xFFFFFFFFu) {
		pr_err("hdma: TOE/DMA3 DMA_ID reads 0x%08x (clock off?), aborting reset\n",
		       id);
		iounmap(pctrl->membase);
		pctrl->membase = NULL;
		return -ENODEV;
	}
	pctrl->chans = MS(id, DMA_ID_CHNR);
	pctrl->port_nrs = MS(id, DMA_ID_PRTNR);
	pctrl->ver = MS(id, DMA_ID_REV);
	/* AVM hdma.c:4139: DMA3/DMA4 report one port more than they expose */
	if (pctrl->port_nrs)
		pctrl->port_nrs--;
	pr_info("hdma: TOE/DMA3 DMA_ID=0x%08x chans=%d ports=%d(post-dec) ver=%d\n",
		id, pctrl->chans, pctrl->port_nrs, pctrl->ver);

	/* DMA3 DT props, 4.9 lantiq/xrx500.dtsi:114-131 */
	pctrl->flags = DMA_CTL_64BIT | DMA_DESC_IN_SRAM | DMA_EN_BYTE_EN;
	pctrl->arb_type = DMA_ARB_PKT;
	pctrl->pollcnt = 24;
	pctrl->labcnt = 0;
	pctrl->orrc = 0;
	pctrl->budget = 20;
	pctrl->burst_mask = DMA_4DW_DESC_MASK;
	pctrl->desc_size = DMA_4DW_DESC_SIZE;

	if (pctrl->port_nrs) {
		pctrl->ports = kcalloc(pctrl->port_nrs, sizeof(*pport),
				       GFP_KERNEL);
		if (!pctrl->ports)
			return -ENOMEM;
		pport = &pctrl->ports[0];
		pport->chan_nrs = pctrl->chans;
		pport->pid = 0;
		pport->name = dma_name[DMA3];
		pport->rxendi = 3;
		pport->txendi = 3;
		pport->rxbl = burst_len_to_burst_cfg(16);
		pport->txbl = burst_len_to_burst_cfg(16);
		pport->txwgt = DMA_TX_PORT_DEFAULT_WEIGHT;
		pport->pkt_drop = DMA_PKT_DROP_DISABLE;
		pport->flush_memcpy = 0;	/* AVM:4298 DMA3/DMA4 */
		pport->chans = kcalloc(pport->chan_nrs, sizeof(*pch),
				       GFP_KERNEL);
		if (!pport->chans)
			return -ENOMEM;
		for (i = 0; i < pport->chan_nrs; i++) {
			pch = &pport->chans[i];
			pch->nr = i;
			pch->onoff = DMA_CH_OFF;
			pch->rst = DMA_CHAN_RST;
			pch->pkt_size = DMA_PKT_SIZE_DEFAULT;
			pch->lnr = -1;
		}
	}

	/* The AVM dma_ctrl_init hardware sequence */
	dma_ctrl_reset(pctrl);
	dma_ctrl_cfg(pctrl);
	for (i = 0; i < pctrl->port_nrs; i++) {
		pport = &pctrl->ports[i];
		dma_set_port_controller_data(pport, pctrl);
		dma_port_cfg(pport);
		for (j = 0; j < pport->chan_nrs; j++) {
			pch = &pport->chans[j];
			dma_set_chan_controller_data(pch, pctrl);
			dma_set_chan_port_data(pch, pport);
			dma_chan_cfg(pch);
		}
	}

	pr_info("hdma: TOE/DMA3 reset done: DMA_CTRL=0x%08x CPOLL=0x%08x\n",
		ltq_dma_r32(pctrl, DMA_CTRL), ltq_dma_r32(pctrl, DMA_CPOLL));
	return 0;
}
EXPORT_SYMBOL_GPL(hdma_ig192_toe_dma3_reset);

int hdma_port_enable(int port_id)
{
	const struct port7_chan_desc *tbl;
	size_t n, i;
	int ret;
	u32 chan;
	struct dmax_chan *pch;

	if (hdma_deq_armed(port_id))
		return 0;

	tbl = hdma_port_chan_tbl(port_id, &n);
	if (!tbl)
		return -EINVAL;

	for (i = 0; i < n; i++) {
		const struct port7_chan_desc *e = &tbl[i];

		chan = _DMA_C(e->cid, e->pid, e->nid);
		ret = ltq_request_dma(chan, e->label);
		if (ret) {
			pr_err("hdma_port_enable(%d): ltq_request_dma(%s) failed: %d\n",
			       port_id, e->label, ret);
			return ret;
		}

		if (e->cbm_deq) {

			ret = init_cbm_dqm_dma_port(e->cbm_deq);
			if (ret) {
				pr_err("hdma_port_enable(%d): init_cbm_dqm_dma_port(%d) failed: %d\n",
				       port_id, e->cbm_deq, ret);
				goto err_release;
			}

			ret = ltq_dma_chan_desc_cfg(chan,
						    CBM_DQM_DESC_PHYS(e->cbm_deq),
						    CBM_DQM_DESC_NUM);
			if (ret) {
				pr_err("hdma_port_enable(%d): ltq_dma_chan_desc_cfg(%s, DQM-window) failed: %d\n",
				       port_id, e->label, ret);
				goto err_release;
			}
			pr_info("hdma_port_enable(%d): %s CBM-managed, desc@phys 0x%08x x%u\n",
				port_id, e->label,
				(u32)CBM_DQM_DESC_PHYS(e->cbm_deq),
				CBM_DQM_DESC_NUM);
			goto chan_on;
		}

		ret = ltq_dma_chan_desc_alloc(chan, HDMA_PORT_DESC_NUM);
		if (ret) {
			pr_err("hdma_port_enable(%d): ltq_dma_chan_desc_alloc(%s) failed: %d\n",
			       port_id, e->label, ret);
			goto err_release;
		}
		pch = dma_chan_l2p(chan);
		if (!pch) {
			pr_err("hdma_port_enable(%d): dma_chan_l2p(%s) returned NULL\n",
			       port_id, e->label);
			ret = -EINVAL;
			goto err_release;
		}
		ret = ltq_dma_chan_desc_cfg(chan, pch->desc_phys,
					    HDMA_PORT_DESC_NUM);
		if (ret) {
			pr_err("hdma_port_enable(%d): ltq_dma_chan_desc_cfg(%s) failed: %d\n",
			       port_id, e->label, ret);
			goto err_release;
		}
chan_on:
		ltq_dma_chan_irq_disable(chan);
		ret = ltq_dma_chan_open(chan);
		if (ret) {
			pr_err("hdma_port_enable(%d): ltq_dma_chan_open(%s) failed: %d\n",
			       port_id, e->label, ret);
			goto err_release;
		}
		pr_info("hdma_port_enable(%d): opened %s (cid=%u pid=%u nid=%u)\n",
			port_id, e->label, e->cid, e->pid, e->nid);
	}

	hdma_deq_set_armed(port_id);
	return 0;

err_release:
	while (1) {
		const struct port7_chan_desc *e = &tbl[i];
		u32 c = _DMA_C(e->cid, e->pid, e->nid);

		ltq_dma_chan_close(c);
		ltq_dma_chan_desc_free(c);
		ltq_free_dma(c);
		if (i == 0)
			break;
		i--;
	}
	return ret;
}

/*
 * Without this, every ndo_stop leaked the DMA channels hdma_port_enable
 * opened.
 *
 * Teardown ordering — strict reverse of hdma_port_enable's
 * (request -> desc_alloc -> desc_cfg -> chan_open):
 *
 *   (1) ltq_dma_chan_close   reverses ltq_dma_chan_open (off + irq_disable)
 *   (2) ltq_dma_chan_desc_free reverses ltq_dma_chan_desc_alloc
 *       (also rolls back desc_cfg state via desc_configured=false in
 *        ltq_dma_chan_desc_free's body)
 *   (3) ltq_free_dma         reverses ltq_request_dma
 *
 * Per-port channel source-of-truth: hdma_port_chan_tbl() (the same lookup
 * hdma_port_enable uses).
 *
 * Failure handling: a single failing step records the error code into
 * `partial` and the loop continues.
 *
 * No AVM analogue because AVM never carried an hdma_port_enable wrapper
 * either.
 */
int hdma_port_disable(int port_id)
{
	const struct port7_chan_desc *tbl;
	size_t n, i;
	int rc;
	int first_err = 0;
	u32 chan;

	/*
	 * Never tear down a CBM-managed DMA2TX egress channel from the netdev
	 * path: the descriptors belong to the CBM dequeue port, not to this
	 * driver.
	 */
	if (hdma_deq_armed(port_id))
		return 0;

	tbl = hdma_port_chan_tbl(port_id, &n);
	if (!tbl)
		return -EINVAL;

	for (i = 0; i < n; i++) {
		const struct port7_chan_desc *e = &tbl[i];

		chan = _DMA_C(e->cid, e->pid, e->nid);

		/* (1) inverse of ltq_dma_chan_open. */
		rc = ltq_dma_chan_close(chan);
		if (rc) {
			pr_err("hdma_port_disable(%d): ltq_dma_chan_close(%s) failed: %d\n",
			       port_id, e->label, rc);
			if (!first_err)
				first_err = rc;
		}

		/*
		 * (2) inverse of ltq_dma_chan_desc_alloc /
		 *     ltq_dma_chan_desc_cfg. ltq_dma_chan_desc_free is
		 *     idempotent (returns 0 on NULL desc_base) so a
		 *     prior partial-teardown path is safe.
		 */
		rc = ltq_dma_chan_desc_free(chan);
		if (rc) {
			pr_err("hdma_port_disable(%d): ltq_dma_chan_desc_free(%s) failed: %d\n",
			       port_id, e->label, rc);
			if (!first_err)
				first_err = rc;
		}

		/* (3) inverse of ltq_request_dma. */
		rc = ltq_free_dma(chan);
		if (rc) {
			pr_err("hdma_port_disable(%d): ltq_free_dma(%s) failed: %d\n",
			       port_id, e->label, rc);
			if (!first_err)
				first_err = rc;
		} else {
			pr_info("hdma_port_disable(%d): released %s (cid=%u pid=%u nid=%u)\n",
				port_id, e->label, e->cid, e->pid, e->nid);
		}
	}
	return first_err;
}

