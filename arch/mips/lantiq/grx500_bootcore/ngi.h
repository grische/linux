#ifndef NGI_H
#define NGI_H
#include <linux/types.h>

#define LTQ_NGI_MAX_PR_REGIONS 8
#define LTQ_NGI_MAX_RT_PR_REGIONS 8
#define LTQ_NUM_TARGET_AGENTS 64
#define LTQ_NUM_INIT_AGENTS 39

#define LTQ_NUM_NGI_INSTANCES 9

/* Protection Region structure */
struct ngi_acl_pr {
	char *pr_name;
	u32 pr_id;
	u32 base;
	u32 size;
	u32 level; /* Evaluation level of PR (0-low, 1-high) */
	u32 group_read_pr; /* Bitset of groups allowed to read */
	u32 group_write_pr; /* Bitset of groups allowed to write */
};

struct ngi_ta_core {
	u32 core_code;
	char *core_name;
	u32 num_pr;
	struct ngi_acl_pr acl_pr[LTQ_NGI_MAX_PR_REGIONS];
};

struct ngi_ia_core {
	u32 core_code;
	char *core_name;
};

struct ngi_rt_core {
	u32 inst_id;
	char *name;
	u32 num_pr;
	struct ngi_acl_pr rt_pr[LTQ_NGI_MAX_RT_PR_REGIONS];
};

struct ngi_dts_cfg {
	u32 bootcore_start;
	u32 bootcore_size;
	u32 memory_size;
};
#endif
