#pragma once
#include <stdint.h>

#define INTERRUPT_GATE 14
#define TRAP_GATE 15

/* This is used when creating interrupt handlers using GCCs __atrribute__((interrupt))

   Example:

   __attribute__((interrupt)) void irq_handler(InterruptFrame* frame) {
       // Do stuff
   }
 */
typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t flags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) InterruptFrame;

void setup_idt();

void register_interrupt(uint8_t irq, uint8_t type, uint64_t handler_address);

