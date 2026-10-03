/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * PCI (Peripheral Component Interconnect) Bus Subsystem Implementation
 * GPLv3 Licensed.
 */

#include "pci.h"
#include "io.h"
#include "serial.h"

uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t d = pci_read_dword(bus, slot, func, offset);
    return (uint16_t)((d >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t d = pci_read_dword(bus, slot, func, offset);
    return (uint8_t)((d >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, val);
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t d = inl(PCI_CONFIG_DATA);
    if ((offset & 2) == 0) {
        d = (d & 0xFFFF0000) | (uint32_t)val;
    } else {
        d = (d & 0x0000FFFF) | ((uint32_t)val << 16);
    }
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, d);
}

void pci_write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t d = inl(PCI_CONFIG_DATA);
    uint32_t shift = (offset & 3) * 8;
    d = (d & ~(0xFFU << shift)) | ((uint32_t)val << shift);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, d);
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_read_word(bus, slot, func, PCI_REG_COMMAND);
    cmd |= (PCI_CMD_BUS_MASTER | PCI_CMD_MEMORY_SPACE | PCI_CMD_IO_SPACE);
    pci_write_word(bus, slot, func, PCI_REG_COMMAND, cmd);
}

static void fill_pci_device(uint8_t bus, uint8_t slot, uint8_t func, struct pci_device* dev) {
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = pci_read_word(bus, slot, func, PCI_REG_VENDOR_ID);
    dev->device_id = pci_read_word(bus, slot, func, PCI_REG_DEVICE_ID);

    uint8_t rev_class_dw3 = pci_read_byte(bus, slot, func, PCI_REG_CLASS);
    uint8_t rev_class_dw2 = pci_read_byte(bus, slot, func, PCI_REG_SUBCLASS);
    uint8_t rev_class_dw1 = pci_read_byte(bus, slot, func, PCI_REG_PROG_IF);
    uint8_t rev_class_dw0 = pci_read_byte(bus, slot, func, PCI_REG_REVISION);

    dev->class_code = rev_class_dw3;
    dev->subclass = rev_class_dw2;
    dev->prog_if = rev_class_dw1;
    dev->revision = rev_class_dw0;

    dev->irq_line = pci_read_byte(bus, slot, func, PCI_REG_INTERRUPT);

    uint32_t bar0 = pci_read_dword(bus, slot, func, PCI_REG_BAR0);
    if (bar0 & 1) {
        dev->bar0_is_io = 1;
        dev->bar0 = bar0 & ~0x3ULL;
    } else {
        dev->bar0_is_io = 0;
        uint8_t type = (bar0 >> 1) & 0x3;
        if (type == 0x2) {
            // 64-bit BAR
            uint32_t bar1 = pci_read_dword(bus, slot, func, PCI_REG_BAR1);
            dev->bar0 = ((uint64_t)bar1 << 32) | (bar0 & ~0xFULL);
        } else {
            // 32-bit BAR
            dev->bar0 = bar0 & ~0xFULL;
        }
    }
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, struct pci_device* dev_out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t v = pci_read_word((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (v == 0xFFFF) continue;

            uint8_t header_type = pci_read_byte((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            uint8_t num_funcs = (header_type & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < num_funcs; func++) {
                uint16_t ven = pci_read_word((uint8_t)bus, slot, func, PCI_REG_VENDOR_ID);
                if (ven == 0xFFFF) continue;

                uint16_t dev = pci_read_word((uint8_t)bus, slot, func, PCI_REG_DEVICE_ID);
                if (ven == vendor_id && dev == device_id) {
                    if (dev_out) {
                        fill_pci_device((uint8_t)bus, slot, func, dev_out);
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

int pci_find_class(uint8_t class_code, uint8_t subclass, struct pci_device* dev_out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t v = pci_read_word((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (v == 0xFFFF) continue;

            uint8_t header_type = pci_read_byte((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            uint8_t num_funcs = (header_type & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < num_funcs; func++) {
                uint16_t ven = pci_read_word((uint8_t)bus, slot, func, PCI_REG_VENDOR_ID);
                if (ven == 0xFFFF) continue;

                uint8_t cc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_CLASS);
                uint8_t sc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_SUBCLASS);

                if (cc == class_code && sc == subclass) {
                    if (dev_out) {
                        fill_pci_device((uint8_t)bus, slot, func, dev_out);
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

void pci_init(void) {
    serial_puts("[+] PCI Bus: Subsystem Initialized. Enumerating devices...\n");
}
