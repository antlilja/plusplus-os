#include "pci.h"

#include "kassert.h"
#include "acpi.h"
#include "memory/paging.h"
#include "memory/entry_pool.h"

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
    VirtualAddress next : 48;
    PhysicalAddress config_phys_addr : 38;
    union {
        struct {
            uint8_t revision_ID;
            uint8_t prog_IF;
            uint8_t subclass;
            uint8_t class_code;
        } __attribute__((packed));
        uint32_t type;
    };
    uint8_t padding;
} __attribute__((packed)) PCIDeviceEntry;

PCIDeviceEntry* g_pci_device_list = 0;

bool get_pci_device(PhysicalAddress* config_space_phys_addr, uint32_t type, uint32_t mask) {
    PCIDeviceEntry* device_entry = g_pci_device_list;
    while (device_entry != 0) {
        if ((device_entry->type & mask) == type) {
            *config_space_phys_addr = device_entry->config_phys_addr << 12;
            return true;
        }
        device_entry = (PCIDeviceEntry*)SIGN_EXT_ADDR(device_entry->next);
    }
    return false;
}

void enumerate_pci_devices() {
    _Static_assert(sizeof(PCIDeviceEntry) == 16, "PCIDeviceEntry not 16 bytes");

    const MCFG* mcfg = ({
        void* table_ptr;
        const bool success = find_table("MCFG", &table_ptr);
        KERNEL_ASSERT(success, "MCFG could not be found")

        (const MCFG*)table_ptr;
    });

    const PhysicalAddress mcfg_addr = (PhysicalAddress)mcfg;
    const MCFGEntry* mcfg_arr = (MCFGEntry*)(mcfg_addr + sizeof(MCFG));
    const uint64_t entries = (mcfg->header.length - sizeof(MCFG)) / sizeof(MCFGEntry);

    for (uint64_t i = 0; i < entries; ++i) {
        const PhysicalAddress base_phys_addr = mcfg_arr[i].base_address;
        for (uint16_t bus = mcfg_arr[i].start_bus; bus < mcfg_arr[i].end_bus; ++bus) {
            for (uint8_t device = 0; device < 32; ++device) {
                for (uint8_t func = 0; func < 8; ++func) {
                    const PCIConfigSpace0* config_space =
                        (const PCIConfigSpace0*)(base_phys_addr +
                                                 (bus << 20 | device << 15 | func << 12));
                    const bool exists = config_space->vendor_id != 0xffff;

                    const bool is_multi_func = (config_space->header_type & 0x80) != 0;

                    if (func > 0 && !is_multi_func) break;

                    if (!exists) continue;

                    PCIDeviceEntry* device_entry = (PCIDeviceEntry*)get_memory_entry();
                    device_entry->revision_ID = config_space->revision_ID;
                    device_entry->prog_IF = config_space->prog_IF;
                    device_entry->subclass = config_space->subclass;
                    device_entry->class_code = config_space->class_code;
                    device_entry->config_phys_addr = ((PhysicalAddress)config_space) >> 12;

                    device_entry->next = (VirtualAddress)g_pci_device_list;
                    g_pci_device_list = device_entry;
                }
            }
        }
    }
}
