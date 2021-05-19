#include "memory/paging.h"

#include "kassert.h"
#include "uefi.h"
#include "util.h"

#include "memory/entry_pool.h"
#include "memory/frame_allocator.h"

#include <stdbool.h>
#include <string.h>

#define PAGE_POOL_THRESHOLD 10

#define PAGE_ENTRY_COUNT 512
#define PT_MEM_RANGE (PAGE_SIZE * PAGE_ENTRY_COUNT)
#define PD_MEM_RANGE (PT_MEM_RANGE * PAGE_ENTRY_COUNT)
#define PDP_MEM_RANGE (PD_MEM_RANGE * PAGE_ENTRY_COUNT)

#define KERNEL_PML4_OFFSET 480ULL
#define KERNEL_OFFSET (KERNEL_PML4_OFFSET * PDP_MEM_RANGE)

#define OFFSET_INDEX_MASK 0x1ffULL

#define PT 0
#define PD 1
#define PDP 2
#define PML4 3

#define GET_LEVEL_INDEX(addr, level) \
    (((addr) & (OFFSET_INDEX_MASK << (12 + 9 * (level)))) >> (12 + 9 * (level)))

typedef struct {
    uint16_t pd_index;
    PageEntry* pd;

    uint16_t pt_index;
    PageEntry* pt;
} PageTableLocation;

typedef MappingEntry PagePoolEntry;

PageEntry __attribute__((aligned(0x1000))) g_pml4[512] = {0};

bool g_paging_execute_disable = false;

AddressSpace g_kernel_space = {
    .current_address = KERNEL_OFFSET,
    .pdp_index = KERNEL_PML4_OFFSET,
    .pdp = 0,
    .prot = 0,
    .free_list = 0,
    .entry_maps = {0},
};

struct {
    uint64_t count;
    PagePoolEntry* head;
} g_page_pool = {0};

// See linker.ld
extern char s_kernel_rodata_start;
extern char s_kernel_rodata_end;
extern char s_kernel_data_start;
extern char s_kernel_data_end;

void new_address_space(AddressSpace* space, uint16_t pdp_index, uint8_t prot) {
    KERNEL_ASSERT(pdp_index < KERNEL_PML4_OFFSET,
                  "AddressSpace can't overlap with kernel address space")

    KERNEL_ASSERT(prot <= 0b1111, "Prot is only 4 bits page entries")
    space->prot = prot;

    space->pdp_index = pdp_index;
    space->current_address = pdp_index * PDP_MEM_RANGE;

    // Make sure virtual address zero is never used
    if (space->current_address == 0) space->current_address += PAGE_SIZE;

    // Allocate PDP
    space->pdp = (PageEntry*)alloc_pages_contiguous(1, PAGING_WRITABLE);
    KERNEL_ASSERT(space->pdp != 0, "Out of memory")

    memset((void*)space->pdp, 0, PAGE_SIZE);
}

void delete_address_space(AddressSpace* space) {
    // Free free list entries
    {
        FreeListEntry* free_entry = space->free_list;
        while (free_entry != 0) {
            MemoryEntry* mem_entry = (MemoryEntry*)free_entry;
            free_entry = (FreeListEntry*)SIGN_EXT_ADDR(free_entry->next);
            free_memory_entry(mem_entry);
        }
    }

    // Free page entries and mapping entries
    {
        for (uint8_t i = 0; i < 2; ++i) {
            MappingEntry* map_entry = space->entry_maps[i];
            while (map_entry != 0) {
                // Free page entry memory
                free_pages_contiguous((void*)SIGN_EXT_ADDR(map_entry->virt_addr << 12), 1);

                // Free memory entry
                MemoryEntry* mem_entry = (MemoryEntry*)map_entry;
                map_entry = (MappingEntry*)SIGN_EXT_ADDR(map_entry->next);
                free_memory_entry(mem_entry);
            }
        }
    }

    // Free PDP
    free_pages_contiguous(space->pdp, 1);
}

void map_address_space(AddressSpace* space) {
    {
        PhysicalAddress pdp_phys_addr;
        const bool success = kvirt_to_phys_addr((VirtualAddress)space->pdp, &pdp_phys_addr);
        KERNEL_ASSERT(success, "Could not find physical address")
        g_pml4[space->pdp_index].phys_addr = pdp_phys_addr >> 12;
    }

    g_pml4[space->pdp_index].present = true;
    g_pml4[space->pdp_index].write = true;
    g_pml4[space->pdp_index].prot = space->prot;
    g_pml4[space->pdp_index].user = space->prot == 3;

    // Invalidate TLB entries
    for (VirtualAddress virt_addr = space->pdp_index * PDP_MEM_RANGE;
         virt_addr < space->current_address;
         virt_addr += PAGE_SIZE) {
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");
    }
}

void unmap_address_space(AddressSpace* space) {
    g_pml4[space->pdp_index].value = 0;

    // Invalidate TLB entries
    for (VirtualAddress virt_addr = space->pdp_index * PDP_MEM_RANGE;
         virt_addr < space->current_address;
         virt_addr += PAGE_SIZE) {
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");
    }
}

PageEntry* get_page_entries(AddressSpace* space, const PageEntry* entry, uint8_t level) {
    PhysicalAddress phys_addr = entry->phys_addr;
    MappingEntry* mapping = space->entry_maps[level];
    while (mapping != 0) {
        if (mapping->phys_addr == phys_addr) {
            return (PageEntry*)SIGN_EXT_ADDR(mapping->virt_addr << 12);
        }
        mapping = (MappingEntry*)SIGN_EXT_ADDR(mapping->next);
    }
    return 0;
}

PageEntry* get_or_alloc_page_entries(AddressSpace* space, PageEntry* entry, uint8_t level) {
    if (entry->present) {
        return get_page_entries(space, entry, level);
    }
    else {
        if (space == &g_kernel_space) {
            if (g_page_pool.count <= PAGE_POOL_THRESHOLD) {
                // Adjust pool count beforehand to avoid getting stuck in an infinite loop
                g_page_pool.count += PAGE_POOL_THRESHOLD;

                PageFrameAllocation* allocation = alloc_frames(PAGE_POOL_THRESHOLD);
                KERNEL_ASSERT(allocation != 0, "Out of memory")

                VirtualAddress virt_addr = kmap_allocation(allocation, PAGING_WRITABLE);

                // Populate pool with allocated pages
                for (uint64_t i = 0; i < PAGE_POOL_THRESHOLD; ++i) {
                    PagePoolEntry* entry = (PagePoolEntry*)get_memory_entry();
                    entry->next = (VirtualAddress)g_page_pool.head;
                    g_page_pool.head = entry;
                    virt_addr += PAGE_SIZE;
                }

                free_frame_allocation_entries(allocation);
            }

            PagePoolEntry* pool_entry = g_page_pool.head;
            g_page_pool.head = (PagePoolEntry*)SIGN_EXT_ADDR(pool_entry->next);
            --g_page_pool.count;

            pool_entry->next = (VirtualAddress)space->entry_maps[level];
            space->entry_maps[level] = pool_entry;

            entry->phys_addr = pool_entry->phys_addr;
            entry->present = true;
            entry->write = true;

            memset((void*)SIGN_EXT_ADDR(pool_entry->virt_addr << 12), 0, PAGE_SIZE);

            PageEntry* entries = (PageEntry*)SIGN_EXT_ADDR(pool_entry->virt_addr << 12);
            return entries;
        }
        else {
            PhysicalAddress phys_addr;
            const bool success = alloc_frames_contiguos(1, &phys_addr);
            KERNEL_ASSERT(success, "Out of memory")

            entry->phys_addr = phys_addr >> 12;
            entry->present = true;
            entry->write = true;
            entry->prot = space->prot;
            entry->user = space->prot == 3;

            const VirtualAddress virt_addr = kmap_phys_range(phys_addr, 1, PAGING_WRITABLE);
            memset((void*)virt_addr, 0, PAGE_SIZE);

            MappingEntry* mapping_entry = (MappingEntry*)get_memory_entry();
            mapping_entry->virt_addr = virt_addr >> 12;
            mapping_entry->phys_addr = phys_addr >> 12;
            mapping_entry->next = (VirtualAddress)space->entry_maps[level];
            space->entry_maps[level] = mapping_entry;

            return (PageEntry*)virt_addr;
        }
    }
}

__attribute__((always_inline)) void populate_page_table_location(AddressSpace* space,
                                                                 VirtualAddress virt_addr,
                                                                 PageTableLocation* location,
                                                                 bool alloc_entries) {
    location->pd_index = GET_LEVEL_INDEX(virt_addr, PDP);
    {
        PageEntry* entry = &space->pdp[location->pd_index];
        location->pd = alloc_entries ? get_or_alloc_page_entries(space, entry, PD)
                                     : get_page_entries(space, entry, PD);
    }

    location->pt_index = GET_LEVEL_INDEX(virt_addr, PD);
    {
        PageEntry* entry = &location->pd[location->pt_index];
        location->pt = alloc_entries ? get_or_alloc_page_entries(space, entry, PT)
                                     : get_page_entries(space, entry, PT);
    }
}

void add_range_to_free_list(AddressSpace* space, VirtualAddress virt_addr, uint64_t pages) {
    FreeListEntry* entry = (FreeListEntry*)get_memory_entry();
    entry->addr = virt_addr >> 12;
    entry->pages = pages;

    entry->next = (VirtualAddress)space->free_list;
    space->free_list = entry;
}

void set_flags(PageEntry* entry, PagingFlags flags) {
    entry->write = (flags & PAGING_WRITABLE) != 0;
    entry->cache_disable = (flags & PAGING_CACHE_DISABLE) != 0;
    entry->write_through = (flags & PAGING_WRITE_THROUGH) != 0;
    entry->execute_disable = ((flags & PAGING_EXECUTABLE) == 0) && g_paging_execute_disable;
}

VirtualAddress alloc_addr_space(AddressSpace* space, uint64_t pages) {
    // Find previously used address space
    FreeListEntry* entry = space->free_list;
    {
        FreeListEntry* last = 0;
        while (entry != 0) {
            // TODO(Anton Lilja, 01/05/2021):
            // We could probably do this a smarter way,
            // so that we don't fragment address space as much
            if (pages < entry->pages) {
                const VirtualAddress addr = entry->addr << 12;
                entry->addr = (addr + pages * PAGE_SIZE) >> 12;
                entry->pages += pages;
                return SIGN_EXT_ADDR(addr);
            }
            else if (entry->pages == pages) {
                if (last == 0) {
                    space->free_list = (FreeListEntry*)entry->next;
                }
                else {
                    last->next = entry->next;
                }

                const VirtualAddress addr = entry->addr << 12;
                free_memory_entry((MemoryEntry*)entry);
                return SIGN_EXT_ADDR(addr);
            }

            last = entry;
            entry = (FreeListEntry*)entry->next;
        }
    }

    VirtualAddress addr = space->current_address;
    space->current_address += pages * PAGE_SIZE;

    KERNEL_ASSERT(space->current_address < ((space->pdp_index + 1) * PDP_MEM_RANGE),
                  "Out of address space")
    return SIGN_EXT_ADDR(addr);
}

void page_table_traversal_helper(AddressSpace* space, VirtualAddress virt_addr,
                                 PageTableLocation* location) {
    const uint16_t new_pd_index = GET_LEVEL_INDEX(virt_addr, PDP);
    const uint16_t new_pt_index = GET_LEVEL_INDEX(virt_addr, PD);
    if (new_pd_index != location->pd_index) {
        location->pd_index = new_pd_index;
        location->pt_index = 0;

        location->pd = get_or_alloc_page_entries(space, &space->pdp[location->pd_index], PD);

        location->pt = get_or_alloc_page_entries(space, &location->pd[location->pt_index], PT);
    }
    else if (new_pt_index != location->pt_index) {
        location->pt_index = new_pt_index;
        location->pt = get_or_alloc_page_entries(space, &location->pd[location->pt_index], PT);
    }
}

void map_range_helper(AddressSpace* space, VirtualAddress virt_addr, PhysicalAddress phys_addr,
                      uint64_t pages, PagingFlags flags, PageTableLocation* location) {
    for (uint64_t i = 0; i < pages; ++i) {

        const uint16_t index = GET_LEVEL_INDEX(virt_addr, PT);
        location->pt[index].phys_addr = phys_addr >> 12;
        location->pt[index].present = true;
        set_flags(&location->pt[index], flags);

        location->pt[index].prot = space->prot;
        location->pt[index].user = space->prot == 3;

        // Invalidate TLB entry for page belonging to virtual address
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");

        phys_addr += PAGE_SIZE;
        virt_addr += PAGE_SIZE;
        page_table_traversal_helper(space, virt_addr, location);
    }
}

VirtualAddress map_allocation(AddressSpace* space, PageFrameAllocation* allocation,
                              PagingFlags flags) {
    const uint64_t total_pages = calculate_allocation_pages(allocation);

    const VirtualAddress virt_addr = alloc_addr_space(space, total_pages);
    VirtualAddress curr_virt_addr = virt_addr;

    PageTableLocation location;
    populate_page_table_location(space, virt_addr, &location, true);

    while (allocation != 0) {
        uint64_t allocation_size = get_frame_order_size(allocation->order);
        uint64_t pages = allocation_size / PAGE_SIZE;
        map_range_helper(space, curr_virt_addr, allocation->addr, pages, flags, &location);
        allocation = allocation->next;
        curr_virt_addr += allocation_size;
    }

    return virt_addr;
}

VirtualAddress map_phys_range(AddressSpace* space, PhysicalAddress phys_addr, uint64_t pages,
                              PagingFlags flags) {
    const VirtualAddress virt_addr = alloc_addr_space(space, pages);

    PageTableLocation location;
    populate_page_table_location(space, virt_addr, &location, true);

    map_range_helper(space, virt_addr, phys_addr, pages, flags, &location);
    return virt_addr;
}

bool claim_virt_range(AddressSpace* space, VirtualAddress virt_addr, uint64_t pages) {
    if (space->current_address <= virt_addr) {
        // Check if virtual address range would be outside of the PDP range
        if (virt_addr + (pages * PAGE_SIZE) > (space->pdp_index + 1) * PDP_MEM_RANGE) return false;

        FreeListEntry* free_entry = (FreeListEntry*)get_memory_entry();
        free_entry->addr = space->current_address >> 12;
        free_entry->pages = (virt_addr - space->current_address) / PAGE_SIZE;
        free_entry->next = (VirtualAddress)space->free_list;
        space->free_list = free_entry;

        space->current_address = virt_addr + pages * PAGE_SIZE;
    }
    else {
        // TODO(Anton Lilja, 11/05/2021):
        // Look through free lists to see if address range is available
        return false;
    }

    return true;
}

bool map_allocation_to_range(AddressSpace* space, PageFrameAllocation* allocation,
                             VirtualAddress virt_addr, PagingFlags flags) {
    const uint64_t total_pages = calculate_allocation_pages(allocation);

    if (claim_virt_range(space, virt_addr, total_pages) == false) return false;

    PageTableLocation location;
    populate_page_table_location(space, virt_addr, &location, true);

    while (allocation != 0) {
        const uint64_t allocation_size = get_frame_order_size(allocation->order);
        const uint64_t pages = allocation_size / PAGE_SIZE;
        map_range_helper(space, virt_addr, allocation->addr, pages, flags, &location);
        allocation = allocation->next;
        virt_addr += allocation_size;
    }

    return true;
}

bool map_to_range(AddressSpace* space, PhysicalAddress phys_addr, VirtualAddress virt_addr,
                  uint64_t pages, PagingFlags flags) {
    if (claim_virt_range(space, virt_addr, pages) == false) return false;

    PageTableLocation location;
    populate_page_table_location(space, virt_addr, &location, true);

    map_range_helper(space, virt_addr, phys_addr, pages, flags, &location);

    return true;
}

void unmap_range(AddressSpace* space, VirtualAddress virt_addr, uint64_t pages) {
    add_range_to_free_list(space, virt_addr, pages);

    PageTableLocation location;
    populate_page_table_location(space, virt_addr, &location, false);

    for (uint64_t i = 0; i < pages; ++i) {
        const uint16_t index = GET_LEVEL_INDEX(virt_addr, PT);
        location.pt[index].value = 0;

        // Invalidate TLB entry for page belonging to virtual address
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");

        virt_addr += PAGE_SIZE;
        page_table_traversal_helper(space, virt_addr, &location);
    }
}

void unmap_and_free_frames(AddressSpace* space, VirtualAddress virt_addr, uint64_t pages) {
    add_range_to_free_list(space, virt_addr, pages);

    PageTableLocation location;
    populate_page_table_location(space, virt_addr, &location, false);

    PhysicalAddress start_phys_addr;
    PhysicalAddress curr_phys_addr;
    PhysicalAddress frame_pages = 0;
    for (uint64_t i = 0; i < pages; ++i) {
        const uint16_t index = GET_LEVEL_INDEX(virt_addr, PT);

        if (frame_pages == 0) {
            start_phys_addr = curr_phys_addr = (location.pt[index].phys_addr << 12);
            frame_pages = 1;
        }
        else {
            curr_phys_addr += PAGE_SIZE;

            if (curr_phys_addr != (location.pt[index].phys_addr << 12) ||
                frame_pages * PAGE_SIZE >= get_frame_order_size(FRAME_ORDERS - 1)) {
                KERNEL_ASSERT(__builtin_popcountll(frame_pages) == 1,
                              "Allocation is not power of 2")
                free_frames_contiguos(start_phys_addr, frame_pages);
                frame_pages = 0;
            }
            else {
                ++frame_pages;
            }
        }

        location.pt[index].value = 0;

        // Invalidate TLB entry for page belonging to virtual address
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");

        virt_addr += PAGE_SIZE;
        page_table_traversal_helper(space, virt_addr, &location);
    }

    if (frame_pages != 0) {
        KERNEL_ASSERT(__builtin_popcountll(frame_pages) == 1, "Allocation is not power of 2")
        free_frames_contiguos(start_phys_addr, frame_pages);
    }
}

bool virt_to_phys_addr(AddressSpace* space, VirtualAddress virt_addr, PhysicalAddress* phys_addr) {
    const uint16_t pd_index = GET_LEVEL_INDEX(virt_addr, PDP);
    const PageEntry* pd = get_page_entries(space, &space->pdp[pd_index], PD);
    if (pd == 0) return false;

    const uint16_t pt_index = GET_LEVEL_INDEX(virt_addr, PD);
    const PageEntry* pt = get_page_entries(space, &pd[pt_index], PT);
    if (pt == 0) return false;

    const uint16_t index = GET_LEVEL_INDEX(virt_addr, PT);
    if (pt[index].present == false) return false;

    *phys_addr = (pt[index].phys_addr << 12) | (virt_addr & OFFSET_INDEX_MASK);
    return true;
}

VirtualAddress kmap_allocation(PageFrameAllocation* allocation, PagingFlags flags) {
    return map_allocation(&g_kernel_space, allocation, flags);
}

VirtualAddress kmap_phys_range(PhysicalAddress phys_addr, uint64_t pages, PagingFlags flags) {
    return map_phys_range(&g_kernel_space, phys_addr, pages, flags);
}

void kunmap_range(VirtualAddress virt_addr, uint64_t pages) {
    unmap_range(&g_kernel_space, virt_addr, pages);
}

void kunmap_and_free_frames(VirtualAddress virt_addr, uint64_t pages) {
    unmap_and_free_frames(&g_kernel_space, virt_addr, pages);
}

bool kvirt_to_phys_addr(VirtualAddress virt_addr, PhysicalAddress* phys_addr) {
    return virt_to_phys_addr(&g_kernel_space, virt_addr, phys_addr);
}

void free_uefi_memory_and_remove_identity_mapping(void* uefi_memory_map) {
    // Free UEFI memory
    {
        const UEFIMemoryMap* memory_map = (const UEFIMemoryMap*)uefi_memory_map;
        for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
            const UEFIMemoryDescriptor* desc = (const UEFIMemoryDescriptor*)&memory_map->buffer[i];
            switch (desc->type) {
                case EfiBootServicesData:
                case EfiRuntimeServicesData:
                case EfiLoaderData: {
                    PhysicalAddress phys_addr = desc->physical_start;
                    uint64_t size = desc->num_pages * PAGE_SIZE;
                    while (size != 0) {
                        uint8_t order = 0;
                        for (; order < FRAME_ORDERS; ++order) {
                            const uint64_t order_size = get_frame_order_size(order);
                            if (order_size > size || (phys_addr % order_size) != 0) break;
                        }
                        --order;

                        KERNEL_ASSERT(order < FRAME_ORDERS, "Order does not exist")

                        const uint64_t order_size = get_frame_order_size(order);
                        free_frames_contiguos(phys_addr, order_size / PAGE_SIZE);
                        size -= order_size;
                        phys_addr += order_size;
                    }
                    break;
                }
                default: break;
            }
        }
    }

    // Clear idenity mapping
    for (uint64_t i = 0; i < KERNEL_PML4_OFFSET; ++i) g_pml4[i].value = 0;

    // Refresh TLB
    asm volatile("mov %%cr3, %%rax\n"
                 "mov %%rax, %%cr3\n"
                 :
                 :
                 : "rax", "cr3", "memory");
}

VirtualAddress initialize_paging(void* uefi_memory_map, PhysicalAddress kernel_phys_addr,
                                 uint64_t kernel_size) {
    _Static_assert(KERNEL_PML4_OFFSET < PAGE_ENTRY_COUNT, "Kernel PML4 offset is out of bounds");
    _Static_assert(sizeof(PageEntry) == 8, "PageEntry is not 8 bytes");
    _Static_assert(sizeof(FreeListEntry) == 16, "FreeListEntry struct not 16 bytes");
    _Static_assert(sizeof(MappingEntry) == 16, "MappingEntry struct not 16 bytes");

    // Check for NX bit with CPUID
    {
        uint32_t edx;
        asm volatile("mov $0x80000001, %%eax\n"
                     "cpuid\n"
                     : "=d"(edx)
                     :
                     : "rax", "rbx", "rcx", "memory", "cc");

        g_paging_execute_disable = ((edx >> 20) & 1) != 0;
    }

    struct {
        PhysicalAddress phys_addr;
        uint64_t total_pages;
        uint64_t entry_pool_pages;
    } frame_allocator;
    alloc_frame_allocator_memory(uefi_memory_map,
                                 &frame_allocator.phys_addr,
                                 &frame_allocator.total_pages,
                                 &frame_allocator.entry_pool_pages);

    const uint64_t frame_allocator_size = frame_allocator.total_pages * PAGE_SIZE;

    const uint64_t starting_pool_size = PAGE_POOL_THRESHOLD * 2 * PAGE_SIZE;

    const uint64_t memory_entries_size = PAGE_SIZE;

    const uint64_t total_size =
        kernel_size + starting_pool_size + frame_allocator_size + memory_entries_size;

    const uint64_t pdp_count = 1;

    uint64_t pd_count = MAX(total_size / PD_MEM_RANGE, 1U);

    // Make sure we have room to map PD entries into virtual memory
    while ((pd_count * PD_MEM_RANGE - total_size) < pd_count * PAGE_SIZE) ++pd_count;

    uint64_t pt_count = MAX(total_size / PT_MEM_RANGE, 1U);

    // Make sure we have room to map PT entries into virtual memory
    while ((pt_count * PT_MEM_RANGE - total_size) < (pt_count + pd_count) * PAGE_SIZE) ++pt_count;

    const uint64_t pages_to_allocate =
        pdp_count + pd_count + pt_count + ((starting_pool_size + memory_entries_size) / PAGE_SIZE);

    KERNEL_ASSERT(pages_to_allocate * PAGE_SIZE < get_memory_size(),
                  "Memory initialization requires more memory than we have")

    KERNEL_ASSERT(pages_to_allocate * PAGE_SIZE < PDP_MEM_RANGE,
                  "Memory initialization requires more than 512GB")

    // Allocate pages required by page table and address allocator
    PhysicalAddress allocated_phys_addr = 0;
    const UEFIMemoryMap* memory_map = (UEFIMemoryMap*)uefi_memory_map;
    for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
        UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];

        // Other memory types are either unusable or have memory that is currently being used
        const bool correct_type = desc->type == EfiConventionalMemory ||
                                  desc->type == EfiRuntimeServicesCode ||
                                  desc->type == EfiBootServicesCode;

        if (!correct_type) continue;

        // Skip memory before 1MB
        if (desc->physical_start < 0x100000) continue;

        if (desc->num_pages < pages_to_allocate) continue;

        allocated_phys_addr = desc->physical_start;
        desc->physical_start += pages_to_allocate * PAGE_SIZE;
        desc->num_pages -= pages_to_allocate;
        break;
    }

    KERNEL_ASSERT(allocated_phys_addr != 0, "Out of memory")

    // Zero out allocated memory
    memset((void*)allocated_phys_addr, 0, pages_to_allocate * PAGE_SIZE);

    // Retain old identity mapping from UEFI
    {
        PageEntry* uefi_pml4;
        asm("mov %%cr3, %[pml4]" : [pml4] "=r"(uefi_pml4));
        for (uint64_t i = 0; i < 512; ++i) {
            if (uefi_pml4[i].present) {
                g_pml4[i].value = uefi_pml4[i].value;
            }
        }
    }

    g_kernel_space.pdp = (PageEntry*)allocated_phys_addr;
    allocated_phys_addr += PAGE_SIZE;

    g_pml4[KERNEL_PML4_OFFSET].phys_addr = (PhysicalAddress)g_kernel_space.pdp >> 12;
    g_pml4[KERNEL_PML4_OFFSET].present = true;
    g_pml4[KERNEL_PML4_OFFSET].write = true;

    // Populate page table with PD and PT entries
    {
        uint64_t curr_pt_count = pt_count;
        for (uint64_t i = 0; i < pd_count; ++i) {
            PageEntry* pd = (PageEntry*)allocated_phys_addr;
            allocated_phys_addr += PAGE_SIZE;

            g_kernel_space.pdp[i].phys_addr = (PhysicalAddress)pd >> 12;
            g_kernel_space.pdp[i].present = true;
            g_kernel_space.pdp[i].write = true;

            for (uint64_t j = 0; j < MIN(curr_pt_count, 512U); ++j) {
                pd[j].phys_addr = (PhysicalAddress)allocated_phys_addr >> 12;
                pd[j].present = true;
                pd[j].write = true;

                allocated_phys_addr += PAGE_SIZE;
            }
            curr_pt_count -= 512;
        }
    }

#define MAP_PAGE(_virt_addr, _phys_addr, _write, _executable)                       \
    {                                                                               \
        const uint16_t pd_index = GET_LEVEL_INDEX(_virt_addr, PDP);                 \
        const uint16_t pt_index = GET_LEVEL_INDEX(_virt_addr, PD);                  \
        PageEntry* pd = (PageEntry*)(g_kernel_space.pdp[pd_index].phys_addr << 12); \
        PageEntry* pt = (PageEntry*)(pd[pt_index].phys_addr << 12);                 \
                                                                                    \
        const uint16_t index = GET_LEVEL_INDEX(_virt_addr, PT);                     \
        pt[index].phys_addr = _phys_addr;                                           \
        pt[index].present = true;                                                   \
        pt[index].write = _write;                                                   \
        pt[index].execute_disable = (!(_executable)) && g_paging_execute_disable;   \
    }

    // Map kernel into virtual memory
    {
        PhysicalAddress curr_phys_addr = kernel_phys_addr;
        while (curr_phys_addr != kernel_phys_addr + kernel_size) {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
            g_kernel_space.current_address += PAGE_SIZE;

            const bool is_data_segment = bound_contains(
                curr_phys_addr, (uint64_t)&s_kernel_data_start, (uint64_t)&s_kernel_data_end);

            const bool is_rodata_segment = bound_contains(
                curr_phys_addr, (uint64_t)&s_kernel_rodata_start, (uint64_t)&s_kernel_rodata_end);

            // Make sure the pages only have privileges that they require
            if (is_data_segment) {
                MAP_PAGE(virt_addr, curr_phys_addr >> 12, true, false)
            }
            else if (is_rodata_segment) {
                MAP_PAGE(virt_addr, curr_phys_addr >> 12, false, false)
            }
            else {
                MAP_PAGE(virt_addr, curr_phys_addr >> 12, true, true)
            }

            curr_phys_addr += PAGE_SIZE;
        }
    }

    // Map memory entries into virtual memory
    const VirtualAddress memory_entries_virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
    for (uint64_t i = 0; i < memory_entries_size / PAGE_SIZE; ++i) {
        MAP_PAGE(g_kernel_space.current_address, allocated_phys_addr >> 12, true, false)
        g_kernel_space.current_address += PAGE_SIZE;
        allocated_phys_addr += PAGE_SIZE;
    }

    // Switch to new page table
    asm("mov %[pml4], %%cr3" : : [pml4] "r"(g_pml4) : "cr3", "memory");

    fill_memory_entry_pool(memory_entries_virt_addr, memory_entries_size / PAGE_SIZE);

    // Initialize page pool
    {
        for (uint64_t i = 0; i < starting_pool_size / PAGE_SIZE; ++i) {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
            g_kernel_space.current_address += PAGE_SIZE;

            MAP_PAGE(virt_addr, allocated_phys_addr >> 12, true, false)

            PagePoolEntry* pool_entry = (PagePoolEntry*)get_memory_entry();
            pool_entry->next = (VirtualAddress)g_page_pool.head;
            pool_entry->phys_addr = allocated_phys_addr >> 12;
            pool_entry->virt_addr = virt_addr >> 12;
            g_page_pool.head = pool_entry;

            allocated_phys_addr += PAGE_SIZE;
        }

        g_page_pool.count += starting_pool_size / PAGE_SIZE;
    }

    // Map frame allocator memory into virtual memory
    const VirtualAddress frame_allocator_virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
    {
        PhysicalAddress curr_phys_addr = frame_allocator.phys_addr;
        while (curr_phys_addr != frame_allocator.phys_addr + frame_allocator_size) {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
            g_kernel_space.current_address += PAGE_SIZE;
            MAP_PAGE(virt_addr, curr_phys_addr >> 12, true, false)
            curr_phys_addr += PAGE_SIZE;
        }
    }

    // Map page entries into virtual memory
    {
        {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
            g_kernel_space.current_address += PAGE_SIZE;

            MAP_PAGE(virt_addr, (PhysicalAddress)g_kernel_space.pdp >> 12, true, false)

            g_kernel_space.pdp = (PageEntry*)virt_addr;
        }

        uint64_t curr_pt_count = pt_count;
        for (uint64_t i = 0; i < pd_count; ++i) {
            // Map PD entry
            {
                const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_space.current_address);
                g_kernel_space.current_address += PAGE_SIZE;

                MAP_PAGE(virt_addr, g_kernel_space.pdp[i].phys_addr, true, false)

                MappingEntry* entry = (MappingEntry*)get_memory_entry();
                entry->next = (VirtualAddress)g_kernel_space.entry_maps[PD];
                entry->phys_addr = g_kernel_space.pdp[i].phys_addr;
                entry->virt_addr = virt_addr >> 12;
                g_kernel_space.entry_maps[PD] = entry;
            }

            // Map PT entries
            PageEntry* pd = (PageEntry*)(g_kernel_space.pdp[i].phys_addr << 12);
            for (uint64_t j = 0; j < MIN(curr_pt_count, 512U); ++j) {
                const VirtualAddress virt_addr = g_kernel_space.current_address;
                g_kernel_space.current_address += PAGE_SIZE;

                MAP_PAGE(virt_addr, pd[j].phys_addr, true, false)

                MappingEntry* entry = (MappingEntry*)get_memory_entry();
                entry->next = (VirtualAddress)g_kernel_space.entry_maps[PT];
                entry->phys_addr = pd[j].phys_addr;
                entry->virt_addr = virt_addr >> 12;
                g_kernel_space.entry_maps[PT] = entry;
            }
            curr_pt_count -= 512;
        }
    }

    // Invalidate TLB to make new paging take effect
    asm("mov %%cr3, %%rax\n"
        "mov %%rax, %%cr3\n"
        :
        :
        : "rax", "cr3", "memory");

    initialize_frame_allocator(frame_allocator_virt_addr,
                               frame_allocator.total_pages,
                               uefi_memory_map,
                               frame_allocator.entry_pool_pages);

    return SIGN_EXT_ADDR(KERNEL_OFFSET);
}
