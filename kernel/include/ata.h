/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * ATA PIO Hard Drive Driver Interface
 * 28-bit LBA disk operations for Primary Master/Slave ATA drives.
 * GPLv3 Licensed.
 */

#ifndef SELD_ATA_H
#define SELD_ATA_H

#include <stdint.h>
#include <stddef.h>

#define ATA_SECTOR_SIZE 512

struct ata_device_info {
    int present;
    uint32_t total_sectors;
    char model[41];
    char serial[21];
    int bus_master_capable;
    uint16_t bm_base_port;
    uint16_t pci_vendor_id;
    uint16_t pci_device_id;
};

/* Bus Master DMA Physical Region Descriptor (PRD) */
struct ata_prd_entry {
    uint32_t phys_addr;  // Memory physical buffer address
    uint16_t byte_count; // Transfer byte count (0 = 64 KiB)
    uint16_t eot;        // Bit 15: End Of Table (0x8000)
} __attribute__((packed));

struct ata_controller_info {
    int      pci_detected;
    uint8_t  pci_bus;
    uint8_t  pci_slot;
    uint8_t  pci_func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  progif;
    int      bus_master_capable;
    uint16_t bm_base_port;
};

void ata_init(void);
struct ata_device_info ata_get_primary_master(void);
struct ata_controller_info ata_get_controller_info(void);
int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);
int ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer);
int ata_soft_reset(void);

#endif /* SELD_ATA_H */
