// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2006-2019 AVM GmbH <fritzbox_info@avm.de> */

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <avm/profile/profile.h>
#include <avm/sammel/debug.h>
#include <linux/kallsyms.h>
#include <linux/env.h>
#include <asm/uaccess.h>
#pragma GCC pop_options

#include "avm_sammel.h"
#include "avm_profile.h"

#include <asm/mach_avm.h>
#include "arch_profile.h"

#if defined(CONFIG_PROC_FS)
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <avm/sammel/simple_proc.h>

struct _arch_profile_ctrl arch_profile_ctrl;
static struct _cpucore_profile cpu_config_default = {
	.cpu_nr_offset  = 0,
	.vpe_nr		= 1,
	.next_core	= NULL
};

static void __proc_read_profiler_perform(struct seq_file *seq,
					 unsigned int format);

/**
 */
struct _pid_field {
	unsigned short pid;
	unsigned char comm[TASK_COMM_LEN + 20];
};


/**
 */
static inline void avm_simple_profiling_unset_busy(void)
{
	atomic_set(&simple_profiling.busy, 0);
}

/**
 */
int __get_userinfo(char *buf, unsigned int maxbuflen, struct mm_struct *mmm,
		   unsigned long addr)
{
	struct vm_area_struct *vm;
	unsigned int i = 0;

	vm = mmm->mmap;
	while (vm) {
		/*--- pr_info("%s[%x]:%p %x - %x vm_mm %p\n", __func__, addr, vm, vm->vm_start, vm->vm_end, vm->vm_mm); ---*/
		if ((addr >= vm->vm_start) && (addr < vm->vm_end)) {
			snprintf(buf, maxbuflen,
				 "seg=%3u of=0x%08lx/0x%lx [%s]", i,
				 addr - (unsigned long)vm->vm_start,
				 (unsigned long)vm->vm_end - (unsigned long)vm->vm_start,
				 (vm->vm_file && vm->vm_file->f_path.dentry) ?  (char *)vm->vm_file->f_path.dentry->d_name.name : "");
			/*--- pr_info("%s\n", buf); ---*/
			return 0;
		}
		vm = vm->vm_next;
		i++;
	}
	return 1;
}

/**
 */
int get_user_info(char *buf, unsigned int maxbuflen, pid_t pid,
		  unsigned long addr)
{
	struct pid *ppid;
	unsigned int ret = 1;

	buf[0] = 0;
	/*--- pr_info("%s: pid=%u: addr=%08x ppid=%p\n", __func__, pid,  addr, find_vpid(pid)); ---*/
	if (!access_ok(VERIFY_READ, addr, sizeof(void *))) {
		return ret;
	}
	if (!pid) {
		return ret;
	}
	ppid = find_get_pid(pid);
	if (ppid) {
		struct task_struct *tsk = get_pid_task(ppid, PIDTYPE_PID);

		if (tsk) {
			struct mm_struct *mm = get_task_mm(tsk);

			if (mm) {
				ret = __get_userinfo(buf, maxbuflen, mm, addr);
				mmput(mm);
			}
			put_task_struct(tsk);
		}
		put_pid(ppid);
	}
	return ret;
}

/**
 */
void format_profile_header(struct seq_file *seq, unsigned long timediff)
{
	unsigned int cores = 0;
	const struct _cpucore_profile *cpu_profile;

	cpu_profile = arch_profile_ctrl.cpu_profile;
	while (cpu_profile) {
		cores++;
		cpu_profile = cpu_profile->next_core;
	}
	seq_printf(seq, "# measure time %lu msec, real cpu-cores %d", timediff,
		   cores ? cores : 1);
	cores = 0;
	cpu_profile = arch_profile_ctrl.cpu_profile;
	while (cpu_profile) {
		seq_printf(seq, " [%u] cpu_ofs %u vpes %u", cores,
			   cpu_profile->cpu_nr_offset, cpu_profile->vpe_nr);
		cores++;
		cpu_profile = cpu_profile->next_core;
	}
	seq_puts(seq, "\n");
	__proc_read_profiler_perform(seq, 1);
}

#ifndef ALIGN_DOWN /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0) */
#  define ALIGN_DOWN(x, a) ALIGN((x) - ((a)-1), (a))
#endif

static unsigned long arch_align_addr(unsigned long addr)
{
	/* falls mips16/tumb-code: keine Unaligneds provozieren */
#if defined(CONFIG_MIPS) || defined(CONFIG_ARM)
	return ALIGN_DOWN(addr, 4);
#else
	return addr;
#endif
}

/**
 */
static void cpy_symbols(unsigned char *Symbols, unsigned char *LrSymbols,
			pid_t act_pid, unsigned int addr1, unsigned int addr2)
{
	if (get_user_info(Symbols, KSYM_NAME_LEN, act_pid, arch_align_addr(addr1))) {
		sprint_symbol(Symbols, addr1);
	}
	if (get_user_info( LrSymbols, KSYM_NAME_LEN, act_pid, arch_align_addr(addr2))) {
		sprint_symbol(LrSymbols, addr2);
	}
}

/**
 */
static pid_t get_act_pid(char *comm, struct _pid_field act_pid_percpu[],
			 struct _avm_profile_data *data)
{
	struct task_struct *tsk;
	struct pid *pid;
	pid_t act_pid = data->id;

	if (act_pid != act_pid_percpu[data->cpu_id].pid) {
		if (act_pid == AVM_PROFILE_IDLE_ID) {
			strlcpy(comm, "IDLE",
				sizeof(act_pid_percpu[data->cpu_id].comm));
		} else if (act_pid) {
			pid = find_get_pid(act_pid);
			if (pid) {
				tsk = get_pid_task(pid, PIDTYPE_PID);
				if (tsk) {
					strlcpy(comm, tsk->comm,
						sizeof(act_pid_percpu[data->cpu_id]
							       .comm));
					put_task_struct(tsk);
				}
				put_pid(pid);
			} else {
				comm_short_cpy(
					comm,
					sizeof(act_pid_percpu[data->cpu_id]
						       .comm),
					data->comm, sizeof(data->comm),
					act_pid);
			}
		} else {
			comm_short_cpy(
				comm, sizeof(act_pid_percpu[data->cpu_id].comm),
				data->comm, sizeof(data->comm), act_pid);
		}
	}
	act_pid_percpu[data->cpu_id].pid = act_pid;
	return act_pid;
}

/**
 */
unsigned int format_profile_line(struct seq_file *seq,
				 struct _avm_profile_data *data,
				 struct _pid_field act_pid_percpu[],
				 unsigned int mode,
				 unsigned int supress_kernel_output)
{
	char *description = "";
	char nonlinuxcom[20];
	unsigned char Symbols[KSYM_NAME_LEN];
	unsigned char LrSymbols[KSYM_NAME_LEN];
	unsigned char traceSymbols1[KSYM_NAME_LEN];
	unsigned char traceSymbols2[KSYM_NAME_LEN];
	unsigned int len = 0;
	unsigned int linux_os;
	pid_t act_pid = 0;
	char *comm = NULL;

	if (data->cpu_id >= num_possible_cpus()) {
		pr_err("[simple-profiling]%s illegal cpu_id %u core=%x tc=%x addr = %p (type=%u - %s)\n",
		       __func__, data->cpu_id, data->core_id, data->tc_id,
		       (void *)data->addr, data->type,
		       avm_profile_data_short_names[data->type]);
		return len;
	}
	linux_os = arch_is_linux_cpu(data->core_id, data->tc_id);
	if (linux_os) {
		/*--- nur Linux-CPU's ---*/
		act_pid = act_pid_percpu[data->cpu_id].pid;
		comm = act_pid_percpu[data->cpu_id].comm;
#if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED)
	} else {
		int commlen = min(sizeof(nonlinuxcom) - 1, sizeof(data->comm));

		memcpy(nonlinuxcom, data->comm, commlen);
		nonlinuxcom[commlen] = 0;
		comm = nonlinuxcom;
#endif /*--- #if defined(AVM_PROFILE_CURRENT_COMM_INCLUDED) ---*/
	}
	switch (data->type) {
	case avm_profile_data_type_unknown:
	default:
		pr_err("[simple-profiling] internal error data type %d unknown (time=%x)\n",
		       data->type, data->time);
		break;
	case avm_profile_data_type_code_begin: /* deprecated */
	case avm_profile_data_type_code_end: /* deprecated */
	case avm_profile_data_type_sched:
		/* FALLTHRU */
	case avm_profile_data_type_code_address_info:
		if (linux_os) {
			act_pid = get_act_pid(comm, act_pid_percpu, data);
		}
		cpy_symbols(Symbols, LrSymbols, act_pid, data->addr, data->lr);
		switch (mode) {
		case 3:
			seq_printf(
				seq,
				"%x;C%x;T%x;S0x%08lx;0x%08X;0x%08X;0x%08X;%s;0x%08lx;%s;0x%08lx;%s;%.*s;%u\n",
				data->cpu_id, data->core_id, data->tc_id,
				(unsigned long)(THREAD_SIZE - data->stack_pos),
				data->time, data->total_access,
				data->total_activate,
				avm_profile_data_short_names[data->type],
				data->addr, Symbols, data->lr, LrSymbols,
				TASK_COMM_LEN, comm, data->id);
			if (supress_kernel_output == 0)
				pr_cont("c");
			break;
		}
		break;
	case avm_profile_data_type_backtrace:
		if (linux_os) {
			act_pid = get_act_pid(comm, act_pid_percpu, data);
		}
		cpy_symbols(Symbols, LrSymbols, act_pid, data->addr, data->lr);
		cpy_symbols(traceSymbols1, traceSymbols2, act_pid,
			    data->total_access, data->total_activate);
		switch (mode) {
		case 3:
			seq_printf(
				seq,
				"%x;C%x;T%x;S0x%08lx;0x%08X;0x%08x;0x%08x;%s;0x%08lx;%s;0x%08lx;%s;%.*s;%u;0x%08x;%s;0x%08x;%s\n",
				data->cpu_id, data->core_id, data->tc_id,
				(unsigned long)(THREAD_SIZE - data->stack_pos),
				data->time,
				0, //data->total_access,
				0, //data->total_activate,
				avm_profile_data_short_names[data->type],
				data->addr, Symbols, data->lr, LrSymbols,
				TASK_COMM_LEN, comm, data->id,
				data->total_access, traceSymbols1,
				data->total_activate, traceSymbols2);

			if (supress_kernel_output == 0)
				pr_cont("c");
			break;
		}
		break;
	case avm_profile_data_type_trace_skb:
	case avm_profile_data_type_trace_spinlock:
		if (supress_kernel_output == 0)
			pr_cont("s");
		goto print_work_trace;
	case avm_profile_data_type_hw_irq_begin:
	case avm_profile_data_type_hw_irq_end:
		description = "interrupted by irq";
		goto print_work_trace;
	case avm_profile_data_type_exception_begin:
	case avm_profile_data_type_exception_end:
	case avm_profile_data_type_irq_disable_begin:
	case avm_profile_data_type_irq_disable_end:
	case avm_profile_data_type_sw_irq_begin:
	case avm_profile_data_type_sw_irq_end:
	case avm_profile_data_type_timer_begin:
	case avm_profile_data_type_timer_end:
	case avm_profile_data_type_tasklet_begin:
	case avm_profile_data_type_tasklet_end:
	case avm_profile_data_type_hi_tasklet_begin:
	case avm_profile_data_type_hi_tasklet_end:
	case avm_profile_data_type_trigger_tasklet_begin:
	case avm_profile_data_type_trigger_tasklet_end:
	case avm_profile_data_type_workitem_begin:
	case avm_profile_data_type_workitem_end:
	case avm_profile_data_type_func_begin:
	case avm_profile_data_type_func_end:
	case avm_profile_data_type_trigger_user_begin:
	case avm_profile_data_type_trigger_user_end:
		description = "id:";
		/* FALLTHRU */
print_work_trace:
		if (get_user_info(Symbols, sizeof(Symbols), act_pid,
				  data->addr)) {
			sprint_symbol(Symbols, data->addr);
		}
		if (get_user_info(LrSymbols, sizeof(LrSymbols), act_pid,
				  data->lr)) {
			sprint_symbol(LrSymbols, data->lr);
		}
		switch (mode) {
		case 3:
			seq_printf(
				seq,
				"%x;C%x;T%x;S0x%08lx;0x%08X;0x%08X;0x%08X;%s;0x%08lx;%s;0x%08lx;%s;%.*s;%u;\n",
				data->cpu_id, data->core_id, data->tc_id,
				(unsigned long)(THREAD_SIZE - data->stack_pos),
				data->time, data->total_access,
				data->total_activate,
				avm_profile_data_short_names[data->type],
				data->addr, Symbols, data->lr, LrSymbols,
				TASK_COMM_LEN, comm, data->id);
			if (supress_kernel_output == 0)
				pr_cont("d");
			break;
		}
	}
	return len;
}

/**
 */
struct _profile_readcsv {
	unsigned int pos;
	unsigned int entries;
	unsigned long timediff;
	struct _pid_field act_pid[NR_CPUS];
	struct _avm_profile_data *data;
	unsigned int cnt;
	unsigned int percent;
	unsigned int one_percent;
};

/**
 * Returns false if pos at or past end of file.
 */
static int update_iter(struct _profile_readcsv *iter, loff_t pos)
{
	/* Module symbols can be accessed randomly. */
	iter->data = avm_simple_profiling_by_idx(pos);
	iter->pos = pos;
	if (iter->data == NULL) {
		pr_cont("[100%%]");
		avm_simple_profiling_unset_busy();
		return 0;
	}
	return 1;
}

/**
 */
static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	(*pos)++;

	if (!update_iter(m->private, *pos)) {
		return NULL;
	}
	return p;
}

/**
 */
static void *s_start(struct seq_file *m, loff_t *pos)
{
	if (!update_iter(m->private, *pos)) {
		return NULL;
	}
	return m->private;
}

/**
 */
static void s_stop(struct seq_file *m, void *p)
{
	avm_simple_profiling_unset_busy();
}

/**
 */
static int s_show(struct seq_file *m, void *p)
{
	struct _profile_readcsv *iter = m->private;

	if (unlikely(iter->pos == 0)) {
		format_profile_header(m, iter->timediff);
	}
	if (iter->data == NULL) {
		avm_simple_profiling_unset_busy();
		return 0;
	}
	if (unlikely(iter->cnt++ >= iter->one_percent)) {
		iter->cnt = 0;
		pr_cont("[%u%%]", ++iter->percent);
	}
	format_profile_line(m, iter->data, iter->act_pid, 3, 1);
	return 0;
}

/**
 */
static const struct seq_operations readcsv_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show
};

/**
 */
static void reset_iter(struct _profile_readcsv *iter, loff_t new_pos)
{
	unsigned int i;

	avm_simple_profiling_enable(sp_enable_off, 0, 0, &iter->entries,
				    &iter->timediff, 1);
	iter->pos = new_pos;
	iter->cnt = 0;
	iter->percent = 0;
	iter->one_percent = iter->entries / 100;
	pr_info("\[%u%%]", iter->percent);
	for (i = 0; i < ARRAY_SIZE(iter->act_pid); i++) {
		strcpy(iter->act_pid[i].comm, "PID_0");
		iter->act_pid[i].pid = 0;
	}
}

/**
 */
static int readcsv_open(struct inode *inode, struct file *file)
{
	/*
	 * We keep iterator in m->private, since normal case is to
	 * s_start from where we left off, so we avoid doing
	 */
	struct _profile_readcsv *iter;
	int ret;

	iter = kmalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter) {
		return -ENOMEM;
	}
	reset_iter(iter, 0);

	ret = seq_open(file, &readcsv_op);
	((struct seq_file *)file->private_data)->private = iter;
	return ret;
}

/**
 */
static const struct file_operations readcsv_operations = {
	.open = readcsv_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private,
};

/**
 */
static int profilestat_show(struct seq_file *seq, void *data)
{
	/*--- pr_info("%s %p %p\n", __func__, seq->private, data); ---*/
	if (seq->private) {
		seq_printf(seq, "%s", (char *)seq->private);
	}
	return 0;
}

#define PROFILE_BUF_SIZE (128 << 10)
/**
 */
static int profilestat_open(struct inode *inode, struct file *file)
{
	const struct _cpucore_profile *cpu_profile;
	unsigned int bufsize = PROFILE_BUF_SIZE;
	unsigned int full = 0;
	char *filename;
	char *buf;

	buf = kmalloc(bufsize, GFP_KERNEL);
	if (buf == NULL) {
		return -ENOMEM;
	}
	buf[0] = 0;
	filename = file->f_path.dentry ?
			   (char *)file->f_path.dentry->d_name.name :
			   "";
	if (strcmp(filename, "statistic") == 0) {
		unsigned char *pbuf = buf;
		unsigned int cpu_nr = 0;

		cpu_profile = arch_profile_ctrl.cpu_profile ?
				      arch_profile_ctrl.cpu_profile :
				      &cpu_config_default;
		while (cpu_profile) {
			unsigned int txtlen =
				profilestat_category(pbuf, bufsize, cpu_nr,
						     cpu_profile->cpu_nr_offset,
						     cpu_profile->vpe_nr, full);

			cpu_nr++;
			pbuf += txtlen;
			bufsize -= txtlen;
			cpu_profile = cpu_profile->next_core;
		}
	} else if (strcmp(filename, "totalcall") == 0) {
		profilestat_totalcall(buf, bufsize, -1, 0, num_possible_cpus(),
				      0);
	} else if (strcmp(filename, "totalweight") == 0) {
		profilestat_totalcall(buf, bufsize, -1, 0, num_possible_cpus(),
				      1);
	}
	avm_simple_profiling_unset_busy();
	/*--- pr_info("%s %p\n", __func__, buf); ---*/
	return single_open(file, profilestat_show, buf);
}

/**
 */
static int profilestat_close(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;

	kfree(seq->private);
	seq->private = NULL;
	return single_release(inode, file);
}

/**
 */
static const struct file_operations profilestat_fops = {
	.open = profilestat_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = profilestat_close,
};
/**
 */
static char *name_trace(char *text, unsigned int txt_len,
			unsigned int tracemask)
{
	snprintf(text, txt_len, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		 tracemask & (1 << avm_profile_data_type_code_address_info)     ? " CODE" : "",
		 tracemask & (1 << avm_profile_data_type_backtrace)             ? " BACKTRACE" : "",
		 tracemask & (1 << avm_profile_data_type_trace_skb)             ? " SKB" : "",
		 tracemask & (1 << avm_profile_data_type_trace_spinlock)        ? " SLCK" : "",
		 tracemask & (1 << avm_profile_data_type_hw_irq_begin)          ? " HWIRQ" : "",
		 tracemask & (1 << avm_profile_data_type_sw_irq_begin)          ? " SWIRQ" : "",
		 tracemask & (1 << avm_profile_data_type_timer_begin)           ? " TIMER" : "",
		 tracemask & (1 << avm_profile_data_type_tasklet_begin)         ? " TASKLET" : "",
		 tracemask & (1 << avm_profile_data_type_hi_tasklet_begin)      ? " HITSKLT" : "",
		 tracemask & (1 << avm_profile_data_type_workitem_begin)        ? " WORKITEM" : "",
		 tracemask & (1 << avm_profile_data_type_trigger_tasklet_begin) ? " TRGTSKLT" : "",
		 tracemask & (1 << avm_profile_data_type_trigger_user_begin)    ? " TRGUSER" : "",
		 tracemask & (1 << avm_profile_data_type_code_begin)            ? " SCHED" : "",
		 tracemask & (1 << avm_profile_data_type_exception_begin)       ? " EXCEPTION" : "",
		 tracemask & (1 << avm_profile_data_type_irq_disable_begin)	? " IRQDISABLE" : "",
		 tracemask & (1 << avm_profile_data_type_func_begin)            ? " FUNC" : "");
	return text;
}

/**
 *
 */
static struct _profile_helper {
	char *action;
	char *help;
	unsigned int tracemask;
	enum _simple_profile_enable_mode mode;
} profile_helper[] = {
	{ .action = "all", .help = "trace all entries",
			.tracemask =
				(1 << avm_profile_data_type_code_address_info) |
				(1 << avm_profile_data_type_trace_skb) |
				(1 << avm_profile_data_type_hw_irq_begin) |
				(1 << avm_profile_data_type_hw_irq_end) |
				(1 << avm_profile_data_type_exception_begin) |
				(1 << avm_profile_data_type_exception_end) |
				(1 << avm_profile_data_type_sw_irq_begin) |
				(1 << avm_profile_data_type_sw_irq_end) |
				(1 << avm_profile_data_type_timer_begin) |
				(1 << avm_profile_data_type_timer_end) |
				(1 << avm_profile_data_type_tasklet_begin) |
				(1 << avm_profile_data_type_tasklet_end) |
				(1 << avm_profile_data_type_hi_tasklet_begin) |
				(1 << avm_profile_data_type_hi_tasklet_end) |
				(1 << avm_profile_data_type_workitem_begin) |
				(1 << avm_profile_data_type_workitem_end) |
				(1 << avm_profile_data_type_tasklet_end) |
				(1 << avm_profile_data_type_hi_tasklet_end) |
				(1 << avm_profile_data_type_trigger_tasklet_begin) |
				(1 << avm_profile_data_type_trigger_tasklet_end) |
				(1 << avm_profile_data_type_trigger_user_begin) |
				(1 << avm_profile_data_type_trigger_user_end) |
				(1 << avm_profile_data_type_code_begin) | /* deprecated */
				(1 << avm_profile_data_type_code_end) | /* deprecated */
				(1 << avm_profile_data_type_sched) |
				(1 << avm_profile_data_type_func_begin) |
				(1 << avm_profile_data_type_func_end) |
				0,
				.mode = sp_enable_on },
	{ .action = "allext", .help = "trace all entries",
				.tracemask =
				(1 << avm_profile_data_type_backtrace) |
				(1 << avm_profile_data_type_trace_skb) |
				(1 << avm_profile_data_type_hw_irq_begin) |
				(1 << avm_profile_data_type_hw_irq_end) |
				(1 << avm_profile_data_type_exception_begin) |
				(1 << avm_profile_data_type_exception_end) |
				(1 << avm_profile_data_type_sw_irq_begin) |
				(1 << avm_profile_data_type_sw_irq_end) |
				(1 << avm_profile_data_type_timer_begin) |
				(1 << avm_profile_data_type_timer_end) |
				(1 << avm_profile_data_type_tasklet_begin) |
				(1 << avm_profile_data_type_tasklet_end) |
				(1 << avm_profile_data_type_hi_tasklet_begin) |
				(1 << avm_profile_data_type_hi_tasklet_end) |
				(1 << avm_profile_data_type_workitem_begin) |
				(1 << avm_profile_data_type_workitem_end) |
				(1 << avm_profile_data_type_tasklet_end) |
				(1 << avm_profile_data_type_hi_tasklet_end) |
				(1 << avm_profile_data_type_trigger_tasklet_begin) |
				(1 << avm_profile_data_type_trigger_tasklet_end) |
				(1 << avm_profile_data_type_trigger_user_begin) |
				(1 << avm_profile_data_type_trigger_user_end) |
				(1 << avm_profile_data_type_code_begin) | /* deprecated */
				(1 << avm_profile_data_type_code_end) | /* deprecated */
				(1 << avm_profile_data_type_sched) |
				(1 << avm_profile_data_type_func_begin) |
				(1 << avm_profile_data_type_func_end) |
				(1 << avm_profile_data_type_irq_disable_begin) |
				(1 << avm_profile_data_type_irq_disable_end) |
				0,
				.mode = sp_enable_on },

	{ .action = "stop",  .help = "stop tracing",
			.tracemask = 0x0,                                                  .mode = sp_enable_max },
	{ .action = "backtrace",  .help = "backtracing",
			.tracemask = (1 << avm_profile_data_type_backtrace),               .mode = sp_enable_on },
	{ .action = "wrap",  .help = "wrap if buffer full",
			.tracemask = 0x0,                                                  .mode = sp_enable_wrap },
	{ .action = "codeuart",  .help = "code-trace over gpio (uart-emulation)",
			.tracemask = (1 << avm_profile_data_type_code_address_info),       .mode = sp_enable_uart },
	{ .action = "skb",  .help = "only skb-trace",
			.tracemask = (1 << avm_profile_data_type_trace_skb),               .mode = sp_enable_on },
	{ .action = "spinlock",  .help = "only spinlock-trace",
			.tracemask = (1 << avm_profile_data_type_trace_spinlock),          .mode = sp_enable_on },
	{ .action = "code",  .help = "only code-trace",
			.tracemask = (1 << avm_profile_data_type_code_address_info),       .mode = sp_enable_on },
	{ .action = "sched",  .help = "only schedule-trace",
			.tracemask =
			(1 << avm_profile_data_type_code_begin) |	/* deprecated */
			(1 << avm_profile_data_type_code_end)   |	/* deprecated */
			(1 << avm_profile_data_type_sched),
			.mode = sp_enable_on },
	{ .action = "hwirq", .help = "hardware irqs",
			.tracemask =
			1 << avm_profile_data_type_hw_irq_begin |
			1 <<  avm_profile_data_type_hw_irq_end,
			.mode = sp_enable_on },
	{ .action = "dirq", .help = "irqs disable",
			.tracemask =
			1 << avm_profile_data_type_irq_disable_begin |
			1 <<  avm_profile_data_type_irq_disable_end,
			.mode = sp_enable_on },
	{ .action = "exception",    .help = "user defined function",
			.tracemask =
			1 << avm_profile_data_type_exception_begin |
			1 << avm_profile_data_type_exception_end,
			.mode = sp_enable_on },
	{ .action = "swirq", .help = "software irqs",
			.tracemask =
			1 << avm_profile_data_type_sw_irq_begin |
			1 <<  avm_profile_data_type_sw_irq_end,
			.mode = sp_enable_on },
	{ .action = "timer", .help = "timer irqs",
			.tracemask =
			1 << avm_profile_data_type_timer_begin |
			1 << avm_profile_data_type_timer_end,
			.mode = sp_enable_on },
	{ .action = "tasklet", .help = "(hi-)tasklets",
			.tracemask =
			1 << avm_profile_data_type_tasklet_begin |
			1 << avm_profile_data_type_tasklet_begin    |
			1 << avm_profile_data_type_hi_tasklet_begin |
			1 << avm_profile_data_type_hi_tasklet_begin,
			.mode = sp_enable_on },
	{ .action = "workitem", .help = "workqueues",
			.tracemask =
			1 << avm_profile_data_type_workitem_begin |
			1 << avm_profile_data_type_workitem_end,
			.mode = sp_enable_on },
	{ .action = "trigger", .help = "tasklet-trigger and user-defined trigger",
			.tracemask =
			1 << avm_profile_data_type_trigger_tasklet_begin |
			1 << avm_profile_data_type_trigger_tasklet_end   |
			1 << avm_profile_data_type_trigger_user_begin    |
			1 << avm_profile_data_type_trigger_user_end,
			.mode = sp_enable_on },
	{ .action = "func",    .help = "user defined function",
			.tracemask =
			1 << avm_profile_data_type_func_begin |
			1 << avm_profile_data_type_func_end,
			.mode = sp_enable_on },
	{ .action = "bh",    .help = "bottom half (sw-irq, tasklets, timer)",
			.tracemask =
			1 << avm_profile_data_type_timer_begin   |
			1 << avm_profile_data_type_timer_end     |
			1 << avm_profile_data_type_tasklet_begin |
			1 << avm_profile_data_type_tasklet_end   |
			1 << avm_profile_data_type_sw_irq_begin  |
			1 << avm_profile_data_type_sw_irq_end,
			.mode = sp_enable_on },
	{ .action = NULL, .mode = sp_enable_off }
};

/**
 */
static void lproc_profile_help(struct seq_file *seq, void *priv)
{
	struct _profile_helper *ph = (struct _profile_helper *)priv;

	seq_puts(
		seq,
		"AVM Profiler Version 3.0\n"
		"csv          - get raw-profile-list for offline evaluation\n"
		"statistic    - get profile statistic (consumption/latency)\n"
#if defined(CONFIG_KALLSYMS)
		"totalcall    - get top of function calls\n"
		"totalweight  - get top of function calls weighted with codelength\n"
	/*--- "totalweight2 - get top of function calls weighted with square-codelength\n"   ---*/
#endif /*--- #if defined(CONFIG_KALLSYMS) ---*/
		"action       - all, stop, ... mbytes=x (see below)\n\n");
	seq_puts(
		seq,
		"parameter(s) for action (use 'no'-prefix to exclude trace-option):\n");
	while (ph->action) {
		seq_printf(seq, "%-10s - %s\n", ph->action, ph->help);
		ph++;
	}
	seq_printf(seq,
		   "mbytes=<x>: size of profiler-buffer (actual: %u MiB)\n\n",
		   (profile_BlockNeeded * (1 << PAGE_SHIFT)) / (1 << 20));
	seq_puts(seq,
		 "example: echo bh workitem wrap > /proc/avm/profile/action\n");

	if (arch_profile_ctrl.performance_counter_help) {
		arch_profile_ctrl.performance_counter_help(seq);
	}
}

/**
 */
static int proc_write_profiler_perform(char *buffer, void *priv)
{
	char parsbuf[512];

	strlcpy(parsbuf, buffer, sizeof(parsbuf));
	/*--- pr_info("%s parsbuf='%s'\n", __func__, parsbuf); ---*/
	if ((strstr(parsbuf, "help"))) {
		if (arch_profile_ctrl.performance_counter_help) {
			arch_profile_ctrl.performance_counter_help(NULL);
		}
		return 0;
	}
	if (arch_profile_ctrl.performance_counter_action) {
		arch_profile_ctrl.performance_counter_action(parsbuf);
	}
	return 0;
}

/**
 */
static void __proc_read_profiler_perform(struct seq_file *seq,
					 unsigned int format)
{
	unsigned int core = 0;
	const struct _cpucore_profile *cpu_profile;

	if (arch_profile_ctrl.profiling_performance_statistic == NULL) {
		return;
	}
	cpu_profile = arch_profile_ctrl.cpu_profile ?
			      arch_profile_ctrl.cpu_profile :
			      &cpu_config_default;
	while (cpu_profile) {
		seq_printf(seq, "Performance-Counter - CORE%u ", core);
		arch_profile_ctrl.profiling_performance_statistic(core, seq,
								  format);
		core++;
		cpu_profile = cpu_profile->next_core;
	}
}

/**
 */
static void proc_read_profiler_perform_human(struct seq_file *seq, void *priv)
{
	__proc_read_profiler_perform(seq, 2);
}

/**
 */
static void proc_read_profiler_perform_list(struct seq_file *seq, void *priv)
{
	__proc_read_profiler_perform(seq, 1);
}

/**
 */
static int proc_write_profiler_action(char *buffer, void *priv)
{
	struct _profile_helper *ph = (struct _profile_helper *)priv;
	unsigned int tracemask = 0, mbytes, BlockNeeded;
	char parsbuf[256], *p;
	enum _simple_profile_enable_mode mode = sp_enable_off;

	strlcpy(parsbuf, buffer, sizeof(parsbuf));
	/*--- pr_info("%s parsbuf='%s'\n", __func__, parsbuf); ---*/
	p = strstr(parsbuf, "mbytes=");
	if (p) {
		sscanf(p, "mbytes=%u", &mbytes);
		if (mbytes == 0) {
			mbytes = 2;
		}
		BlockNeeded = mbytes * (1 << (20 - PAGE_SHIFT));
		avm_simple_profiling_memresize(BlockNeeded);
	}
	while (ph->action) {
		p = strstr(parsbuf, ph->action);
		if (p) {
			if ((ph->mode) > mode) {
				mode = ph->mode;
			}
			if (mode == sp_enable_uart) {
				tracemask =
					ph->tracemask; /*--- nur exklusiv code tracen ---*/
				break;
			}
			if (((p - parsbuf) >= 2) &&
			    ((p[-2] == 'n') && (p[-1] == 'o'))) {
				tracemask &= ~ph->tracemask;
			} else {
				tracemask |= ph->tracemask;
			}
		}
		ph++;
	}
	if (mode >= sp_enable_max) {
		mode = sp_enable_off;
	}
	if (tracemask & (1 << avm_profile_data_type_backtrace)) {
		tracemask &= ~(1 << avm_profile_data_type_code_address_info);
	}
	if (mode != sp_enable_off) {
		pr_info("[simple-profiling]trace following hooks: %s\n",
			name_trace(parsbuf, sizeof(parsbuf), tracemask));
	}
	/*--- pr_info("%s %x %x\n", __func__, mode, tracemask); ---*/
	avm_simple_profiling_enable(mode, 1, tracemask, NULL, NULL, 0);
	return 0;
}

static struct proc_dir_entry *profileprocdir;

#define PROC_PROFILEDIR "avm/profile"
/**
 */
static int __init avm_profiler_init(void)
{
	profileprocdir = proc_mkdir(PROC_PROFILEDIR, NULL);
	if (profileprocdir == NULL) {
		return 0;
	}
	proc_create("csv", 0440, profileprocdir, &readcsv_operations);
	proc_create("statistic", 0440, profileprocdir, &profilestat_fops);
#if defined(CONFIG_KALLSYMS)
	proc_create("totalcall", 0440, profileprocdir, &profilestat_fops);
	proc_create("totalweight", 0440, profileprocdir, &profilestat_fops);
	/*--- proc_create("totalweight2", 0440, profileprocdir, &profilestat_fops); ---*/
#endif /*--- #if defined(CONFIG_KALLSYMS) ---*/
	add_simple_proc_file("avm/profile/help", NULL, lproc_profile_help,
			     profile_helper);
	add_simple_proc_file("avm/profile/action", proc_write_profiler_action,
			     NULL, profile_helper);
	add_simple_proc_file("avm/profile/perform", proc_write_profiler_perform,
			     proc_read_profiler_perform_human, profile_helper);
	add_simple_proc_file("avm/profile/performlist", NULL,
			     proc_read_profiler_perform_list, NULL);
	pr_info("AVM Simple Profiling enabled Version %u.0\n",
		AVM_PROFILING_VERSION);
	return 0;
}

device_initcall(avm_profiler_init);

/**
 */
static unsigned int check_keyword(const char *string, const char *keyword)
{
	unsigned int mbytes = 0;
	char *p;

	if (!string) {
		return 0;
	}
	p = strstr(string, keyword);
	if (!p) {
		return 0;
	}
	if (p != string) {
		p--;
		if ((*p != ' ') && (*p != '\t')) {
			return 0;
		}
		p++;
	}
	p += strlen(keyword);
	if (*p == '=') {
		sscanf(p + 1, "%u", &mbytes);
	} else if ((*p == ' ') || (*p == '\t') || (*p == 0)) {
		mbytes = 30;
	}
	return mbytes;
}

static unsigned int boot_profiling;

/**
 */
static int __init boot_profiling_start(void)
{
	const char *kernel_args;
	unsigned int tracemask =
		(1 << avm_profile_data_type_code_address_info) |
		(1 << avm_profile_data_type_hw_irq_begin) |
		(1 << avm_profile_data_type_hw_irq_end) |
		(1 << avm_profile_data_type_exception_begin) |
		(1 << avm_profile_data_type_exception_end) |
		(1 << avm_profile_data_type_sw_irq_begin) |
		(1 << avm_profile_data_type_sw_irq_end) |
		(1 << avm_profile_data_type_timer_begin) |
		(1 << avm_profile_data_type_timer_end) |
		(1 << avm_profile_data_type_tasklet_begin) |
		(1 << avm_profile_data_type_tasklet_end) |
		(1 << avm_profile_data_type_hi_tasklet_begin) |
		(1 << avm_profile_data_type_hi_tasklet_end) |
		(1 << avm_profile_data_type_workitem_begin) |
		(1 << avm_profile_data_type_workitem_end) |
		(1 << avm_profile_data_type_tasklet_end) |
		(1 << avm_profile_data_type_hi_tasklet_end) |
		(1 << avm_profile_data_type_trigger_tasklet_begin) |
		(1 << avm_profile_data_type_trigger_tasklet_end) |
		(1 << avm_profile_data_type_trigger_user_begin) |
		(1 << avm_profile_data_type_trigger_user_end) |
		(1 << avm_profile_data_type_code_begin) | /* deprecated */
		(1 << avm_profile_data_type_code_end) | /* deprecated */
		(1 << avm_profile_data_type_sched) |
		/*--- (1 << avm_profile_data_type_irq_disable_begin) | ---*/
		/*--- (1 << avm_profile_data_type_irq_disable_end) | ---*/
		(1 << avm_profile_data_type_func_begin) |
		(1 << avm_profile_data_type_func_end) | 0;

	kernel_args = prom_getenv("kernel_args");
	boot_profiling = check_keyword(kernel_args, "boot_profiling");
	if (boot_profiling) {
		if (boot_profiling > 50) {
			boot_profiling = 50;
		}
		pr_err("----------------- Attention! - boot-profiling active (need %u mbytes) ! -----------------\n",
		       boot_profiling);
		avm_simple_profiling_memresize(boot_profiling *
					       (1 << (20 - PAGE_SHIFT)));
		avm_simple_profiling_enable(sp_enable_on, 1, tracemask, NULL,
					    NULL, 0);
	}
	return 0;
}

late_initcall(boot_profiling_start);

/**
 */
void boot_profiling_stop(void)
{
	if (boot_profiling) {
		avm_simple_profiling_enable(sp_enable_off, 0, 0, NULL, NULL, 0);
		pr_err("\n----------------- Boot-profiling stopped ! -----------------\n");
		boot_profiling = 0;
	}
}

#if defined(CONFIG_AVM_DEBUG_MODULE) && (CONFIG_AVM_DEBUG_MODULE == 1)
/**
 */
void avm_profiler_exit(void)
{
	if (profileprocdir) {
		remove_proc_entry("csv", profileprocdir);
		remove_proc_entry("statistic", profileprocdir);
		remove_proc_entry("totalcall", profileprocdir);
		remove_proc_entry("totalweight", profileprocdir);
		/*--- remove_proc_entry("totalweight2", profileprocdir); ---*/
		remove_simple_proc_file("avm/profile/action");
		remove_simple_proc_file("avm/profile/perform");
		remove_simple_proc_file("avm/profile/performlist");
		remove_simple_proc_file("avm/profile/help");
		remove_proc_entry(PROC_PROFILEDIR, NULL);
		profileprocdir = NULL;
	}
}
}

module_exit(avm_profiler_exit);
#endif /*--- #if defined(CONFIG_AVM_DEBUG_MODULE) && (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
