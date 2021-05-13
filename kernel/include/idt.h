#pragma once
#include <stdint.h>
#include <stdbool.h>

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

typedef struct {
    uint64_t err;
    uint64_t rip;
    uint64_t cs;
    uint64_t flags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) ErrorCodeInterruptFrame;

void setup_idt();

// Registers interrupt
void register_interrupt(uint8_t irq, uint8_t type, bool ist, void* handler);

// Unregisters interrupt
void unregister_interrupt(uint8_t irq);
