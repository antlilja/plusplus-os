#pragma once
#include <stdint.h>
#include <stdbool.h>

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

bool find_table(const char* signature, void** table_ptr);

void initialize_acpi(void* rsdp);
