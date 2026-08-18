// SPDX-License-Identifier: GPL-2.0
/*
 * nic_driver.c - Basic template for a PCI/PCIe Ethernet NIC driver.
 *
 * This is a skeleton: it registers a PCI driver, brings up a net_device,
 * and wires up the ndo_/NAPI/interrupt plumbing with TODO stubs where
 * hardware-specific register access belongs. Update PCI_VENDOR_ID /
 * PCI_DEVICE_ID and the TODO sections for your actual chip.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/version.h>

#define DRV_NAME	"waterdog_driver"
#define DRV_VERSION	"0.1"

/* TODO: replace with your device's real vendor/device ID */
#define MY_VENDOR_ID	0x10ec
#define MY_DEVICE_ID	0x8139

#define TX_RING_SIZE	64
#define RX_RING_SIZE	64
#define NAPI_WEIGHT	64

struct nic_priv {
	struct pci_dev *pdev;
	struct net_device *ndev;
	void __iomem *hw_addr;		/* BAR0 mapped registers */
	struct napi_struct napi;
	spinlock_t lock;
	int irq;

	/* TODO: real TX/RX descriptor rings, DMA buffers, etc. go here */
};

/* ---------------------------------------------------------------------
 * NAPI poll / interrupt handling
 * ------------------------------------------------------------------- */

static int nic_poll(struct napi_struct *napi, int budget)
{
	// struct nic_priv *priv = container_of(napi, struct nic_priv, napi);
	int work_done = 0;

	/* TODO: walk the RX ring, allocate/handle sk_buffs, call
	 * napi_gro_receive() or netif_receive_skb() for each packet,
	 * and increment work_done up to budget.
	 */

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		/* TODO: re-enable RX interrupt on the device */
	}

	return work_done;
}

static irqreturn_t nic_irq_handler(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct nic_priv *priv = netdev_priv(ndev);

	/* TODO: read/ack interrupt status register; if this IRQ isn't
	 * ours, return IRQ_NONE.
	 */

	if (napi_schedule_prep(&priv->napi)) {
		/* TODO: mask RX interrupt on the device here */
		__napi_schedule(&priv->napi);
	}

	return IRQ_HANDLED;
}

/* ---------------------------------------------------------------------
 * net_device_ops
 * ------------------------------------------------------------------- */

static int nic_open(struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);
	int err;

	err = request_irq(priv->irq, nic_irq_handler, IRQF_SHARED,
			   DRV_NAME, ndev);
	if (err) {
		netdev_err(ndev, "request_irq failed: %d\n", err);
		return err;
	}

	/* TODO: allocate TX/RX descriptor rings + DMA buffers, program
	 * the device with their addresses, and enable RX/TX on the MAC.
	 */

	napi_enable(&priv->napi);
	netif_start_queue(ndev);

	return 0;
}

static int nic_stop(struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);

	netif_stop_queue(ndev);
	napi_disable(&priv->napi);

	/* TODO: disable RX/TX on the device, free descriptor rings and
	 * DMA buffers.
	 */

	free_irq(priv->irq, ndev);

	return 0;
}

static netdev_tx_t nic_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct nic_priv *priv = netdev_priv(ndev);

	/* TODO: map skb for DMA, place it on the TX ring, kick the
	 * device's TX doorbell. Stop the queue with netif_stop_queue()
	 * when the ring is full, and wake it again from the TX
	 * completion path (NAPI or a dedicated TX IRQ) via
	 * netif_wake_queue().
	 */

	dev_kfree_skb(skb);
	priv->ndev->stats.tx_packets++;

	return NETDEV_TX_OK;
}

static void nic_tx_timeout(struct net_device *ndev, unsigned int txqueue)
{
	netdev_warn(ndev, "transmit timed out\n");

	/* TODO: reset TX ring / device state */
}

static int nic_set_mac_address(struct net_device *ndev, void *addr)
{
	int err;

	err = eth_mac_addr(ndev, addr);
	if (err)
		return err;

	/* TODO: program the new MAC address into the device's
	 * hardware address filter registers.
	 */

	return 0;
}

static const struct net_device_ops nic_netdev_ops = {
	.ndo_open		= nic_open,
	.ndo_stop		= nic_stop,
	.ndo_start_xmit		= nic_start_xmit,
	.ndo_tx_timeout		= nic_tx_timeout,
	.ndo_set_mac_address	= nic_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
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
	.get_drvinfo	= nic_get_drvinfo,
	.get_link	= ethtool_op_get_link,
};

/* ---------------------------------------------------------------------
 * PCI probe / remove
 * ------------------------------------------------------------------- */

static int nic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *ndev;
	struct nic_priv *priv;
	int err;

	err = pci_enable_device(pdev);
	if (err)
		return dev_err_probe(&pdev->dev, err, "pci_enable_device failed\n");

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

	priv->hw_addr = pci_iomap(pdev, 0, 0);
	if (!priv->hw_addr) {
		err = -EIO;
		goto err_free_netdev;
	}

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
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, nic_pci_tbl);

static struct pci_driver nic_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= nic_pci_tbl,
	.probe		= nic_probe,
	.remove		= nic_remove,
};

module_pci_driver(nic_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("waterdog <waterdog@google.com>");
MODULE_DESCRIPTION("Basic PCI NIC driver template");
MODULE_VERSION(DRV_VERSION);
