#include "gdt.h"
#include <stdint.h>

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
    GDTEntry user_code;
    GDTEntry user_data;
    GDTEntry tss_low;
    GDTEntry tss_high;
} __attribute__((packed)) g_gdt = {
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
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    // Interrupt stack table
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    // IO bitmap
    uint16_t iopb_offset;
} __attribute__((packed)) g_tss = {0};

// Setup defines in assembly
// Weird C nested macros are required here to concatenate
// the inline assembly strings with the value of the macro as a string
#define STR(x) #x
#define XSTR(s) STR(s)
asm(".equ KERNEL_CODE_SEGMENT, " XSTR(GDT_KERNEL_CODE_SEGMENT) "\n");
asm(".equ KERNEL_DATA_SEGMENT, " XSTR(GDT_KERNEL_DATA_SEGMENT) "\n");
asm(".equ TSS_SEGMENT, " XSTR(GDT_TSS_SEGMENT) "\n");
#undef XSTR
#undef STR

// Define the function set_gdt in assembly
asm("set_gdt_and_tss:\n"

    // Disable interrupts before setting GDT and TSS
    "cli\n"

    // Set pointer to GDT
    "lgdt (%rdi)\n"

    // Setup offset to TSS
    "mov $TSS_SEGMENT, %ax\n"
    "ltr %ax\n"

    // Set all segment registers to the kernel data segment
    "mov $KERNEL_DATA_SEGMENT, %ax\n"
    "mov %ax, %ds\n"
    "mov %ax, %es\n"
    "mov %ax, %fs\n"
    "mov %ax, %gs\n"
    "mov %ax, %ss\n"

    // Push code segment onto stack before return address to use far return
    // this sets the code segment register (cs)
    "pop %rdi\n"
    "mov $KERNEL_CODE_SEGMENT, %rax\n"
    "push %rax\n"
    "push %rdi\n"
    "lretq\n");

// This is the function defined in assembly
extern void set_gdt_and_tss(void* gdt);

void setup_gdt_and_tss() {
    // Set io bitmap offset to the size of the TSS because we are not using it.
    g_tss.iopb_offset = sizeof(g_tss);

    // TODO (Anton Lilja, 29-03-2021):
    // Setup TSS properly with an interrupt stack table and privilege level stack pointers.

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
        uint64_t base;
    } __attribute__((packed)) gdt;

    // TODO(Anton Lilja, 10-04-21):
    // We have to do manual assignment here because the compiler might try to optimize in virtual
    // address pointers at compile time otherwise.
    // Find way to disable this optimization or
    // make sure to map kernel correctly before any of this code.
    gdt.size = sizeof(g_gdt) - 1;
    gdt.base = (uint64_t)&g_gdt;

    set_gdt_and_tss((void*)&gdt);
}
