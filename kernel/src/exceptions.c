// https://wiki.osdev.org/Exceptions
#include "exceptions.h"

#include "idt.h"
#include "rendering.h"

#define DIV_BY_ZERO 0
#define DEBUG 1
#define NON_MASKABLE_INTERRUPT 2
#define BREAKPOINT 3
#define OVERFLOW 4
#define BOUND_RANGE_EXCEEDED 5
#define INVALID_OPCODE 6
#define DEVICE_NOT_AVAILABLE 7
#define DOUBLE_FAULT 8
#define INVALID_TSS 10
#define SEGMENT_NOT_PRESENT 11
#define STACK_SEGMENTATION_FAULT 12
#define GENERAL_PROTECTION_FAULT 13
#define PAGE_FAULT 14
#define FLOAT_EXCEPTION 16
#define ALIGNMENT_CHECK 17
#define MACHINE_CHECK 18
#define SIMD_FLOAT_EXCEPTION 19

__attribute__((interrupt)) void page_fault(ErrorCodeInterruptFrame* frame) {
    clear_screen(0);
    uint64_t x = 10;
    uint64_t y = 10;

    g_fg_color = 0xff0000;
    put_string("PAGE FAULT", x, y);

    x += put_string("Instruction   (RIP): ", x, ++y);
    x += put_hex(frame->rip, x, y);
    x = 10;

    x += put_string("Stack pointer (RSP): ", x, ++y);
    x += put_hex(frame->rsp, x, y);
    x = 10;

    // CR2 contains the address read that cause the exception
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    x += put_string("Page fault address: ", x, ++y);
    x += put_hex(cr2, x, y);
    x = 10;
    y++;

    // Error codes are found in https://wiki.osdev.org/Exceptions#Page_Fault
    put_string("Page fault info: ", x, ++y);
    put_string((frame->err & 1) ? "page-protection violation" : "non-present page", x, ++y);
    put_string((frame->err & 2) ? "while writing" : "while reading", x, ++y);
    put_string((frame->err & 16) ? "during an instruction fetch" : "", x, ++y);

    while (1)
        ;
}

void register_exception_interrupts() {
    register_interrupt(PAGE_FAULT, INTERRUPT_GATE, false, (void*)&page_fault);
}
