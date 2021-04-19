#include <efi.h>

#include "elf-loader.h"
#include "rsdp-finder.h"

EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* st) {
    EFI_STATUS status;

    // Disable the watchdog timer
    // (if the timer were to run out the application would be restarted)
    status = st->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    if (EFI_ERROR(status)) return status;

    // Locate the graphics output protocol and populate frame buffer
    struct {
        EFI_PHYSICAL_ADDRESS address;
        UINT64 width;
        UINT64 height;
        UINT64 pixels_per_scanline;
        EFI_GRAPHICS_PIXEL_FORMAT format;
    } __attribute__((packed)) frame_buffer;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        status = st->BootServices->LocateProtocol(&guid, NULL, (void**)&gop);
        if (EFI_ERROR(status)) return status;

        UINTN size_of_info;
        status = gop->QueryMode(gop, gop->Mode->Mode, &size_of_info, &info);
        if (EFI_ERROR(status)) {
            if (status != EFI_NOT_STARTED) return status;
            status = gop->SetMode(gop, 0);
            if (EFI_ERROR(status)) return status;
        }

        // The kernel expects either BGR or RGB format
        if (info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor &&
            info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {
            return EFI_UNSUPPORTED;
        }

        // Populate frame buffer which will be passed along to the kernel
        frame_buffer.address = gop->Mode->FrameBufferBase;
        frame_buffer.format = info->PixelFormat;
        frame_buffer.pixels_per_scanline = info->PixelsPerScanLine;
        frame_buffer.width = info->HorizontalResolution;
        frame_buffer.height = info->VerticalResolution;
    }

    // Locate the file system protocol
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* file_system_protocol;
    {
        EFI_GUID guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
        status = st->BootServices->LocateProtocol(&guid, NULL, (void**)&file_system_protocol);
        if (EFI_ERROR(status)) return status;
    }

    // Open root file system
    EFI_FILE* root;
    status = file_system_protocol->OpenVolume(file_system_protocol, &root);
    if (EFI_ERROR(status)) return status;

    // Load kernel ELF image, get entry point and stack addresses
    EFI_PHYSICAL_ADDRESS kernel_entry;
    status = load_kernel(st, root, L"KERNEL.ELF", &kernel_entry);
    if (EFI_ERROR(status)) return status;

    // Close file system
    status = root->Close(root);
    if (EFI_ERROR(status)) return status;

    // Find Root System Description Pointer
    EFI_PHYSICAL_ADDRESS rsdp_addr;

    status = find_rsdp(st, &rsdp_addr);
    if (EFI_ERROR(status)) return status;

    // Get the memory map
    struct {
        UINT64 nbytes;
        UINT8* buffer;
        UINTN mapkey;
        UINT64 desc_size;
        UINT32 desc_version;
    } __attribute__((packed)) memory_map;
    {
        // Zero out memory map
        memory_map.nbytes = 0;
        memory_map.buffer = NULL;
        memory_map.mapkey = 0;
        memory_map.desc_size = 0;
        memory_map.desc_version = 0;

        // GetMemoryMap will always fail here because nbytes is zero,
        // nbytes will be set to the required size.
        status = st->BootServices->GetMemoryMap(&memory_map.nbytes,
                                                (EFI_MEMORY_DESCRIPTOR*)memory_map.buffer,
                                                &memory_map.mapkey,
                                                &memory_map.desc_size,
                                                &memory_map.desc_version);
        if (EFI_ERROR(status)) {
            if (status != EFI_BUFFER_TOO_SMALL) {
                return status;
            }
        }

        // We have to increase the size by at least 2
        // because the memory map size could increase after
        // we allocate.
        memory_map.nbytes += memory_map.desc_size * 2;

        // Allocate the right size buffer
        status = st->BootServices->AllocatePool(
            EfiLoaderData, memory_map.nbytes, (void**)&memory_map.buffer);
        if (EFI_ERROR(status)) return status;

        // Call GetMemoryMap again to actually get the information
        status = st->BootServices->GetMemoryMap(&memory_map.nbytes,
                                                (EFI_MEMORY_DESCRIPTOR*)memory_map.buffer,
                                                &memory_map.mapkey,
                                                &memory_map.desc_size,
                                                &memory_map.desc_version);
        if (EFI_ERROR(status)) return status;
    }

    // Exit boot services before handing over control to the kernel
    status = st->BootServices->ExitBootServices(image_handle, memory_map.mapkey);
    if (EFI_ERROR(status)) return status;

    asm(
        // Set up arguments in correct registers
        "mov %[memory_map], %%rdi\n"
        "mov %[frame_buffer], %%rsi\n"
        "mov %[rsdp_addr], %%rdx\n"

        // Push code segment and kernel entry addres onto stack and do a long return
        "movw %%cs, %%ax\n"
        "push %%rax\n"
        "push %[kernel_entry]\n"
        "lretq\n"
        : // No output
          // Input
        : [ kernel_entry ] "r"(kernel_entry),
          [ memory_map ] "r"(&memory_map),
          [ frame_buffer ] "r"(&frame_buffer),
          [ rsdp_addr ] "r"(rsdp_addr)
        // Clobbers
        : "rax", "rdi", "rsi", "rdx");

    // This point will never be reached
    return EFI_LOAD_ERROR;
}
