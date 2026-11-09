// SPDX-License-Identifier: GPL-2.0+

/*
 * Ueber echo "<func>(arg0, .... arg4) [cpu<x>] [simulate]" >/proc/avm/call
 * eine beliebige Kernelfunktion ausfuehren - nur Entwicklerversion
 */
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <avm/sammel/simple_proc.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <avm/watchdog/watchdog.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/mm.h>
#pragma GCC pop_options

#include <avm/event/event.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#include <asm/avm_enh/avm_enh.h>
#endif
#include <asm/mach_avm.h>


#define print_out(seq, args...)                                                \
	{                                                                      \
		if (seq)                                                       \
			seq_printf(seq, args);                                 \
		else                                                           \
			pr_err(args);                                          \
	}

struct _call_list {
	unsigned int args[5];
	unsigned int argcount;
	unsigned int argstring_mask;
	unsigned int simulate;
	unsigned int asmlink;
	unsigned long func;
	char *tmpbuf;
};

static char *print_callback_string(char *txt, unsigned int txt_len,
				   struct _call_list *call)
{
	char *p = txt;
	unsigned int idx = 0, len;

	len = snprintf(p, txt_len, "%pS(", (void *)call->func);
	if (txt_len >= len) {
		txt_len -= len;
		p += len;
	}

	for (idx = 0; idx < call->argcount; idx++) {
		if (call->argstring_mask & (1 << idx)) {
			len = snprintf(p, txt_len, "%s\"%s\"", idx ? ", " : "",
				       (char *)call->args[idx]);
		} else {
			len = snprintf(p, txt_len, "%s0x%x", idx ? ", " : "",
				       call->args[idx]);
		}
		if (txt_len >= len) {
			txt_len -= len;
			p += len;
		}
	}
	snprintf(p, txt_len, ")");
	return txt;
}

typedef unsigned int (*call_func0_t)(void);
typedef unsigned int (*call_func1_t)(unsigned int);
typedef unsigned int (*call_func2_t)(unsigned int, unsigned int);
typedef unsigned int (*call_func3_t)(unsigned int, unsigned int, unsigned int);
typedef unsigned int (*call_func4_t)(unsigned int, unsigned int, unsigned int,
		unsigned int);
typedef unsigned int (*call_func5_t)(unsigned int, unsigned int, unsigned int,
		unsigned int, unsigned int);

static void call_function(struct _call_list *call)
{
	unsigned int ret = 0;
	char txt[256];

	if (call->simulate) {
		pr_err("Call%s:%s\n", call->asmlink ? "(asmlinkage)" : "",
		       print_callback_string(txt, sizeof(txt), call));
		kfree(call);
		return;
	}
	if (call->asmlink) {
		switch (call->argcount) {
		case 0:
			ret = ((asmlinkage call_func0_t)call->func)();
			break;
		case 1:
			ret = ((asmlinkage call_func1_t)call->func)(call->args[0]);
			break;
		case 2:
			ret = ((asmlinkage call_func2_t)call->func)(
				call->args[0], call->args[1]);
			break;
		case 3:
			ret = ((asmlinkage call_func3_t)call->func)(
				call->args[0], call->args[1], call->args[2]);
			break;
		case 4:
			ret = ((asmlinkage call_func4_t)call->func)(
				call->args[0], call->args[1], call->args[2],
				call->args[3]);
			break;
		case 5:
			ret = ((asmlinkage call_func5_t)call->func)(
				call->args[0], call->args[1], call->args[2],
				call->args[3], call->args[4]);
			break;
		}
	} else {
		switch (call->argcount) {
		case 0:
			ret = ((call_func0_t)call->func)();
			break;
		case 1:
			ret = ((call_func1_t)call->func)(call->args[0]);
			break;
		case 2:
			ret = ((call_func2_t)call->func)(
				call->args[0], call->args[1]);
			break;
		case 3:
			ret = ((call_func3_t)call->func)(
				call->args[0], call->args[1], call->args[2]);
			break;
		case 4:
			ret = ((call_func4_t)call->func)(
				call->args[0], call->args[1], call->args[2],
				call->args[3]);
			break;
		case 5:
			ret = ((call_func5_t)call->func)(
				call->args[0], call->args[1], call->args[2],
				call->args[3], call->args[4]);
			break;
		}
	}

	pr_err("\n----> Called%s:%s ret=0x%08x\n",
	       call->asmlink ? "(asmlinkage)" : "",
	       print_callback_string(txt, sizeof(txt), call), ret);
	kfree(call);
}

#if defined(CONFIG_SMP)
static void start_func_on_cpu(void *param)
{
	call_function((struct _call_list *)param);
}
#endif /*--- #if defined(CONFIG_SMP) ---*/

#define SKIP_UNTIL_SPACES_OR_SIGN(txt, sign)                                   \
	do {                                                                   \
		while (*txt &&                                                 \
		       ((*txt != ' ') && (*txt != '\t') && (*txt != sign)))    \
			txt++;                                                 \
	} while (0)
#define SKIP_UNTIL_SEPERATOR(txt)                                              \
	do {                                                                   \
		while (*txt && ((*txt != ' ') && (*txt != ',') &&              \
				(*txt != '\t') && (*txt != ')')))              \
			txt++;                                                 \
	} while (0)
#define SKIP_UNTIL(txt, sign)                                                  \
	do {                                                                   \
		while (*txt && (*txt != sign))                                 \
			txt++;                                                 \
	} while (0)

#define SKIP_SPACES(p) { while ((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) (p)++; }

/*
 * format: (arg1, arg2, arg3 ...)
 * arg<x>  0x<val>   -> hex
 *          val      -> dezimal
 *          'string' -> string
 *          "string" -> string
 *
 * ret: NULL Fehler, sonst pointer hinter ')'
 * Beispiel:
 * echo 'strstr("test", "s" ) ' >/proc/avm/call
 */
static char *scan_arguments(char *txt, struct _call_list *call)
{
	unsigned int idx = 0;

	txt = skip_spaces(txt);
	SKIP_UNTIL(txt, '(');
	if (*txt == 0) {
		pr_err("invalid arguments - missing '('\n");
		return NULL;
	}
	txt++;
	for (;;) {
		txt = skip_spaces(txt);
		if (txt[0] == 0) {
			pr_err("%s:missing ')'\n", __func__);
			return NULL;
		}
		if (txt[0] == ')') {
			txt++;
			break;
		}
		if (idx >= ARRAY_SIZE(call->args)) {
			pr_err("%s:too much arguments\n", __func__);
			return NULL;
		}
		if (txt[0] == '0' && txt[1] == 'x') {
			sscanf(txt, "0x%x", &call->args[idx]);
		} else if (txt[0] == '\'' || txt[0] == '"') {
			unsigned int len;
			unsigned char endsign = txt[0];
			char *end;

			txt++;
			end = txt;
			SKIP_UNTIL(end, endsign);
			if (*end != endsign) {
				pr_err("%s:invalid arguments - missing %c\n",
				       __func__, endsign);
				return NULL;
			}
			len = end - txt;
			memcpy(call->tmpbuf, txt, len);
			call->args[idx] = (unsigned int)call->tmpbuf;
			call->argstring_mask |= 1 << idx;
			call->tmpbuf += len + 1;
			txt = end + 1;
		} else {
			sscanf(txt, "%d", &call->args[idx]);
		}
		idx++;
		SKIP_UNTIL_SEPERATOR(txt);
		if (*txt == ',')
			txt++;
	}
	call->argcount = idx;
	return txt;
}

/*
 * fuehrt eine Funktion aus:
 */
static int lproc_call(char *buffer, void *priv)
{
	struct _call_list *pcall;
	char namebuf[KSYM_NAME_LEN];
	char *p1, *p = buffer;
	unsigned int cpu, this_cpu;

	pcall = kzalloc(sizeof(struct _call_list) + strlen(buffer) + 1 +
				ARRAY_SIZE(pcall->args),
			GFP_KERNEL);
	if (pcall == NULL) {
		return 0;
	}
	pcall->tmpbuf = (char *)(pcall + 1);
	p = skip_spaces(p);
	/*--- extrahiere Funktionsname/Funktionspointer: ---*/
	p1 = p;
	SKIP_UNTIL_SPACES_OR_SIGN(p1, '(');
	if (*p1) {
		size_t len = min((size_t)(p1 - p), sizeof(namebuf) - 1);

		memcpy(namebuf, p, len);
		namebuf[len] = 0;
		pcall->func = (unsigned long)kallsyms_lookup_name(namebuf);
	}
	if (pcall->func == 0) {
		sscanf(p, "%lx", &pcall->func);
	}
	if (!func_ptr_is_kernel_text((void *)pcall->func)) {
		pr_err("invalid func-addr use: <funcname/addr>(arg0, .. arg4) [cpu<x>] [simulate] [asmlinkage] ('arg' for string)\n");
		kfree(pcall);
		return 0;
	}
	/*--- extrahiere Argumente: ---*/
	p = scan_arguments(p, pcall);
	if (p == NULL) {
		kfree(pcall);
		return 0;
	}
	/*--- nur simulieren (geparste Argumente anzeigen) ? ---*/
	if (strstr(p, "sim")) {
		pcall->simulate = 1;
	}
	if (strstr(p, "asm")) {
		pcall->asmlink = 1;
	}
	/*---ausfuehren auf CPU<x> ? ---*/
	this_cpu = get_cpu();
	p = strstr(p, "cpu");
	if (p) {
		p += sizeof("cpu") - 1;
		cpu = p[0] - '0';
	} else {
		cpu = this_cpu;
	}
#if defined(CONFIG_SMP)
	if (cpu_online(cpu) && (cpu != this_cpu)) {
		smp_call_function_single(cpu, start_func_on_cpu, pcall, 0);
	} else
#endif /*--- #if defined(CONFIG_SMP) ---*/
		call_function(pcall);
	put_cpu();
	return 0;
}

static void simulate_kernel_crash(void)
{
	int *inval_pointer = NULL;

	*inval_pointer = 0x43524153;
}

/**
 */
static void event_dummy(void *context, enum _avm_event_id id)
{
}

/**
 */
static void event_received(void *context, unsigned char *buf, unsigned int len)
{
	msleep(10); /*--- Abarbeitung kuenstlich verzoegern ---*/
	/*---	pr_err("%s: %u:\n", __func__, __LINE__); ---*/
};

static struct task_struct *event_kthread;
static unsigned int event_trigger_msec, event_trigger_count;

/**
 * use remote_watchdog-event (only used on puma6) to simulate event-overun or stress a little bit
 */
static int simulate_event_ovr_thread(void *data)
{
	void *sink_handle, *source_handle;
	struct _avm_event_id_mask id_mask;
	const struct _avm_event_remotewatchdog event = {
		.event_header = { .id = avm_event_id_remotewatchdog },
		.cmd = wdt_trigger,
		.name = "DUMMY",
		.param = 120 + WDT_DEFAULT_TIME * 4,
	};
	source_handle = avm_event_source_register("avm_event_remotewatchdog",
						  avm_event_build_id_mask(&id_mask, 1,
						  avm_event_id_remotewatchdog),
		event_dummy, NULL);
	sink_handle = avm_event_sink_register("avm_event_remotewatchdog_sink",
					      avm_event_build_id_mask(&id_mask, 1,
					      avm_event_id_remotewatchdog),
		event_received, NULL);
	if ((source_handle == NULL) || (sink_handle == NULL)) {
		goto exit_thread;
	}
	while (!kthread_should_stop() && event_trigger_count) {
		struct _avm_event_remotewatchdog *pevent = kmalloc(sizeof(struct _avm_event_remotewatchdog),
								   GFP_KERNEL);

		event_trigger_count--;
		if (pevent == NULL) {
			pr_warn("%s: can't alloc event\n", __func__);
			break;
		}
		memcpy(pevent, &event,
		       sizeof(struct _avm_event_remotewatchdog));
		avm_event_source_trigger(source_handle, avm_event_id_remotewatchdog,
					 sizeof(struct _avm_event_remotewatchdog), pevent);
		msleep(event_trigger_msec);
	}
exit_thread:
	if (source_handle)
		avm_event_source_release(source_handle);
	if (sink_handle)
		avm_event_sink_release(sink_handle);

	event_kthread = NULL;
	return 0;
}

static void **memtable;
static unsigned int free_type;

static void simulate_oom(unsigned int slab, unsigned int order,
			 unsigned int limit)
{
	ktime_t start_time, stop_time;
	unsigned long tdiff;
	unsigned int size = 1U << order;
	unsigned int i, ii;

	if (memtable) {
		i = 0;
		start_time = ktime_get();
		while (memtable[i]) {
			if (free_type) {
				kfree(memtable[i]);
			} else {
				/*--- pr_info("[%u]vfree(%p)\n",  i, memtable[i]); ---*/
				vfree(memtable[i]);
			}
			i++;
		}
		stop_time = ktime_get();
		tdiff = ktime_us_delta(stop_time, start_time);
		pr_err("%s: measured %lu ms (%lu us/free)\n", __func__, tdiff / 1000, tdiff / limit);

		kfree(memtable);
		memtable = NULL;
	}
	if (limit) {
		memtable = kzalloc((limit + 1) * sizeof(void *), GFP_KERNEL);
		free_type = slab;
	} else {
		limit = (unsigned int)-1;
	}
	start_time = ktime_get();
	for (i = 0; i < limit; i++) {
		unsigned int *p;

		if (slab) {
			p = kmalloc(size, GFP_KERNEL);
			if (memtable) {
				memtable[i] = p;
			}
		} else {
			p = vmalloc(size);
			if (memtable) {
				memtable[i] = p;
				/*--- pr_info("[%u]vmalloc(%u) = %p\n",  i, size, memtable[i]); ---*/
			}
		}
		if (p == NULL) {
			break;
		}
		for (ii = 0; ii < size / sizeof(unsigned int); ii++) {
			p[ii] = ii ^ (unsigned int)p;
		}
	}
	stop_time = ktime_get();
	tdiff = ktime_us_delta(stop_time, start_time);

	pr_err("%s: measured %lu ms (%lu us/alloc)\n", __func__, tdiff / 1000, tdiff / limit);
}

#define SIMULATE_WD "simulate_wdr"
static void simulate_wdr(void)
{
	AVM_WATCHDOG_register(0, SIMULATE_WD, sizeof(SIMULATE_WD));
}

#if defined(CONFIG_SMP)
static void knockout_cpu(void *dummy)
{
	local_irq_disable();
	for (;;)
		;
}
#endif /*--- #if defined(CONFIG_SMP) ---*/
#define SKIP_UNTIL_SPACES(txt)                                                 \
	do {                                                                   \
		while (*txt && ((*txt != ' ') && (*txt != '\t')))              \
			txt++;                                                 \
	} while (0)

static unsigned long scan_addr_and_param(char *txt, unsigned int *param)
{
	size_t len;
	char namebuf[KSYM_NAME_LEN];
	unsigned long addr;
	char *p;

	txt = skip_spaces(txt);
	p = txt;
	SKIP_UNTIL_SPACES(p);
	len = min((size_t)(p - txt), sizeof(namebuf) - 1);
	memcpy(namebuf, txt, len);
	namebuf[len] = 0;
	addr = (unsigned long)kallsyms_lookup_name(namebuf);
	if (addr == 0) {
		sscanf(txt, "%lx %x", &addr, param);
	} else {
		if (*p)
			p++;

		txt = skip_spaces(txt);
		sscanf(p, "%x", param);
	}
	return addr;
}

struct cmd {
	const char *name;
	const char *help;

	int (*func)(char *params);
};

#if defined(print_memory_classifier)
enum _mclass_type {
	check_kmalloc,
	check_kmalloc_o,
	check_vmalloc,
	check_page,
	check_page_o,
	check_task,
	check_stack,
	check_moduleaddr
};

struct _mclass_check {
	enum _mclass_type type;
	const char *mclass_name;
};

#define MCLASS_ENTRY(a) { .type = a, .mclass_name = #a }

static const struct _mclass_check mclass_check[] = {
	MCLASS_ENTRY(check_kmalloc),
	MCLASS_ENTRY(check_kmalloc_o),
	MCLASS_ENTRY(check_vmalloc),
	MCLASS_ENTRY(check_page),
	MCLASS_ENTRY(check_page_o),
	MCLASS_ENTRY(check_task),
	MCLASS_ENTRY(check_stack),
	MCLASS_ENTRY(check_moduleaddr)
};

static void *mclass_alloc_function(enum _mclass_type type)
{
	register unsigned long sp asm("sp");
	struct page *pg;
	void *p = NULL;

	switch (type) {
	case check_kmalloc:
		p = kmalloc(1000, GFP_KERNEL);
		break;
	case check_kmalloc_o:
		p = kmalloc(16000, GFP_KERNEL);
		break;
	case check_vmalloc:
		p = vmalloc(12000);
		break;
	case check_page:
		pg = alloc_pages(GFP_KERNEL, 0);
		if (pg)
			p = page_address(pg);
		break;
	case check_page_o:
		pg = alloc_pages(GFP_KERNEL | __GFP_COMP, 3);
		if (pg)
			p = page_address(pg);
		break;
	case check_task:
		p = current;
		break;
	case check_stack:
		p = (void *)sp;
		break;
	case check_moduleaddr:
		p = (void *)_THIS_IP_;
		break;
	default:
		break;
	}
	return p;
}
/**
 */
static void mclass_free_function(void *p, enum _mclass_type type)
{
	struct page *pg;

	switch (type) {
	case check_kmalloc:
		kfree(p);
		break;
	case check_vmalloc:
		vfree(p);
		break;
	case check_page:
	case check_page_o:
		if (p) {
			pg = virt_to_page(p);
			__free_pages(pg, compound_order(pg));
		}
		break;
	case check_task:
	case check_stack:
	case check_moduleaddr:
	default:
		break;
	}
}
/**
 */
static int class_cmd(char *p)
{
	unsigned long addr = 0;
	unsigned int i, val = 0;
	char sym[KSYM_SYMBOL_LEN];

	addr = scan_addr_and_param(p, &val);
	if (addr == 0) {
		pr_err("Check memory-classifiers:\n");
		for (i = 0; i < ARRAY_SIZE(mclass_check); i++) {
			void *p;

			p = mclass_alloc_function(mclass_check[i].type);
			pr_err("[%u]%-16s: %p -> %s\n", i,
			       mclass_check[i].mclass_name, p,
			       print_memory_classifier(sym, sizeof(sym), (unsigned long)p, 0));
			mclass_free_function(p, mclass_check[i].type);
		}
		return 0;
	}
	if (val == 0)
		val = 1;
	for (i = 0; i < val; i++) {
		print_memory_classifier(sym, sizeof(sym), addr, 0);
		if (sym[0] || (i == 0) || (i == (val - 1))) {
			pr_err("%08lx --> %s\n", addr, sym);
		}
		addr += PAGE_SIZE;
	}
	return 0;
}
#endif

static int wdr_cmd(char *p)
{
	pr_err("\nSimulate Watchdog-Reboot with '%s'\n", SIMULATE_WD);
	simulate_wdr();

	return 0;
}

static int waddr_cmd(char *p)
{
	unsigned long addr;
	unsigned int val = 0;

	addr = scan_addr_and_param(p, &val);
	if (addr < PAGE_SIZE) {
		return 0;
	}
	pr_err("waddr: *(%lx) = %x\n", addr, val);
	*((unsigned int *)addr) = val;
	wmb();

	return 0;
}

static int raddr_cmd(char *p)
{
	unsigned long addr;
	unsigned int i, s = 3, count = 0, val;

	addr = scan_addr_and_param(p, &count);
	if (addr == 0) {
		return 0;
	}
	if (count == 0)
		count = 1;
	pr_err("raddr: addr=%lx count=%x\n", addr, count);
	for (i = 0; i < count; i++) {
		val = *((unsigned int *)addr);
		mb();
		if (++s == 4) {
			s = 0;
			pr_cont("\n%08lx: %08x", addr, val);
		} else {
			pr_cont(" %08x", val);
		}
		addr += 4;
	}
	pr_cont("\n");

	return 0;
}

static int oomslab_cmd(char *p)
{
	unsigned int order = 0, limit = 0;

	sscanf(p, "%u %u", &order, &limit);
	if (order == 0) {
		order = 14;
	}
	pr_err("\nSimulate OOM per kmalloc order=%u limit=%u\n", order, limit);
	simulate_oom(1, order, limit);

	return 0;
}

static int oom_cmd(char *p)
{
	unsigned int order = 0, limit = 0;

	sscanf(p, "%u %u", &order, &limit);
	if (order == 0) {
		order = 14;
	}
	pr_err("\nSimulate OOM per vmalloc order=%u limit=%u\n", order, limit);
	simulate_oom(0, order, limit);

	return 0;
}

static int bug_on_cmd(char *p)
{
	int is_true = false;

	BUG_ON(is_true == false);

	return 0;
}

static int event_ovr_cmd(char *p)
{
	sscanf(p, "%u %u", &event_trigger_msec, &event_trigger_count);
	if (event_trigger_count == 0) {
		event_trigger_count = 100;
	}
	if (event_trigger_msec > 1000) {
		event_trigger_msec = 1000;
	}
	if (event_kthread) {
		kthread_stop(event_kthread);
	}
	event_kthread =
		kthread_run(simulate_event_ovr_thread, NULL, "event_ovr");

	return 0;
}

/**
 */
struct _generic_stat {
	unsigned long long cnt;
	unsigned long long avg;
	unsigned long max;
};

/**
 * SLUB-based
 */
static int size_tab[] = {64, 128, 192, 256, 512, 1024, 2048, 4096, 8192};
#define MAX_ALLOC_ORDER  ARRAY_SIZE(size_tab)

/**
 * packet in alloced memory
 */
struct _allocstress_data {
	unsigned long size;
	unsigned long order_idx;
	unsigned long expire;
	struct _allocstress_data *next;
};

struct _alloc_stress {
	/* internal: */
	struct task_struct *alloc_task;
	unsigned int alloc_act_size;
	unsigned int force_stop;
	unsigned int num_threads;
	struct _allocstress_data *alloc_anchor;
	struct _allocstress_data *last_alloc_anchor;
	spinlock_t lock;
	/* configurable: */
#define DEFAULT_ALLOC_LIMIT			(8 << 20)
	unsigned int alloc_limit;	/* maximal limit for alloc for one cpu */
#define DEFAULT_ALLOC_AGE			(HZ / 10)
	unsigned int alloc_age;		/* age until free in jiffies */
#define DEFAULT_ORDER_MASK			(0x1FF)
	unsigned int order_mask;	/* ordermask: see size_tab[] */
	unsigned int one_order_idx;	/* only one order set */
	unsigned int gfp_flag;		/* GFP_ATOMIC or GFP_KERNEL */

	/*--- statistic ---*/
	struct _generic_stat alloc_stat[MAX_ALLOC_ORDER];
	struct _generic_stat free_stat[MAX_ALLOC_ORDER];
	unsigned long long alloc_sum_size;
	unsigned long start_jiffies;
	unsigned long stop_jiffies;
};

struct _alloc_stress_configuration {
	unsigned int gfp_flag;
	unsigned int alloc_limit;
	unsigned int alloc_age;
	unsigned int order_mask;
};

static DEFINE_PER_CPU(struct _alloc_stress, alloc_stress);

/**
 * collect statistic
 */
static void generic_stat(struct _generic_stat *pgstat, unsigned long val)
{
	if (pgstat->cnt == 0) {
		pgstat->max = 0;
		pgstat->avg = 0;
	}
	pgstat->cnt++;
	if (val > pgstat->max)
		pgstat->max = val;
	pgstat->avg += val;
}
/**
 */
static void reset_generic_stat(struct _generic_stat *pgstat)
{
	pgstat->cnt = 0;
}
/**
 * combine (add/max) statistic from psrc to pdest
 */
static void add_generic_stat(struct _generic_stat *pdest,
			      struct _generic_stat *psrc,
			      unsigned int reset)
{
	if (psrc->cnt == 0) {
		return;
	}
	pdest->cnt += psrc->cnt;
	pdest->avg += psrc->avg;
	if (psrc->max > pdest->max) {
		pdest->max = psrc->max;
	}
	if (reset) {
		psrc->cnt = 0;
	}
}

/**
 * print generic statistic
 */
static void print_generic(struct seq_file *seq, const char *prefix, int i, struct _generic_stat *pgstat)
{
	unsigned long long us_erg;
	unsigned long ns_erg;
	char prebuf[32];
	unsigned long usec_norm = avm_get_cyclefreq() / (1000 * 1000);
	unsigned long long cnt;

	if ((i >= 0) && (i < ARRAY_SIZE(size_tab))) {
		snprintf(prebuf, sizeof(prebuf), "%s-%u", prefix, size_tab[i]);
	} else {
		snprintf(prebuf, sizeof(prebuf), "%s", prefix);
	}
	cnt = pgstat->cnt;
	if (cnt == 0)
		return;

	us_erg = pgstat->avg;
	while (cnt > UINT_MAX) {
		cnt    >>= 1;
		us_erg >>= 1;
	}
	do_div(us_erg, cnt);
	ns_erg = do_div(us_erg, usec_norm);
	ns_erg = (ns_erg * 1000) / usec_norm;

	print_out(seq, "%-13s: cnt =%10llu avg =%5llu.%03lu max =%7lu us\n",
		  prebuf,
		  pgstat->cnt, us_erg, ns_erg, pgstat->max / usec_norm);
}

/**
 */
static unsigned short pseudo_random(void)
{
	static unsigned short pn_16 = 0xead1;
	unsigned short val = avm_get_cycles();

	pn_16 = (pn_16 >> 1) ^ ((-(pn_16 & 1)) & 0xb400);

	val ^= pn_16;
	return val;
}
#ifdef CONFIG_SLUB_STATS
#include <linux/slub_def.h>
static unsigned long old_slubstat[MAX_ALLOC_ORDER][NR_SLUB_STAT_ITEMS];

static struct kmem_cache *get_kcache(unsigned int order)
{
	unsigned int i;
	unsigned int size = size_tab[order];

	for (i = 0; i < ARRAY_SIZE(kmalloc_caches); i++)
		if (kmalloc_caches[i] && (kmalloc_caches[i]->object_size == size))
			return kmalloc_caches[i];
	return NULL;
}
static void slub_stat(struct seq_file *seq, int order)
{
	int i, cpu;
	unsigned long alloc_sum, free_sum;
	unsigned long new_slubstat[NR_SLUB_STAT_ITEMS];
	struct kmem_cache *s;

	s = get_kcache(order);
	if (s == NULL)
		return;

	for (i = 0; i < NR_SLUB_STAT_ITEMS; i++) {
		unsigned long sum  = 0;

		for_each_online_cpu(cpu) {
			unsigned int x = per_cpu_ptr(s->cpu_slab, cpu)->stat[i];

			sum += x;
		}
		new_slubstat[i] = sum - old_slubstat[order][i];
		old_slubstat[order][i] = new_slubstat[i];
	}
	alloc_sum = new_slubstat[ALLOC_FASTPATH] + new_slubstat[ALLOC_SLOWPATH];
	free_sum = new_slubstat[FREE_FASTPATH] + new_slubstat[FREE_SLOWPATH];
	if (alloc_sum && free_sum) {
		print_out(seq, "%-13s: alloc(%lu)/free(%lu) fast=%3lu%%/%3lu%% from page=%3lu%%/%3lu%% alloc-refill=%3lu%%\n",
			  s->name,
			  alloc_sum, free_sum,
			  (new_slubstat[ALLOC_FASTPATH] * 100) / alloc_sum,
			  (new_slubstat[FREE_FASTPATH] * 100) / free_sum,
			  (new_slubstat[ALLOC_SLAB] * 100) / alloc_sum,
			  (new_slubstat[FREE_SLAB] * 100) / free_sum,
			  (new_slubstat[ALLOC_REFILL] * 100) / alloc_sum);
	}
}
#endif
/**
 * get malloc-order randomized depend on order-mask
 */
static unsigned int get_random_order(struct _alloc_stress *pas)
{
	unsigned int order_idx;

	/*--- only one order chooseable: ---*/
	if (pas->one_order_idx < MAX_ALLOC_ORDER)
		return pas->one_order_idx;

	do {
		order_idx = pseudo_random() % MAX_ALLOC_ORDER;
	} while (!(pas->order_mask & (1 << order_idx)));

	return order_idx;
}
/**
 */
static char *print_size_order(char *txt, int txtlen, unsigned int order_mask)
{
	char *ptxt = txt;
	unsigned int i;

	txt[0] = 0;
	for (i = 0; i < ARRAY_SIZE(size_tab); i++) {
		int local_add_len;

		if ((order_mask & (1 << i)) == 0) {
			continue;
		}
		local_add_len = snprintf(ptxt, txtlen, "kmalloc-%u ", size_tab[i]);
		if (local_add_len > 0) {
			int tail = min_t(int, txtlen, local_add_len);
			(ptxt) += tail, (txtlen) -= tail;
		}
	}
	if (ptxt != txt)
		ptxt[-1] = 0;
	return txt;
}
/**
 */
static char *human_bytes(char *buf, int buflen, unsigned long long bytes)
{
	static const char *const tab[] = { "B", "kiB", "MiB", "GiB", "TiB" };
	unsigned int i = 0, rest = 0;

	while (bytes > (size_t)1024 && (i < ARRAY_SIZE(tab) - 1)) {
		rest = do_div(bytes, 1024);
		i++;
	}
	if (rest) {
		snprintf(buf, buflen, "%u.%03u %s", (unsigned int)bytes, (rest * 1000) / 1024, tab[i]);
	} else {
		snprintf(buf, buflen, "%u %s", (unsigned int)bytes, tab[i]);
	}
	return buf;
}
/**
 * summarize statistic
 */
static void print_alloc_statistic(struct seq_file *seq, unsigned int reset)
{
	char txtbuf[2][256];
	struct _generic_stat free_stat, alloc_stat, sum_free_stat, sum_alloc_stat;
	unsigned long tdiff;
	unsigned long long alloc_sum_size = 0;
	unsigned long start_jiffies = jiffies;
	unsigned long stop_jiffies;
	int cpu, first = 1;
	unsigned int i, order_flag = 0;

	memset(&sum_alloc_stat, 0, sizeof(sum_alloc_stat));
	memset(&sum_free_stat, 0, sizeof(sum_free_stat));

	for_each_online_cpu(cpu) {
		struct _alloc_stress *pas = &per_cpu(alloc_stress, cpu);

		if (cpu >= pas->num_threads)
			break;
		/*--- time_after(a,b) returns true if the time a is after time b. ---*/
		if (time_after(start_jiffies, pas->start_jiffies)) {
			start_jiffies = pas->start_jiffies;
		}
		spin_lock(&pas->lock);
		alloc_sum_size += pas->alloc_sum_size;
		if (reset) {
			pas->start_jiffies  = jiffies;
			pas->stop_jiffies   = jiffies;
			pas->alloc_sum_size = 0;
		}
		spin_unlock(&pas->lock);
	}
	stop_jiffies = start_jiffies;
	for_each_online_cpu(cpu) {
		struct _alloc_stress *pas = &per_cpu(alloc_stress, cpu);

		if (cpu >= pas->num_threads)
			break;
		if (time_after(pas->stop_jiffies, stop_jiffies)) {
			stop_jiffies = pas->stop_jiffies;
		}
	}
	tdiff = (stop_jiffies - start_jiffies) | 0x1;

	for (i = 0; i < MAX_ALLOC_ORDER; i++) {
		memset(&alloc_stat, 0, sizeof(alloc_stat));
		memset(&free_stat, 0, sizeof(free_stat));
		for_each_online_cpu(cpu) {
			struct _alloc_stress *pas = &per_cpu(alloc_stress, cpu);

			if (cpu >= pas->num_threads)
				break;
			spin_lock(&pas->lock);
			if (pas->alloc_stat[i].cnt)
				order_flag |= (1 << i);

			add_generic_stat(&alloc_stat, &pas->alloc_stat[i], reset);
			add_generic_stat(&free_stat, &pas->free_stat[i], reset);
			spin_unlock(&pas->lock);
			if (first && alloc_sum_size) {

				print_out(seq, "-----------------------------------------------------------------\n");
				print_out(seq, "measure-time: %lu.%lu s limit= %u * %u KByte age=%u (%u ms) %s %s\n",
						tdiff / HZ,  ((tdiff % HZ) * 10) / HZ,
						pas->num_threads,
						pas->alloc_limit / 1024,
						pas->alloc_age, (pas->alloc_age * 1000) / HZ,
						((pas->gfp_flag & GFP_KERNEL) == GFP_KERNEL) ? "GFP_KERNEL" : "GFP_ATOMIC",
						print_size_order(txtbuf[0], sizeof(txtbuf[0]), pas->order_mask));
				first = 0;
			}
		}
		print_generic(seq, "kmalloc", i, &alloc_stat);
		print_generic(seq, "kfree", i, &free_stat);
		add_generic_stat(&sum_alloc_stat, &alloc_stat, 0);
		add_generic_stat(&sum_free_stat, &free_stat, 0);
	}

	if (alloc_sum_size && (hweight32(order_flag) > 1)) {
		print_out(seq, "-----------------------------------------------------------------\n");
		print_generic(seq, "kmalloc-sum", -1, &sum_alloc_stat);
		print_generic(seq, "kfree-sum", -1, &sum_free_stat);
	}
#ifdef CONFIG_SLUB_STATS
	for (i = 0; i < MAX_ALLOC_ORDER; i++) {
		if (order_flag & (1 << i))
			slub_stat(seq, i);
	}
#endif
	if (alloc_sum_size) {
		unsigned long long allocs_per_sec = sum_alloc_stat.cnt * HZ;
		unsigned long long bytes_per_sec = alloc_sum_size * HZ;

		do_div(allocs_per_sec, tdiff);
		do_div(bytes_per_sec, tdiff);

		print_out(seq, "alloc-sum-size: %s (%s/s - %lu/s - brutto)\n",
			  human_bytes(txtbuf[0], sizeof(txtbuf[0]), alloc_sum_size),
			  human_bytes(txtbuf[1], sizeof(txtbuf[1]), bytes_per_sec),
			  (unsigned long)allocs_per_sec);
	}
}
/**
 */
static int alloc_entry(struct _alloc_stress *pas)
{
	unsigned int order_idx;
	unsigned long ta, te;
	struct _allocstress_data *pdata;
	unsigned char *buf;
	size_t size;

	order_idx = get_random_order(pas);
	size	  = size_tab[order_idx];

	ta = avm_get_cycles();
	buf = kmalloc(size, pas->gfp_flag);
	te = avm_get_cycles();

	if (buf == NULL) {
		return -ENOMEM;
	}
	pdata = (struct _allocstress_data *)buf;

	memset(buf, (smp_processor_id() << 4) | order_idx, size);
	pdata->expire = jiffies + pas->alloc_age;
	pdata->size = size;
	pdata->order_idx = order_idx;
	pdata->next	 = NULL;
	pas->alloc_sum_size += size;

	spin_lock(&pas->lock);
	if (pas->last_alloc_anchor) {
		pas->last_alloc_anchor->next = pdata;
	} else {
		pas->alloc_anchor = pdata;
	}
	pas->last_alloc_anchor = pdata;
	pas->alloc_act_size += size;
	generic_stat(&pas->alloc_stat[order_idx], te - ta);
	spin_unlock(&pas->lock);
	return 0;
}
/**
 */
static void alloc_buffers(struct _alloc_stress *pas)
{
	int ret;

	while ((pas->force_stop == 0) && pas->alloc_act_size < pas->alloc_limit) {
		ret = alloc_entry(pas);
		if (ret) {
			if (ret == -ENOMEM) {
				unsigned long tsjiffies = jiffies;

				pr_err("%s: error on alloc\n", __func__);
				msleep(1000);
				pas->start_jiffies += (jiffies - tsjiffies);
			}
			break;
		}
	}
}
/**
 */
static int free_buffers(struct _alloc_stress *pas, int force)
{
	unsigned long ta, te;
	unsigned int order_idx;
	struct _allocstress_data *pdata;

	for (;;) {
		spin_lock(&pas->lock);

		if ((force == 0) && pas->alloc_act_size < pas->alloc_limit / 2) {
			spin_unlock(&pas->lock);
			break;
		}
		if (pas->alloc_anchor == NULL) {
			spin_unlock(&pas->lock);
			break;
		}
		pdata = pas->alloc_anchor;

		if ((force == 0) && !time_after(jiffies, pdata->expire)) {
			spin_unlock(&pas->lock);
			break;
		}

		pas->alloc_anchor = pdata->next;
		if (pdata->next == NULL) {
			pas->last_alloc_anchor = NULL;
		}
		pas->alloc_act_size -= pdata->size;
		spin_unlock(&pas->lock);

		order_idx = pdata->order_idx;

		ta = avm_get_cycles();
		kfree(pdata);
		te = avm_get_cycles();

		spin_lock(&pas->lock);
		generic_stat(&pas->free_stat[order_idx], te - ta);
		spin_unlock(&pas->lock);
	}
	return 0;
}

/**
 */
static void reset_statistic(struct _alloc_stress *pas)
{
	unsigned int i;

	spin_lock(&pas->lock);
	for (i = 0; i < MAX_ALLOC_ORDER; i++) {
		reset_generic_stat(&pas->alloc_stat[i]);
		reset_generic_stat(&pas->free_stat[i]);
	}
	pas->stop_jiffies   = jiffies;
	pas->start_jiffies  = jiffies;
	pas->alloc_act_size = 0;
	spin_unlock(&pas->lock);
}
/**
 */
static int alloc_stress_thread(void *data)
{
	int cpu;
	unsigned long expire = jiffies  + 10 * HZ;
	struct _alloc_stress *pat, *pas = data;

	reset_statistic(pas);

	while (!kthread_should_stop()) {

		pas->stop_jiffies   = jiffies;
		alloc_buffers(pas);
		for_each_online_cpu(cpu) {
			if ((cpu == smp_processor_id()) && (pas->num_threads > 1)) {
				continue;
			}
			pat = &per_cpu(alloc_stress, cpu);
			free_buffers(pat, 0);
			/*--- usleep_range(1000 + (10 * cpu), 1500 - (10 * cpu)); ---*/
			msleep(1);
		}
		if ((smp_processor_id() == 0) && time_after(jiffies, expire)) {
			expire = jiffies + 20 * HZ;
			pas->stop_jiffies   = jiffies;
			print_alloc_statistic(NULL, 0);
		}
	}
	pr_debug("%s: done\n", __func__);
	return 0;
}
/**
 */
static void alloc_stress_stop(int cpu)
{
	struct _alloc_stress *pas;

	pas = &per_cpu(alloc_stress, cpu);

	if (pas->alloc_task) {
		pas->force_stop = 1;
		kthread_stop(pas->alloc_task);
		pas->alloc_task = NULL;
	}
	free_buffers(pas, 1);
	pas->stop_jiffies   = jiffies;

	if (pas->alloc_act_size)
		pr_err("%s: error: alloc_act_size)%u\n", __func__, pas->alloc_act_size);
}

/**
 */
static void alloc_stress_start(struct _alloc_stress_configuration *pconf, int num_threads, int cpu)
{
	char txt[128];
	struct _alloc_stress *pas;

	pas = &per_cpu(alloc_stress, cpu);

	pas->force_stop	    = 0;
	pas->alloc_sum_size = 0;
	pas->num_threads    = num_threads;

	if (cpu >= num_threads) {
		/* limited threads: do not start on this cpu */
		return;
	}
	if (pconf) {
		pas->alloc_limit = pconf->alloc_limit * 1024;
		pas->order_mask  = pconf->order_mask;
		pas->alloc_age   = pconf->alloc_age;
		pas->gfp_flag    = pconf->gfp_flag;
	}
	pas->num_threads = num_threads;
	if (pas->gfp_flag == 0)
		pas->gfp_flag = GFP_ATOMIC | __GFP_NORETRY;
	if (pas->alloc_limit == 0)
		pas->alloc_limit = DEFAULT_ALLOC_LIMIT;
	if (pas->alloc_age == 0)
		pas->alloc_age = DEFAULT_ALLOC_AGE;
	if (pas->order_mask == 0)
		pas->order_mask = DEFAULT_ORDER_MASK;

	if (hweight32(pas->order_mask) == 1) {
		pas->one_order_idx = fls(pas->order_mask) - 1;
	} else {
		pas->one_order_idx = MAX_ALLOC_ORDER;
	}
	pr_err("%s:cpu%u limit=%u KByte age=%u %s order-mask=0x%x(%s)\n", __func__,
	       cpu,
	       pas->alloc_limit / 1024, pas->alloc_age,
	       ((pas->gfp_flag & GFP_KERNEL) == GFP_KERNEL) ? "GFP_KERNEL" : "GFP_ATOMIC",
	       pas->order_mask,
	       print_size_order(txt, sizeof(txt), pas->order_mask));

	pas->alloc_task = kthread_create(alloc_stress_thread, pas, "allocstress_%d", cpu);

	if (!IS_ERR(pas->alloc_task)) {
		kthread_bind(pas->alloc_task, cpu);
		wake_up_process(pas->alloc_task);
	}
}
/**
 */
static unsigned int parse_obj(char *p, char *name, int hex)
{
	unsigned int value = 0;

	p = strstr(p, name);
	if (p == NULL) {
		return 0;
	}
	p += strlen(name);
	SKIP_SPACES(p);
	if (hex)  {
		sscanf(p, "%x", &value);
	} else {
		sscanf(p, "%u", &value);
	}
	return value;
}

/**
 */
static void lproc_allocstress_read(struct seq_file *seq, void *ctx __maybe_unused)
{
	print_alloc_statistic(seq, 0);
}

/**
 */
static int alloc_stress_cmd(char *p)
{
	static int init_alloc_proc;
	int cpu;
	unsigned int num_threads = 0;
	struct _alloc_stress_configuration config = {0};

	if (init_alloc_proc == 0) {
		init_alloc_proc = !add_simple_proc_file("avm/allocstress", NULL,
							lproc_allocstress_read, NULL);
	}
	for_each_online_cpu(cpu)
		alloc_stress_stop(cpu);

	if (strstr(p, "stop")) {
		print_alloc_statistic(NULL, 0);
		return 0;
	}
	config.alloc_limit = parse_obj(p, "limit=", 0);
	config.order_mask  = parse_obj(p, "order=", 1);
	config.alloc_age   = parse_obj(p, "age=", 0);
	num_threads = parse_obj(p, "num=", 0);

	if (strstr(p, "GFP_KERNEL")) {
		config.gfp_flag = GFP_KERNEL;
	}
	if ((num_threads == 0) || (num_threads > num_possible_cpus())) {
		num_threads = num_possible_cpus();
	}
	for_each_online_cpu(cpu)
		alloc_stress_start(&config, num_threads, cpu);
	return 0;
}

static int faila_cmd(char *p __maybe_unused)
{
	void *s;

	pr_err("\nforce double-free:\n");
	s = kmalloc(192, GFP_KERNEL);
	pr_err("\ncall kfree(%pS)\n", s);
	kfree(s);
	pr_err("\ncall kfree(%pS)\n", s);
	kfree(s);
	pr_err("\nforce invalid free-pointer:\n");
	s = kmalloc(512, GFP_KERNEL);
	pr_err("\ncall kfree(%pS) - pointer with offset 0x20\n", s);
	kfree(s + 20);
	pr_err("\ncall kfree(%pS)\n", s);
	kfree(s);
	return 0;
}
/**
 */
static int kcrash_cmd(char *p __maybe_unused)
{
	pr_err("\nSimulate Kernel-Crash\n");
	simulate_kernel_crash();

	return 0;
}

static int hw_wdog_cmd(char *p __maybe_unused)
{
#if defined(CONFIG_SMP)
	on_each_cpu(knockout_cpu, NULL, 0);
#endif /*--- #if defined(CONFIG_SMP) ---*/
	local_irq_disable();
	for (;;)
		;

	return 0;
}

static DEFINE_PER_CPU(struct timer_list, trigger_timer);

static void waste_timer_func(unsigned long percent)
{
	int cpu;
	unsigned long ta, te, tdiff;
	struct timer_list *pt;

	ta = avm_get_cycles();
	tdiff = ((avm_get_cyclefreq() / 10 / 100) * percent);

	cpu = get_cpu();
	pt = &per_cpu(trigger_timer, cpu);
	mod_timer(pt, pt->expires + msecs_to_jiffies(100));
	put_cpu();

	for (;;) {
		te = avm_get_cycles();
		if ((te - ta) > tdiff) {
			break;
		}
		udelay(10);
	}
	{
		static int count[4];

		if (count[cpu]++ > 50) {
			pr_err("%s: %u: percent=%lu te=%lu ta=%lu tdiff=%lu\n",
			       __func__, __LINE__, percent, te, ta, tdiff);
			count[cpu] = 0;
		}
	}
}

static int timer_cmd(char *p)
{
	int cpu;
	cpumask_t cpu_mask;
	unsigned int percent;

	percent = 50;
	cpu = (int)num_possible_cpus();
	sscanf(p, "%u %d",  &percent, &cpu);

	if (cpu >= (int)num_possible_cpus()) {
		cpumask_setall(&cpu_mask);
	} else {
		cpumask_clear(&cpu_mask);
		cpumask_set_cpu(cpu, &cpu_mask);
	}
	if (percent > 100) {
		percent = 100;
	}
	pr_err("\nSimulate waste %u %% on timer on %scpu%c\n",
	       percent,
	       cpu >= (int)num_possible_cpus() ? "all " : "",
	       cpu >= (int)num_possible_cpus() ? 's' : cpu + '0');
	for_each_online_cpu(cpu) {
		struct timer_list *pt = &per_cpu(trigger_timer, cpu);

		if (!cpumask_test_cpu(cpu, &cpu_mask)) {
			continue;
		}
		if (pt->function) {
			del_timer_sync(pt);
			pt->function = NULL;
		}
		if (percent == 0) {
			continue;
		}
		setup_timer(pt, waste_timer_func, percent);
		pt->expires = jiffies + msecs_to_jiffies(100);
		add_timer_on(pt, cpu);
	}
	return 0;
}
/*
 * NOTICE: currently command names are only matched as prefix, therefore
 * longer more specific commands must be listed first.
 */
// clang-format off
static struct cmd cmds[] = {
#if defined(print_memory_classifier)
	{.name = "class", .func = class_cmd, .help = "<addr> <page-count> - Classify pages, addr=0: quick-check"},
#endif
	{.name = "wdr", .func = wdr_cmd},
	{.name = "waddr", .func = waddr_cmd, .help = "<addr> <val> - Write to address"},
	{.name = "raddr", .func = raddr_cmd, .help = "<addr> <count> - Read from address"},
	{.name = "oomslab", .func = oomslab_cmd, .help = "- OOM via slab allocation"},
	{.name = "oom", .func = oom_cmd, .help = "- OOM via vmalloc"},
	{.name = "bug_on", .func = bug_on_cmd},
	{.name = "event_ovr", .func = event_ovr_cmd, .help = "<trigger_msec> <count>"},
	{.name = "kcrash", .func = kcrash_cmd},
	{.name = "hw_wdog", .func = hw_wdog_cmd, .help = "- Lock up all cpus"},
	{.name = "timer", .func = timer_cmd, .help = "- waste <percent> time in sw-irq on <cpu>"},
	{.name = "alloc", .func = alloc_stress_cmd, .help = "- alloc limit=<kbyte> order=0x<order-mask> age=<age>(jiffies) num=<threads> [stop][GFP_KERNEL]"},
	{.name = "faila", .func = faila_cmd, .help = "check double-free, invalid kfree-pointer"},
	{}
};
// clang-format on

static int lproc_simulate_write(char *buffer, void *priv)
{
	int i;
	char parsbuf[256], *p;

	strncpy(parsbuf, buffer, sizeof(parsbuf) - 1);
	parsbuf[sizeof(parsbuf) - 1] = '\0';

	p = parsbuf;
	p = skip_spaces(p);

	for (i = 0; cmds[i].name != NULL; i++) {
		size_t len = strlen(cmds[i].name);

		if (strncmp(p, cmds[i].name, len) == 0) {
			p += len;
			p = skip_spaces(p);

			return cmds[i].func(p);
		}
	}

	pr_err("Unkown simulate command %s\n", p);
	return 0;
}

static void lproc_simulate_read(struct seq_file *f, void *ctx)
{
	int i;

	for (i = 0; cmds[i].name != NULL; i++) {
		seq_printf(f, "%s %s\n", cmds[i].name, cmds[i].help ?: "");
	}
}

static int __init avm_proc_call_init(void)
{
	int cpu;

	add_simple_proc_file("avm/call", lproc_call, NULL, NULL);
	add_simple_proc_file("avm/simulate", lproc_simulate_write,
			     lproc_simulate_read, NULL);

	for_each_online_cpu(cpu) {
		struct _alloc_stress *pas = &per_cpu(alloc_stress, cpu);

		spin_lock_init(&pas->lock);
	}
	return 0;
}
module_init(avm_proc_call_init);

MODULE_LICENSE("GPL");
