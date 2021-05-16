// A function in the syscall table can have up to 3 64-bit parameters, and return both 64 and 128
// bits. If you can make sure the ECX parameter is unused, all but the stack parameters can be used.

#include "syscalls.h"
#include "gdt.h"

// Number of entries in the syscall table, can be increased when needed
#define NUM_SYSCALLS 1

void* g_syscall_table[NUM_SYSCALLS];

__attribute__((naked)) void syscall_dispatcher() {
    asm volatile("push %%rsp\n" // Store userspace stack pointer
                 "push %%rcx\n" // rcx contains rip before syscall
                 "push %%r11\n" // r11 contains flags before syscall
                 "push %%rbp\n"
                 "mov %%rsp, %%rbp\n"

                 // Make sure syscall is within bounds
                 "cmp %[num_syscalls], %%rax\n"
                 "jge _oob\n"

                 // Turn rax into a table entry pointer
                 "shl $3, %%rax\n"
                 "lea g_syscall_table(%%rip), %%rcx\n"
                 "add %%rcx, %%rax\n"

                 // Make sure syscall exists
                 "cmpq $0, (%%rax)\n"
                 "jz _oob\n"

                 "call *(%%rax)\n"

                 "_oob:\n"
                 "pop %%rbp\n"
                 "pop %%r11\n" // Set sysret flags
                 "pop %%rcx\n" // Set sysret rip
                 "pop %%rsp\n" // retrieve userspace stack pointer
                 "sysretq"
                 :
                 : [ num_syscalls ] "i"(NUM_SYSCALLS)
                 : "rcx", "rax");
}

void syscall_halt() {
    while (1)
        ;
}

void prepare_syscalls() {
    // Enable SCE and set syscall address
    {
        uint64_t syscall_addr = (uint64_t)&syscall_dispatcher;
        uint32_t addr_low = (uint32_t)syscall_addr & 0xffffffff;
        uint32_t addr_high = (uint32_t)(syscall_addr >> 32) & 0xffffffff;

        // Bits 32-47 contain kernel code segment,
        // While bits 48-63 contains user base segment,
        // Where code = base + 0x10 and data = base + 0x8
        uint32_t star_high = GDT_KERNEL_CODE_SEGMENT | (((GDT_USER_CODE_SEGMENT | 3) - 0x10) << 16);

        asm volatile(
            // Set bit 1 (system calls) in EFER MSR (0xC0000080)
            "mov $0xC0000080, %%rcx\n"
            "rdmsr\n"
            "or $1, %%eax\n"
            "wrmsr\n"

            // Set STAR MSR (0xC0000081), which sets cs and ss
            "mov $0xC0000081, %%rcx\n"
            "rdmsr\n"
            "mov %[star_high], %%edx\n"
            "wrmsr\n"

            // Set syscall location
            "mov $0xC0000082, %%rcx\n"
            "mov %[addr_low], %%eax\n"
            "mov %[addr_high], %%edx\n"
            "wrmsr"
            :
            : [ addr_low ] "g"(addr_low), [ addr_high ] "g"(addr_high), [ star_high ] "g"(star_high)
            : "rax", "rcx", "rdx");
    }

    // Fill syscall table, which are then called through the dispatcher
    g_syscall_table[SYSCALL_HALT] = &syscall_halt;
}
