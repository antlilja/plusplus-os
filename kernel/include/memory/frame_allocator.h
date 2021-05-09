#pragma once
#include "memory/defs.h"

#include <stdbool.h>

#define FRAME_ORDERS 8

typedef struct {
    void* next;
    struct {
        uint64_t addr : 57;
        uint8_t order : 7;
    } __attribute__((packed));
} PageFrameAllocation;

void alloc_frame_allocator_memory(void* uefi_memory_map, PhysicalAddress* phys_addr,
                                  uint64_t* total_pages, uint64_t* entry_pool_pages);

void initialize_frame_allocator(VirtualAddress virt_addr, uint64_t total_pages,
                                void* uefi_memory_map, uint64_t entry_pool_pages);

// Get the size of blocks of specified order
uint64_t get_order_block_size(uint8_t order);

// Get the min size order from number of pages
// Return FRAME_ORDERS if order doesn't exist
uint8_t get_min_size_order(uint64_t pages);

// Frees frame allocation list
void free_frame_allocation_entries(PageFrameAllocation* allocations);

// Allocate page frames (allocation may consist of several non-contiguos blocks)
// Underlying memory for PageFrameAllocation structs is owned by the allocator
PageFrameAllocation* alloc_frames(uint64_t pages);

// Frees allocation
// Underlying memory for PageFrameAllocation structs will be reclaimed by the allocator
void free_frames(PageFrameAllocation* allocation);

// Allocate a block of contiguos memory (useful for DMA)
bool alloc_frames_contiguos(uint64_t pages, PhysicalAddress* out_addr);

// Free block of contiguos memory
void free_frames_contiguos(PhysicalAddress addr, uint64_t pages);
