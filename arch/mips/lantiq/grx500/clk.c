#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/export.h>
#include <lantiq_soc.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#include "../clk.h"
#include <asm/mach_avm.h>

void __iomem *ltq_cgu_membase;

#define MS(_v, _f, _p)  (((_v) & (_f)) >> _p)

//! Register Offset (relative)
#define CGU_PLL0A_CFG1 0x8
#define CGU_PLL0A_CFG1_CPU_CLKM_SEL_MASK 0x20000000u

//! Field PLL_CLK4 - PLL Clock Output 4
#define CGU_PLL0A_CFG1_PLL_CLK4_POS 12
//! Field PLL_CLK4 - PLL Clock Output 4
#define CGU_PLL0A_CFG1_PLL_CLK4_MASK 0xF000u

//! Register Offset (relative)
#define CGU_PLL0B_CFG1 0x38

//! Field PLL_CLK3 - PLL Clock Output 3
#define CGU_PLL0B_CFG1_PLL_CLK3_POS 8
//! Field PLL_CLK3 - PLL Clock Output 3
#define CGU_PLL0B_CFG1_PLL_CLK3_MASK 0xF00u

//! Field PLL_CLK4 - PLL Clock Output 4
#define CGU_PLL0B_CFG1_PLL_CLK4_POS 12
//! Field PLL_CLK4 - PLL Clock Output 4
#define CGU_PLL0B_CFG1_PLL_CLK4_MASK 0xF000u

static u32 ltq_grx500_clk_div_to_div(u32 clk_div)
{
	u32 div = 2;

	if (clk_div < 6)
		div = clk_div + 1;
	else {
		switch (clk_div) {
		case 6:
			div = 8;
			break;
		case 7:
			div = 10;
			break;
		case 8:
			div = 12;
			break;
		case 9:
			div = 16;
			break;
		case 10:
			div = 20;
			break;
		case 11:
			div = 24;
			break;
		case 12:
			div = 32;
			break;
		case 13:
			div = 40;
			break;
		case 14:
			div = 48;
			break;
		case 15:
			div = 64;
			break;
		default:
			div = 2;
			break;
		}
	}
	return div;
}

/**
 */
static unsigned long ltq_grx500_cpu_hz(void)
{
#ifdef CONFIG_USE_EMULATOR
	return xrx500_cpu_clk;
#else
	unsigned long pllclk;
	u32 val;
	u32 clk4_div;
	u32 div;

	val = ltq_cgu_r32(CGU_PLL0A_CFG1);

	if (val & CGU_PLL0A_CFG1_CPU_CLKM_SEL_MASK) {
		pllclk = CLOCK_2000M;
		clk4_div = MS(ltq_cgu_r32(CGU_PLL0B_CFG1),
				CGU_PLL0B_CFG1_PLL_CLK4_MASK,
				CGU_PLL0B_CFG1_PLL_CLK4_POS);
	} else {
		pllclk = CLOCK_2400M;
		clk4_div = MS(ltq_cgu_r32(CGU_PLL0A_CFG1),
				CGU_PLL0A_CFG1_PLL_CLK4_MASK,
				CGU_PLL0A_CFG1_PLL_CLK4_POS);
	}
	div = ltq_grx500_clk_div_to_div(clk4_div);
	return (unsigned long)(pllclk / div);
#endif
}

/**
 */
static unsigned long ltq_grx500_fpi_hz(void)
{
#ifdef CONFIG_USE_EMULATOR
	return ltq_grx500_cpu_hz() / 4;
#else
	return CLOCK_200M;
#endif
}

/**
 */
static unsigned long ltq_grx500_ddr_hz(void)
{
	u32 div;
	u32 clk3_div;
	unsigned long pllclk = CLOCK_2000M;

	clk3_div = MS(ltq_cgu_r32(CGU_PLL0B_CFG1),
			CGU_PLL0B_CFG1_PLL_CLK3_MASK,
			CGU_PLL0B_CFG1_PLL_CLK3_POS);
	div = ltq_grx500_clk_div_to_div(clk3_div);
	return (unsigned long)(2 * (pllclk / div));
}


/*
 * compatibility with avm-sdk
 */
unsigned int lantiq_get_clock(enum _avm_clock_id clock_id)
{
	switch (clock_id) {
	case avm_clock_id_cpu:
		return ltq_grx500_cpu_hz();
	case avm_clock_id_fpi:
		return ltq_grx500_fpi_hz();
	case avm_clock_id_ddr:
		return ltq_grx500_ddr_hz();
	case avm_clock_id_usb:
		//TODO
		return 0;
	case avm_clock_id_pci:
		//Fixed to 100 MHz laut Datenblatt
		return CLOCK_100M;
	case avm_clock_id_ephy:
		//Fixed to 25 MHz laut Datenblatt
		return CLOCK_25M;
	default:
		break;
	}
	return 0;
}

EXPORT_SYMBOL(lantiq_get_clock);

int __init ltq_cgu_init(void)
{
	struct device_node *np_cgu;
	struct resource res_cgu;

	np_cgu = of_find_compatible_node(NULL, NULL, "lantiq,cgu-grx500");
	/* check if all the cgu register ranges are available */
	if (!np_cgu )
		panic("Failed to load cgu nodes from devicetree");

	if (of_address_to_resource(np_cgu, 0, &res_cgu))
		panic("Failed to get cgu resources");

	if (!(request_mem_region(res_cgu.start, resource_size(&res_cgu), res_cgu.name)))
		pr_err("%s Failed to request cgu resources", __func__);

	ltq_cgu_membase = ioremap_nocache(res_cgu.start,
					  resource_size(&res_cgu));

	if (!ltq_cgu_membase)
		panic("Failed to remap cgu resources");

	pr_debug("%s cpu=%u fpi=%u ddr=%u Hz\n", __func__,
			lantiq_get_clock(avm_clock_id_cpu),
			lantiq_get_clock(avm_clock_id_fpi),
			lantiq_get_clock(avm_clock_id_ddr));
	return 0;
}
arch_initcall(ltq_cgu_init);
