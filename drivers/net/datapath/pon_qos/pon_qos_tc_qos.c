// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright (c) 2020 Intel Corporation
 *
 *****************************************************************************/
#include <linux/netdevice.h>
#include <net/datapath_api.h>
#include <net/datapath_api_qos.h>
#include <net/pon_qos_tc.h>
#include "pon_qos_tc_qos.h"

static LIST_HEAD(port_list);

struct pon_qos_port *pon_qos_port_get(struct net_device *dev)
{
	struct pon_qos_port *p, *n;

	list_for_each_entry_safe (p, n, &port_list, list) {
		if (p->dev == dev)
			return p;
	}

	return NULL;
}

struct pon_qos_port *pon_qos_port_alloc(struct net_device *dev)
{
	struct pon_qos_port *port = NULL;

	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return NULL;

	port->dev = dev;
	INIT_RADIX_TREE(&port->qdiscs, GFP_KERNEL);

	list_add(&port->list, &port_list);

	return port;
}

int pon_qos_port_delete(struct pon_qos_port *port)
{
	list_del(&port->list);
	kfree(port);

	return 0;
}

int pon_qos_fill_port_data(struct pon_qos_qdisc *sch)
{
	dp_subif_t subif = { 0 };
	struct dp_dequeue_res deq = { 0 };
	static struct dp_queue_res q_res[PON_QOS_TC_MAX_Q];
	int ret;

	if (sch->deq_idx < 0) {
		ret = dp_get_netif_subifid(sch->dev, NULL, NULL, 0, &subif, 0);
		if (ret < 0)
			return -1;
		/* This is PON DS port so mark this */
		sch->port = subif.port_id;
		sch->deq_idx = 0;
		sch->ds = true;
	}

	deq.dp_port = sch->port;
	deq.cqm_deq_idx = sch->deq_idx;
	deq.q_res = q_res;
	deq.q_res_size = ARRAY_SIZE(q_res);
	ret = dp_deq_port_res_get(&deq, 0);
	if (ret < 0)
		return -2;

	/* save hw egress prot settings */
	sch->inst = subif.inst;
	sch->def_q = q_res[0].q_id;
	sch->epn = deq.cqm_deq_port;

	return 0;
}

int pon_qos_get_port_info(struct pon_qos_qdisc *sch)
{
	dp_subif_t subif = { 0 };
	int ret = 0;

	if (sch->deq_idx < 0 || sch->port < 0) {
		ret = dp_get_netif_subifid(sch->dev, NULL, NULL, 0, &subif, 0);
		if (ret < 0)
			return -1;
		/* This is PON DS port so mark this */
		sch->port = subif.port_id;
		sch->deq_idx = 0;
		sch->ds = true;
	}

	return 0;
}

int pon_qos_alloc_qdisc(struct pon_qos_qdisc **qdisc)
{
	struct pon_qos_qdisc *qp;

	qp = kzalloc(sizeof(**qdisc), GFP_KERNEL);
	if (!qp)
		return -1;

	*qdisc = qp;

	return 0;
}

void pon_qos_free_qdisc(struct pon_qos_qdisc *qdisc)
{
	kfree(qdisc);
}

struct pon_qos_qdisc *pon_qos_qdisc_find(struct pon_qos_port *port, u32 handle)
{
	return radix_tree_lookup(&port->qdiscs, TC_H_MAJ(handle));
}

int pon_qos_get_sch_by_handle(struct net_device *dev,
			      u32 handle,
			      struct pon_qos_qdisc **sch)
{
	struct pon_qos_port *port = NULL;

	port = pon_qos_port_get(dev);
	if (!port)
		return -1;

	*sch = pon_qos_qdisc_find(port, handle);
	if (!*sch)
		return -2;

	return 0;
}

int pon_qos_get_queue_by_handle(struct net_device *dev,
				u32 handle,
				struct pon_qos_q_data **qid)
{
	struct pon_qos_qdisc *qdisc = NULL;
	int idx = TC_H_MIN(handle) - 1;
	int ret;

	ret = pon_qos_get_sch_by_handle(dev, handle, &qdisc);
	if (ret < 0)
		return -1;

	if (idx < 0 || idx >= PON_QOS_TC_MAX_Q)
		return -2;

	if (!qdisc->qids[idx].qid)
		return -3;

	*qid = &qdisc->qids[idx];

	return 0;
}
EXPORT_SYMBOL(pon_qos_get_queue_by_handle);

/* TODO: proper error handling */
int pon_qos_add_sched(struct pon_qos_qdisc *sch, int prio)
{
	struct dp_node_link node = { 0 };
	struct dp_node_alloc anode = { 0 };
	struct dp_node_prio prio_info = { 0 };
	int ret;

	if (sch->parent != TC_H_ROOT)
		return -1;

	/* change def queue prio */
	prio_info.inst = sch->inst;
	prio_info.id.q_id = sch->def_q;
	prio_info.type = DP_NODE_QUEUE;
	ret = dp_qos_link_prio_get(&prio_info, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, " failed to get prio node\n");
		return -1;
	}

	/* Re-set input and policy */
	prio_info.arbi = ARBITRATION_WSP;
	prio_info.prio_wfq = 7;
	ret = dp_qos_link_prio_set(&prio_info, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "set input of link failed\n");
		return -1;
	}

	/* Allocate sched */
	anode.inst = sch->inst;
	anode.dp_port = sch->port;
	anode.type = DP_NODE_SCH;
	anode.id.sch_id = DP_NODE_AUTO_ID;
	ret = dp_node_alloc(&anode, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "sch_id alloc fialed\n");
		return -1;
	}
	sch->sch_id = anode.id.sch_id;

	/* Link SP/WRR sched to port/sched */
	netdev_dbg(sch->dev, "adding sched id %u\n", node.node_id.sch_id);
	node.inst = sch->inst;
	node.dp_port = sch->port;
	node.p_node_type = DP_NODE_PORT;
	node.p_node_id.cqm_deq_port = sch->epn;
	node.arbi = (sch->type == PON_QOS_QDISC_DRR) ?
			    ARBITRATION_WRR :
			    ARBITRATION_WSP;
	node.prio_wfq = prio;
	node.node_type = DP_NODE_SCH;
	node.node_id.sch_id = sch->sch_id;
	node.cqm_deq_port.cqm_deq_port = sch->epn;
	if (dp_node_link_add(&node, 0) == DP_FAILURE) {
		netdev_err(sch->dev, "failed to link WRR sched to port\n");
		dp_node_free(&anode, DP_NODE_AUTO_FREE_RES);
		return -1;
	}

	return 0;
}

int pon_qos_add_staged_sched(struct pon_qos_qdisc *psch,
			     struct pon_qos_qdisc *csch,
			     int prio)
{
	struct dp_node_link node = { 0 };
	struct dp_node_alloc anode = { 0 };
	int ret;

	/* Allocate sched */
	anode.inst = csch->inst;
	anode.dp_port = csch->port;
	anode.type = DP_NODE_SCH;
	anode.id.sch_id = DP_NODE_AUTO_ID;
	ret = dp_node_alloc(&anode, 0);
	if (ret == DP_FAILURE) {
		netdev_err(csch->dev, "sch_id alloc fialed\n");
		return -1;
	}
	csch->sch_id = anode.id.sch_id;

	/* Link SP/WRR sched to port/sched */
	/*node.inst = csch->inst;*/
	node.dp_port = csch->port;
	node.p_node_type = DP_NODE_SCH;
	node.p_node_id.sch_id = psch->sch_id;
	node.arbi = (psch->type == PON_QOS_QDISC_DRR) ?
			    ARBITRATION_WRR :
			    ARBITRATION_WSP;
	node.prio_wfq = prio;
	node.node_type = DP_NODE_SCH;
	node.node_id.sch_id = csch->sch_id;
	node.cqm_deq_port.cqm_deq_port = csch->epn;
	netdev_dbg(csch->dev, "%s: adding sched id %u\n",
		   __func__, node.node_id.sch_id);
	if (dp_node_link_add(&node, 0) == DP_FAILURE) {
		netdev_err(csch->dev,
			   "failed to link sched %d to sched %d in: %d\n",
			   csch->sch_id, psch->sch_id, prio);
		dp_node_free(&anode, DP_NODE_AUTO_FREE_RES);
		return -1;
	}

	return 0;
}

int pon_qos_sched_del(struct pon_qos_qdisc *sch)
{
	struct dp_node_link node = { 0 };
	struct dp_node_alloc anode = { 0 };
	int ret;

	node.node_type = DP_NODE_SCH;
	node.node_id.sch_id = sch->sch_id;
	ret = dp_node_unlink(&node, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "sched id %d unlink failed\n",
			   node.node_id.sch_id);
		return -1;
	}

	anode.type = DP_NODE_SCH;
	anode.id.sch_id = sch->sch_id;
	ret = dp_node_free(&anode, DP_NODE_AUTO_FREE_RES);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "sched id %d free failed\n",
			   node.node_id.sch_id);
		return -1;
	}

	sch->use_cnt = 0;

	return 0;
}

int pon_qos_add_child_qdisc(struct net_device *dev,
			    struct pon_qos_port *port,
			    enum pon_qos_qdisc_type type,
			    u32 parent,
			    u32 handle)
{
	struct pon_qos_qdisc *qdisc = NULL;
	struct pon_qos_qdisc *csch = NULL;
	struct pon_qos_qdisc *psch = NULL;
	int idx = TC_H_MIN(parent) - 1;
	int prio_w = idx;
	int ret;

	if (!dev || !port)
		return -1;

	if (parent == TC_H_ROOT)
		idx = 0;

	if (idx < 0 || idx >= PON_QOS_TC_MAX_Q)
		return -1;

	netdev_dbg(dev, "searching qdisc %#x idx: %d\n", parent, idx);
	if (parent != TC_H_ROOT) {
		psch = pon_qos_qdisc_find(port, parent);
		if (!psch) {
			netdev_err(dev, "%s: parent not found\n", __func__);
			return -1;
		}
	} else {
		psch = &port->root_qdisc;
	}

	/* if there is a queue on this input then delete it */
	if (psch->qids[idx].qid) {
		netdev_dbg(dev, "parent: %#x handle: %#x prio: %d\n",
			   psch->parent, psch->handle, psch->qids[idx].p_w);
		prio_w = psch->qids[idx].p_w;
		ret = pon_qos_queue_del(psch, idx);
		if (ret < 0) {
			netdev_err(dev,
				   "%s: queue delete failed\n", __func__);
			return -1;
		}
	}
	/* if there is a sched on this input return with error for now */
	csch = psch->children[idx];
	if (csch && csch->sch_id) {
		netdev_err(dev, "%s input allocated by sched\n", __func__);
		return -1;
	}

	if (parent != TC_H_ROOT) {
		/* ready to add the sched */
		ret = pon_qos_alloc_qdisc(&qdisc);
		if (ret < 0)
			return -1;

		psch->children[idx] = qdisc;
		psch->num_children++;
		/* get port data from parent qdisc */
		qdisc->dev = dev;
		qdisc->port = psch->port;
		qdisc->deq_idx = psch->deq_idx;
		qdisc->inst = psch->inst;
		qdisc->def_q = psch->def_q;
		qdisc->epn = psch->epn;
	} else {
		qdisc = psch;
		qdisc->dev = dev;
		ret = pon_qos_fill_port_data(qdisc);
		if (ret < 0) {
			netdev_err(dev,
				   "%s: failed getting port hw config %d\n",
				   __func__, ret);
			goto err_free_qdisc;
		}
	}

	qdisc->type = type;
	qdisc->handle = handle;
	qdisc->parent = parent;
	qdisc->use_cnt = 1;

	if (parent == TC_H_ROOT) {
		ret = pon_qos_add_sched(psch, idx);
		if (ret < 0) {
			netdev_err(dev, "%s: add sched failed\n", __func__);
			goto err_free_qdisc;
		}
		netdev_dbg(dev, "%s: add root sch id %d\n",
			   __func__, psch->sch_id);
	} else {
		netdev_dbg(dev, "root id: %d parent id: %d idx: %d <=> %d\n",
			   port->root_qdisc.sch_id, psch->sch_id, idx, prio_w);
		ret = pon_qos_add_staged_sched(psch, qdisc, prio_w);
		if (ret < 0) {
			netdev_err(dev, "%s: add sched failed\n", __func__);
			goto err_free_qdisc;
		}
	}

	/* add to radix tree here */
	ret = radix_tree_insert(&port->qdiscs, TC_H_MAJ(handle), qdisc);
	if (ret) {
		netdev_err(dev, "qdisc insertion to radix tree failed: %d\n",
			   ret);
		goto err_free_sched;
	}

	return 0;

err_free_sched:
	pon_qos_sched_del(qdisc);
err_free_qdisc:
	if (parent != TC_H_ROOT) {
		netdev_err(dev, "%s: freeing child qdisc\n", __func__);
		psch->children[idx] = NULL;
		psch->num_children--;
		pon_qos_free_qdisc(qdisc);
	}
	return -1;
}

static int pon_qos_get_subif_idx(struct net_device *dev, int *subif)
{
	dp_subif_t dp_subif = { 0 };
	int ret;

	if (!dev)
		return -1;

	ret = dp_get_netif_subifid(dev, NULL, NULL, 0, &dp_subif, 0);
	if (ret < 0) {
		netdev_err(dev, "%s: subif idx get failed\n", __func__);
		return -1;
	}

	*subif = dp_subif.subif;

	return 0;
}

static int pon_qos_sched_policy_update(struct pon_qos_qdisc *sch,
				       int arbi, int prio_w, int idx)
{
	struct dp_node_prio prio_info = { 0 };
	int ret;

	/* update parent sched policy and set prio_w correctly */
	prio_info.id.sch_id = sch->qids[idx].qid;
	prio_info.type = DP_NODE_QUEUE;
	ret = dp_qos_link_prio_get(&prio_info, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "%s :get prio node failed\n", __func__);
		return -1;
	}

	/* Re-set policy */
	prio_info.arbi = (sch->type == PON_QOS_QDISC_DRR) ?
				 ARBITRATION_WRR :
				 ARBITRATION_WSP;
	ret = dp_qos_link_prio_set(&prio_info, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "%s: set sch arbi failed\n", __func__);
		return -1;
	}

	prio_info.id.sch_id = sch->qids[idx].qid;
	prio_info.type = DP_NODE_QUEUE;
	ret = dp_qos_link_prio_get(&prio_info, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "%s :get prio node failed\n", __func__);
		return -1;
	}

	prio_info.prio_wfq = prio_w;
	ret = dp_qos_link_prio_set(&prio_info, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "%s: set sch arbi failed\n", __func__);
		return -1;
	}

	return 0;
}

int pon_qos_queue_add(struct pon_qos_qdisc *sch, int arbi, int prio_w, int idx)
{
	struct dp_node_link node = { 0 };
	struct dp_node_alloc anode = { 0 };
	int ret;

	if (!sch)
		return -1;

	anode.inst = sch->inst;
	anode.dp_port = sch->port;
	anode.type = DP_NODE_QUEUE;
	anode.id.q_id = DP_NODE_AUTO_ID;
	ret = dp_node_alloc(&anode, 0);
	if (ret == DP_FAILURE) {
		netdev_err(sch->dev, "failed to alloc queue\n");
		return -1;
	}

	sch->qids[idx].qid = anode.id.q_id;
	sch->qids[idx].arbi = arbi;
	sch->qids[idx].p_w = prio_w;

	node.arbi = (arbi == PON_QOS_QDISC_DRR) ?
			    ARBITRATION_WRR :
			    ARBITRATION_WSP;
	node.prio_wfq = prio_w;
	node.node_type = DP_NODE_QUEUE;
	node.node_id.q_id = sch->qids[idx].qid;
	node.p_node_type = DP_NODE_SCH;
	node.p_node_id.sch_id = sch->sch_id;
	if (dp_node_link_add(&node, 0) == DP_FAILURE) {
		netdev_err(sch->dev, "failed to link queue\n");
		anode.type = DP_NODE_QUEUE;
		anode.id.q_id = sch->qids[idx].qid;
		ret = dp_node_free(&anode, DP_NODE_AUTO_FREE_RES);
		if (ret == DP_FAILURE)
			netdev_err(sch->dev, "qid %d free failed\n",
				   anode.id.q_id);
		memset(&sch->qids[idx], 0, sizeof(struct pon_qos_q_data));
		return -1;
	}

	sch->num_q++;
	ret = pon_qos_sched_policy_update(sch, arbi, prio_w, idx);
	if (ret < 0)
		netdev_err(sch->dev, "%s:policy set fail\n", __func__);

	return 0;
}

int pon_qos_queue_del(struct pon_qos_qdisc *sch, int idx)
{
	struct dp_node_alloc anode = { 0 };
	int ret;

	if (!sch)
		return -1;

	if (idx < 0 || idx > PON_QOS_TC_MAX_Q - 1)
		return -1;

	anode.type = DP_NODE_QUEUE;
	anode.id.q_id = sch->qids[idx].qid;
	if (!anode.id.q_id /*|| atomic_read(&sch->qids[idx].ref_cnt)*/)
		return -1;

	ret = dp_node_free(&anode, DP_NODE_AUTO_FREE_RES);
	if (ret == DP_FAILURE)
		netdev_err(sch->dev, "qid %d free failed\n",
			   anode.id.q_id);

	memset(&sch->qids[idx], 0, sizeof(struct pon_qos_q_data));
	sch->num_q--;

	netdev_dbg(sch->dev, "qid: %i deleted\n", anode.id.q_id);
	return 0;
}

int pon_qos_tc_qdisc_unlink(struct pon_qos_port *p, struct pon_qos_qdisc *sch)
{
	int idx = TC_H_MIN(sch->parent) - 1;
	struct pon_qos_qdisc *psch = NULL;

	if (!p || !sch)
		return -EINVAL;

	if (idx < 0 || idx >= PON_QOS_TC_MAX_Q)
		return -EINVAL;

	psch = pon_qos_qdisc_find(p, sch->parent);
	if (!psch)
		return -EINVAL;

	psch->children[idx] = NULL;
	psch->num_children--;

	return 0;
}

int pon_qos_qdisc_tree_del(struct pon_qos_port *p, struct pon_qos_qdisc *root)
{
	int i, ret;

	/* try removing the queues */
	for (i = 0; i < PON_QOS_TC_MAX_Q; i++) {
		if (root->qids[i].qid) {
			netdev_dbg(root->dev, "%s: deleting qid: %d\n",
				   __func__, root->qids[i].qid);
			ret = pon_qos_queue_del(root, i);
			if (ret < 0)
				return -1;
		}
	}

	for (i = 0; i < PON_QOS_TC_MAX_Q; i++) {
		struct pon_qos_qdisc *sch = root->children[i];

		if (sch && sch->use_cnt) {
			netdev_dbg(sch->dev, "%s: delete child %u of %u\n",
				   __func__,
				   root->children[i]->sch_id,
				   root->sch_id);
			ret = pon_qos_qdisc_tree_del(p, sch);
			if (ret < 0)
				return -1;
		}
	}

	netdev_dbg(root->dev, "%s: del sched id: %d use: %d\n",
		   __func__, root->sch_id, root->use_cnt);
	if (!root->use_cnt)
		return 0;

	ret = pon_qos_sched_del(root);
	if (ret < 0)
		return -1;

	p->sch_num--;
	if (root->parent != TC_H_ROOT && TC_H_MIN(root->parent)) {
		ret = pon_qos_tc_qdisc_unlink(p, root);
		if (ret < 0)
			netdev_err(root->dev, "%s: sch[%d] unlink failed\n",
				   __func__, root->sch_id);
	}

	WARN_ON(radix_tree_delete(&p->qdiscs, TC_H_MAJ(root->handle)) != root);
	if (root->parent != TC_H_ROOT)
		pon_qos_free_qdisc(root);
	else
		pon_qos_port_delete(p);

	return 0;
}

static void pon_qos_dump_qdisc(struct pon_qos_qdisc *sch)
{
	netdev_dbg(sch->dev, "hdl:%#x pid:%#x sta:%d numq:%d num sched:%d\n",
		   sch->handle, sch->parent, sch->use_cnt,
		   sch->num_q, sch->num_children);
}

int pon_qos_tc_sched_status(struct pon_qos_port *p, struct pon_qos_qdisc *root)
{
	int i;

	pon_qos_dump_qdisc(root);

	for (i = 0; i < PON_QOS_TC_MAX_Q; i++) {
		struct pon_qos_qdisc *sch = root->children[i];

		if (!sch)
			continue;

		pon_qos_tc_sched_status(p, sch);
	}

	return 0;
}

static int pon_qos_set_qmap(struct pon_qos_q_data *qid,
			    int port, int tc, bool en)
{
	struct dp_queue_map_set qmap_set = { 0 };
	int ret;

	qmap_set.inst = 0;
	qmap_set.q_id = en ? qid->qid : 0;

	qmap_set.map.dp_port = port;
	qmap_set.map.subif = tc;
	qmap_set.map.class = tc;
	qmap_set.mask.flowid = 1;
	qmap_set.mask.enc = 1;
	qmap_set.mask.dec = 1;
	qmap_set.mask.mpe1 = 1;
	qmap_set.mask.mpe2 = 1;

	ret = dp_queue_map_set(&qmap_set, 0);
	if (ret == DP_FAILURE) {
		pr_err("%s: queue map set failed\n", __func__);
		return -1;
	}

	return 0;
}

static int pon_qos_config_cpu_port(struct pon_qos_q_data *qid,
				   int port, int tc, bool en)
{
	int ret, i;

	/* only 8 classes for CPU, starting from TC=8 */
	tc += 8;

	ret = pon_qos_set_qmap(qid, port, tc, en);
	if (ret != 0)
		return -1;

	if (!en)
		atomic_dec(&qid->ref_cnt);
	else
		atomic_inc(&qid->ref_cnt);

	if (tc != 15)
		return 0;

	/* map class [0..7] and 15 to low prio queue */
	for (i = 0; i < 7; i++) {
		ret = pon_qos_set_qmap(qid, port, i, en);
		if (ret != 0)
			pr_err("%s: cpu port config failed\n", __func__);
	}

	return ret;
}

int pon_qos_update_qmap(struct net_device *dev,
			struct pon_qos_qmap_tc *q_tc, bool en)
{
	struct pon_qos_port *port = NULL;
	struct pon_qos_qdisc *sch = NULL;
	struct pon_qos_q_data *qid = NULL;
	int ret, tc;

	port = pon_qos_port_get(dev);
	if (!port)
		return -1;

	sch = &port->root_qdisc;
	if (!sch)
		return -1;

	ret = pon_qos_get_queue_by_handle(dev, q_tc->handle, &qid);
	if (ret < 0 || !qid) {
		netdev_err(dev, "%s: handle not found err %d\n",
			   __func__, ret);
		return -1;
	}

	if (q_tc->indev) {
		ret = pon_qos_get_subif_idx(q_tc->indev, &tc);
		if (ret < 0)
			return -1;
		/* TODO: Force TC if PON DS */
	} else {
		tc = q_tc->tc;
	}

	netdev_dbg(dev, "port: %d deq_i: %d qid: %d handl: %#x tc: %d ds:%s\n",
		   sch->port, sch->deq_idx, qid->qid, q_tc->handle, tc,
		   sch->ds ? "true" : "false");

	/* TODO: port zero is currently the CPU port */
	if (!sch->port) {
		ret = pon_qos_config_cpu_port(qid, sch->port, tc, en);
	} else {
		/* TODO: Queue sharing case */
		ret = pon_qos_set_qmap(qid, sch->port, tc, en);
		if (ret != 0)
			netdev_err(dev, "%s: qmap set failed\n", __func__);

		if (!en)
			atomic_dec(&qid->ref_cnt);
		else
			atomic_inc(&qid->ref_cnt);
	}

	return ret;
}
EXPORT_SYMBOL(pon_qos_update_qmap);

int pon_qos_ports_cleanup(void)
{
	struct pon_qos_port *p, *n;
	int ret;

	list_for_each_entry_safe (p, n, &port_list, list) {
		ret = pon_qos_qdisc_tree_del(p, &p->root_qdisc);
		if (ret < 0) {
			pr_err("%s: error freeing port\n", __func__);
			return -1;
		}
		pon_qos_port_delete(p);
	}

	return 0;
}
