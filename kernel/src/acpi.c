#include "acpi.h"

#include "kassert.h"
#include "memory.h"
#include "memory/paging.h"
#include "memory/entry_pool.h"
#include "uefi.h"

#include <string.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_phys_addr;
    uint32_t length;
    PhysicalAddress xsdt_phys_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) RSDP;

typedef ACPISDTHeader XSDT;

const XSDT* g_xsdt;

bool sdt_is_valid(const ACPISDTHeader* sdt, char* signature) {
    if (memcmp(sdt->signature, signature, 4)) return false;

    uint8_t* table_bytes = (uint8_t*)sdt;

    // All bytes of the table summed must be equal to 0 (mod 0x100)
    uint8_t sum = 0;
    for (uint32_t i = 0; i < sdt->length; i++) {
        sum += table_bytes[i];
    }

    return !sum;
}
    for (uint64_t i = 0; i < entries; ++i) {
typedef struct {
    PhysicalAddress phys;
    VirtualAddress virt;
    uint64_t size;
} ACPIMemRemap;

ACPIMemRemap* g_remap_list;
uint64_t g_remap_count;

void prepare_acpi_memory(void* uefi_memory_map) {
    UEFIMemoryMap* memory_map = (UEFIMemoryMap*)uefi_memory_map;

    uint64_t entry_count = 0;

    // Count pages used for ACPI
    for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
        UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];
        if (desc->type != EfiACPIMemoryNVS && desc->type != EfiACPIReclaimMemory) continue;

        entry_count += 1;
    }

    { // allocate and page remap entries

        // Round up needed pages to store remap entries
        uint64_t needed_pages = 1 + (entry_count * sizeof(ACPIMemRemap)) / PAGE_SIZE;

        PageFrameAllocation* alloc = alloc_frames(needed_pages);
        g_remap_list = (ACPIMemRemap*)map_allocation(alloc, PAGING_WRITABLE);

        // dealloc pool entry
        while (alloc != 0) {
            void* next = alloc->next;
            free_memory_entry((MemoryEntry*)alloc);

            alloc = next;
        }
    }

    // Store remap info
    for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
        UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];
        if (desc->type != EfiACPIMemoryNVS && desc->type != EfiACPIReclaimMemory) continue;

        const VirtualAddress virt = map_range(desc->physical_start, desc->num_pages, 0);
        ACPIMemRemap* entry = &g_remap_list[g_remap_count++];

        entry->phys = desc->physical_start;
        entry->virt = virt;
        entry->size = desc->num_pages * PAGE_SIZE;
    }
}

VirtualAddress get_virtual_acpi_address(PhysicalAddress physical) {
    for (uint64_t i = 0; i < g_remap_count; i++) {
        ACPIMemRemap* entry = &g_remap_list[i];

        // Find address in range
        if (entry->phys < physical && entry->phys + entry->size > physical)
            return (entry->virt - entry->phys + physical);
        }

    return 0;
    }

void initialize_acpi(PhysicalAddress rsdp_ptr) {
    RSDP* rsdp = (RSDP*)get_virtual_acpi_address(rsdp_ptr);
    KERNEL_ASSERT(rsdp, "RSDP VIRTUAL ADDRESS NOT FOUND");

    { // Validate rsdp, sum & 0xFF from bytes 0 - 20 AND 20 - 36 must be zero

        // Check signature
        const bool valid_rsdp_sig = !memcmp(rsdp, "RSD PTR ", 8);
        KERNEL_ASSERT(valid_rsdp_sig, "INVALID RSDP SIGNATURE");

        uint8_t sum = 0;

        for (int i = 0; i < 20; i++) sum += ((uint8_t*)rsdp)[i];
        KERNEL_ASSERT(!sum, "INVALID RSDP");

        // sum = 0 by condition
        for (int i = 20; i < 36; i++) sum += ((uint8_t*)rsdp)[i];
        KERNEL_ASSERT(!sum, "INVALID RSDP");
}

    g_xsdt = (const XSDT*)get_virtual_acpi_address(rsdp->xsdt_phys_addr);
    KERNEL_ASSERT(g_xsdt, "XSDT VIRTUAL ADDRESS NOT FOUND");

    bool valid_xsdt = sdt_is_valid(g_xsdt, "XSDT");
    KERNEL_ASSERT(valid_xsdt, "INVALID XSDT");
}
