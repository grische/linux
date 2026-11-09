// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2018-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/oom.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/magic.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task_stack.h>
#endif

#include <avm/sammel/simple_proc.h>
#include <avm/sammel/helpers.h>
#include <linux/mm.h>
#include "avm_sammel.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0) && \
	!LINUX_STABLE_VERSION_HAS_SUBLEVEL(4, 4, 207)
static inline void *try_get_task_stack(struct task_struct *tsk)
{
	return task_stack_page(tsk);
}

static inline void put_task_stack(struct task_struct *tsk)
{
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define task_stack_end_corrupted(task)                                         \
	(*(end_of_stack(task)) != STACK_END_MAGIC)
#endif

/**
 * @brief check if stackpointer valid and no stack overflow detected
 * return 0 success
 *      < 0 corrupt
 */
static int check_task_stack_consistence(struct task_struct *task)
{
	if (!virt_addr_valid(task_stack_page(task))) {
		/*--- invalid stackpointer on task ---*/
		pr_emerg("FATAL ERROR: task_struct maybe corrupt: invalid stackpointer on %s (%d) (stack=%p) task_struct: %pS(%80ph)\n",
			 task->comm, task_pid_nr(task), task_stack_page(task), task, task);
		return -EFAULT;
	}
	if (task != &init_task && task_stack_end_corrupted(task)) {
		pr_emerg("FATAL ERROR: corrupt stack on %s (%d) (stack=%p)\n",
			 task->comm, task_pid_nr(task), task_stack_page(task));
		/* for overflow detection */
		return -EINVAL;
	}
	return 0;
}

/**
 * @brief get the task where consumed most of stack
 *	      stackusage only valid if CONFIG_DEBUG_STACK_USAGE set
 *	      stack-corruption independend
 * @param	  comm      task-name (reserve TASK_COMM_LEN)
 * @return > 0 minimal stack is free
 *	   < 0 -EBUSY can't get lock
 *	       -EINVAL stack-corruption
 *	       -EFAULT invalid stack-address or task-address
 */
static int get_taskstack_minfree(char *comm, int *pid,
				 struct seq_file *seq)
{
	unsigned long flags = 0;
	int err;
	int stack_free __maybe_unused, min_stack_free = THREAD_SIZE,
				       min_pid = 0;
	struct task_struct *g, *task, *err_task = NULL, *prev_task = NULL, *min_task = NULL;

	if (comm) {
		comm[0] = 0;
	}
	rte_local_irq_save(flags);
	if (!read_trylock(&tasklist_lock)) {
		rte_local_irq_restore(flags);
		return -EBUSY;
	}
	do_each_thread(g, task) {
		if (!virt_addr_valid(task)) {
			min_stack_free = -EFAULT;
			err_task = task;
			goto stackfree_return;
		}
		if (!try_get_task_stack(task))
			continue;

		err = check_task_stack_consistence(task);
		if (err) {
			min_task = task;
			min_stack_free = err;
			min_pid = task_pid_nr(task);
			put_task_stack(task);
			goto stackfree_return;
		}
#if defined(CONFIG_DEBUG_STACK_USAGE)
		stack_free = stack_not_used(task);
		if (seq) {
			seq_printf(
				seq,
				"%-16s (%5d) stack depth: %5lu (%5u bytes left)\n",
				task->comm, task_pid_nr(task),
				THREAD_SIZE - stack_free, stack_free);
		}
		if (stack_free < min_stack_free) {
			min_task = task;
			min_stack_free = stack_free;
			min_pid = task_pid_nr(task);
		}
		prev_task = task;
		put_task_stack(task);
#endif /*--- #if defined(CONFIG_DEBUG_STACK_USAGE) ---*/
	} while_each_thread(g, task);
stackfree_return:
	if (min_task && comm) {
		strlcpy(comm, min_task->comm, TASK_COMM_LEN);
	}
	read_unlock(&tasklist_lock);
	rte_local_irq_restore(flags);
	if (pid)
		*pid = min_pid;
	if (err_task) {
		pr_emerg("%s: FATAL ERROR: bad task-pointer %p\n", __func__, task);
		if (prev_task)
			pr_emerg("%s: previous task: %s %pS %64ph\n", __func__, prev_task->comm, prev_task, prev_task);
	}
	return min_stack_free;
}

/**
 * @brief stack checking
 * @param task == NULL over all threads
 */
void avm_stack_check(struct task_struct *task)
{
	char comm[TASK_COMM_LEN];
	int min_stack_free, pid;

	if (virt_addr_valid(task)) {
		if (!try_get_task_stack(task)) {
			pr_emerg("ERROR: no refcount on %s (%d) (stack=%pS)\n",
				 task->comm, task_pid_nr(task), task_stack_page(task));
			return;
		}
		check_task_stack_consistence(task);
		put_task_stack(task);
		return;
	}
	min_stack_free = get_taskstack_minfree(comm, &pid, NULL);
	switch (min_stack_free) {
	case -EBUSY:
	case -EFAULT:
	case -EINVAL:
		break;
	default:
		if (comm[0]) {
			pr_emerg(
				"%s (%d) used greatest stack depth: %6u bytes left\n",
				comm, pid, min_stack_free);
		}
		break;
	}
}

/**
 */
static unsigned long check_task_area(struct task_struct *task,
				     unsigned long addr, char *comm,
				     unsigned int commlen)
{
	if ((addr >= (unsigned long)task) &&
	    (addr < (unsigned long)(task + 1))) {
		if (comm) {
			snprintf(comm, commlen, "task_struct(%s)", task->comm);
		}
		return (unsigned long)task;
	}
	return 0;
}

/**
 */
static unsigned long check_stack_area(struct task_struct *task,
				      unsigned long addr, char *comm,
				      unsigned int commlen)
{
	unsigned long stack = (unsigned long)task_stack_page(task);

	/*--- pr_err("%s: %u: %s: %lx %lx\n", __func__, __LINE__, task->comm, stack, addr); ---*/
	if ((addr >= stack) && (addr < (stack + THREAD_SIZE))) {
		if (comm) {
			snprintf(comm, commlen, "%s(%s)",
				 addr < (unsigned long)
				(task_thread_info(task) + 1) ?
					 "threadinfo" :
					 "stack",
				 task->comm);
		}
		return stack;
	}
	return 0;
}

/**
 * @brief check if pointer inside a task_page
 * @param	comm      task-name
 *		commlen   len of comm (please reserve 10 bytes more than TASK_COMM_LEN)
 * @return = 0  outside
 */
unsigned long get_taskstack_area(unsigned long addr, char *comm,
				 unsigned int commlen, int only_current)
{
	unsigned long flags = 0;
	unsigned long start = 0;
	struct task_struct *g, *task, *prev_task = NULL, *err_task = NULL;

	if (only_current) {
		start = check_task_area(current, addr, comm, commlen);
		if (start) {
			return start;
		}
		return check_stack_area(current, addr, comm, commlen);
	}
	rte_local_irq_save(flags);
	if (!read_trylock(&tasklist_lock)) {
		rte_local_irq_restore(flags);
		return 0;
	}
	do_each_thread(g, task) {
		if (!virt_addr_valid(task)) {
			err_task = task;
			goto end_task;
		}
		start = check_task_area(task, addr, comm, commlen);
		if (start)
			goto end_task;
		start = check_stack_area(task, addr, comm, commlen);
		if (start)
			goto end_task;
		prev_task = task;
	} while_each_thread(g, task);
end_task:
	read_unlock(&tasklist_lock);
	rte_local_irq_restore(flags);
	if (err_task) {
		pr_emerg("%s: FATAL ERROR: bad task-pointer %p\n", __func__, err_task);
		if (prev_task)
			pr_emerg("%s: previous task: %s %pS %64ph\n", __func__, prev_task->comm, prev_task, prev_task);
	}
	return start;
}

#if defined(CONFIG_DEBUG_STACK_USAGE) && defined(CONFIG_PROC_FS)
/**
 * @brief show tack-usage over all tasks
 * @param seq for proc-device
 */
static void avm_stack_all_tasks(struct seq_file *seq, void *priv)
{
	char comm[TASK_COMM_LEN];
	int min_stack_free, pid;

	if (!seq) {
		return;
	}
	min_stack_free = get_taskstack_minfree(comm, &pid, seq);
	switch (min_stack_free) {
	case -EBUSY:
		break;
	case -EFAULT:
	case -EINVAL:
		seq_printf(seq, "ERROR: %s on %s\n", comm, min_stack_free == -EFAULT ?
			   "invalid stackaddress" : "stack overflow");
		break;
	default:
		if (comm[0]) {
			seq_printf(
				seq,
				"%-16s (%5d) used greatest stack depth: %5u bytes left\n",
				comm, pid, min_stack_free);
		}
		break;
	}
}

/**
 */
static int __init avm_stack_usage_init(void)
{
	add_simple_proc_file("avm/stack_usage", NULL, avm_stack_all_tasks,
			     NULL);
	return 0;
}

late_initcall(avm_stack_usage_init);
#endif /*--- #if defined(CONFIG_DEBUG_STACK_USAGE) && defined(CONFIG_PROC_FS) ---*/
