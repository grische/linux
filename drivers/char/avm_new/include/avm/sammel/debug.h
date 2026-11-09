/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _linux_avm_debug_h_
#define _linux_avm_debug_h_
/*
 * die /dev/debug-Schnittstelle
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/notifier.h>
#include <linux/ratelimit.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
/* Linux < v5.15 does not ship a version of stdarg.h, therefore we
 * unfortunately have to use the user-space version for now. */
#include <stdarg.h>
#else
#include <linux/stdarg.h>
#endif

/**
 * Debug-Funktion am Treiber anmelden
 * prefix: der Inputdaten werden nach diesem Prefix durchsucht, und bei Match
 * wird die CallbackFkt aufgerufen
 * um also den string 'blabla=haha' zum Treiber angemeldet mit prefix 'unicate_' zu transportieren
 * ist einfach ein "echo unicate_blabla=haha >/dev/debug" auf der Konsole auszufuehren
 * ret: handle (fuer UnRegister)
 */
void *avm_DebugCallRegister(char *prefix,
			    void (*CallBackDebug)(char *string, void *refdata),
			    void *refdata);

/**
 * Debug-Funktion am Treiber abmelden
 */
void avm_DebugCallUnRegister(void *handle);

/**
 * signal 0 ...31
 * signal: 0 supportdata per pushmail
 */
void avm_DebugSignal(unsigned int signal);
asmlinkage void avm_DebugSignalLog(unsigned int signal, char *fmt, ...);

/**
 */
struct file;
struct inode;

typedef ssize_t (*avm_debug_write_t)(struct file *filp,
				     const char *write_buffer,
				     size_t write_length, loff_t *write_pos);
typedef ssize_t (*avm_debug_read_t)(struct file *filp, char *read_buffer,
				    size_t max_read_length, loff_t *read_pos);
typedef int (*avm_debug_open_t)(struct inode *inode, struct file *filp);
typedef int (*avm_debug_close_t)(struct inode *inode, struct file *filp);
typedef long (*avm_debug_ioctl_t)(struct file *filp, unsigned int cmd,
				  unsigned long args);

int avm_debug_register_minor(int minor, avm_debug_open_t open,
			     avm_debug_close_t close, avm_debug_write_t write,
			     avm_debug_read_t read, avm_debug_ioctl_t ioctl);

int avm_debug_release_minor(int minor);

/**
 * Genutzte Minor Nummern
 */
#define NLR_AUDIO_MINOR 1

/**
 */
#define AVM_DEBUG_MAX_MINOR 1
#define AVM_DEBUG_MINOR_COUNT (AVM_DEBUG_MAX_MINOR + 1)
/**
 * avm_stack_usage.c
 */
/**
 * @brief check if pointer inside task_page
 * @param	comm      task-name
 *		commlen   len of comm (please reserve 10 bytes more than TASK_COMM_LEN)
 * @return == 0 outside else start-pointer
 */
unsigned long get_taskstack_area(unsigned long addr, char *comm,
				 unsigned int commlen, int only_current);

/**
 * avm_logger.c
 */
enum _avm_logger_flag {
	logger_no_printk_trace = 0x1 << 0,
	/* log-link also in /proc/avm/log_cr */
	logger_log_cr_dir = 0x1 << 1,
	/* log-link also in /proc/avm/log_sd */
	logger_log_sd_dir = 0x1 << 2,
};

#if defined(CONFIG_PROC_FS)
/**
 * @brief:       create logger
 * the log is locate in /proc/avm/log/<loggername>
 * @param ringbuf_size  size of ringbuffer (minsize is PAGE_SIZE)
 * @logger_name			name of logger
 * @flag				or-able features (see above)
 *
 * @return handle - PTR or ERR_PTR
 */
void *avm_logger_create(size_t ringbuf_size, const char *logger_name,
			enum _avm_logger_flag flag);

/**
 * close logger
 */
void avm_logger_close(void *handle);

/**
 * @brief logger - output also to printk
 * @param handle logger-handle from avm_logger_create()
 * @param fmt and variable list like printk
 */
asmlinkage __printf(2, 3) int avm_logger_printk(void *handle, const char *fmt,
						...);
int avm_logger_ratelimit(void *handle, struct ratelimit_state *rs,
			 const char *func);

#define avm_logger_printk_ratelimited(handle, fmt, ...)                        \
	({                                                                     \
		static DEFINE_RATELIMIT_STATE(_avmlogger_rs,                   \
					      DEFAULT_RATELIMIT_INTERVAL,      \
					      DEFAULT_RATELIMIT_BURST);        \
									       \
		if (avm_logger_ratelimit(handle, &_avmlogger_rs, __func__))    \
			avm_logger_printk(handle, fmt, ##__VA_ARGS__);         \
	})
#else /*--- #if defined(CONFIG_PROC_FS) ---*/
static inline void *avm_logger_create(size_t ringbuf_size,
				      const char *logger_name,
				      enum _avm_logger_flag flag)
{
	return NULL;
}

static inline void avm_logger_close(void *handle)
{
}
#define avm_logger_printk(handle, fmt, ...) pr_info(fmt)
#define avm_logger_printk_ratelimited(handle, fmt, ...) printk_ratelimited(fmt)

#endif /*--- #else ---*/ /*--- #if defined(CONFIG_PROC_FS) ---*/

/*
 * Allows to inject information into the avm_oom_show_memstat stream.
 *
 * This is called in an atomic context and possible during panic, so ensure
 * robust handling.
 *
 * The provided data is a seq_file to output the data. Use sseq_* functions
 * to ensure flushing the output.
 */
int avm_oom_info_chain_register(struct notifier_block *nb);
int avm_oom_info_chain_unregister(struct notifier_block *nb);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#  define vprintk_emit_nodict(_facility, _ll, _fmt, _args)	\
					vprintk_emit(_facility, _ll, NULL, _fmt, _args)
#else
#  define vprintk_emit_nodict(_facility, _ll, _fmt, _args)	\
					vprintk_emit(_facility, _ll, NULL, 0, _fmt, _args)
#  ifndef LOGLEVEL_DEFAULT
#    define LOGLEVEL_DEFAULT 	-1
#  endif
#endif

#endif /*--- #ifndef _linux_avm_debug_h_ ---*/
