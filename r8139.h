/* Max size of an Ethernet frame (incl. header) the RX ring's length field
 * should ever report; used to sanity-check the length before trusting it. */
#define MAX_ETH_FRAME_SIZE 1536

#define RTL_REG_MAC0    0x00
#define RTL_REG_MAC4    0x04
#define RTL_REG_MAR0    0x08
#define RTL_REG_MAR4    0x0C
#define RTL_REG_TSD0    0x10  /* Transmit Status Descriptor 0~3 (32-bit) */
#define RTL_REG_TSAD0   0x20  /* Transmit Start Address Descriptor 0~3 (32-bit) */
#define RTL_REG_RBSTART 0x30  /* Receive Buffer Start Address */
#define RTL_REG_COMMAND 0x37  /* Command Register (8-bit) */
#define RTL_REG_CAPR    0x38  /* Current Address Read Pointer */
#define RTL_REG_RCR     0x44  /* Receive Configuration Register */
#define RTL_REG_9346CR   0x50


#define CFG9346_UNLOCK   0xC0   /* EEM1:0 = 11, Config Write Enable */
#define CFG9346_LOCK     0x00   /* Normal / Network mode */

/* RX Buffer Size: 8K + 16 bytes overflow + 1.5K padding */
#define RX_BUF_LEN      8192
#define RX_BUF_TOT_LEN (RX_BUF_LEN + 16 + 2048)
#define RTL8139_CAPR_INIT 0xFFF0

#define CR_BUFE         (1 << 0) /* Buffer Empty */
#define CR_TE           (1 << 2) /* Transmit Enable */
#define CR_RE           (1 << 3) /* Receive Enable */
#define CR_RST          (1 << 4) /* Reset */

#define NUM_TX_DESC    4     /* RTL8139 hardware limit: 4 TX descriptors */
#define TX_BUF_SIZE    1536  /* Maximum size for a single TX buffer */

/* TSD Register Bit Definitions */
#define TSD_OWN        (1 << 13) /* 1: TX complete, slot owned by CPU; 0: Slot owned by NIC */
#define TSD_TABT       (1 << 31) /* Transmit Abort bit */

/* RTL8139 Interrupt Status/Mask Register Bits */
#define RTL_REG_ISR    0x3E  /* Interrupt Status Register (16-bit) */
#define RTL_REG_IMR    0x3C  /* Interrupt Mask Register (16-bit) */

#define INT_ROK        (1 << 0)  /* Receive OK */
#define INT_RER        (1 << 1)  /* Receive Error */
#define INT_TOK        (1 << 2)  /* Transmit OK */
#define INT_TER        (1 << 3)  /* Transmit Error */

#define RTL_RX_PACKET_STATUS_ROK					(1 << 0)
#define RTL_RX_PACKET_STATUS_FRAME_ALIGNMENT_ERROR	(1 << 1)
#define RTL_RX_PACKET_STATUS_CRC_ERROR				(1 << 2)
#define RTL_RX_PACKET_STATUS_LONG					(1 << 3)
#define RTL_RX_PACKET_STATUS_RUNT					(1 << 4)
#define RTL_RX_PACKET_STATUS_INVALID_SYMBOL_ERROR	(1 << 5)
#define RTL_RX_PACKET_STATUS_IS_BROADCAST			(1 << 13)
#define RTL_RX_PACKET_STATUS_MAC_ADDRESS_MATCHES	(1 << 14)
#define RTL_RX_PACKET_STATUS_IS_MULTICAST			(1 << 15)

// 0x44, RCR
/* Bits 7-0: Receive Configuration Flags */
#define RCR_ACCEPT_ALL_PACKETS                         (1 << 0)
#define RCR_ACCEPT_MAC_MATCH_PACKETS                   (1 << 1)
#define RCR_ACCEPT_MULTICAST_PACKETS                   (1 << 2)
#define RCR_ACCEPT_BROADCAST_PACKETS                   (1 << 3)
#define RCR_ACCEPT_RUNT_PACKETS                        (1 << 4)
#define RCR_ACCEPT_ERROR_PACKETS                       (1 << 5)
#define RTL_RX_CONFIG_FLAG_DO_NOT_WRAP                 (1 << 7)
/* Bits 8-10: Max DMA Burst Size per Rx DMA Operation */
#define RCR_MXDMA_OFFSET         8
#define RCR_MXDMA_MASK           (0x7 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_16             (0 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_32             (1 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_64             (2 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_128            (3 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_256            (4 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_512            (5 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_1024           (6 << RCR_MXDMA_OFFSET)
#define RCR_MXDMA_UNLIMITED      (7 << RCR_MXDMA_OFFSET)

/* Bits 11-12: Rx Buffer Length (Ring Buffer Size) */
#define RCR_RBLEN_OFFSET         11
#define RCR_RBLEN_MASK           (0x3 << RCR_RBLEN_OFFSET)
#define RCR_RBLEN_8K             (0 << RCR_RBLEN_OFFSET) /* 8K  + 16 bytes */
#define RCR_RBLEN_16K            (1 << RCR_RBLEN_OFFSET) /* 16K + 16 bytes */
#define RCR_RBLEN_32K            (2 << RCR_RBLEN_OFFSET) /* 32K + 16 bytes */
#define RCR_RBLEN_64K            (3 << RCR_RBLEN_OFFSET) /* 64K + 16 bytes */

/* Bits 13-15: Rx FIFO Threshold (DMA trigger threshold) */
#define RCR_RXFTH_OFFSET         13
#define RCR_RXFTH_MASK           (0x7 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_16             (0 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_32             (1 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_64             (2 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_128            (3 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_256            (4 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_512            (5 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_1024           (6 << RCR_RXFTH_OFFSET)
#define RCR_RXFTH_NONE           (7 << RCR_RXFTH_OFFSET) /* Store & Forward */

/* Bits 24-27: Rx Error Packets Length Threshold */
#define RCR_RER839_OFFSET        24
#define RCR_RER839_MASK          (0xF << RCR_RER839_OFFSET)

/* Bits 28-31: Early Rx Threshold */
#define RCR_ERTH_OFFSET          28
#define RCR_ERTH_MASK            (0xF << RCR_ERTH_OFFSET)