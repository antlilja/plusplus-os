#include "apic.h"

#include "acpi.h"
#include "port_io.h"
#include "memory/paging.h"
#include "kassert.h"

// Gets lower register offset for irq redtable entry
#define REDTBL_OFFSET(irq) (2 * (irq) + 0x10)
#define GET_BITRANGE_VALUE(num, lb, ub) (((num) & ((1U << (ub)) - (1U << (lb)))) >> (lb))

// IOAPIC IO
typedef volatile struct {
    uint32_t current_register __attribute__((aligned(0x10)));
    uint32_t register_value __attribute__((aligned(0x10)));
} __attribute__((packed)) IOAPIC;


uint32_t read_ioapic_register(void* ioapic_address, uint32_t offset) {
    IOAPIC* ioapic = (IOAPIC*)ioapic_address;

    ioapic->current_register = offset;
    return ioapic->register_value;
}

void write_ioapic_register(void* ioapic_address, uint32_t offset, uint32_t value) {
    IOAPIC* ioapic = (IOAPIC*)ioapic_address;

    ioapic->current_register = offset;
    ioapic->register_value = value;
}

uint32_t read_ioapic_id(void* ioapic_address) {
    const uint32_t r0 = read_ioapic_register(ioapic_address, 0);

    // Bits 24-27 contains the id
    return GET_BITRANGE_VALUE(r0, 24, 27);
}

uint32_t read_ioapic_version(void* ioapic_address) {
    const uint32_t r1 = read_ioapic_register(ioapic_address, 1);

    // Bits 0-8 contains the id
    return GET_BITRANGE_VALUE(r1, 0, 8);
}

uint32_t read_ioapic_mre(void* ioapic_address) {
    const uint32_t r1 = read_ioapic_register(ioapic_address, 1);

    // Bits 16-24 contains the Max Redirection Entry
    return GET_BITRANGE_VALUE(r1, 16, 24);
}

void read_redtable_entry(void* ioapic_address, uint32_t irq, IOREDTBLEntry* entry) {
    const uint8_t lower_offset = REDTBL_OFFSET(irq);
    const uint8_t upper_offset = lower_offset + 1;

    entry->lower_dword = read_ioapic_register(ioapic_address, lower_offset);
    entry->upper_dword = read_ioapic_register(ioapic_address, upper_offset);
}

void write_redtable_entry(void* ioapic_address, uint32_t irq, IOREDTBLEntry* entry) {
    const uint8_t lower_offset = REDTBL_OFFSET(irq);
    const uint8_t upper_offset = lower_offset + 1;

    write_ioapic_register(ioapic_address, lower_offset, entry->lower_dword);
    write_ioapic_register(ioapic_address, upper_offset, entry->upper_dword);
}
