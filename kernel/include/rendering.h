#pragma once
#include <stdint.h>

typedef enum {
    e_RGB8,
    e_BGR8,
    e_BitMask,
    e_BltOnly,
    e_FBFormatMax,
} FBFormat;

typedef struct {
    void* address;
    uint64_t width;
    uint64_t height;
    uint64_t pixels_per_scanline;
    FBFormat format;
} __attribute__((packed)) Framebuffer;

extern uint32_t g_bg_color;
extern uint32_t g_fg_color;
extern Framebuffer g_frame_buffer;

void remap_framebuffer();

// Color pixel at pixel coordinates
void put_pixel(uint64_t x, uint64_t y, uint32_t color);

// Put character at character coordinates
void put_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg);

// Put value in binary at character coordinates
// returns the number of characters written (including 0b)
uint64_t put_binary(uint64_t value, uint64_t x, uint64_t y);

// Put exact amount of bits at character coordinates
void put_binary_len(uint64_t value, uint64_t x, uint64_t y, uint8_t bits);

void put_binary_32(uint64_t value, uint64_t x, uint64_t y);
void put_binary_64(uint64_t value, uint64_t x, uint64_t y);

// Put value in decimal at character coordinates
// returns the number of characters written (including '-')
uint64_t put_int(int64_t value, uint64_t x, uint64_t y);

// Put value in decimal at character coordinates
// returns the number of characters written
uint64_t put_uint(uint64_t value, uint64_t x, uint64_t y);

// Put value in hex at character coordinates
// returns the number of characters written (including 0x)
uint64_t put_hex(uint64_t value, uint64_t x, uint64_t y);

// Put exact amount of hex characters (nibbles) at character coordinates
void put_hex_len(uint64_t value, uint64_t x, uint64_t y, uint64_t nibbles);

// Put 32 bits of an integer at character coordinates
void put_hex_32(uint64_t value, uint64_t x, uint64_t y);

// Put 64 bits of an integer at character coordinates
void put_hex_64(uint64_t value, uint64_t x, uint64_t y);

// Clear screen with a certain color
void clear_screen(uint32_t color);

// Put string at character coordinates and return length
uint64_t put_string(const char* str, uint64_t x, uint64_t y);
