#include "idt.h"
#include "gdt.h"

#include <stdint.h>
#include <string.h>

// https://wiki.osdev.org/Interrupt_Descriptor_Table#IDT_in_IA-32e_Mode_.2864-bit_IDT.29
typedef struct {
    uint16_t offset_0_15;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_16_31;
    uint32_t offset_32_63;
    uint32_t reserved;
} __attribute__((packed)) IDTEntry;

// IDT should be 8-byte aligned:
// https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf#G11.25354
IDTEntry __attribute__((aligned(8))) g_idt[256] = {0};

void setup_idt() {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) idt = {
        .size = sizeof(g_idt) - 1,
        .base = (void*)&g_idt,
    };

    asm(
        // Set IDT
        "lidt (%[idt])\n"
        // Enable interrupts
        "sti\n"
        : // No output
          // Input
        : [idt] "r"(&idt));
}

// General way to register an interrupt where you can set all values
void set_idt_gate(uint8_t irq, uint8_t type, uint64_t handler_address, uint16_t segment_selector,
                  uint8_t ist, uint8_t privilege) {
    g_idt[irq].offset_0_15 = handler_address & 0xffff;
    g_idt[irq].selector = segment_selector;
    g_idt[irq].ist = ist;
    g_idt[irq].type_attr = 0b10000000 | ((privilege & 0b11) << 6) | type;
    g_idt[irq].offset_16_31 = (handler_address >> 16) & 0xffff;
    g_idt[irq].offset_32_63 = (handler_address >> 32) & 0xffffffff;
}

void register_interrupt(uint8_t irq, uint8_t type, bool ist, void* handler) {
    set_idt_gate(irq, type, (uint64_t)handler, GDT_KERNEL_CODE_SEGMENT, 0, ist ? 1 : 0);
}

void unregister_interrupt(uint8_t irq) { memset((void*)&g_idt[irq], 0, sizeof(IDTEntry)); }
