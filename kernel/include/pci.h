/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * PCI (Peripheral Component Interconnect) Bus Subsystem Interface
 * OpSec Hardened Configuration Space Access & Device Enumeration.
 * GPLv3 Licensed.
 */

#ifndef SELD_PCI_H
#define SELD_PCI_H

#include <stdint.h>
#include <stddef.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* PCI Configuration Registers */
#define PCI_REG_VENDOR_ID     0x00
#define PCI_REG_DEVICE_ID     0x02
#define PCI_REG_COMMAND       0x04
#define PCI_REG_STATUS        0x06
#define PCI_REG_REVISION      0x08
#define PCI_REG_PROG_IF       0x09
#define PCI_REG_SUBCLASS      0x0A
#define PCI_REG_CLASS         0x0B
#define PCI_REG_HEADER_TYPE   0x0E
#define PCI_REG_BAR0          0x10
#define PCI_REG_BAR1          0x14
#define PCI_REG_BAR2          0x18
#define PCI_REG_BAR3          0x1C
#define PCI_REG_BAR4          0x20
#define PCI_REG_BAR5          0x24
#define PCI_REG_INTERRUPT     0x3C

/* PCI Command Register Bits */
#define PCI_CMD_IO_SPACE      (1 << 0)
#define PCI_CMD_MEMORY_SPACE  (1 << 1)
#define PCI_CMD_BUS_MASTER    (1 << 2)

/* PCI Classes */
#define PCI_CLASS_NETWORK     0x02
#define PCI_SUBCLASS_ETHERNET 0x00

struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;
    uint8_t  irq_line;
    uint64_t bar0;
    uint8_t  bar0_is_io;
};

/* Raw Configuration Space Access */
uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);
void pci_write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val);

/* High-level PCI Bus Operations */
void pci_init(void);
int  pci_find_device(uint16_t vendor_id, uint16_t device_id, struct pci_device* dev_out);
int  pci_find_class(uint8_t class_code, uint8_t subclass, struct pci_device* dev_out);
void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func);

#endif /* SELD_PCI_H */
