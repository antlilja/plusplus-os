// https://wiki.osdev.org/PCI

#pragma once
#include "memory.h"

#include <stdbool.h>
#include <stdint.h>

#define PCI_DEVICE_CLASS_MASK 0xff000000
#define PCI_DEVICE_SUBCLASS_MASK 0xffff0000
#define PCI_DEVICE_PROG_IF_MASK 0xffffff00
#define PCI_DEVICE_REVISION_ID_MASK 0xffffffff

// PCI config space for header type 0
typedef volatile struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_ID;
    uint8_t prog_IF;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t BIST;
    uint32_t BAR0;
    uint32_t BAR1;
    uint32_t BAR2;
    uint32_t BAR3;
    uint32_t BAR4;
    uint32_t BAR5;
    uint32_t cardbus_CIS_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_ROM_base_addr;
    uint8_t capabilities_pointer;
    uint8_t reserved[7];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_grant;
} __attribute__((packed)) PCIConfigSpace0;

PCIConfigSpace0* get_pci_device(uint32_t type, uint32_t mask);

void enumerate_pci_devices();
