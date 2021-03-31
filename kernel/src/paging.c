#include "paging.h"

#include <stdbool.h>

// FLAGS TO BE APPLIED WHEN ALLOCATING NEW PAGETABLES
#define PAGETABLE_DEFAULT (PAGING_PRESENT | PAGING_WRITABLE)

#define GET_TABLE_PTR(page_entry) (PageEntry*)(page_entry & PAGING_PTRMASK)

PageEntry* get_cr3() {
    uint64_t* ptr;
    asm("mov %%cr3, %0" : "=r"(ptr));

    return ptr;
}

void set_cr3(PageEntry* P4) {
    asm("mov %0, %%cr3"
        : // no outputs
        : "r"(P4));
}

PageEntry page_zone_stack[PAGETABLE_SIZE] __attribute__((aligned(4096)));
PageEntry* page_zone_pointer = &page_zone_stack[0];

/*
    TEMPORARY PAGE ALLOCATION

    When heap allocation is implemented, we can actively check if pagetable contains similar
   children. If a pagetable can be remade as a single large page then all children will be freed and
   should be available for new page tables.
*/
PageEntry* alloc_pagetable() { return page_zone_pointer += 512; }

// correctly indexed list containing the masks for each page table layer
const PageFlags pagingmasks[5] = {
    (1L << 12) - (1L << 0),  // Offset
    (1L << 21) - (1L << 12), // P1: bits (21 - 12]
    (1L << 30) - (1L << 21), // P2: bits (30 - 21]
    (1L << 39) - (1L << 30), // P3: bits (39 - 30]
    (1L << 48) - (1L << 39)  // P4: bits (48 - 39]
};

PageEntry create_entry(uint64_t address, PageFlags flags) {
    return (address & PAGING_PTRMASK) | flags | PAGING_PRESENT;
}

// gets the table layer P's index of an address
uint64_t get_table(uint64_t virtaddr, uint8_t P) {
    return (virtaddr & pagingmasks[P]) >> (3 + 9 * P);
}

// splits a large page into a pagetable
// depth is the paging layer of the table entry
// returns the corrected pagetable entry
PageEntry subdivide_large_table(PageEntry page_entry, uint64_t depth) {
    const bool is_large = page_entry & PAGING_LARGE;
    if (!is_large) return page_entry;

    uint64_t table_addr = GET_PAGE_ADDRESS(page_entry);

    PageEntry* new_table = alloc_pagetable();
    uint64_t new_addr = (uint64_t)new_table;

    PageFlags flags = GET_PAGE_FLAGS(page_entry);

    // P1 entries can't be large
    if (depth > 2) flags &= ~PAGING_LARGE;

    uint64_t delta = 1UL << (3 + 9 * depth);

    // apply flags and update address in new page table
    for (int i = 0; i < 512; i++) {
        new_table[i] = create_entry(table_addr + i * delta, flags);
    }

    return create_entry(new_addr, PAGETABLE_DEFAULT);
}

// allocates a new table if needed and returns the entry pointing said table
PageEntry alloc_missing_table(PageEntry page_entry) {
    const bool is_present = page_entry & PAGING_PRESENT;
    if (is_present) return page_entry;

    const uint64_t alloc_addr = (uint64_t)alloc_pagetable();
    return create_entry(alloc_addr, PAGETABLE_DEFAULT);
}

typedef struct {
    uint64_t virtaddr_low;
    uint64_t virtaddr_high;
    uint64_t physoffset;
    PageFlags flags;
} MapParams;

// recursive helper function for map_pages
void rec_setpage(PageEntry* table, uint64_t table_addr, uint8_t depth, MapParams* params) {
    // align table address with virutal address
    // this is the same as finding the lower bound of a table
    if (table_addr < params->virtaddr_low) {
        table_addr += params->virtaddr_low & pagingmasks[depth];
    }

    uint64_t delta = 1L << (3 + 9 * depth);

    // go through all relevant page entries
    int low = get_table(table_addr, depth);
    while (low < 512 && table_addr <= params->virtaddr_high) {
        const uint64_t physaddr = table_addr + params->physoffset;

        const bool filled_page = (table_addr + delta) <= params->virtaddr_high;
        const bool is_large = table[low] & PAGING_LARGE;
        const bool is_present = table[low] & PAGING_PRESENT;
        const bool P1 = depth <= 1, P2 = depth == 2;

        if (P1) goto set_mapping;

        if (P2 && filled_page) {
            if (is_large || !is_present) goto set_mapping;

            // entry points to an already subdivided table
            goto iterate_table;
        }

        // table entry must be split into subsegments
        table[low] = alloc_missing_table(table[low]);
        table[low] = subdivide_large_table(table[low], depth);

    iterate_table : // go through the next layers page entries
    {
        PageEntry* next_table = GET_TABLE_PTR(table[low]);
        rec_setpage(next_table, table_addr, depth - 1, params);

        goto next;
    }

    set_mapping : // maps pagetable adresses and goes to the next iteration
    {
        PageFlags applied_flags = params->flags & ~PAGING_PTRMASK;
        if (!P1) applied_flags |= PAGING_LARGE;

        table[low] = create_entry(physaddr, applied_flags);
    }

    next: // incremenet current page
        table_addr += delta;
        low++;
    }
}

void map_pages(PageEntry* table, uint64_t low, uint64_t high, uint64_t phys_low, PageFlags flags) {
    MapParams params = {
        low & PAGING_PTRMASK, high & PAGING_PTRMASK, (phys_low - low) & PAGING_PTRMASK, flags};

    rec_setpage(table, 0, 4, &params);
}

void identity_map_pages(PageEntry* table, uint64_t low, uint64_t high, PageFlags flags) {
    map_pages(table, low, high, low, flags);
}

PageEntry* fetch_page_entry(PageEntry* table, uint64_t virtaddr) {
    for (int i = 4; i > 0; --i) {
        // get relative table address
        const int P = get_table(virtaddr, i);
        const bool is_large = table[P] & PAGING_LARGE;

        // return when a page is reached
        if (i == 1 || is_large) return &table[P];

        // check for missing page
        const bool is_present = table[P] & PAGING_PRESENT;
        if (!is_present) return 0;

        table = GET_TABLE_PTR(table[P]);
    }

    return 0;
}
