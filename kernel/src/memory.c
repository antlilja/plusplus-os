#include "memory.h"

#include "memory/frame_allocator.h"

void initialize_memory(void* uefi_memory_map) { initialize_frame_allocator(uefi_memory_map); }
