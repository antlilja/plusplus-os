// Programmable Interrupt Controller
// https://wiki.osdev.org/PIC
#include <stdint.h>

#define PIC1		    0x20
#define PIC2		    0xA0

#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)


// will initialize PIC and slave to interrupts 0x20 to 0x30
// https://wiki.osdev.org/Interrupts#General_IBM-PC_Compatible_Interrupt_Information
void setup_interrupt_requests();

// sends an end of interrupt (EOI) to the used PIC(s)
// irq_line is 0x20 lower than the IDT index
// as example, the keyboard interrupt has IDT index 0x21 and therefore irq_line = 0x01
void send_end_of_interrupt(uint8_t irq_line);

// enables request handler irq_line by removing it from the correct PIC's mask
// irq_line is 0x20 lower than the interrupts IDT index
void enable_interrupt_request(uint8_t irq_line);

// disables request handler irq_line by adding it to the correct PIC's mask
// irq_line is 0x20 lower than the interrupts IDT index
void disable_interrupt_request(uint8_t irq_line);
