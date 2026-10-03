/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Intel e1000 Gigabit Ethernet Network Interface Driver Interface
 * Supports Intel 82540EP (QEMU/VBox) and related 8254x/8257x family.
 * Hardened Ring Buffers & DMA Descriptors.
 * GPLv3 Licensed.
 */

#ifndef SELD_E1000_H
#define SELD_E1000_H

#include <stdint.h>
#include <stddef.h>

/* Number of descriptors in rings (must be multiple of 8, max 64 for minimal footprint) */
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32
#define E1000_PKT_BUF_SZ  2048

/* E1000 MMIO Register Offsets */
#define E1000_REG_CTRL      0x0000 /* Device Control */
#define E1000_REG_STATUS    0x0008 /* Device Status */
#define E1000_REG_EECD      0x0010 /* EEPROM/Flash Control/Data */
#define E1000_REG_EERD      0x0014 /* EEPROM Read */
#define E1000_REG_CTRL_EXT  0x0018 /* Extended Device Control */
#define E1000_REG_ICR       0x00C0 /* Interrupt Cause Read */
#define E1000_REG_ITR       0x00C4 /* Interrupt Throttling Rate */
#define E1000_REG_ICS       0x00C8 /* Interrupt Cause Set */
#define E1000_REG_IMS       0x00D0 /* Interrupt Mask Set */
#define E1000_REG_IMC       0x00D8 /* Interrupt Mask Clear */
#define E1000_REG_RCTL      0x0100 /* Receive Control */
#define E1000_REG_RDBAL     0x2800 /* RX Descriptor Base Address Low */
#define E1000_REG_RDBAH     0x2804 /* RX Descriptor Base Address High */
#define E1000_REG_RDLEN     0x2808 /* RX Descriptor Length */
#define E1000_REG_RDH       0x2810 /* RX Descriptor Head */
#define E1000_REG_RDT       0x2818 /* RX Descriptor Tail */
#define E1000_REG_TCTL      0x0400 /* Transmit Control */
#define E1000_REG_TIPG      0x0410 /* Transmit Inter-Packet Gap */
#define E1000_REG_TDBAL     0x3800 /* TX Descriptor Base Address Low */
#define E1000_REG_TDBAH     0x3804 /* TX Descriptor Base Address High */
#define E1000_REG_TDLEN     0x3808 /* TX Descriptor Length */
#define E1000_REG_TDH       0x3810 /* TX Descriptor Head */
#define E1000_REG_TDT       0x3818 /* TX Descriptor Tail */
#define E1000_REG_MTA       0x5200 /* Multicast Table Array (128 entries) */
#define E1000_REG_RAL       0x5400 /* Receive Address Low (MAC) */
#define E1000_REG_RAH       0x5404 /* Receive Address High (MAC) */

/* Control Register (CTRL) Bits */
#define E1000_CTRL_FD       (1 << 0)  /* Full Duplex */
#define E1000_CTRL_ASDE     (1 << 5)  /* Auto-Speed Detection Enable */
#define E1000_CTRL_SLU      (1 << 6)  /* Set Link Up */
#define E1000_CTRL_RST      (1 << 26) /* Device Reset */

/* Status Register (STATUS) Bits */
#define E1000_STATUS_LU     (1 << 1)  /* Link Up */

/* Receive Control (RCTL) Bits */
#define E1000_RCTL_EN       (1 << 1)  /* Receiver Enable */
#define E1000_RCTL_SBP      (1 << 2)  /* Store Bad Packets */
#define E1000_RCTL_UPE      (1 << 3)  /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE      (1 << 4)  /* Multicast Promiscuous Enable */
#define E1000_RCTL_LPE      (1 << 5)  /* Long Packet Enable */
#define E1000_RCTL_BAM      (1 << 15) /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048 (0)     /* Buffer size 2048 bytes */
#define E1000_RCTL_SECRC    (1 << 26) /* Strip Ethernet CRC */

/* Transmit Control (TCTL) Bits */
#define E1000_TCTL_EN       (1 << 1)  /* Transmit Enable */
#define E1000_TCTL_PSP      (1 << 3)  /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT 4         /* Collision Threshold */
#define E1000_TCTL_COLD_SHIFT 12      /* Collision Distance */

/* RX Descriptor Status & Errors */
#define E1000_RXD_STAT_DD   (1 << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_EOP  (1 << 1)  /* End of Packet */
#define E1000_RXD_ERR_CE    (1 << 0)  /* CRC Error */
#define E1000_RXD_ERR_SE    (1 << 1)  /* Symbol Error */
#define E1000_RXD_ERR_SEQ   (1 << 2)  /* Sequence Error */

/* TX Descriptor Command & Status */
#define E1000_TXD_CMD_EOP   (1 << 0)  /* End of Packet */
#define E1000_TXD_CMD_IFCS  (1 << 1)  /* Insert FCS/CRC */
#define E1000_TXD_CMD_IC    (1 << 2)  /* Insert Checksum */
#define E1000_TXD_CMD_RS    (1 << 3)  /* Report Status */
#define E1000_TXD_STAT_DD   (1 << 0)  /* Descriptor Done */

/* Hardware RX Descriptor (16 bytes) */
struct e1000_rx_desc {
    uint64_t buffer_addr; /* Physical address of packet buffer */
    uint16_t length;      /* Packet length */
    uint16_t checksum;    /* Checksum field */
    uint8_t  status;      /* Status flags (DD, EOP) */
    uint8_t  errors;      /* Error flags */
    uint16_t special;
} __attribute__((packed));

/* Hardware TX Descriptor (16 bytes) */
struct e1000_tx_desc {
    uint64_t buffer_addr; /* Physical address of packet buffer */
    uint16_t length;      /* Packet length */
    uint8_t  cso;         /* Checksum offset */
    uint8_t  cmd;         /* Command flags (EOP, IFCS, RS) */
    uint8_t  status;      /* Status flags (DD) */
    uint8_t  css;         /* Checksum start */
    uint16_t special;
} __attribute__((packed));

/* Driver Network Statistics */
struct net_driver_stats {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t rx_errors;
};

/* Intel e1000 Driver Functions */
int  e1000_init(void);
int  e1000_is_active(void);
const uint8_t* e1000_get_mac(void);
int  e1000_send_packet(const void* data, size_t len);
int  e1000_poll_packet(void* buf, size_t max_len);
struct net_driver_stats e1000_get_stats(void);

#endif /* SELD_E1000_H */
