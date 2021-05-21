#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char file_name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t time_created_decisecond;
    uint16_t time_created;
    uint16_t data_created;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t time_modified;
    uint16_t date_modified;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) DirectoryEntry;

void start_dir_read();

bool read_dir_next(DirectoryEntry* entry);

void prepare_fat16_disk(uint8_t port);

void read_file_entry(DirectoryEntry* entry, void* buffer);
