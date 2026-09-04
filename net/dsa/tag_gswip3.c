// SPDX-License-Identifier: GPL-2.0
/*
 * Intel / Lantiq GSWIP 3.0 PMAC tag support
 *
 * Copyright (C) 2026 Grische <github@grische.xyz>
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/unaligned.h>
#include <net/dsa.h>

#include "tag.h"

#define GSWIP3_NAME			"gswip3"

/* The header is eight bytes long in both directions and is prepended before
 * the destination MAC address, with no EtherType marker to find it by.
 */
#define GSWIP3_HEADER_LEN		8

/* Byte 2 */
#define GSWIP3_SPPID			GENMASK(7, 4)	/* source port */
#define  GSWIP3_SPPID_CPU		0
#define GSWIP3_RX_CLASS			GENMASK(3, 0)

/* Byte 3, transmit only */
#define GSWIP3_TX_PORT_MAP_EN		BIT(7)
#define GSWIP3_TX_TIME_DIS		BIT(5)
#define GSWIP3_TX_CLASS_EN		BIT(4)
#define GSWIP3_TX_PKT_TYPE		GENMASK(1, 0)

/* Byte 4, transmit only. The rest of bytes 4 and 5 is a 13-bit
 * sub-interface id, which addresses a logical sub-port of the source port
 * and has no meaning for the flat port model this tag serves.
 */
#define GSWIP3_TX_FCS_INS_DIS		BIT(7)
#define GSWIP3_TX_REDIRECT		BIT(6)
#define GSWIP3_TX_TIME_STMP		BIT(5)

static struct sk_buff *gswip3_tag_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct dsa_port *dp = dsa_user_to_port(dev);
	u8 *gswip3_tag;

	/* A conduit that reaches the switch ports through more than one
	 * hardware queue selects between them by user port index.
	 */
	skb_set_queue_mapping(skb, dp->index);

	skb_push(skb, GSWIP3_HEADER_LEN);
	gswip3_tag = skb->data;

	/* Bytes 0 and 1 hold the offsets the checksum engine inserts
	 * at, under the bit in byte 0 that asks for the insertion. All
	 * of it stays clear.
	 */
	gswip3_tag[0] = 0;
	gswip3_tag[1] = 0;
	gswip3_tag[2] = FIELD_PREP(GSWIP3_SPPID, GSWIP3_SPPID_CPU);
	gswip3_tag[3] = GSWIP3_TX_PORT_MAP_EN | GSWIP3_TX_CLASS_EN;
	/* GSWIP3_TX_REDIRECT, which diverts the frame into the packet
	 * accelerator instead of straight to the egress port, stays clear.
	 */
	gswip3_tag[4] = 0;
	gswip3_tag[5] = 0;

	/* Destination port map, ports 0 to 7 in byte 7 and 8 to 15 in byte 6 */
	put_unaligned_be16(dsa_xmit_port_mask(skb, dev), &gswip3_tag[6]);

	return skb;
}

static struct sk_buff *gswip3_tag_rcv(struct sk_buff *skb,
				      struct net_device *dev)
{
	u8 *gswip3_tag;
	int port;

	if (unlikely(!pskb_may_pull(skb, GSWIP3_HEADER_LEN)))
		return NULL;

	/* eth_type_trans() has already consumed the eight tag bytes together
	 * with the first six bytes of the destination MAC address.
	 */
	gswip3_tag = skb->data - ETH_HLEN;

	/* Get source port information */
	port = FIELD_GET(GSWIP3_SPPID, gswip3_tag[2]);
	skb->dev = dsa_conduit_find_user(dev, 0, port);
	if (!skb->dev)
		return NULL;

	/* remove GSWIP tag */
	skb_pull_rcsum(skb, GSWIP3_HEADER_LEN);

	/* offload_fwd_mark stays clear, because the received tag holds
	 * nothing to set it from. The source-port field read above is
	 * the only forwarding information the tag carries, and it
	 * records where the frame came from rather than what the switch
	 * decided about the rest of the fabric. There is therefore no
	 * condition to set the mark on, and the bridge forwards in
	 * software.
	 */

	return skb;
}

static const struct dsa_device_ops gswip3_netdev_ops = {
	.name = GSWIP3_NAME,
	.proto = DSA_TAG_PROTO_GSWIP3,
	.xmit = gswip3_tag_xmit,
	.rcv = gswip3_tag_rcv,
	.needed_headroom = GSWIP3_HEADER_LEN,
	/* The tag displaces the destination MAC address by its own length,
	 * so the conduit cannot filter received frames on it.
	 */
	.promisc_on_conduit = true,
};

MODULE_DESCRIPTION("DSA tag driver for Lantiq / Intel GSWIP 3.0 switches");
MODULE_LICENSE("GPL");
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_GSWIP3, GSWIP3_NAME);

module_dsa_tag_driver(gswip3_netdev_ops);
