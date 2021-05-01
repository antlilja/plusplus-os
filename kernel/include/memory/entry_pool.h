#pragma once
#include "memory.h"

typedef struct {
    void* next;
    uint64_t payload;
} MemoryEntry;

void fill_memory_entry_pool(VirtualAddress addr, uint64_t pages);

MemoryEntry* get_memory_entry();
void free_memory_entry(MemoryEntry* entry);
