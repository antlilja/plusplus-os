#include "elf-loader.h"

#include <efi.h>

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

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} __attribute__((packed)) Elf64_Phdr;

EFI_STATUS load_kernel(EFI_SYSTEM_TABLE* st, EFI_FILE* root, CHAR16* filename,
                       EFI_PHYSICAL_ADDRESS* entry_point) {
    EFI_STATUS status;

    // Open kernel elf file
    EFI_FILE* file;
    status = root->Open(root, &file, filename, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
    if (EFI_ERROR(status)) return status;

    // Set position in file to the beginning
    status = file->SetPosition(file, 0);
    if (EFI_ERROR(status)) return status;

    // Allocate memory for the file header
    UINTN len = sizeof(Elf64_Ehdr);
    Elf64_Ehdr* file_header;
    status = st->BootServices->AllocatePool(EfiLoaderData, len, (void**)&file_header);
    if (EFI_ERROR(status)) return status;

    // Read into file header struct
    status = file->Read(file, &len, file_header);
    if (EFI_ERROR(status)) return status;

    // Validate ELF identity
    {
        // Validate that the magic numbers are correct
        if (file_header->e_ident[EI_MAG0] != L'\x7f' || file_header->e_ident[EI_MAG1] != L'E' ||
            file_header->e_ident[EI_MAG2] != L'L' || file_header->e_ident[EI_MAG3] != L'F') {
            return EFI_UNSUPPORTED;
        }

        // Make sure it's a 64 bit ELF file
        if (file_header->e_ident[EI_CLASS] != ELFCLASS64) return EFI_UNSUPPORTED;

        // Make sure file is in big-endian format
        if (file_header->e_ident[EI_DATA] == ELFDATA2MSB) return EFI_UNSUPPORTED;
    }

    // Allocate memory for the program header
    Elf64_Phdr* program_headers;
    len = sizeof(Elf64_Phdr) * file_header->e_phnum;
    status = st->BootServices->AllocatePool(EfiLoaderData, len, (void**)&program_headers);
    if (EFI_ERROR(status)) return status;

    // Set file position to the program header offset
    status = file->SetPosition(file, file_header->e_phoff);
    if (EFI_ERROR(status)) return status;

    // Read into program header struct
    status = file->Read(file, &len, (void*)program_headers);
    if (EFI_ERROR(status)) return status;

    // Get number of required kernel pages and the entry offset
    UINT64 num_kernel_pages = 0;
    UINT64 entry_point_offset = 0;
    UINT64 low_virt_addr = 0xffffffffffffffffULL;
    {
        UINT64 high_virt_addr = 0;
        UINTN loaded_segments = 0;
        for (UINTN p = 0; p < file_header->e_phnum; ++p) {
            Elf64_Phdr* header = &program_headers[p];
            if (header->p_type == PT_LOAD) {
                if (header->p_vaddr < low_virt_addr) low_virt_addr = header->p_vaddr;
                if (header->p_vaddr > high_virt_addr)
                    high_virt_addr = header->p_vaddr + header->p_memsz;
                ++loaded_segments;
            }
        }

        // If no segments were loaded then there was no executable code
        if (loaded_segments == 0) return EFI_NOT_FOUND;

        entry_point_offset = file_header->e_entry - low_virt_addr;

        num_kernel_pages = EFI_SIZE_TO_PAGES(high_virt_addr - low_virt_addr);
    }

    // Allocate kernel pages
    EFI_PHYSICAL_ADDRESS kernel_base_addr = 0;
    status = st->BootServices->AllocatePages(
        AllocateAnyPages, EfiLoaderCode, num_kernel_pages, &kernel_base_addr);
    if (EFI_ERROR(status)) return status;

    // Set kernel entry point
    *entry_point = kernel_base_addr + entry_point_offset;

    // Read PT_LOAD segments into memory
    for (UINTN p = 0; p < file_header->e_phnum; ++p) {
        Elf64_Phdr* header = &program_headers[p];
        if (header->p_type == PT_LOAD) {
            EFI_PHYSICAL_ADDRESS physical_addr = kernel_base_addr + header->p_vaddr - low_virt_addr;

            // File size and memory size can be different
            if (header->p_filesz > 0) {
                // Set file position to the segment offset
                status = file->SetPosition(file, header->p_offset);
                if (EFI_ERROR(status)) return status;

                // Read segment into memory
                len = header->p_filesz;
                status = file->Read(file, &len, (void*)physical_addr);
                if (EFI_ERROR(status)) return status;
            }

            // Zero fill the remaining memory in the pages according to ELF spec
            st->BootServices->SetMem((void*)(physical_addr + header->p_filesz),
                                     (UINTN)(header->p_memsz - header->p_filesz),
                                     0);
        }
    }

    // Free file header and program headers
    status = st->BootServices->FreePool(program_headers);
    if (EFI_ERROR(status)) return status;
    status = st->BootServices->FreePool(file_header);
    if (EFI_ERROR(status)) return status;

    // Close kernel ELF file
    status = file->Close(file);
    if (EFI_ERROR(status)) return status;

    return EFI_SUCCESS;
}
