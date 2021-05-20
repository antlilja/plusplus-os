#include "ahci.h"

#include "ahci/hba.h"
#include "ahci/fis.h"

#include "kassert.h"
#include "pci.h"
#include "util.h"
#include "memory.h"
#include "memory/paging.h"

#include <string.h>

#define PRDT_LEN 8

#define CMD_TABLE_SIZE ((((128 + PRDT_LEN * 16) + 128 - 1) / 128) * 128)

typedef struct {
    uint8_t port;
    CMDHeader* cmd_header;
    void* fis_base;
    CMDTable* cmd_tables;
} Device;

struct {
    AHCIController* controller;

    uint8_t port_count;
    uint8_t cmd_slots;

    Device* devices;
    uint8_t device_count;
} g_ahci = {0};

void start_cmd(AHCIPort* port) {
    while (port->cmd & PORT_CMD_CR)
        ;

    port->cmd |= PORT_CMD_FRE;
    port->cmd |= PORT_CMD_ST;
}

void stop_cmd(AHCIPort* port) {
    port->cmd &= ~PORT_CMD_ST;
    port->cmd &= ~PORT_CMD_FRE;

    while (true) {
        if (port->cmd & PORT_CMD_FR) continue;
        if (port->cmd & PORT_CMD_CR) continue;

        break;
    }
}

void initialize_ahci() {
    // Setup PCI config space
    {
        PCIConfigSpace0* config_space = get_pci_device(0x01060000, PCI_DEVICE_SUBCLASS_MASK);
        KERNEL_ASSERT(config_space != 0, "AHCI controller not found")

        const uint32_t abar = config_space->BAR5;

        const PhysicalAddress controller_phys_addr = config_space->BAR5 & 0xfffffff0;

        const uint32_t abar_pages = ({
            // Set all abar bits to ones
            config_space->BAR5 = 0xffffffff;

            // Calculate size
            uint32_t size = ~(config_space->BAR5 & 0xfffffff0) + 1;

            // Restore abar
            config_space->BAR5 = abar;

            (size + PAGE_SIZE - 1) / PAGE_SIZE;
        });

        g_ahci.controller = (AHCIController*)kmap_phys_range(
            controller_phys_addr, abar_pages, PAGING_WRITABLE | PAGING_CACHE_DISABLE);

        KERNEL_ASSERT(g_ahci.controller->capabilities & (1 << 31),
                      "AHCI controller does not support 64-bit addressing")
    }

    g_ahci.port_count = (g_ahci.controller->capabilities & 0x1f) + 1;
    g_ahci.cmd_slots = ((g_ahci.controller->capabilities >> 8) & 0x1f) + 1;

    g_ahci.devices = kalloc(sizeof(Device) * g_ahci.port_count);

    const uint32_t port_impl = g_ahci.controller->port_impl;
    for (uint32_t p = 0; p < g_ahci.port_count; ++p) {
        if ((port_impl & (1 << p)) == 0) continue;

        AHCIPort* port = &g_ahci.controller->ports[p];

        // Make sure drive is present on port
        {
            const uint32_t sata_status = port->sata_status;

            if ((sata_status & 0b111) != PORT_DET_PRESENT) continue;
            if (((sata_status >> 8) & 0b111) != PORT_IPM_ACTIVE) continue;
        }

        // Make sure port has an ATA drive on it
        if (port->signature != SATA_SIG_ATA) continue;

        stop_cmd(port);

        Device* device = &g_ahci.devices[g_ahci.device_count++];
        device->port = p;

        const uint64_t cmd_headers_size = round_up_to_multiple(32 * g_ahci.cmd_slots, 256);
        const uint64_t fis_size = 256;
        const uint64_t cmd_tables_size = g_ahci.cmd_slots * CMD_TABLE_SIZE;

        void* dma_ptr = ({
            const uint64_t size = cmd_headers_size + fis_size + cmd_tables_size;
            const uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

            alloc_pages_contiguous(pages, PAGING_WRITABLE);
        });
        KERNEL_ASSERT(dma_ptr, "Failed to allocate memory")

        // Setup command headers
        {
            PhysicalAddress phys_addr;
            const bool success = kvirt_to_phys_addr((VirtualAddress)dma_ptr, &phys_addr);
            KERNEL_ASSERT(success, "Failed to get physical address")

            port->cmd_list_base = phys_addr;

            memset(dma_ptr, 0, 1024);
            device->cmd_header = (CMDHeader*)dma_ptr;
            dma_ptr += 1024;
        }

        // Setup memory for FIS
        {
            PhysicalAddress phys_addr;
            const bool success = kvirt_to_phys_addr((VirtualAddress)dma_ptr, &phys_addr);
            KERNEL_ASSERT(success, "Failed to get physical address")
            port->fis_base = phys_addr;

            memset(dma_ptr, 0, 256);
            device->fis_base = dma_ptr;
            dma_ptr += 256;
        }

        // Setup command tables
        CMDHeader* cmd_header = device->cmd_header;
        device->cmd_tables = (CMDTable*)dma_ptr;
        for (uint32_t i = 0; i < g_ahci.cmd_slots; ++i) {
            cmd_header[i].prdt_len = PRDT_LEN;

            PhysicalAddress phys_addr;
            const bool success = kvirt_to_phys_addr((VirtualAddress)dma_ptr, &phys_addr);
            KERNEL_ASSERT(success, "Failed to get physical address")
            KERNEL_ASSERT((phys_addr & 0b111111) == 0, "Not aligned")
            cmd_header[i].cmd_table_base = phys_addr;

            memset(dma_ptr, 0, CMD_TABLE_SIZE);
            dma_ptr += CMD_TABLE_SIZE;
        }

        start_cmd(port);
    }
}
