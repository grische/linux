/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#include <asm/mach-ikan_mips/vx180.h>
#else /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2.6.28) ---*/
#include <vx180.h>
#endif /*--- #else ---*/ /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2.6.28) ---*/
#include <linux/ioport.h>
#include "i2c.h"
#include "dynpll.h"

#define AVM_DYNPLL_DEBUG

#if defined(AVM_DYNPLL_DEBUG)
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)    printk(KERN_WARNING args)
#define DEB_NOTE(args...)    printk(KERN_NOTICE args)
#define DEB_INFO(args...)    printk(KERN_INFO args)
#define DEB_TRACE(args...)   printk(KERN_INFO args)
#else/*--- #if defined(AVM_DYNPLL_DEBUG) ---*/
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)
#define DEB_NOTE(args...)
#define DEB_INFO(args...)
#define DEB_TRACE(args...)
#endif/*--- #else ---*//*--- #if defined(AVM_DYNPLL_DEBUG) ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct resource dynpll_gpioressource[] = {
{
	.name  = "dynpll_s0",
	.flags = IORESOURCE_IO,
	.start = DYNPLL_GPIO_S0,
	.end   = DYNPLL_GPIO_S0
},
{
	.name  = "dynpll_sdata",
	.flags = IORESOURCE_IO,
	.start = DYNPLL_GPIO_SDATA,
	.end   = DYNPLL_GPIO_SDATA
},
{
	.name  = "dynpll_sclk",
	.flags = IORESOURCE_IO,
	.start = DYNPLL_GPIO_SCLK,
	.end   = DYNPLL_GPIO_SCLK
}
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _CDCEL949_generic_conf_register {
	/*--- 0x0 ---*/
	unsigned char e_el:1;       /*--- Device Identification (read only): 1 is CDCE949 (3.3V), 0 is CDCEL949 (1.8V) ---*/
	unsigned char rid:3;        /*--- Revision Identification Number (read only) ---*/
	unsigned char vid:4;        /*--- Vendor Identification Number (read only) ---*/
	/*--- 0x1 ---*/
	unsigned char reserved1:1;
	unsigned char eepip:1;      /*--- EEPROM Programming Status 0 programming is complete 1 is in programming mode ---*/
	unsigned char eelock:1;     /*--- Permaently Lock EEPROM 0 not locked ---*/

	unsigned char pwdn:1;       /*--- Device power down (overwrites S0/S1/S2 setting; configuration register settings are unchanged)     ---*/
								/*--- 0 - device active (all PLLs and all outputs are enabled) ---*/
								/*--- 1 - device power down (all PLLs in power down and all outputs in TriState) ---*/
	unsigned char inclk:2;      /*--- Input clock selection: 0 Xtal, 1 vcxo, 2 lvcmos ---*/
	unsigned char slave_adr:2;  /*--- Programmable Address Bits A0 and A1 of the Slave Receiver Address ---*/
	/*--- 0x2 ---*/
	unsigned char m1:1;         /*--- Clock source selection for output Y1: 0 input clock 1 PLL1 clock ---*/
	unsigned char spicon:1;     /*--- Operation mode selection for pin 22/23 0 i2c-interface 1 S1/S2 ---*/
	unsigned char y1st1:2;      /*--- Y1-State1 Definition (applies to Y1_ST1 and Y1_ST0) */
	unsigned char y1st0:2;      /*--- Y1-State0 Definition (applies to Y1_ST1 and Y1_ST0) */
								/*--- 00 - device power down (all PLLs in power down and all outputs in 3-state) ---*/
								/*--- 01 - Y1 disabled to tristate ---*/
								/*--- 10 - Y1 disables to 0 ---*/
								/*--- 11 - Y1 enabled (normal operation) ---*/
#define PDIV1(a)  (((a)->pdiv1_98 << 8) | (a)->pdiv1_07)
#define SET_PDIV1(a, b) (((a)->pdiv1_98 = (b) >> 8), (a)->pdiv1_07 = (b))
	unsigned char pdiv1_98:2;
	/*--- 0x3 ---*/
	unsigned char pdiv1_07;    /*--- 10-Bit Y1-Output-Divider Pdiv1: 0: divider reset and stand-by ---*/
	/*--- 0x4 ---*/
	/*--- These are the bits of the Control Terminal Register. The user can pre-define up to eight different control settings. These settings can ---*/
	/*--- then be selected by the external control pins, S0, S1, and S2. ---*/
	unsigned char y1_state;    /*--- Y1_x State Selection  ---*/
	/*--- 0x5 ---*/
	unsigned char xcsel:5;      /*--- Crystal load capacitor ---*/
	unsigned char reserved2:3;
	/*--- 0x6 ---*/
	unsigned char bcount:7;      /*--- 7-Bit Byte Count (Defines the number of Bytes which will be sent from this device device at the next Block ---*/
	unsigned char eewrite:1;    /*--- Initiate EEPROM Write Cycle  ---*/
								/*--- 0 - no EEPROM write cycle ---*/
								/*--- 1 - start EEPROM write cycle (internal configuration register are saved to the EEPROM) ---*/
};
/*--------------------------------------------------------------------------------*\
	0x10
\*--------------------------------------------------------------------------------*/
struct _CDCEL949_pll1_conf_register {
	/*--------------------------------------------------------------------------------*\
	SSC1: PLL1 SSC Selection (Modulation Amount)
	The user can pre-define up to eight different control settings. In normal device operation, these settings can be selected by the external
	control pins, S0, S1, and S2.
	\*--------------------------------------------------------------------------------*/
	/*--- 0x10 ---*/
	unsigned char ssc1_7:3;
	unsigned char ssc1_6:3;
	unsigned char ssc1_5_21:2;
	/*--- 0x11 ---*/
	unsigned char ssc1_5_0:1;
	unsigned char ssc1_4:3;
	unsigned char ssc1_3:3;
	unsigned char ssc1_2_2:1;
	/*--- 0x12 ---*/
	unsigned char ssc1_2_10:2;
	unsigned char ssc1_1:3;
	unsigned char ssc1_0:3;
	/*--- FS1_x: PLL1 Frequency Selection ---*/
	/*--- 0x13 ---*/
	unsigned char fs1_x; /*--- 0 - fVCO1_0 (predefined by PLL1_0 - Multiplier/Divider value) ---*/
						 /*--- 1 - fVCO1_1 (predefined by PLL1_1 - Multiplier/Divider value) ---*/
	/*--- 0x14 ---*/
	unsigned char mux1:1;  /*--- 0 PLL1 1 Bypass ---*/
	unsigned char m2:1;    /*--- Output Y2 Multiplexer: 0 Pdiv1 1 Pdiv2 ---*/
	unsigned char m3:2;    /*--- Output Y3 Multiplexer: 00 Pdiv1 01 Pdiv2 10 Pdiv3 ---*/
	unsigned char y2y3_st1:2;   /*--- Y2,Y3-State0/1 definition: 00 - Y2/Y3 disabled to TriState (PLL1 is in power down)
																 01 - Y2/Y3 disabled to TriState (PLL1 on)
																 10 - Y2/Y3 disabled to low (PLL1 on)
																 11 - Y2/Y3 enabled (normal operation, PLL1 on)  ---*/
	unsigned char y2y3_st0:2;
	/*--- 0x15 ---*/
	unsigned char y2y3_x;      /*--- Y2Y3_x Output State Selection ---*/
	/*--- 0x16 ---*/
	unsigned char ssc1dc:1;    /*--- PLL1 SSC down/center selection: 0 down 1 center ---*/
	unsigned char pdiv2:7;     /*--- pdiv2   0 reset and standby ---*/
	/*--- 0x17 ---*/
	unsigned char reserved:1;  /*--- PLL1 SSC down/center selection: 0 down 1 center ---*/
	unsigned char pdiv3:7;     /*--- pdiv3 0 reset and standby  ---*/
	/*--- 0x18 ---*/
#define PLL1_0N(a) (((a)->pll1_0N_114 << 4) | (a)->pll1_0N_30)
#define SET_PLL1_0N(a, b) (((a)->pll1_0N_114 = ((b) >> 4)), (a)->pll1_0N_30 = (b) & 0xF)
	unsigned char pll1_0N_114;
	/*--- 0x19 ---*/
	unsigned char pll1_0N_30:4;
#define PLL1_0R(a) (((a)->pll1_0R_85 << 5) | (a)->pll1_0R_40)
#define SET_PLL1_0R(a, b) (((a)->pll1_0R_85 = (b) >> 5), (a)->pll1_0R_40 = (b) & 0x1F)
	unsigned char pll1_0R_85:4;
	/*--- 0x1A ---*/
	unsigned char pll1_0R_40:5;
#define PLL1_0Q(a) (((a)->pll1_0Q_53 << 3) | (a)->pll1_0Q_20)
#define SET_PLL1_0Q(a, b) (((a)->pll1_0Q_53 = (b) >> 3), (a)->pll1_0Q_20 = (b) & 0x7)
	unsigned char pll1_0Q_53:3;
	/*--- 0x1B ---*/
	unsigned char pll1_0Q_20:3;
	unsigned char pll1_0P:3;
	unsigned char vco1_0_range:2;
	/*--- 0x1C ---*/
#define PLL1_1N(a) (((a)->pll1_1N_114 << 4) | (a)->pll1_1N_30)
#define SET_PLL1_1N(a, b) (((a)->pll1_1N_114 = (b) >> 4), (a)->pll1_1N_30 = (b) & 0x7)
	unsigned char pll1_1N_114;
	/*--- 0x1D ---*/
	unsigned char pll1_1N_30:4;
#define PLL1_1R(a) (((a)->pll1_1R_85 << 5) | (a)->pll1_1R_40)
#define SET_PLL1_1R(a, b) (((a)->pll1_1R_85 = (b) >> 5), (a)->pll1_1R_40 = (b) & 0x1F)
	unsigned char pll1_1R_85:4;
	/*--- 0x1E ---*/
	unsigned char pll1_1R_40:5;
#define PLL1_1Q(a) (((a)->pll1_1Q_53 << 3) | (a)->pll1_1Q_20)
#define SET_PLL1_1Q(a, b) (((a)->pll1_1Q_53 = (b) >> 3), (a)->pll1_1Q_20 = (b) & 0x7)
	unsigned char pll1_1Q_53:3;
	/*--- 0x1F ---*/
	unsigned char pll1_1Q_20:3;
	unsigned char pll1_1P:3;
	unsigned char vco1_1_range:2;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _CDCEL949_shadow_register {
	struct _CDCEL949_generic_conf_register generic;
	unsigned char reserved[9];
	struct _CDCEL949_pll1_conf_register pll1;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _dynpllmgr {
	struct _CDCEL949_shadow_register shadowreg;
	spinlock_t   dynpll_lock;
	unsigned int Mref;
	unsigned int Nref;
	unsigned int actstate;
	unsigned int actfreq;
	unsigned int M_St[2];
	unsigned int N_St[2];
	int init;
} gdynpll;

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _pll_values {
	unsigned int P;
	unsigned int Q;
	unsigned int R;
	unsigned int vcorange;
	unsigned int Ns;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
inline int mylog2(int val) {
	return ffs(val) - 1;
}
/*--------------------------------------------------------------------------------*\
  Ermittelt die Registerwerte aus M, N, freqin
  PLL MULTIPLIER/DIVIDER DEFINITION

	fOUT = fIN / Pdiv * N / M

	where
	M (1 to 511) and N (1 to 4095) are the multiplier/divider values of the PLL;
	Pdiv (1 to 127) is the output divider.

	fVCO = fIN * N / M

	P = 4 - int(log2( N / M) {if P < 0 then P = 0}

	Q = int (N' / M)
	R = N'-M * Q
	N'= N * 2^P

	where
	N >= M
	80 MHz < fVCO > 230 MHz.
	N:Step
\*--------------------------------------------------------------------------------*/
static void calculate_pll_values(int freqin, int M, int N, struct _pll_values *pv) {
	int P, Q, R, n, vcofreq;

	P = 4 - mylog2(N / M);
	if(P < 0) P = 0;
	n = N * (1 << P);
	Q = n / M;
	R = n  - M * Q;

	vcofreq = (freqin * N ) / M;
	if(vcofreq < 125000000) {
		pv->vcorange = 0;
	} else if(vcofreq < 150000000) {
		pv->vcorange = 1;
	} else if(vcofreq < 175000000) {
		pv->vcorange = 2;
	} else {
		pv->vcorange = 3;
	}
	pv->P  = P;
	pv->Q  = Q;
	pv->R  = R;
	pv->Ns = n;
	DEB_TRACE("M=%d N=%d P=%d n=%d Q=%d R=%d\n", M, N, P, n, Q, R);
}

#define print_outfreq(outfreqkhz)  {unsigned M, N, PDIV; DEB_TRACE("freq: %u -> %u (M=%u N=%u PDIV=%u)\n", outfreqkhz * 1000 , calculate_m_n_div(25 * 1000 * 1000, outfreqkhz * 1000, &M, &N, &PDIV), M, N, PDIV); }
/*--------------------------------------------------------------------------------*\
 * Ermittelt aus infreq nach  outfreq die notwendigen Werte M, N, Pdiv
 * ret: calculated outfreq
\*--------------------------------------------------------------------------------*/
static int calculate_m_n_div(unsigned int infreq, unsigned int outfreq, unsigned int *M, unsigned int *N, unsigned int *Pdiv) {
	unsigned int m, n, pdiv = 1, outfreqpre, outfreq_c;
	unsigned int delta , bestdelta = (unsigned int)-1, bestfreq = 0;
	for(m = 1; m < 512; m++) {
		for(n = m; n < 4096; n++) {
			outfreqpre = (infreq * n) / m;
			if(outfreqpre > 230 * 1000000) {
				continue;
			}
			if(outfreqpre < 80 * 1000000) {
				continue;
			}
			pdiv = 1;
			do {
				outfreq_c = outfreqpre / pdiv;
				delta = outfreq_c > outfreq ? (outfreq_c - outfreq) : (outfreq - outfreq_c);
				if(delta < bestdelta) {
					bestdelta = delta;
					bestfreq = outfreq_c;
					if(delta == 0) {
						if(M) *M = m;
						if(N) *N = n;
						if(Pdiv) *Pdiv = pdiv;
						return outfreq_c;
					}
				}
			} while(pdiv++ < 127);
		}
	}
	if(M) *M = m;
	if(N) *N = n;
	if(Pdiv) *Pdiv = pdiv;
	return bestfreq;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void print_pll1(char *prefix, struct _CDCEL949_pll1_conf_register *pll1) {
	DEB_ERR("pll1(%d): % *B\n", sizeof(*pll1), sizeof(*pll1), pll1);

	DEB_ERR("%sSSC1_0(%x) SSC1_1(%x) FS1_x(%x) Mux1=%s M2=pdiv%c M3=pdiv%c Y2Y3_ST0=%s Y2Y3_ST1=%s Y2Y3_x=%x SSC1DC=%s PDIV2=%d PDIV3=%d\n"
		   "0N/R/Q/P/vco-range=%d/%d/%d/%d/%d 1N/R/Q/P/vcorange=%d/%d/%d/%d/%d\n",
							prefix,
							pll1->ssc1_0,
							pll1->ssc1_1,
							pll1->fs1_x,
							pll1->mux1 ? "bypass" : "pll1",
							pll1->m2 ? '2' : '1',
							pll1->m3 == 0 ? '1' :
							pll1->m3 == 1 ? '2' :
							pll1->m3 == 2 ? '3' : '?',
							pll1->y2y3_st1 == 0 ? "pd" :
							pll1->y2y3_st1 == 1 ? "Y1 disabled(tristate)" :
							pll1->y2y3_st1 == 2 ? "Y1 disabled(low)" :
							pll1->y2y3_st1 == 3 ? "Y1 enabled" : "?",
							pll1->y2y3_st0 == 0 ? "pd" :
							pll1->y2y3_st0 == 1 ? "Y1 disabled(tristate)" :
							pll1->y2y3_st0 == 2 ? "Y1 disabled(low)" :
							pll1->y2y3_st0 == 3 ? "Y1 enabled" : "?",
							pll1->y2y3_x,
							pll1->ssc1dc ? "center" : "down",
							pll1->pdiv2,
							pll1->pdiv3,
							PLL1_0N(pll1),
							PLL1_0R(pll1),
							PLL1_0Q(pll1),
							pll1->pll1_0P,
							pll1->vco1_0_range,
							PLL1_1N(pll1),
							PLL1_1R(pll1),
							PLL1_1Q(pll1),
							pll1->pll1_1P,
							pll1->vco1_1_range
							);
#if defined(AVM_DYNPLL_DEBUG)
	mdelay(1000);
#endif/*--- #if defined(AVM_DYNPLL_DEBUG) ---*/
}
/*--------------------------------------------------------------------------------*\
 * Lese Generic-Conf und PLL1-Bereich aus
 * ret = 0 ok
\*--------------------------------------------------------------------------------*/
static int read_registerset(struct _dynpllmgr *pdynpllmgr, struct _CDCEL949_shadow_register *preg, unsigned int show) {
	long flags;
	unsigned int i;
	unsigned char *p;

	spin_lock_irqsave(&pdynpllmgr->dynpll_lock, flags);
	I2C_WriteByte(CDCEL949_DEVICE_ID, CDCEL949_BUFCOUNT_ADDR, sizeof(struct _CDCEL949_shadow_register) << 1);
	if(I2C_ReadByte(CDCEL949_DEVICE_ID, CDCEL949_BUFCOUNT_ADDR) != sizeof(struct _CDCEL949_shadow_register) << 1) {
		spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
		DEB_ERR("error on read generic_conf_register#1\n");
		return 1;
	}
	p = (unsigned char *)preg;
	if(I2C_ReadBuffer(CDCEL949_DEVICE_ID, CDCEL949_GENERAL_ADDR, p, sizeof(*preg)) < sizeof(*preg)) {
		spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
		DEB_ERR("error on read generic_conf_register#2\n");
		return 1;
	}
	spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
	DEB_TRACE("dynpll_read(%d): % *B\n", sizeof(*preg), sizeof(*preg), p);

	DEB_ERR("CDCE949 %sV Revision %d Vendor %d\n", preg->generic.e_el ? "3.3" : "1.8", preg->generic.rid, preg->generic.vid);
#if !defined(AVM_DYNPLL_DEBUG)
	if(show)
#endif/*--- #if !defined(AVM_DYNPLL_DEBUG) ---*/
	{
		DEB_ERR("%sinclk=%s m1=%s Y1_ST1=%s Y1_ST0=%s PDIV1=%d Y1_x=%x XSEL=%d\n",
							preg->generic.pwdn ? "powerdown " : "",
							preg->generic.inclk == 0 ? "xtal" :
							preg->generic.inclk == 0 ? "vcxo" :
							preg->generic.inclk == 0 ? "lvcmos" : "?",
							preg->generic.m1    == 0 ? "input clk" : "pll",
							preg->generic.y1st1 == 0 ? "pd" :
							preg->generic.y1st1 == 1 ? "Y1 disabled(tristate)" :
							preg->generic.y1st1 == 2 ? "Y1 disabled(low)" :
							preg->generic.y1st1 == 3 ? "Y1 enabled" : "?",
							preg->generic.y1st0 == 0 ? "pd" :
							preg->generic.y1st0 == 1 ? "Y1 disabled(tristate)" :
							preg->generic.y1st0 == 2 ? "Y1 disabled(low)" :
							preg->generic.y1st0 == 3 ? "Y1 enabled" : "?",
							PDIV1(&preg->generic),
							preg->generic.y1_state,
							preg->generic.xcsel
				   );

		print_pll1("pll1:", &preg->pll1);
	}
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void setintial_genreg(struct _dynpllmgr *pdynpllmgr) {
	struct _CDCEL949_generic_conf_register *gen = &pdynpllmgr->shadowreg.generic;
	long flags;
	gen->pwdn  = 0;
	gen->inclk = 0;       /*--- inclk = xtal ---*/
	gen->m1 = 0;          /*--- Y1 get input clk ---*/
	gen->y1st1 = 3;
	gen->y1st0 = 3;
	SET_PDIV1(gen, 1);
	gen->y1_state = 2;
	/*--- gen->xcsel = ? ---*/

	spin_lock_irqsave(&pdynpllmgr->dynpll_lock, flags);
	I2C_WriteBuffer(CDCEL949_DEVICE_ID, CDCEL949_GENERAL_ADDR, sizeof(*gen), (unsigned char *)gen); /*--- programm gen ---*/
	spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
}
/*--------------------------------------------------------------------------------*\
 * verwende Pdiv2 -> fuer beide States gleich
 * State 1: hat fvco1 * nscale / mscale
 * ret: pdiv2 NACH Anschalten der PLL
 * ret: < 0 Fehler!
\*--------------------------------------------------------------------------------*/
static int prepare_pll1reg(struct _CDCEL949_pll1_conf_register *pll1, unsigned int infreq, unsigned int stdfreq, unsigned mscale, unsigned int nscale, unsigned int *Mref, unsigned int *Nref) {
	unsigned int realoutfreq, M, N, Pdiv, pllbypassfreq, vcofreq;
	struct _pll_values pllval;
#if 1
	realoutfreq = calculate_m_n_div(infreq, stdfreq, &M, &N, &Pdiv);
	DEB_TRACE("infreq %d stdfreq %d(%d) M=%d N=%d Pdiv=%d\n", infreq, realoutfreq, stdfreq, M, N, Pdiv);

	pll1->mux1  = 1;        /*--- settings bypass for config ---*/
	pll1->m2    = 1;        /*--- Y2 use Pdiv2   ---*/

	pll1->fs1_x   = 2;      /*--- State 0 -> fvco0, State 1 -> fvco1  ---*/
	pll1->y2y3_x  = 2;      /*--- State 0 -> Y2_St0, State1 -> Y2_St1 ---*/
	pll1->pdiv2   = Pdiv;

	/*--------------------------------------------------------------------------------*\
		Problem: wenn pdiv1 VOR Anschalten der PLL1 gesetzt wird, wird der Prozessor evtl. zu langsam getaktet (bleibt stehen):
		versuchsweise zeigt sich dass 25 /3 MHz noch geht, aber 25 / 4 MHz , nicht mehr!
	\*--------------------------------------------------------------------------------*/
	for(;;) {
		pllbypassfreq= infreq / pll1->pdiv2;
		if(pllbypassfreq < 25000000 / 3) {
			if(pll1->pdiv2 == 1) {
				DEB_ERR("dynpll: error on calculate pdiv for pll-setting freq: %d -> %d pdfreq=%d", infreq, stdfreq, stdfreq * nscale / mscale);
				return -1;
			}
			pll1->pdiv2--;
			continue;
		}
		break;
	}
	if((M * mscale) > 511) {
		DEB_ERR("dynpll: error mscale %d to high M %d \n", mscale, M);
		return -2;
	}
	if((N * nscale) > 4095) {
		DEB_ERR("dynpll: error nscale %d to high N=%d\n", nscale, N);
		return -3;
	}
	vcofreq = infreq * (N * nscale) / (M * mscale);
	if(vcofreq < 25000000 / 3) {
		/*--- checke ob minimierte Frequenz hoch genug  ---*/
		DEB_ERR("dynpll: error minized frequency %d to low\n", realoutfreq * nscale / mscale);
		return -4;
	}
	/*--- nun aber noch checken ob PLL mit diesem niedrigeren Pdiv Wert auch nicht über 25 MHz kommt (State1 - also mit N / scale) ---*/
	if((vcofreq / pll1->pdiv2) > 25000000) {
		DEB_ERR("dynpll: error vcofreq with minimized pdiv (%d) is to high freq: %d -> %d actfreq=%d\n", pll1->pdiv2, infreq, stdfreq, vcofreq / pll1->pdiv2);
		return -5;
	}

	if(Mref) *Mref = M;
	if(Nref) *Nref = N;
	/*--- pll1->ssc1dc = ?; ---*/
   /*--- pll1->ssc1_0 = ?  ---*/
   /*--- calculate_pll_values(infreq, M , N , &pllval); ---*/
   /*--- calculate_pll_values(infreq, M * mscale, N * nscale, &pllval); ---*/
   calculate_pll_values(infreq, M * mscale, N * nscale, &pllval);
   /*--- State 0 ---*/
   pll1->y2y3_st0 = 3;      /*--- Y2 State 0 enabled ---*/

   SET_PLL1_0N(pll1, pllval.Ns);
   SET_PLL1_0R(pll1, pllval.R);
   SET_PLL1_0Q(pll1, pllval.Q);
   pll1->pll1_0P      = pllval.P;
   pll1->vco1_0_range = pllval.vcorange;

	/*--- pll1->ssc1_1 = ? ---*/
	/*--- State 1 ---*/
	pll1->y2y3_st1 = 3;   /*--- Y2 State 1 enabled ---*/

	calculate_pll_values(infreq, M * mscale, N * nscale, &pllval);
	SET_PLL1_1N(pll1, pllval.Ns);
	SET_PLL1_1R(pll1, pllval.R);
	SET_PLL1_1Q(pll1, pllval.Q);
	pll1->pll1_1P      = pllval.P;
	pll1->vco1_1_range = pllval.vcorange;
#else
	pll1->mux1   = 1;     /*--- bypass ---*/
	pll1->m2     = 1;     /*--- Y2 use Pdiv2   ---*/
	pll1->pdiv2  = 3;

	pll1->fs1_x   = 2;    /*--- State 0 -> fvco0, State 1 -> fvco1  ---*/
	pll1->y2y3_x  = 2;    /*--- State 0 -> Y2_St0, State1 -> Y2_St1 ---*/

	pll1->y2y3_st0 = 3;   /*--- Y2 State 0 enabled ---*/
	pll1->y2y3_st1 = 3;   /*--- Y2 State 1 enabled ---*/
#endif

	print_pll1("prepare", pll1);
	return Pdiv;
}
#define NSCALE 1
#define MSCALE 2
/*--------------------------------------------------------------------------------*\
 * ermittelt bei Erfolg auch die Mref, Nref
\*--------------------------------------------------------------------------------*/
static int  setintial_pll1reg(struct _dynpllmgr *pdynpllmgr){
	long flags;
	struct _CDCEL949_pll1_conf_register *pll1 = &pdynpllmgr->shadowreg.pll1;
	/*--- pdiv2 wird erst mal so konfiguriert, dass Prozessor ohne PLL leben kann: ---*/
	int pdiv2 = prepare_pll1reg(pll1, XTAL_FREQUENCY, STD_FREQUENCY, MSCALE, NSCALE, &pdynpllmgr->Mref, &pdynpllmgr->Nref);
	if(pdiv2 <= 0) {
		return 1;
	}
	pdynpllmgr->N_St[0] = pdynpllmgr->Nref * NSCALE;
	pdynpllmgr->N_St[1] = pdynpllmgr->Nref * NSCALE;
	pdynpllmgr->M_St[0] = pdynpllmgr->Mref * MSCALE;
	pdynpllmgr->M_St[1] = pdynpllmgr->Mref * MSCALE;
	pdynpllmgr->actfreq = 25;   /*--- 25 / 2 ---*/

	spin_lock_irqsave(&pdynpllmgr->dynpll_lock, flags);
	I2C_WriteBuffer(CDCEL949_DEVICE_ID, CDCEL949_PLL1_ADDR, sizeof(*pll1), (unsigned char *)pll1); /*--- programm pll (pll1 in bypass) ---*/
	pll1->mux1  = 0; /*--- pll1 an ---*/
	I2C_WriteByte(CDCEL949_DEVICE_ID, CDCEL949_PLL1_MUX_ADDR, ((unsigned char *)pll1)[4]);
	/*--- nun Pdiv1 richtig stellen: ---*/
	if(pll1->pdiv2 != pdiv2) {
		pll1->pdiv2 = pdiv2; /*--- pll1 an - nun pdiv2 korrigieren ---*/
		I2C_WriteByte(CDCEL949_DEVICE_ID, CDCEL949_PLL1_PDIV2_ADDR, ((unsigned char *)pll1)[6]);
	}
	I2C_ReadBuffer(CDCEL949_DEVICE_ID, CDCEL949_PLL1_ADDR, (unsigned char *)pll1, sizeof(*pll1));
	spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
	print_pll1("after write", pll1);
	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int requestinit(void) {
	int i;

	for(i = 0; i < sizeof(dynpll_gpioressource) / sizeof(struct resource); i++) {
		if(request_resource(&gpio_resource, &dynpll_gpioressource[i])) {
			printk(KERN_ERR"[dynpll]ERROR: dynpll-resource %d gpio %u-%u not available\n", i, (unsigned int)dynpll_gpioressource[i].start, (unsigned int)dynpll_gpioressource[i].end);
			return 1;
		}
	}
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int dynpll_showregister(void) {
	struct _dynpllmgr *pdynpllmgr = &gdynpll;
	struct _CDCEL949_shadow_register reg;

	if(dynpll_init()) {
		return 1;
	}
	return read_registerset(pdynpllmgr, &reg, 1);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int dynpll_init(void) {
	struct _dynpllmgr *pdynpllmgr = &gdynpll;
	unsigned long flags;
	int i;

	if(pdynpllmgr->init) {
		return 0;
	}
#if 0
	if(requestinit()) {
		return 1;
	}
#endif
	memset(pdynpllmgr, 0, sizeof(*pdynpllmgr));
	spin_lock_init(&pdynpllmgr->dynpll_lock);

	VX180_GPIO_SETDIR(DYNPLL_GPIO_S0, VX180_GPIO_AS_OUTPUT);
	VX180_GPIO_OUTPUT(DYNPLL_GPIO_S0, 1); /*--- use State 1 ---*/
	pdynpllmgr->actstate = 1;

	spin_lock_irqsave(&pdynpllmgr->dynpll_lock, flags);
	I2C_Init(DYNPLL_GPIO_SDATA, DYNPLL_GPIO_SCLK);
	spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);

	if(read_registerset(pdynpllmgr, &pdynpllmgr->shadowreg, 0)) {
		return 1;
	}
	setintial_genreg(pdynpllmgr);
	if(setintial_pll1reg(pdynpllmgr )) {
		return 1;
	}
	/*--- for(i = 0; i < 5; i++) { ---*/
	/*---     mdelay(1000); ---*/
	/*---     DEB_TRACE("<%d>", i); ---*/
	/*--- } ---*/
	DEB_TRACE("\nnow switch to State 0\n");
	read_registerset(pdynpllmgr, &pdynpllmgr->shadowreg, 0);
	mdelay(50);

	pdynpllmgr->init = 1;
	dynpll_setfrequency(25 * 2);
	read_registerset(pdynpllmgr, &pdynpllmgr->shadowreg, 0);
	return 0;
}
EXPORT_SYMBOL(dynpll_init);
/*--------------------------------------------------------------------------------*\
 * freqout = (STD_FREQUENCY * nscale) / mscale
\*--------------------------------------------------------------------------------*/
int dynpll_setfrequencyscale(unsigned int mscale, unsigned int nscale) {
	struct _pll_values pllval;
	struct _dynpllmgr *pdynpllmgr = &gdynpll;
	struct _CDCEL949_pll1_conf_register *pll1;
	unsigned int Mnew, Nnew;
	long flags;

	if(dynpll_init()) {
		return 1;
	}
	if((mscale|nscale) == 0) {
		DEB_ERR("dynpll: invalid mscale or nscale\n");
		return 1;
	}
	Mnew = pdynpllmgr->Mref * mscale;
	if(Mnew > 511) {
		DEB_ERR("dynpll: error mscale to high M=%d mscale=%d\n", pdynpllmgr->Mref, mscale);
		return 1;
	}
	Nnew = pdynpllmgr->Nref * nscale;
	if(Nnew > 4095) {
		DEB_ERR("dynpll: error nscale to high N=%d nscale=%d\n", pdynpllmgr->Nref, nscale);
		return -3;
	}
	spin_lock_irqsave(&pdynpllmgr->dynpll_lock, flags);
	if((pdynpllmgr->M_St[pdynpllmgr->actstate] == Mnew) && (pdynpllmgr->N_St[pdynpllmgr->actstate] == Nnew)) {
		DEB_TRACE("dynpll: same frequency\n");
		msleep(500);
		spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
		return 0;
	}
	pdynpllmgr->actstate = !pdynpllmgr->actstate;
	if((pdynpllmgr->M_St[pdynpllmgr->actstate] == Mnew) && (pdynpllmgr->N_St[!pdynpllmgr->actstate] == Nnew)) {
		DEB_TRACE("dynpll: same frequency but other State\n");
		msleep(500);
		VX180_GPIO_OUTPUT(DYNPLL_GPIO_S0, pdynpllmgr->actstate); /*--- switch to other State ---*/
		spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
		return 0;
	}
	/*--- change pll of inactive state: ---*/
	calculate_pll_values(XTAL_FREQUENCY, Mnew, Nnew, &pllval);

	pdynpllmgr->M_St[pdynpllmgr->actstate] = Mnew;
	pdynpllmgr->N_St[pdynpllmgr->actstate] = Nnew;

	pll1 = &pdynpllmgr->shadowreg.pll1;
	if( pdynpllmgr->actstate == 0) {
		/*--- State 0 ---*/
		SET_PLL1_0N(pll1, pllval.Ns);
		SET_PLL1_0R(pll1, pllval.R);
		SET_PLL1_0Q(pll1, pllval.Q);
		pll1->pll1_0P      = pllval.P;
		pll1->vco1_0_range = pllval.vcorange;
		I2C_WriteBuffer(CDCEL949_DEVICE_ID, CDCEL949_PLL1_0xADDR, CDCEL949_PLL1_xx_LEN, (unsigned char *)pll1 + CDCEL949_PLL1_0xADDR - CDCEL949_PLL1_ADDR);
	 } else {
		/*--- State 1 ---*/
		SET_PLL1_1N(pll1, pllval.Ns);
		SET_PLL1_1R(pll1, pllval.R);
		SET_PLL1_1Q(pll1, pllval.Q);
		pll1->pll1_1P      = pllval.P;
		pll1->vco1_1_range = pllval.vcorange;
		I2C_WriteBuffer(CDCEL949_DEVICE_ID, CDCEL949_PLL1_1xADDR, CDCEL949_PLL1_xx_LEN, (unsigned char *)pll1 + CDCEL949_PLL1_1xADDR - CDCEL949_PLL1_ADDR);
	}
	DEB_TRACE("dynpll: new frequency mscale %d nscale %d\n", mscale, nscale);
	VX180_GPIO_OUTPUT(DYNPLL_GPIO_S0, pdynpllmgr->actstate); /*--- and now switch to other State ---*/
	spin_unlock_irqrestore(&pdynpllmgr->dynpll_lock, flags);
	return 0;
}

/*--------------------------------------------------------------------------------*\
 * newfreq in 500 KHz-Schritten anpassen
\*--------------------------------------------------------------------------------*/
int dynpll_setfrequency(unsigned int newfreq) {
	struct _dynpllmgr *pdynpllmgr = &gdynpll;
	int i, step = 1, actfreq;

	if(newfreq < pdynpllmgr->actfreq) {
		step = -1;
	}
	actfreq = pdynpllmgr->actfreq;

	while(newfreq != actfreq) {
		actfreq += step;
		if(actfreq < 9 * 2) {
			break;
		}
		if(actfreq >  25 * 2) {
			break;
		}
		if(dynpll_setfrequencyscale(50, actfreq)) {
			return 1;
		}
		pdynpllmgr->actfreq = actfreq;
	}
	return 0;
}
