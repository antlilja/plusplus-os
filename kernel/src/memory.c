#include "memory.h"

#include "kassert.h"
#include "uefi.h"
#include "memory/paging.h"
#include "memory/frame_allocator.h"

uint64_t g_memory_size = 0;

void* alloc_pages(uint64_t pages, PagingFlags paging_flags) {
    PageFrameAllocation* allocation = alloc_frames(pages);
    if (allocation == 0) return 0;

    void* ptr = (void*)map_allocation(allocation, paging_flags);

    free_frame_allocation_entries(allocation);
    return ptr;
}

void free_pages(void* ptr, uint64_t pages) { unmap_and_free_frames((VirtualAddress)ptr, pages); }

void* alloc_pages_contiguous(uint64_t pages, PagingFlags paging_flags) {
    PhysicalAddress phys_addr;
    if (!alloc_frames_contiguos(pages, &phys_addr)) return 0;

    return (void*)map_range(phys_addr, pages, paging_flags);
}

void free_pages_contiguous(void* ptr, uint64_t pages) {
    PhysicalAddress phys_addr;
    const bool success = get_physical_address((VirtualAddress)ptr, &phys_addr);
    KERNEL_ASSERT(success, "Physical address does not exist");

    unmap((VirtualAddress)ptr, pages);
    free_frames_contiguos(phys_addr, pages);
}

uint64_t get_memory_size() { return g_memory_size; }

void initialize_memory(void* uefi_memory_map, PhysicalAddress* kernel_phys_addr,
                       VirtualAddress* kernel_virt_addr) {
    // Calculate memory size, get kernel address and kernel size
    uint64_t kernel_size = 0;
    {
        const UEFIMemoryMap* memory_map = (UEFIMemoryMap*)uefi_memory_map;
        for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
            const UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];
            PhysicalAddress high_addr = desc->physical_start + desc->num_pages * PAGE_SIZE;
            if (high_addr > g_memory_size) g_memory_size = high_addr;

            if (desc->type == EfiLoaderCode) {
                *kernel_phys_addr = desc->physical_start;
                kernel_size = desc->num_pages * PAGE_SIZE;
            }
        }
    }
    KERNEL_ASSERT(kernel_size != 0, "Could not find kernel address")

    // This call does a lot of things:
    // * Creates new page table and maps kernel into it.
    // * Initializes the paging system
    // * Initializes the frame allocator
    *kernel_virt_addr = initialize_paging(uefi_memory_map, *kernel_phys_addr, kernel_size);
}
