/*
 * Packet Accelerator Fragmentation
 *
 * SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
 *
 * vim:set noexpandtab shiftwidth=8 textwidth=100:
 *
 * Copyright (c) 2011-2020 AVM GmbH <info@avm.de>
 * All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/byteorder/generic.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#include "avm_pa.h"
#include "avm_pa_intern.h"

static inline u16 calc_frag_size(u16 mtu, u16 len)
{
	u16 frag_num = len/mtu;
	u16 frag_size;

	if (len % mtu)
		frag_num++;

	frag_size = len / frag_num;
	if (frag_size & 7) { /* mod 8 */
		if (frag_num > 1
		   && (((frag_num - 1)*(frag_size & 7) + frag_size) > mtu)) {
			frag_num++;
			frag_size = len / frag_num;
		}
	}
	frag_size = frag_size & ~7; /* multiple of 8 */
	return frag_size;
}

/* Some options are not allowed in IP fragments, replace those with
 * NOP. This avoids resizing the IP header.
 *
 * Can't call Linux ip_options_fragment(), that depends on skb->cb.
 */
static void zero_fragment_options(struct iphdr *iph)
{
	unsigned char *s = (unsigned char *) iph;
	unsigned char *p = (unsigned char *) (iph + 1);
	unsigned char *e = s + PA_IPHLEN(iph);
	unsigned char olen;

	while (p < e) {
		if (*p == IPOPT_EOL) {
			return;
		} else if (*p == IPOPT_NOP) {
			p++;
		} else {
			olen = p[1];
			if (olen < 2 || p+olen > e)
				return;
			if (!IPOPT_COPIED(*p))
				memset(p, IPOPT_NOP, olen);
			p += olen;
		}
	}
}


static inline struct sk_buff *
pa_alloc_fragment(struct sk_buff *src, size_t len, size_t hroom)
{
	struct sk_buff *skb;
	/* Remember: src->data points to the network header, and so does the new
	 * skb->data. The ethernet header is part of src's headroom must be set
	 * separately.
	 */
	skb = alloc_skb(hroom + len, GFP_ATOMIC);
	if (skb) {
		skb_reserve(skb, hroom);
		skb_reset_network_header(skb);
		skb_put(skb, len);
	}
	return skb;
}

static struct sk_buff *pa_fragment_ipv4(struct sk_buff *skb, int omtu)
{
	u16 offset, iphlen, len, left, mf, frag_size = 0;
	unsigned char *start, *data;
	struct iphdr *iph;
	struct sk_buff **pnext;
	int hroom, mtu;

	iph = ip_hdr(skb);
	iphlen = (u16) PA_IPHLEN(iph);

	/* ensure that fragments have their ip header aligned */
	hroom = ALIGN(skb_headroom(skb), 4);

	offset = (u16)((ntohs(iph->frag_off) & IP_OFFSET) << 3);
	mf = (u16)(iph->frag_off & htons(IP_MF));

	/* set mtu to multiple of 8 */
	mtu = ALIGN_DOWN(omtu - iphlen, 8);
	frag_size = calc_frag_size(mtu, skb->len - iphlen);

	/* the first skb is re-used and is the head of the queue */
	start = skb->data + iphlen;
	data  = start + frag_size;
	left  = skb->len - iphlen - frag_size;
	skb->len = iphlen + frag_size;
	iph->tot_len = htons((u16) skb->len);
	iph->frag_off |= htons(IP_MF);
	/* todo: xor */
	ip_send_check(iph);

	pnext = &skb->next;
	while (left > 0) {
		struct iphdr *niph;
		struct sk_buff *nskb;

		len = (left > mtu) ? frag_size : left;
		nskb = pa_alloc_fragment(skb, iphlen + len, hroom);
		if (!nskb)
			goto err;

		memcpy(nskb->data, iph, iphlen);
		memcpy(nskb->data + iphlen, data, len);
		niph = ip_hdr(nskb);
		niph->frag_off = htons((offset + (data - start)) >> 3);
		if (left > len || mf)
			niph->frag_off |= htons(IP_MF);
		zero_fragment_options(niph);
		data += len;
		left -= len;
		niph->tot_len = htons(len + iphlen);
		ip_send_check(niph);

		*pnext = nskb;
		pnext = &nskb->next;
	}
	return skb;

err:
	kfree_skb_list(skb);
	return NULL;
}


struct sk_buff *pa_fragment_ipv6(struct sk_buff *skb, int omtu)
{
	u16 hlen, len, frag_size;
	struct ipv6hdr *ipv6h;
	struct ipv6fraghdr *fragh;
	unsigned char *p, *start, *data;
	u32 id;
	int hroom, mtu, left;
	struct sk_buff **pnext;

	/* make room for the fragment header */
	p = skb_push(skb, sizeof(struct ipv6fraghdr));
	memmove(p, p + sizeof(struct ipv6fraghdr), sizeof(struct ipv6hdr));

	ipv6h = (struct ipv6hdr *) p;
	fragh = (struct ipv6fraghdr *) (ipv6h + 1);
	hlen = sizeof(struct ipv6hdr) + sizeof(struct ipv6fraghdr);
	skb_reset_network_header(skb);
	skb_set_transport_header(skb, hlen);

	hroom = ALIGN(skb_headroom(skb), 4);

	/* set mtu to multiple of 8 */
	mtu = ALIGN_DOWN(omtu - hlen, 8);
	frag_size = calc_frag_size(mtu, skb->len - hlen);
	get_random_bytes(&id, sizeof(id));

	/* the first skb is re-used and is the head of the queue */
	start = skb->data + hlen;
	data  = start + frag_size;
	left  = skb->len - hlen - frag_size;
	skb->len = hlen + frag_size;
	/* fixup first ipv6 header */
	fragh->reserved = 0;
	fragh->nexthdr = ipv6h->nexthdr;
	fragh->frag_off = htons(IP6_MF);
	fragh->identification = id;
	ipv6h->nexthdr = IPPROTO_FRAGMENT;
	ipv6h->payload_len = htons(frag_size + sizeof(struct ipv6fraghdr));

	pnext = &skb->next;
	while (left > 0) {
		struct ipv6hdr *nipv6h;
		struct ipv6fraghdr *nfragh;
		struct sk_buff *nskb;

		len = (left > mtu) ? frag_size : left;
		nskb = pa_alloc_fragment(skb, hlen + len, hroom);
		if (!nskb)
			goto err;
		memcpy(nskb->data, ipv6h, hlen);
		memcpy(nskb->data + hlen, data, len);
		nipv6h = (struct ipv6hdr *) nskb->data;
		nfragh = (struct ipv6fraghdr *) (nipv6h + 1);
		nfragh->frag_off = htons((u16) (data - start));
		if (left > len)
			nfragh->frag_off |= htons(IP6_MF);
		data += len;
		left -= len;
		nipv6h->payload_len = htons(len + sizeof(struct ipv6fraghdr));

		*pnext = nskb;
		pnext = &nskb->next;
	}

	return skb;

err:
	kfree_skb_list(skb);
	return NULL;
}

static inline u16 get_outer_ipversion(struct avm_pa_egress *egress)
{
	u16 version;

	version = AVM_PA_PKTTYPE_IPENCAP_VERSION(egress->match.pkttype);
	if (version == 0)
		version = AVM_PA_PKTTYPE_IP_VERSION(egress->match.pkttype);
	return version;
}

static int prepare_skb(struct sk_buff *skb)
{
	if (skb_linearize_cow(skb) < 0)
		return -ENOMEM;

	/* We're going to create fragments, this cancels GSO.
	 *
	 * It would probably be better to not fragment and let GSO handle it,
	 * which possibly creates (TCP) segments instead of IP fragments.
	 *
	 * However we lack a proper mechanism to detect GSO support on egress
	 * for any type of encapsulation.
	 *
	 * On the other hand, we're called only if fragmentation is permitted. Most
	 * of the time this implies UDP, and here GSO will just create IP fragments
	 * as well including software checksumming. So for UDP I strongly believe this
	 * code performs better because we're more specialized.
	 */
	skb_shinfo(skb)->gso_size = 0;
	if (skb->ip_summed == CHECKSUM_PARTIAL)
		return skb_checksum_help(skb);

	return 0;
}

struct sk_buff *avm_pa_fragment(struct avm_pa_egress *egress, struct sk_buff *skb)
{
	u16 total_len, ip_version;

	ip_version = get_outer_ipversion(egress);

	/* skb->protocol may not be set yet */
	if (ip_version == 4) {
		struct iphdr *iph = ip_hdr(skb);

		total_len = ntohs(iph->tot_len);
		if (total_len > egress->mtu) {
			if (prepare_skb(skb))
				goto err;

			return pa_fragment_ipv4(skb, egress->mtu);
		}
	} else if (ip_version == 6) {
		struct ipv6hdr *ipv6h = ipv6_hdr(skb);

		total_len = sizeof(*ipv6h) + ntohs(ipv6h->payload_len);
		if (total_len > egress->mtu) {
			if (prepare_skb(skb))
				goto err;

			return pa_fragment_ipv6(skb, egress->mtu);
		}
	}

	return skb;

err:
	kfree_skb(skb);
	return NULL;
}
