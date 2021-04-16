#include "rendering.h"
#include "usb.h"
#include "pci.h"
#include "port_io.h"

#include <stdint.h>
#include <string.h>

/*
Reference: https://wiki.osdev.org/Universal_Host_Controller_Interface
*/

#define UHCI_CONTROLLER_ADDRESS 0x0C030000
#define UHCI_CONTROLLER_MASK DEVICE_PROG_IF_MASK

#define BAR0_OFFSET 0x10
#define BAR4_OFFSET 0x20

#define MAX_QH 8
#define MAX_TD 32

// UHCI Controller I/O Registers
#define REG_CMD 0x00       // Usb Command
#define REG_STS 0x02       // Usb Status
#define REG_INTR 0x04      // Usb Interrupt Enable
#define REG_FRNUM 0x06     // Frame Number
#define REG_FRBASEADD 0x08 // Frame List Base Address
#define REG_SOFMOD 0x0C    // Start Of Frame Modify
#define REG_PORTSC1 0x10   // Port 1 Status/Control
#define REG_PORTSC2 0x12   // Port 2 Status/Control

// Command Register
#define CMD_RUN (1 << 0)
#define CMD_HOST_CONTROLLER_RESET (1 << 1)
#define CMD_GLOBAL_RESET (1 << 2)
#define CMD_GLOBAL_SUSPEND (1 << 3)
#define CMD_GLOBAL_RESUME (1 << 4)
#define CMD_SOFTWARE_DEBUG (1 << 5)
#define CMD_CONFIGURE (1 << 6)
#define CMD_MAX_PACKET (1 << 7)

// Status Register
#define STS_INTERRUPT (1 << 0)
#define STS_ERROR_INTERRUPT (1 << 1)
#define STS_RESUME_DETECTED (1 << 2)
#define STS_SYSTEM_ERROR (1 << 3)
#define STS_PROCESS_ERROR (1 << 4)
#define STS_HALTED (1 << 5)

// Interrupt Enable Register
#define INTR_TIMEOUT (1 << 0)
#define INTR_RESUME (1 << 1)
#define INTR_COMPLETE (1 << 2)
#define INTR_SHORT_PACKET (1 << 3)

// Transfer Descriptor

// TD Link Pointer
#define TD_PTR_TERMINATE (1 << 0)
#define TD_PTR_QH (1 << 1)
#define TD_PTR_DEPTH (1 << 2)

// TD Control and Status
#define TD_CS_ACTLEN 0x000007ff
#define TD_CS_BITSTUFF (1 << 17)    // Bitstuff Error
#define TD_CS_CRC_TIMEOUT (1 << 18) // CRC/Time Out Error
#define TD_CS_NAK (1 << 19)         // NAK Received
#define TD_CS_BABBLE (1 << 20)      // Babble Detected
#define TD_CS_DATABUFFER (1 << 21)  // Data Buffer Error
#define TD_CS_STALLED (1 << 22)     // Stalled
#define TD_CS_ACTIVE (1 << 23)      // Active
#define TD_CS_IOC (1 << 24)         // Interrupt on Complete
#define TD_CS_IOS (1 << 25)         // Isochronous Select
#define TD_CS_LOW_SPEED (1 << 26)   // Low Speed Device
#define TD_CS_ERROR_MASK (3 << 27)  // Error counter
#define TD_CS_ERROR_SHIFT 27
#define TD_CS_SPD (1 << 29) // Short Packet Detect

// TD Token
#define TD_TOK_PID_MASK 0x000000ff     // Packet Identification
#define TD_TOK_DEVADDR_MASK 0x00007f00 // Device Address
#define TD_TOK_DEVADDR_SHIFT 8
#define TD_TOK_ENDP_MASK 00x0078000 // Endpoint
#define TD_TOK_ENDP_SHIFT 15
#define TD_TOK_D 0x00080000 // Data Toggle
#define TD_TOK_D_SHIFT 19
#define TD_TOK_MAXLEN_MASK 0xffe00000 // Maximum Length
#define TD_TOK_MAXLEN_SHIFT 21

#define TD_PACKET_IN 0x69
#define TD_PACKET_OUT 0xe1
#define TD_PACKET_SETUP 0x2d

// ---------------------------------------------------------------------------------------

typedef struct UhciTD {
    volatile uint32_t link;
    volatile uint32_t cs;
    volatile uint32_t token;
    volatile uint32_t buffer;

    // Internal fields
    uint32_t td_next;
    uint8_t active;
    uint8_t pad[11];
} UhciTD;

typedef struct UhciQH {
    volatile uint32_t head;
    volatile uint32_t element;

} UhciQH;

// Uhci Controller
typedef struct UhciController {
    uint16_t io_address;
    uint16_t* frame_list;
    UhciQH* qh_pool;
    UhciTD* td_pool;
    UhciQH* asyncQH;

} UhciController;

// ---------------------------------------------------------------------------------------
void print_io_reg(uint16_t io_address) {
    put_string("REG_CMD", 10, 16);
    put_string("REG_STS", 10, 17);
    put_string("REG_INTR", 10, 18);
    put_string("REG_FRNUM", 10, 19);
    put_string("REG_FRBASEADD", 10, 20);
    put_string("REG_SOFMOD", 10, 21);
    put_string("REG_PORTSC1", 10, 22);
    put_string("REG_PORTSC2", 10, 23);
    put_hex_32(port_in_u16(io_address + REG_CMD), 30, 16);
    put_hex_32(port_in_u16(io_address + REG_STS), 30, 17);
    put_hex_32(port_in_u16(io_address + REG_INTR), 30, 18);
    put_hex_32(port_in_u16(io_address + REG_FRNUM), 30, 19);
    put_hex_32(port_in_u32(io_address + REG_FRBASEADD), 30, 20);
    put_hex_32(port_in_u8(io_address + REG_SOFMOD), 30, 21);
    put_hex_32(port_in_u16(io_address + REG_PORTSC1), 30, 22);
    put_hex_32(port_in_u16(io_address + REG_PORTSC2), 30, 23);
}
void UhciInit() {
    uint32_t device_address =
        pci_find_next_device(UHCI_CONTROLLER_ADDRESS, UHCI_CONTROLLER_MASK, 0);
    uint16_t io_address =
        pci_read_config_u32(device_address, BAR4_OFFSET) & ~0x3; // Add better function in pci
    // // Controller initialization
    // UhciController *hc = VMAlloc(sizeof(UhciController));
    // hc->io_address = io_address;
    // hc->frame_list = VMAlloc(1024 * sizeof(uint32_t));
    // hc->qh_pool = (UhciQH *)VMAlloc(sizeof(UhciQH) * MAX_QH);
    // hc->td_pool = (UhciTD *)VMAlloc(sizeof(UhciTD) * MAX_TD);

    // memset(hc->qh_pool, 0, sizeof(UhciQH) * MAX_QH);
    // memset(hc->td_pool, 0, sizeof(UhciTD) * MAX_TD);

    // // Frame list setup
    // UhciQH *qh = UhciAllocQH(hc);
    // qh->head = TD_PTR_TERMINATE;
    // qh->element = TD_PTR_TERMINATE;
    // // qh->transfer = 0;
    // // qh->qhLink.prev = &qh->qhLink;
    // // qh->qhLink.next = &qh->qhLink;

    // hc->asyncQH = qh;
    // for (uint32_t i = 0; i < 1024; ++i)
    // {
    //     hc->frame_list[i] = TD_PTR_QH | (uint32_t)(uintptr_t)qh;
    // }

    // Disable interrupts
    port_out_u16(io_address + REG_INTR, 0);

    // // Assign frame list
    // port_out_u16(hc->io_address + REG_FRNUM, 0);
    // port_out_u32(hc->io_address + REG_FRBASEADD, (uint32_t)(uintptr_t)hc->frame_list);
    // port_out_u16(hc->io_address + REG_SOFMOD, 0x40);

    // // Clear status
    // port_out_u16(hc->io_address + REG_STS, 0xffff);

    // Start controller
    port_out_u16(io_address + REG_CMD, CMD_RUN);

    // put_hex_32(hc->frame_list);

    // uint32_t frame_list_base_address = port_in_u32(io_address+REG_FRBASEADD);
    // uint32_t frame_list_entry = port_in_u32(frame_list_base_address);

    // put_hex_64(frame_list_base_address, 10, 13);
    // put_hex_64(io_address+REG_FRBASEADD, 10, 14);

    while (1) {
        print_io_reg(io_address);
    }
}

/*
TODO:
1. Fetch and decode Transfer Descriptor
2. USB transaction
3. Update data structures

Executing TD:
1. Fetch TD
2. Build Token
3. Issue request for data
4.
*/

/*
https://docs.freebsd.org/en/books/arch-handbook/usb-hc.html
TODO:
Fetch pointer for current frame from the framelist
Execute TDs for all packets in that frame (last one will be a QH)
QHs for the individual interrupt transfers
QH for all control transfers
Interrupt-On Completion bit is set by the HC driver (if complete)

*/