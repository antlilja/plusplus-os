#pragma once
#include "memory/defs.h"
#include "memory/paging.h"
#include "memory/slab_allocator.h"

// Allocates physically non contiguos pages which are mapped with flags applied
void* alloc_pages(uint64_t pages, PagingFlags paging_flags);

// Frees pages allocated by alloc_pages
void free_pages(void* ptr, uint64_t pages);

// Allocates physically contiguos pages which are mapped with flags applied
void* alloc_pages_contiguous(uint64_t pages, PagingFlags paging_flags);

// Frees pages allocated by alloc_pages_contiguos
void free_pages_contiguous(void* ptr, uint64_t pages);

void initialize_memory(void* uefi_memory_map, PhysicalAddress* kernel_phys_addr,
                       VirtualAddress* kernel_virt_addr);

uint64_t get_memory_size();
