/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __arch_profile_grx_h__
#define __arch_profile_grx_h__

#include <asm/time.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
#include <asm/gic.h>
#else
#include <linux/irqchip/mips-gic.h>
#endif
#include <asm/yield_context.h>
#include <asm/mach_avm.h>
#include <lantiq.h>
#include <linux/avm_hw_config.h>
#include <asm/ltq_gic_timer.h>

#if !defined(CONFIG_CEVT_GIC)
#define PROFILING_IN_YIELD
#endif /*--- #if defined(CONFIG_CEVT_GIC) ---*/

#define PROFILING_CORES 2
#define PROFILING_MAX_COUNT_TCS 6

#define PROFILING_CPU_HAS_TC
#define PROFILING_MAX_PERF_REGISTER 2 /*--- 2 x Perf-counter ---*/

#if defined(ARCH_MIPS_PROFILE_C)
static struct _cpucore_profile mips_cpu_config[2] = {
	/*--- linux_os_mask wird dynamisch gefuellt s.u. ---*/
	{ .cpu_nr_offset = 0,
	  .vpe_nr = 2,
	  .linux_os_mask = 0,
	  .next_core = &mips_cpu_config[1] },
	{ .cpu_nr_offset = 2,
	  .vpe_nr = 2,
	  .linux_os_mask = 0,
	  .next_core = NULL },
};

#define PROFILING_USEC_TO_TIMER(usec) ((usec)*125)
#define PROFILING_TRIGGER_SHIFT       19
#define PROFILING_TRIGGER_MASK                                                 \
	((1 << PROFILING_TRIGGER_SHIFT) -                                      \
	 1) /*--- Range: shift=19: / 600 = 873 us ---*/

/*--- GPIO fuer Uart missbrauchen: ---*/
#define GRX_GPIO_OUT_SET_OFFSET       0x40
#define GRX_GPIO_OUT_CLEAR_OFFSET     0x44

#include <lantiq_soc.h>

#define ASC_UART1_BASE                                                         \
	((unsigned int *)KSEG1ADDR(0x16700000)) /*--- ASC-UART1 ---*/
// clang-format off
typedef struct {        /* ifx_asc_reg_t */
	volatile unsigned int  asc_clc;                            /*0x0000*/
	volatile unsigned int  asc_pisel;                          /*0x0004*/
	volatile unsigned int  asc_id;                             /*0x0008*/
	volatile unsigned int  asc_rsvd1[1];   /* for mapping */   /*0x000C*/
	volatile unsigned int  asc_mcon;                           /*0x0010*/
	volatile unsigned int  asc_state;                          /*0x0014*/
	volatile unsigned int  asc_whbstate;                       /*0x0018*/
	volatile unsigned int  asc_rsvd2[1];   /* for mapping */   /*0x001C*/
	volatile unsigned int  asc_tbuf;                           /*0x0020*/
	volatile unsigned int  asc_rbuf;                           /*0x0024*/
	volatile unsigned int  asc_rsvd3[2];   /* for mapping */   /*0x0028*/
	volatile unsigned int  asc_abcon;                          /*0x0030*/
	volatile unsigned int  asc_abstat;     /* not used */      /*0x0034*/
	volatile unsigned int  asc_whbabcon;                       /*0x0038*/
	volatile unsigned int  asc_whbabstat;  /* not used */      /*0x003C*/
	volatile unsigned int  asc_rxfcon;                         /*0x0040*/
	volatile unsigned int  asc_txfcon;                         /*0x0044*/
	volatile unsigned int  asc_fstat;                          /*0x0048*/
	volatile unsigned int  asc_rsvd4[1];   /* for mapping */   /*0x004C*/
	volatile unsigned int  asc_bg;                             /*0x0050*/
	volatile unsigned int  asc_bg_timer;                       /*0x0054*/
	volatile unsigned int  asc_fdv;                            /*0x0058*/
	volatile unsigned int  asc_pmw;                            /*0x005C*/
	volatile unsigned int  asc_modcon;                         /*0x0060*/
	volatile unsigned int  asc_modstat;                        /*0x0064*/
	volatile unsigned int  asc_rsvd5[2];   /* for mapping */   /*0x0068*/
	volatile unsigned int  asc_sfcc;                           /*0x0070*/
	volatile unsigned int  asc_rsvd6[3];   /* for mapping */   /*0x0074*/
	volatile unsigned int  asc_eomcon;                         /*0x0080*/
	volatile unsigned int  asc_rsvd7[26];   /* for mapping */  /*0x0084*/
	volatile unsigned int  asc_dmacon;                         /*0x00EC*/
	volatile unsigned int  asc_rsvd8[1];   /* for mapping */   /*0x00F0*/
	volatile unsigned int  asc_irnen;                          /*0x00F4*/
	volatile unsigned int  asc_irncr;                          /*0x00F8*/
	volatile unsigned int  asc_irnicr;                         /*0x00FC*/
} ifx_asc_reg_t;

#define ASCID_TX32                      0x80000000
#define ASCFSTAT_TXFFLMASK              0x1F00
#define ASCFSTAT_RXFFLMASK              0x001F

/* WHBSTATE register's bits and bitfields */
#define ASCWHBSTATE_CLRREN              0x00000001
#define ASCWHBSTATE_SETREN              0x00000002
#define ASCWHBSTATE_CLRPE               0x00000004
#define ASCWHBSTATE_CLRFE               0x00000008
#define ASCWHBSTATE_CLRRUE              0x00000010
#define ASCWHBSTATE_CLRROE              0x00000020
#define ASCWHBSTATE_CLRTOE              0x00000040
#define ASCWHBSTATE_CLRBE               0x00000080
#define ASCWHBSTATE_SETPE               0x00000100
#define ASCWHBSTATE_SETFE               0x00000200
#define ASCWHBSTATE_SETRUE              0x00000400
#define ASCWHBSTATE_SETROE              0x00000800
#define ASCWHBSTATE_SETTOE              0x00001000
#define ASCWHBSTATE_SETBE               0x00002000

/* RXFCON register's bits and bitfields */
#define ASCRXFCON_RXFEN                 0x0001
#define ASCRXFCON_RXFFLU                0x0002
#define ASCRXFCON_RXFITLMASK            0x3F00
#define ASCRXFCON_RXFITLOFF             8

/* TXFCON register's bits and bitfields */
#define ASCTXFCON_TXFEN                 0x0001
#define ASCTXFCON_TXFFLU                0x0002
#define ASCTXFCON_TXFITLMASK            0x3F00
#define ASCTXFCON_TXFITLOFF             8

/* STATE register's bits and bitfields */
#define ASCSTATE_REN                    0x00000001
#define ASCSTATE_PE                     0x00010000
#define ASCSTATE_FE                     0x00020000
#define ASCSTATE_RUE                    0x00040000
#define ASCSTATE_ROE                    0x00080000
#define ASCSTATE_TOE                    0x00100000
#define ASCSTATE_BE                     0x00200000
#define ASCSTATE_TXBVMASK               0x07000000
#define ASCSTATE_TXBVOFFSET             24
#define ASCSTATE_TXEOM                  0x08000000
#define ASCSTATE_RXBVMASK               0x70000000
#define ASCSTATE_RXBVOFFSET             28
#define ASCSTATE_RXEOM                  0x80000000
#define ASCSTATE_ANY                    (ASCSTATE_PE|ASCSTATE_FE|ASCSTATE_ROE)

/* CON register's bits and bitfields */
#define ASCMCON_MODEMASK                0x0000000f
#define ASCMCON_M_8ASYNC                0x0
#define ASCMCON_M_8IRDA                 0x1
#define ASCMCON_M_7ASYNC                0x2
#define ASCMCON_M_7IRDA                 0x3
#define ASCMCON_WLSMASK                 0x0000000c
#define ASCMCON_WLSOFFSET               2
#define ASCMCON_WLS_8BIT                0x0
#define ASCMCON_WLS_7BIT                0x1
#define ASCMCON_PEN                     0x00000010
#define ASCMCON_ODD                     0x00000020
#define ASCMCON_SP                      0x00000040
#define ASCMCON_STP                     0x00000080
#define ASCMCON_BRS                     0x00000100
#define ASCMCON_FDE                     0x00000200
#define ASCMCON_ERRCLK                  0x00000400
#define ASCMCON_EMMASK                  0x00001800
#define ASCMCON_EMOFFSET                11
#define ASCMCON_EM_ECHO_OFF             0x0
#define ASCMCON_EM_ECHO_AB              0x1
#define ASCMCON_EM_ECHO_ON              0x2
#define ASCMCON_LB                      0x00002000
#define ASCMCON_ACO                     0x00004000
#define ASCMCON_R                       0x00008000
#define ASCMCON_PAL                     0x00010000
#define ASCMCON_FEN                     0x00020000
#define ASCMCON_RUEN                    0x00040000
#define ASCMCON_ROEN                    0x00080000
#define ASCMCON_TOEN                    0x00100000
#define ASCMCON_BEN                     0x00200000
#define ASCMCON_TXINV                   0x01000000
#define ASCMCON_RXINV                   0x02000000
#define ASCMCON_TXMSB                   0x04000000
#define ASCMCON_RXMSB                   0x08000000

/* CLC register's bits and bitfields */
#define ASCCLC_DISR                     0x00000001
#define ASCCLC_DISS                     0x00000002
#define ASCCLC_RMCMASK                  0x0000FF00
#define ASCCLC_RMCOFFSET                8

/* interrupt lines masks for the ASC device interrupts*/
/* change these macroses if it's necessary */
#define IFX_ASC_IRQ_LINE_ALL            0x000000FF      /* all IRQs */
#define IFX_ASC_IRQ_LINE_MASK_ALL       0x00000000      /* disable/mask all IRQs */

#define IFX_ASC_IRQ_LINE_TIR            0x00000001      /* Tx Int */
#define IFX_ASC_IRQ_LINE_RIR            0x00000002      /* Rx Int */
#define IFX_ASC_IRQ_LINE_EIR            0x00000004      /* Error Int */
#define IFX_ASC_IRQ_LINE_TBIR           0x00000008      /* Tx Buffer Int */
#define IFX_ASC_IRQ_LINE_ABSTIR         0x00000010      /* Autobaud Start Int */
#define IFX_ASC_IRQ_LINE_ABDETIP        0x00000020      /* Autobaud Detection Int */
#define IFX_ASC_IRQ_LINE_MSIR           0x00000040      /* Modem Status Int */
#define IFX_ASC_IRQ_LINE_SFCIR          0x00000080      /* Software Flow Control Int */

#define IFX_ASC_TXFIFO_FL               1
#define IFX_ASC_RXFIFO_FL               1
#define IFX_ASC_TXFIFO_FULL             16

/* TXFCON register's bits and bitfields */
#define ASCTXFCON_TXFEN                 0x0001
#define ASCTXFCON_TXFFLU                0x0002
#define ASCTXFCON_TXFITLMASK            0x3F00
#define ASCTXFCON_TXFITLOFF             8
// clang-format on

/**
 */
static void get_fdv_and_reload_value(unsigned int baudrate, unsigned int *fdv,
				     unsigned int *reload)
{
	struct clk *pfpiclk;
	unsigned int fpi_clk;
	unsigned long long baudrate1 = (unsigned long long)baudrate * 8192;
	unsigned long long baudrate2 = (unsigned long long)baudrate * 1000;
	unsigned long long fdv_over_bg_fpi;
	unsigned long long fdv_over_bg;
	unsigned long long difference;
	unsigned long long min_difference;
	unsigned int bg;

	pfpiclk = clk_get_sys("fpi", "fpi");
	if (!pfpiclk || IS_ERR(pfpiclk)) {
		pr_err("failed to get fpi clk\n");
		clk_put(pfpiclk);
		return;
	}
	fpi_clk = clk_get_rate(pfpiclk);
	/* Sanity check first */
	if (baudrate >= (fpi_clk >> 4)) {
		pr_err("%s current fpi clock %u can't provide baudrate %u\n",
		       __func__, fpi_clk, baudrate);
		clk_put(pfpiclk);
		return;
	}
	/*--- pr_err("%s current fpi clock %u baudrate %u\n", __func__, fpi_clk, baudrate); ---*/
	min_difference = UINT_MAX;
	fdv_over_bg_fpi = baudrate1;
	for (bg = 1; bg <= 8192; bg++, fdv_over_bg_fpi += baudrate1) {
		fdv_over_bg = fdv_over_bg_fpi + fpi_clk / 2;
		do_div(fdv_over_bg, fpi_clk);
		if (fdv_over_bg <= 512) {
			difference = fdv_over_bg * fpi_clk * 1000;
			do_div(difference, 8192 * bg);
			if (difference < baudrate2)
				difference = baudrate2 - difference;
			else
				difference -= baudrate2;
			if (difference < min_difference) {
				*fdv = (unsigned int)fdv_over_bg & 511;
				*reload = bg - 1;
				min_difference = difference;
			}
			/* Perfect one found */
			if (min_difference == 0)
				break;
		}
	}
	clk_put(pfpiclk);
	/*--- printk(KERN_ERR"%s baud=%u *fdv=%u *reload=%u\n", __func__, baudrate, *fdv, *reload); ---*/
}

/**
 * hier dynamisch linux_os_mask auffuellen
 */
static inline void arch_init_mips_cpu_config(void)
{
	unsigned int core;
	/*--- hier dynamisch linux_os_mask auffuellen ---*/
	unsigned int tc;

	memset(cpu_nr_to_tc_and_core, 0xFF, sizeof(cpu_nr_to_tc_and_core));
	for (core = 0; core < ARRAY_SIZE(mips_cpu_config); core++) {
		unsigned int linux_os_mask = 0;

		for (tc = 0; tc < PROFILING_MAX_COUNT_TCS; tc++) {
			int cpu_id = get_cpuid_by_mt(core, tc, NULL);

			if (cpu_id < 0) {
				continue;
			}
			cpu_nr_to_tc_and_core[cpu_id].core = core;
			cpu_nr_to_tc_and_core[cpu_id].tc = tc;
			linux_os_mask |= 1 << tc;
		}
		mips_cpu_config[core].linux_os_mask = linux_os_mask;
		/*--- printk(KERN_INFO"%s: linux_os_mask %x\n", __func__, linux_os_mask); ---*/
	}
}

/**
 * nur dummy
 */
int arch_trigger_valid(void)
{
	return 1;
}

/**
 */
static inline void arch_set_next_trigger(unsigned int next)
{
	cycle_t next_cnt = (cycle_t)next + gic_read_count();

	gic_write_compare(next_cnt);
}

/**
 */
static inline unsigned int arch_get_profile_irq(void)
{
	unsigned int irq_nr;
	gic_timer_get_irq(&irq_nr);
	return irq_nr;
}

/**
 */
static inline int arch_yield_map_setup(unsigned int cpu, unsigned int irq,
				       unsigned int mode, unsigned int pin)
{
	return gic_map_setup(cpu, irq, mode, pin);
}

/**
 * ret: 0 ok
 */
static inline int arch_uart_init(unsigned int core, unsigned int baud)
{
	ifx_asc_reg_t *asc_reg = (ifx_asc_reg_t *)ASC_UART1_BASE;
	unsigned int fdv = 0, reload = 0;

	asc_reg->asc_clc = 0x1 << ASCCLC_RMCOFFSET; /* Set CLC register*/
	asc_reg->asc_mcon = ASCMCON_M_8ASYNC |
			    0x00c00000; /* Initialy we are in async mode */

	asc_reg->asc_irnen =
		IFX_ASC_IRQ_LINE_MASK_ALL; /* disable ASC interrupts in module */
	asc_reg->asc_irncr = 0xFF;

	asc_reg->asc_rxfcon |= ASCRXFCON_RXFEN; /* enable receiver */
	asc_reg->asc_txfcon |= ASCTXFCON_TXFEN; /* enable tranmsitter */
	asc_reg->asc_mcon   |= ASCMCON_FEN;     /* enable error signals */

	/* Clear all error interrupts and disable receiver */
	asc_reg->asc_whbstate = ASCWHBSTATE_CLRPE | ASCWHBSTATE_CLRFE |
				ASCWHBSTATE_CLRRUE | ASCWHBSTATE_CLRROE |
				ASCWHBSTATE_CLRTOE | ASCWHBSTATE_CLRBE;
	asc_reg->asc_eomcon = 0x00010300;

	asc_reg->asc_mcon |= ASCMCON_FDE; /* set up to use divisor of 2 */
	/* now we can write the new baudrate into the register */
	get_fdv_and_reload_value(baud, &fdv, &reload);
	/*--- printk(KERN_INFO"[simple-profiling]%s baud %d asc_reg %p asc_id=%x fdv=%x reload=%x\n", __func__, baud, asc_reg, asc_reg->asc_id, fdv, reload); ---*/

	asc_reg->asc_fdv = fdv;
	asc_reg->asc_bg = reload;

	asc_reg->asc_mcon =
		(asc_reg->asc_mcon & ~ASCMCON_WLSMASK) |
		(ASCMCON_WLS_8BIT << ASCMCON_WLSOFFSET); /*--- 8 bit ---*/
	asc_reg->asc_mcon &= ~ASCMCON_PEN; /*--- no parity ---*/
	asc_reg->asc_mcon &= ~ASCMCON_STP; /*--- number of stopp-bits ---*/

	asc_reg->asc_mcon |= ASCMCON_R; /* turn on baudrate generator */
	return 0;
}

/**
 */
static inline void arch_uart_send_byte(unsigned int core, unsigned char value)
{
	ifx_asc_reg_t *asc_reg = (ifx_asc_reg_t *)ASC_UART1_BASE;

	while (asc_reg->asc_fstat & ASCFSTAT_TXFFLMASK)
		;

	/*---     if((asc_reg->asc_id & ASCID_TX32)) { ---*/
	/*---         *(((char*)&asc_reg->asc_tbuf) + 3) = value; ---*/
	/*---     } else { ---*/
	asc_reg->asc_tbuf = value;
	/*---     } ---*/
}

/**
 */
static inline void arch_uart_send_word(unsigned int core, unsigned int val32)
{
	arch_uart_send_byte(core, (unsigned char)(val32 >> 0));
	arch_uart_send_byte(core, (unsigned char)(val32 >> 8));
	arch_uart_send_byte(core, (unsigned char)(val32 >> 16));
	arch_uart_send_byte(core, (unsigned char)(val32 >> 24));
}

/**
 */
static inline void arch_next_trigger_for_cpu(int cpu, unsigned int next)
{
	cycle_t next_cnt;

	if (next < PROFILING_USEC_TO_TIMER(1))
		next = PROFILING_USEC_TO_TIMER(1);
	next_cnt = (cycle_t)next + gic_read_count();
	gic_write_cpu_compare(next_cnt, cpu);
}
#endif /*--- #if defined(ARCH_MIPS_PROFILE_C) ---*/
#endif /*--- #ifndef __arch_profile_grx_h__ ---*/
