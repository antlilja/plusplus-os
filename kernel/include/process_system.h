#pragma once
#include "memory/paging.h"

#include <stdint.h>

AddressSpace* get_current_process_addr_space();
uint64_t get_current_process_pid();
