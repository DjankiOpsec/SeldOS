/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * Intel e1000 Gigabit Ethernet Network Interface Driver Implementation
 * Supports Intel 82540EP (QEMU/VBox) & compatible Gigabit controllers.
 * GPLv3 Licensed.
 */

#include "e1000.h"
#include "pci.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "idt.h"
#include "pit.h"

static struct pci_device s_pci_dev;
static uint8_t* s_mmio_base = NULL;
static uint8_t  s_mac_addr[6] = {0};
static int      s_initialized = 0;

static struct e1000_rx_desc* s_rx_descs = NULL;
static uint8_t*              s_rx_buffers[E1000_NUM_RX_DESC];
static uint32_t              s_rx_cur = 0;

static struct e1000_tx_desc* s_tx_descs = NULL;
static uint8_t*              s_tx_buffers[E1000_NUM_TX_DESC];
static uint32_t              s_tx_tail = 0;

static struct net_driver_stats s_stats = {0};

/* Known Intel Network Controller Device IDs */
static const uint16_t s_intel_dev_ids[] = {
    0x100E, // 82540EP (Standard QEMU)
    0x1004, // 82543GC
    0x100F, // 82545EM
    0x1019, // 82547EI
    0x107C, // 82541PI
    0x10D3, // 82574L
    0x10EA, // 82577LM
    0x153A  // I217-LM
};

static inline uint32_t e1000_read32(uint32_t reg) {
    return *(volatile uint32_t*)(s_mmio_base + reg);
}

static inline void e1000_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(s_mmio_base + reg) = val;
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    e1000_write32(E1000_REG_EERD, 1 | ((uint32_t)addr << 8));
    for (int timeout = 0; timeout < 10000; timeout++) {
        uint32_t val = e1000_read32(E1000_REG_EERD);
        if (val & (1 << 4)) {
            return (uint16_t)((val >> 16) & 0xFFFF);
        }
    }
    return 0xFFFF;
}

static void e1000_read_mac(void) {
    // Try EEPROM first
    uint16_t w0 = e1000_read_eeprom(0);
    uint16_t w1 = e1000_read_eeprom(1);
    uint16_t w2 = e1000_read_eeprom(2);

    if (w0 != 0xFFFF && (w0 | w1 | w2) != 0) {
        s_mac_addr[0] = (uint8_t)(w0 & 0xFF);
        s_mac_addr[1] = (uint8_t)((w0 >> 8) & 0xFF);
        s_mac_addr[2] = (uint8_t)(w1 & 0xFF);
        s_mac_addr[3] = (uint8_t)((w1 >> 8) & 0xFF);
        s_mac_addr[4] = (uint8_t)(w2 & 0xFF);
        s_mac_addr[5] = (uint8_t)((w2 >> 8) & 0xFF);
        return;
    }

    // Fallback: Read Receive Address Registers (RAL / RAH)
    uint32_t ral = e1000_read32(E1000_REG_RAL);
    uint32_t rah = e1000_read32(E1000_REG_RAH);

    s_mac_addr[0] = (uint8_t)(ral & 0xFF);
    s_mac_addr[1] = (uint8_t)((ral >> 8) & 0xFF);
    s_mac_addr[2] = (uint8_t)((ral >> 16) & 0xFF);
    s_mac_addr[3] = (uint8_t)((ral >> 24) & 0xFF);
    s_mac_addr[4] = (uint8_t)(rah & 0xFF);
    s_mac_addr[5] = (uint8_t)((rah >> 8) & 0xFF);

    // If still all zeroes or FF, assign OpSec sovereign MAC
    if ((s_mac_addr[0] | s_mac_addr[1] | s_mac_addr[2] | s_mac_addr[3] | s_mac_addr[4] | s_mac_addr[5]) == 0 ||
        (s_mac_addr[0] & s_mac_addr[1] & s_mac_addr[2] & s_mac_addr[3] & s_mac_addr[4] & s_mac_addr[5]) == 0xFF) {
        s_mac_addr[0] = 0x52; // Locally administered unicast
        s_mac_addr[1] = 0x53; // 'S'
        s_mac_addr[2] = 0x45; // 'E'
        s_mac_addr[3] = 0x4C; // 'L'
        s_mac_addr[4] = 0x44; // 'D'
        s_mac_addr[5] = 0x01;
    }
}

static void e1000_irq_handler(struct interrupt_frame* frame) {
    (void)frame;
    uint32_t icr = e1000_read32(E1000_REG_ICR);
    if (!icr) return;
    // Interrupt handled and acknowledged by reading ICR
}

int e1000_init(void) {
    serial_puts("[+] e1000: Probing PCI bus for Intel Gigabit Ethernet controllers...\n");

    int found = -1;
    size_t num_ids = sizeof(s_intel_dev_ids) / sizeof(s_intel_dev_ids[0]);

    for (size_t i = 0; i < num_ids; i++) {
        if (pci_find_device(0x8086, s_intel_dev_ids[i], &s_pci_dev) == 0) {
            found = 0;
            break;
        }
    }

    if (found < 0) {
        // Fallback: look for any Ethernet controller by Class
        if (pci_find_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET, &s_pci_dev) == 0) {
            if (s_pci_dev.vendor_id == 0x8086) {
                found = 0;
            }
        }
    }

    if (found < 0) {
        serial_puts("[-] e1000: No supported Intel network controller detected on PCI bus.\n");
        return -1;
    }

    serial_puts("[+] e1000: Found Intel NIC (Device: 0x");
    serial_print_hex(s_pci_dev.device_id);
    serial_puts(", Bus: ");
    serial_print_dec(s_pci_dev.bus);
    serial_puts(", Slot: ");
    serial_print_dec(s_pci_dev.slot);
    serial_puts(", IRQ: ");
    serial_print_dec(s_pci_dev.irq_line);
    serial_puts(")\n");

    // Enable PCI Bus Mastering, Memory Space, and I/O Space
    pci_enable_bus_mastering(s_pci_dev.bus, s_pci_dev.slot, s_pci_dev.func);

    if (s_pci_dev.bar0_is_io || s_pci_dev.bar0 == 0) {
        serial_puts("[-] e1000: BAR0 is not a valid Memory-Mapped I/O region!\n");
        return -2;
    }

    // Map BAR0 MMIO window into Higher-Half address space
    s_mmio_base = (uint8_t*)phys_to_virt(s_pci_dev.bar0);

    serial_puts("[+] e1000: MMIO mapped at physical 0x");
    serial_print_hex(s_pci_dev.bar0);
    serial_puts(" -> virtual 0x");
    serial_print_hex((uint64_t)s_mmio_base);
    serial_puts("\n");

    // Mask all interrupts during initialization
    e1000_write32(E1000_REG_IMC, 0xFFFFFFFF);
    e1000_read32(E1000_REG_ICR); // Clear pending

    // Device reset
    uint32_t ctrl = e1000_read32(E1000_REG_CTRL);
    e1000_write32(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);

    // Wait for reset to finish
    pit_sleep_ms(20);

    // Disable interrupts again after reset
    e1000_write32(E1000_REG_IMC, 0xFFFFFFFF);
    e1000_read32(E1000_REG_ICR);

    // Configure Link & Control: Set Link Up (SLU), Auto-Speed Detect, Full Duplex
    ctrl = e1000_read32(E1000_REG_CTRL);
    ctrl |= (E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD);
    ctrl &= ~E1000_CTRL_RST;
    e1000_write32(E1000_REG_CTRL, ctrl);

    // Clear Multicast Table Array (MTA)
    for (int i = 0; i < 128; i++) {
        e1000_write32(E1000_REG_MTA + (i * 4), 0);
    }

    // Read MAC address
    e1000_read_mac();
    serial_puts("[+] e1000: Hardware MAC Address: ");
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        serial_putchar(hex_chars[(s_mac_addr[i] >> 4) & 0x0F]);
        serial_putchar(hex_chars[s_mac_addr[i] & 0x0F]);
        if (i < 5) serial_putchar(':');
    }
    serial_puts("\n");

    // Initialize RX Ring Descriptors (4096-byte frame for 32 descriptors * 16 bytes = 512 bytes)
    void* rx_ring_frame = pmm_alloc_frame();
    if (!rx_ring_frame) {
        serial_puts("[-] e1000: Out of memory allocating RX descriptor frame!\n");
        return -3;
    }
    s_rx_descs = (struct e1000_rx_desc*)phys_to_virt((uint64_t)rx_ring_frame);
    memset(s_rx_descs, 0, PAGE_SIZE);

    // Allocate packet buffers (each page frame holds 2 buffers of 2048 bytes)
    for (int i = 0; i < E1000_NUM_RX_DESC; i += 2) {
        void* buf_frame = pmm_alloc_frame();
        if (!buf_frame) {
            serial_puts("[-] e1000: Out of memory allocating RX packet buffers!\n");
            return -4;
        }
        uint64_t phys0 = (uint64_t)buf_frame;
        uint64_t phys1 = (uint64_t)buf_frame + 2048;

        s_rx_buffers[i] = (uint8_t*)phys_to_virt(phys0);
        s_rx_buffers[i + 1] = (uint8_t*)phys_to_virt(phys1);

        s_rx_descs[i].buffer_addr = phys0;
        s_rx_descs[i].status = 0;

        s_rx_descs[i + 1].buffer_addr = phys1;
        s_rx_descs[i + 1].status = 0;
    }

    e1000_write32(E1000_REG_RDBAL, (uint32_t)(uint64_t)rx_ring_frame);
    e1000_write32(E1000_REG_RDBAH, 0);
    e1000_write32(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    e1000_write32(E1000_REG_RDH, 0);
    e1000_write32(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);
    s_rx_cur = 0;

    // Enable Receiver: Broadcast Accept (BAM), Strip CRC (SECRC), Buffer size 2048
    e1000_write32(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);

    // Initialize TX Ring Descriptors
    void* tx_ring_frame = pmm_alloc_frame();
    if (!tx_ring_frame) {
        serial_puts("[-] e1000: Out of memory allocating TX descriptor frame!\n");
        return -5;
    }
    s_tx_descs = (struct e1000_tx_desc*)phys_to_virt((uint64_t)tx_ring_frame);
    memset(s_tx_descs, 0, PAGE_SIZE);

    for (int i = 0; i < E1000_NUM_TX_DESC; i += 2) {
        void* buf_frame = pmm_alloc_frame();
        if (!buf_frame) {
            serial_puts("[-] e1000: Out of memory allocating TX packet buffers!\n");
            return -6;
        }
        uint64_t phys0 = (uint64_t)buf_frame;
        uint64_t phys1 = (uint64_t)buf_frame + 2048;

        s_tx_buffers[i] = (uint8_t*)phys_to_virt(phys0);
        s_tx_buffers[i + 1] = (uint8_t*)phys_to_virt(phys1);

        s_tx_descs[i].buffer_addr = phys0;
        s_tx_descs[i].cmd = 0;
        s_tx_descs[i].status = E1000_TXD_STAT_DD;

        s_tx_descs[i + 1].buffer_addr = phys1;
        s_tx_descs[i + 1].cmd = 0;
        s_tx_descs[i + 1].status = E1000_TXD_STAT_DD;
    }

    e1000_write32(E1000_REG_TDBAL, (uint32_t)(uint64_t)tx_ring_frame);
    e1000_write32(E1000_REG_TDBAH, 0);
    e1000_write32(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    e1000_write32(E1000_REG_TDH, 0);
    e1000_write32(E1000_REG_TDT, 0);
    s_tx_tail = 0;

    // Standard Transmit Inter-Packet Gap (IPG)
    e1000_write32(E1000_REG_TIPG, 0x0060200A);

    // Enable Transmitter: Pad short packets, Collision threshold 15, Collision distance 64
    e1000_write32(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                                  (15 << E1000_TCTL_CT_SHIFT) |
                                  (64 << E1000_TCTL_COLD_SHIFT));

    // Register IRQ handler & unmask IRQ on 8259 PIC if assigned
    if (s_pci_dev.irq_line > 0 && s_pci_dev.irq_line < 16) {
        irq_register_handler(s_pci_dev.irq_line, e1000_irq_handler);
        pic_unmask_irq(s_pci_dev.irq_line);
        // Enable selected interrupts: RX Timer, RX overrun, Link status change, TXDW
        e1000_write32(E1000_REG_IMS, (1 << 7) | (1 << 4) | (1 << 2) | (1 << 0));
        serial_puts("[+] e1000: IRQ ");
        serial_print_dec(s_pci_dev.irq_line);
        serial_puts(" registered and unmasked.\n");
    }

    // Verify Link Status
    uint32_t status = e1000_read32(E1000_REG_STATUS);
    serial_puts("[+] e1000: Status: 0x");
    serial_print_hex(status);
    if (status & E1000_STATUS_LU) {
        serial_puts(" (Link UP - Carrier Active)\n");
    } else {
        serial_puts(" (Link DOWN - Carrier Inactive)\n");
    }

    s_initialized = 1;
    return 0;
}

int e1000_is_active(void) {
    return s_initialized;
}

const uint8_t* e1000_get_mac(void) {
    return s_mac_addr;
}

int e1000_send_packet(const void* data, size_t len) {
    if (!s_initialized || !data || len == 0 || len > E1000_PKT_BUF_SZ) {
        s_stats.tx_dropped++;
        return -1;
    }

    uint32_t tail = s_tx_tail;

    // Check if the descriptor is currently owned by the card
    int timeout = 50000;
    while (s_tx_descs[tail].cmd && !(s_tx_descs[tail].status & E1000_TXD_STAT_DD)) {
        if (--timeout == 0) {
            serial_puts("[-] e1000: TX Ring buffer stalled!\n");
            s_stats.tx_dropped++;
            return -2;
        }
    }

    // Copy payload to the DMA buffer
    memcpy(s_tx_buffers[tail], data, len);

    // Setup descriptor
    s_tx_descs[tail].length = (uint16_t)len;
    s_tx_descs[tail].cso = 0;
    s_tx_descs[tail].css = 0;
    s_tx_descs[tail].special = 0;
    s_tx_descs[tail].status = 0;
    s_tx_descs[tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;

    // Advance tail pointer
    s_tx_tail = (s_tx_tail + 1) % E1000_NUM_TX_DESC;
    e1000_write32(E1000_REG_TDT, s_tx_tail);

    s_stats.tx_packets++;
    s_stats.tx_bytes += len;
    return 0;
}

int e1000_poll_packet(void* buf, size_t max_len) {
    if (!s_initialized || !buf || max_len == 0) {
        return 0;
    }

    uint32_t cur = s_rx_cur;

    if (!(s_rx_descs[cur].status & E1000_RXD_STAT_DD)) {
        return 0; // No packet ready
    }

    uint16_t len = s_rx_descs[cur].length;
    uint8_t errors = s_rx_descs[cur].errors;
    uint8_t status = s_rx_descs[cur].status;

    int ret_len = 0;

    if (errors == 0 && (status & E1000_RXD_STAT_EOP) && len <= max_len) {
        memcpy(buf, s_rx_buffers[cur], len);
        ret_len = (int)len;
        s_stats.rx_packets++;
        s_stats.rx_bytes += len;
    } else {
        s_stats.rx_dropped++;
        if (errors) s_stats.rx_errors++;
    }

    // Reset descriptor and return to card
    s_rx_descs[cur].status = 0;
    s_rx_descs[cur].errors = 0;
    s_rx_cur = (s_rx_cur + 1) % E1000_NUM_RX_DESC;

    // Update hardware Tail to acknowledge packet consumed
    e1000_write32(E1000_REG_RDT, cur);

    return ret_len;
}

struct net_driver_stats e1000_get_stats(void) {
    return s_stats;
}
