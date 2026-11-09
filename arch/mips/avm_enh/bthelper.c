// SPDX-License-Identifier: GPL-2.0+
/**
 *   mips-helper-functions for stackdump on smp etc.
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <asm/avm_enh/avm_enh.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/mmu_context.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/branch.h>
#include <asm/mmu_context.h>
#include <asm/types.h>
#include <asm/stacktrace.h>
#include <avm/sammel/simple_proc.h>
#include <asm/yield_context.h>
#include <linux/vmalloc.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <asm/mach_avm.h>
#include <avm/sammel/simple_mempool.h>
#include <avm/sammel/debug.h>
#include <asm/mach_avm.h>
#include <asm/sections.h>
#include <asm/page.h>
#include <linux/kallsyms.h>
#include <linux/mm_types.h>
#include <linux/pfn.h>
#include <avm/enh/fw_info.h>
#include <avm/enh/module_mem.h>

#define snprintf_add(ptxt, txtlen, args...)	do {					\
	if (ptxt == NULL) {								\
		pr_err(args);								\
	} else {									\
		int local_add_len;							\
											\
		local_add_len = snprintf(ptxt, txtlen, args);				\
		if (local_add_len > 0) {						\
			int tail = min_t(int, txtlen, local_add_len);			\
											\
			(ptxt) += tail, (txtlen) -= tail;				\
		}									\
	}										\
	} while (0)
extern void show_backtrace(struct task_struct *task, const struct pt_regs *regs);
#if defined(CONFIG_AVM_ENHANCED)

#ifdef CONFIG_AVM_YIELD
enum _monitor_ipi_type {
	monitor_nop = 0x0,
	monitor_bt = 0x1,
};

struct _monitor_bt {
	unsigned int valid;
	struct pt_regs ptregs;
};

struct _monitor_ipi {
	enum _monitor_ipi_type type;
	union __monitor_ipi {
		struct _monitor_bt bt[YIELD_MONITOR_MAX_TC];
	} buf;
	unsigned int core;
	unsigned int init;
	unsigned int irq;
	atomic_t ready;
	spinlock_t lock;
};
static atomic_t backtrace_busy;
static struct _monitor_ipi IPI_Monitor[YIELD_MONITOR_MAX_CORES];
static int monitor_ipi_yield_context(int signal __attribute__((unused)),
				     void *handle);
static int __get_userinfo(char *buf, unsigned int maxbuflen,
			  struct mm_struct *mmm, unsigned long addr);
#endif
/**
 */
static void print_cp0_status(unsigned int cp0_status)
{
	if (cpu_has_3kex) {
		pr_err("Status:  %08x %s%s%s%s%s\n", cp0_status,
		       (cp0_status & ST0_KUO) ? "KUo " : "",
		       (cp0_status & ST0_IEO) ? "IEo " : "",
		       (cp0_status & ST0_KUP) ? "KUp " : "",
		       (cp0_status & ST0_IEP) ? "IEp " : "",
		       (cp0_status & ST0_IEC) ? "IEc " : "");
	} else if (cpu_has_4kex) {
		pr_err("Status:  %08x %s%s%s%s%s%s%s\n", cp0_status,
		       (cp0_status & ST0_KX) ? "KX " : "",
		       (cp0_status & ST0_SX) ? "SX " : "",
		       (cp0_status & ST0_UX) ? "UX " : "",
		       (cp0_status & ST0_KSU) == KSU_USER ?
			       "USER " :
			       (cp0_status & ST0_KSU) == KSU_SUPERVISOR ?
			       "SUPERVISOR " :
			       (cp0_status & ST0_KSU) == KSU_KERNEL ?
			       "KERNEL " :
			       "BAD MODE ",
		       (cp0_status & ST0_ERL) ? "ERL " : "",
		       (cp0_status & ST0_EXL) ? "EXL " : "",
		       (cp0_status & ST0_IE) ? "IE" : "");
	}
}

/**
 */
static struct _cpuid_to_mt_info {
	unsigned int core;
	unsigned int tc;
	unsigned int linuxos;
	char name[12];
} cpu_id_to_mt_info[NR_CPUS];

/**
 */
#define IS_KERNEL_ADDR		  0x1
#define IS_MODULE_ADDR		  0x2
#define IS_VMALLOC_ADDR		  0x3
#define IS_STACK_ADDR		  0x4

/**
 */
static int memory_classifier(unsigned long addr)
{

	if ((addr >= (unsigned long)_stext) && (addr <= (unsigned long)_end)) {
		return IS_KERNEL_ADDR;
	} else if (is_module_text_address(addr)) {
		return IS_MODULE_ADDR;
	} else if (is_vmalloc_addr((void *)addr)) {
		return IS_VMALLOC_ADDR;
	} else if (object_is_on_stack((void *)addr)) {
		return IS_STACK_ADDR;
	}
	return 0;
}

/**
 * @return NULL no virtual addr
 */
static struct page *memory_page_classifier(unsigned long addr)
{
	if (virt_addr_valid(addr)) {
		return virt_to_head_page((void *)addr);
	}
	return NULL;
}

/**
 */
static char *print_vmflags(char *txt, unsigned int txtlen, unsigned long vm_flags)
{
	char *txt_start = txt;

	if (txt == NULL)
		return NULL;
	txt[0] = 0;
	if (vm_flags & VM_IOREMAP)
		snprintf_add(txt, txtlen, "ioremap ");
	if (vm_flags & VM_ALLOC)
		snprintf_add(txt, txtlen, "vmalloc ");
	if (vm_flags & VM_MAP)
		snprintf_add(txt, txtlen, "vmap ");
	if (vm_flags & VM_USERMAP)
		snprintf_add(txt, txtlen, "user ");
	if (vm_flags & VM_KASAN)
		snprintf_add(txt, txtlen, "kasan ");
	if (txt_start[0] == 0)
		snprintf_add(txt, txtlen, "vm_flags=0x%lx ", vm_flags);
	return txt_start;
}

/**
 */
static int print_modulealloc_area(char *txt, unsigned int txtlen, unsigned long addr)
{
	unsigned long size;
	unsigned int alloced;
	char *modname;
	unsigned long start = get_modulealloc_area(addr, &modname, &alloced, &size);

	if (start) {
		snprintf(txt, txtlen, "[module-alloc: 0x%p (%s + 0x%lx) size: %lu%s]",
			 (void *)addr,
			 modname[0] ? modname : "free-modulebase",
			 addr - start, size, alloced ? "" :
			 modname[0] ? " freed" : " reserved");
		return 1;
	}
	return 0;
}

/**
 */
char *arch_print_memory_classifier(char *txt, unsigned int txtlen, unsigned long addr, int include_addr_prefix)
{
	char sym[KSYM_SYMBOL_LEN], *modname;
	char txtbuf[TASK_COMM_LEN + 16];
	char *txt_start = txt;
	unsigned long caller, size, offset, start, flags, vmflags;
	int freed, type;
	const char *name = NULL;
	struct page *page;
	struct zone *zone;

	if (include_addr_prefix) {
		snprintf_add(txt, txtlen, "0x%08lx ", addr);
	} else {
		txt[0] = 0;
	}
	type = memory_classifier(addr);
	switch (type) {
	case IS_KERNEL_ADDR:
	case IS_MODULE_ADDR:
#ifdef CONFIG_KALLSYMS
		name = kallsyms_lookup(addr, &size, &offset, &modname, sym);
		if (name) {
			snprintf_add(txt, txtlen, "%s+%#lx/%#lx", name, offset, size);
			if (modname) {
				snprintf_add(txt, txtlen, " [%s]", modname);
			}
			return txt_start;
		}
#endif
		if (!name) {
			/*--- it can be module-area and we didn't found symbols ---*/
			print_modulealloc_area(txt, txtlen, addr);
		}
		return txt_start;
	case IS_VMALLOC_ADDR:
		if (is_yield_context()) {
			return txt_start;
		}
		start = get_vmap_area(addr, &caller, &size, &vmflags);
		if (start) {
			snprintf(txt, txtlen, "[%s: size:%lu start:%p+0x%lx alloced by:%pS]",
						print_vmflags(txtbuf, sizeof(txtbuf), vmflags),
						size, (void *)start, addr - start,  (void *)caller);
		}
		return txt_start;
	case IS_STACK_ADDR:
		break;
	default:
		break;
	}
	if (print_modulealloc_area(txt, txtlen, addr)) {
		return txt_start;
	}
	start = get_taskstack_area(addr, txtbuf, sizeof(txtbuf), type == IS_STACK_ADDR ? 1 : 0);
	if (start) {
		snprintf(txt, txtlen, "[%s: %p+0x%lx]", txtbuf, (void *)start, addr - start);
		return txt_start;
	}
	start = get_simplemempool_area(addr, &caller, txtbuf, sizeof(txtbuf), &size, &freed);
	if (start) {
		if (caller) {
			snprintf(sym, sizeof(sym), " allocated by:%pS ", (void *)caller);
		} else {
			sym[0] = 0;
		}
		snprintf(txt, txtlen, "[smempool: type:%s size:%lu start:%p%c0x%lx %s%s]", txtbuf,
				  size,
				 (void *)start,
				  addr >= start ? '+' : '-',
				  addr >= start ? addr - start : start - addr,
				  sym,
				  freed == 0   ? "" :
				  freed == 1   ? "free" :
				  freed == 2   ? "ctrl" : "padding");
		return txt_start;
	}
	if (is_yield_context()) {
		return txt_start;
	}
	page = memory_page_classifier(addr);
	if (!page) {
		return txt_start;
	}
	zone = page_zone(page);
	if (!spin_trylock_irqsave(&zone->lock, flags)) {
		return txt_start;
	}
	if (PageSlab(page)) {
		start = get_kmemalloc_area(addr, &caller, &name, &size, &freed);
		if (start) {
			if (caller) {
				snprintf(sym, sizeof(sym), " %s by:%pS", freed ? "freed" : "allocated", (void *)caller);
			} else {
				sym[0] = 0;
			}
			snprintf(txt, txtlen, "[slab: type:%s size:%lu start:0x%p+0x%lx%s]", name, size, (void *)start,
					addr - start,
					sym);
		}
	} else if (PageReserved(page)) {
		snprintf(txt, txtlen, "[page: type:reserved]");
	} else if (page_ref_count(page)) {
		char ordertxt[32];
		unsigned int order;
		unsigned long current_pc = 0;

#if defined(CONFIG_AVM_PAGE_TRACE)
		current_pc = avm_get_page_current_pc(page);
#endif/*--- #if defined(CONFIG_AVM_PAGE_TRACE) ---*/
		if (current_pc) {
			snprintf(sym, sizeof(sym), " by:%pS", (void *)current_pc);
		} else {
			sym[0] = 0;
		}
		order = compound_order(page);
		if (order) {
			snprintf(ordertxt, sizeof(ordertxt), " O%u[%lu]",
				 order,
				 page_to_pfn(virt_to_page(addr)) - page_to_pfn(page));
		} else {
			ordertxt[0] = 0;
		}
		snprintf(txt, txtlen, "[page%s: type:alloc%s]", ordertxt, sym);
	}
	spin_unlock_irqrestore(&zone->lock, flags);
	return txt_start;
}
EXPORT_SYMBOL(print_memory_classifier);

/**
 */
static int match_data(unsigned long data, unsigned long data_array[], unsigned int array_elements)
{
	unsigned int i;

	for (i = 0; i < array_elements; i++) {
		if (data_array[i] == 0) {
			data_array[i] = data;
			return 0;
		}
		if (data_array[i] == data) {
			return 1;
		}
	}
	return 0;
}

/**
 */
void arch_show_stacktrace_memoryclassifier(const struct pt_regs *pregs)
{
	unsigned long data_hist[40];
	char txt[KSYM_SYMBOL_LEN];
	unsigned int start = 0, limit = 0;
	unsigned long stackdata;
	unsigned long __user *sp;
	mm_segment_t old_fs;

	if (pregs == NULL) {
		return;
	}
	old_fs = get_fs();
	sp = (unsigned long __user *)pregs->regs[29];

	while ((unsigned long) sp & (PAGE_SIZE - 1)) {
		if (limit > 39) {
			break;
		}
		set_fs(KERNEL_DS);

		if (__get_user(stackdata, sp++)) {
			break;
		}
		if (start >= ARRAY_SIZE(data_hist)) {
			pr_err("...\n");
			break;
		}
		if (stackdata && match_data(stackdata, data_hist, ARRAY_SIZE(data_hist))) {
			continue;
		}
		print_memory_classifier(txt, sizeof(txt), stackdata, 0);
		if (txt[0]) {
			if (start == 0) {
				pr_err("Classified pointer on stack:\n");
			}
			start++;
			pr_err("%08lx %s\n", stackdata, txt);
		}
		set_fs(old_fs);
		limit++;
	}
}

/**
 */
void arch_show_register_memoryclassifier(const struct pt_regs *pregs)
{
	char txt[KSYM_SYMBOL_LEN];
	unsigned int i, start = 0;

	if (pregs == NULL) {
		return;
	}
	for (i = 0; i < ARRAY_SIZE(pregs->regs); i++) {
		print_memory_classifier(txt, sizeof(txt), pregs->regs[i], 0);
		if (txt[0]) {
			if (start == 0) {
				start = 1;
				pr_err("Classified pointer on registers:\n");
			}
			pr_err(" $%2u : %08lx %s\n", i, pregs->regs[i], txt);

		}
	}
}
/**
 * collect info about which cpuid use core_x, tc_x and linux-mode
 * (needed for perfect backtrace)
 */
void avm_register_cpuid(char *name, unsigned int cpuid, unsigned int core,
			unsigned int tc)
{
	struct _cpuid_to_mt_info *p_mt = &cpu_id_to_mt_info[cpuid];

	if (cpuid > num_possible_cpus()) {
		pr_err("%s: invalid cpu_id=%u\n", __func__, cpuid);
		return;
	}
	if (name && (strcmp(name, "LINUX") == 0)) {
		p_mt->linuxos = 1;
	} else {
		p_mt->linuxos = 0;
	}
	if (name) {
		snprintf(p_mt->name, sizeof(p_mt->name), name);
	} else {
		p_mt->name[0] = 0;
	}
	p_mt->core = core;
	p_mt->tc = tc;
	pr_err("%s: cpu_id=%u: %s core=%u tc=%u\n", __func__, cpuid, name, core, tc);
}
/**
 * Request which (linux-)cpu_id  behind tc/core
 * ret:  cpu_id - if cpu_id == -1: no linux-os
 * name: name of OS on this CPU
 */
int get_cpuid_by_mt(unsigned int core, unsigned int tc, char **name)
{
	unsigned int cpu;

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		struct _cpuid_to_mt_info *p_mt = &cpu_id_to_mt_info[cpu];

		if ((p_mt->core == core) && (p_mt->tc == tc)) {
			if (name)
				*name = p_mt->name;
			return p_mt->linuxos ? cpu : -1;
		}
	}
	if (name)
		*name = "Other";
	return -1;
}

/**
 * Request which tc/core behind linux-cpu
 *
 * ret:   0      Linux-OS
 *      < 0      invalid CPU_ID
 *      > 0      other
 */
int get_mt_by_cpuid(unsigned int cpu_id, unsigned int *core, unsigned int *tc)
{
	struct _cpuid_to_mt_info *p_mt = &cpu_id_to_mt_info[cpu_id];

	if (cpu_id >= ARRAY_SIZE(cpu_id_to_mt_info)) {
		return -ERANGE;
	}
	*core = p_mt->core;
	*tc = p_mt->tc;
	return p_mt->linuxos ? 0 : 1;
}
#ifdef CONFIG_AVM_YIELD
/**
 * Absetzen eines Monitor-Befehls um synchron Infos von anderen Core zu erhalten
 * ret: 0 ok
 */
static int trigger_yield_monitor(unsigned int core, enum _monitor_ipi_type type,
				 void *buf)
{
	int ret = 0;
	unsigned long flags, retry = 1000;
	struct _monitor_ipi *m_ipi = &IPI_Monitor[core];

	if (core > YIELD_MONITOR_MAX_CORES) {
		return -EINVAL;
	}
	if (m_ipi->init == 0) {
		return -EACCES;
	}
	spin_lock_irqsave(&m_ipi->lock, flags);
	atomic_set(&m_ipi->ready, 0);
	m_ipi->type = type;
	wmb();
	if (cpu_id_to_mt_info[smp_processor_id()].core == core) {
		/*--- Umweg ueber yield nicht notwendig ---*/
		monitor_ipi_yield_context(0, m_ipi);
	} else {
		gic_trigger_irq(m_ipi->irq, 1);
	}
	while (retry && atomic_read(&m_ipi->ready) == 0) {
		udelay(1);
		retry--;
	}
	if (atomic_read(&m_ipi->ready)) {
		switch (type) {
		case monitor_bt:
			memcpy(buf, &m_ipi->buf.bt, sizeof(m_ipi->buf.bt));
			break;
		case monitor_nop:
		default:
			break;
		}
	} else {
		pr_err("%s: cpu=%u timeout_error", __func__, smp_processor_id());
		ret = -EACCES;
	}
	/*--- pr_err("%s: retry=%lu\n", __func__, retry); ---*/
	spin_unlock_irqrestore(&m_ipi->lock, flags);
	return ret;
}
#endif

#ifdef CONFIG_AVM_YIELD
#define READ_TCGPR(_ptregs, reg) ((_ptregs)->regs[reg] = mftgpr(reg))

/**
 */
static void fill_bt_data_per_core(struct _monitor_bt bt[], unsigned int el)
{
	unsigned long haltval;
	unsigned int tc;

	for (tc = 0; tc < el; tc++) {
		settc(tc);
		if (!(read_tc_c0_tcstatus() & TCSTATUS_A)) {
			bt[tc].valid = 0;
			continue;
		}
		if (read_tc_c0_tcbind() == read_c0_tcbind()) {
			/* Are we dumping ourself?  */
			haltval = 0; /* Then we're not halted, and mustn't be */
		} else {
			haltval = read_tc_c0_tchalt();
			write_tc_c0_tchalt(1);
		}
		READ_TCGPR(&bt[tc].ptregs, 1);
		READ_TCGPR(&bt[tc].ptregs, 2);
		READ_TCGPR(&bt[tc].ptregs, 3);
		READ_TCGPR(&bt[tc].ptregs, 4);
		READ_TCGPR(&bt[tc].ptregs, 5);
		READ_TCGPR(&bt[tc].ptregs, 6);
		READ_TCGPR(&bt[tc].ptregs, 7);
		READ_TCGPR(&bt[tc].ptregs, 8);
		READ_TCGPR(&bt[tc].ptregs, 9);
		READ_TCGPR(&bt[tc].ptregs, 10);
		READ_TCGPR(&bt[tc].ptregs, 11);
		READ_TCGPR(&bt[tc].ptregs, 12);
		READ_TCGPR(&bt[tc].ptregs, 13);
		READ_TCGPR(&bt[tc].ptregs, 14);
		READ_TCGPR(&bt[tc].ptregs, 15);
		READ_TCGPR(&bt[tc].ptregs, 16);
		READ_TCGPR(&bt[tc].ptregs, 17);
		READ_TCGPR(&bt[tc].ptregs, 18);
		READ_TCGPR(&bt[tc].ptregs, 19);
		READ_TCGPR(&bt[tc].ptregs, 20);
		READ_TCGPR(&bt[tc].ptregs, 21);
		READ_TCGPR(&bt[tc].ptregs, 22);
		READ_TCGPR(&bt[tc].ptregs, 23);
		READ_TCGPR(&bt[tc].ptregs, 24);
		READ_TCGPR(&bt[tc].ptregs, 25);
		READ_TCGPR(&bt[tc].ptregs, 26);
		READ_TCGPR(&bt[tc].ptregs, 27);
		READ_TCGPR(&bt[tc].ptregs, 28);
		READ_TCGPR(&bt[tc].ptregs, 29);
		READ_TCGPR(&bt[tc].ptregs, 30);
		READ_TCGPR(&bt[tc].ptregs, 31);

		bt[tc].ptregs.cp0_status = read_vpe_c0_status();
		bt[tc].ptregs.cp0_badvaddr = read_vpe_c0_badvaddr();
		bt[tc].ptregs.cp0_cause = read_vpe_c0_cause();
		bt[tc].ptregs.cp0_epc = read_tc_c0_tcrestart();
#ifdef CONFIG_MIPS_MT_SMTC
		bt[tc].ptregs.cp0_tcstatus = read_tc_c0_tcstatus();
#endif /* CONFIG_MIPS_MT_SMTC */
		bt[tc].valid = 1;
		if (!haltval) {
			write_tc_c0_tchalt(0);
		}
	}
}

/**
 * Yield-Context!
 */
static int monitor_ipi_yield_context(int signal __attribute__((unused)),
				     void *handle)
{
	struct _monitor_ipi *m_ipi = (struct _monitor_ipi *)handle;

	gic_trigger_irq(m_ipi->irq, 0);
	if (atomic_read(&m_ipi->ready)) {
		return YIELD_HANDLED;
	}
	switch (m_ipi->type) {
	case monitor_bt:
		fill_bt_data_per_core(m_ipi->buf.bt, ARRAY_SIZE(m_ipi->buf.bt));
		break;
	case monitor_nop:
	default:
		break;
	}
	wmb();
	atomic_set(&m_ipi->ready, 1);
	return YIELD_HANDLED;
}

/**
 */
static int bthelper_probe(struct platform_device *pdev)
{
	int ret, core;
	struct device_node *node = pdev->dev.of_node;
	struct resource irqres[2];

	ret = of_irq_to_resource_table(node, irqres, 2);
	if (ret != 2) {
		pr_err("%s: interrupts not found in dt\n", __func__);
		return -ENODEV;
	}
	for (core = 0; core < YIELD_MONITOR_MAX_CORES; core++) {
		int irq = irqres[core].start;

		spin_lock_init(&IPI_Monitor[core].lock);
		IPI_Monitor[core].init = 0;
		IPI_Monitor[core].core = core;
		IPI_Monitor[core].irq = irq;
		ret = gic_map_setup(
			YIELD_CPU_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			irq, 2 /*--- yield ---*/,
			YIELD_SIGNAL_BY_ID(YIELD_MONITOR_IPI_ID(core)));
		if (ret) {
			pr_err("%s: error %d on gic_map_setup(%u,%u)",
			       __func__, ret,
			       YIELD_CPU_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			       irq);
			return -ENODEV;
		}
		ret = gic_map_irq_type(irq, irqres[core].flags & IRQF_TRIGGER_MASK);
		if (ret) {
			pr_err("%s: error %d on gic_map_irq_type(%u)",
			       __func__, ret, irq);
			return -ENODEV;
		}
		ret = request_yield_handler_on(
			YIELD_CPU_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			YIELD_TC_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			YIELD_SIGNAL_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			monitor_ipi_yield_context, &IPI_Monitor[core]);
		if (ret < 0) {
			pr_err("%s: error %d on request_yield_handler_on(%u,%u,%u)",
			       __func__, ret,
			       YIELD_CPU_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			       YIELD_TC_BY_ID(YIELD_MONITOR_IPI_ID(core)),
			       YIELD_SIGNAL_BY_ID(YIELD_MONITOR_IPI_ID(core)));
			return -ENODEV;
		}
		IPI_Monitor[core].init = 1;
		atomic_set(&backtrace_busy, 0);
		pr_err("%s: core %d initialized\n", __func__, core);
	}
	return 0;
}

static const struct of_device_id bthelper_match[] = {
	{ .compatible = "avm,bthelper" },
	{},
};

MODULE_DEVICE_TABLE(of, bthelper_match);

static struct platform_driver bthelper_driver = {
	.probe = bthelper_probe,
	.driver = {
		.name = "avm-bthelper",
		.of_match_table = bthelper_match,
		.owner = THIS_MODULE,
	},
};
builtin_platform_driver(bthelper_driver);

static struct _monitor_bt bt[YIELD_MONITOR_MAX_TC];
extern unsigned long kernelsp[NR_CPUS];

/**
 */
static inline int is_yieldthread(unsigned int tc)
{
	return (tc >= YIELD_FIRST_TC) && (tc <= YIELD_LAST_TC);
}
#endif
/**
 * CPU0-CPU1: CORE0: VPE0, VPE1
 * CPU2-CPU3: CORE1: VPE0, VPE1
 */
bool arch_trigger_all_cpu_backtrace(const cpumask_t *cpu_mask)
{
#ifdef CONFIG_AVM_YIELD
	char txtbuf[2][64];
	unsigned long gp;
	struct task_struct *task;
	int act_cpu = smp_processor_id();
	unsigned int core;
	char *name;

	if (atomic_add_return(1, &backtrace_busy) > 1) {
		return 0;
	}
	for (core = 0; core < YIELD_MONITOR_MAX_CORES; core++) {
		unsigned int tc;

		memset(&bt, 0, sizeof(bt));

		if (trigger_yield_monitor(core, monitor_bt, &bt)) {
			continue;
		}
		for (tc = 0; tc < ARRAY_SIZE(bt); tc++) {
			int cpuid;
			struct pt_regs *ptregs = &bt[tc].ptregs;

			if (bt[tc].valid == 0) {
				continue;
			}
			cpuid = get_cpuid_by_mt(core, tc, &name);

			if ((cpuid < 0) && !is_yieldthread(tc)) {
				/*--- sowas wie MPE-Fw: keine Linux-konformes thread_info ---*/
				pr_err("---- CORE%u TC%u ----- (%s)\n",
				       core, tc, name);
				print_cp0_status(ptregs->cp0_status);
				pr_err("[<%p>] %pS\n[<%p>] %pS\n\n",
				       (void *)ptregs->cp0_epc,
				       (void *)ptregs->cp0_epc,
				       (void *)ptregs->regs[31],
				       (void *)ptregs->regs[31]);
				continue;
			}
			if (cpu_mask && (cpuid >= 0) &&
			    (cpumask_test_cpu(cpuid, cpu_mask) == 0)) {
				continue;
			}
			if ((act_cpu == cpuid) && !user_mode(ptregs) &&
			    (in_nmi() || in_irq())) {
				if (get_irq_regs()) {
					ptregs = get_irq_regs(); /*--- wir wollen nicht den aktuellen Irq/NMI/Exception-Kontext tracen ---*/
				} else {
					pr_err("%s: tc=%u act_cpu=%u cpuid=%u get_irq_regs() == NULL - check genex.S\n",
					       __func__, tc, act_cpu, cpuid);
				}
			}
			if ((cpuid >= 0) && user_mode(ptregs)) {
				/*--- wir sind im USER-Mode: Spezialbehandlung fuer gp-Ermittlung (da virtuelle Adressen) ---*/
				gp = kernelsp[cpuid] & ~THREAD_MASK; /*--- kseg0-gp ermitteln ---*/
			} else {
				gp = ptregs->regs[28]; /*--- kseg0-gp ermitteln ---*/
			}
			if (cpuid >= 0) {
				pr_err("---- CPU_ID=%u CORE%u TC%u ----- (%s)\n",
				       cpuid, core, tc, name);
				print_cp0_status(ptregs->cp0_status);
			} else {
				pr_err("---- CORE%u TC%u ----- (YIELD-Thread)\n",
				       core, tc);
			}
			if (virt_addr_valid(gp) && ((gp & THREAD_MASK) == 0)) { /*--- valid gp-address ? ---*/
				const int field = 2 * sizeof(unsigned long);
				struct thread_info *thread = (struct thread_info *)gp;

				task = thread->task;
				if (!virt_addr_valid(task)   /*--- valide kseg0-task-address ? ---*/) {
					pr_err("invalid task-pointer %p\n", task);
					continue;
				}
				pr_err("current: %s (pid: %d, threadinfo=%p, task=%p, sp=%08lx tls=0x%0*lx)\n",
				       task->comm, task->pid, thread, task, ptregs->regs[29], field, thread->tp_value);
				if (!user_mode(ptregs)) {
					show_backtrace(task, ptregs);
					continue;
				}
				pr_err("[<%08lx>] %s\n[<%08lx>] %s\n",
				       ptregs->cp0_epc,
				       get_user_symbol(txtbuf[0], sizeof(txtbuf[0]), gp, ptregs->cp0_epc),
				       ptregs->regs[31],
				       get_user_symbol(txtbuf[1], sizeof(txtbuf[1]), gp, ptregs->regs[31]));
			}
		}
	}
	atomic_set(&backtrace_busy, 0);
	return 1;
#else
	return 0;
#endif
}

#define IS_MIPS16_EXTEND_OR_JAL(a)                                             \
	((((a) >> (27 - 16)) == 30) |                                          \
	 (((a) >> (27 - 16)) == 3)) /*--- Opcode EXTEND oder JAL ---*/
/**
 */
static int check_pc_adress(unsigned int __user *pc, unsigned int usermode)
{
	if (((unsigned long)pc & (0xFF << 24)) == (PAGE_POISON << 24)) {
		return 1;
	}
	if (((unsigned long)pc & (0xFF << 24)) == (POISON_INUSE << 24)) {
		return 2;
	}
	if (((unsigned long)pc & (0xFF << 24)) == (POISON_FREE << 24)) {
		return 3;
	}
	if (((unsigned long)pc & (0xFF << 24)) == (POISON_END << 24)) {
		return 4;
	}
	if (((unsigned long)pc & (0xFF << 24)) == (POISON_FREE_INITMEM << 24)) {
		return 5;
	}
	if (!usermode &&
	    ((unsigned long)!kernel_text_address((unsigned long)pc))) {
		return 6;
	}
	return 0;
}
/**
 */
static int print_code_range(struct seq_file *seq, const char *prefix,
			    unsigned int __user *pc, unsigned int mips16,
			    unsigned int usermode, int left_offset,
			    int right_offset, unsigned long *badaddr)
{
	int ret;

	ret = check_pc_adress(pc, usermode);
	if (ret) {
		if (seq == NULL)
			pr_err("no code-dump, address could be %s (no memory at this address).\n",
			       ret == 1 ? "PAGE_POISON" :
			       ret == 2 ? "POISON_INUSE" :
			       ret == 3 ? "POISON_FREE" :
			       ret == 4 ? "POISON_END" :
			       ret == 5 ? "POISON_FREE_INITMEM" : "INVAL_KSEGx");
		return 0;
	}
	if (!mips16) {
		signed int i;
		unsigned long access_addr = (unsigned long)pc + sizeof(unsigned int) * left_offset;

		if (seq == NULL)
			pr_err("Code(0x%08lx):", access_addr);
		for (i = left_offset; i < right_offset; i++) {
			unsigned int pc_value;

			access_addr = (unsigned long)pc + sizeof(unsigned int) * i;
			if (usermode) {
				if (!unlikely(access_ok(VERIFY_READ, access_addr, 4))) {
					if (seq == NULL)
						pr_err("[%s] illegal address 0x%lx (sigill)\n", prefix, access_addr);
					*badaddr = (unsigned long)access_addr;
					return -1; /*--- sigill; ---*/
				}
				if (__get_user(pc_value, (unsigned int __user *)access_addr)) {
					if (seq == NULL)
						pr_err("[%s] load from address 0x%lx failed (sigbus)\n", prefix,
										 access_addr);
					*badaddr = (unsigned long)access_addr;
					return -2; /*--- sigbus; ---*/
				}
			} else {
				pc_value = *((unsigned int *)access_addr);
			}
			if (seq) {
				seq_printf(seq, "%s %s0x%08x%s",
					   i == left_offset ? prefix : "",
					   i == 0 ? "<" : "", pc_value,
					   i == 0 ? ">" : "");
			} else {
				pr_cont(" %s0x%08x%s", i == 0 ? "<" : "", pc_value, i == 0 ? ">" : "");
			}
		}
	} else {
		/*--- wegen EXT-Code nur step by step vorhangeln: ---*/
		unsigned short code0, code1;
		unsigned long pc_addr = (unsigned long)pc & ~0x1;
		unsigned long access_addr = (unsigned long)pc & ~0x1;
		unsigned long end_addr = ((unsigned long)pc & ~0x1) + sizeof(unsigned short) * right_offset;

		while (left_offset < 0) {
			if (usermode && !unlikely(access_ok(VERIFY_READ, access_addr, 4))) {
				if (seq == NULL)
					pr_err("[%s] illegal address 0x%lx (sigill)\n", prefix, access_addr);
				*badaddr = (unsigned long)access_addr;
				return -1; /*--- sigill; ---*/
			}
			if (__get_user(code1, (unsigned short __user *)(access_addr - sizeof(short)))) {
				if (seq == NULL)
					pr_err("[%s] load from 16 bit address 0x%lx failed (sigbus)\n", prefix,
								access_addr);
				*badaddr = (unsigned long)access_addr;
				return -2; /*--- sigbus; ---*/
			}
			if (__get_user(code0, (unsigned short __user *)(access_addr - 2 * sizeof(short)))) {
				if (seq == NULL)
					pr_err("[%s] load from 16 bit address 0x%lx failed (sigbus)\n", prefix,
								access_addr);
				*badaddr = (unsigned long)access_addr;
				return -2; /*--- sigbus; ---*/
			}
			if (IS_MIPS16_EXTEND_OR_JAL(code0)) {
				access_addr -= 2 * sizeof(short);
			} else {
				access_addr -= sizeof(short);
			}
			left_offset++;
		}
		if (seq) {
			seq_printf(seq, "%s", prefix);
		} else {
			pr_err("Code(0x%08lx):", access_addr);
		}
		while (access_addr < end_addr) {
			if (__get_user(code0, (unsigned short __user *)(access_addr))) {
				if (seq == NULL)
					pr_err("[%s] load from 16 bit address 0x%lx failed (sigbus)\n",
					       prefix, access_addr);
				*badaddr = (unsigned long)access_addr;
				return -2; /*--- sigbus; ---*/
			}
			if (access_addr == pc_addr) {
				if (IS_MIPS16_EXTEND_OR_JAL(code0)) {
					access_addr += sizeof(short);
					if (__get_user(code1, (unsigned short __user *)(access_addr))) {
						if (seq == NULL)
							pr_err("[%s] load from 16 bit address 0x%lx failed (sigbus)\n",
							       prefix, access_addr);
						*badaddr = (unsigned long)access_addr;
						return -2; /*--- sigbus; ---*/
					}
					if (seq) {
						seq_printf(seq, " <0x%04x %04x>", code0, code1);
					} else {
						pr_cont(" <0x%04x %04x>", code0, code1);
					}
				} else {
					if (seq) {
						seq_printf(seq, " <0x%04x>", code0);
					} else {
						pr_cont(" <0x%04x>", code0);
					}
				}
			} else {
				if (seq) {
					seq_printf(seq, " 0x%04x", code0);
				} else {
					pr_cont(" 0x%04x", code0);
				}
			}
			access_addr += sizeof(short);
		}
	}
	if (seq == NULL)
		pr_cont("\n");
	return 0;
}
/**
 */
static int __get_userinfo(char *buf, unsigned int maxbuflen,
			  struct mm_struct *mmm, unsigned long addr)
{
	struct vm_area_struct *vm;
	struct vm_area_struct *vm_last = NULL;
	unsigned int i = 0;

	if (!virt_addr_valid(mmm)) {
		return 1;
	}
	vm = mmm->mmap;
	while (vm) {
		/*--- pr_err("%s[%x]:%p %x - %x vm_mm %p\n", __func__, addr, vm, vm->vm_start, vm->vm_end, vm->vm_mm); ---*/
		if (!virt_addr_valid(vm)) {
			snprintf(buf, maxbuflen, "%s: error: corrupt vm %p vm_last=%pS\n", __func__, vm, vm_last);
			return 1;
		}
		if ((addr >= vm->vm_start) && (addr < vm->vm_end)) {
			snprintf(buf, maxbuflen, "seg=%3u of=0x%08lx/0x%lx [%s]", i, addr - (unsigned long)vm->vm_start,
				 (unsigned long)vm->vm_end - (unsigned long)vm->vm_start,
				 (vm->vm_file && vm->vm_file->f_path.dentry) ? (char *)vm->vm_file->f_path.dentry->d_name.name : ""
				 );
			/*--- pr_err("%s", buf); ---*/
			return 0;
		}
		vm_last = vm;
		vm = vm->vm_next;
		i++;
	}
	return 1;
}

/**
 *  gp muss valide (nicht-virtuelle) Kerneladresse sein
 *  Schwachstelle: thread/task nicht mehr valid (in dem Moment freigegeben)
 */
char *get_user_symbol(char *txt, unsigned int maxttxlen, unsigned long gp, unsigned long addr)
{
	unsigned long flags;
	struct task_struct *task = NULL;
	struct thread_info *thread = (struct thread_info *)gp;

	txt[0] = 0;
	if (virt_addr_valid(gp) && ((gp & THREAD_MASK) == 0)) { /*--- valide gp-address ? ---*/
		return txt;
	}
	local_irq_save(flags);
	task = thread->task;
	if (task == NULL) {
		local_irq_restore(flags);
		return txt;
	}
	if (spin_trylock(&task->alloc_lock)) {
		/**
		 * kritisch: mmput()/get_task_mm() darf im Irq-Kontext nicht verwendet werden,
		 * mmput hoffentlich nur im task_lock()
		 */
		__get_userinfo(txt, maxttxlen, task->mm, addr);
		spin_unlock(&task->alloc_lock);
	}
	local_irq_restore(flags);
	return txt;
}

/**
 */
static int print_mem_config(struct mm_struct *mmm, unsigned long addr)
{
	struct vm_area_struct *vm;
	struct vm_area_struct *vm_last = NULL;
	unsigned int i = 0;

	if (!virt_addr_valid(mmm))
		return 0;
	vm = mmm->mmap;
	while (vm) {
		if (!virt_addr_valid(vm)) {
			pr_err("%s: error: corrupt vm %p vm_last=%pS\n", __func__, vm, vm_last);
			return 0;
		}
		if ((addr >= vm->vm_start) && (addr < vm->vm_end)) {
			pr_err("Adresse-Segment(%d): 0x%lx: 0x%lx :0x%lx (offset 0x%lx)",
					i, vm->vm_start, addr, vm->vm_end, addr - vm->vm_start);
			if (vm->vm_file) {
				pr_err(" Path='%s'", vm->vm_file->f_path.dentry->d_name.name);
			}
			pr_err("\n");
			return 0;
		}
		vm_last = vm;
		vm = vm->vm_next;
		i++;
	}
	return 1;
}

/**
 */
void show_code_position_by_epc(char *prefix, struct pt_regs *regs)
{
	unsigned int __user *pc;
	unsigned long addr = 0UL;

	pc = (unsigned int __user *)exception_epc(regs);
	pr_err("[%s] pc=0x%p(%pS) addr=0x%08lx task=%s pid=%d ra=0x%08lx(%pS)\n",
	       prefix, pc, pc, regs->cp0_badvaddr, current->comm, current->pid,
			regs->regs[31], (void *)regs->regs[31]
		  );
	print_code_range(NULL, prefix, pc, regs->cp0_epc & 0x1, user_mode(regs), -2, 3, &addr);
	if (user_mode(regs)) {
		if (print_mem_config(current->active_mm, (unsigned long)pc))
			print_mem_config(current->mm, (unsigned long)pc);
	}
}

/**
 * Unaligned-Helper
 */
#define UNALIGNED_WARN 0x1
#define UNALIGNED_BACKTRACE 0x8
enum _unaligned_mode {
	UNALIGNED_ACTION_FIXUP = 2,
	UNALIGNED_ACTION_FIXUP_WARN        = 3 | UNALIGNED_WARN, /*-- verodern eigentlich Dummy :-) ---*/
	UNALIGNED_ACTION_SIGNAL = 4,
	UNALIGNED_ACTION_SIGNAL_WARN       = 5 | UNALIGNED_WARN, /*-- verodern eigentlich Dummy :-) ---*/
	UNALIGNED_ACTION_FIXUP_WARN_BT     = UNALIGNED_ACTION_FIXUP_WARN | UNALIGNED_WARN | UNALIGNED_BACKTRACE
};

static int ai_usermode = UNALIGNED_ACTION_FIXUP
	/*--- | UNALIGNED_WARN ---*/
	;
static int ai_kernelmode = UNALIGNED_ACTION_FIXUP
	/*--- | UNALIGNED_WARN ---*/
	;

#define DEVELOPER_TIME_MEASURE_SEC  (60)
#define DEVELOPER_TIME_MEASURE_PERIOD 8
#define DEVELOPER_TIME_WINDOW_SEC   (DEVELOPER_TIME_MEASURE_SEC * DEVELOPER_TIME_MEASURE_PERIOD)
#define DEVELOPER_WARNING_THRESHOLD (DEVELOPER_TIME_WINDOW_SEC)	/* one unaligned per second */

static struct _ai_info {
	unsigned long ai_count;
	unsigned long last_pc;
	char last_comm[TASK_COMM_LEN];

	int ai_mode;
	unsigned long ts_window;
	unsigned long ai_count_ts;
} ai_info[2];

#define AI_SYS 0
#define AI_USER 1

#define ai_user ai_info[AI_USER].ai_count
#define ai_sys ai_info[AI_SYS].ai_count

/**
 */
static inline int inc_ai_info(int ai_idx, unsigned long pc)
{
	struct _ai_info *pai = &ai_info[ai_idx];
	int ai_mode;

	pai->ai_count++;
	pai->last_pc = pc;
	memcpy(pai->last_comm, current->comm, TASK_COMM_LEN);

	ai_mode = pai->ai_mode;
	if (likely(ai_mode == 0))
		return 0;

	if (time_is_before_jiffies(pai->ts_window)) {
		unsigned long diff;

		pai->ts_window = jiffies + (DEVELOPER_TIME_MEASURE_SEC * HZ);
		/* approximate ... because not normalized with ts-window
		 * (ts-window can be greater)
		 */
		diff = pai->ai_count - pai->ai_count_ts;
		pr_debug("%s:%s ai_count=%lu: ai_count_ts=%lu (diff=%lu)\n",
			 __func__,
			 ai_idx == AI_SYS ? "SYS" : "USER",
			 pai->ai_count, pai->ai_count_ts, diff);

		/* slide approx. DEVELOPER_TIME_MEASURE_SEC */
		pai->ai_count_ts = pai->ai_count - (diff * (DEVELOPER_TIME_MEASURE_PERIOD - 1)) / ( DEVELOPER_TIME_MEASURE_PERIOD);
		if (diff > DEVELOPER_WARNING_THRESHOLD) {
			pai->ai_mode = 0;
			return ai_mode;
		}
	}
	return 0;
}

static inline void setup_developer_ai_info(int ai_idx, int ai_mode)
{
	struct _ai_info *pai = &ai_info[ai_idx];

	pai->ts_window = jiffies;
	pai->ai_mode   = ai_mode;
}

#define UNALIGNED_MAX_SCORE_ENTRIES 8
#if defined(UNALIGNED_MAX_SCORE_ENTRIES)
/**
 */
struct _unaligned_access_score_entry {
	unsigned long unaligneds;
	unsigned long ts;
	unsigned short mips16;
	unsigned short caddr_of; /*--- > 0: pc-offset in codetable in bytes---*/
#define CODE_ENTRIES 5
	unsigned int code[CODE_ENTRIES];    /*--- Code muss gesichert werden, da evtl. Virtual Memory ---*/
	char shortcomm[8]; /*--- Name mal sichern falls beendet ---*/
	pid_t pid;
	void *pc;
};
#define LEFT_CODE_OFFSET 2
#define RIGHT_CODE_OFFSET(a) min((a), CODE_ENTRIES - LEFT_CODE_OFFSET)

/**
 * 0: Fehler
 * sonst: liefert byte-offset zum pc
 */
static unsigned short cpy_code_range(unsigned int __user *pc,
				     unsigned int usermode,
				     unsigned int codetable[],
				     unsigned int code_entries, int left_offset)
{
	signed int i;
	unsigned int idx = 0;
	int right_offset;
	unsigned short ret = 0;
	int mips16_offset = ((unsigned long)pc) & 0x2;
	unsigned long access_addr = ((unsigned long)pc) & ~0x3;

	right_offset = left_offset + code_entries;
	if (check_pc_adress(pc, usermode)) {
		memset(codetable, 0, code_entries * sizeof(unsigned int));
		return ret;
	}
	access_addr += sizeof(unsigned int) * left_offset;
	for (i = left_offset; i < right_offset; i++) {
		unsigned int pc_value;

		if (usermode) {
			if (!unlikely(access_ok(VERIFY_READ, access_addr, 4))) {
				return ret;
			}
			if (__get_user(pc_value,
				       (unsigned int __user *)access_addr)) {
				return ret;
			}
		} else {
			pc_value = *((unsigned int *)access_addr);
		}
		codetable[idx] = pc_value;
		if (i == 0) {
			ret = (unsigned char *)&codetable[idx] -
			      (unsigned char *)&codetable[0] + mips16_offset;
		}
		idx++;
		access_addr += sizeof(unsigned int);
	}
	return ret;
}

/**
 * usermode: 0 Kernel
 *           1 MIPS32
 *           2 MIPS16
 */
static void
add_unaligned_access_to_table(void *pc,
			      struct _unaligned_access_score_entry table[],
			      unsigned int table_entries, unsigned int usermode)
{
	unsigned int ts_idx = 0, ts_score = 0;
	unsigned int i;

	for (i = 0; i < table_entries; i++) {
		if (table[i].unaligneds) {
			if (usermode) {
				if (task_pid_nr(current) != table[i].pid) {
					continue;
				}
			}
			if (pc == table[i].pc) {
				table[i].unaligneds++;
				/*--- pr_err("%s:[%u]add %s %s pid=%u pc=%p %lu\n", __func__, i, usermode ? "USER" : "KERNEL",  current->comm, table[i].pid, pc, table[i].unaligneds); ---*/
				return;
			}
			continue;
		}
		/*--- pr_err("%s:[%u]initial %s %s pc=%p pid=%u\n", __func__, i, usermode ? "USER" : "KERNEL",  current->comm, pc, table[i].pid); ---*/
		ts_idx = i;
		goto table_settings;
	}
	/*--- alle besetzt: bewerte per unaligneds / per time ---*/
	for (i = 0; i < table_entries; i++) {
		unsigned long diff = jiffies - table[i].ts;
		unsigned int score = diff / (table[i].unaligneds | 0x1);
		/*--- pr_err("%s:[%u]score %d diff %u unaligneds=%lu ts_score %u idx=%u\n", __func__,  i, score, diff, table[i].unaligneds, ts_score, ts_idx); ---*/
		if (score > ts_score) {
			ts_score = score;
			ts_idx = i;
		}
	}
	/*--- pr_err("%s:[%u]replace %s old: unaligneds=%lu pc=%p pid=%u ts_score=%u new=%s pc=%p\n", __func__, ts_idx, usermode ? "USER" : "KERNEL", table[ts_idx].unaligneds, table[ts_idx].pc, table[ts_idx].pid, ts_score, current->comm, pc); ---*/
table_settings:
	table[ts_idx].unaligneds = 1;
	table[ts_idx].pc = pc;
	table[ts_idx].ts = jiffies;
	table[ts_idx].pid = task_pid_nr(current);
	table[ts_idx].mips16 = (usermode & 0x2) ? 1 : 0;
	table[ts_idx].caddr_of =
		cpy_code_range(pc, usermode, table[ts_idx].code,
			       ARRAY_SIZE(table[ts_idx].code),
			       -LEFT_CODE_OFFSET);
	strncpy(table[ts_idx].shortcomm, current->comm,
		sizeof(table[ts_idx].shortcomm));
}

/**
 */
static char *norm_ai_per_timer(char *txt, unsigned int txtlen, unsigned long ai, unsigned int sec)
{
	unsigned long long tmp, value;
	unsigned long rem;
	char *unit = "s";

	if (sec >= 60 * 60) {
		unit  = "h";
		tmp   = (unsigned long long)ai * 60 * 60 * 100;
		value = (unsigned long long)ai * 60 * 60;
	} else if (sec >= 60) {
		unit  = "min";
		tmp   = (unsigned long long)ai * 60 * 100;
		value = (unsigned long long)ai * 60;
	} else {
		if (sec == 0)
			sec = 1;
		tmp   = (ai * 100);
		value = ai;
	}
	do_div(tmp, sec);
	rem = do_div(tmp, 100);
	do_div(value, sec);
	snprintf(txt, txtlen, "%5lu.%02lu/%s", (unsigned long)value, rem, unit);
	return txt;
}

/**
 */
static unsigned long
show_unaligned_access_table(struct seq_file *seq,
			    struct _unaligned_access_score_entry table[],
			    unsigned int table_entries, unsigned int usermode)
{
	char text[64];
	struct timespec uptime;
	char comm[sizeof(table[0].shortcomm) + 1];
	unsigned int i;
	unsigned long sum = 0;
	unsigned long address;
	unsigned long boot_sec;
	unsigned long boot_ai;

	get_monotonic_boottime(&uptime);
	boot_sec = uptime.tv_sec;
	boot_ai  = ai_info[usermode ? AI_USER : AI_SYS].ai_count;
	seq_printf(seq, "%s:\nunaligneds\t unaligneds/time (all: %s)\t\n",
		   usermode ? "User-Scorelist" : "System-Scorelist",
		   norm_ai_per_timer(text, sizeof(text), boot_ai, boot_sec));
	for (i = 0; i < table_entries; i++) {
		if (table[i].unaligneds) {
			unsigned long ai_per_time = table[i].unaligneds;
			unsigned long sec = (jiffies - table[i].ts) / HZ;

			sum += table[i].unaligneds;
			if (sec == 0)
				sec = 1;
			if (usermode) {
				struct pid *ppid;
				struct task_struct *tsk = NULL;
				char *pcomm;

				ppid = find_get_pid(table[i].pid);
				if (ppid) {
					tsk = get_pid_task(ppid, PIDTYPE_PID);
				}
				if (tsk) {
					pcomm = tsk->comm;
				} else {
					memcpy(comm, table[i].shortcomm, sizeof(comm) - 1);
					comm[sizeof(comm) - 1] = 0;
					pcomm = comm;
					tsk = NULL;
				}
				seq_printf(seq, "%10lu \t%s \t %s(%u) pc=0x%p ",
						table[i].unaligneds,
						norm_ai_per_timer(text, sizeof(text), ai_per_time, sec),
						pcomm,
						table[i].pid, table[i].pc);
				if (table[i].caddr_of) {
					print_code_range(seq, ":",
							(unsigned int __user *)((unsigned long)&table[i].code + table[i].caddr_of),
							table[i].mips16, 0 /* use copy in kernel space */, -LEFT_CODE_OFFSET,
							RIGHT_CODE_OFFSET(2), &address);
				}
				if (tsk) {
					put_task_struct(tsk);
				}
				if (ppid) {
					put_pid(ppid);
				}
				seq_puts(seq, "\n");
			} else {
				seq_printf(seq, "%10lu \t%s \t 0x%p(%pS) ", table[i].unaligneds,
						norm_ai_per_timer(text, sizeof(text), ai_per_time, sec),
						table[i].pc, table[i].pc);
				if (table[i].caddr_of) {
					print_code_range(seq, ":",
							(unsigned int __user *)((unsigned long)&table[i].code + table[i].caddr_of),
							table[i].mips16, 0, -LEFT_CODE_OFFSET, RIGHT_CODE_OFFSET(2), &address);
				}
				seq_puts(seq, "\n");
			}
		}
	}
	return sum;
}
static struct _unaligned_access_score_entry
	user_score_table[UNALIGNED_MAX_SCORE_ENTRIES],
	sys_score_table[UNALIGNED_MAX_SCORE_ENTRIES];
#endif /*--- #if defined(UNALIGNED_MAX_SCORE_ENTRIES) ---*/
/**
 * \\brief:
 * Liefere Unaligned-Daten
 * user: 0 Kernel 1 Userland
 * ret: Zeiger auf last_comm des Userprozesses
 */
const char *get_last_unaligned_info(unsigned long *ai_count,
				    unsigned long *last_pc, int user)
{
	int idx = user ? AI_USER : AI_SYS;

	if (ai_count)
		*ai_count = ai_info[idx].ai_count;
	if (last_pc)
		*last_pc = ai_info[idx].last_pc;
	return ai_info[idx].last_comm;
}

int track_unaligned_position(struct pt_regs *regs)
{
	unsigned long addr = 0UL;
	unsigned int __user *pc;

	pc = (unsigned int __user *)exception_epc(regs);

	if (!user_mode(regs)) {
		ai_kernelmode |= inc_ai_info(AI_SYS, (unsigned long)pc);
#if defined(UNALIGNED_MAX_SCORE_ENTRIES)
		add_unaligned_access_to_table(pc, sys_score_table,
					      ARRAY_SIZE(sys_score_table), 0);
#endif /*--- #if defined(UNALIGNED_MAX_SCORE_ENTRIES) ---*/
		if (ai_kernelmode & UNALIGNED_WARN) {
			int ret;

			pr_err("[kernel-unaligned %lu] pc=0x%p(%pS) addr=0x%08lx task=%s pid=%d ra=0x%08lx(%pS)\n",
				  ai_sys, pc, pc, regs->cp0_badvaddr, current->comm, current->pid,
				  regs->regs[31], (void *)regs->regs[31]
			);
			ret = print_code_range(NULL, "kernel-unaligned", pc, regs->cp0_epc & 0x1, user_mode(regs), -2, 3, &addr);
			if (ret) {
				return ret;
			}
			if (print_mem_config(current->active_mm, (unsigned long)pc))
				print_mem_config(current->mm, (unsigned long)pc);

			if (ai_kernelmode & UNALIGNED_BACKTRACE) {
				show_backtrace(current, regs);
			}
		}
		return (ai_kernelmode & UNALIGNED_ACTION_SIGNAL) ? -2 : 0;
	}
	ai_usermode |= inc_ai_info(AI_USER, (unsigned long)pc);
#if defined(UNALIGNED_MAX_SCORE_ENTRIES)
	add_unaligned_access_to_table(pc, user_score_table,
				      ARRAY_SIZE(user_score_table),
				      1 + (regs->cp0_epc & 0x1));
#endif /*--- #if defined(UNALIGNED_MAX_SCORE_ENTRIES) ---*/

	if (ai_usermode & UNALIGNED_WARN) {
		int ret;

		pr_err("Alignment trap %lu: %s (%d) PC=0x%p Address=0x%08lx\n",
		       ai_user,
		       current->comm, task_pid_nr(current), pc,
		       regs->cp0_badvaddr);
		ret = print_code_range(NULL, "kernel-unaligned", pc,
				       regs->cp0_epc & 0x1, user_mode(regs), -2,
				       3, &addr);
		if (ret) {
			return ret;
		}
		if (print_mem_config(current->active_mm, (unsigned long)pc)) {
			print_mem_config(current->mm, (unsigned long)pc);
		}
		/*--- show_registers(regs); ---*/
	}
	return (ai_usermode & UNALIGNED_ACTION_SIGNAL) ? -2 : 0;
}

/**
 */
static const char *map_mode(enum _unaligned_mode mode)
{
	switch (mode) {
	case UNALIGNED_ACTION_FIXUP:
		return "(fixup)      ";
	case UNALIGNED_ACTION_FIXUP_WARN:
		return "(fixup+warn) ";
	case UNALIGNED_ACTION_SIGNAL:
		return "(signal)     ";
	case UNALIGNED_ACTION_SIGNAL_WARN:
		return "(signal+warn)";
	case UNALIGNED_ACTION_FIXUP_WARN_BT:
		return "(fixup+warn+backtrace)";
	}
	return "";
}
/*-
 */
static void unalignment_proc_read(struct seq_file *seq,
				  void *ref __maybe_unused)
{
	seq_printf(seq, "User:\t\t%lu\n", ai_user);
	seq_printf(seq, "System:\t\t%lu\n", ai_sys);
	seq_printf(seq, "User faults:\t%i %s\n", ai_usermode,
		   map_mode(ai_usermode));
	seq_printf(seq, "Kernel faults:\t%i %s\n", ai_kernelmode,
		   map_mode(ai_kernelmode));
#if defined(UNALIGNED_MAX_SCORE_ENTRIES)
	if (ai_user) {
		if (show_unaligned_access_table(seq, user_score_table,
						ARRAY_SIZE(user_score_table),
						1) != ai_user) {
			seq_puts(seq,
				 "... only the newest user-unaligneds shown\n");
		}
	}
	if (ai_sys) {
		if (show_unaligned_access_table(seq, sys_score_table,
						ARRAY_SIZE(sys_score_table),
						0) != ai_sys) {
			seq_puts(seq,
				 "... only the newest kernel-unaligneds shown\n");
		}
	}
#endif /*--- #if defined(UNALIGNED_MAX_SCORE_ENTRIES) ---*/
}
/*--- #define DSP_ASE_UNALIGNED_CHECK ---*/
#if defined(DSP_ASE_UNALIGNED_CHECK)

/**
 */
static inline unsigned int lwx_unaligned_check(void *addr, unsigned int index)
{
	unsigned int erg;

	__asm__ __volatile__(".set\tnoat\n"
			     "LWX %0, %1(%2)\n"
			     : "=r"(erg)
			     : "r"(index), "r"((unsigned long)addr)
			     : "cc");
	return erg;
}

/**
 */
static inline signed int lhx_unaligned_check(void *addr, unsigned int index)
{
	signed int erg;

	__asm__ __volatile__(".set\tnoat\n"
			     "LHX %0, %1(%2)\n"
			     : "=r"(erg)
			     : "r"(index), "r"((unsigned long)addr)
			     : "cc");
	return erg;
}

/**
 */
static unsigned int test_table[] = {
	  0x1 <<  0, 0xF1 <<  0, 0x3 <<  0, 0xFF <<  0,
	  0x1 <<  8, 0xF1 <<  8, 0x3 <<  8, 0xFF <<  8,
	  0x1 << 16, 0xF1 << 16, 0x3 << 16, 0xFF << 16,
	  0x1 << 24, 0xF1 << 24, 0x3 << 24, 0xFF << 24
};

/**
 */
static signed short test2_table[] = {
	0x1 << 0, 0xF1 << 0, 0x3 << 0, 0xFF << 0,
	0x1 << 8, 0xF1 << 8, 0x3 << 8, 0xFF << 8,
};

/**
 */
static void check_dsp_ase_unaligneds(void)
{
	unsigned int i;
	unsigned int unalignedtable[16 + 1];
	unsigned int *p = (unsigned int *)((unsigned char *)unalignedtable + 1);

	memcpy(p, test_table, sizeof(test_table));
	for (i = 0; i < ARRAY_SIZE(test_table); i++) {
		unsigned int val = lwx_unaligned_check(p, i * 4);

		if (val != test_table[i]) {
			pr_err("%s:#1 error: val(%x)  != table[%u] (%x)\n", __func__, val, i, test_table[i]);
		}
	}
	for (i = 0; i < ARRAY_SIZE(test_table); i++) {
		unsigned int val = lwx_unaligned_check(unalignedtable, (i * 4) + 1);

		if (val != test_table[i]) {
			pr_err("%s:#2 error: val(%x)  != table[%u] (%x)\n", __func__, val, i, test_table[i]);
		}
	}
	memcpy(p, test2_table, sizeof(test2_table));
	for (i = 0; i < ARRAY_SIZE(test2_table); i++) {
		signed int val = lhx_unaligned_check(p, i * 2);

		if (val != (signed int)test2_table[i]) {
			pr_err("%s:#3 error: val(%x)  != table[%u] (%x)\n", __func__, val, i, (signed int)test2_table[i]);
		}
	}
	for (i = 0; i < ARRAY_SIZE(test2_table); i++) {
		signed int val = lhx_unaligned_check(unalignedtable, (i * 2) + 1);

		if (val != (signed int)test2_table[i]) {
			pr_err("%s:#4 error: val(%x)  != table[%u] (%x)\n", __func__, val, i, (signed int)test2_table[i]);
		}
	}
}
#endif /*--- #if defined(DSP_ASE_UNALIGNED_CHECK) ---*/

/**
 */
static int unalignment_proc_write(char *cmd, void *ref __maybe_unused)
{
	int mode = cmd[0];

#if defined(DSP_ASE_UNALIGNED_CHECK)
	if (mode == 'T') {
		check_dsp_ase_unaligneds();
		return 0;
	}
#endif /*--- #if defined(DSP_ASE_UNALIGNED_CHECK) ---*/
	if (mode >= '2' && mode <= '5') {
		ai_usermode = mode - '0';
		pr_err("set user unaligned-mode: %s\n", map_mode(ai_usermode));
	} else if (mode >= '6' && mode <= '8') {
		ai_kernelmode = mode == '6' ? UNALIGNED_ACTION_FIXUP :
				mode == '7' ? UNALIGNED_ACTION_FIXUP_WARN : UNALIGNED_ACTION_FIXUP_WARN_BT;
		pr_err("set kernel unaligned-mode: %s\n", map_mode(ai_kernelmode));
	} else {
		pr_err("parameter: user   '2' %s '3' %s '4' %s '5' %s\n", map_mode(UNALIGNED_ACTION_FIXUP),
		       map_mode(UNALIGNED_ACTION_FIXUP_WARN),
		       map_mode(UNALIGNED_ACTION_SIGNAL),
		       map_mode(UNALIGNED_ACTION_SIGNAL_WARN));
		pr_err("           system '6' %s '7' %s '8' %s\n", map_mode(UNALIGNED_ACTION_FIXUP),
		       map_mode(UNALIGNED_ACTION_FIXUP_WARN),
		       map_mode(UNALIGNED_ACTION_FIXUP_WARN_BT));
	}
	return 0;
}
#endif

/**
 */
static int __init alignment_init(void)
{
	int ret;

#if IS_ENABLED(CONFIG_AVM_SAMMEL)
	proc_mkdir("cpu", NULL);
	ret = add_simple_proc_file(
			"cpu/alignment",
			unalignment_proc_write,
			unalignment_proc_read,
			NULL);
#endif

	if (avm_fw_is_internal()) {
		setup_developer_ai_info(AI_SYS, UNALIGNED_WARN);
		setup_developer_ai_info(AI_USER, UNALIGNED_WARN);
	}

	return ret;
}
device_initcall(alignment_init);
