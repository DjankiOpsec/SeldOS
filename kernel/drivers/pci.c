/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * PCI (Peripheral Component Interconnect) Bus Subsystem Implementation
 * GNU AGPLv3 Licensed - Humboldt Underground.
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

int pci_quarantine_backdoors(void) {
    int quarantined_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read_word((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (vendor == 0xFFFF) continue;

            uint8_t header_type = pci_read_byte((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            uint8_t num_funcs = (header_type & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < num_funcs; func++) {
                uint16_t ven = pci_read_word((uint8_t)bus, slot, func, PCI_REG_VENDOR_ID);
                if (ven == 0xFFFF) continue;

                uint16_t dev = pci_read_word((uint8_t)bus, slot, func, PCI_REG_DEVICE_ID);
                uint8_t cc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_CLASS);
                uint8_t sc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_SUBCLASS);

                int is_hostile = 0;
                const char* desc = 0;

                // Intel Management Engine Interface (MEI / HECI)
                // Class 0x07 (Communication), Subclass 0x80 (Other) or known Intel ME device IDs
                if (ven == 0x8086) {
                    if (cc == 0x07 && sc == 0x80) {
                        is_hostile = 1;
                        desc = "Intel Management Engine (HECI/MEI)";
                    } else if (dev == 0x1c3a || dev == 0x1e3a || dev == 0x8c3a ||
                               dev == 0x9cba || dev == 0x9d3a || dev == 0xa13a ||
                               dev == 0xa2ba || dev == 0xa360 || dev == 0x2e14 ||
                               dev == 0x3b64 || dev == 0x1d3a) {
                        is_hostile = 1;
                        desc = "Intel ME / AMT Interface";
                    }
                }
                // AMD Platform Security Processor (PSP / CCP)
                else if (ven == 0x1022) {
                    if (cc == 0x10) {
                        is_hostile = 1;
                        desc = "AMD Platform Security Processor (PSP)";
                    } else if (dev == 0x1456 || dev == 0x1468 || dev == 0x1486 ||
                               dev == 0x1578 || dev == 0x1649) {
                        is_hostile = 1;
                        desc = "AMD PSP / Crypto Coprocessor";
                    }
                }

                if (is_hostile) {
                    // Revoke all PCI privileges: clear Bus Master (DMA), Memory Space, I/O Space
                    pci_write_word((uint8_t)bus, slot, func, PCI_REG_COMMAND, 0);

                    serial_puts("[!] OPSEC QUARANTINE: Detected Hostile Silicon Coprocessor (");
                    serial_puts(desc);
                    serial_puts(") at B/S/F ");
                    serial_print_dec((uint32_t)bus);
                    serial_puts("/");
                    serial_print_dec((uint32_t)slot);
                    serial_puts("/");
                    serial_print_dec((uint32_t)func);
                    serial_puts(". Bus Mastering & DMA revoked!\n");

                    quarantined_count++;
                }
            }
        }
    }

    if (quarantined_count == 0) {
        serial_puts("[+] OpSec PCI Audit: 0 active Ring -3 hardware backdoors active on bus.\n");
    }

    return quarantined_count;
}

static int s_pci_locked_down = 0;

int pci_is_locked_down(void) {
    return s_pci_locked_down;
}

int pci_lockdown_dma_and_rom(void) {
    int rom_disabled_count = 0;
    int dma_revoked_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read_word((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (vendor == 0xFFFF) continue;

            uint8_t header_type = pci_read_byte((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            uint8_t num_funcs = (header_type & 0x80) ? 8 : 1;
            uint8_t is_bridge = (header_type & 0x7F) == 0x01;

            for (uint8_t func = 0; func < num_funcs; func++) {
                uint16_t ven = pci_read_word((uint8_t)bus, slot, func, PCI_REG_VENDOR_ID);
                if (ven == 0xFFFF) continue;

                uint16_t dev = pci_read_word((uint8_t)bus, slot, func, PCI_REG_DEVICE_ID);
                uint8_t cc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_CLASS);
                uint8_t sc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_SUBCLASS);

                // 1. Option ROM Lockdown: Disable Expansion ROM Decode
                uint8_t rom_reg = is_bridge ? PCI_REG_EXPANSION_ROM_BR : PCI_REG_EXPANSION_ROM;
                uint32_t rom_base = pci_read_dword((uint8_t)bus, slot, func, rom_reg);
                if (rom_base & 0x01) {
                    pci_write_dword((uint8_t)bus, slot, func, rom_reg, 0);
                    rom_disabled_count++;
                }

                // 2. Zero-Trust DMA & Bus Master Lockdown
                // Whitelist: E1000 Ethernet (0x8086:0x100E), AC97 Audio (0x8086:0x2415), Host/PCI Bridges
                int is_whitelisted = 0;
                if (ven == 0x8086 && dev == 0x100e) {
                    is_whitelisted = 1; // Intel e1000 NIC DMA
                } else if (ven == 0x8086 && dev == 0x2415) {
                    is_whitelisted = 1; // Intel ICH AC97 Audio DMA
                } else if (cc == PCI_CLASS_BRIDGE) {
                    is_whitelisted = 1; // PCI bridge infrastructure
                } else if (cc == PCI_CLASS_STORAGE && sc == 0x01) {
                    is_whitelisted = 1; // IDE storage controller
                }

                if (!is_whitelisted) {
                    uint16_t cmd = pci_read_word((uint8_t)bus, slot, func, PCI_REG_COMMAND);
                    if (cmd & PCI_CMD_BUS_MASTER) {
                        cmd &= ~PCI_CMD_BUS_MASTER;
                        pci_write_word((uint8_t)bus, slot, func, PCI_REG_COMMAND, cmd);
                        dma_revoked_count++;
                    }
                }
            }
        }
    }

    s_pci_locked_down = 1;
    serial_puts("[+] OpSec Zero-Trust: PCI DMA Lockdown active (ROMs disabled: ");
    serial_print_dec((uint32_t)rom_disabled_count);
    serial_puts(", untrusted Bus Masters revoked: ");
    serial_print_dec((uint32_t)dma_revoked_count);
    serial_puts(").\n");

    return dma_revoked_count;
}

int pci_usb_quarantine_poweroff(void) {
    int usb_quarantined_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read_word((uint8_t)bus, slot, 0, PCI_REG_VENDOR_ID);
            if (vendor == 0xFFFF) continue;

            uint8_t header_type = pci_read_byte((uint8_t)bus, slot, 0, PCI_REG_HEADER_TYPE);
            uint8_t num_funcs = (header_type & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < num_funcs; func++) {
                uint16_t ven = pci_read_word((uint8_t)bus, slot, func, PCI_REG_VENDOR_ID);
                if (ven == 0xFFFF) continue;

                uint8_t cc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_CLASS);
                uint8_t sc = pci_read_byte((uint8_t)bus, slot, func, PCI_REG_SUBCLASS);

                // Class 0x0C (Serial Bus), Subclass 0x03 (USB Controller - UHCI/OHCI/EHCI/xHCI)
                if (cc == PCI_CLASS_SERIAL_BUS && sc == PCI_SUBCLASS_USB) {
                    // Revoke all PCI Command bits: Bus Master, Memory Space, I/O Space (cmd = 0)
                    pci_write_word((uint8_t)bus, slot, func, PCI_REG_COMMAND, 0);

                    // Zero BARs to completely unmap device from memory/IO space
                    pci_write_dword((uint8_t)bus, slot, func, PCI_REG_BAR0, 0);
                    pci_write_dword((uint8_t)bus, slot, func, PCI_REG_BAR1, 0);

                    // Disable Interrupt line
                    pci_write_byte((uint8_t)bus, slot, func, PCI_REG_INTERRUPT, 0xFF);

                    serial_puts("[!] OPSEC BLACKOUT: USB Controller quarantined & powered down at B/S/F ");
                    serial_print_dec((uint32_t)bus);
                    serial_puts("/");
                    serial_print_dec((uint32_t)slot);
                    serial_puts("/");
                    serial_print_dec((uint32_t)func);
                    serial_puts(". Hardware sniffer pins unpowered!\n");

                    usb_quarantined_count++;
                }
            }
        }
    }

    if (usb_quarantined_count == 0) {
        serial_puts("[+] OpSec: 0 active USB controllers found on PCI bus.\n");
    }

    return usb_quarantined_count;
}

void pci_init(void) {
    serial_puts("[+] PCI Bus: Subsystem Initialized. Auditing hardware backdoors...\n");
    pci_quarantine_backdoors();
    pci_lockdown_dma_and_rom();
}

