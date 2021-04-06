#include <stdint.h>
#include <stdbool.h>
#include "port_io.h"
#include "pci.h"

#define PCI_REG_CONFIG_ADDRESS 0xCF8
#define PCI_REG_CONFIG_DATA 0xCFC

#define VENDOR_ID_OFFSET 0x00
#define HEADER_TYPE_OFFSET 0x0E
// bottom offset for u32 containg Class code, Subclass, Prog IF and Revision ID
#define DEVICE_TYPE_OFFSET 0x08

#define DEVICE_CLASS_MASK 0xff000000
#define DEVICE_SUBCLASS_MASK 0xffff0000
#define DEVICE_PROG_IF_MASK 0xffffff00

#define CONFIG_ADDRESS_ZERO 0x80000000

// black-box, don't look!
__attribute__((always_inline)) uint32_t create_config_address(uint8_t bus, uint8_t slot,
                                                              uint8_t func, uint8_t offset) {
    return (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)(slot & 0x1F) << 11) |
                      ((uint32_t)(func & 0x7) << 8) | ((uint32_t)offset & 0xfc) |
                      ((uint32_t)0x80000000));
}

uint8_t pci_read_config_u8(uint32_t base_address, uint8_t offset) {
    uint32_t config_address = (base_address & 0xffffff00) | (offset & ~0b11);
    port_out_u32(PCI_REG_CONFIG_ADDRESS, config_address);
    uint32_t config_data = port_in_u32(PCI_REG_CONFIG_DATA) >> ((offset & 0b11) * 8);
    return (uint8_t)(config_data & 0xff);
}

uint16_t pci_read_config_u16(uint32_t base_address, uint8_t offset) {
    uint32_t config_address = (base_address & 0xffffff00) | (offset & ~0b11);
    port_out_u32(PCI_REG_CONFIG_ADDRESS, config_address);
    uint32_t config_data = port_in_u32(PCI_REG_CONFIG_DATA) >> ((offset & 0b10) * 8);
    return (uint16_t)(config_data & 0xffff);
}

uint32_t pci_read_config_u32(uint32_t base_address, uint8_t offset) {
    uint32_t config_address = (base_address & 0xffffff00) | (offset & ~0b11);
    port_out_u32(PCI_REG_CONFIG_ADDRESS, config_address);
    uint32_t config_data = port_in_u32(PCI_REG_CONFIG_DATA);
    return config_data;
}

// uses vendor ID to check if device exists
bool device_exists(uint32_t address) {
    return pci_read_config_u16(address, VENDOR_ID_OFFSET) != 0xffff;
}

// returns true if device has multiple distinct functions
bool device_is_multi_func(uint32_t address) {
    return (pci_read_config_u8(address, HEADER_TYPE_OFFSET) & 0x80) != 0;
}

// Returns the next valid address
uint32_t next_base_address(uint32_t last) {
    if (last == 0) return CONFIG_ADDRESS_ZERO; // Special exception, return first address.
    uint32_t bus = (last & 0x00ff0000) >> 16;
    uint32_t slot = (last & 0x0000f800) >> 11;
    uint32_t func = (last & 0x00000700) >> 8;

    func++;
    // skips to next slot if this slot's func 0 is either empty or non-multi func
    if (func < 8 && (func > 1 || (device_exists(last) && device_is_multi_func(last)))) {
        return create_config_address(bus, slot, func, 0);
    }

    func = 0;
    slot++;
    if (slot < 32) {
        return create_config_address(bus, slot, func, 0);
    }

    slot = 0;
    bus++;
    if (bus < 256) {
        return create_config_address(bus, slot, func, 0);
    }
    return 0; // No remaining addresses
}

uint32_t pci_find_next_device(uint32_t target_device_type, uint32_t mask, uint32_t start) {
    uint32_t address = start;
    do {
        address = next_base_address(address);
        if (device_exists(address)) {
            uint32_t device_type = pci_read_config_u32(address, DEVICE_TYPE_OFFSET);
            if ((device_type & mask) == (target_device_type & mask)) {
                return address;
            }
        }
    } while (address != 0);
    return 0;
}
