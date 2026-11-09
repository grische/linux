#ifndef __avm_enh_h__
#define __avm_enh_h__

#include <asm/avm_enh/bugtable.h>

/**
 * bthelper.c
 */
bool arch_trigger_all_cpu_backtrace(const cpumask_t *cpu_mask);
char *get_user_symbol(char *txt, unsigned int maxttxlen, unsigned long gp,
		      unsigned long addr);
void show_code_position_by_epc(char *prefix, struct pt_regs *regs);

/**
 * @brief:
 *   track unaligned access initiators in kernel and userspace
 *   and decide, what to do based on ai_{kernel,usermode}
 * @return:
 *   sigbus : -2
 *   sigill : -1
 *   Ok     : 0
 */
#if defined(CONFIG_AVM_ENHANCED)
int track_unaligned_position(struct pt_regs *regs);
#else
static inline int track_unaligned_position(struct pt_regs *regs) { return 0; }
#endif

#define __get_last_unaligned_info
const char *get_last_unaligned_info(unsigned long *ai_count,
				    unsigned long *last_pc, int user);
#define avm_trigger_all_cpu_backtrace(dummy, cpumask)                          \
	arch_trigger_all_cpu_backtrace(cpumask)

char *arch_print_memory_classifier(char *txt, unsigned int txtlen, unsigned long addr, int include_addr_prefix);
#define print_memory_classifier arch_print_memory_classifier

void arch_show_register_memoryclassifier(const struct pt_regs *pregs);
#define show_register_memoryclassifier arch_show_register_memoryclassifier
void arch_show_stacktrace_memoryclassifier(const struct pt_regs *pregs);
#define show_stacktrace_memoryclassifier arch_show_stacktrace_memoryclassifier

/**
 * stop all cpu's over monitor-yield-thread
 */
int avm_stop_all_other_cpus(void);

/**

 * vmalloc.c
 */
unsigned long get_vmap_area(unsigned long addr, unsigned long *caller, unsigned long *size, unsigned long *vmflags);
int get_used_vmalloc_mem(void);

/**
 * slub.c
 */
unsigned long get_kmemalloc_area(unsigned long addr, unsigned long *caller, const char **cache_name,
								 unsigned long *size, int *freed);
/**
 * kernel/time/tick-sched.c:
 * jiffies-correction in nmi-handler
 */
extern void avm_tick_jiffies_update(void);

/*
 * serial_core.c:
 * piglet interface
 */
#ifdef CONFIG_AVM_TTY_POLLING
struct polling_tty_port;

struct polling_tty_port *avm_tty_poll_acquire(const char *ttyname);
int  avm_tty_poll_configure(struct polling_tty_port *pport, int baud,
			    int parity, int bits, int flow);
void avm_tty_poll_release(struct polling_tty_port *pport);
int  avm_tty_poll_get_char(struct polling_tty_port *pport);
void avm_tty_poll_put_char(struct polling_tty_port *pport, unsigned char c);
#endif

/**
 * expression.c
 * expression-parser deluxe
 */
enum _cond_type {
	cond_type_unknown  = 0,
	cond_type_absolute = 1, /*--- 0x123   ---*/
	cond_type_addr     = 2, /*--- *0x123  -> muss get_value_by_type() liefern    ---*/
	cond_type_register = 3, /*--- $0, $1 ... -> muss get_value_by_type() liefern ---*/
	cond_type_macro    = 4, /*--- () ---*/
};

/**
 * \\brief: Handle anlegen fuer prepare_condition()
 * Export-Funktion
 * max_conditions: Anzahl der Conditions (0: default =100)
 * (*get_value_by_type): Callback fuer Aufloesung ret = 0: ok sonst Fehler
 * (*get_value_by_symbol): Callback fuer Symbol oder Registername (wenn type = cond_type_register)
 * falls cond_type_register: get_value_by_symbol() wird auch mit sym=NULL aufgerufen um register-idx abzuchecken!
 * ret = 0: ok sonst Fehler
 */
void *condition_alloc(
                      int (*get_value_by_type)(void *symbolref, enum _cond_type type, unsigned long *value),
                      int (*get_value_by_symbol)(void *typeref, enum _cond_type type, char *sym, unsigned long *value),
		unsigned int max_conditions);

/**
 */
void condition_free(void *handle);

/**
 */
void condition_put(void *handle);
void condition_get(void *handle);

/**
 * Export-Funktion
 * Expression parsen
 * Ruft (falls noetig) Callback get_value_by_symbol(symbolref,...) auf
 * stringlen: 0 string ist nullterminiert
 * ret 0: ok
 */
int condition_parse(void *handle, void *symbolref, char *string, unsigned int stringlen);

/**
 * Export-Funktion
 * Expression berechnen
 * Ruft (falls noetig) Callback get_value_by_type(typeref,..) auf
 * ret 0: ok
 */
int condition_calculate(void *handle, void *typeref, unsigned long *value);

/**
 * Export-Funktion
 * falls txt = NULL -> stdout
 */
char *condition_print(void *handle, char *txt, unsigned int txt_len);

#endif /* __avm_enh_h__ */
