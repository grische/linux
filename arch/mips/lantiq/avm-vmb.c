/*
 * Copyright (C) 2007  MIPS Technologies, Inc.
 *	All rights reserved.

 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Arbitrary Monitor interface
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/addrspace.h>
#include <asm/mips-boards/launch.h>
#include <asm/mipsmtregs.h>

#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <asm/cacheflush.h>
#include <asm/smp-ops.h>
#include <asm/traps.h>
#include <asm/fw/fw.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <asm/ltq_vmb.h>
#include <linux/irqchip/mips-gic.h>

#include <asm/mach_avm.h>

#define AVM_MAX_VPE 4

struct avm_vmb_status {
	wait_queue_head_t wq;
	int ipi_irq;
	volatile uint8_t cpu_active;
} vpe[AVM_MAX_VPE];

static int __init avm_vmb_cpu_active(unsigned int hcpu) {
	unsigned long tcbind = read_c0_tcbind();

	avm_register_cpuid("LINUX", hcpu, get_ebase_cpunum() / 2, (tcbind & TCBIND_CURTC) >> TCBIND_CURTC_SHIFT);
	vpe[hcpu].cpu_active = 1;
	wake_up_interruptible(&vpe[hcpu].wq);

	return 0;
}


static int __init avm_init_cpu(int cpu) {
	struct device_node *np;
	char name[16];
	int ret;

	BUG_ON(cpu == 0);

	memset(name, '\0', sizeof(name));
	sprintf(name, "%s%d", "/cpus/cpu@", cpu);

	np = of_find_node_by_path(name);
	if (!np)
		return 0;

	ret = of_property_read_u32(np, "vmb-fw-ipi", &vpe[cpu].ipi_irq);
	if (ret < 0)
		return 0;

	init_waitqueue_head(&vpe[cpu].wq);
	vpe[cpu].cpu_active = 0;
	set_cpu_present(cpu, true);

	return 1;
}

static int __init avm_vmb_init(void)
{
	init_waitqueue_head(&vpe[0].wq);
	vpe[0].cpu_active = 1;

	cpuhp_setup_state(CPUHP_AP_INTEL_VMB_ONLINE, "AVM_VMB_ON_LINE", avm_vmb_cpu_active, NULL);

	return 0;
}
early_initcall(avm_vmb_init);

int amon_cpu_avail(int cpu)
{
	struct FW_vmb_msg_t *fw_msg = ((void *)CPU_LAUNCH) + (vmb_msg_size * cpu);

	if (cpu < 0 || cpu >= AVM_MAX_VPE)
		return 0;

	if (fw_msg->status != IBL_IN_WAIT)
		return 0;

	return avm_init_cpu(cpu);
}

void amon_cpu_start(int cpu, unsigned long pc, unsigned long sp,
		    unsigned long gp, unsigned long a0)
{
	struct VMB_fw_msg_t *vmb_msg = ((void *)CPU_LAUNCH) + (vmb_msg_size * cpu) + sizeof(struct FW_vmb_msg_t);

	BUG_ON(vpe[cpu].cpu_active);

	memset(vmb_msg, 0, sizeof(struct VMB_fw_msg_t));

	vmb_msg->msg_id = VMB_CPU_START;
#if defined(CONFIG_VMB_LAUNCH_KSEG1)
	vmb_msg->cpu_launch.start_addr = CKSEG1ADDR(pc);
#else
	vmb_msg->cpu_launch.start_addr = pc;
#endif
	vmb_msg->cpu_launch.sp = sp;
	vmb_msg->cpu_launch.gp = (unsigned long)gp;
	vmb_msg->cpu_launch.a0 = a0;

	gic_send_ipi_simple(vpe[cpu].ipi_irq, 0);

	/* Wait for timeout */
	wait_event_interruptible(vpe[cpu].wq, (vpe[cpu].cpu_active == 1));
	memset(vmb_msg, 0, sizeof(struct VMB_fw_msg_t));
}
