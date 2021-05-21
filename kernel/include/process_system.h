#pragma once
#include "memory/paging.h"

#include <stdint.h>

AddressSpace* get_current_process_addr_space();
uint64_t get_current_process_pid();

// Starts a user process
// NOTE: This function should only be called when at least one process is already running
void start_user_process(const void* elf_data);

void initialize_process_system(void* elf_data);
