#pragma once
#include <stdint.h>

// Port defines
#define PORT_CMD_ICC_ACTIVE (1 << 28)
#define PORT_CMD_ICC_SLUMBER (6 << 28)
#define PORT_CMD_ICC_MASK (0xf << 28)
#define PORT_CMD_ATAPI (1 << 24)
#define PORT_CMD_CR (1 << 15)
#define PORT_CMD_FR (1 << 14)
#define PORT_CMD_FRE (1 << 4)
#define PORT_CMD_CLO (1 << 3)
#define PORT_CMD_POD (1 << 2)
#define PORT_CMD_SUD (1 << 1)
#define PORT_CMD_ST (1 << 0)

#define PORT_IPM_ACTIVE 1
#define PORT_DET_PRESENT 3

#define PORT_IS_TFES (1 << 30)

// ATA defines
#define SATA_SIG_ATA 0x00000101

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35

#define ATA_DEV_DRQ 0x8
#define ATA_DEV_BUSY 0x80

// Other defines

typedef volatile struct {
    uint64_t cmd_list_base;     // 0x00, command list base address, 1K-byte aligned // 64 or 2*32?
    uint64_t fis_base;          // 0x08, FIS base address, 256-byte align44    ed // 64 or 2*32?
    uint32_t interrupt_status;  // 0x10, interrupt status
    uint32_t interrupt_enable;  // 0x14, interrupt enable
    uint32_t cmd;               // 0x18, command and status
    uint32_t _rsv0;             // 0x1C, Reserved
    uint32_t task_file_data;    // 0x20, task file data
    uint32_t signature;         // 0x24, signature
    uint32_t sata_status;       // 0x28, SATA status (SCR0:SStatus)
    uint32_t sata_ctrl;         // 0x2C, SATA control (SCR2:SControl)
    uint32_t sata_error;        // 0x30, SATA error (SCR1:SError)
    uint32_t sata_active;       // 0x34, SATA active (SCR3:SActive)
    uint32_t cmd_issue;         // 0x38, command issue
    uint32_t sata_notification; // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fis_switch_ctrl;   // 0x40, FIS-based switch control
    uint32_t _rsv1[11];         // 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4];         // 0x70 ~ 0x7F, vendor specific
} __attribute__((packed)) AHCIPort;

typedef volatile struct {
    uint32_t capabilities;        // 0x00, Host capability
    uint32_t global_ctrl;         // 0x04, Global host control
    uint32_t interrupt_status;    // 0x08, Interrupt status
    uint32_t port_impl;           // 0x0C, Port implemented
    uint32_t ahci_ver;            // 0x10, Version
    uint32_t ccc_ctrl;            // 0x14, Command completion coalescing control
    uint32_t ccc_ports;           // 0x18, Command completion coalescing ports
    uint32_t em_enclosure;        // 0x1C, Enclosure management location // THIS SHOULD NOT BE ZERO!
    uint32_t em_ctrl;             // 0x20, Enclosure management control
    uint32_t capabilities2;       // 0x24, Host capabilities extended
    uint32_t bios_handoff;        // 0x28, BIOS/OS handoff control and status
    uint8_t _rsv[0xA0 - 0x2C];    // 0x2C - 0x9F, Reserved
    uint8_t vendor[0x100 - 0xA0]; // 0xA0 - 0xFF, Vendor specific registers
    AHCIPort ports[1];
} __attribute__((packed)) AHCIController;

typedef struct {
    uint8_t cmd_fis_len : 5;  // Command FIS length in DWORDS, 2 ~ 16
    uint8_t atapi : 1;        // ATAPI // We dont need this...
    uint8_t write : 1;        // Write, 1: H2D, 0: D2H
    uint8_t Prefetchable : 1; // Prefetchable
    uint8_t reset : 1;        // Reset
    uint8_t bist : 1;         // BIST
    uint8_t clear : 1;        // Clear busy upon R_OK // What dose this mean???
    uint8_t _rsv0 : 1;        // Reserved
    uint8_t port_mult : 4;    // Port multiplier port
    uint16_t prdt_len;        // Physical region descriptor table length in entries
    uint32_t prd_byte_count;  // Physical region descriptor byte count transferred
    uint64_t cmd_table_base;  // Command table descriptor base address // 32 or 64?
    uint32_t _rsv[4];         // Reserved
} __attribute__((packed)) CMDHeader;

typedef struct {
    uint64_t data_base;              // Data base address
    uint32_t rsv0;                   // Reserved
    uint32_t byte_count : 22;        // Byte count, 4M max
    uint32_t _rsv1 : 9;              // Reserved
    uint32_t interrupt : 1;          // Interrupt on completion
} __attribute__((packed)) PRDTEntry; // item in physical region descriptor table (part of cmd table)

typedef struct {
    uint8_t cmd_fis[64];     // Command FIS
    uint8_t atapi_cmd[16];   // ATAPI command, 12 or 16 bytes
    uint8_t _rsv[48];        // Reserved
    PRDTEntry prdt_entry[1]; // Physical region descriptor table entries, 0 ~ 65535 // HOW?
} __attribute__((packed)) CMDTable;
