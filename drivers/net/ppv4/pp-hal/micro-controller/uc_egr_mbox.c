/*
 * Description: PP Egress micro-controllers mailbox
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2019-2020 Intel Corporation
 */

#define pr_fmt(fmt) "[PP_UC_MBOX]:%s:%d: " fmt, __func__, __LINE__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/bitops.h>

#include "pp_regs.h"
#include "pp_common.h"
#include "uc_regs.h"
#include "uc.h"
#include "uc_internal.h"
#include "infra.h"

/**
 * @brief the offset of the command buffer inside the uc job
 */
#define UC_MBOX_CMD_BUFF_OFF 32

/**
 * @brief the size of the command buffer inside the uc job
 */
#define UC_MBOX_CMD_BUFF_SZ (256 - UC_MBOX_CMD_BUFF_OFF)

/**
 * @brief the command buffer pointer from the uc point of view
 */
#define UC_MBOX_CMD_BUFF_UCV (0xF1000000 + UC_MBOX_CMD_BUFF_OFF)

/**
 * @brief the command buffer pointer from the host point of view
 */
#define UC_MBOX_CMD_BUFF_ADR (UC_DCCM(EGRESS, 0, UC_MBOX_CMD_BUFF_OFF))

/**
 * @brief the mailbox command job index mask
 */
#define UC_MBOX_CMD_IDX_MSK 0x7

/**
 * @brief uc internal memory queue offset
 */
#define UC_MBOX_QUEUE_OFF 0x800

/**
 * @brief uc internal memory cpu offset
 */
#define UC_MBOX_CPU_OFF 0x40000

/**
 * @brief mailbox command job index (0..7)
 */
static u8 cmd_idx;

/**
 * @brief mailbox queue offset
 */
static u32 cmd_q_off;

/**
 * @brief locks database and HW accesses
 */
static spinlock_t mbox_lock;

/**
 * @brief Acquire mbox lock
 */
static inline void __mbox_lock(void)
{
	pr_debug("caller %ps\n", __builtin_return_address(0));

	/* we are not allowed to run in irq context */
	WARN(in_irq(), "uc mbox shouldn't be used from irq context\n");
	spin_lock_bh(&mbox_lock);
}

/**
 * @brief Release mbox lock
 */
static inline void __mbox_unlock(void)
{
	pr_debug("caller %ps\n", __builtin_return_address(0));

	spin_unlock_bh(&mbox_lock);
}

static bool __cmd_is_done(void)
{
	u64 reg;

	if (pp_silicon_step_get() == PP_SSTEP_A)
		reg = UC_A_CCU_GPREG_REG(EGRESS, CMD_DONE_REG_IDX);
	else
		reg = UC_B_CCU_GPREG_REG(EGRESS, CMD_DONE_REG_IDX);

	/* wait for command done signal */
	if (pp_reg_poll(reg, U8_MAX, MBOX_CMD_SUCCESS)) {
		pr_err("cmd error %u\n", PP_REG_RD32(reg));
		return false;
	}

	return true;
}

static void __cmd_q_off_set(void)
{
	u32 val;
	u8 qid, cid;

	if (pp_silicon_step_get() == PP_SSTEP_A)
		val = PP_REG_RD32(UC_A_CCU_GPREG_REG(EGRESS, CMD_DONE_REG_IDX));
	else
		val = PP_REG_RD32(UC_B_CCU_GPREG_REG(EGRESS, CMD_DONE_REG_IDX));

	qid = (val >> 8) & U8_MAX;
	cid = (val >> 16) & U8_MAX;

	cmd_q_off = qid * UC_MBOX_QUEUE_OFF + cid * UC_MBOX_CPU_OFF;
	pr_debug("cmd qid=%hhu cid=%hhu q_off=%u\n", qid, cid, cmd_q_off);
}

static s32 __cmd_send(u32 type, u32 param, u32 addr, u16 len)
{
	u32 flags = BIT(WR_FLAG_IS_DATA);

	/* write the massage format to the HW interface */
	if (pp_silicon_step_get() == PP_SSTEP_A) {
		PP_REG_WR32(UC_A_CCU_GPREG_REG(EGRESS, CMD_DONE_REG_IDX),
			    MBOX_CMD_BUSY);
		PP_REG_WR32(UC_EGR_MBOX_MSG0_WR_DATA0, type);
		PP_REG_WR32(UC_EGR_MBOX_MSG0_WR_DATA1, param);
		PP_REG_WR32(UC_EGR_MBOX_MSG0_WR_ADDRESS, addr);
		PP_REG_WR32(UC_EGR_MBOX_MSG0_WR_FLAGS, flags);
		PP_REG_WR32(UC_EGR_MBOX_MSG0_WR_LENGTH, len);
		/* trigger the command */
		PP_REG_WR32(UC_SW_WRITER_MBOX_CH_TRIGGER, UC_MBOX_PORT);
	} else {
		PP_REG_WR32(UC_B_CCU_GPREG_REG(EGRESS, CMD_DONE_REG_IDX),
			    MBOX_CMD_BUSY);
		PP_REG_WR32(UC_EGR_B_MBOX_PORT_WR_DATA0(UC_MBOX_PORT), type);
		PP_REG_WR32(UC_EGR_B_MBOX_PORT_WR_DATA1(UC_MBOX_PORT), param);
		PP_REG_WR32(UC_EGR_B_MBOX_PORT_WR_ADDRESS(UC_MBOX_PORT), addr);
		/* trigger the command */
		PP_REG_WR32(UC_EGR_B_MBOX_PORT_WR_LENGTH_FLAGS(UC_MBOX_PORT),
			    (flags << 16) | len);
	}

	/* verify command is done */
	if (unlikely(!__cmd_is_done()))
		return -EPERM;

	if (type == UC_CMD_SYNC)
		__cmd_q_off_set();

	return 0;
}

static bool __cmd_is_valid(enum uc_mbox_cmd_type type)
{
	if (type > UC_CMD_LAST || type < UC_CMD_FIRST) {
		pr_err("invalid command type %u\n", (u32)type);
		return false;
	}

	return true;
}

static bool __cmd_is_buff_len_valid(u16 len)
{
	if (len > UC_MBOX_CMD_BUFF_SZ) {
		pr_err("buffer to write too big %hu, max is %u\n", len,
		       (u32)UC_MBOX_CMD_BUFF_SZ);
		return false;
	}

	return true;
}

static inline u16 __cmd_next_buf_off(void)
{
	return (((cmd_idx++ & UC_MBOX_CMD_IDX_MSK) << 8) + cmd_q_off);
}

s32 uc_egr_mbox_cmd_send(enum uc_mbox_cmd_type type, u32 param,
			 const void *in_buf, u16 in_len,
			 void *out_buf, u16 out_len)
{
	s32 ret;
	u16 cmd_buf_off = __cmd_next_buf_off();
	void *cmd_buff = (void *)(UC_MBOX_CMD_BUFF_ADR + cmd_buf_off);

	pr_debug("uc mbox cmd %u, param %u, in_buf %ps, in_len %hu, out_buf %ps, out_len %hu\n",
		 (u32)type, param, in_buf, in_len, out_buf, out_len);

	if (!uc_is_cluster_valid(UC_IS_EGR))
		return -EPERM;

	if (!__cmd_is_valid(type))
		return -EINVAL;

	if (!__cmd_is_buff_len_valid(in_len) ||
	    !__cmd_is_buff_len_valid(out_len))
		return -EINVAL;

	__mbox_lock();
	/* copy data to FW buffer */
	if (in_buf && in_len)
		memcpy(cmd_buff, in_buf, in_len);

	/* send the command */
	ret = __cmd_send(type, param, UC_MBOX_CMD_BUFF_UCV, in_len);
	if (ret)
		goto unlock;

	/* copy data to caller buffer */
	if (out_buf && out_len)
		memcpy(out_buf, cmd_buff, out_len);

unlock:
	__mbox_unlock();
	return ret;
}

s32 uc_egr_mbox_init(void)
{
	/* init lock */
	spin_lock_init(&mbox_lock);

	if (pp_silicon_step_get() != PP_SSTEP_A)
		PP_REG_WR32(UC_EGR_B_WRITER_PORT_ENABLE(UC_MBOX_PORT), 1);

	return uc_egr_mbox_cmd_send(UC_CMD_SYNC, 0, NULL, 0, NULL, 0);
}

void uc_egr_mbox_exit(void)
{
}
