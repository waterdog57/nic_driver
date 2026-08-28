/* Max size of an Ethernet frame (incl. header) the RX ring's length field
 * should ever report; used to sanity-check the length before trusting it. */
#define MAX_ETH_FRAME_SIZE 1536

#define RTL_REG_MAC0    0x00
#define RTL_REG_MAC4    0x04
#define RTL_REG_TSD0    0x10  /* Transmit Status Descriptor 0~3 (32-bit) */
#define RTL_REG_TSAD0   0x20  /* Transmit Start Address Descriptor 0~3 (32-bit) */
#define RTL_REG_RBSTART 0x30  /* Receive Buffer Start Address */
#define RTL_REG_COMMAND 0x37  /* Command Register (8-bit) */
#define RTL_REG_CAPR    0x38  /* Current Address Read Pointer */
#define RTL_REG_RCR     0x44  /* Receive Configuration Register */

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