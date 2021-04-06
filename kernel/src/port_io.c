#include <stdint.h>
#include "port_io.h"

void port_out_u8(uint16_t addr, uint8_t data) {
    asm("movw %[addr], %%dx\n"
        "movb %[data], %%al\n"
        "outb %%al, %%dx\n"
        :
        : [data] "r"(data), [addr] "r"(addr)
        : "dx", "al");
}

void port_out_u16(uint16_t addr, uint16_t data) {
    asm("movw %[addr], %%dx\n"
        "movw %[data], %%ax\n"
        "outw %%ax, %%dx\n"
        :
        : [data] "r"(data), [addr] "r"(addr)
        : "dx", "ax");
}

void port_out_u32(uint16_t addr, uint32_t data) {
    asm("movw %[addr], %%dx\n"
        "movl %[data], %%eax\n"
        "outl %%eax, %%dx\n"
        :
        : [data] "r"(data), [addr] "r"(addr)
        : "dx", "eax");
}

uint8_t port_in_u8(uint16_t addr) {
    uint8_t data;
    asm("movw %[addr], %%dx\n"
        "inb %%dx, %%al\n"
        "movb %%al, %[data]\n"
        : [data] "=r"(data)
        : [addr] "r"(addr)
        : "dx", "al");
    return data;
}

uint16_t port_in_u16(uint16_t addr) {
    uint16_t data;
    asm("movw %[addr], %%dx\n"
        "inw %%dx, %%ax\n"
        "movw %%ax, %[data]\n"
        : [data] "=r"(data)
        : [addr] "r"(addr)
        : "dx", "ax");
    return data;
}

uint32_t port_in_u32(uint16_t addr) {
    uint32_t data;
    asm("movw %[addr], %%dx\n"
        "inl %%dx, %%eax\n"
        "movl %%eax, %[data]\n"
        : [data] "=r"(data)
        : [addr] "r"(addr)
        : "dx", "eax");
    return data;
}
