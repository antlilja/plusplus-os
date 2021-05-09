#include "memory/frame_allocator.h"

#include "uefi.h"
#include "util.h"
#include "kassert.h"
#include "memory.h"
#include "memory/entry_pool.h"

#include <string.h>

#define MIN_BLOCK_SIZE PAGE_SIZE

// Free list entry
typedef struct {
    void* next;
    uint64_t addr;
} ListEntry;

// Free lists for all block sizes
struct {
    ListEntry* head;
    uint64_t* buddy_map;
} g_free_lists[FRAME_ORDERS] = {0};

uint64_t g_block_sizes[FRAME_ORDERS];

uint64_t get_frame_order_size(uint8_t order) {
    KERNEL_ASSERT(order < FRAME_ORDERS, "Not an order")
    return g_block_sizes[order];
}

uint8_t get_min_size_order(uint64_t pages) {
    KERNEL_ASSERT(pages != 0, "Can't have a zero sized allocation")

    const uint64_t order_0_blocks =
        MIN_BLOCK_SIZE == PAGE_SIZE
            ? pages
            : ((pages * PAGE_SIZE) + g_block_sizes[0] - 1) / g_block_sizes[0];

    // The result of __builtin_clzll is undefined when input is zero
    if (order_0_blocks == 1) return 0;

    return MIN(64 - __builtin_clzll(order_0_blocks - 1), FRAME_ORDERS);
}

void free_frame_allocation_entries(PageFrameAllocation* allocations) {
    while (allocations != 0) {
        MemoryEntry* memory_entry = (MemoryEntry*)allocations;
        allocations = allocations->next;
        free_memory_entry(memory_entry);
    }
}

// Calculate array index and bit index for buddy corresponding to address and order
void calc_buddy_index(uint64_t addr, uint8_t order, uint64_t* arr_index, uint8_t* bit_index) {
    KERNEL_ASSERT(order < (FRAME_ORDERS - 1), "Order does not have a buddy map")
    *arr_index = addr / g_block_sizes[order];
    *arr_index -= (*arr_index) % 2;
    *arr_index /= 2;

    *bit_index = (*arr_index) & 0b111111;
    *arr_index &= ~0b111111;

    *arr_index /= 64;
}

bool get_buddy_bit(uint64_t addr, uint8_t order) {
    uint64_t arr_index;
    uint8_t bit_index;
    calc_buddy_index(addr, order, &arr_index, &bit_index);

    return (g_free_lists[order].buddy_map[arr_index] >> bit_index) & 1;
}

void toggle_buddy_bit(uint64_t addr, uint8_t order) {
    // The last order does not have a buddy map
    if (order == FRAME_ORDERS - 1) return;

    uint64_t arr_index;
    uint8_t bit_index;
    calc_buddy_index(addr, order, &arr_index, &bit_index);

    g_free_lists[order].buddy_map[arr_index] ^= (1ULL << bit_index);
}

void split_entry(ListEntry* entry, uint8_t order) {
    // Toogle buddy bit to mark block as allocated
    toggle_buddy_bit(entry->addr, order);

    ListEntry* left = entry;
    ListEntry* right = (ListEntry*)get_memory_entry();

    right->addr = left->addr + g_block_sizes[order - 1];
    right->next = g_free_lists[order - 1].head;

    left->next = right;
    g_free_lists[order - 1].head = left;
}

PageFrameAllocation* alloc_frames(uint64_t pages) {
    uint64_t size = pages * PAGE_SIZE;

    // Allocation list which will be returned to caller
    PageFrameAllocation* front = 0;
    PageFrameAllocation* back = 0;

    // Loop until enough memory has been allocated
    while (size != 0) {
        // Get biggest order which fits into allocations size
        int8_t order_to_alloc = 0;
        while (order_to_alloc < FRAME_ORDERS) {
            // Size big enough for order
            if (size < g_block_sizes[order_to_alloc]) break;

            ++order_to_alloc;
        }
        --order_to_alloc;

        // Split bigger blocks if none of the correct size are available
        // or create allocation from smaller blocks
        {
            int8_t order = order_to_alloc + 1;
            while (g_free_lists[order_to_alloc].head == 0) {
                if (g_free_lists[order].head == 0) {
                    ++order;

                    if (order < FRAME_ORDERS) continue;

                    // Allocate from smaller blocks because no bigger are available
                    --order_to_alloc;
                    while (order_to_alloc >= 0) {
                        if (g_free_lists[order_to_alloc].head != 0) break;
                        --order_to_alloc;
                    }

                    // Cleanup allocation if we are out of memory
                    if (order_to_alloc < 0) {
                        free_frames(front);
                        return 0;
                    }

                    break;
                }

                // Remove entry from free list
                ListEntry* entry = g_free_lists[order].head;
                g_free_lists[order].head = entry->next;

                split_entry(entry, order);

                --order;
            }
        }

        // Get first block of appropriate size
        ListEntry* entry = g_free_lists[order_to_alloc].head;

        // Remove block from free list
        g_free_lists[order_to_alloc].head = entry->next;

        // Toogle buddy bit to mark block as allocated
        toggle_buddy_bit(entry->addr, order_to_alloc);

        // Add allocation to allocation list
        {
            PhysicalAddress entry_addr = entry->addr;
            if (front == 0) {
                front = (PageFrameAllocation*)entry;
                back = front;
            }
            else {
                PageFrameAllocation* allocation = (PageFrameAllocation*)entry;
                back->next = allocation;
                back = allocation;
            }

            back->addr = entry_addr;
            back->order = order_to_alloc;
            back->next = 0;
        }

        size -= g_block_sizes[order_to_alloc];
    }

    return front;
}

void free_frames(PageFrameAllocation* allocation) {
    // Loop until all allocations have been freed
    while (allocation != 0) {
        uint8_t order = allocation->order;

        KERNEL_ASSERT(order < FRAME_ORDERS, "Not an order")

        // Convert allocation to entry
        ListEntry* entry = (ListEntry*)allocation;
        entry->addr = allocation->addr;

        // Get next allocation
        allocation = allocation->next;

        // Toogle buddy bit to mark block as freed
        toggle_buddy_bit(entry->addr, order);

        // Add entry back to free list
        entry->next = g_free_lists[order].head;
        g_free_lists[order].head = entry;

        // Merge blocks as far up as possible
        while (order < (FRAME_ORDERS - 1) && !get_buddy_bit(entry->addr, order)) {
            // Remove entry from free list
            g_free_lists[order].head = entry->next;

            uint64_t other_addr = entry->addr;
            if (entry->addr % g_block_sizes[order + 1] == 0) {
                other_addr += g_block_sizes[order];
            }
            else {
                other_addr -= g_block_sizes[order];
            }

            ListEntry* buddy_entry = g_free_lists[order].head;
            ListEntry* last_entry = 0;
            while (buddy_entry != 0 && buddy_entry->addr != other_addr) {
                last_entry = buddy_entry;
                buddy_entry = buddy_entry->next;
            }

            // The buddy entry should always be present or something has gone terribly wrong
            KERNEL_ASSERT(buddy_entry != 0, "Buddy entry not present")

            // Remove other entry from free list
            if (buddy_entry == g_free_lists[order].head) {
                g_free_lists[order].head = buddy_entry->next;
            }
            else {
                last_entry->next = buddy_entry->next;
            }

            // Swap entries if the buddy entry is on the left
            if (other_addr < entry->addr) {
                ListEntry* tmp = buddy_entry;
                buddy_entry = entry;
                entry = tmp;
            }

            free_memory_entry((MemoryEntry*)buddy_entry);

            // Put entry in free list
            entry->next = g_free_lists[order + 1].head;
            g_free_lists[order + 1].head = entry;

            // Toogle buddy bit to mark block as freed
            toggle_buddy_bit(entry->addr, order + 1);

            ++order;
        }
    }
}

bool alloc_frames_contiguos(uint64_t pages, PhysicalAddress* out_addr) {
    const uint8_t order_to_alloc = get_min_size_order(pages);
    KERNEL_ASSERT(order_to_alloc < FRAME_ORDERS, "Not an order")

    // Split bigger blocks if none of the correct size are available
    {
        int8_t order = order_to_alloc + 1;
        while (g_free_lists[order_to_alloc].head == 0) {
            if (g_free_lists[order].head == 0) {
                // Out of memory
                if (++order >= FRAME_ORDERS) return false;
            }

            // Remove entyr from free list
            ListEntry* entry = g_free_lists[order].head;
            g_free_lists[order].head = entry->next;

            split_entry(entry, order);

            --order;
        }
    }

    ListEntry* entry = g_free_lists[order_to_alloc].head;

    // Remove block from free list
    g_free_lists[order_to_alloc].head = entry->next;

    free_memory_entry((MemoryEntry*)entry);

    // Mark block as allocated
    toggle_buddy_bit(entry->addr, order_to_alloc);

    *out_addr = entry->addr;
    return true;
}

void free_frames_contiguos(PhysicalAddress addr, uint64_t pages) {
    PageFrameAllocation* allocation = (PageFrameAllocation*)get_memory_entry();
    allocation->addr = addr;
    allocation->order = get_min_size_order(pages);

    free_frames(allocation);
}

// Removes address ranges from free lists
bool remove_range(PhysicalAddress addr, uint64_t pages) {
    uint64_t size = pages * PAGE_SIZE;
    while (size != 0) {
        // Find largest order size which range fits into
        int32_t order_to_alloc = 1;
        while (order_to_alloc < FRAME_ORDERS) {
            // Address aligns with block size of order
            const bool addr_align = (addr % g_block_sizes[order_to_alloc]) == 0;

            // Size big enough for order
            const bool big_enough = size >= g_block_sizes[order_to_alloc];

            if (!addr_align || !big_enough) break;

            ++order_to_alloc;
        }
        --order_to_alloc;

        for (int32_t order = FRAME_ORDERS - 1; order >= 0; --order) {
            // Look for entry which contains addr in its range
            ListEntry* curr_entry = g_free_lists[order].head;
            ListEntry* last_entry = 0;
            while (curr_entry != 0 &&
                   !range_contains(addr, curr_entry->addr, g_block_sizes[order])) {
                last_entry = curr_entry;
                curr_entry = curr_entry->next;
            }

            // Entry for address not found in current order free list
            if (curr_entry == 0) {
                // Requested block not available
                if (order == order_to_alloc) return false;

                continue;
            }

            // Remove entry from free list
            if (curr_entry == g_free_lists[order].head) {
                g_free_lists[order].head = curr_entry->next;
            }
            else {
                last_entry->next = curr_entry->next;
            }

            // Remove entry from free list
            if (order == order_to_alloc) {
                // Toogle buddy bit to mark block as allocated
                toggle_buddy_bit(curr_entry->addr, order);

                free_memory_entry((MemoryEntry*)curr_entry);
                break;
            }
            // Split entry into two entries one order lower
            else {
                KERNEL_ASSERT(order != 0, "Can't split order 0 block")
                split_entry(curr_entry, order);
            }
        }

        size -= g_block_sizes[order_to_alloc];
        addr += g_block_sizes[order_to_alloc];
    }

    return true;
}

void alloc_frame_allocator_memory(void* uefi_memory_map, PhysicalAddress* phys_addr,
                                  uint64_t* total_pages, uint64_t* entry_pool_pages) {
    // Calculate block sizes
    g_block_sizes[0] = MIN_BLOCK_SIZE;
    for (uint64_t i = 1; i < FRAME_ORDERS; ++i) {
        g_block_sizes[i] = g_block_sizes[i - 1] * 2;
    }

    // Calculate size required by bitmaps
    uint64_t total_bitmaps_size = 0;
    {
        // List entries required at start by all max order blocks
        // We double the amount of free list entries required at the start
        const uint64_t free_list_entries =
            (get_memory_size() / g_block_sizes[FRAME_ORDERS - 1]) * 2;

        *entry_pool_pages =
            round_up_to_multiple(free_list_entries * sizeof(ListEntry), PAGE_SIZE) / PAGE_SIZE;

        // Calculate memory required by bitmaps
        for (int i = 0; i < (FRAME_ORDERS - 1); ++i) {
            total_bitmaps_size += get_memory_size() / g_block_sizes[i];
        }

        total_bitmaps_size = round_up_to_multiple(total_bitmaps_size, PAGE_SIZE) / PAGE_SIZE;
    }

    // The total pages to allocate for the entry pool and the bitmaps
    *total_pages = *entry_pool_pages + total_bitmaps_size;

    // Allocate memory for free lists and bitmaps
    *phys_addr = 0;
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

        if (desc->num_pages < *total_pages) continue;

        *phys_addr = desc->physical_start;

        desc->num_pages -= *total_pages;
        desc->physical_start += *total_pages * PAGE_SIZE;
        KERNEL_ASSERT(phys_addr != 0, "Not enough memory for frame allocator")
        return;
    }
}

void initialize_frame_allocator(VirtualAddress virt_addr, uint64_t total_pages,
                                void* uefi_memory_map, uint64_t entry_pool_pages) {
    _Static_assert(sizeof(ListEntry) == 16, "Size of ListEntry is not 16 bytes");
    _Static_assert(sizeof(PageFrameAllocation) == 16,
                   "Size of PageFrameAllocation is not 16 bytes");

    // Zero out memory used for allocator
    memset((void*)virt_addr, 0, total_pages * PAGE_SIZE);

    // Populate entry pool and buddy maps
    {
        fill_memory_entry_pool(virt_addr, entry_pool_pages);
        virt_addr += entry_pool_pages * PAGE_SIZE;

        // Populate buddy maps
        for (uint64_t i = 0; i < (FRAME_ORDERS - 1); ++i) {
            g_free_lists[i].buddy_map = (uint64_t*)virt_addr;
            virt_addr += get_memory_size() / g_block_sizes[i];
        }
    }

    // Populate free lists with max order sized blocks
    {
        PhysicalAddress curr_addr = 0;

        // Add first entry to free list
        ListEntry* curr_entry = (ListEntry*)get_memory_entry();
        curr_entry->addr = curr_addr;
        curr_addr += g_block_sizes[FRAME_ORDERS - 1];
        g_free_lists[FRAME_ORDERS - 1].head = curr_entry;

        const uint64_t block_count = get_memory_size() / g_block_sizes[FRAME_ORDERS - 1];
        for (uint64_t i = 1; i < block_count; ++i) {
            if (curr_addr >= get_memory_size()) break;

            ListEntry* next_entry = (ListEntry*)get_memory_entry();
            next_entry->addr = curr_addr;
            curr_addr += g_block_sizes[FRAME_ORDERS - 1];

            curr_entry->next = next_entry;
            curr_entry = next_entry;
        }
    }

    // Remove all unusable frames from allocator
    {
        const UEFIMemoryMap* memory_map = (UEFIMemoryMap*)uefi_memory_map;
        const UEFIMemoryDescriptor* last = 0;
        for (uint64_t i = 0; i < memory_map->buffer_size; i += memory_map->desc_size) {
            const UEFIMemoryDescriptor* desc = (UEFIMemoryDescriptor*)&memory_map->buffer[i];

            // Remove frames not present in the memory map
            {
                const PhysicalAddress addr = last->physical_start + last->num_pages * PAGE_SIZE;
                if (addr != desc->physical_start && last != 0) {
                    const uint64_t size = desc->physical_start - addr;

                    KERNEL_ASSERT((addr % PAGE_SIZE) == 0, "Address not page aligned")
                    KERNEL_ASSERT((size % PAGE_SIZE) == 0, "Size not page aligned")

                    const bool success = remove_range(addr, size / PAGE_SIZE);
                    KERNEL_ASSERT(success, "Failed to remove unusable frames from frame allocator")
                }
            }

            // Remove frames of specific memory types
            switch (desc->type) {
                case EfiBootServicesCode:
                case EfiRuntimeServicesCode:
                case EfiConventionalMemory: break;
                default: {
                    const bool success = remove_range(desc->physical_start, desc->num_pages);
                    KERNEL_ASSERT(success, "Failed to remove unusable frames from frame allocator")
                    break;
                }
            }

            last = desc;
        }
    }
}
