#pragma once
#include <stdint.h>
#include <stdbool.h>

void read_write_sectors(uint8_t device_id, uint64_t sector, uint16_t sector_count, void* buffer,
                        bool write);

void initialize_ahci();
