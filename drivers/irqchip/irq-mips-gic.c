/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2012 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/bitmap.h>
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/mips-gic.h>
#include <linux/of_address.h>
#include <linux/sched.h>
#include <linux/smp.h>

#include <asm/mips-cm.h>
#include <asm/setup.h>
#include <asm/traps.h>
#include <asm/yield_context.h>

#include <dt-bindings/interrupt-controller/mips-gic.h>

#if defined(CONFIG_AVM_ENHANCED)
#include <linux/proc_fs.h>
#include <avm/sammel/simple_proc.h>
#include <linux/kernel_stat.h>
#endif/*--- #if defined(CONFIG_AVM_ENHANCED) ---*/
#if defined(CONFIG_AVM_POWER)
#include <avm/power/power.h>
#else
#define avm_cpu_wait_end()
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/

/* The following defintions must match the static interrupt routing table */
#define GIC_VI2_NUM_INTRS 64
#define GIC_VI3_NUM_INTRS 64
#define GIC_VI4_NUM_INTRS 32
#define GIC_VI5_NUM_INTRS 32
#define GIC_VI6_NUM_INTRS 64

#define GIC_VI2_INTRS_BASE 0
#define GIC_VI3_INTRS_BASE GIC_VI2_NUM_INTRS
#define GIC_VI4_INTRS_BASE (GIC_VI2_NUM_INTRS + GIC_VI3_NUM_INTRS)
#define GIC_VI5_INTRS_BASE (GIC_VI2_NUM_INTRS + GIC_VI3_NUM_INTRS + GIC_VI4_NUM_INTRS)
#define GIC_VI6_INTRS_BASE (GIC_VI2_NUM_INTRS + GIC_VI3_NUM_INTRS + GIC_VI4_NUM_INTRS + GIC_VI5_NUM_INTRS)

/* offset = (irq_base /32) *4 = irq_base >> 3 */
#define GIC_VI2_SH_PEND (GIC_SH_PEND_OFS + (GIC_VI2_INTRS_BASE >> 3))
#define GIC_VI2_SH_MASK (GIC_SH_MASK_OFS + (GIC_VI2_INTRS_BASE >> 3))

#define GIC_VI3_SH_PEND (GIC_SH_PEND_OFS + (GIC_VI3_INTRS_BASE >> 3))
#define GIC_VI3_SH_MASK (GIC_SH_MASK_OFS + (GIC_VI3_INTRS_BASE >> 3))

#define GIC_VI4_SH_PEND (GIC_SH_PEND_OFS + (GIC_VI4_INTRS_BASE >> 3))
#define GIC_VI4_SH_MASK (GIC_SH_MASK_OFS + (GIC_VI4_INTRS_BASE >> 3))

#define GIC_VI5_SH_PEND (GIC_SH_PEND_OFS + (GIC_VI5_INTRS_BASE >> 3))
#define GIC_VI5_SH_MASK (GIC_SH_MASK_OFS + (GIC_VI5_INTRS_BASE >> 3))

#define GIC_VI6_SH_PEND (GIC_SH_PEND_OFS + (GIC_VI6_INTRS_BASE >> 3))
#define GIC_VI6_SH_MASK (GIC_SH_MASK_OFS + (GIC_VI6_INTRS_BASE >> 3))

unsigned int gic_present;

struct gic_pcpu_mask {
	DECLARE_BITMAP(pcpu_mask, GIC_MAX_INTRS);
};

struct gic_irq_spec {
	enum {
		GIC_DEVICE,
		GIC_IPI
	} type;

	union {
		struct cpumask *ipimask;
		unsigned int hwirq;
	};
};

static unsigned long __gic_base_addr;

static void __iomem *gic_base;
static struct gic_pcpu_mask pcpu_masks[NR_CPUS];
static DEFINE_SPINLOCK(gic_lock);
static struct irq_domain *gic_irq_domain;
static struct irq_domain *gic_dev_domain;
static struct irq_domain *gic_ipi_domain;
static int gic_shared_intrs;
static unsigned int gic_vpes;
static unsigned int gic_cpu_pin;
static unsigned int timer_cpu_pin;
static struct irq_chip gic_level_irq_controller, gic_edge_irq_controller;
DECLARE_BITMAP(ipi_resrv, GIC_MAX_INTRS);
DECLARE_BITMAP(ipi_available, GIC_MAX_INTRS);
static unsigned int gic_reserved_list[GIC_MAX_INTRS];
static unsigned int gic_reserved_list_count;

static void __gic_irq_dispatch(void);

static u32 gic_irq_to_pin(unsigned int irq)
{
	if (irq < GIC_VI3_INTRS_BASE)
		return GIC_CPU_INT0;
	else if (irq < GIC_VI4_INTRS_BASE)
		return GIC_CPU_INT1;
	else if (irq < GIC_VI5_INTRS_BASE)
		return GIC_CPU_INT2;
	else if (irq < GIC_VI6_INTRS_BASE)
		return GIC_CPU_INT3;
	else
		return GIC_CPU_INT4;
}

static inline u32 gic_read32(unsigned int reg)
{
	return __raw_readl(gic_base + reg);
}

static inline u64 gic_read64(unsigned int reg)
{
	return __raw_readq(gic_base + reg);
}

static inline unsigned long gic_read(unsigned int reg)
{
	if (!mips_cm_is64)
		return gic_read32(reg);
	else
		return gic_read64(reg);
}

static inline void gic_write32(unsigned int reg, u32 val)
{
	return __raw_writel(val, gic_base + reg);
}

static inline void gic_write64(unsigned int reg, u64 val)
{
	return __raw_writeq(val, gic_base + reg);
}

static inline void gic_write(unsigned int reg, unsigned long val)
{
	if (!mips_cm_is64)
		return gic_write32(reg, (u32)val);
	else
		return gic_write64(reg, (u64)val);
}

static inline void gic_update_bits(unsigned int reg, unsigned long mask,
				   unsigned long val)
{
	unsigned long regval;

	regval = gic_read(reg);
	regval &= ~mask;
	regval |= val;
	gic_write(reg, regval);
}

static inline void gic_reset_mask(unsigned int intr)
{
	gic_write(GIC_REG(SHARED, GIC_SH_RMASK) + GIC_INTR_OFS(intr),
		  1ul << GIC_INTR_BIT(intr));
}

static inline void gic_set_mask(unsigned int intr)
{
	gic_write(GIC_REG(SHARED, GIC_SH_SMASK) + GIC_INTR_OFS(intr),
		  1ul << GIC_INTR_BIT(intr));
}

static inline void gic_set_polarity(unsigned int intr, unsigned int pol)
{
	gic_update_bits(GIC_REG(SHARED, GIC_SH_SET_POLARITY) +
			GIC_INTR_OFS(intr), 1ul << GIC_INTR_BIT(intr),
			(unsigned long)pol << GIC_INTR_BIT(intr));
}

static inline void gic_set_trigger(unsigned int intr, unsigned int trig)
{
	gic_update_bits(GIC_REG(SHARED, GIC_SH_SET_TRIGGER) +
			GIC_INTR_OFS(intr), 1ul << GIC_INTR_BIT(intr),
			(unsigned long)trig << GIC_INTR_BIT(intr));
}

static inline void gic_set_dual_edge(unsigned int intr, unsigned int dual)
{
	gic_update_bits(GIC_REG(SHARED, GIC_SH_SET_DUAL) + GIC_INTR_OFS(intr),
			1ul << GIC_INTR_BIT(intr),
			(unsigned long)dual << GIC_INTR_BIT(intr));
}

static inline void gic_map_to_nmi(unsigned int intr)
{
	gic_write32(GIC_REG(SHARED, GIC_SH_INTR_MAP_TO_PIN_BASE) +
		    GIC_SH_MAP_TO_PIN(intr), GIC_MAP_TO_NMI_MSK);
}

static inline void gic_map_to_pin(unsigned int intr, unsigned int pin)
{
	gic_write32(GIC_REG(SHARED, GIC_SH_INTR_MAP_TO_PIN_BASE) +
		    GIC_SH_MAP_TO_PIN(intr), GIC_MAP_TO_PIN_MSK | pin);
}

static inline void gic_yield_map_to_pin(unsigned int intr, unsigned int pin)
{
	gic_write32(GIC_REG(SHARED, GIC_SH_INTR_MAP_TO_PIN_BASE) +
		    GIC_SH_MAP_TO_PIN(intr),
		    GIC_MAP_TO_YQ_MSK | pin);
}

static inline void gic_map_to_vpe(unsigned int intr, unsigned int vpe)
{
	gic_write(GIC_REG(SHARED, GIC_SH_INTR_MAP_TO_VPE_BASE) +
		  GIC_SH_MAP_TO_VPE_REG_OFF(intr, vpe),
		  GIC_SH_MAP_TO_VPE_REG_BIT(vpe));
}

static inline unsigned int gic_local_irq_map_reg(unsigned int intr)
{
	unsigned int i;
	static const unsigned int local_irq_vpeother_reg_map[GIC_NUM_LOCAL_INTRS][2] =  {
		{ GIC_LOCAL_INT_WD,	 GIC_REG(VPE_OTHER, GIC_VPE_WD_MAP)	 },
		{ GIC_LOCAL_INT_COMPARE, GIC_REG(VPE_OTHER, GIC_VPE_COMPARE_MAP) },
		{ GIC_LOCAL_INT_TIMER,	 GIC_REG(VPE_OTHER, GIC_VPE_TIMER_MAP)	 },
		{ GIC_LOCAL_INT_PERFCTR, GIC_REG(VPE_OTHER, GIC_VPE_PERFCTR_MAP) },
		{ GIC_LOCAL_INT_SWINT0,	 GIC_REG(VPE_OTHER, GIC_VPE_SWINT0_MAP)	 },
		{ GIC_LOCAL_INT_SWINT1,  GIC_REG(VPE_OTHER, GIC_VPE_SWINT1_MAP)	 },
		{ GIC_LOCAL_INT_FDC,	 GIC_REG(VPE_OTHER, GIC_VPE_FDC_MAP)	 },
	};

	for (i = 0; i < ARRAY_SIZE(local_irq_vpeother_reg_map); i++) {
		if (local_irq_vpeother_reg_map[i][0] == intr) {
			return local_irq_vpeother_reg_map[i][1];
		}
	}
	return (unsigned int)(-EINVAL);
}

static inline void gic_local_irq_write_map_reg(int vpe, unsigned int intr, uint32_t val)
{
	unsigned long flags;
	unsigned int reg;

	reg = gic_local_irq_map_reg(intr);
	if (reg == (unsigned int)(-EINVAL)) {
		return;
	}
	spin_lock_irqsave(&gic_lock, flags);
	gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), vpe);
	gic_write(reg, val);
	spin_unlock_irqrestore(&gic_lock, flags);
}

static inline void gic_local_irq_map_to_nmi(int vpe, unsigned int intr)
{
	gic_local_irq_write_map_reg(vpe, intr, GIC_MAP_TO_NMI_MSK);
}

static inline void gic_local_irq_map_to_pin(int vpe, unsigned int intr, unsigned int pin)
{
	gic_local_irq_write_map_reg(vpe, intr, GIC_MAP_TO_PIN_MSK | pin);
}

static inline void gic_local_irq_yield_map_to_pin(int vpe, unsigned int intr, unsigned int pin)
{
	gic_local_irq_write_map_reg(vpe, intr, GIC_MAP_TO_YQ_MSK | pin);
}

cycle_t gic_read_count(void)
{
	unsigned int hi, hi2, lo;

	if (mips_cm_is64)
		return (cycle_t)gic_read(GIC_REG(SHARED, GIC_SH_COUNTER));

	do {
		hi = gic_read32(GIC_REG(SHARED, GIC_SH_COUNTER_63_32));
		lo = gic_read32(GIC_REG(SHARED, GIC_SH_COUNTER_31_00));
		hi2 = gic_read32(GIC_REG(SHARED, GIC_SH_COUNTER_63_32));
	} while (hi2 != hi);

	return (((cycle_t) hi) << 32) + lo;
}

unsigned int gic_get_count_width(void)
{
	unsigned int bits, config;

	config = gic_read(GIC_REG(SHARED, GIC_SH_CONFIG));
	bits = 32 + 4 * ((config & GIC_SH_CONFIG_COUNTBITS_MSK) >>
			 GIC_SH_CONFIG_COUNTBITS_SHF);

	return bits;
}

void gic_write_compare(cycle_t cnt)
{
	if (mips_cm_is64) {
		gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_COMPARE), cnt);
	} else {
		gic_write32(GIC_REG(VPE_LOCAL, GIC_VPE_COMPARE_HI),
					(int)(cnt >> 32));
		gic_write32(GIC_REG(VPE_LOCAL, GIC_VPE_COMPARE_LO),
					(int)(cnt & 0xffffffff));
	}
}

void gic_write_cpu_compare(cycle_t cnt, int cpu)
{
	unsigned long flags;

	yield_spin_lock_irqsave(&gic_lock, flags);

	gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), mips_cm_vp_id(cpu));

	if (mips_cm_is64) {
		gic_write(GIC_REG(VPE_OTHER, GIC_VPE_COMPARE), cnt);
	} else {
		gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_COMPARE_HI),
					(int)(cnt >> 32));
		gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_COMPARE_LO),
					(int)(cnt & 0xffffffff));
	}

	yield_spin_unlock_irqrestore(&gic_lock, flags);
}

/**
 */
void gic_write_compare_for_cpu(int cpu, cycle_t cnt)
{
	unsigned long flags;

	yield_spin_lock_irqsave(&gic_lock, flags);
	gic_write32(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), mips_cm_vp_id(cpu));
	gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_COMPARE_HI), (int)(cnt >> 32));
	gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_COMPARE_LO), (int)(cnt & 0xffffffff));
	yield_spin_unlock_irqrestore(&gic_lock, flags);
}

#ifdef CONFIG_CLKSRC_MIPS_GIC
cycle_t gic_read_compare(void)
{
	unsigned int hi, lo;

	if (mips_cm_is64)
		return (cycle_t)gic_read(GIC_REG(VPE_LOCAL, GIC_VPE_COMPARE));

	hi = gic_read32(GIC_REG(VPE_LOCAL, GIC_VPE_COMPARE_HI));
	lo = gic_read32(GIC_REG(VPE_LOCAL, GIC_VPE_COMPARE_LO));

	return (((cycle_t) hi) << 32) + lo;
}

void gic_start_count(void)
{
	u32 gicconfig;

	/* Start the counter */
	gicconfig = gic_read(GIC_REG(SHARED, GIC_SH_CONFIG));
	gicconfig &= ~(1 << GIC_SH_CONFIG_COUNTSTOP_SHF);
	gic_write(GIC_REG(SHARED, GIC_SH_CONFIG), gicconfig);
}

void gic_stop_count(void)
{
	u32 gicconfig;

	/* Stop the counter */
	gicconfig = gic_read(GIC_REG(SHARED, GIC_SH_CONFIG));
	gicconfig |= 1 << GIC_SH_CONFIG_COUNTSTOP_SHF;
	gic_write(GIC_REG(SHARED, GIC_SH_CONFIG), gicconfig);
}

#endif

unsigned int gic_read_local_vp_id(void)
{
	unsigned long ident;

	ident = gic_read(GIC_REG(VPE_LOCAL, GIC_VP_IDENT));
	return ident & GIC_VP_IDENT_VCNUM_MSK;
}

static bool gic_local_irq_is_routable(int intr)
{
	u32 vpe_ctl;

	/* All local interrupts are routable in EIC mode. */
	if (cpu_has_veic)
		return true;

	vpe_ctl = gic_read32(GIC_REG(VPE_LOCAL, GIC_VPE_CTL));
	switch (intr) {
	case GIC_LOCAL_INT_TIMER:
		return vpe_ctl & GIC_VPE_CTL_TIMER_RTBL_MSK;
	case GIC_LOCAL_INT_PERFCTR:
		return vpe_ctl & GIC_VPE_CTL_PERFCNT_RTBL_MSK;
	case GIC_LOCAL_INT_FDC:
		return vpe_ctl & GIC_VPE_CTL_FDC_RTBL_MSK;
	case GIC_LOCAL_INT_SWINT0:
	case GIC_LOCAL_INT_SWINT1:
		return vpe_ctl & GIC_VPE_CTL_SWINT_RTBL_MSK;
	default:
		return true;
	}
}

static void gic_bind_eic_interrupt(int irq, int set)
{
	/* Convert irq vector # to hw int # */
	irq -= GIC_PIN_TO_VEC_OFFSET;

	/* Set irq to use shadow set */
	gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_EIC_SHADOW_SET_BASE) +
		  GIC_VPE_EIC_SS(irq), set);
}

static void gic_send_ipi(struct irq_data *d, unsigned int cpu)
{
	irq_hw_number_t hwirq = GIC_HWIRQ_TO_SHARED(irqd_to_hwirq(d));

	gic_write(GIC_REG(SHARED, GIC_SH_WEDGE), GIC_SH_WEDGE_SET(hwirq));
}

int gic_get_c0_compare_int(void)
{
	if (!gic_local_irq_is_routable(GIC_LOCAL_INT_TIMER))
		return MIPS_CPU_IRQ_BASE + cp0_compare_irq;
	return irq_create_mapping(gic_irq_domain,
				  GIC_LOCAL_TO_HWIRQ(GIC_LOCAL_INT_TIMER));
}

int gic_get_c0_perfcount_int(void)
{
	if (!gic_local_irq_is_routable(GIC_LOCAL_INT_PERFCTR)) {
		/* Is the performance counter shared with the timer? */
		if (cp0_perfcount_irq < 0)
			return -1;
		return MIPS_CPU_IRQ_BASE + cp0_perfcount_irq;
	}
	return irq_create_mapping(gic_irq_domain,
				  GIC_LOCAL_TO_HWIRQ(GIC_LOCAL_INT_PERFCTR));
}

int gic_get_c0_fdc_int(void)
{
	if (!gic_local_irq_is_routable(GIC_LOCAL_INT_FDC)) {
		/* Is the FDC IRQ even present? */
		if (cp0_fdc_irq < 0)
			return -1;
		return MIPS_CPU_IRQ_BASE + cp0_fdc_irq;
	}

	return irq_create_mapping(gic_irq_domain,
				  GIC_LOCAL_TO_HWIRQ(GIC_LOCAL_INT_FDC));
}

int gic_get_usm_range(struct resource *gic_usm_res)
{
	if (!gic_present)
		return -1;

	gic_usm_res->start = __gic_base_addr + USM_VISIBLE_SECTION_OFS;
	gic_usm_res->end = gic_usm_res->start + (USM_VISIBLE_SECTION_SIZE - 1);

	return 0;
}

static void gic_handle_shared_int(bool chained)
{
	unsigned int i, intr, virq;
	unsigned long *pcpu_mask;
	unsigned long pending_reg, intrmask_reg;
	unsigned long pending;

	/* Get per-cpu bitmaps */
	pcpu_mask = pcpu_masks[smp_processor_id()].pcpu_mask;

	pending_reg = GIC_REG(SHARED, GIC_SH_PEND);
	intrmask_reg = GIC_REG(SHARED, GIC_SH_MASK);

	for (i = 0; i < BITS_TO_LONGS(gic_shared_intrs);) {
		pending = gic_read(pending_reg);
		pending &= gic_read(intrmask_reg);
		pending &= pcpu_mask[i];
		if ((intr = ffs(pending)-1) != -1) {
			virq = irq_linear_revmap(gic_irq_domain,
				GIC_SHARED_TO_HWIRQ(intr +
					i * BITS_PER_LONG));
			if (chained)
				generic_handle_irq(virq);
			else
				do_IRQ(virq);
			continue;
		}
		pending_reg += 4;
		intrmask_reg += 4;
		i++;
	}
}

#define GIC_SHARED_IRQ_DISPATCH(X)                                                                              \
	do {                                                                                                    \
		unsigned int i, intr, virq;                                                                     \
		unsigned long *pcpu_mask;                                                                       \
		unsigned long pending_reg, intrmask_reg;                                                        \
		unsigned long pending;                                                                          \
														\
		/* Get per-cpu bitmaps */                                                                       \
		pcpu_mask = pcpu_masks[smp_processor_id()].pcpu_mask + (GIC_VI##X##_INTRS_BASE >> 5);           \
		pending_reg = GIC_REG_ADDR(SHARED, GIC_VI##X##_SH_PEND);                                        \
		intrmask_reg = GIC_REG_ADDR(SHARED, GIC_VI##X##_SH_MASK);                                       \
		for (i = 0; i < BITS_TO_LONGS(GIC_VI##X##_NUM_INTRS); ) {                                       \
			pending = gic_read(pending_reg);                                                        \
			pending &= gic_read(intrmask_reg);                                                      \
			pending &= pcpu_mask[i];                                                                \
			if ((intr = ffs(pending)-1) != -1) {                                                    \
				virq = irq_linear_revmap(gic_irq_domain,                                        \
					GIC_SHARED_TO_HWIRQ(intr + i * BITS_PER_LONG + GIC_VI##X##_INTRS_BASE));\
				do_IRQ(virq);                                                                   \
				continue;                                                                       \
			}                                                                                       \
			pending_reg += 4;                                                                       \
			intrmask_reg += 4;                                                                      \
			i++;                                                                                    \
		}                                                                                               \
	} while (0)

#define GIC_VIx_IRQ_DISPATCH(x)                           \
	static void gic_shared_irq_vi##x##_dispatch(void) \
	{                                                 \
		avm_cpu_wait_end();                       \
		GIC_SHARED_IRQ_DISPATCH(x);               \
	}

GIC_VIx_IRQ_DISPATCH(2)
	GIC_VIx_IRQ_DISPATCH(3)
		GIC_VIx_IRQ_DISPATCH(4)
			GIC_VIx_IRQ_DISPATCH(5)
				GIC_VIx_IRQ_DISPATCH(6)

					static void gic_mask_irq(struct irq_data *d)
{
	gic_reset_mask(GIC_HWIRQ_TO_SHARED(d->hwirq));
}

static void gic_unmask_irq(struct irq_data *d)
{
	gic_set_mask(GIC_HWIRQ_TO_SHARED(d->hwirq));
}

static void gic_ack_irq(struct irq_data *d)
{
	unsigned int irq = GIC_HWIRQ_TO_SHARED(d->hwirq);

	gic_write(GIC_REG(SHARED, GIC_SH_WEDGE), GIC_SH_WEDGE_CLR(irq));
}

static int gic_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int irq = GIC_HWIRQ_TO_SHARED(d->hwirq);
	unsigned long flags;
	bool is_edge;

	spin_lock_irqsave(&gic_lock, flags);
	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_EDGE_FALLING:
		gic_set_polarity(irq, GIC_POL_NEG);
		gic_set_trigger(irq, GIC_TRIG_EDGE);
		gic_set_dual_edge(irq, GIC_TRIG_DUAL_DISABLE);
		is_edge = true;
		break;
	case IRQ_TYPE_EDGE_RISING:
		gic_set_polarity(irq, GIC_POL_POS);
		gic_set_trigger(irq, GIC_TRIG_EDGE);
		gic_set_dual_edge(irq, GIC_TRIG_DUAL_DISABLE);
		is_edge = true;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		/* polarity is irrelevant in this case */
		gic_set_trigger(irq, GIC_TRIG_EDGE);
		gic_set_dual_edge(irq, GIC_TRIG_DUAL_ENABLE);
		is_edge = true;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		gic_set_polarity(irq, GIC_POL_NEG);
		gic_set_trigger(irq, GIC_TRIG_LEVEL);
		gic_set_dual_edge(irq, GIC_TRIG_DUAL_DISABLE);
		is_edge = false;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
	default:
		gic_set_polarity(irq, GIC_POL_POS);
		gic_set_trigger(irq, GIC_TRIG_LEVEL);
		gic_set_dual_edge(irq, GIC_TRIG_DUAL_DISABLE);
		is_edge = false;
		break;
	}

	if (is_edge)
		irq_set_chip_handler_name_locked(d, &gic_edge_irq_controller,
						 handle_edge_irq, NULL);
	else
		irq_set_chip_handler_name_locked(d, &gic_level_irq_controller,
						 handle_level_irq, NULL);
	spin_unlock_irqrestore(&gic_lock, flags);

	return 0;
}

#ifdef CONFIG_SMP
static int gic_set_affinity(struct irq_data *d, const struct cpumask *cpumask,
			    bool force)
{
	unsigned int irq = GIC_HWIRQ_TO_SHARED(d->hwirq);
	cpumask_t	tmp = CPU_MASK_NONE;
	unsigned long	flags;
	unsigned int	i;

	cpumask_and(&tmp, cpumask, cpu_possible_mask);
	if (cpumask_empty(&tmp))
		return -EINVAL;

	/* Assumption : cpumask refers to a single CPU */
	spin_lock_irqsave(&gic_lock, flags);

	/* Re-route this IRQ */
	gic_map_to_vpe(irq, mips_cm_vp_id(cpumask_first(&tmp)));

	/* Update the pcpu_masks */
	for (i = 0; i < min(gic_vpes, num_possible_cpus()); i++)
		clear_bit(irq, pcpu_masks[i].pcpu_mask);
	set_bit(irq, pcpu_masks[cpumask_first(&tmp)].pcpu_mask);

	cpumask_copy(irq_data_get_affinity_mask(d), cpumask);
	spin_unlock_irqrestore(&gic_lock, flags);

	return IRQ_SET_MASK_OK_NOCOPY;
}

void gic_send_ipi_simple(unsigned int hwirq, unsigned int cpu)
{
	gic_write(GIC_REG(SHARED, GIC_SH_WEDGE), GIC_SH_WEDGE_SET(hwirq));
}
EXPORT_SYMBOL_GPL(gic_send_ipi_simple);

int gic_trigger_irq(unsigned int irq, unsigned int set)
{
	irq_hw_number_t hwirq;
	struct irq_data *d = irq_get_irq_data(irq);

	if (unlikely(!d))
		return -EINVAL;

	hwirq = GIC_HWIRQ_TO_SHARED(irqd_to_hwirq(d));

	gic_write(GIC_REG(SHARED, GIC_SH_WEDGE), set ? GIC_SH_WEDGE_SET(hwirq) : GIC_SH_WEDGE_CLR(hwirq));
	return 0;
}
EXPORT_SYMBOL(gic_trigger_irq);

int gic_clear_edge(unsigned int irq)
{
	irq_hw_number_t hwirq;
	struct irq_data *d = irq_get_irq_data(irq);

	if (unlikely(!d))
		return -EINVAL;

	hwirq = GIC_HWIRQ_TO_SHARED(irqd_to_hwirq(d));

	gic_write(GIC_REG(SHARED, GIC_SH_WEDGE), hwirq);
	return 0;
}
EXPORT_SYMBOL_GPL(gic_clear_edge);

#endif

static struct irq_chip gic_level_irq_controller = {
	.name = "MIPS GIC",
	.irq_enable = gic_unmask_irq,
	.irq_disable = gic_mask_irq,
	.irq_mask = gic_mask_irq,
	.irq_unmask = gic_unmask_irq,
	.irq_set_type = gic_set_type,
#ifdef CONFIG_SMP
	.irq_set_affinity = gic_set_affinity,
#endif
};

static struct irq_chip gic_edge_irq_controller = {
	.name = "MIPS GIC",
	.irq_enable = gic_unmask_irq,
	.irq_disable = gic_mask_irq,
	.irq_ack = gic_ack_irq,
	.irq_mask = gic_mask_irq,
	.irq_unmask = gic_unmask_irq,
	.irq_set_type = gic_set_type,
#ifdef CONFIG_SMP
	.irq_set_affinity = gic_set_affinity,
#endif
	.ipi_send_single = gic_send_ipi,
};

static void gic_handle_local_int(bool chained)
{
	unsigned long pending, masked;
	unsigned int intr, virq;

	pending = gic_read32(GIC_REG(VPE_LOCAL, GIC_VPE_PEND));
	masked = gic_read32(GIC_REG(VPE_LOCAL, GIC_VPE_MASK));

	bitmap_and(&pending, &pending, &masked, GIC_NUM_LOCAL_INTRS);

	for_each_set_bit(intr, &pending, GIC_NUM_LOCAL_INTRS) {
		virq = irq_linear_revmap(gic_irq_domain,
					 GIC_LOCAL_TO_HWIRQ(intr));
		if (chained)
			generic_handle_irq(virq);
		else
			do_IRQ(virq);
	}
}

static void gic_local_irq_dispatch(void)
{
	gic_handle_local_int(false);
}

static void gic_mask_local_irq(struct irq_data *d)
{
	int intr = GIC_HWIRQ_TO_LOCAL(d->hwirq);

	gic_write32(GIC_REG(VPE_LOCAL, GIC_VPE_RMASK), 1 << intr);
}

static void gic_unmask_local_irq(struct irq_data *d)
{
	int intr = GIC_HWIRQ_TO_LOCAL(d->hwirq);

	gic_write32(GIC_REG(VPE_LOCAL, GIC_VPE_SMASK), 1 << intr);
}

static void gic_mask_local_irq_vpeother(int vpe, int local_irq)
{
	unsigned long flags;

	spin_lock_irqsave(&gic_lock, flags);
	gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), vpe);
	gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_RMASK), 1 << local_irq);
	spin_unlock_irqrestore(&gic_lock, flags);
}

static void gic_unmask_local_irq_vpeother(int vpe, int local_irq)
{
	unsigned long flags;

	spin_lock_irqsave(&gic_lock, flags);
	gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), vpe);
	gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_SMASK), 1 << local_irq);
	spin_unlock_irqrestore(&gic_lock, flags);
}

static struct irq_chip gic_local_irq_controller = {
	.name			=	"MIPS GIC Local",
	.irq_mask		=	gic_mask_local_irq,
	.irq_unmask		=	gic_unmask_local_irq,
};

static void gic_mask_local_irq_all_vpes(struct irq_data *d)
{
	int intr = GIC_HWIRQ_TO_LOCAL(d->hwirq);
	int i;
	unsigned long flags;

	spin_lock_irqsave(&gic_lock, flags);
	for (i = 0; i < gic_vpes; i++) {
		gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR),
			  mips_cm_vp_id(i));
		gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_RMASK), 1 << intr);
	}
	spin_unlock_irqrestore(&gic_lock, flags);
}

static void gic_unmask_local_irq_all_vpes(struct irq_data *d)
{
	int intr = GIC_HWIRQ_TO_LOCAL(d->hwirq);
	int i;
	unsigned long flags;

	spin_lock_irqsave(&gic_lock, flags);
	for (i = 0; i < gic_vpes; i++) {
		gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR),
			  mips_cm_vp_id(i));
		gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_SMASK), 1 << intr);
	}
	spin_unlock_irqrestore(&gic_lock, flags);
}

static struct irq_chip gic_all_vpes_local_irq_controller = {
	.name			=	"MIPS GIC Local",
	.irq_mask		=	gic_mask_local_irq_all_vpes,
	.irq_unmask		=	gic_unmask_local_irq_all_vpes,
};

static void __gic_irq_dispatch(void)
{
	gic_handle_local_int(false);
	gic_handle_shared_int(false);
}

static void gic_irq_dispatch(struct irq_desc *desc)
{
	gic_handle_local_int(true);
	gic_handle_shared_int(true);
}

static bool gic_reserved_list_pins(unsigned int pin)
{
	unsigned int i;

	for (i = 0; i < gic_reserved_list_count; i++) {
		if (gic_reserved_list[i] == pin)
			return true;
	}
	return false;
}

static void __init gic_basic_init(void)
{
	unsigned int i;

	board_bind_eic_interrupt = &gic_bind_eic_interrupt;

	/* Setup defaults */
	for (i = 0; i < gic_shared_intrs; i++) {
		if (!gic_reserved_list_pins(i)) {
			gic_set_polarity(i, GIC_POL_POS);
			gic_set_trigger(i, GIC_TRIG_LEVEL);
			gic_reset_mask(i);
		}
	}

	for (i = 0; i < gic_vpes; i++) {
		unsigned int j;

		gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR),
			  mips_cm_vp_id(i));
		for (j = 0; j < GIC_NUM_LOCAL_INTRS; j++) {
			if (!gic_local_irq_is_routable(j))
				continue;
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_RMASK), 1 << j);
		}
	}
}

static int gic_local_int_pin_init(int intr)
{
	int i;
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&gic_lock, flags);
	for (i = 0; i < gic_vpes; i++) {
		u32 val = GIC_MAP_TO_PIN_MSK | gic_cpu_pin;

		gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR),
			  mips_cm_vp_id(i));

		switch (intr) {
		case GIC_LOCAL_INT_WD:
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_WD_MAP), val);
			break;
		case GIC_LOCAL_INT_COMPARE:
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_COMPARE_MAP),
				    val);
			break;
		case GIC_LOCAL_INT_TIMER:
			/* CONFIG_MIPS_CMP workaround (see __gic_init) */
			val = GIC_MAP_TO_PIN_MSK | timer_cpu_pin;
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_TIMER_MAP),
				    val);
			break;
		case GIC_LOCAL_INT_PERFCTR:
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_PERFCTR_MAP),
				    val);
			break;
		case GIC_LOCAL_INT_SWINT0:
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_SWINT0_MAP),
				    val);
			break;
		case GIC_LOCAL_INT_SWINT1:
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_SWINT1_MAP),
				    val);
			break;
		case GIC_LOCAL_INT_FDC:
			gic_write32(GIC_REG(VPE_OTHER, GIC_VPE_FDC_MAP), val);
			break;
		default:
			pr_err("Invalid local IRQ %d\n", intr);
			ret = -EINVAL;
			break;
		}
	}
	spin_unlock_irqrestore(&gic_lock, flags);

	return ret;
}

static int gic_local_irq_domain_map(struct irq_domain *d, unsigned int virq,
				    irq_hw_number_t hw)
{
	int intr = GIC_HWIRQ_TO_LOCAL(hw);

	if (!gic_local_irq_is_routable(intr))
		return -EPERM;

	return gic_local_int_pin_init(intr);
}

static int gic_shared_irq_domain_map(struct irq_domain *d, unsigned int virq,
				     irq_hw_number_t hw, unsigned int vpe)
{
	int intr = GIC_HWIRQ_TO_SHARED(hw);
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&gic_lock, flags);
	if (cpu_has_vint)
		gic_map_to_pin(intr, gic_irq_to_pin(intr));
	else
		gic_map_to_pin(intr, gic_cpu_pin);
	gic_map_to_vpe(intr, mips_cm_vp_id(vpe));
	for (i = 0; i < min(gic_vpes, num_possible_cpus()); i++)
		clear_bit(intr, pcpu_masks[i].pcpu_mask);
	set_bit(intr, pcpu_masks[vpe].pcpu_mask);
	spin_unlock_irqrestore(&gic_lock, flags);

	return 0;
}

static int gic_setup_dev_chip(struct irq_domain *d, unsigned int virq,
			      unsigned int hwirq)
{
	struct irq_chip *chip;
	int err;

	if (hwirq >= GIC_SHARED_HWIRQ_BASE) {
		err = irq_domain_set_hwirq_and_chip(d, virq, hwirq,
						    &gic_level_irq_controller,
						    NULL);
	} else {
		switch (GIC_HWIRQ_TO_LOCAL(hwirq)) {
		case GIC_LOCAL_INT_TIMER:
		case GIC_LOCAL_INT_PERFCTR:
		case GIC_LOCAL_INT_FDC:
			/*
			 * HACK: These are all really percpu interrupts, but
			 * the rest of the MIPS kernel code does not use the
			 * percpu IRQ API for them.
			 */
			chip = &gic_all_vpes_local_irq_controller;
			irq_set_handler(virq, handle_percpu_irq);
			break;

		default:
			chip = &gic_local_irq_controller;
			irq_set_handler(virq, handle_percpu_devid_irq);
			irq_set_percpu_devid(virq);
			break;
		}

		err = irq_domain_set_hwirq_and_chip(d, virq, hwirq,
						    chip, NULL);
	}

	return err;
}

static int gic_irq_domain_alloc(struct irq_domain *d, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	struct gic_irq_spec *spec = arg;
	irq_hw_number_t hwirq, base_hwirq;
	int cpu, ret, i;

	if (spec->type == GIC_DEVICE) {
		/* verify that shared irqs don't conflict with an IPI irq */
		if ((spec->hwirq >= GIC_SHARED_HWIRQ_BASE) &&
		    test_bit(GIC_HWIRQ_TO_SHARED(spec->hwirq), ipi_resrv))
			return -EBUSY;

		return gic_setup_dev_chip(d, virq, spec->hwirq);
	} else {
		base_hwirq = find_first_bit(ipi_available, gic_shared_intrs);
		if (base_hwirq == gic_shared_intrs) {
			return -ENOMEM;
		}

		/* check that we have enough space */
		for (i = base_hwirq; i < nr_irqs; i++) {
			if (!test_bit(i, ipi_available))
				return -EBUSY;
		}
		bitmap_clear(ipi_available, base_hwirq, nr_irqs);

		/* map the hwirq for each cpu consecutively */
		i = 0;
		for_each_cpu(cpu, spec->ipimask) {
			hwirq = GIC_SHARED_TO_HWIRQ(base_hwirq + i);

			ret = irq_domain_set_hwirq_and_chip(d, virq + i, hwirq,
							    &gic_level_irq_controller,
							    NULL);
			if (ret)
				goto error;

			irq_set_handler(virq + i, handle_level_irq);

			ret = gic_shared_irq_domain_map(d, virq + i, hwirq, cpu);
			if (ret)
				goto error;

			i++;
		}

		/*
		 * tell the parent about the base hwirq we allocated so it can
		 * set its own domain data
		 */
		spec->hwirq = base_hwirq;
	}

	return 0;
error:
	bitmap_set(ipi_available, base_hwirq, nr_irqs);
	return ret;
}

void gic_irq_domain_free(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs)
{
	irq_hw_number_t base_hwirq;
	struct irq_data *data;

	data = irq_get_irq_data(virq);
	if (!data)
		return;

	base_hwirq = GIC_HWIRQ_TO_SHARED(irqd_to_hwirq(data));
	bitmap_set(ipi_available, base_hwirq, nr_irqs);
}

int gic_irq_domain_match(struct irq_domain *d, struct device_node *node,
			 enum irq_domain_bus_token bus_token)
{
	/* this domain should'nt be accessed directly */
	return 0;
}

static const struct irq_domain_ops gic_irq_domain_ops = {
	.alloc = gic_irq_domain_alloc,
	.free = gic_irq_domain_free,
	.match = gic_irq_domain_match,
};

static int gic_dev_domain_xlate(struct irq_domain *d, struct device_node *ctrlr,
				const u32 *intspec, unsigned int intsize,
				irq_hw_number_t *out_hwirq,
				unsigned int *out_type)
{
	if (intsize != 3)
		return -EINVAL;

	if (intspec[0] == GIC_SHARED)
		*out_hwirq = GIC_SHARED_TO_HWIRQ(intspec[1]);
	else if (intspec[0] == GIC_LOCAL)
		*out_hwirq = GIC_LOCAL_TO_HWIRQ(intspec[1]);
	else
		return -EINVAL;
	*out_type = intspec[2] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static int gic_dev_domain_alloc(struct irq_domain *d, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	struct irq_fwspec *fwspec = arg;
	struct gic_irq_spec spec = {
		.type = GIC_DEVICE,
	};
	int i, ret;

	if (fwspec->param[0] == GIC_SHARED)
		spec.hwirq = GIC_SHARED_TO_HWIRQ(fwspec->param[1]);
	else
		spec.hwirq = GIC_LOCAL_TO_HWIRQ(fwspec->param[1]);

	ret = irq_domain_alloc_irqs_parent(d, virq, nr_irqs, &spec);
	if (ret)
		return ret;

	for (i = 0; i < nr_irqs; i++) {
		ret = gic_setup_dev_chip(d, virq + i, spec.hwirq + i);
		if (ret)
			goto error;
	}

	return 0;

error:
	irq_domain_free_irqs_parent(d, virq, nr_irqs);
	return ret;
}

void gic_dev_domain_free(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs)
{
	/* no real allocation is done for dev irqs, so no need to free anything */
	return;
}

static void gic_dev_domain_activate(struct irq_domain *domain,
				    struct irq_data *d)
{
	if (GIC_HWIRQ_TO_LOCAL(d->hwirq) < GIC_NUM_LOCAL_INTRS)
		gic_local_irq_domain_map(domain, d->irq, d->hwirq);
	else
		gic_shared_irq_domain_map(domain, d->irq, d->hwirq, 0);
}

static struct irq_domain_ops gic_dev_domain_ops = {
	.xlate = gic_dev_domain_xlate,
	.alloc = gic_dev_domain_alloc,
	.free = gic_dev_domain_free,
	.activate = gic_dev_domain_activate,
};

static int gic_ipi_domain_xlate(struct irq_domain *d, struct device_node *ctrlr,
				const u32 *intspec, unsigned int intsize,
				irq_hw_number_t *out_hwirq,
				unsigned int *out_type)
{
	/*
	 * There's nothing to translate here. hwirq is dynamically allocated and
	 * the irq type is always edge triggered.
	 */
	*out_hwirq = 0;
	*out_type = IRQ_TYPE_EDGE_RISING;

	return 0;
}

static int gic_ipi_domain_alloc(struct irq_domain *d, unsigned int virq,
				unsigned int nr_irqs, void *arg)
{
	struct cpumask *ipimask = arg;
	struct gic_irq_spec spec = {
		.type = GIC_IPI,
		.ipimask = ipimask
	};
	int ret, i;

	ret = irq_domain_alloc_irqs_parent(d, virq, nr_irqs, &spec);
	if (ret)
		return ret;

	/* the parent should have set spec.hwirq to the base_hwirq it allocated */
	for (i = 0; i < nr_irqs; i++) {
		ret = irq_domain_set_hwirq_and_chip(d, virq + i,
						    GIC_SHARED_TO_HWIRQ(spec.hwirq + i),
						    &gic_edge_irq_controller,
						    NULL);
		if (ret)
			goto error;

		ret = irq_set_irq_type(virq + i, IRQ_TYPE_EDGE_RISING);
		if (ret)
			goto error;
	}

	return 0;
error:
	irq_domain_free_irqs_parent(d, virq, nr_irqs);
	return ret;
}

void gic_ipi_domain_free(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs)
{
	irq_domain_free_irqs_parent(d, virq, nr_irqs);
}

int gic_ipi_domain_match(struct irq_domain *d, struct device_node *node,
			 enum irq_domain_bus_token bus_token)
{
	bool is_ipi;

	switch (bus_token) {
	case DOMAIN_BUS_IPI:
		is_ipi = d->bus_token == bus_token;
		return (!node || to_of_node(d->fwnode) == node) && is_ipi;
		break;
	default:
		return 0;
	}
}

static struct irq_domain_ops gic_ipi_domain_ops = {
	.xlate = gic_ipi_domain_xlate,
	.alloc = gic_ipi_domain_alloc,
	.free = gic_ipi_domain_free,
	.match = gic_ipi_domain_match,
};

static void __init gic_map_single_int(struct device_node *node,
				      unsigned int irq)
{
	unsigned int linux_irq;
	struct irq_fwspec local_int_fwspec = {
		.fwnode = &node->fwnode,
		.param_count = 3,
		.param = {
			[0] = GIC_LOCAL,
			[1] = irq,
			[2] = IRQ_TYPE_NONE,
		},
	};

	if (!gic_local_irq_is_routable(irq))
		return;

	linux_irq = irq_create_fwspec_mapping(&local_int_fwspec);
	WARN_ON(!linux_irq);
}

static void __init gic_map_interrupts(struct device_node *node)
{
	gic_map_single_int(node, GIC_LOCAL_INT_TIMER);
	gic_map_single_int(node, GIC_LOCAL_INT_PERFCTR);
}

static void __init __gic_init(unsigned long gic_base_addr,
			      unsigned long gic_addrspace_size,
			      unsigned int cpu_vec, unsigned int irqbase,
			      struct device_node *node)
{
	unsigned int gicconfig, cpu;
	unsigned int v[2];

	__gic_base_addr = gic_base_addr;

	gic_base = ioremap_nocache(gic_base_addr, gic_addrspace_size);

	gicconfig = gic_read(GIC_REG(SHARED, GIC_SH_CONFIG));
	gic_shared_intrs = (gicconfig & GIC_SH_CONFIG_NUMINTRS_MSK) >>
		   GIC_SH_CONFIG_NUMINTRS_SHF;
	gic_shared_intrs = ((gic_shared_intrs + 1) * 8);

	gic_vpes = (gicconfig & GIC_SH_CONFIG_NUMVPES_MSK) >>
		  GIC_SH_CONFIG_NUMVPES_SHF;
	gic_vpes = gic_vpes + 1;

	if (cpu_has_veic) {
		/* Set EIC mode for all VPEs */
		for_each_present_cpu(cpu) {
			gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR),
				  mips_cm_vp_id(cpu));
			gic_write(GIC_REG(VPE_OTHER, GIC_VPE_CTL),
				  GIC_VPE_CTL_EIC_MODE_MSK);
		}

		/* Always use vector 1 in EIC mode */
		gic_cpu_pin = 0;
		timer_cpu_pin = gic_cpu_pin;
		set_vi_handler(gic_cpu_pin + GIC_PIN_TO_VEC_OFFSET,
			       __gic_irq_dispatch);
	} else {
		if (cpu_has_vint) {
			/* install generic handler */
			/* 2 - Normal
			 * 3 - Normal
			 * 4 - Normal
			 * 5 - Normal
			 * 6 - Normal
			 * 7 - Local
			 */
			set_vi_handler(2, gic_shared_irq_vi2_dispatch);
			set_vi_handler(3, gic_shared_irq_vi3_dispatch);
			set_vi_handler(4, gic_shared_irq_vi4_dispatch);
			set_vi_handler(5, gic_shared_irq_vi5_dispatch);
			set_vi_handler(6, gic_shared_irq_vi6_dispatch);
			set_vi_handler(7, gic_local_irq_dispatch);
			gic_cpu_pin = 7 - GIC_CPU_PIN_OFFSET;
			timer_cpu_pin = gic_cpu_pin;
			gic_local_int_pin_init(GIC_LOCAL_INT_WD);
			gic_local_int_pin_init(GIC_LOCAL_INT_COMPARE);
			gic_local_int_pin_init(GIC_LOCAL_INT_PERFCTR);
			change_c0_status(ST0_IM, STATUSF_IP2 | STATUSF_IP3 | STATUSF_IP4 | STATUSF_IP5 |
							 STATUSF_IP6 | STATUSF_IP7);
			back_to_back_c0_hazard();
		} else {
			gic_cpu_pin = cpu_vec - GIC_CPU_PIN_OFFSET;
			irq_set_chained_handler(MIPS_CPU_IRQ_BASE + cpu_vec,
						gic_irq_dispatch);
			/*
			 * With the CMP implementation of SMP (deprecated),
			 * other CPUs are started by the bootloader and put
			 * into a timer based waiting poll loop. We must not
			 * re-route those CPU's local timer interrupts as the
			 * wait instruction will never finish, so just handle
			 * whatever CPU interrupt it is routed to by default.
			 *
			 * This workaround should be removed when CMP support
			 * is dropped.
			 */
			if (IS_ENABLED(CONFIG_MIPS_CMP) &&
			    gic_local_irq_is_routable(GIC_LOCAL_INT_TIMER)) {
				timer_cpu_pin = gic_read32(GIC_REG(VPE_LOCAL,
								   GIC_VPE_TIMER_MAP)) &
						GIC_MAP_MSK;
				irq_set_chained_handler(MIPS_CPU_IRQ_BASE +
								GIC_CPU_PIN_OFFSET +
								timer_cpu_pin,
							gic_irq_dispatch);
			} else {
				timer_cpu_pin = gic_cpu_pin;
			}
		}
	}

	gic_irq_domain = irq_domain_add_simple(node, GIC_NUM_LOCAL_INTRS +
					       gic_shared_intrs, irqbase,
					       &gic_irq_domain_ops, NULL);
	if (!gic_irq_domain)
		panic("Failed to add GIC IRQ domain");
	gic_irq_domain->name = "mips-gic-irq";

	gic_dev_domain = irq_domain_add_hierarchy(gic_irq_domain, 0,
						  GIC_NUM_LOCAL_INTRS + gic_shared_intrs,
						  node, &gic_dev_domain_ops, NULL);
	if (!gic_dev_domain)
		panic("Failed to add GIC DEV domain");
	gic_dev_domain->name = "mips-gic-dev";

	gic_ipi_domain = irq_domain_add_hierarchy(gic_irq_domain,
						  IRQ_DOMAIN_FLAG_IPI_PER_CPU,
						  GIC_NUM_LOCAL_INTRS + gic_shared_intrs,
						  node, &gic_ipi_domain_ops, NULL);
	if (!gic_ipi_domain)
		panic("Failed to add GIC IPI domain");

	gic_ipi_domain->name = "mips-gic-ipi";
	gic_ipi_domain->bus_token = DOMAIN_BUS_IPI;

	if (node &&
	    !of_property_read_u32_array(node, "mti,reserved-ipi-vectors", v, 2)) {
		bitmap_set(ipi_resrv, v[0], v[1]);
	} else {
		/* Make the last 2 * gic_vpes available for IPIs */
		bitmap_set(ipi_resrv,
			   gic_shared_intrs - 2 * gic_vpes,
			   2 * gic_vpes);
	}

	bitmap_copy(ipi_available, ipi_resrv, GIC_MAX_INTRS);
	if (node) {
		int t = 0;

		t = of_property_count_u32_elems(node, "mti,reserved-list");
		if (t < 0 || t > GIC_MAX_INTRS)
			gic_reserved_list_count = 0;
		else
			gic_reserved_list_count = t;
	}

	if (node &&
	    gic_reserved_list_count &&
	    of_property_read_u32_array(node, "mti,reserved-list",
				       gic_reserved_list,
				       gic_reserved_list_count))
		gic_reserved_list_count = 0;

	gic_basic_init();
	gic_map_interrupts(node);
}

void __init gic_init(unsigned long gic_base_addr,
		     unsigned long gic_addrspace_size,
		     unsigned int cpu_vec, unsigned int irqbase)
{
	__gic_init(gic_base_addr, gic_addrspace_size, cpu_vec, irqbase, NULL);
}

int gic_yield_setup(unsigned int cpu, unsigned int pin, unsigned int irq)
{
	int cpux, vpe;
	unsigned long flags;
	irq_hw_number_t hwirq;
	struct irq_data *d = irq_get_irq_data(irq);

	if (unlikely(!d))
		return -EINVAL;
	/* Sanity check */
	if ((cpu >= nr_cpu_ids) || (pin > 0xF))
		return -EINVAL;
	hwirq = irqd_to_hwirq(d);
	vpe   = mips_cm_vp_id(cpu);

	if (hwirq >= GIC_SHARED_HWIRQ_BASE) {
		hwirq = GIC_HWIRQ_TO_SHARED(hwirq);
		spin_lock_irqsave(&gic_lock, flags);
		gic_yield_map_to_pin(hwirq, pin);
		gic_map_to_vpe(hwirq, vpe);
		/* Clear all yield related percpu mask */
		for_each_possible_cpu(cpux)
			clear_bit(hwirq, pcpu_masks[cpux].pcpu_mask);
		spin_unlock_irqrestore(&gic_lock, flags);
	} else {
		hwirq = GIC_HWIRQ_TO_LOCAL(hwirq);
		gic_local_irq_yield_map_to_pin(vpe, hwirq, pin);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(gic_yield_setup);

int gic_nmi_setup(unsigned int cpu, unsigned int irq)
{
	int cpux, vpe;
	unsigned long flags;
	irq_hw_number_t hwirq;
	struct irq_data *d = irq_get_irq_data(irq);

	if (unlikely(!d))
		return -EINVAL;

	/* Sanity check */
	if (cpu >= nr_cpu_ids)
		return -EINVAL;
	hwirq = irqd_to_hwirq(d);
	vpe   = mips_cm_vp_id(cpu);

	if (hwirq >= GIC_SHARED_HWIRQ_BASE) {
		hwirq = GIC_HWIRQ_TO_SHARED(hwirq);
		spin_lock_irqsave(&gic_lock, flags);
		gic_map_to_nmi(hwirq);
		gic_map_to_vpe(hwirq, vpe);
		/* Clear all nmi related percpu mask */
		for_each_possible_cpu(cpux)
			clear_bit(hwirq, pcpu_masks[cpux].pcpu_mask);
		set_bit(hwirq, pcpu_masks[cpu].pcpu_mask);
		spin_unlock_irqrestore(&gic_lock, flags);
	} else {
		hwirq = GIC_HWIRQ_TO_LOCAL(hwirq);
		gic_local_irq_map_to_nmi(vpe, hwirq);
	}
	return 0;
}

/**
 * cpu:  linux-cpu to bind
 * irq   (linux-)irqnmb
 * mode: 0 irq
 *       1 nmi
 *       2 yield
 * pin:  if mode==2 (yield): signal (0-15)
 *       if mode==0 (irq):   non-eic: ip0-ip5
 */
int gic_map_setup(unsigned int cpu, unsigned int irq, unsigned int mode, unsigned int pin)
{
	int ret;
	struct irq_data *d = irq_get_irq_data(irq);
	int hw_irq;

	switch (mode) {
#if 0
	case 0: /* IRQ */
		return gic_irq_setup(cpu, irq, pin);
#endif
	case 1: /* NMI */
		ret = gic_nmi_setup(cpu, irq);
		break;
	case 2: /* yield */
		ret = gic_yield_setup(cpu, pin, irq);
		break;
	default:
		pr_err("%s not supported mode=%u\n", __func__, mode);
		return -EINVAL;
	}
	if (ret)
		return ret;
	hw_irq = irqd_to_hwirq(d);
	if (hw_irq >= GIC_SHARED_HWIRQ_BASE)
		gic_unmask_irq(d);
	else
		gic_unmask_local_irq_vpeother(mips_cm_vp_id(cpu), GIC_HWIRQ_TO_LOCAL(hw_irq));
	return 0;
}
EXPORT_SYMBOL(gic_map_setup);

int gic_map_irq_type(unsigned int irq, unsigned int type)
{
	struct irq_data *d = irq_get_irq_data(irq);

	if (!d) {
		return -EINVAL;
	}
	return gic_set_type(d, type);
}
EXPORT_SYMBOL(gic_map_irq_type);

unsigned long gic_read_reg(unsigned int reg)
{
	return gic_read(reg);
}
EXPORT_SYMBOL_GPL(gic_read_reg);

void gic_write_reg(unsigned int reg, unsigned long val)
{
	return gic_write(reg, val);
}
EXPORT_SYMBOL_GPL(gic_write_reg);

static int __init gic_of_init(struct device_node *node,
			      struct device_node *parent)
{
	struct resource res;
	unsigned int cpu_vec, i = 0, reserved = 0;
	phys_addr_t gic_base;
	size_t gic_len;

	/* Find the first available CPU vector. */
	while (!of_property_read_u32_index(node, "mti,reserved-cpu-vectors",
					   i++, &cpu_vec))
		reserved |= BIT(cpu_vec);
	for (cpu_vec = 2; cpu_vec < 8; cpu_vec++) {
		if (!(reserved & BIT(cpu_vec)))
			break;
	}
	if (cpu_vec == 8) {
		pr_err("No CPU vectors available for GIC\n");
		return -ENODEV;
	}

	if (of_address_to_resource(node, 0, &res)) {
		/*
		 * Probe the CM for the GIC base address if not specified
		 * in the device-tree.
		 */
		if (mips_cm_present()) {
			gic_base = read_gcr_gic_base() &
				~CM_GCR_GIC_BASE_GICEN_MSK;
			gic_len = 0x20000;
		} else {
			pr_err("Failed to get GIC memory range\n");
			return -ENODEV;
		}
	} else {
		gic_base = res.start;
		gic_len = resource_size(&res);
	}

	if (mips_cm_present()) {
		write_gcr_gic_base(gic_base | CM_GCR_GIC_BASE_GICEN_MSK);
		/* Ensure GIC region is enabled before trying to access it */
		__sync();
	}
	gic_present = true;

	__gic_init(gic_base, gic_len, cpu_vec, 0, node);

	return 0;
}
IRQCHIP_DECLARE(mips_gic, "mti,gic", gic_of_init);

#if defined(CONFIG_AVM_ENHANCED)
/**
 */
char *name_of_local_irq(int local_irq)
{
	switch (local_irq) {
	case GIC_LOCAL_INT_WD:
		return "GIC_LOCAL_INT_WD     ";	/* GIC watchdog */
	case GIC_LOCAL_INT_COMPARE:
		return "GIC_LOCAL_INT_COMPARE";	/* GIC count and compare timer */
	case GIC_LOCAL_INT_TIMER:
		return "GIC_LOCAL_INT_TIMER  ";	/* CPU CP0 timer interrupt */
	case GIC_LOCAL_INT_PERFCTR:
		return "GIC_LOCAL_INT_PERFCTR";	/* CPU performance counter */
	case GIC_LOCAL_INT_SWINT0:
		return "GIC_LOCAL_INT_SWINT0 ";	/* CPU software interrupt 0 */
	case GIC_LOCAL_INT_SWINT1:
		return "GIC_LOCAL_INT_SWINT1 ";	/* CPU software interrupt 1 */
	case GIC_LOCAL_INT_FDC:
		return "GIC_LOCAL_INT_FDC    ";	/* CPU fast debug channel */
	}
	return "GIC_LOCAL_?";
}

/**
 * attention! set GIC_VPE_OTHER_ADDR before
 */
static inline unsigned int gic_local_irq_read_map_reg(unsigned int intr)
{
	unsigned int reg;

	reg = gic_local_irq_map_reg(intr);
	if (reg == (unsigned int)(-EINVAL)) {
		return 0;
	}
	return gic_read32(reg);
}

/**
 */
static char *print_desc_irq_info(char *text, unsigned int textlen, unsigned int irq)
{
	struct irq_desc *desc;
	char domain[10];
	unsigned int any_count = 0;
	int cpu;

	desc = irq_to_desc(irq);
	if (desc && desc->action) {
		for_each_online_cpu(cpu) {
			any_count += kstat_irqs_cpu(irq, cpu);
		}
		if (desc->irq_data.domain)
			snprintf(domain, sizeof(domain), " %-3d", (int) desc->irq_data.hwirq);
		else {
			domain[0] = 0;
		}
		snprintf(text, textlen, " %-12s%s %-16s %-16s %10u",
			 desc->irq_data.chip && desc->irq_data.chip->name ? desc->irq_data.chip->name : "-",
			 domain,
			 desc->name ? : "",
			 desc->action->name ? : "",
			 any_count);
	} else {
		text[0] = 0;
	}
	return text;
}
/**
 */
static void show_local_irq_status(struct seq_file *seq)
{
	unsigned int virq, local_irq, cpu, pin_map[NR_CPUS][GIC_NUM_LOCAL_INTRS], pending[NR_CPUS], mask[NR_CPUS];
	char irq_info[128];
	unsigned long flags;

	seq_puts(seq, "local interrupts:\n");
	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		spin_lock_irqsave(&gic_lock, flags);
		gic_write(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), cpu);
		for (local_irq = 0; local_irq < GIC_NUM_LOCAL_INTRS; local_irq++) {
			pin_map[cpu][local_irq] = gic_local_irq_read_map_reg(local_irq);
		}
		pending[cpu] = gic_read(GIC_REG(VPE_OTHER, GIC_VPE_PEND));
		mask[cpu]    = gic_read(GIC_REG(VPE_OTHER, GIC_VPE_MASK));
		spin_unlock_irqrestore(&gic_lock, flags);
	}
	for (local_irq = 0; local_irq < GIC_NUM_LOCAL_INTRS; local_irq++) {
		virq = irq_linear_revmap(gic_irq_domain, GIC_LOCAL_TO_HWIRQ(local_irq));

		seq_printf(seq, "%u(%3u) %s", local_irq, virq, name_of_local_irq(local_irq));
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			seq_printf(seq, " CPU%x: %s%s%s(0x%x) %s %s", cpu,
				   pin_map[cpu][local_irq] & GIC_MAP_TO_PIN_MSK ? "IRQ   " : "",
				   pin_map[cpu][local_irq] & GIC_MAP_TO_YQ_MSK  ? "YIELD " : "",
				   pin_map[cpu][local_irq] & GIC_MAP_TO_NMI_MSK ? "NMI   " : "",
				   pin_map[cpu][local_irq] & GIC_MAP_MSK,
				   (mask[cpu] >> local_irq) & 1    ? "EN " : "DIS",
				   (pending[cpu] >> local_irq) & 1 ? "PEND" : "    ");
		}
		seq_printf(seq, " %s\n", print_desc_irq_info(irq_info, sizeof(irq_info), virq));

	}
}

/**
 */
static unsigned int is_irq_pending(unsigned int intr)
{
	unsigned int pending;

	pending = gic_read(GIC_REG(SHARED, GIC_SH_PEND) + GIC_INTR_OFS(intr));
	return (pending & (1 << GIC_INTR_BIT(intr))) ? 1 : 0;
}

/**
 */
static inline unsigned int is_irq_enabled(unsigned int intr)
{
	unsigned int mask;

	mask = gic_read(GIC_REG(SHARED, GIC_SH_MASK) + GIC_INTR_OFS(intr));
	return (mask & (1 << GIC_INTR_BIT(intr))) ? 1 : 0;
}

/**
 * intr: shared interrupt base (begin on zero)
 */
static void show_irq_status(unsigned int intr, struct seq_file *seq)
{
	unsigned int pin_map, cpux, val, polarity, trigger, dual_en, virq;
	char cpumap[NR_CPUS + 1];
	char irq_info[128];

	memset(cpumap, 0, sizeof(cpumap));

	pin_map = gic_read32(GIC_REG(SHARED, GIC_SH_INTR_MAP_TO_PIN_BASE) + GIC_SH_MAP_TO_PIN(intr));

	for_each_possible_cpu(cpux) {
		val = gic_read(GIC_REG(SHARED, GIC_SH_INTR_MAP_TO_VPE_BASE) + GIC_SH_MAP_TO_VPE_REG_OFF(intr, cpux));
		cpumap[cpux] = val & GIC_SH_MAP_TO_VPE_REG_BIT(cpux) ? '0' + cpux : ' ';
	}
	polarity = gic_read(GIC_REG(SHARED, GIC_SH_SET_POLARITY) + GIC_INTR_OFS(intr));
	trigger  = gic_read(GIC_REG(SHARED, GIC_SH_SET_TRIGGER) + GIC_INTR_OFS(intr));
	dual_en  = gic_read(GIC_REG(SHARED, GIC_SH_SET_DUAL) + GIC_INTR_OFS(intr));
	virq	 = irq_linear_revmap(gic_irq_domain, GIC_SHARED_TO_HWIRQ(intr));

	seq_printf(seq, "%3u(%3u) %s %s%s%s(0x%x) %s %s %s %s %s%s\n", intr, virq,
		   cpumap,
		   pin_map & GIC_MAP_TO_PIN_MSK ? "IRQ  " : "",
		   pin_map & GIC_MAP_TO_YQ_MSK  ? "YIELD" : "",
		   pin_map & GIC_MAP_TO_NMI_MSK ? "NMI  " : "",
		   pin_map & GIC_MAP_MSK,
		   ((polarity >> GIC_INTR_BIT(intr)) & 1) == GIC_POL_POS          ? "POS"   : "NEG",
		   ((trigger >> GIC_INTR_BIT(intr)) & 1)  == GIC_TRIG_EDGE        ? "EDGE " : "LEVEL",
		   ((dual_en >> GIC_INTR_BIT(intr)) & 1)  == GIC_TRIG_DUAL_ENABLE ? "DUAL"  : "    ",
		   is_irq_enabled(intr) ? "ENABLED " : "DISABLED",
		   is_irq_pending(intr) ? " PEND"    : "     ",
		   (pin_map & GIC_MAP_TO_PIN_MSK) ? print_desc_irq_info(irq_info, sizeof(irq_info), virq) : ""
		   );
}

/**
 * ret: negval -> not found/no range/no match
 */
static int generic_irq_param_parse(char *string, char *match, int maxval, char *matchstrg1, char *matchstrg2,
				   char *matchstrg3)
{
	char *p = string;
	int ret = -1;

	p = strstr(string, match);
	if (p &&
	    ((p == string) || isspace(*(p - 1)))) {
		p += strlen(match);
		while (*p == ' ' || *p == '\t')
			p++;
		if (matchstrg1 && strncmp(p, matchstrg1, strlen(matchstrg1)) == 0) {
			ret = 0;
		} else if (matchstrg2 && strncmp(p, matchstrg2, strlen(matchstrg2)) == 0) {
			ret = 1;
		} else if (matchstrg3 && strncmp(p, matchstrg3, strlen(matchstrg3)) == 0) {
			ret = 2;
		} else if (*p) {
			sscanf(p, "%d", &ret);
			if (ret > maxval) {
				ret = -1;
			}
		}
	}
	return ret;
}

/**
 */
static int grx_irq_set(char *string, void *priv __maybe_unused)
{
	int trigger, mask, pol, dual_en, pinmap, signal, cpu, wedge;
	int local_intr, shared_intr = -1;
	unsigned long flags;
	char buf[768];
	struct seq_file seq;
	int vpe;

	local_intr = generic_irq_param_parse(string, "local_intr=", GIC_NUM_LOCAL_INTRS, NULL, NULL, NULL);
	if (local_intr < 0)
		shared_intr = generic_irq_param_parse(string, "intr=", gic_shared_intrs, NULL, NULL, NULL);

	trigger = generic_irq_param_parse(string, "trigger=", 1, "edge", "level", NULL);
	pol     = generic_irq_param_parse(string, "pol=", 1, "neg", "pos", NULL);
	wedge   = generic_irq_param_parse(string, "wedge=", 1, NULL, NULL, NULL);
	dual_en = generic_irq_param_parse(string, "dual=", 1, NULL, NULL, NULL);
	mask    = generic_irq_param_parse(string, "mask=", 1, NULL, NULL, NULL);
	pinmap  = generic_irq_param_parse(string, "pinmap=", 2, "irq", "nmi", "yield");
	signal  = generic_irq_param_parse(string, "signal=", 15, NULL, NULL, NULL);
	cpu     = generic_irq_param_parse(string, "cpu=", num_possible_cpus() - 1, NULL, NULL, NULL);
	if (((local_intr < 0) && (shared_intr < 0)) || (strstr(string, "help"))) {
		pr_err("use: intr=<val> or local_intr=<val> trigger=<edge|level> mask=<0|1> dual=<0|1> pol=<neg|pos> pinmap=<irq|nmi|yield> cpu=<nr> wedge=<0|1>\n");
		return 0;
	}
	if (cpu >= 0) {
		vpe = mips_cm_vp_id(cpu);
	} else {
		vpe = mips_cm_vp_id(smp_processor_id());
	}
	if (shared_intr >= 0) {
		spin_lock_irqsave(&gic_lock, flags);
		if (trigger >= 0) {
			gic_set_trigger(shared_intr, trigger);
		}
		if (pol >= 0) {
			gic_set_polarity(shared_intr, pol);
		}
		if (dual_en >= 0) {
			gic_set_dual_edge(shared_intr, dual_en);
		}
		spin_unlock_irqrestore(&gic_lock, flags);
	}
	if (mask >= 0) {
		if (shared_intr >= 0) {
			if (mask)
				gic_set_mask(shared_intr);
			else
				gic_reset_mask(shared_intr);
		} else {
			if (mask) {
				gic_unmask_local_irq_vpeother(vpe, local_intr);
			} else {
				gic_mask_local_irq_vpeother(vpe, local_intr);
			}
		}
	}
	if (pinmap >= 0) {
		pinmap = (pinmap == 0) ? GIC_MAP_TO_PIN_MSK :
			 (pinmap == 1) ? GIC_MAP_TO_NMI_MSK : GIC_MAP_TO_YQ_MSK;
		if (signal < 0) {
			signal = 0;
		}
		if (shared_intr >= 0) {
			switch (pinmap) {
			case GIC_MAP_TO_PIN_MSK:
				if (cpu_has_vint)
					gic_map_to_pin(shared_intr, gic_irq_to_pin(shared_intr));
				else
					gic_map_to_pin(shared_intr, gic_cpu_pin);
				break;
			case GIC_MAP_TO_NMI_MSK:
				gic_map_to_nmi(shared_intr);
				break;
			case GIC_MAP_TO_YQ_MSK:
				gic_yield_map_to_pin(shared_intr, pinmap | signal);
				break;
			}

		} else {
			gic_local_irq_write_map_reg(vpe, local_intr, pinmap | signal);
		}
	}
	if ((wedge >= 0) && (shared_intr >= 0)) {
		spin_lock_irqsave(&gic_lock, flags);
		if ( cpu >= 0)
			gic_map_to_vpe(shared_intr, vpe);
		gic_write(GIC_REG(SHARED, GIC_SH_WEDGE), shared_intr | ((wedge) << 31));
		spin_unlock_irqrestore(&gic_lock, flags);
	}
	memset(&seq, 0, sizeof(seq));
	buf[0] = 0;
	seq.buf = buf;
	seq.size = sizeof(buf);
	if (shared_intr >= 0) {
		show_irq_status(shared_intr, &seq);
	} else {
		show_local_irq_status(&seq);
	}
	pr_err("\n%s\n", seq.buf);
	return 0;
}

/**
 */
static void grx_irq_list(struct seq_file *seq, void *priv)
{
	int shared_intr, cpu;

	show_local_irq_status(seq);
	seq_puts(seq, "shared interrupts:\n");
	for (shared_intr = 0; shared_intr < gic_shared_intrs; shared_intr++) {
		show_irq_status(shared_intr, seq);
	}
	seq_puts(seq, "pcpu_mask\n");
	for_each_possible_cpu(cpu) {
		char text[512];

		scnprintf(text, sizeof(text), "%*pb", NR_IRQS, pcpu_masks[cpu].pcpu_mask);
		seq_printf(seq, "cpu%d: %s\n", cpu, text);
	}
}

static struct proc_dir_entry *irqprocdir;

/**
 */
static int __init avm_irqproc_init(void)
{
#define PROC_GPIODIR   "avm/irq"
	irqprocdir = proc_mkdir(PROC_GPIODIR, NULL);
	if (irqprocdir == NULL) {
		return 0;
	}
	add_simple_proc_file("avm/irq/list", NULL, grx_irq_list, NULL);
	add_simple_proc_file("avm/irq/set", grx_irq_set, NULL, NULL);
	return 0;
}

late_initcall(avm_irqproc_init);

/**
 */
static int nmi_timer_notify(struct notifier_block *self, unsigned long dummy, void *param);

/**
 */
static struct notifier_block nmi_timer_nb = {
	.notifier_call = nmi_timer_notify,
	.priority = INT_MAX - 1,
};

/**
 */
static void dump_irq_status(void)
{
	int shared_intr, virq, cpu;
	struct seq_file seq;
	char buf[256];

	pr_err("shared interrupts:\n");
	for (shared_intr = 0; shared_intr < gic_shared_intrs; shared_intr++) {
		int any_count = 0;

		virq = irq_linear_revmap(gic_irq_domain, GIC_SHARED_TO_HWIRQ(shared_intr));

		for_each_online_cpu(cpu) {
			any_count += kstat_irqs_cpu(virq, cpu);
		}
		if (!any_count) {
			continue;
		}
		memset(&seq, 0, sizeof(seq));
		seq.buf = buf;
		buf[0] = 0;
		seq.size = sizeof(buf);
		show_irq_status(shared_intr, &seq);
		pr_err("%s", seq.buf);
	}
}

/**
 */
static int nmi_timer_notify(struct notifier_block *self __maybe_unused, unsigned long dummy __maybe_unused,
			    void *param __maybe_unused)
{
	dump_irq_status();
	return NOTIFY_OK;
}

/**
 */
static int __init small_irq_statistic_init(void)
{
	/*--- pr_err("%s: init\n", __func__); ---*/
	register_nmi_notifier(&nmi_timer_nb);
	return 0;
}

late_initcall(small_irq_statistic_init);
#endif/*--- #if defined(CONFIG_AVM_ENHANCED) ---*/
