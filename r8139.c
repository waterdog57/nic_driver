// SPDX-License-Identifier: GPL-2.0
/*
 * nic_driver.c - Basic template for a PCI/PCIe Ethernet NIC driver.
 *
 * This is a skeleton: it registers a PCI driver, brings up a net_device,
 * and wires up the ndo_/NAPI/interrupt plumbing with TODO stubs where
 * hardware-specific register access belongs. Update PCI_VENDOR_ID /
 * PCI_DEVICE_ID and the TODO sections for your actual chip.
 */
#define DEBUG

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include "r8139.h"

#define DRV_NAME "waterdog_driver"
#define DRV_VERSION "0.1"

/* TODO: replace with your device's real vendor/device ID */
#define MY_VENDOR_ID 0x10ec
#define MY_DEVICE_ID 0x8139

#define NAPI_WEIGHT 64

struct nic_priv {
	struct pci_dev *pdev;
	struct net_device *ndev;
	void __iomem *hw_addr; /* BAR0 mapped registers */
	struct napi_struct napi;
	spinlock_t lock;
	int irq;

	/* TODO: real TX/RX descriptor rings, DMA buffers, etc. go here */
	/* TX Ring Control */
	u8 *tx_buf[NUM_TX_DESC]; /* Virtual addresses for DMA buffers */
	dma_addr_t tx_buf_dma[NUM_TX_DESC]; /* Physical DMA addresses */
	unsigned int tx_head; /* Head index for next TX descriptor (0~3) */
	unsigned int tx_tail; /* Tail index for next TX cleanup (0~3) */
	unsigned int tx_free; /* Number of remaining available slots */
	/*  RX Ring Control */
	u8 *rx_buf; /* Virtual addresses for DMA buffers */
	dma_addr_t rx_buf_dma; /* Physical DMA addresses */
	unsigned int rx_offset; /* Head index for next RX descriptor (0~63) */
};

/* ---------------------------------------------------------------------
 * NAPI poll / interrupt handling
 * ------------------------------------------------------------------- */

/* Program RBSTART/CAPR and reset the software read cursor. Used both at
 * initial bring-up and to recover after the chip hands us a corrupted Rx
 * header (see the "Bug?" comment in the reference 8139too.c driver). */
static void nic_rx_ring_init(struct nic_priv *priv)
{
	writel(priv->rx_buf_dma, priv->hw_addr + RTL_REG_RBSTART);
	writel(RTL8139_CAPR_INIT, priv->hw_addr + RTL_REG_CAPR);
	priv->rx_offset = 0;
}

static void nic_rx_reset(struct nic_priv *priv)
{
	u8 cmd = readb(priv->hw_addr + RTL_REG_COMMAND);
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	writeb(cmd & ~CR_RE, priv->hw_addr + RTL_REG_COMMAND);
	nic_rx_ring_init(priv);
	writeb(cmd | CR_RE, priv->hw_addr + RTL_REG_COMMAND);
	spin_unlock_irqrestore(&priv->lock, flags);
}

static int nic_poll(struct napi_struct *napi, int budget)
{
	struct nic_priv *priv = netdev_priv(napi->dev);
	u16 tmp;
	u32 rx_status;
	u16 pkt_len;
	u8 *rx_ring = priv->rx_buf;
	struct sk_buff *skb;
	int work_done = 0;
	u32 ring_offset;

	while (work_done < budget) {
		if (readb(priv->hw_addr + RTL_REG_COMMAND) & CR_BUFE) {
			break;
		}
		ring_offset = priv->rx_offset % RX_BUF_LEN;

		rmb(); /* Ensure we read the latest data from the NIC's RX buffer */
		rx_status = le16_to_cpu(*(u16 *)(rx_ring + ring_offset));
		pkt_len = le16_to_cpu(*(u16 *)(rx_ring + ring_offset + 2));

		if (pkt_len < 4 || pkt_len > MAX_ETH_FRAME_SIZE + 4) {
			netdev_err(
				priv->ndev,
				"invalid rx pkt_len %u, status 0x%x, resetting rx ring\n",
				pkt_len, rx_status);
			priv->ndev->stats.rx_errors++;
			/* Chip handed us a corrupted header; rx_offset/CAPR
			 * would otherwise stay pointed at the bad slot forever.
			 */
			nic_rx_reset(priv);
			break;
		}

		// crc : 4 bytes
		pkt_len -= 4;

		if (rx_status & RTL_RX_PACKET_STATUS_ROK) {
			/* Allocate SKB and copy data */
			skb = netdev_alloc_skb_ip_align(priv->ndev, pkt_len);
			if (!skb) {
				netdev_err(priv->ndev,
					   "Failed to allocate skb\n");
				priv->ndev->stats.rx_dropped++;
				break;
			}
			memcpy(skb_put(skb, pkt_len), rx_ring + ring_offset + 4,
			       pkt_len);
			skb->protocol = eth_type_trans(skb, priv->ndev);

			napi_gro_receive(napi, skb);
			priv->ndev->stats.rx_packets++;
			priv->ndev->stats.rx_bytes += pkt_len;
			work_done++;
		} else {
			priv->ndev->stats.rx_errors++;
		}
		priv->rx_offset = (priv->rx_offset + pkt_len + 4 + 4 + 3) &
				  ~3; /* Align to 4 bytes */
		writew(priv->rx_offset - 0x10,
		       priv->hw_addr +
			       RTL_REG_CAPR); /* Update CAPR to indicate processed data */
	}

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		/* TODO: re-enable RX interrupt on the device */
		tmp = readw(priv->hw_addr + RTL_REG_IMR);
		tmp |= (INT_ROK | INT_RER);
		writew(tmp,
		       priv->hw_addr + RTL_REG_IMR); /* enable RX interrupts */
		// netdev_info(priv->ndev, "RX interrupt re-enabled\n");
	}

	return work_done;
}

static void nic_tx_cleanup(struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);
	unsigned int entry;
	u32 tsd, txcfg;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	/* 1. Check if any TX slot is pending completion */
	while (priv->tx_free < NUM_TX_DESC) {
		entry = priv->tx_tail;

		/* 2. Read TSD register to check if the NIC has completed transmission */
		tsd = readl(priv->hw_addr + RTL_REG_TSD0 + (entry * 4));

		if (!(tsd & TSD_OWN)) {
			/* Slot is still owned by NIC, stop cleanup */
			break;
		}

		/* 3. Slot is done; the chip already retried the frame via
		 * its own CSMA/CD engine (see NCC) before giving up, so
		 * there is nothing left for the driver to retry here -
		 * just account for how it finished and free the slot. */
		if (!(tsd & TSD_TOK)) {
			ndev->stats.tx_errors++;

			if (tsd & TSD_TABT) {
				ndev->stats.tx_aborted_errors++;
				txcfg = readl(priv->hw_addr + RTL_REG_TXCONFIG);
				writel(txcfg | TXCONFIG_CLRABT,
				       priv->hw_addr + RTL_REG_TXCONFIG);
			}
			if (tsd & TSD_OWC)
				ndev->stats.tx_window_errors++;
			if (tsd & TSD_CRS)
				ndev->stats.tx_carrier_errors++;
			if (tsd & TSD_TUN)
				ndev->stats.tx_fifo_errors++;

			netdev_err(ndev, "tx error, TSD[%u] = 0x%08x\n", entry,
				   tsd);
		}
		ndev->stats.collisions += (tsd & TSD_NCC_MASK) >> TSD_NCC_SHIFT;

		/* 3. Transmission completed, free the slot */
		priv->tx_tail = (priv->tx_tail + 1) % NUM_TX_DESC;
		priv->tx_free++;

		/* Wake up the OS queue if it was stopped */
		if (priv->tx_free == 1)
			netif_wake_queue(ndev);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

static void nic_link_change(struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);
	u16 bmsr;

	/* BMSR's link-status bit latches low on a link failure and only
	 * re-samples the live state after being read; read it twice so a
	 * stale "up" reading from before a cable pull doesn't linger
	 * (same trick as the generic mii_link_ok() helper). */
	// Basic Mode Status Register (internal PHY, MII-compatible) follows  IEEE 802.3 clause 22.2.4.2 MII register 1.
	// you should include the header file <linux/mii.h> to get the BMSR_LSTATUS definition.
	readw(priv->hw_addr + RTL_REG_BMSR);
	bmsr = readw(priv->hw_addr + RTL_REG_BMSR);

	if (bmsr & BMSR_LSTATUS) {
		if (!netif_carrier_ok(ndev)) {
			netdev_info(ndev, "link up\n");
			netif_carrier_on(ndev);
		}
	} else {
		if (netif_carrier_ok(ndev)) {
			netdev_info(ndev, "link down\n");
			netif_carrier_off(ndev);
		}
	}
}

static irqreturn_t nic_irq_handler(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct nic_priv *priv = netdev_priv(ndev);
	u16 status;
	u16 tmp;

	/* TODO: read/ack interrupt status register; if this IRQ isn't
	 * ours, return IRQ_NONE.
	 */
	status = readw(priv->hw_addr + RTL_REG_ISR);

	if (status == 0xFFFF || status == 0x0000) {
		/* This is not our interrupt */
		return IRQ_NONE;
	}
	writew(status, priv->hw_addr + RTL_REG_ISR); /* ack */

	if (status & (INT_TOK | INT_TER)) {
		nic_tx_cleanup(ndev);
	}

	if (status & (INT_ROK | INT_RER)) {
		if (napi_schedule_prep(&priv->napi)) {
			tmp = readw(priv->hw_addr + RTL_REG_IMR);
			tmp &= ~(INT_ROK | INT_RER);
			writew(tmp, priv->hw_addr + RTL_REG_IMR);
			__napi_schedule(&priv->napi);
		}
	}

	if (status & INT_RX_BUFFER_OVERFLOW) {
		netdev_warn(ndev, "RX buffer overflow\n");
		priv->ndev->stats.rx_over_errors++;
		nic_rx_reset(priv);
	}
	if (status & INT_LINK_CHANGE) {
		nic_link_change(ndev);
		netdev_info(ndev, "INT - Link status changed\n");
	}
	if (status & INT_RX_FIFO_OVERFLOW) {
		netdev_warn(ndev, "RX FIFO overflow\n");
		priv->ndev->stats.rx_fifo_errors++;
		nic_rx_reset(priv);
	}

	return IRQ_HANDLED;
}

/* ---------------------------------------------------------------------
 * net_device_ops
 * ------------------------------------------------------------------- */

static int nic_open(struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);
	int err, i;
	u8 timeout = 100;
	u8 tmp8;
	u16 tmp16;

	netdev_info(ndev, "nic_open called\n");

	err = request_irq(priv->irq, nic_irq_handler, IRQF_SHARED, DRV_NAME,
			  ndev);
	if (err) {
		netdev_err(ndev, "request_irq failed: %d\n", err);
		return err;
	}

	// soft reset
	writeb(CR_RST, priv->hw_addr + RTL_REG_COMMAND);
	while (readb(priv->hw_addr + RTL_REG_COMMAND) & CR_RST) {
		udelay(10);
		timeout--;
		if (timeout == 0) {
			netdev_err(ndev, "Timeout waiting for soft reset\n");
			return -ETIMEDOUT;
		}
	}

	// set mac
	writeb(CFG9346_UNLOCK,
	       priv->hw_addr + RTL_REG_9346CR); /* Unlock EEPROM access */
	writel(get_unaligned_le32(ndev->dev_addr),
	       priv->hw_addr + RTL_REG_MAC0);
	writew(get_unaligned_le16(ndev->dev_addr + 4),
	       priv->hw_addr + RTL_REG_MAC4);
	writeb(CFG9346_LOCK,
	       priv->hw_addr + RTL_REG_9346CR); /* Lock EEPROM access */
	netdev_info(ndev, "MAC address set %x\n",
		    readl(priv->hw_addr + RTL_REG_MAC0));
	netdev_info(ndev, "MAC address set %x\n",
		    readl(priv->hw_addr + RTL_REG_MAC4));

	// TX
	/* 1. Allocate coherent DMA memory for all 4 TX slots */
	for (i = 0; i < NUM_TX_DESC; i++) {
		priv->tx_buf[i] =
			dma_alloc_coherent(&priv->pdev->dev, TX_BUF_SIZE,
					   &priv->tx_buf_dma[i], GFP_KERNEL);
		if (!priv->tx_buf[i])
			goto err_free_dma;
	}
	priv->tx_head = 0;
	priv->tx_tail = 0;
	priv->tx_free = NUM_TX_DESC;
	netdev_info(ndev, "tx alloc done.\n");
	for (i = 0; i < NUM_TX_DESC; i++) {
		netdev_info(ndev, "tx_buf[%d]: %p, tx_buf_dma[%d]: %p\n", i,
			    priv->tx_buf[i], i, (void *)priv->tx_buf_dma[i]);
	}

	// RX
	// packet format : 4 byte header(len) + data
	priv->rx_buf = dma_alloc_coherent(&priv->pdev->dev, RX_BUF_TOT_LEN,
					  &priv->rx_buf_dma, GFP_KERNEL);
	if (!priv->rx_buf)
		goto err_free_dma_rx;
	netdev_info(ndev, "rx alloc done.\n");
	netdev_info(ndev, "rx_buf: %p, rx_buf_dma: %p\n", priv->rx_buf,
		    (void *)priv->rx_buf_dma);
	nic_rx_ring_init(priv);

	//debug
	netdev_info(ndev, "RCR: 0x%x\n", readl(priv->hw_addr + RTL_REG_RCR));

	//tx imr
	tmp16 = readw(priv->hw_addr + RTL_REG_IMR);
	tmp16 |= (INT_TOK | INT_TER);
	writew(tmp16, priv->hw_addr + RTL_REG_IMR); /* enable TX interrupts */

	//tx enable
	tmp8 = readb(priv->hw_addr + RTL_REG_COMMAND);
	tmp8 |= (CR_TE);
	writeb(tmp8, priv->hw_addr + RTL_REG_COMMAND); /* TX enable*/

	//rx imr
	tmp16 = readw(priv->hw_addr + RTL_REG_IMR);
	tmp16 |= (INT_ROK | INT_RER);
	writew(tmp16, priv->hw_addr + RTL_REG_IMR); /* enable RX interrupts */

	//rx enable
	tmp8 = readb(priv->hw_addr + RTL_REG_COMMAND);
	tmp8 |= (CR_RE);
	writeb(tmp8, priv->hw_addr + RTL_REG_COMMAND); /* RX enable*/

	// other int
	tmp16 = readw(priv->hw_addr + RTL_REG_IMR);
	tmp16 |= (INT_RX_BUFFER_OVERFLOW | INT_LINK_CHANGE |
		  INT_RX_FIFO_OVERFLOW);
	writew(tmp16, priv->hw_addr + RTL_REG_IMR); /* enable RX interrupts */

	netdev_info(ndev, "IMR     0x3c : 0x%x\n",
		    readw(priv->hw_addr + RTL_REG_IMR));
	netdev_info(ndev, "COMMAND 0x37 : 0x%x\n",
		    readb(priv->hw_addr + RTL_REG_COMMAND));

	napi_enable(&priv->napi);
	netif_start_queue(ndev);

	return 0;

err_free_dma_rx:
err_free_dma:
	/* Free previously allocated DMA memory on failure */
	while (--i >= 0) {
		if (priv->tx_buf[i]) {
			dma_free_coherent(&priv->pdev->dev, TX_BUF_SIZE,
					  priv->tx_buf[i], priv->tx_buf_dma[i]);
		}
	}
	return -ENOMEM;
}

static int nic_stop(struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);
	int i;
	u8 tmp;

	netdev_info(ndev, "nic_stop called\n");

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);

	/* TODO: disable RX/TX on the device, free descriptor rings and
	 * DMA buffers.
	 */
	// disable TX interrupts
	tmp = readw(priv->hw_addr + RTL_REG_IMR);
	tmp &= ~(INT_TOK | INT_TER);
	writew(tmp, priv->hw_addr + RTL_REG_IMR);
	// disable RX interrupts
	tmp = readw(priv->hw_addr + RTL_REG_IMR);
	tmp &= ~(INT_ROK | INT_RER);
	writew(tmp, priv->hw_addr + RTL_REG_IMR);
	// others
	tmp = readw(priv->hw_addr + RTL_REG_IMR);
	tmp &= ~(INT_RX_BUFFER_OVERFLOW | INT_LINK_CHANGE |
		 INT_RX_FIFO_OVERFLOW);
	writew(tmp, priv->hw_addr + RTL_REG_IMR);

	// tx en
	tmp = readb(priv->hw_addr + RTL_REG_COMMAND);
	tmp &= ~(CR_TE);
	writeb(tmp, priv->hw_addr + RTL_REG_COMMAND);
	// rx en
	tmp = readb(priv->hw_addr + RTL_REG_COMMAND);
	tmp &= ~(CR_RE);
	writeb(tmp, priv->hw_addr + RTL_REG_COMMAND);

	free_irq(priv->irq, ndev);

	// tx
	for (i = 0; i < NUM_TX_DESC; i++) {
		dma_free_coherent(&priv->pdev->dev, TX_BUF_SIZE,
				  priv->tx_buf[i], priv->tx_buf_dma[i]);
		priv->tx_buf[i] = 0;
		priv->tx_buf_dma[i] = 0;
	}
	priv->tx_head = 0;
	priv->tx_tail = 0;
	priv->tx_free = NUM_TX_DESC;
	netdev_info(ndev, "tx dma_free_coherent done.\n");

	// rx
	dma_free_coherent(&priv->pdev->dev, RX_BUF_TOT_LEN, priv->rx_buf,
			  priv->rx_buf_dma);
	priv->rx_buf = 0;
	netdev_info(ndev, "rx dma_free_coherent done.\n");

	netif_carrier_off(ndev);

	return 0;
}

static netdev_tx_t nic_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);
	unsigned int entry;
	u32 tsd;
	unsigned long flags;

	if (unlikely(skb->len > TX_BUF_SIZE)) {
		netdev_err(ndev, "skb length %u exceeds TX buffer size %u\n",
			   skb->len, TX_BUF_SIZE);
		priv->ndev->stats.tx_dropped++;
		dev_consume_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* TODO: map skb for DMA, place it on the TX ring, kick the
	 * device's TX doorbell. Stop the queue with netif_stop_queue()
	 * when the ring is full, and wake it again from the TX
	 * completion path (NAPI or a dedicated TX IRQ) via
	 * netif_wake_queue().
	 */
	spin_lock_irqsave(&priv->lock, flags);

	/* 1. Check if any TX slot is available */
	if (priv->tx_free == 0) {
		netif_stop_queue(ndev);
		spin_unlock_irqrestore(&priv->lock, flags);
		return NETDEV_TX_BUSY;
	}

	entry = priv->tx_head;

	/* 2. Copy SKB packet data into the dedicated TX DMA buffer */
	memset(priv->tx_buf[entry], 0, TX_BUF_SIZE);
	skb_copy_from_linear_data(skb, priv->tx_buf[entry], skb->len);

	/* 3. Write physical DMA address to TSAD register (0x20, 0x24, 0x28, 0x2C) */
	writel(priv->tx_buf_dma[entry],
	       priv->hw_addr + RTL_REG_TSAD0 + (entry * 4));

	/* 4. Write frame length to TSD register (0x10, 0x14, 0x18, 0x1C). 
     *    Writing to TSD triggers the NIC to start DMA transmission.
     *    Note: Minimum Ethernet frame size is 60 bytes (excluding CRC).
     */
	tsd = max_t(unsigned int, skb->len, 60);
	writel(tsd, priv->hw_addr + RTL_REG_TSD0 + (entry * 4));

	/* 5. Update TX descriptors state and indices */
	priv->tx_head = (priv->tx_head + 1) % NUM_TX_DESC;
	priv->tx_free--;

	/* Stop the OS queue if all slots are occupied */
	if (priv->tx_free == 0)
		netif_stop_queue(ndev);

	spin_unlock_irqrestore(&priv->lock, flags);

	dev_consume_skb_any(skb);
	priv->ndev->stats.tx_packets++;
	priv->ndev->stats.tx_bytes += skb->len;

	return NETDEV_TX_OK;
}

static void nic_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	struct nic_priv *priv = netdev_priv(ndev);
	unsigned long flags;
	u8 cmd;

	netdev_warn(ndev, "transmit timed out\n");

	/* TODO: reset TX ring / device state */
	/* 1. Protect state modification with spinlock */
	spin_lock_irqsave(&priv->lock, flags);

	/* Update error statistics */
	ndev->stats.tx_errors++;

	/* 2. Reset RTL8139 hardware transmitter */
	/* Disable TE (Transmit Enable) temporarily */
	cmd = readb(priv->hw_addr + RTL_REG_COMMAND);
	cmd &= ~CR_TE;
	writeb(cmd, priv->hw_addr + RTL_REG_COMMAND);

	/* Reset TX Software descriptors state */
	priv->tx_head = 0;
	priv->tx_tail = 0;
	priv->tx_free = NUM_TX_DESC;

	/* Re-enable TE (Transmit Enable) on hardware */
	cmd |= CR_TE;
	writeb(cmd, priv->hw_addr + RTL_REG_COMMAND);

	/* 3. Update kernel transmit timestamp and wake up the queue */
	netif_trans_update(ndev);
	netif_wake_queue(ndev);

	spin_unlock_irqrestore(&priv->lock, flags);
}

/* Multicast hash filter address count above which we give up on the
 * 64-bit hash table and just accept all multicast frames instead. */
#define NIC_MC_FILTER_LIMIT 32

static void nic_set_rx_mode(struct net_device *dev)
{
	struct nic_priv *priv = netdev_priv(dev);
	u32 rcr;
	u32 mc_filter[2];
	int bit;
	// unsigned long flags;
	u8 tmp8;

	netdev_info(dev, "%s: dev->flags = 0x%x\n", __func__, dev->flags);

	/* Preserve MXDMA/RBLEN/RXFTH/etc, only touch the accept-mode bits. */
	rcr = readl(priv->hw_addr + RTL_REG_RCR);
	rcr &= ~(RCR_ACCEPT_ALL_PACKETS | RCR_ACCEPT_MAC_MATCH_PACKETS |
		 RCR_ACCEPT_MULTICAST_PACKETS | RCR_ACCEPT_BROADCAST_PACKETS);

	rcr |= RCR_ACCEPT_MAC_MATCH_PACKETS | RCR_ACCEPT_BROADCAST_PACKETS;

	if (dev->flags & IFF_PROMISC) {
		netdev_info(dev, "Setting IFF_PROMISC mode\n");
		rcr |= RCR_ACCEPT_ALL_PACKETS | RCR_ACCEPT_MULTICAST_PACKETS;
		mc_filter[0] = mc_filter[1] = 0xFFFFFFFF;
	} else if ((dev->flags & IFF_ALLMULTI) ||
		   netdev_mc_count(dev) > NIC_MC_FILTER_LIMIT) {
		netdev_info(dev, "Setting IFF_ALLMULTI mode\n");
		rcr |= RCR_ACCEPT_MULTICAST_PACKETS;
		mc_filter[0] = mc_filter[1] = 0xFFFFFFFF;
	} else if (netdev_mc_empty(dev)) {
		mc_filter[0] = mc_filter[1] = 0;
	} else {
		struct netdev_hw_addr *ha;

		rcr |= RCR_ACCEPT_MULTICAST_PACKETS;
		mc_filter[0] = mc_filter[1] = 0;
		netdev_for_each_mc_addr(ha, dev) {
			bit = ether_crc(ETH_ALEN, ha->addr) >> 26;

			mc_filter[bit >> 5] |= 1 << (bit & 31);
		}
	}

	// spin_lock_irqsave(&priv->lock, flags);
	napi_disable(&priv->napi);

	tmp8 = readb(priv->hw_addr + RTL_REG_COMMAND);
	writeb(tmp8 & ~CR_RE, priv->hw_addr + RTL_REG_COMMAND);

	writel(mc_filter[0], priv->hw_addr + RTL_REG_MAR0);
	writel(mc_filter[1], priv->hw_addr + RTL_REG_MAR4);

	writel(rcr, priv->hw_addr + RTL_REG_RCR);

	nic_rx_ring_init(priv);
	tmp8 = readb(priv->hw_addr + RTL_REG_COMMAND);
	writeb(tmp8 | CR_RE, priv->hw_addr + RTL_REG_COMMAND);
	// spin_unlock_irqrestore(&priv->lock, flags);
	napi_enable(&priv->napi);

	netdev_info(dev, "RCR 0x%x\n", rcr);
}

static int nic_set_mac_address(struct net_device *ndev, void *addr)
{
	int err;
	struct nic_priv *priv = netdev_priv(ndev);
	struct sockaddr *saddr = addr;

	err = eth_mac_addr(ndev, addr);
	if (err)
		return err;

	writeb(CFG9346_UNLOCK,
	       priv->hw_addr + RTL_REG_9346CR); /* Unlock EEPROM access */

	/* TODO: program the new MAC address into the device's
	 * hardware address filter registers.
	 */
	writel(get_unaligned_le32(saddr->sa_data),
	       priv->hw_addr + RTL_REG_MAC0);
	writew(get_unaligned_le16(saddr->sa_data + 4),
	       priv->hw_addr + RTL_REG_MAC4);

	netdev_info(ndev, "MAC address set %x\n",
		    readl(priv->hw_addr + RTL_REG_MAC0));
	netdev_info(ndev, "MAC address set %x\n",
		    readl(priv->hw_addr + RTL_REG_MAC4));

	writeb(CFG9346_LOCK,
	       priv->hw_addr + RTL_REG_9346CR); /* Lock EEPROM access */

	return 0;
}

static const struct net_device_ops nic_netdev_ops = {
	.ndo_open = nic_open,
	.ndo_stop = nic_stop,
	.ndo_start_xmit = nic_start_xmit,
	.ndo_set_rx_mode = nic_set_rx_mode,
	.ndo_tx_timeout = nic_tx_timeout,
	.ndo_set_mac_address = nic_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
};

/* ---------------------------------------------------------------------
 * ethtool_ops (minimal)
 * ------------------------------------------------------------------- */

static void nic_get_drvinfo(struct net_device *ndev,
			    struct ethtool_drvinfo *info)
{
	struct nic_priv *priv = netdev_priv(ndev);

	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	strscpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
}

static const struct ethtool_ops nic_ethtool_ops = {
	.get_drvinfo = nic_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

/* ---------------------------------------------------------------------
 * PCI probe / remove
 * ------------------------------------------------------------------- */

static int nic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *ndev;
	struct nic_priv *priv;
	int err;
	phys_addr_t mmio_start;
	phys_addr_t mmio_len;

	err = pci_enable_device(pdev);
	if (err)
		return dev_err_probe(&pdev->dev, err,
				     "pci_enable_device failed\n");

	err = pci_request_regions(pdev, DRV_NAME);
	if (err)
		goto err_disable_device;

	pci_set_master(pdev);

	err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err) {
		dev_err(&pdev->dev, "no usable DMA configuration\n");
		goto err_release_regions;
	}

	ndev = alloc_etherdev(sizeof(struct nic_priv));
	if (!ndev) {
		err = -ENOMEM;
		goto err_release_regions;
	}
	SET_NETDEV_DEV(ndev, &pdev->dev);

	priv = netdev_priv(ndev);
	priv->pdev = pdev;
	priv->ndev = ndev;
	priv->irq = pdev->irq;
	spin_lock_init(&priv->lock);

	// read BAR 1 for MMIO registers
	mmio_start = pci_resource_start(pdev, 1);
	mmio_len = pci_resource_len(pdev, 1);

	priv->hw_addr = pci_iomap(pdev, 1, mmio_len);
	if (!priv->hw_addr) {
		err = -EIO;
		goto err_free_netdev;
	}
	dev_info(&pdev->dev, "mapped BAR1 at %p (len=%pa)\n", priv->hw_addr,
		 &mmio_len);

	ndev->netdev_ops = &nic_netdev_ops;
	ndev->ethtool_ops = &nic_ethtool_ops;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
	netif_napi_add(ndev, &priv->napi, nic_poll);
#else
	netif_napi_add(ndev, &priv->napi, nic_poll, NAPI_WEIGHT);
#endif

	/* TODO: read the permanent MAC address from EEPROM/hardware
	 * instead of generating a random one.
	 */
	eth_hw_addr_random(ndev);

	pci_set_drvdata(pdev, ndev);

	err = register_netdev(ndev);
	if (err) {
		dev_err(&pdev->dev, "register_netdev failed: %d\n", err);
		goto err_iounmap;
	}

	netdev_info(ndev, "%s v%s bound to %s\n", DRV_NAME, DRV_VERSION,
		    pci_name(pdev));

	return 0;

err_iounmap:
	netif_napi_del(&priv->napi);
	pci_iounmap(pdev, priv->hw_addr);
err_free_netdev:
	free_netdev(ndev);
err_release_regions:
	pci_release_regions(pdev);
err_disable_device:
	pci_disable_device(pdev);
	return err;
}

static void nic_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct nic_priv *priv = netdev_priv(ndev);

	unregister_netdev(ndev);
	netif_napi_del(&priv->napi);
	pci_iounmap(pdev, priv->hw_addr);
	free_netdev(ndev);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static const struct pci_device_id nic_pci_tbl[] = {
	{ PCI_DEVICE(MY_VENDOR_ID, MY_DEVICE_ID) },
	{
		0,
	}
};
MODULE_DEVICE_TABLE(pci, nic_pci_tbl);

static struct pci_driver nic_pci_driver = {
	.name = DRV_NAME,
	.id_table = nic_pci_tbl,
	.probe = nic_probe,
	.remove = nic_remove,
};

module_pci_driver(nic_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("waterdog <waterdog@google.com>");
MODULE_DESCRIPTION("Basic PCI NIC driver template");
MODULE_VERSION(DRV_VERSION);
