#include "rendering.h"
#include <stdint.h>

__attribute__((naked)) void* syscall_alloc_pages(uint64_t pages) {
    asm volatile("mov $3, %rax\n"
                 "syscall\n"
                 "ret");
}

__attribute__((naked)) void syscall_get_framebuffer(Framebuffer* fb) {
    asm volatile("mov $2, %rax\n"
                 "syscall\n"
                 "ret");
}

__attribute__((naked)) char syscall_getch() {
    asm volatile("mov $3, %rax\n"
                 "syscall\n"
                 "ret");
}

void draw_line(uint32_t color, uint64_t x, uint64_t h) {
    uint64_t y = 0;
    const uint64_t bottom = g_frame_buffer.height - 1;

    for (; y < h; y++) put_pixel(x, bottom - y, color);
    for (; y < 256; y++) put_pixel(x, bottom - y, g_bg_color);
}

void _start() {
    syscall_get_framebuffer(&g_frame_buffer);

    clear_screen(g_bg_color);
    put_string("ENTER NUMBERS TO SORT separated by ,", 0, 0);
    put_string("PRESS q TO FINISH", 0, 1);
    put_string("PRESS D to demonstrate", 0, 2);

    uint64_t numbers = 0;
    uint8_t numarr[512];

    uint64_t cindex = 0;
    uint8_t cnumber = 0;
    while (1) {
        char c = syscall_getch();
        if (c == 'd' || c == 'D') {
            for (int i = 0; i < 200; i++) {
                numbers++;
                numarr[i] = 200 - i;
            }

            break;
        }

        if (c >= '0' && c <= '9') {
            cnumber *= 10;
            cnumber += c - '0';

            put_char(c, cindex, 3, g_fg_color, g_bg_color);
        }
        else if (c == ',') {
            numarr[numbers++] = cnumber;
            put_char(c, cindex, 3, g_fg_color, g_bg_color);

            cindex += 1;
            cnumber = 0;
        }
        else if (c == 'q') {
            break;
        }

        cindex += 1;
    }

    uint64_t x0 = (g_frame_buffer.width - numbers) / 2;

    for (uint64_t i = 0; i < numbers; i++) {
        draw_line(g_fg_color, x0 + i, numarr[i]);
    }

    // delay
    for (uint64_t k = 0; k < 10000000; k++) put_pixel(0, 0, 0);

    for (uint64_t j = 0; j < numbers; j++) {
        for (uint64_t i = 1; i < numbers - j; i++) {
            if (numarr[i - 1] > numarr[i]) {
                const uint8_t temp = numarr[i - 1];
                numarr[i - 1] = numarr[i];
                numarr[i] = temp;
            }
            draw_line(0xff0000, x0 + i, numarr[i]);
            draw_line(g_fg_color, x0 + i - 1, numarr[i - 1]);

            // delay
            for (uint64_t k = 0; k < 100000; k++) put_pixel(0, 0, 0);
        }

        draw_line(g_fg_color, x0 + numbers - j - 1, numarr[numbers - j - 1]);
    }

    uint64_t y = 4;
    uint64_t x = 0;

    for (uint64_t i = 0; i < numbers; i++) {
        x += put_uint(numarr[i], x, y);
        x += put_string(", ", x, y);
        if (x >= g_frame_buffer.width / 8 - 5) {
            y++;
            x = 0;
        }
    }

    while (1)
        ;
}
