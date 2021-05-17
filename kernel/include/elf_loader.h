#pragma once
#include "memory/paging.h"

#include <stdbool.h>

bool load_elf_from_buff(AddressSpace* addr_space, const void* data, void** entry_point);
