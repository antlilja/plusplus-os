#include "gdt.h"
#include <stdint.h>

#include "memory.h"

#define TSS_STACK_PAGES 2

// https://wiki.osdev.org/Global_Descriptor_Table
typedef struct {
    uint16_t limit_0_15;
    uint16_t base_0_15;
    uint8_t base_16_23;
    uint8_t access;
    uint8_t limit_16_19_and_flags;
    uint8_t base_24_31;
} __attribute__((packed)) GDTEntry;

struct {
    GDTEntry null;
    GDTEntry kernel_code;
    GDTEntry kernel_data;
    GDTEntry null2;
    GDTEntry user_data;
    GDTEntry user_code;
    GDTEntry tss_low;
    GDTEntry tss_high;
} __attribute__((packed)) __attribute__((aligned(8))) g_gdt = {
    // https://wiki.osdev.org/Global_Descriptor_Table
    // Null segments are required
    .null = {0, 0, 0, 0, 0, 0},
    .null2 = {0, 0, 0, 0, 0, 0},
    // Privilege level zero kernel segments
    .kernel_code = {0, 0, 0, 0b10011010, 0b10100000, 0},
    .kernel_data = {0, 0, 0, 0b10010010, 0b10100000, 0},
    // Privilege level three user segments
    .user_code = {0, 0, 0, 0b11111010, 0b10100000, 0},
    .user_data = {0, 0, 0, 0b11110010, 0b10100000, 0},
    // TSS segment
    .tss_low = {0, 0, 0, 0b10001001, 0b10100000, 0},
    .tss_high = {0, 0, 0, 0, 0, 0},
};

// https://wiki.osdev.org/Task_State_Segment#x86_64_Structure
struct {
    uint32_t reserved0;
    // Stack pointers for different privilege levels
    void* rsp[3];
    uint64_t reserved1;
    void* interrupt_stack_table[7];
    uint8_t reserved2[10];
    // IO bitmap
    uint16_t iopb_offset;
} __attribute__((packed)) __attribute__((aligned(8))) g_tss = {0};

__attribute__((naked)) void set_gdt_and_tss(void* __attribute__((unused)) gdt) {
    asm volatile(
        // Disable interrupts before setting GDT and TSS
        "cli\n"

        // Set pointer to GDT
        "lgdt (%%rdi)\n"

        // Setup offset to TSS
        "mov %[tss_segment], %%ax\n"
        "ltr %%ax\n"

        // Set all segment registers to the kernel data segment
        "mov %[kernel_data_segment], %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"

        // Push code segment onto stack before return address to use far return
        // this sets the code segment register (cs)
        "pop %%rdi\n"
        "mov %[kernel_code_segment], %%rax\n"
        "push %%rax\n"
        "push %%rdi\n"
        "lretq\n"
        :
        : [kernel_code_segment] "i"(GDT_KERNEL_CODE_SEGMENT),
          [kernel_data_segment] "i"(GDT_KERNEL_DATA_SEGMENT),
          [tss_segment] "i"(GDT_TSS_SEGMENT));
}

void set_tss_kernel_stack(void* stack_ptr) { g_tss.rsp[0] = stack_ptr; }

void setup_gdt_and_tss() {
    // Set io bitmap offset to the size of the TSS because we are not using it.
    g_tss.iopb_offset = sizeof(g_tss);

    // Allocate one entry of the interrupt descriptor table
    g_tss.interrupt_stack_table[0] =
        (void*)alloc_pages(TSS_STACK_PAGES, PAGING_WRITABLE) + TSS_STACK_PAGES * PAGE_SIZE;

    // Setup GDT entry for the TSS
    // The address is split up into several fields
    uint64_t tss_base = (uint64_t)&g_tss;
    g_gdt.tss_low.limit_0_15 = sizeof(g_tss);
    g_gdt.tss_low.base_0_15 = tss_base & 0xffff;
    g_gdt.tss_low.base_16_23 = (tss_base >> 16) & 0xff;
    g_gdt.tss_low.base_24_31 = (tss_base >> 24) & 0xff;
    g_gdt.tss_high.limit_0_15 = (tss_base >> 32) & 0xffff;
    g_gdt.tss_high.base_0_15 = (tss_base >> 48) & 0xffff;

    // We will give a pointer to this struct to the lgdt instruction
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) gdt = {
        .size = sizeof(g_gdt) - 1,
        .base = (void*)&g_gdt,
    };

    set_gdt_and_tss((void*)&gdt);
}
