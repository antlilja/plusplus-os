#include <stdint.h>
#include <stdbool.h>
// Representation of PCI Header for AHCI controller. 
// Based on PCI Header spec from chapter 2.1 https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
typedef struct {
    // Identifiers
    uint16_t device_id;
    uint16_t vendor_id;

    // Command
    uint8_t reserved0 : 5;
    bool interrupt_disable : 1;
    bool fbe : 1;
    bool see : 1;
    bool wcc : 1;
    bool pee : 1;
    bool vga : 1;
    bool sce : 1;
    bool bus_master_enable : 1;
    bool memory_space_enable : 1;
    bool io_space_enable : 1;

    // Device status
    bool dpe : 1;
    bool sse : 1;
    bool rma : 1;
    bool rta : 1;
    bool sta : 1;
    uint8_t devt : 2;
    bool dpd : 1;
    bool fbc : 1;
    bool reserved1 : 1;
    bool c66 : 1;
    bool cl : 1;
    bool interrupt_status : 1;
    uint8_t reserved2 : 3;

    // Revision ID
    uint8_t revision_id;

    // Class code
    uint8_t class_code; // 0x01
    uint8_t subclass_code; // 0x06
    uint8_t prog_if; // 0x01 (possibly 0x00 or 0x02)

    // CLS
    uint8_t cache_line_size;

    // MLT
    uint8_t master_latency_timer;

    // Header Type
    bool mul_func_device : 1;
    uint8_t header_layout : 7;

    // Built in self test
    uint8_t bist;

    // BARS
    uint32_t bar[5];
    // ABAR
    uint32_t abar;

    // Sub System Identifiers
    uint16_t subsystem_id;
    uint16_t subsystem_vendor_id;

    // Expansion ROM
    uint32_t rom_base_address;

    // Capabilities Pointer
    uint8_t cp;
    
    // Empty
    uint64_t reserved3 : 56;

    // Interrupt Information
    uint8_t i_pin;
    uint8_t i_line;

    // Minimum Grant
    uint8_t grant;

    // Maximum Latency
    uint8_t latency;
} AHCI_CONTROLLER;

// Commands from https://wiki.osdev.org/ATA_Command_Matrix
#define ATA_COMMAND_READ_DMA_EXT 0x25
#define ATA_COMMAND_IDENTIFY 0xEC

#define FIS_TYPE_REG_H2D 0x27   // Register FIS - host to device
#define FIS_TYPE_REG_D2H 0x34   // Register FIS - device to host
#define FIS_TYPE_DMA_ACT 0x39   // DMA activate FIS - device to host
#define FIS_TYPE_DMA_SETUP 0x41 // DMA setup FIS - bidirectional
#define FIS_TYPE_DATA 0x46      // Data FIS - bidirectional
#define FIS_TYPE_BIST 0x58      // BIST activate FIS - bidirectional
#define FIS_TYPE_PIO_SETUP 0x5F // PIO setup FIS - device to host
#define FIS_TYPE_DEV_BITS 0xA1  // Set device bits FIS - device to host

typedef struct {
    // DWORD 0
    uint8_t fis_type; // FIS_TYPE_REG_H2D

    uint8_t pmport : 4; // Port multiplier
    uint8_t reserved0 : 3;
    uint8_t c : 1;      // 1: Command, 0: Control

    uint8_t command; // ATA_COMMAND
    uint8_t feature_regl; // Feature register, 7:0

    // DWORD 1
    uint8_t lba0;   // LBA low register, 7:0
    uint8_t lba1;   // LBA mid register, 15:8
    uint8_t lba2;   // LBA high register, 23:16
    uint8_t device_reg; // Device register

    // DWORD 2
    uint8_t lba3;     // LBA register, 31:24
    uint8_t lba4;     // LBA register, 39:32
    uint8_t lba5;     // LBA register, 47:40
    uint8_t feature_regh; // Feature register, 15:8

    // DWORD 3
    uint8_t countl;  // Count register, 7:0
    uint8_t counth;  // Count register, 15:8
    uint8_t icc;     // Isochronous command completion
    uint8_t control_reg; // Control register

    // DWORD 4
    uint32_t reserved1; // Reserved
} FIS_REG_H2D;

typedef struct {
    // DWORD 0
    uint8_t fis_type; // FIS_TYPE_REG_D2H

    uint8_t pmport : 4; // Port multiplier
    uint8_t reserved0 : 2;
    uint8_t interrupt : 1;
    uint8_t reserved1 : 1;

    uint8_t status_reg;
    uint8_t error_reg;

    // DWORD 1
    uint8_t lba0;   // LBA low register, 7:0
    uint8_t lba1;   // LBA mid register, 15:8
    uint8_t lba2;   // LBA high register, 23:16
    uint8_t device_reg;

    // DWORD 2
    uint8_t lba3; // LBA register, 31:24
    uint8_t lba4; // LBA register, 39:32
    uint8_t lba5; // LBA register, 47:40
    uint8_t reserved2;

    // DWORD 3
    uint8_t countl;  // Count register, 7:0
    uint8_t counth;  // Count register, 15:8
    uint16_t reserved3;

    // DWORD 4
    uint32_t reserved4;
} FIS_REG_D2H;
