#include "fat.h"

#include "ahci.h"
#include "memory/slab_allocator.h"
#include "rendering.h"

#include <string.h>

#define LONG_NAME_BUFFER_SIZE 64

typedef struct {
    uint8_t bootjmp[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t table_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t table_size_16;
    uint16_t sectors_per_track;
    uint16_t head_side_count;
    uint32_t hidden_sector_count;
    uint32_t total_sectors_32;

    // Extension
    unsigned char bios_drive_num;
    unsigned char reserved1;
    unsigned char boot_signature;
    unsigned int volume_id;
    unsigned char volume_label[11];
    unsigned char fat_type_label[8];
} __attribute__((packed)) FATBPB;

typedef struct {
    char file_name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint16_t time_created;
    uint16_t data_created;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t time_modified;
    uint16_t date_modified;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) NameEntry;

uint8_t g_disk_port;

uint16_t g_sector_size;
uint16_t g_sectors_per_cluster;
uint16_t g_first_data_sector;
uint16_t g_first_fat_sector;
uint16_t g_fat_size;

uint16_t g_root_dir_sectors;
void* g_dir_sector_buffer;
void* g_fats_buffer;

void* g_long_filename_buffer;
void* g_lf_top;

uint16_t g_current_sector;
uint16_t g_current_index;
uint64_t g_current_cluster;

FATBPB g_fat_boot;

void prepare_fat16_disk(uint8_t port) {
    g_disk_port = port;
    FATBPB fat_boot;

    g_fats_buffer = kalloc(512);

    read_write_sectors(g_disk_port, 0, 1, g_fats_buffer, false);
    memcpy(&fat_boot, g_fats_buffer, sizeof(FATBPB));

    g_current_sector = 0;
    g_current_cluster = 0;
    g_current_index = 0;
    g_fat_size = fat_boot.table_size_16;
    g_sector_size = fat_boot.bytes_per_sector;
    g_sectors_per_cluster = fat_boot.sectors_per_cluster;

    uint16_t table_section_size = fat_boot.table_count * g_fat_size;

    g_root_dir_sectors = ((fat_boot.root_entry_count * 32) + (fat_boot.bytes_per_sector - 1)) /
                         fat_boot.bytes_per_sector;

    put_hex(g_root_dir_sectors, 0, 0);

    g_first_fat_sector = fat_boot.reserved_sector_count;
    g_first_data_sector = g_first_fat_sector + table_section_size + g_root_dir_sectors;

    g_dir_sector_buffer = kalloc(fat_boot.bytes_per_sector);
    g_long_filename_buffer = kalloc(LONG_NAME_BUFFER_SIZE);
}

bool next_dir_index(void** entry_ptr) {
    *entry_ptr = g_dir_sector_buffer + g_current_index; // 32 byte entries
    if (*(uint8_t*)entry_ptr == 0) return false;

    // Next index
    g_current_index += 32;
    if (g_current_index >= g_sector_size) {
        g_current_sector++;
        g_current_index = 0;

        read_write_sectors(g_disk_port, g_current_sector, 1, g_dir_sector_buffer, false);
    }

    return true;
}

void start_dir_read() {
    g_current_sector = g_first_data_sector - g_root_dir_sectors;
    read_write_sectors(g_disk_port, g_current_sector, 1, g_dir_sector_buffer, false);

    g_current_index = 0;
    g_current_cluster = 0;
}

uint64_t y = 25;
bool read_dir_next(DirectoryEntry* entry) {
    void* entry_ptr;
    bool exists = next_dir_index(&entry_ptr);
    if (exists == false) return false;

    // Find entry that isn't a long name, because long names take time to implement.
    while (*(uint8_t*)(entry_ptr + 11) == 0x0F) {
        next_dir_index(&entry_ptr);
    }

    memcpy(entry, entry_ptr, sizeof(DirectoryEntry));
    return true;
}

void read_file_entry(DirectoryEntry* entry, void* buffer) {
    uint16_t cluster = entry->cluster_low;
    uint16_t sector = ((cluster - 2) * g_sectors_per_cluster) + g_first_data_sector;
    put_hex(sector, 45, 28);
    put_hex(cluster, 55, 28);
    put_hex(g_sectors_per_cluster, 55, 29);

    uint64_t x = 0;
    while (true) {
        read_write_sectors(g_disk_port, sector, g_sectors_per_cluster, buffer, false);
        buffer += g_sector_size * g_sectors_per_cluster;

        uint16_t fat_offset = cluster + (cluster / 2);
        uint16_t fat_sector = g_first_fat_sector + (fat_offset / g_sector_size);
        uint16_t index = fat_offset % g_sector_size;

        read_write_sectors(g_disk_port, fat_sector, 1, g_fats_buffer, false);

        uint16_t table_value = *(uint16_t*)(g_fats_buffer + index);

        if (cluster & 0x0001)
            table_value = table_value >> 4;
        else
            table_value = table_value & 0x0FFF;

        if (table_value >= 0xFF8) break;
        if (table_value == 0xFF7) break;

        cluster = table_value;
        sector = ((cluster - 2) * g_sectors_per_cluster) + g_first_data_sector;
    }
}
