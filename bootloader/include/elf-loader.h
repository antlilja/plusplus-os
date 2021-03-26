#pragma once
#include <efi.h>

EFI_STATUS load_kernel(EFI_SYSTEM_TABLE* st, EFI_FILE* root, CHAR16* filename,
                       EFI_PHYSICAL_ADDRESS* entry_point, EFI_PHYSICAL_ADDRESS* stack);
