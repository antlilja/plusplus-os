#pragma once
#include <stdint.h>

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiMaxMemoryType
} UEFIMemoryType;

typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t num_pages;
    uint64_t attribute;
} __attribute__((packed)) UEFIMemoryDescriptor;

typedef struct {
    uint64_t buffer_size;
    uint8_t* buffer;
    uint64_t mapkey;
    uint64_t desc_size;
    uint32_t desc_version;
} __attribute__((packed)) UEFIMemoryMap;
