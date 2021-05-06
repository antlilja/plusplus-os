#include "rsdp-finder.h"

// I could not find any functions comparing two EFI_GUIDs.

// Macro of GUID must be put in memory for comparisons
const EFI_GUID acpi_guid_bytes = ACPI_20_TABLE_GUID;
const uint8_t* acpi_guid = (uint8_t*)&acpi_guid_bytes;

EFI_STATUS find_rsdp(EFI_SYSTEM_TABLE* st, EFI_PHYSICAL_ADDRESS* rsdp) {

    // Iterate config table
    for (UINTN i = 0; i < st->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE* table = st->ConfigurationTable + i;

        const uint8_t* guid = (uint8_t*)&table->VendorGuid;

        { // Memcmp
            int j = 0;
            while (j < 16 && guid[j] == acpi_guid[j]) j++;

            // If entire guid matched
            if (j == 16) {
                *rsdp = (EFI_PHYSICAL_ADDRESS)table->VendorTable;

                return EFI_SUCCESS;
            }
        }
    }

    // No rsdp could be found for ACPI 2.0
    return EFI_NOT_FOUND;
}
