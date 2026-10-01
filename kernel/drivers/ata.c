/*
 * SeldOS - Humboldt Kernel Project
 * Copyright (C) 2026 Free Software Foundation, Inc.
 *
 * ATA Hard Drive Driver Implementation
 * 28-bit LBA disk operations for Primary Master ATA bus.
 * Resilient PIO engine with timeout recovery, soft-reset, and PCI Bus Master IDE detection.
 * GPLv3 Licensed.
 */

#include "ata.h"
#include "io.h"
#include "string.h"
#include "serial.h"

// Primary ATA Bus Ports
#define ATA_DATA         0x1F0
#define ATA_ERROR        0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LO       0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HI       0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_STATUS       0x1F7
#define ATA_COMMAND      0x1F7

// Device Control / Alternate Status Ports
#define ATA_DEV_CTL      0x3F6
#define ATA_ALT_STATUS   0x3F6

// Status Bits
#define ATA_SR_BSY  0x80 // Busy
#define ATA_SR_DRDY 0x40 // Drive Ready
#define ATA_SR_DF   0x20 // Drive Fault
#define ATA_SR_DSC  0x10 // Drive Seek Complete
#define ATA_SR_DRQ  0x08 // Data Request Ready
#define ATA_SR_CORR 0x04 // Corrected Data
#define ATA_SR_IDX  0x02 // Index
#define ATA_SR_ERR  0x01 // Error

// Error Register Bits
#define ATA_ER_BBK   0x80 // Bad Block Detected
#define ATA_ER_UNC   0x40 // Uncorrectable Data Error
#define ATA_ER_MC    0x20 // Media Changed
#define ATA_ER_IDNF  0x10 // ID Not Found
#define ATA_ER_MCR   0x08 // Media Change Request
#define ATA_ER_ABRT  0x04 // Command Aborted
#define ATA_ER_TK0NF 0x02 // Track 0 Not Found
#define ATA_ER_AMNF  0x01 // Address Mark Not Found

// Commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_CACHE_FLUSH     0xE7

// PCI Configuration
#define PCI_CONFIG_ADDRESS      0xCF8
#define PCI_CONFIG_DATA         0xCFC
#define ATA_MAX_RETRIES         3

static struct ata_device_info primary_master;
static struct ata_controller_info controller_info;

static void ata_400ns_delay(void) {
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

/* PCI Configuration Space Accessors */
static uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, slot, func, offset);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

static void pci_write_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t address = (uint32_t)((1U << 31)
                                | ((uint32_t)bus << 16)
                                | ((uint32_t)slot << 11)
                                | ((uint32_t)func << 8)
                                | (offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t dword = inl(PCI_CONFIG_DATA);
    if ((offset & 2) == 0) {
        dword = (dword & 0xFFFF0000) | (uint32_t)val;
    } else {
        dword = (dword & 0x0000FFFF) | ((uint32_t)val << 16);
    }
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, dword);
}

/* Enumerate PCI bus to detect IDE controller and Bus Master DMA capabilities */
static void ata_pci_detect(void) {
    memset(&controller_info, 0, sizeof(controller_info));

    for (uint16_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_config_word((uint8_t)bus, slot, func, 0x00);
                if (vendor == 0xFFFF || vendor == 0x0000) {
                    if (func == 0) break; // No device in this slot
                    continue;
                }

                uint32_t class_rev = pci_read_config_dword((uint8_t)bus, slot, func, 0x08);
                uint8_t class_code = (uint8_t)(class_rev >> 24);
                uint8_t subclass = (uint8_t)(class_rev >> 16);
                uint8_t prog_if = (uint8_t)(class_rev >> 8);

                // Class 0x01: Mass Storage, Subclass 0x01: IDE Controller
                if (class_code == 0x01 && subclass == 0x01) {
                    uint16_t device = pci_read_config_word((uint8_t)bus, slot, func, 0x02);
                    uint32_t bar4 = pci_read_config_dword((uint8_t)bus, slot, func, 0x20);

                    controller_info.pci_detected = 1;
                    controller_info.pci_bus = (uint8_t)bus;
                    controller_info.pci_slot = slot;
                    controller_info.pci_func = func;
                    controller_info.vendor_id = vendor;
                    controller_info.device_id = device;
                    controller_info.progif = prog_if;

                    // Bus Master capability indicated by bit 7 of ProgIF
                    if (prog_if & 0x80) {
                        controller_info.bus_master_capable = 1;
                        controller_info.bm_base_port = (uint16_t)(bar4 & 0xFFFC);

                        // Ensure Bus Mastering and I/O Space are enabled in PCI Command Register
                        uint16_t pci_cmd = pci_read_config_word((uint8_t)bus, slot, func, 0x04);
                        if (!(pci_cmd & 0x04)) {
                            pci_write_config_word((uint8_t)bus, slot, func, 0x04, (uint16_t)(pci_cmd | 0x05));
                        }
                    }
                    return;
                }

                // If single-function device, don't check other functions
                if (func == 0) {
                    uint32_t hdr = pci_read_config_dword((uint8_t)bus, slot, 0, 0x0C);
                    uint8_t header_type = (uint8_t)(hdr >> 16);
                    if (!(header_type & 0x80)) {
                        break;
                    }
                }
            }
        }
    }
}

static void ata_log_error(uint8_t status, uint8_t err) {
    serial_puts("[-] ATA Error detected! Status: ");
    serial_print_hex(status);
    serial_puts(", Error reg: ");
    serial_print_hex(err);
    serial_puts(" [");
    if (err & ATA_ER_BBK)   serial_puts(" BBK");
    if (err & ATA_ER_UNC)   serial_puts(" UNC");
    if (err & ATA_ER_MC)    serial_puts(" MC");
    if (err & ATA_ER_IDNF)  serial_puts(" IDNF");
    if (err & ATA_ER_MCR)   serial_puts(" MCR");
    if (err & ATA_ER_ABRT)  serial_puts(" ABRT");
    if (err & ATA_ER_TK0NF) serial_puts(" TK0NF");
    if (err & ATA_ER_AMNF)  serial_puts(" AMNF");
    serial_puts(" ]\n");
}

int ata_soft_reset(void) {
    // Assert SRST (bit 2) and nIEN (bit 1) on Device Control Register
    outb(ATA_DEV_CTL, 0x04 | 0x02);
    ata_400ns_delay();
    io_wait();
    io_wait();

    // Release SRST
    outb(ATA_DEV_CTL, 0x02);
    ata_400ns_delay();

    // Wait for BSY to clear
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0xFF) return -1;
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
    }
    return -1; // Reset timeout
}

static int ata_poll(int check_drq) {
    ata_400ns_delay();

    for (int i = 0; i < 200000; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0xFF) {
            return -1; // Floating bus / no device
        }
        if (!(status & ATA_SR_BSY)) {
            if (status & (ATA_SR_ERR | ATA_SR_DF)) {
                uint8_t err = inb(ATA_ERROR);
                ata_log_error(status, err);
                return -1; // Disk error / fault
            }
            if (check_drq) {
                if (status & ATA_SR_DRQ) {
                    return 0; // Ready for data transfer
                }
            } else {
                return 0; // Operation finished
            }
        }
    }
    serial_puts("[-] ATA: Operation timed out during polling.\n");
    return -1; // Timeout
}

void ata_init(void) {
    memset(&primary_master, 0, sizeof(primary_master));

    // Probe PCI bus for IDE controllers and Bus Master DMA capabilities
    ata_pci_detect();
    if (controller_info.pci_detected) {
        serial_puts("[+] ATA: PCI IDE Controller detected (Vendor: ");
        serial_print_hex(controller_info.vendor_id);
        serial_puts(", Device: ");
        serial_print_hex(controller_info.device_id);
        serial_puts(")\n");
        if (controller_info.bus_master_capable) {
            serial_puts("[+] ATA: Bus Master DMA supported at I/O Port ");
            serial_print_hex(controller_info.bm_base_port);
            serial_puts("\n");
        } else {
            serial_puts("[*] ATA: Bus Master DMA not reported by PCI ProgIF.\n");
        }
    } else {
        serial_puts("[*] ATA: PCI IDE controller not enumerated via PCI bus scan.\n");
    }

    // Suppress IRQ14 during PIO polling mode
    outb(ATA_DEV_CTL, 0x02);
    ata_400ns_delay();

    // Select master drive
    outb(ATA_DRIVE_HEAD, 0xA0);
    ata_400ns_delay();

    // Zero out sector count & LBA registers
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_400ns_delay();

    uint8_t status = inb(ATA_STATUS);
    if (status == 0 || status == 0xFF) {
        serial_puts("[-] ATA: Primary Master drive not detected.\n");
        return;
    }

    if (ata_poll(1) != 0) {
        serial_puts("[-] ATA: Drive poll failed during IDENTIFY.\n");
        return;
    }

    uint16_t identify_buf[256];
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = inw(ATA_DATA);
    }

    primary_master.present = 1;
    primary_master.total_sectors = (uint32_t)identify_buf[60] | ((uint32_t)identify_buf[61] << 16);
    primary_master.bus_master_capable = controller_info.bus_master_capable;
    primary_master.bm_base_port = controller_info.bm_base_port;
    primary_master.pci_vendor_id = controller_info.vendor_id;
    primary_master.pci_device_id = controller_info.device_id;

    // Read Model String (words 27-46, byte-swapped)
    int idx = 0;
    for (int i = 27; i <= 46; i++) {
        primary_master.model[idx++] = (char)(identify_buf[i] >> 8);
        primary_master.model[idx++] = (char)(identify_buf[i] & 0xFF);
    }
    primary_master.model[40] = '\0';

    // Trim trailing spaces from model
    for (int i = 39; i >= 0 && primary_master.model[i] == ' '; i--) {
        primary_master.model[i] = '\0';
    }

    serial_puts("[+] ATA: Primary Master online: ");
    serial_puts(primary_master.model);
    serial_puts(" (");
    serial_print_dec((primary_master.total_sectors * 512) / (1024 * 1024));
    serial_puts(" MiB)\n");
}

struct ata_device_info ata_get_primary_master(void) {
    return primary_master;
}

struct ata_controller_info ata_get_controller_info(void) {
    return controller_info;
}

static int ata_read_sectors_once(uint32_t lba, uint8_t count, void* buffer) {
    outb(ATA_DEV_CTL, 0x02); // Suppress IRQ during PIO transfer
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_400ns_delay();

    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    uint16_t* target = (uint16_t*)buffer;

    for (int i = 0; i < count; i++) {
        if (ata_poll(1) != 0) {
            return -1;
        }
        for (int j = 0; j < 256; j++) {
            *target++ = inw(ATA_DATA);
        }
    }
    return 0;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer) {
    if (!primary_master.present || count == 0 || buffer == NULL) {
        return -1;
    }
    // 28-bit LBA bounds check
    if (lba > 0x0FFFFFFF || (primary_master.total_sectors > 0 && lba + count > primary_master.total_sectors)) {
        serial_puts("[-] ATA: Read request out of bounds.\n");
        return -1;
    }

    for (int retry = 0; retry < ATA_MAX_RETRIES; retry++) {
        if (ata_read_sectors_once(lba, count, buffer) == 0) {
            return 0;
        }
        serial_puts("[!] ATA: Read error at LBA ");
        serial_print_dec(lba);
        serial_puts(", performing soft reset and retrying...\n");
        ata_soft_reset();
    }
    return -1;
}

static int ata_write_sectors_once(uint32_t lba, uint8_t count, const void* buffer) {
    outb(ATA_DEV_CTL, 0x02); // Suppress IRQ during PIO transfer
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    ata_400ns_delay();

    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    const uint16_t* src = (const uint16_t*)buffer;

    for (int i = 0; i < count; i++) {
        if (ata_poll(1) != 0) {
            return -1;
        }
        for (int j = 0; j < 256; j++) {
            outw(ATA_DATA, *src++);
        }
    }

    // Flush cache to ensure persistent write
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_poll(0) != 0) {
        return -1;
    }

    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer) {
    if (!primary_master.present || count == 0 || buffer == NULL) {
        return -1;
    }
    // 28-bit LBA bounds check
    if (lba > 0x0FFFFFFF || (primary_master.total_sectors > 0 && lba + count > primary_master.total_sectors)) {
        serial_puts("[-] ATA: Write request out of bounds.\n");
        return -1;
    }

    for (int retry = 0; retry < ATA_MAX_RETRIES; retry++) {
        if (ata_write_sectors_once(lba, count, buffer) == 0) {
            return 0;
        }
        serial_puts("[!] ATA: Write error at LBA ");
        serial_print_dec(lba);
        serial_puts(", performing soft reset and retrying...\n");
        ata_soft_reset();
    }
    return -1;
}
