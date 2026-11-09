/*
 * pp_regs.c
 * Description: PP Registers Translation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018-2020 Intel Corporation
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include "pp_regs.h"

/**
 * @brief Add function name and line number for all pr_* prints
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "[PP_REGS]:%s:%d: " fmt, __func__, __LINE__
#endif

#define PP_REG_DBG(format, arg...) pr_debug(format, ##arg)

bool simulation_dbg;

s32 pp_raw_reg_poll(u64 addr, u32 mask, u32 val, u32 retries,
		    u32 *attempts, u32 *read_val, enum pp_poll_op op)
{
	u32 reg_val = 0;
	u32 fld_val = 0;
	s32 ret = 0;

	if (!mask || !retries)
		return -EINVAL;

	/* Always success in case of simulation environment */
	if (simulation_dbg)
		goto done;

	retries = min(retries, PP_REG_MAX_RETRIES);

	do {
		reg_val = PP_REG_RD32((unsigned long)addr);
		fld_val = PP_FIELD_GET(mask, reg_val);
		pr_debug("reg %#x, expected val %#x, actual val %#x\n",
			 reg_val, val, fld_val);

		if (op == PP_POLL_EQ && fld_val == val)
			goto done;
		if (op == PP_POLL_NE && fld_val != val)
			goto done;
		if (op == PP_POLL_LT && fld_val < val)
			goto done;
		if (op == PP_POLL_GT && fld_val > val)
			goto done;

		if (attempts)
			(*attempts)++;

	} while (--retries);

	ret = -EPERM;
done:
	if (read_val)
		*read_val = reg_val & mask;

	return ret;
}

/* TBD - reduce back to 30 once no need for ddr allocations bookeeping */
#define PPV4_MAX_IO_REGIONS (500)

static struct pp_io_region regions[PPV4_MAX_IO_REGIONS];
static u32 regions_cnt;

static int __init simics_env(char *str)
{
	pr_info("Simics Simulation Enabled\n");
	simulation_dbg = true;

	return 0;
}

early_param("simics", simics_env);

s32 pp_region_add(struct pp_io_region *region)
{
	if (!region) {
		pr_err("Null Region\n");
		return -EINVAL;
	}

	if (regions_cnt == PPV4_MAX_IO_REGIONS) {
		pr_err("Regions count exceeds maximum of %u\n",
		       PPV4_MAX_IO_REGIONS);
		return -ENOMEM;
	}

	memcpy(&regions[regions_cnt], region, sizeof(*region));
	regions_cnt++;

	return 0;
}

void *pp_phys_to_virt(phys_addr_t addr)
{
	u32 idx;
	unsigned long offset;

	for (idx = 0; idx < regions_cnt; idx++) {
		if ((addr >= regions[idx].phys) &&
		    (addr < (regions[idx].phys + regions[idx].sz))) {
			offset = addr - regions[idx].phys;
			return (void *)((unsigned long)regions[idx].virt +
					offset);
		}
	}

	return NULL;
}

phys_addr_t pp_virt_to_phys(void *addr)
{
	u32 idx;
	unsigned long offset;

	for (idx = 0; idx < regions_cnt; idx++) {
		if ((addr >= regions[idx].virt) &&
		    (addr < (void *)((unsigned long)regions[idx].virt +
				     regions[idx].sz))) {
			offset = (unsigned long)addr -
				 (unsigned long)regions[idx].virt;
			return regions[idx].phys + offset;
		}
	}

	return 0;
}

inline void pp_reg_wr32(void *addr, u32 data)
{
	PP_REG_DBG("%#lx: %#x\n", (unsigned long)pp_virt_to_phys(addr), data);

	__raw_writel(data, addr);
}

inline u32 pp_reg_rd32(void *addr)
{
	u32 reg = 0;

	PP_REG_DBG("read %#lx\n",
		   (unsigned long)pp_virt_to_phys(addr));

	reg = __raw_readl(addr);

	PP_REG_DBG("%#lx: %#x\n",
		   (unsigned long)pp_virt_to_phys(addr), reg);
	return reg;
}

inline u16 pp_reg_rd16(void *addr)
{
	u16 reg = 0;

	PP_REG_DBG("read %#lx\n", (unsigned long)pp_virt_to_phys(addr));

	reg = __raw_readw(addr);

	PP_REG_DBG("%#lx: %#x\n", (unsigned long)pp_virt_to_phys(addr), reg);
	return reg;
}

inline u8 pp_reg_rd8(void *addr)
{
	u8 reg = 0;

	PP_REG_DBG("read %#lx\n", (unsigned long)pp_virt_to_phys(addr));

	reg = __raw_readb(addr);

	PP_REG_DBG("%#lx: %#x\n", (unsigned long)pp_virt_to_phys(addr), reg);
	return reg;
}

/**
 * @brief Read 'cnt' from Packet processor registers
 * @param src register's virtual address
 * @param dst buffer to save the data read
 * @param cnt number of bytes to read
 */
inline void pp_memcpy(void *dst, void *src, u32 cnt)
{
	u32 byte_reminder = cnt % 4;
	u32 word_cnt = cnt / 4;
	u32 word_idx, rem_idx;
	u32 *buf = (u32 *)dst;
	u32 byte_offset;
	u8 *byte_buf;

	for (word_idx = 0; word_idx < word_cnt; word_idx++) {
		*buf = pp_reg_rd32((void *)((unsigned long)src +
					    (word_idx * 4)));
		buf++;
	}
	byte_buf = (u8 *)buf;

	for (rem_idx = 0; rem_idx < byte_reminder; rem_idx++) {
		byte_offset = (word_cnt * 4) + rem_idx;
		*byte_buf = pp_reg_rd8((void *)((unsigned long)src +
						byte_offset));
		byte_buf++;
	}
}

void *pp_dma_alloc(size_t size, gfp_t flag, dma_addr_t *dma_handle)
{
	void *addr;
#ifndef CONFIG_PPV4_LGM
	struct pp_io_region region;
#endif
	addr = dma_zalloc_coherent(pp_dev_get(), size, dma_handle, flag);
	if (!addr)
		return NULL;

#ifndef CONFIG_PPV4_LGM
	/* For FLM make sure to store original virtual address */
	region.virt = addr;
	region.phys = *dma_handle;
	region.sz = size;

	pp_region_add(&region);

	/* Overwrite virtual address */
	addr = phys_to_virt(*dma_handle);
#endif

	return addr;
}

void pp_dma_free(size_t size, void *address, dma_addr_t *dma_handle)
{
	void *virt_addr;

	/* In FLM use stored virtual address rather than the user's address */
	if (!IS_ENABLED(CONFIG_PPV4_LGM))
		virt_addr = pp_phys_to_virt(*dma_handle);
	else
		virt_addr = address;

	dma_free_coherent(pp_dev_get(), size, virt_addr, *dma_handle);
}
