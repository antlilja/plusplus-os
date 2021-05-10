// https://wiki.osdev.org/IOAPIC
// https://habr.com/en/post/446312/ Has a nice graphical overview of APICs
// https://wiki.osdev.org/MADT

#pragma once

#include "memory.h"

#include <stdint.h>
#include <stdbool.h>

// 8259 + slave has 16(-1) IRQ lines
#define MAX_8259_IRQ_COUNT 16

// MADT Structs
typedef struct {
    uint8_t ioapic_id;
    PhysicalAddress physical_address;
    void* virtual_address;
    // Starting global system interrupt for ioapic
    uint32_t gsi_offset;
    uint32_t used_irqs;
} __attribute__((packed)) IOAPICInfo;

typedef struct {
    bool present;
    uint32_t gsi;
    // If (flags & 2) then the interrupt is active when low,
    // and if (flags & 8) then interrupt is level-triggered
    uint16_t flags;
} GSIOverrideEntry;

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


extern LocalAPIC* g_lapic;
extern GSIOverrideEntry g_gsi_override_table[MAX_8259_IRQ_COUNT];

// Finds, pages and prepares LAPICs and IOAPICs
void setup_apic();

// Gets the Local APIC id from a ACPI processor id
bool get_lapic_id(uint8_t acpi_id, uint8_t* lapic_id);

// Gets a pointer to the IOAPICInfo that deals with a certain Global System Interrupt
IOAPICInfo* get_responsible_ioapic(uint32_t gsi);

// Writes all segments of a IOREDTBL entry
void read_redtable_entry(void* ioapic_address, uint32_t irq, IOREDTBLEntry* entry);

// Reads all segments of a IOREDTBL entry
void write_redtable_entry(void* ioapic_address, uint32_t irq, IOREDTBLEntry* entry);

uint32_t read_ioapic_id(void* ioapic_address);
uint32_t read_ioapic_version(void* ioapic_address);

// The Max Redirection Entry is "how many IRQs can this I/O APIC handle - 1"
uint32_t read_ioapic_mre(void* ioapic_address);
