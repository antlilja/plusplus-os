#pragma once
#include <efi.h>

EFI_STATUS find_rsdp(EFI_SYSTEM_TABLE* st, EFI_PHYSICAL_ADDRESS* rsdp);
