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

#define SIGN_EXT_ADDR(addr) ((addr) | (((((addr) >> 47) & 1) * 0xffffULL) << 48))

typedef union {
    struct {
        bool present : 1;
        bool write : 1;
        bool user : 1;
        bool write_through : 1;
        bool cache_disable : 1;
        bool accessed : 1;
        bool dirty : 1;
        bool large : 1;
        bool global : 1;
        uint8_t ignored0 : 3;
        PhysicalAddress phys_addr : 40;
        uint8_t ignored1 : 7;
        uint8_t prot : 4;
        bool execute_disable : 1;
    } __attribute__((packed));
    uint64_t value;
} PageEntry;

typedef struct {
    VirtualAddress next : 48;
    VirtualAddress addr : 36;
    uint64_t pages : 44;
} __attribute__((packed)) FreeListEntry;

typedef struct {
    VirtualAddress next : 48;
    VirtualAddress virt_addr : 36;
    PhysicalAddress phys_addr : 38;
} __attribute__((packed)) MappingEntry;

typedef MappingEntry PagePoolEntry;

PageEntry __attribute__((aligned(0x1000))) g_pml4[512] = {0};
PageEntry __attribute__((aligned(0x1000))) g_kernel_pdp[512] = {0};

struct {
    VirtualAddress current_address;
    FreeListEntry* free_list;
} g_kernel_map = {
    .current_address = KERNEL_OFFSET,
    .free_list = 0,
};

struct {
    uint64_t count;
    PagePoolEntry* head;
} g_page_pool = {0};

// Holds physical to virtual mapping of page table entries
MappingEntry* g_page_entry_mapping_list = 0;

// See linker.ld
extern char s_kernel_rodata_start;
extern char s_kernel_rodata_end;
extern char s_kernel_data_start;
extern char s_kernel_data_end;
extern char s_stack_top;

PageEntry* get_page_entries(PageEntry* entry) {
    if (entry->present) {
        PhysicalAddress phys_addr = entry->phys_addr;
        MappingEntry* mapping = g_page_entry_mapping_list;
        while (mapping != 0) {
            if (mapping->phys_addr == phys_addr) {
                return (PageEntry*)SIGN_EXT_ADDR(mapping->virt_addr << 12);
            }
            mapping = (MappingEntry*)SIGN_EXT_ADDR(mapping->next);
        }
        return 0;
    }
    else {
        if (g_page_pool.count <= PAGE_POOL_THRESHOLD) {
            // Adjust pool count beforehand to avoid getting stuck in an infinite loop
            g_page_pool.count += PAGE_POOL_THRESHOLD;

            PageFrameAllocation* allocation = alloc_frames(PAGE_POOL_THRESHOLD);
            KERNEL_ASSERT(allocation != 0, "Out of memory")

            VirtualAddress virt_addr = map_allocation(allocation);

            // Populate pool with allocated pages
            for (uint64_t i = 0; i < PAGE_POOL_THRESHOLD; ++i) {
                PagePoolEntry* entry = (PagePoolEntry*)get_memory_entry();
                entry->next = (VirtualAddress)g_page_pool.head;
                g_page_pool.head = entry;
                virt_addr += PAGE_SIZE;
            }

            while (allocation != 0) {
                // Add page to page entry mapping list
                uint64_t size = get_order_block_size(allocation->order);
                PhysicalAddress phys_addr = allocation->addr;
                while (size != 0) {
                    MappingEntry* mapping_entry = (MappingEntry*)get_memory_entry();
                    mapping_entry->phys_addr = phys_addr >> 12;
                    mapping_entry->virt_addr = virt_addr >> 12;
                    mapping_entry->next = (VirtualAddress)g_page_entry_mapping_list;
                    g_page_entry_mapping_list = mapping_entry;
                    phys_addr += PAGE_SIZE;
                    virt_addr += PAGE_SIZE;
                    size -= PAGE_SIZE;
                }

                // Free allocation entry because we're not going free the memory anyway
                MemoryEntry* entry = (MemoryEntry*)allocation;
                allocation = allocation->next;
                free_memory_entry(entry);
            }
        }

        PagePoolEntry* pool_entry = g_page_pool.head;
        g_page_pool.head = (PagePoolEntry*)SIGN_EXT_ADDR(pool_entry->next);
        --g_page_pool.count;

        // TODO(Anton Lilja, 01/05/2021):
        // Disable execute privileges if functionality is available
        entry->phys_addr = pool_entry->phys_addr;
        entry->present = true;
        entry->write = true;

        PageEntry* entries = (PageEntry*)SIGN_EXT_ADDR(pool_entry->virt_addr << 12);

        free_memory_entry((MemoryEntry*)pool_entry);

        return entries;
    }
}

VirtualAddress alloc_addr_space(uint64_t pages) {
    // Find previously used address space
    FreeListEntry* entry = g_kernel_map.free_list;
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
                return addr;
            }
            else if (entry->pages == pages) {
                if (last == 0) {
                    g_kernel_map.free_list = (FreeListEntry*)entry->next;
                }
                else {
                    last->next = entry->next;
                }

                const VirtualAddress addr = entry->addr << 12;
                free_memory_entry((MemoryEntry*)entry);
                return addr;
            }

            last = entry;
            entry = (FreeListEntry*)entry->next;
        }
    }

    VirtualAddress addr = g_kernel_map.current_address;
    g_kernel_map.current_address += pages * PAGE_SIZE;

    // TODO(Anton Lilja, 01/05/2021):
    // Make use of all address space in [KERNEL_OFFSET, 2^48)
    KERNEL_ASSERT(g_kernel_map.current_address < (KERNEL_OFFSET + PDP_MEM_RANGE),
                  "Out of address space")
    return SIGN_EXT_ADDR(addr);
}

void page_table_traversal_helper(VirtualAddress virt_addr, uint16_t* pd_index, PageEntry** pd_ptr,
                                 uint16_t* pt_index, PageEntry** pt_ptr) {
    const uint16_t new_pd_index = GET_LEVEL_INDEX(virt_addr, PDP);
    const uint16_t new_pt_index = GET_LEVEL_INDEX(virt_addr, PD);
    if (new_pd_index != *pd_index) {
        *pd_index = new_pd_index;
        *pt_index = 0;

        *pd_ptr = get_page_entries(&g_kernel_pdp[*pd_index]);

        PageEntry* pd = *pd_ptr;
        *pt_ptr = get_page_entries(&pd[*pt_index]);
    }
    else if (new_pt_index != *pt_index) {
        PageEntry* pd = *pd_ptr;
        *pt_index = new_pt_index;
        *pt_ptr = get_page_entries(&pd[*pt_index]);
    }
}

void map_range_helper(VirtualAddress virt_addr, PhysicalAddress phys_addr, uint64_t pages,
                      uint16_t* pd_index, PageEntry** pd_ptr, uint16_t* pt_index,
                      PageEntry** pt_ptr) {
    for (uint64_t i = 0; i < pages; ++i) {
        PageEntry* pt = *pt_ptr;

        const uint16_t index = GET_LEVEL_INDEX(virt_addr, PT);
        pt[index].phys_addr = phys_addr >> 12;
        pt[index].present = true;
        pt[index].write = true;

        // Invalidate TLB entry for page belonging to virtual address
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");

        phys_addr += PAGE_SIZE;
        virt_addr += PAGE_SIZE;
        page_table_traversal_helper(virt_addr, pd_index, pd_ptr, pt_index, pt_ptr);
    }
}

VirtualAddress map_allocation(PageFrameAllocation* allocation) {
    // Calculate total size of mapping
    uint64_t size = 0;
    {
        PageFrameAllocation* alloc = allocation;
        while (alloc != 0) {
            size += get_order_block_size(alloc->order);
            alloc = alloc->next;
        }
    }

    const VirtualAddress virt_addr = alloc_addr_space(size / PAGE_SIZE);
    VirtualAddress curr_virt_addr = virt_addr;

    uint16_t pd_index = GET_LEVEL_INDEX(curr_virt_addr, PDP);
    PageEntry* pd = get_page_entries(&g_kernel_pdp[pd_index]);

    uint16_t pt_index = GET_LEVEL_INDEX(curr_virt_addr, PD);
    PageEntry* pt = get_page_entries(&pd[pt_index]);

    while (allocation != 0) {
        uint64_t allocation_size = get_order_block_size(allocation->order);
        uint64_t pages = allocation_size / PAGE_SIZE;
        map_range_helper(curr_virt_addr, allocation->addr, pages, &pd_index, &pd, &pt_index, &pt);
        allocation = allocation->next;
        curr_virt_addr += allocation_size;
    }

    return virt_addr;
}

VirtualAddress map_range(PhysicalAddress phys_addr, uint64_t pages) {
    const VirtualAddress virt_addr = alloc_addr_space(pages);

    uint16_t pd_index = GET_LEVEL_INDEX(virt_addr, PDP);
    PageEntry* pd = get_page_entries(&g_kernel_pdp[pd_index]);

    uint16_t pt_index = GET_LEVEL_INDEX(virt_addr, PD);
    PageEntry* pt = get_page_entries(&pd[pt_index]);

    map_range_helper(virt_addr, phys_addr, pages, &pd_index, &pd, &pt_index, &pt);
    return virt_addr;
}

void unmap(VirtualAddress virt_addr, uint64_t pages) {
    // Add range to free list
    {
        FreeListEntry* entry = (FreeListEntry*)get_memory_entry();
        entry->addr = virt_addr >> 12;
        entry->pages = pages;

        entry->next = (VirtualAddress)g_kernel_map.free_list;
        g_kernel_map.free_list = entry;
    }

    uint16_t pd_index = GET_LEVEL_INDEX(virt_addr, PDP);
    PageEntry* pd = get_page_entries(&g_kernel_pdp[pd_index]);

    uint16_t pt_index = GET_LEVEL_INDEX(virt_addr, PD);
    PageEntry* pt = get_page_entries(&pd[pt_index]);
    for (uint64_t i = 0; i < pages; ++i) {
        const uint16_t index = GET_LEVEL_INDEX(virt_addr, PT);
        pt[index].value = 0;

        // Invalidate TLB entry for page belonging to virtual address
        asm volatile("invlpg (%[virt_addr])\n" : : [virt_addr] "r"(virt_addr) : "memory");

        virt_addr += PAGE_SIZE;
        page_table_traversal_helper(virt_addr, &pd_index, &pd, &pt_index, &pt);
    }
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
                            if (get_order_block_size(order) > size ||
                                phys_addr % get_order_block_size(order) != 0)
                                break;
                        }
                        --order;

                        free_frames_contiguos(phys_addr, order);
                        size -= get_order_block_size(order);
                        phys_addr += get_order_block_size(order);
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

    uint64_t pd_count = MAX(total_size / PD_MEM_RANGE, 1U);

    // Make sure we have room to map PD entries into virtual memory
    while ((pd_count * PD_MEM_RANGE - total_size) < pd_count * PAGE_SIZE) ++pd_count;

    uint64_t pt_count = MAX(total_size / PT_MEM_RANGE, 1U);

    // Make sure we have room to map PT entries into virtual memory
    while ((pt_count * PT_MEM_RANGE - total_size) < (pt_count + pd_count) * PAGE_SIZE) ++pt_count;

    const uint64_t pages_to_allocate = pd_count + pt_count;

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

    g_pml4[KERNEL_PML4_OFFSET].phys_addr = (PhysicalAddress)g_kernel_pdp >> 12;
    g_pml4[KERNEL_PML4_OFFSET].present = true;
    g_pml4[KERNEL_PML4_OFFSET].write = true;

    // Populate page table with PD and PT entries
    {
        uint64_t curr_pt_count = pt_count;
        for (uint64_t i = 0; i < pd_count; ++i) {
            PageEntry* pd = (PageEntry*)allocated_phys_addr;
            allocated_phys_addr += PAGE_SIZE;

            g_kernel_pdp[i].phys_addr = (PhysicalAddress)pd >> 12;
            g_kernel_pdp[i].present = true;
            g_kernel_pdp[i].write = true;

            for (uint64_t j = 0; j < MIN(curr_pt_count, 512U); ++j) {
                pd[j].phys_addr = (PhysicalAddress)allocated_phys_addr >> 12;
                pd[j].present = true;
                pd[j].write = true;

                allocated_phys_addr += PAGE_SIZE;
            }
            curr_pt_count -= 512;
        }
    }

#define MAP_PAGE(_virt_addr, _phys_addr, _write)                              \
    {                                                                         \
        const uint16_t pd_index = GET_LEVEL_INDEX(_virt_addr, PDP);           \
        const uint16_t pt_index = GET_LEVEL_INDEX(_virt_addr, PD);            \
        PageEntry* pd = (PageEntry*)(g_kernel_pdp[pd_index].phys_addr << 12); \
        PageEntry* pt = (PageEntry*)(pd[pt_index].phys_addr << 12);           \
                                                                              \
        const uint16_t index = GET_LEVEL_INDEX(_virt_addr, PT);               \
        pt[index].phys_addr = _phys_addr;                                     \
        pt[index].present = true;                                             \
        pt[index].write = _write;                                             \
    }

    // Map kernel into virtual memory
    {
        PhysicalAddress curr_phys_addr = kernel_phys_addr;
        while (curr_phys_addr != kernel_phys_addr + kernel_size) {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_map.current_address);
            g_kernel_map.current_address += PAGE_SIZE;

            const bool is_data_segment = bound_contains(
                curr_phys_addr, (uint64_t)&s_kernel_data_start, (uint64_t)&s_kernel_data_end);

            const bool is_rodata_segment = bound_contains(
                curr_phys_addr, (uint64_t)&s_kernel_rodata_start, (uint64_t)&s_kernel_rodata_end);

            // Make sure the pages only have privileges that they require
            if (is_data_segment) {
                // TODO (Anton Lilja, 29-04-21):
                // Disable execute privileges if functionality is available
                MAP_PAGE(virt_addr, curr_phys_addr >> 12, true)
            }
            else if (is_rodata_segment) {
                // TODO (Anton Lilja, 29-04-21):
                // Disable execute privileges if functionality is available
                MAP_PAGE(virt_addr, curr_phys_addr >> 12, false)
            }
            else {
                MAP_PAGE(virt_addr, curr_phys_addr >> 12, true)
            }

            curr_phys_addr += PAGE_SIZE;
        }
    }

    // Map memory entries into virtual memory
    const VirtualAddress memory_entries_virt_addr = SIGN_EXT_ADDR(g_kernel_map.current_address);
    for (uint64_t i = 0; i < memory_entries_size / PAGE_SIZE; ++i) {
        MAP_PAGE(g_kernel_map.current_address, allocated_phys_addr >> 12, true)
        g_kernel_map.current_address += PAGE_SIZE;
        allocated_phys_addr += PAGE_SIZE;
    }

    // Switch to new page table
    asm("mov %[pml4], %%cr3" : : [pml4] "r"(g_pml4) : "cr3", "memory");

    fill_memory_entry_pool(memory_entries_virt_addr, memory_entries_size / PAGE_SIZE);

    // Initialize page pool
    {
        for (uint64_t i = 0; i < starting_pool_size / PAGE_SIZE; ++i) {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_map.current_address);
            g_kernel_map.current_address += PAGE_SIZE;

            MAP_PAGE(virt_addr, allocated_phys_addr >> 12, true)

            // Add page to page pool
            {
                PagePoolEntry* pool_entry = (PagePoolEntry*)get_memory_entry();
                pool_entry->next = (VirtualAddress)g_page_pool.head;
                pool_entry->phys_addr = allocated_phys_addr >> 12;
                pool_entry->virt_addr = virt_addr >> 12;
                g_page_pool.head = pool_entry;
            }

            // Add page to page entry mapping list
            {
                MappingEntry* mapping_entry = (MappingEntry*)get_memory_entry();
                mapping_entry->phys_addr = allocated_phys_addr >> 12;
                mapping_entry->virt_addr = virt_addr >> 12;
                mapping_entry->next = (VirtualAddress)g_page_entry_mapping_list;
                g_page_entry_mapping_list = mapping_entry;
            }

            allocated_phys_addr += PAGE_SIZE;
        }

        g_page_pool.count += starting_pool_size / PAGE_SIZE;
    }

    // Map frame allocator memory into virtual memory
    const VirtualAddress frame_allocator_virt_addr = SIGN_EXT_ADDR(g_kernel_map.current_address);
    {
        PhysicalAddress curr_phys_addr = frame_allocator.phys_addr;
        while (curr_phys_addr != frame_allocator.phys_addr + frame_allocator_size) {
            const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_map.current_address);
            g_kernel_map.current_address += PAGE_SIZE;
            MAP_PAGE(virt_addr, curr_phys_addr >> 12, true)
            curr_phys_addr += PAGE_SIZE;
        }
    }

    // Map page entries into virtual memory
    {
        uint64_t curr_pt_count = pt_count;
        for (uint64_t i = 0; i < pd_count; ++i) {
            // Map PD entry
            {
                const VirtualAddress virt_addr = SIGN_EXT_ADDR(g_kernel_map.current_address);
                g_kernel_map.current_address += PAGE_SIZE;

                MAP_PAGE(virt_addr, g_kernel_pdp[i].phys_addr, true)

                MappingEntry* entry = (MappingEntry*)get_memory_entry();
                entry->next = (VirtualAddress)g_page_entry_mapping_list;
                entry->phys_addr = g_kernel_pdp[i].phys_addr;
                entry->virt_addr = virt_addr >> 12;
                g_page_entry_mapping_list = entry;
            }

            // Map PT entries
            PageEntry* pd = (PageEntry*)(g_kernel_pdp[i].phys_addr << 12);
            for (uint64_t j = 0; j < MIN(curr_pt_count, 512U); ++j) {
                const VirtualAddress virt_addr = g_kernel_map.current_address;
                g_kernel_map.current_address += PAGE_SIZE;

                MAP_PAGE(virt_addr, pd[j].phys_addr, true)

                MappingEntry* entry = (MappingEntry*)get_memory_entry();
                entry->next = (VirtualAddress)g_page_entry_mapping_list;
                entry->phys_addr = pd[j].phys_addr;
                entry->virt_addr = virt_addr >> 12;
                g_page_entry_mapping_list = entry;
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