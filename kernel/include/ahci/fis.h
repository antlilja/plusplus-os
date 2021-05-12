#include <stdint.h>


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
    uint8_t fis_type; // FIS_TYPE_REG_H2D

    uint8_t pmport : 4; // Port multiplier
    uint8_t _rsv0 : 3;
    uint8_t c : 1; // 1: Command, 0: Control

    uint8_t cmd;      // ATA_COMMAND
    uint8_t feature_regl; // Feature register, 7:0

    uint8_t lba0;       // LBA low register, 7:0
    uint8_t lba1;       // LBA mid register, 15:8
    uint8_t lba2;       // LBA high register, 23:16
    uint8_t device_reg; // Device register

    uint8_t lba3;         // LBA register, 31:24
    uint8_t lba4;         // LBA register, 39:32
    uint8_t lba5;         // LBA register, 47:40
    uint8_t feature_regh; // Feature register, 15:8

    uint8_t countl;      // Count register, 7:0
    uint8_t counth;      // Count register, 15:8 // replace with single??
    uint8_t icc;         // Isochronous command completion
    uint8_t control_reg; // Control register

    uint32_t _rsv1; // Reserved
} FISRegH2D;

typedef struct {
    uint8_t fis_type; // FIS_TYPE_REG_D2H

    uint8_t pmport : 4; // Port multiplier
    uint8_t reserved0 : 2;
    uint8_t interrupt : 1;
    uint8_t _rsv0 : 1;

    uint8_t status_reg;
    uint8_t error_reg;

    uint8_t lba0; // LBA low register, 7:0
    uint8_t lba1; // LBA mid register, 15:8
    uint8_t lba2; // LBA high register, 23:16
    uint8_t device_reg;

    uint8_t lba3; // LBA register, 31:24
    uint8_t lba4; // LBA register, 39:32
    uint8_t lba5; // LBA register, 47:40
    uint8_t _rsv1;

    uint8_t countl; // Count register, 7:0
    uint8_t counth; // Count register, 15:8
    uint16_t _rsv2;

    uint32_t _rsv3;
} FISRegD2H;

