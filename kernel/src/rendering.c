#include "rendering.h"
#include "font.h"

Framebuffer* g_frame_buffer;
uint32_t g_bg_color = 0x00000000;
uint32_t g_fg_color = 0xffee2a7a;

void put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    // write to frame buffer, assumes 4 byte color.
    *((uint32_t*)(g_frame_buffer->address + 4 * g_frame_buffer->pixels_per_scanline * y + 4 * x)) =
        color;
}

void clear_screen(uint32_t color) {
    for (uint64_t x = 0; x < g_frame_buffer->width; x++)
        for (uint64_t y = 0; y < g_frame_buffer->height; y++) put_pixel(x, y, color);
}

void put_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg) {
    const uint8_t* glyph = font + (c - 31) * 16;
    int mask[8] = {128, 64, 32, 16, 8, 4, 2, 1};

    for (int cy = 0; cy < 16; ++cy) {
        for (int cx = 0; cx < 8; ++cx) {
            put_pixel(cx + c, cy + y, (glyph[cy] & mask[cx]) ? fg : bg);
        }
    }
}

uint64_t print_string(char* str, uint64_t x, uint64_t y) {
    uint64_t i = 0;
    while (str[i] != 0) {
        if (str[i] >= '!' && str[i] < '~') put_char(str[i], x + i, y, g_fg_color, g_bg_color);

        i++;
    }

    return i;
}

void print_hex_len(uint64_t value, uint64_t x, uint64_t y, uint64_t nibbles) {
    put_char('0', x, y, g_fg_color, g_bg_color);
    put_char('x', x + 1, y, g_fg_color, g_bg_color);

    for (uint64_t i = 0; i < nibbles; i++) {
        const int k = (nibbles - 1 - i) * 4;
        const uint8_t num = (value & (0xFUL << k)) >> k;
        char c = '0' + num;

        if (num >= 10) c = 'A' + num - 10;

        put_char(c, x + 2 + i, y, g_fg_color, g_bg_color);
    }
}

uint64_t print_hex(uint64_t value, uint64_t x, uint64_t y) {
    // find largest nonzero nibble
    uint64_t nibbles = 16;
    while (nibbles > 2 && !(value & (0xFUL << 4 * --nibbles)))
        ;

    print_hex_len(value, x, y, ++nibbles);
    return nibbles + 2;
}

void print_hex_32(uint64_t value, uint64_t x, uint64_t y) { print_hex_len(value, x, y, 8); }

void print_hex_64(uint64_t value, uint64_t x, uint64_t y) { print_hex_len(value, x, y, 16); }
