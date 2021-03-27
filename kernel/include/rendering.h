#pragma once
#include <stdint.h>

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

extern uint64_t g_bg_color;
extern uint64_t g_fg_color;
extern Framebuffer* g_frame_buffer;

void put_pixel(uint64_t x, uint64_t y, uint32_t color);
void put_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg);

//print value in hex, where size is determined and returned
uint64_t print_hex(uint64_t value, uint64_t x, uint64_t y);

//print a certain amount of hex characters
void print_hex_padleft(uint64_t value, uint64_t x, uint64_t y, uint64_t nibbles);

//print 32 bits of an integer at character coordinates
void print_hex_32(uint64_t value, uint64_t x, uint64_t y);

//print 64 bits of an integer at character coordinates
void print_hex_64(uint64_t value, uint64_t x, uint64_t y);

//clear screen with a certain color
void clear_screen(uint64_t color);

//print string and return length
uint64_t print_string(char* str, uint64_t x, uint64_t y);