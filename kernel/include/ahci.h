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
