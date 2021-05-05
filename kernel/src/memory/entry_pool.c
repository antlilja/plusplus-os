#include "memory/entry_pool.h"

#include "memory/paging.h"
#include "memory/frame_allocator.h"
#include "kassert.h"

#define ENTRY_THRESHOLD 20

struct {
    uint64_t count;
    MemoryEntry* head;
} g_memory_entry_pool = {0};

void fill_memory_entry_pool(VirtualAddress addr, uint64_t pages) {
    _Static_assert(sizeof(MemoryEntry) == 16, "MemoryEntry struct is not 16 bytes");

    const uint64_t count = (pages * PAGE_SIZE) / sizeof(MemoryEntry);
    for (uint64_t i = 0; i < count; ++i) {
        MemoryEntry* entry = (MemoryEntry*)addr;
        entry->next = g_memory_entry_pool.head;
        g_memory_entry_pool.head = entry;
        addr += sizeof(MemoryEntry);
    }
    g_memory_entry_pool.count += count;
}

MemoryEntry* get_memory_entry() {
    if (g_memory_entry_pool.count < ENTRY_THRESHOLD) {
        g_memory_entry_pool.count += ENTRY_THRESHOLD;

        PageFrameAllocation* allocation = alloc_frames(0);
        KERNEL_ASSERT(allocation != 0, "Out of memory")

        const VirtualAddress virt_addr = map_allocation(allocation);

        g_memory_entry_pool.count -= ENTRY_THRESHOLD;

        fill_memory_entry_pool(virt_addr, 1);

        while (allocation != 0) {
            MemoryEntry* entry = (MemoryEntry*)allocation;
            allocation = allocation->next;

            free_memory_entry(entry);
        }
    }

    MemoryEntry* entry = g_memory_entry_pool.head;
    g_memory_entry_pool.head = entry->next;
    entry->next = 0;
    --g_memory_entry_pool.count;
    return entry;
}

void free_memory_entry(MemoryEntry* entry) {
    ++g_memory_entry_pool.count;
    entry->next = g_memory_entry_pool.head;
    g_memory_entry_pool.head = entry;
}
