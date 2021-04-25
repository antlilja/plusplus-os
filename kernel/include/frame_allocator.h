#pragma once
#include "memory.h"

#include <stdbool.h>

typedef struct {
    void* next;
    struct {
        uint64_t addr : 57;
        uint8_t order : 7;
    } __attribute__((packed));
} PageFrameAllocation;

void initialize_frame_allocator(void* uefi_memory_map);

// Allocate page frames (allocation may consist of several non-contiguos blocks)
// Underlying memory for PageFrameAllocation structs is owned by the allocator
PageFrameAllocation* alloc_frames(uint64_t pages);

// Frees allocation
// Underlying memory for PageFrameAllocation structs will be reclaimed by the allocator
void free_frames(PageFrameAllocation* allocation);

// Allocate a block of contiguos memory (useful for DMA)
bool alloc_frames_contiguos(uint8_t order, PhysicalAddress* out_addr);

// Free block of contiguos memory
void free_frames_contiguos(PhysicalAddress addr, uint8_t order);
