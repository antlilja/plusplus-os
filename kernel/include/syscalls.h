#pragma once

#include <stdint.h>

#define SYSCALL_HALT 0

// Enables syscalls and fills syscall table
void prepare_syscalls();
