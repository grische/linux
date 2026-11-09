/*
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  Copyright (C) 2014 Nagaraj Suresh<suresh.nagaraj@lantiq.com>
 */

#define pr_fmt(fmt) "ngi: " fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

#include <asm/addrspace.h>

#include <lantiq.h>
#include <lantiq_soc.h>
//#include <lantiq_irq.h>
#include <asm/uaccess.h>

#include "ngi_reg.h"
#include "ngi.h"
#include "ngi_map.h"

/*
 * WW: to make the IRQ work as intended
 * until a decent conversion to irq domain
 * controller driver
 */
#define BOOTCORE_LEGACY_IRQ
#include <asm/setup.h>
#include <grx500_bootcore_interrupt.h>
#include <linux/version.h>

#define NGI_MAX_TA 25
#define NGI_MAX_IA 25
#define NGI_MAX_GROUP_VALUE 64
#define NGI_DEF_WAIT_CYCLES 100000
#define NGI_REGION_SIZE 0x20
#define NGI_COMPONENT_SIZE 0x400
#define NGI_REGION(x) ((x)*NGI_REGION_SIZE)

#define NGI_F_IA 1
#define NGI_F_TA 2
#define NGI_F_CORE 4

/* Helper macros to build permission register values */
#define PM_FROM_KB(x) ((x) << 10)
#define PM_TO_KB(x) ((x) >> 10)
#define PM_SIZE_TO_KB(x) (x == 0 ? 0 : ilog2(PM_TO_KB(x)) + 1)
#define PM_SIZE_FROM_KB(x) (x == 0 ? 0 : PM_FROM_KB(1 << ((x)-1)))
#define PM_MASK(x) ((1 << (x)) - 1)
#define PM_SHIFT_MATCH_ADDR_SPACE 0
#define PM_SIZE_MATCH_ADDR_SPACE 3
#define PM_MASK_MATCH_ADDR_SPACE (PM_MASK(PM_SIZE_MATCH_ADDR_SPACE))
#define PM_SHIFT_MATCH_SIZE 3
#define PM_SIZE_MATCH_SIZE 5
#define PM_MASK_MATCH_SIZE (PM_MASK(PM_SIZE_MATCH_SIZE))
#define PM_SHIFT_MATCH_LEVEL 9
#define PM_SIZE_MATCH_LEVEL 1
#define PM_MASK_MATCH_LEVEL (PM_MASK(PM_SIZE_MATCH_LEVEL))
#define PM_SHIFT_MATCH_BASE_ADDR 10

static struct proc_dir_entry *g_ngi_root_proc;
static int g_ngi_root_proc_once;

/* Error handling */
struct ngi_err_info {
	char *agent_name;
	u32 offset;
};

static const char *const ngi_name[LTQ_NUM_NGI_INSTANCES] = {
	"SSX0",
	"SSX1",
	"SSX2",
	"SSX3",
	"SSX4",
	"SSX5",
	"SSX6",
	"SSX7",
	"SSX8",
};

/* Initiator Agent */
struct ngi_ia {
	char *name;
	u32 offset;
	unsigned short rev_code;
	unsigned short core_code;
	unsigned short vendor_code;
	struct proc_dir_entry *proc;
};

struct ngi_inst;

/* Target Agent */
struct ngi_ta {
	struct ngi_inst *pinst; /* Back pointer to the instance */
	u32 offset;
	u32 pm_match_offset;
	u32 pm_read_offset;
	u32 pm_write_offset;
	char *name;
	unsigned short rev_code;
	unsigned short core_code;
	unsigned short vendor_code;
	u32 num_pr;
	struct ngi_acl_pr acl_pr[LTQ_NGI_MAX_PR_REGIONS];
	struct proc_dir_entry *proc;
};

/* Register Target*/
struct ngi_rt {
	struct ngi_inst *pinst; /* Back pointer to the instance */
	u32 pm_read_offset;
	u32 pm_write_offset;
	char *name;
	u32 num_pr;
	struct ngi_acl_pr rt_pr[LTQ_NGI_MAX_RT_PR_REGIONS];
	struct proc_dir_entry *proc;
};

/* Generic stuff for all the Instances */
struct ngi_props {
	u32 network_rev;
	u32 network_id;
	u8 initid;
	u8 timeout_base;
	u8 clk_gate_disable;
	u8 si_ctrl_clk_gate_disable;
	u32 num_ta;
	u32 num_ia;
	struct ngi_ta target_agents[NGI_MAX_TA];
	struct ngi_ia init_agents[NGI_MAX_IA];
	struct ngi_rt rt_agent;
};

struct ngi_inst {
	int inst_id;
	const char *name;
	struct device *dev;
	void __iomem *membase;
	struct proc_dir_entry *proc;
	struct proc_dir_entry *ta_proc;
	struct proc_dir_entry *ia_proc;
	struct proc_dir_entry *rt_proc;
	struct ngi_props props;
};

struct ngi_agent_core {
	u32 ngi_inst_id;
	u32 core_code;
};

struct ngi_addr_match {
	u32 low;
	u32 high;
};

static struct ngi_inst ltq_ngi_instance[LTQ_NUM_NGI_INSTANCES];

/*
 * handy ngi register accessor
 */
static inline unsigned int ltq_ngi_r32(struct ngi_inst *pinst, u32 offset)
{
	return ltq_r32(pinst->membase + offset);
}

static inline void ltq_ngi_w32(struct ngi_inst *pinst, u32 value, u32 offset)
{
	return ltq_w32(value, pinst->membase + offset);
}

static inline void ltq_ngi_w32_mask(struct ngi_inst *pinst, u32 clear, u32 set, u32 offset)
{
	return ltq_w32_mask(clear, set, pinst->membase + (offset));
}

static char *ltq_lookup_ia_code(u32 core_code)
{
	int i;

	for (i = 0; i < LTQ_NUM_INIT_AGENTS; i++) {
		if (ngi_ia_core_table[i].core_code == core_code)
			break;
	}

	if (i < LTQ_NUM_INIT_AGENTS)
		return ngi_ia_core_table[i].core_name;
	else
		return NULL;
}

static int ltq_lookup_ta_index(u32 core_code)
{
	int i;

	for (i = 0; i < LTQ_NUM_TARGET_AGENTS; i++) {
		if (ngi_ta_core_table[i].core_code == core_code)
			break;
	}

	if (i < LTQ_NUM_TARGET_AGENTS)
		return i;
	else
		return -1;
}

static int ltq_lookup_rt_index(u32 inst_id)
{
	int i;

	for (i = 0; i < LTQ_NUM_NGI_INSTANCES; i++) {
		if (ngi_rt_core_table[i].inst_id == inst_id)
			break;
	}

	if (i < LTQ_NUM_NGI_INSTANCES)
		return i;
	else
		return -1;
}

static struct ngi_addr_match ngi_build_match_value(const struct ngi_acl_pr *pr)
{
	u64 tmp;
	struct ngi_addr_match result;

	if (pr->pr_id == 0)
		panic("Region 0 has no match register\n");
	if (pr->base & (~PAGE_MASK))
		panic("Matching base addresses must be page aligned\n");
	if (pr->size != 0 && !is_power_of_2(pr->size))
		panic("Matching region size must be zero or power of 2");
	if (PM_SIZE_TO_KB(pr->size) > PM_MASK_MATCH_SIZE)
		panic("Matching region size is too large");
	if (pr->level > PM_MASK_MATCH_LEVEL)
		panic("Matching region level is too large");

	/* addr space must be 1. We do not know why. */
	tmp = (((u64)(PM_TO_KB(pr->base))) << PM_SHIFT_MATCH_BASE_ADDR) |
	      (PM_SIZE_TO_KB(pr->size) << PM_SHIFT_MATCH_SIZE) |
	      (pr->level << PM_SHIFT_MATCH_LEVEL) |
	      (1 << PM_SHIFT_MATCH_ADDR_SPACE);

	result.low = (u32)tmp;
	result.high = (u32)(tmp >> 32);
	return result;
}

static void ngi_print_protection_regions(struct seq_file *s,
					 const struct ngi_acl_pr *acl_list,
					 unsigned int num,
					 bool is_reg)
{
	int i;

	for (i = 0; i < num; ++i) {
		const struct ngi_acl_pr *acl = &acl_list[i];

		seq_printf(s, "\nProtection Region: %s (%u)\n", acl->pr_name, acl->pr_id);
		seq_puts(s, "-----------------------------------------\n");

		if (!is_reg && acl->pr_id > 0) {
			seq_printf(s, "Match      = [0x%08x, 0x%08x)\n", acl->base, acl->base + acl->size);
			seq_printf(s, "Match Size =  0x%08x (%u KiB = %u MiB)\n", acl->size, acl->size >> 10, acl->size >> 20);
			seq_printf(s, "Level      = %u\n", acl->level);
		}

		seq_printf(s, "Readers    = 0x%08x\n", acl->group_read_pr);
		seq_printf(s, "Writers    = 0x%08x\n", acl->group_write_pr);
		seq_puts(s, "-----------------------------------------\n");
	}
}

static int ngi_inst_enum_read_proc(struct seq_file *s, void *v)
{
	struct ngi_inst *pinst = s->private;
	struct ngi_props *pprops = &(pinst->props);

	seq_puts(s, "\nEnumeration Info\n");
	seq_puts(s, "-----------------------------------------\n");
	seq_printf(s, "Instance ID                 = %d\n", pinst->inst_id);
	seq_printf(s, "Name                        = %s\n", pinst->name);
	seq_printf(s, "Memory base                 = 0x%08x\n", (unsigned int)(pinst->membase));
	seq_printf(s, "Number or TAs               = %d\n", pprops->num_ta);
	seq_printf(s, "Number of IAs               = %d\n", pprops->num_ia);
	seq_printf(s, "Network Rev                 = 0x%08x\n", pprops->network_rev);
	seq_printf(s, "Network Id                  = 0x%08x\n", pprops->network_id);
	seq_printf(s, "InitId                      = 0x%08x\n", pprops->initid);
	seq_printf(s, "Timeout Base                = 0x%08x\n", pprops->timeout_base);
	seq_printf(s, "Clk Gate Disable            = 0x%08x\n", pprops->clk_gate_disable);
	seq_printf(s, "SI Control clk gate disable = 0x%08x\n", pprops->si_ctrl_clk_gate_disable);
	seq_puts(s, "-----------------------------------------\n");

	return 0;
}

static int ngi_inst_enum_read_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ngi_inst_enum_read_proc, PDE_DATA(inode));
}

static const struct file_operations ngi_inst_enum_proc_fops = {
	.open = ngi_inst_enum_read_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Target Agent related info proc fs */
static int ngi_ta_info_read_proc(struct seq_file *s, void *v)
{
	struct ngi_ta *pta = s->private;

	seq_puts(s, "\nTransfer Agent Info\n");
	seq_puts(s, "-----------------------------------------\n");
	seq_printf(s, "Name                         = %s\n", pta->name);
	seq_printf(s, "Rev code                     = 0x%08x\n", (unsigned int)(pta->rev_code));
	seq_printf(s, "Core code                    = 0x%08x\n", (unsigned int)(pta->core_code));
	seq_printf(s, "Vendor code                  = 0x%08x\n", (unsigned int)(pta->vendor_code));
	seq_printf(s, "Reg. Offset                  = 0x%08x\n", (unsigned int)(pta->offset));
	seq_printf(s, "Number of Protection Regions = %d\n", pta->num_pr);
	seq_puts(s, "-----------------------------------------\n");
	return 0;
}

static int ngi_ta_info_read_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ngi_ta_info_read_proc, PDE_DATA(inode));
}

static const struct file_operations ngi_ta_info_proc_fops = {
	.open = ngi_ta_info_read_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Init Agent related info proc fs */
static int ngi_ia_info_read_proc(struct seq_file *s, void *v)
{
	struct ngi_ia *pia = s->private;

	seq_puts(s, "\nInitiator Agent Info\n");
	seq_puts(s, "-----------------------------------------\n");
	seq_printf(s, "Name                         = %s\n", pia->name);
	seq_printf(s, "Rev code                     = 0x%08x\n", (unsigned int)(pia->rev_code));
	seq_printf(s, "Core code                    = 0x%08x\n", (unsigned int)(pia->core_code));
	seq_printf(s, "Vendor code                  = 0x%08x\n", (unsigned int)(pia->vendor_code));
	seq_printf(s, "Reg. Offset                  = 0x%08x\n", (unsigned int)(pia->offset));
	seq_puts(s, "-----------------------------------------\n");

	return 0;
}

static int ngi_ia_info_read_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ngi_ia_info_read_proc, PDE_DATA(inode));
}

static const struct file_operations ngi_ia_info_proc_fops = {
	.open = ngi_ia_info_read_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ngi_ta_pm_info_proc_read(struct seq_file *s, void *v)
{
	struct ngi_ta *pta = s->private;

	ngi_print_protection_regions(s, pta->acl_pr, pta->num_pr, false);
	return 0;
}

static int ngi_ta_pm_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ngi_ta_pm_info_proc_read, PDE_DATA(inode));
}

static const struct file_operations ngi_ta_pm_info_proc_fops = {
	.open = ngi_ta_pm_info_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int ngi_rt_pm_info_proc_read(struct seq_file *s, void *v)
{
	struct ngi_rt *prt = s->private;

	ngi_print_protection_regions(s, prt->rt_pr, prt->num_pr, true);
	return 0;
}

static int ngi_rt_pm_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ngi_rt_pm_info_proc_read, PDE_DATA(inode));
}

static const struct file_operations ngi_rt_pm_info_proc_fops = {
	.open = ngi_rt_pm_info_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int ngi_proc_init(struct ngi_inst *pinst)
{
	char proc_name[64] = { 0 };
	struct proc_dir_entry *entry;
	struct ngi_props *pprops = &pinst->props;
	struct ngi_ta *pta;
	struct ngi_ia *pia;
	struct ngi_rt *prt;
	int i;

	if (!g_ngi_root_proc_once) {
		strcpy(proc_name, "driver/ngi");
		g_ngi_root_proc = proc_mkdir(proc_name, NULL);
		if (!g_ngi_root_proc)
			return -ENOMEM;
		g_ngi_root_proc_once++;
	}

	strcpy(proc_name, pinst->name);
	pinst->proc = proc_mkdir(proc_name, g_ngi_root_proc);
	if (!pinst->proc)
		return -ENOMEM;

	entry = proc_create_data("enumeration_info", 0, pinst->proc,
				 &ngi_inst_enum_proc_fops, pinst);
	if (!entry)
		goto err1;

	strcpy(proc_name, "target_agents");
	pinst->ta_proc = proc_mkdir(proc_name, pinst->proc);
	if (!pinst->ta_proc)
		return -ENOMEM;

	/* Create the Target Agents proc */
	for (i = 0; i < pprops->num_ta; i++) {
		pta = &(pprops->target_agents[i]);

		strcpy(proc_name, pta->name);
		pta->proc = proc_mkdir(proc_name, pinst->ta_proc);
		if (!pta->proc)
			return -ENOMEM;

		entry = proc_create_data("info", 0, pta->proc,
					 &ngi_ta_info_proc_fops, pta);
		if (!entry)
			goto err1;

		entry = proc_create_data("protection_info", 0, pta->proc,
					 &ngi_ta_pm_info_proc_fops, pta);
		if (!entry)
			goto err1;
	}

	pr_debug("created ngi ta proc success ..\n");

	/* Create the Init Agents related proc */
	strcpy(proc_name, "init_agents");
	pinst->ia_proc = proc_mkdir(proc_name, pinst->proc);
	if (!pinst->ia_proc)
		return -ENOMEM;

	for (i = 0; i < pinst->props.num_ia; i++) {
		pia = &(pprops->init_agents[i]);

		strcpy(proc_name, pia->name);
		pia->proc = proc_mkdir(proc_name, pinst->ia_proc);
		if (!pia->proc)
			return -ENOMEM;

		entry = proc_create_data("info", 0, pia->proc,
					 &ngi_ia_info_proc_fops, pia);
		if (!entry)
			goto err1;
	}
	pr_debug("created ngi ia proc success ..\n");

	/* Create the Register Target related proc */
	strcpy(proc_name, "reg_target");
	pinst->rt_proc = proc_mkdir(proc_name, pinst->proc);
	if (!pinst->rt_proc)
		return -ENOMEM;

	prt = &(pprops->rt_agent);

	strcpy(proc_name, prt->name);
	prt->proc = proc_mkdir(proc_name, pinst->rt_proc);
	if (!prt->proc)
		return -ENOMEM;

	entry = proc_create_data("protection_info", 0, prt->proc,
				 &ngi_rt_pm_info_proc_fops, prt);
	if (!entry)
		goto err1;

	return 0;
err1:
	remove_proc_entry(proc_name, NULL);
	return -ENOMEM;
}

static void ltq_write_ta_protection(const struct ngi_ta *ta)
{
	u32 i;

	for (i = 0; i < ta->num_pr; i++) {
		const struct ngi_acl_pr *acl = &ta->acl_pr[i];
		struct ngi_inst *pinst = ta->pinst;

		/* Program the registers with the above values */
		ltq_ngi_w32(pinst, acl->group_read_pr, (ta->pm_read_offset + NGI_REGION(i)));
		ltq_ngi_w32(pinst, acl->group_write_pr, (ta->pm_write_offset + NGI_REGION(i)));

		/* Region 0 has no match register */
		if (acl->pr_id > 0) {
			struct ngi_addr_match reg_val = ngi_build_match_value(acl);
			u32 reg_addr = ta->pm_match_offset + NGI_REGION(i);

			ltq_ngi_w32(pinst, reg_val.low, reg_addr);
			ltq_ngi_w32(pinst, reg_val.high, reg_addr + 4);
		}
	}
}

static void ltq_write_rt_protection(const struct ngi_rt *rt)
{
	u32 i;

	for (i = 0; i < rt->num_pr; i++) {
		const struct ngi_acl_pr *acl = &rt->rt_pr[i];
		struct ngi_inst *pinst = rt->pinst;

		/* Program the registers with the above values */
		ltq_ngi_w32(pinst, acl->group_read_pr, (rt->pm_read_offset + NGI_REGION(i)));
		ltq_ngi_w32(pinst, acl->group_write_pr, (rt->pm_write_offset + NGI_REGION(i)));
	}
}

static int ltq_ngi_enumerate(struct ngi_inst *pinst)
{
	int ia_count = 0, ta_count = 0, ngi_ta_count = 0, ngi_ia_count = 0;
	u32 initial_comp_addr;
	u32 initial_read_pm_addr;
	u32 initial_write_pm_addr;
	u32 initial_addr_match_addr;
	u32 initial_rt_read_pm_addr;
	u32 initial_rt_write_pm_addr;
	u32 comp_code;
	int i, table_index, rt_table_index;
	const struct ngi_rt_core *rt_table = NULL;
	int temp_count;

	pr_debug("%s called..\n", __func__);

	/* Read the RT.component register */
	if (((ltq_ngi_r32(pinst, TREG0_RT_COMPONENT) & TREG0_RT_COMPONENT_CODE_MASK) >> TREG0_RT_COMPONENT_CODE_POS) != 0x6200)
		panic("not an NGI segment !!\n");

	/* Read the Network, Initid, Network Control */
	pinst->props.network_rev = ltq_ngi_r32(pinst, TREG0_RT_NETWORK + 4) & 0xFFFF; /* upper 32-bits (bit 32~63) */
	pinst->props.network_id = (ltq_ngi_r32(pinst, TREG0_RT_NETWORK + 4) >> 16) & 0xFFFF; /* upper 32-bits (bit 32~63) */

	pinst->props.initid = ltq_ngi_r32(pinst, TREG0_RT_INITID_READBACK) & TREG0_RT_INITID_READBACK_INITID_MASK;
	pinst->props.timeout_base = (ltq_ngi_r32(pinst, TREG0_RT_NETWORK_CONTROL) & TREG0_RT_NETWORK_CONTROL_TIMEOUT_BASE_MASK) >> TREG0_RT_NETWORK_CONTROL_TIMEOUT_BASE_POS;
	pinst->props.clk_gate_disable = (ltq_ngi_r32(pinst, TREG0_RT_NETWORK_CONTROL + 4) & (TREG0_RT_NETWORK_CONTROL_CLOCK_GATE_DISABLE_MASK >> 32) >> (TREG0_RT_NETWORK_CONTROL_CLOCK_GATE_DISABLE_POS - 32)); /* upper 32-bits (bit 32~63) */
	pinst->props.si_ctrl_clk_gate_disable = (ltq_ngi_r32(pinst, TREG0_SI_CONTROL + 4) & (TREG0_SI_CONTROL_CLOCK_GATE_DISABLE_MASK >> 32) >> (TREG0_SI_CONTROL_CLOCK_GATE_DISABLE_POS - 32)); /* upper 32-bits (bit 32~63) */

	/*--------------------------------------------------------------------*/
	/* Enumerate the Target Agents */
	/*--------------------------------------------------------------------*/
	initial_comp_addr = TSSB_TA_COMPONENT;
	initial_read_pm_addr = TSSB_PM_READ_PERMISSION_0;
	initial_write_pm_addr = TSSB_PM_WRITE_PERMISSION_0;
	initial_addr_match_addr = TSSB_PM_ADDR_MATCH_1 - NGI_REGION_SIZE;

	if (pinst->inst_id == 0)
		ngi_ta_count = 15;
	else if (pinst->inst_id == 1)
		ngi_ta_count = 4;
	else if (pinst->inst_id == 2)
		ngi_ta_count = 8;
	else if (pinst->inst_id == 3)
		ngi_ta_count = 13;
	else if (pinst->inst_id == 4)
		ngi_ta_count = 16;
	else if (pinst->inst_id == 6)
		ngi_ta_count = 5;

	for (temp_count = 0; temp_count < ngi_ta_count; temp_count++) {
		pr_debug("reading from offset: %x \n", (unsigned int)initial_comp_addr);
		/* Read the component register */
		comp_code = (ltq_ngi_r32(pinst, initial_comp_addr) & TSSB_TA_COMPONENT_CODE_MASK) >> TSSB_TA_COMPONENT_CODE_POS;
		if (comp_code == 0x6020) {
			struct ngi_ta *ta = &pinst->props.target_agents[ta_count];
			const struct ngi_ta_core *table = NULL;

			/* Read the TA.Core */
			ta->rev_code = ltq_ngi_r32(pinst, initial_comp_addr + 0x18) & TSSB_TA_CORE_REV_CODE_MASK;
			ta->core_code = (ltq_ngi_r32(pinst, initial_comp_addr + 0x18) & TSSB_TA_CORE_CORE_CODE_MASK) >> TSSB_TA_CORE_CORE_CODE_POS;
			ta->vendor_code = ltq_ngi_r32(pinst, initial_comp_addr + 0x18 + 4) & (TSSB_TA_CORE_VENDOR_CODE_MASK >> 32); /* upper 32-bits (bit 32~63) */

			/* Get the default table index based on the vendor code */
			table_index = ltq_lookup_ta_index(ta->core_code);
			table = &ngi_ta_core_table[table_index];
			pr_debug("table_index = %d and core_code = %x\n", table_index, (unsigned int)(ta->core_code));

			if (table_index < 0) {
				pr_err("%s: couldn't find the TA info from the table\n", __func__);
				continue;
			}

			/* Store the name and other info */
			ta->name = table->core_name;
			ta->offset = initial_comp_addr;
			ta->pm_match_offset = initial_addr_match_addr;
			ta->pm_read_offset = initial_read_pm_addr;
			ta->pm_write_offset = initial_write_pm_addr;
			ta->pinst = pinst;

			/* Store all the properties of Protection Region from the table */
			ta->num_pr = table->num_pr;
			memcpy(ta->acl_pr, table->acl_pr, sizeof(struct ngi_acl_pr) * ta->num_pr);
			for (i = 0; i < ta->num_pr; i++) {
				ta->acl_pr[i].pr_id = i;
			}
			ltq_write_ta_protection(ta);

			/* Set the default REQ_TIMEOUT to be 4 */
			pr_debug("request timeout for TA: %s BEFORE is : %x \n",
				 ta->name, ltq_ngi_r32(pinst, initial_comp_addr + NGI_REGION_SIZE));
			ltq_ngi_w32_mask(pinst, 0, 4 << TSSB_TA_AGENT_CONTROL_REQ_TIMEOUT_POS, (initial_comp_addr + NGI_REGION_SIZE));
			pr_debug("request timeout for TA: %s AFTER is : %x \n",
				 ta->name, ltq_ngi_r32(pinst, initial_comp_addr + NGI_REGION_SIZE));
			ta_count++;
		}
		initial_comp_addr += NGI_COMPONENT_SIZE;
		initial_read_pm_addr += NGI_COMPONENT_SIZE;
		initial_write_pm_addr += NGI_COMPONENT_SIZE;
		initial_addr_match_addr += NGI_COMPONENT_SIZE;
	}

	pinst->props.num_ta = ta_count;

	/*--------------------------------------------------------------------*/
	/* Enumerate the Initiator Agents */
	/*--------------------------------------------------------------------*/
	initial_comp_addr = IE97W_IA_COMPONENT;
	if (pinst->inst_id == 0)
		ngi_ia_count = 13;
	else if (pinst->inst_id == 1)
		ngi_ia_count = 7;
	else if (pinst->inst_id == 2)
		ngi_ia_count = 8;
	else if (pinst->inst_id == 3)
		ngi_ia_count = 7;
	else if (pinst->inst_id == 4)
		ngi_ia_count = 1;
	else if (pinst->inst_id == 6)
		ngi_ia_count = 3;

	for (temp_count = 0; temp_count < ngi_ia_count; temp_count++) {
		/* Read the component register */
		comp_code = (ltq_ngi_r32(pinst, initial_comp_addr) & IE97W_IA_COMPONENT_CODE_MASK) >> IE97W_IA_COMPONENT_CODE_POS;
		if (comp_code == 0x6010) {
			struct ngi_ia *ia = &pinst->props.init_agents[ia_count];

			/* Read the IA.Core */
			ia->rev_code = ltq_ngi_r32(pinst, initial_comp_addr + 0x18) & IE97W_IA_CORE_REV_CODE_MASK;
			ia->core_code = (ltq_ngi_r32(pinst, initial_comp_addr + 0x18) & IE97W_IA_CORE_CORE_CODE_MASK) >> IE97W_IA_CORE_CORE_CODE_POS;
			ia->vendor_code = ltq_ngi_r32(pinst, initial_comp_addr + 0x18 + 4) & (IE97W_IA_CORE_VENDOR_CODE_MASK >> 32); /* upper 32-bits (bit 32~63) */

			/* Lookup the core code and get the name and store */
			ia->name = ltq_lookup_ia_code(ia->core_code);
			ia->offset = initial_comp_addr;
			pr_debug("core name = %s and core_code = %x\n", ia->name, (unsigned int)(ia->core_code));

			ia_count++;
			initial_comp_addr += NGI_COMPONENT_SIZE;
		}
	}

	pinst->props.num_ia = ia_count;

	/*--------------------------------------------------------------------*/
	/* Enumerate the Register Target protection region */
	/*--------------------------------------------------------------------*/
	initial_rt_read_pm_addr = TREG0_PM_READ_PERMISSION_0;
	initial_rt_write_pm_addr = TREG0_PM_WRITE_PERMISSION_0;

	/* Register target related stuff */
	rt_table_index = ltq_lookup_rt_index(pinst->inst_id);
	rt_table = &ngi_rt_core_table[rt_table_index];
	pr_debug("table_index of RT = %d\n", rt_table_index);

	if (rt_table_index < 0) {
		pr_err("%s: couldn't find the RT info from the table\n", __func__);
		return -ENODEV;
	}

	/* Change the RT NETWORK Control timeout base to 4 */
	pr_debug("timeout base for TREG: %s BEFORE is : %x\n", pinst->props.rt_agent.name, ltq_ngi_r32(pinst, TREG0_RT_NETWORK_CONTROL));
	ltq_ngi_w32_mask(pinst, 0, 4 << TREG0_RT_NETWORK_CONTROL_TIMEOUT_BASE_POS, TREG0_RT_NETWORK_CONTROL);
	pr_debug("timeout base for TREG: %s AFTER is : %x\n", pinst->props.rt_agent.name, ltq_ngi_r32(pinst, TREG0_RT_NETWORK_CONTROL));

	/* Store the name and other info */
	pinst->props.rt_agent.name = rt_table->name;
	pinst->props.rt_agent.pm_read_offset = initial_rt_read_pm_addr;
	pinst->props.rt_agent.pm_write_offset = initial_rt_write_pm_addr;
	pinst->props.rt_agent.pinst = pinst;
	pinst->props.rt_agent.num_pr = rt_table->num_pr;

	memcpy(pinst->props.rt_agent.rt_pr, rt_table->rt_pr, sizeof(struct ngi_rt) * pinst->props.rt_agent.num_pr);
	ltq_write_rt_protection(&pinst->props.rt_agent);

	return 0;
}

static int ltq_ngi_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *memres;
	static u32 inst_id;
	struct ngi_inst *pinstance;
	pr_debug("%s called..\n", __func__);

	/* DMA controller idx */
	if (!of_property_read_u32(node, "lantiq,ngi-inst-id", &inst_id))
		pdev->id = inst_id;
	else
		panic("Failed to get NGI instance id\n");

	if (pdev->id < 0 || pdev->id >= LTQ_NUM_NGI_INSTANCES)
		return -EINVAL;

	pinstance = &ltq_ngi_instance[pdev->id];

	memset(pinstance, 0, sizeof(*pinstance));

	pinstance->inst_id = pdev->id;
	pinstance->name = ngi_name[pinstance->inst_id];

	/* Link controller to platform device */
	pinstance->dev = &pdev->dev;
	memres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!memres)
		panic("Failed to get NGI resource");

	/* remap dma register range */
	pinstance->membase = devm_ioremap_resource(&pdev->dev, memres);

	if (IS_ERR(pinstance->membase))
		panic("Failed to remap NGI resource");

	pr_debug("NGI membase: %x\n", (unsigned int)(pinstance->membase));

	/* Enumerate the bus */
	ltq_ngi_enumerate(pinstance);

	/* Init the procfs for this instance */
	ngi_proc_init(pinstance);

	return 0;
}

static const struct of_device_id ltq_ngi_match[] = {
	{ .compatible = "lantiq,ngi-xrx500" },
	{},
};
MODULE_DEVICE_TABLE(of, ltq_xrx500_match);

static struct platform_driver ltq_ngi_driver = {
	.probe = ltq_ngi_probe,
	.driver = {
		.name = "ngi-xrx500",
		.owner = THIS_MODULE,
		.of_match_table = ltq_ngi_match,
	},
};

static int find_ta_pr(const char *ta_name, const char *pr_name, struct ngi_ta **o_ta, struct ngi_acl_pr **o_acl)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(ltq_ngi_instance); ++i) {
		struct ngi_props *props = &ltq_ngi_instance[i].props;
		u32 j;

		for (j = 0; j < props->num_ta; ++j) {
			struct ngi_ta *ta = &props->target_agents[j];
			u32 v;

			if  (strcmp(ta_name, ta->name))
				continue;

			for (v = 0; v < ta->num_pr; ++v) {
				struct ngi_acl_pr *acl = &ta->acl_pr[v];

				if (strcmp(pr_name, acl->pr_name))
					continue;

				*o_ta = ta;
				*o_acl = acl;
				return 0;
			}
		}
	}
	return -ENOENT;
}

static void seal_protection(void)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(ltq_ngi_instance); ++i) {
		struct ngi_rt *rt = &ltq_ngi_instance[i].props.rt_agent;

		rt->rt_pr[1].group_read_pr = 0;
		rt->rt_pr[1].group_write_pr = 0;
		ltq_write_rt_protection(rt);
	}
}

void ngi_surrender_eip123(void)
{
	struct ngi_ta *ta_ddr, *ta_eip123;
	struct ngi_acl_pr *acl_bootcore, *acl_eip123;

	BUG_ON(find_ta_pr("TA_DDR", "Bootcore", &ta_ddr, &acl_bootcore));
	BUG_ON(find_ta_pr("TA_EIP123", "PR0", &ta_eip123, &acl_eip123));

	/* lockout eip123 from bootcore mem */
	acl_bootcore->group_read_pr = PM_DDR_GROUP_BOOTCORE;
	acl_bootcore->group_write_pr = PM_DDR_GROUP_BOOTCORE;
	ltq_write_ta_protection(ta_ddr);

	/* switch eip123 access to interaptive */
	acl_eip123->group_read_pr = PM_DDR_GROUP_INTERAPTIVE;
	acl_eip123->group_write_pr = PM_DDR_GROUP_INTERAPTIVE;
	ltq_write_ta_protection(ta_eip123);

	/* prevent ourself from changing the config back */
	seal_protection();
}
EXPORT_SYMBOL(ngi_surrender_eip123);

static int __init read_dts_cfg(struct ngi_dts_cfg *cfg)
{
	struct device_node *node;
	struct property *prop;
	int prop_len;
	const __be32 *iter;
	int ret = -EINVAL;

	node = of_find_node_by_path("/avm,ngi-protection");
	if (!node)
		goto out;

	prop = of_find_property(node, "ram-size", &prop_len);
	if (!prop)
		goto put_node;

	iter = of_prop_next_u32(prop, NULL, &cfg->memory_size);
	if (!iter)
		goto put_node;

	prop = of_find_property(node, "bootcore", &prop_len);
	if (!prop)
		goto put_node;

	iter = of_prop_next_u32(prop, NULL, &cfg->bootcore_start);
	if (!iter)
		goto put_node;

	iter = of_prop_next_u32(prop, iter, &cfg->bootcore_size);
	if (!iter)
		goto put_node;

	ret = 0;
put_node:
	of_node_put(node);
out:
	return ret;
}

static int __init find_pr(const char *name, struct ngi_ta_core **ddr, u32 *idx)
{
	unsigned int i, j;

	for (i = 0; i < LTQ_NUM_TARGET_AGENTS; ++i) {
		struct ngi_ta_core *core = &ngi_ta_core_table[i];

		if (core->core_code != TA_DDR_CORE_CODE)
			continue;

		for (j = 0; j < core->num_pr; ++j) {
			struct ngi_acl_pr *acl = &core->acl_pr[j];

			if (strcmp(acl->pr_name, name) != 0)
				continue;

			*ddr = core;
			*idx = j;
			return 0;
		}
	}
	return -ENOENT;
}

static int __init protect_bootcore(const struct ngi_dts_cfg *cfg)
{
	struct ngi_ta_core *ddr;
	u32 idx;
	int ret;

	ret = find_pr(BOOTCORE, &ddr, &idx);
	if (ret)
		return ret;

	pr_info("Protecting Bootcore: [0x%08x, 0x%08x]\n", cfg->bootcore_start,
		cfg->bootcore_start - 1 + cfg->bootcore_size);

	ddr->acl_pr[idx].base = cfg->bootcore_start;
	ddr->acl_pr[idx].size = cfg->bootcore_size;
	return 0;
}

#define NGI_RAM_START 0x20000000
#define NGI_RAM_END 0x9fffffff
#define NGI_RAM_MAX_SIZE (NGI_RAM_END - NGI_RAM_START + 1)
static int __init protect_unattached(const struct ngi_dts_cfg *cfg)
{
	const u32 memory_size = cfg->memory_size << 20;
	struct ngi_ta_core *ddr;
	u32 size, end, idx;
	int ret;

	if (!memory_size || memory_size > NGI_RAM_MAX_SIZE)
		return -EINVAL;

	end = NGI_RAM_END + 1;
	size = NGI_RAM_MAX_SIZE - memory_size;

	ret = find_pr(UNATTACHED_MEMORY, &ddr, &idx);
	if (ret)
		return ret;

	pr_info("%uMB attached. Protecting: [0x%08x, 0x%08x]\n",
		cfg->memory_size, end - size, end - 1);

	while (idx < ddr->num_pr && size) {
		struct ngi_acl_pr *pr = &ddr->acl_pr[idx];

		pr->pr_name = UNATTACHED_MEMORY;
		pr->level = PM_LEVEL_HIGH;
		pr->group_read_pr = PM_DDR_GROUP_NONE;
		pr->group_write_pr = PM_DDR_GROUP_NONE;
		pr->size = rounddown_pow_of_two(size);
		pr->base = end - pr->size;

		pr_info("Unattached: [0x%08x, 0x%08x]\n",
			pr->base, pr->base - 1 + pr->size);

		end  -= pr->size;
		size -= pr->size;
		++idx;
	}
	return size == 0 ? 0 : -EINVAL;
}

static int __init ltq_ngi_init(void)
{
	struct ngi_dts_cfg cfg;

	if (read_dts_cfg(&cfg))
		panic("ngi: Failed to read dts node\n");

	if (protect_bootcore(&cfg))
		panic("ngi: Failed to protect bootcore\n");

	if (protect_unattached(&cfg))
		panic("ngi: Failed to protect unattached memory\n");

	if (platform_driver_register(&ltq_ngi_driver))
		panic("ngi-xrx500: Error registering platform driver!\n");

	return 0;
}

core_initcall(ltq_ngi_init);
