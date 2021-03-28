#include "paging.h"

#define create_table_entry(addr) \
    (((uint64_t)addr & PAGING_PTRMASK) | PAGETABLE_DEFAULT | PAGING_PRESENT)

uint64_t* get_cr3() {
    uint64_t* ptr;
    asm("mov %%cr3, %0" : "=r"(ptr));

    return ptr;
}

void set_cr3(uint64_t* P4) {
    asm("mov %0, %%cr3"
        : // no outputs
        : "r"(P4));
}

uint64_t page_zone_stack[PAGETABLE_SIZE] __attribute__((aligned(4096)));
uint64_t* page_zone_pointer = &page_zone_stack[0];

/*
    TEMPORARY PAGE ALLOCATION

    When heap allocation is implemented, we can actively check if pagetable contains similar
   children. If a pagetable can be remade as a single large page then all children will be freed and
   should be available for new page tables.
*/
uint64_t* alloc_pagetable() { return page_zone_pointer += 512; }

// correctly indexed list containing the masks for each page table layer
const uint64_t pagingmasks[5] = {
    (1L << 12) - (1L << 0),  // Offset
    (1L << 21) - (1L << 12), // P1: bits (21 - 12]
    (1L << 30) - (1L << 21), // P2: bits (30 - 21]
    (1L << 39) - (1L << 30), // P3: bits (39 - 30]
    (1L << 48) - (1L << 39)  // P4: bits (48 - 39]
};

// gets the table layer P's index of an address
uint64_t get_table(uint64_t physaddr, uint8_t P) {
    return (physaddr & pagingmasks[P]) >> (3 + 9 * P);
}

// splits a large page into a pagetable
// depth is the paging layer of the table entry
// returns the corrected pagetable entry
uint64_t subdivide_large_table(uint64_t table_entry, uint64_t depth) {
    const bool large = table_entry & PAGING_LARGE;
    if (!large) return table_entry;

    uint64_t current_addr = table_entry & PAGING_PTRMASK;
    uint64_t* new_addr = alloc_pagetable();
    uint64_t flags = (table_entry & ~PAGING_PTRMASK);

    // P1 entries can't be large
    if (depth > 2) flags &= ~PAGING_LARGE;

    uint64_t delta = 1UL << (3 + 9 * depth);

    for (int i = 0; i < 512; i++) {
        // apply flags and update address in new page table
        new_addr[i] = current_addr + i * delta + PAGETABLE_DEFAULT;
    }

    return create_table_entry(new_addr);
}

// allocates a new table if needed and returns the entry pointing said table
uint64_t alloc_missing_table(uint64_t table_entry) {
    if (table_entry & PAGING_PRESENT) {
        return table_entry;
    }

    return create_table_entry(alloc_pagetable());
}

typedef struct {
    uint64_t virtaddr_low;
    uint64_t virtaddr_high;
    uint64_t physoffset;
    uint64_t flags;
} MapParams;

// recursive helper function for map_pages
void rec_setpage(uint64_t* table, uint64_t tableaddr, uint8_t depth, MapParams* params) {
    // align table address with virutal address
    // this is the same as finding the lower bound of a table
    if (tableaddr < params->virtaddr_low) {
        tableaddr += params->virtaddr_low & pagingmasks[depth];
    }

    uint64_t delta = 1L << (3 + 9 * depth);

    // go through all relevant tables
    int low = get_table(tableaddr, depth);
    while (low < 512 && tableaddr <= params->virtaddr_high) {
        const uint64_t physaddr = tableaddr + params->physoffset;

        const bool filled_page = (tableaddr + delta) <= params->virtaddr_high;
        const bool is_large = table[low] & PAGING_LARGE;
        const bool is_present = table[low] & PAGING_PRESENT;
        const bool P1 = depth <= 1, P2 = depth == 2;

        uint64_t applied_flags = params->flags & ~PAGING_PTRMASK;

        // set_value maps pagetable adresses and goes to the next iteration
        if (P1) goto set_value;

        if (P2 && filled_page) {
            if (is_large || !is_present) goto set_value;

            // already subdivided
            rec_setpage((uint64_t*)(table[low] & PAGING_PTRMASK), tableaddr, depth - 1, params);
            goto next;
        }

        // table entry must be split into subsegments
        table[low] = alloc_missing_table(table[low]);
        table[low] = subdivide_large_table(table[low], depth);

        rec_setpage((uint64_t*)(table[low] & PAGING_PTRMASK), tableaddr, depth - 1, params);
        goto next;

    set_value:

        if (!P1) applied_flags |= PAGING_LARGE;

        table[low] = (physaddr & PAGING_PTRMASK) | applied_flags;

    next:
        // incremenet current page
        tableaddr += delta;
        low++;
    }
}

void map_pages(uint64_t* table, uint64_t low, uint64_t high, uint64_t phys_low, uint64_t flags) {
    MapParams params = {
        low & PAGING_PTRMASK, high & PAGING_PTRMASK, (phys_low - low) & PAGING_PTRMASK, flags};

    rec_setpage(table, 0, 4, &params);
}

void identity_map_pages(uint64_t* table, uint64_t low, uint64_t high, uint64_t flags) {
    map_pages(table, low, high, low, flags);
}

uint64_t* fetch_page(uint64_t* table, uint64_t virtaddr) {
    for (int i = 4; i > 0; --i) {
        // get relative table address
        const int P = get_table(virtaddr, i);
        const bool large = table[P] & PAGING_LARGE;

        // return when a page is reached
        if (i == 1 || large) return &table[P];

        // check for missing page
        const bool present = table[P] & PAGING_PRESENT;
        if (!present) return 0;

        table = (uint64_t*)(table[P] & PAGING_PTRMASK);
    }

    return 0;
}
