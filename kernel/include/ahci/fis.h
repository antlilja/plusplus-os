#pragma once
#include <stdint.h>

#define FIS_TYPE_REG_H2D 0x27

typedef struct {
    uint8_t fis_type; // FIS_TYPE_REG_H2D

    uint8_t pmport : 4; // Port multiplier
    uint8_t _rsv0 : 3;
    uint8_t c : 1; // 1: Command, 0: Control

    uint8_t cmd;          // ATA_COMMAND
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

    uint8_t _rsv1[4]; // Reserved
} __attribute__((packed)) FISRegH2D;
