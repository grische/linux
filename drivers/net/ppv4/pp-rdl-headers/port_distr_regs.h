/**
 * port_dist_regs.h
 * Description: port_distr_s HW registers definitions
 * Auto-generated file by RDL Parser, DO NOT CHANGE
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2018 Intel Corporation
 */

#ifndef _PP_PORT_DIST_H_
#define _PP_PORT_DIST_H_

#define PP_PORT_DIST_GEN_DATE_STR "Monday Oct 15, 2018 [2:34:27 pm]"
#define PP_PORT_DIST_BASE (0xF0004000ULL)

/**
 * SW_REG_NAME : PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_REG
 * HW_REG_NAME : port_distr_source_port_to_rpb_port
 * DESCRIPTION : match source port n to rpb port and tc + tc select
 *               - 0 to take tc from status word, 1 to take tc from
 *               table.
 *
 *  Register Fields :
 *   [31: 5][RO] - Reserved
 *   [ 4: 3][RW] - MISSING_DESCRIPTION
 *   [ 2: 2][RW] - MISSING_DESCRIPTION
 *   [ 1: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_REG ((PORT_DIST_BASE_ADDR) + 0x0)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_RESERVED_1_OFF (5)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_RESERVED_1_LEN (27)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_RESERVED_1_MSK (0xFFFFFFE0)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_RESERVED_1_RST (0x0)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_OFF (3)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_LEN (2)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_MSK (0x00000018)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_RST (0x0)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_SELECT_OFF (2)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_SELECT_LEN (1)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_SELECT_MSK (0x00000004)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_TC_SELECT_RST (0x0)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_OFF (0)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_LEN (2)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_MSK (0x00000003)
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_REG_IDX
 * NUM OF REGISTERS : 256
 */
#define PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_REG_IDX(idx) \
	(PP_PORT_DIST_SOURCE_PORT_TO_RPB_PORT_REG + ((idx) << 2))

/**
 * SW_REG_NAME : PP_PORT_DIST_PKT_CNT_REG
 * HW_REG_NAME : port_distr_pkt_cnt
 * DESCRIPTION : counts packets transmitted. counter n for port i
 *               tc j: n=4*i+j
 *
 *  Register Fields :
 *   [31: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_PKT_CNT_REG ((PORT_DIST_BASE_ADDR) + 0x400)
#define PP_PORT_DIST_PKT_CNT_OFF (0)
#define PP_PORT_DIST_PKT_CNT_LEN (32)
#define PP_PORT_DIST_PKT_CNT_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_PKT_CNT_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_PKT_CNT_REG_IDX
 * NUM OF REGISTERS : 16
 */
#define PP_PORT_DIST_PKT_CNT_REG_IDX(idx) \
	(PP_PORT_DIST_PKT_CNT_REG + ((idx) << 2))

/**
 * SW_REG_NAME : PP_PORT_DIST_BYTE_CNT_REG
 * HW_REG_NAME : port_distr_byte_cnt
 * DESCRIPTION : counts bytes transmitted. counter n for port i tc
 *               j: n=4*i+j
 *
 *  Register Fields :
 *   [31: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_BYTE_CNT_REG ((PORT_DIST_BASE_ADDR) + 0x440)
#define PP_PORT_DIST_BYTE_CNT_OFF (0)
#define PP_PORT_DIST_BYTE_CNT_LEN (32)
#define PP_PORT_DIST_BYTE_CNT_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_BYTE_CNT_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_BYTE_CNT_REG_IDX
 * NUM OF REGISTERS : 16
 */
#define PP_PORT_DIST_BYTE_CNT_REG_IDX(idx) \
	(PP_PORT_DIST_BYTE_CNT_REG + ((idx) << 2))

/**
 * SW_REG_NAME : PP_PORT_DIST_INIT_DONE_REG
 * HW_REG_NAME : port_distr_init_done
 * DESCRIPTION : counters memories zero init done.
 *
 *  Register Fields :
 *   [31: 1][RO] - Reserved
 *   [ 0: 0][RO] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_INIT_DONE_REG ((PORT_DIST_BASE_ADDR) + 0x480)
#define PP_PORT_DIST_INIT_DONE_RSVD0_OFF (1)
#define PP_PORT_DIST_INIT_DONE_RSVD0_LEN (31)
#define PP_PORT_DIST_INIT_DONE_RSVD0_MSK (0xFFFFFFFE)
#define PP_PORT_DIST_INIT_DONE_RSVD0_RST (0x0)
#define PP_PORT_DIST_INIT_DONE_OFF (0)
#define PP_PORT_DIST_INIT_DONE_LEN (1)
#define PP_PORT_DIST_INIT_DONE_MSK (0x00000001)
#define PP_PORT_DIST_INIT_DONE_RST (0x0)

/**
 * SW_REG_NAME : PP_PORT_DIST_PORT_BYTE_CNT_LO_REG
 * HW_REG_NAME : port_distr_port_byte_cnt_lo
 * DESCRIPTION : counts bytes transmitted. 48 bit counter
 *
 *  Register Fields :
 *   [31: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_PORT_BYTE_CNT_LO_REG ((PORT_DIST_BASE_ADDR) + 0x500)
#define PP_PORT_DIST_PORT_BYTE_CNT_LO_OFF (0)
#define PP_PORT_DIST_PORT_BYTE_CNT_LO_LEN (32)
#define PP_PORT_DIST_PORT_BYTE_CNT_LO_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_PORT_BYTE_CNT_LO_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_PORT_BYTE_CNT_LO_REG_IDX
 * NUM OF REGISTERS : 256
 */
#define PP_PORT_DIST_PORT_BYTE_CNT_LO_REG_IDX(idx) \
	(PP_PORT_DIST_PORT_BYTE_CNT_LO_REG + ((idx) << 3))

/**
 * SW_REG_NAME : PP_PORT_DIST_PORT_BYTE_CNT_HI_REG
 * HW_REG_NAME : port_distr_port_byte_cnt_hi
 * DESCRIPTION : counts bytes transmitted. 48 bit counter
 *
 *  Register Fields :
 *   [31:16][RO] - Reserved
 *   [15: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_REG ((PORT_DIST_BASE_ADDR) + 0x504)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_RSVD0_OFF (16)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_RSVD0_LEN (16)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_RSVD0_MSK (0xFFFF0000)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_RSVD0_RST (0x0)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_OFF (0)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_LEN (16)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_MSK (0x0000FFFF)
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_PORT_BYTE_CNT_HI_REG_IDX
 * NUM OF REGISTERS : 256
 */
#define PP_PORT_DIST_PORT_BYTE_CNT_HI_REG_IDX(idx) \
	(PP_PORT_DIST_PORT_BYTE_CNT_HI_REG + ((idx) << 3))

/**
 * SW_REG_NAME : PP_PORT_DIST_PORT_PKT_CNT_LO_REG
 * HW_REG_NAME : port_distr_port_pkt_cnt_lo
 * DESCRIPTION : counts pkts transmitted. 40 bit counter
 *
 *  Register Fields :
 *   [31: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_PORT_PKT_CNT_LO_REG ((PORT_DIST_BASE_ADDR) + 0xd00)
#define PP_PORT_DIST_PORT_PKT_CNT_LO_OFF (0)
#define PP_PORT_DIST_PORT_PKT_CNT_LO_LEN (32)
#define PP_PORT_DIST_PORT_PKT_CNT_LO_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_PORT_PKT_CNT_LO_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_PORT_PKT_CNT_LO_REG_IDX
 * NUM OF REGISTERS : 256
 */
#define PP_PORT_DIST_PORT_PKT_CNT_LO_REG_IDX(idx) \
	(PP_PORT_DIST_PORT_PKT_CNT_LO_REG + ((idx) << 3))

/**
 * SW_REG_NAME : PP_PORT_DIST_PORT_PKT_CNT_HI_REG
 * HW_REG_NAME : port_distr_port_pkt_cnt_hi
 * DESCRIPTION : counts pkts transmitted. 40 bit counter
 *
 *  Register Fields :
 *   [31: 8][RO] - Reserved
 *   [ 7: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_PORT_PKT_CNT_HI_REG ((PORT_DIST_BASE_ADDR) + 0xd04)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_RSVD0_OFF (8)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_RSVD0_LEN (24)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_RSVD0_MSK (0xFFFFFF00)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_RSVD0_RST (0x0)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_OFF (0)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_LEN (8)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_MSK (0x000000FF)
#define PP_PORT_DIST_PORT_PKT_CNT_HI_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_PORT_PKT_CNT_HI_REG_IDX
 * NUM OF REGISTERS : 256
 */
#define PP_PORT_DIST_PORT_PKT_CNT_HI_REG_IDX(idx) \
	(PP_PORT_DIST_PORT_PKT_CNT_HI_REG + ((idx) << 3))

/**
 * SW_REG_NAME : PP_PORT_DIST_PKT_COUNT_REG
 * HW_REG_NAME : packet_count
 * DESCRIPTION : counts packet SOP at the period on cycles [define
 *               at the cycles_reg]
 *
 *  Register Fields :
 *   [31: 0][RO] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_PKT_COUNT_REG ((PORT_DIST_BASE_ADDR) + 0x1500)
#define PP_PORT_DIST_PKT_COUNT_PKT_COUNT_I_OFF (0)
#define PP_PORT_DIST_PKT_COUNT_PKT_COUNT_I_LEN (32)
#define PP_PORT_DIST_PKT_COUNT_PKT_COUNT_I_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_PKT_COUNT_PKT_COUNT_I_RST (0x0)

/**
 * SW_REG_NAME : PP_PORT_DIST_CYCLES_PERIOD_REG
 * HW_REG_NAME : cycles_period
 * DESCRIPTION : define the period cycles which the packet count
 *               register work
 *
 *  Register Fields :
 *   [31: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_CYCLES_PERIOD_REG ((PORT_DIST_BASE_ADDR) + 0x1504)
#define PP_PORT_DIST_CYCLES_PERIOD_CYCLES_PERIOD_I_OFF (0)
#define PP_PORT_DIST_CYCLES_PERIOD_CYCLES_PERIOD_I_LEN (32)
#define PP_PORT_DIST_CYCLES_PERIOD_CYCLES_PERIOD_I_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_CYCLES_PERIOD_CYCLES_PERIOD_I_RST (0x0)

/**
 * SW_REG_NAME : PP_PORT_DIST_MPPS_GO_REG
 * HW_REG_NAME : mpps_go
 * DESCRIPTION : mpps_go ; start to count the MPPS [ 1- start
 *               counting ; read 0 mean that the period cycle done
 *
 *  Register Fields :
 *   [31: 1][RO] - Reserved
 *   [ 0: 0][RW] - MISSING_DESCRIPTION
 *
 */
#define PP_PORT_DIST_MPPS_GO_REG ((PORT_DIST_BASE_ADDR) + 0x1508)
#define PP_PORT_DIST_MPPS_GO_RSVD0_OFF (1)
#define PP_PORT_DIST_MPPS_GO_RSVD0_LEN (31)
#define PP_PORT_DIST_MPPS_GO_RSVD0_MSK (0xFFFFFFFE)
#define PP_PORT_DIST_MPPS_GO_RSVD0_RST (0x0)
#define PP_PORT_DIST_MPPS_GO_GO_I_OFF (0)
#define PP_PORT_DIST_MPPS_GO_GO_I_LEN (1)
#define PP_PORT_DIST_MPPS_GO_GO_I_MSK (0x00000001)
#define PP_PORT_DIST_MPPS_GO_GO_I_RST (0x0)

/**
 * SW_REG_NAME : PP_PORT_DIST_STATUS_DBG_REG
 * HW_REG_NAME : status_dbg_reg
 * DESCRIPTION : status_dbg -capture the input status for debug ;
 *               will be sticky when Error_bit is aserted in the
 *               STW ; write status_dbg[0] will release the sticky
 *
 *  Register Fields :
 *   [31: 0][RW] - status_dbg
 *
 */
#define PP_PORT_DIST_STATUS_DBG_REG ((PORT_DIST_BASE_ADDR) + 0x150C)
#define PP_PORT_DIST_STATUS_DBG_STATUS_DBG_I_OFF (0)
#define PP_PORT_DIST_STATUS_DBG_STATUS_DBG_I_LEN (32)
#define PP_PORT_DIST_STATUS_DBG_STATUS_DBG_I_MSK (0xFFFFFFFF)
#define PP_PORT_DIST_STATUS_DBG_STATUS_DBG_I_RST (0x0)
/**
 * REG_IDX_ACCESS   : PP_PORT_DIST_STATUS_DBG_REG_IDX
 * NUM OF REGISTERS : 5
 */
#define PP_PORT_DIST_STATUS_DBG_REG_IDX(idx) \
	(PP_PORT_DIST_STATUS_DBG_REG + ((idx) << 2))

#endif
