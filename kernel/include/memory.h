#pragma once
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 0x1000

typedef union {
    struct {
        bool present : 1;
        bool write : 1;
        bool user : 1;
        bool write_through : 1;
        bool cache_disable : 1;
        bool accessed : 1;
        bool dirty : 1;
        bool large : 1;
        bool global : 1;
        uint8_t ignored0 : 3;
        uint64_t phys_address : 40;
        uint8_t ignored1 : 7;
        uint8_t prot : 4;
        bool execute_disable : 1;
    } __attribute__((packed));
    uint64_t value;
} PageEntry;

typedef uint64_t PhysicalAddress;
typedef uint64_t VirtualAddress;

void remap_kernel_setup_stack(void* uefi_memory_map);
