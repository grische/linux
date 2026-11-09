// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2017-2019 AVM GmbH <fritzbox_info@avm.de> */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/clock.h>
#endif

#include <avm/sammel/debug.h>
#include <avm/compat/proc_fs.h>
#include "avm_sammel.h"

#if defined(CONFIG_PROC_FS)

struct _logger_tag {
#define MARK_CR (0x1 << 0)
#define MARK_SD (0x1 << 1)
	unsigned long long ts_nsec; /* time of first print */
	unsigned char marker;
	unsigned char cpu;
	unsigned short len;
} __packed;

#define LOGGER_TEXTBUF_SIZE (1024 - 32)
#define LOGGER_MAGIC 0x15102009
struct _logger_priv {
	struct list_head list;
	unsigned int magic;
	unsigned int last_is_no_cr;
	atomic_t ref_count;
	struct work_struct workitem_free_logger;
	char *log_name;
	char *log_cr_name;
	char *log_sd_name;
	char *logbuf;
	enum _avm_logger_flag logger_flag;
	unsigned int proc_marker;
	unsigned int proc_read_eof;
	unsigned int proc_read_pos;
	unsigned int proc_last_read_pos;
	unsigned int logbuf_write_pos;
	unsigned int first_valid_tag;
	unsigned int logbuf_size;
	spinlock_t lock;
	struct semaphore sema;
	char textbuf[LOGGER_TEXTBUF_SIZE];
};
static DEFINE_SPINLOCK(avm_logger_lock);
static LIST_HEAD(avm_logger_list);

#define PROC_AVM_PREFIX		"avm/"
#define PROC_PREFIX_CR		PROC_AVM_PREFIX"log_cr"
#define PROC_PREFIX_SD		PROC_AVM_PREFIX"log_sd"
#define PROC_PREFIX_LOG		PROC_AVM_PREFIX"log"

/* #define DBG_TRC(args...)	pr_info("[avm_logger]"args) */
/* #define DBG_LOG_TRC(args...)	pr_info("[avm_logger]"args) */

#ifndef DBG_LOG_TRC
#define DBG_LOG_TRC(args...)	no_printk(args)
#endif

#ifndef DBG_TRC
#define DBG_TRC(args...)	no_printk(args)
#endif


#define __get_logger_handle(p) atomic_inc(&(p)->ref_count)
/**
 * @brief create reference on logger instance
 */
static inline struct _logger_priv *get_logger_handle(void *handle)
{
	struct _logger_priv *plogpriv = (struct _logger_priv *)handle;
	unsigned long flags;

	if (plogpriv == NULL) {
		DBG_LOG_TRC("%s: invalid handle %p on %pS\n", __func__,
			    plogpriv, (void *)_RET_IP_);
		return NULL;
	}
	rte_spin_lock_irqsave(&avm_logger_lock, flags);
	if (plogpriv->magic != LOGGER_MAGIC) {
		rte_spin_unlock_irqrestore(&avm_logger_lock, flags);
		pr_err("[avm_logger]%s: invalid handle %p on %pS\n", __func__,
		       plogpriv, (void *)_RET_IP_);
		return NULL;
	}
	__get_logger_handle(plogpriv);
	rte_spin_unlock_irqrestore(&avm_logger_lock, flags);
	return plogpriv;
}

/**
 * only linux-kthread-context allowed
 */
static void free_logger(struct _logger_priv *plogpriv)
{
	if (plogpriv == NULL)
		return;
	plogpriv->magic = 0;
	if (*plogpriv->log_name)
		remove_proc_entry(plogpriv->log_name, NULL);
	if (*plogpriv->log_cr_name)
		remove_proc_entry(plogpriv->log_cr_name, NULL);
	if (*plogpriv->log_sd_name)
		remove_proc_entry(plogpriv->log_sd_name, NULL);
	kfree(plogpriv->logbuf);
	kfree(plogpriv);
}

/**
 */
static void free_logger_wq(struct work_struct *workptr)
{
	struct _logger_priv *plogpriv = container_of(
		workptr, struct _logger_priv, workitem_free_logger);

	free_logger(plogpriv);
}

/**
 * @brief free reference on logger-instance
 * any context allowed - (also non-linux)
 */
static void put_logger_handle(struct _logger_priv *plogpriv)
{
	unsigned long flags;

	rte_spin_lock_irqsave(&avm_logger_lock, flags);
	if (plogpriv->magic != LOGGER_MAGIC) {
		rte_spin_unlock_irqrestore(&avm_logger_lock, flags);
		pr_err("[avm_logger]%s: invalid handle %p\n", __func__,
		       plogpriv);
		dump_stack();
		return;
	}
	DBG_LOG_TRC("%s(%p) ref_count=%d - %pS\n", __func__, plogpriv,
		    atomic_read(&plogpriv->ref_count), (void *)_RET_IP_);
	/*--- atomic_sub_and_test() returns true if the result is 0, or false for all other ---*/
	if (atomic_sub_and_test(1, &plogpriv->ref_count) == false) {
		rte_spin_unlock_irqrestore(&avm_logger_lock, flags);
		return;
	}
	DBG_LOG_TRC("%s(%p) - free all\n", __func__, plogpriv);
	plogpriv->magic = 0;
	list_del(&plogpriv->list);
	rte_spin_unlock_irqrestore(&avm_logger_lock, flags);
	/*--- all free's only in kthread-context ---*/
	rte_schedule_work(&plogpriv->workitem_free_logger);
}

/**
 */
static inline unsigned int norm_position(struct _logger_priv *plogpriv,
					 unsigned int pos)
{
	if (pos >= plogpriv->logbuf_size) {
		pos -= plogpriv->logbuf_size;
	}
	return pos;
}

/**
 */
static inline unsigned int tag_overlap(unsigned int start, unsigned int end,
				       unsigned int pos)
{
	if (start < end) {
		return (pos >= start) && (pos < end);
	} else if (pos >= start) {
		return 1;
	} else if (pos < end) {
		return 1;
	}
	return 0;
}

/**
 */
static inline unsigned int position_inc(struct _logger_priv *plogpriv, unsigned int pos)
{
	pos++;
	if (pos >= plogpriv->logbuf_size)
		pos = 0;
	return pos;
}

/**
 */
static inline unsigned int position_add(struct _logger_priv *plogpriv, unsigned int pos, unsigned int add_value)
{
	pos += add_value;
	if (pos >= plogpriv->logbuf_size)
		pos = pos % plogpriv->logbuf_size;
	return pos;
}


/**
 * @return new relative pos
 */
static inline unsigned int fill_logtagstruct(struct _logger_priv *plogpriv,
					     unsigned int pos,
					     struct _logger_tag *plogtag)
{
	unsigned char *buf = (unsigned char *)plogtag;
	unsigned int i;

	for (i = 0; i < sizeof(struct _logger_tag); i++) {
		buf[i] = plogpriv->logbuf[pos];
		pos = position_inc(plogpriv, pos);
	}
	return pos;
}

/**
 * @return new relative pos
 */
static inline unsigned int set_logtag(struct _logger_priv *plogpriv,
				      unsigned int pos,
				      struct _logger_tag *plogtag)
{
	unsigned char *dst = (unsigned char *)plogtag;
	unsigned int i;

	for (i = 0; i < sizeof(struct _logger_tag); i++) {
		plogpriv->logbuf[pos] = dst[i];
		pos = position_inc(plogpriv, pos);
	}
	return pos;
}

/**
 * @return new relative pos
 */
static inline unsigned int copy_to_ringbuf(struct _logger_priv *plogpriv,
					   unsigned int pos, const char *src,
					   unsigned int size)
{
	if ((pos + size) > plogpriv->logbuf_size) {
		unsigned int split;

		split = plogpriv->logbuf_size - pos;
		memcpy(&plogpriv->logbuf[pos], src, split);
		src += split;
		size -= split;
		pos = 0;
	}
	memcpy(&plogpriv->logbuf[pos], src, size);
	pos = position_add(plogpriv, pos, size);
	return pos;
}

/**
 * @return new relative pos
 */
static inline unsigned int copy_from_ringbuf(struct _logger_priv *plogpriv,
					     unsigned int pos,
					     unsigned int logtag_len, char *dst,
					     unsigned int max_dst_size)
{
	unsigned int size;

	if (logtag_len > max_dst_size - 1) {
		size = max_dst_size - 1;
	} else {
		size = logtag_len;
	}
	if ((pos + size) > plogpriv->logbuf_size) {
		unsigned int split;

		split = plogpriv->logbuf_size - pos;
		memcpy(dst, &plogpriv->logbuf[pos], split);
		dst += split;
		size -= split;
		logtag_len -= split;
		pos = 0;
	}
	memcpy(dst, &plogpriv->logbuf[pos], size);
	dst[size] = 0;
	return norm_position(plogpriv, pos + logtag_len);
}

/**
 */
static void append_tag(struct _logger_priv *plogpriv, const char *text,
		       unsigned int textlen, unsigned int cpu,
		       unsigned long long ts_nsec)
{
	struct _logger_tag logtag;
	unsigned int spos, epos;
	unsigned int old_first;

	if (unlikely(textlen == 0)) {
		return;
	}
	spos = norm_position(plogpriv, plogpriv->logbuf_write_pos);
	epos = norm_position(plogpriv, spos + textlen + 2 * sizeof(struct _logger_tag));
	/*--- new tag ovelaps old tag ?  - change first_valid_tag ---*/
	DBG_TRC("%s: spos=%u epos=%u first_valid_tag=%u\n", __func__, spos,
		epos, plogpriv->first_valid_tag);
	old_first = plogpriv->first_valid_tag;
	while (tag_overlap(spos, epos, plogpriv->first_valid_tag)) {
		fill_logtagstruct(plogpriv, plogpriv->first_valid_tag, &logtag);
		if (unlikely(logtag.len == 0)) {
			break;
		}
		plogpriv->first_valid_tag = norm_position(
			plogpriv, plogpriv->first_valid_tag + logtag.len);
		/*--- signalize to proc-interface, that tags overwritten ---*/
		DBG_TRC("\t\t-> new first_valid_tag=%u\n",
			plogpriv->first_valid_tag);
		if (tag_overlap(old_first, plogpriv->first_valid_tag,
				plogpriv->proc_read_pos)) {
			plogpriv->proc_read_pos = plogpriv->first_valid_tag;
		}
	}
	logtag.len = textlen + sizeof(struct _logger_tag);
	logtag.marker = 0;
	logtag.cpu = cpu;
	logtag.ts_nsec = ts_nsec;
	spos = set_logtag(plogpriv, spos, &logtag);
	plogpriv->logbuf_write_pos = copy_to_ringbuf(plogpriv, spos, text, textlen);
	DBG_TRC("%s: textlen=%u+(%zu) ts_ns=%llu, wpos=%u\n", __func__, textlen,
		sizeof(struct _logger_tag), logtag.ts_nsec,
		plogpriv->logbuf_write_pos);
	logtag.len = 0; /*--- marks the end of buffer ---*/
	set_logtag(plogpriv, plogpriv->logbuf_write_pos, &logtag);
}

/**
 */
static unsigned int get_text(struct _logger_priv *plogpriv, unsigned int pos,
			     char *text, unsigned int textlen,
			     unsigned int *cpu, unsigned long long *ts_nsec,
			     unsigned int marker)
{
	struct _logger_tag logtag;

	text[0] = 0;
	for (;;) {
		fill_logtagstruct(plogpriv, pos, &logtag);
		DBG_TRC("%s: pos=%u ts_ns=%llu marker=%x taglen=%u\n", __func__,
			pos, logtag.ts_nsec, logtag.marker, logtag.len);
		if (logtag.len == 0) {
			/*--- empty log marks an eof ---*/
			plogpriv->proc_read_eof = 1;
			break;
		}
		if (marker & logtag.marker) {
			pos = norm_position(plogpriv, pos + logtag.len);
			continue;
		}
		logtag.marker |= marker;
		pos = set_logtag(plogpriv, pos, &logtag);
		pos = copy_from_ringbuf(plogpriv, pos,
					logtag.len - sizeof(struct _logger_tag),
					text, textlen);
		if (cpu)
			*cpu = logtag.cpu;
		if (ts_nsec)
			*ts_nsec = logtag.ts_nsec;
		break;
	}
	return pos;
}

/**
 */
static char *print_time(u64 ts, char *buf)
{
	unsigned long rem_nsec;

	rem_nsec = do_div(ts, 1000000000);
	sprintf(buf, "[%5lu.%06lu] ", (unsigned long)ts, rem_nsec / 1000);
	return buf;
}

static char *print_cpu(unsigned int cpu, char *buf)
{
	if (cpu & 0x80) {
#if defined(CONFIG_AVM_FASTIRQ)
		sprintf(buf, "[F%x] ", cpu & 0xF);
#elif defined(CONFIG_MIPS)
		sprintf(buf, "[Y%x:%x] ", cpu & 0xF, (cpu >> 4) & 0x7);
#endif /*--- #elif defined(CONFIG_MIPS) ---*/
	} else {
		sprintf(buf, "[%x] ", cpu);
	}
	return buf;
}

/**
 */
static void *s_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct _logger_priv *plogpriv = (struct _logger_priv *)m->private;

	DBG_LOG_TRC("%s: readpos=%u read_eof=%u\n", __func__,
		    plogpriv->proc_read_pos, plogpriv->proc_read_eof);
	if (plogpriv->proc_read_eof) {
		return NULL;
	}
	return p;
}

/**
 */
static void *s_start(struct seq_file *m, loff_t *pos)
{
	struct _logger_priv *plogpriv = (struct _logger_priv *)m->private;

	DBG_LOG_TRC("%s: readpos=%u read_eof=%u\n", __func__,
		    plogpriv->proc_read_pos, plogpriv->proc_read_eof);
	return plogpriv;
}

/**
 */
static void s_stop(struct seq_file *m, void *p)
{
}

/**
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-larger-than="
static int s_show(struct seq_file *m, void *p)
{
	struct _logger_priv *plogpriv = (struct _logger_priv *)m->private;
	char textbuf[LOGGER_TEXTBUF_SIZE];
	char txttime[32], txtcpu[32];
	int lost = 0;
	unsigned long flags = 0;
	unsigned int cpu = 0;
	unsigned long long ts_nsec = 0;

	DBG_LOG_TRC("%s: readpos=%u read_eof=%u\n", __func__,
		    plogpriv->proc_read_pos, plogpriv->proc_read_eof);
	rte_spin_lock_irqsave(&plogpriv->lock, flags);
	if (plogpriv->proc_last_read_pos != plogpriv->proc_read_pos) {
		lost = 1;
	}
	plogpriv->proc_read_eof = 0;
	plogpriv->proc_read_pos = get_text(plogpriv, plogpriv->proc_read_pos,
					   textbuf, sizeof(textbuf), &cpu,
					   &ts_nsec, plogpriv->proc_marker);
	plogpriv->proc_last_read_pos = plogpriv->proc_read_pos;
	rte_spin_unlock_irqrestore(&plogpriv->lock, flags);
	if (lost) {
		seq_puts( m, "logger: some lines lost here - logger-memory too small?");
	}
	if (textbuf[0]) {
		seq_printf(m, "%s%s%s",
			   plogpriv->last_is_no_cr ? "" : print_time(ts_nsec, txttime),
			   plogpriv->last_is_no_cr ? "" : print_cpu(cpu, txtcpu), textbuf);
		if (textbuf[strlen(textbuf) - 1] == '\n')
			plogpriv->last_is_no_cr = 0;
		else
			plogpriv->last_is_no_cr = 1;
	}
	return 0;
}
#pragma GCC diagnostic pop

/**
 */
static const struct seq_operations avm_logger_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show
};

/**
 */
static int proc_logger_open(struct inode *inode, struct file *file)
{
	int ret = 0, cr = 0, found = 0;
	struct _logger_priv *plogpriv;
	unsigned long flags;
	char fullpath[512];
	const char *proc_name = "", *path = "";

	if (file->f_path.dentry) {
		proc_name = file->f_path.dentry->d_name.name;
		if (file->f_path.dentry->d_parent) {
			path = file->f_path.dentry->d_parent->d_name.name;
		}
	}
	snprintf(fullpath, sizeof(fullpath), "%s%s/%s", PROC_AVM_PREFIX, path,
		 proc_name);
	DBG_LOG_TRC("%s: '%s' '%s' -> '%s'\n", __func__, path, proc_name,
		    fullpath);
	rte_spin_lock_irqsave(&avm_logger_lock, flags);
	list_for_each_entry(plogpriv, &avm_logger_list, list) {
		if (strcmp(plogpriv->log_name, fullpath) == 0) {
			cr = 0, found = 1;
			break;
		}
		if (strcmp(plogpriv->log_cr_name, fullpath) == 0) {
			cr = MARK_CR, found = 1;
			break;
		}
		if (strcmp(plogpriv->log_sd_name, fullpath) == 0) {
			cr = MARK_SD, found = 1;
			break;
		}
	}
	if (found) {
		__get_logger_handle(plogpriv);
	}
	rte_spin_unlock_irqrestore(&avm_logger_lock, flags);
	if (found == 0) {
		return -ENODEV;
	}
	if (down_interruptible(&plogpriv->sema)) {
		return -EBUSY;
	}
	plogpriv->proc_marker = cr;
	plogpriv->proc_read_eof = 0;
	rte_spin_lock_irqsave(&plogpriv->lock, flags);
	plogpriv->proc_read_pos = plogpriv->first_valid_tag;
	plogpriv->proc_last_read_pos = plogpriv->proc_read_pos;
	rte_spin_unlock_irqrestore(&plogpriv->lock, flags);
	/*
	 * We keep iterator in m->private, since normal case is to
	 * s_start from where we left off, so we avoid doing
	 */
	ret = seq_open(file, &avm_logger_op);
	if (ret) {
		up(&plogpriv->sema);
		put_logger_handle(plogpriv);
	} else {
		((struct seq_file *)file->private_data)->private = plogpriv;
	}
	return ret;
}

/**
 */
static int proc_logger_close(struct inode *inode, struct file *file)
{
	int ret;
	struct seq_file *seq = file->private_data;
	struct _logger_priv *plogpriv = (struct _logger_priv *)seq->private;

	up(&plogpriv->sema);
	seq->private = NULL;
	ret = seq_release(inode, file);
	put_logger_handle(plogpriv);
	return ret;
}

/**
 */
static const struct proc_ops avm_logger_stat_fops = {
	.proc_open = proc_logger_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = proc_logger_close,
};

/**
 * @brief:       create logger
 * the log is located in /proc/avm/log/<loggername>
 * @param ringbuf_size  size of ringbuffer (minsize is PAGE_SIZE)
 * @logger_name			name of logger
 * @flag				or-able features
 *
 * @return handle - PTR or ERR_PTR
 */
void *avm_logger_create(size_t ringbuf_size, const char *logger_name,
			enum _avm_logger_flag flag)
{
	unsigned long flags;
	int name_len, result;
	struct _logger_priv *plogpriv = NULL;
	char *logbuf;

	if (ringbuf_size < PAGE_SIZE) {
		ringbuf_size = PAGE_SIZE;
	}
	if (logger_name == NULL) {
		return ERR_PTR(-EINVAL);
	}
	logbuf = kzalloc(ringbuf_size, GFP_KERNEL);
	if (logbuf == NULL) {
		return ERR_PTR(-ENOMEM);
	}
	DBG_LOG_TRC("%s(%s size=%zu)\n", __func__, logger_name, ringbuf_size);
	name_len = strlen(logger_name) + 1;
	plogpriv = kzalloc(sizeof(struct _logger_priv) + 3 * (name_len + 1) +
				   sizeof(PROC_PREFIX_SD) +
				   sizeof(PROC_PREFIX_CR) +
				   sizeof(PROC_PREFIX_LOG),
			   GFP_KERNEL);
	if (plogpriv == NULL) {
		result = -ENOMEM;
		goto fail;
	}
	plogpriv->magic       = LOGGER_MAGIC;
	plogpriv->logger_flag = flag;
	plogpriv->logbuf      = logbuf;
	logbuf		      = NULL;
	plogpriv->logbuf_size = ringbuf_size;
	plogpriv->log_name    = (char *)(plogpriv + 1);
	plogpriv->log_cr_name =
		plogpriv->log_name + name_len + sizeof(PROC_PREFIX_LOG);
	plogpriv->log_sd_name =
		plogpriv->log_cr_name + name_len + sizeof(PROC_PREFIX_CR);

	sprintf(plogpriv->log_name, "%s/%s", PROC_PREFIX_LOG, logger_name);
	if (!proc_create(plogpriv->log_name, 0440, NULL,
			 &avm_logger_stat_fops)) {
		plogpriv->log_name[0] = 0;
		result = -ENODEV;
		goto fail;
	}
	if (plogpriv->logger_flag & logger_log_cr_dir) {
		sprintf(plogpriv->log_cr_name, "%s/%s", PROC_PREFIX_CR,
			logger_name);
		if (!proc_create(plogpriv->log_cr_name, 0440, NULL,
				 &avm_logger_stat_fops)) {
			plogpriv->log_cr_name[0] = 0;
			result = -ENODEV;
			goto fail;
		}
	}
	if (plogpriv->logger_flag & logger_log_sd_dir) {
		sprintf(plogpriv->log_sd_name, "%s/%s", PROC_PREFIX_SD,
			logger_name);
		if (!proc_create(plogpriv->log_sd_name, 0440, NULL,
				 &avm_logger_stat_fops)) {
			plogpriv->log_sd_name[0] = 0;
			result = -ENODEV;
			goto fail;
		}
	}
	spin_lock_init(&plogpriv->lock);
	sema_init(&plogpriv->sema, 1);
	INIT_WORK(&plogpriv->workitem_free_logger, free_logger_wq);
	atomic_set(&plogpriv->ref_count, 1);

	rte_spin_lock_irqsave(&avm_logger_lock, flags);
	list_add(&plogpriv->list, &avm_logger_list);
	rte_spin_unlock_irqrestore(&avm_logger_lock, flags);

	DBG_LOG_TRC("%s handle=%p done\n", __func__, plogpriv);
	return plogpriv;
fail:
	pr_err("[avm_logger]%s: failed error=%d: %pS\n", __func__, result,
	       (void *)_RET_IP_);
	free_logger(plogpriv);
	kfree(logbuf);
	return ERR_PTR(result);
}
EXPORT_SYMBOL(avm_logger_create);

/**
 */
static asmlinkage int avmlogger_vprintk(struct _logger_priv *plogpriv,
					const char *fmt, va_list args)
{
	unsigned long long ts_nsec;
	unsigned long flags;
	unsigned int text_len;
	const char *text;
	int cpu;

#if defined(CONFIG_SMP) && defined(CONFIG_MIPS)
	int nonlinux_context = is_yield_context();
#elif defined(CONFIG_AVM_FASTIRQ)
	int nonlinux_context = firq_is_avm_rte();
#else
#define nonlinux_context 0
#endif /*--- #if defined(CONFIG_AVM_DEBUG) ---*/

	ts_nsec = rte_local_clock();
	cpu = get_cpu();
	if (nonlinux_context) {
		cpu |= ((nonlinux_context - 1) << 4) | 0x80;
	}
	rte_spin_lock_irqsave(&plogpriv->lock, flags);
	text_len = vscnprintf(plogpriv->textbuf, sizeof(plogpriv->textbuf), fmt,
			      args);
	if ((plogpriv->logger_flag & logger_no_printk_trace) == 0) {
		pr_info("%s", plogpriv->textbuf);
	}
	text = printk_skip_level(plogpriv->textbuf);
	text_len -= text - plogpriv->textbuf;

	append_tag(plogpriv, text, text_len, cpu, ts_nsec);
	rte_spin_unlock_irqrestore(&plogpriv->lock, flags);
	put_cpu();
	return text_len;
}

/*
 * - rate limiting
 * @rs: ratelimit_state data
 * @func: name of calling function
 *
 * This enforces a rate limit: not more than @rs->burst callbacks
 * in every @rs->interval
 *
 * RETURNS:
 * 0 means callbacks will be suppressed.
 * 1 means go ahead and do it.
 */
int avm_logger_ratelimit(void *handle, struct ratelimit_state *rs,
			 const char *func)
{
	unsigned long flags;
	int ret;

	if (!rs->interval)
		return 1;

	/*
	 * If we contend on this state's lock then almost
	 * by definition we are too busy to print a message,
	 * in addition to the one that will be printed by
	 * the entity that is holding the lock already:
	 */

	if (!rte_raw_spin_trylock_irqsave(&rs->lock, flags))
		return 0;
	if (!rs->begin)
		rs->begin = jiffies;

	if (time_is_before_jiffies(rs->begin + rs->interval)) {
		if (rs->missed)
			avm_logger_printk(handle,
					  KERN_WARNING
					  "%s: %d callbacks suppressed\n",
					  func, rs->missed);
		rs->begin = 0;
		rs->printed = 0;
		rs->missed = 0;
	}
	if (rs->burst && rs->burst > rs->printed) {
		rs->printed++;
		ret = 1;
	} else {
		rs->missed++;
		ret = 0;
	}
	rte_raw_spin_unlock_irqrestore(&rs->lock, flags);
	return ret;
}
EXPORT_SYMBOL(avm_logger_ratelimit);
/**
 * @brief logger - output also to printk
 * @param handle logger-handle from avm_logger_create()
 * @param fmt and variable list like printk
 */
asmlinkage int avm_logger_printk(void *handle, const char *fmt, ...)
{
	struct _logger_priv *plogpriv = get_logger_handle(handle);
	va_list args;
	int r;

	va_start(args, fmt);
	if (plogpriv == NULL) {
		/*--- fallback if logger-handle invalid ---*/
		r = vprintk(fmt, args);
	} else {
		r = avmlogger_vprintk(plogpriv, fmt, args);
		put_logger_handle(plogpriv);
	}
	va_end(args);
	return r;
}
EXPORT_SYMBOL(avm_logger_printk);
/**
 * close logger
 */
void avm_logger_close(void *handle)
{
	if (handle) {
		DBG_LOG_TRC("%s(%p)\n", __func__, handle);
		put_logger_handle(handle);
	}
}
EXPORT_SYMBOL(avm_logger_close);

/**
 * @brief create avm/log
 */
static int __init avm_logger_init(void)
{
	proc_mkdir(PROC_PREFIX_LOG, NULL);
	return 0;
}

late_initcall(avm_logger_init);
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
