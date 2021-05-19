#include "ps2.h"
#include "port_io.h"
#include "apic.h"
#include "idt.h"
#include "kassert.h"
#include "rendering.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define PS2_STATUS 0x64
#define PS2_KEYCODE 0x60

#define KEYCODE_LSHIFT 0x2A
#define KEYCODE_RSHIFT 0x36

bool g_kbstatus[0x80];
volatile char g_last_char_changed = 0;

char translation_table[0x80] = {
    0,   0,   '1', '2', '3',  '4', '5', '6',  '7', '8', '9', '0', '-', '=', 0,   0,   'q', 'w',
    'e', 'r', 't', 'y', 'u',  'i', 'o', 'p',  '[', ']', 0,   0,   'a', 's', 'd', 'f', 'g', 'h',
    'j', 'k', 'l', ';', '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/'};

char shift_translation_table[0x80] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,   'Q', 'W',
    'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,   'A', 'S', 'D', 'F', 'G', 'H',
    'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?'};

bool upper_case() { return g_kbstatus[KEYCODE_LSHIFT] || g_kbstatus[KEYCODE_RSHIFT]; }

char scan_code_to_letter(uint8_t scan_code) {
    char* table = upper_case() ? shift_translation_table : translation_table;
    return table[scan_code];
}

bool has_byte() { return (port_in_u8(PS2_STATUS) & 1) != 0; }

char ps2_getch() {
    g_last_char_changed = 0;

    // Wait for key press
    while (g_last_char_changed == 0)
        ;

    return g_last_char_changed;
}

__attribute__((interrupt)) void kb_handler(InterruptFrame* __attribute__((unused)) frame) {
    if (has_byte() == false) return;

    uint8_t scan_code = port_in_u8(PS2_KEYCODE);
    const bool pressed = !(scan_code & 0x80); // sign bit

    scan_code &= ~0x80;
    g_kbstatus[scan_code] = pressed;

    if (pressed) {
        g_last_char_changed = scan_code_to_letter(scan_code);
    }

    g_lapic->eoi = 0;
}

void register_ps2_interrupt() {

    // 1 is the PIC irq for ps/2 kb
    const uint32_t gsi = g_gsi_override_table[1].gsi;
    IOAPICInfo* ioapic = get_responsible_ioapic(gsi);

    KERNEL_ASSERT(ioapic, "NO IOAPIC THAT USES KB WAS FOUND");

    const uint32_t irq = gsi - ioapic->gsi_offset;

    IOREDTBLEntry entry;
    read_redtable_entry(ioapic->virtual_address, irq, &entry);
    // Enable interrupt
    entry.mask = 0;

    // Fixed delivery mode
    entry.delivery_mode = 0;

    // Interrupt Vector
    entry.vector = 0x21;

    // Destination lapic
    entry.destination = g_lapic->id;

    write_redtable_entry(ioapic->virtual_address, irq, &entry);

    register_interrupt(0x21, INTERRUPT_GATE, false, &kb_handler);
}
