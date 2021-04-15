#include "memory.h"

#include "uefi.h"
#include "kassert.h"

#include <stdbool.h>
#include <string.h>

#define PDP_MEM_RANGE 0x8000000000ULL

#define KERNEL_PML4_OFFSET 480ULL
#define KERNEL_OFFSET (KERNEL_PML4_OFFSET * PDP_MEM_RANGE + PAGE_SIZE)

#define PAGING_PTRMASK 0x7FFFFFFFFF000

#define STACK_PAGE_COUNT 4

PageEntry __attribute__((aligned(0x1000))) g_pml4[512];

extern char stage2_kernel_entry;

// Rounds n up to nearest multiple of m
uint64_t round_up(uint64_t n, uint64_t m) { return ((n + m - 1) / m) * m; }

// Sign extend virtual address
VirtualAddress sign_ext_addr(VirtualAddress addr) {
    return addr | (((addr >> 47) & 1) * 0xffffULL) << 48;
}

_Noreturn void remap_kernel_setup_stack(void* uefi_memory_map) {
    UEFIMemoryMap* memory_map = (UEFIMemoryMap*)uefi_memory_map;

    // Get kernel physical address and page count
    PhysicalAddress kernel_addr;
    uint64_t kernel_pages;
    {
        bool found = false;
        for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
            UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];
            if (desc->type == EfiLoaderCode) {
                KERNEL_ASSERT(!found, "More than one kernel?")
                kernel_addr = desc->physical_start;
                kernel_pages = desc->num_pages;
                found = true;
            }
        }
    }

    // Get page frames for page tables and stack
    PhysicalAddress pdp_phys_addr;
    PhysicalAddress pd_phys_addr;
    PhysicalAddress pt_phys_addr;
    PhysicalAddress stack_phys_addr;
    {
        struct {
            uint8_t paging : 1;
            uint8_t stack : 1;
        } found = {0};
        for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
            UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];

            const bool correct_type = desc->type == EfiConventionalMemory ||
                                      desc->type == EfiRuntimeServicesCode ||
                                      desc->type == EfiBootServicesCode;

            // Skip memory before 1MB which can be used to start up threads later
            const bool after_1M = desc->physical_start > 0x400000;

            if (!correct_type || !after_1M) continue;

            if (!found.paging && desc->num_pages >= 4) {
                KERNEL_ASSERT((desc->physical_start & 0xfff) == 0, "Not page aligned")
                pdp_phys_addr = (desc->physical_start + 0 * PAGE_SIZE);
                pd_phys_addr = (desc->physical_start + 1 * PAGE_SIZE);
                pt_phys_addr = (desc->physical_start + 2 * PAGE_SIZE);

                // Shrink range in memory map
                desc->num_pages -= 3;
                desc->physical_start += (3 * PAGE_SIZE);
                found.paging = true;
            }

            if (!found.stack && desc->num_pages >= 1) {
                KERNEL_ASSERT((desc->physical_start & 0xfff) == 0, "Not page aligned")
                stack_phys_addr = desc->physical_start;

                desc->num_pages -= STACK_PAGE_COUNT;
                desc->physical_start += (STACK_PAGE_COUNT * PAGE_SIZE);
                found.stack = true;
            }

            if (found.paging && found.stack) break;
        }

        KERNEL_ASSERT(found.paging && found.stack, "Out of memory");
    }

    PageEntry* pdp = (PageEntry*)pdp_phys_addr;
    PageEntry* pd = (PageEntry*)pd_phys_addr;
    PageEntry* pt = (PageEntry*)pt_phys_addr;

    // Zero out pages used for page table
    for (int i = 0; i < 512; ++i) {
        g_pml4[i].value = 0;
        pdp[i].value = 0;
        pd[i].value = 0;
        pt[i].value = 0;
    }

    // Keep old mapping from UEFI
    {
        PageEntry* boot_pml4;
        asm("mov %%cr3, %[pml4]" : [pml4] "=r"(boot_pml4));
        for (uint64_t i = 0; i < 512; ++i) {
            if (boot_pml4[i].present) {
                g_pml4[i].value = boot_pml4[i].value;
            }
        }
    }

    // Populate upper page table entries
    {
        g_pml4[KERNEL_PML4_OFFSET].phys_address = (pdp_phys_addr & PAGING_PTRMASK) >> 12;
        g_pml4[KERNEL_PML4_OFFSET].present = true;
        g_pml4[KERNEL_PML4_OFFSET].write = true;

        pdp[0].phys_address = (pd_phys_addr & PAGING_PTRMASK) >> 12;
        pdp[0].present = true;
        pdp[0].write = true;

        pd[0].phys_address = (pt_phys_addr & PAGING_PTRMASK) >> 12;
        pd[0].present = true;
        pd[0].write = true;
    }

    // Map kernel and stack into virtual memory
    {
        uint64_t pt_offset = 1;

        for (uint64_t i = 0; i < kernel_pages; ++i) {
            uint64_t addr = (kernel_addr + i * PAGE_SIZE) & PAGING_PTRMASK;
            pt[i + pt_offset].phys_address = addr >> 12;
            pt[i + pt_offset].present = true;
            pt[i + pt_offset].write = true;
        }
        pt_offset += kernel_pages;

        for (uint64_t i = 0; i < STACK_PAGE_COUNT; ++i) {
            uint64_t addr = (stack_phys_addr + i * PAGE_SIZE) & PAGING_PTRMASK;
            pt[i + pt_offset].phys_address = addr >> 12;
            pt[i + pt_offset].present = true;
            pt[i + pt_offset].write = true;
        }
    }

    // Switch to new page table
    asm("mov %[pml4], %%cr3" : : [pml4] "r"(g_pml4) : "cr3");

    VirtualAddress kernel_virt_addr = sign_ext_addr(KERNEL_OFFSET);
    uint64_t stage2_offset = ((uint64_t)&stage2_kernel_entry) - kernel_addr;

    VirtualAddress stage2_addr = kernel_virt_addr + stage2_offset;

    VirtualAddress stack_virt_addr = sign_ext_addr(KERNEL_OFFSET + PAGE_SIZE * kernel_pages);

    asm(
        // Setup new stack
        "mov %[stack], %%rsp\n"
        "mov %[stack], %%rbp\n"

        // Do a long ret to stage2 kernel entry
        "movw %%cs, %%ax\n"
        "push %%rax\n"
        "push %[stage2]\n"
        "lretq\n"
        :
        : [stage2] "r"(stage2_addr), [stack] "r"(stack_virt_addr)
        : "rsp", "rbp");

    // This point will never be reached
    while (1)
        ;
}
