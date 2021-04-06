// https://wiki.osdev.org/PCI

#pragma once
#include <stdint.h>

#define DEVICE_CLASS_MASK 0xff000000
#define DEVICE_SUBCLASS_MASK 0xffff0000
#define DEVICE_PROG_IF_MASK 0xffffff00

/*
READ FROM CONFIG SPACE

Config data format:
    u32:  m=|                             0 |
    u16:  m=|             2 |             0 |
    u8 :  m=|     3 |     2 |     1 |     0 |
            |[32-24] [23-16]  [15-8]  [7-0] | <- bits
    b=      |-------------------------------|
    0x00    |Device ID      |Vendor ID      |
    0x04    |Status         |Command        |
    0x08    |Class  |Subcl. |Prog IF|Rev.ID |
    0x0C    |BIST   |Head t.|Latency|CLS    |
    ...     ...
    0x3C    |<depends on header type>       |

    Get config data feild of device by using a read_config function with offset=(b + m)
*/

// Reads u32 of config data at offset from base_address, bit 1&0: ignored.
uint32_t pci_read_config_u32(uint32_t base_address, uint8_t offset);

// Reads u16 config data at offset from base_address, bit 1: determines which word of long that is
// returned (1 being the highest half). bit 0: ignored.
uint16_t pci_read_config_u16(uint32_t base_address, uint8_t offset);

// Reads u8 config data at offset from base_address, bit 1&0: controlls which byte of long is
// returned (0 being lowest and 3 baing highest).
uint8_t pci_read_config_u8(uint32_t base_address, uint8_t offset);

// Iterates trough addresses AFTER start, if (start=0) search begins at first address. If no match
// is found return is 0. Returns base address of first device that matches target_device_type at
// mask. Great refrence for class codes: https://wiki.osdev.org/PCI#Class_Codes.
// Masks: DEVICE_CLASS_MASK matches Class code, DEVICE_SUBCLASS_MASK also matches Subclass, and
// DEVICE_PROG_IF_MASK also matches Prog IF.
// Example: to find a Floppy disk controller (ClassCode=0x01, Subclass=0x02) use
// target_device_type=0x01020000, mask=DEVICE_SUBCLASS_MASK.
uint32_t pci_find_next_device(uint32_t target_device_type, uint32_t mask, uint32_t start);
