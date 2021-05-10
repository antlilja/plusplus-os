// https://wiki.osdev.org/IOAPIC
// https://habr.com/en/post/446312/ Has a nice graphical overview of APICs

#pragma once

#include "memory.h"

#include <stdint.h>
#include <stdbool.h>


// IOAPIC Structs
typedef union {
    struct {
        uint64_t vector : 8;
        uint64_t delivery_mode : 3;
        uint64_t destination_mode : 1;
        uint64_t delivery_status : 1;
        uint64_t pin_polarity : 1;
        uint64_t remote_irr : 1;
        uint64_t trigger_mode : 1;
        uint64_t mask : 1;
        uint64_t reserved : 39;
        uint64_t destination : 8;
    };

    struct {
        uint32_t lower_dword;
        uint32_t upper_dword;
    };
} __attribute__((packed)) IOREDTBLEntry;


// Writes all segments of a IOREDTBL entry
void read_redtable_entry(void* ioapic_address, uint32_t irq, IOREDTBLEntry* entry);

// Reads all segments of a IOREDTBL entry
void write_redtable_entry(void* ioapic_address, uint32_t irq, IOREDTBLEntry* entry);

uint32_t read_ioapic_id(void* ioapic_address);
uint32_t read_ioapic_version(void* ioapic_address);

// The Max Redirection Entry is "how many IRQs can this I/O APIC handle - 1"
uint32_t read_ioapic_mre(void* ioapic_address);
