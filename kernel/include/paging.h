// https://wiki.osdev.org/Paging         x86 2 layer paging
// https://os.phil-opp.com/page-tables/  x64 4 layer paging
// P4 P3 P2 P1 represent the different pagetable layers

/*
    This is currently set up to provide convenient way of creating pagetables, 
    mapping and contstraining memory usage, which is a necessity for processes. 

    Under the assumption that each process use their own pagetables, which
    is described here:
    
    https://forum.osdev.org/viewtopic.php?f=1&t=32996
    https://en.wikipedia.org/wiki/Context_switch#Cost

    Then setting up a new page table could look like this:

        PageEntry* new_p4 = create_pagetable();
        map_pages(new_p4, 0, program_size, program_start, PAGING_WRITEABLE | PAGING_USERSPACE | ...);
        map_pages(new_p4, data_start, data_end, program_start + data_start, PAGING_USERSPACE | ...);

    ... go through each section and set flags ...

    then when the process is swapped, you can get or set the table used by the MMU

        set_cr3(new_p4);

    when the process is killed, its pagetable can be freed
    
        free_pagetable(new_p4, 4);
*/

//uefi pagetables are placed in a nonreadable section by default

#pragma once
#include <stdint.h>

typedef uint64_t PageFlags;
typedef uint64_t PageEntry;

#define GET_PAGE_FLAGS(page_entry) ((page_entry) & ~PAGING_PTRMASK)
#define GET_PAGE_ADDRESS(page_entry) ((page_entry) & PAGING_PTRMASK)

#define PAGING_PRESENT      (1UL << 0)
#define PAGING_WRITABLE     (1UL << 1)
#define PAGING_USERSPACE    (1UL << 2)

#define PAGING_WRITETHROUGH (1UL << 3)
#define PAGING_NOCACHE      (1UL << 4)
#define PAGING_ACCESSED     (1UL << 5)
#define PAGING_DIRTY        (1UL << 6)

#define PAGING_LARGE        (1UL << 7)
#define PAGING_GLOBAL       (1UL << 8)
#define PAGING_DIR_NONEXEC  (1UL << 63)

#define PAGING_PTRMASK ((1UL << 48) - (1UL << 12))    //bits 48 - 12

// returns the P4 pointer
PageEntry* get_cr3();

// sets the P4 pointer
void set_cr3(PageEntry* P4);

//maps all virtual addresses between virtaddr_low and virtaddr_high relatve to phys_low and makes sure they're in the pagetable
//
// ex:
// map_pages(P4, 0x4000, 0x8000, 0x0000, PAGETABLE_DEFAULT)
// will map all virtual addresses 0x4000 - 0x8000 to 0x0000 - 0x4000 using the flags in
// PAGETABLE_DEFAULT
void map_pages(PageEntry* table, uint64_t virtaddr_low, uint64_t virtaddr_high, uint64_t phys_low,
               uint64_t flags);

// identity maps any address between virtaddr_low and virtaddr_high and makes sure they're in the
// pagetable
void identity_map_pages(PageEntry* table, uint64_t virtaddr_low, uint64_t virtaddr_high,
                        uint64_t flags);

// gets a pointer to the pagetable entry that maps virtaddr to physical space
// returns 0 if virtaddr is unpaged
PageEntry* fetch_page_entry(PageEntry* table, uint64_t virtaddr);

// creates a new page table containing 512 entries
// note that the same pagetable structure is used for all depths
PageEntry* create_pagetable();

// frees a pagetable of certain depth layer
// as example, the P4 table has a depth of 4
void free_pagetable(PageEntry* table, uint64_t depth);
