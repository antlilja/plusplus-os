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

__attribute__((naked)) void context_switch_handler() {
    asm volatile(
        // Save rax on stack
        "push %rax\n"
        "push %rbx\n"

        // Put g_process_queue ptr in rax
        "lea g_process_queue(%rip), %rax\n"

        // Check if tail ptr is zero (This means there is only one process)
        "mov 0x8(%rax), %rbx\n"
        "cmpq $0x0, %rbx\n"
        "jnz __switch_process\n"

        // We only have to send an EOI because we're jumping back to the same process

        // Send EOI to local APIC
        "lea g_lapic(%rip), %rax\n"
        "mov (%rax), %rax\n"
        "movl $0x0, 0xb0(%rax)\n"
        "pop %rbx\n"
        "pop %rax\n"

        "iretq\n"

        // We are switching to a new process
        "__switch_process:\n"

        // Save interrupt stack ptr in rbx
        "mov %rsp, %rbx\n"

        // Switch over to process stack to save registers onto
        "mov 0x28(%rbx), %rsp\n"

        // Save rip
        "mov 0x10(%rbx), %rax\n"
        "push %rax\n"

        // Save cs
        "mov 0x18(%rbx), %rax\n"
        "push %rax\n"

        // Save rflags
        "mov 0x20(%rbx), %rax\n"
        "push %rax\n"

        // Save ss
        "mov 0x30(%rbx), %rax\n"
        "push %rax\n"

        // Save rbp
        "push %rbp\n"

        // Save rax
        "mov 0x8(%rbx), %rax\n"
        "push %rax\n"

        // Save rbx
        "mov (%rbx), %rax\n"
        "push %rax\n"

        // Save general purpose registers
        "push %rcx\n"
        "push %rdx\n"
        "push %rsi\n"
        "push %rdi\n"
        "push %r8\n"
        "push %r9\n"
        "push %r10\n"
        "push %r11\n"
        "push %r12\n"
        "push %r13\n"
        "push %r14\n"
        "push %r15\n"

        // Save process kernel stack ptr
        "lea g_tss(%rip), %rax\n"
        "mov 0x4(%rax), %rcx\n"
        "push %rcx\n"

        // Put head ptr into rax
        "lea g_process_queue(%rip), %rax\n"
        "mov (%rax), %rax\n"

        // Save current process rsp
        "mov %rsp, 0x18(%rax)\n"

        // Restore interrupt stack ptr
        "mov %rbx, %rsp\n"

        // Unmap address space of old process
        "mov 0x8(%rax), %rdi\n"
        "call unmap_address_space\n"

        // Put head at back of queue
        // pop_from_queue returns the new head ptr (in rax)
        "call pop_from_queue\n"

        // Map address space of new process
        "mov 0x8(%rax), %rdi\n"
        "call map_address_space\n"

        // Save interrupt stack ptr to rbx
        "mov %rsp, %rbx\n"

        // Set rsp to user stack ptr to restore registers
        "mov 0x18(%rax), %rsp\n"

        // Replace old process kernel stack ptr with new process kernel stack ptr
        "pop %rcx\n"
        "lea g_tss(%rip), %rdx\n"
        "mov %rcx, 0x4(%rdx)\n"

        // Restore general purpose registers
        "pop %r15\n"
        "pop %r14\n"
        "pop %r13\n"
        "pop %r12\n"
        "pop %r11\n"
        "pop %r10\n"
        "pop %r9\n"
        "pop %r8\n"
        "pop %rdi\n"
        "pop %rsi\n"
        "pop %rdx\n"
        "pop %rcx\n"

        // Pop rbx and replace old rbx on kernel stack
        "pop %rax\n"
        "mov %rax, (%rbx)\n"

        // Pop rax and replace old rax on kernel stack
        "pop %rax\n"
        "mov %rax, 0x8(%rbx)\n"

        "pop %rbp\n"

        // Pop ss and replace old ss on kernel stack
        "pop %rax\n"
        "mov %rax, 0x30(%rbx)\n"

        // Pop rflags and replace old rflags on kernel stack
        "pop %rax\n"
        "mov %rax, 0x20(%rbx)\n"

        // Pop cs and replace cs on kernel stack
        "pop %rax\n"
        "mov %rax, 0x18(%rbx)\n"

        // Pop rip and replace old rip on kernel stack
        "pop %rax\n"
        "mov %rax, 0x10(%rbx)\n"

        // Restore interrupt stack ptr
        "mov %rbx, %rsp\n"

        // Restore rbx
        "pop %rbx\n"

        // Send EOI to local APIC
        "lea g_lapic(%rip), %rax\n"
        "mov (%rax), %rax\n"
        "movl $0x0, 0xb0(%rax)\n"

        // Restore rax
        "pop %rax\n"

        // Return from interrupt
        "iretq\n");
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
        process->rsp = (void*)map_allocation(process->addr_space, allocation, PAGING_WRITABLE) +
                       USER_STACK_SIZE - USER_STACK_SAVE_SIZE;
        free_frame_allocation_entries(allocation);
    }

    // Zero out area of user stack that registers will be popped from
    memset(process->rsp, 0, sizeof(uint64_t) * 20);

    {
        uint64_t* rspu64 = (uint64_t*)process->rsp;

        void* kernel_stack =
            alloc_pages(KERNEL_STACK_SIZE / PAGE_SIZE, PAGING_WRITABLE) + KERNEL_STACK_SIZE;

        rspu64[0] = (uint64_t)kernel_stack; // Kernel stack
        rspu64[19] = (uint64_t)entry;       // rip
        rspu64[18] = GDT_USER_CODE_SEGMENT; // cs
        rspu64[17] = 0x202;                 // rflags
        rspu64[16] = GDT_USER_DATA_SEGMENT; // ss
    }

    // Put process in queue
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

    // Unmask timer interrupt
    g_lapic->lvt_timer &= ~APIC_TIMER_MASKED_MASK;

#if 0
    Process* process = alloc_process_and_addr_space(3);
    map_address_space(process->addr_space);
    g_process_queue.head = process;

    void* entry;
    {
        const bool success = load_elf_from_buff(process->addr_space, elf_data, &entry);
        KERNEL_ASSERT(success, "Failed to load terminal ELF file")
    }

    // Allocate process stack
    {
        PageFrameAllocation* allocation = alloc_frames(USER_STACK_SIZE / PAGE_SIZE);
        process->rsp = (void*)map_allocation(process->addr_space, allocation, PAGING_WRITABLE) +
                       USER_STACK_SIZE;
        free_frame_allocation_entries(allocation);
    }

    // Allocate and set kernel stack
    {
        void* kernel_stack =
            alloc_pages(KERNEL_STACK_SIZE / PAGE_SIZE, PAGING_WRITABLE) + KERNEL_STACK_SIZE;

        set_tss_kernel_stack(kernel_stack);
    }

    // Start local APIC timer
    g_lapic->initial_count = 0xffffff;

    // Jump to userspace program
    asm volatile("mov %0, %%rcx\n"
                 "mov %1, %%rsp\n"
                 "mov $0x0202, %%r11\n"
                 "sysretq\n"
                 :
                 : "g"(entry), "g"(process->rsp));
#endif
}
