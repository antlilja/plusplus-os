#pragma once
#include <stdint.h>

// The naked attribute is used to avoid the function prologue and epilogue

// wrapper for outb
__attribute__((naked)) void port_out_u8(uint16_t addr, uint8_t data);

// wrapper for outw
__attribute__((naked)) void port_out_u16(uint16_t addr, uint16_t data);

// wrapper for outl
__attribute__((naked)) void port_out_u32(uint16_t addr, uint32_t data);

// wrapper for inb
__attribute__((naked)) uint8_t port_in_u8(uint16_t addr);

// wrapper for inw
__attribute__((naked)) uint16_t port_in_u16(uint16_t addr);

// wrapper for inl
__attribute__((naked)) uint32_t port_in_u32(uint16_t addr);
