#include "apic.h"

#include "acpi.h"
#include "port_io.h"
#include "memory/paging.h"
#include "kassert.h"

#define MAX_IOAPIC_COUNT 4
#define MAX_LAPIC_COUNT 16

#define LOCAL_APIC_ENTRY 0
#define IO_APIC_ENTRY 1
#define INTERRUPT_SOURCE_OVERRIDE_ENTRY 2
#define NON_MASKABLE_INTERRUPTS_ENTRY 4
#define LOCAL_APIC_ADDRESS_OVERRIDE_ENTRY 5

#define APIC_BASE_MSR 0x1b
#define APIC_BASE_MSR_ENABLE 0x800

// Gets lower register offset for irq redtable entry
#define REDTBL_OFFSET(irq) (2 * (irq) + 0x10)
#define GET_BITRANGE_VALUE(num, lb, ub) (((num) & ((1U << (ub)) - (1U << (lb)))) >> (lb))

// IOAPIC IO
typedef volatile struct {
    uint32_t current_register __attribute__((aligned(0x10)));
    uint32_t register_value __attribute__((aligned(0x10)));
} __attribute__((packed)) IOAPIC;

// MADT Entry structs to avoid offsets, this also makes code super bloated on multiple fronts
typedef struct {
    ACPISDTHeader header;
    uint32_t local_apic_phys_addr;
    uint32_t flags;
} __attribute__((packed)) MADT;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) MADTEntry;

typedef struct {
    MADTEntry header;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) LocalAPICEntry;

typedef struct {
    MADTEntry header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t global_system_interrupt_base;
} __attribute__((packed)) IOAPICEntry;

typedef struct {
    MADTEntry header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed)) InterruptSourceOverrideEntry;

typedef struct {
    MADTEntry header;
    uint8_t acpi_processor_id;
    uint16_t flags;
    uint8_t LINT;
} __attribute__((packed)) NonMaskableInterruptsEntry;

typedef struct {
    MADTEntry header;
    uint8_t reserved[2];
    PhysicalAddress local_apic_phys_addr;
} __attribute__((packed)) LocalAPICAddressOverrideEntry;

// Pointer to the MMIO responsible for the Local APIC
LocalAPIC* g_lapic;

// Conversion table from 8259 IRQs to GSI
GSIOverrideEntry g_gsi_override_table[MAX_8259_IRQ_COUNT];

// References to the MADT's IOAPIC entries, uses g_ioapic_count
IOAPICInfo g_found_ioapics[MAX_IOAPIC_COUNT];
uint64_t g_ioapic_count = 0;

// References to the MADT's Local APIC entries, uses g_lapic_count
LocalAPICEntry* g_found_lapics[MAX_LAPIC_COUNT];
uint64_t g_lapic_count = 0;

void setup_apic() {

    { // Disable 8259 PIC. According to osdev, the PIC also have to be remapped to not raise
      // unwanted exceptions. Simply masking the interrupts isn't enough.

        // Start ICW4 Configuration
        port_out_u8(0x20, 0x11);
        port_out_u8(0xA0, 0x11);

        // Remap offsets to 0x20 - 0x30
        port_out_u8(0x21, 0x20);
        port_out_u8(0xA1, 0x28);

        // Cascade slave
        port_out_u8(0x21, 0x04);
        port_out_u8(0xA1, 0x02);

        // ICW4 x8086 mode
        port_out_u8(0x21, 0x01);
        port_out_u8(0xA1, 0x01);
        // end of configuration sequence

        // mask all interrupts
        port_out_u8(0x21, 0xff);
        port_out_u8(0xA1, 0xff);
    }

    const MADT* madt = (const MADT*)find_table("APIC");
    KERNEL_ASSERT(madt, "MADT NOT FOUND");

    { // Validate MADT
        bool madt_valid = sdt_is_valid(&madt->header, "APIC");
        KERNEL_ASSERT(madt_valid, "INVALID MADT");
    }

    // Fill Override table to have a corrent redirection when GSI = 8259 IRQ.
    for (int i = 0; i < MAX_8259_IRQ_COUNT; i++) g_gsi_override_table[i].gsi = i;

    PhysicalAddress local_apic_phys_addr = (PhysicalAddress)madt->local_apic_phys_addr;
    { // Parse MADT

        const void* end = (void*)madt + madt->header.length;
        void* curr_addr = (void*)madt + sizeof(MADT);

        // Entry offsets are found in
        while (curr_addr < end) {
            MADTEntry* entry_header = (MADTEntry*)curr_addr;
            curr_addr += entry_header->length;

            switch (entry_header->type) {
                case LOCAL_APIC_ENTRY:
                    g_found_lapics[g_lapic_count++] = (LocalAPICEntry*)entry_header;
                    break;

                case IO_APIC_ENTRY: {
                    const IOAPICEntry* entry = (const IOAPICEntry*)entry_header;

                    IOAPICInfo* ioapic = &g_found_ioapics[g_ioapic_count++];
                    ioapic->gsi_offset = entry->global_system_interrupt_base;
                    ioapic->ioapic_id = entry->ioapic_id;
                    ioapic->physical_address = (PhysicalAddress)entry->ioapic_address;

                    // Page each IOAPIC
                    ioapic->virtual_address = (void*)kmap_phys_range(
                        ioapic->physical_address, 1, PAGING_WRITABLE | PAGING_CACHE_DISABLE);

                    ioapic->used_irqs = 1 + read_ioapic_mre(ioapic->virtual_address);

                    { // ID mismatch could indicate that the IOAPIC can't be read
                        const uint8_t fetched_id = (uint8_t)read_ioapic_id(ioapic->virtual_address);
                        const uint8_t stored_id = ioapic->ioapic_id;

                        KERNEL_ASSERT(stored_id == fetched_id, "ID MISMATCH WHEN LOADING IOAPIC");
                    }

                    break;
                }

                case INTERRUPT_SOURCE_OVERRIDE_ENTRY: {
                    const InterruptSourceOverrideEntry* entry =
                        (const InterruptSourceOverrideEntry*)entry_header;

                    GSIOverrideEntry* gsi_override_entry = &g_gsi_override_table[entry->irq_source];
                    gsi_override_entry->present = true;
                    gsi_override_entry->gsi = entry->global_system_interrupt;
                    gsi_override_entry->flags = entry->flags;

                    break;
                }

                case LOCAL_APIC_ADDRESS_OVERRIDE_ENTRY: {
                    const LocalAPICAddressOverrideEntry* entry =
                        (const LocalAPICAddressOverrideEntry*)entry_header;

                    local_apic_phys_addr = entry->local_apic_phys_addr;
                    break;
                }

                default: break;
            }
        }
    }

    { // Enable Local APIC

        // Hardware enable LAPIC in case UEFI hasn't done so
        {
            uint32_t low, high;

            // EAX is lower half and EDX is upper half of 64 bit register.
            // ECX specifies which register to write to
            asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(APIC_BASE_MSR));

            // Set bit 11 to enable APIC
            low |= APIC_BASE_MSR_ENABLE;

            asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(APIC_BASE_MSR));
        }

        // Page Local APIC
        g_lapic = (LocalAPIC*)kmap_phys_range(
            local_apic_phys_addr, 1, PAGING_WRITABLE | PAGING_CACHE_DISABLE);

        // Set bit 8 of the Spurious Vector Register to enable the xAPIC
        // Using 0xFF as the Spurious Vector because osdev said so
        g_lapic->spurious_interrupt_vector = 0xFF | (1U << 8);

        g_lapic->task_priority &= (~(0xff));
    }
}

IOAPICInfo* get_responsible_ioapic(uint32_t gsi) {
    for (uint64_t i = 0; i < g_ioapic_count; i++) {
        IOAPICInfo* ioapic = &g_found_ioapics[i];

        // Check if ioapic contains the GSI
        if (ioapic->gsi_offset <= gsi && ioapic->gsi_offset + ioapic->used_irqs > gsi) {
            return ioapic;
        }
    }

    return (IOAPICInfo*)0;
}

bool get_lapic_id(uint8_t acpi_id, uint8_t* lapic_id) {
    for (uint64_t i = 0; i < g_lapic_count; i++) {
        LocalAPICEntry* entry = g_found_lapics[i];

        if (entry->processor_id == acpi_id) {
            *lapic_id = entry->apic_id;
            return true;
        }
    }

    return false;
}

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
