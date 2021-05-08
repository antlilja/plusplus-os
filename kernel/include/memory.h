#pragma once
#include "memory/defs.h"



void initialize_memory(void* uefi_memory_map, PhysicalAddress* kernel_phys_addr,
                       VirtualAddress* kernel_virt_addr);

uint64_t get_memory_size();
