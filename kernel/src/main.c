#include "memory/paging.h"
#include "rendering.h"
#include "memory.h"
#include "gdt.h"
#include "idt.h"
#include "acpi.h"
#include "pci.h"
#include "apic.h"
#include "exceptions.h"
#include "syscalls.h"
#include "process_system.h"
#include "ps2.h"
#include "ahci.h"
#include "fat.h"
#include "util.h"
#include "kassert.h"

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

_Noreturn void kernel_entry(void* mm, void* fb, PhysicalAddress rsdp) {
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

    remap_framebuffer();
    put_string("Framebuffer remapped to virtual address space", 10, 11);

    prepare_acpi_memory(mm);
    put_string("Prepared ACPI memory", 10, 12);

    // This disables interrupts
    setup_gdt_and_tss();
    put_string("Global descriptor table initalized", 10, 13);

    // Interrupts are enabled here.
    // They can be registered using the register_interrupt(...) function
    setup_idt();
    put_string("Interrupt descriptor table initalized", 10, 14);

    register_exception_interrupts();
    put_string("Registered exception interrupts", 10, 15);

    // After this point all physical addresses have to be mapped to virtual memory
    // NOTE: The memory pointed at by mm and fb should NOT be used after this point
    free_uefi_memory_and_remove_identity_mapping(mm);
    put_string("UEFI data deallocated and identity mapping removed", 10, 16);

    initialize_acpi(rsdp);
    put_string("ACPI Initialized", 10, 17);

    enumerate_pci_devices();
    put_string("PCI devices enumerated", 10, 18);

    setup_apic();
    put_string("APIC(s) set up and usable", 10, 19);

    initialize_ahci();
    put_string("AHCI Initialized", 10, 20);

    prepare_syscalls();
    put_string("Syscalls enabled", 10, 21);

    register_ps2_interrupt();
    put_string("Keyboard initialized", 10, 22);

    prepare_fat16_disk(0);

    DirectoryEntry entry;
    start_dir_read();

    uint64_t y = 25;
    void* terminal_buffer = 0;
    while (read_dir_next(&entry)) {
        put_string(entry.file_name, 10, ++y);

        if (!memcmp(entry.file_name, "SORTEX", 6)) {
            terminal_buffer = alloc_pages_contiguous(
                round_up_to_multiple(entry.file_size, PAGE_SIZE) / PAGE_SIZE, PAGING_WRITABLE);

            read_file_entry(&entry, terminal_buffer);
            break;
        }
    }

    KERNEL_ASSERT(terminal_buffer, "TERMINAL NOT FOUND")

    initialize_process_system(terminal_buffer);
    put_string("Process system initialized", 10, 23);

    // This function can't return
    while (1)
        ;
}
