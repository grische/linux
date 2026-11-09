/*
 * General MIPS MT support routines, usable in AP/SP and SMVP.
 * Copyright (C) 2005 Mips Technologies, Inc
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/security.h>

#include <asm/cpu.h>
#include <asm/processor.h>
#include <linux/atomic.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/mipsmtregs.h>
#include <asm/r4kcache.h>
#include <asm/cacheflush.h>

int vpelimit;

static int __init maxvpes(char *str)
{
	get_option(&str, &vpelimit);

	return 1;
}

__setup("maxvpes=", maxvpes);

int tclimit;

static int __init maxtcs(char *str)
{
	get_option(&str, &tclimit);

	return 1;
}

__setup("maxtcs=", maxtcs);

/*
 * Dump new MIPS MT state for the core. Does not leave TCs halted.
 * Takes an argument which taken to be a pre-call MVPControl value.
 */

void mips_mt_regdump(unsigned long mvpctl)
{
	unsigned long flags;
	unsigned long vpflags;
	unsigned long mvpconf0;
	int nvpe;
	int ntc;
	int i;
	int tc;
	int curr_tc = -1;
	unsigned long haltval;
	unsigned long tcstatval;
	unsigned long vpe[16][6];
	unsigned long tcreg[16][8];

	local_irq_save(flags);
	/**-------------------------------------------------------------------------------**\
	 * START-CRITICAL START-CRITICAL START-CRITICAL START-CRITICAL START-CRITICAL
	 * Achtung kritischer Abschnitt - hier keine printks (ergo spinlocks  etc.) verwenden
	 * -> Deadlock-Gefahr!
	\*--------------------------------------------------------------------------------**/
	vpflags = dvpe();
	mvpconf0 = read_c0_mvpconf0();
	nvpe = ((mvpconf0 & MVPCONF0_PVPE) >> MVPCONF0_PVPE_SHIFT) + 1;
	ntc = ((mvpconf0 & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
	for (i = 0; i < nvpe; i++) {
		for (tc = 0; tc < ntc; tc++) {
			settc(tc);
			if ((read_tc_c0_tcbind() & TCBIND_CURVPE) == i) {
				vpe[i][0] = read_vpe_c0_vpecontrol();
				vpe[i][1] = read_vpe_c0_vpeconf0();
				vpe[i][2] = read_vpe_c0_status();
				vpe[i][3] = read_vpe_c0_epc();
				vpe[i][4] = read_vpe_c0_cause();
				vpe[i][5] = read_vpe_c0_config7();
				break; /* Next VPE */
			}
		}
	}
	for (tc = 0; tc < ntc; tc++) {
		settc(tc);
		if (read_tc_c0_tcbind() == read_c0_tcbind()) {
			/* Are we dumping ourself?  */
			haltval = 0; /* Then we're not halted, and mustn't be */
			tcstatval = flags; /* And pre-dump TCStatus is flags */
			curr_tc = tc;
		} else {
			haltval = read_tc_c0_tchalt();
			write_tc_c0_tchalt(1);
			tcstatval = read_tc_c0_tcstatus();
		}
		tcreg[tc][0] = tcstatval;
		tcreg[tc][1] = read_tc_c0_tcbind();
		tcreg[tc][2] = read_tc_c0_tcrestart();
		tcreg[tc][3] = haltval;
		tcreg[tc][4] = read_tc_c0_tccontext();
#if defined(CONFIG_AVM_ENHANCED)
		if (!haltval) {
			tcreg[tc][5] = read_tc_gpr_sp();
			tcreg[tc][6] = read_tc_gpr_gp();
			tcreg[tc][7] = read_tc_gpr_ra();
		}
#endif /*--- #if defined(CONFIG_AVM_ENHANCED) ---*/
		if (!haltval) {
			write_tc_c0_tchalt(0);
		}
	}
	evpe(vpflags);
	/*-------------------------------------------------------------------------------*\
	 * END-CRITICAL END-CRITICAL END-CRITICAL END-CRITICAL END-CRITICAL
	\*-------------------------------------------------------------------------------*/
	printk(KERN_EMERG "=== MIPS MT State Dump ===\n");
	printk(KERN_EMERG "-- Global State --\n");
	printk(KERN_EMERG "   MVPControl Read: %08lx\n", vpflags);
	printk(KERN_EMERG "   MVPConf0       : %08lx\n", mvpconf0);
	printk(KERN_EMERG "-- per-VPE State --\n");
	for (i = 0; i < nvpe; i++) {
		printk(KERN_EMERG "VPE %d\n", i);
		printk(KERN_EMERG "   VPEControl   : %08lx\n", vpe[i][0]);
		printk(KERN_EMERG "   VPEConf0     : %08lx\n", vpe[i][1]);
		printk(KERN_EMERG "   VPE%d.Status  : %08lx\n", i, vpe[i][2]);
		printk(KERN_EMERG "   VPE%d.EPC     : %08lx %pS\n", i,
		       vpe[i][3], (void *)vpe[i][3]);
		printk(KERN_EMERG "   VPE%d.Cause   : %08lx\n", i, vpe[i][4]);
		printk(KERN_EMERG "   VPE%d.Config7 : %08lx\n", i, vpe[i][5]);
	}
	printk(KERN_EMERG "-- per-TC State --\n");
	for (tc = 0; tc < ntc; tc++) {
		if (curr_tc == tc) {
			/* Are we dumping ourself?  */
			printk(KERN_EMERG
			       "TC %d (current TC with VPE EPC above)\n",
			       tc);
		} else {
			printk(KERN_EMERG "TC %d\n", tc);
		}
		printk(KERN_EMERG "   TCStatus : %08lx\n", tcreg[tc][0]);
		printk(KERN_EMERG "   TCBind   : %08lx\n", tcreg[tc][1]);
		printk(KERN_EMERG "   TCRestart: %08lx %pS\n", tcreg[tc][2],
		       (void *)tcreg[tc][2]);
		printk(KERN_EMERG "   TCHalt   : %08lx\n", tcreg[tc][3]);
		printk(KERN_EMERG "   TCContext: %08lx\n", tcreg[tc][4]);
#if defined(CONFIG_AVM_ENHANCED)
		if (!tcreg[tc][3]) {
			printk(KERN_EMERG "   SP:    %08lx\n", tcreg[tc][5]);
			printk(KERN_EMERG "   GP:    %08lx\n", tcreg[tc][6]);
			printk(KERN_EMERG "   RA:    %08lx %pS\n", tcreg[tc][7],
			       (void *)tcreg[tc][7]);
		}
#endif /*--- #if defined(CONFIG_AVM_ENHANCED) ---*/
	}
	printk(KERN_EMERG "===========================\n");
	local_irq_restore(flags);
}

static int mt_opt_norps;
static int mt_opt_rpsctl = -1;
static int mt_opt_nblsu = -1;
static int mt_opt_forceconfig7;
static int mt_opt_config7 = -1;

static int __init rps_disable(char *s)
{
	mt_opt_norps = 1;
	return 1;
}
__setup("norps", rps_disable);

static int __init rpsctl_set(char *str)
{
	get_option(&str, &mt_opt_rpsctl);
	return 1;
}
__setup("rpsctl=", rpsctl_set);

static int __init nblsu_set(char *str)
{
	get_option(&str, &mt_opt_nblsu);
	return 1;
}
__setup("nblsu=", nblsu_set);

static int __init config7_set(char *str)
{
	get_option(&str, &mt_opt_config7);
	mt_opt_forceconfig7 = 1;
	return 1;
}
__setup("config7=", config7_set);

/* Experimental cache flush control parameters that should go away some day */
int mt_protiflush;
int mt_protdflush;
int mt_n_iflushes = 1;
int mt_n_dflushes = 1;

static int __init set_protiflush(char *s)
{
	mt_protiflush = 1;
	return 1;
}
__setup("protiflush", set_protiflush);

static int __init set_protdflush(char *s)
{
	mt_protdflush = 1;
	return 1;
}
__setup("protdflush", set_protdflush);

static int __init niflush(char *s)
{
	get_option(&s, &mt_n_iflushes);
	return 1;
}
__setup("niflush=", niflush);

static int __init ndflush(char *s)
{
	get_option(&s, &mt_n_dflushes);
	return 1;
}
__setup("ndflush=", ndflush);

static unsigned int itc_base;

static int __init set_itc_base(char *str)
{
	get_option(&str, &itc_base);
	return 1;
}

__setup("itcbase=", set_itc_base);

void mips_mt_set_cpuoptions(void)
{
	unsigned int oconfig7 = read_c0_config7();
	unsigned int nconfig7 = oconfig7;

	if (mt_opt_norps) {
		printk("\"norps\" option deprecated: use \"rpsctl=\"\n");
	}
	if (mt_opt_rpsctl >= 0) {
		printk("34K return prediction stack override set to %d.\n",
			mt_opt_rpsctl);
		if (mt_opt_rpsctl)
			nconfig7 |= (1 << 2);
		else
			nconfig7 &= ~(1 << 2);
	}
	if (mt_opt_nblsu >= 0) {
		printk("34K ALU/LSU sync override set to %d.\n", mt_opt_nblsu);
		if (mt_opt_nblsu)
			nconfig7 |= (1 << 5);
		else
			nconfig7 &= ~(1 << 5);
	}
	if (mt_opt_forceconfig7) {
		printk("CP0.Config7 forced to 0x%08x.\n", mt_opt_config7);
		nconfig7 = mt_opt_config7;
	}
	if (oconfig7 != nconfig7) {
		__asm__ __volatile("sync");
		write_c0_config7(nconfig7);
		ehb();
		printk("Config7: 0x%08x\n", read_c0_config7());
	}

	/* Report Cache management debug options */
	if (mt_protiflush)
		printk("I-cache flushes single-threaded\n");
	if (mt_protdflush)
		printk("D-cache flushes single-threaded\n");
	if (mt_n_iflushes != 1)
		printk("I-Cache Flushes Repeated %d times\n", mt_n_iflushes);
	if (mt_n_dflushes != 1)
		printk("D-Cache Flushes Repeated %d times\n", mt_n_dflushes);

	if (itc_base != 0) {
		/*
		 * Configure ITC mapping.  This code is very
		 * specific to the 34K core family, which uses
		 * a special mode bit ("ITC") in the ErrCtl
		 * register to enable access to ITC control
		 * registers via cache "tag" operations.
		 */
		unsigned long ectlval;
		unsigned long itcblkgrn;

		/* ErrCtl register is known as "ecc" to Linux */
		ectlval = read_c0_ecc();
		write_c0_ecc(ectlval | (0x1 << 26));
		ehb();
#define INDEX_0 (0x80000000)
#define INDEX_8 (0x80000008)
		/* Read "cache tag" for Dcache pseudo-index 8 */
		cache_op(Index_Load_Tag_D, INDEX_8);
		ehb();
		itcblkgrn = read_c0_dtaglo();
		itcblkgrn &= 0xfffe0000;
		/* Set for 128 byte pitch of ITC cells */
		itcblkgrn |= 0x00000c00;
		/* Stage in Tag register */
		write_c0_dtaglo(itcblkgrn);
		ehb();
		/* Write out to ITU with CACHE op */
		cache_op(Index_Store_Tag_D, INDEX_8);
		/* Now set base address, and turn ITC on with 0x1 bit */
		write_c0_dtaglo((itc_base & 0xfffffc00) | 0x1 );
		ehb();
		/* Write out to ITU with CACHE op */
		cache_op(Index_Store_Tag_D, INDEX_0);
		write_c0_ecc(ectlval);
		ehb();
		printk("Mapped %ld ITC cells starting at 0x%08x\n",
			((itcblkgrn & 0x7fe00000) >> 20), itc_base);
	}
}

/*
 * Function to protect cache flushes from concurrent execution
 * depends on MP software model chosen.
 */

void mt_cflush_lockdown(void)
{
	/* FILL IN VSMP and AP/SP VERSIONS HERE */
}

void mt_cflush_release(void)
{
	/* FILL IN VSMP and AP/SP VERSIONS HERE */
}

struct class *mt_class;

static int __init mt_init(void)
{
	struct class *mtc;

	mtc = class_create(THIS_MODULE, "mt");
	if (IS_ERR(mtc))
		return PTR_ERR(mtc);

	mt_class = mtc;

	return 0;
}

subsys_initcall(mt_init);
