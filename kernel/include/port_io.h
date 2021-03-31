#pragma once
#include <stdint.h>

// wrapper for outb
void port_out_u8 (uint16_t addr, uint8_t data);

// wrapper for outw
void port_out_u16 (uint16_t addr, uint16_t data);

// wrapper for outl
void port_out_u32 (uint16_t addr, uint32_t data);

// wrapper for inb
uint8_t port_in_u8 (uint16_t addr);

// wrapper for inw
uint16_t port_in_u16 (uint16_t addr);

// wrapper for inl
uint32_t port_in_u32 (uint16_t addr);

