#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) ACPISDTHeader;

void prepare_acpi_memory(void* uefi_memory_map);
void initialize_acpi(PhysicalAddress rsdp_ptr);

VirtualAddress get_virtual_acpi_address(PhysicalAddress physical);
