#include <stdint.h>
#include "rendering.h"

typedef struct MemoryMap {
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

    print_string("it's gamertime B)", 10, 10);
    print_hex_32(0xdeadbeef, 10, 11);
    print_hex_64(0xdeadbeefdeadbeefUL, 10, 12);

    uint64_t rip;
    asm("leaq (%%rip), %0" : "=r"(rip));

    g_bg_color = 0xff000000;

    // don't do this
    print_hex(rip, 10 + print_string("instruction pointer: ", 10, 15), 15);

    // This function can't return
    while (1)
        ;
}
