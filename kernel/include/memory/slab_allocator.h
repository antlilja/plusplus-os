#pragma once
#include "memory/paging.h"

#include <stdint.h>

// Allocates a block of memory which is at least the size specified
void* kalloc(uint64_t size);

// Frees memory allocated by kalloc
void kfree(void* ptr);

void initialize_slab_allocator();
