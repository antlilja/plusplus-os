#include "rendering.h"
#include "gdt.h"
#include "idt.h"
#include "debug_log.h"

#include <stdint.h>
#include <string.h>

_Noreturn void kernel_entry(void* mm, void* fb, void* rsdp) {
    // set frame buffer
    memcpy((void*)&g_frame_buffer, (void*)fb, sizeof(g_frame_buffer));
    clear_screen(g_bg_color);

    put_string("       _     _____  _____  ", 10, 4);
    put_string("   _ _| |_  |     ||  ___| ", 10, 5);
    put_string(" _| |_   _| |  |  ||___  | ", 10, 6);
    put_string("|_   _|_|   |_____||_____| ", 10, 7);
    put_string("  |_|               Week-1 ", 10, 8);

    init_global_debug_log(5, 10);
    // This disables interrupts
    // They can be turned back on after setting up the IDT
    setup_gdt_and_tss();

    db_println_string("Global descriptor table initalized");

    // Interrupts are enabled here.
    // They can be registered using the register_interrupt(...) function
    setup_idt();
    db_println_string("Interrupt descriptor table initalized");

    // This function can't return
    while (1)
        ;
}
