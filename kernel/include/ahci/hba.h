#include <stdint.h>
#include <stdbool.h>


#define PDRT_LEN 8

// Pci header struct
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
    uint8_t class_code;    // 0x01
    uint8_t subclass_code; // 0x06
    uint8_t prog_if;       // 0x01 (possibly 0x00 or 0x02)

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
} PCIHeaderAHCIController;

typedef volatile struct {
    uint64_t cmd_list_base;     // 0x00, command list base address, 1K-byte aligned // 64 or 2*32?
    uint64_t fis_base;          // 0x08, FIS base address, 256-byte align44    ed // 64 or 2*32?
    uint32_t interrupt_status;  // 0x10, interrupt status
    uint32_t inyerrupt_enable;  // 0x14, interrupt enable
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
} AHCIPort;                     // incrementable

typedef struct {
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
    AHCIPort port[1]; // 0x100 - 0x10FF, Port control registers // HOW LONG IS THIS LIST?
    // I assume we can pass over to ports[1+], but ptr increment will be wack
} AHCIMemory; // probs: non-incrementable

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
} CmdHeader;

typedef struct {
    uint64_t data_base;       // Data base address
    uint32_t rsv0;            // Reserved
    uint32_t byte_count : 22; // Byte count, 4M max
    uint32_t _rsv1 : 9;       // Reserved
    uint32_t interrupt : 1;   // Interrupt on completion
} PRDTEntry;                  // item in physical region descriptor table (part of cmd table)

typedef struct {
    uint8_t cmd_fis[64];            // Command FIS
    uint8_t atapi_cmd[16];          // ATAPI command, 12 or 16 bytes
    uint8_t _rsv[48];               // Reserved
    PRDTEntry prdt_entry[PDRT_LEN]; // Physical region descriptor table entries, 0 ~ 65535 // HOW?
                                    // // IT SEEMS WE SET THIS OURSELVES
} CmdTable;
