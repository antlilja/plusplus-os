#pragma once
#include "memory/defs.h"
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

// Unmaps virtual address range and frees previously mapped frames
void unmap_and_free_frames(VirtualAddress virt_addr, uint64_t pages);

// Translates a virtual address into the corresponding physical address
// Returns false if the virtual address isn't mapped
bool get_physical_address(VirtualAddress virt_addr, PhysicalAddress* phys_addr);

void free_uefi_memory_and_remove_identity_mapping(void* uefi_memory_map);

VirtualAddress initialize_paging(void* uefi_memory_map, PhysicalAddress kernel_phys_addr,
                                 uint64_t kernel_size);
