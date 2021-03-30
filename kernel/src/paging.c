#include "paging.h"

// FLAGS TO BE APPLIED WHEN ALLOCATING NEW PAGETABLES
#define PAGETABLE_DEFAULT (PAGING_PRESENT | PAGING_WRITABLE)

#define GET_TABLE_PTR(page_entry) (PageEntry*)(page_entry & PAGING_PTRMASK)
#define GET_LAYER_PAGESIZE(depth) (1UL << (3 + 9 * depth))

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

/*
    LESS TEMPORARY PAGE ALLOCATION

    When an entire 2MB pagetable is mapped/flagged the same, it will be replaced by a large page and
   its content will be free to use for new tables. This is a replacement to freeing and allocating
   sections manually, limiting paging to the process of mapping and constraining memory usage.

   With 8 GB of memory, the theoretical MAXIMAL gain of space from manual page allocation is 32 KB.
   Instead, the next step would be to place superflous pagetables on disk, as described here:
   https://en.wikipedia.org/wiki/Page_table#Role_of_the_page_table
*/
PageEntry page_zone_stack[PAGETABLE_SIZE] __attribute__((aligned(4096)));
PageEntry* page_zone_pointer = &page_zone_stack[0];

// hopefully, less than a quarter of the page stack should only be free at once
#define FREED_TABLE_SIZE (PAGETABLE_SIZE / 512 / 4)
PageEntry* free_tables_stack[FREED_TABLE_SIZE];
uint64_t free_tables_count;

// leaks occur if too many tables are freed in total, this will count how many are leaked.
// can be used for diagnostics later on.
uint64_t leaked_tables = 0;

PageEntry* alloc_pagetable() {
    if (free_tables_count > 0) {
        PageEntry* table = free_tables_stack[free_tables_count];

        // clear contents of freed table
        // REPLACE WITH MEMZERO WHEN AVALIABLE
        for (int i = 0; i < 512; i++) table[i] = 0UL;

        free_tables_count--;
        return table;
    }

    return page_zone_pointer += 512;
}

// frees the table a page entry points to
void free_pagetable(PageEntry page_entry) {
    if (free_tables_count >= FREED_TABLE_SIZE) {
        leaked_tables++;

        return;
    }

    free_tables_stack[free_tables_count] = GET_TABLE_PTR(page_entry);
    free_tables_count++;
}

// correctly indexed list containing the masks for each page table layer
const PageFlags pagingmasks[5] = {
    (1L << 12) - (1L << 0),  // Offset
    (1L << 21) - (1L << 12), // P1: bits (21 - 12]
    (1L << 30) - (1L << 21), // P2: bits (30 - 21]
    (1L << 39) - (1L << 30), // P3: bits (39 - 30]
    (1L << 48) - (1L << 39)  // P4: bits (48 - 39]
};

// returns if a pagetable entry can be expressed as a large page, rather than a table
bool check_table_redundancy(PageEntry page_entry, uint64_t page_depth) {
    const bool is_large = page_entry & PAGING_LARGE;

    if (is_large) return false;

    const PageEntry* table = GET_TABLE_PTR(page_entry);
    const PageFlags expected_flags = GET_PAGE_FLAGS(table[0]);

    // get the tables pages size
    const uint64_t delta = GET_LAYER_PAGESIZE(page_depth - 1);

    uint64_t expected_address = GET_PAGE_ADDRESS(table[0]);

    // check each page for similar mapping and flags
    for (int i = 1; i < 512; i++) {
        expected_address += delta;

        const uint64_t address = GET_PAGE_ADDRESS(table[i]);
        if (address != expected_address) return false;

        const PageFlags flags = GET_PAGE_FLAGS(table[i]);
        if (flags != expected_flags) return false;
    }

    return true;
}

PageEntry create_entry(uint64_t address, PageFlags flags) {
    return (address & PAGING_PTRMASK) | flags | PAGING_PRESENT;
}

// gets the table layer P's index of an address
uint64_t get_table_index(uint64_t virtaddr, uint8_t P) {
    return (virtaddr & pagingmasks[P]) >> (3 + 9 * P);
}

// splits a large page into a pagetable.
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

    uint64_t delta = GET_LAYER_PAGESIZE(depth);

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

    uint64_t delta = GET_LAYER_PAGESIZE(depth);

    // go through all relevant page entries
    int low = get_table_index(table_addr, depth);
    while (low < 512 && table_addr <= params->virtaddr_high) {
        const uint64_t physaddr = table_addr + params->physoffset;

        // determines if all pagetable entries are covered.
        const bool filled_page =
            table_addr >= params->virtaddr_low && table_addr + delta <= params->virtaddr_high;

        const bool is_large = table[low] & PAGING_LARGE;
        const bool is_present = table[low] & PAGING_PRESENT;
        const bool P1 = depth <= 1, P2 = depth == 2;

        if (P1) goto set_mapping;

        if (P2 && filled_page) {
            if (is_large || !is_present) goto set_mapping;

            // entry points to an already subdivided table
            // since all children will be mapped the same and share the same flags,
            // it's instead freed and remade into a large page
            goto collect_table_pages;
        }

        // table entry must be split into subsegments
        table[low] = alloc_missing_table(table[low]);
        table[low] = subdivide_large_table(table[low], depth);

    iterate_table : // go through the next layers page entries
    {
        PageEntry* next_table = GET_TABLE_PTR(table[low]);
        rec_setpage(next_table, table_addr, depth - 1, params);

        // free pagetables that could be represented as large pages.
        if (!is_large && P2 && check_table_redundancy(table[low], depth)) {
            goto collect_table_pages;
        }

        goto next;
    }

    collect_table_pages : // frees a pagetable and turns it into a large page
    {
        free_pagetable(table[low]);
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
        const int P = get_table_index(virtaddr, i);
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
