#include "rendering.h"
#include "memory.h"
#include "gdt.h"
#include "idt.h"

#include <stdint.h>
#include <string.h>

__attribute__((naked)) void jump_to_kernel_virtual(PhysicalAddress __attribute__((unused))
                                                   kernel_phys_addr,
                                                   VirtualAddress __attribute__((unused))
                                                   kernel_virt_addr) {
    asm(
        // Calculate virtual address for base pointer
        "sub %rdi, %rbp\n"
        "add %rsi, %rbp\n"

        // Calculate virtual address for stack pointer
        "sub %rdi, %rsp\n"
        "add %rsi, %rsp\n"

        // Pop return address
        "pop %rdx\n"

        // Push cs offset onto stack
        "mov %cs, %ax\n"
        "push %rax\n"

        // Calculate virtual return address and push it onto stack
        "sub %rdi, %rdx\n"
        "add %rsi, %rdx\n"
        "push %rdx\n"

        // Do long return
        "lretq\n");
}

_Noreturn void kernel_entry(void* mm, void* fb, void* rsdp) {
    // Set frame buffer
    memcpy((void*)&g_frame_buffer, (void*)fb, sizeof(g_frame_buffer));
    clear_screen(g_bg_color);

    put_string("       _     _____  _____  ", 10, 4);
    put_string("   _ _| |_  |     ||  ___| ", 10, 5);
    put_string(" _| |_   _| |  |  ||___  | ", 10, 6);
    put_string("|_   _|_|   |_____||_____| ", 10, 7);
    put_string("  |_|               Week-1 ", 10, 8);

    // Initalize memory systems
    PhysicalAddress kernel_phys_addr;
    PhysicalAddress kernel_virt_addr;
    initialize_memory(mm, &kernel_phys_addr, &kernel_virt_addr);
    put_string("Memory initialized", 10, 9);

    jump_to_kernel_virtual(kernel_phys_addr, kernel_virt_addr);
    put_string("Kernel now running in virtual address space", 10, 10);

    // This disables interrupts
    setup_gdt_and_tss();
    put_string("Global descriptor table initalized", 10, 12);

    // Interrupts are enabled here.
    // They can be registered using the register_interrupt(...) function
    setup_idt();
    put_string("Interrupt descriptor table initalized", 10, 13);

    // This function can't return
    while (1)
        ;
}
