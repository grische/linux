/*
 * Description: PP micro-controller common definitions
 *
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2020 Intel Corporation
 */
#ifndef __PP_UC_COMMON_H__
#define __PP_UC_COMMON_H__

/* UC version - version major and minor numbers must be aligned with
 * the fw version, if the fw was changed without any interface changes
 * only the build number should be changed and no need to update
 * the version here
 */
#define EGRESS_VER_MAJOR 1
#define EGRESS_VER_MINOR 11

#define INGRESS_VER_MAJOR 1
#define INGRESS_VER_MINOR 2
/**
 * @define CMD_DONE_REG_IDX
 * @brief GP register index for the mailbox command done signal
 */
#define CMD_DONE_REG_IDX 2

/**
 * @enum uc_mbox_cmd_type
 * @brief mailbox command type
 */
enum uc_mbox_cmd_type {
	UC_CMD_FIRST,
	/*! synchronized the host with the mbox queue and cpu id */
	UC_CMD_SYNC = UC_CMD_FIRST,
	/*! send init info to UC */
	UC_CMD_INIT,
	/*! get the egress uc version */
	UC_CMD_VERSION,
	/*! reset the UC logger */
	UC_CMD_LOGGER_RESET,
	/*! set UC logger level */
	UC_CMD_LOGGER_LEVEL_SET,
	/*! set the multicast pp port id */
	UC_CMD_MCAST_PID,
	/*! set the multicast pp rx queue id (second cycle) */
	UC_CMD_MCAST_QUEUE,
	/*! set the multicast group bitmap */
	UC_CMD_MCAST_DST,
	/*! get the multicast packets stats */
	UC_CMD_MCAST_STATS,
	/*! set the ipsec pp port id */
	UC_CMD_IPSEC_PID,
	/*! set the ipsec pp rx queue id (second cycle) */
	UC_CMD_IPSEC_QUEUE,
	/*! set the reassembly info */
	UC_CMD_REASSEMBLY_INFO,
	/*! get reassembly stats in SRAM */
	UC_CMD_REASSEMBLY_STATS_OFF,
	/*! set reassembly context timeout */
	UC_CMD_REASSEMBLY_TIMEOUT_THR,
	/* read UC AUX register value */
	UC_CMD_AUX_REG_RD,
	/* write UC AUX register value */
	UC_CMD_AUX_REG_WR,
	/*! get the fragmentation stats */
	UC_CMD_FRAG_STATS,
	/*! Get Tdox configuration */
	UC_CMD_TDOX_CONF_GET,
	/*! Create Tdox record */
	UC_CMD_TDOX_CREATE,
	/*! Create Tdox record */
	UC_CMD_TDOX_REMOVE,
	/*! Get Tdox record */
	UC_CMD_TDOX_STATS,
	/*! Checker mailbox */
	UC_CMD_CHK_MBX,

	UC_CMD_LAST = UC_CMD_CHK_MBX,
	UC_CMD_MAX = U32_MAX,
};

/**
 * @enum mbox_cmd_err
 * @brief mailbox command error code
 */
enum mbox_cmd_err {
	MBOX_CMD_SUCCESS,
	MBOX_CMD_BUSY,
	MBOX_CMD_UNSUPPORTED,
	MBOX_CMD_INVALID_PARAM,
};

/**
 * @enum uc_log_level
 * @brief logger log level
 */
enum uc_log_level {
	UC_LOG_FATAL,
	UC_LOG_ERROR,
	UC_LOG_WARN,
	UC_LOG_INFO,
	UC_LOG_DEBUG,
	UC_LOG_LEVELS_NUM
};

/**
 * @struct uc_log_msg
 * @brief UC log message
 * @note keep the structure aligned to power of 2
 */
struct uc_log_msg {
	u32 val;
	u32 ts;
	u8 level;
	u8 rsrv;
	char str[38];
	char func[16];
};

/**
 * @struct eg_uc_init_info
 * @brief UC init info
 */
struct eg_uc_init_info {
	/*! logger buffer base address  */
	u32 logger_buff;
	/*! logger buffer size in bytes */
	u32 logger_buff_sz;
	/* checker base address */
	u32 chk_base;
	/* checker counters cache base */
	u32 chk_ctrs_cache_base;
	/* checker dsi ddr base */
	u32 chk_dsi_ddr_base;
};

/**
 * @struct mbox_aux_reg_wr
 * @brief UC Mailbox command intended to aux register
 */
struct mbox_aux_reg_wr {
	/*! aux register */
	u32 reg;
	/*! register value */
	u32 val;
};

/**
 * @struct uc_chk_cmd
 * @brief UC Mailbox command intended to checker
 */
struct uc_chk_cmd {
	/*! session id */
	u32 id;
	/*! checker mailbox command */
	u32 cmd;
};

/**
 * @struct mcast_stats
 */
struct mcast_stats {
	/*! RX packet counter */
	u32 rx_pkt;
	/*! TX packet counter */
	u32 tx_pkt;
	/*! drop packet counter */
	u32 drop_pkt;
	/*! reserved */
	u32 reserved;
};

/**
 * @struct ipsec_stats
 */
struct ipsec_stats {
	/*! RX packet counter */
	u32 rx_pkt;
	/*! TX packet counter */
	u32 tx_pkt;
	/*! error packet counter */
	u32 error_pkt;
	/*! reserved */
	u32 reserved;
};

/**
 * @brief Reassembly network function parameters for the FW
 */
struct reassembly_info {
	/*! host base policy */
	u8 host_base_policy;
	/*! host policies bitmap */
	u8 host_policies_bmap;
	/*! host pid */
	u8 host_pid;
	u8 reserved;
	/*! host queue to send packets to host upon failure */
	u16 host_q;
	/*! 2nd round queue */
	u16 pp_rx_q;
	/*! exception si base address */
	u32 excp_si_base;
	/*! rxdma regs base for calculating buffer size */
	u32 rxdma_base;
	/*! si base address */
	u32 si_base;
};

/**
 * @struct smgr_reass_uc_cpu_stats
 * @brief reassembly UC cpu statistics
 */
struct reassembly_stats {
	u32 rx_pkts;
	u32 tx_pkts;
	u32 reassembled;
	u32 accelerated;
	u32 diverted;
	u32 matched;
	u32 not_matched;
	u32 cntxs_starv;
	u32 cntx_overflow;
	u32 timedout;
	u32 frags_starv;
	u32 duplicates;
	u32 unsupported;
	u32 invalid_len;
	u32 dropped;
	u32 res;
};

/**
 * @struct frag_stats
 */
struct frag_stats {
	/*! RX packet counter */
	u32 rx_pkt;
	/*! TX packet counter */
	u32 tx_pkt;
	/*! Drop packet counter */
	u32 total_drops;
	/*! bmgr drops */
	u32 bmgr_drops;
	/*! don't frag drops */
	u32 df_drops;
	/*! More frags required than supported */
	u32 max_frags_drops;
	/*! Reserved */
	u32 reserved1;
	/*! Reserved */
	u32 reserved2;
};

/**
 * @brief Tdox session information
 */
struct tdox_info {
	u32 supp_enable : 1,
		is_candidate : 1,
		low_queue : 9,
		high_queue : 9,
		threshold : 8;
};

/**
 * @brief Config read from tdox at boot
 */
struct tdox_uc_config {
	u32 record_state_offset;
};

/**
 * @brief Tdox uc create command
 */
struct tdox_record_create_cmd {
	struct tdox_info info;
	u16 sess_id;
	u16 tdox_id;
};

/**
 * @brief Tdox uc remove command
 */
struct tdox_record_remove_cmd {
	u32 tdox_id;
};

#endif /* __PP_UC_COMMON_H__ */
