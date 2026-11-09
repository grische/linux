/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2014 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*------------------------------------------------------------------------------------------*/
#ifndef _tffs_efi_h_
#define _tffs_efi_h_

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/zlib.h>
#include <avm/tffs/tffs.h>
#include <linux/env.h>
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#define MAX_NAME_LEN    0x100UL
#define MAX_VAR_SIZE    0x100UL

struct TFFS_EFI_State {
    enum _tffs_id id;
    struct avm_efivar_resource *res;
    const uint8_t *efidata;
    loff_t start;
    size_t len;
    size_t offset;
};

#define PANIC_MODE_BIT   0
#define PANIC_MODE       (1 << PANIC_MODE_BIT)

struct tffs_efi_ctx {
    void                    *notify_priv;
    tffs3_notify_fn         notify_cb;
};
#endif /*--- #ifndef _tffs_efi_h_ ---*/
