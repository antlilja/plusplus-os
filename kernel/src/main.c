#include <stdint.h>
#include "rendering.h"
#include "gdt.h"
#include "idt.h"

typedef struct {
    uint64_t buffer_size;
    uint8_t* buffer;
    uint64_t mapkey;
    uint64_t desc_size;
    uint32_t desc_version;
} __attribute__((packed)) MemoryMap;

_Noreturn void kernel_entry(MemoryMap* mm, Framebuffer* fb) {
    // set frame buffer
    g_frame_buffer = fb;
    clear_screen(g_bg_color);

    put_string("       _     _____  _____  ", 10, 4);
    put_string("   _ _| |_  |     ||  ___| ", 10, 5);
    put_string(" _| |_   _| |  |  ||___  | ", 10, 6);
    put_string("|_   _|_|   |_____||_____| ", 10, 7);
    put_string("  |_|               v0.0.0 ", 10, 8);

    // This disables interrupts
    // They can be turned back on after setting up the IDT
    setup_gdt_and_tss();

    put_string("Global descriptor table initalized", 10, 10);

    // Interrupts are enabled here.
    // They can be registered using the register_interrupt(...) function
    setup_idt();
    put_string("Interrupt descriptor table initalized", 10, 11);

    // This function can't return
    while (1)
        ;
}
