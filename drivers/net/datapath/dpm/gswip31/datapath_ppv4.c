// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#include <net/datapath_api.h>
#include "../datapath.h"
#include "datapath_misc.h"

void dp_print_sched_conf(const struct pp_qos_sched_conf *conf)
{
	pr_info("=====Common Node properties====\n");
	pr_info("bw_limit              %x\n",
		conf->common_prop.bandwidth_limit);
	pr_info("shared_bw_grp         %x\n",
		conf->common_prop.shared_bw_group);

	pr_info("=====Parent Node properties====\n");
	pr_info("best_effort_enable    %x\n",
		conf->sched_parent_prop.best_effort_enable);

	if (conf->sched_parent_prop.arbitration == PP_QOS_ARBITRATION_WSP)
		pr_info("arbi                  %s\n", "WSP");
	else if (conf->sched_parent_prop.arbitration == PP_QOS_ARBITRATION_WRR)
		pr_info("arbi                  %s\n", "WRR");
	else
		pr_info("arbi                  %s\n", "WFQ");

	pr_info("=====Child Node properties====\n");
	pr_info("parent	               %x\n", conf->sched_child_prop.parent);
	pr_info("priority              %x\n", conf->sched_child_prop.priority);
	pr_info("bandwidth_share       %x\n",
		conf->sched_child_prop.wrr_weight);

	pr_info("\n\n");
}

void dp_print_port_conf(const struct pp_qos_port_conf *conf)
{
	pr_info("=====Common Node properties====\n");

	pr_info("bw_limit              %x\n",
		conf->common_prop.bandwidth_limit);
	pr_info("shared_bw_grp         %x\n",
		conf->common_prop.shared_bw_group);

	pr_info("=====Parent Node properties====\n");
	pr_info("best_effort_enable    %x\n",
		conf->port_parent_prop.best_effort_enable);

	if (conf->port_parent_prop.arbitration == PP_QOS_ARBITRATION_WSP)
		pr_info("arbi                  %s\n", "WSP");
	else if (conf->port_parent_prop.arbitration == PP_QOS_ARBITRATION_WRR)
		pr_info("arbi                  %s\n", "WRR");
	else
		pr_info("arbi                  %s\n", "WFQ");

	pr_info("ring_address          %lx\n", conf->ring_address);
	pr_info("ring_size             %x\n", conf->ring_size);
	pr_info("packet_credit_enable  %x\n", conf->packet_credit_enable);
	pr_info("credit                %x\n", conf->credit);
	pr_info("disable               %x\n", conf->disable);
	pr_info("green_threshold       %x\n", conf->green_threshold);
	pr_info("yellow_threshold      %x\n", conf->yellow_threshold);
}

void dp_print_queue_conf(const struct pp_qos_queue_conf *conf)
{
	pr_info("=====Common Node properties====\n");
	pr_info("bw_limit              %x\n",
		conf->common_prop.bandwidth_limit);
	pr_info("shared_bw_grp         %x\n",
		conf->common_prop.shared_bw_group);

	pr_info("=====Child Node properties====\n");
	pr_info("parent	               %x\n", conf->queue_child_prop.parent);
	pr_info("priority              %x\n", conf->queue_child_prop.priority);
	pr_info("bandwidth_share       %x\n",
		conf->queue_child_prop.wrr_weight);

	pr_info("max_burst                   %x\n", conf->max_burst);
	pr_info("blocked                     %x\n", conf->blocked);
	pr_info("wred_enable                 %x\n", conf->wred_enable);
	pr_info("wred_fixed_drop_prob_enable %x\n",
		conf->wred_fixed_drop_prob_enable);
	pr_info("wred_min_avg_green          %x\n", conf->wred_min_avg_green);
	pr_info("wred_max_avg_green          %x\n", conf->wred_max_avg_green);
	pr_info("wred_slope_green            %x\n", conf->wred_slope_green);
	pr_info("wred_fixed_drop_prob_green  %x\n",
		conf->wred_fixed_drop_prob_green);
	pr_info("wred_min_avg_yellow         %x\n", conf->wred_min_avg_yellow);
	pr_info("wred_slope_yellow           %x\n", conf->wred_slope_yellow);
	pr_info("wred_fixed_drop_prob_yellow %x\n",
		conf->wred_fixed_drop_prob_yellow);
	pr_info("wred_min_guaranteed         %x\n", conf->wred_min_guaranteed);
	pr_info("wred_max_allowed            %x\n", conf->wred_max_allowed);
	pr_info("codel_en                    %x\n", conf->codel_en);
	pr_info("eir                         %x\n", conf->eir);
}

int dp_qos_queue_remove(struct pp_qos_dev *qdev, u32 id)
{
	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	return pp_qos_queue_remove(qdev, id);
}

int dp_qos_queue_allocate(struct pp_qos_dev *qdev, u32 *id)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, *id);

	ret = pp_qos_queue_allocate(qdev, id);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS)
		pr_info("%s %s QoS Dev %p ID %d\n",
			__func__, ret ? "Fail" : "OK", qdev, *id);
#endif

	return ret;
}

int dp_qos_queue_info_get(struct pp_qos_dev *qdev, u32 id,
			  struct pp_qos_queue_info *info)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	ret = pp_qos_queue_info_get(qdev, id, info);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s %s QoS Dev %p ID %d\n",
			__func__, ret ? "Fail" : "OK", qdev, id);
		pr_info("================================\n");
		pr_info("port ID         %x\n", info->port_id);
		pr_info("physical ID     %x\n", info->physical_id);
	}
#endif
	return ret;
}

int dp_qos_port_remove(struct pp_qos_dev *qdev, u32 id)
{
	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	return pp_qos_port_remove(qdev, id);
}

int dp_qos_sched_allocate(struct pp_qos_dev *qdev, u32 *id)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, *id);

	ret = pp_qos_sched_allocate(qdev, id);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS)
		pr_info("%s %s QoS Dev %p ID %d\n",
			__func__, ret ? "Fail" : "OK", qdev, *id);
#endif
	return ret;
}

int dp_qos_sched_remove(struct pp_qos_dev *qdev, u32 id)
{
	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	return pp_qos_sched_remove(qdev, id);
}

int dp_qos_port_allocate(struct pp_qos_dev *qdev,
			 u32 physical_id, u32 *id)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, *id);

	ret = pp_qos_port_allocate(qdev, physical_id, id);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS)
		pr_info("%s %s QoS Dev %p ID %d\n",
			__func__, ret ? "Fail" : "OK", qdev, *id);
#endif
	return ret;
}

int dp_qos_port_set(struct pp_qos_dev *qdev, u32 id,
		    const struct pp_qos_port_conf *conf)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s QoS Dev %p ID %d\n", __func__, qdev, id);
		pr_info("================================\n");
		dp_print_port_conf(conf);
	}
#endif

	return pp_qos_port_set(qdev, id, conf);
}

void dp_qos_port_conf_set_default(struct pp_qos_port_conf *conf)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s\n", __func__);
		pr_info("================================\n");
		dp_print_port_conf(conf);
	}
#endif

	pp_qos_port_conf_set_default(conf);
}

void dp_qos_queue_conf_set_default(struct pp_qos_queue_conf *conf)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s\n", __func__);
		pr_info("================================\n");
		dp_print_queue_conf(conf);
	}
#endif

	pp_qos_queue_conf_set_default(conf);
}

int dp_qos_queue_set(struct pp_qos_dev *qdev, u32 id,
		     const struct pp_qos_queue_conf *conf)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("Attach q_node=%d to parent_node=%d\n",
			id, conf->queue_child_prop.parent);
		pr_info("%s QoS Dev %p ID %d\n", __func__, qdev, id);
		pr_info("================================\n");
		dp_print_queue_conf(conf);
	}
#endif
	return pp_qos_queue_set(qdev, id, conf);
}

void dp_qos_sched_conf_set_default(struct pp_qos_sched_conf *conf)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s\n", __func__);
		pr_info("================================\n");
		dp_print_sched_conf(conf);
	}
#endif
	pp_qos_sched_conf_set_default(conf);
}

int dp_qos_sched_set(struct pp_qos_dev *qdev, u32 id,
		     const struct pp_qos_sched_conf *conf)
{
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s QoS Dev %p ID %d\n", __func__, qdev, id);
		pr_info("================================\n");
		dp_print_sched_conf(conf);
	}
#endif
	return pp_qos_sched_set(qdev, id, conf);
}

int dp_qos_queue_conf_get(struct pp_qos_dev *qdev, u32 id,
			  struct pp_qos_queue_conf *conf)
{
	int ret = 0;

	ret = pp_qos_queue_conf_get(qdev, id, conf);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("%s QoS Dev %p ID %d\n", __func__, qdev, id);
		pr_info("================================\n");
		dp_print_queue_conf(conf);
	}
#endif
	return ret;
}

int dp_qos_sched_conf_get(struct pp_qos_dev *qdev, u32 id,
			  struct pp_qos_sched_conf *conf)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	ret = pp_qos_sched_conf_get(qdev, id, conf);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS) {
		pr_info("================================\n");
		dp_print_sched_conf(conf);
	}
#endif
	return ret;
}

int dp_qos_sched_get_queues(struct pp_qos_dev *qdev, u32 id,
			    u16 *queue_ids, u32 size,
			    u32 *queues_num)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	ret = pp_qos_sched_get_queues(qdev, id, queue_ids, size,
				      queues_num);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS)
		pr_info("%s ret = %d queue_ids %d size %d queue_num %d\n",
			__func__, ret, *queue_ids, size, *queues_num);
#endif
	return ret;
}

int dp_qos_port_get_queues(struct pp_qos_dev *qdev, u32 id,
			   u16 *queue_ids, u32 size,
			   u32 *queues_num)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	ret = pp_qos_port_get_queues(qdev, id, queue_ids, size, queues_num);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS)
		pr_info("%s ret = %d queue_ids %d size %d queue_num %d\n",
			__func__, ret, *queue_ids, size, *queues_num);
#endif
	return ret;
}

int dp_qos_port_conf_get(struct pp_qos_dev *qdev, u32 id,
			 struct pp_qos_port_conf *conf)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, id);

	ret = pp_qos_port_conf_get(qdev, id, conf);

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_QOS)
		dp_print_port_conf(conf);
#endif
	return ret;
}

int dp_qos_queue_allocate_id_phy(struct pp_qos_dev *qdev, u32 *id,
				 u32 *phy)
{
	int ret = 0;

	DP_DEBUG(DP_DBG_FLAG_QOS, "%s QoS Dev %p ID %d\n", __func__, qdev, *id);

	ret = pp_qos_queue_allocate_id_phy(qdev, id, phy);

	return ret;
}

struct pp_qos_dev *dp_qos_dev_open(u32 id)
{
	DP_DEBUG(DP_DBG_FLAG_QOS, "%s ID %d\n", __func__, id);

	return pp_qos_dev_open(id);
}

int dp_pp_alloc_port(struct ppv4_port *info)
{
	int qos_p_id = 0;
	struct pp_qos_port_conf conf;
	struct hal_priv *priv = HAL(info->inst);
	struct pp_qos_dev *qos_dev = priv->qdev;

	if (dp_qos_port_allocate(qos_dev,
				 info->cqm_deq_port,
				 &qos_p_id)) {
		pr_err("Failed to alloc QoS for deq_port=%d\n",
		       info->cqm_deq_port);
		return -1;
	}

	dp_qos_port_conf_set_default(&conf);
	conf.port_parent_prop.arbitration = PP_QOS_ARBITRATION_WSP;
	conf.ring_address = (ulong)info->tx_ring_addr;
	conf.ring_size = info->tx_ring_size;
	conf.packet_credit_enable = 1;
	conf.credit = info->tx_pkt_credit;

	if (dp_qos_port_set(qos_dev, qos_p_id, &conf)) {
		pr_err("qos_port_set fail for port(cqm/qos) %d/%d\n",
		       info->cqm_deq_port, qos_p_id);
		dp_qos_port_remove(qos_dev, qos_p_id);
		return -1;
	}
	info->node_id = qos_p_id;
	priv->deq_port_stat[info->cqm_deq_port].flag = PP_NODE_ALLOC;
	priv->deq_port_stat[info->cqm_deq_port].node_id = qos_p_id;
	info->node_id = qos_p_id;
	return 0;
}

int dp_pp_alloc_queue(struct ppv4_queue *info)
{
	struct pp_qos_queue_info q_info;
	struct hal_priv *priv = HAL(info->inst);
	struct pp_qos_dev *qos_dev = priv->qdev;
	int q_node_id;

	if (dp_qos_queue_allocate_id_phy(qos_dev, &q_node_id,
					 &q_info.physical_id)) {
		pr_err("qos_queue_allocate fail\n");
		return -1;
	}

	info->qid = q_info.physical_id;
	info->node_id = q_node_id;

	DP_DEBUG(DP_DBG_FLAG_QOS, "Attached q[%d/%d] to parent_node=%d\n",
		 q_info.physical_id, info->node_id,
		 info->parent);
	return 0;
}

int init_ppv4_qos(int inst, int flag)
{
	union qos_init {
		struct pp_qos_port_conf p_conf;
		struct pp_qos_queue_conf q_conf;
		struct pp_qos_queue_info q_info;
	};
	union qos_init *t = NULL;
	int res = DP_FAILURE;
	struct hal_priv *priv = HAL(inst);
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_QOS_HAL)
	u32 q, idx;
	struct cbm_tx_push *flush_port;
	struct cbm_cpu_port_data cpu_data = { 0 };
#endif
	struct cqm_port_info *deq_pinfo;

	if (!priv) {
		pr_err("priv is NULL\n");
		return DP_FAILURE;
	}
	if (!(flag & DP_PLATFORM_INIT)) {
		/*need to implement de-initialization for qos later*/
		priv->qdev = NULL;
		return DP_SUCCESS;
	}
	{
		extern s32 pp_dev_late_init(void);
		/* PHU: BSP has pp_net_adapter.ko to do this (and nothing else).
		 * Unfortunately, there are no dependencies to force it to
		 * load early enough.
		 */
		pp_dev_late_init();
	}
	priv->qdev = dp_qos_dev_open(dp_port_prop[inst].qos_inst);
	if (!priv->qdev) {
		pr_err("Could not open qos instance %d\n",
		       dp_port_prop[inst].qos_inst);
		return DP_FAILURE;
	}

	t = devm_kzalloc(&g_dp_dev->dev, sizeof(*t), GFP_ATOMIC);
	if (!t)
		return DP_FAILURE;
	if (cbm_cpu_port_get(&cpu_data, 0)) {
		pr_err("cbm_cpu_port_get for CPU port?\n");
		goto EXIT;
	}
	/* Sotre drop/flush port's info */
	flush_port = &cpu_data.dq_tx_flush_info;
	idx = flush_port->deq_port;
	if (idx == 0 || idx >= ARRAY_SIZE(dp_deq_port_tbl[inst])) {
		pr_err("Wrog DP Flush port[%d]\n", idx);
		goto EXIT;
	}
	deq_pinfo = get_dp_deqport_info(inst, idx);

	priv->cqm_drop_p = idx;
	deq_pinfo->tx_pkt_credit = flush_port->tx_pkt_credit;
	deq_pinfo->txpush_addr = (void *)flush_port->txpush_addr;
	deq_pinfo->txpush_addr_qos = (void *)flush_port->txpush_addr_qos;
	deq_pinfo->tx_ring_size = flush_port->tx_ring_size;
	deq_pinfo->dp_port = 0; /* dummy one */
	DP_DEBUG(DP_DBG_FLAG_QOS,
		 "%s [%d]: ring addr/push=0x%p/0x%p size=%d pkt_credit=%d\n",
		 "DP Flush port",
		 priv->cqm_drop_p,
		 deq_pinfo->txpush_addr_qos,
		 deq_pinfo->txpush_addr,
		 deq_pinfo->tx_ring_size,
		 deq_pinfo->tx_pkt_credit);
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_QOS_HAL)
	DP_DEBUG(DP_DBG_FLAG_DBG, "priv=%p deq_port_stat=%p q_dev=%p\n",
		 priv, priv ? priv->deq_port_stat : NULL,
		 priv ? priv->qdev : NULL);
	if (dp_qos_port_allocate(priv->qdev,
				 priv->cqm_drop_p,
				 &priv->ppv4_drop_p)) {
		pr_err("Failed to alloc  qos drop port=%d\n",
		       priv->cqm_drop_p);
		goto EXIT;
	}
	dp_qos_port_conf_set_default(&t->p_conf);
	t->p_conf.port_parent_prop.arbitration = PP_QOS_ARBITRATION_WRR;
	t->p_conf.ring_address = (unsigned long)deq_pinfo->txpush_addr_qos;
	t->p_conf.ring_size = deq_pinfo->tx_ring_size;
	t->p_conf.packet_credit_enable = 1;
	t->p_conf.credit = deq_pinfo->tx_pkt_credit;
	t->p_conf.disable = 0; /*allowed for dequeue*/

	if (dp_qos_port_set(priv->qdev, priv->ppv4_drop_p, &t->p_conf)) {
		pr_err("qos_port_set fail for port(cqm/qos) %d/%d\n",
		       priv->cqm_drop_p, priv->ppv4_drop_p);
		dp_qos_port_remove(priv->qdev, priv->ppv4_drop_p);
		goto EXIT;
	}

	if (dp_qos_queue_allocate_id_phy(priv->qdev, &q, &t->q_info.physical_id)) {
		pr_err("qos_queue_allocate fail\n");
		dp_qos_port_remove(priv->qdev, q);
		goto EXIT;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "ppv4_drop_q alloc ok q_node=%d\n", q);

	/* Making Q to Drop Port */
	dp_qos_queue_conf_set_default(&t->q_conf);
	t->q_conf.blocked = 1; /*drop mode */
	t->q_conf.wred_enable = 0;
	t->q_conf.wred_max_allowed = 0; /*max qocc in pkt */
	t->q_conf.queue_child_prop.parent = priv->ppv4_drop_p;
	if (dp_qos_queue_set(priv->qdev, q, &t->q_conf)) {
		pr_err("qos_queue_set fail for node_id=%d to parent=%d\n",
		       q, t->q_conf.queue_child_prop.parent);
		goto EXIT;
	}
	DP_DEBUG(DP_DBG_FLAG_QOS, "To attach q_node=%d to parent_node=%d\n",
		 q, priv->ppv4_drop_p);

	priv->ppv4_drop_q = t->q_info.physical_id;
	DP_DEBUG(DP_DBG_FLAG_QOS, "Drop queue q[%d/%d] to parent=%d/%d\n",
		 priv->ppv4_drop_q, q,
		 priv->cqm_drop_p,
		 priv->ppv4_drop_p);
#endif /* end of CONFIG_INTEL_DATAPATH_QOS_HAL */
	DP_DEBUG(DP_DBG_FLAG_DBG, "%s done\n", __func__);
	res = DP_SUCCESS;

EXIT:
	devm_kfree(&g_dp_dev->dev, t);
	return res;
}
