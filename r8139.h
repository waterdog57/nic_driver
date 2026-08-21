/* RTL8139 TX Registers (Offset difference is 4 bytes for each slot) */
#define RTL_REG_TSD0   0x10  /* Transmit Status Descriptor 0~3 (32-bit) */
#define RTL_REG_TSAD0  0x20  /* Transmit Start Address Descriptor 0~3 (32-bit) */

#define NUM_TX_DESC    4     /* RTL8139 hardware limit: 4 TX descriptors */
#define TX_BUF_SIZE    1536  /* Maximum size for a single TX buffer */

/* TSD Register Bit Definitions */
#define TSD_OWN        (1 << 13) /* 1: TX complete, slot owned by CPU; 0: Slot owned by NIC */
#define TSD_TABT       (1 << 31) /* Transmit Abort bit */

