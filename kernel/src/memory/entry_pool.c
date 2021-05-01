#include "memory/entry_pool.h"

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
        PhysicalAddress addr = 0;
        bool success = alloc_frames_contiguos(0, &addr);

        // TODO(Anton Lilja, 01/05/2021):
        // This is temporary and only required because we're using physical addressing,
        // The next member in MemoryEntry has to be able to be 0 to indicate end of list.
        if (addr == 0 && success) {
            success = alloc_frames_contiguos(0, &addr);
            free_frames_contiguos(0, 0);
        }

        KERNEL_ASSERT(success, "Out of memory")
        fill_memory_entry_pool(addr, 1);
    }

    MemoryEntry* entry = g_memory_entry_pool.head;
    g_memory_entry_pool.head = entry->next;
    --g_memory_entry_pool.count;
    return entry;
}

void free_memory_entry(MemoryEntry* entry) {
    ++g_memory_entry_pool.count;
    entry->next = g_memory_entry_pool.head;
    g_memory_entry_pool.head = entry;
}
