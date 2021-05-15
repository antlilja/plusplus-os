#include "pci.h"

#include "kassert.h"
#include "acpi.h"
#include "memory.h"
#include "memory/paging.h"

#include <string.h>

typedef struct {
    ACPISDTHeader header;
    uint8_t reserved[8];
} __attribute__((packed)) MCFG;

typedef struct {
    PhysicalAddress base_address;
    uint16_t segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
    uint8_t reserved[4];
} __attribute__((packed)) MCFGEntry;

typedef struct {
    void* next;
    PCIConfigSpace0* config_space;
    union {
        struct {
            uint8_t revision_ID;
            uint8_t prog_IF;
            uint8_t subclass;
            uint8_t class_code;
        } __attribute__((packed));
        uint32_t type;
    };
} __attribute__((packed)) PCIDeviceEntry;

PCIDeviceEntry* g_pci_device_list = 0;

PCIConfigSpace0* get_pci_device(uint32_t type, uint32_t mask) {
    PCIDeviceEntry* device_entry = g_pci_device_list;
    while (device_entry != 0) {
        if ((device_entry->type & mask) == type) return device_entry->config_space;

        device_entry = (PCIDeviceEntry*)device_entry->next;
    }

    return 0;
}

void enumerate_pci_devices() {
    const MCFG* mcfg = (const MCFG*)find_table("MCFG");
    KERNEL_ASSERT(mcfg, "MCFG TABLE NOT FOUND");

    const VirtualAddress mcfg_addr = (VirtualAddress)mcfg;
    const MCFGEntry* mcfg_arr = (MCFGEntry*)(mcfg_addr + sizeof(MCFG));
    const uint64_t entries = (mcfg->header.length - sizeof(MCFG)) / sizeof(MCFGEntry);

    for (uint64_t i = 0; i < entries; ++i) {
        const PhysicalAddress base_phys_addr = mcfg_arr[i].base_address;
        for (uint16_t bus = mcfg_arr[i].start_bus; bus < mcfg_arr[i].end_bus; ++bus) {
            for (uint8_t device = 0; device < 32; ++device) {
                for (uint8_t func = 0; func < 8; ++func) {

                    PhysicalAddress config_phys_addr =
                        base_phys_addr + ((bus << 20) | (device << 15) | (func << 12));

                    PCIConfigSpace0* config_space = (PCIConfigSpace0*)kmap_phys_range(
                        config_phys_addr, 1, PAGING_WRITABLE | PAGING_CACHE_DISABLE);

                    const bool exists = config_space->vendor_id != 0xffff;
                    const bool is_multi_func = (config_space->header_type & 0x80) != 0;

                    if (func > 0 && !is_multi_func) {
                        kunmap_range((VirtualAddress)config_space, 1);
                        break;
                    }

                    if (!exists) {
                        kunmap_range((VirtualAddress)config_space, 1);
                        continue;
                    }

                    PCIDeviceEntry* device_entry = kalloc(sizeof(PCIDeviceEntry));
                    device_entry->revision_ID = config_space->revision_ID;
                    device_entry->prog_IF = config_space->prog_IF;
                    device_entry->subclass = config_space->subclass;
                    device_entry->class_code = config_space->class_code;
                    device_entry->config_space = config_space;

                    device_entry->next = (void*)g_pci_device_list;
                    g_pci_device_list = device_entry;
                }
            }
        }
    }
}
