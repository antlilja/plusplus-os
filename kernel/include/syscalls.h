#pragma once

#include <stdint.h>

// void syscall_halt()
#define SYSCALL_HALT 0

// void* syscall_alloc_pages(uint64_t pages)
#define SYSCALL_ALLOC_PAGES 1

// void syscall_get_framebuffer(Framebuffer* fb)
#define SYSCALL_GET_FRAMEBUFFER 2

// char syscall_getch()
#define SYSCALL_GETCH 3

// bool syscall_get_keystate(uint8_t keycode)
#define SYSCALL_GET_KEYSTATE 4

// Enables syscalls and fills syscall table
void prepare_syscalls();
