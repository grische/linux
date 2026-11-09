/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_COMPAT_PROC_FS_H
#define _AVM_COMPAT_PROC_FS_H

#include <linux/version.h>
#include <linux/proc_fs.h>

/* Since Linux v5.17 (commit 359745d78351 ("proc: remove PDE_DATA()
 * completely", 2022-01-21)), PDE_DATA() has been removed and its calls shall
 * be replaced by pde_data().
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
#define pde_data PDE_DATA
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
/* Provide the new-style procfs kernel API for old kernels.
 *
 * See mainline commits:
 * commit 97a32539b956 proc: convert everything to "struct proc_ops"
 * commit d56c0d45f0e2 proc: decouple proc from VFS with "struct proc_ops"
 *
 * proc_ops replaces file_operations and must be passed to proc_* interfaces.
 *
 * Conversion rule (see commit 97a32539b956):
 * llseek          => proc_lseek
 * unlocked_ioctl  => proc_ioctl
 * *               => proc_*
 * owner           => delete assignment
 */
#ifndef AVM_COMPAT_NO_PROC_OPS
/* Define if you don't want the proc_ops backport, e.g. if the
 * proc_read definition conflicts with members in unrelated types.
 */
struct proc_ops {
#define proc_open    _proc_fops.open
#define proc_read    _proc_fops.read
#define proc_write   _proc_fops.write
#define proc_lseek   _proc_fops.llseek
#define proc_ioctl   _proc_fops.unlocked_ioctl
#define proc_release _proc_fops.release
	struct file_operations _proc_fops;
};

#define proc_create_data(name, mode, proc_dir, proc_ops, data) \
	proc_create_data(name, mode, proc_dir, &(proc_ops)->_proc_fops, data)

#define proc_create(name, mode, proc_dir, proc_ops) \
	proc_create(name, mode, proc_dir, &(proc_ops)->_proc_fops)

#endif /* AVM_COMPAT_NO_PROC_OPS */
#endif

#endif /* _AVM_COMPAT_PROC_FS_H */
