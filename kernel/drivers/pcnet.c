/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * AMD PCnet-FAST III (Am79C973 / PCnet-PCI II/III) Network Interface Driver
 * Supports VirtualBox default NIC, VMware, and legacy AMD Ethernet hardware.
 * GPLv3 Licensed.
 */

#include "pcnet.h"
#include "pci.h"
#include "io.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "pit.h"

#define PCNET_NUM_DESC 16
#define PCNET_DESC_LOG2 4
#define PCNET_BUF_SIZE  1536

/* 32-bit Initialization Block */
struct pcnet_init_block32 {
    uint16_t mode;        // 0 = Normal mode
    uint8_t  rlen;        // Log2(RX descriptors) << 4
    uint8_t  tlen;        // Log2(TX descriptors) << 4
    uint8_t  mac[6];      // Hardware MAC address
    uint16_t _reserved;   // Reserved
    uint32_t filter[2];   // Logical address filter (multicast)
    uint32_t rx_ring;     // 32-bit physical address of RX ring
    uint32_t tx_ring;     // 32-bit physical address of TX ring
} __attribute__((packed, aligned(16)));

/* 32-bit RX Ring Descriptor (RMD) */
struct pcnet_rx_desc {
    uint32_t buffer_phys; // Physical buffer address
    int16_t  buf_length;  // 2's complement of buffer size | 0xF000
    uint16_t status;      // Status flags (0x8000 = OWN by card)
    uint32_t msg_length;  // Message length (bits 0..11)
    uint32_t res;
} __attribute__((packed, aligned(16)));

/* 32-bit TX Ring Descriptor (TMD) */
struct pcnet_tx_desc {
    uint32_t buffer_phys; // Physical buffer address
    int16_t  length;      // 2's complement of packet length | 0xF000
    uint16_t status;      // Status flags (0x8300 = OWN | STP | ENP)
    uint32_t misc;
    uint32_t res;
} __attribute__((packed, aligned(16)));

static struct pci_device          s_pci_dev;
static uint16_t                   s_io_base = 0;
static uint8_t                    s_mac_addr[6] = {0};
static int                        s_initialized = 0;

static struct pcnet_init_block32* s_init_block = NULL;
static struct pcnet_rx_desc*      s_rx_descs = NULL;
static struct pcnet_tx_desc*      s_tx_descs = NULL;

static uint8_t*                   s_rx_bufs[PCNET_NUM_DESC];
static uint8_t*                   s_tx_bufs[PCNET_NUM_DESC];
static uint32_t                   s_rx_cur = 0;
static uint32_t                   s_tx_cur = 0;

static inline void pcnet_write_csr(uint16_t io, uint16_t reg, uint16_t val) {
    outw(io + 0x12, reg); // RAP
    outw(io + 0x10, val); // RDP
}

static inline uint16_t pcnet_read_csr(uint16_t io, uint16_t reg) {
    outw(io + 0x12, reg); // RAP
    return inw(io + 0x10); // RDP
}

static inline void pcnet_write_bcr(uint16_t io, uint16_t reg, uint16_t val) {
    outw(io + 0x12, reg); // RAP
    outw(io + 0x16, val); // BDP
}

static inline uint16_t pcnet_read_bcr(uint16_t io, uint16_t reg) {
    outw(io + 0x12, reg); // RAP
    return inw(io + 0x16); // BDP
}

int pcnet_init(void) {
    serial_puts("[+] pcnet: Probing PCI bus for AMD PCnet controllers...\n");

    // 1. Locate AMD PCnet PCI controller (Vendor 0x1022, Device 0x2000)
    int found = pci_find_device(0x1022, 0x2000, &s_pci_dev);
    if (found != 0) {
        if (pci_find_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET, &s_pci_dev) == 0) {
            if (s_pci_dev.vendor_id == 0x1022) {
                found = 0;
            }
        }
    }

    if (found != 0) {
        return -1; // No PCnet device detected
    }

    // 2. Enable Bus Mastering, I/O Space, and Memory Space
    pci_enable_bus_mastering(s_pci_dev.bus, s_pci_dev.slot, s_pci_dev.func);

    if (!s_pci_dev.bar0_is_io || s_pci_dev.bar0 == 0) {
        serial_puts("[-] pcnet: BAR0 is not a valid I/O port region!\n");
        return -2;
    }
    s_io_base = (uint16_t)s_pci_dev.bar0;

    // 3. Read Hardware MAC Address from APROM (I/O base + 0..5)
    for (int i = 0; i < 6; i++) {
        s_mac_addr[i] = inb(s_io_base + i);
    }

    // 4. Hardware Reset
    inw(s_io_base + 0x14); // 16-bit WIO Reset read
    pit_sleep_ms(5);

    // Stop device
    pcnet_write_csr(s_io_base, 0, 0x0004); // STOP (bit 2)
    pit_sleep_ms(2);

    // 5. Select 32-bit software mode (BCR20)
    // SWSTYLE = 2 (PCnet-PCI 32-bit software style, 32-bit descriptors)
    pcnet_write_bcr(s_io_base, 20, 0x0102);

    // 6. Allocate DMA descriptor frame
    void* ring_frame = pmm_alloc_frame();
    if (!ring_frame) {
        serial_puts("[-] pcnet: Out of memory allocating descriptor frame!\n");
        return -3;
    }
    uint64_t ring_phys = (uint64_t)ring_frame;
    uint8_t* ring_virt = (uint8_t*)phys_to_virt(ring_phys);
    memset(ring_virt, 0, PAGE_SIZE);

    s_init_block = (struct pcnet_init_block32*)ring_virt;
    s_rx_descs = (struct pcnet_rx_desc*)(ring_virt + 64);
    s_tx_descs = (struct pcnet_tx_desc*)(ring_virt + 64 + (PCNET_NUM_DESC * sizeof(struct pcnet_rx_desc)));

    uint32_t rx_ring_phys = (uint32_t)(ring_phys + 64);
    uint32_t tx_ring_phys = (uint32_t)(ring_phys + 64 + (PCNET_NUM_DESC * sizeof(struct pcnet_rx_desc)));

    // Allocate packet buffers (2 buffers per 4096-byte page)
    for (int i = 0; i < PCNET_NUM_DESC; i += 2) {
        void* rx_f = pmm_alloc_frame();
        if (!rx_f) return -4;
        uint64_t rx_phys0 = (uint64_t)rx_f;
        uint64_t rx_phys1 = (uint64_t)rx_f + 2048;

        s_rx_bufs[i] = (uint8_t*)phys_to_virt(rx_phys0);
        s_rx_bufs[i+1] = (uint8_t*)phys_to_virt(rx_phys1);

        s_rx_descs[i].buffer_phys = (uint32_t)rx_phys0;
        s_rx_descs[i].buf_length = (int16_t)(((-PCNET_BUF_SIZE) & 0x0FFF) | 0xF000);
        s_rx_descs[i].status = 0x8000; // OWN by card
        s_rx_descs[i].msg_length = 0;
        s_rx_descs[i].res = 0;

        s_rx_descs[i+1].buffer_phys = (uint32_t)rx_phys1;
        s_rx_descs[i+1].buf_length = (int16_t)(((-PCNET_BUF_SIZE) & 0x0FFF) | 0xF000);
        s_rx_descs[i+1].status = 0x8000; // OWN by card
        s_rx_descs[i+1].msg_length = 0;
        s_rx_descs[i+1].res = 0;

        void* tx_f = pmm_alloc_frame();
        if (!tx_f) return -5;
        uint64_t tx_phys0 = (uint64_t)tx_f;
        uint64_t tx_phys1 = (uint64_t)tx_f + 2048;

        s_tx_bufs[i] = (uint8_t*)phys_to_virt(tx_phys0);
        s_tx_bufs[i+1] = (uint8_t*)phys_to_virt(tx_phys1);

        s_tx_descs[i].buffer_phys = (uint32_t)tx_phys0;
        s_tx_descs[i].length = 0xF000;
        s_tx_descs[i].status = 0; // Host owns buffer
        s_tx_descs[i].misc = 0;
        s_tx_descs[i].res = 0;

        s_tx_descs[i+1].buffer_phys = (uint32_t)tx_phys1;
        s_tx_descs[i+1].length = 0xF000;
        s_tx_descs[i+1].status = 0;
        s_tx_descs[i+1].misc = 0;
        s_tx_descs[i+1].res = 0;
    }

    s_rx_cur = 0;
    s_tx_cur = 0;

    // 7. Setup Initialization Block
    s_init_block->mode = 0x0000; // Normal mode
    s_init_block->rlen = (PCNET_DESC_LOG2 << 4);
    s_init_block->tlen = (PCNET_DESC_LOG2 << 4);
    memcpy(s_init_block->mac, s_mac_addr, 6);
    s_init_block->_reserved = 0;
    s_init_block->filter[0] = 0;
    s_init_block->filter[1] = 0;
    s_init_block->rx_ring = rx_ring_phys;
    s_init_block->tx_ring = tx_ring_phys;

    // 8. Load Init Block physical address into CSR1 & CSR2
    uint32_t init_phys = (uint32_t)ring_phys;
    pcnet_write_csr(s_io_base, 1, (uint16_t)(init_phys & 0xFFFF));
    pcnet_write_csr(s_io_base, 2, (uint16_t)((init_phys >> 16) & 0xFFFF));

    // 9. Trigger Controller Initialization (CSR0 = INIT | 0x0001)
    pcnet_write_csr(s_io_base, 0, 0x0001);

    // Wait for IDON (bit 8, 0x0100)
    int ok = 0;
    for (int timeout = 0; timeout < 500; timeout++) {
        uint16_t csr0 = pcnet_read_csr(s_io_base, 0);
        if (csr0 & 0x0100) {
            ok = 1;
            break;
        }
        pit_sleep_ms(1);
    }
    if (!ok) {
        serial_puts("[-] pcnet: Initialization timed out (no IDON)!\n");
        return -6;
    }

    // Acknowledge IDON and Start Controller (CSR0 = 0x0142: STRT | INEA | IDON acknowledge)
    pcnet_write_csr(s_io_base, 0, 0x0142);

    s_initialized = 1;

    serial_puts("[+] pcnet: Found AMD PCnet NIC (Device: 0x2000, I/O: 0x");
    serial_print_hex(s_io_base);
    serial_puts(")\n");
    serial_puts("[+] pcnet: Hardware MAC Address: ");
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        serial_putchar(hex_chars[(s_mac_addr[i] >> 4) & 0x0F]);
        serial_putchar(hex_chars[s_mac_addr[i] & 0x0F]);
        if (i < 5) serial_putchar(':');
    }
    serial_puts("\n");
    serial_puts("[+] pcnet: Carrier Online - Status: 0x");
    serial_print_hex(pcnet_read_csr(s_io_base, 0));
    serial_puts("\n");

    return 0;
}

int pcnet_is_active(void) {
    return s_initialized;
}

const uint8_t* pcnet_get_mac(void) {
    return s_mac_addr;
}

int pcnet_send_packet(const void* data, size_t len) {
    if (!s_initialized || !data || len == 0 || len > 1518) return -1;

    struct pcnet_tx_desc* tx = &s_tx_descs[s_tx_cur];
    if (tx->status & 0x8000) {
        // Wait briefly for previous packet transmission
        for (int i = 0; i < 50; i++) {
            if (!(tx->status & 0x8000)) break;
            pit_sleep_ms(1);
        }
        if (tx->status & 0x8000) return -2; // Ring busy
    }

    size_t send_len = len < 60 ? 60 : len;
    memcpy(s_tx_bufs[s_tx_cur], data, len);
    if (send_len > len) {
        memset((uint8_t*)s_tx_bufs[s_tx_cur] + len, 0, send_len - len);
    }

    tx->length = (int16_t)(((-(int)send_len) & 0x0FFF) | 0xF000);
    tx->misc = 0;
    tx->status = 0x8300; // OWN (0x8000) | STP (0x0200) | ENP (0x0100)

    s_tx_cur = (s_tx_cur + 1) % PCNET_NUM_DESC;

    // Demand Transmission: TDMD (bit 3 = 0x0008) + INEA (0x0040)
    pcnet_write_csr(s_io_base, 0, 0x0048);
    return 0;
}

int pcnet_poll_packet(void* buf, size_t max_len) {
    if (!s_initialized || !buf || max_len == 0) return 0;

    struct pcnet_rx_desc* rx = &s_rx_descs[s_rx_cur];
    if (rx->status & 0x8000) {
        // Still owned by card
        return 0;
    }

    // Host owns descriptor
    if (rx->status & 0x4000) {
        // Error bit set: recycle descriptor
        rx->status = 0x8000;
        s_rx_cur = (s_rx_cur + 1) % PCNET_NUM_DESC;
        return 0;
    }

    size_t pkt_len = (size_t)(rx->msg_length & 0x0FFF);
    if (pkt_len > max_len) pkt_len = max_len;

    memcpy(buf, s_rx_bufs[s_rx_cur], pkt_len);

    // Return descriptor to controller
    rx->status = 0x8000;
    s_rx_cur = (s_rx_cur + 1) % PCNET_NUM_DESC;

    // Acknowledge RINT (bit 10 = 0x0400) + INEA (0x0040)
    pcnet_write_csr(s_io_base, 0, 0x0440);

    return (int)pkt_len;
}
