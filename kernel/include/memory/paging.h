#pragma once
#include "memory.h"
#include "memory/frame_allocator.h"

#define PAGING_WRITABLE 1
#define PAGING_EXECUTABLE 2
#define PAGING_CACHE_DISABLE 4
#define PAGING_WRITE_THROUGH 8

#define SIGN_EXT_ADDR(addr) ((addr) | (((((addr) >> 47) & 1) * 0xffffULL) << 48))

typedef uint32_t PagingFlags;

// Maps discrete allocations into contiguos virtual address space
VirtualAddress map_allocation(PageFrameAllocation* allocation, PagingFlags flags);

// Maps the physical address range to available virtual address space
VirtualAddress map_range(PhysicalAddress phys_addr, uint64_t pages, PagingFlags flags);

// Unmaps virtual address space
void unmap(VirtualAddress virt_addr, uint64_t pages);

void free_uefi_memory_and_remove_identity_mapping(void* uefi_memory_map);

VirtualAddress initialize_paging(void* uefi_memory_map, PhysicalAddress kernel_phys_addr,
                                 uint64_t kernel_size);
