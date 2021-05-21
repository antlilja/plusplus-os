#include <stdint.h>

typedef struct {
    void* address;
    uint64_t width;
    uint64_t height;
    uint64_t pixels_per_scanline;
    uint32_t format;
} __attribute__((packed)) Framebuffer;

void __attribute__((naked)) get_framebuffer(Framebuffer* __attribute__((unused)) fb) {
    asm volatile("mov $2, %rax\n"
                 "syscall\n"
                 "ret");
}


_Noreturn void _start() {  
    Framebuffer fb;
    get_framebuffer(&fb);

    for (uint64_t i = 0; i < fb.height * fb.width; ++i) {
        ((uint32_t*)fb.address)[i] = 0xff0000;
    }

    while (1)
        ;
}
