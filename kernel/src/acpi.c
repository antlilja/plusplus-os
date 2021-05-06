#include "acpi.h"

#include "memory.h"

#include <string.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char OEM_ID[6];
    uint8_t revision;
    uint32_t rsdt_phys_addr;
    uint32_t len;
    PhysicalAddress xsdt_phys_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) RSDP;

typedef ACPISDTHeader XSDT;

const XSDT* g_xsdt;

bool find_table(const char* signature, void** table_ptr) {
    const PhysicalAddress* xsdt_arr =
        (const PhysicalAddress*)((PhysicalAddress)g_xsdt + sizeof(XSDT));
    const uint64_t entries = (g_xsdt->len - sizeof(XSDT)) / sizeof(PhysicalAddress);
    for (uint64_t i = 0; i < entries; ++i) {
        const ACPISDTHeader* header = (const ACPISDTHeader*)xsdt_arr[i];
        if (memcmp(header->signature, signature, 4) == 0) {
            *table_ptr = (void*)header;
            return true;
        }
    }

    return false;
}

void initialize_acpi(void* rsdp_ptr) {
    const RSDP* rsdp = (const RSDP*)rsdp_ptr;
    g_xsdt = (const XSDT*)rsdp->xsdt_phys_addr;
}
