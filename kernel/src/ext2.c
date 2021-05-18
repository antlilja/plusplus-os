#include <stdint.h>
#include <string.h>

#define BLOCK_GROUPS 1
#define BLOCKS_PER_GROUP 1438
#define BLOCK_SIZE 1024
#define INODE_SIZE 128
#define INODES_PER_GROUP 184
#define INODES_PER_BLOCK BLOCK_SIZE / INODE_SIZE
//#define DATA_BLOCKS_PER_GROUP 30 // change this

typedef struct {
    uint32_t mode;     // file or folder
    uint32_t size;     // in bytes
    uint32_t data[12]; // pointers to physical blocks of data
    uint32_t singly_block;
    uint32_t doubly_block;
    uint32_t triply_block;
} __attribute__((packed)) inode_t;

typedef struct {
    uint32_t inode_number;
    uint16_t size;
    uint8_t name_length;
    uint8_t file_type;               // 1 = file, 2 = dir
} __attribute__((packed)) directory; // pad name with \0 until multiple of 4

typedef struct {
    // uint32_t magic_number; // 0xEF53 for ext2
    uint32_t block_size;
    // uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    // number of free blocks and free inodes
    uint32_t first_inode;
} __attribute__((packed)) superblock;

typedef struct {
    superblock* sb;
    uint32_t free_blocks_bitmap;
    uint32_t free_inodes_bitmap;
    uint8_t* inode_table;
    // uint8_t *data;
} __attribute__((packed)) block_group;

/*
---------------------------------------------------------------------------------------------
*****************************************CODE************************************************
---------------------------------------------------------------------------------------------
*/

void read_inode(block_group* bg, inode_t* inode_buffer, uint32_t inode_index) {
    uint32_t block_group_index = (inode_index - 1) / INODES_PER_GROUP;
    // Get block group with the correct index (we currently only have one block group)

    // Find index and corresponding inode
    uint32_t inode_index = (inode_index - 1) % INODES_PER_GROUP;
    uint32_t inode_block = (inode_index * INODE_SIZE) / BLOCK_SIZE;
    // Read block to block_buf (add start of inode table)
    inode_t* _inode = (inode_t*)block_buf;
    inode_index = inode_index % (INODES_PER_BLOCK);
    for (uint32_t i = 0; i < inode_index; i++) _inode++;
    memcpy(inode_buffer, _inode, INODE_SIZE);
}

void write_inode(block_group* bg, inode_t* inode_buffer, uint32_t inode_index) {
    uint32_t block_group_index = (inode_index - 1) / INODES_PER_GROUP;
    // Get block group with the correct index (we currently only have one block group)

    // Find index and corresponding inode
    uint32_t inode_index = (inode_index - 1) % INODES_PER_GROUP;
    uint32_t inode_block = (inode_index * INODE_SIZE) / BLOCK_SIZE;
    // Read block to block_buf (add start of inode table)
    inode_t* _inode = (inode_t*)block_buf;
    inode_index = inode_index % (INODES_PER_BLOCK);
    for (uint32_t i = 0; i < inode_index; i++) _inode++;
    memcpy(inode_buffer, _inode, INODE_SIZE);
    // Write block_buf to disk
}

// get_inode_block used by create_file()

uint32_t read_directory() {}

// read_root_directory()

uint8_t find_file_inode() {
    // Return id of inode of the file
}

static inode_t* curr_inode = 0;
uint8_t read_file(char* fn) {
    if (!curr_inode) curr_inode = (inode_t*)malloc(INODE_SIZE);
    char* filename = fn;

    if (!find_file_inode()) {
        // Print file not found;
        return 0;
    }
    // Read 12 normal buffers
    for (uint8_t i = 0; i < 12; i++) {
        // Read blocks to buffer (check that not empty)
    }

    // Read singly block
    // Read doubly block
    return 1;
}

void new_inode_id(uint32_t* id) {
    // Find id for next inode (free)
    // Done in the same way as alloc_block

    // Loop through block groups (currently only one)
    for (uint8_t i = 0; BLOCK_GROUPS; i++) {
        if (1) { // check if free inode exists
            // Find free inode and update superblock
            return;
        }
    }
}

void alloc_block(block_group* bg) {
    // Loop through free blocks bitmap and find free slot
    for (uint8_t i = 0; i < BLOCK_GROUPS; i++) {
        if (1) { // check if free blocks exist
            // Check for first free block in bitmap
            uint8_t first_free = 0;
            uint8_t free_block =
                (40 - first_free) * BLOCK_SIZE; // change this to fetch index of free blocks
            // Allocate free_block
            // Change bitmap

            // update superblock with new number of unallocatedblocks
            return; // return pointer to block (change input pointer)
        }
        // get to next block group
    }
}
void create_file() {}

void write_file(char* filename, char* buf, uint32_t len) {
    inode_t* inode = 0;
    uint32_t inode_id = find_file_inode(); // find inode with filename
    inode_id++;
    if (inode_id == 1) return 0;
    if (!inode) inode = (inode_t*)malloc(INODE_SIZE); // change malloc
    read_inode();

    if (inode->size == 0) {
        uint32_t blocks_to_alloc = len / BLOCK_SIZE;
        blocks_to_alloc++; // We have to allocate atleast one block

        if (blocks_to_alloc > 12) {
            // Max size is 12Kb
            return 0;
        }

        for (uint8_t i = 0; i < blocks_to_alloc; i++) {
            uint32_t bid = 0;  // change name
            alloc_block(&bid); // needs fix
            inode->data[i] = bid;
        }

        inode->size += len;
        write_inode(inode, inode_id - 1); // needs fix

        // Write buf to blocks in disk
        for (uint8_t i = 0; i < blocks_to_alloc; i++) {
            /*
            TODO: loop through blocks and write
            */
        }
        return 1;
    }

    // Inode already has data
    uint32_t last_data_block = inode->size / BLOCK_SIZE;
    uint32_t last_data_offset = (inode->size) % BLOCK_SIZE;

    read_block(last_data_block);

    if (len < BLOCK_SIZE - last_data_offset) {
        /*
        Write buf to last_data_block
        */
        return 1;
    }

    return 0;
}

void mount() {
    // Needs fix to work with multiple block groups
    for (uint8_t i = 0; i < BLOCK_GROUPS; i++) {
        uint8_t* buf = (uint8_t*)malloc(BLOCK_SIZE * BLOCKS_PER_GROUP);
        block_group* bg = (block_group*)buf;

        bg->sb->block_size = BLOCK_SIZE;
        bg->sb->inodes_per_group = INODES_PER_GROUP;
        bg->sb->first_inode = 1;

        bg->free_blocks_bitmap = 0;
        bg->free_inodes_bitmap = 0;
        bg->inode_table = (uint8_t*)malloc(INODE_SIZE * INODES_PER_GROUP);
        // bg->data = (uint8_t *)malloc(BLOCK_SIZE * DATA_BLOCKS_PER_GROUP);
        free(buf);
    }
}