#include "elf-loader.h"
#include "efidef.h"

#include <efi.h>
#include <stdbool.h>

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
#define PT_DYNAMIC 2

#define PF_X 1

#define DT_NULL 0
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_GNU_HASH 0x6ffffef5

typedef struct Elf64_Ehdr {
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

typedef struct Elf64_Phdr {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} __attribute__((packed)) Elf64_Phdr;

typedef struct Elf64_Dyn {
    INT64 d_tag;
    UINT64 d_val_or_addr;
} __attribute__((packed)) Elf64_Dyn;

typedef struct Elf64_Sym {
    UINT32 st_name;
    UINT8 st_info;
    UINT8 st_other;
    UINT16 st_shndx;
    UINT64 st_value;
    UINT64 st_size;
} __attribute__((packed)) Elf64_Sym;

UINT32 gnu_hash(const char* name) {
    UINT32 hash = 5381;
    for (; *name; name++) {
        hash = (hash << 5) + hash + *name;
    }
    return hash;
}

INT32 strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        ++a;
        ++b;
    }
    return (*(UINT8*)a) - (*(UINT8*)b);
}

EFI_STATUS load_kernel(EFI_SYSTEM_TABLE* st, EFI_FILE* root, CHAR16* filename,
                       EFI_PHYSICAL_ADDRESS* entry_point, EFI_PHYSICAL_ADDRESS* stack) {
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

    // Set entry point
    *entry_point = (EFI_PHYSICAL_ADDRESS)file_header->e_entry;

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

    // Read PT_LOAD and PT_DYNAMIC segments into memory
    Elf64_Dyn* dynamic_table = NULL;
    UINTN loaded_segments = 0;
    for (UINTN p = 0; p < file_header->e_phnum; ++p) {
        Elf64_Phdr* header = &program_headers[p];
        if (header->p_type == PT_LOAD) {
            EFI_MEMORY_TYPE mem_type = EfiLoaderData;
            if (header->p_flags & PF_X) mem_type = EfiLoaderCode;

            // Allocate pages for segment
            status = st->BootServices->AllocatePages(AllocateAnyPages,
                                                     mem_type,
                                                     EFI_SIZE_TO_PAGES(header->p_memsz),
                                                     (EFI_PHYSICAL_ADDRESS*)header->p_vaddr);
            if (EFI_ERROR(status)) return status;

            // File size and memory size can be different
            if (header->p_filesz > 0) {
                // Set file position to the segment offset
                status = file->SetPosition(file, header->p_offset);
                if (EFI_ERROR(status)) return status;

                // Read segment into memory
                len = header->p_filesz;
                status = file->Read(file, &len, (void*)header->p_vaddr);
                if (EFI_ERROR(status)) return status;
            }

            // Zero fill the remaining memory in the pages according to ELF spec
            st->BootServices->SetMem((void*)(header->p_vaddr + header->p_filesz),
                                     (UINTN)(header->p_memsz - header->p_filesz),
                                     0);
            ++loaded_segments;
        }
        // Load dynamic table
        else if (header->p_type == PT_DYNAMIC) {
            // Set file position to the offset
            status = file->SetPosition(file, header->p_offset);
            if (EFI_ERROR(status)) return status;

            // Allocate memory for dynamic table
            status = st->BootServices->AllocatePool(
                EfiLoaderData, header->p_memsz, (void**)&dynamic_table);
            if (EFI_ERROR(status)) return status;

            // Read dynamic table into memory
            len = header->p_memsz;
            status = file->Read(file, &len, (void*)dynamic_table);
            if (EFI_ERROR(status)) return status;
        }
    }

    // If no segments were loaded then there was no executable code
    if (loaded_segments == 0) return EFI_NOT_FOUND;

    // No dynamic table exists
    if (dynamic_table == NULL) return EFI_NOT_FOUND;

    // Get address to the top of stack from the symbol table
    {
        *stack = 0;
        UINT32* hashtab = NULL;
        char* strtab = NULL;
        Elf64_Sym* symtab = NULL;
        bool hash_is_gnu = false;

        // Loop over dynamic table
        for (int i = 0; dynamic_table[i].d_tag != DT_NULL; ++i) {
            switch (dynamic_table[i].d_tag) {
                case DT_STRTAB: {
                    strtab = (char*)dynamic_table[i].d_val_or_addr;
                    break;
                }
                case DT_SYMTAB: {
                    symtab = (Elf64_Sym*)dynamic_table[i].d_val_or_addr;
                    break;
                }
                case DT_GNU_HASH: {
                    hashtab = (UINT32*)dynamic_table[i].d_val_or_addr;
                    hash_is_gnu = true;
                    break;
                }
                case DT_HASH: {
                    if (!hash_is_gnu) hashtab = (UINT32*)dynamic_table[i].d_val_or_addr;
                    break;
                }
            }
        }

        if (strtab == NULL || symtab == NULL || hashtab == NULL) return EFI_NOT_FOUND;

        // There are two types of hash tables used by ELF binaries.
        if (hash_is_gnu) {
            const UINT32 bucket_count = hashtab[0];
            const UINT32 symbol_offset = hashtab[1];
            const UINT32 bloom_size = hashtab[2];
            const UINT32 bloom_shift = hashtab[3];
            const UINT64* bloom_filter = (UINT64*)&hashtab[4];
            const UINT32* buckets = (UINT32*)&bloom_filter[bloom_size];
            const UINT32* chain = &buckets[bucket_count];

            const UINT32 hash = gnu_hash("__stack_top__");

            const UINT64 word = bloom_filter[(hash / 64) % bloom_size];
            const UINT64 mask = (UINT64)1 << (hash % 64) | (UINT64)1
                                                               << ((hash >> bloom_shift) % 64);

            // Stack was not found
            if ((word & mask) != mask) return EFI_NOT_FOUND;

            UINT32 symbol_index = buckets[hash % bucket_count];
            if (symbol_index < symbol_offset) return EFI_NOT_FOUND;

            while (true) {
                const char* symbol_name = strtab + symtab[symbol_index].st_name;
                const UINT32 symbol_hash = chain[symbol_index - symbol_offset];

                if ((hash | 1) == (symbol_hash | 1) && strcmp(symbol_name, "__stack_top__") == 0) {
                    *stack = (EFI_PHYSICAL_ADDRESS)symtab[symbol_index].st_value;
                    break;
                }

                if (hash & 1) break;

                ++symbol_index;
            }
        }
        else {
            // TODO (Anton Lilja, 26-03-2021): Implement standard ELF hash table lookup
            return EFI_UNSUPPORTED;
        }
    }

    // No stack was found
    if (*stack == 0) return EFI_NOT_FOUND;

    // Free file header, program headers, and dynamic table
    status = st->BootServices->FreePool(dynamic_table);
    if (EFI_ERROR(status)) return status;
    status = st->BootServices->FreePool(program_headers);
    if (EFI_ERROR(status)) return status;
    status = st->BootServices->FreePool(file_header);
    if (EFI_ERROR(status)) return status;

    // Close kernel ELF file
    status = file->Close(file);
    if (EFI_ERROR(status)) return status;

    return EFI_SUCCESS;
}
