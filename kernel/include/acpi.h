#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char signature[4];
    uint32_t len;
    uint8_t revision;
    uint8_t checksum;
    char OEM_ID[6];
    char OEM_table_ID[8];
    uint32_t OEM_revision;
    uint32_t creator_ID;
    uint32_t creator_revision;
} __attribute__((packed)) ACPISDTHeader;

bool find_table(const char* signature, void** table_ptr);

void initialize_acpi(void* rsdp);
