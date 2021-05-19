#include <stdint.h>
#include <stdbool.h>
#include "ahci.h"
#include "ahci/fis.h"
#include "ahci/hba.h"
#include "pci.h"
#include "memory/frame_allocator.h"
#include "memory.h"
#include "kassert.h"
#include "rendering.h"
#include "memory/paging.h"
#include "util.h"
#include <stddef.h>
#include "memory/defs.h"

#define CMD_TABLE_SIZE (sizeof(CmdTable) + PRDT_LEN * sizeof(PRDTEntry))

uint8_t g_max_ports;
uint8_t g_n_ports;

AHCIMemory* g_ahci_device;

CmdHeader* g_virt_cmd_list_base[32]; // port_no as idx
uint8_t* g_virt_fis_base[32];
CmdTable* g_virt_cmd_table_base[32][32]; // port_no, cmd_slot

AHCIPort* get_port(uint8_t port_no) {
    return (AHCIPort*)((uint8_t*)g_ahci_device + sizeof(AHCIMemory) + sizeof(AHCIPort) * port_no);
}

CmdHeader* get_cmd_header(uint8_t port_no, uint8_t cmd_slot) {
    return g_virt_cmd_list_base[port_no] + cmd_slot;
}

CmdTable* get_cmd_table(uint8_t port_no, uint8_t cmd_slot) {
    return g_virt_cmd_table_base[port_no][cmd_slot];
}

PRDTEntry* get_prdt_entry(uint8_t port_no, uint8_t cmd_slot, uint16_t prdt_entry) {
    CmdTable* cmd_table = get_cmd_table(port_no, cmd_slot);
    return (PRDTEntry*)((void*)cmd_table + sizeof(CmdTable) + prdt_entry * sizeof(PRDTEntry));
} // seems to work now

// Port Controll function
// we sould probalby replace these later, maybe with bit fields... idk
#define HBA_PxCMD_ST 0x0001
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_FR 0x4000
#define HBA_PxCMD_CR 0x8000

// OBS BLATENT THEFT FROM OSDEV
void start_cmd(uint8_t port_no) {
    AHCIPort* port = get_port(port_no);
    // Wait until CR (bit15) is cleared
    while (port->cmd & HBA_PxCMD_CR)
        ;

    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

// OBS BLATENT THEFT FROM OSDEV
void stop_cmd(uint8_t port_no) {
    AHCIPort* port = get_port(port_no);
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
    AHCIPort* port = get_port(port_no);
    for (uint32_t spin = 0; spin < 10000000; spin++) {
        if ((port->task_file_data & (0x88)) == 0) {
            return true;
        }
    }
    return false; // Port is hung!
}

bool wait_for_done(uint8_t port_no, uint8_t cmd_slot) {
    AHCIPort* port = get_port(port_no);
    while (1) {
        if ((port->cmd_issue & (1 << cmd_slot)) == 0) {
            break;
        }
        if (port->interrupt_status & (1 << 30)) {
            return false;
        }
    }
    if (port->interrupt_status & (1 << 30)) {
        KERNEL_ASSERT(false, "Read disk error")
        return false;
    }
    return true;
}

// Setup functions
void* alloc_port_memory(uint8_t max_ports) {
    KERNEL_ASSERT((PRDT_LEN * 16) % 128 == 0, "prdt dose not align properly, use multiples of 8")
    uint64_t byte_count = (1024 + 256 + CMD_TABLE_SIZE * 32) * max_ports;
    return alloc_pages_contiguous(round_up_to_multiple(byte_count, 4096) / 4096,
                                  PAGING_WRITABLE | PAGING_CACHE_DISABLE);
}

bool setup_ports(uint8_t max_ports) {
    // alloc memory
    void* base = alloc_port_memory(max_ports);
    for (uint8_t port_no = 0; port_no < max_ports; port_no++) {
        stop_cmd(port_no);
        AHCIPort* port = get_port(port_no);
        g_virt_cmd_list_base[port_no] = base + 1024 * port_no;
        kvirt_to_phys_addr((VirtualAddress)g_virt_cmd_list_base[port_no],
                           (volatile PhysicalAddress*)&port->cmd_list_base);
        g_virt_fis_base[port_no] = base + 1024 * max_ports + 256 * port_no;
        kvirt_to_phys_addr((VirtualAddress)g_virt_fis_base[port_no],
                           (volatile PhysicalAddress*)&port->fis_base);
        for (uint8_t cmd_slot = 0; cmd_slot < 32; cmd_slot++) {
            g_virt_cmd_table_base[port_no][cmd_slot] =
                base + ((1024 + 256) * max_ports) + (port_no * 32 + cmd_slot) * CMD_TABLE_SIZE;
            kvirt_to_phys_addr((VirtualAddress)g_virt_cmd_table_base[port_no][cmd_slot],
                               &(get_cmd_header(port_no, cmd_slot)->cmd_table_base));
        }
        start_cmd(port_no);
    }
    return true;
}

bool setup_ahci() {
    // setup with abar
    PCIConfigSpace0* ahci_config = get_pci_device(0x01060000, PCI_DEVICE_SUBCLASS_MASK);
    KERNEL_ASSERT(ahci_config, "Ahci device not found!")
    PhysicalAddress abar = ahci_config->BAR5;
    ahci_config->BAR5 = 0xFFFFFFFF;
    uint32_t abar_size = ~(ahci_config->BAR5 & 0xFFFFFFF0ULL) + 1;
    ahci_config->BAR5 = abar;
    g_ahci_device =
        (AHCIMemory*)kmap_phys_range(abar & 0xFFFFFFF0ULL,
                                     round_up_to_multiple(abar_size, PAGE_SIZE) / PAGE_SIZE,
                                     PAGING_WRITABLE | PAGING_CACHE_DISABLE);
    // HBA controller reset (bit 0)
    g_ahci_device->global_ctrl |= 1;
    while (g_ahci_device->global_ctrl & 1)
        ;
    // set global ctrl bit 31
    g_ahci_device->global_ctrl |= 1 << 31;
    // max ports
    uint32_t max_ports = (g_ahci_device->capabilities & 0x1f) + 1;
    // setup ports
    setup_ports(max_ports);
    return true;
}

int8_t free_cmd_slot(AHCIPort* port) {
    uint32_t slots = (port->sata_active | port->cmd_issue);
    for (uint8_t cmd_slot = 0; cmd_slot < 32; cmd_slot++) {
        if ((slots & 1) == 0) {
            return cmd_slot;
        }
        slots >>= 1;
    }

    return -1;
}

bool read_to_buffer(uint8_t port_no, uint64_t start, uint32_t count, VirtualAddress vbuffer) {
    PhysicalAddress buffer;
    bool flag = kvirt_to_phys_addr(vbuffer, &buffer);
    KERNEL_ASSERT(flag, "Ahci could not get physical adress for buffer.")
    AHCIPort* port = get_port(port_no);

    int8_t cmd_slot = free_cmd_slot(port);
    KERNEL_ASSERT(cmd_slot != -1, "No free cmd_slot could be found.");
    // TODO Retry x amount of times before failing.

    CmdHeader* cmd_header = get_cmd_header(port_no, cmd_slot);
    cmd_header->cmd_fis_len = sizeof(FISRegH2D) / 4; // len in dwords
    cmd_header->write = 0;

    CmdTable* cmd_table = get_cmd_table(port_no, cmd_slot);

    FISRegH2D* cmd_fis = (FISRegH2D*)cmd_table;
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->c = 1;
    cmd_fis->cmd = ATA_COMMAND_READ_DMA_EXT;

    cmd_fis->lba0 = (uint8_t)start;
    cmd_fis->lba1 = (uint8_t)(start >> 8);
    cmd_fis->lba2 = (uint8_t)(start >> 16);
    cmd_fis->lba3 = (uint8_t)(start >> 24);
    cmd_fis->lba4 = (uint8_t)(start >> 32);
    cmd_fis->lba5 = (uint8_t)(start >> 40);
    cmd_fis->device_reg = 1 << 6; // LBA mode bit

    // OSdev does this, why?
    cmd_fis->countl = count & 0xFF;
    cmd_fis->counth = (count >> 8) & 0xFF;

    // MEMSET?
    PRDTEntry* prdt_entry = get_prdt_entry(port_no, cmd_slot, 0);
    while (1) {
        prdt_entry->data_base = buffer;
        prdt_entry->byte_count = MIN(16u, count) * 512 - 1;
        prdt_entry->interrupt = 1;
        buffer += prdt_entry->byte_count;
        if (count <= 16) {
            break;
        }
        count -= 16;
        prdt_entry++;
    }

    bool port_is_free = wait_for_port(port_no);
    KERNEL_ASSERT(port_is_free, "Ahci, port is hung!")

    port->cmd_issue |= 1 << cmd_slot;

    wait_for_done(port_no, cmd_slot); // should probs be an error code.
    return true;
}
