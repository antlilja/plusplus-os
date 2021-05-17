#include "process_system.h"

#include "memory.h"

#include <string.h>

// Stores process information
// All registers except rsp are stored on the user stack when process is not running
typedef struct {
    void* next;               // 0x00
    AddressSpace* addr_space; // 0x8
    uint64_t pid;             // 0x10
    void* rsp;                // 0x18
} __attribute__((packed)) Process;

_Static_assert(sizeof(Process) == 32, "Size of Process struct is not 32 bytes");

struct {
    Process* head; // 0x0
    Process* tail; // 0x8
} __attribute__((packed)) g_process_queue = {0};

_Static_assert(sizeof(g_process_queue) == 16, "Size of g_process_queue is not 16 bytes");

__attribute__((always_inline)) uint64_t generate_pid() {
    static uint64_t pid = 0;
    return pid++;
}

AddressSpace* get_current_process_addr_space() { return g_process_queue.head->addr_space; }
uint64_t get_current_process_pid() { return g_process_queue.head->pid; }

Process* pop_from_queue() {
    if (g_process_queue.tail == 0) return 0;

    Process* head = g_process_queue.head;
    g_process_queue.head = head->next;
    head->next = 0;
    g_process_queue.tail->next = head;
    g_process_queue.tail = head;

    return g_process_queue.head;
}

Process* alloc_process_and_addr_space(uint8_t paging_prot) {
    // Allocate Process struct
    Process* process = kalloc(sizeof(Process));
    memset(process, 0, sizeof(Process));

    // Allocate AddressSpace struct
    process->addr_space = kalloc(sizeof(AddressSpace));
    memset(process->addr_space, 0, sizeof(AddressSpace));

    // Create new address space
    new_address_space(process->addr_space, 0, paging_prot);

    return process;
}
