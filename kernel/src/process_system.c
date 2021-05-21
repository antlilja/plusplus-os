#include "process_system.h"

#include "gdt.h"
#include "idt.h"
#include "apic.h"
#include "kassert.h"
#include "elf_loader.h"
#include "memory.h"
#include "memory/paging.h"
#include "memory/frame_allocator.h"

#include <string.h>

// APIC timer defines
#define APIC_TIMER_IRQ_MASK 0xff
#define APIC_TIMER_MASKED_MASK (1 << 16)
#define APIC_TIMER_MODE_MASK (0b11 << 17)
#define APIC_TIMER_PERIODIC_MODE (0b1 << 17)
#define APIC_TIMER_IRQ 32
#define APIC_TIMER_INITIAL_COUNT 0xffffff

// Stack defines
#define KERNEL_STACK_SIZE 0x4000
#define USER_STACK_SIZE 0x8000
#define USER_STACK_SAVE_SIZE (sizeof(uint64_t) * 20)

// Stores process information
// All registers except rsp are stored on the user stack when process is not running
typedef struct {
    void* next;               // 0x00
    AddressSpace* addr_space; // 0x8
    uint64_t pid;             // 0x10
    void* context_stack_ptr;  // 0x18
} Process;

struct {
    Process* head; // 0x0
    Process* tail; // 0x8
} __attribute__((packed)) g_process_queue = {0};

_Static_assert(sizeof(g_process_queue) == 16, "Size of g_process_queue is not 16 bytes");

__attribute__((always_inline)) uint64_t generate_pid() {
    static uint64_t pid = 0;
    return pid++;
}

void context_switch(void* rsp) {
    // Save register and return state in on process stack
    {
        void* process_stack = ((void**)rsp)[18] - USER_STACK_SAVE_SIZE;

        memmove(process_stack, rsp, USER_STACK_SAVE_SIZE);

        g_process_queue.head->context_stack_ptr = process_stack;
    }

    unmap_address_space(g_process_queue.head->addr_space);

    Process* tmp = g_process_queue.head;
    if (tmp->next != 0) {
        g_process_queue.head = tmp->next;

        tmp->next = 0;
        g_process_queue.tail->next = tmp;
        g_process_queue.tail = tmp;
    }

    map_address_space(g_process_queue.head->addr_space);

    // Register register and return state in on process stack
    {
        void* process_stack = g_process_queue.head->context_stack_ptr;
        memmove(rsp, process_stack, USER_STACK_SAVE_SIZE);
    }
}

__attribute__((naked)) void context_switch_handler() {
    asm volatile(
        // Save all registers
        "push %rax\n"
        "push %rbx\n"
        "push %rcx\n"
        "push %rdx\n"
        "push %rsi\n"
        "push %rdi\n"
        "push %rbp\n"
        "push %r8\n"
        "push %r9\n"
        "push %r10\n"
        "push %r11\n"
        "push %r12\n"
        "push %r13\n"
        "push %r14\n"
        "push %r15\n"

        // Switch process
        "mov %rsp, %rdi\n"
        "call context_switch\n"

        // Restore registers
        "pop %r15\n"
        "pop %r14\n"
        "pop %r13\n"
        "pop %r12\n"
        "pop %r11\n"
        "pop %r10\n"
        "pop %r9\n"
        "pop %r8\n"
        "pop %rbp\n"
        "pop %rdi\n"
        "pop %rsi\n"
        "pop %rdx\n"
        "pop %rcx\n"
        "pop %rbx\n"

        // Send EOI to local APIC
        "lea g_lapic(%rip), %rax\n"
        "mov (%rax), %rax\n"
        "movl $0x0, 0xb0(%rax)\n"
        "pop %rax\n"

        "iretq\n");
}

AddressSpace* get_current_process_addr_space() { return g_process_queue.head->addr_space; }
uint64_t get_current_process_pid() { return g_process_queue.head->pid; }

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

void start_user_process(const void* elf_data) {
    KERNEL_ASSERT(g_process_queue.head != 0,
                  "start_user_process can't be called without previously running process")

    Process* process = alloc_process_and_addr_space(3);
    map_address_space(process->addr_space);

    void* entry;
    {
        const bool success = load_elf_from_buff(process->addr_space, elf_data, &entry);
        KERNEL_ASSERT(success, "Failed to load terminal ELF file")
    }

    // Allocate user stack
    {
        PageFrameAllocation* allocation = alloc_frames(USER_STACK_SIZE / PAGE_SIZE);
        process->context_stack_ptr =
            (void*)map_allocation(process->addr_space, allocation, PAGING_WRITABLE) +
            USER_STACK_SIZE - USER_STACK_SAVE_SIZE;
        free_frame_allocation_entries(allocation);
    }

    // Zero out area of user stack that registers will be popped from
    memset(process->context_stack_ptr, 0, USER_STACK_SAVE_SIZE);

    {
        uint64_t* rspu64 = (uint64_t*)process->context_stack_ptr;
        rspu64[15] = (uint64_t)entry;                      // rip
        rspu64[16] = GDT_USER_CODE_SEGMENT | 3;            // cs
        rspu64[17] = 0x202;                                // rflags
        rspu64[18] = (uint64_t)process->context_stack_ptr; // rsp
        rspu64[19] = GDT_USER_DATA_SEGMENT | 3;            // ss
    }

    g_process_queue.tail->next = process;
    g_process_queue.tail = process;
}

void initialize_process_system() {
    // Initialize local APIC timer
    register_interrupt(APIC_TIMER_IRQ, INTERRUPT_GATE, false, (void*)&context_switch_handler);

    // Set mode to periodic mode
    g_lapic->lvt_timer &= ~APIC_TIMER_MODE_MASK;
    g_lapic->lvt_timer |= APIC_TIMER_PERIODIC_MODE;

    // Set irq
    g_lapic->lvt_timer &= ~APIC_TIMER_IRQ_MASK;
    g_lapic->lvt_timer |= APIC_TIMER_IRQ;

    // Mask LINT0 and LINT1 interrupts
    g_lapic->lvt_lint0 |= APIC_TIMER_MODE_MASK;
    g_lapic->lvt_lint1 |= APIC_TIMER_MODE_MASK;

    // Set divice config to 2
    g_lapic->divide_configuration = 0x0;

#if 0
    // Setup shared kernel stack
    {
        void* kernel_stack =
            alloc_pages(KERNEL_STACK_SIZE / PAGE_SIZE, PAGING_WRITABLE) + KERNEL_STACK_SIZE;

        set_tss_kernel_stack(kernel_stack);
    }

    Process* process = alloc_process_and_addr_space(3);
    map_address_space(process->addr_space);
    g_process_queue.head = process;
    g_process_queue.tail = process;

    void* entry;
    {
        const bool success = load_elf_from_buff(process->addr_space, elf_data, &entry);
        KERNEL_ASSERT(success, "Failed to load terminal ELF file")
    }

    // Allocate process stack
    {
        PageFrameAllocation* allocation = alloc_frames(USER_STACK_SIZE / PAGE_SIZE);
        process->context_stack_ptr =
            (void*)map_allocation(process->addr_space, allocation, PAGING_WRITABLE) +
            USER_STACK_SIZE;
        free_frame_allocation_entries(allocation);
    }

    // Unmask timer interrupt
    g_lapic->lvt_timer &= ~APIC_TIMER_MASKED_MASK;

    // Start local APIC timer
    g_lapic->initial_count = 0xffffff;

    // Jump to userspace program
    asm volatile("mov %0, %%rcx\n"
                 "mov %1, %%rsp\n"
                 "mov $0x0202, %%r11\n"
                 "sysretq\n"
                 :
                 : "g"(entry), "g"(process->context_stack_ptr));
#endif
}
