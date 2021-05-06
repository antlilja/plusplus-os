#include <stdint.h>
#include <stdbool.h>
#include "ahci.h"
#include "ahci/fis.h"
#include "ahci/hba.h"
#include "pci.h"
#include "memory.h"
#include "kassert.h"

FISRegH2D fis_construct_h2d() {}

AHCIMemory* g_ahci = 0;

uint32_t g_n_ports = 0;

#include "rendering.h"
// #include "debug_log.h" // :))

// MEMORY ALLOC!
// cmd list: 1k * ports
// fis resp: 256 * ports
// cmd tble size: 128 + 16 * prdt_len // OBS MUST BE 128 aligned!!
// cmd tbles: ports * (128 + 16 * prdt_len)
// total = (1k + 256 + 128 + 16 * prdt_len) * ports

// offsets
// cmd list: base + 1k * port_no
// fis resp: base + 1k * port_cnt + 256 * port_no
// cmd tble: base + 1k * port_cnt + 256 * port_cnt + port_no * tabl_size
// prdt entry: base + 1k * port_cnt + 256 * port_cnt + port_no * tabl_size + entry_no * 16

uint64_t g_base;

bool alloc_ahci_memory(uint8_t n_ports, uint16_t prdt_len) {
    KERNEL_ASSERT((prdt_len * 16) % 128 == 0, "prdt dose not align properly, use multiples of 8")
    uint64_t byte_count = (1024 + 256 + 128 + 16 * prdt_len) * n_ports;
    uint8_t order = 0;
    while ((1 << order) * 4096 < byte_count) {
        order++;
    }
    return alloc_frames_contiguos(order, g_base);
}

#define HBA_PxCMD_ST 0x0001
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_FR 0x4000
#define HBA_PxCMD_CR 0x8000

// Start command  // OBS BLATENT THEFT
void start_cmd(AHCIPort* port) {
    // Wait until CR (bit15) is cleared
    while (port->cmd & HBA_PxCMD_CR)
        ;

    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

// Stop command engine // OBS BLATENT THEFT
void stop_cmd(AHCIPort* port) {
    // Clear ST (bit0)
    port->cmd &= ~HBA_PxCMD_ST;

    // Clear FRE (bit4)
    port->cmd &= ~HBA_PxCMD_FRE;

    // Wait until FR (bit14), CR (bit15) are cleared
    while (1) {
        if (port->cmd & HBA_PxCMD_FR) continue;
        if (port->cmd & HBA_PxCMD_CR) continue;
        break;
    }
}

bool wait_for_port(uint8_t port_no) {
    AHCIPort* port = (AHCIPort*)&g_ahci->port[port_no];
    uint32_t spin = 0;
    while (
        (port->task_file_data & (0x88)) && // 0x88 is the number of device bussy and device drq (??)
        spin < 1000000) {                  // arbitrary high number
        spin++;
    }
    if (spin == 1000000) {
        KERNEL_ASSERT(false, "Port is hung");
        return false;
    }
    return true;
}

bool setup_ahci() {
    uint32_t ahci_pci_addr = pci_find_next_device(0x01060000, DEVICE_SUBCLASS_MASK, 0);
    if (!ahci_pci_addr) {
        return false;
    }
    g_ahci = (AHCIMemory*)pci_read_config_u32(ahci_pci_addr, 0x24);

    uint32_t max_ports = (g_ahci->capabilities & 0x1f) + 1;
    // uint32_t cmd_slots = ((g_ahci->capabilities >> 8) & 0x1f) + 1;

    uint64_t base = alloc_ahci_memory(max_ports, PDRT_LEN);

    // setup ports
    for (uint32_t port_no = 0; port_no < max_ports; port_no++) {
        AHCIPort* port = &(g_ahci->port[port_no]);
        stop_cmd(port);
        port->cmd_list_base = base + 1024 * port_no;
        port->fis_base = base + 1024 * max_ports + 256 * port_no;
        ((CmdHeader*)port->cmd_list_base)->cmd_table_base =
            base + 1024 * max_ports + 256 * max_ports + port_no * PDRT_LEN;
        start_cmd(port);
    }
    return true;
}

int8_t free_cmd_slot(uint8_t port_no) { return 0; } // placeholder, duh!

bool read_to_buffer(uint8_t port_no, uint64_t start, uint32_t count, uint16_t* buffer) {
    // osd: clear interupt bit
    // osd: find cmd slot
    int8_t cmd_slot = free_cmd_slot(port_no);
    // osd: build header
    CmdHeader* cmd_header = (CmdHeader*)g_ahci->port[port_no].cmd_list_base + cmd_slot;
    cmd_header->cmd_fis_len = sizeof(FISRegH2D) / sizeof(uint32_t);
    cmd_header->write = 0;
    // mem set ?
    CmdTable* cmd_table = cmd_header->cmd_table_base;
    uint32_t i = 0;
    while (count > 16) {
        cmd_table->prdt_entry[i].data_base = buffer;
        cmd_table->prdt_entry[i].byte_count = 8 * 1024 - 1; // Requires "-1" - osdev
        cmd_table->prdt_entry[i].interrupt = 1;
        buffer += 4 * 1024; // words not bytes
        count -= 16;        // 16 sectors
        i++;
    }
    cmd_table->prdt_entry[i].data_base = buffer;
    cmd_table->prdt_entry[i].byte_count = count * 512 - 1; // Requires "-1" - osdev
    cmd_table->prdt_entry[i].interrupt = 1;

    FISRegH2D* cmd_fis = (FISRegH2D*)(&cmd_table->cmd_fis);
    cmd_fis->lba0 = (uint8_t)start;
    cmd_fis->lba1 = (uint8_t)(start >> 8);
    cmd_fis->lba2 = (uint8_t)(start >> 16);
    cmd_fis->lba3 = (uint8_t)(start >> 24);
    cmd_fis->lba4 = (uint8_t)(start >> 32);
    cmd_fis->lba5 = (uint8_t)(start >> 40);
    cmd_fis->device_reg = 1 << 6; // LBA mode

    // not sure why... or how...
    cmd_fis->countl = count & 0xFF;
    cmd_fis->counth = (count >> 8) & 0xFF;

    bool port_is_free = wait_for_port(port_no);
    KERNEL_ASSERT(port_is_free, "Ahci port is hung!"); // remove later

    // some more stuff!
}
