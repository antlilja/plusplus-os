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

static uint8_t translate_table[0x80] = {
    0xff, 0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A,
    0x2B, 0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30, 0x28, 0xE0,
    0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33, 0x34, 0x35, 0xE1, 0x31, 0x1D,
    0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xE5, 0xff, 0xE2, 0x2C, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0x3E, 0x40, 0x41, 0x42, 0x43, 0xff, 0xff, 0x5F, 0x60, 0x61, 0x56, 0x5C,
    0x97, 0x5E, 0x57, 0x59, 0x5A, 0x5B, 0x62, 0x63, 0xff, 0xff, 0x64, 0x44, 0x45};

bool upper_case() { return g_kbstatus[KEYCODE_LSHIFT] || g_kbstatus[KEYCODE_RSHIFT]; }

char scan_code_to_letter(uint8_t scan_code) {
    // This should be a set of translation tables from keycode to ascii for shift and altgr
    scan_code = translate_table[scan_code];

    // Letters
    if (scan_code >= 4 && scan_code <= 0x1d) {
        const uint8_t offset = (upper_case() ? 'A' : 'a') - 4;

        return scan_code + offset;
    }

    // Numbers 1 - 9
    if (scan_code >= 0x1E && scan_code < 0x27) {
        const uint8_t offset = (upper_case() ? '!' : '1') - 0x1E;

        return scan_code + offset;
    }

    // Zero
    if (scan_code == 0x27) return upper_case() ? '=' : '0';

    return 0;
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
