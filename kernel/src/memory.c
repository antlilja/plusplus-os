#include "memory.h"

#include "uefi.h"
#include "memory/frame_allocator.h"

uint64_t g_memory_size = 0;

void initialize_memory(void* uefi_memory_map) {
    // Calculate memory size
    {
        const UEFIMemoryMap* memory_map = (UEFIMemoryMap*)uefi_memory_map;
        for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
            const UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];
            PhysicalAddress high_addr = desc->physical_start + desc->num_pages * PAGE_SIZE;
            if (high_addr > g_memory_size) g_memory_size = high_addr;
        }
    }

    initialize_frame_allocator(uefi_memory_map);
}

uint64_t get_memory_size() { return g_memory_size; }
