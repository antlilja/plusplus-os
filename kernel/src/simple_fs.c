
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define BLOCKS 20
#define BLOCK_SIZE 1024
#define FILES_PER_BLOCK 1024 / 32

void* buf;
// buf = malloc(BLOCKS * BLOCK_SIZE);
// memset(buf, 0, BLOCKS * BLOCK_SIZE);
typedef struct {
    char name[16];
    uint16_t size;
    uint16_t data_blocks[14];
} __attribute__((packed)) file_entry;

int get_file_index(char* name) {
    file_entry* file_entries = (file_entry*)buf;
    for (int i = 0; i < FILES_PER_BLOCK; i++) {
        if (strcmp(file_entries[i].name, name) == 0) return i;
    }
    return -1;
}

uint16_t get_data_block() {
    uint64_t bitmask = *(uint64_t*)(buf + BLOCK_SIZE);
    for (int i = 2; i < BLOCKS; i++) {
        if (bitmask & 1 << i == 0) {
            bitmask += 1 << i;
            return i;
        }
    }
}

void delete_file(char* name) {
    int index = get_file_index(name);
    if (index == -1) return;
    uint64_t bitmask = *(uint64_t*)(buf + BLOCK_SIZE);
    file_entry* file_entries = (file_entry*)buf;
    for (int i = 0; i < 14; i++) {
        uint16_t block = file_entries[index].data_blocks[i];
        if (block && bitmask & 1 << block) {
            bitmask -= 1 << i;
        }
    }
    memset((void*)&file_entries[index], 0, 32);
}
int create_file(char* name, uint16_t size) {
    if (get_file_index(name) != -1) return -1;
    int index = -1;
    file_entry* file_entries = (file_entry*)buf;
    for (int i = 0; i < FILES_PER_BLOCK; i++) {
        if (file_entries[i].name[0] == 0) {
            index = i;
            break;
        }
    }
    strcpy(file_entries[index].name, name);
    file_entries[index].size = size;
    uint16_t blocks = size / BLOCK_SIZE;
    blocks += 1 ? size % BLOCK_SIZE : 0;
    for (int i = 0; i < blocks; i++) {
        file_entries[index].data_blocks[i] = get_data_block();
    }
    return index;
}