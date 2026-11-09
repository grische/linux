/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (C) 2023 AVM GmbH <fritzbox_info@avm.de> */

#ifndef AVM_OF_OVERLAY_H
#define AVM_OF_OVERLAY_H

struct dt_overlay_mem {
	phys_addr_t base;
	phys_addr_t size;
	void *dtbo;
};

const struct dt_overlay_mem *avm_of_overlay_get_dtbo_mem(int dtbo_idx);

#endif /* AVM_OF_OVERLAY_H */
