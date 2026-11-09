/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _AVM_HELPERS_H_

/* Return:
 *	< 0: Error (-EIO, -EPERM, -EINVAL)
 *	= 0: Empty file
 *	> 0: Number of bytes read
 */
int avm_read_from_file(const char *filename, char *str, int maxlen);

/* Return:
 *	< 0: Error (-EIO, -EPERM, -EINVAL)
 *	= 0: Success
 */
int avm_write_to_file(const char *filename, const char *str, int len);

#define _AVM_HELPERS_H_
#endif /* _AVM_HELPERS_H */
