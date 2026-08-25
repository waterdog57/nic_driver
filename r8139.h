/* RTL8139 TX Registers (Offset difference is 4 bytes for each slot) */
#define RTL_REG_MAC0    0x00
#define RTL_REG_MAC4    0x04
#define RTL_REG_TSD0    0x10  /* Transmit Status Descriptor 0~3 (32-bit) */
#define RTL_REG_TSAD0   0x20  /* Transmit Start Address Descriptor 0~3 (32-bit) */
#define RTL_REG_COMMAND 0x37  /* Command Register (8-bit) */
#define CR_TE           (1 << 2) /* Transmit Enable */
#define CR_RE           (1 << 3) /* Receive Enable */

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