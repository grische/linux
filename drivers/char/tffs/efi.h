/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de> */
#ifndef _tffs_efi_h_
#define _tffs_efi_h_

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/zlib.h>
#include <avm/tffs/tffs.h>
#include <linux/env.h>
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#define MAX_NAME_LEN 0x100UL
#define MAX_VAR_SIZE 0x100UL

struct TFFS_EFI_State {
	enum _tffs_id id;
	struct avm_efivar_resource *res;
	const uint8_t *efidata;
	loff_t start;
	size_t len;
	size_t offset;
};

#define PANIC_MODE_BIT 0
#define PANIC_MODE     (1 << PANIC_MODE_BIT)

struct tffs_efi_ctx {
	void *notify_priv;
	tffs3_notify_fn notify_cb;
};
#endif /*--- #ifndef _tffs_efi_h_ ---*/
