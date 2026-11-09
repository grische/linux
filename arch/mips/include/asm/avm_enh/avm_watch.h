#ifndef __avm_watch_h__
#define __avm_watch_h__

#if defined(CONFIG_AVM_WP)
/**
 * ret: 1 -> handled
 *      0 -> use ptrace-watchpoint interface
 */
int avm_wp_dispatcher(struct pt_regs *regs);

/**
 */
enum _wp_type {
    wp_type_write        = (1 << 0),
    wp_type_read         = (1 << 1),
    wp_type_instruction  = (1 << 2)
};

/*
 * Custom watchpoint handler function type.
 *
 * Executed when a watchpoint is hit, after the initial bookkeeping (recording
 * the access in the watchpoint's history) is done.
 *
 * ref:			arbitrary pointer. Set using set_watchpoint
 * status:		the kind of access that triggered the watchpoint
 * deferred:	whether the watchpoint exception was deferred
 * regs:		register state prior to triggering the exception
 *
 * return 1 to keep watchpoint active and 0 to delete the watchpoint.
 */
typedef int (*wp_handler_func)(void *ref, enum _wp_type status, int deferred, struct pt_regs *regs);

/**
 * use default-watchpointhandler
 * watch_nr: interaptiv 0-3 (0-1 instruction, 2-3 data)
 * addr:     watchpoint-addr (double-word)
 * mask:     Bit 11 - 3: bits which ignored on addr (watch-area)
 * type:     interaptiv: 0/1 wp_type_read/wp_type_write
 *                       3/4 instruction
 * cpu_id:    >= NR_CPUS - all cpus
*/
int set_watchpoint(int watch_nr, unsigned long addr, int mask, enum _wp_type type, int cpu_id,
			wp_handler_func wp_handler, void *ref);

/**
 * disable watchpoint temporarily on the local CPU
 * watch_nr: interaptiv 0-3 (0-1 instruction, 2-3 data)
 */
void local_wp_off(int watch_nr);

/**
 * re-enable watchpoint
 * CAUTION: This method is only to re-enable locally disabled watchpoints that
 * have been set with set_watchpoint() initially. watch_nr, addr, and type
 * arguments MUST be identical with the original set_watchpoint call.
 * watch_nr: interaptiv 0-3 (0-1 instruction, 2-3 data)
 * addr:     watchpoint-addr (double-word)
 * type:     interaptiv: 0/1 wp_type_read/wp_type_write
 *                       3/4 instruction
 */
void local_wp_on(int watch_nr, unsigned long addr, enum _wp_type type);

/**
 * use default-watchpointhandler
 * and trigger panic if watchpoint occur
 * watch_nr: interaptiv 0-3 (0-1 instruction, 2-3 data)
 * addr:     watchpoint-addr (double-word)
 * mask:     Bit 11 - 3: bits which ignored on addr (watch-area)
 * type:     interaptiv: 0/1 wp_type_read/wp_type_write
 *                       3/4 instruction
 * cpu_id:    >= NR_CPUS - all cpus
 */
int set_watchpoint_fatal(int watch_nr, unsigned long addr, int mask, enum _wp_type type, int cpu_id);

/**
 * delete watchpoint
 * watch_nr: interaptiv 0-3 (0-1 instruction, 2-3 data)
 * cpu_id:    >= NR_CPUS - all cpus
 */
void del_watchpoint(int watch_nr, int cpu_id);


/**
 * only current cpu!
 * watch_nr: interaptiv 0-3 (0-1 instruction, 2-3 data)
 */
int watchpoint_busy(int watch_nr);
#endif /* CONFIG_AVM_WP */
#endif /* __avm_watch_h__ */
