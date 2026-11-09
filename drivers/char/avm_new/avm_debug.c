// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2007-2019 AVM GmbH <fritzbox_info@avm.de> */

#if defined(CONFIG_AVM_FASTIRQ) && !defined(CONFIG_ARM_AVALANCHE_SOC)
#if defined(CONFIG_AVM_GENERIC_RTE)
#include <avm/rte/avm_fiq_groups.h>
#elif defined(CONFIG_AVM_FASTIRQ_ARCH_ARM_COMMON)
#include <asm/avm_enh/avm_fiq_groups.h>
#else
#include <mach/avm_fiq_groups.h>
#endif
#define CLIENT_FIQ_PRIO FIQ_PRIO_MONITOR
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/socket.h>
#include <linux/jiffies.h>
#include <linux/un.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/net.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/sysrq.h>

#include <avm/compat/misc.h>
#include <avm/sammel/debug.h>

#if defined(CONFIG_SMP) && defined(CONFIG_MIPS)
#include <asm/yield_context.h>
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_SMP) && defined(CONFIG_MIPS) ---*/

#include <linux/kthread.h>
#include <linux/kmsg_dump.h>

#include <linux/cdev.h>
#include <asm/mach_avm.h>

#include "avm_sammel.h"
#include "avm_debug.h"

#include <linux/fs.h>

#define DBG_ERR(args...) pr_err(args)
/*--- #define DBG_INFO(args...)     pr_err(args) ---*/
#define DBG_INFO(args...) no_printk(args)
/*--- #define DBG_NOTE(args...)     pr_err(args) ---*/
#define DBG_NOTE(args...) no_printk(args)

#define MAX_PRE_DMESGBUFFER (1 << 15)

#define AVM_DBG_SIGNAL   "AVMDBG_SIGNAL"

/**
 */
struct _debug_client {
	void *refdata;
	char *prefix;
	void (*CallBackDebug)(char *string, void *refdata);
	struct _debug_client *next;
};

/**
 */
static struct _avm_debug {
	unsigned int init;
	spinlock_t client_lock;
	spinlock_t signallock;
	atomic_t open_flag;
	unsigned int major;
	dev_t        device;
	struct cdev *cdev;
	struct class *osclass;
	struct _debug_client *dbg_clientAnker;
	struct task_struct *kthread;
	unsigned int signal;
	wait_queue_head_t wait_queue;
	struct socket *s_push;
} avm_debug;

static int avm_debug_open(struct inode *inode, struct file *filp);
static int avm_debug_close(struct inode *inode, struct file *filp);
static long avm_debug_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long args);
static ssize_t avm_debug_write(struct file *filp, const char *write_buffer,
			       size_t write_length, loff_t *write_pos);
static struct _debug_client *find_dbgclient_by_prefix(char *prefix);

static int avmdebug_thread(void *data);

/**
 */
avm_debug_write_t avm_debug_write_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_read_t avm_debug_read_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_open_t avm_debug_open_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_close_t avm_debug_close_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_ioctl_t avm_debug_ioctl_minor[AVM_DEBUG_MAX_MINOR + 1];

/**
 */
static ssize_t avm_debug_write_dummy(struct file *filp,
				     const char *buff,
				     size_t count,
				     loff_t *off)
{
	return -ENOENT;
}

static ssize_t avm_debug_read_dummy(struct file *filp,
				    char *buff,
				    size_t count,
				    loff_t *off)
{
	return -ENOENT;
}

static long avm_debug_ioctl_dummy(struct file *filp,
				  unsigned cmd,
				  unsigned long arg)
{
	return -ENOENT;
}

/**
 */
static const struct file_operations avm_debug_fops = {
	.owner =   THIS_MODULE,
	.open =    avm_debug_open,
	.release = avm_debug_close,
	.write =   avm_debug_write,
	.unlocked_ioctl = avm_debug_ioctl,
};

/**
 */
static inline int avmdebug_lock(spinlock_t *lock, unsigned long *flags,
				unsigned int force)
{
#if defined(CONFIG_SMP)
	int try = 0;

	if (force) {
		rte_spin_lock_irqsave(lock, *flags);
		return 1;
	}
	/*--- if interrupted with a fatal error on this vpe (and the other vpe is already disabled) ---*/
	while (!rte_spin_trylock_irqsave(lock, *flags)) {
		if (try++ > 3) {
			return 0;
		}
		mdelay(1);
	}
#else /*--- #if defined(CONFIG_SMP)  ---*/
	spin_lock_irqsave(lock, *flags);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_SMP)  ---*/
	return 1;
}

/**
 */
static inline void avmdebug_unlock(spinlock_t *lock, unsigned long flags)
{
	rte_spin_unlock_irqrestore(lock, flags);
}

/**
 */
int avm_debug_register_minor(int minor, avm_debug_open_t open,
			     avm_debug_close_t close, avm_debug_write_t write,
			     avm_debug_read_t read, avm_debug_ioctl_t ioctl)
{
	if ((minor < 1) || (minor > AVM_DEBUG_MAX_MINOR)) {
		return -ENXIO;
	}

	if ((avm_debug_write_minor[minor] != avm_debug_write_dummy) ||
	    (avm_debug_read_minor[minor] != avm_debug_read_dummy) ||
	    (avm_debug_ioctl_minor[minor] != avm_debug_ioctl_dummy) ||
	    (avm_debug_open_minor[minor] != NULL) ||
	    (avm_debug_close_minor[minor] != NULL)) {
		return -EEXIST;
	}
	avm_debug_write_minor[minor] = write ? write : avm_debug_write_dummy;
	avm_debug_read_minor[minor]  = read ? read : avm_debug_read_dummy;
	avm_debug_ioctl_minor[minor] = ioctl ? ioctl : avm_debug_ioctl_dummy;
	avm_debug_open_minor[minor]  = open;
	avm_debug_close_minor[minor] = close;
	return 0;
}
EXPORT_SYMBOL(avm_debug_register_minor);

/**
 */
int avm_debug_release_minor(int minor)
{
	if ((minor < 1) || (minor > AVM_DEBUG_MAX_MINOR)) {
		return -ENXIO;
	}
	avm_debug_write_minor[minor] = avm_debug_write_dummy;
	avm_debug_read_minor[minor]  = avm_debug_read_dummy;
	avm_debug_ioctl_minor[minor] = avm_debug_ioctl_dummy;
	avm_debug_open_minor[minor]  = NULL;
	avm_debug_close_minor[minor] = NULL;
	return 0;
}
EXPORT_SYMBOL(avm_debug_release_minor);

/**
 */
int __init avm_debug_init(void)
{
	int reason, minor;

	for (minor = 1; minor <= AVM_DEBUG_MAX_MINOR; minor++) {
		avm_debug_write_minor[minor] = avm_debug_write_dummy;
		avm_debug_read_minor[minor]  = avm_debug_read_dummy;
		avm_debug_ioctl_minor[minor] = avm_debug_ioctl_dummy;
		avm_debug_open_minor[minor]  = NULL;
		avm_debug_close_minor[minor] = NULL;
	}

	memset((void *)&avm_debug, 0, sizeof(avm_debug));
	DBG_INFO("[avm_debug] register_chrdev_region()\n");
	reason = alloc_chrdev_region(&avm_debug.device, 0,
				     AVM_DEBUG_MINOR_COUNT, "debug");
	if (reason) {
		DBG_ERR("[avm_debug] register_chrdev_region failed: reason %d!\n",
			reason);
		return -ERESTARTSYS;
	}
	avm_debug.cdev = cdev_alloc();
	if (!avm_debug.cdev) {
		unregister_chrdev_region(avm_debug.device,
					 AVM_DEBUG_MINOR_COUNT);
		DBG_ERR("[avm_debug] cdev_alloc failed!\n");
		return -ERESTARTSYS;
	}
	spin_lock_init(&avm_debug.client_lock);
	spin_lock_init(&avm_debug.signallock);

	avm_debug.cdev->owner = avm_debug_fops.owner;
	avm_debug.cdev->ops = &avm_debug_fops;
	kobject_set_name(&(avm_debug.cdev->kobj), "debug");

	DBG_INFO("[avm_debug] major %d (success)\n", MAJOR(avm_debug.device));
	if (cdev_add(avm_debug.cdev, avm_debug.device, AVM_DEBUG_MINOR_COUNT)) {
		kobject_put(&avm_debug.cdev->kobj);
		unregister_chrdev_region(avm_debug.device,
					 AVM_DEBUG_MINOR_COUNT);
		DBG_ERR("[avm_debug] cdev_add failed!\n");
		return -ERESTARTSYS;
	}

	/*--- Geraetedatei anlegen: ---*/
	avm_debug.osclass = class_create(THIS_MODULE, "debug");
	device_create(avm_debug.osclass, NULL, avm_debug.device, NULL, "debug");
	init_waitqueue_head(&avm_debug.wait_queue);

	avm_debug.kthread =
		kthread_run(avmdebug_thread, &avm_debug, "avm_debugd");
	BUG_ON((avm_debug.kthread == NULL) || IS_ERR(avm_debug.kthread));

	atomic_set(&avm_debug.open_flag, 0);
	avm_debug.init = 1;
	return 0;
}

/**
 */
static inline int avm_debug_kthread_exit(struct task_struct **_kthread)
{
	struct task_struct *kthread = *_kthread;

	if (!kthread) {
		return -EINVAL;
	}
	return kthread_stop(kthread);
}

/**
 */
void avm_debug_cleanup(void)
{
	if (avm_debug.init == 0) {
		return;
	}
	avm_debug.init = 0;

	avm_debug_kthread_exit(&avm_debug.kthread);

	DBG_INFO("[avm_debug] unregister_chrdev(%u)\n",
		 MAJOR(avm_debug.device));
	device_destroy(avm_debug.osclass, avm_debug.device);
	class_destroy(avm_debug.osclass);
	cdev_del(avm_debug.cdev);
	unregister_chrdev_region(avm_debug.device, AVM_DEBUG_MINOR_COUNT);
}

/**
 */
static int avm_debug_open(struct inode *inode, struct file *filp)
{
	size_t minor = MINOR(inode->i_rdev);

	filp->private_data = (void *)minor;
	if ((minor < AVM_DEBUG_MAX_MINOR) && avm_debug_open_minor[minor]) {
		return (*avm_debug_open_minor[minor])(inode, filp);
	}
	if (filp->f_mode & FMODE_READ) {
		if (atomic_add_return(1, &avm_debug.open_flag) > 1) {
			return -EBUSY;
		}
	}
	DBG_INFO("[avm_debug]:%s\n", __func__);
	return 0;
}

/**
 */
static int avm_debug_close(struct inode *inode, struct file *filp)
{
	size_t minor = (size_t)filp->private_data;

	DBG_INFO("[avm_debug]: %s\n", __func__);
	if (avm_debug_close_minor[minor]) {
		return (*avm_debug_close_minor[minor])(inode, filp);
	}
	if (filp->f_mode & FMODE_READ) {
		atomic_set(&avm_debug.open_flag, 0);
	}
	return 0;
}

/**
 */
static long avm_debug_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long args)
{
	size_t minor = (size_t)filp->private_data;

	if (avm_debug_ioctl_minor[minor])
		return (*avm_debug_ioctl_minor[minor])(filp, cmd, args);

	return -ENXIO;
}

/**
 */
static inline unsigned int inc_idx(unsigned int idx, unsigned int max_idx)
{
	if (++idx >= max_idx) {
		return 0;
	}
	return idx;
}

/**
 */
/**
 */
static ssize_t avm_debug_write(struct file *filp, const char *write_buffer,
			       size_t write_length, loff_t *write_pos)
{
	char Buffer[512], *p, *pa = NULL;

	if (filp) {
		size_t minor = (size_t)filp->private_data;

		if (avm_debug_write_minor[minor])
			return (*avm_debug_write_minor[minor])(
				filp, write_buffer, write_length, write_pos);
	}
	if (write_pos != NULL) {
		DBG_INFO("[avm_debug]: write_length = %zu *write_pos = 0x%llx\n",
			 write_length, *write_pos);
	}

	if (write_length >= sizeof(Buffer)) {
		write_length = sizeof(Buffer) - 1;
		DBG_NOTE("[avm_debug] long line reduce to %zu bytes\n",
			 write_length);
	}
	if (filp == NULL) {
		memcpy(Buffer, write_buffer, write_length);
	} else {
		if (copy_from_user(Buffer, write_buffer, write_length)) {
			DBG_ERR("[avm_debug]: write: copy_from_user failed\n");
			return -EFAULT;
		}
	}
	Buffer[write_length] = '\0';
	DBG_NOTE("[avm_debug] len %zu = '%s'\n", write_length, Buffer);
	p = strchr(Buffer, 0x0A);
	if (p) {
		*p = '\0';
		pa = p + 1;
		write_length = strlen(Buffer) + 1;
		DBG_NOTE(
			"[avm_debug] multi line reduce to %zu bytes. remain %s\n",
			write_length, pa);
	}
	p = Buffer;
	/**
	 * cmd extrahieren
	 */
	p = skip_spaces(p);
	if (!strncmp(AVM_DBG_SIGNAL, p, sizeof(AVM_DBG_SIGNAL) - 1)) {
		int val = -1;

		p += sizeof(AVM_DBG_SIGNAL) - 1;
		p = skip_spaces(p);
		if (*p)
			sscanf(p, "%d", &val);
		if (val != -1) {
			if (pa)
				printk(KERN_ERR "avm_DebugSignal: %s\n", pa);
			avm_DebugSignal(val | 0x80000000);
		}
	} else {
		struct _debug_client *pdbg = find_dbgclient_by_prefix(p);

		if (pdbg) {
			pdbg->CallBackDebug(p + strlen(pdbg->prefix),
					    pdbg->refdata);
		} else {
			pr_err("\n[avm_debug]unknown mode: %s\n", p);
			return -EINVAL;
		}
	}
	return write_length;
}

/**
 */
static struct _debug_client *find_dbgclient_by_prefix(char *prefix)
{
	struct _debug_client *pdbg;
	unsigned long flags;

	avmdebug_lock(&avm_debug.client_lock, &flags, 1);
	pdbg = avm_debug.dbg_clientAnker;
	while (pdbg) {
		if (strncmp(prefix, pdbg->prefix, strlen(pdbg->prefix)) == 0) {
			avmdebug_unlock(&avm_debug.client_lock, flags);
			return pdbg;
		}
		pdbg = pdbg->next;
	}
	avmdebug_unlock(&avm_debug.client_lock, flags);
	return NULL;
}

/**
 */
static struct _debug_client *
add_dbgclient(char *prefix, void (*CallBackDebug)(char *string, void *refdata),
	      void *refdata)
{
	struct _debug_client *pdbg;
	unsigned long flags;

	pdbg = kmalloc(sizeof(struct _debug_client) + strlen(prefix) + 1,
		       GFP_KERNEL);
	if (pdbg == NULL) {
		return NULL;
	}
	pdbg->CallBackDebug = CallBackDebug;
	pdbg->refdata       = refdata;
	pdbg->prefix        = (char *)pdbg + sizeof(struct _debug_client);
	strcpy(pdbg->prefix, prefix);
	pdbg->next = NULL;
	avmdebug_lock(&avm_debug.client_lock, &flags, 1);
	pdbg->next = avm_debug.dbg_clientAnker;
	avm_debug.dbg_clientAnker = pdbg;
	avmdebug_unlock(&avm_debug.client_lock, flags);
	return pdbg;
}

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
			    void *refdata)
{
	struct _debug_client *client;

	DBG_INFO("[avm_debug]%s(\"%s\", 0x%p, %p)\n", __func__, prefix,
		 CallBackDebug, refdata);

	if (prefix == NULL || CallBackDebug == NULL) {
		DBG_ERR("[avm_debug]%s(\"%s\", 0x%p, %p): invalid param\n",
			__func__, prefix, CallBackDebug, refdata);
		return NULL;
	}
	prefix = skip_spaces(prefix);
	client = find_dbgclient_by_prefix(prefix);
	if (client) {
		DBG_ERR("[avm_debug]%s: prefix '%s' already exist\n", __func__,
			prefix);
		return NULL;
	}
	return add_dbgclient(prefix, CallBackDebug, refdata);
}
EXPORT_SYMBOL(avm_DebugCallRegister);

/**
 * Debug-Funktion am Treiber abmelden
 */
void avm_DebugCallUnRegister(void *handle)
{
	struct _debug_client *pdbg, *prev = NULL;
	unsigned long flags;

	DBG_INFO("[avm_debug]%s: %p done\n", __func__, handle);
	avmdebug_lock(&avm_debug.client_lock, &flags, 1);
	pdbg = avm_debug.dbg_clientAnker;
	while (pdbg) {
		if (pdbg == handle) {
			if (prev == NULL) {
				avm_debug.dbg_clientAnker = pdbg->next;
			} else {
				prev->next = pdbg->next;
			}
			avmdebug_unlock(&avm_debug.client_lock, flags);
			DBG_INFO("[avm_debug]%s: %p done\n", __func__, pdbg);
			kfree(pdbg);
			return;
		}
		prev = pdbg;
		pdbg = pdbg->next;
	}
	avmdebug_unlock(&avm_debug.client_lock, flags);
	DBG_ERR("[avm_debug]%s: error: no handle for %p found\n", __func__,
		pdbg);
}
EXPORT_SYMBOL(avm_DebugCallUnRegister);

/*
 * signal 0 .. 31
 * signal: 0 -> pushmail 2
 * signal: 1 -> crashreport
 *
 *  YIELD-Kontext fest
 */
void avm_DebugSignal(unsigned int signal)
{
	if (avm_debug.init) {
		unsigned long flags;

		pr_notice("%s: %x %s %d [%s]\n", __func__, signal & 0x1F,
			signal & 0x80000000 ? "user pid:" : "kernel info:",
			(signal & ~0x80000000) >> 8, current->comm);
		signal &= 0x1F;
		avmdebug_lock(&avm_debug.signallock, &flags, 1);
		avm_debug.signal |= 0x1 << signal;
		avmdebug_unlock(&avm_debug.signallock, flags);
		rte_wake_up_interruptible(&avm_debug.wait_queue);
	}
}
EXPORT_SYMBOL(avm_DebugSignal);

/**
 * signal 0 .. 31
 */
asmlinkage void avm_DebugSignalLog(unsigned int signal, char *fmt, ...)
{
	if (avm_debug.init) {
		va_list marker;

		va_start(marker, fmt);
		pr_notice("avm_DebugSignal:");
		vprintk_emit_nodict(0, LOGLEVEL_NOTICE, fmt, marker);
		pr_cont("\n");
		va_end(marker);
		avm_DebugSignal(signal);
	}
}
EXPORT_SYMBOL(avm_DebugSignalLog);

/**
 */
static int send_buf(struct socket *sock, struct sockaddr_un *paddr,
		    unsigned char *buffer, unsigned int length,
		    unsigned int wait)
{
	struct msghdr msg;
	struct kvec vec;

	if (sock->sk == NULL) {
		return 0;
	}
	vec.iov_base = (void *)buffer;
	vec.iov_len = length;

	msg.msg_name        = paddr;
	msg.msg_namelen     = sizeof(*paddr);
	msg.msg_control     = NULL;
	msg.msg_controllen  = 0;
	msg.msg_flags       = wait ? 0 : MSG_DONTWAIT;

	length = kernel_sendmsg(sock, &msg, &vec, 1, length);

	return length;
}
#define PUSHMSG(a)                                                             \
	{                                                                      \
		.str = "\x80" a, .size = sizeof(a) + 1                         \
	}
static struct _pushmsg {
	char *str;
	int size;
} pushmsg[] = { PUSHMSG("pushmail 2"), PUSHMSG("crashreport") };

/**
 * aus dem Thread-Kontext pushmsg an den ctrlmgr
 */
static void push_mail(struct _avm_debug *pdbg, unsigned int mode)
{
	/*
	 * unix_mkname() in the unix socket protocol layer assumes that unix
	 * sockets are bound from userspace only, which is probably fair
	 * enough.  It can therefore rely on extra room being available beyond
	 * sizeof(struct sockaddr_un), since the generic socket syscall uses a
	 * struct sockaddr_storage for the socket address.  Accommodate for
	 * that by using a struct sockaddr_storage here as well.
	 */
	struct sockaddr_storage addr;
	struct sockaddr_un *uaddr;
	int err;

	uaddr = (struct sockaddr_un *)&addr;
	if (pdbg->s_push == NULL) {
		err = sock_create(AF_UNIX, SOCK_DGRAM, 0, &pdbg->s_push);
		if (err < 0) {
			printk(
				KERN_ERR
				"[avmdebug]%s: error during creation of socket %d\n",
				__func__, err);
			return;
		}
		uaddr->sun_family = AF_UNIX;
		strlcpy(uaddr->sun_path, "/var/tmp/me_avmdebug.ctl",
			sizeof(uaddr->sun_path));
		if ((pdbg->s_push->ops == NULL) ||
		    (pdbg->s_push->ops->bind == NULL)) {
			printk(KERN_ERR
					"[avmdebug]%s:bind not supported\n",
					__func__);
			sock_release(pdbg->s_push);
			pdbg->s_push = NULL;
			return;
		}
		err = pdbg->s_push->ops->bind(
			pdbg->s_push, (struct sockaddr *)uaddr, sizeof(*uaddr));
		if (err < 0) {
			printk(KERN_ERR
					"[avmdebug]%s:bind failed %d\n",
					__func__, err);
			sock_release(pdbg->s_push);
			pdbg->s_push = NULL;
			return;
		}
	}
	if (mode >= ARRAY_SIZE(pushmsg)) {
		mode = 0;
	}
	printk(KERN_ERR "[avmdebug] push: %s\n",
			pushmsg[mode].str + 1);
	uaddr->sun_family = AF_UNIX;
	strlcpy(uaddr->sun_path, "/var/tmp/me_ctlmgr.ctl", sizeof(uaddr->sun_path));
	err = send_buf(pdbg->s_push, uaddr, pushmsg[mode].str,
		       pushmsg[mode].size, 1);
	if (err != pushmsg[mode].size) {
		printk(KERN_ERR "[avmdebug]%s: failed with ret=%d\n",
				__func__, err);
	}
}

/**
 */
static void push_mailexit(struct _avm_debug *pdbg)
{
#if 0
	if (pdbg->s_push->file && s_push->file->f_dentry && s_push->file->f_dentry->d_op &&
		s_push->file->f_dentry->d_op->d_release) {
		printk("unlink %s\n", addr.sun_path);
		s_push->file->f_dentry->d_op->d_release(s_push->file->f_dentry);
		/*--- s_push->file->f_dentry->d_op->d_delete(s_push->file->f_dentry); ---*/
	}
	err = sys_unlink(addr.sun_path);
	printk("sys_unlink %s %d\n", addr.sun_path, err);
#endif
	if (pdbg->s_push) {
		sock_release(pdbg->s_push);
		pdbg->s_push = NULL;
	}
}
#define TIME_DIFF(act, old) ((unsigned long)(act) - (unsigned long)(old))
#define PUSHMAIL_RETRIGGER (300 * HZ) /*--- alle 5 Minuten ---*/
#define AVMDEBUGTHREAD_WAKE_UP (60 * 60 * HZ)

/**
 */
static int avmdebug_thread(void *data)
{
	struct _avm_debug *pdbg = (struct _avm_debug *)data;
	int ret = 0;
	unsigned long flags, sig;
	unsigned long timeout = AVMDEBUGTHREAD_WAKE_UP;
	unsigned long pushmail_lastjiffies = jiffies - PUSHMAIL_RETRIGGER;
	unsigned long remark_timeoutsignal = 0;

	set_user_nice(current, 19);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible_timeout(pdbg->wait_queue,
						       pdbg->signal, timeout);
		if (ret == -ERESTARTSYS) {
			/* interrupted by signal -> exit */
			break;
		}
		spin_lock_irqsave(&pdbg->signallock, flags);
		sig = pdbg->signal | remark_timeoutsignal;
		pdbg->signal = 0;
		spin_unlock_irqrestore(&pdbg->signallock, flags);

		/*--- printk("[avmdebug]sig %lx tsig %lx timeout %ld s\n", sig, remark_timeoutsignal, timeout / HZ); ---*/
		if (sig & (0x1 << 0)) {
			unsigned long diff_last_pushmail_jiffies =
				TIME_DIFF(jiffies, pushmail_lastjiffies);

			if (diff_last_pushmail_jiffies > PUSHMAIL_RETRIGGER) {
				push_mail(pdbg, 0);
				timeout = AVMDEBUGTHREAD_WAKE_UP;
				remark_timeoutsignal &= ~0x1 << 0;
				pushmail_lastjiffies = jiffies;
			} else {
				remark_timeoutsignal |= 0x1 << 0;
				timeout =
					min(timeout,
					    PUSHMAIL_RETRIGGER -
						    diff_last_pushmail_jiffies);
				/*--- printk("[avmdebug] trigger too " "early: wait %d sec", timeout / HZ); ---*/
			}
		}
		if (sig & (0x1 << 1)) {
			push_mail(pdbg, 1); /*--- Crashreport ---*/
		}
	}
	push_mailexit(pdbg);
	pdbg->kthread = NULL;
	return ret;
}
