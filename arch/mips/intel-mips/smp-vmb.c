// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2009-2015 Lantiq Deutschland GmbH
 * Copyright (C) 2016 Intel Corporation.
 *
 * SMP bring-up for Intel/Lantiq xRX500 (GRX350, GRX550) through the firmware
 * mailbox its boot loader provides.
 *
 * All four VPEs — two interAptiv cores with two VPEs each — are already
 * running when Linux is entered, so releasing a sibling is a message rather
 * than a reset: write where to jump, what stack to use and what to put in $gp
 * into that CPU's mailbox slot, then ring the doorbell the firmware listens
 * on. The CPU arrives at smp_bootstrap like any other. Nothing here touches
 * the Coherence Manager or the CPC.
 */

#define pr_fmt(fmt) "vmb-smp: " fmt

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/smp.h>
#include <linux/string.h>

#include <asm/addrspace.h>
#include <asm/cpu-info.h>
#include <asm/mips-cps.h>
#include <asm/mipsmtregs.h>
#include <asm/mips_mt.h>
#include <asm/processor.h>
#include <asm/smp-ops.h>
#include <asm/time.h>

#include "common.h"

/*
 * The mailbox lives at physical 0x20000000 -- the first page of RAM -- and is
 * reached through its uncached alias, which is what the firmware writes it
 * through too. The device tree reserves the page (and the rest of the
 * firmware's low memory) as no-map, because MIPS allocates memblock bottom-up
 * during early boot and would otherwise put the unflattened device tree
 * straight on top of it.
 */
#define VMB_MAILBOX_PHYS	0x20000000

#define VMB_MAX_CPUS		4
#define VMB_VPES_PER_CORE	2

/* Message identifiers, Linux to firmware. Only the launch is used here. */
#define VMB_CPU_START		0x00000001

#define IBL_IN_WAIT		0x00000008

/* Firmware to Linux. */
struct vmb_fw_msg {
	u32 status;
	u32 priv_info;
};

struct vmb_cpu_launch {
	u32 start_addr;
	u32 sp;
	u32 gp;
	u32 a0;
	u32 eva;
	u32 mt_group;
	u32 yield_res;
	u32 priv_info;
};

struct vmb_tc_launch {
	u32 tc_num;
	u32 mt_group;
	u32 start_addr;
	u32 sp;
	u32 gp;
	u32 a0;
	u32 state;
	u32 priv_info;
};

/*
 * Linux to firmware. The TC descriptors are part of the ABI and have to be
 * present and zeroed even though we never run more than one thread context
 * per VPE.
 */
struct vmb_msg {
	u32 msg_id;
	struct vmb_cpu_launch cpu_launch;
	struct vmb_tc_launch tc_launch[4];
	u32 tc_num;
};

/* One 176-byte slot per CPU, the firmware's half first. */
struct vmb_slot {
	struct vmb_fw_msg fw;
	struct vmb_msg cmd;
};

static struct vmb_slot *vmb_mailbox;

/* GIC shared interrupt that releases each CPU, from its cpu node. */
static unsigned int vmb_fw_ipi[VMB_MAX_CPUS];

/*
 * How long to wait for a released CPU to reach set_cpu_online(). Bounded on
 * purpose: a firmware that never answers must fail the bring-up rather than
 * hang the boot.
 */
#define VMB_LAUNCH_TIMEOUT_MS	2000

static struct vmb_slot *vmb_map_mailbox(void)
{
	BUILD_BUG_ON(sizeof(struct vmb_slot) != 0xb0);

	return (struct vmb_slot *)KSEG1ADDR(VMB_MAILBOX_PHYS);
}

/* Is a boot loader parked in this CPU's slot, waiting to be told what to do? */
static bool vmb_cpu_parked(unsigned int cpu)
{
	return READ_ONCE(vmb_mailbox[cpu].fw.status) == IBL_IN_WAIT;
}

static int __init vmb_read_fw_ipi(unsigned int cpu, unsigned int *hwirq)
{
	struct device_node *np;
	u32 val;
	int err;

	np = of_get_cpu_node(cpu, NULL);
	if (!np)
		return -ENODEV;

	err = of_property_read_u32(np, "vmb-fw-ipi", &val);
	of_node_put(np);
	if (err)
		return err;

	*hwirq = val;
	return 0;
}

static void __init vmb_smp_setup(void)
{
	unsigned int cpu, hwirq;
	unsigned int ncpus = 1;

	/*
	 * CPU 0 is the VPE this code runs on. The mailbox ABI numbers slots
	 * core * 2 + vpe, so the slot index is the topology.
	 */
	cpu_set_cluster(&cpu_data[0], 0);
	cpu_set_core(&cpu_data[0], 0);
	cpu_set_vpe_id(&cpu_data[0], 0);
	smp_num_siblings = VMB_VPES_PER_CORE;
	__cpu_number_map[0] = 0;
	__cpu_logical_map[0] = 0;

	for (cpu = 1; cpu < min_t(unsigned int, VMB_MAX_CPUS, NR_CPUS); cpu++) {
		if (!vmb_cpu_parked(cpu)) {
			pr_warn("CPU%u: no boot loader waiting (status 0x%08x)\n",
				cpu, READ_ONCE(vmb_mailbox[cpu].fw.status));
			break;
		}

		if (vmb_read_fw_ipi(cpu, &hwirq)) {
			pr_warn("CPU%u: parked, but no vmb-fw-ipi in the device tree\n",
				cpu);
			break;
		}

		vmb_fw_ipi[cpu] = hwirq;

		cpu_set_cluster(&cpu_data[cpu], 0);
		cpu_set_core(&cpu_data[cpu], cpu / VMB_VPES_PER_CORE);
		cpu_set_vpe_id(&cpu_data[cpu], cpu % VMB_VPES_PER_CORE);

		set_cpu_possible(cpu, true);
		set_cpu_present(cpu, true);
		__cpu_number_map[cpu] = cpu;
		__cpu_logical_map[cpu] = cpu;

		ncpus++;
	}

	/*
	 * Stopping at the first CPU that is not available, rather than skipping
	 * it, is deliberate: prefill_possible_map() in arch/mips/kernel/setup.c
	 * compacts the possible mask to the first num_possible_cpus() entries
	 * straight after this returns, so a hole would silently become a
	 * different CPU.
	 */
	pr_info("VPE topology {%u,%u} total %u, %u released by the firmware\n",
		VMB_VPES_PER_CORE, VMB_VPES_PER_CORE, VMB_MAX_CPUS, ncpus);

#ifdef CONFIG_MIPS_MT_FPAFF
	/* If we have an FPU, enroll ourselves in the FPU-full mask */
	if (cpu_has_fpu)
		cpumask_set_cpu(0, &mt_fpu_cpumask);
#endif /* CONFIG_MIPS_MT_FPAFF */
}

static void __init vmb_prepare_cpus(unsigned int max_cpus)
{
	unsigned int cca = read_c0_config() & CONF_CM_CMASK;

	/*
	 * Report the CCA rather than change it: the boot chain establishes
	 * coherency before Linux runs, and the vendor kernel only reads it
	 * back. 4 is CWBE and 5 is CWB.
	 */
	if (cca == 4 || cca == 5)
		pr_info("CCA %u is coherent, multi-core is fine\n", cca);
	else
		pr_warn("CCA %u is not coherent, multi-core will not work\n",
			cca);

	if (IS_ENABLED(CONFIG_MIPS_MT))
		mips_mt_set_cpuoptions();
}

static int vmb_boot_secondary(int cpu, struct task_struct *idle)
{
	struct vmb_slot *slot = &vmb_mailbox[cpu];
	unsigned long timeout;

	/*
	 * The doorbell is a GIC shared interrupt, rung by writing an edge into
	 * GIC_SH_WEDGE. boot_secondary runs long after the irqchip has probed,
	 * so the mapping is there, but check rather than fault.
	 */
	if (!mips_gic_present()) {
		pr_err("CPU%d: no GIC, cannot ring the doorbell\n", cpu);
		return -ENODEV;
	}

	memset(&slot->cmd, 0, sizeof(slot->cmd));

	slot->cmd.msg_id = VMB_CPU_START;
	/*
	 * A plain cached kernel virtual address: the released VPE enters
	 * through the same segmentation this kernel was linked for, so there
	 * is no low-memory bootstrap stub to place.
	 */
	slot->cmd.cpu_launch.start_addr = (unsigned long)&smp_bootstrap;
	slot->cmd.cpu_launch.sp = __KSTK_TOS(idle);
	slot->cmd.cpu_launch.gp = (unsigned long)task_thread_info(idle);
	slot->cmd.cpu_launch.a0 = 0;

	/* The command must be in the mailbox before the doorbell rings. */
	wmb();

	write_gic_wedge(GIC_WEDGE_RW | vmb_fw_ipi[cpu]);

	/* Wait for the CPU to arrive before clearing the command. */
	timeout = jiffies + msecs_to_jiffies(VMB_LAUNCH_TIMEOUT_MS);
	while (!cpu_online(cpu)) {
		if (time_after(jiffies, timeout)) {
			pr_err("CPU%d: no answer %u ms after doorbell %u (status 0x%08x)\n",
			       cpu, VMB_LAUNCH_TIMEOUT_MS, vmb_fw_ipi[cpu],
			       READ_ONCE(slot->fw.status));
			/*
			 * Leave the command in place: it is the only evidence
			 * of what the firmware was asked to do.
			 */
			return -EIO;
		}
		cpu_relax();
	}

	memset(&slot->cmd, 0, sizeof(slot->cmd));

	return 0;
}

static void vmb_init_secondary(void)
{
	unsigned int vpe;

	write_c0_errorepc(0);

	/*
	 * The firmware left this VPE's identity where the mailbox slot said it
	 * would be. Say so if it did not -- getting core or VPE wrong here
	 * misroutes every interrupt this CPU is meant to take, because the GIC
	 * derives its VP identifier from exactly these two fields.
	 */
	if (cpu_has_mipsmt) {
		vpe = read_c0_tcbind() & TCBIND_CURVPE;
		if (vpe != cpu_vpe_id(&current_cpu_data) ||
		    get_ebase_cpunum() / VMB_VPES_PER_CORE !=
		    cpu_core(&current_cpu_data))
			pr_warn("CPU%d: firmware placed us on core %u VPE %u, expected core %u VPE %u\n",
				smp_processor_id(),
				get_ebase_cpunum() / VMB_VPES_PER_CORE, vpe,
				cpu_core(&current_cpu_data),
				cpu_vpe_id(&current_cpu_data));
	}

	if (cpu_has_veic)
		clear_c0_status(ST0_IM);
	else
		change_c0_status(ST0_IM, STATUSF_IP2 | STATUSF_IP3 |
					 STATUSF_IP4 | STATUSF_IP5 |
					 STATUSF_IP6 | STATUSF_IP7);
}

static void vmb_smp_finish(void)
{
	write_c0_compare(read_c0_count() + (8 * mips_hpt_frequency / HZ));

#ifdef CONFIG_MIPS_MT_FPAFF
	/* If we have an FPU, enroll ourselves in the FPU-full mask */
	if (cpu_has_fpu)
		cpumask_set_cpu(smp_processor_id(), &mt_fpu_cpumask);
#endif /* CONFIG_MIPS_MT_FPAFF */

	local_irq_enable();
}

static const struct plat_smp_ops vmb_smp_ops = {
	.smp_setup		= vmb_smp_setup,
	.prepare_cpus		= vmb_prepare_cpus,
	.boot_secondary		= vmb_boot_secondary,
	.init_secondary		= vmb_init_secondary,
	.smp_finish		= vmb_smp_finish,
	.send_ipi_single	= mips_smp_send_ipi_single,
	.send_ipi_mask		= mips_smp_send_ipi_mask,
};

int __init register_vmb_smp_ops(void)
{
	unsigned int cpu;

	/*
	 * Two VPEs per core is the whole point, and without MIPS MT SMP the
	 * kernel refuses to distinguish them: cpu_vpe_id() is compiled to
	 * return 0, so both VPEs of a core would share one GIC VP identifier
	 * and every interrupt aimed at the odd one would land on the even one.
	 * Refuse loudly rather than boot into that.
	 */
	if (!IS_ENABLED(CONFIG_MIPS_MT_SMP)) {
		pr_warn("needs CONFIG_MIPS_MT_SMP=y to tell the VPEs of a core apart\n");
		return -ENODEV;
	}

	vmb_mailbox = vmb_map_mailbox();

	/*
	 * Read the mailbox before anything can have overwritten it. prom_init()
	 * runs ahead of arch_mem_init(), so no allocation has happened yet even
	 * on a kernel whose device tree forgot to reserve the page.
	 */
	for (cpu = 1; cpu < VMB_MAX_CPUS; cpu++) {
		if (vmb_cpu_parked(cpu)) {
			register_smp_ops(&vmb_smp_ops);
			return 0;
		}
	}

	pr_warn("no parked CPU in the mailbox at 0x%08x; slots read 0x%08x 0x%08x 0x%08x\n",
		VMB_MAILBOX_PHYS,
		READ_ONCE(vmb_mailbox[1].fw.status),
		READ_ONCE(vmb_mailbox[2].fw.status),
		READ_ONCE(vmb_mailbox[3].fw.status));

	return -ENODEV;
}
