// SPDX-License-Identifier: GPL-2.0-only
/*
 * Conduit network device for the xRX500 central buffer manager.
 *
 * Derived from the AVM FRITZ!Box 7560 GPL release (Linux 4.9.198),
 * drivers/net/ethernet/lantiq/cqm/grx500/cbm.c, cqm/grx500/cbm_config.c,
 * tmu/drv_tmu_ll.c and drivers/dma/intel/hdma.c.
 *
 * The transmit path on this silicon is a CPU enqueue into the buffer manager,
 * not a private descriptor ring. A transmit DMA channel is hard-bound to one
 * buffer-manager dequeue port and its descriptor base must point at that
 * port's descriptor window inside the manager, so the channel cannot be given
 * a ring in DRAM. Frame length and the transfer request travel out of band
 * over the manager-to-DMA handshake rather than in the descriptor, and no
 * completion interrupt exists on such a channel. Transmitting therefore means:
 * take a segment out of the free-segment queue manager, copy the frame into it
 * at the fixed data offset, write one enqueue descriptor, and let the hardware
 * recycle the segment after egress.
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/build_bug.h>
#include <linux/cleanup.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <asm/barrier.h>
#include <asm/io.h>

/*
 * Channel management of the receive DMA controller is still owned by the
 * driver that also runs the existing datapath, because a per-channel access
 * on that controller is a select-then-touch pair whose lock lives there and
 * because its interrupt line is claimed there. The receive path below drives
 * its channel through that interface for as long as both drivers are loaded;
 * the calls become local register writes once the other driver goes away.
 */
#include "dma/lantiq_dmax.h"

#define XRX500_CBM_DRV_NAME "xrx500-cbm"

/*
 * Data offset inside a buffer-manager segment.
 *
 * AVM cqm/cqm_common.h:18 fixes 128 bytes of DMA data offset; the vendor adds
 * the network stack's own alignment and pad on top (AVM cqm/grx500/cbm.h:189).
 * NET_SKB_PAD is not an ABI constant, so the composed offset is asserted at
 * build time rather than assumed.
 */
#define XRX500_CBM_DMA_DATA_OFFSET	128
#define XRX500_CBM_TX_OFFSET \
	(XRX500_CBM_DMA_DATA_OFFSET + NET_IP_ALIGN + NET_SKB_PAD)

/* Segment geometry of the standard pool (AVM cqm/grx500/cbm.c:1095-1139). */
#define XRX500_CBM_STD_SEG_SIZE		2048U
#define XRX500_CBM_JBO_SEG_SIZE		8192U

/*
 * The switch enforces a 1500 byte payload ceiling on a front-panel port;
 * raising the MAC frame-length limit does not move it. The conduit carries
 * that plus the switch header the tagging driver prepends, which is what the
 * switch layer sets its own ceiling from. Neither is what the segment
 * arithmetic would allow.
 */
#define XRX500_CBM_MAX_MTU		(ETH_DATA_LEN + PMAC_TX_HDR_LEN)

/* Buffer-manager register windows */

/*
 * Control window (AVM cqm/grx500/reg/cbm.h:2617-2660).
 *   0x200 standard-pool base, physical
 *   0x204 standard-pool base, coherent alias
 *   0x208 jumbo-pool base, physical
 *   0x20C jumbo-pool base, coherent alias
 *   0x210 control; bit 17 selects the jumbo segment size
 */
#define CBM_SBA_0			0x200
#define CBM_SBA_1			0x204
#define CBM_JBA_0			0x208
#define CBM_JBA_1			0x20C
#define CBM_CTRL			0x210
#define CBM_CTRL_JSEL			BIT(17)

/*
 * The bus masters address the pools through the coherent alias
 * (AVM cqm/grx500/cbm.h:181).
 */
#define CBM_COHERENT_ALIAS(addr)	((((u32)(addr)) & 0x1FFFFFFF) | 0xC0000000)

/*
 * Enqueue manager (AVM cqm/grx500/reg/cbm_eqm.h:25, :2941, :9541).
 * Per-port windows are 4 KiB apart. Offsets inside a CPU ingress-port window
 * are those of AVM's cbm_eqm_cpu_igp_reg overlay (AVM cqm/grx500/cbm.h:356).
 */
#define CBM_EQM_CTRL			0x0000
#define CBM_EQM_CTRL_EN			BIT(0)
#define CBM_EQM_CTRL_QEN		BIT(4)
#define CBM_EQM_CTRL_SNOOPEN		BIT(6)
#define CBM_EQM_CTRL_PDEN		BIT(8)
#define CBM_EQM_CTRL_DLYSEL		(3U << 9)

#define CBM_EQM_CPU_IGP_0		0x10000
#define CBM_EQM_PORT_STRIDE		0x1000

#define CBM_EQM_IGP_CFG			0x00
#define CBM_EQM_IGP_POCC		0x08
#define CBM_EQM_IGP_IRNEN		0x28
#define CBM_EQM_IGP_DCNTR		0x4C
#define CBM_EQM_IGP_NEW_SPTR		0x80
#define CBM_EQM_IGP_DESC0		0x100

/* AVM cqm/grx500/cbm.c:1335-1342 CPU ingress-port arming values. */
#define CBM_EQM_CPU_IGP_CFG_VAL		0xFC7
#define CBM_EQM_CPU_IGP_IRNEN_ALL	0x3F
#define CBM_EQM_IGP_DCNTR_DLY		0x10

/*
 * Dequeue manager (AVM cqm/grx500/reg/cbm_dqm.h:25, :155, :199, :4839).
 * Offsets inside a CPU egress-port window follow AVM's cbm_dqm_cpu_egp_reg
 * overlay (AVM cqm/grx500/cbm.h:398).
 */
#define CBM_DQM_CTRL			0x0000
#define CBM_DQM_CTRL_EN			BIT(0)
#define CBM_DQM_CTRL_QEN		BIT(4)

#define CBM_DQM_CPU_EGP_0		0x10000
#define CBM_DQM_DMA_EGP_6		0x16000
#define CBM_DQM_PORT_STRIDE		0x1000

#define CBM_DQM_EGP_CFG			0x00
#define CBM_DQM_EGP_PTR_RTN		0x80

#define CBM_DQM_EGP_CFG_EPMAP_SHIFT	16
#define CBM_DQM_EGP_CFG_EPMAP_MASK	0x7F0000U
/* AVM cqm/grx500/cbm.c:1385-1392: dequeue request plus per-port counting. */
#define CBM_DQM_DMA_EGP_CFG_DQREQ	BIT(0)
#define CBM_DQM_DMA_EGP_CFG_DQPCEN	BIT(8)
/* AVM cqm/grx500/cbm.c:1404-1441 CPU egress-port config. */
#define CBM_DQM_CPU_EGP_CFG_VAL		0x10F

/* One descriptor-staging window per port (AVM cqm/grx500/reg/cbm_desc64b.h). */
#define CBM_DESC_EGP_5			0x40000
#define CBM_DESC_PORT_STRIDE		0x1000
#define CBM_DESC_PER_DMA_PORT		2

/*
 * Free-segment queue manager (AVM cqm/grx500/reg/fsqm.h). Two instances, one
 * per pool. RCNT and RAM are large indexed arrays, one word per segment.
 */
#define FSQM_CTRL			0x00000
#define FSQM_IO_BUF_RD			0x00008
#define FSQM_IO_BUF_WR			0x0000C
#define FSQM_IRNEN			0x00018
#define FSQM_OFSQ			0x00024
#define FSQM_OFSC			0x0002C
#define FSQM_FSQT0			0x00030
#define FSQM_FSQT1			0x00070
#define FSQM_FSQT2			0x000B0
#define FSQM_FSQT3			0x000F0
#define FSQM_FSQT4			0x00130
#define FSQM_LSARNG			0x00180
#define FSQM_RCNT			0x80000
#define FSQM_RAM			0xC0000

#define FSQM_LSARNG_MAX_SHIFT		16
#define FSQM_OFSQ_TAIL_SHIFT		16
#define FSQM_OFSC_MASK			0x00007FFFU
/* AVM cqm/grx500/cbm.c:1106: the last list slot carries this sentinel. */
#define FSQM_LIST_END			0x7FFFU
/* AVM cqm/grx500/cbm.c:1129 interrupt-enable composition. */
#define FSQM_IRNEN_VAL			0x0111101FU

/*
 * A segment request answers with this value when the pool is exhausted
 * (AVM cqm/grx500/cbm.c, cbm_buffer_alloc_grx500). The same constant doubles
 * as the segment-aligned address mask on the return path.
 */
#define FSQM_POOL_EMPTY			0xFFFFF800U
/* AVM cqm/cqm_common.h:19 default retry budget for a segment request. */
#define FSQM_ALLOC_RETRIES		20U

#define CBM_CPU_PORT_NUM		4

/* Traffic-management unit */

/* AVM tmu/drv_tmu_reg.h register offsets. */
#define TMU_CTRL			0x000
#define TMU_CTRL_ACT_EN			BIT(0)
#define TMU_QEMT			0x200
#define TMU_QSMT			0x210
#define TMU_QTHT0			0x220
#define TMU_QTHT1			0x224
#define TMU_QTHT2			0x228
#define TMU_QTHT3			0x22C
#define TMU_QTHT4			0x230
#define TMU_QMTC			0x270
#define TMU_EPMT			0x300
#define TMU_EPMTC			0x340
#define TMU_SBITR0			0x400
#define TMU_SBITR1			0x404
#define TMU_SBITR2			0x408
#define TMU_SBITR3			0x40C
#define TMU_SBITC			0x410
#define TMU_SBOTR0			0x420
#define TMU_SBOTC			0x430
#define TMU_CFGEPN			0x71C
#define TMU_CFGCMD			0x720

#define TMU_QMTC_QEW			BIT(8)
#define TMU_QMTC_QSW			BIT(9)
#define TMU_QMTC_QTW			BIT(10)
#define TMU_QMTC_QEV			BIT(24)
#define TMU_QMTC_QSV			BIT(25)
#define TMU_QMTC_QTV			BIT(26)

#define TMU_EPMT_EPE			BIT(31)
#define TMU_EPMT_SBID_MASK		0x0000007FU
#define TMU_EPMTC_EMW			BIT(8)
#define TMU_EPMTC_EMR			BIT(16)
#define TMU_EPMTC_EMV			BIT(24)
#define TMU_EPMTC_EOV			BIT(25)
#define TMU_EPMTC_ETV			BIT(26)
#define TMU_EPMTC_EDV			BIT(27)
#define TMU_EPMTC_VAL \
	(TMU_EPMTC_EMV | TMU_EPMTC_EOV | TMU_EPMTC_ETV | TMU_EPMTC_EDV)
#define TMU_EPMTC_EPN_MASK		0x0000007FU

#define TMU_SBOTR0_SOE			BIT(31)
#define TMU_SBOTR0_LVL_SHIFT		16
#define TMU_SBOTR0_LVL_MASK		0x00070000U
#define TMU_SBOTR0_OMID_MASK		0x000003FFU
#define TMU_SBOTC_VAL			BIT(18)
#define TMU_SBOTC_WRITE			BIT(16)

#define TMU_SBITR0_SIE			BIT(31)
#define TMU_SBITR0_QSID_MASK		0x000000FFU
#define TMU_SBITC_VAL			BIT(18)
#define TMU_SBITC_SEL			BIT(17)
#define TMU_SBITC_WRITE			BIT(16)
/* AVM: token-bucket id 255 means no shaper on this input. */
#define TMU_SBIT_NO_SHAPER		255

#define TMU_QEMT_EPN_MASK		0x0000007FU
#define TMU_QSMT_SBIN_MASK		0x000003FFU
#define TMU_QTHT0_QE			BIT(31)

#define TMU_CFGCMD_VAL			BIT(16)
#define TMU_CFGCMD_EP_ON		0xB0000000U
#define TMU_CFGCMD_SB_INPUT_ON		0x00000000U
#define TMU_CFGSBIN_MASK		0x000003FFU

/*
 * Queue threshold defaults (AVM tmu/drv_tmu_ll.h:59-99, reproduced so the
 * queue this driver creates gets the same tail-drop and weighted-random-drop
 * configuration the vendor gives every egress queue).
 */
#define TMU_EPTT0_DEFAULT		0x120
#define TMU_QTHT0_DEFAULT		0x00011320U
#define TMU_QTHT1_DEFAULT		((TMU_EPTT0_DEFAULT / 8) / 2)
#define TMU_QTHT2_DEFAULT		(TMU_EPTT0_DEFAULT / 8)
#define TMU_QTHT3_DEFAULT		0x0FFF
#define TMU_QTHT4_0_DEFAULT		0x0004
#define TMU_QTHT4_1_DEFAULT		0x0000

/*
 * A scheduler-block input id is the block id shifted by three plus the input
 * index (AVM tmu/drv_tmu_ll.c:2819-2847). The first sixteen block ids are
 * reserved, so a queue id maps to its block by subtracting this base.
 */
#define TMU_SBID_BASE			16
#define TMU_SBIN_SHIFT			3

/* An indirect table command completes within a bounded number of reads. */
#define TMU_CMD_TIMEOUT_US		1000

/* Transmit DMA controller */

/*
 * AVM drivers/dma/intel/hdma.c register offsets. The per-channel registers
 * are a window selected by writing the channel number to the channel-select
 * register, so a channel access is select-then-touch and the pair has to be
 * atomic against every other user of the same controller.
 *
 * While this driver runs beside the existing DMA driver that selector is
 * shared with a lock this driver cannot take, so the channel is touched at
 * probe only. The other users of the selector on this controller are the
 * per-channel interrupt path, which cannot run because these channels raise
 * no interrupt, and a port enable that returns early once a port is armed.
 * The lock becomes this driver's when the channel setup moves here for good.
 *
 * The register file aliases above 0x100 and offset 0x0C is a decode hole that
 * raises a bus error, so nothing here reads outside the named offsets.
 */
#define DMA_ID				0x0008
#define DMA_CTRL			0x0010
#define DMA_CTRL_DS_FOD			BIT(7)
#define DMA_CS				0x0018
#define DMA_CCTRL			0x001C
#define DMA_CCTRL_ON			BIT(0)
#define DMA_CCTRL_DIR_TX		BIT(8)
#define DMA_CCTRL_CLASS_SHIFT		9
#define DMA_CCTRL_CLASS_MASK		0x000E00U
#define DMA_CCTRL_CLASSH_SHIFT		18
#define DMA_CCTRL_CLASSH_MASK		0x0C0000U
#define DMA_CDBA			0x0020
#define DMA_CDLEN			0x0024
#define DMA_CIE				0x002C

/* Datapath egress resources */

/**
 * struct xrx500_cbm_egress - egress resources of one datapath port.
 * @dp_port:  datapath port id, which is also the switch port number.
 * @deq_port: buffer-manager dequeue port that drains the port.
 * @tmu_qid:  traffic-management queue feeding @deq_port.
 * @dma_chan: channel number within the transmit DMA controller.
 *
 * Transcribed from AVM cqm/grx500/cbm_config.c's dequeue-DMA rows joined to
 * the endpoint-group table at AVM cqm/grx500/cbm.c:566-600. Only the rows for
 * ports served by the internal switch are carried: the dequeue port is a
 * table lookup, not a closed form, and the six consecutive rows below make it
 * look like one.
 *
 * The traffic-management egress-port number equals the dequeue port number on
 * this silicon, so no separate column is needed for it.
 */
struct xrx500_cbm_egress {
	u32 dp_port;
	u32 deq_port;
	u32 tmu_qid;
	u32 dma_chan;
};

static const struct xrx500_cbm_egress xrx500_cbm_egress_lan[] = {
	{ .dp_port = 2, .deq_port =  7, .tmu_qid = 17, .dma_chan = 2 },
	{ .dp_port = 3, .deq_port =  8, .tmu_qid = 18, .dma_chan = 3 },
	{ .dp_port = 4, .deq_port =  9, .tmu_qid = 19, .dma_chan = 4 },
	{ .dp_port = 5, .deq_port = 10, .tmu_qid = 20, .dma_chan = 5 },
};

/*
 * The second macro's single front-panel port. Its dequeue port is drained by
 * the first pair of DMA controllers rather than the second, so the row
 * carries a channel number that is only meaningful against the controller the
 * conduit's own transmit phandle names.
 */
static const struct xrx500_cbm_egress xrx500_cbm_egress_wan[] = {
	{ .dp_port = 15, .deq_port = 19, .tmu_qid = 28, .dma_chan = 15 },
};

/*
 * Transmit queue zero carries no port. A frame reaches this device either
 * from the tagging driver, which records the port it is for in the socket
 * buffer, or from the local stack, which does not and whose frame therefore
 * has no switch header in front of it. The first case selects one of the
 * queues above zero, one per port; the second stays on queue zero, which has
 * nowhere to send it.
 */
#define XRX500_CBM_TXQ_NONE		0

/*
 * The topology this driver replaces gives the first macro's receive ring a
 * consumer of its own: a peripheral-to-peripheral copy into the second
 * macro's transmit controller, which walks the very same descriptors. Nothing
 * consumes the second macro's ring that way - it is admitted straight into
 * the buffer manager, which simply stops being fed once the ring moves - so
 * the second macro names no consumer to stop.
 */
#define CBM_RX_RELAY_NONE		0

/**
 * struct xrx500_cbm_macro - what one switch macro's conduit serves.
 * @eg:       egress rows, one per front-panel port of the macro.
 * @num:      rows in @eg.
 * @rx_relay: channel of the receive ring's previous consumer, or
 *            %CBM_RX_RELAY_NONE where the ring has none.
 *
 * Indexed by the conduit's own register value, which names the macro.
 */
struct xrx500_cbm_macro {
	const struct xrx500_cbm_egress *eg;
	unsigned int num;
	u32 rx_relay;
};

static const struct xrx500_cbm_macro xrx500_cbm_macros[] = {
	{ xrx500_cbm_egress_lan, ARRAY_SIZE(xrx500_cbm_egress_lan),
	  DMA1TX_LAN_SWITCH_CLASS0 },
	{ xrx500_cbm_egress_wan, ARRAY_SIZE(xrx500_cbm_egress_wan),
	  CBM_RX_RELAY_NONE },
};

/* Wire format */

/*
 * Eight-byte switch header prepended to a transmitted frame
 * (AVM datapath/datapath_api_gswip30.h:279-349). The tagging driver writes
 * it; the conduit needs only its length, to size the segment and the minimum
 * frame, and to know that the frame it is handed already carries it.
 */
#define PMAC_TX_HDR_LEN			8

/*
 * The switch discards a frame whose Ethernet part is below the minimum
 * length, and the header does not count towards it.
 */
#define XRX500_CBM_MIN_FRAME		(ETH_ZLEN + PMAC_TX_HDR_LEN)

/*
 * Enqueue descriptor word order in the CPU ingress-port window
 * (AVM cqm/grx500/cbm.c:1787-1789 and the enqueue site at :2560):
 *   +0x0 destination sub-interface
 *   +0x4 endpoint and colour
 *   +0x8 data pointer, eight-byte aligned
 *   +0xC ownership, length and the residual byte offset; written last
 */
#define CBM_TXD_DW1_EP_SHIFT		8
#define CBM_TXD_DW1_EP_MASK		0x00000F00U
#define CBM_TXD_DW1_COLOR_SHIFT		12
#define CBM_TXD_DW1_COLOR_GREEN		1U
#define CBM_TXD_DW3_SOP			BIT(29)
#define CBM_TXD_DW3_EOP			BIT(28)
#define CBM_TXD_DW3_BYTE_OFF_SHIFT	23
#define CBM_TXD_DW3_LEN_MASK		0x0000FFFFU

/*
 * The same eight-byte header arrives ahead of every received frame
 * (AVM datapath/datapath_api_gswip30.h:353-386). Only the source port is
 * consumed here; it is the switch port the frame came in on, in the high
 * nibble of byte 2. The receive descriptor's endpoint field is zero on this
 * path, so the header is the only source-port information there is.
 */
#define PMAC_RX_HDR_LEN			8
#define PMAC_RX_SPPID_NUM		16

/* Receive DMA ring */

/**
 * struct xrx500_cbm_rxd - one receive descriptor.
 * @dw0:  classification result, produced by the switch, unused here.
 * @dw1:  colour and endpoint; the endpoint is zero on this path.
 * @addr: where the engine writes the frame.
 * @ctl:  ownership, packet delimiters, byte offset and length.
 *
 * Sixteen bytes, and the two words of each pair are transposed on a
 * big-endian processor because the datapath crossbar's own byte order does
 * not follow the core's (AVM drivers/dma/intel/hdma.h carries one structure
 * per strapping). Reproducing the transposition is deliberate: treating it as
 * a defect and swapping the words back puts every field in the wrong place.
 *
 * @ctl is the commit word in both directions. The engine hands a descriptor
 * back by clearing ownership in it, and the driver hands one over by setting
 * ownership in it once everything else is in place. Its field positions are
 * the same either way round.
 */
struct xrx500_cbm_rxd {
#ifdef __BIG_ENDIAN
	u32 dw1;
	u32 dw0;
	u32 ctl;
	u32 addr;
#else
	u32 dw0;
	u32 dw1;
	u32 addr;
	u32 ctl;
#endif
};

static_assert(sizeof(struct xrx500_cbm_rxd) == 16);

#define CBM_RXD_CTL_LEN			GENMASK(15, 0)
#define CBM_RXD_CTL_QID			GENMASK(22, 19)
#define CBM_RXD_CTL_BOFF		GENMASK(25, 23)
#define CBM_RXD_CTL_EOP			BIT(28)
#define CBM_RXD_CTL_SOP			BIT(29)
#define CBM_RXD_CTL_C			BIT(30)
#define CBM_RXD_CTL_OWN			BIT(31)

/*
 * Only channel zero of a receive controller is wired as a switch target
 * (AVM cqm/grx500/lantiq_cbm_api.h:72), so there is one hardware receive
 * queue per macro; spreading receive work over the other processors is a
 * software concern. Which controller it is comes from the device tree, so the
 * handle is composed at probe rather than named by a constant.
 */
#define CBM_RX_CHAN_NR			DMA_CHANNEL_0
#define CBM_RX_CHAN_PORT		0

/*
 * Ring geometry. The descriptor length field is twelve bits wide, and a ring
 * short enough to be overrun by one burst of receive traffic shows up as
 * transport-level retransmission rather than as a driver error, so the lower
 * bound is generous rather than minimal.
 */
#define CBM_RX_RING_MIN			16U
#define CBM_RX_RING_MAX			1024U
#define CBM_RX_RING_DEFAULT		256U

/* Driver state */

/**
 * struct xrx500_cbm_pool - one hardware-managed segment pool.
 * @phys:     physical base of the reserved region.
 * @virt:     write-back mapping of the same region.
 * @size:     usable size after alignment.
 * @seg_size: segment size the manager hands out.
 * @segs:     number of segments in the pool.
 */
struct xrx500_cbm_pool {
	phys_addr_t phys;
	void *virt;
	size_t size;
	u32 seg_size;
	u32 segs;
};

/**
 * struct xrx500_cbm_stats - transmit accounting the hardware cannot provide.
 * @pops:       segments taken out of the free-segment manager.
 * @rollbacks:  segments handed back because the enqueue never happened.
 * @pool_empty: segment requests that found the pool exhausted.
 * @stops:      times the transmit queue was stopped for back pressure.
 * @wakes:      times the restart poll released the queue.
 * @polls:      restart-poll invocations.
 * @noport:     frames offered without a port, which is every frame that did
 *              not come from a switch port's own network device.
 *
 * @pops minus @rollbacks is the number of segments handed to the hardware. On
 * a healthy path that difference grows without bound while the manager's own
 * free-segment count stays level, because the hardware recycles each segment
 * after egress. A free-segment count that walks down instead is the signature
 * of a segment the hardware never took ownership of.
 */
struct xrx500_cbm_stats {
	u64 pops;
	u64 rollbacks;
	u64 pool_empty;
	u64 stops;
	u64 wakes;
	u64 polls;
	u64 noport;
};

/**
 * struct xrx500_cbm_rx_stats - receive accounting.
 * @polls:       poll invocations.
 * @completions: descriptors handed back by the engine.
 * @bytes:       bytes in those descriptors, switch header included.
 * @delivered:   frames handed to the network stack.
 * @wraps:       times consumption passed the end of the ring.
 * @malformed:   descriptors whose length or delimiters made no sense.
 * @drop_noskb:  frames dropped because no socket buffer was available.
 */
struct xrx500_cbm_rx_stats {
	u64 polls;
	u64 completions;
	u64 bytes;
	u64 delivered;
	u64 wraps;
	u64 malformed;
	u64 drop_noskb;
};

/**
 * struct xrx500_cbm_rx_buf - one ring buffer.
 * @va:  page-fragment base.
 * @dma: streaming mapping of the frame area inside it.
 */
struct xrx500_cbm_rx_buf {
	void *va;
	dma_addr_t dma;
};

/**
 * struct xrx500_cbm_rx - receive ring state.
 * @napi:      poll context; there is one ring, so there is one of these.
 * @ring:      the descriptor ring, mapped uncached.
 * @ring_pa:   bus address of the ring, which is what the engine is given.
 * @ring_len:  descriptors in the ring.
 * @buf:       one entry per descriptor.
 * @next:      descriptor consumption resumes here.
 * @buf_len:   bytes of a ring buffer the engine may write.
 * @boff:      byte offset requested of the engine inside a ring buffer.
 * @traced:    malformed descriptors dumped so far.
 * @active:    the ring is live and the poll may be scheduled.
 * @aborted:   consumption gave up; the ring is left alone for inspection.
 * @stats:     receive accounting.
 * @sppid:     frames seen per source port, which is what the switch header
 *             carries and what a per-port demultiplexer will key on.
 */
struct xrx500_cbm_rx {
	struct napi_struct napi;
	struct xrx500_cbm_rxd *ring;
	dma_addr_t ring_pa;
	unsigned int ring_len;
	struct xrx500_cbm_rx_buf *buf;
	unsigned int next;
	unsigned int buf_len;
	unsigned int boff;
	unsigned int traced;
	bool active;
	bool aborted;
	struct xrx500_cbm_rx_stats stats;
	u64 sppid[PMAC_RX_SPPID_NUM];
};

/**
 * struct xrx500_cbm - conduit driver state.
 * @dev:         backing platform device.
 * @ndev:        the conduit network device.
 * @cbm:         buffer-manager control window.
 * @eqm:         enqueue-manager window.
 * @dqm:         dequeue-manager window.
 * @desc:        descriptor-staging window shared by both managers.
 * @qidt:        queue-index table window.
 * @tmu:         traffic-management window.
 * @fsqm:        the two free-segment manager instances.
 * @dma:         transmit DMA controller window.
 * @dma_desc_pa: physical base of @desc, needed as a DMA descriptor base.
 * @rx_chan:     receive channel handle on the controller the device tree
 *               names for this conduit.
 * @rx_relay:    channel of the receive ring's previous consumer, or
 *               %CBM_RX_RELAY_NONE.
 * @std:         standard segment pool.
 * @jbo:         jumbo segment pool.
 * @eg:          egress resources, one row per port this conduit serves.
 * @eg_num:      rows in @eg.
 * @restart:     back-pressure release poll.
 * @stats:       transmit accounting.
 * @force_empty: remaining synthetic pool-empty answers, for exercising the
 *               back-pressure path on a pool that is never really exhausted.
 *               Guarded by the transmit lock, which both spenders hold.
 * @closing:     the device is going down; the release poll must not rearm.
 * @rx:          receive ring state.
 * @node:        entry on the list of bound conduits.
 */
struct xrx500_cbm {
	struct device *dev;
	struct net_device *ndev;

	void __iomem *cbm;
	void __iomem *eqm;
	void __iomem *dqm;
	void __iomem *desc;
	void __iomem *qidt;
	void __iomem *tmu;
	void __iomem *fsqm[2];
	void __iomem *dma;
	phys_addr_t dma_desc_pa;
	u32 rx_chan;
	u32 rx_relay;

	struct xrx500_cbm_pool std;
	struct xrx500_cbm_pool jbo;

	const struct xrx500_cbm_egress *eg;
	unsigned int eg_num;

	struct timer_list restart;

	struct xrx500_cbm_stats stats;
	unsigned int force_empty;
	bool closing;

	struct xrx500_cbm_rx rx;
	struct list_head node;
};

/*
 * The enqueue manager's descriptor windows and the traffic manager's
 * indirect-table staging window are one piece of hardware each, shared by
 * every conduit the buffer manager serves, so the locks that serialise them
 * belong to the driver rather than to a conduit.
 */
static DEFINE_SPINLOCK(xrx500_cbm_tx_lock);
static DEFINE_SPINLOCK(xrx500_cbm_tmu_lock);

/*
 * Take ownership of the buffer manager: seed both segment pools, program the
 * pool bases and enable both managers. Off by default, because the shipped
 * buffer-manager driver performs the same bring-up and a second pass would
 * discard the live free-segment list.
 */
static bool own_cbm;
module_param(own_cbm, bool, 0444);
MODULE_PARM_DESC(own_cbm, "run the full buffer-manager bring-up");

/*
 * Program the transmit DMA channel rather than only verifying it. Off by
 * default: the channel is already armed for this dequeue port, and once armed
 * a buffer-manager-managed channel is meant to stay armed.
 */
static bool own_dma_chan;
module_param(own_dma_chan, bool, 0444);
MODULE_PARM_DESC(own_dma_chan, "program the transmit DMA channel");

/*
 * Terminate the switch's receive ring on the processor. Off by default,
 * because the existing datapath consumes the same ring through a
 * peripheral-to-peripheral copy into the second switch instance and only one
 * consumer of a ring can be right.
 */
static bool own_rx = true;
module_param(own_rx, bool, 0444);
MODULE_PARM_DESC(own_rx, "terminate the switch receive ring on the CPU");

/*
 * Descriptors in the receive ring. Settable, because the ring is built when
 * the device comes up, so a size can be changed without a reboot.
 */
static unsigned int rx_ring_size = CBM_RX_RING_DEFAULT;
module_param(rx_ring_size, uint, 0644);
MODULE_PARM_DESC(rx_ring_size, "descriptors in the receive ring");

/*
 * Byte offset asked of the engine inside each ring buffer, sampled when the
 * device comes up. A copy-free receive path would need the engine to honour
 * the descriptor's byte-offset field in this direction; the consuming code
 * reads back the offset the engine reports either way, so any value here is
 * safe.
 */
static unsigned int rx_boff;
module_param(rx_boff, uint, 0644);
MODULE_PARM_DESC(rx_boff, "byte offset requested of the receive engine");

/* Give up consuming the ring after this many malformed descriptors. */
static unsigned int rx_errcap = 32;
module_param(rx_errcap, uint, 0644);
MODULE_PARM_DESC(rx_errcap, "stop consuming after this many bad descriptors");

/*
 * Writing N makes the next N answers about the pool report it as exhausted.
 * The transmit path spends one per segment request and the release poll one
 * per invocation, so N drives the stop, the poll and the wake in turn.
 */
static unsigned int tx_force_empty;
static int xrx500_cbm_set_force_empty(const char *val,
				      const struct kernel_param *kp);
static const struct kernel_param_ops xrx500_cbm_force_empty_ops = {
	.set = xrx500_cbm_set_force_empty,
	.get = param_get_uint,
};
module_param_cb(tx_force_empty, &xrx500_cbm_force_empty_ops,
		&tx_force_empty, 0644);
MODULE_PARM_DESC(tx_force_empty,
		 "synthesise N exhausted-pool segment requests");

/*
 * The parameter callbacks below reach every bound conduit through this list,
 * which the same mutex guards against a bind or unbind running beside them.
 */
static DEFINE_MUTEX(xrx500_cbm_lock);
static LIST_HEAD(xrx500_cbm_conduits);

/* Register access */

/*
 * The datapath initiators are strapped big-endian and the descriptor windows
 * are byte-ordered accordingly, so every access is a raw one. Adding a
 * byte-swapping accessor here would transpose the descriptor words.
 */
static inline u32 cbm_r32(void __iomem *base, u32 off)
{
	return __raw_readl(base + off);
}

static inline void cbm_w32(void __iomem *base, u32 off, u32 val)
{
	__raw_writel(val, base + off);
}

static inline u32 eqm_cpu_off(u32 pid, u32 reg)
{
	return CBM_EQM_CPU_IGP_0 + pid * CBM_EQM_PORT_STRIDE + reg;
}

static inline u32 dqm_cpu_off(u32 pid, u32 reg)
{
	return CBM_DQM_CPU_EGP_0 + pid * CBM_DQM_PORT_STRIDE + reg;
}

/* Traffic-management unit */

/*
 * Each indirect table is written by staging the payload registers, then
 * writing a command word, then waiting for the command's own valid bit. The
 * wait is bounded here; the vendor spins without a bound
 * (AVM tmu/drv_tmu_ll.c:1535-1549).
 *
 * The staging registers are a single global window, so a stage-then-command
 * pair has to exclude every other writer of the same window. While this
 * driver runs beside the existing traffic-management code that exclusion is
 * incomplete, because the other side holds a lock of its own. The exposure is
 * bounded to probe: this driver writes these tables once and never again.
 */
static int tmu_poll_valid(struct xrx500_cbm *priv, u32 cmd_off, u32 val_bit)
{
	u32 tmp;
	int i;

	for (i = 0; i < TMU_CMD_TIMEOUT_US; i++) {
		tmp = cbm_r32(priv->tmu, cmd_off);
		if (tmp & val_bit)
			return 0;
		udelay(1);
	}
	return -ETIMEDOUT;
}

static int tmu_write_cmd(struct xrx500_cbm *priv, u32 cmd_off, u32 cmd,
			 u32 val_bit)
{
	cbm_w32(priv->tmu, cmd_off, cmd);
	return tmu_poll_valid(priv, cmd_off, val_bit);
}

/* Queue-to-egress-port mapping (AVM tmu/drv_tmu_ll.c:454-470). */
static int tmu_queue_map_port(struct xrx500_cbm *priv, u32 qid, u32 epn)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	cbm_w32(priv->tmu, TMU_QEMT, epn & TMU_QEMT_EPN_MASK);
	ret = tmu_write_cmd(priv, TMU_QMTC, TMU_QMTC_QEW | qid, TMU_QMTC_QEV);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	return ret;
}

/* Queue-to-scheduler-input mapping (AVM tmu/drv_tmu_ll.c:490-506). */
static int tmu_queue_map_sched(struct xrx500_cbm *priv, u32 qid, u32 sbin)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	cbm_w32(priv->tmu, TMU_QSMT, sbin & TMU_QSMT_SBIN_MASK);
	ret = tmu_write_cmd(priv, TMU_QMTC, TMU_QMTC_QSW | qid, TMU_QMTC_QSV);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	return ret;
}

/*
 * Queue thresholds with the queue enabled (AVM tmu/drv_tmu_ll.c:526-550 and
 * the defaults at AVM tmu/drv_tmu_ll.h:59-99).
 */
static int tmu_queue_enable(struct xrx500_cbm *priv, u32 qid)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	cbm_w32(priv->tmu, TMU_QTHT0, TMU_QTHT0_DEFAULT | TMU_QTHT0_QE);
	cbm_w32(priv->tmu, TMU_QTHT1,
		(TMU_QTHT1_DEFAULT << 16) | TMU_QTHT1_DEFAULT);
	cbm_w32(priv->tmu, TMU_QTHT2,
		(TMU_QTHT2_DEFAULT << 16) | TMU_QTHT2_DEFAULT);
	cbm_w32(priv->tmu, TMU_QTHT3,
		(TMU_QTHT3_DEFAULT << 16) | TMU_QTHT3_DEFAULT);
	cbm_w32(priv->tmu, TMU_QTHT4,
		(TMU_QTHT4_1_DEFAULT << 16) | TMU_QTHT4_0_DEFAULT);
	ret = tmu_write_cmd(priv, TMU_QMTC, TMU_QMTC_QTW | qid, TMU_QMTC_QTV);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	return ret;
}

/* Egress-port mapping entry read (AVM tmu/drv_tmu_ll.c:866-884). */
static int tmu_port_map_read(struct xrx500_cbm *priv, u32 epn, bool *enabled,
			     u32 *sbid)
{
	unsigned long flags;
	int ret;
	u32 tmp;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	ret = tmu_write_cmd(priv, TMU_EPMTC,
			    TMU_EPMTC_EMR | (epn & TMU_EPMTC_EPN_MASK),
			    TMU_EPMTC_VAL);
	tmp = cbm_r32(priv->tmu, TMU_EPMT);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	if (ret)
		return ret;

	*sbid = tmp & TMU_EPMT_SBID_MASK;
	*enabled = !!(tmp & TMU_EPMT_EPE);
	return 0;
}

/* Egress-port mapping entry write (AVM tmu/drv_tmu_ll.c:846-864). */
static int tmu_port_map_write(struct xrx500_cbm *priv, u32 epn, bool enable,
			      u32 sbid)
{
	unsigned long flags;
	u32 tmp = sbid & TMU_EPMT_SBID_MASK;
	int ret;

	if (enable)
		tmp |= TMU_EPMT_EPE;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	cbm_w32(priv->tmu, TMU_EPMT, tmp);
	ret = tmu_write_cmd(priv, TMU_EPMTC,
			    TMU_EPMTC_EMW | (epn & TMU_EPMTC_EPN_MASK),
			    TMU_EPMTC_VAL);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	return ret;
}

/*
 * Link a scheduler block's output to an egress port
 * (AVM tmu/drv_tmu_ll.c:2230-2252 with the level and output-member id of a
 * flat, single-level topology).
 */
static int tmu_sched_link_port(struct xrx500_cbm *priv, u32 sbid, u32 epn)
{
	unsigned long flags;
	bool enabled;
	u32 linked;
	u32 sbot0;
	int ret;

	sbot0 = TMU_SBOTR0_SOE;
	sbot0 |= (0U << TMU_SBOTR0_LVL_SHIFT) & TMU_SBOTR0_LVL_MASK;
	sbot0 |= epn & TMU_SBOTR0_OMID_MASK;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	cbm_w32(priv->tmu, TMU_SBOTR0, sbot0);
	ret = tmu_write_cmd(priv, TMU_SBOTC, TMU_SBOTC_WRITE | sbid,
			    TMU_SBOTC_VAL);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	if (ret)
		return ret;

	/* The egress port points back at the block that feeds it. */
	ret = tmu_port_map_read(priv, epn, &enabled, &linked);
	if (ret)
		return ret;
	return tmu_port_map_write(priv, epn, enabled, sbid);
}

/*
 * Link a scheduler-block input to a queue with no shaper
 * (AVM tmu/drv_tmu_ll.c:2754-2782).
 */
static int tmu_sched_link_queue(struct xrx500_cbm *priv, u32 sbin, u32 qid)
{
	unsigned long flags;
	u32 sbit0 = TMU_SBITR0_SIE | (qid & TMU_SBITR0_QSID_MASK);
	int ret;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	cbm_w32(priv->tmu, TMU_SBITR0, sbit0);
	cbm_w32(priv->tmu, TMU_SBITR1, TMU_SBIT_NO_SHAPER);
	cbm_w32(priv->tmu, TMU_SBITR2, 0);
	cbm_w32(priv->tmu, TMU_SBITR3, 0);
	ret = tmu_write_cmd(priv, TMU_SBITC,
			    TMU_SBITC_WRITE | TMU_SBITC_SEL | sbin,
			    TMU_SBITC_VAL);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	return ret;
}

/* Commit an egress-port or scheduler-input state change. */
static int tmu_config_cmd(struct xrx500_cbm *priv, u32 cmd)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&xrx500_cbm_tmu_lock, flags);
	ret = tmu_write_cmd(priv, TMU_CFGCMD, cmd, TMU_CFGCMD_VAL);
	spin_unlock_irqrestore(&xrx500_cbm_tmu_lock, flags);
	return ret;
}

/**
 * xrx500_cbm_tmu_egress_path() - build the egress path of one dequeue port.
 * @priv: driver state.
 * @eg:   egress resources of the port.
 *
 * One scheduler block, one queue, no shaper and no weight: the only topology
 * this datapath needs (AVM tmu/drv_tmu_ll.c:2819-2847). The egress-port
 * number equals the dequeue port number and the scheduler block id is the
 * queue id less the reserved base.
 */
static int xrx500_cbm_tmu_egress_path(struct xrx500_cbm *priv,
				      const struct xrx500_cbm_egress *eg)
{
	u32 epn = eg->deq_port;
	u32 qid = eg->tmu_qid;
	u32 sbid = qid - TMU_SBID_BASE;
	u32 sbin = sbid << TMU_SBIN_SHIFT;
	int ret;

	if (!(cbm_r32(priv->tmu, TMU_CTRL) & TMU_CTRL_ACT_EN)) {
		dev_info(priv->dev, "traffic manager not active yet\n");
		return -EPROBE_DEFER;
	}

	/* Scheduler block feeds the egress port, and the port points back. */
	ret = tmu_sched_link_port(priv, sbid, epn);
	if (ret)
		return ret;

	/* Enable the egress port and commit the change. */
	ret = tmu_port_map_write(priv, epn, true, sbid);
	if (ret)
		return ret;
	ret = tmu_config_cmd(priv, TMU_CFGCMD_EP_ON |
				   (epn & TMU_EPMTC_EPN_MASK));
	if (ret)
		return ret;

	ret = tmu_queue_map_port(priv, qid, epn);
	if (ret)
		return ret;
	ret = tmu_queue_map_sched(priv, qid, sbin);
	if (ret)
		return ret;
	ret = tmu_queue_enable(priv, qid);
	if (ret)
		return ret;

	ret = tmu_config_cmd(priv, TMU_CFGCMD_SB_INPUT_ON |
				   (sbin & TMU_CFGSBIN_MASK));
	if (ret)
		return ret;

	ret = tmu_sched_link_queue(priv, sbin, qid);
	if (ret)
		return ret;

	dev_info(priv->dev,
		 "egress path: port %u block %u queue %u input %u\n",
		 epn, sbid, qid, sbin);
	return 0;
}

/* Queue-index table */

/*
 * The queue-index table maps an enqueue descriptor's endpoint, class and
 * offload bits onto a traffic-management queue. It is global and keyed on the
 * endpoint, with no per-ingress-port binding. Each word holds four eight-bit
 * queue ids.
 *
 * A CPU enqueue carries class zero and no offload bits, so exactly one slot
 * per endpoint is consulted: the one at endpoint shifted left by four
 * (AVM cqm/grx500/cbm.c:1458-1476).
 */
/*
 * Every slot the table can address is filled with a queue id that discards
 * before any endpoint is given a real one, so an endpoint nobody programmed
 * drops rather than enqueues into whatever the table happened to hold
 * (AVM cqm/grx500/cbm.c, the probe-time drop fill).
 */
#define XRX500_CBM_QIDT_WORDS		0x1000
#define XRX500_CBM_QIDT_DROP		0xFFFFFFFFU

static void xrx500_cbm_qidt_drop_fill(struct xrx500_cbm *priv)
{
	u32 i;

	for (i = 0; i < XRX500_CBM_QIDT_WORDS; i++)
		cbm_w32(priv->qidt, i * 4, XRX500_CBM_QIDT_DROP);
	wmb(); /* the table must be filled before any endpoint enqueues */
}

static void xrx500_cbm_qidt_set(struct xrx500_cbm *priv, u32 ep, u8 qid)
{
	u32 idx = ((ep << 4) & 0xF0) >> 2;
	u32 shift = (((ep << 4) & 0xF0) % 4) << 3;
	u32 mask = 0xFFU << shift;
	u32 val;

	val = cbm_r32(priv->qidt, idx * 4);
	val = (val & ~mask) | ((u32)qid << shift);
	cbm_w32(priv->qidt, idx * 4, val);
	wmb(); /* the table must be live before the first enqueue */
}

/* Free-segment queue manager */

/*
 * Seed one manager instance's linked-list memory and bring it online.
 *
 * AVM cqm/grx500/cbm.c:1095-1139 walks slots one through the segment count
 * inclusive: slot i-1 receives i modulo the count, except that the
 * second-to-last slot receives the end sentinel. Rewriting the loop to a
 * zero-based form leaves the last slot uninitialised and the manager stalls
 * on the first segment return. The reference count of every slot is set in
 * the same pass (AVM cqm/grx500/cbm.c:1112-1115).
 */
static void fsqm_seed_list(void __iomem *base, u32 segs)
{
	u32 i;

	for (i = 1; i <= segs; i++) {
		u32 slot = i - 1;
		u32 next = (i == segs - 1) ? FSQM_LIST_END : (i % segs);

		cbm_w32(base, FSQM_RAM + (slot << 2), next);
		cbm_w32(base, FSQM_RCNT + (slot << 2), 1);
	}
}

/**
 * fsqm_arm() - bring one free-segment manager instance online.
 * @base: instance window.
 * @segs: segments the instance manages.
 *
 * The last list entry is not usable, so the highest segment address is two
 * below the count (AVM cqm/grx500/cbm.c:1119). The command buffers are
 * drained before the control register is written, and the control register is
 * written last so the engine never observes a partial configuration
 * (AVM cqm/grx500/cbm.c:1086-1091).
 *
 * The threshold registers use left-to-right integer division exactly as the
 * vendor writes them; folding the multiply in first changes three of the five
 * values.
 */
static void fsqm_arm(void __iomem *base, u32 segs)
{
	u32 maxlsa = segs - 2;

	fsqm_seed_list(base, segs);

	cbm_w32(base, FSQM_LSARNG, maxlsa << FSQM_LSARNG_MAX_SHIFT);
	cbm_w32(base, FSQM_OFSQ, maxlsa << FSQM_OFSQ_TAIL_SHIFT);
	cbm_w32(base, FSQM_OFSC, (maxlsa + 1) & FSQM_OFSC_MASK);
	cbm_w32(base, FSQM_IRNEN, FSQM_IRNEN_VAL);

	cbm_w32(base, FSQM_FSQT0, segs / 6 * 5);
	cbm_w32(base, FSQM_FSQT1, segs / 6 * 4);
	cbm_w32(base, FSQM_FSQT2, segs / 6 * 3);
	cbm_w32(base, FSQM_FSQT3, segs / 6 * 2);
	cbm_w32(base, FSQM_FSQT4, segs / 6);

	cbm_w32(base, FSQM_IO_BUF_RD, 0);
	cbm_w32(base, FSQM_IO_BUF_WR, 0);
	cbm_w32(base, FSQM_CTRL, 1);
	wmb(); /* the instance must be live before a segment is requested */
}

/**
 * xrx500_cbm_free_segs() - free-segment count of the standard pool.
 * @priv: driver state.
 *
 * The manager keeps no per-segment length, only a linked list and a
 * reference count, so this cumulative count is the only occupancy figure it
 * publishes.
 */
static u32 xrx500_cbm_free_segs(struct xrx500_cbm *priv)
{
	return cbm_r32(priv->fsqm[0], FSQM_OFSC) & FSQM_OFSC_MASK;
}

/**
 * xrx500_cbm_seg_to_virt() - map a segment address into the pool mapping.
 * @priv: driver state.
 * @phys: segment address as the manager reported it.
 *
 * Returns NULL for an address outside both pools, which is how a corrupted
 * segment answer is caught before it is dereferenced.
 */
static void *xrx500_cbm_seg_to_virt(struct xrx500_cbm *priv, u32 phys)
{
	const struct xrx500_cbm_pool *pools[] = { &priv->std, &priv->jbo };
	int i;

	for (i = 0; i < ARRAY_SIZE(pools); i++) {
		const struct xrx500_cbm_pool *p = pools[i];
		phys_addr_t lo = p->phys;
		phys_addr_t hi = lo + (phys_addr_t)p->segs * p->seg_size;

		if (!p->virt)
			continue;
		if ((phys_addr_t)phys >= lo && (phys_addr_t)phys < hi)
			return (u8 *)p->virt + ((phys_addr_t)phys - lo);
	}
	return NULL;
}

/**
 * xrx500_cbm_seg_put() - hand a segment back to the manager.
 * @priv: driver state.
 * @pid:  CPU egress port to return through.
 * @pa:   segment address.
 *
 * Only for a segment that was taken but never enqueued. After a successful
 * enqueue the segment belongs to the hardware, which recycles it once the
 * frame has egressed; returning it here as well would hand the same segment
 * out twice.
 *
 * The return register is one per port, so @pid must be the caller's own port
 * and the caller must stay on that CPU across both the port choice and this
 * write (AVM cqm/grx500/cbm.c:1039 and its two call sites at :2645 and
 * :2695, which both return on the running CPU's port). A return written into
 * a port that was never configured neither faults nor logs; it drops the
 * segment.
 */
static void xrx500_cbm_seg_put(struct xrx500_cbm *priv, u32 pid, u32 pa)
{
	cbm_w32(priv->dqm, dqm_cpu_off(pid, CBM_DQM_EGP_PTR_RTN),
		pa & FSQM_POOL_EMPTY);
	wmb(); /* the return must land before the segment is reused */
	priv->stats.rollbacks++;
}

/**
 * xrx500_cbm_seg_get() - take a standard segment out of the manager.
 * @priv: driver state.
 * @pid:  CPU port to request through and to return through on failure.
 * @pa:   out, the segment address.
 *
 * The request register answers with an exhausted-pool marker while no segment
 * is available; the vendor re-reads a bounded number of times before giving
 * up (AVM cqm/cqm_common.h:19). Interrupts are off across the retry loop
 * because the register is one per port and a nested requester on the same
 * port would consume this caller's answer.
 *
 * A segment whose address falls outside both pools cannot be written to, but
 * it has still left the manager, so it is returned rather than dropped.
 *
 * On failure @pa distinguishes the two cases: zero for an exhausted pool,
 * the offending address for a segment that could not be translated.
 */
static void *xrx500_cbm_seg_get(struct xrx500_cbm *priv, u32 pid, u32 *pa)
{
	u32 off = eqm_cpu_off(pid, CBM_EQM_IGP_NEW_SPTR);
	unsigned long flags;
	unsigned int i = 0;
	u32 addr;
	void *va;

	if (priv->force_empty) {
		priv->force_empty--;
		priv->stats.pool_empty++;
		return NULL;
	}

	local_irq_save(flags);
	do {
		addr = cbm_r32(priv->eqm, off);
	} while ((addr & FSQM_POOL_EMPTY) == FSQM_POOL_EMPTY &&
		 i++ < FSQM_ALLOC_RETRIES);
	local_irq_restore(flags);

	if ((addr & FSQM_POOL_EMPTY) == FSQM_POOL_EMPTY) {
		priv->stats.pool_empty++;
		return NULL;
	}

	priv->stats.pops++;
	*pa = addr;

	va = xrx500_cbm_seg_to_virt(priv, *pa);
	if (!va) {
		net_err_ratelimited("%s: segment 0x%08x outside both pools\n",
				    netdev_name(priv->ndev), addr);
		xrx500_cbm_seg_put(priv, pid, addr);
		return NULL;
	}

	*pa = addr;
	return va;
}

/* Manager port bring-up */

/*
 * Arm one CPU ingress port of the enqueue manager
 * (AVM cqm/grx500/cbm.c:1335-1342).
 */
static void xrx500_cbm_eqm_cpu_port(struct xrx500_cbm *priv, u32 pid)
{
	cbm_w32(priv->eqm, eqm_cpu_off(pid, CBM_EQM_IGP_CFG),
		CBM_EQM_CPU_IGP_CFG_VAL);
	cbm_w32(priv->eqm, eqm_cpu_off(pid, CBM_EQM_IGP_POCC), 0);
	cbm_w32(priv->eqm, eqm_cpu_off(pid, CBM_EQM_IGP_IRNEN),
		CBM_EQM_CPU_IGP_IRNEN_ALL);
	wmb(); /* the port must be armed before it is requested through */
}

/*
 * Arm one CPU egress port of the dequeue manager.
 *
 * Every port a CPU can name has to be armed even when no queue or table entry
 * points at it, because it is a segment-return port: a return written into an
 * unconfigured port neither faults nor logs, it silently drops the segment
 * (AVM cqm/grx500/cbm.c:5753-5759, whose comment states the reason).
 */
static void xrx500_cbm_dqm_cpu_port(struct xrx500_cbm *priv, u32 pid)
{
	u32 off = dqm_cpu_off(pid, CBM_DQM_EGP_CFG);
	u32 cfg = CBM_DQM_CPU_EGP_CFG_VAL |
		  ((pid << CBM_DQM_EGP_CFG_EPMAP_SHIFT) &
		   CBM_DQM_EGP_CFG_EPMAP_MASK);
	u32 now = cbm_r32(priv->dqm, off);

	/*
	 * One of these ports carries the receive dequeue while the existing
	 * datapath is loaded, so the register is only written when it does not
	 * already hold the value this driver derives. A divergence is worth
	 * seeing: it means the two derivations disagree.
	 */
	if (now == cfg)
		return;

	dev_info(priv->dev, "return port %u: 0x%08x -> 0x%08x\n",
		 pid, now, cfg);
	cbm_w32(priv->dqm, off, cfg);
	wmb(); /* the port must be armed before a segment is returned */
}

/**
 * xrx500_cbm_dqm_dma_port() - bring up the DMA egress port that drains one of
 *                             this conduit's datapath ports.
 * @priv:       driver state.
 * @eg:         egress resources of the port.
 * @clear_desc: also clear the port's two staging descriptors.
 *
 * The port is configured with the dequeue request and per-port counting
 * enabled and its egress-port map pointing at itself
 * (AVM cqm/grx500/cbm.c:1378-1392). Both registers touched here live in the
 * dequeue-manager window and can be written at any time.
 *
 * The staging descriptors do not. They live in the descriptor window that the
 * transmit channel bound to this port reads from, and a CPU access to that
 * window while the channel is live wedges the bus. They are therefore cleared
 * only when this driver owns the channel and has turned it off first.
 */
static void xrx500_cbm_dqm_dma_port(struct xrx500_cbm *priv,
				    const struct xrx500_cbm_egress *eg,
				    bool clear_desc)
{
	u32 deq = eg->deq_port;
	u32 cfg;
	int i;

	if (clear_desc) {
		u32 base = CBM_DESC_EGP_5 + (deq - 5) * CBM_DESC_PORT_STRIDE;

		for (i = 0; i < CBM_DESC_PER_DMA_PORT; i++) {
			u32 d = base + i * 16;

			cbm_w32(priv->desc, d + 0x0, 0);
			cbm_w32(priv->desc, d + 0x4, 0);
			cbm_w32(priv->desc, d + 0x8, 0);
			cbm_w32(priv->desc, d + 0xC, 0);
		}
		wmb(); /* staging clear before the port dequeues */
	}

	cfg = CBM_DQM_DMA_EGP_CFG_DQREQ | CBM_DQM_DMA_EGP_CFG_DQPCEN |
	      ((deq << CBM_DQM_EGP_CFG_EPMAP_SHIFT) &
	       CBM_DQM_EGP_CFG_EPMAP_MASK);
	cbm_w32(priv->dqm,
		CBM_DQM_DMA_EGP_6 + (deq - 6) * CBM_DQM_PORT_STRIDE +
			CBM_DQM_EGP_CFG,
		cfg);
	wmb(); /* the port must be configured before the channel drains it */

	dev_info(priv->dev, "dequeue port %u configured (0x%08x)\n", deq, cfg);
}

/**
 * xrx500_cbm_pools_arm() - seed both segment pools and publish their bases.
 * @priv: driver state.
 *
 * The pool bases are written before the segment-size selector so the engine
 * cannot observe a base that disagrees with the size it is told to use. The
 * bus masters use the coherent alias of each base, and the physical form is
 * published alongside it because the descriptor engine reads both.
 */
static void xrx500_cbm_pools_arm(struct xrx500_cbm *priv)
{
	u32 ctrl;

	fsqm_arm(priv->fsqm[0], priv->std.segs);
	fsqm_arm(priv->fsqm[1], priv->jbo.segs);

	cbm_w32(priv->cbm, CBM_SBA_0, (u32)priv->std.phys);
	cbm_w32(priv->cbm, CBM_JBA_0, (u32)priv->jbo.phys);
	cbm_w32(priv->cbm, CBM_SBA_1, CBM_COHERENT_ALIAS(priv->std.phys));
	cbm_w32(priv->cbm, CBM_JBA_1, CBM_COHERENT_ALIAS(priv->jbo.phys));

	ctrl = cbm_r32(priv->cbm, CBM_CTRL);
	if (priv->jbo.seg_size == 0x2000)
		ctrl &= ~CBM_CTRL_JSEL;
	else
		ctrl |= CBM_CTRL_JSEL;
	cbm_w32(priv->cbm, CBM_CTRL, ctrl);
	wmb(); /* pool state must be complete before either manager runs */

	dev_info(priv->dev, "pools armed, %u free standard segments\n",
		 xrx500_cbm_free_segs(priv));
}

/**
 * xrx500_cbm_managers_enable() - enable both managers and the enqueue delay.
 * @priv: driver state.
 *
 * The dequeue manager is enabled before the enqueue manager so nothing can be
 * admitted before there is anything to drain it (AVM cqm/grx500/cbm.c:5605-
 * 5657). The enqueue delay counters follow, which is what lets the manager
 * coalesce an enqueue with the descriptor write that commits it.
 */
static void xrx500_cbm_managers_enable(struct xrx500_cbm *priv)
{
	u32 ctrl;
	int i;

	cbm_w32(priv->dqm, CBM_DQM_CTRL, CBM_DQM_CTRL_EN | CBM_DQM_CTRL_QEN);
	cbm_w32(priv->eqm, CBM_EQM_CTRL, CBM_EQM_CTRL_EN | CBM_EQM_CTRL_QEN);
	wmb(); /* both managers live before the delay counters are set */

	ctrl = cbm_r32(priv->eqm, CBM_EQM_CTRL);
	ctrl |= CBM_EQM_CTRL_PDEN | CBM_EQM_CTRL_SNOOPEN | CBM_EQM_CTRL_DLYSEL;
	cbm_w32(priv->eqm, CBM_EQM_CTRL, ctrl);
	for (i = 0; i <= 15; i++)
		cbm_w32(priv->eqm, eqm_cpu_off(i, CBM_EQM_IGP_DCNTR),
			CBM_EQM_IGP_DCNTR_DLY);
	wmb(); /* delay configuration complete before the first enqueue */
}

/* Transmit DMA channel */

/*
 * Descriptor base of a dequeue port inside the descriptor-staging window.
 * A transmit channel bound to that port must be given this address and a
 * length of two; it cannot be given a ring in DRAM.
 */
static phys_addr_t xrx500_cbm_deq_desc_pa(struct xrx500_cbm *priv,
					  const struct xrx500_cbm_egress *eg)
{
	return priv->dma_desc_pa + CBM_DESC_EGP_5 +
	       (phys_addr_t)(eg->deq_port - 5) * CBM_DESC_PORT_STRIDE;
}

/**
 * xrx500_cbm_dma_chan_check() - compare the channel against this driver's own
 *                               resource table.
 * @priv: driver state.
 * @eg:   egress resources of the port the channel drains.
 *
 * Reading the controller's own registers is safe at any time. Reading the
 * descriptor-staging window while the channel is live is not, and nothing
 * here does it.
 */
static void xrx500_cbm_dma_chan_check(struct xrx500_cbm *priv,
				      const struct xrx500_cbm_egress *eg)
{
	phys_addr_t want = xrx500_cbm_deq_desc_pa(priv, eg);
	u32 cctrl, cdba, cdlen;

	cbm_w32(priv->dma, DMA_CS, eg->dma_chan);
	cctrl = cbm_r32(priv->dma, DMA_CCTRL);
	cdba = cbm_r32(priv->dma, DMA_CDBA);
	cdlen = cbm_r32(priv->dma, DMA_CDLEN);

	dev_info(priv->dev,
		 "channel %u: control 0x%08x base 0x%08x length %u\n",
		 eg->dma_chan, cctrl, cdba, cdlen);

	if (cdba != (u32)want)
		dev_warn(priv->dev,
			 "channel %u descriptor base 0x%08x, expected 0x%08x\n",
			 eg->dma_chan, cdba, (u32)want);
	if (cdlen != CBM_DESC_PER_DMA_PORT)
		dev_warn(priv->dev,
			 "channel %u descriptor length %u, expected %u\n",
			 eg->dma_chan, cdlen, CBM_DESC_PER_DMA_PORT);
	if (!(cctrl & DMA_CCTRL_ON))
		dev_warn(priv->dev, "channel %u is off\n", eg->dma_chan);
	if (!(cctrl & DMA_CCTRL_DIR_TX))
		dev_warn(priv->dev, "channel %u is not a transmit channel\n",
			 eg->dma_chan);
}

/**
 * xrx500_cbm_dma_chan_setup() - point the channel at its dequeue port and
 *                               switch it on.
 * @priv: driver state.
 * @eg:   egress resources of the port the channel drains.
 *
 * The channel class equals the channel number for these bindings; the field
 * is split across two ranges of the control register
 * (AVM drivers/dma/intel/hdma.c:661-683). Interrupts are left masked: a
 * channel that fetches its descriptors on demand under manager flow control
 * signals nothing per frame, so there is no completion to take.
 *
 * On-demand descriptor fetch is enabled last, after the channel is otherwise
 * complete.
 */
#define XRX500_CBM_CHAN_OFF_TRIES	10000

static void xrx500_cbm_dma_chan_setup(struct xrx500_cbm *priv,
				      const struct xrx500_cbm_egress *eg)
{
	u32 chan = eg->dma_chan;
	phys_addr_t desc = xrx500_cbm_deq_desc_pa(priv, eg);
	u32 cctrl;
	int i;

	cbm_w32(priv->dma, DMA_CS, chan);
	cctrl = cbm_r32(priv->dma, DMA_CCTRL);
	cctrl &= ~DMA_CCTRL_ON;
	cbm_w32(priv->dma, DMA_CCTRL, cctrl);

	/*
	 * The enable bit clears itself only once a descriptor already in
	 * flight has finished, so the wait is not optional: touching the
	 * staging window while the channel is still running wedges the bus.
	 */
	for (i = 0; i < XRX500_CBM_CHAN_OFF_TRIES; i++) {
		if (!(cbm_r32(priv->dma, DMA_CCTRL) & DMA_CCTRL_ON))
			break;
		udelay(1);
	}
	if (i == XRX500_CBM_CHAN_OFF_TRIES) {
		dev_err(priv->dev,
			"channel %u will not stop; leaving it alone\n", chan);
		return;
	}

	xrx500_cbm_dqm_dma_port(priv, eg, true);

	cbm_w32(priv->dma, DMA_CS, chan);
	cbm_w32(priv->dma, DMA_CDBA, (u32)desc);
	cbm_w32(priv->dma, DMA_CDLEN, CBM_DESC_PER_DMA_PORT);
	cbm_w32(priv->dma, DMA_CIE, 0);

	cctrl &= ~(DMA_CCTRL_CLASS_MASK | DMA_CCTRL_CLASSH_MASK);
	cctrl |= ((chan & 0x7) << DMA_CCTRL_CLASS_SHIFT) &
		 DMA_CCTRL_CLASS_MASK;
	cctrl |= (((chan >> 3) & 0x3) << DMA_CCTRL_CLASSH_SHIFT) &
		 DMA_CCTRL_CLASSH_MASK;
	cbm_w32(priv->dma, DMA_CCTRL, cctrl);
	wmb(); /* descriptors and class in place before the channel runs */

	cbm_w32(priv->dma, DMA_CCTRL, cctrl | DMA_CCTRL_ON);

	/* On-demand descriptor fetch belongs at the end of controller setup. */
	cbm_w32(priv->dma, DMA_CTRL,
		cbm_r32(priv->dma, DMA_CTRL) | DMA_CTRL_DS_FOD);
	wmb(); /* fetch mode live before the first dequeue */

	dev_info(priv->dev,
		 "channel %u programmed: base 0x%08x length %u control 0x%08x\n",
		 chan, (u32)desc, CBM_DESC_PER_DMA_PORT,
		 cbm_r32(priv->dma, DMA_CCTRL));
}

/* Transmit */

/*
 * Back-pressure release.
 *
 * The egress channel raises no completion interrupt, so nothing tells the
 * driver that the buffer manager has recycled a segment. Two mechanisms could
 * release a queue stopped for an exhausted pool: a poll folded into the
 * receive softirq, or a poll of its own. This driver uses a poll of its own,
 * because a transmit-only flow generates no receive work and would then never
 * be released - a deadlock, not a latency question. The poll is single-shot
 * and rearms itself, so it costs nothing while the queue is running.
 */
#define XRX500_CBM_RESTART_DELAY	1

/*
 * Free segments the pool must hold before the queue is released. One segment
 * is enough to transmit one frame; the margin keeps the queue from being
 * released straight back into an exhausted pool.
 */
#define XRX500_CBM_RESTART_WATERMARK	8

static void xrx500_cbm_restart(struct timer_list *t)
{
	struct xrx500_cbm *priv = timer_container_of(priv, t, restart);
	bool forced;

	if (READ_ONCE(priv->closing))
		return;

	priv->stats.polls++;

	/*
	 * A synthetic exhausted-pool answer is consumed here as well as in the
	 * transmit path, under the lock that path holds while it takes one.
	 * The transmit path cannot run while the queue it stopped is stopped,
	 * so a count only that path spent would hold the queue closed for
	 * every answer past the first until it was cleared by hand.
	 */
	scoped_guard(spinlock_irqsave, &xrx500_cbm_tx_lock) {
		forced = priv->force_empty != 0;
		if (forced)
			priv->force_empty--;
	}

	if (forced ||
	    xrx500_cbm_free_segs(priv) < XRX500_CBM_RESTART_WATERMARK) {
		mod_timer(&priv->restart, jiffies + XRX500_CBM_RESTART_DELAY);
		return;
	}

	priv->stats.wakes++;
	netif_tx_wake_all_queues(priv->ndev);
}

/**
 * xrx500_cbm_select_queue() - decode the port the tagging driver chose.
 * @ndev:   the conduit.
 * @skb:    frame to transmit.
 * @sb_dev: unused.
 *
 * The tagging driver records the switch port in the socket buffer's queue
 * mapping, and this turns that into the index of the row holding the port's
 * dequeue port, traffic-management queue and DMA channel. A frame that names
 * no port lands on the queue that has none.
 *
 * The core skips this callback altogether on a device with a single transmit
 * queue and then overwrites the mapping with zero, which is why the conduit
 * is allocated with one queue per port and one more.
 */
static u16 xrx500_cbm_select_queue(struct net_device *ndev,
				   struct sk_buff *skb,
				   struct net_device *sb_dev)
{
	struct xrx500_cbm *priv = netdev_priv(ndev);
	u16 port = skb_get_queue_mapping(skb);
	unsigned int i;

	for (i = 0; i < priv->eg_num; i++)
		if (priv->eg[i].dp_port == port)
			return i + 1;

	return XRX500_CBM_TXQ_NONE;
}

/**
 * xrx500_cbm_xmit() - enqueue one frame into the buffer manager.
 * @skb:  frame to transmit.
 * @ndev: the conduit.
 *
 * The frame arrives with the switch header already in front of it and is
 * copied into the segment exactly as handed over; which port it leaves by is
 * in that header, and the egress resources it travels through come from the
 * queue the frame was placed on.
 *
 * There is no transmit completion on this path. The frame is copied into a
 * hardware-owned segment, one descriptor is written, and the socket buffer is
 * released immediately: the driver never owns the frame past the enqueue and
 * the hardware recycles the segment after egress. Consequently there is no
 * byte-queue accounting and no completion poll.
 *
 * The descriptor is committed by its last word. Everything else - the two
 * leading words and the data pointer - must be visible first
 * (AVM cqm/grx500/cbm.c:2560-2578).
 */
static netdev_tx_t xrx500_cbm_xmit(struct sk_buff *skb,
				   struct net_device *ndev)
{
	struct xrx500_cbm *priv = netdev_priv(ndev);
	const struct xrx500_cbm_egress *eg;
	unsigned long flags;
	u32 seg_pa = 0;
	u32 data_pa;
	u32 total;
	u32 dw1, dw3;
	u32 desc;
	u32 pid;
	u16 queue;
	u8 *buf;

	/*
	 * The frame offset is composed from a network-stack pad that is not an
	 * ABI constant. It has to stay clear of the switch header and leave a
	 * whole maximum-size frame inside one standard segment.
	 */
	BUILD_BUG_ON(XRX500_CBM_TX_OFFSET < XRX500_CBM_DMA_DATA_OFFSET);
	BUILD_BUG_ON(XRX500_CBM_TX_OFFSET + PMAC_TX_HDR_LEN +
		     VLAN_ETH_FRAME_LEN > XRX500_CBM_STD_SEG_SIZE);

	/*
	 * A frame from the local stack carries no switch header and names no
	 * port, and the switch would read its first eight bytes as one. The
	 * conduit is the processor's end of a switch datapath, not an
	 * interface of its own, so there is nowhere to send it.
	 */
	queue = skb_get_queue_mapping(skb);
	if (queue == XRX500_CBM_TXQ_NONE || queue > priv->eg_num) {
		priv->stats.noport++;
		goto drop;
	}
	eg = &priv->eg[queue - 1];

	if (skb_linearize(skb))
		goto drop;

	if (skb->len < XRX500_CBM_MIN_FRAME &&
	    skb_padto(skb, XRX500_CBM_MIN_FRAME))
		goto drop_nofree;
	if (skb->len < XRX500_CBM_MIN_FRAME)
		skb_put(skb, XRX500_CBM_MIN_FRAME - skb->len);

	total = skb->len;
	if (XRX500_CBM_TX_OFFSET + total > XRX500_CBM_STD_SEG_SIZE)
		goto drop;

	spin_lock_irqsave(&xrx500_cbm_tx_lock, flags);

	/*
	 * The segment request and the enqueue both address a per-port window,
	 * and the rollback return addresses the matching egress port, so all
	 * three have to name the same CPU. Interrupts are already off here,
	 * which pins it.
	 */
	pid = raw_smp_processor_id();
	if (pid >= CBM_CPU_PORT_NUM)
		pid = 0;

	buf = xrx500_cbm_seg_get(priv, pid, &seg_pa);
	if (!buf) {
		/*
		 * An exhausted pool is the only back pressure this path has.
		 * Stop the queue and let the release poll reopen it. A segment
		 * that could not be translated is a different failure and the
		 * frame is simply dropped.
		 */
		if (!seg_pa) {
			priv->stats.stops++;
			netif_tx_stop_all_queues(ndev);
			mod_timer(&priv->restart,
				  jiffies + XRX500_CBM_RESTART_DELAY);
			spin_unlock_irqrestore(&xrx500_cbm_tx_lock, flags);
			return NETDEV_TX_BUSY;
		}
		spin_unlock_irqrestore(&xrx500_cbm_tx_lock, flags);
		goto drop;
	}

	memcpy(buf + XRX500_CBM_TX_OFFSET, skb->data, skb->len);

	/*
	 * The segment pool is a reserved region carried with no kernel page
	 * mapping of its own, so the streaming DMA helpers cannot walk it and
	 * the write-back is issued against the pool mapping directly.
	 */
	dma_cache_wback((unsigned long)(buf + XRX500_CBM_TX_OFFSET), total);

	data_pa = seg_pa + XRX500_CBM_TX_OFFSET;

	dw1 = ((eg->dp_port << CBM_TXD_DW1_EP_SHIFT) &
	       CBM_TXD_DW1_EP_MASK) |
	      (CBM_TXD_DW1_COLOR_GREEN << CBM_TXD_DW1_COLOR_SHIFT);
	dw3 = CBM_TXD_DW3_SOP | CBM_TXD_DW3_EOP |
	      ((data_pa & 0x7) << CBM_TXD_DW3_BYTE_OFF_SHIFT) |
	      (total & CBM_TXD_DW3_LEN_MASK);

	desc = eqm_cpu_off(pid, CBM_EQM_IGP_DESC0);
	cbm_w32(priv->eqm, desc + 0x0, 0);
	cbm_w32(priv->eqm, desc + 0x4, dw1);
	cbm_w32(priv->eqm, desc + 0x8, data_pa & ~0x7U);
	wmb();  /* publish the descriptor body before the commit word */
	cbm_w32(priv->eqm, desc + 0xC, dw3);
	wmb();  /* make the commit globally visible */

	dev_sw_netstats_tx_add(ndev, 1, skb->len);
	spin_unlock_irqrestore(&xrx500_cbm_tx_lock, flags);

	dev_consume_skb_any(skb);
	return NETDEV_TX_OK;

drop:
	dev_kfree_skb_any(skb);
drop_nofree:
	dev_core_stats_tx_dropped_inc(ndev);
	return NETDEV_TX_OK;
}

/* Receive */

/*
 * Read one descriptor. The ring is mapped uncached, so no cache maintenance
 * belongs here, but the commit word has to be read before the fields it
 * guards, because it is what decides whether they mean anything.
 */
static void xrx500_cbm_rxd_read(const struct xrx500_cbm_rxd *src,
				struct xrx500_cbm_rxd *dst)
{
	dst->ctl = READ_ONCE(src->ctl);
	rmb();	/* ownership before the fields it guards */
	dst->dw0 = READ_ONCE(src->dw0);
	dst->dw1 = READ_ONCE(src->dw1);
	dst->addr = READ_ONCE(src->addr);
}

/*
 * Hand a descriptor to the engine: address, delimiters, offset and capacity
 * first with ownership still clear, then ownership on its own, so the engine
 * can never see a descriptor it owns whose address or length is stale
 * (AVM drivers/dma/intel/hdma.c ring seeding).
 */
static void xrx500_cbm_rxd_arm(struct xrx500_cbm_rxd *d, dma_addr_t addr,
			       u32 boff, u32 len)
{
	u32 ctl = CBM_RXD_CTL_SOP | CBM_RXD_CTL_EOP |
		  FIELD_PREP(CBM_RXD_CTL_BOFF, boff) |
		  FIELD_PREP(CBM_RXD_CTL_LEN, len);

	WRITE_ONCE(d->addr, (u32)addr);
	WRITE_ONCE(d->ctl, ctl);
	wmb();	/* the descriptor body before ownership transfers */
	WRITE_ONCE(d->ctl, ctl | CBM_RXD_CTL_OWN);
	wmb();	/* ownership before the next descriptor is touched */
}

/*
 * A descriptor the ring hands back malformed is counted twice over, on the
 * ring and on the interface, but a count does not say which field was wrong.
 * Dump the first few of them so that does not need a rebuild to find out.
 * Only the first few: a ring that fills with nonsense would otherwise cost a
 * line per descriptor, and the ring is abandoned after rx_errcap of them
 * anyway.
 */
#define XRX500_CBM_RX_TRACE_MAX		8

static void xrx500_cbm_rx_trace(struct xrx500_cbm *priv, unsigned int idx,
				const struct xrx500_cbm_rxd *d)
{
	u32 ctl = d->ctl;

	netdev_dbg(priv->ndev,
		   "rx desc[%u] dw0=0x%08x dw1=0x%08x addr=0x%08x ctl=0x%08x own=%u c=%u sop=%u eop=%u boff=%lu qid=%lu len=%lu\n",
		   idx, d->dw0, d->dw1, d->addr, ctl,
		   !!(ctl & CBM_RXD_CTL_OWN), !!(ctl & CBM_RXD_CTL_C),
		   !!(ctl & CBM_RXD_CTL_SOP), !!(ctl & CBM_RXD_CTL_EOP),
		   FIELD_GET(CBM_RXD_CTL_BOFF, ctl),
		   FIELD_GET(CBM_RXD_CTL_QID, ctl),
		   FIELD_GET(CBM_RXD_CTL_LEN, ctl));
}

/*
 * Stop consuming without disturbing anything else. The channel keeps its
 * ring and stays on; with no descriptor rearmed it fills the ring once and
 * then stalls, which leaves the descriptors that caused this readable.
 */
static void xrx500_cbm_rx_abort(struct xrx500_cbm *priv, const char *why)
{
	if (priv->rx.aborted)
		return;

	priv->rx.aborted = true;
	netdev_err(priv->ndev, "receive stopped: %s\n", why);
	netdev_err(priv->ndev,
		   "the ring is left as it is; no descriptor will be rearmed\n");
}

static bool xrx500_cbm_rxd_sane(const struct xrx500_cbm_rx *rx, u32 ctl)
{
	u32 len = FIELD_GET(CBM_RXD_CTL_LEN, ctl);
	u32 boff = FIELD_GET(CBM_RXD_CTL_BOFF, ctl);

	if (len < PMAC_RX_HDR_LEN + ETH_HLEN)
		return false;
	if (boff + len > rx->buf_len)
		return false;
	/* A frame that is neither the start nor the end of one is a split. */
	return (ctl & CBM_RXD_CTL_SOP) && (ctl & CBM_RXD_CTL_EOP);
}

/**
 * xrx500_cbm_rx_one() - deliver the frame in one completed descriptor.
 * @priv: driver state.
 * @idx:  descriptor index, which is also the ring buffer index.
 * @d:    the descriptor, already read out of the ring and checked.
 *
 * The frame is copied into a socket buffer rather than handed up on the ring
 * buffer itself. The switch header is eight bytes and the engine addresses
 * memory in eight-byte units, so a frame's Ethernet header can be aligned
 * for the engine or aligned for the network layer, but not for both; on this
 * architecture an unaligned network header is a fault rather than a slowdown,
 * so the copy is what makes the alignment right.
 *
 * The frame is handed up with the switch header still in front of it: the
 * tagging driver reads the source port out of it and pulls it, and the helper
 * that classifies an Ethernet frame consumes exactly the header plus the
 * first six bytes behind it, which is the offset that driver expects to find
 * it at. The source port is decoded here only to be counted.
 */
static void xrx500_cbm_rx_one(struct xrx500_cbm *priv, unsigned int idx,
			      const struct xrx500_cbm_rxd *d)
{
	struct xrx500_cbm_rx *rx = &priv->rx;
	struct xrx500_cbm_rx_buf *buf = &rx->buf[idx];
	u32 len = FIELD_GET(CBM_RXD_CTL_LEN, d->ctl);
	u32 boff = FIELD_GET(CBM_RXD_CTL_BOFF, d->ctl);
	struct net_device *ndev = priv->ndev;
	struct sk_buff *skb;
	const u8 *frame;
	u32 sppid;

	/*
	 * The engine wrote this buffer behind the processor's back. What makes
	 * the read below safe is that the buffer's cache lines were
	 * invalidated when it was handed to the device, and are invalidated
	 * again by the caller before it is handed back, so no line covering it
	 * can be resident and stale. The call is what the streaming interface
	 * requires here and is what carries that on cores that also need a
	 * post-transfer invalidate.
	 */
	dma_sync_single_range_for_cpu(priv->dev, buf->dma, 0, boff + len,
				      DMA_FROM_DEVICE);

	frame = (const u8 *)buf->va + boff;
	rx->stats.bytes += len;

	sppid = frame[2] >> 4;
	rx->sppid[sppid]++;

	skb = napi_alloc_skb(&rx->napi, len);
	if (!skb) {
		rx->stats.drop_noskb++;
		dev_core_stats_rx_dropped_inc(ndev);
		return;
	}

	memcpy(skb_put(skb, len), frame, len);
	dev_sw_netstats_rx_add(ndev, len - PMAC_RX_HDR_LEN);

	/*
	 * The protocol has to be taken from the return value. This helper
	 * classifies the frame, sets its device and consumes the Ethernet
	 * header, but it does not store the protocol; a socket buffer that
	 * reaches the stack without one matches no protocol handler and is
	 * dropped there, which counts against the device rather than against
	 * anything this driver can see. On a device serving a switch it
	 * answers with the protocol that routes the frame to the tagging
	 * driver instead of reading one out of the frame.
	 */
	skb->protocol = eth_type_trans(skb, ndev);
	rx->stats.delivered++;
	napi_gro_receive(&rx->napi, skb);
}

static int xrx500_cbm_rx_poll(struct napi_struct *napi, int budget)
{
	struct xrx500_cbm_rx *rx = container_of(napi, struct xrx500_cbm_rx,
						napi);
	struct xrx500_cbm *priv = container_of(rx, struct xrx500_cbm, rx);
	int done = 0;

	rx->stats.polls++;

	while (done < budget && !rx->aborted) {
		unsigned int idx = rx->next;
		struct xrx500_cbm_rxd d;

		xrx500_cbm_rxd_read(&rx->ring[idx], &d);

		/* Still the engine's, or back but not completed. */
		if ((d.ctl & CBM_RXD_CTL_OWN) || !(d.ctl & CBM_RXD_CTL_C))
			break;

		rx->stats.completions++;

		if (xrx500_cbm_rxd_sane(rx, d.ctl)) {
			xrx500_cbm_rx_one(priv, idx, &d);
		} else {
			rx->stats.malformed++;
			/*
			 * Also account it on the interface, so that a ring
			 * full of nonsense is visible to the ordinary tools
			 * rather than only to this driver's own report.
			 */
			priv->ndev->stats.rx_errors++;
			if (rx->traced < XRX500_CBM_RX_TRACE_MAX) {
				rx->traced++;
				xrx500_cbm_rx_trace(priv, idx, &d);
			}
			if (rx->stats.malformed >= rx_errcap) {
				xrx500_cbm_rx_abort(priv,
						    "too many bad descriptors");
				break;
			}
		}

		/*
		 * The buffer goes back to a device that does not snoop the
		 * caches, so it is handed over here rather than where the
		 * frame is consumed: a descriptor this driver rejected is
		 * rearmed too, and that path never looked at the buffer.
		 */
		dma_sync_single_range_for_device(priv->dev, rx->buf[idx].dma, 0,
						 rx->buf_len, DMA_FROM_DEVICE);

		xrx500_cbm_rxd_arm(&rx->ring[idx], rx->buf[idx].dma, rx->boff,
				   rx->buf_len - rx->boff);

		rx->next = idx + 1;
		if (rx->next == rx->ring_len) {
			rx->next = 0;
			rx->stats.wraps++;
		}
		done++;
	}

	/*
	 * Unmasking the channel is what makes the next frame arrive, so a
	 * failure here is a receive path that goes quiet with nothing to read.
	 * Say so instead.
	 */
	if (done < budget && napi_complete_done(napi, done) &&
	    !rx->aborted && READ_ONCE(rx->active) &&
	    ltq_dma_chan_irq_enable(priv->rx_chan))
		xrx500_cbm_rx_abort(priv, "channel interrupt not rearmed");

	return done;
}

/*
 * Channel completion. The driver that owns the receive controller masks the
 * channel's interrupt before calling this and runs it out of a softirq, so
 * scheduling the poll is all there is to do; the poll unmasks the channel
 * again when it has drained the ring.
 */
static int xrx500_cbm_rx_intr(u32 chan, void *data, int flags)
{
	struct xrx500_cbm *priv = data;

	if (priv && READ_ONCE(priv->rx.active) && !priv->rx.aborted)
		napi_schedule(&priv->rx.napi);

	return 0;
}

static void xrx500_cbm_rx_ring_free(struct xrx500_cbm *priv)
{
	struct xrx500_cbm_rx *rx = &priv->rx;
	unsigned int i;

	if (rx->buf) {
		for (i = 0; i < rx->ring_len; i++) {
			if (!rx->buf[i].va)
				continue;
			dma_unmap_single(priv->dev, rx->buf[i].dma,
					 rx->buf_len, DMA_FROM_DEVICE);
			kfree(rx->buf[i].va);
		}
		kfree(rx->buf);
		rx->buf = NULL;
	}

	if (rx->ring) {
		dma_free_coherent(priv->dev,
				  rx->ring_len * sizeof(*rx->ring), rx->ring,
				  rx->ring_pa);
		rx->ring = NULL;
	}
}

/**
 * xrx500_cbm_rx_ring_alloc() - build the receive ring and its buffers.
 * @priv: driver state.
 *
 * The coherent allocator is what the ring has to come from: on this platform
 * it returns an uncached mapping and refuses any page the uncached window
 * does not reach, which is exactly the constraint a descriptor ring is under.
 * The frame buffers have no such constraint and are ordinary allocations
 * mapped for the device, so they may live anywhere in memory.
 *
 * Return: 0 on success, negative errno otherwise.
 */
static int xrx500_cbm_rx_ring_alloc(struct xrx500_cbm *priv)
{
	struct xrx500_cbm_rx *rx = &priv->rx;
	unsigned int len = READ_ONCE(rx_ring_size);
	unsigned int boff = READ_ONCE(rx_boff);
	unsigned int i;

	/* Both are writable, so each is sampled once and then validated. */
	if (len < CBM_RX_RING_MIN || len > CBM_RX_RING_MAX) {
		netdev_err(priv->ndev, "receive ring size %u out of range\n",
			   len);
		return -EINVAL;
	}

	/* The descriptor's byte-offset field is three bits wide. */
	if (boff > FIELD_MAX(CBM_RXD_CTL_BOFF)) {
		netdev_err(priv->ndev, "receive byte offset %u out of range\n",
			   boff);
		return -EINVAL;
	}

	rx->ring_len = len;
	rx->boff = boff;

	/*
	 * A buffer has to hold the switch header, the largest frame the switch
	 * can deliver and the frame check sequence in case this direction ever
	 * keeps it, and it has to hold all of that behind the largest byte
	 * offset the engine can be asked for — the descriptor is armed with
	 * the remainder, so an offset shortens the frame that fits rather than
	 * growing the buffer. Anything longer than this is rejected as
	 * malformed, so the margin is what keeps a legitimate frame out of
	 * that count.
	 */
	rx->buf_len = ALIGN(PMAC_RX_HDR_LEN + VLAN_ETH_FRAME_LEN + ETH_FCS_LEN +
			    FIELD_MAX(CBM_RXD_CTL_BOFF), 8);
	rx->next = 0;
	rx->traced = 0;
	rx->aborted = false;
	memset(&rx->stats, 0, sizeof(rx->stats));
	memset(rx->sppid, 0, sizeof(rx->sppid));

	rx->ring = dma_alloc_coherent(priv->dev,
				      rx->ring_len * sizeof(*rx->ring),
				      &rx->ring_pa, GFP_KERNEL);
	if (!rx->ring)
		return -ENOMEM;

	rx->buf = kcalloc(rx->ring_len, sizeof(*rx->buf), GFP_KERNEL);
	if (!rx->buf)
		goto err;

	for (i = 0; i < rx->ring_len; i++) {
		/*
		 * A slab allocation of this size is aligned to a cache line
		 * and occupies whole lines, which is what a buffer mapped for
		 * a device that does not snoop the caches has to be.
		 */
		rx->buf[i].va = kmalloc(rx->buf_len, GFP_KERNEL);
		if (!rx->buf[i].va)
			goto err;

		rx->buf[i].dma = dma_map_single(priv->dev, rx->buf[i].va,
						rx->buf_len, DMA_FROM_DEVICE);
		if (dma_mapping_error(priv->dev, rx->buf[i].dma)) {
			kfree(rx->buf[i].va);
			rx->buf[i].va = NULL;
			goto err;
		}

		xrx500_cbm_rxd_arm(&rx->ring[i], rx->buf[i].dma, rx->boff,
				   rx->buf_len - rx->boff);
	}

	netdev_info(priv->ndev,
		    "receive ring at %pad: %u descriptors of %u bytes, offset %u\n",
		    &rx->ring_pa, rx->ring_len, rx->buf_len, rx->boff);
	return 0;

err:
	xrx500_cbm_rx_ring_free(priv);
	return -ENOMEM;
}

/**
 * xrx500_cbm_rx_take() - point the switch's receive channel at this ring.
 * @priv: driver state.
 *
 * The channel has been running against a ring of its own, so its internal
 * descriptor index is wherever the last frame left it. Giving it a new base
 * does not move that index, and a consumer starting at descriptor zero
 * against an engine starting anywhere else looks exactly like a receive path
 * that delivers nothing until the engine happens to lap the ring. So the
 * channel is reset first, which is also what puts both sides at zero.
 *
 * A ring that had a consumer of its own names it, and that consumer is
 * stopped before the base changes and after the reset, so a controller that
 * is not there at all is discovered while the existing path is still whole.
 *
 * Return: 0 on success, negative errno otherwise.
 */
static int xrx500_cbm_rx_take(struct xrx500_cbm *priv)
{
	dma_addr_t cur;
	int ret;

	ret = ltq_dma_chan_reset(priv->rx_chan);
	if (ret)
		return ret;

	if (priv->rx_relay != CBM_RX_RELAY_NONE) {
		ltq_dma_chan_irq_disable(priv->rx_relay);
		ltq_dma_chan_off(priv->rx_relay);
	}

	/*
	 * Release the ring the channel used to have, so that the bookkeeping
	 * the other driver keeps for it stops describing a ring this one now
	 * owns the base of.
	 */
	ltq_dma_chan_desc_free(priv->rx_chan);

	ret = ltq_dma_chan_desc_cfg(priv->rx_chan, priv->rx.ring_pa,
				    priv->rx.ring_len);
	if (ret)
		goto err;

	ret = ltq_dma_chan_on(priv->rx_chan);
	if (ret)
		goto err;

	ret = ltq_dma_chan_irq_enable(priv->rx_chan);
	if (ret)
		goto err;

	cur = ltq_dma_chan_get_curr_desc_addr(priv->rx_chan);
	netdev_info(priv->ndev, "receive channel taken, engine at %pad\n", &cur);
	return 0;

err:
	netdev_err(priv->ndev,
		   "receive channel not taken: %d; the existing receive path is down until a reboot\n",
		   ret);
	return ret;
}

static void xrx500_cbm_rx_release(struct xrx500_cbm *priv)
{
	ltq_dma_chan_irq_disable(priv->rx_chan);
	ltq_dma_chan_off(priv->rx_chan);
}

/* Network device */

static int xrx500_cbm_open(struct net_device *ndev)
{
	struct xrx500_cbm *priv = netdev_priv(ndev);
	int ret;

	WRITE_ONCE(priv->closing, false);

	if (own_rx) {
		scoped_guard(mutex, &xrx500_cbm_lock)
			ret = xrx500_cbm_rx_ring_alloc(priv);
		if (ret)
			return ret;

		napi_enable(&priv->rx.napi);
		WRITE_ONCE(priv->rx.active, true);

		ret = xrx500_cbm_rx_take(priv);
		if (ret) {
			WRITE_ONCE(priv->rx.active, false);
			xrx500_cbm_rx_release(priv);
			napi_disable(&priv->rx.napi);
			scoped_guard(mutex, &xrx500_cbm_lock)
				xrx500_cbm_rx_ring_free(priv);
			return ret;
		}

		/* Anything the engine completed before the poll existed. */
		napi_schedule(&priv->rx.napi);
	}

	netif_tx_start_all_queues(ndev);
	/*
	 * The conduit's link is the switch's CPU port, which has no
	 * negotiation and is up whenever the datapath is.
	 */
	netif_carrier_on(ndev);
	return 0;
}

static int xrx500_cbm_stop(struct net_device *ndev)
{
	struct xrx500_cbm *priv = netdev_priv(ndev);

	netif_carrier_off(ndev);
	netif_tx_stop_all_queues(ndev);

	/*
	 * The release poll rearms itself, and it cannot be shut down here:
	 * shutting a timer down is permanent, so a device that came up again
	 * would have a release poll that never runs and a transmit queue that
	 * stays stopped the first time the segment pool empties. Marking the
	 * device as going down instead stops the poll rearming, and the wait
	 * then drains it.
	 */
	WRITE_ONCE(priv->closing, true);
	timer_delete_sync(&priv->restart);

	if (READ_ONCE(priv->rx.active)) {
		/*
		 * Order matters: stop new polls being scheduled, then stop
		 * the engine, then wait for the poll that may be running, and
		 * only then release what it reads. A poll already past its
		 * completion can rearm the channel interrupt behind the
		 * release, so the mask is reasserted once nothing can poll.
		 */
		WRITE_ONCE(priv->rx.active, false);
		xrx500_cbm_rx_release(priv);
		napi_disable(&priv->rx.napi);
		ltq_dma_chan_irq_disable(priv->rx_chan);
		scoped_guard(mutex, &xrx500_cbm_lock)
			xrx500_cbm_rx_ring_free(priv);
	}

	return 0;
}

/*
 * Counters this driver keeps because the hardware keeps none. The transmit
 * path has no completion at all, so what it accounts for is the segment: how
 * many left the manager, how many came back unused, and how many are with the
 * hardware right now. The receive path accounts for the descriptor, because a
 * descriptor the engine completed and this driver rejected never reaches the
 * frame counters the stack keeps.
 *
 * The last three are the state of the ring rather than a count, and the
 * per-source-port block is the demultiplexing the switch header drives: on a
 * conduit serving a switch it says which port each frame came in on, and it is
 * how a delivery that goes to the wrong user device is caught.
 */
static const char xrx500_cbm_stat_strings[][ETH_GSTRING_LEN] = {
	"tx_segments_taken",
	"tx_segments_returned",
	"tx_segments_outstanding",
	"tx_pool_empty",
	"tx_queue_stops",
	"tx_queue_wakes",
	"tx_restart_polls",
	"tx_dropped_no_port",
	"tx_pool_free",
	"rx_polls",
	"rx_completions",
	"rx_completed_bytes",
	"rx_delivered",
	"rx_ring_wraps",
	"rx_malformed",
	"rx_dropped_no_buffer",
	"rx_ring_armed",
	"rx_ring_completed",
	"rx_ring_aborted",
};

#define XRX500_CBM_STAT_NUM	(ARRAY_SIZE(xrx500_cbm_stat_strings) + \
				 PMAC_RX_SPPID_NUM)

static int xrx500_cbm_get_sset_count(struct net_device *ndev, int sset)
{
	if (sset != ETH_SS_STATS)
		return -EOPNOTSUPP;

	return XRX500_CBM_STAT_NUM;
}

static void xrx500_cbm_get_strings(struct net_device *ndev, u32 sset, u8 *data)
{
	unsigned int i;

	if (sset != ETH_SS_STATS)
		return;

	for (i = 0; i < ARRAY_SIZE(xrx500_cbm_stat_strings); i++)
		ethtool_puts(&data, xrx500_cbm_stat_strings[i]);

	for (i = 0; i < PMAC_RX_SPPID_NUM; i++)
		ethtool_sprintf(&data, "rx_source_port_%u", i);
}

static void xrx500_cbm_get_ethtool_stats(struct net_device *ndev,
					 struct ethtool_stats *stats, u64 *data)
{
	struct xrx500_cbm *priv = netdev_priv(ndev);
	struct xrx500_cbm_rx *rx = &priv->rx;
	unsigned int armed = 0;
	unsigned int done = 0;
	unsigned int i;

	/*
	 * The ring is built when the device comes up and released when it goes
	 * down, and this runs under the same lock the network stack holds
	 * across both, so it is either there for the whole of this walk or
	 * absent for the whole of it. Testing the pointer once says that; a
	 * test inside the loop would suggest a race this cannot be in, and
	 * would not close it if it were.
	 */
	if (rx->ring) {
		for (i = 0; i < rx->ring_len; i++) {
			u32 ctl = READ_ONCE(rx->ring[i].ctl);

			if (ctl & CBM_RXD_CTL_OWN)
				armed++;
			else if (ctl & CBM_RXD_CTL_C)
				done++;
		}
	}

	i = 0;
	data[i++] = priv->stats.pops;
	data[i++] = priv->stats.rollbacks;
	data[i++] = priv->stats.pops - priv->stats.rollbacks;
	data[i++] = priv->stats.pool_empty;
	data[i++] = priv->stats.stops;
	data[i++] = priv->stats.wakes;
	data[i++] = priv->stats.polls;
	data[i++] = priv->stats.noport;
	data[i++] = xrx500_cbm_free_segs(priv);
	data[i++] = rx->stats.polls;
	data[i++] = rx->stats.completions;
	data[i++] = rx->stats.bytes;
	data[i++] = rx->stats.delivered;
	data[i++] = rx->stats.wraps;
	data[i++] = rx->stats.malformed;
	data[i++] = rx->stats.drop_noskb;
	data[i++] = armed;
	data[i++] = done;
	data[i++] = rx->aborted;

	memcpy(&data[i], rx->sppid, sizeof(rx->sppid));
}

static const struct ethtool_ops xrx500_cbm_ethtool_ops = {
	.get_link		= ethtool_op_get_link,
	.get_sset_count		= xrx500_cbm_get_sset_count,
	.get_strings		= xrx500_cbm_get_strings,
	.get_ethtool_stats	= xrx500_cbm_get_ethtool_stats,
};

static const struct net_device_ops xrx500_cbm_netdev_ops = {
	.ndo_open = xrx500_cbm_open,
	.ndo_stop = xrx500_cbm_stop,
	.ndo_start_xmit = xrx500_cbm_xmit,
	.ndo_select_queue = xrx500_cbm_select_queue,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};

/* Parameter callbacks */

static int xrx500_cbm_set_force_empty(const char *val,
				      const struct kernel_param *kp)
{
	struct xrx500_cbm *priv;
	int ret = param_set_uint(val, kp);

	if (ret)
		return ret;

	guard(mutex)(&xrx500_cbm_lock);
	list_for_each_entry(priv, &xrx500_cbm_conduits, node)
		scoped_guard(spinlock_irqsave, &xrx500_cbm_tx_lock)
			priv->force_empty = tx_force_empty;
	return 0;
}

/* Probe */

/**
 * xrx500_cbm_map() - map one named window of a referenced node.
 * @priv: driver state.
 * @np:   node carrying the windows.
 * @name: reg-names entry to map.
 * @pa:   optional out, physical base of the window.
 *
 * The window is mapped without claiming it. The buffer manager's windows are
 * described once and shared while this driver runs beside the existing
 * datapath; the claim moves here when that driver goes away.
 *
 * Resolution is by name, never by index: the same node's windows are fetched
 * by index elsewhere, which makes a reorder of the list a silent mismapping.
 */
static void __iomem *xrx500_cbm_map(struct xrx500_cbm *priv,
				    struct device_node *np, const char *name,
				    phys_addr_t *pa)
{
	struct resource res;
	void __iomem *base;
	int idx;
	int ret;

	idx = of_property_match_string(np, "reg-names", name);
	if (idx < 0) {
		dev_err(priv->dev, "%pOF has no window named %s\n", np, name);
		return IOMEM_ERR_PTR(idx);
	}

	ret = of_address_to_resource(np, idx, &res);
	if (ret) {
		dev_err(priv->dev, "%pOF window %s unresolvable: %d\n",
			np, name, ret);
		return IOMEM_ERR_PTR(ret);
	}

	base = devm_ioremap(priv->dev, res.start, resource_size(&res));
	if (!base) {
		dev_err(priv->dev, "%pOF window %s not mappable\n", np, name);
		return IOMEM_ERR_PTR(-ENOMEM);
	}

	if (pa)
		*pa = res.start;
	return base;
}

/**
 * xrx500_cbm_pool_get() - resolve and map one segment pool.
 * @priv:     driver state.
 * @np:       node carrying the memory-region list.
 * @name:     memory-region-names entry.
 * @seg_size: segment size the manager hands out of this pool.
 * @pool:     out.
 *
 * The regions are reserved with no kernel mapping, so the write-back mapping
 * taken here is the only access path to them.
 */
static int xrx500_cbm_pool_get(struct xrx500_cbm *priv, struct device_node *np,
			       const char *name, u32 seg_size,
			       struct xrx500_cbm_pool *pool)
{
	struct device_node *rnp;
	struct reserved_mem *rmem;
	phys_addr_t shift;
	void *virt;
	int idx;

	idx = of_property_match_string(np, "memory-region-names", name);
	if (idx < 0) {
		dev_err(priv->dev, "no memory region named %s\n", name);
		return idx;
	}

	rnp = of_parse_phandle(np, "memory-region", idx);
	if (!rnp) {
		dev_err(priv->dev, "memory region %s unresolvable\n", name);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(rnp);
	of_node_put(rnp);
	if (!rmem) {
		dev_err(priv->dev, "memory region %s not reserved\n", name);
		return -ENODEV;
	}

	if (rmem->size < (phys_addr_t)seg_size * 2) {
		dev_err(priv->dev, "memory region %s holds under two segments\n",
			name);
		return -EINVAL;
	}

	virt = devm_memremap(priv->dev, rmem->base, rmem->size, MEMREMAP_WB);
	if (IS_ERR_OR_NULL(virt)) {
		dev_err(priv->dev, "memory region %s not mappable\n", name);
		return virt ? PTR_ERR(virt) : -ENOMEM;
	}

	/*
	 * The manager addresses the pool in whole segments from its base, so
	 * the base has to sit on a segment boundary. The shift is applied to
	 * the mapping and the physical base together, or the two translations
	 * would disagree by the adjustment.
	 */
	shift = ALIGN(rmem->base, seg_size) - rmem->base;
	if (shift >= rmem->size) {
		dev_err(priv->dev, "memory region %s cannot be aligned\n",
			name);
		return -EINVAL;
	}

	pool->phys = rmem->base + shift;
	pool->virt = (u8 *)virt + shift;
	pool->size = rmem->size - shift;
	pool->seg_size = seg_size;
	pool->segs = pool->size / seg_size;

	dev_info(priv->dev, "%s at %pa, %u segments of %u bytes\n",
		 name, &pool->phys, pool->segs, pool->seg_size);
	return 0;
}

/**
 * xrx500_cbm_dma_win() - map the register window of a referenced controller.
 * @priv: driver state.
 * @name: phandle property naming the controller.
 * @cid:  optional out, the controller's identity.
 */
static void __iomem *xrx500_cbm_dma_win(struct xrx500_cbm *priv,
					const char *name, int *cid)
{
	struct device_node *np;
	struct resource res;
	void __iomem *base;
	int ret;

	np = of_parse_phandle(priv->dev->of_node, name, 0);
	if (!np) {
		dev_err(priv->dev, "no %s phandle\n", name);
		return IOMEM_ERR_PTR(-ENODEV);
	}

	ret = of_address_to_resource(np, 0, &res);
	of_node_put(np);
	if (ret) {
		dev_err(priv->dev, "%s window unresolvable: %d\n", name, ret);
		return IOMEM_ERR_PTR(ret);
	}

	if (cid) {
		*cid = ltq_dma_ctrl_id_by_phys(res.start);
		if (*cid < 0) {
			dev_err(priv->dev, "%s is not a packet DMA controller\n",
				name);
			return IOMEM_ERR_PTR(*cid);
		}
	}

	base = devm_ioremap(priv->dev, res.start, resource_size(&res));
	if (!base) {
		dev_err(priv->dev, "%s window not mappable\n", name);
		return IOMEM_ERR_PTR(-ENOMEM);
	}

	return base;
}

static int xrx500_cbm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *mgr = np->parent;
	const struct xrx500_cbm_macro *macro;
	struct net_device *ndev;
	struct xrx500_cbm *priv;
	void __iomem *rx_win;
	int rx_cid;
	u32 index;
	int ret;
	u32 i;

	/*
	 * The conduit's own register value names the switch macro it serves,
	 * which is what decides the set of egress resources it has.
	 */
	ret = of_property_read_u32(np, "reg", &index);
	if (ret) {
		dev_err(dev, "no switch macro index\n");
		return ret;
	}
	if (index >= ARRAY_SIZE(xrx500_cbm_macros)) {
		dev_err(dev, "switch macro %u has no conduit resources\n",
			index);
		return -EINVAL;
	}
	macro = &xrx500_cbm_macros[index];

	/*
	 * One transmit queue per port the conduit serves, plus the queue that
	 * carries no port. A single-queue device would never reach the queue
	 * callback and every frame would go to the first port.
	 */
	ndev = alloc_etherdev_mqs(sizeof(*priv), macro->num + 1, 1);
	if (!ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, dev);
	priv = netdev_priv(ndev);
	priv->dev = dev;
	priv->ndev = ndev;
	priv->eg = macro->eg;
	priv->eg_num = macro->num;
	priv->rx_relay = macro->rx_relay;
	INIT_LIST_HEAD(&priv->node);
	timer_setup(&priv->restart, xrx500_cbm_restart, 0);

	/*
	 * The windows and the segment pools belong to the buffer manager, and
	 * this device is a child of the node that describes them, so they are
	 * read off the parent rather than followed through a phandle.
	 */
	priv->cbm = xrx500_cbm_map(priv, mgr, "cbm", NULL);
	priv->eqm = xrx500_cbm_map(priv, mgr, "eqm", NULL);
	priv->dqm = xrx500_cbm_map(priv, mgr, "dqm", NULL);
	priv->desc = xrx500_cbm_map(priv, mgr, "dma-desc", &priv->dma_desc_pa);
	priv->qidt = xrx500_cbm_map(priv, mgr, "qidt", NULL);
	priv->tmu = xrx500_cbm_map(priv, mgr, "tmu", NULL);
	priv->fsqm[0] = xrx500_cbm_map(priv, mgr, "fsqm0", NULL);
	priv->fsqm[1] = xrx500_cbm_map(priv, mgr, "fsqm1", NULL);

	if (IS_ERR(priv->cbm) || IS_ERR(priv->eqm) || IS_ERR(priv->dqm) ||
	    IS_ERR(priv->desc) || IS_ERR(priv->qidt) || IS_ERR(priv->tmu) ||
	    IS_ERR(priv->fsqm[0]) || IS_ERR(priv->fsqm[1])) {
		ret = -ENODEV;
		goto err_free;
	}

	ret = xrx500_cbm_pool_get(priv, mgr, "std-pool",
				  XRX500_CBM_STD_SEG_SIZE, &priv->std);
	if (!ret)
		ret = xrx500_cbm_pool_get(priv, mgr, "jumbo-pool",
					  XRX500_CBM_JBO_SEG_SIZE, &priv->jbo);
	if (ret)
		goto err_free;

	priv->dma = xrx500_cbm_dma_win(priv, "lantiq,transmit-dma", NULL);
	if (IS_ERR(priv->dma)) {
		ret = PTR_ERR(priv->dma);
		goto err_free;
	}
	if (!cbm_r32(priv->dma, DMA_ID) ||
	    cbm_r32(priv->dma, DMA_ID) == 0xFFFFFFFFU) {
		dev_err(dev, "transmit DMA controller does not answer\n");
		ret = -ENODEV;
		goto err_free;
	}

	/*
	 * Only the receive controller's identity is wanted here: the ring is
	 * driven through the channel interface, which addresses it by handle.
	 */
	rx_win = xrx500_cbm_dma_win(priv, "lantiq,receive-dma", &rx_cid);
	if (IS_ERR(rx_win)) {
		ret = PTR_ERR(rx_win);
		goto err_free;
	}
	priv->rx_chan = _DMA_C(rx_cid, CBM_RX_CHAN_PORT, CBM_RX_CHAN_NR);

	/*
	 * Without ownership the segment pools have to be live already, or the
	 * first transmit would find an empty free list.
	 */
	if (!own_cbm && !xrx500_cbm_free_segs(priv)) {
		dev_info(dev, "segment pools not armed yet\n");
		ret = -EPROBE_DEFER;
		goto err_free;
	}

	/*
	 * Bring-up order, and it is load bearing: the segment pools, then the
	 * ports, then the queue-index table and the egress path, and only then
	 * the managers, with the DMA channel last of all. Enabling a manager
	 * before its endpoint has a queue-index entry and a traffic-management
	 * queue leaves it admitting into a table nothing has written yet
	 * (AVM cqm/grx500/cbm.c splits its own controller enable out of the
	 * hardware init for exactly this reason).
	 *
	 * Adding one datapath port to managers that are already running keeps
	 * the tail of the same order and skips the steps that would restart
	 * what is running.
	 */
	if (own_cbm) {
		xrx500_cbm_pools_arm(priv);
		xrx500_cbm_qidt_drop_fill(priv);
	}

	/*
	 * Rearming the CPU ports makes this driver state its own requirements
	 * rather than inherit them; the writes are the same values the running
	 * configuration already holds.
	 */
	for (i = 0; i < CBM_CPU_PORT_NUM; i++)
		xrx500_cbm_dqm_cpu_port(priv, i);
	for (i = 0; i < CBM_CPU_PORT_NUM; i++)
		xrx500_cbm_eqm_cpu_port(priv, i);

	/*
	 * Every port the conduit serves gets its own dequeue port, egress path
	 * and queue-index entry, because the frame's egress port is chosen per
	 * frame and the resources behind each are separate.
	 */
	for (i = 0; i < priv->eg_num; i++) {
		const struct xrx500_cbm_egress *eg = &priv->eg[i];

		xrx500_cbm_dqm_dma_port(priv, eg, false);

		ret = xrx500_cbm_tmu_egress_path(priv, eg);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
					"egress path for port %u not built: %d\n",
					eg->dp_port, ret);
			goto err_qidt;
		}

		xrx500_cbm_qidt_set(priv, eg->dp_port, (u8)eg->tmu_qid);
	}

	if (own_cbm)
		xrx500_cbm_managers_enable(priv);

	for (i = 0; i < priv->eg_num; i++) {
		if (own_dma_chan)
			xrx500_cbm_dma_chan_setup(priv, &priv->eg[i]);
		else
			xrx500_cbm_dma_chan_check(priv, &priv->eg[i]);
	}

	netif_napi_add(ndev, &priv->rx.napi, xrx500_cbm_rx_poll);

	/*
	 * The completion callback is registered once and for the lifetime of
	 * the binding, rather than when the device comes up, so that going
	 * down and up again does not repeatedly install and clear a callback
	 * in a driver this one does not own. It does nothing until the ring
	 * is live, which is what the ownership flag it reads is for.
	 */
	if (own_rx) {
		ret = ltq_dma_chan_pseudo_irq_handler_callback_cfg(priv->rx_chan,
								   xrx500_cbm_rx_intr,
								   priv);
		if (ret) {
			dev_err(dev, "receive completion callback: %d\n", ret);
			goto err_qidt;
		}
	}

	ndev->netdev_ops = &xrx500_cbm_netdev_ops;
	ndev->ethtool_ops = &xrx500_cbm_ethtool_ops;
	ndev->pcpu_stat_type = NETDEV_PCPU_STAT_TSTATS;
	ndev->needed_headroom = 0;
	ndev->min_mtu = ETH_MIN_MTU;
	ndev->max_mtu = XRX500_CBM_MAX_MTU;
	ndev->mtu = ETH_DATA_LEN;
	strscpy(ndev->name, "cbm%d", IFNAMSIZ);
	eth_hw_addr_random(ndev);
	netif_carrier_off(ndev);

	ret = register_netdev(ndev);
	if (ret) {
		dev_err(dev, "register_netdev: %d\n", ret);
		goto err_qidt;
	}

	platform_set_drvdata(pdev, priv);
	scoped_guard(mutex, &xrx500_cbm_lock)
		list_add_tail(&priv->node, &xrx500_cbm_conduits);

	for (i = 0; i < priv->eg_num; i++)
		dev_info(dev,
			 "%s port %u on queue %u: dequeue %u traffic queue %u channel %u\n",
			 netdev_name(ndev), priv->eg[i].dp_port, i + 1,
			 priv->eg[i].deq_port, priv->eg[i].tmu_qid,
			 priv->eg[i].dma_chan);
	dev_info(dev, "%s ready, %u free segments\n", netdev_name(ndev),
		 xrx500_cbm_free_segs(priv));
	return 0;

err_qidt:
	for (i = 0; i < priv->eg_num; i++)
		xrx500_cbm_qidt_set(priv, priv->eg[i].dp_port, 0);
err_free:
	timer_shutdown_sync(&priv->restart);
	free_netdev(ndev);
	return ret;
}

static void xrx500_cbm_remove(struct platform_device *pdev)
{
	struct xrx500_cbm *priv = platform_get_drvdata(pdev);
	unsigned int i;

	mutex_lock(&xrx500_cbm_lock);
	list_del(&priv->node);
	mutex_unlock(&xrx500_cbm_lock);

	unregister_netdev(priv->ndev);

	/*
	 * The completion callback is held by the receive controller's driver,
	 * which outlives this one, so its data pointer has to stop referring
	 * to state that is about to be freed. The channel is already off and
	 * its interrupt masked by the time this runs, so nothing can raise a
	 * new completion; a completion already being dispatched when this
	 * store lands is a window only an unbind can reach, and this driver is
	 * built in.
	 */
	if (own_rx)
		ltq_dma_chan_pseudo_irq_handler_callback_cfg(priv->rx_chan,
							     xrx500_cbm_rx_intr,
							     NULL);

	timer_shutdown_sync(&priv->restart);
	for (i = 0; i < priv->eg_num; i++)
		xrx500_cbm_qidt_set(priv, priv->eg[i].dp_port, 0);
	free_netdev(priv->ndev);
}

static const struct of_device_id xrx500_cbm_of_match[] = {
	{ .compatible = "lantiq,xrx500-conduit" },
	{ }
};
MODULE_DEVICE_TABLE(of, xrx500_cbm_of_match);

static struct platform_driver xrx500_cbm_driver = {
	.probe = xrx500_cbm_probe,
	.remove = xrx500_cbm_remove,
	.driver = {
		.name = XRX500_CBM_DRV_NAME,
		.of_match_table = xrx500_cbm_of_match,
	},
};
module_platform_driver(xrx500_cbm_driver);

MODULE_DESCRIPTION("xRX500 buffer-manager conduit network device");
MODULE_LICENSE("GPL");
