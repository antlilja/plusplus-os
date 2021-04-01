#include "port_io.h"

// The unsued attribute is used to avoid otherwise unavoidable unused parameter warnings
#define UNUSED __attribute__((unused))

__attribute__((naked)) void port_out_u8(UNUSED uint16_t addr, UNUSED uint8_t data) {
    asm("movw %di, %dx\n"
        "movw %si, %ax\n"
        "outb %al, %dx\n"
        "ret\n");
}

__attribute__((naked)) void port_out_u16(UNUSED uint16_t addr, UNUSED uint16_t data) {
    asm("movw %di, %dx\n"
        "movw %si, %ax\n"
        "outw %ax, %dx\n"
        "ret\n");
}

__attribute__((naked)) void port_out_u32(UNUSED uint16_t addr, UNUSED uint32_t data) {
    asm("movw %di, %dx\n"
        "movl %esi, %eax\n"
        "outl %eax, %dx\n"
        "ret\n");
}

__attribute__((naked)) uint8_t port_in_u8(UNUSED uint16_t addr) {
    asm("movw %di, %dx\n"
        "inb %dx, %al\n"
        "ret\n");
}

__attribute__((naked)) uint16_t port_in_u16(UNUSED uint16_t addr) {
    asm("movw %di, %dx\n"
        "inw %dx, %ax\n"
        "ret\n");
}

__attribute__((naked)) uint32_t port_in_u32(UNUSED uint16_t addr) {
    asm("movw %di, %dx\n"
        "inl %dx, %eax\n"
        "ret\n");
}
