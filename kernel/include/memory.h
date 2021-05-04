#pragma once
#include <stdint.h>

#define PAGE_SIZE 0x1000ULL

typedef uint64_t PhysicalAddress;
typedef uint64_t VirtualAddress;

void initialize_memory(void* uefi_memory_map, PhysicalAddress* kernel_phys_addr,
                       VirtualAddress* kernel_virt_addr);

uint64_t get_memory_size();
