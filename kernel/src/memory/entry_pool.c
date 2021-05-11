#include "memory/entry_pool.h"

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

        void* memory = alloc_pages(1, PAGING_WRITABLE);
        KERNEL_ASSERT(memory != 0, "Out of memory")

        fill_memory_entry_pool((VirtualAddress)memory, 1);

        g_memory_entry_pool.count -= ENTRY_THRESHOLD;
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
