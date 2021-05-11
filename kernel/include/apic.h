// https://wiki.osdev.org/IOAPIC
// https://habr.com/en/post/446312/ Has a nice graphical overview of APICs
// https://wiki.osdev.org/MADT
// https://wiki.osdev.org/APIC

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

// Local APIC Structs
typedef struct {
    const uint32_t isr0 __attribute__((aligned(0x10)));
    const uint32_t isr1 __attribute__((aligned(0x10)));
    const uint32_t isr2 __attribute__((aligned(0x10)));
    const uint32_t isr3 __attribute__((aligned(0x10)));
    const uint32_t isr4 __attribute__((aligned(0x10)));
    const uint32_t isr5 __attribute__((aligned(0x10)));
    const uint32_t isr6 __attribute__((aligned(0x10)));
    const uint32_t isr7 __attribute__((aligned(0x10)));
} __attribute__((packed)) ISR;

typedef struct {
    const uint32_t tmr0 __attribute__((aligned(0x10)));
    const uint32_t tmr1 __attribute__((aligned(0x10)));
    const uint32_t tmr2 __attribute__((aligned(0x10)));
    const uint32_t tmr3 __attribute__((aligned(0x10)));
    const uint32_t tmr4 __attribute__((aligned(0x10)));
    const uint32_t tmr5 __attribute__((aligned(0x10)));
    const uint32_t tmr6 __attribute__((aligned(0x10)));
    const uint32_t tmr7 __attribute__((aligned(0x10)));
} __attribute__((packed)) TMR;

typedef struct {
    const uint32_t irr0 __attribute__((aligned(0x10)));
    const uint32_t irr1 __attribute__((aligned(0x10)));
    const uint32_t irr2 __attribute__((aligned(0x10)));
    const uint32_t irr3 __attribute__((aligned(0x10)));
    const uint32_t irr4 __attribute__((aligned(0x10)));
    const uint32_t irr5 __attribute__((aligned(0x10)));
    const uint32_t irr6 __attribute__((aligned(0x10)));
    const uint32_t irr7 __attribute__((aligned(0x10)));
} __attribute__((packed)) IRR;

typedef volatile struct {
    uint8_t reserved0[0x20] __attribute__((aligned(0x10)));
    uint32_t id __attribute__((aligned(0x10)));
    uint32_t version __attribute__((aligned(0x10)));
    uint8_t reserved1[0x80 - 0x40] __attribute__((aligned(0x10)));
    uint32_t task_priority __attribute__((aligned(0x10)));
    const uint32_t arbitration_priority __attribute__((aligned(0x10)));
    const uint32_t processor_priority __attribute__((aligned(0x10)));
    uint32_t eoi __attribute__((aligned(0x10)));
    const uint32_t remote_read __attribute__((aligned(0x10)));
    uint32_t logical_destination __attribute__((aligned(0x10)));
    uint32_t destination_format __attribute__((aligned(0x10)));
    uint32_t spurious_interrupt_vector __attribute__((aligned(0x10)));
    const ISR in_service __attribute__((aligned(0x10)));
    const TMR trigger_mode __attribute__((aligned(0x10)));
    const IRR interrupt_request __attribute__((aligned(0x10)));
    const uint32_t error_status __attribute__((aligned(0x10)));
    uint8_t reserved2[0x300 - 0x290] __attribute__((aligned(0x10)));
    uint32_t icr_send __attribute__((aligned(0x10)));
    uint32_t icr_data __attribute__((aligned(0x10)));
    uint32_t lvt_timer __attribute__((aligned(0x10)));
    uint32_t lvt_thermal_sensor __attribute__((aligned(0x10)));
    // LVT Performance Monitoring Counters Register
    uint32_t lvt_pmcr __attribute__((aligned(0x10)));
    uint32_t lvt_lint0 __attribute__((aligned(0x10)));
    uint32_t lvt_lint1 __attribute__((aligned(0x10)));
    uint32_t lvt_error __attribute__((aligned(0x10)));
    uint32_t initial_count __attribute__((aligned(0x10)));
    const uint32_t current_count __attribute__((aligned(0x10)));
    uint8_t reserved4[0x3E0 - 0x3A0] __attribute__((aligned(0x10)));
    uint32_t divide_configuration __attribute__((aligned(0x10)));
} __attribute__((packed)) LocalAPIC;

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
