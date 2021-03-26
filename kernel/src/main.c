#include <stdint.h>

typedef struct MemoryMap {
    uint64_t buffer_size;
    uint8_t* buffer;
    uint64_t mapkey;
    uint64_t desc_size;
    uint32_t desc_version;
} __attribute__((packed)) MemoryMap;

typedef enum {
    e_RGB8,
    e_BGR8,
    e_BitMask,
    e_BltOnly,
    e_FBFormatMax,
} FBFormat;

typedef struct Framebuffer {
    void* address;
    uint64_t width;
    uint64_t height;
    uint64_t pixels_per_scanline;
    FBFormat format;
} __attribute__((packed)) Framebuffer;

_Noreturn void kernel_entry(MemoryMap* mm, Framebuffer* fb) {
    // Fills the framebuffer with a beautiful color
    for (int y = 0; y < fb->height; ++y) {
        for (int x = 0; x < fb->width; ++x) {
            *((uint32_t*)(fb->address + 4 * fb->pixels_per_scanline * y + 4 * x)) = 0xffee2a7a;
        }
    }

    // This function can't return
    while (1)
        ;
}
