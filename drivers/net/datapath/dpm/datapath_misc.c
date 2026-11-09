// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) Intel Corporation
 * Author: Shao Guohua <guohua.shao@intel.com>
 */

#include <linux/types.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/datapath_api.h>
#include "datapath.h"
#if IS_ENABLED(CONFIG_PPA_API_SW_FASTPATH) || \
	IS_ENABLED(CONFIG_LTQ_PPA_API_SW_FASTPATH)
#include <net/ppa/ppa_api.h>
#endif

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_CPUFREQ)
#include <linux/cpufreq.h>
static int dp_coc_cpufreq_transition_notifier(struct notifier_block *nb,
					      unsigned long event, void *data);
static int dp_coc_cpufreq_policy_notifier(struct notifier_block *nb,
					  unsigned long event, void *data);
#endif

struct hlist_head dp_subif_list[DP_SUBIF_LIST_HASH_SIZE];
char *parser_flags_str[] = {
	"PARSER_FLAGS_NO",
	"PARSER_FLAGS_END",
	"PARSER_FLAGS_CAPWAP",
	"PARSER_FLAGS_GRE",
	"PARSER_FLAGS_LEN",
	"PARSER_FLAGS_GREK",
	"PARSER_FLAGS_NN1",
	"PARSER_FLAGS_NN2",

	"PARSER_FLAGS_ITAG",
	"PARSER_FLAGS_1VLAN",
	"PARSER_FLAGS_2VLAN",
	"PARSER_FLAGS_3VLAN",
	"PARSER_FLAGS_4VLAN",
	"PARSER_FLAGS_SNAP",
	"PARSER_FLAGS_PPPOES",
	"PARSER_FLAGS_1IPV4",

	"PARSER_FLAGS_1IPV6",
	"PARSER_FLAGS_2IPV4",
	"PARSER_FLAGS_2IPV6",
	"PARSER_FLAGS_ROUTEXP",
	"PARSER_FLAGS_TCP",
	"PARSER_FLAGS_1UDP",
	"PARSER_FLAGS_IGMP",
	"PARSER_FLAGS_IPV4OPT",

	"PARSER_FLAGS_IPV6EXT",
	"PARSER_FLAGS_TCPACK",
	"PARSER_FLAGS_IPFRAG",
	"PARSER_FLAGS_EAPOL",
	"PARSER_FLAGS_2IPV6EXT",
	"PARSER_FLAGS_2UDP",
	"PARSER_FLAGS_L2TPNEXP",
	"PARSER_FLAGS_LROEXP",

	"PARSER_FLAGS_L2TP",
	"PARSER_FLAGS_GRE_VLAN1",
	"PARSER_FLAGS_GRE_VLAN2",
	"PARSER_FLAGS_GRE_PPPOE",
	"PARSER_FLAGS_BYTE4_BIT4_UNDEF",
	"PARSER_FLAGS_BYTE4_BIT5_UNDEF",
	"PARSER_FLAGS_BYTE4_BIT6_UNDEF",
	"PARSER_FLAGS_BYTE4_BIT7_UNDEF",

	"PARSER_FLAGS_BYTE5_BIT0_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT1_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT2_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT3_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT4_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT5_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT6_UNDEF",
	"PARSER_FLAGS_BYTE5_BIT7_UNDEF",

	"PARSER_FLAGS_BYTE6_BIT0_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT1_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT2_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT3_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT4_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT5_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT6_UNDEF",
	"PARSER_FLAGS_BYTE6_BIT7_UNDEF",

	"PARSER_FLAGS_BYTE7_BIT0_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT1_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT2_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT3_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT4_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT5_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT6_UNDEF",
	"PARSER_FLAGS_BYTE7_BIT7_UNDEF",

	/*Must be put at the end of the enum */
	"PARSER_FLAGS_MAX"
};

void dump_parser_flag(char *buf)
{
	int i, j;
	unsigned char *pflags = buf + PKT_PMAC_OFFSET - 1;
	unsigned char *poffset = buf;
	char *p;
	int len;

	if (!buf) {
		pr_err("%s buf NULL\n", __func__);
		return;
	}
	p = devm_kzalloc(&g_dp_dev->dev, 2000, GFP_ATOMIC);
	if (!p)
		return;

	/* one TCP example: offset
	 * offset 0
	 *  00 3a 00 00 00 00 00 00 00 00 00 00 00 00 00 0e
	 * 00 00 00 16 22 00 00 00 00 00 00 00 00 00 00 2e
	 * 00 00 00 00 00 00 00 00
	 * flags: FLAG_L2TPFLAG_NO
	 * 00 00 00 00 80 18 80 00
	 */
	pr_info("paser flag at 0x%px: ", buf);
	len = 0;
	for (i = 0; i < 8; i++)
		len += sprintf(p + len, "%02x ", *(pflags - 7 + i));
	pr_info("%s\n", p);
#if 1
	pr_info("paser flag: ");
	len = 0;
	for (i = 0; i < 8; i++)
		len += sprintf(p + len, "%02x ", *(pflags - i));
	pr_info("%s(reverse)\n", p);
#endif

	for (i = 0; i < PASAR_FLAGS_NUM; i++) { /*8 flags per byte */
		for (j = 0; j < 8; j++) { /*8 bits per byte */
			if ((i * 8 + j) >= PASER_FLAGS_MAX)
				break;

			if ((*(pflags - i)) & (1 << j)) { /*flag is set */
				if ((i * 8 + j) < PASAR_OFFSETS_NUM)
					pr_info("  Flag %02d offset=%02d: %s\n",
						i * 8 + j,
						*(poffset + i * 8 + j),
						parser_flags_str[i * 8 + j]);
				else
					pr_info("  Flag %02d %s (No offset)\n",
						i * 8 + j,
						parser_flags_str[i * 8 + j]);
			}
		}
	}
	devm_kfree(&g_dp_dev->dev, p);
}

/*will be used at any context */
void dp_dump_raw_data(const void *buf, int len, char *prefix_str)
{
	int i, j, l;
	int line_num = 32;
	const unsigned char *p = buf;
	int bytes = line_num * 8 + 100;
	char *s;

	if (!p) {
		pr_err("dp_dump_raw_data: p NULL ?\n");
		return;
	}
	s = devm_kzalloc(&g_dp_dev->dev, bytes, GFP_ATOMIC);
	if (!s)
		return;
	sprintf(s, "%s in hex at 0x%px\n",
		prefix_str ? (char *)prefix_str : "Data", p);
	pr_info("%s", s);

	for (i = 0; i < len; i += line_num) {
		l = sprintf(s, " %06d: ", i);
		for (j = 0; (j < line_num) && (i + j < len); j++)
			l += sprintf(s + l, "%02x ", p[i + j]);
		sprintf(s + l, "\n");
		pr_info("%s", s);
	}
	devm_kfree(&g_dp_dev->dev, s);
}
EXPORT_SYMBOL(dp_dump_raw_data);

#define PROTOCOL_IPIP 4
#define PROTOCOL_TCP 6
#define PROTOCOL_UDP 17
#define PROTOCOL_ENCAPSULATED_IPV6 41
#define PROTOCOL_ROUTING 43
#define PROTOCOL_NONE 59
#define PROTOCOL_IPV6_FRAGMENT 44

#define TWO_MAC_SIZE 12
#define VLAN_HDR_SIZE 4
#define PPPOE_HDR_SIZE 8
#define IPV6_HDR_SIZE 40
#define IPV6_EXTENSION_SIZE 8

#define IP_CHKSUM_OFFSET_IPV4 10
#define UDP_CHKSUM_OFFSET 6
#define TCP_CHKSUM_OFFSET 16
/*Workaround: Currently need to includes PMAC
 *although spec said it starts from mac address. ?
 */
struct ip_hdr_info {
	u8 ip_ver;
	u8 proto; /*udp/tcp */
	u16 ip_offset; /*this offset is based on L2 MAC header */
	u16 udp_tcp_offset; /*this offset is based on ip header */
	u16 next_ip_hdr_offset; /*0 - means no next valid ip header.*/
	/* Based on current IP header */
	u8 is_fragment; /*0 means non fragmented packet */
};

/*input p: pointers to ip header
 * output info:
 * return: 0:  it is UDP/TCP packet
 * -1: not UDP/TCP
 */
#define DP_IP_VER4 4
#define DP_IP_VER6 6
int get_ip_hdr_info(u8 *pdata, int len, struct ip_hdr_info *info)
{
	int ip_hdr_size;
	u8 *p = pdata;
	struct iphdr *iphdr = (struct iphdr *)pdata;

	memset((void *)info, 0, sizeof(*info));
	info->ip_ver = p[0] >> 4;

	if (info->ip_ver == DP_IP_VER4) { /*ipv4 */
		ip_hdr_size = (p[0] & 0xf) << 2;
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
			 "IPV4 pkt with protocol 0x%x with ip hdr size %d\n",
			 p[9], ip_hdr_size);
#endif
		info->proto = p[9];

		if ((info->proto == PROTOCOL_UDP) ||
		    (info->proto == PROTOCOL_TCP)) {
			if ((iphdr->frag_off & IP_MF) ||
			    (iphdr->frag_off & IP_OFFSET)) {
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
				DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
					 "frag pkt:off=%x,IP_MF=%x,IP_OFFSET=%x\n",
					 iphdr->frag_off, IP_MF, IP_OFFSET);
#endif
				info->udp_tcp_offset = (p[0] & 0x0f) << 2;
				info->is_fragment = 1;
				return -1;
			}
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
			DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
				 "%s packet with src/dst port:%u/%u\n",
				 (p[9] ==
				  PROTOCOL_UDP) ?
					 "UDP" :
					 "TCP",
				 *(unsigned short *)(pdata +
						     ip_hdr_size),
				 *(unsigned short *)(pdata +
						     ip_hdr_size +
						     2));
#endif
			info->udp_tcp_offset = (p[0] & 0x0f) << 2;
			return 0;
		} else if (p[9] == PROTOCOL_ENCAPSULATED_IPV6) {
			/*6RD */
			info->next_ip_hdr_offset = (p[0] & 0x0f) << 2;
			return 0;
		}
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
			 "Not supported extension hdr:0x%x\n", p[9]);
#endif
		return -1;
	} else if (info->ip_ver == DP_IP_VER6) { /*ipv6 */
		int ip_hdr_size;
		u8 next_hdr;
		u8 udp_tcp_h_offset;
		u8 first_extension = 1;

		ip_hdr_size = IPV6_HDR_SIZE;
		udp_tcp_h_offset = IPV6_HDR_SIZE;
		next_hdr = p[6];
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
		if (dp_dbg_flag & DP_DBG_FLAG_DUMP_TX) {
			int i;

			pr_info("IPV6 packet with next hdr:0x%x\n", next_hdr);
			pr_info(" src IP: ");
			for (i = 0; i < 16; i++)
				pr_info("%02x%s", pdata[8 + i],
					(i != 15) ? ":" : " ");

			pr_info("\n");

			pr_info(" Dst IP: ");

			for (i = 0; i < 16; i++)
				pr_info("%02x%s", pdata[24 + i],
					(i != 15) ? ":" : " ");

			pr_info("\n");
		}
#endif
		while (1) {
			/*Next Header: UDP/TCP */
			if ((next_hdr == PROTOCOL_UDP) ||
			    (next_hdr == PROTOCOL_TCP)) {
				info->proto = next_hdr;

				if (!first_extension)
					udp_tcp_h_offset +=
						IPV6_EXTENSION_SIZE + p[1];

				info->udp_tcp_offset = udp_tcp_h_offset;
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
				DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
					 "IP6 UDP:src/dst port:%u/%u udp_tcp_off=%d\n",
					 *(unsigned short *)(pdata +
							     udp_tcp_h_offset),
					 *(unsigned short *)(pdata +
							     udp_tcp_h_offset + 2),
					 udp_tcp_h_offset);
#endif
				return 0;
			} else if (next_hdr == PROTOCOL_IPIP) { /*dslite */
				if (!first_extension)
					udp_tcp_h_offset +=
						IPV6_EXTENSION_SIZE + p[1];

				info->next_ip_hdr_offset = udp_tcp_h_offset;
				return 0;
			} else if (next_hdr == PROTOCOL_IPV6_FRAGMENT) {
				pr_info_once("fragmented IPV6 packet !\n");
				info->is_fragment = 1;
				return -1;
			}
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
			DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
				 "Skip extension hdr:0x%x\n", next_hdr);
#endif
			if ((next_hdr == PROTOCOL_NONE) ||
			    (next_hdr == PROTOCOL_ENCAPSULATED_IPV6))
				break;

			if (first_extension) {
				/*skip ip header */
				p += IPV6_HDR_SIZE;
				first_extension = 0;
			} else {
				/*TO NEXT */
				udp_tcp_h_offset +=
					IPV6_EXTENSION_SIZE + p[1];
				p += IPV6_EXTENSION_SIZE + p[1];
			}
			next_hdr = p[0];
			if (udp_tcp_h_offset > len) {
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
				DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
					 "\n- Wrong IPV6 packet header ?\n");
#endif
				break;
			}
		}
	}

	/*not supported protocol */
	return -1;
}

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_MANUAL_PARSE)
int ip_offset_hw_adjust = 8;

/*parse protol and get the ip_offset/tcp_h_offset and its type:
 * return: 0-found udp/tcp header, -1 - not found  udp/tcp header
 * Note: skb->data points to pmac header, not L2 MAC header;
 */
int get_chksum_offset(struct sk_buff *skb, u32 *ip_offset,
		      u32 *tcp_h_offset, u32 *tcp_type)
{
	u8 *p_l2_mac = skb->data;
	u8 *p = p_l2_mac + TWO_MAC_SIZE;
	struct ip_hdr_info pkt_info[2];
	u8 ip_num = 0;
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	int i;
#endif
	int len;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return -1;

	*ip_offset = 0;
	*tcp_h_offset = 0;

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
			 "flags DP_TX_CAL_CHKSUM is set\n");
#endif

	while ((p[0] == 0x81) && (p[1] == 0x00)) /*skip vlan header */
		p += VLAN_HDR_SIZE;

	if ((p[0] == 0x88) && (p[1] == 0x64)) /*skip pppoe header */
		p += PPPOE_HDR_SIZE;

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
	DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
		 "To find ip header:%02x %02x %02x %02x %02x %02x %02x %02x\n",
		 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
#endif
	if (((p[0] != 0x08) || (p[1] != 0x00)) &&
	    ((p[0] != 0x86) && (p[1] != 0xdd))) {
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG_PROTOCOL_PARSE)
		DP_DEBUG(DP_DBG_FLAG_DUMP_TX, "None IP type:%02x%02x\n", p[0],
			 p[1]);
#endif
	}

	p += 2; /* jump to ip header */
	len = skb->len - TWO_MAC_SIZE - 2;

	while (1) {
		if (get_ip_hdr_info(p, len, &pkt_info[ip_num]) == 0) {
			pkt_info[ip_num].ip_offset = (uintptr_t)p -
						     (unsigned long)p_l2_mac;

			if (pkt_info[ip_num].next_ip_hdr_offset) {
				p += pkt_info[ip_num].next_ip_hdr_offset;
				ip_num++;

				if (ip_num >= ARRAY_SIZE(pkt_info))
					return -1;

				len -= pkt_info[ip_num].next_ip_hdr_offset;
				continue;

			} else {
				ip_num++;

				if (ip_num >= ARRAY_SIZE(pkt_info))
					return -1;

				break;
			}
		} else {
			/*Not UDP/TCP and cannot do checksum calculation */
			pr_info_once("Not UDP/TCP and cannot do checksum cal!\n");
			return -1;
		}
	}
#if IS_ENABLED(CONFIG_INTEL_DATAPATH_DBG)
	if (dp_dbg_flag & DP_DBG_FLAG_DUMP_TX) {
		for (i = 0; i < ip_num; i++) {
			DP_DEBUG(DP_DBG_FLAG_DUMP_TX,
				 "Parse:ip[%d]:v=%d prot=%d ip_off=%d udp_tcp_off=%d, n_hdr_off=%d\n",
				 i, pkt_info[i].ip_ver, pkt_info[i].proto,
				 pkt_info[i].ip_offset,
				 pkt_info[i].udp_tcp_offset,
				 pkt_info[i].next_ip_hdr_offset);
		}
	}
#endif
	if (ip_num == 1) {
		if (pkt_info[0].ip_ver == DP_IP_VER4) {
			*ip_offset = pkt_info[0].ip_offset;
			*tcp_h_offset = pkt_info[0].udp_tcp_offset;

			if (pkt_info[0].proto == PROTOCOL_UDP)
				*tcp_type = UDP_OVER_IPV4;
			else
				*tcp_type = TCP_OVER_IPV4;

			if (pkt_info[0].is_fragment)
				return 1;

			return 0;
		} else if (pkt_info[0].ip_ver == DP_IP_VER6) {
			*ip_offset = pkt_info[0].ip_offset;
			*tcp_h_offset = pkt_info[0].udp_tcp_offset;

			if (pkt_info[0].proto == PROTOCOL_UDP) {
				*tcp_type = UDP_OVER_IPV6;
			} else {
				*tcp_type = TCP_OVER_IPV6;
				if (pkt_info[0].is_fragment)
					return 1;
			}

			return 0;
		}
	} else if (ip_num == 2) {
		/*for tunnels:current for 6rd/dslite only */
		if ((pkt_info[0].ip_ver == DP_IP_VER4) &&
		    (pkt_info[1].ip_ver == DP_IP_VER6)) {
			/*6rd */
			*ip_offset = pkt_info[0].ip_offset;
			*tcp_h_offset =
				(pkt_info[0].next_ip_hdr_offset +
				 pkt_info[1].udp_tcp_offset);

			if (pkt_info[1].proto == PROTOCOL_UDP)
				*tcp_type = UDP_OVER_IPV6_IPV4;
			else
				*tcp_type = TCP_OVER_IPV6_IPV4;

			if (pkt_info[0].is_fragment)
				return 1;

			return 0;

		} else if ((pkt_info[0].ip_ver == DP_IP_VER6) &&
			   (pkt_info[1].ip_ver == DP_IP_VER4)) { /*dslite */
			*ip_offset = pkt_info[0].ip_offset;
			*tcp_h_offset =
				(pkt_info[0].next_ip_hdr_offset +
				 pkt_info[1].udp_tcp_offset);

			if (pkt_info[1].proto == PROTOCOL_UDP)
				*tcp_type = UDP_OVER_IPV4_IPV6;
			else
				*tcp_type = TCP_OVER_IPV4_IPV6;

			if (pkt_info[0].is_fragment)
				return 1;

			return 0;
		}
	}

	return -1;
}
#else /* CONFIG_INTEL_DATAPATH_MANUAL_PARSE */
/*parse protol and get the ip_offset/tcp_h_offset and its type
 * based on skb_inner_network_header/skb_network_header/
 *           skb_inner_transport_header/skb_transport_header
 * return: 0-found udp/tcp header, -1 - not found  udp/tcp header
 *         -2 - checksum present already
 *  Note: skb->data points to pmac header, not L2 MAC header;
 */
int ip_offset_hw_adjust = 8;
int get_chksum_offset(struct sk_buff *skb, u32 *ip_offset,
		      u32 *tcp_h_offset, u32 *tcp_type)
{
	struct iphdr *inner, *outer;
	struct tcphdr *tcph;
	struct udphdr *udph;
	uint8_t ipproto;

	/*
	 * type field looks like this:
	 * bit 0: transport protocol
	 * bit 1: outer IP version
	 * bit 2: encap
	 */
	enum {
		type_encap_mask = TCP_OVER_IPV6_IPV4 ^ TCP_OVER_IPV4,
		type_network_mask = TCP_OVER_IPV6 ^ TCP_OVER_IPV4,
		type_transport_mask = UDP_OVER_IPV4 ^ TCP_OVER_IPV4,
	};

	if (skb->ip_summed != CHECKSUM_PARTIAL) {
		return -2;
	}

	*tcp_type = 0;

	/* inner == outer without encapsulation */
	if (!skb->encapsulation) {
		skb_reset_inner_headers(skb);
	}
	outer = (struct iphdr *)skb_network_header(skb);
	inner = (struct iphdr *)skb_inner_network_header(skb);

	if (skb->encapsulation) {
		/* only support IPv6 in IPv4 or vice-versa */
		if (outer->version == inner->version)
			return -1;
		/* set encapsulation bit */
		*tcp_type |= TCP_OVER_IPV6_IPV4 & type_encap_mask;
	}

	*ip_offset = skb_network_offset(skb);
	*tcp_h_offset =
		(uint32_t)(skb_inner_transport_header(skb) - skb_network_header(skb));

	/*
	 * set IP version and determine ipproto
	 */
	switch (inner->version) {
		uint16_t frag_off;
		int exthdr_start;

	case DP_IP_VER4:
		if (skb->encapsulation) {
			*tcp_type |= TCP_OVER_IPV6 & type_network_mask;
		} else {
			*tcp_type |= TCP_OVER_IPV4 & type_network_mask;
		}
		ipproto = inner->protocol;
		break;

	case DP_IP_VER6:
		if (skb->encapsulation)
			*tcp_type |= TCP_OVER_IPV4 & type_network_mask;
		else
			*tcp_type |= TCP_OVER_IPV6 & type_network_mask;

		/* skip to last nexthdr field for ipproto */
		ipproto = ((struct ipv6hdr *)inner)->nexthdr;
		exthdr_start = skb_inner_network_offset(skb) + sizeof(struct ipv6hdr);
		ipv6_skip_exthdr(skb, exthdr_start, &ipproto, &frag_off);

		break;

	default:
		pr_err("unknown IP version (%hhu)\n", inner->version);
		return -1;
	}

	/*
	 * clear transport csums and set transport protocol
	 */
	switch (ipproto) {
	case IPPROTO_TCP:
		tcph = (void *)skb_inner_transport_header(skb);
		*tcp_type |= TCP_OVER_IPV4 & type_transport_mask;
		break;
	case IPPROTO_UDP:
		udph = (void *)skb_inner_transport_header(skb);
		*tcp_type |= UDP_OVER_IPV4 & type_transport_mask;
		break;
	default:
		pr_err("unknown ipproto (%hu)\n", skb->protocol);
		return -1;
	}

	return 0;
}
#endif /* CONFIG_INTEL_DATAPATH_MANUAL_PARSE */

char *dp_skb_csum_str(struct sk_buff *skb)
{
	if (!skb)
		return "NULL";
	if (skb->ip_summed == CHECKSUM_PARTIAL)
		return "HW Checksum";
	if (skb->ip_summed == CHECKSUM_NONE)
		return "SW Checksum";
	return "Unknown";
}

int low_10dec(u64 x)
{
	char buf[26];
	char *p;
	int len;

	sprintf(buf, "%llu", x);
	len = strlen(buf);
	if (len >= 10)
		p = buf + len - 10;
	else
		p = buf;

	return dp_atoi(p);
}

int high_10dec(u64 x)
{
	char buf[26];
	int len;

	sprintf(buf, "%llu", x);
	len = strlen(buf);
	if (len >= 10)
		buf[len - 10] = 0;
	else
		buf[0] = 0;

	return dp_atoi(buf);
}

int dp_ingress_ctp_tc_map_set(struct dp_tc_cfg *tc, int flag)
{
	struct dp_meter_subif mtr_subif = { 0 };
	struct inst_info *dp_info;

	if (unlikely(dp_late_init() <= 0)) {
		pr_err("%s failed: datapath not initialized yet\n", __func__);
		return DP_FAILURE;
	}

	if (!tc) {
		pr_err("%s: tc_cfg is NULL\n", __func__);
		return DP_FAILURE;
	}

	if (dp_get_netif_subifid(tc->dev, NULL, NULL, NULL,
				 &mtr_subif.subif, 0)) {
		DP_DEBUG(DP_DBG_FLAG_DBG, "get subifid fail(%s)\n",
			 tc->dev ? tc->dev->name : "NULL");
		return DP_FAILURE;
	}
	mtr_subif.inst = mtr_subif.subif.inst;
	dp_info = get_dp_prop_info(mtr_subif.inst);

	if (!dp_info->dp_ctp_tc_map_set)
		return DP_FAILURE;
	return dp_info->dp_ctp_tc_map_set(tc, flag, &mtr_subif);
}
EXPORT_SYMBOL(dp_ingress_ctp_tc_map_set);

int dp_meter_alloc(int inst, int *meterid, int flag)
{
	struct inst_info *dp_info;

	if (unlikely(dp_late_init() <= 0)) {
		pr_err("%s failed: datapath not initialized yet\n", __func__);
		return DP_FAILURE;
	}

	if (!meterid || is_invalid_inst(inst))
		return DP_FAILURE;

	dp_info = get_dp_prop_info(inst);

	if (!dp_info->dp_meter_alloc)
		return DP_FAILURE;

	return dp_info->dp_meter_alloc(inst, meterid, flag);
}
EXPORT_SYMBOL(dp_meter_alloc);

int dp_meter_add(struct net_device *dev, struct dp_meter_cfg *meter, int flag)
{
	struct dp_meter_subif mtr_subif = { 0 };
	struct inst_info *dp_info;

	if (unlikely(dp_late_init() <= 0)) {
		pr_err("%s failed: datapath not initialized yet\n", __func__);
		return DP_FAILURE;
	}

	if (!dev || !meter) {
		pr_err("%s failed: dev or meter_cfg can not be NULL\n", __func__);
		return DP_FAILURE;
	}

	if ((flag & DP_METER_ATTACH_CTP) ||
	    (flag & DP_METER_ATTACH_BRPORT) ||
	    (flag & DP_METER_ATTACH_PCE)) {
		if (dp_get_netif_subifid(dev, NULL, NULL,
					 NULL, &mtr_subif.subif, 0)) {
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "get subifid fail:%s\n",
				 dev ? dev->name : "NULL");
			return DP_FAILURE;
		}
		mtr_subif.inst = mtr_subif.subif.inst;
	} else if (flag & DP_METER_ATTACH_BRIDGE) {
		mtr_subif.fid = dp_get_fid_by_brname(dev, &mtr_subif.inst);
		if (mtr_subif.fid < 0) {
			pr_err("fid less then 0\n");
			return DP_FAILURE;
		}
	} else {
		pr_err("Meter Flag not set\n");
		return DP_FAILURE;
	}

	dp_info = get_dp_prop_info(mtr_subif.inst);

	if (!dp_info->dp_meter_add)
		return DP_FAILURE;
	return dp_info->dp_meter_add(dev, meter, flag, &mtr_subif);
}
EXPORT_SYMBOL(dp_meter_add);

int dp_meter_del(struct net_device *dev, struct dp_meter_cfg *meter, int flag)
{
	struct dp_meter_subif mtr_subif = { 0 };
	struct inst_info *dp_info;

	if (unlikely(dp_late_init() <= 0)) {
		pr_err("%s failed: datapath not initialized yet\n", __func__);
		return DP_FAILURE;
	}

	if (!dev || !meter) {
		pr_err("%s failed: dev or meter_cfg can not be NULL\n", __func__);
		return DP_FAILURE;
	}

	if ((flag & DP_METER_ATTACH_CTP) ||
	    (flag & DP_METER_ATTACH_BRPORT) ||
	    (flag & DP_METER_ATTACH_PCE)) {
		if (dp_get_netif_subifid(dev, NULL, NULL,
					 NULL, &mtr_subif.subif, 0)) {
			DP_DEBUG(DP_DBG_FLAG_DBG,
				 "get subifid fail:%s\n",
				 dev ? dev->name : "NULL");
			return DP_FAILURE;
		}
		mtr_subif.inst = mtr_subif.subif.inst;
	} else if (flag & DP_METER_ATTACH_BRIDGE) {
		mtr_subif.fid = dp_get_fid_by_brname(dev, &mtr_subif.inst);
		if (mtr_subif.fid < 0) {
			pr_err("fid less then 0\n");
			return DP_FAILURE;
		}
	} else {
		pr_err("Meter Flag not set\n");
		return DP_FAILURE;
	}

	dp_info = get_dp_prop_info(mtr_subif.inst);
	if (!dp_info->dp_meter_del)
		return DP_FAILURE;
	return dp_info->dp_meter_del(dev, meter, flag, &mtr_subif);
}
EXPORT_SYMBOL(dp_meter_del);

#if (!IS_ENABLED(CONFIG_INTEL_DATAPATH_SWITCHDEV))
int dp_get_fid_by_brname(struct net_device *dev, int *inst)
{
	pr_err("API not support when SWDEV disabled\n");
	return -1;
}
#endif

void dp_subif_reclaim(struct rcu_head *rp)
{
	struct dp_subif_cache *dp_subif =
		container_of(rp, struct dp_subif_cache, rcu);

	devm_kfree(&g_dp_dev->dev, dp_subif);
}

u32 dp_subif_hash(struct net_device *dev)
{
	unsigned long index;

	index = (unsigned long)dev;
	/*Note: it is 4K alignment. Need tune later */
	return (u32)((index >>
		      DP_SUBIF_LIST_HASH_SHIFT) %
		     DP_SUBIF_LIST_HASH_SIZE);
}

int dp_subif_list_init(void)
{
	int i;

	for (i = 0; i < DP_SUBIF_LIST_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&dp_subif_list[i]);

	return 0;
}

struct dp_subif_cache *dp_subif_lookup_safe(struct hlist_head *head,
					    struct net_device *dev, void *data)
{
	struct dp_subif_cache *item;
	struct hlist_node *n;

	hlist_for_each_entry_safe (item, n, head, hlist) {
		if (dev) {
			if (item->dev == dev)
				return item;
		}
	}
	return NULL;
}

int32_t dp_del_subif(struct net_device *netif, void *data, dp_subif_t *subif,
		     char *subif_name, u32 flags)
{
	struct dp_subif_cache *dp_subif;
	u32 idx;

	idx = dp_subif_hash(netif);
	dp_subif = dp_subif_lookup_safe(&dp_subif_list[idx], netif, data);
	if (!dp_subif) {
		pr_err("Failed dp_subif_lookup: %s\n",
		       netif ? netif->name : "NULL");
		return -1;
	}
	hlist_del_rcu(&dp_subif->hlist);
	call_rcu_bh(&dp_subif->rcu, dp_subif_reclaim);
	return 1;
}

int32_t dp_update_subif(struct net_device *netif, void *data,
			dp_subif_t *subif, char *subif_name, u32 flags,
			dp_get_netif_subifid_fn_t subifid_fn_t)
{
	struct dp_subif_cache *dp_subif_new, *dp_subif;
	u32 idx;

	idx = dp_subif_hash(netif);
	dp_subif = dp_subif_lookup_safe(&dp_subif_list[idx], netif, data);
	if (!dp_subif) { /*alloc new */
		dp_subif = devm_kzalloc(&g_dp_dev->dev, sizeof(*dp_subif),
					GFP_ATOMIC);
		if (!dp_subif)
			return -1;
		memcpy(&dp_subif->subif, subif, sizeof(dp_subif_t));
		dp_subif->data = (u8 *)data;
		dp_subif->dev = netif;
		if (subif_name)
			strncpy(dp_subif->name, subif_name,
				sizeof(dp_subif->name) - 1);
		dp_subif->subif_fn = subifid_fn_t;
		hlist_add_head_rcu(&dp_subif->hlist, &dp_subif_list[idx]);
	} else {
		dp_subif_new = devm_kzalloc(&g_dp_dev->dev, sizeof(*dp_subif),
					    GFP_ATOMIC);
		if (!dp_subif_new)
			return -1;
		memcpy(&dp_subif_new->subif, subif, sizeof(dp_subif_t));
		dp_subif_new->data = (u8 *)data;
		dp_subif_new->dev = netif;
		if (subif_name)
			strncpy(dp_subif_new->name, subif_name,
				sizeof(dp_subif->name) - 1);
		dp_subif_new->subif_fn = subifid_fn_t;
		hlist_replace_rcu(&dp_subif->hlist,
				  &dp_subif_new->hlist);
		call_rcu_bh(&dp_subif->rcu, dp_subif_reclaim);
	}
	return 0;
}

int32_t dp_sync_subifid(struct net_device *dev, char *subif_name,
			dp_subif_t *subif_id, struct dp_subif_data *data,
			u32 flags, int *f_subif_up)
{
	void *subif_data = NULL;
	struct pmac_port_info *port;

	/* Note: workaround to set dummy subif_data via subif_name for DSL case.
	 *       During dp_get_netif_subifID, subif_data is used to get its PVC
	 *       information.
	 * Later VRX518/618 need to provide valid subif_data in order to support
	 * multiple DSL instances during dp_register_subif_ext
	 */
	port = get_dp_port_info(0, subif_id->port_id);
	if (!port)
		return DP_FAILURE;
	if (is_dsl(port) && !dev)
		subif_data = (void *)subif_name;
	/*check flag for register / deregister to update/del */
	if (flags & DP_F_DEREGISTER) {
		/* subif info not required for data->ctp_dev */
		if (dp_get_netif_subifid_priv(dev, NULL, subif_data, NULL,
					      &subif_id[0], 0))
			*f_subif_up = 0;
		else
			*f_subif_up = 1;
	} else {
		if (dp_get_netif_subifid_priv(dev, NULL, subif_data,
					      NULL, &subif_id[0], 0)) {
			pr_err("DP subif synchronization fail\n");
			return DP_FAILURE;
		}
		if (data && data->ctp_dev) {
			if (dp_get_netif_subifid_priv(data->ctp_dev, NULL,
						      subif_data, NULL,
						      &subif_id[1], 0))
				return DP_FAILURE;
		}
		*f_subif_up = 1;
	}
	return 0;
}

int32_t dp_sync_subifid_priv(struct net_device *dev, char *subif_name,
			     dp_subif_t *subif_id, struct dp_subif_data *data,
			     u32 flags, dp_get_netif_subifid_fn_t subifid_fn,
			     int *f_subif_up)
{
	void *subif_data = NULL;
	struct pmac_port_info *port;

	/* Note: workaround to set dummy subif_data via subif_name for DSL case.
	 *       During dp_get_netif_subifID, subif_data is used to get its PVC
	 *       information.
	 * Later VRX518/618 need to provide valid subif_data in order to support
	 * multiple DSL instances during dp_register_subif_ext
	 */
	port = get_dp_port_info(0, subif_id->port_id);
	if (!port)
		return DP_FAILURE;
	if (is_dsl(port) && !dev)
		subif_data = (void *)subif_name;
	/*check flag for register / deregister to update/del */
	if (flags & DP_F_DEREGISTER) {
		if (data && data->ctp_dev)
			dp_del_subif(data->ctp_dev, subif_data, &subif_id[1],
				     NULL, flags);

		if (*f_subif_up == 0)
			dp_del_subif(dev, subif_data, &subif_id[0], subif_name,
				     flags);
		else if (*f_subif_up == 1)
			dp_update_subif(dev, subif_data, &subif_id[0],
					subif_name, flags, subifid_fn);
	} else {
		if (*f_subif_up == 1) {
			dp_update_subif(dev, subif_data, &subif_id[0],
					subif_name, flags, subifid_fn);
			if (data && data->ctp_dev)
				dp_update_subif(data->ctp_dev, subif_data,
						&subif_id[1], NULL, flags,
						subifid_fn);
		}
	}
	return 0;
}

#if IS_ENABLED(CONFIG_INTEL_DATAPATH_CPUFREQ)
static int dp_coc_cpufreq_policy_notifier(struct notifier_block *nb,
					  unsigned long event, void *data)
{
	int inst = 0;
	struct cpufreq_policy *policy = data;
	struct inst_info *dp_info = get_dp_prop_info(inst);

	DP_DEBUG(DP_DBG_FLAG_COC, "%s; cpu=%d\n",
		 event ? "CPUFREQ_NOTIFY" : "CPUFREQ_ADJUST",
		 policy->cpu);
	if (event != CPUFREQ_ADJUST) {
		DP_DEBUG(DP_DBG_FLAG_COC, "policy (min, max, cur):%u, %u, %u\n",
			 policy->min, policy->max, policy->cur);
		return NOTIFY_DONE;
	}
	return dp_info->dp_handle_cpufreq_event(POLICY_NOTIFY, policy);
}

/* keep track of frequency transitions */
static int dp_coc_cpufreq_transition_notifier(struct notifier_block *nb,
					      unsigned long event, void *data)
{
	int inst = 0;
	struct cpufreq_freqs *freq = data;
	struct inst_info *dp_info = get_dp_prop_info(inst);

	if (event == CPUFREQ_PRECHANGE)
		return dp_info->dp_handle_cpufreq_event(PRE_CHANGE, freq);
	else if (event == CPUFREQ_POSTCHANGE)
		return dp_info->dp_handle_cpufreq_event(POST_CHANGE, freq);

	return NOTIFY_OK;
}

static struct notifier_block dp_coc_cpufreq_transition_notifier_block = {
	.notifier_call = dp_coc_cpufreq_transition_notifier
};

static struct notifier_block dp_coc_cpufreq_policy_notifier_block = {
	.notifier_call = dp_coc_cpufreq_policy_notifier
};

int dp_cpufreq_notify_init(int inst)
{
	if (cpufreq_register_notifier(&dp_coc_cpufreq_transition_notifier_block,
				      CPUFREQ_TRANSITION_NOTIFIER)) {
		pr_err("cpufreq transiiton register_notifier failed?\n");
		return -1;
	}
	if (cpufreq_register_notifier(&dp_coc_cpufreq_policy_notifier_block,
				      CPUFREQ_POLICY_NOTIFIER)) {
		pr_err("cpufreq policy register_notifier failed?\n");
		return -1;
	}
	return 0;
}

int dp_cpufreq_notify_exit(void)
{
	if (cpufreq_unregister_notifier(&dp_coc_cpufreq_transition_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER)) {
		pr_err("cpufreq transition unregister_notifier failed?\n");
		return -1;
	}
	if (cpufreq_unregister_notifier(&dp_coc_cpufreq_policy_notifier_block,
					CPUFREQ_POLICY_NOTIFIER)) {
		pr_err("cpufreq policy unregister_notifier failed?\n");
		return -1;
	}
	return 0;
}
#endif
/**
 * get_dma_chan_idx - Get available dma chan index from dp_dma_chan_tbl.
 * @inst: DP instance.
 * @num_dma_chan: Number of DMA channels.
 * Description: Find free dma channel index from dp_dma_chan_tbl.
 * Return: Base idx on success DP_FAILURE on failure.
 */
u32 get_dma_chan_idx(int inst, int num_dma_chan)
{
	u32 base, match;

	if (!num_dma_chan)
		return DP_FAILURE;
	if (!dp_dma_chan_tbl[inst]) {
		pr_err("dp_dma_chan_tbl[%d] NULL !!\n", inst);
		return DP_FAILURE;
	}

	for (base = 0; base < DP_MAX_DMA_CHAN; base++) {
		for (match = 0; (match < num_dma_chan) &&
				((base + match) < DP_MAX_DMA_CHAN);
		     match++) {
			if (atomic_read(&(dp_dma_chan_tbl[inst] +
					  (base + match))
						 ->ref_cnt))
				break;
		}
		if (match == num_dma_chan)
			return base;
	}
	pr_err("No free chan available from chan table!!\n");
	return DP_FAILURE;
}

/**
 * alloc_dma_chan_tbl: Dynamic allocation of dp_dma_chan_tbl.
 * @inst: DP instance.
 * Return: DP_SUCCESS on success DP_FAILURE on failure.
 */
u32 alloc_dma_chan_tbl(int inst)
{
	dp_dma_chan_tbl[inst] = devm_kzalloc(&g_dp_dev->dev,
					     (sizeof(struct dma_chan_info) *
					      DP_MAX_DMA_CHAN),
					     GFP_ATOMIC);
	if (!dp_dma_chan_tbl[inst])
		return DP_FAILURE;
	return DP_SUCCESS;
}

/**
 * alloc_dp_port_info: Dynamic allocation of alloc_dp_port_info.
 * @inst: DP instance.
 * Return: DP_SUCCESS on success DP_FAILURE on failure.
 */
u32 alloc_dp_port_subif_info(int inst)
{
	int port_id;
	int max_dp_ports; /* max dp ports */
	int max_subif; /* max subif per port */
	struct inst_info *info = NULL;

	if (is_invalid_inst(inst))
		return DP_FAILURE;

	/* Retrieve the hw capabilities */
	info = get_dp_prop_info(inst);
	max_dp_ports = info->cap.max_num_dp_ports;
	max_subif = info->cap.max_num_subif_per_port;

	dp_port_info[inst] = devm_kzalloc(&g_dp_dev->dev,
					  (sizeof(struct pmac_port_info) *
					   max_dp_ports),
					  GFP_ATOMIC);
	if (!dp_port_info[inst])
		return DP_FAILURE;
	for (port_id = 0; port_id < max_dp_ports; port_id++) {
		get_dp_port_info(inst, port_id)->subif_info =
			devm_kzalloc(&g_dp_dev->dev,
				     sizeof(struct dp_subif_info) * max_subif,
				     GFP_ATOMIC);
		if (!get_dp_port_info(inst, port_id)->subif_info) {
			pr_err("Failed for kmalloc: %zu bytes\n",
			       max_subif * sizeof(struct dp_subif_info));
			while (--port_id >= 0)
				devm_kfree(&g_dp_dev->dev,
					   get_dp_port_info(inst,
							    port_id)
						   ->subif_info);
			return DP_FAILURE;
		}
	}
	return DP_SUCCESS;
}

/**
 * free_dma_chan_tbl: Free dp_dma_chan_tbl.
 * @inst: DP instance.
 */
void free_dma_chan_tbl(int inst)
{
	/* free dma chan tbl */
	devm_kfree(&g_dp_dev->dev, dp_dma_chan_tbl[inst]);
}

/**
 * free_dp_port_subif_info: free port subif info.
 * @inst: DP instance.
 */
void free_dp_port_subif_info(int inst)
{
	int port_id;
	int max_dp_ports;
	struct pmac_port_info *port_info;
	struct inst_info *info = NULL;

	/* Retrieve the hw capabilities */
	info = get_dp_prop_info(inst);
	max_dp_ports = info->cap.max_num_dp_ports;

	if (dp_port_info[inst]) {
		for (port_id = 0; port_id < max_dp_ports; port_id++) {
			port_info = get_dp_port_info(inst, port_id);
			devm_kfree(&g_dp_dev->dev, port_info->subif_info);
		}
		devm_kfree(&g_dp_dev->dev, dp_port_info[inst]);
	}
}
