#include "rendering.h"
#include "font.h"

#include "util.h"
#include "memory/paging.h"

Framebuffer g_frame_buffer;
uint32_t g_bg_color = 0x00000000;
uint32_t g_fg_color = 0xffee2a7a;

void remap_framebuffer() {
    const uint64_t framebuffer_size = round_up_to_multiple(
        g_frame_buffer.width * g_frame_buffer.height * sizeof(uint32_t), PAGE_SIZE);

    const uint64_t framebuffer_pages = framebuffer_size / PAGE_SIZE;

    g_frame_buffer.address = (void*)kmap_phys_range((PhysicalAddress)g_frame_buffer.address,
                                                    framebuffer_pages,
                                                    PAGING_WRITABLE | PAGING_CACHE_DISABLE);
}

void put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    // write to frame buffer, assumes 4 byte color.
    *((uint32_t*)(g_frame_buffer.address + 4 * g_frame_buffer.pixels_per_scanline * y + 4 * x)) =
        color;
}

void clear_screen(uint32_t color) {
    for (uint64_t x = 0; x < g_frame_buffer.width; x++)
        for (uint64_t y = 0; y < g_frame_buffer.height; y++) put_pixel(x, y, color);
}

void put_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg) {
    const uint8_t* glyph = font + (c - 31) * 16;
    int mask[8] = {128, 64, 32, 16, 8, 4, 2, 1};

    for (int cy = 0; cy < 16; ++cy) {
        for (int cx = 0; cx < 8; ++cx) {
            put_pixel(cx + x * 8, cy + y * 16, (glyph[cy] & mask[cx]) ? fg : bg);
        }
    }
}

uint64_t put_string(const char* str, uint64_t x, uint64_t y) {
    uint64_t i = 0;
    while (str[i] != 0) {
        if (str[i] >= '!' && str[i] < '~') put_char(str[i], x + i, y, g_fg_color, g_bg_color);

        i++;
    }

    return i;
}

uint64_t put_binary(uint64_t value, uint64_t x, uint64_t y) {
    if (value == 0) {
        put_string("0b0", x, y);
        return 3;
    }

    uint8_t leading_zeroes = __builtin_clzll(value);

    put_binary_len(value, x, y, 64 - leading_zeroes);

    return 2 + 64 - leading_zeroes;
}

void put_binary_len(uint64_t value, uint64_t x, uint64_t y, uint8_t bits) {
    put_string("0b", x, y);

    for (uint8_t i = 0; i < bits; ++i) {
        const uint8_t bit = (value >> (bits - i - 1)) & 0b1;

        const char c = '0' + bit;
        put_char(c, x + 2 + i, y, g_fg_color, g_bg_color);
    }
}

void put_binary_32(uint64_t value, uint64_t x, uint64_t y) { put_binary_len(value, x, y, 32); }
void put_binary_64(uint64_t value, uint64_t x, uint64_t y) { put_binary_len(value, x, y, 64); }

uint64_t put_int(int64_t value, uint64_t x, uint64_t y) {
    if (value < 0) {
        put_char('-', x, y, g_fg_color, g_bg_color);
        return put_uint(-value, x + 1, y) + 1;
    }

    return put_uint(value, x, y);
}

uint64_t put_uint(uint64_t value, uint64_t x, uint64_t y) {
    if (value == 0) {
        put_char('0', x, y, g_fg_color, g_bg_color);
        return 1;
    }

    char buf[21];
    char* ptr = buf;

    while (value != 0) {
        uint8_t rem = value % 10;
        *ptr = '0' + rem;
        ++ptr;
        value /= 10;
    }

    *ptr = '\0';

    // Reverse string
    char* begin = buf;
    char* end = ptr - 1;
    while (begin < end) {
        char tmp = *begin;
        *begin = *end;
        *end = tmp;
        ++begin;
        --end;
    }

    put_string(buf, x, y);
    return ptr - buf;
}

void put_hex_len(uint64_t value, uint64_t x, uint64_t y, uint64_t nibbles) {
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

uint64_t put_hex(uint64_t value, uint64_t x, uint64_t y) {
    // find largest nonzero nibble
    uint64_t nibbles = 16;
    while (nibbles > 2 && !(value & (0xFUL << 4 * --nibbles)))
        ;

    put_hex_len(value, x, y, ++nibbles);
    return nibbles + 2;
}

void put_hex_32(uint64_t value, uint64_t x, uint64_t y) { put_hex_len(value, x, y, 8); }

void put_hex_64(uint64_t value, uint64_t x, uint64_t y) { put_hex_len(value, x, y, 16); }
