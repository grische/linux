/*------------------------------------------------------------------------------------------*\
 *
 *   Copyright (C) 2016 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   checker-function
\*------------------------------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#define __AVM_OPTION_CHECK(options, mips_option, cpu_option, fmt, mips_option_str, cpu_option_str) \
	if (!!((options) & (mips_option)) != !!(cpu_option))                           \
		printk(KERN_ERR                                                        \
		       "%s: ERROR: option missmatch cpu_has_... : 0x"fmt" & %s != %s\n", \
		       __func__, options, mips_option_str, cpu_option_str);

#define AVM_OPTION_CHECK_I(options, mips_option, cpu_option) \
	__AVM_OPTION_CHECK(options, mips_option, cpu_option, "%04x", #mips_option, #cpu_option)

#define AVM_OPTION_CHECK_L(options, mips_option, cpu_option) \
	__AVM_OPTION_CHECK(options, mips_option, cpu_option, "%08lx", #mips_option, #cpu_option)

#define AVM_OPTION_CHECK_LL(options, mips_option, cpu_option) \
	__AVM_OPTION_CHECK(options, mips_option, cpu_option, "%016llx", #mips_option, #cpu_option)

/*--------------------------------------------------------------------------------*\
 * aus Performance-Gruenden (->Compiler-Optimierung wirksam) werden diverse CPU-Features
 * (z.B. cpu_has_llsc) fest gesetzt in cpu-features-override.h
 *  hier wird gecheckt ob diese auch korrekt gesetzt
\*--------------------------------------------------------------------------------*/
void avm_check_cpu_features(void)
{
	struct cpuinfo_mips *c = &cpu_data[0];

	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_32FPR, cpu_has_32fpr);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_3K_CACHE, cpu_has_3k_cache);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_4KEX, cpu_has_4kex);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_4K_CACHE, cpu_has_4k_cache);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_BADINSTR, cpu_has_badinstr);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_BADINSTRP, cpu_has_badinstrp);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_BP_GHIST, cpu_has_bp_ghist);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_COUNTER, cpu_has_counter);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_CDMM, cpu_has_cdmm);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_CTXTC, cpu_has_contextconfig);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_DIVEC, cpu_has_divec);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_EJTAG, cpu_has_ejtag);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_EBASE_WG, cpu_has_ebase_wg);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_EVA, cpu_has_eva);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_FPU, cpu_has_fpu);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_HTW, cpu_has_htw);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_LPA, cpu_has_lpa);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_LLSC, cpu_has_llsc);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_MAAR, cpu_has_maar);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_MCHECK, cpu_has_mcheck);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_MICROMIPS, cpu_has_mmips);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_MVH, cpu_has_mvh);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_NOFPUEX, cpu_has_nofpuex);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_PCI, cpu_has_perf_cntr_intr_bit);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_PERF, cpu_has_perf);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_RIXI, cpu_has_rixi);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_RIXIEX, cpu_has_rixiex);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_SEGMENTS, cpu_has_segments);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_SP, cpu_has_small_pages);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_TLB, cpu_has_tlb);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_TLBINV, cpu_has_tlbinv);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_TX39_CACHE, cpu_has_tx39_cache);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_ULRI, cpu_has_userlocal);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_VCE, cpu_has_vce);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_VEIC, cpu_has_veic);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_VINT, cpu_has_vint);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_VP, cpu_has_vp);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_WATCH, cpu_has_watch);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_DSP, cpu_has_dsp);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_DSP2P, cpu_has_dsp2);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_DSP3, cpu_has_dsp3);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_MDMX, cpu_has_mdmx);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_MIPS16, cpu_has_mips16);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_MIPSMT, cpu_has_mipsmt);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_MSA, cpu_has_msa);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_SMARTMIPS, cpu_has_smartmips);
	AVM_OPTION_CHECK_L(c->ases, MIPS_ASE_VZ, cpu_has_vz);

	printk(KERN_ERR
	       "%s: mips-options (cache flags preliminary!): 0x%016llx ases 0x%08lx\n",
	       __func__, c->options, c->ases);
	printk(KERN_ERR
	       "%s: (preliminary flags: MIPS_CPU_CACHE_CDEX_P MIPS_CPU_PREFETCH MIPS_CPU_INCLUSIVE_CACHES MIPS_CPU_CACHE_CDEX_S)\n",
	       __func__);
}

void avm_check_isa_features(void)
{
	struct cpuinfo_mips *c = &cpu_data[0];

	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_II, cpu_has_mips_2);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_III, cpu_has_mips_3);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_IV, cpu_has_mips_4);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_V, cpu_has_mips_5);

	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_M32R1, cpu_has_mips32r1);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_M32R2, cpu_has_mips32r2);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_M32R6, cpu_has_mips32r6);

	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_M64R1, cpu_has_mips64r1);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_M64R2, cpu_has_mips64r2);
	AVM_OPTION_CHECK_I(c->isa_level, MIPS_CPU_ISA_M64R6, cpu_has_mips64r6);

	printk(KERN_ERR "%s: isa_level 0x%08x\n", __func__, c->isa_level);
}

void avm_check_pcache_features(void)
{
	struct cpuinfo_mips *c = &cpu_data[0];

	AVM_OPTION_CHECK_I(c->icache.flags, MIPS_CACHE_VTAG, cpu_has_vtag_icache);
	AVM_OPTION_CHECK_I(c->icache.flags, MIPS_CACHE_IC_F_DC, cpu_has_ic_fills_f_dc);
	AVM_OPTION_CHECK_I(c->icache.flags, MIPS_IC_SNOOPS_REMOTE, cpu_icache_snoops_remote_store);

	AVM_OPTION_CHECK_I(c->dcache.flags, MIPS_CACHE_ALIASES, cpu_has_dc_aliases);
	AVM_OPTION_CHECK_I(c->dcache.flags, MIPS_CACHE_PINDEX, cpu_has_pindexed_dcache);

	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_CACHE_CDEX_P, cpu_has_cache_cdex_p);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_PREFETCH, cpu_has_prefetch);
	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_INCLUSIVE_CACHES, cpu_has_inclusive_pcaches);

	printk(KERN_ERR
	       "%s: mips-options (preliminary MIPS_CPU_CACHE_CDEX_S flag): 0x%016llx icache.flags 0x%08x dcache.flags 0x%08x\n",
	       __func__, c->options, c->icache.flags, c->dcache.flags);
}

void avm_check_scache_features(void)
{
	struct cpuinfo_mips *c = &cpu_data[0];

	AVM_OPTION_CHECK_LL(c->options, MIPS_CPU_CACHE_CDEX_S, cpu_has_cache_cdex_s);

	printk(KERN_ERR "%s: mips-options: 0x%016llx\n", __func__, c->options);
}
