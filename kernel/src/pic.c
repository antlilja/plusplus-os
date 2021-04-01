#include "pic.h"
#include "port_io.h"

void setup_interrupt_requests() {
    // init with icw4
    port_out_u8(PIC1_COMMAND, 0x11);
    port_out_u8(PIC2_COMMAND, 0x11);

    // remap offsets to 0x20 - 0x30
    port_out_u8(PIC1_DATA, 0x20);
    port_out_u8(PIC2_DATA, 0x28);

    // cascade slave
    port_out_u8(PIC1_DATA, 0x04);
    port_out_u8(PIC2_DATA, 0x02);

    // ICW4 x8086 mode
    port_out_u8(PIC1_DATA, 0x01);
    port_out_u8(PIC2_DATA, 0x01);
    // end of initialization sequence

    // mask all interrupts
    port_out_u8(PIC1_DATA, 0xff);
    port_out_u8(PIC2_DATA, 0xff);
}

void send_end_of_interrupt(uint8_t irq_line) {
    // 0x20 is an EOI command.
    if (irq_line > 8) port_out_u8(PIC2_COMMAND, 0x20);

    port_out_u8(PIC1_COMMAND, 0x20);
}

void enable_interrupt_request(uint8_t irq_line) {
    uint16_t channel = PIC1_DATA;
    if (irq_line > 8) {
        channel = PIC2_DATA;
        irq_line -= 8;
    }

    // each PIC has a 1 bit flag for each request
    const uint8_t mask = port_in_u8(channel);
    port_out_u8(channel, mask & ~(1 << irq_line));
}

void disable_interrupt_request(uint8_t irq_line) {
    uint16_t channel = PIC1_DATA;
    if (irq_line > 8) {
        channel = PIC2_DATA;
        irq_line -= 8;
    }

    // each PIC has a 1 bit flag for each request
    const uint8_t mask = port_in_u8(channel);
    port_out_u8(channel, mask | (1 << irq_line));
}
