#include "elf_loader.h"

#include "memory/frame_allocator.h"
#include "memory/paging.h"
#include "util.h"
#include "memory.h"

#include <string.h>

#define EI_NIDENT 16

#define ELFCLASS64 2
#define ELFDATA2MSB 2

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5

#define PT_LOAD 1

#define PF_X 1
#define PF_W 2

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64Phdr;

bool load_elf_from_buff(AddressSpace* addr_space, const void* data, void** entry_point) {
    const Elf64Ehdr* file_header = (const Elf64Ehdr*)data;

    // Validate ELF file
    {
        // Validate that the magic numbers are correct
        if (file_header->e_ident[EI_MAG0] != L'\x7f' || file_header->e_ident[EI_MAG1] != L'E' ||
            file_header->e_ident[EI_MAG2] != L'L' || file_header->e_ident[EI_MAG3] != L'F') {
            return false;
        }

        // Make sure it's a 64 bit ELF file
        if (file_header->e_ident[EI_CLASS] != ELFCLASS64) return false;

        // Make sure file is in big-endian format
        if (file_header->e_ident[EI_DATA] == ELFDATA2MSB) return false;
    }

    const Elf64Phdr* program_headers = (const Elf64Phdr*)(data + file_header->e_phoff);

    for (uint64_t p = 0; p < file_header->e_phnum; ++p) {
        const Elf64Phdr* header = &program_headers[p];
        if (header->p_type == PT_LOAD) {

            // Make sure segments are 4K aligned
            if ((header->p_vaddr % PAGE_SIZE) != 0) return false;

            // Allocate physical memory
            const uint64_t pages = (header->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
            PageFrameAllocation* allocation = alloc_frames(pages);
            if (allocation == 0) return false;

            // Calculate paging flag values from program header
            const PagingFlags flags = ((header->p_flags & PF_X) != 0 ? PAGING_EXECUTABLE : 0) |
                                      ((header->p_flags & PF_W) != 0 ? PAGING_WRITABLE : 0);

            // Map memory to address specified by program header
            {
                const bool success =
                    map_allocation_to_range(addr_space, allocation, header->p_vaddr, flags);
                free_frame_allocation_entries(allocation);
                if (success == false) return false;
            }

            // Clear allocated memory acording to ELF spec
            memset((void*)header->p_vaddr, 0, pages * PAGE_SIZE);

            // Copy over segment data to allocated memory
            memcpy((void*)header->p_vaddr, data + header->p_offset, header->p_filesz);
        }
    }

    *entry_point = (void*)file_header->e_entry;
    return true;
}
