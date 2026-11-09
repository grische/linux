// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2014-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <avm/watchdog/watchdog.h>
#include "avm_sammel.h"
#include <avm/event/event.h>
#include "ar7wdt_private.h"

#include <bspchip.h>

#define RTK_WDG_EN()   (REG32(BSP_WDTCTRLR) |= WDT_E)
#define RTK_WDG_DIS()  (REG32(BSP_WDTCTRLR) &= (~WDT_E))
#define RTK_WDG_KICK() (REG32(BSP_WDCNTRR) = REG32(BSP_WDCNTRR) | WDT_KICK)

unsigned int wdg_timeout_ph2 = 5; // 5 second timeout after phase one timeout (chip reset)
unsigned int wdg_clk_sc = 3;
unsigned int wdg_rest_mode = 0;

//for LX 200M Hz
void set_rtk_wdt_ph1_threshold(int sec)
{
	unsigned int reg = 0;

	reg = REG32(BSP_WDTCTRLR);
	reg &= ~(WDT_PH12_TO_MSK << WDT_PH1_TO_SHIFT);

	reg &= ~(WDT_PH12_TO_MSK << WDT_PH2_TO_SHIFT);
	reg |= (wdg_timeout_ph2 << WDT_PH2_TO_SHIFT);

	reg |= (wdg_clk_sc << WDT_CLK_SC_SHIFT);
	reg |= wdg_rest_mode;

	if (sec == 0) {
		REG32(BSP_WDTCTRLR) = reg | (31 << WDT_PH1_TO_SHIFT);
	} else {
		REG32(BSP_WDTCTRLR) = reg | (sec << WDT_PH1_TO_SHIFT);
	}
}

static irqreturn_t watch_dog_timer_phase1_timeout_isr(int irq, void *dev_instance)
{
	panic("Watchdog expired!");
}

void ar7wdt_hw_init(void)
{
	int err = 0;
	err = request_irq(BSP_WDT_PH1TO_IRQ, watch_dog_timer_phase1_timeout_isr, 0, "Watch Dog Timer Phase1 Timeout", NULL);

	if (err) {
		pr_err("requesting wdt timeout irq failed\n");
		return;
	}

	set_rtk_wdt_ph1_threshold(31);
	RTK_WDG_EN();
}

void ar7wdt_hw_deinit(void)
{
	RTK_WDG_DIS();
}

void ar7wdt_hw_trigger(void)
{
	RTK_WDG_KICK();
}
EXPORT_SYMBOL(ar7wdt_hw_trigger);
