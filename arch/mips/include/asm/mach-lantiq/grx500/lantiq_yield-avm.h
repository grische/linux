#ifndef __lantiq_yield_avm_h__
#define __lantiq_yield_avm_h__

#if defined(CONFIG_SOC_GRX500)

#include <linux/irqchip/mips-gic.h>
#include <asm/mach-lantiq/grx500/irq.h>

#define YIELD_ID(cpu, tc, yield_signal)                                   \
	((((cpu)&0xF) << 24) | (((tc)&0xF) << 16)|     \
	 ((yield_signal)&0xF))
#define YIELD_CPU_BY_ID(id) ((id) >> 24)
#define YIELD_TC_BY_ID(id) (((id) >> 16) & 0xF)
#define YIELD_SIGNAL_BY_ID(id) ((id)&0xF)
#define YIELD_SIGNALMASK_BY_ID(id) (1 << ((id)&0xF))

/*--------------------------------------------------------------------------------*\
 * (first) Yield-to-Linux-IPI-IRQ
\*--------------------------------------------------------------------------------*/
#define GRX_AVM_IPI_YIELD_IRQ_CPU(cpu) (0 + (cpu) + MIPS_GIC_IRQ_BASE)

/*--------------------------------------------------------------------------------*\
 * needed for gic_map_setup()
\*--------------------------------------------------------------------------------*/
#define GRX_PCM_TXIR (GIC_SHARED_TO_HWIRQ(51) + MIPS_GIC_IRQ_BASE)
#define GRX_PCM_RXIR (GIC_SHARED_TO_HWIRQ(52) + MIPS_GIC_IRQ_BASE)

#define GRX_WDT_IRQ                                                            \
	(MIPS_GIC_IRQ_BASE + GIC_LOCAL_TO_HWIRQ(GIC_LOCAL_INT_WD))
#define YIELD_MONITOR_MAX_CORES 2
#define YIELD_MONITOR_MAX_TC 6
#define YIELD_MONITOR_IPI_ID(core)                                             \
	YIELD_ID(core ? 2 : 0, 2, 0xe)

#define GRX_COMPARE_IRQ                                                        \
	(MIPS_GIC_IRQ_BASE + GIC_LOCAL_TO_HWIRQ(GIC_LOCAL_INT_COMPARE))
#define YIELD_PROFILE_IPI_ID(core)                                             \
	YIELD_ID(core ? 2 : 0, 2, /*GRX_COMPARE_IRQ,*/ 0xd)
/*--------------------------------------------------------------------------------*\
 * mbahr@avm.de
 * valid yield signals:
 * signal is wellknown choosen ... 0x0 - 0xf really assignment follows with gic_yield_setup()
 * to activate please add enum in yield-config-table arch/mips/avm_enh/avm_yieldconfig.c
 * restrictions: no overlappig of signal-number on the same cpu (ergo vpe)
\*--------------------------------------------------------------------------------*/
enum _yield_signal_id {
	YIELD_PCMFRAME_ID = YIELD_ID(0, 4, /*GRX_PCM_RXIR,*/
				     0xf), /*--- TDM-Frame (125 us period) ---*/
	YIELD_PCMLINK_ID = YIELD_ID(0, 3, /*GRX_PCM_TXIR,*/
				    0xc), /*--- PCMLINK (4 ms period) ----*/
	YIELD_MONITOR_IPI0_ID = YIELD_MONITOR_IPI_ID(0),
	YIELD_MONITOR_IPI1_ID = YIELD_MONITOR_IPI_ID(1),
	YIELD_PROFILE_IPI0_ID = YIELD_PROFILE_IPI_ID(0),
	YIELD_PROFILE_IPI1_ID = YIELD_PROFILE_IPI_ID(1),
	YIELD_GPTC_ID         = YIELD_ID(0, 5, 0xb),
	YIELD_INVAL_ID = YIELD_ID(NR_CPUS, 0, 0),
};
/*--------------------------------------------------------------------------------*\
 * cpu:  cpu to bind
 * irq   (linux-)irqnmb
 * mode: 0 irq
 *       1 nmi
 *       2 yield
 * pin:  if mode==2 (yield): signal (0-15)
 *       if mode==0 (irq):   non-eic: ip0-ip5
\*--------------------------------------------------------------------------------*/
#define GIC_IRQMODE_IRQ   0
#define GIC_IRQMODE_NMI   1
#define GIC_IRQMODE_YIELD 2
extern int gic_map_setup(unsigned int cpu, unsigned int irq, unsigned int mode,
			 unsigned int pin);

/*--------------------------------------------------------------------------------*\
 * trigger works only if irq configured in edge-mode
 * set: 0: clear else set
\*--------------------------------------------------------------------------------*/
extern int gic_trigger_irq(unsigned int intr, unsigned int set);

/*--------------------------------------------------------------------------------*\
 * type:    IRQ_TYPE_EDGE_FALLING
 *          IRQ_TYPE_EDGE_RISING
 *          IRQ_TYPE_EDGE_BOTH
 *          IRQ_TYPE_LEVEL_LOW
 *          IRQ_TYPE_LEVEL_HIGH
\*--------------------------------------------------------------------------------*/
extern int gic_map_irq_type(unsigned int irq, unsigned int type);

/*--------------------------------------------------------------------------------*\
 * IRQ/YIELD/NMI anschalten
 * nicht aus Irq-Context verwenden (aber Yield erlaubt)
\*--------------------------------------------------------------------------------*/
extern void gic_enable_mask_for_cpu(int cpu, int irq);

/*--------------------------------------------------------------------------------*\
 * IRQ/YIELD/NMI ausschalten
 * nicht aus Irq-Context verwenden (aber Yield erlaubt)
\*--------------------------------------------------------------------------------*/
extern void gic_disable_mask_for_cpu(int cpu, int irq);

/*--------------------------------------------------------------------------------*\
 * Write-Compare fuer spez. CPU schreiben
 * nicht aus Irq-Context verwenden (aber Yield erlaubt)
\*--------------------------------------------------------------------------------*/
void gic_write_compare_for_cpu(int cpu, cycle_t cnt);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
extern void avm_yield_setup(void);

extern int yield_hw_to_virtirq(int hwirq);

#endif /*--- #if defined(CONFIG_AVM_ENHANCED) ---*/

#endif /*--- #ifndef __lantiq_yield_avm_h__ ---*/
