//https://wiki.osdev.org/Paging         x86 2 layer paging
//https://os.phil-opp.com/page-tables/  x64 4 layer paging
//P4 P3 P2 P1 represent the different pagetable layers


//uefi pagetables are placed in a nonreadable section by default

#pragma once
#include <stdint.h>
#include <stdbool.h>

#define PAGETABLE_SIZE 0x100000

//temporary solution before memory allocation is done
extern uint64_t page_zone_stack[PAGETABLE_SIZE];
extern uint64_t* page_zone_pointer;

#define PAGING_PRESENT    (1L << 0)
#define PAGING_WRITABLE   (1L << 1)
#define PAGING_USERSPACE  (1L << 2)

#define PAGING_WRITETHROUGH (1L << 3)
#define PAGING_NOCACHE      (1L << 4)
#define PAGING_ACCESSED     (1L << 5)
#define PAGING_DIRTY        (1L << 6)

#define PAGING_LARGE        (1L << 7)
#define PAGING_GLOBAL       (1L << 8)
#define PAGING_DIR_NONEXEC  (1L << 63)

#define PAGING_PTRMASK ((1L << 48) - (1L << 12))    //bits 48 - 12

//FLAGS TO BE APPLIED WHEN ALLOCATING NEW PAGETABLES
#define PAGETABLE_DEFAULT (PAGING_PRESENT | PAGING_WRITABLE)

//returns the P4 pointer
uint64_t* get_cr3();

//sets the P4 pointer
void set_cr3(uint64_t* P4);


//maps all virtual adidresses between virtaddr_low and virtaddr_high relatve to phys_low and makes sure they're in the pagetable
//
//ex:
//map_pages(P4, 0x4000, 0x8000, 0x0000, PAGETABLE_DEFAULT)
//will map all virtual addresses 0x4000 - 0x8000 to 0x0000 - 0x4000 using the flags in PAGETABLE_DEFAULT
void map_pages(uint64_t* table, uint64_t virtaddr_low, uint64_t virtaddr_high, uint64_t phys_low, uint64_t flags);

//identity maps any address between virtaddr_low and virtaddr_high and makes sure they're in the pagetable
//all pages will have PAGETABLE_DEFAULT set 
void identity_map_pages(uint64_t* table, uint64_t virtaddr_low, uint64_t virtaddr_high, uint64_t flags);

//gets a pointer to the pagetable entry that maps virtaddr to physical space
//returns 0 if virtaddr is unpaged
uint64_t* fetch_page(uint64_t* table, uint64_t virtaddr);