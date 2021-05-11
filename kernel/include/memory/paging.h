#pragma once
#include "memory/defs.h"
#include "memory/frame_allocator.h"

#define PAGING_WRITABLE 1
#define PAGING_EXECUTABLE 2
#define PAGING_CACHE_DISABLE 4
#define PAGING_WRITE_THROUGH 8

#define SIGN_EXT_ADDR(addr) ((addr) | (((((addr) >> 47) & 1) * 0xffffULL) << 48))

typedef uint32_t PagingFlags;

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

typedef struct {
    VirtualAddress current_address;

    uint16_t pdp_index;
    PageEntry* pdp;

    uint8_t prot : 4;

    FreeListEntry* free_list;

    // Holds physical to virtual mapping of page table entries
    MappingEntry* entry_maps[2];
} AddressSpace;

// Fills in the AddressSpace struct, checks for sane values and allocates memory for PDP
void new_address_space(AddressSpace* space, uint16_t pdp_index, uint8_t prot);

// Frees page entries, lists and PDP
void delete_address_space(AddressSpace* space);

// Maps address space
void map_address_space(AddressSpace* space);

// Unmaps address space
void unmap_address_space(AddressSpace* space);

// Maps discrete allocations into contiguos virtual address space
VirtualAddress map_allocation(AddressSpace* space, PageFrameAllocation* allocation,
                              PagingFlags flags);

// Maps the physical address range to available virtual address range
VirtualAddress map_phys_range(AddressSpace* space, PhysicalAddress phys_addr, uint64_t pages,
                              PagingFlags flags);

// Maps the physical address range to specified virtual address range if available
// Returns false if address space isn't available or out of range
bool map_to_range(AddressSpace* space, PhysicalAddress phys_addr, VirtualAddress virt_addr,
                  uint64_t pages, PagingFlags flags);

// Unmaps virtual address range
void unmap_range(AddressSpace* space, VirtualAddress virt_addr, uint64_t pages);

// Unmaps virtual address range and frees previously mapped frames
void unmap_and_free_frames(AddressSpace* space, VirtualAddress virt_addr, uint64_t pages);

// Translates a virtual address into the corresponding physical address
// Returns false if the virtual address isn't mapped
bool virt_to_phys_addr(AddressSpace* space, VirtualAddress virt_addr, PhysicalAddress* phys_addr);

VirtualAddress kmap_allocation(PageFrameAllocation* allocation, PagingFlags flags);

VirtualAddress kmap_phys_range(PhysicalAddress phys_addr, uint64_t pages, PagingFlags flags);

void kunmap_range(VirtualAddress virt_addr, uint64_t pages);

void kunmap_and_free_frames(VirtualAddress virt_addr, uint64_t pages);

bool kvirt_to_phys_addr(VirtualAddress virt_addr, PhysicalAddress* phys_addr);

void free_uefi_memory_and_remove_identity_mapping(void* uefi_memory_map);

VirtualAddress initialize_paging(void* uefi_memory_map, PhysicalAddress kernel_phys_addr,
                                 uint64_t kernel_size);
